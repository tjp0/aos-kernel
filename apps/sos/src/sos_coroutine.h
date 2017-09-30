#pragma once

#include <stdint.h>
#include <utils/picoro.h>

struct lock;
struct semaphore;

int coro_sleep(uint64_t delay);

struct lock *lock_create(void);
void lock_destroy(struct lock *l);
int lock(struct lock *lock);
int unlock(struct lock *lock);

struct semaphore *semaphore_create(void);
int semaphore_destroy(struct semaphore *s);
int signal(struct semaphore *s);
int wait(struct semaphore *s);
