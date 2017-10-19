#pragma once

#include <libcoro.h>
#include <stdbool.h>
#include <stdint.h>
// Toggle semaphore
// you create a semaphore
// and other coroutines wait() until you signal()

struct lock;
struct semaphore;
int coro_sleep(uint64_t delay);
struct lock *lock_create(const char *string);
void lock_destroy(struct lock *l);
int lock(struct lock *lock);
int unlock(struct lock *lock);
bool lock_owned(struct lock *lock);

struct semaphore *semaphore_create(void);
int semaphore_destroy(struct semaphore *s);
int signal(struct semaphore *s, void *val);
void *wait(struct semaphore *s);

#define LOCK(l)                                                 \
	do {                                                        \
		lock(l);                                                \
		if (verbose > 4) {                                      \
			printf("<%s:%u>: %s", current_process()->name,      \
				   current_process()->pid, #l);                 \
			printf(" locked by <%s:%u>\n", __func__, __LINE__); \
		}                                                       \
	} while (0)

#define UNLOCK(l)                                                 \
	do {                                                          \
		if (verbose > 4) {                                        \
			printf("<%s:%u>: %s", current_process()->name,        \
				   current_process()->pid, #l);                   \
			printf(" unlocked by <%s:%u>\n", __func__, __LINE__); \
		}                                                         \
		unlock(l);                                                \
	} while (0)
