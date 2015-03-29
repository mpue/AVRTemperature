/**

	ATMega and TSIC temperature measurement

	Written and maintained by Matthias Pueski

	Copyright (c) 2013 Matthias Pueski

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/**
 *	Title   : Measuring temperature with the ATMega8 and a TSIC 0306
 *	          This version is interrrupt driven
 *	Author  : Matthias Pueski
 *	File    : main.c
 *	Hardware: ATMega8/16MHz
 *
 *	DESCRIPTION :
 *
 *	This program measures the temperature of a TSIC0306 temperature sensor, which is
 *	connected to the INT1 pin. The sensor itself is powered via the ZACPIN defined
 *	in zacwire.h
 *
 *	At the beginning the TSIC is powered on and when the first falling edge occurs the measurement
 *	starts. Timer 1 is started in normal mode. When the first rising edge occurs the timer is being
 *	stopped and the value from the TCNT1 register is taken as strobe value.
 *	From now on on each falling edge of the INT1 pin the timer1 is started in ctc mode.	Each time
 *	the OCR1A register is equal to the timer value, the TIMER1_COMPA_vect interrupt	service routine
 *	is being triggered. This is exactly the 50% duty cycle time which is needed to detect the state
 *	of each bit.
 *	After measuring the MSB the parity bit and the stop bit are being skipped, then the measurement
 *	for the LSB continues, at the end the parity bit is skipped again and the measurement is complete.
 *	The whole thing is driven by a "state machine" like mechanism. This in fact not a real state machine,
 *	since we have no events, rules or transitions.
 *
 *  This code doesn't represent the most efficient solution. The goal was to understand the zacwire protocol
 *	and to write human readable code. Feel free to improve the code and give suggestions.
 *
 *	Author : Matthias Pueski (pueski@gmx.de)
 *
 */
#ifndef F_CPU
#define F_CPU 8000000L
#endif

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "zacwire.h"
#include "int_ctrl.h"
#include "timer_ctrl.h"

#define MUXDELAY 5

// initial state of the reading "state machine"
zacstate state = ZAC_READING_START_BIT_LOW;
// half the value of tstrobe
volatile uint16_t strobe = 0;
// high byte of a zacwire packet
volatile uint8_t zac_high_byte = 0;
// low byte of a zacwire packet
volatile uint8_t zac_low_byte = 0;
// the bit number of the byte currently being read including parity
volatile uint8_t zac_current_bit = 8;
// output buffer
volatile uint8_t buffer[2] = {0};

#define setPin(PORT,PIN) PORT |= (1 << PIN)
#define clearPin(PORT,PIN) PORT &= ~(1 << PIN)

typedef uint8_t bool;

#define true 1
#define false 0


// interrupt service routine for the timer1 ctc
ISR (TIMER1_COMPA_vect) {

	// stop the counter and reset timer count
	timer1_stop_ctc();
	TCNT1 = 0;

	// reading the HIGH byte of a zacwire packet
	if (state == ZAC_READING_HIGH_BYTE) {
		// read bits ignoring the last bit (parity)
		// actually reading 9 bits in total
		if (PIND & (1 << PIND3)) {
			if (zac_current_bit > 0) {
				zac_high_byte |= (1 << (zac_current_bit - 1));
			}
		}
		// decrement current bit every trigger
		if (zac_current_bit > 0) {
			zac_current_bit--;
		}
		// have read all bits from HIGH byte, switch
		// to LOW byte
		else {
			state = ZAC_READING_LOW_BYTE;
			// now we have one bit more since there comes another stop bit
			zac_current_bit = 9;
		}
	}

	// after reading the parity bit of the first byte, we can omit
	// the stop bit, since the next falling edge does not
	// happen until the next start bit (LOW byte)

	// reading the LOW byte of a zacwire packet
	else if (state == ZAC_READING_LOW_BYTE) {
		// read bits ignoring the first bit (start bit) and the last bit (parity)
		// actually reading 10 bits in total
		if (PIND & (1 << PIND3)) {
			if (zac_current_bit > 0 && zac_current_bit < 9) {
				zac_low_byte |= (1 << (zac_current_bit - 1));
			}
		}
		// decrement current bit every trigger
		if (zac_current_bit > 0) {
			zac_current_bit--;
		}
		// all bits read => back to the beginning
		else {
			zac_current_bit = 8;
			state = ZAC_READING_START_BIT_LOW;
		}
	}
}

// Interrupt service routine for the int1 pin
ISR (INT1_vect) {
	/**
	 * Beginning of zacwire packet, first falling edge
	 * start measuring tstrobe time
	 */
	if (state == ZAC_READING_START_BIT_LOW) {
		// if there was on measurement cycle, store data
		// from zacwire in usb transfer buffer
		if (zac_high_byte >= 0)
			buffer[0] = zac_high_byte;
		if (zac_low_byte >= 0)
			buffer[1] = zac_low_byte;
		// reset measured values
		zac_high_byte = 0;
		zac_low_byte = 0;
		// start timer1 and use rising edge for int1
		// if the next rising edge happens this is the second half of the start bit
		int1_switch_rising_edge();
		timer1_start_normal();
		state = ZAC_READING_START_BIT_HIGH;
	}
	else if (state == ZAC_READING_START_BIT_HIGH) {
		// rising edge of the start bit
		// at this stage the first rising edge of the start bit happened
		// we now have the strobe time acquired
		timer1_stop();
		strobe = TCNT1;
		// select falling edge, thus the startbit is complete
		// on next falling edge
		int1_switch_falling_edge();
		// next stage
		state = ZAC_ACQUIRING_TSTROBE;
	}
	else if (state == ZAC_ACQUIRING_TSTROBE) {
		// falling edge of start bit occured, reset timer count
		TCNT1 = 0;
		// start timer 1 in ctc mode, timer interrupt then occurs
		// each tstrobe/2 (50% duty cycle)
		timer1_start_ctc(strobe);
		// we now start reading the first byte
		state = ZAC_READING_HIGH_BYTE;
	}
	// now we are reading either the MSB or the LSB, start timer1 in ctc mode each falling edge
	else if (state == ZAC_READING_HIGH_BYTE || state == ZAC_READING_LOW_BYTE) {
		TCNT1 = 0;
		timer1_start_ctc(strobe);
	}
}

void displayNumber(uint8_t num, bool withDot) {

	setPin(PORTC, 5);
	setPin(PORTC, 4);
	setPin(PORTC, 3);
	setPin(PORTC, 2);
	setPin(PORTB, 4);
	setPin(PORTB, 3);
	setPin(PORTB, 2);
	setPin(PORTB, 1);

	if (withDot) {
		clearPin(PORTB, 1);
	}

	if (num == 0) {
		clearPin(PORTC, 4);
		clearPin(PORTC, 3);
		clearPin(PORTC, 2);
		clearPin(PORTB, 4);
		clearPin(PORTB, 3);
		clearPin(PORTB, 2);
	}
	else if (num == 1) {
		clearPin(PORTB, 3);
		clearPin(PORTB, 2);
	}
	else if (num == 2) {
		clearPin(PORTC, 5);
		clearPin(PORTC, 3);
		clearPin(PORTC, 2);
		clearPin(PORTB, 4);
		clearPin(PORTB, 3);
	}
	else if (num == 3) {
		clearPin(PORTC, 5);
		clearPin(PORTC, 2);
		clearPin(PORTB, 4);
		clearPin(PORTB, 3);
		clearPin(PORTB, 2);
	}
	else if (num == 4) {
		clearPin(PORTC, 5);
		clearPin(PORTC, 4);
		clearPin(PORTB, 3);
		clearPin(PORTB, 2);
	}
	else if (num == 5) {
		clearPin(PORTC, 5);
		clearPin(PORTC, 4);
		clearPin(PORTC, 2);
		clearPin(PORTB, 4);
		clearPin(PORTB, 2);
	}
	else if (num == 6) {
		clearPin(PORTC, 5);
		clearPin(PORTC, 4);
		clearPin(PORTC, 3);
		clearPin(PORTC, 2);
		clearPin(PORTB, 4);
		clearPin(PORTB, 2);
	}
	else if (num == 7) {
		clearPin(PORTB, 4);
		clearPin(PORTB, 3);
		clearPin(PORTB, 2);
	}
	else if (num == 8) {
		clearPin(PORTC, 5);
		clearPin(PORTC, 4);
		clearPin(PORTC, 3);
		clearPin(PORTC, 2);
		clearPin(PORTB, 4);
		clearPin(PORTB, 3);
		clearPin(PORTB, 2);
	}
	else if (num == 9) {
		clearPin(PORTC, 5);
		clearPin(PORTC, 4);
		clearPin(PORTC, 2);
		clearPin(PORTB, 4);
		clearPin(PORTB, 3);
		clearPin(PORTB, 2);
	}

}

void displayMinus(void) {

	setPin(PORTC, 5);
	setPin(PORTC, 4);
	setPin(PORTC, 3);
	setPin(PORTC, 2);
	setPin(PORTB, 4);
	setPin(PORTB, 3);
	setPin(PORTB, 2);
	setPin(PORTB, 1);

	clearPin(PORTC, 5);
}

void displayNothing(void) {

	setPin(PORTC, 5);
	setPin(PORTC, 4);
	setPin(PORTC, 3);
	setPin(PORTC, 2);
	setPin(PORTB, 4);
	setPin(PORTB, 3);
	setPin(PORTB, 2);
	setPin(PORTB, 1);
}

void selectSegment(uint8_t num){

	if (num == 0) {
		setPin(PORTD, 0);
		clearPin(PORTD, 1);
		clearPin(PORTD, 2);
		clearPin(PORTD, 4);
	}
	else if (num == 1) {
		clearPin(PORTD, 0);
		setPin(PORTD, 1);
		clearPin(PORTD, 2);
		clearPin(PORTD, 4);

	}
	else if (num == 2) {
		clearPin(PORTD, 0);
		clearPin(PORTD, 1);
		setPin(PORTD, 2);
		clearPin(PORTD, 4);
	}
	else if (num == 3) {
		clearPin(PORTD, 0);
		clearPin(PORTD, 1);
		clearPin(PORTD, 2);
		setPin(PORTD, 4);
	}

}

void displayMeasure(float value) {

	int val;

	bool minus = false;

	if (value < 0) {
		minus = true;
		value = value * - 1;
	}

	if (value < 1) {
		val = value * 1000;
	}
	else if (value < 10) {
		val = value * 100;
	}
	else if (value < 100){
		val = value * 10;
	}
	else {
		val = value;
	}

	int hundreds = val / 100;
	int tenths = (val - hundreds * 100) / 10;
	int ones = val - (hundreds * 100) - (tenths * 10);

	selectSegment(0);
	displayNumber(ones, false);
	_delay_ms(MUXDELAY);

	selectSegment(1);

	if (value < 1)
		displayNumber(tenths, false);
	else if (value < 10)
		displayNumber(tenths, false);
	else if (value < 100)
		displayNumber(tenths, true);
	else
		displayNumber(tenths, false);
	_delay_ms(MUXDELAY);

	selectSegment(2);

	if (value < 1)
		displayNumber(hundreds, false);
	else if (value < 10)
		displayNumber(hundreds, true);
	else if (value < 100)
		displayNumber(hundreds, false);
	else
		displayNumber(hundreds, false);

	_delay_ms(MUXDELAY);

	selectSegment(3);

	if (minus) {
		displayMinus();
	}
	else {
		displayNothing();
	}

	_delay_ms(MUXDELAY);

}

int main(void) {

	DDRC |= 1 << DDC5;
	DDRC |= 1 << DDC4;
	DDRC |= 1 << DDC3;
	DDRC |= 1 << DDC2;
//
	DDRB = 0xFF;
	PORTB = 0xFF;

//
	DDRD |= 1 << DDD0;
	DDRD |= 1 << DDD1;
	DDRD |= 1 << DDD2;
	DDRD |= 1 << DDD4;
	DDRD |= 1 << DDD5;
//
	int1_enable();
	int1_select_falling_edge();
//	// turn TSIC on
//	//zac_on();
//
	setPin(PORTD, 5);
//
	_delay_us(125);
	// enable interrupts
	sei();
	// for temperature calculations
	uint16_t temperature = 0;
	float value = 0.0f;
//
	// main loop
	for(;;) {

		temperature = (buffer[0] << 8) | buffer [1];
		value = ((float)temperature / 2047 * 200) - 50;

		// display value
		displayMeasure(value);

	}
	// never happens, just to make the compiler happy



	return 0;
}
