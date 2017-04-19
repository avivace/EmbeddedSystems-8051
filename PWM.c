/*
Basic PWM routine
Controlling LED brightness with the PWM duty-cycle.
*/

// Definitions
#include <c8051f020.h>

sbit Led = P1^6;
unsigned char pwm_width = 0;
bit pwm_flag = 0;

void init (void) {
    EA = 0;         	// Disable all interrupts sources
    WDTCN = 0xde;   	// Disable the watchdog
    WDTCN = 0xad;
    OSCICN &= 0x14; 	// Disable missing clock detector and set
                    	//  internal osc at 2 MHz as the clock source
    XBR0 = 0x00;    	// Set and enable the crossbar
    XBR1 = 0x00;
    XBR2 = 0x40;
  
    P1MDOUT |= 0x40; 	// Set P1.6 to push-pull

	// Timer 3 (100ms)
	TMR3H = 0xff;		// Starting value, 65280
	TMR3L = 0x00;
	TMR3RLH = 0xff;		// Reload value, 65280
	TMR3RLL = 0x00;
	EIE2 |= 0x01;   	// Enable Timer 3 interrupt (ET3)
	TMR3CN |= 0x04; 	// Start Timer 3 (TR3)

	EA = 1; 			// Enable each interrupt according to its setting
}

void main(void){
	init();
	while(1){
		pwm_width++;
		if (pwm_width==255) pwm_width = 0;
	}
}

void timer3_ISR(void) interrupt 14 {
	if (!pwm_flag) {		// High level 
		pwm_flag = 1;
		Led = 1;			// Set PWM pin 
		TMR3L = 255 - pwm_width;	// Load timer 
	} else {				// Low level 
		pwm_flag = 0;
		Led = 0;			// Clear PWM pin 
		TMR3L = pwm_width;	// Load timer 
	}
	TMR3CN &= 0x7f;			// Clear Timer 3 overflow flag (TF3)
}