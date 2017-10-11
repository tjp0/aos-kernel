/*
 * libcoro - Modelled off picoro for a near drop-in replacement for SOS.
 */

#pragma once
#include <stdint.h>

typedef struct coroutine *coro;

/*
 * Create a coroutine that will run fun(). The coroutine starts off suspended.
 * When it is first resumed, the argument to resume() is passed to fun().
 * If fun() returns, its return value is returned by resume() as if the
 * coroutine yielded, except that the coroutine is then no longer resumable
 * and may be discarded.
 */
coro coroutine(void *fun(void *arg));

/* Return the number of the stack currently in use */
int current_coro_num(void);

/* Return the currently running coroutine */
coro current_coro(void);

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
