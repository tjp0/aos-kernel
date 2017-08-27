#pragma once
#include <sos_coroutine.h>

int syscall_usleep(struct process* process, int milliseconds);

int syscall_time_stamp(struct process* process, int64_t* timestamp);