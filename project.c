// Includes
//------------------------------------------------------------------------------------
#include <c8051f020.h> 						// SFR declarations
#include <math.h>
//------------------------------------------------------------------------------------
// Global CONSTANTS
//------------------------------------------------------------------------------------
#define ACC_ADDR 0x98 						// Address for accelerometer 8 bit(lsb is not significant here, set to 0)
#define LCD_ADDR 0x7c						// Address for LCD display 8 bit(lsb is not significant here, set to 0)
#define THERM_ADDR 0x90						// Address for thermometer 8 bit(lsb is not significant here, set to 0)
#define WRITE 0xfe
#define READ 0x01

// SMBus states:
// MT = Master Transmitter
// MR = Master Receiver
#define SMB_BUS_ERROR 0x00 			// (all modes) BUS ERROR
#define SMB_START 0x08 					// (MT & MR) START transmitted
#define SMB_RP_START 0x10 				// (MT & MR) repeated START
#define SMB_MTADDACK 0x18 				// (MT) Slave address + W transmitted;
													// ACK received
#define SMB_MTADDNACK 0x20 			// (MT) Slave address + W transmitted;
													// NACK received
#define SMB_MTDBACK 0x28 				// (MT) data byte transmitted; ACK rec’vd
#define SMB_MTDBNACK 0x30 				// (MT) data byte transmitted; NACK rec’vd
#define SMB_MTARBLOST 0x38 			// (MT) arbitration lost
#define SMB_MRADDACK 0x40 				// (MR) Slave address + R transmitted;
													// ACK received
#define SMB_MRADDNACK 0x48 			// (MR) Slave address + R transmitted;
													// NACK received
#define SMB_MRDBACK 0x50 				// (MR) data byte rec’vd; ACK transmitted
#define SMB_MRDBNACK 0x58 				// (MR) data byte rec’vd; NACK transmitted

//------------------------------------------------------------------------------------
// Global VARIABLES
//------------------------------------------------------------------------------------

bit SM_BUSY; 	// This bit is set when a send or receive is started
						// It is cleared by the ISR when the operation is finished
														
unsigned char COMMAND;

char *DATA;
	
unsigned char BYTE_LEFT_S;
unsigned char BYTE_LEFT_R;

char ACCELERATION[3];

unsigned char TEMPERATURE[2];
unsigned char TEMP;

unsigned char index;

float DEGREE_CONV = 57.295f;

float BUFFERX[8];
float BUFFERY[8];
float BUFFERZ[8];

//01 clear display, c0 goto second line commands: 0x80

unsigned char FIRSTLINE[17] = "@X=   ß Y=   ß  ";
unsigned char SECONDLINE[17]= "@Z=   ß T=+  ßC  ";
	
unsigned char clear_display[2] = {0x80, 0x01};
unsigned char goto_secondline[2] = {0x80, 0xc0};
														
bit	rs;
bit getData;
bit sendData;
bit getTemp;
														
sbit Led = P0^6;
unsigned char pwm_width;
unsigned char count = 0;
unsigned char t2count = 0;
unsigned char t4count3 = 0;
unsigned char t4count10 = 0;

bit pwm_flag = 0;
bit raising = 1;
bit config = 0;
bit button_mode = 0;
bit Led_state = 1;														


//------------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------------
void SMBUS_ISR (void);
void Timer2_ISR(void);
void Button_ISR(void);
void timer3_ISR(void);
void init_ACC(char *array_acc);
void send_DISPLAY(char *display_init, unsigned char length);
void read_ACC(void);
void read_THERM(void);
void Timer4_ISR(void);
float media(float *buffer);
void refresh_angle(float x, float y, float z);
void refresh_temperature(void);
														
//------------------------------------------------------------------------------------
// MAIN routine
//------------------------------------------------------------------------------------
														
void main (void) {
	unsigned char toSend[8] = {0x38, 0x39, 0x14, 0x74, 0x54, 0x6f, 0x0c, 0x01};
	float convert, sin, angle, mediax = 0, mediay = 0, mediaz = 0;

	char array_acc[2] = {0x07, 0x01};
	convert = 0.047;
	EA = 0;
	WDTCN = 0xde; 						// disable watchdog timer
	WDTCN = 0xad;
	OSCICN &= 0x14; 					// Disable missing clock detector and set
														// internal osc at 2 MHz as the clock source
	
	XBR0 = 0x05; 							// Route SMBus to GPIO pins through crossbar, enable UART0
														// in order to properly connect pins.
	
	XBR2 = 0x40; 							// Enable crossbar and weak pull-ups
	
	SMB0CN = 0x44; 						// Enable SMBus with ACKs on acknowledge cycle
	SMB0CR = -80; 						// SMBus clock rate = 100kHz.
	EIE1 |= 2; 								// SMBus interrupt enable
	SM_BUSY = 0; 							// Free SMBus for first transfer.
	
	P0MDOUT |= 0x40; 					// Set P1.6 to push-pull
	pwm_width = 0;
	EIE2 |= 0x20;
	
	// Timer 2 (100 ms) 
	RCAP2H = 0xb8;  					// Reload value, 47150
	RCAP2L = 0x2e;
	TH2 = 0xb8;     					// Starting value, 47150
	TL2 = 0x2e;
	IE |= 0x20;		     				// Enable Timer 2 interrupt (ET2)
	// End Timer 2
	
	
	// Timer 3 (dim)
	TMR3H = 0xff;							// Starting value, 65280
	TMR3L = 0x00;
	TMR3RLH = 0xff;						// Reload value, 65280
	TMR3RLL = 0x00;
	EIE2 |= 0x01;   					// Enable Timer 3 interrupt (ET3)
		
	TMR3CN |= 0x04; 					// Start Timer 3 (TR3)
	// End Timer 3
	
	// Timer 4 (100ms, always) 
	RCAP4H = 0xb8;  					// Reload value, 47150
	RCAP4L = 0x2e;
	TH4 = 0xb8;     					// Starting value, 47150
	TL4 = 0x2e;
	EIE2 |= 0x04;		     			// Enable Timer 4 interrupt
	T4CON |= 0x04;						// Start Timer 4
	// End Timer 4
	
	getData = 0;
	index = 0;
	

	EA = 1; 									// Global interrupt enable

	init_ACC(array_acc);
	send_DISPLAY(toSend, 8);
	
	

	while(1) {
		if(getData){
			read_ACC();
			getData = 0;
			if((ACCELERATION[0] >> 5) == 1)
				ACCELERATION[0] -= 64;
			
			sin = ACCELERATION[0] * convert;
			if(sin >= -1 && sin <= 1)
				angle = asin(sin);
			
			BUFFERX[index] = angle * DEGREE_CONV;
			
			if((ACCELERATION[1] >> 5) == 1)
				ACCELERATION[1] -= 64;
			sin = ACCELERATION[1] * convert;
			if(sin >= -1 && sin <= 1)
				angle = asin(sin);
			
			BUFFERY[index] = angle * DEGREE_CONV;
			
			if((ACCELERATION[2] >> 5) == 1)
				ACCELERATION[2] -= 64;
			sin = ACCELERATION[2] * convert;
			if(sin >= -1 && sin <= 1)
				angle = acos(sin);
			else
				angle = 0;
			
			BUFFERZ[index] = angle * DEGREE_CONV;
			
			if(index == 7)
				index = 0;
			else
				index++;
			
		}
		
		
		if(sendData){
			sendData = 0;
			mediax = media(BUFFERX);
			mediay = media(BUFFERY);
			mediaz = media(BUFFERZ);
			
			refresh_angle(mediax, mediay, mediaz);
			
			send_DISPLAY(clear_display, 2);
			send_DISPLAY(FIRSTLINE, 15);
			send_DISPLAY(goto_secondline, 2);
			send_DISPLAY(SECONDLINE, 15);
			
			
		}
		
		
		if(getTemp) {
			getTemp = 0;
			
			read_THERM();
			
			refresh_temperature();
			
			
		}
		
	
	}
	
}


//------------------------------------------------------------------------------------
// SMBus interrupt service routine
//------------------------------------------------------------------------------------
void SMBUS_ISR (void) interrupt 7 {
		switch (SMB0STA) {			// Status code for SMBus 
			
			case SMB_START:
				SMB0DAT = COMMAND;
				STA = 0;
				break;
			
			case SMB_RP_START:
				SMB0DAT = COMMAND | READ;
				STA = 0;
				break;
			
			case SMB_MTADDACK:
				SMB0DAT = *DATA;
				DATA++;
				BYTE_LEFT_S--;
				STA = 0;
				break;
			
			case SMB_MTADDNACK:
				STO = 1;
				STA = 1;
				break;
			
			case SMB_MTDBACK:
				if(BYTE_LEFT_S > 0) {
					SMB0DAT = *DATA;
					BYTE_LEFT_S--;
					DATA++;
				}
				else if(rs){
					STO = 0;
					STA = 1;
				}
				else {
					STO = 1;
					SM_BUSY = 0;
				}
				break;
			
			case SMB_MTDBNACK:
				STA = 1;
				break;
			
			case SMB_MTARBLOST:
				STO = 1;
				break;
			
			case SMB_MRADDACK:
				STA = 0;
				break;
			
			case SMB_MRADDNACK:
				STA = 1;
				STO = 1;
				break;
			
			case SMB_MRDBACK:
				if(BYTE_LEFT_R > 0){
					if(COMMAND == 0x98)
						ACCELERATION[3-BYTE_LEFT_R] = SMB0DAT;
					else if(COMMAND == 0x91)
						TEMPERATURE[2-BYTE_LEFT_R] = SMB0DAT;
					
					BYTE_LEFT_R--;
				}
				else {
					AA = 0;
					STO = 0;
					SM_BUSY = 0;
				}
				break;
			case SMB_MRDBNACK:
				STO = 1;
				break;
			
			}
		
		SI = 0;
}

void send_DISPLAY(char *display_init, unsigned char length){
	while(SM_BUSY);
	SM_BUSY = 1;
	SMB0CN = 0x44;
	
	DATA = display_init;
	COMMAND = LCD_ADDR & WRITE;
	BYTE_LEFT_S = length;
	BYTE_LEFT_R = 0;
	rs = 0;
	
	STO = 0;
	STA = 1;
	while(SM_BUSY);
	
}

void init_ACC(char *array_acc) {
	while(SM_BUSY);
	SM_BUSY = 1;
	SMB0CN = 0x44;
	
	
	COMMAND = ACC_ADDR;
	DATA = array_acc;
	BYTE_LEFT_S = 2;
	rs = 0;
	
	STO = 0;
	STA = 1;
	while(SM_BUSY);
}

void read_ACC(void) {
	char begin = 0x00;
	while(SM_BUSY);
	SM_BUSY = 1;
	SMB0CN = 0x44;
	
	COMMAND = ACC_ADDR;
	
	DATA = &begin;		// funziona solo se c'è il while SM_BUSY in fondo (puntatore a variabile sullo stack)
	BYTE_LEFT_R = 3;
	BYTE_LEFT_S = 1;
	
	rs = 1;
	
	STO = 0;
	STA = 1;
	while(SM_BUSY);
}

void read_THERM() {
	while(SM_BUSY);
	SM_BUSY = 1;
	SMB0CN = 0x44;
	
	rs = 0;
	COMMAND = THERM_ADDR | READ;
	BYTE_LEFT_S = 0;
	BYTE_LEFT_R = 2;
	
	STO = 0;
	STA = 1;
	while(SM_BUSY);
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
		P3IF ^= 0x08;	 							// Toggle falling/rising
		
		if(!button_mode){ 					// Pressing the button
			  TH2 = 0xb8;     				// "forced" reload
				TL2 = 0x2e;
				T2CON |= 0x04;  				// Start Timer 2 (TR2)
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
		P3IF &= 0x7f; 							// reset interrupt bit	
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
			if (pwm_width == 250) raising = 0;
			if (pwm_width == 5) raising = 1;
			count = 0;
		}
	if (config) count++;
	TMR3CN &= 0x7f;			// Clear Timer 3 overflow flag (TF3)
}

void Timer4_ISR(void) interrupt 16 {
	t4count3++;
	t4count10++;
	getData = 1;

	if(t4count3 == 3){
		sendData = 1;
		t4count3 = 0;
	}
		
	
	if(t4count10 == 10){
		getTemp = 1;
		t4count10 = 0;
	}

	T4CON &= 0x7f;
}

float media(float *valori) {
	unsigned char i = 0;
  float acc = 0;
  for (i; i<8; i++){
		//if(!valori[i])
			acc += valori[i];
  }
  return acc/8;
}

void refresh_angle(float x, float y, float z){
	if(z >= 90){
		z -= 90;
		z = -z;
	}
	
	if(x >= 90){
		x -= 90;
		x = -x;
	}		

		
	if(y >= 90){
		y	-= 90;
		y = -y;
	}	
	
  if (x >= 0)
	 FIRSTLINE[3] = '+';
  else{
		x = -x;
	 FIRSTLINE[3] = '-';
	}

  if (y >= 0)
	 FIRSTLINE[10] = '+';
  else{
	 FIRSTLINE[10] = '-';
		y = -y;
	}

  if (z >= 0)
	 SECONDLINE[3] = '+';
  else{
	 SECONDLINE[3] = '-';
		z = -z;
	}

  FIRSTLINE[4] = (unsigned char) ((x+0.5) / 10) + 48;
  FIRSTLINE[5] = (unsigned char) (x+0.5) % 10;
	FIRSTLINE[5] += 48;
  FIRSTLINE[11] = (unsigned char) ((y+0.5) / 10) + 48;
  FIRSTLINE[12] = (unsigned char) (y+0.5) % 10;
	FIRSTLINE[12] += 48;
  SECONDLINE[4] = (unsigned char) ((z+0.5) / 10) + 48;
  SECONDLINE[5] = (unsigned char) (z+0.5) % 10;
	SECONDLINE[5] += 48;
}

void refresh_temperature(void) {
	unsigned char first_digit;
	unsigned char second_digit;
	TEMP = 0;
	TEMPERATURE[1] &= 0xf8;
	if(TEMPERATURE[0] >> 7) {
		char ekomi = 'a';
		// robe da fare se negativo mancano i due segni
	}
	else {
		TEMP = TEMPERATURE[0];
		TEMP = TEMP << 1;
		TEMPERATURE[1] = TEMPERATURE[1] >> 7;
		TEMP |= TEMPERATURE[1];
		first_digit = TEMP / 10;
		second_digit = TEMP % 10;
		SECONDLINE[11] = first_digit + 48;
		SECONDLINE[12] = second_digit + 48;

		}	

	
}