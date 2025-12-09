#include <stdint.h>
#include <stdbool.h> // type bool for giop.h
#include <string.h>
#include <math.h>
#include "inc/hw_types.h"
#include "inc/tm4c1294ncpdt.h"
#include <stdio.h>   // Debug only
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>     // GPIO_PIN_X
#include <inc/hw_memmap.h>      // GPIO_PORTX_BASE

#include "display.h"

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
// Std character sizes
#define CHAR_HEIGHT 12  
#define CHAR_WIDTH 8

#define GAUGE_WIDTH 800         // Gauge spans 800 pixels (2 per speed unit)
#define GAUGE_HEIGHT 240        // From which position the height of the gauge starts
#define NEEDLE_HEIGHT 200       // Height of needle
#define TICK_HEIGHT 5           // Height of gauge tick

#define SPEED_CHAR_HEIGHT 20    // Height of number characters beneath the gauge lines
#define ODO_CHAR_HEIGHT 80      // Height of number characters of the Odometer and Direction

#define XDIR 394
#define YDIR 310

#define XODO 364
#define YODO 330

#define XTSPD 382
#define YTSPD 145

#define MAX_SPEED 400
#define CENTER_POINT_X 400
#define CENTER_POINT_Y 280
#define OUTER_ARC_RAD 270
#define INNER_ARC_RAD 255
#define NEEDLE_LENGTH 245
#define NUM_TICKS 40            // Tick every 10 km/h
#define SHORT_TICK 5
#define LONG_TICK 15
#define SPEED_STEP 10


/********************************************************************************/
// Global Variables 
/********************************************************************************/
// uint32_t sysClock; // Variable for system clock  
// some predefined basic colors to use with names 
enum colors{BLACK=0x00000000,WHITE=0x00FFFFFF,GREY=0x00AAAAAA,RED=0x00FF0000,GREEN=0x0000FF00,BLUE=0x000000FF,YELLOW=0x00FFFF00}; 
// same values as array for indexed colors
int colorarray[]={0x00000000,0x00FFFFFF,0x00AAAAAA,0x00FF0000,0x0000FF00,0x000000FF,0x00FFFF00};
static bool prev_dir = 0;
int prev_x1 = 0;
int prev_y1 = 0;

/********************************************************************************/
// Pixel map of digits
/********************************************************************************/

// Bitmap number 0-9
static const uint8_t digit_tacho[10][12] = {    // 12x8 pixels
    {0x3C,0x7E,0xE7,0xC3,0xC3,0xC3,0xC3,0xC3,0xE7,0x7E,0x3C,0x00},    
    {0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E,0x00},    
    {0x3C,0x7E,0xC7,0x06,0x0E,0x1C,0x38,0x70,0xE0,0xFF,0xFF,0x00},    
    {0x7E,0xFF,0x83,0x07,0x1E,0x1E,0x07,0x03,0x83,0xFF,0x7E,0x00},    
    {0x0E,0x1E,0x3E,0x6E,0xCE,0x8E,0xFF,0xFF,0x0E,0x0E,0x0E,0x00},    
    {0xFE,0xFE,0xC0,0xC0,0xFC,0xFE,0x07,0x03,0xC3,0xFE,0x7C,0x00},    
    {0x3E,0x7E,0xE0,0xC0,0xFC,0xFE,0xE7,0xC3,0xE7,0x7E,0x3C,0x00},   
    {0xFF,0xFF,0x03,0x07,0x0E,0x1C,0x38,0x70,0x70,0x70,0x70,0x00},  
    {0x3C,0x7E,0xE7,0xE7,0x7E,0x3C,0x7E,0xE7,0xE7,0x7E,0x3C,0x00},
    {0x3C,0x7E,0xE7,0xC3,0xE7,0x7F,0x3F,0x07,0x0F,0x7E,0x7C,0x00}
};

// Bitmap direction indicator : V / R
static const uint8_t char_dir[2][12] = {    // 12x8 pixels
    {0xFC,0x82,0x82,0x82,0xFC,0x88,0x84,0x82,0x82,0x00,0x00,0x00}, // R
    {0x81,0x81,0x81,0x81,0x42,0x42,0x24,0x24,0x18,0x18,0x00,0x00}  // V
};

// Bitmap :  KM/H
static const uint8_t char_kmh[4][12] = {
    {0x81,0x82,0x84,0x88,0x90,0xE0,0x90,0x88,0x84,0x82,0x81,0x00}, // K
    {0x81,0xC3,0xA5,0x99,0x89,0x81,0x81,0x81,0x81,0x81,0x81,0x01}, // M
    {0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x00,0x00,0x00}, // /
    {0x81,0x81,0x81,0x81,0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0x00}  // H
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

void reset_background(void){    // for main to call
    fill_rect(0, 0, MAX_X, MAX_Y, BLACK);
}

// same as fillrect(), but with defined end pixel coordinate
void draw_pixels(uint32_t min_x, uint32_t min_y, uint32_t max_x, uint32_t max_y, uint32_t color){
    window_set(min_x, min_y, max_x, max_y);
    
    uint32_t width  = (max_x - min_x + 1);
    uint32_t height = (max_y - min_y + 1);
    uint32_t total  = width * height;
    uint32_t i = 0;

    write_command(0x2C);

    // loop to fill entire window with color
    for(i = 0; i < total; i++){
        write_data((color >> 16) & 0xFF);
        write_data((color >> 8) & 0xFF);
        write_data(color & 0xFF);
    }
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
/*
void draw_needle_gauge(uint32_t speed){
    uint32_t this_needle;

    // Find position of the needle w.R.t gauge width
    speed = speed /100; // Watch out: Speed is in factor of 100 here
    if (speed>MAX_SPEED) speed = MAX_SPEED;
    this_needle = (speed * GAUGE_WIDTH) / MAX_SPEED;

    // Clear previous needle position only if position has changed!
    if (prev_needle != this_needle)
        draw_pixels(prev_needle, GAUGE_HEIGHT, prev_needle, GAUGE_HEIGHT+NEEDLE_HEIGHT-1, BLACK);

    
    // THEN draw the current needle  
    draw_pixels(this_needle, GAUGE_HEIGHT, this_needle, GAUGE_HEIGHT+NEEDLE_HEIGHT-1, RED);
    
    prev_needle = this_needle; // save current needle position
}
*/

// Draw a single digit 
void draw_digit_tacho(int digit, int x, int y, uint32_t color) {
    if (digit < 0 || digit > 9) return;
    int row=0;
    int col=0;
    // draw line by line
    for ( row = 0; row < CHAR_HEIGHT; row++) {
        uint8_t bits = digit_tacho[digit][row];
        for ( col = 0; col < CHAR_WIDTH; col++) {
            if (bits & (1 << (7 - col))) {
                draw_pixel_single(x + col, y + row, color);
            }
        }
    }
}

// Draw the ticks and speed number on the tachometer gauge
/*
void draw_gauge_ticks(void) {
    const int step = 20; // each step equals to 10 km/h
    int value = 0;
    int j = 0;
    int x = 0;

    // draw along the whole width of the display
    for (value = 0; value < MAX_X; value += step) {
        x = value; // start from edge 
        
        if (value%50 == 0 || value == 0  ){ // draw large tick
            for (j = 0; j < TICK_HEIGHT*3; j++) {
                draw_pixel_single(x, MAX_Y - TICK_HEIGHT*3 - SPEED_CHAR_HEIGHT*2 + j, WHITE);
            }
            // show number under large ticks
            // display the speed numbers below the ticks
            draw_number_tacho(value/2,x,(int)MAX_Y - TICK_HEIGHT*3-SPEED_CHAR_HEIGHT,WHITE);
        }
        else {  // small ticks
            for (j = 0; j < TICK_HEIGHT; j++) {
                draw_pixel_single(x, MAX_Y - TICK_HEIGHT - SPEED_CHAR_HEIGHT*2 + j, WHITE);
            }
        }
    }

    // draw last tick for 400kmh
    for (j = 0; j < TICK_HEIGHT*3; j++)
        draw_pixel_single(MAX_X-1, MAX_Y - TICK_HEIGHT*3 - SPEED_CHAR_HEIGHT*2 + j, WHITE);
    draw_number_tacho(value/2,x-10,(int)MAX_Y - TICK_HEIGHT*3-SPEED_CHAR_HEIGHT,WHITE); // 30 == length of numbers

}
*/

// --- ODOMETER ---
void draw_char_km(int km, int x, int y, uint32_t color) {
    int row=0;
    int col=0;
    // draw row by row of digit bitmap
    for ( row = 0; row < 12; row++) {
        uint8_t bits = char_kmh[km][row];
        for ( col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                draw_pixel_single(x + col, y + row, color);
            }
        }
    }
}

void draw_odometer(float distance){
    int distance_int = (int)distance;
    int distance_frac = (int)((distance-distance_int) *100); // 2 d.p.

    char int_buf[4];
    char frac_buf[3];

    // Format data to buffer
    if (distance_int > 999) distance_int = 999; // maximal distance set to 999 (3 integers)
    
    snprintf(int_buf, sizeof(int_buf), "%03d", distance_int);
    snprintf(frac_buf, sizeof(frac_buf), "%02d", distance_frac);

    int total_width = (strlen(int_buf) + strlen(frac_buf) + 1) * 9; // +1 for comma, 9 for width of char and 1 pixel for spacing
    fill_rect(XODO, YODO, (uint32_t)total_width, (uint32_t)CHAR_HEIGHT, BLACK); // clear area

    // integer part
    int cursor = XODO;
    int i = 0;
    int j = 0;
    for (i = 0; int_buf[i] != '\0'; i++) {
        int digit = int_buf[i] - '0';
        draw_digit_tacho(digit, cursor, YODO, WHITE);
        cursor += 9; // spacing
    }

    // draw comma
    for (j = 0; j < 2; j++) {
        draw_pixel_single(cursor + 2, YODO + 10 + j, WHITE); // two-pixel comma
    }
    cursor += 8;

    // fractional part
    for (i = 0; frac_buf[i] != '\0'; i++) {
        int digit = frac_buf[i] - '0';
        draw_digit_tacho(digit, cursor, YODO, WHITE);
        cursor += 9;
    }
    cursor += 8;
    
    // draw km
    for (j=0 ; j <2 ; j++){
        draw_char_km(j, cursor, YODO, WHITE);
        cursor += 9;
    }
}

// --- DIRECTION ---
void draw_char_dir(int dir_f, int x, int y, uint32_t color) {
    int row=0;
    int col=0;
    // draw row by row of digit bitmap
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
        fill_rect(XDIR, YDIR, 20, 20, BLACK);
        if(directionForwards)   // forwards
            draw_char_dir(directionForwards, XDIR, YDIR, WHITE);
        else    // backwards
            draw_char_dir(directionForwards, XDIR, YDIR, WHITE);
    }
    prev_dir = directionForwards;
}

// --- TACHOMETER ---
/*STANDARD BRESENHAM EQUATION*/
void bresenham(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; /* error value e_xy */

    while (1) {
        draw_pixel_single(x0, y0,color);
        if (x0 == x1 && y0 == y1) break;    /*reached last pixel coord*/
        e2 = 2 * err;
        if (e2 > dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
        if (e2 < dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
    }
}

// draw outside border arc 
void rasterArc(int x0, int y0, int radius)
{
    int f = 1 - radius;
    int ddF_x = 0;
    int ddF_y = -2 * radius;
    int x = 0;
    int y = radius;

    draw_pixel_single(x0, y0 + radius,YELLOW);  // Top
    //draw_pixel_single(x0, y0 - radius,YELLOW);  // Bottom
    draw_pixel_single(x0 + radius, y0,YELLOW);  // Left
    draw_pixel_single(x0 - radius, y0,YELLOW);  // Right

    while(x < y)
    {
        if (f >= 0)
        {
            y -= 1;
            ddF_y += 2;
            f += ddF_y;
        }
        x += 1;
        ddF_x += 2;
        f += ddF_x + 1;

        //draw_pixel_single(x0 + x, y0 + y,YELLOW);   // Bottom-right
        //draw_pixel_single(x0 - x, y0 + y,YELLOW);   // Bottom-left
        draw_pixel_single(x0 + x, y0 - y,YELLOW);   // Top-right
        draw_pixel_single(x0 - x, y0 - y,YELLOW);   // Top-left
        draw_pixel_single(x0 + y, y0 + x,YELLOW);   // Right-bottom
        draw_pixel_single(x0 - y, y0 + x,YELLOW);   // Left-bottom
        draw_pixel_single(x0 + y, y0 - x,YELLOW);   // Right-top
        draw_pixel_single(x0 - y, y0 - x,YELLOW);   // Left-top
    }
}

void bresenham_needle(int x0, int y0, uint32_t speed){
    speed = speed /100; // Watch out: Speed is in factor of 100 here
    double s = (double)speed;

    if (s>(double)MAX_SPEED) s = (double)MAX_SPEED; 
    int r = NEEDLE_LENGTH;    // length of needle (almost) touching the arc
    
    // Find angle to the length of the arc ()
    double angle = 0.0f;
    angle = (5.0/4.0)*M_PI - ((s/(double)MAX_SPEED) * (3.0/2.0)*M_PI); // adjust to 270 deg or 3/2*M_PI
    int x1 = x0 + (int) r * cos(angle);
    int y1 = y0 - (int) r * sin(angle);
    
    // Clear previous needle pixel position if changed
    if (prev_x1 != x1 || prev_y1 != y1)
        bresenham(x0,  y0,  prev_x1,  prev_y1, BLACK);
    
    // Draw new needle 
    bresenham(x0,  y0,  x1,  y1, YELLOW);
 
    // save old needle position
    prev_x1 = x1;
    prev_y1 = y1;
}

// Draw a series of digits. x & y are text anchor at top left
void draw_number_tacho(int number, int x, int y, uint32_t color){
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", number); // speed int to string

    int i = 0;
    // cycle through the digits
    while (buf[i] != '\0') {
        int digit = buf[i] - '0';
        draw_digit_tacho(digit, x + (i * (CHAR_WIDTH + 1)), y, color); // 8 pixels + 1 pixel spacing
        i++;
    }
}

void bresenham_ticks(int x0, int y0){    // at r = 260, short = 5, long = 15 
    double start_angle = (5.0/4.0) * M_PI;
    double end_angle = -(1.0/4.0) * M_PI;

    int i;
    for (i = 0; i <= NUM_TICKS ; i++){
        // Determine angle 
        double tick_ratio = (double) i / (double) NUM_TICKS; 
        double angle = start_angle + tick_ratio * (end_angle - start_angle);
        
        // Find start and end coord of tick
        int x_out = x0 + (int) OUTER_ARC_RAD * cos(angle);
        int y_out = y0 - (int) OUTER_ARC_RAD * sin(angle);

        int x_in , y_in = 0;

        if( i % 5 == 0 || i == 0) { // long tick
            x_in = x0 + (int) (OUTER_ARC_RAD-LONG_TICK) * cos(angle);
            y_in = y0 - (int) (OUTER_ARC_RAD-LONG_TICK) * sin(angle);

            // speed number , only by long ticks
            int speed_value = i * SPEED_STEP;

            float r_text = OUTER_ARC_RAD - LONG_TICK - 25;   // adjust 25 px inward
            float px = x0 + r_text * cos(angle);
            float py = y0 - r_text * sin(angle);

            // Determine coordinate where number spawns
            float tx , ty  = 0 ;

            if ( angle < M_PI/6 )  
            {
                // RIGHT side
                tx = px - (3) * CHAR_WIDTH;
                ty = py - CHAR_HEIGHT/2;
            }
            else if (angle >= M_PI/6 && angle <= 5*M_PI/6)
            {
                // TOP arc
                tx = px - CHAR_WIDTH;
                ty = py + CHAR_HEIGHT/4;
            }
            else
            {
                // LEFT side
                tx = px + CHAR_WIDTH/4;
                ty = py - CHAR_HEIGHT/2;
            }

            draw_number_tacho(speed_value, tx,ty,WHITE);

        }
        else{   // short tick
            x_in = x0 + (int) (OUTER_ARC_RAD-SHORT_TICK) * cos(angle);
            y_in = y0 - (int) (OUTER_ARC_RAD-SHORT_TICK) * sin(angle);
        }

        bresenham(x_out, y_out, x_in, y_in, YELLOW);

    }

    // km/h at 90°
    int j = 0;
    int cursor = 0;
    for (j=0 ; j <4 ; j++){
        draw_char_km(j, XTSPD + cursor, YTSPD, WHITE);
        cursor += 9;
    }
}



/*For call from main*/
void draw_arc(void){
    rasterArc(CENTER_POINT_X, CENTER_POINT_Y, OUTER_ARC_RAD);
}

void draw_bresenham(uint32_t speed){
    bresenham_needle(CENTER_POINT_X, CENTER_POINT_Y, speed);
}

void draw_bresenham_ticks(void){
    bresenham_ticks(CENTER_POINT_X, CENTER_POINT_Y);
}
