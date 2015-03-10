//******************************************************************************
//   MSP430G2xx3 Demo - USCI_B0, SPI 3-Wire Master Incremented Data
//
//   Description: SPI master talks to SPI slave using 3-wire mode. Incrementing
//   data is sent by the master starting at 0x01. Received data is expected to
//   be same as the previous transmission.  USCI RX ISR is used to handle
//   communication with the CPU, normally in LPM0. If high, P1.0 indicates
//   valid data reception.
//   ACLK = n/a, MCLK = SMCLK = DCO ~1.2MHz, BRCLK = SMCLK/2
//
//   Use with SPI Slave Data Echo code example. If slave is in debug mode, P3.6
//   slave reset signal conflicts with slave's JTAG; to work around, use IAR's
//   "Release JTAG on Go" on slave device.  If breakpoints are set in
//   slave RX ISR, master must stopped also to avoid overrunning slave
//   RXBUF.
//
//                    MSP430G2xx3
//                 -----------------
//             /|\|              XIN|-
//              | |                 |
//              --|RST          XOUT|-
//                |                 |
//                |             P1.7|-> Data Out (UCB0SIMO)
//                |                 |
//          LED <-|P1.0         P1.6|<- Data In (UCB0SOMI)
//                |                 |
//  Slave reset <-|P1.4         P1.5|-> Serial Clock Out (UCB0CLK)
//
//
//   D. Dang
//   Texas Instruments Inc.
//   February 2011
//   Built with CCS Version 4.2.0 and IAR Embedded Workbench Version: 5.10
//******************************************************************************

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
//  Slave reset <-|             P1.5|-> Serial Clock Out (UCB0CLK)
//

#include "msp430g2553.h"

unsigned char mst_data_MSB, mst_data_LSB, slv_data;
unsigned char second_byte_sent;

void main(void)
{
  volatile unsigned int i;

  WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
  P1OUT = 0x00;                             // reset P1 outputs
  P1DIR |= BIT6;							// setup 1.6 as digital GPIO
  P1SEL &= ~BIT6;
  P1SEL2 &= ~BIT6;
  UCB0CTL0 = UCCKPH + UCCKPL + UCMSB + UCMST + UCSYNC;  // 3-pin, 8-bit SPI master
  UCB0CTL1 = UCSSEL_2 + UCSWRST;                     	// SMCLK
  UCB0BR0 = 8;                          	//8-bit words
  UCB0BR1 = 0;                              //
  UCB0MCTL = 0;                             // No modulation
  UCB0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
  IFG2 &= ~UCB0RXIE;						// no interrupt pending
  IE2 |= UCB0RXIE;                          // Enable USCI0 RX interrupt on B0


  mst_data_MSB = 0x01;                          // Initialize data values
  mst_data_LSB = 0x01;

  second_byte_sent = 0;	// init flag

  P1OUT |= BIT6;		// pulse FS
  P1OUT &= ~BIT6;

  UCB0TXBUF = mst_data_MSB;                     // Transmit first character

  // enable interrupts?
}

// Test for valid RX and TX character
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR(void)
{
  volatile unsigned int i;

  while (!(IFG2 & UCB0TXIFG));              // USCI_B0 TX buffer ready?

  // if we haven't sent the second byte yet, send it
  if(!second_byte_sent) {
  	// send byte
  	UCB0TXBUF = mst_data_LSB;

  	// set flag
  	second_byte_sent = 1;
  }
}

void question6(void) {
	
	P1DIR |= BIT2;             // P1.2 to output
	P1SEL |= BIT2;             // P1.2 to TA0.1

	CCR0 = 40000-1;            // PWM Period = 16MHz / (CCR0 * clock_divider) want = 50hz. (16MHz / 8 = 2 MHz), 2,000,000Hz / 50Hz = 40000 = CCR0
	CCTL1 = OUTMOD_3;          // set up timerA to operate in Set/Reset mode
	CCR1 = 3400;               // CCR1 PWM duty cycle = 8.5% (90deg = 1.8ms = 8.5%)
	TACTL = TASSEL_2 + MC_1;   // SMCLK, up mode
}
