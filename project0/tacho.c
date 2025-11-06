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


// Macros 
#define MOTOR_S1 GPIO_PIN_0
#define MOTOR_S2 GPIO_PIN_1
#define MOTOR_PORT GPIO_PORTP_BASE
#define WINDOW_PERIOD 100   // millisecond
#define CIRCUMFERENCE 0.444f // Circumference of motor wheel on the board, adjust according to max speed!

// Global variables
volatile uint32_t edgeCountWindowS1 = 0; // Number of pulses in this time frame
volatile uint32_t edgeCountWindowS2 = 0;
volatile float rpm = 0.0f;
volatile bool directionForwards = true;
volatile bool prevS1 = false, prevS2 = false, thisS1 = false, thisS2 = false; // Signal flag for direction calculation
uint32_t count = 0;
uint32_t speed = 0;

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

    // Clear pending interrupts
    GPIOIntClear(MOTOR_PORT, GPIOIntStatus(MOTOR_PORT, false)); 

    // Define Interupt handler for this PORT 
    GPIOIntRegister(MOTOR_PORT, motor_interrupt_handler); // NOTE: function pointer passed, not function itself!

    // Enable interrupt
    GPIOIntEnable(MOTOR_PORT, MOTOR_S1|MOTOR_S2);

    IntEnable(INT_GPIOP0);
}

void motor_interrupt_handler(){
    // Get interrupt status for PORT P 
    uint32_t stat = GPIOIntStatus(MOTOR_PORT,true);     // Get current interrupt status
    
    if (stat & MOTOR_S1){
        if(GPIOPinRead(MOTOR_PORT, MOTOR_S1)){  // Rising edge
            edgeCountWindowS1++;
            thisS1 = true;
        } else thisS1 = false;  // Falling edge
    }
    if (stat & MOTOR_S2){
        if(GPIOPinRead(MOTOR_PORT, MOTOR_S2)){  // Rising edge
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
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    count = edgeCountWindowS1;

    // One redundant signal?
    edgeCountWindowS1 = 0;
    edgeCountWindowS2 = 0;
}

void calc_speed_dir(){
    rpm = (count / 0.1f ) * (60.0f / 2.0f); // number of revolutions per minute
    float speed_f = rpm * CIRCUMFERENCE * 0.06f;

    //UARTprintf("RPM: %.2f, Speed: %.2f, Direction: %s\n", rpm, speed, directionForwards ? "V":"R"); 
    int rpm_i = (int)rpm;
    speed = (int)(speed_f * 100); // two decimals
    UARTprintf("RPM: %d, Speed: %d.%02d km/h, Direction: %s\n",rpm_i, speed / 100,speed % 100, directionForwards ? "V" : "R");
}



