#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "utils/uartstdio.h"
#include "driverlib/timer.h"
#include "inc/hw_ints.h"

#include "interrupt.h"

// Macros 
#define MOTOR_S1 GPIO_PIN_0
#define MOTOR_S2 GPIO_PIN_1
#define MOTOR_PORT GPIO_PORTP_BASE
#define WINDOW_PERIOD 100   // millisecond
//#define CIRCUMFERENCE 0.444f // Circumference of motor wheel on the board, adjust according to max speed!
#define CIRCUMFERENCE 0.5f

// Global variables
volatile uint32_t edgeCountWindowS1 = 0; // Number of pulses in this time frame
volatile float rpm = 0.0f;
volatile bool directionForwards = true;
volatile bool prevS1 = false, prevS2 = false, thisS1 = false, thisS2 = false; // Signal flag for direction calculation
volatile uint32_t count = 0;
volatile uint32_t speed = 0;
volatile bool calc_flag = false;
volatile float distance_total = 0.0f;
bool max_dist_reached = false;

// Inputs from motors at Port P
void init_motor_ports_interrupts(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOP)){};

    // Set pins 1 and 0 as input, SW controlled
    GPIOPinTypeGPIOInput(MOTOR_PORT, MOTOR_S1 | MOTOR_S2);

    // Set Interrupt type
    GPIOIntTypeSet(MOTOR_PORT, MOTOR_S1 | MOTOR_S2, GPIO_BOTH_EDGES);

    // Clear pending interrupts
    GPIOIntClear(MOTOR_PORT, GPIOIntStatus(MOTOR_PORT, false)); 

    // Define Interupt handler for this PORT 
    GPIOIntRegister(MOTOR_PORT, motor_interrupt_handler); // NOTE: function pointer passed, not function itself!

    // Enable interrupt
    GPIOIntEnable(MOTOR_PORT, MOTOR_S1|MOTOR_S2);

    // NVIC on Port P0 aka. signal S1
    IntEnable(INT_GPIOP0);
    IntPrioritySet(INT_GPIOP0, 0x0); // Prio 1 (Most sig. 3 bits)

}

void motor_interrupt_handler(){
    // Read the Raw Interrupt Status directly
    // Get current interrupt status, masked interrupt to prevent triggering during handler
    uint32_t stat = GPIOIntStatus(MOTOR_PORT,true);     

    if (stat & MOTOR_S1){   // Interrupt by s1
        if(GPIOPinRead(MOTOR_PORT, MOTOR_S1)){  // Rising edge
            edgeCountWindowS1++;
            thisS1 = true;
        } else thisS1 = false;  // Falling edge
    } else {
        if(GPIOPinRead(MOTOR_PORT, MOTOR_S2)){  // Rising edge 
            thisS2 = true;
        } else thisS2 = false;  // Falling edge
    }

    GPIOIntClear(MOTOR_PORT, stat);    // Clear current interrupt

    if(((prevS1 == true && thisS1 == false) && (prevS2 == true && thisS2 == true)) ||
        ((prevS1 == false  && thisS1 == false) && (prevS2 == true && thisS2 == false)) ||
        ((prevS1 == false  && thisS1 == true) && (prevS2 == false && thisS2 == false)) ||
        ((prevS1 == true  && thisS1 == true) && (prevS2 == false && thisS2 == true)) ){
            directionForwards = true;
    } else directionForwards = false;

    prevS1 = thisS1;
    prevS2 = thisS2;
}

// Periodic timer with interrupt
void init_timer_interrupt(void){
    TimerDisable(TIMER0_BASE, TIMER_A);

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, window_timer_period - 1);

    // Register interrupt
    TimerIntRegister(TIMER0_BASE, TIMER_A, timer_interrupt_handler);
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    
    // Interrupt enable
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    
    // NVIC on timer 0A
    IntEnable(INT_TIMER0A);
    IntPrioritySet(INT_TIMER0A, 0x20); // Prio 2 (Most sig. 3 bits)

    TimerEnable(TIMER0_BASE, TIMER_A);
}

// Once the window period of 100 ms has been reached, timer interrupt !
void timer_interrupt_handler(void){
    // Clear interrupt flag 
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    count = edgeCountWindowS1;

    edgeCountWindowS1 = 0;
    calc_flag = true;
}

void calc_speed_dir(){
    rpm = (count / 0.1f ) * (60.0f / 2.0f); // number of revolutions per minute, 
    float speed_f = rpm * CIRCUMFERENCE * 0.06f;    // taking average within 100ms

    //UARTprintf("RPM: %.2f, Speed: %.2f, Direction: %s\n", rpm, speed, directionForwards ? "V":"R"); // debug
    int rpm_i = (int)rpm;
    speed = (int)(speed_f * 100); // for two decimals in kmh !!100 MULTIPLE HERE!!

    // Calculate distance travelled
    float delta_distance = 0;
    delta_distance = (speed_f / 3600.0f) * 0.1f;  //km
    
    if (!max_dist_reached) {  // only able to set this back to 0 with reset
        if (distance_total >= 999.99) {
            max_dist_reached = true;
            distance_total = 999.99;
        } else distance_total += delta_distance; 
    }

    int distance_int = (int)distance_total;                // whole km
    int distance_frac = (int)((distance_total - distance_int) * 100); // two decimals

    // debug
    UARTprintf( "RPM: %d, Speed: %d.%02d km/h, Direction: %s, Distance: %03d,%02d km\n",
        rpm_i,
        speed / 100,
        speed % 100,
        directionForwards ? "V" : "R",
        distance_int,
        distance_frac);

    // Reset my calculation begin flag
    calc_flag = false;
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
    IntPrioritySet(INT_TIMER1A, 0x40); // Prio 3 (Most sig. 3 bits)
    IntEnable(INT_TIMER1A);
    TimerEnable(TIMER1_BASE, TIMER_A);
}

void display_interrupt_handler(void){
    // Clear interrupt flag 
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    update_display = true;
}
