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

/* passed to the list library in foreach() */
static int resume_coro(void *coro, void *var);

/* returns true if (a == b) */
static int cmp_equality(void *a, void *b);

/* returns 1 if coro is locked, 0 if not locked. -1 on error */
static int is_locked(struct lock *lock);

struct lock {
	bool locked;
	list_t *coros_waiting; /* list of coros which are blocked on this lock */
};

struct semaphore {
	list_t *coros_waiting;
};

/*
 * coro sleep
 */

void coro_sleep_awaken(uint32_t id, void *data) { resume((coro)data, 0); }

int coro_sleep(uint64_t delay) {
	if (!register_timer(delay, coro_sleep_awaken, current_coro())) {
		return -1;
	}
	yield(0);
	return 0;
}

/*
 * semaphore primitives
 */

struct semaphore *semaphore_create(void) {
	struct semaphore *s = malloc(sizeof(struct semaphore));
	if (!s) {
		return NULL;
	}

	s->coros_waiting = malloc(sizeof(list_t));

	if (!s->coros_waiting) {
		free(s);
		return NULL;
	}

	list_init(s->coros_waiting);

	return s;
}

int semaphore_destroy(struct semaphore *s) {
	list_remove_all(s->coros_waiting);
	free(s->coros_waiting);
	free(s);
	return 0;
}

int signal(struct semaphore *s, void *val) {
	/* resume coros */
	/* return a value to each of them from the signaller*/
	list_foreach_var(s->coros_waiting, resume_coro, val);
	list_remove_all(s->coros_waiting);
	return 0;
}

/* Retuns the value sent by the signaller*/
void *wait(struct semaphore *s) {
	list_append(s->coros_waiting, current_coro());
	return yield(0);
}

/*
 * lock primitives
 */

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
	free(l->coros_waiting);
	free(l);
}

int lock(struct lock *l) {
	if (is_locked(l)) {
		trace(5);
		list_append(l->coros_waiting, current_coro());
		yield(0);
		trace(5);
	}

	kassert(l->locked == UNLOCKED);

	l->locked = LOCKED;

	return 0;
}

/* lock becomes unlocked. resume the next coro available */
int unlock(struct lock *l) {
	l->locked = UNLOCKED;

	trace(5);

	coro c = (coro)list_pop(l->coros_waiting);
	dprintf(5, "current coro: %p, popped: %p\n", (void *)current_coro(),
			(void *)c);
	trace(5);
	if (c) {
		resume(c, 0);
		trace(5);
	}
	trace(5);

	return 0;
}

/*
 * helpers
 */

/* function passed as an argument to list_foreach() during unlock() */
static int resume_coro(void *coro, void *var) {
	trace(5);
	dprintf(0, "coro %p\n", coro);
	resume(coro, var);
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
