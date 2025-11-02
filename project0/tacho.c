#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "utils/uartstdio.h"
#include "driverlib/timer.h"

// Macros 
#define MOTOR_S1 GPIO_PIN_0
#define MOTOR_S2 GPIO_PIN_1
#define MOTOR_PORT GPIO_PORTP_BASE
#define WINDOW_PERIOD 100   // millisecond
#define CIRCUMFERENCE 0.0188f // Circumference of motor wheel on the board, adjust according to max speed!

// Global variables
volatile uint32_t edgeCountWindowS1 = 0; // Number of pulses in this time frame
volatile uint32_t edgeCountWindowS2 = 0;
volatile float rpm = 0.0f;
volatile bool directionForwards = true;
volatile bool prevS1 = false, prevS2 = false, thisS1 = false, thisS2 = false; // Signal flag for direction calculation
uint32_t count = 0;

// convert timer ticks to seconds
/*static inline float ticks_to_second(uint32_t ticks){
    return ticks/SysCtlClockGet();
}*/

// Function prototype declarations
void init_motor_ports_interrupts(void);
void motor_interrupt_handler(void);
void timer_interrupt_handler(void);
void calc_speed_dir(void);




// Inputs from motors at Port P
void init_motor_ports_interrupts(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOP)){};

    // Set pins 1 and 0 as input, SW controlled
    GPIOPinTypeGPIOInput(MOTOR_PORT, MOTOR_S1 | MOTOR_S2);

    // Set Interrupt type
    GPIOIntTypeSet(MOTOR_PORT, MOTOR_S1 | MOTOR_S2, GPIO_BOTH_EDGES);

    // Define Interupt handler for this PORT 
    GPIOIntRegister(MOTOR_PORT, motor_interrupt_handler); // NOTE: function pointer passed, not function itself!

    // Enable interrupt
    GPIOIntEnable(MOTOR_PORT, MOTOR_S1|MOTOR_S2);

    //IntEnable(INT_GPIOP);
}

void motor_interrupt_handler(){
    // Get interrupt status for PORT P 
    uint32_t stat = GPIOIntStatus(MOTOR_PORT,true);     // Get current interrupt status
    
    if (stat & MOTOR_S1){
        if(GPIOPinRead(MOTOR_PORT, MOTOR_S1)){
            edgeCountWindowS1++;
            thisS1 = true;
        } else thisS1 = false;  // Falling edge
    }
    else{
        if(GPIOPinRead(MOTOR_PORT, MOTOR_S2)){
            edgeCountWindowS2++;
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

// Once the window period of 100 ms has been reached, timer interrupt !
void timer_interrupt_handler(void){
    // Clear interrupt flag 
    TimerIntClear(TIMER1_BASE, TIMER_TIMB_TIMEOUT);
    count = edgeCountWindowS1;

    // One redundant signal?
    edgeCountWindowS1 = 0;
    edgeCountWindowS2 = 0;
}

void calc_speed_dir(){
    rpm = (count / 0.1f ) * 2.0f;
    float speed = rpm * CIRCUMFERENCE * 0.06f;

    UARTprintf("RPM: %.2f, Speed: %.2f, Direction: %s\n", rpm, speed, directionForwards ? "V":"R"); 
    
}



