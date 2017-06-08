/*
 * ledif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef LEDIF_H_
#define LEDIF_H_
#include "typedefs.h"
#include "gpsif.h"

void led_initport(void);

static inline void led_off(void)
{
	P7OUT &= ~(BIT6 | BIT7);	//turn off leds
}

static inline void led_on_green(void)
{
	P7OUT |= BIT6;	//turn green led on
}

static inline void led_on_red(void)
{
	P7OUT |= BIT7;	//turn red led on
}

static inline void led_off_green(void)
{
	P7OUT &= ~BIT6;	//turn green led off
}

static inline void led_off_red(void)
{
	P7OUT &= ~BIT7;	//turn red led off
}

static inline void led_ok(void)
{
	led_off();
	P7OUT |= BIT6;	//turn green led on
}

static inline void led_error(void)
{
	led_off();
	P7OUT |= BIT7;	//turn red led on
}

static inline void led_swap_red(void)
{
	P7OUT ^= BIT7;	//turn red led on
}

static inline void led_swap_green(void)
{
	P7OUT ^= BIT6;	//turn red led on
}


static inline void led_flash_msg_long(U16 pulseCfg)
{
	gps_set_txbusy(true);
	TA3CCTL0 = 0; //disable CCIE flag
	TA3CTL = TACLR;
	if (pulseCfg > 0)
	{//for response longer than 1 second the Ublox answer in 1.1s ... count up and down
		TA3CCR0 = 0xffff;
		TA3EX0 = TAIDEX_7;
		TA3CTL = TASSEL__SMCLK | ID__8 | MC__UPDOWN | TAIE;        // SMCLK, start timer
	}else
	{
		TA3CCR0 = 0x6000;
		TA3EX0 = TAIDEX_7;
		TA3CTL = TASSEL__SMCLK | ID__8 | MC__UPDOWN | TAIE;        // SMCLK, start timer
	}
}

void led_flash_green_short(void);
void led_flash_green_long(void);
void led_flash_red_short(void);
void led_flash_red_long(void);

/*
void led_ok(void);
void led_error(void);
void led_off(void);
*/

#endif /* LEDIF_H_ */
