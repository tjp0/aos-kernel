#include <assert.h>
#include <ipc.h>
#include <sel4/sel4.h>
#include <sos.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

int sos_stat(const char *path, sos_stat_t *buf) {
	return SYSCALL_ARG2(SOS_SYSCALL_STAT, (int)path, (int)buf);
}

int sos_getdirent(int pos, char *name, size_t nbyte) {
	return SYSCALL_ARG3(SOS_SYSCALL_GETDIRENT, pos, name, nbyte);
}

int sos_sys_close(int file) { return SYSCALL_ARG1(SOS_SYSCALL_CLOSE, file); }

size_t sos_write(void *vData, size_t count) {
	// implement this to use your syscall

	struct ipc_command ipc = ipc_create();

	ipc_packi(&ipc, SOS_SYSCALL_SERIALWRITE);
	ipc_packi(&ipc, (seL4_Word)vData);
	ipc_packi(&ipc, count);
	ipc_call(&ipc, SOS_IPC_EP_CAP);

	return count;
	// return sos_debug_print(vData, count);
}

size_t sos_read(void *vData, size_t count) {
	// implement this to use your syscall
	return 0;
}
