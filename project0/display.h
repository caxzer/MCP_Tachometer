#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

// Display constants
#define MAX_X 800
#define MAX_Y 480

// Color enum
enum colors {BLACK=0x00000000, WHITE=0x00FFFFFF, GREY=0x00AAAAAA, RED=0x00FF0000, GREEN=0x0000FF00, BLUE=0x000000FF, YELLOW=0x00FFFF00};

// Needle gauge globals
extern uint32_t gauge_x;
extern uint32_t gauge_y;
extern uint32_t gauge_width;
extern uint32_t gauge_height;
extern uint32_t max_speed;
extern volatile uint32_t speed;

// Display functions
void init_ports_display(void);
void configure_display_controller_large(void);
void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void draw_pixel(uint32_t min_x, uint32_t min_y, uint32_t max_x, uint32_t max_y, uint32_t color);
void draw_pixel_single(uint32_t x, uint32_t y, uint32_t color);
void draw_needle_gauge(uint32_t speed, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void draw_gauge_ticks(void);

#endif
