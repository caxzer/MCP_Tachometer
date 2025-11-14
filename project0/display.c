#include <stdint.h>
#include <stdbool.h> // type bool for giop.h
#include <string.h>
#include "inc/hw_types.h"
#include "inc/tm4c1294ncpdt.h"
#include <stdio.h>   // Debug only
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>     // GPIO_PIN_X
#include <inc/hw_memmap.h>      // GPIO_PORTX_BASE

// Macros/constants for display initialization
#define RST 0x10
#define INITIAL_STATE (0x1F)
#define SOFTWARE_RESET (0x01)
#define SET_PLL_MN (0xE2)
#define START_PLL (0xE0)
#define LOCK_PLL (0xE0)  // same as START_PLL
#define SET_LSHIFT (0xE6)
#define SET_LCD_MODE (0xB0)
#define SET_HORI_PERIOD (0xB4)
#define SET_VERT_PERIOD (0xB6)
#define SET_ADRESS_MODE (0x36)
#define SET_PIXEL_DATA_FORMAT (0xF0)
#define SET_DISPLAY_ON (0x29)
#define SET_DISPLAY_OFF (0x29) // not tested ?? 

// Display initialization
#define MAX_X 800
#define MAX_Y 480

/********************************************************************************/
// Gauge Macros
/********************************************************************************/
#define GAUGE_MIN 0
#define GAUGE_MAX 400
#define GAUGE_WIDTH 400        // gauge spans 400 pixels (1 per speed unit)
#define GAUGE_X 10              // pixel position from which the gauge line starts from left edge
#define GAUGE_Y 465             // pixel position from which the gauge lines start from top
#define GAUGE_HEIGHT 5          // Height of gauge tick

#define SPEED_CHAR_HEIGHT 20  // Height of number characters beneath the gauge lines
#define ODO_CHAR_HEIGHT 80  // Height of number characterof the Odometer


/********************************************************************************/
// Global Variables 
/********************************************************************************/
// uint32_t sysClock; // Variable for system clock  
// some predefined basic colors to use with names 
enum colors{BLACK=0x00000000,WHITE=0x00FFFFFF,GREY=0x00AAAAAA,RED=0x00FF0000,GREEN=0x0000FF00,BLUE=0x000000FF,YELLOW=0x00FFFF00}; 
// same values as array for indexed colors
int colorarray[]={0x00000000,0x00FFFFFF,0x00AAAAAA,0x00FF0000,0x0000FF00,0x000000FF,0x00FFFF00};
uint32_t gauge_x = 0;      // left margin
uint32_t gauge_y = 200;     // vertical postion
uint32_t gauge_width = 800; // length of horizontal "loading" gauge bar
uint32_t gauge_height = 200; // height of the horizontal "loading" gauge bar
uint32_t max_speed = 400;
static uint32_t prev_needle = 0;
static bool prev_dir = 0;

/********************************************************************************/
// Pixel map of digits
/********************************************************************************/

static const uint8_t digit_tacho[10][12] = {
    // 0
    {0x3C,0x7E,0xE7,0xC3,0xC3,0xC3,0xC3,0xC3,0xE7,0x7E,0x3C,0x00},
    // 1
    {0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E,0x00},
    // 2
    {0x3C,0x7E,0xC7,0x06,0x0E,0x1C,0x38,0x70,0xE0,0xFF,0xFF,0x00},
    // 3
    {0x7E,0xFF,0x83,0x07,0x1E,0x1E,0x07,0x03,0x83,0xFF,0x7E,0x00},
    // 4
    {0x0E,0x1E,0x3E,0x6E,0xCE,0x8E,0xFF,0xFF,0x0E,0x0E,0x0E,0x00},
    // 5
    {0xFE,0xFE,0xC0,0xC0,0xFC,0xFE,0x07,0x03,0xC3,0xFE,0x7C,0x00},
    // 6
    {0x3E,0x7E,0xE0,0xC0,0xFC,0xFE,0xE7,0xC3,0xE7,0x7E,0x3C,0x00},
    // 7
    {0xFF,0xFF,0x03,0x07,0x0E,0x1C,0x38,0x70,0x70,0x70,0x70,0x00},
    // 8
    {0x3C,0x7E,0xE7,0xE7,0x7E,0x3C,0x7E,0xE7,0xE7,0x7E,0x3C,0x00},
    // 9
    {0x3C,0x7E,0xE7,0xC3,0xE7,0x7F,0x3F,0x07,0x0F,0x7E,0x7C,0x00}
};

static const uint8_t char_dir[2][12] = {
    // R
    {0xFC,0x82,0x82,0x82,0xFC,0x88,0x84,0x82,0x82,0x00,0x00,0x00},
    // V
    {0x81,0x81,0x81,0x81,0x42,0x42,0x24,0x24,0x18,0x18,0x00,0x00}
    
};




/********************************************************************************
 	 Elementary output functions  => speed optimized as inline
*********************************************************************************/
inline void write_command(unsigned char command)
{ 	GPIO_PORTM_DATA_R = command;        // Write command byte
    GPIO_PORTL_DATA_R = 0x11;           // Chip select = 0, Command mode select = 0, Write state = 0
    GPIO_PORTL_DATA_R = 0x1F;           // Initial state
}
/********************************************************************************/
inline void write_data(unsigned char data)
{ 	GPIO_PORTM_DATA_R = data;           // Write data byte
    GPIO_PORTL_DATA_R = 0x15;           // Chip select = 0, Write state = 0
    GPIO_PORTL_DATA_R = 0x1F;           // Initial state
}
/********************************************************************************/
inline void window_set(min_x, min_y, max_x, max_y)
{
    write_command(0x2A);           // Set row address x-axis
    write_data(min_x >> 8);        // Set start  address           (high byte)
    write_data(min_x);             // as above                     (low byte)
    write_data(max_x >> 8);        // Set stop address             (high byte)
    write_data(max_x);             // as above                     (low byte) 
    write_command(0x2B);           // Set column address (y-axis)
    write_data(min_y >> 8);        // Set start column address     (high byte)
    write_data(min_y);             // as above                     (low byte)
    write_data(max_y >> 8);        // Set stop column address      (high byte)
    write_data(max_y);             // as above                     (low byte)
}
/********************************************************************************/
void init_ports_display(void)
{
	// Set Port M Pins 0-7: used as Output of LCD Data
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);            // enable clock-gate Port M
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOM));     // wait until clock ready
	GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, 0xFF);
	// Set Port L Pins 0-4: used as Output of LCD Control signals:
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);            // Clock Port L
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOL));
	GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3| GPIO_PIN_4);
}

void configure_display_controller_large (void) // 800 x 480 pixel 
{
//////////////////////////////////////////////////////////////////////////////////
    GPIO_PORTL_DATA_R = INITIAL_STATE;      // Initial state
    GPIO_PORTL_DATA_R &= ~RST;              // Hardware reset
    SysCtlDelay(10000);                     // wait >1 ms
    GPIO_PORTL_DATA_R |= RST;               //
    SysCtlDelay(12000);                     // wait >1 ms

    write_command(SOFTWARE_RESET);          // Software reset
    SysCtlDelay(120000);                    // wait >10 ms

    write_command(SET_PLL_MN);               // Set PLL Freq to 120 MHz
    write_data(0x24);                        //
    write_data(0x02);                        //
    write_data(0x04);                        //

    write_command(START_PLL);                // Start PLL
    write_data(0x01);                        //
    SysCtlDelay(10000);                      // wait 1 ms

    write_command(START_PLL);                // Lock PLL
    write_data(0x03);                        //
    SysCtlDelay(10000);                      // wait 1 ms

    write_command(SOFTWARE_RESET);           // Software reset
    SysCtlDelay(100000);                     // wait 10 ms

    write_command(0xe6);                    // Set pixel clock frequency
    write_data(0x01);                       // KRR Set LCD Pixel Clock 9MHz
    write_data(0x70);                       // KRR
    write_data(0xA3);                       // KRR

    write_command(SET_LCD_MODE);          // SET LCD MODE SIZE, manual p. 44
    write_data(0x20);                     // ..TFT panel 24bit
    write_data(0x00);                     // ..TFT mode
    write_data(0x03);                     // SET horizontal size = 800-1 (high byte)
    write_data(0x1F);                     // SET horizontal size = 800-1 (low byte)
    write_data(0x01);                     // Set vertical size = 480-1 (high byte)
    write_data(0xDF);                     // Set vertical size = 480-1 (low byte)
    write_data(0x00);                     // Even line RGB sequence / Odd line RGB sequence RGB

    write_command(SET_HORI_PERIOD);       // Set Horizontal Period
    write_data(0x03);                     // Horizontal total period (display + non-displayed)  (high byte)
    write_data(0x5E);                     // Horizontal total period (display + non-display) (low byte)
    write_data(0x00);                     // Non-displayed period between the start of the horizontal sync (LLINE) signal and the first displayed data.
    write_data(0x46);                     // Low byte of the non-display period between the start of the horizontal sync (LLINE) signal and the first display data
    write_data(0x09);                     // Set the vertical sync width
    write_data(0x00);                     // Set horiz.Sync pulse start    (high byte)
    write_data(0x08);                     // Set horiz.Sync pulse start    (low byte)
    write_data(0x00);                     //

    write_command(SET_VERT_PERIOD);         // Set vertical periods, manual  p. 49
    write_data(0x01);                       // Vertical total period (display + non-displayed) in lines (high byte)
    write_data(0xFE);                       // as above (low byte) = total 510  lines
    write_data(0x00);                       //
    write_data(0x0C);                       // The non-displayed period in lines between the start of the frame and the first
                                            // display data = 12 line.s
    write_data(0x0A);                       // Set the vertiacla sync width = 10 pixels
    write_data(0x00);                       // Set vertical sync pulse start position (high byte)
    write_data(0x04);                       // as above (low byte) = total sync pulse start position is 4 lines

    write_command(SET_ADRESS_MODE);         // Pixel address counting = flip display , manual p. 36
    write_data(0x03);                       // necessary to match with touch screen addressing

//  write_command(0x0A);                    // Power control mode not tested in detail
//  write_data(0x1C);

    write_command(SET_PIXEL_DATA_FORMAT);    // set pixel data format 8bit manual p. 78
    write_data(0x00);

    write_command(SET_DISPLAY_ON);           // Set display on  manual p. 78
}


/********************************************************************************/
// Tachometer Gauge here!
// Fill a rectangle with a single color and defined w&h with respect to starting coordinates 
void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    uint32_t i;
    uint32_t total_pixels;
    
    if (width == 0 || height == 0) return; // nothing to do
    total_pixels = width * height;

    // set the window to the whole rectangle
    window_set(x, y, x + width - 1, y + height - 1);
    write_command(0x2C); // Memory write
    for (i = 0; i < total_pixels; i++) {
        write_data((color >> 16) & 0xFF); // R
        write_data((color >> 8) & 0xFF);  // G
        write_data(color & 0xFF);         // B
    }
}

// Custom window size
void draw_pixel (uint32_t min_x, uint32_t min_y, uint32_t max_x, uint32_t max_y , uint32_t color){
    window_set(min_x, min_y, max_x, max_y);         // set row for one line
    write_command(0x2C);                    // start memory write
    write_data((color>>16)&0xff);   // R
    write_data((color>>8)&0xff);    // G
    write_data(color&0xff);         // B
}

// 1x1 window size
void draw_pixel_single(uint32_t x, uint32_t y, uint32_t color) {
    window_set(x, y, x, y); 
    write_command(0x2C);      // memory write
    write_data((color>>16)&0xFF);
    write_data((color>>8)&0xFF);
    write_data(color&0xFF);
}

// Needle gauge
void draw_needle_gauge(uint32_t speed){
    uint32_t this_needle;
    uint32_t i, j;

    // Find position of the needle w.R.t gauge width
    speed = speed /100; // Watch out: Speed in in factor of 100 here
    if (speed>max_speed) speed = max_speed;
    this_needle = (speed * gauge_width) / max_speed;

    // Clear previous needle position only if position has changed!
    if (prev_needle != this_needle)
        for(i = 0;i<gauge_width;i++){
            draw_pixel(prev_needle, 200+i, prev_needle, 200+i, BLACK);
        }
    for(j=0; j<gauge_height; j++){    // THEN draw the needle
        draw_pixel(this_needle, 200+j, this_needle, 200+j, RED);
    }
    
    // Save current needle position
    prev_needle = this_needle;
}

void draw_digit_tacho(int digit, int x, int y, uint32_t color) {
    if (digit < 0 || digit > 9) return;
    int row=0;
    int col=0;
    for ( row = 0; row < 12; row++) {
        uint8_t bits = digit_tacho[digit][row];
        for ( col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                draw_pixel_single(x + col, y + row, color);
            }
        }
    }
}

void draw_number_tacho(int number, int x, int y, uint32_t color){
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", number); // int to string

    int i = 0;
    while (buf[i] != '\0') {
        int digit = buf[i] - '0';
        draw_digit_tacho(digit, x + (i * 9), y, color); // 9 = 8 pixels + 1 spacing
        i++;
    }
}

// Draw the ticks and speed number on the tachometer gauge
void draw_gauge_ticks(void) {
    const int step = 20; // each step equals to 10 km/h
    int value = 0;
    int j = 0;
    int x = 0;

    // draw along the whle width of the display
    for (value = 0; value < MAX_X; value += step) {
        x = value; // start from edge 
        
        if (value%50 == 0 || value == 0  ){ // draw large tick
            for (j = 0; j < GAUGE_HEIGHT*3; j++) {
                draw_pixel_single(x, MAX_Y - GAUGE_HEIGHT*3 - SPEED_CHAR_HEIGHT*2 + j, WHITE);
            }
            // show number under large ticks
            // display the speed numbers below the ticks
            //if(value == 0)
                draw_number_tacho(value/2,x,(int)MAX_Y - GAUGE_HEIGHT*3-SPEED_CHAR_HEIGHT,WHITE);
            //else
                //draw_number_tacho(value/2,x - 5 ,(int)MAX_Y - GAUGE_HEIGHT*3-SPEED_CHAR_HEIGHT,WHITE);
        }
        else {  // small ticks
            for (j = 0; j < GAUGE_HEIGHT; j++) {
                draw_pixel_single(x, MAX_Y - GAUGE_HEIGHT - SPEED_CHAR_HEIGHT*2 + j, WHITE);
            }
        }
    }

    // draw last tick for 400kmh
    for (j = 0; j < GAUGE_HEIGHT*3; j++)
        draw_pixel_single(MAX_X-1, MAX_Y - GAUGE_HEIGHT*3 - SPEED_CHAR_HEIGHT*2 + j, WHITE);
    draw_number_tacho(value/2,x-10,(int)MAX_Y - GAUGE_HEIGHT*3-SPEED_CHAR_HEIGHT,WHITE); // 30 == length of numbers

}



void draw_odometer(float distance){
    int distance_int = (int)distance;
    int distance_frac = (int)((distance-distance_int) *100); // 2 d.p.

    char int_buf[8];
    char frac_buf[4];

    // Format data to buffer
    snprintf(int_buf, sizeof(int_buf), "%d", distance_int);
    snprintf(frac_buf, sizeof(frac_buf), "%02d", distance_frac);

    int total_width = (strlen(int_buf) + strlen(frac_buf) + 1) * 9; // +1 for comma
    int height = 16;  // approximate height of your digit bitmaps
    fill_rect(0, 0, (uint32_t)total_width, (uint32_t)height, BLACK); // clear area

    // integer part
    int cursor = 0;
    int i = 0;
    int j = 0;
    for (i = 0; int_buf[i] != '\0'; i++) {
        int digit = int_buf[i] - '0';
        draw_digit_tacho(digit, cursor, 0, WHITE);
        cursor += 9; // spacing
    }

    // draw comma
    for (j = 0; j < 2; j++) {
        draw_pixel_single(cursor + 2, 10 + j, WHITE); // two-pixel comma
    }
    cursor += 8;

    // fractional part
    for (i = 0; frac_buf[i] != '\0'; i++) {
        int digit = frac_buf[i] - '0';
        draw_digit_tacho(digit, cursor, 0, WHITE);
        cursor += 9;
    }
}

void draw_char_dir(int dir_f, int x, int y, uint32_t color) {
    int row=0;
    int col=0;
    for ( row = 0; row < 12; row++) {
        uint8_t bits = char_dir[dir_f][row];
        for ( col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                draw_pixel_single(x + col, y + row, color);
            }
        }
    }
}

void draw_direction(bool directionForwards){
    // clear if directions different
    if(directionForwards != prev_dir){
        fill_rect(770, 0, 20, 20, BLACK);
        
        if(directionForwards)   // forwards
            draw_char_dir(directionForwards, 770, 0, WHITE);
        else    // backwards
            draw_char_dir(directionForwards, 770, 0, WHITE);
    }
    prev_dir = directionForwards;
}

// Combination of all the functions here
void update_display(void){

}
