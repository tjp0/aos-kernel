/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <sos.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <utils/endian.h>

#include <sel4/sel4.h>

int sos_sys_open(const char *path, fmode_t mode) {
	// assert(!"You need to implement this");
	return 0;
}

int sos_sys_read(int file, char *buf, size_t nbyte) {
	// assert(!"You need to implement this");
	return 0;
}

int sos_sys_write(int file, const char *buf, size_t nbyte) {
	assert(!"You need to implement this");
	return -1;
}

void sos_sys_usleep(int msec) { SYSCALL_ARG1(SOS_SYSCALL_USLEEP, msec); }

int64_t sos_sys_time_stamp(void) {
	struct ipc_command ipc = ipc_create();
	ipc_packi(&ipc, SOS_SYSCALL_TIMESTAMP);
	int err = ipc_call(&ipc, SOS_IPC_EP_CAP);
	if (err < 0) {
		return -1;
	}

	uint32_t a;
	uint32_t b;

	ipc_unpacki(&ipc, &a);
	ipc_unpacki(&ipc, &b);

	uint64_t ret;
	join32to64(a, b, &ret);
	return (int64_t)ret;
}
size_t sos_debug_print(const void *vData, size_t count) {
	size_t i;
	const char *realdata = vData;
	for (i = 0; i < count; i++) seL4_DebugPutChar(realdata[i]);
	return count;
}