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
 *      Description: Simple milestone 0 code.
 *      		     Libc will need sos_write & sos_read implemented.
 *
 *      Author:      Ben Leslie
 *
 ****************************************************************************/

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "ttyout.h"

#include <ipc.h>
#include <sel4/sel4.h>

void ttyout_init(void) { /* Perform any initialisation you require here */
}

size_t sos_write(void *vData, size_t count) {
	// implement this to use your syscall

	struct ipc_command ipc = ipc_create();

	ipc_packi(&ipc, 1);
	size_t length = ipc_maxs(&ipc);
	if (length > count) {
		length = count;
	}

	ipc_packs(&ipc, length, vData);
	ipc_call(&ipc, SYSCALL_ENDPOINT_SLOT);

	return length;
	// return sos_debug_print(vData, count);
}

size_t sos_read(void *vData, size_t count) {
	// implement this to use your syscall
	return 0;
}
