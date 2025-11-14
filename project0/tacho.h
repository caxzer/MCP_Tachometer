#ifndef TACHO_H_
#define TACHO_H_

#include <stdint.h>
#include <stdbool.h>

void init_motor_ports_interrupts(void);
void motor_interrupt_handler(void);
void timer_interrupt_handler(void);
void calc_speed_dir(void);

extern volatile float rpm;
extern volatile bool directionForwards;
extern volatile uint32_t speed;
extern volatile float distance_total;


#endif
