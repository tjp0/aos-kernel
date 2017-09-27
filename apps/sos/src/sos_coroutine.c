#include <clock/clock.h>
#include <sos_coroutine.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/debug.h>
#include <sys/kassert.h>
#include <utils/list.h>

#define verbose 0

#define UNLOCKED 0
#define LOCKED 1

static int resume_coro(void *coro);

/* check if coro is already waiting on a lock */
static int is_waiting_on(struct lock *l, coro c);

/* coro to wait on a lock */
static int wait_on(struct lock *l, coro c);

/* returns true if (a == b) */
static int cmp_equality(void *a, void *b);

/* returns 1 if coro is locked, 0 if not locked. -1 on error */
static int is_locked(struct lock *lock);

struct lock {
	bool locked;
	list_t *coros_waiting; /* list of coros which are blocked on this lock */
};

struct signal {};

void coro_sleep_awaken(uint32_t id, void *data) { resume((coro)data, 0); }

int coro_sleep(uint64_t delay) {
	if (!register_timer(delay, coro_sleep_awaken, current_coro())) {
		return -1;
	}
	yield(0);
	return 0;
}

int lock(struct lock *l) {
	if (is_locked(l)) {
		trace(5);
		wait_on(l, current_coro());
		yield(0);
		trace(5);
	}

	kassert(l->locked == UNLOCKED);

	l->locked = LOCKED;

	return 0;
}

int unlock(struct lock *l) {
	int err;
	l->locked = UNLOCKED;

	trace(5);

	coro c = (coro)list_pop(l->coros_waiting);
	dprintf(5, "current coro: %p, popped: %p\n", (void *)current_coro(),
			(void *)c);
	trace(5);
	if (c) {
		resume(c, 0);
		trace(5);
		// list_remove(l, c, cmp_equality);
	}
	trace(5);

	// /* resume any coros which are blocked on it */
	// err = list_foreach(lock->coros_waiting, resume_coro);
	// if (err) {
	// 	return -1;
	// }

	// /* remove all previously blocked coros */
	// err = list_remove_all(lock->coros_waiting);
	// if (err) {
	// 	return -1;
	// }

	return 0;
}

/* check if coro is already waiting on a lock */
static int is_waiting_on(struct lock *l, coro c) {
	trace(5);
	return list_exists(l->coros_waiting, (void *)c, cmp_equality);
}

/* coro to wait on a lock */
static int wait_on(struct lock *l, coro c) {
	trace(5);
	return list_append(l->coros_waiting, (void *)c);
}

/* function passed as an argument to list_foreach() during unlock() */
static int resume_coro(void *coro) {
	trace(5);
	resume(coro, 0);
	trace(5);
	return 0;
}

static int cmp_equality(void *a, void *b) { return !(a == b); }

/* returns 1 if lock is locked, -1 on error */
static int is_locked(struct lock *lock) {
	if (!lock) {
		return -1;
	}

	return lock->locked;
}

/* returns NULL on error */
struct lock *lock_create(void) {
	/* create new lock */
	struct lock *l = malloc(sizeof(struct lock));

	if (!l) {
		return NULL;
	}

	l->locked = UNLOCKED;
	l->coros_waiting = malloc(sizeof(list_t));

	if (!l->coros_waiting) {
		free(l);
		return NULL;
	}

	list_init(l->coros_waiting);

	return l;
};

void lock_destroy(struct lock *l) {
	l->locked = UNLOCKED;
	list_remove_all(l->coros_waiting);
	list_destroy(l->coros_waiting);
	free(l);
}
