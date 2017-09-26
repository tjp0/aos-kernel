#pragma once

#include <stdint.h>
#include <utils/picoro.h>

struct lock;

int coro_sleep(uint64_t delay);

int lock(struct lock *lock);
int unlock(struct lock *lock);
struct lock *lock_create(void);
void lock_destroy(struct lock *l);
