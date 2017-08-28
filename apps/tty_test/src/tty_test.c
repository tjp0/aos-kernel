/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/****************************************************************************
 *
 *      $Id:  $
 *
 *      Description: Simple milestone 0 test.
 *
 *      Author:			Godfrey van der Linden
 *      Original Author:	Ben Leslie
 *
 ****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <inttypes.h>
#include <ipc.h>
#include <sel4/sel4.h>
#include <sos.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ttyout.h"

int main(void) {
	/* initialise communication */

	pt_test();
	char* array = alloca(1000);

	for (char i = 0; i < 255; i++) {
		array[(int)i] = i;
	}
	for (char i = 0; i < 255; i++) {
		assert(array[(int)i] == i);
	}

	printf("Running buf test\n");
	test_buffers();

	do {
		printf("task:\tHello world, I'm\ttty_test!\n");
		// printf("World hole");
		int64_t microseconds = sos_sys_time_stamp();
		int64_t seconds = microseconds / (1000 * 1000);
		printf("Recieved timestamp: %lld (%lld seconds since boot)\n",
			   microseconds, seconds);
		sos_sys_usleep(1000);  // Implement this as a syscall
	} while (1);

	return 0;
}
