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

// Macros
#define WINDOW_MS        100
#define WINDOW_TIMER_PERIOD 1000

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
    window_timer_period = sysclk / (1000 / WINDOW_MS);
}

// Down counter timer with interrupt
void init_timer_interrupt(void){
    TimerDisable(TIMER0_BASE, TIMER_A);

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, window_timer_period - 1);

    // Register interrupt
    TimerIntRegister(TIMER0_BASE, TIMER_A, timer_interrupt_handler);
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    
    // Interrupt enable
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER0A);

    TimerEnable(TIMER0_BASE, TIMER_A);
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

int main(void)
{
    // Setup phase
    init_clock();                   // Initialise system clock
    init_uart();                    // Setup UART connection to PC for Debugging
    init_motor_ports_interrupts();  // Setup ports for motors and enable their interrupts
    init_timer();                   // Setup timer
    IntMasterEnable();              // Crucial: EN NVIC for whole board
    init_timer_interrupt();         // Enable interrupts for timer
    
    // Check for UART functionality, startup message
    UARTprintf("KMZ60 RPM Measurement started. \n");
    
    // Loop Forever
    while(1)
    {   
        calc_speed_dir();   // Introduce delay?
    }
}
