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

#include "clock.h"
#include "beep.h"

#define MAX_TIMERS 100

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



void init_timer_list(void);
void init_beep(void);
timer_struct *create_timer_add_to_list(uint64_t delay, timer_callback_t callback, void *data);
uint32_t add_timer_to_beep(timer_struct *timer);
void free_timer(timer_struct * timer);
timer_struct * timer_from_id(uint32_t id);



/*
 * Initialise driver. Performs implicit stop_timer() if already initialised.
 *    interrupt_ep:       A (possibly badged) async endpoint that the driver
                          should use for deliverying interrupts to
 *
 * Returns CLOCK_R_OK iff successful.
 */
int start_timer(seL4_CPtr interrupt_ep){
	init_timer_list();
	init_beep();
	return CLOCK_R_OK;
}


void init_timer_list(void){
	first_free = (timer_struct *)malloc(sizeof(timer_struct) * MAX_TIMERS);
	timer_list = first_free;

	int i = 0;
	timer_struct *current_timer = &(first_free[0]);
	for (i = 0; i < MAX_TIMERS - 1; ++i) {
		current_timer->next_timer = &(first_free[i+1]);
		current_timer->callback = 0;
		current_timer->data = 0;
		current_timer->id = i+1; // don't have a 0 timer

		current_timer = current_timer->next_timer;
	}

	current_timer->next_timer = 0;
	current_timer->callback = 0;
	current_timer->data = 0;
	current_timer->id = i+1; // == MAX_TIMERS
}

void init_beep(void){
	beep_struct *beep = malloc(get_beep_size_for_n_nodes(MAX_TIMERS));
	make_beep(beep, MAX_TIMERS);
}

timer_struct *create_timer_add_to_list(uint64_t delay, timer_callback_t callback, void *data){
	timer_struct *free_timer_struct = first_free;
	if (free_timer_struct == 0){
		printf("NO TIMERS LEFT\n");
		return 0;
	}

	first_free = first_free->next_timer;

	free_timer_struct->delay = delay;
	free_timer_struct->callback = callback;
	free_timer_struct->data = data;
	free_timer_struct->next_timer = 0;

	return free_timer_struct;
}


uint32_t add_timer_to_beep(timer_struct *timer){
	// add the delay as the priority and it's id as the data item
	node_data nd = push(beep, timer->delay, (node_data)timer->id);
	if (timer_from_id(nd) != timer){
		printf("FAILED TO PUSH\n");
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
	timer_struct * timer = create_timer_add_to_list(delay, callback, data);
	if (timer == 0) {
		return 0;
	}
	uint32_t res = add_timer_to_beep(timer);
	if (res == 1) {
		return 0;
	}

	// now we do the bit where it's all
	// in blah seconds go call timer_interrupt

	// if this delay is smaller than our current smallest
	// deal with it. use peek or something

	return timer->id;
}


void free_timer(timer_struct * timer){
	timer->next_timer = first_free;
	first_free = timer;

	timer->callback = 0;
	timer->delay = 0;
	timer->data = 0;
}

timer_struct * timer_from_id(uint32_t id){
	uint32_t index = id - 1;
	assert(index >= 0);
	assert(index < MAX_TIMERS);
	return &(timer_list[index]);
}


/*
 * Remove a previously registered callback by its ID
 *    id: Unique ID returned by register_time
 * Returns CLOCK_R_OK iff successful.
 */
int remove_timer(uint32_t id){
	timer_struct * timer = timer_from_id(id);
	free_timer(timer);
	return CLOCK_R_OK;
}

/*
 * Handle an interrupt message sent to 'interrupt_ep' from start_timer
 *
 * Returns CLOCK_R_OK iff successful
 */
int timer_interrupt(void){
	if (is_empty(beep)){
		printf("NO TIMER IS REGISTERED! why am i being interupted!\n");
		return CLOCK_R_FAIL;
	}
	uint32_t id = pop(beep);
	timer_struct * timer = timer_from_id(id);
	timer->callback(id, timer->data);
	return CLOCK_R_OK;
}

/*
 * Returns present time in microseconds since booting.
 *
 * Returns a negative value if failure.
 */
timestamp_t time_stamp(void){
	return 0;
}

/*
 * Stop clock driver operation.
 *
 * Returns CLOCK_R_OK iff successful.
 */
int stop_timer(void){
	return 0;
}
