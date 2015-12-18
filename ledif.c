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
	P6DIR |= BIT5 | BIT6;                     // Set P6.0 to output direction
	P6OUT  = BIT5 | BIT6;
	led_off();
}
