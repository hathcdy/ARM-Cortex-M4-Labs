#include <stdint.h>
#include <stdbool.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"

#define IDLE 0
#define PRESS 1
#define RELEASE 2

int flag = IDLE;
int count = 0;

/*

* Function Name: switchPinConfig()

* Input: none

* Output: none

* Description: Set PORTF Pin 0 and Pin 4 as input. Note that Pin 0 is locked.

* Example Call: switchPinConfig();

*/
void switchPinConfig(void)
{
	// GPIO PORTF Pin 0 and Pin4
	GPIODirModeSet(GPIO_PORTF_BASE,GPIO_PIN_4,GPIO_DIR_MODE_IN); // Set Pin-4 of PORT F as Input. Modifiy this to use another switch
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4);
	GPIOPadConfigSet(GPIO_PORTF_BASE,GPIO_PIN_4,GPIO_STRENGTH_12MA,GPIO_PIN_TYPE_STD_WPU);
}

int main(void)
{
	uint32_t ui32Period;

	/*
	 * Configure the system clock to run at 40MHz¡£
	 */
	SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);

	/*
	 * Enable the GPIO peripheral and configure the pins connected to the LEDs as outputs.
	 */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);

	/*
	 * Before calling any peripheral specific driverLib function we must enable the clock to that peripheral.
	 */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	/*
	 * Configure Timer 0 as a 32-bit timer in periodic mode.
	 */
	TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

	ui32Period = (SysCtlClockGet() / 10) / 2;
	TimerLoadSet(TIMER0_BASE, TIMER_A, ui32Period -1);

	/*
	 * IntEnable enables the specific vector associated with Timer0A.
	 */
	IntEnable(INT_TIMER0A);
	/*
	 * TimerIntEnable, enables a specific event within the timer to generate an interrupt.
	 */
	TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	/*
	 * IntMasterEnable() is the master interrupt enable API for all interrupts.
	 */
	IntMasterEnable();

	/*
	 * Start the timer and interrupts will begin triggering on the timeouts.
	 */
	TimerEnable(TIMER0_BASE, TIMER_A);

	switchPinConfig();

	while(1)
	{
	}
}

void Timer0IntHandler(void)
{
	// Clear the timer interrupt
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

	if (flag == IDLE) {
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4)) {
			// key pressed
			flag = PRESS;
		}
	} else if (flag == PRESS) {
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4)) {
			// key still pressed
			flag = RELEASE;
		}
	} else {
		if (!GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4)) {
			// key released
			flag = IDLE;
			count++;
			if (count == 3) count = 0;
		}
	}

	if (count == 0) {
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 2);
	} else if (count == 1) {
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 8);
	} else {
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 4);
	}
//	if(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_2))
//	{
//		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0);
//	}
//	else
//	{
//		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 4);
//	}

}


