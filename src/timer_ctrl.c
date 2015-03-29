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

/*
 * timer_ctrl.c
 *
 *  Created on: 23.02.2013
 *      Author: Püski
 */
#include <avr/io.h>
#include "timer_ctrl.h"

void timer1_start_normal() {
	TCCR1B = (0 << CS12) | (0 << CS11) | (1 << CS10); // no prescaler
}
void timer1_stop() {
	TCCR1B = (0 << CS12) | (0 << CS11) | (0 << CS10); // stop timer
}

void timer1_stop_ctc() {
	TCCR1B = (0 << CS12) | (0 << CS11) | (0 << CS10); // stop timer
	TIMSK &= ~(1 << OCIE1A); // disable ctc interrupt
}

void timer1_start_ctc(uint16_t cmp) {
	OCR1A = cmp; // set value to output compare register
	TCCR1B = (1 << WGM12) | (0 << CS12) | (0 << CS11) | (1 << CS10); // ctc, no prescaler
	//Enable the Output Compare A interrupt
	TIMSK |= (1 << OCIE1A);
}

