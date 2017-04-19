/*
Lab3
*/

// Definitions
#include <c8051f020.h>

sbit Led = P1^6;
unsigned char pwm_width;
unsigned char count = 0;
unsigned char t2count = 0;

bit pwm_flag = 0;
bit raising = 1;
bit config = 0;
bit button_mode = 0;
bit Led_state = 1;

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
	pwm_width = 0;
		
	// Timer 2 (100 ms) 
    RCAP2H = 0xb8;  	// Reload value, 47150
    RCAP2L = 0x2e;
    TH2 = 0xb8;     	// Starting value, 47150
    TL2 = 0x2e;
    IE |= 0x20;		     // Enable Timer 2 interrupt (ET2)
		
	// Timer 3 (dim)
	TMR3H = 0xff;		// Starting value, 65280
	TMR3L = 0x00;
	TMR3RLH = 0xff;		// Reload value, 65280
	TMR3RLL = 0x00;
	EIE2 |= 0x01;   	// Enable Timer 3 interrupt (ET3)
		
	TMR3CN |= 0x04; 	// Start Timer 3 (TR3)
	EIE2 |= 0x20;
		
	EA = 1; 			// Enable each interrupt according to its setting
}

void main(void){
	init();
	while(1);
}

void timer3_ISR(void) interrupt 14 {
	
	if (Led_state){
		if (!pwm_flag) {		// High level 
			pwm_flag = 1;
			Led = 1;			// Set PWM pin 
			TMR3L = 255 - pwm_width;	// Load timer 
		} else {				// Low level 
			pwm_flag = 0;
			Led = 0;			// Clear PWM pin 
			TMR3L = pwm_width;	// Load timer 
		}
	}
	
	if (!Led_state) Led = 0;
	
	if (config && count==10){	// Step Speed
			if (raising) pwm_width++;
			if (!raising) pwm_width--;
			if (pwm_width == 250\) raising = 0;
			if (pwm_width == 5) raising = 1;
			count = 0;
		}
	if (config) count++;
	TMR3CN &= 0x7f;			// Clear Timer 3 overflow flag (TF3)
}

void Timer2_ISR(void) interrupt 5 {
		t2count++;
		if (t2count > 10){	// Button remained pressed for more than 1000ms
			config = 1;
		}
		T2CON &= 0x7f; // Clear Timer 2 overflow flag (TF2)
}

void Button_ISR(void) interrupt 19 {
		button_mode = P3IF & 0x08;
		P3IF ^= 0x08;	 // Toggle falling/rising
		
		if(!button_mode){ // Pressing the button
			  TH2 = 0xb8;     // "forced" reload
				TL2 = 0x2e;
				T2CON |= 0x04;  	// Start Timer 2 (TR2)
				t2count = 0;
		}
		else if (button_mode && (t2count < 5)){ // Releasing the button in a 500ms window (fast press)
				t2count = 0;
				Led_state = !Led_state;
			
				T2CON &= 0x00;
		}
		else if (button_mode && (t2count > 10)){ // Releasing the button after more than 1000ms
				t2count = 0;
				config = 0;
				T2CON &= 0x00;
		}
		P3IF &= 0x7f; // reset interrupt bit	
}