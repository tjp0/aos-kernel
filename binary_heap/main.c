/*
 * Test the binary heap data structure
 * by jordan
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <clock/clock.h>
#include "beep.h"

#define RANDOM_WALK_NUM 1000

#define RANDOM_MOD 1000

void test_beep(void);
void test_size(int nodes);
void fill_pop(beep_struct *beep, int num);
void push_n_rand(beep_struct *beep, int num);
void pop_n_rand(beep_struct *beep, int size);

void test_beep(void) {
	// Test in powers of 2
	for (int i = 0; i < 10; ++i) {
		// +- 5
		for (int j = -5; j < 5; ++j) {
			int size = (2 << i) + j;
			if (size >= 0) {
				// printf("testing size %d\n", size);
				test_size(size);
			}
		}
	}
}

// push num many things then pop them off
void fill_pop(beep_struct *beep, int num) {
	assert(is_empty(beep));
	push_n_rand(beep, num);

	assert(num_nodes(beep) == num);

	pop_n_rand(beep, num);
	assert(is_empty(beep));
}

// push n random things to beep
void push_n_rand(beep_struct *beep, int size) {
	int i = 0;
	for (i = 0; i < size; ++i) {
		node_data num = (node_data)((uint64_t)rand() % RANDOM_MOD);
		push(beep, (int)(num), num);
	}
}

// pop n random things to beep and check that they come off in decending order
void pop_n_rand(beep_struct *beep, int size) {
	node_data prev = 0;
	int i = 0;
	for (i = 0; i < size; ++i) {
		node_data next = pop(beep);
		assert(next >= prev);
		assert(next < RANDOM_MOD);
		prev = next;
	}
}

// push and then pop on an empty beep
void push_pop(beep_struct *beep, int num) {
	push(beep, num, (node_data)num);
	node_data next = pop(beep);
	assert(next == num);
	assert(next < RANDOM_MOD);
}

void test_size(int nodes) {
	beep_struct *beep = malloc(get_beep_size_for_n_nodes(nodes));
	make_beep(beep, nodes);

	srand(time(NULL));
	int i = 0;
	int max = nodes;
	int count = max;

	// printf("Testing fill pop\n");
	fill_pop(beep, nodes);

	// printf("Testing push pop\n");
	for (i = 0; i < count * 2; ++i) {
		int num = rand() % RANDOM_MOD;
		push_pop(beep, num);
	}

	// printf("Testing random walk\n");

	// do a random walk of pushing and popping
	// keep track of what nums are pushed and popped
	char push_list[RANDOM_WALK_NUM] = {0};
	char pop_list[RANDOM_WALK_NUM] = {0};

	for (i = 0; i < 10000; ++i) {
		int decision = rand() % 2;
		if (decision) {
			// if (!is_full(beep)){
			int pri = rand() % RANDOM_MOD;
			int num = rand() % RANDOM_WALK_NUM;
			int res = push(beep, pri, (node_data)num);
			// printf("res %d\n", res);
			if (res >= 0) {
				// printf("PUSHING %d\n", num);
				push_list[(uint64_t)num]++;
			}
			// }
		} else {
			// if (!is_empty(beep)){
			node_data num = pop(beep);
			// printf("NUM %d\n", num);
			if ((signed)num >= 0) {  // huh wow, did not know I could do that
				assert(num < RANDOM_WALK_NUM);
				// printf("hi\n");
				pop_list[(uint64_t)num]++;
			}
			// }
		}
	}
	while (!is_empty(beep)) {
		node_data num = pop(beep);
		assert(num < RANDOM_WALK_NUM);

		if (num >= 0) {
			pop_list[(uint64_t)num]++;
		}
	}

	// printf("push_list = [");
	// for (i = 0; i < RANDOM_WALK_NUM; ++i) {
	// 	printf("%d, ", push_list[i]);
	// }
	// printf("]\npop_list = [");
	// for (i = 0; i < RANDOM_WALK_NUM; ++i) {
	// 	printf("%d, ", pop_list[i]);
	// }
	// printf("]\n");

	for (i = 0; i < RANDOM_WALK_NUM; ++i) {
		// printf("%d == %d\n",push_list[i], pop_list[i]);
		assert(push_list[i] == pop_list[i]);
	}

	free(beep);
}

int main(int argc, char const *argv[]) {
	test_beep();
	timer_init();

	register_timer(100, NULL, NULL);

	// remove_timer(0);
	return 0;
}
