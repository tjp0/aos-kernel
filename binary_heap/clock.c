/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include <clock/clock.h>
#include "timer.h"
#include "beep.h"

#define verbose 5
#include <sys/debug.h>

#define MAX_TIMERS 1


typedef struct _timer_struct {
	struct _timer_struct *next_timer;
	timer_callback_t callback;
	void *data;
	uint64_t delay;
	int id;
}timer_struct;


typedef timer_struct * timer_id;

// have a pointer to the first free 
static timer_struct *first_free;
static timer_struct *timer_list;
static beep_struct *beep;
static uint32_t del_count;

static void init_timer_list(void);
static void init_beep(void);
static timer_struct *create_timer_add_to_list(uint64_t delay, timer_callback_t callback, void *data);
static uint32_t add_timer_to_beep(timer_struct *timer);
static void free_timer(timer_struct * timer);
static timer_struct * timer_from_id(uint32_t id);
int is_timer_deleted(timer_struct* timer);

static void print_timer(timer_struct *timer, char *name);

static void fill_list(timer_struct *start, unsigned int num, unsigned int offset);

void timer_init(void){
	init_timer_list();
	init_beep();
	del_count = 0;
}

static void init_timer_list(void){
	first_free = (timer_struct *)malloc(sizeof(timer_struct) * MAX_TIMERS);
	assert(first_free != NULL);
	timer_list = first_free;
	fill_list(first_free, MAX_TIMERS, 1);
}


static void fill_list(timer_struct *start, unsigned int num, unsigned int offset){

	timer_struct *current_timer = &(start[0]);
	int i = 0;
	for (i = 0; i < num - 1; ++i) {
		current_timer->next_timer = &(start[i+1]);
		current_timer->callback = 0;
		current_timer->data = 0;
		current_timer->id = i+offset; // don't have a 0 timer

		current_timer = current_timer->next_timer;
	}

	current_timer->next_timer = 0;
	current_timer->callback = 0;
	current_timer->data = 0;
	current_timer->id = i+offset; // == MAX_TIMERS
}


static void init_beep(void){
	beep = malloc(get_beep_size_for_n_nodes(MAX_TIMERS));
	assert(beep != NULL);
	make_beep(beep, MAX_TIMERS);
}

static void expand(void){
	printf("EXPANDING\n");

	/* Expand the beep */
	// double the number of nodes we can have
	unsigned int current_capacity = node_capacity(beep);
	unsigned int new_capacity = current_capacity*2;
	beep = realloc(beep, get_beep_size_for_n_nodes(new_capacity));
	assert(beep != NULL);
	update_size(beep, new_capacity);

	/* Expand the free list */
	// realloc the timer list and then update all the pointers in the free list
	timer_struct *old = timer_list;
	timer_list = (timer_struct *)realloc(timer_list, sizeof(timer_struct) * new_capacity);
	assert(timer_list != NULL);
	// timer_list = first_free;
	// calculate where we have moved to from the realloc
	uint64_t diff = timer_list - old;
	// update all the nodes
	timer_struct *cur = first_free + diff;
	while (cur && cur->next_timer) {
		dprintf(0,"shifting %d\n", cur->id);
		cur->next_timer += diff;
		cur = cur->next_timer;
	}
	if (cur) {
		dprintf(0,"cur %d\n", cur->id);

		// we need to fill current_capacity many slots
		// starting at current_capacity + 1 since the array starts at 1
		fill_list(&(timer_list[current_capacity]), current_capacity, current_capacity + 1);
		cur->next_timer = &(timer_list[current_capacity]);
		first_free = cur;
	}else{
		first_free = &(timer_list[current_capacity]);
		dprintf(0,"fresh %d\n", first_free->id);
	}

	print_timer(first_free, "first_free_after_expand");
}

static void print_timer(timer_struct *timer, char *name){
	dprintf(0, "%s = %p\n", name, timer);
	dprintf(0, "%s->id = %d\n", name, timer->id);
	dprintf(0, "%s->data = %d\n", name, timer->data);
	dprintf(0, "%s->next_timer = %p\n", name, timer->next_timer);
}

static timer_struct *create_timer_add_to_list(uint64_t delay, timer_callback_t callback, void *data){
	timer_struct *free_timer_struct = first_free;
	print_timer(first_free, "first_free");
	if (free_timer_struct == NULL){
		dprintf(0,"NO TIMERS LEFT!!\n");
		return 0;
	}
	timer_struct *next_timer = first_free->next_timer;
	if (!next_timer){
		// The list is nearly full, so we should expand
		expand();
		next_timer = first_free->next_timer;
		free_timer_struct = first_free;
	}

	first_free = next_timer;

	free_timer_struct->delay = delay;
	free_timer_struct->callback = callback;
	free_timer_struct->data = data;
	free_timer_struct->next_timer = 0;

	return free_timer_struct;
}


static uint32_t add_timer_to_beep(timer_struct *timer){
	// add the delay as the priority and it's id as the data item
	node_data nd = push(beep, timer->delay, (node_data)timer->id);
	printf("nd: (%u)\n",nd);
	if (timer_from_id(nd) != timer){
		dprintf(0,"FAILED TO PUSH\n");
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
uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data){
	assert(callback != 0);
	delay *= 1000;
	delay += time_stamp();
	timer_struct * timer = create_timer_add_to_list(delay, callback, data);
	if (timer == 0) {
		return 0;
	}
	uint32_t res = add_timer_to_beep(timer);
	if (res == CLOCK_R_FAIL) {
		return 0;
	}
	node_data cur = peek(beep);
	timer_struct* soonest_timer = timer_from_id(cur);
	
	if(soonest_timer == timer) {
		epit2_sleepto(delay);
	}

	return timer->id;
}


static void free_timer(timer_struct * timer){
	timer->next_timer = first_free;
	first_free = timer;
	timer->callback = 0;
	timer->delay = 0;
	timer->data = 0;
	del_count++;
}

static timer_struct * timer_from_id(uint32_t id){
	printf("ID: (%u)\n",id);
	uint32_t index = id - 1;
	if (DEBUG) {
		printf("(%u)/%d/%d\n",index, num_nodes(beep), node_capacity(beep));
	}
	assert(index >= 0);
	assert(index < node_capacity(beep));
	return &(timer_list[index]);
}


/*
 * Remove a previously registered callback by its ID
 *    id: Unique ID returned by register_time
 * Returns CLOCK_R_OK iff successful.
 */
int remove_timer(uint32_t id){
	timer_struct * timer = timer_from_id(id);
	print_timer(timer, "timer_to_be_deleted");
	if (is_timer_deleted(timer)){
		return CLOCK_R_FAIL;
	}

	free_timer(timer);
	print_timer(timer, "timer_deleted");
	delete_element(beep, (node_data)timer->id);
	return CLOCK_R_OK;
}

/*
 * Handle an interrupt message sent to 'interrupt_ep' from start_timer
 *
 * Returns CLOCK_R_OK iff successful
 */
int timer_interrupt(void){
	if (is_empty(beep)){
		dprintf(0,"NO TIMER IS REGISTERED! why am i being interupted!\n");
		return CLOCK_R_FAIL;
	}
	while(peek(beep) != -1 && timer_from_id(peek(beep))->delay <= time_stamp())
	{
		node_data id = pop(beep);
		timer_struct * timer = timer_from_id(id);
		assert(timer->delay <= time_stamp());
		if (!is_timer_deleted(timer)){
			timer->callback(id, timer->data);
		}else{
			del_count--;
		}
		free_timer(timer);
	}

	if(peek(beep) != -1) {
		epit2_sleepto(timer_from_id(peek(beep))->delay);
	}
	return CLOCK_R_OK;
}

int is_timer_deleted(timer_struct* timer){
	return timer->callback == 0;
}
