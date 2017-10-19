#include <libcoro.h>
#include <process.h>
#include <sos_coroutine.h>
#include <stdlib.h>
#include <sys/kassert.h>
#include <utils/list.h>
#define verbose 0
#include <sys/debug.h>
struct semaphore* event_garbage;
list_t garbage_coroutines;

void init_garbage(void) {
	event_garbage = semaphore_create();
	list_init(&garbage_coroutines);
}

/* Req: The coroutine should finish immediately after adding itself to this */
void add_garbage_coroutine(coro c) {
	dprintf(5, "Garbage added coro: %p\n", c);
	kassert(c != NULL);
	list_append(&garbage_coroutines, c);
	signal(event_garbage, NULL);
}

static int garbage_coroutine_clean(void* c) {
	kassert(c != NULL);
	coro co = (coro)c;
	regions_print(sos_process.vspace.regions);
	dprintf(5, "Garbage got coro: %p\n", co);
	if (coro_idle(co)) {
		dprintf(5, "Garbage freeing coro: %p\n", co);
		coroutine_free(co);
		return 1;
	}
	return 0;
}

void garbage_loop(void) {
	while (1) {
		wait(event_garbage);
		coro_sleep(0); /* A quick hack; this will yield immediately after
						  the event; gives the caller time to finish cleanup
						  if it's killing itself */
		dprintf(5, "Garbage alive\n");
		list_foreach_del(&garbage_coroutines, garbage_coroutine_clean);
	}
}
