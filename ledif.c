/*
 * ledif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <msp430.h>
#include "ledif.h"

void led_initport(void)
{
	P7DIR |= BIT6 | BIT7;                     // Set P7.6 and P7.7 to output direction
	P7OUT  = BIT6 | BIT7;				      // Set logic '1' to 7.6 and P7.7
	led_off();
}

void led_flash_green_short(void)
{
	led_on_green();
	TA0CTL = TASSEL__SMCLK | ID__1 | MC__UP;        // SMCLK, start timer
}

void led_flash_green_long(void)
{
	led_on_green();
	TA0CTL = TASSEL__SMCLK | ID__8 | MC__UP;        // SMCLK, start timer
	TA0EX0 = TAIDEX_7;
}

void led_flash_red_long(void)
{
	led_on_red();
	TA1CTL = TASSEL__SMCLK | ID__8 | MC__UP;        // SMCLK, start timer
	TA1EX0 = TAIDEX_7;
}
