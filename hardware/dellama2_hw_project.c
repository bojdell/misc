/******************************************************************************

HW Project
GE 423 - Spring(2015)
Bodecker DellaMaria
dellama2

MSP430 Alarm Clock feat. BQ32000 RTC

*******************************************************************************/

#include "msp430g2553.h"
#include "UART.h"

// servo positions
#define SERVO_0deg			40000 - 1200
#define SERVO_90deg			40000 - 3600
#define SERVO_180deg		40000 - 6000

// other constants
#define SPK				BIT0
#define BUTTON			BIT3
#define ALARM_OFFSET	10

// struct to keep track of time data
typedef struct {
	char prev_seconds_1s;
	char seconds_10s;
	char seconds_1s;
	char minutes_10s;
	char minutes_1s;
	char hours_10s;
	char hours_1s;
} Timestamp;

// Timestamp instance to keep track of the current time
Timestamp current_time = {
	0,		// prev_seconds
	0, 0,	// seconds (10s, 1s)
	3, 5,	// minutes
	0, 11	// hours
};

// Timestamp instance to keep track of an alarm
Timestamp alarm1 = {
	0,		// prev_seconds
	2, 0,	// seconds (10s, 1s)
	3, 5,	// minutes
	0, 11	// hours
};

// global vars
char newprint = 0;
unsigned long timecnt = 0;
unsigned long stop_ctr = 0;
unsigned int speaker_ctr = 0;
unsigned int speaker_period = 1;
unsigned int servo1_position = SERVO_90deg;
int g_i = 0;
int alarm_sounding = 0;
int alarm_set = 0;

// Idle function
void idle(int us)
{
  us = 16*us;
  for(g_i=0;g_i<us;g_i++);
}


// sets device (rtc) register at reg_address to value via i2c
void setRegister(unsigned char reg_address, unsigned char value) {
	//Transmit address
	UCB0CTL1 |= UCTXSTT + UCTR;   	//generate start condition in transmit mode
	//Transmit data
	UCB0TXBUF = reg_address;
	while(UCB0CTL1 & UCTXSTT);		//wait until start condition is sent
	while((!UCB0STAT & UCNACKIFG) && (!IFG2 & UCB0TXIFG)); //wait for slave to acknowledge and master to finish transmitting
	idle(15);//idle to wait for transfer to finish

	UCB0TXBUF = value;
	while((!UCB0STAT & UCNACKIFG) && (!IFG2 & UCB0TXIFG)); //wait for slave to acknowledge and master to finish transmitting
	idle(15);//idle to wait for transfer to finish

	UCB0CTL1 |= UCTXSTP;        //send stop condition after 2 bytes of data
	while(UCB0CTL1 & UCTXSTP); //wait for stop condition to sent before moving on

}

// gets and returns value of device (rtc) register at reg_address via i2c
unsigned char getRegister(unsigned int reg_address) {
	//Transmit address
	UCB0CTL1 |= UCTXSTT + UCTR;   	//generate start condition in transmit mode

	//Transmit data
	UCB0TXBUF = (reg_address) & 0xFF;
	stop_ctr = 0;
	while(UCB0CTL1 & UCTXSTT);
	while((!UCB0STAT & UCNACKIFG) && (!IFG2 & UCB0TXIFG)); //wait for slave to acknowledge and master to finish transmitting
	idle(15);//idle to wait for transfer to finish

	//Recieve
	UCB0CTL1 &= ~UCTR;			//receive
	UCB0CTL1 |= UCTXSTT;		//send another start condition in receive mode
	idle(15);
	while(UCB0CTL1 & UCTXSTT);	//wait until start condition is sent


	UCB0CTL1 |= UCTXSTP;        //send stop condition after 1 byte of data
	while(!IFG2  & UCB0RXIFG);  //wait for master to finish receiving
	idle(15);
	unsigned char value = UCB0RXBUF;
	while(UCB0CTL1 & UCTXSTP); //wait for stop condition to sent before moving on
	return value;
}

// prints a Timestamp struct to stdout
void printTimestamp(Timestamp * ts) {
	if(ts->hours_10s) {
		UART_printf("%d%d:%d%d.%d%d\r\n", ts->hours_10s, ts->hours_1s,
			ts->minutes_10s, ts->minutes_1s, ts->seconds_10s, ts->seconds_1s);
	}
	else {
		UART_printf(" %d:%d%d.%d%d\r\n", ts->hours_1s,
			ts->minutes_10s, ts->minutes_1s, ts->seconds_10s, ts->seconds_1s);
	}
}

// sets the RTC clock value to new_time
void setTime(Timestamp * new_time) {
	char seconds = (new_time->seconds_10s << 4) | (new_time->seconds_1s);
	setRegister(0x00, seconds);
	char minutes = (new_time->minutes_10s << 4) | (new_time->minutes_1s);
	setRegister(0x01, minutes);
	char hours = (new_time->hours_10s << 4) | (new_time->hours_1s);
	setRegister(0x02, hours);
}

// reads the RTC clock value into ts
void getTime(Timestamp * ts) {
	ts->prev_seconds_1s = ts->seconds_1s;
	char seconds = getRegister(0x00);
	ts->seconds_10s = (seconds >> 4) & 0x7;
	ts->seconds_1s = seconds & 0xF;
	char minutes = getRegister(0x01);
	ts->minutes_10s = (minutes >> 4) & 0x7;
	ts->minutes_1s = minutes & 0xF;
	char hours = getRegister(0x02);
	ts->hours_10s = (hours >> 4) & 0x3;
	ts->hours_1s = hours & 0xF;
}

// checks if time1 represents the same moment in time as time2
char timesAreEqual(Timestamp * time1, Timestamp * time2) {
	char result =
		time1->seconds_1s 	^ time2->seconds_1s |
		time1->seconds_10s 	^ time2->seconds_10s |
		time1->minutes_1s 	^ time2->minutes_1s |
		time1->minutes_10s 	^ time2->minutes_10s |
		time1->hours_1s 	^ time2->hours_1s |
		time1->hours_10s 	^ time2->hours_10s;
	if(result)
		return 0;
	else
		return 1;
}

// registers an alarm with the system that is offset from the timestamp time
void setAlarm(Timestamp * alarm, Timestamp * time, int offset) {
	alarm->seconds_1s 	= time->seconds_1s + offset%10;
	alarm->seconds_10s 	= time->seconds_10s + offset/10;
	alarm->minutes_1s 	= time->minutes_1s;
	alarm->minutes_10s 	= time->minutes_10s;
	alarm->hours_1s 	= time->hours_1s;
	alarm->hours_10s 	= time->hours_10s;
	alarm_set = 1;
}

// this is what happens when the alarm goes off
void triggerAlarm() {
	UART_printf("*** ALARM 1 ***\r\n");
	alarm_sounding = 1;
	servo1_position = SERVO_180deg;
}

void main(void) {

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);

	DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
	BCSCTL1 = CALBC1_16MHZ; 

	// Initialize Port 1
	P1SEL |= 0xc0;
	P1SEL2 |= 0xc0;
	P1REN = 0x00;
	P1DIR |= 0xc0;
	P1OUT &= ~0xc0;

	// enable LED and Speaker
	P1SEL &= ~(BIT4 + SPK);
	P1SEL2 &= ~(BIT4 + SPK);
	P1DIR |= BIT4 + SPK;
	P1OUT &= ~(BIT4 + SPK);

	// enable alarm control button
	P1SEL &= ~(BUTTON);
	P1SEL2 &= ~(BUTTON);
	P1DIR &= ~(BUTTON);
	P1REN |= BUTTON;
	P1IE |= BUTTON; // P1.3 interrupt enabled
	P1IFG &= ~BUTTON; // P1.3 IFG cleared

	//initialize I2C
	UCB0I2CSA = 0x68;
	UCB0CTL0 = UCMST + UCMODE_3 + UCSYNC;        // 2-pin, 7-bit address I2C master, synchronous mode
	UCB0CTL1 = UCSSEL_2 + UCSWRST;               // SMCLK
	UCB0BR0 = 128;                               //
	UCB0BR1 = 0;                                 //
	UCB0CTL1 &= ~UCSWRST;                        // **Initialize USCI state machine**
	IFG2 &= ~UCB0RXIE;                           // Receive interrupt when buffer has 1 complete character, clears interrupt flag

	// Servo 1
	P2DIR |= BIT2;		// set pin 2.2 as output
	P2SEL |= BIT2;		// select TA mode
	P2SEL2 &= ~BIT2;	// select TA mode

	TA1CCR0 = 40000-1;            // PWM Period = 16MHz / (CCR0 * clock_divider) want = 50hz. (16MHz / 8 = 2 MHz), 2,000,000Hz / 50Hz = 40000 = CCR0
	TA1CCTL1 = OUTMOD_3;          // set up timerA to operate in Set/Reset mode
	TA1CCR1 = SERVO_0deg;               	// CCR1 PWM duty cycle = 8.5% (0deg = 0.6ms = 3%, 90deg = 1.8ms = 9%, 180deg = 3.0 ms = 15%)
	TA1CTL = TASSEL_2 + MC_1 + ID1 + ID0;   // SMCLK, up mode, divide by 8

	// Music config
//	P1DIR |= SPK; //Spk as output
//	P1SEL |= SPK; //Speaker as TA0.0
//	P1SEL2 &= ~SPK;
////	TA0CCR2
//	TA0CCTL1 = OUTMOD_7;          // reset/set mode

	// Timer A Config
	TACCTL0 = CCIE;       		  	// Enable Periodic interrupt
	TACCR0 = 16000;               	// period = 1ms
	//TACCR1 = 8000;					// duty cycle = 50%
	TACTL = TASSEL_2 + MC_1;		// source SMCLK, up mode

	Init_UART(9600,1);	// Initialize UART for 9600 baud serial communication

	_BIS_SR(GIE); 		// Enable global interrupt

	idle(2000);
	UART_printf("Started\r\n");
	idle(2000);

	alarm_sounding = 0;

	setTime(&current_time);
	printTimestamp(&current_time);
	idle(2000);

	setAlarm(&alarm1, &current_time, ALARM_OFFSET);
	UART_printf("Alarm set!\r\n");
	idle(2000);

	while(1) {
		if(newprint) {
			printTimestamp(&current_time);
			if(alarm_set) {
				if(timesAreEqual(&current_time, &alarm1)) {
					triggerAlarm();
				}
			}
			if(alarm_sounding) {
				P1OUT ^= BIT4;
//				if(servo1_position == SERVO_180deg)
//					servo1_position = SERVO_0deg;
//				else
//					servo1_position = SERVO_180deg;
			}
			TA1CCR1 = servo1_position;
			newprint = 0;
		}

	}
}

int delta_limit = 50;
int readtime = 110;
int deltatime = 0;
int enabled = 0;

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
	// generate square wave audio for speaker
	if(alarm_sounding) {
		if(speaker_ctr == speaker_period) {
			// oscillate output
			P1OUT ^= SPK;
			speaker_ctr = 0;
		}
		else {
			speaker_ctr++;
		}
	}
	timecnt++; // Keep track of time for main while loop.
	deltatime++;

	// update our current time from the RTC every delta_limit milliseconds
	if (deltatime == delta_limit) {
		getTime(&current_time);
		// if any of the values of current_time have changed (i.e. just the seconds), print the new data
		if(current_time.seconds_1s != current_time.prev_seconds_1s) {
			newprint = 1;
		}
		deltatime = 0;
	}
}


/*
// ADC 10 ISR - Called when a sequence of conversions (A7-A0) have completed
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {

}
*/


// USCI Transmit ISR - Called when TXBUF is empty (ready to accept another character)
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void) {

	// print stuff
	if(IFG2&UCA0TXIFG) {		// USCI_A0 requested TX interrupt
		if(printf_flag) {
			if (currentindex == txcount) {
				senddone = 1;
				printf_flag = 0;
				IFG2 &= ~UCA0TXIFG;
			} else {
				UCA0TXBUF = printbuff[currentindex];
				currentindex++;
			}
		} else if(UART_flag) {
			if(!donesending) {
				UCA0TXBUF = txbuff[txindex];
				if(txbuff[txindex] == 255) {
					donesending = 1;
					txindex = 0;
				}
				else txindex++;
			}
		} else {  // interrupt after sendchar call so just set senddone flag since only one char is sent
			senddone = 1;
		}

		IFG2 &= ~UCA0TXIFG;
	}

	if(IFG2&UCB0TXIFG) {	// USCI_B0 requested TX interrupt (UCB0TXBUF is empty)

		IFG2 &= ~UCB0TXIFG;   // clear IFG
	}
}


// USCI Receive ISR - Called when shift register has been transferred to RXBUF
// Indicates completion of TX/RX operation
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {

	if(IFG2&UCB0RXIFG) {  // USCI_B0 requested RX interrupt (UCB0RXBUF is full)

		IFG2 &= ~UCB0RXIFG;   // clear IFG
	}

	if(IFG2&UCA0RXIFG) {  // USCI_A0 requested RX interrupt (UCA0RXBUF is full)

//    Uncomment this block of code if you would like to use this COM protocol that uses 253 as STARTCHAR and 255 as STOPCHAR
/*		if(!started) {	// Haven't started a message yet
			if(UCA0RXBUF == 253) {
				started = 1;
				newmsg = 0;
			}
		}
		else {	// In process of receiving a message		
			if((UCA0RXBUF != 255) && (msgindex < (MAX_NUM_FLOATS*5))) {
				rxbuff[msgindex] = UCA0RXBUF;

				msgindex++;
			} else {	// Stop char received or too much data received
				if(UCA0RXBUF == 255) {	// Message completed
					newmsg = 1;
					rxbuff[msgindex] = 255;	// "Null"-terminate the array
				}
				started = 0;
				msgindex = 0;
			}
		}
*/

		IFG2 &= ~UCA0RXIFG;
	}

}

// Port 1 interrupt service routine
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
	// use the button 1.3 to disable an alarm that is sounding or to initialize a new alarm when none is set
	if((P1IFG & BUTTON) && (P1IN & BUTTON)) {
		if(alarm_sounding) {
			alarm_sounding = 0;
			alarm_set = 0;
			UART_printf("Alarm Disabled\r\n");
			idle(2000);
		}
		else {
			if(!alarm_set) {
				setAlarm(&alarm1, &current_time, ALARM_OFFSET);
				UART_printf("Alarm set!\r\n");
				idle(2000);
			}
		}
		P1IFG &= ~BUTTON; // P1.3 IFG cleared
	}
}
