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
	//PORT2:
	//BIT2 - PWR-GPS-SENSE
	//BIT1 - GPS-TIMEPULSE
	P2DIR = 0xFF ^ (BIT2 | BIT1);             // Set all but P2.2 and P2.1 to output direction
	P2REN = BIT2 | BIT1;                      // Pull resistor enable for P2.2, 2.1
	P2OUT = 0;                                // Pull-down resistor on P2.x

	P2IES = 0;                                // P2.x Lo-to-Hi edge
	P2IE = 0;                                 // P2.x interrupt disable
	P2IE |= BIT2;                             // P2.2 interrupt enaable - GPS-POWER
	P2IE &= ~BIT1;                            // P2.1 interrupt disable
	P2IFG = 0;                                // Clear all P2.x interrupt flags

	//PORT3:
	//P3BIT7 - SW-CFG-GPS
	//P3BIT2 - PWR-USB-SENSE
	P3DIR = 0xFF ^ (BIT7 | BIT2); // Set all but P3.7 and 3.2 to output direction
	P3REN = BIT7 | BIT2;        // Pull resistor enable for P3.7, 3.6 and 3.2
	P3OUT = 0;                         // Pull-down resistor on P3.x

	P3IES = 0;                                // P3.x Lo-to-Hi edge

	//enable interrupt for yellow button SW-CFG-GPS
	P3IE |= BIT7;

	P3IE |= BIT2;							// P3.2 interrupt enable - USB-POWER

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

	if (P3IN & BIT7)
	{
		dbg_txmsg("\nYellow pin is HIGH");
		but_yel_pres++;
		led_swap_green();
	}else
	{
		but_yel_pres = 0;
	}

	if (P3IN & BIT6)
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
