#pragma once
#include <libcoro.h>
void init_garbage(void);
void add_garbage_coroutine(coro c);
void garbage_loop(void);
