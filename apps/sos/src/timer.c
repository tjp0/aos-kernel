/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <clock/clock.h>
#include "beep.h"
#include "timer.h"

#define verbose 0
#include <sys/debug.h>

#define TIMER_INITIAL_CAPACITY 256

typedef struct _timer_struct {
	struct _timer_struct *next_timer;
	timer_callback_t callback;
	void *data;
	uint64_t delay;
	uint32_t id;
#ifdef DEBUG_TIMER
	uint32_t debug_cookie;
#endif
} timer_struct;

typedef timer_struct *timer_id;

// have a pointer to the first free
static timer_struct *first_free;
static timer_struct *timer_list;
static beep_struct *beep;
static uint32_t del_count;

static void init_timer_list(void);
static void init_beep(void);
static timer_struct *create_timer_add_to_list(uint64_t delay,
											  timer_callback_t callback,
											  void *data);
static uint32_t add_timer_to_beep(timer_struct *timer);
static void free_timer(timer_struct *timer);
static timer_struct *timer_from_id(uint32_t id);
int is_timer_deleted(timer_struct *timer);

static void print_timer(timer_struct *timer, char *name);

static void fill_list(timer_struct *start, unsigned int num,
					  unsigned int offset);

#ifdef DEBUG_TIMER
uint32_t DEBUG_COOKIE = 0xb3b3000;
static inline void update_cookie(void) {
	for (int i = 0; i < node_capacity(beep); ++i) {
		timer_list[i].debug_cookie = DEBUG_COOKIE;
	}
}
#endif

void timer_init(void) {
	init_timer_list();
	init_beep();
	del_count = 0;
}

static void init_timer_list(void) {
	first_free =
		(timer_struct *)malloc(sizeof(timer_struct) * TIMER_INITIAL_CAPACITY);
	assert(first_free != NULL);
	timer_list = first_free;
	fill_list(first_free, TIMER_INITIAL_CAPACITY, 1);
#ifdef DEBUG_TIMER
	update_cookie();
#endif
}

static void fill_list(timer_struct *start, unsigned int num,
					  unsigned int offset) {
	timer_struct *current_timer = &(start[0]);
	int i = 0;
	for (i = 0; i < num - 1; ++i) {
		current_timer->next_timer = &(start[i + 1]);
		current_timer->callback = 0;
		current_timer->data = 0;
		current_timer->id = i + offset;  // don't have a 0 timer
		current_timer = current_timer->next_timer;
	}
	current_timer->next_timer = 0;
	current_timer->callback = 0;
	current_timer->data = 0;
	current_timer->id = i + offset;  // == TIMER_INITIAL_CAPACITY
}

static void init_beep(void) {
	beep = malloc(get_beep_size_for_n_nodes(TIMER_INITIAL_CAPACITY));
	assert(beep != NULL);
	make_beep(beep, TIMER_INITIAL_CAPACITY);
}

static int expand(void) {
	if (DEBUG) {
		dprintf(0, "EXPANDING\n");
	}

	/* Expand the beep */
	// double the number of nodes we can have
	unsigned int current_capacity = node_capacity(beep);
	unsigned int new_capacity = current_capacity * 2;
	beep_struct *realloced =
		realloc(beep, get_beep_size_for_n_nodes(new_capacity));
	if (realloced == NULL) {
		if (DEBUG) {
			dprintf(0, "Failed to expand\n");
		}
		return 0;
	}
	beep = realloced;

	assert(beep != NULL);
	update_size(beep, new_capacity);

	/* Expand the free list */
	// realloc the timer list and then update all the pointers in the free list
	timer_struct *old = timer_list;
	timer_struct *temp =
		realloc(timer_list, sizeof(timer_struct) * new_capacity);
	if (temp == NULL) {
		if (DEBUG) {
			dprintf(0, "Failed to expand\n");
		}
		return 0;
	}
	timer_list = temp;

#ifdef DEBUG_TIMER
	DEBUG_COOKIE++;
#endif

	// we need to fill current_capacity many slots
	// starting at current_capacity + 1 since the array starts at 1
	fill_list(&(timer_list[current_capacity]), current_capacity,
			  current_capacity + 1);

#ifdef DEBUG_TIMER
	update_cookie();
#endif

	// timer_list = first_free;
	// calculate where we have moved to from the realloc
	int64_t diff = ((uint8_t *)timer_list) - (uint8_t *)old;
	// update all the nodes
	// Note: In C, pointer arithmatic takes into account the size of the type
	timer_struct *cur = (timer_struct *)(((uint8_t *)first_free) + diff);
	while (cur && cur->next_timer) {
		cur->next_timer = (timer_struct *)(((uint8_t *)cur->next_timer) + diff);
		assert(cur == timer_from_id(cur->id));
		cur = cur->next_timer;
	}
	if (cur) {
		dprintf(0, "c\n");
		cur->next_timer = &(timer_list[current_capacity]);
		first_free = cur;
	} else {
		dprintf(0, "d\n");
		first_free = &(timer_list[current_capacity]);
	}
	dprintf(0, "ff == %p\n", first_free);

	cur = first_free;
	while (cur) {
		print_timer(cur, "all free timers");
		cur = cur->next_timer;
	}

	return 1;
}

static void print_timer(timer_struct *timer, char *name) {
#ifdef DEBUG_TIMER
	dprintf(0, "%s->debug_cookie = %u\n", name, timer->debug_cookie);
	assert(timer->debug_cookie == DEBUG_COOKIE);
#endif
	assert(timer == timer_from_id(timer->id));
	dprintf(0, "%s = %p\n", name, timer);
	dprintf(0, "%s->id = %d\n", name, timer->id);
	dprintf(0, "%s->data = %d\n", name, timer->data);
	dprintf(0, "%s->next_timer = %p\n", name, timer->next_timer);
}
#ifdef DEBUG_TIMER
static void print_list(void) {
	for (int i = 0; i < node_capacity(beep); ++i) {
		/* code */
		dprintf(0, "%d\n", timer_list[i].id);
	}
}
#endif

static timer_struct *create_timer_add_to_list(uint64_t delay,
											  timer_callback_t callback,
											  void *data) {
	if (first_free == NULL) {
		if (DEBUG) {
			dprintf(0, "NO TIMERS LEFT!!\n");
		}
		return 0;
	}

	print_timer(first_free, "first_free");
	if (!first_free->next_timer) {
		// The list is nearly full, so we should expand
		if (expand()) {
			dprintf(0, "Finished expanding\n");
			print_timer(first_free, "first_free");
		}
	}

	timer_struct *free_timer_struct = first_free;
	first_free = first_free->next_timer;

	print_timer(first_free, "first_free");

	assert(free_timer_struct->data == 0);
	free_timer_struct->delay = delay;
	free_timer_struct->callback = callback;
	free_timer_struct->data = data;
	free_timer_struct->next_timer = 0;

#ifdef DEBUG_TIMER
	print_list();
	print_timer(free_timer_struct, "free_timer_struct");
	print_timer(timer_from_id(free_timer_struct->id), "id_timer");
	assert(free_timer_struct->debug_cookie == DEBUG_COOKIE);
	assert(timer_from_id(free_timer_struct->id)->debug_cookie == DEBUG_COOKIE);
#endif
	assert(free_timer_struct == timer_from_id(free_timer_struct->id));

	return free_timer_struct;
}

static uint32_t add_timer_to_beep(timer_struct *timer) {
	// add the delay as the priority and it's id as the data item
	node_data nd = push(beep, timer->delay, (node_data)timer->id);
	if (timer_from_id(nd) != timer) {
		if (DEBUG) {
			print_timer(timer, "bad_push");
			print_timer(timer_from_id(nd), "timer_from_id");
			dprintf(0, "given :%d\n", timer->id);
			dprintf(0, "returned :%d\n", nd);
			dprintf(0, "FAILED TO PUSH\n");
		}
		return CLOCK_R_FAIL;
	}
	return 0;
}

/*
 * Register a callback to be called after a given delay
 *    delay:  Delay time in microseconds before callback is invoked
 *    callback: Function to be called
 *    data: Custom data to be passed to callback function
 *
 * Returns 0 on failure, otherwise a unique ID for this timeout
 */
uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data) {
	dprintf(0, "registering timer with delay %llu\n", delay);
	assert(callback != 0);
	delay += time_stamp();
	dprintf(0, "which will be called at %llu\n", delay);

	timer_struct *timer = create_timer_add_to_list(delay, callback, data);
	if (timer == 0) {
		return 0;
	}
	uint32_t res = add_timer_to_beep(timer);
	if (res == CLOCK_R_FAIL) {
		return 0;
	}
	node_data cur = peek(beep);
	timer_struct *soonest_timer = timer_from_id(cur);

	if (soonest_timer == timer) {
		dprintf(0, "updating soonest timer %d until %llu\n", cur, delay);
		epit2_sleepto(delay);
	}

	return timer->id;
}

static void free_timer(timer_struct *timer) {
#ifdef DEBUG_TIMER
	assert(timer->debug_cookie == DEBUG_COOKIE);
#endif
	timer->next_timer = first_free;
	first_free = timer;
	timer->callback = 0;
	timer->delay = 0;
	timer->data = 0;
	del_count++;
}

static timer_struct *timer_from_id(uint32_t id) {
	uint32_t index = id - 1;
	assert(index >= 0);
	assert(index < node_capacity(beep));
	return &(timer_list[index]);
}

/*
 * Remove a previously registered callback by its ID
 *    id: Unique ID returned by register_time
 * Returns CLOCK_R_OK iff successful.
 */
int remove_timer(uint32_t id) {
	timer_struct *timer = timer_from_id(id);
	if (is_timer_deleted(timer)) {
		return CLOCK_R_FAIL;
	}

	free_timer(timer);
	delete_element(beep, (node_data)timer->id);
	return CLOCK_R_OK;
}

/*
 * Handle an interrupt message sent to 'interrupt_ep' from start_timer
 *
 * Returns CLOCK_R_OK iff successful
 */
int timer_interrupt(void) {
	if (is_empty(beep)) {
		// this happens when you remove all timers
		return CLOCK_R_FAIL;
	}
	while (peek(beep) != -1 &&
		   timer_from_id(peek(beep))->delay <= time_stamp()) {
		node_data id = pop(beep);
		timer_struct *timer = timer_from_id(id);
		assert(timer->delay <= time_stamp());
		if (!is_timer_deleted(timer)) {
			timer->callback(id, timer->data);
		} else {
			del_count--;
		}
		/* Bugfix: If the timer list is expanded during the callback, this
		 * pointer may not be valid */
		timer = timer_from_id(id);
		free_timer(timer);
	}

	if (peek(beep) != -1) {
		epit2_sleepto(timer_from_id(peek(beep))->delay);
	}
	return CLOCK_R_OK;
}

int is_timer_deleted(timer_struct *timer) { return timer->callback == 0; }
