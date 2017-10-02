#include <assert.h>
#include <ipc.h>
#include <sel4/sel4.h>
#include <sos.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

pid_t sos_process_create(const char *path) {
	return (pid_t)SYSCALL_ARG1(SOS_SYSCALL_PROCESS_CREATE, (uint32_t)path);
}

int sos_process_delete(pid_t pid) {
	return SYSCALL_ARG1(SOS_SYSCALL_PROCESS_KILL, pid);
}

pid_t sos_process_wait(pid_t pid) {
	return (pid_t)SYSCALL_ARG1(SOS_SYSCALL_PROCESS_WAIT, pid);
}

pid_t sos_my_id(void) { return SYSCALL_ARG0(SOS_SYSCALL_PROCESS_MY_ID); }

int sos_process_status(sos_process_t *processes, unsigned max) {
	return SYSCALL_ARG2(SOS_SYSCALL_PROCESS_STATUS, (uint32_t)processes, max);
}
