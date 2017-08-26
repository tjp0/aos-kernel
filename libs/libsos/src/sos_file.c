#include <assert.h>
#include <ipc.h>
#include <sel4/sel4.h>
#include <sos.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

int sos_stat(const char *path, sos_stat_t *buf) {
	assert(!"sos_stat not implemented");
	return -1;
}

int sos_getdirent(int pos, char *name, size_t nbyte) {
	assert(!"sos_getdirent not implemented");
	return -1;
}

int sos_sys_close(int file) {
	assert(!"sos_sys_close not implemented");
	return -1;
}

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
