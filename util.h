/*
 * util.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef UTIL_H_
#define UTIL_H_
#include "typedefs.h"

//useful when reading output from console
//#define OUTPUT_PRINT_HEX

/** @brief Converts number from 0 to 15
 * to ASCII hex 0 to F */
#pragma FUNC_ALWAYS_INLINE(util_num2hex)
inline U8 util_num2hex(U8 * innum)
{
	//takes only numbers 0 to 15!!!
	U8 retval;

	retval = *innum;
	if (retval <= 9)
	{
		retval += 48; //ASCII 0-9
	}else
	{
		retval += 55; //ASCII A-F
	}

	return retval;
}

#endif /* UTIL_H_ */
