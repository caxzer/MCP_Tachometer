#include <stdint.h>
#include <stdbool.h> // type bool for giop.h
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

// Gauge Macros
#define GAUGE_MIN 0
#define GAUGE_MAX 400
#define GAUGE_WIDTH 400        // gauge spans 400 pixels (1 per speed unit)
#define GAUGE_X 10
#define GAUGE_Y 200
#define GAUGE_HEIGHT 5

// Global Variables 
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
// Progress bar gauge
/*void draw_horizontal_gauge(uint32_t speed, uint32_t x, uint32_t y, uint32_t width, uint32_t height){
    uint32_t filled = (speed * width) / 400; // pixels filled along gauge width with respect to speed
    uint32_t i, j;
    uint32_t color_filled = RED;
    uint32_t color_empty = GREY;

     for(j=0; j<height; j++){
        window_set(x, y+j, x+width-1, y+j);  // set row for one line
        write_command(0x2C);                 // start memory write
        for(i=0; i<width; i++){
            uint32_t color = (i < filled) ? color_filled : color_empty; // Bi-colored
            write_data((color>>16)&0xff);
            write_data((color>>8)&0xff);
            write_data(color&0xff);
        }
    }
}*/

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

void draw_pixel (uint32_t min_x, uint32_t min_y, uint32_t max_x, uint32_t max_y , uint32_t color){
    window_set(min_x, min_y, max_x, max_y);         // set row for one line
    write_command(0x2C);                    // start memory write
    write_data((color>>16)&0xff);   // R
    write_data((color>>8)&0xff);    // G
    write_data(color&0xff);         // B
}

void draw_pixel_single(uint32_t x, uint32_t y, uint32_t color) {
    window_set(x, y, x, y);   // set a 1x1 window
    write_command(0x2C);      // memory write
    write_data((color>>16)&0xFF);
    write_data((color>>8)&0xFF);
    write_data(color&0xFF);
}

// Needle gauge
void draw_needle_gauge(uint32_t speed, uint32_t x, uint32_t y, uint32_t width, uint32_t height){
    uint32_t this_needle;
    uint32_t i, j;

    // Find position of te needle
    if (speed>max_speed) speed = max_speed;
    this_needle = x + (speed * width) / max_speed;

    // Clear previous needle position only if position changed!
    if (prev_needle != 0 && prev_needle != this_needle)
        for(i = 0;i<width;i++){
            draw_pixel(prev_needle, y+i, prev_needle, y+i, BLACK);
            /*
            window_set(prev_needle, y+i, prev_needle, y+i);  // set row for one line
            write_command(0x2C);                 // start memory write
            uint32_t color = BLACK;
            write_data((color>>16)&0xff);   // R
            write_data((color>>8)&0xff);    // G
            write_data(color&0xff);         // B
            */
        }

    // Draw the needle 
     for(j=0; j<height; j++){
        draw_pixel(this_needle, y+j, this_needle, y+j, RED);
        /*
        window_set(this_needle, y+j, this_needle, y+j);  // set row for one line
        write_command(0x2C);                 // start memory write
        uint32_t color = RED;
        write_data((color>>16)&0xff);   // R
        write_data((color>>8)&0xff);    // G
        write_data(color&0xff);         // B
        */
        
    }

    // Save current needle position
    prev_needle = this_needle;
}
uint32_t map(uint32_t val, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max){
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void draw_scale(void){
    uint32_t step = 10;
    uint32_t val = 0;
    int j = 0;

    for (val = gauge_x ; val < gauge_width ; val+=step){
        uint32_t x = map(val, 0, max_speed, gauge_x, gauge_x + gauge_width);
        
        for (j = 0; j < GAUGE_HEIGHT; j++) {
            draw_pixel_single(x, GAUGE_Y + GAUGE_HEIGHT + j, WHITE);
        }

    }
}

void draw_gauge_ticks(void) {
    int step = 50; // draw tick marks every 50 units
    int tick_height = 5;
    int value = 0;
    int j = 0;
    for (value = GAUGE_MIN; value <= GAUGE_MAX; value += step) {
        int x = GAUGE_X + value; // one pixel per speed unit
        for (j = 0; j < tick_height; j++) {
            draw_pixel_single(x, GAUGE_Y + GAUGE_HEIGHT + j, WHITE);
        }
    }
}
