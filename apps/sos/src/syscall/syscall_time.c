#include <clock/clock.h>
#include <process.h>
#include <sos_coroutine.h>

#define verbose 0
#include <sys/debug.h>

int syscall_usleep(struct process* process, int milliseconds) {
	return coro_sleep(milliseconds * 1000);
}

int syscall_time_stamp(struct process* process, int64_t* timestamp) {
	int64_t ts = time_stamp();
	dprintf(0, "Requested timestamp %lld\n", ts);
	/* If there's an error, return the errorcode */
	if (ts < 0) {
		return 1;
	}
	*timestamp = ts;
	return 0;
}
