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

// Sub-modules
#include "tacho.h"

// Macros
#define WINDOW_MS        100
#define WINDOW_TIMER_PERIOD (SysCtlClockGet() / (1000 / WINDOW_MS))

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

// Function prototype declarations
void init_clock(void);
void init_uart(void);
void init_timer(void);
void init_timer_interrupt(void);

void init_clock(void){
    uint32_t sysclk = SysCtlClockFreqSet(
    SYSCTL_OSC_MAIN | SYSCTL_XTAL_25MHZ | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480,
    120000000);
}

void init_timer(void){
    // Timer for System
    // Enable Timer 0
    //SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    // wait for Timer0 to be ready
    //while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0)){};

    // Timer for window
    // Enable Timer 1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    // Wait for Timer 1 to be ready
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1)){};
}

// Down counter timer with interrupt
void init_timer_interrupt(void){
    // Disable TIMER 1, use Timer B , reserve Timer A for display?
    TimerDisable(TIMER1_BASE, TIMER_B);

    TimerConfigure(TIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_B_PERIODIC);
    TimerLoadSet(TIMER1_BASE, TIMER_B, WINDOW_TIMER_PERIOD-1);

    // Register interrupt
    TimerIntRegister(TIMER1_BASE, TIMER_B, timer_interrupt_handler);
    TimerIntClear(TIMER1_BASE, TIMER_TIMB_TIMEOUT);
    
    // Interrupt enable
    TimerIntEnable(TIMER1_BASE, TIMER_TIMB_TIMEOUT);
    //IntEnable(INT_TIMER1B);

    TimerEnable(TIMER1_BASE, TIMER_B);
}

void init_uart(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, SysCtlClockGet());
}

int main(void)
{
    // Setup phase
    init_clock();
    init_uart();
    init_motor_ports_interrupts();
    init_timer();
    init_timer_interrupt();

    UARTprintf("KMZ60 RPM Measurement started. \n");
    printf("KMZ60 Initialization sequence...");

    // Loop Forever
    while(1)
    {   
        calc_speed_dir();
        /* EXAMPLE CODE
        // Turn on the LED
        GPIOPinWrite(GPIO_PORTN_BASE, (USER_LED1|USER_LED2), USER_LED1);
        // Delay for a bit
        SysCtlDelay(ui32SysClock/6);
        // Turn on the LED
        GPIOPinWrite(GPIO_PORTN_BASE, (USER_LED1|USER_LED2), USER_LED2);
        // Delay for a bit
        SysCtlDelay(ui32SysClock/6);
        */
    }
}
