#include <assert.h>
#include <sos.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <sel4/sel4.h>

pid_t sos_process_create(const char *path) {
	assert(!"sos_process_create not implemented");
	return -1;
}

int sos_process_delete(pid_t pid) {
	assert(!"sos_process_delete not implemented");
	return -1;
}

pid_t sos_process_wait(pid_t pid) {
	assert(!"sos_process_wait not implemented");
	return -1;
}

int sos_process_status(sos_process_t *processes, unsigned max) {
	assert(!"sos_process_status not implemented");
	return -1;
}