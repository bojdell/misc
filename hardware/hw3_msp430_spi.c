/******************************************************************************
 * GE 423: Mechatronics
 * Spring 2015
 * 
 * HW 3 - Problems 5 & 6
 * 
 * Bodecker DellaMaria
 * dellama2@illinois.edu
 *******************************************************************************/

//                    MSP430G2xx3
//                 -----------------
//             /|\|              XIN|-
//              | |                 |
//              --|RST          XOUT|-
//                |                 |
//                |                 |-> Data Out (UCB0SIMO)
//                |                 |
//           FS <-| P1.6        P1.7|<- Data In (UCB0SOMI)
//                |                 |
//                |             P1.5|-> Serial Clock Out (UCB0CLK)
//

#include "msp430g2553.h"
#include "UART.h"

#define LIGHTS_OFF			200		// photoresistor threshold (calibrated via readings)
#define LIGHTS_ON			800		// calibtrated lights on value

// for PWM duty cycle, inverted because I used OUTMOD_3
#define SERVO_0deg			40000 - 1200	// 0deg = 0.6ms = 3%
#define SERVO_90deg			40000 - 3600	// 90deg = 1.8ms = 9%
#define SERVO_180deg		40000 - 6000	// 180deg = 3.0 ms = 15%
#define MAX_ADC_VAL			0x03ff			// max value that can be sent to 10-bit ADC

char newprint = 0;
unsigned long timecnt = 0;  

int adc_val = 0, lights_timer_start = 0, lights_timer = 0;
int adcready=0;

unsigned char mst_data_MSB, mst_data_LSB;
unsigned char second_byte_sent;
unsigned int ramp_val = 0;

unsigned int servo_position = SERVO_90deg;

void main(void) {

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) while(1);

	DCOCTL = CALDCO_16MHZ;    // Set uC to run at approximately 16 Mhz
	BCSCTL1 = CALBC1_16MHZ; 

	// Servo 1
	P2DIR |= BIT2;		// set pin 2.2 as output
	P2SEL |= BIT2;		// select TA mode
	P2SEL2 &= ~BIT2;	// select TA mode

	TA1CCR0 = 40000-1;            // PWM Period = 16MHz / (CCR0 * clock_divider) want = 50hz. (16MHz / 8 = 2 MHz), 2,000,000Hz / 50Hz = 40000 = CCR0
	TA1CCTL1 = OUTMOD_3;          // set up timerA to operate in Set/Reset mode
	TA1CCR1 = SERVO_0deg;               	// CCR1 PWM duty cycle = 8.5% (0deg = 0.6ms = 3%, 90deg = 1.8ms = 9%, 180deg = 3.0 ms = 15%)
	TA1CTL = TASSEL_2 + MC_1 + ID1 + ID0;   // SMCLK, up mode, divide by 8

	// Servo 2
	P2DIR |= BIT4;		// set pin 2.4 as output
	P2SEL |= BIT4;		// select TA mode
	P2SEL2 &= ~BIT4;	// select TA mode

	TA1CCTL2 = OUTMOD_3;          // set up to operate in Set/Reset mode
	TA1CCR2 = SERVO_0deg;         // CCR1 PWM duty cycle = 8.5% (0deg = 0.6ms = 3%, 90deg = 1.8ms = 9%, 180deg = 3.0 ms = 15%)


	P1OUT = 0x00;           // reset P1 outputs
	P1DIR |= BIT6;			// set up 1.6 as digital GPIO for FS
	P1SEL &= ~BIT6;
	P1SEL2 &= ~BIT6;

	// set up 1.5 as UCB0CLK for SCLK
	P1SEL |= BIT5;
	P1SEL2 |= BIT5;

	// set up 1.7 as UCB0SIMO for DIN
	P1SEL |= BIT7;
	P1SEL2 |= BIT7;

	UCB0CTL0 = UCCKPH + UCCKPL + UCMSB + UCMST + UCSYNC;  	// 3-pin, 8-bit SPI master
	UCB0CTL1 = UCSSEL_2 + UCSWRST;                    		// SMCLK
	UCB0BR0 = 0x80;                          	//8-bit words
	UCB0BR1 = 0;                            //
//	UCB0MCTL = 0;                           // No modulation
	UCB0CTL1 &= ~UCSWRST;                   // **Initialize USCI state machine**
	IFG2 &= ~UCB0RXIE;						// no interrupt pending
	IE2 |= UCB0RXIE;                        // Enable USCI0 RX interrupt on B0

	ADC10CTL0 = ADC10SHT_2 + ADC10ON + ADC10IE; // ADC10ON, interrupt enabled
//	ADC10AE0 |= BIT0;	// enable input A0
//	ADC10CTL1 = INCH_0; // input A0
	ADC10AE0 |= BIT3;	// enable input A3
	ADC10CTL1 = INCH_3; // input A3


	mst_data_MSB = (char)(ramp_val >> 8);   // Initialize data values
	mst_data_LSB = (char)ramp_val;

	second_byte_sent = 0;			// init flag

	// Timer A Config
	TACCTL0 = CCIE;       		// Enable Periodic interrupt
	TACCR0 = 16000;                // period = 1ms
	TACTL = TASSEL_2 + MC_1; // source SMCLK, up mode


	Init_UART(9600,1);	// Initialize UART for 9600 baud serial communication

	_BIS_SR(GIE); 		// Enable global interrupt


	while(1) {

		if(newmsg) {
			newmsg = 0;
		}

		if (newprint)  {
			ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start

			// print some data
			UART_printf("Hello %x %x %x %d\n\r",(unsigned int)(ramp_val), (int)(mst_data_MSB), (int)(mst_data_LSB), (unsigned int)(adc_val));
			
			// update servo positions
			TA1CCR1 = servo_position;
			TA1CCR2 = servo_position;

			newprint = 0;
		}
		if(adcready) {
			// update ramp val (normalize to potentiometer reading)
			ramp_val = (int)(((float)adc_val / LIGHTS_ON) * 1024);
			adcready = 0;
		}

	}
}


// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
	timecnt++; // Keep track of time for main while loop.
//	ramp_val++;	// used in abscence of pot.
	if(ramp_val > MAX_ADC_VAL)	// reset ramp if over max val
		ramp_val = 0;

	mst_data_MSB = (char)(ramp_val >> 6);	// parse out bytes from 16-bit ramp_val
	mst_data_LSB = (char)(ramp_val << 2);

	second_byte_sent = 0;			// init flag for second byte
	P1OUT |= BIT6;					// pulse FS
	P1OUT &= ~BIT6;

	UCB0TXBUF = mst_data_MSB;       // Transmit first character

	if ((timecnt%250) == 0) {
		newprint = 1;  // flag main while loop that .5 seconds have gone by.
	}
	if ((timecnt%2000) == 0) {
		if(servo_position == SERVO_90deg)
			servo_position = SERVO_0deg;
		else
			servo_position = SERVO_90deg;
	}

}



// ADC 10 ISR - Called when a sequence of conversions (A7-A0) have completed
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
	adc_val = ADC10MEM;					// Saves measured value.
	adcready=1;							// tell main loop adc data is ready
}

char the_char = 'z';
// USCI Receive ISR - Called when shift register has been transferred to RXBUF
// Indicates completion of TX/RX operation
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {

	if(IFG2&UCB0RXIFG) {  // USCI_B0 requested RX interrupt (UCB0RXBUF is full)
		// if we haven't sent the second byte yet, send it
		if(!second_byte_sent) {
			// send byte
			UCB0TXBUF = mst_data_LSB;

			// set flag
			second_byte_sent = 1;
		}

		IFG2 &= ~UCB0RXIFG;   // clear IFG
	}

	if(IFG2&UCA0RXIFG) {  // USCI_A0 requested RX interrupt (UCA0RXBUF is full)
		the_char = UCA0RXBUF;	// read the character
		IFG2 &= ~UCA0RXIFG;
	}
}


// USCI Transmit ISR - Called when TXBUF is empty (ready to accept another character)
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void) {

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


