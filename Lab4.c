/*
Lab4
*/
// Includes
//------------------------------------------------------------------------------------
#include <c8051f000.h> 						// SFR declarations
//------------------------------------------------------------------------------------
// Global CONSTANTS
//------------------------------------------------------------------------------------
#define LED_ADDR 0x7c						// Address for display 8bit (lsb is not significant here, set to 0)
#define THERM_ADDR	0x90					// Address for thermometer 8bit (lsb is not significant here, set to 0)
#define WRITE 0xfe
#define READ 0x01

// SMBus states:
// MT = Master Transmitter
// MR = Master Receiver
#define SMB_BUS_ERROR 0x00 				// (all modes) BUS ERROR
#define SMB_START 0x08 						// (MT & MR) START transmitted
#define SMB_RP_START 0x10 				// (MT & MR) repeated START
#define SMB_MTADDACK 0x18 				// (MT) Slave address + W transmitted;
																	// ACK received
#define SMB_MTADDNACK 0x20 				// (MT) Slave address + W transmitted;
																	// NACK received
#define SMB_MTDBACK 0x28 					// (MT) data byte transmitted; ACK rec’vd
#define SMB_MTDBNACK 0x30 				// (MT) data byte transmitted; NACK rec’vd
#define SMB_MTARBLOST 0x38 				// (MT) arbitration lost
#define SMB_MRADDACK 0x40 				// (MR) Slave address + R transmitted;
																	// ACK received
#define SMB_MRADDNACK 0x48 				// (MR) Slave address + R transmitted;
																	// NACK received
#define SMB_MRDBACK 0x50 					// (MR) data byte rec’vd; ACK transmitted
#define SMB_MRDBNACK 0x58 				// (MR) data byte rec’vd; NACK transmitted

//------------------------------------------------------------------------------------
// Global VARIABLES
//------------------------------------------------------------------------------------

bit SM_BUSY; 								// This bit is set when a send or receive is started
														// It is cleared by the ISR when the operation is finished

char *WORD;									// Holds the pointer to the data sent / received 

char COMMAND;								// Holds the slave address + R/W bit for use in the SMBus ISR.

unsigned char HIGH_TEMP;							// Holds high value of the temperature
unsigned char LOW_TEMP;							// Holds low value of the temperature	
	
unsigned char BYTE_LEFT;			// Number of bytes to send

unsigned char TEMP;

//------------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------------
void SMBus_ISR(void);
void DISPLAY_Send(char *toSend, unsigned char numBytes);
void THERM_Read(void);
void send_init(char *toSend);
														
//------------------------------------------------------------------------------------
// MAIN routine
//------------------------------------------------------------------------------------
														
void main (void) {
	unsigned char toSend[14] = {0x38, 0x39, 0x14, 0x74, 0x54, 0x6f, 0x0c, 0x01, 0x40};
	
	WDTCN = 0xde; 						// disable watchdog timer
	WDTCN = 0xad;
	OSCICN &= 0x14; 					// Set internal oscillator to highest setting (16 MHz)??
	XBR0 = 0x05; 							// Route SMBus to GPIO pins through crossbar, enable UART0
														// in order to properly connect pins.
	XBR2 = 0x40; 							// Enable crossbar and weak pull-ups
	SMB0CN = 0x44; 						// Enable SMBus with ACKs on acknowledge cycle
	SMB0CR = -80; 						// SMBus clock rate = 100kHz.
	EIE1 |= 2; 								// SMBus interrupt enable
	EA = 1; 									// Global interrupt enable
	SM_BUSY = 0; 							// Free SMBus for first transfer.
	
	THERM_Read();
	send_init(toSend);
	DISPLAY_Send(toSend, 13);
	while(1);
	
}


// SMBus interrupt service routine:
void SMBUS_ISR (void) interrupt 7 {
		switch (SMB0STA) {			// Status code for SMBus 
			
			case SMB_START:
				SMB0DAT = COMMAND;
				STA = 0;
				break;
			
			case SMB_RP_START:
				SMB0DAT = COMMAND;
				STA = 0;
				break;
			
			case SMB_MTADDACK:
				SMB0DAT = *WORD;
				BYTE_LEFT--;
				STA = 0;
				break;
			
			case SMB_MTADDNACK:
				STO = 1;
				STA = 1;
				break;
			
			case SMB_MTDBACK:
				if(BYTE_LEFT > 0) {
					WORD++;
					BYTE_LEFT--;
					SMB0DAT = *WORD;
				}
				else
					STO = 1;
				break;
			
			case SMB_MTDBNACK:
				STO = 1;
				STA = 1;
				break;
			
			case SMB_MTARBLOST:
				STO = 1;
				STA = 1;
				break;
			
			case SMB_MRADDACK:
				STA = 0;
				break;
			
			
			case SMB_MRADDNACK:
				STA = 1;
				STO = 1;
				break;
			
			case SMB_MRDBACK:
				if(BYTE_LEFT == 2)
					HIGH_TEMP = SMB0DAT;
				if(BYTE_LEFT == 1)
					LOW_TEMP = SMB0DAT;
				
				BYTE_LEFT--;
				if(BYTE_LEFT == 0)
					AA = 0;
				break;
					
				
			case SMB_MRDBNACK:
				STO = 1;
			
			default:
				STO = 1;
				SM_BUSY = 0;
				break;
			
			}
		SI = 0;
}

void DISPLAY_Send(char *toSend, unsigned char numBytes) {
	while(SM_BUSY);										// Wait for SMBus to be free.
	SM_BUSY = 1;
	SMB0CN = 0x44;
	
	WORD = toSend;
	BYTE_LEFT = numBytes;
	COMMAND = LED_ADDR & WRITE;
	
	STO = 0;													// Start transfer
	STA = 1;
	
}

void THERM_Read(void) {
	while(SM_BUSY);										// Wait for SMBus to be free.
	SM_BUSY = 1;
	SMB0CN = 0x44;
	BYTE_LEFT = 2;
	
	COMMAND = THERM_ADDR | READ;
	
	STO = 0;													// Start transfer
	STA = 1;
	while(SM_BUSY);
}

void send_init(char *toSend) {
	unsigned char first_digit;
	unsigned char second_digit;
	TEMP = 0;
	LOW_TEMP &= 0xf8;
	if(HIGH_TEMP >> 7) {
		char ekomi = 'a';
		// robe da fare se negativo
	}
	else {
		TEMP = HIGH_TEMP;
		TEMP = TEMP << 1;
		LOW_TEMP = LOW_TEMP >> 7;
		TEMP |= LOW_TEMP;
		first_digit = TEMP / 10;
		second_digit = TEMP % 10;
		toSend[9] = first_digit + 0x30;
		toSend[10] = second_digit + 0x30;
		toSend[11] = 0xdf;
		toSend[12] = 0x43;
		}	
}
