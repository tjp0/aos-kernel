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
	return SYSCALL_ARG3(SOS_SYSCALL_GETDIRENT, pos, (int)name, nbyte);
}

int sos_sys_close(int file) { return SYSCALL_ARG1(SOS_SYSCALL_CLOSE, file); }
