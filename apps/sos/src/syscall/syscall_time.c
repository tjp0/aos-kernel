#include <process.h>
#include <sos_coroutine.h>

int syscall_usleep(struct process* process, int milliseconds) {
	return coro_sleep(milliseconds * 1000);
}