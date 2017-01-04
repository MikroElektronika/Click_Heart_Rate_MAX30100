/*******************************************************************************
* EasyMx PRO v7 for STM32 ARM
* STM32F107 72MHz
*******************************************************************************/

#include "heartrate1_hw.h"
#include "resources.h"
#include <stdbool.h>

#define DataIsReady()     ( dataReady == 0 )
#define DataIsNotReady()  ( dataReady != 0 )


sbit dataReady at GPIOD_IDR.B10;

void InitTimer2(){
  RCC_APB1ENR.TIM2EN = 1;
  TIM2_CR1.CEN = 0;
  TIM2_PSC = 1;
  TIM2_ARR = 35999;
  NVIC_IntEnable(IVT_INT_TIM2);
  TIM2_DIER.UIE = 1;
  TIM2_CR1.CEN = 1;
}


static void display_init()
{
    TFT_Init_ILI9341_8bit( 320, 240 );
    TFT_Set_Pen( CL_WHITE, 1 );
    TFT_Set_Brush( 1, CL_WHITE, 0, 0, 0, 0 );
    TFT_Set_Font( TFT_defaultFont, CL_BLACK, FO_HORIZONTAL );
    TFT_Fill_Screen( CL_WHITE );
    TFT_Set_Pen( CL_Black, 1 );
    TFT_Line( 20, 220, 300, 220 );
    TFT_LIne( 20,  40, 300,  40 );
    TFT_Set_Font( &HandelGothic_BT21x22_Regular, CL_RED, FO_HORIZONTAL );
    TFT_Write_Text( "Heartrate click", 100, 10 );
    TFT_Set_Font( &Verdana12x13_Regular, CL_BLACK, FO_HORIZONTAL );
    TFT_Write_Text( "EasyMx PRO v7 for STM32", 19, 223 );
    TFT_Set_Font( &Verdana12x13_Regular, CL_RED, FO_HORIZONTAL );
    TFT_Write_Text( "www.mikroe.com", 200, 223 );
    TFT_Set_Font( &TFT_defaultFont, CL_BLACK, FO_HORIZONTAL );
}

static void values_init()
{
    TFT_Write_Text( "Values from the sensor", 20, 55 );
    TFT_Write_Text( "IR:", 20, 70 );
    TFT_Write_Text( "RED:", 20, 85 );
    TFT_Write_Text( "Place finger on the sensor!", 20, 115 );
}

static void clear_ir_red( unsigned int startX, unsigned int startY )
{
    TFT_Set_Pen( CL_WHITE, 1 );
    TFT_Rectangle( startX, startY, 155, startY + 15 );
}

static void clear_area( unsigned int startX, unsigned int startY )
{
    TFT_Set_Pen( CL_WHITE, 1 );
    TFT_Rectangle( startX, startY, 310, startY + 15 );
}

//------------------------------------------------------------------------------
// Display IR and RED values from the sensor
//------------------------------------------------------------------------------
static void display_ir_red(uint16_t ir_val, uint16_t red_val)
{
    char ir_screen[20], red_screen[16];
    TFT_Set_Pen( CL_Black, 1 );
    WordToStr( ir_val, ir_screen );
    clear_ir_red( 80, 70 );
    TFT_Write_Text( ir_screen, 80, 70 );
    WordToStr( red_val, red_screen);
    clear_ir_red( 80, 85 );
    TFT_Set_Pen( CL_Black, 1 );
    TFT_Write_Text( red_screen, 80, 85 );
}

/*******************************************************************************
* System init
*******************************************************************************/
static void system_init()
{
    GPIO_Digital_Input( &GPIOD_BASE, _GPIO_PINMASK_10 );
    I2C1_Init_Advanced( 100000, &_GPIO_MODULE_I2C1_PB67 );
    Delay_ms( 100 );
    display_init();
    hr_init( MAX30100_I2C_ADR );
    Delay_ms( 100 );
    UART1_Init(57600);
}

/*******************************************************************************
* Main program
********************************************************************************
* sample_num -  number of read samples
* ir_buff, red_buff - Raw values from LED diodes
* ir_average, red_average - averaged values
* Data is sent through UART to MikroPlot
*******************************************************************************/

    static uint32_t miliseconds_counter = 0;

void main()
{
   uint16_t ir_average;
   uint16_t red_average;
   char ir_string[20], red_string[20], time_string[20];
   uint16_t temp_buffer_ctr;
   uint8_t sample_num;
   unsigned long ir_buff[16]  = {0}, red_buff[16] = {0};
   static bool first_measurement, measurement_continues;

   system_init();
   values_init();
   first_measurement = true;
   measurement_continues = false;

   while ( 1 )
   {
        if ( DataIsReady() && (( hr_get_status() & 0x20 ) != 0) )
        {

           sample_num = hr_read_diodes( ir_buff, red_buff );               // Read IR and RED sensor data and store it in ir_buff and red_buff
           if ( sample_num >= 1 )
           {
                ir_average = 0;
                red_average = 0;
                for ( temp_buffer_ctr = 0; temp_buffer_ctr < sample_num; temp_buffer_ctr++ )
                {
                    ir_average += ir_buff[temp_buffer_ctr];
                    red_average += red_buff[temp_buffer_ctr];
                }
                ir_average  /= sample_num;                                         // calculate the average value for this reading
                red_average /= sample_num;
                display_ir_red( ir_average, red_average );                         // display the calculated averages on the TFT display

                if(ir_average > 10000 )
                {
                    if (measurement_continues == false && first_measurement == false)
                    {
                       measurement_continues = true;
                       clear_area( 20, 115 );
                       TFT_Write_Text( "Measuring and sending data", 20, 115 );
                    }

                    if (first_measurement == true)                                 // if this is our first measurement, start the timer to count miliseconds
                    {
                        clear_area( 20, 115 );
                        TFT_Write_Text( "Measuring and sending data", 20, 115 );
                        InitTimer2();
                        EnableInterrupts();
                        Uart1_Write_Text("START\r\n");
                        first_measurement = false;
                    }

                    FloatToStr(ir_average,ir_string);                              // convert HR values and time values to string, and send them to MikroPlot
                    Floattostr(miliseconds_counter, time_string);
                    ltrim(time_string);
                    strcat(ir_string, ",");
                    strcat(ir_string,time_string);
                    strcat(ir_string, "\r\n");
                    UART1_Write_Text(ir_string);
                }
                else
                {
                        clear_area( 20, 115 );
                        TFT_Write_Text( "Place finger on the sensor!", 20, 115 );
                        measurement_continues = false;
                }
            }
        }
    }
}

void Timer2_interrupt() iv IVT_INT_TIM2
{
  TIM2_SR.UIF = 0;
  miliseconds_counter++;
}
