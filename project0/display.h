#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

// Display functions
void init_ports_display(void);
void configure_display_controller_large(void);

void reset_background(void);

void draw_needle_gauge(uint32_t speed);
void draw_gauge_ticks(void);

void draw_odometer(float distance);
void draw_direction(bool directionForwards);

void draw_arc(void);
void draw_bresenham(uint32_t speed);
void draw_bresenham_ticks(void);

#endif
