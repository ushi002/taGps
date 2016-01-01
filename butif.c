/*
 * butif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <msp430.h>
#include "butif.h"
#include "dbgif.h"
#include "ledif.h"

static void initport(void)
{
	//BIT1 is GPS-TIMEPULSE!!!
	P2DIR = 0xFF ^ (BIT0 | BIT1);             // Set all but P2.0 and P2.1 to output direction
	P2REN = BIT1 | BIT0;                      // Pull resistor enable for P2.0, 2.1
	P2OUT = 0;                                // Pull-down resistor on P2.x

	P2IES = 0;                                // P2.0 Lo/Hi edge
	P2IE = BIT0;                              // P2.0 interrupt enable
	P2IFG = 0;                                // Clear all P2 interrupt flags

	P7DIR = 0xFF ^ (BIT0);             // Set all but P7.0 to output direction
	P7REN = BIT0;                      // Pull resistor enable for P7.0
	P7OUT = 0;                         // Pull-down resistor on P7.0

}

void but_init(void)
{
	//initialize port:
	initport();

}

void but_check(void)
{
	const U16 HOLD_BUTT_TO_RESET = 5;
	static U16 but_yel_pres = 0;
	static U16 but_bla_pres = 0;
	static Boolean ready_to_reset = false;

	if (P7IN & BIT0)
	{
		dbg_txmsg("\nYellow pin is HIGH");
		but_yel_pres++;
		led_swap_green();
	}else
	{
		but_yel_pres = 0;
	}

	if (P2IN & BIT0)
	{
		dbg_txmsg("\nBlack pin is HIGH");
		but_bla_pres++;
		led_swap_red();
	}else
	{
		but_bla_pres = 0;
	}

	if (but_yel_pres == HOLD_BUTT_TO_RESET &&
			but_bla_pres == HOLD_BUTT_TO_RESET)
	{
		ready_to_reset = true;
	}

	if (ready_to_reset)
	{
		if (but_yel_pres == 0 &&
				but_bla_pres == 0)
		{
			dbg_txmsg("\nRESET MEM!");
			ready_to_reset = false;
		}
		if (but_yel_pres == 6 ||
						but_bla_pres == 6)
		{
			ready_to_reset = false;
		}
	}
}
