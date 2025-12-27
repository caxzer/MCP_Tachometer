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
#include "interrupt.h"
#include "display.h"

// Macros
#define WINDOW_MS 100
#define DISPLAY_WINDOW_MS 50
#define WARNING_LIMIT 30  // Seconds before warning light show up 
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
uint32_t warning_timer_period;
volatile bool update_display = false;

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

    // Enable Timer 2
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    // Wait for Timer 2 to be ready
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER2)){};

    window_timer_period = sysclk / (1000 / WINDOW_MS); // 100ms
    display_timer_period = sysclk / (1000 / DISPLAY_WINDOW_MS); // 50ms
    warning_timer_period = sysclk / (1000 / 10e3); // 10 seconds into ms
}

void init_uart(void){
    // Enabe UART for debug
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA)) {}

    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0)) {}

    // Configure UART
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, sysclk);
}

int main(void)
{
    // Setup phase
    init_clock();                   // Initialise system clock
    init_uart();                    // Setup UART connection to PC for Debugging
    init_timer();                   // Setup timer

    IntMasterDisable();              // Crucial: NVIC for whole board
    init_motor_ports_interrupts();  // Setup ports for motors and enable their interrupts
    init_timer_interrupt();         // Enable interrupts for timer0 - window
    display_timer_interrupt();      // Enable interrupts for timer1 - display
    IntMasterEnable();              // Allow interrupts for CPU 

    init_ports_display();           // Init Port L for Display Control and Port M for Display Data
	configure_display_controller_large();  // display init and configuration

    // Check for UART functionality, startup message
    UARTprintf("KMZ60 Measurement started. \n");
    
    // Clear screen with black background
    reset_background();
    
    // Bresenham arc
    draw_bresenham_ticks();
    draw_arc(); 

    // Loop Forever
    while(1)
    {   
        // Speed variable update every 100ms
        if(calc_flag){          
            calc_speed_dir(); 
            draw_odometer(distance_total);
            draw_direction(directionForwards);
        }
        
        // Display refresh every 20ms (50 Hz)
        if(update_display){
            /*Draw needle with bresenham algo*/
            draw_bresenham(speed);
            draw_bresenham_ticks(); // draw the numbers!
            update_display = false;
        }
        
    }
}
