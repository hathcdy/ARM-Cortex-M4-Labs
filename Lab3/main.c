#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "inc/hw_gpio.h"
#include "driverlib/rom.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"

#define PWM_FREQUENCY 55
#define IDLE 0
#define PRESS 1
#define RELEASE 2
#define AUTO 0
#define MANUAL1 1
#define MANUAL2 2
#define MANUAL3 3

int state1 = IDLE; // SW1状态
int state2 = IDLE; // SW2状态
int flag_1 = 0;
int flag_2 = 0;
int pressTime1 = 0; // 记录SW1按下次数
int pressTime2 = 0;
int vector = 1; // 控制Auto模式下颜色变化速度
int mode;  // 模式
int R = 0; // 控制红灯亮度
int G = 0; // 控制绿灯亮度
int B = 0; // 控制蓝灯亮度
int state = 0;

int main(void)
{
	volatile uint32_t ui32Load;
	volatile uint32_t ui32PWMClock;

	/*
	 * Adjust the brightness of the LED.
	 */
	volatile uint8_t ui8Adjust;
	ui8Adjust = 83;

	SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
	SysCtlPWMClockSet(SYSCTL_PWMDIV_64);

	/*
	 * Enable the PWM1 and the GPIOF module (for the LaunchPad buttons on PF0 and PF4 and PWM output on PF1)。
	 */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

	/*
	 * Port F pin 1 (PF1) must be configured as a PWM output pin for module 1, PWM generator 2 (check out the schematic).
	 */
	GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);
	GPIOPinConfigure(GPIO_PF1_M1PWM5);
	GPIOPinConfigure(GPIO_PF2_M1PWM6);
	GPIOPinConfigure(GPIO_PF3_M1PWM7);

	/*
	 * Unlock the GPIO commit control register.
	 */
	HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
	HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
	HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;

	/*
	 * Configures PF0 & 4 as inputs.
	 */
	GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_DIR_MODE_IN);
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0);
	/*
	 * Configure the internal pull-up resistors on both pins. The drive strength setting is merely a place keeper and has no function for an input.
	 */
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

	ui32PWMClock = SysCtlClockGet() / 64;

	/*
	 * Divide the PWM clock by the desired frequency (55Hz) to determine the count to be loaded into the Load register.
	 */
	ui32Load = (ui32PWMClock / PWM_FREQUENCY) - 1;

	/*
	 * Configure module 1 PWM generator 2 as a down-counter and load the count value.
	 */
	PWMGenConfigure(PWM1_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN);
	PWMGenPeriodSet(PWM1_BASE, PWM_GEN_2, ui32Load);
	PWMGenConfigure(PWM1_BASE, PWM_GEN_3, PWM_GEN_MODE_DOWN);
	PWMGenPeriodSet(PWM1_BASE, PWM_GEN_3, ui32Load);

	/*
	 * Set the pulsewidth.
	 */
	PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, ui8Adjust * ui32Load / 1000);
	PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ui8Adjust * ui32Load / 1000);
	PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, ui8Adjust * ui32Load / 1000);

	/*
	 * PWM module 1, generator 2 needs to be enabled as an output and enabled to run.
	 */
	PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, true);
	PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, false);
	PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, true);
	PWMGenEnable(PWM1_BASE, PWM_GEN_2);
	PWMGenEnable(PWM1_BASE, PWM_GEN_3);

	uint32_t ui32Period;
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

	mode = AUTO;
	R = 1000;

	while(1)
	{
		if (mode == AUTO) {
			PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, R * ui32Load / 1000);
			PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, B * ui32Load / 1000);
			PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, G * ui32Load / 1000);

			if (state == 0) {
				// red -, green +, blue 0
				PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, false);
				PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, true);
				R -= vector;
				if (R < 0) R = 0;
				G += vector;
				if (G > 1000) {
					G = 1000;
					state = 1;
				}
				B = 0;
			} else if (state == 1) {
				// red 0, green -, blue +
				PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, false);
				PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, true);
				R = 0;
				G -= vector;
				if (G < 0) G = 0;
				B += vector;
				if (B > 1000) {
					B = 1000;
					state = 2;
				}
			} else {
				// red +, green 0, blue -
				PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, true);
				PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, false);
				G = 0;
				B -= vector;
				if (B < 0) {
					B = 0;
				}
				R += vector;
				if (R > 1000) {
					R = 1000;
					state = 0;
				}
			}
		} else if (mode == MANUAL1) {
			/*
			 * Manual mode 1: intensity of Red LED can be controlled using SW1 and SW2.
			 */
			if (GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4)==0x00) {
				R++;
				if (R > 1000) R = 1000;
				PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, R * ui32Load / 1000);
			}
			if (GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_0)==0x00) {
				R--;
				if (R < 0) R = 0;
				PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, R * ui32Load / 1000);
			}
		} else if (mode == MANUAL2) {
			/*
			 * Manual mode 2: intensity of Blue LED can be controlled using SW1 and SW2.
			 */
			if (GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4)==0x00) {
				B++;
				if (B > 1000) B = 1000;
				PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, B * ui32Load / 1000);
			}
			if (GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_0)==0x00) {
				B--;
				if (B < 0) B = 0;
				PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, B * ui32Load / 1000);
			}
		} else {
			/*
			 * Manual mode 3: intensity of Green LED can be controlled using SW1 and SW2.
			 */
			if (GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4)==0x00) {
				G++;
				if (G > 1000) G = 1000;
				PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, G * ui32Load / 1000);
			}
			if (GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_0)==0x00) {
				G--;
				if (G < 0) G = 0;
				PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, G * ui32Load / 1000);
			}
		}
		SysCtlDelay(100000);
	}

}

void Timer0IntHandler(void)
{
	// Clear the timer interrupt
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

	if (state1 == IDLE) {
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) == 0) {
			// SW1 press
			state1 = PRESS;
			if (flag_2 == 1) pressTime1++;
		}
	} else if (state1 == PRESS) {
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) == 0) {
			// SW1 still press
			state1 = RELEASE;
			flag_1 = 1;
		}
	} else {
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) != 0) {
			// SW1 release
			state1 = IDLE;
			flag_1 = 0;
		}
	}

	if (state2 == IDLE) {
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) == 0) {
			// SW2 press
			state2 = PRESS;
		}
	} else if (state2 == PRESS) {
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) == 0) {
			// SW2 still press
			state2 = RELEASE;
			flag_2 = 1;
		}
	} else {
		// SW2 long pressed, SW1 press once
		if (pressTime1 == 1) {
			mode = MANUAL1;
			PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, true);
			PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, false);
			PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, false);
		}
		else if (pressTime1 == 2) {
			// SW2 long pressed, SW1 press twice
			mode = MANUAL2;
			PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, false);
			PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, true);
			PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, false);
		}
		// SW1 and SW2 long pressed
		if (flag_1 == 1) {
			mode = MANUAL3;
			PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, false);
			PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, false);
			PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, true);
		}
		if (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) != 0) {
			// SW2 release
			state2 = IDLE;
			flag_2 = 0;
			pressTime1 = 0;
		}
	}

	if (mode == AUTO) {
		/*
		 * Auto mode: SW1 will increase the speed of color transition and SW2 will decrease the speed.
		 */
		if (state1 == PRESS || state1 == RELEASE) {
			vector++;
			return;
		}
		if (state2 == PRESS || state2 == RELEASE) {
			vector--;
			if (vector < 1) vector = 1;
			return;
		}
	} else if (mode == MANUAL1) {
		/*
		 * Manual mode 1: intensity of Red LED can be controlled using SW1 and SW2.
		 */
	} else if (mode == MANUAL2) {
		/*
		 * Manual mode 2: intensity of Blue LED can be controlled using SW1 and SW2.
		 */
	} else {
		/*
		 * Manual mode 3: intensity of Green LED can be controlled using SW1 and SW2.
		 */
	}

}

