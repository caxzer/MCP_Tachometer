#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "utils/uartstdio.h"
#include "driverlib/pin_map.h"
#include "inc/hw_ints.h"

// Sub-modules
#include "tacho.h"
#include "display.h"

// Macros
#define WINDOW_MS 100
#define DISPLAY_WINDOW_MS 50
#define MOTOR_S1 GPIO_PIN_0
#define MOTOR_S2 GPIO_PIN_1
#define MOTOR_PORT GPIO_PORTP_BASE

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

// Global variables
uint32_t sysclk;
uint32_t window_timer_period;
uint32_t display_timer_period;
volatile bool update_display = false;

// Function prototype declarations
void init_clock(void);
void init_uart(void);
void init_timer(void);
void init_timer_interrupt(void);

void init_clock(void){
    // Configure 120Mhz clock from 25Mhz crystal and PLL
    sysclk = SysCtlClockFreqSet(
    SYSCTL_OSC_MAIN | SYSCTL_XTAL_25MHZ | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240,
    120000000);
}

void init_timer(void){
    // Timer for window
    // Enable Timer 0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    // Wait for Timer 0 to be ready
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0)){};
    // Enable Timer 1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    // Wait for Timer 1 to be ready
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1)){};
    window_timer_period = sysclk / (1000 / WINDOW_MS);
    display_timer_period = sysclk / (1000 / DISPLAY_WINDOW_MS);
}

void init_uart(void){
    // Enabe UART
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0)) {}

    // Configure UART
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);

    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, sysclk);
}

void display_interrupt_handler(void){
    // Clear interrupt flag 
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    update_display = true;
}

void display_timer_interrupt(void){

    TimerDisable(TIMER1_BASE, TIMER_A);

    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC); // 32-bit res
    TimerLoadSet(TIMER1_BASE, TIMER_A, display_timer_period - 1);

    // Register interrupt
    TimerIntRegister(TIMER1_BASE, TIMER_A, display_interrupt_handler);
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    
    // Interrupt enable
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    
    // NVIC on timer 1A
    IntPrioritySet(INT_TIMER1A, 0x20); // Prio 2 (Most sig. 3 bits)
    IntEnable(INT_TIMER1A);
    TimerEnable(TIMER1_BASE, TIMER_A);
}



int main(void)
{
    // Setup phase
    init_clock();                   // Initialise system clock
    init_uart();                    // Setup UART connection to PC for Debugging
    init_motor_ports_interrupts();  // Setup ports for motors and enable their interrupts
    init_timer();                   // Setup timer
    IntMasterEnable();              // Crucial: EN NVIC for whole board
    init_timer_interrupt();         // Enable interrupts for timer0 - window
    display_timer_interrupt();      // Enable interrupts for timer1 - display
    init_ports_display();           // Init Port L for Display Control and Port M for Display Data
	configure_display_controller_large();  // initalize and configuration

    // Check for UART functionality, startup message
    UARTprintf("KMZ60 RPM Measurement started. \n");
    
    // Clear screen with black background
    reset_background();
    draw_bresenham_ticks();
    draw_arc();  // bresenham half arc

    // Loop Forever
    while(1)
    {   
        if(calc_flag){ // move calc flag to here, so we get a speed update every 100ms
            calc_speed_dir(); //delivers us a value every 100ms
            
            /*Odometer*/
            draw_odometer(distance_total);
            
            /*Direction indicator */
            draw_direction(directionForwards);
        }
        
        //draw_needle_gauge(speed);
        if(update_display){ // update every 20ms-ish
            /*Draw needle with bresenham algo*/
            draw_bresenham(speed);
            draw_bresenham_ticks(); // draw the numbers!
            update_display = false;
        }
        
    }
}
