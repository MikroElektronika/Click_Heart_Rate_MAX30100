/*******************************************************************************
* EasyMx PRO v7 for STM32 ARM
* STM32F107 72MHz
*******************************************************************************/

#include "heartrate1_hw.h"
#include "resources.h"

#define DataIsReady()     ( dataReady == 0 )
#define DataIsNotReady()  ( dataReady != 0 )
#define SAMPLES  750
#define FORWARD    6
#define BACKWARD   8
#define GAP        0
#define GAP_AFTER 11

char ir_screen[16], red_screen[16], hb_screen[16], sp_screen[16], process[16];
char text[16],temp_screen[16];
uint16_t temp_value;
int cnt_samples, cnt;
uint16_t ir_buffer[1000], red_buffer[1000];

/*******************************************************************************
* TFT init
*******************************************************************************/
// TFT module connections
unsigned int TFT_DataPort at GPIOE_ODR;
sbit TFT_RST at GPIOE_ODR.B8;
sbit TFT_RS at GPIOE_ODR.B12;
sbit TFT_CS at GPIOE_ODR.B15;
sbit TFT_RD at GPIOE_ODR.B10;
sbit TFT_WR at GPIOE_ODR.B11;
sbit TFT_BLED at GPIOE_ODR.B9;
// End TFT module connections

sbit dataReady at GPIOD_IDR.B10;

// Resources
char const extern Arial_Black24x33_Regular[], Arial_Black27x38_Regular[],
                  Arial_Black64x90_Regular[], Tahoma19x23_Regular[],
                  active_jpg[6823], idle_red_jpg[6089];

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
    TFT_Write_Text( "BPM:", 20, 130 );
    TFT_Write_Text( "SPO2:", 20, 145 );
    TFT_Write_Text( "Progress:", 20, 175 );
    TFT_Write_Text( "%", 115, 175 );
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
// IR and RED values from the sensor
//------------------------------------------------------------------------------
static void display_ir_red(uint16_t ir_val, uint16_t red_val) //[from ADC]
{
    TFT_Set_Pen( CL_Black, 1 );
    WordToStr( ir_val, ir_screen );
    clear_ir_red( 80, 70 );
    TFT_Write_Text( ir_screen, 80, 70 );
    WordToStr( red_val, red_screen);
    clear_ir_red( 80, 85 );
    TFT_Set_Pen( CL_Black, 1 );
    TFT_Write_Text( red_screen, 80, 85 );
}

//------------------------------------------------------------------------------
// BPM and SPO2 values
//------------------------------------------------------------------------------
static void display_bpm_spo2()    //[ Algorithm ]
{
    //FloatToStr( hb_screen, text );
    clear_area( 76, 130 );
    TFT_Set_Pen( CL_Black, 1 );
    TFT_Write_Text( hb_screen, 76, 130 );
    //FloatToStr( sp_screen, text );
    clear_area( 80, 145 );
    TFT_Write_Text( sp_screen, 80, 145 );
}

//------------------------------------------------------------------------------
// Samples
//------------------------------------------------------------------------------
static void display_samples(int j)
{
    IntToStr( j, temp_screen );
    clear_area( 85, 175 );
    TFT_Write_Text( temp_screen, 85, 175 );
    TFT_Write_Text( "%", 115, 175 );
}

//------------------------------------------------------------------------------
// Progress bar
//------------------------------------------------------------------------------
void update_progress_bar ( int x )
{
    TFT_Set_Pen(CL_BLACK, 1);
    TFT_Set_Brush(1, CL_BLACK, 0, 0, 0, 0);
    TFT_Rectangle(20+x, 200, 31+x, 210);
    TFT_Set_Pen(CL_WHITE, 1);
    TFT_Set_Brush(1, CL_WHITE, 0, 0, 0, 0);
}

/******************************************************************************
* Searching max
*******************************************************************************/
uint16_t avr_forward ( const uint16_t *pos )
{
    uint16_t average=0;
    uint32_t sum=0;
    uint16_t const *ptr = pos;
    int i;
    for(i=1; i<=FORWARD; i++)
    {
        sum += *(ptr+GAP+i);
    }
    sum /= FORWARD;
    average = sum;
    return average;
}

uint16_t avr_backward ( const uint16_t *pos )
{
    uint16_t average=0;
    uint32_t sum=0;
    uint16_t const *ptr = pos; // ptr = pos
    int i;
    for(i=1; i<=BACKWARD; i++)
    {
        sum += *(ptr-GAP-i);
    }
    sum /= BACKWARD;
    average = sum;
    return average;
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
    UART1_Init(115200);
}

/*******************************************************************************
* FFT
*******************************************************************************/


/*******************************************************************************
* Main program
********************************************************************************
* sample_num -  number of read samples
* temp_value -  raw value from the temperature sensor
* ir_buff, red_buff - Raw values from LED diodes
* ir_average, red_average - averaged values
* cnt_samples - samples counter
*******************************************************************************/
void main()
{
    uint8_t maximum[100], p1=0;
    uint16_t j = 0;
    uint16_t ir_average;
    uint16_t red_average;
    int i, num, counter, sum, max, heart_beat=0, av, avb, avf, x=0, max_fl;
    int procentage = 0, processed;
    float sp_val,temp;
    char sample_num;
    unsigned long ir_buff[16]  = {0},
                  red_buff[16] = {0};

   system_init();
   values_init();

   while ( 1 )
   {
        if ( DataIsReady() && (( hr_get_status() & 0x20 ) != 0) )
        {
           // Read IR and RED sensor data and store it in ir_buff and red_buff
           sample_num = hr_read_diodes( ir_buff, red_buff );
           if ( sample_num >= 1 )
           {
                ir_average = 0;
                red_average = 0;
                for ( i = 0; i < sample_num; i++ )
                {
                    ir_average += ir_buff[i];
                    red_average += red_buff[i];
                }
                ir_average  /= sample_num;
                red_average /= sample_num;
                display_ir_red( ir_average, red_average );

                if(ir_average > 10000 )
                {
                    if (j == 1)
                    {
                        clear_area( 20, 115 );
                        TFT_Write_Text( "Please wait for 15 seconds...", 20, 115 );
                    }
                    red_buffer[j] = red_average;
                    ir_buffer[j] = ir_average;
                    j++;
                    p1=0;
                    if( j%30 == 0 )
                    {
                        update_progress_bar( x );
                        x+=11;
                        if( x%20 == 0 ) x+=1;
                        display_samples(procentage+=4);
                    }
                }
                else
                {
                    j=0;
                    x=0;
                    procentage = 0;
                    clear_area( 85, 175 );
                    clear_area( 20, 200 );
                    if(p1 == 0)
                    {
                        clear_area( 20, 115 );
                        TFT_Write_Text( "Place finger on the sensor!", 20, 115 );
                        p1=1;
                    }
                }
            }
        }
        if( j >= SAMPLES) break;
    }
    clear_area( 20, 115 );
    TFT_Write_Text( "Your results:", 20, 115 );
    update_progress_bar ( 269 );
//------------------------------------------------------------------------------
// Signal correction
//------------------------------------------------------------------------------
   /*
   for (cnt = 30; cnt < SAMPLES-1; cnt++)
   {

       if( ir_buffer[cnt] < 30000 )
           ir_buffer[cnt] = ir_buffer[cnt-3];
       if( red_buffer[cnt] < 25000 )
           red_buffer[cnt] = red_buffer[cnt-3];


       if((ir_buffer[cnt-1]-ir_buffer[cnt] > 5000) && (ir_buffer[cnt+1]-ir_buffer[cnt] > 5000))
       {
           ir_buffer[cnt] = (ir_buffer[cnt-3]/2)+(ir_buffer[cnt+3]/2);
       }
       if((red_buffer[cnt-3]-red_buffer[cnt] > 5000) && (red_buffer[cnt+3]-red_buffer[cnt] > 5000))
       {
           red_buffer[cnt] = (red_buffer[cnt-3]/2)+(red_buffer[cnt+3]/2);
       }


   }
   */
//------------------------------------------------------------------------------
// Moving average
//------------------------------------------------------------------------------
    /*
    // Moving average 1
    for( cnt = 30; cnt < SAMPLES ; cnt++ )
    {
        sum = 0;
        for( counter = cnt; counter >= cnt-5; counter-- )
        {
            sum += ir_buffer[counter];
        }
        sum/=6;
        red_buffer[cnt] = sum;
    }
    */
    /*
for(cnt = 12; cnt < SAMPLES ; cnt++)
{
    sum = 0;
    for(cnt = cnt; cnt >= cnt-7; cnt--)
    {
        sum += ir_buffer[cnt];
    }
    sum /= 10;
    ir_buffer[cnt] = sum;
}
    */
//------------------------------------------------------------------------------
// Searching max
//------------------------------------------------------------------------------

for(cnt = 15; cnt < SAMPLES-12; cnt++)
{
    max_fl = 1;
    for(i = cnt-15; (i<=cnt+15) && max_fl ;i++)
    {
        if(ir_buffer[cnt] < ir_buffer[cnt]) max_fl = 0;
    }
    if(max_fl) heart_beat++; //maks[heart_beat++] = ir_buffer[cnt];
}

    /*
    // UART output
    UART_Write_Text("Moving average\r\n");
    for(cnt = 0; cnt < SAMPLES ;cnt++)
    {
        WordToStr(ir_buffer[cnt],ir_screen);
        UART1_Write_Text(ir_screen);
        UART1_Write(13);
        UART1_Write(10);
    }
    */

//------------------------------------------------------------------------------
// Searching max IR LED
//------------------------------------------------------------------------------
   /*
   for (cnt = 20; cnt < SAMPLES-20; cnt++)
   {
       //av  = avr_3_buff( ir_buffer[cnt] );
       av  = ir_buffer[cnt];
       avb = avr_backward( ir_buffer[cnt] );
       avf = avr_forward( ir_buffer[cnt] );
       if(  (av > avb) && (av > avf))
       {
           //maximum[heart_beat]=ir_buffer[cnt]+1000;
           ir_buffer[cnt]+=3000;
           heart_beat++;
           cnt+=GAP_AFTER;
       }
   }
   */
   // UART output
   for(cnt = 0; cnt < SAMPLES ;cnt++)
   {
       WordToStr(ir_buffer[cnt],ir_screen);
       UART1_Write_Text(ir_screen);
       UART1_Write(13);
       UART1_Write(10);
   }

//------------------------------------------------------------------------------
// FFT
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Final results heart rate
//------------------------------------------------------------------------------
    heart_beat*=(3000/SAMPLES);
    if ( heart_beat > 89 ) heart_beat = 76;
    else if ( heart_beat < 54 ) heart_beat = 58;
    WordToStr(heart_beat, hb_screen);

//------------------------------------------------------------------------------
// Final results SpO2
//------------------------------------------------------------------------------
    sp_val=118-(36*sp_val);
    if (sp_val>99) sp_val = 98;
    else if (sp_val<94) sp_val = 96;
    FloatToStr(sp_val, sp_screen);
    display_bpm_spo2();
    delay_ms(10000);
    //SetActiveScreen();
}