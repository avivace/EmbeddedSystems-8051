/*
Lab 2 - Request 4

While in Mode A, the led blinks with a 1 second period.
While in Mode B, the led stays still in the same status it was when the Mode
switched.
The mode switch happens when the button is pressed two times in a 200 ms
time span.
*/

// Definitions
#include <c8051f020.h>

sbit Button = P3^7;
sbit Led = P1^6;
bit pressed = 0;    // 1 while we're in window of time waiting for the
                    //  second pressure
bit mode = 0;       // 0 stands for Mode A, 1 for Mode B
unsigned char counter3 = 0;
unsigned char counter2 = 0;

void init (void) {
    EA = 0;         // Disable all interrupts sources
    WDTCN = 0xde;   // Disable the watchdog
    WDTCN = 0xad;
    OSCICN &= 0x14; // Disable missing clock detector and set
                    //  internal osc at 2 MHz as the clock source
    XBR0 = 0x00;    // Set and enable the crossbar
    XBR1 = 0x00;
    XBR2 = 0x40;
  
    P1MDOUT |= 0x40;// Set P1.6 to push-pull
    EIE2 |= 0x20;   // Enable External Interrupt 7 (EX7)

    // Timer 3 (100ms)
    TMR3H = 0xb8;   // Starting value, 47150
    TMR3L = 0x2e;   
    TMR3RLH = 0xb8; // Reload value, 47150
    TMR3RLL = 0x2e; 
    EIE2 |= 0x01;   // Enable Timer 3 interrupt (ET3)
  
    // Timer 2 (100 ms) 
    RCAP2H = 0xb8;  // Reload value, 47150
    RCAP2L = 0x2e;
    TH2 = 0xb8;     // Starting value, 47150
    TL2 = 0x2e;
    IE |= 0x20;     // Enable Timer 2 interrupt (ET2)
    
    EA = 1;         // Enable each interrupt according to its setting

    TMR3CN |= 0x04; // Start Timer 3 (TR3)
    T2CON |= 0x04;  // Start Timer 2 (TR2)
}

void main(void){
    init();
    while(1);
}

void Timer3_ISR(void) interrupt 14 {
    counter3++;
    if (counter3 == 5){
        if(!mode) Led = !Led; // In Mode A, blink every 500ms
        counter3 = 0;
    }
    TMR3CN &= 0x7f; // Clear Timer 3 overflow flag (TF3)
}

void Timer2_ISR(void) interrupt 5 {
    if (pressed){ 
        counter2++;
        if(counter2 == 2) pressed = 0; 
        // 200ms timeout, clear the pressed state
    }
    T2CON &= 0x7f; // Clear Timer 2 overflow flag (TF2)
}

void Button_ISR(void) interrupt 19 {
    if(!pressed){ // "First" pressure, reset and enters the pressed state
        TH2 = 0xb8;
        TL2 = 0x2e;
        pressed = 1;
    }
    else {        // This pressure follows another one not older than 200ms
        pressed = 0;
        mode = !mode;
    }
    P3IF &= 0x7f; // Clear Interrupt 7 pending flag (IE7)
}