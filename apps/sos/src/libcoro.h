/*
 * libcoro - Modified version of picoro for a near drop-in replacement for SOS.
 * Uses some code/API from picoro
 */

#pragma once
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
struct process;
struct coroutine {
	jmp_buf context;
	struct coroutine *next;
	void *stack_head;
	struct process *process;
	bool idle;
	uint32_t debug;
};

typedef struct coroutine *coro;
/* Creates a new coroutine that can be passed
 * to coroutine_start */
coro coroutine_create(struct process *process);

/* Frees a coroutine */
void coroutine_free(coro c);
/*
 * Initializes a coroutine (c) to run a function
 * resume it to start
 */
coro coroutine_start(coro c, void *fun(void *arg));

/* Return the currently running coroutine */
coro current_coro(void);
/* Return the process this coro is running under */
struct process *current_process(void);

/* Returns if the coro is idle */
bool coro_idle(coro c);

/*
 * Returns false when the coroutine has run to completion
 * or when it is blocked inside resume().
 */
int resumable(coro c);

/*
 * Transfer control to another coroutine. The second argument is returned by
 * yield() inside the target coroutine (except for the first time resume() is
 * called). A coroutine that is blocked inside resume() is not resumable.
 */
void *resume(coro c, void *arg);

/*
 * Transfer control back to the coroutine that resumed this one. The argument
 * is returned by resume() in the destination coroutine. A coroutine that is
 * blocked inside yield() may be resumed by any other coroutine.
 */
void *yield(void *arg);
