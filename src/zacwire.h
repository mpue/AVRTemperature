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
 * zakwire.h
 *
 *  Created on: 17.01.2013
 *      Author: Püski
 */

#ifndef ZACKWIRE_H_
#define ZACWIRE_H_

#define ZACPORT PORTD
#define ZACPIN  PIND4

void zac_on(void);
void zac_off(void);

typedef enum state {
	ZAC_READING_START_BIT_LOW,
	ZAC_READING_START_BIT_HIGH,
	ZAC_ACQUIRING_TSTROBE,
	ZAC_STROBE_ACQUIRED,
	ZAC_READING_LOW_BYTE,
	ZAC_READING_HIGH_BYTE,
} zacstate;

extern zacstate zstate;

#endif /* ZACWIRE_H_ */
