#pragma once
#include <libcoro.h>
void init_garbage(void);

/* Adds a coroutine to be garbage collected */
void add_garbage_coroutine(coro c);

/* SOS's init coro enters this; collects
 * provided coroutine garbage when it is safe
 * to do so */
void garbage_loop(void);
