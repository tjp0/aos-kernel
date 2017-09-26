#pragma once

#include <stdint.h>
#include <utils/picoro.h>

struct lock;

int coro_sleep(uint64_t delay);

int lock(struct lock *lock);
int unlock(struct lock *lock);

/* check if coro is already waiting on a lock */
int is_waiting_on(struct lock *l, coro c);

/* coro to wait on a lock */
int wait_on(struct lock *l, coro c);

int resume_coro(void *coro);
int find_lock(void *a, void *b);

/* returns 1 if coro is locked, 0 if not locked. -1 on error */
int is_locked(struct lock *lock);

struct lock *lock_create(void);
