/*
 * ledif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef LEDIF_H_
#define LEDIF_H_
#include "typedefs.h"

void led_initport(void);

static inline void led_off(void)
{
	P6OUT &= ~(BIT6 | BIT5);	//turn off leds
}

static inline void led_ok(void)
{
	led_off();
	P6OUT |= BIT6;	//turn green led on
}

static inline void led_error(void)
{
	led_off();
	P6OUT |= BIT5;	//turn red led on
}

static inline void led_swap_red(void)
{
	P6OUT ^= BIT5;	//turn red led on
}

static inline void led_swap_green(void)
{
	P6OUT ^= BIT6;	//turn red led on
}

/*
void led_ok(void);
void led_error(void);
void led_off(void);
*/

#endif /* LEDIF_H_ */
