/*
 * spiif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef SPIIF_H_
#define SPIIF_H_
#include "typedefs.h"

#define MEM_MAP_SIZE 	24

extern const U8 * spiif_pmmap;

void spi_init(void);

#endif /* SPIIF_H_ */
