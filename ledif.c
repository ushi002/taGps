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
