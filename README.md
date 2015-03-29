AVRTemperature
==============================

Measuring temperature with the ATMega8 and a TSIC 0306. This version is interrupt driven.

Hardware: ATMega8/16MHz
 
This program measures the temperature of a TSIC0306 temperature sensor, which is
connected to the INT1 pin. The sensor itself is powered via the ZACPIN defined
in zacwire.h

At the beginning the TSIC is powered on and when the first falling edge occurs the measurement
starts. Timer 1 is started in normal mode. When the first rising edge occurs the timer is being
stopped and the value from the TCNT1 register is taken as strobe value.

From now on on each falling edge of the INT1 pin the timer1 is started in ctc mode. Each time
the OCR1A register is equal to the timer value, the TIMER1_COMPA_vect interrupt service routine
is being triggered. This is exactly the 50% duty cycle time which is needed to detect the state
of each bit.

After measuring the MSB the parity bit and the stop bit are being skipped, then the measurement
for the LSB continues, at the end the parity bit is skipped again and the measurement is complete.
The whole thing is driven by a "state machine" like mechanism. This in fact not a real state machine,
since we have no events, rules or transitions.
 
This code doesn't represent the most efficient solution. The goal was to understand the zacwire protocol
and to write human readable code. Feel free to improve the code and give suggestions.

Usage
==============================
You must have avrdude and avr-gcc installed. Use an ATMega8@16MHz, given the attached schematic.

Program with:

make program

You'll have to adjust your programmer before. Don't mess up the fuses!

Schematics and board layout resides in the eagle folder.   