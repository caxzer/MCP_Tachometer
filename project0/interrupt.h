#ifndef INTERRUPT_H_
#define INTERRUPT_H_

#include <stdint.h>
#include <stdbool.h>

// Prototype declarations
void init_motor_ports_interrupts(void);
void motor_interrupt_handler(void);
void init_timer_interrupt(void);
void timer_interrupt_handler(void);
void calc_speed_dir(void);
void display_timer_interrupt(void);
void display_interrupt_handler(void);


// Variable declarations
extern volatile float rpm;
extern volatile bool directionForwards;
extern volatile uint32_t speed;
extern volatile float distance_total;
extern uint32_t window_timer_period;
extern uint32_t display_timer_period;
extern uint32_t warning_timer_period;
extern volatile bool calc_flag;
extern volatile bool update_display;
extern volatile bool warning_flag; // no handler, only deliver flag to display module

#endif
