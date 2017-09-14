#pragma once
#include <stdint.h>
int coro_sleep(uint64_t delay);

struct lock;

void lock(struct lock* lock);

void unlock(struct lock* lock);

struct lock* lock_create(void);
