#include <clock/clock.h>
#include <sos_coroutine.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <utils/list.h>

#define UNLOCKED 0
#define LOCKED 1

struct lock {
	bool locked;
	list_t *coros_waiting; /* list of coros which are blocked on this lock */
};

/* linked list of locks */
list_t *lock_list;

void coro_sleep_awaken(uint32_t id, void *data) { resume((coro)data, 0); }

int coro_sleep(uint64_t delay) {
	if (!register_timer(delay, coro_sleep_awaken, current_coro())) {
		return -1;
	}
	yield(0);
	return 0;
}

int lock(struct lock *lock) {
	int err;

	if (is_locked(lock)) {
		/* already locked, check that it exists in the list */
		if (!list_exists(lock_list, lock, find_lock)) {
			err = list_append(lock_list, (void *)lock);
			if (err) {
				return -1;
			}
		}

		return 0;
	}

	/* set as locked, insert into linked list */
	lock->locked = LOCKED;

	err = list_append(lock_list, lock);
	if (err) {
		lock->locked = UNLOCKED;
		return -1;
	}

	return 0;
}

int unlock(struct lock *lock) {
	int err;
	lock->locked = UNLOCKED;

	/* resume any coros which are blocked on it */
	err = list_foreach(lock->coros_waiting, resume_coro);
	if (err) {
		return -1;
	}

	/* remove all previously blocked coros */
	err = list_remove_all(lock->coros_waiting);
	if (err) {
		return -1;
	}

	/* remove from lock_list. do this at the end since it inherently free's */
	err = list_remove(lock_list, (void *)lock, find_lock);

	return err;
}

/* check if coro is already waiting on a lock */
int is_waiting_on(struct lock *l, coro c) {
	return list_exists(lock_list, (void *)c, find_lock);
}

/* coro to wait on a lock */
int wait_on(struct lock *l, coro c) {
	return list_append(l->coros_waiting, (void *)c);
}

/* function passed as an argument to list_foreach() during unlock() */
int resume_coro(void *coro) {
	resume(coro, 0);
	return 0;
}

int find_lock(void *a, void *b) { return !(a == b); }

/* returns 1 if lock is locked, -1 on error */
int is_locked(struct lock *lock) {
	if (!lock) {
		return -1;
	}

	return lock->locked;
}

/* returns NULL on error */
struct lock *lock_create(void) {
	/* lazily create linked list. not to be free'd */
	if (!lock_list) {
		lock_list = malloc(sizeof(list_t));
		if (!lock_list) {
			return NULL;
		}

		list_init(lock_list);
	}

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

	/* add it to lock_list */
	list_append(lock_list, (void *)l);

	return l;
};
