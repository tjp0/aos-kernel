#pragma once

#include <stdint.h>
#include <utils/picoro.h>

struct lock;
struct semaphore;

int coro_sleep(uint64_t delay);

int lock(struct lock *lock);
int unlock(struct lock *lock);
struct lock *lock_create(void);
void lock_destroy(struct lock *l);

struct semaphore *semaphore_create(coro c);
int semaphore_destroy(struct semaphore *s);
int signal(struct semaphore *s);
int wait(struct semaphore *s);
