
// #include <assert.h>
// #include <fcntl.h>
// #include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <sys/time.h>
// #include <time.h>
// #include <unistd.h>
// #include <utils/time.h>

/* Your OS header file */
#include <sos.h>

#define NUM_CHILDREN 3

int main(int argc, char const *argv[]) {
	printf("Starting up parent process");

	pid_t pids[NUM_CHILDREN];

	for (int i = 0; i < NUM_CHILDREN; i++) {
		pids[i] = sos_process_create("test_child");
		if (pids[i] > 0) {
			printf("Created child %u\n", pids[i]);
		} else {
			printf("Failed to create child\n");
		}
	}

	for (int i = 0; i < NUM_CHILDREN; i++) {
		if (pids[i] > 0) {
			sos_process_wait(pids[i]);
			printf("Child %d finished\n", pids[i]);
		}
	}

	for (int i = 0; i < NUM_CHILDREN; i++) {
		pids[i] = sos_process_create("test_child_inf");
		if (pids[i] >= 0) {
			printf("Created child %d\n", pids[i]);
		} else {
			printf("Failed to create child\n");
		}
	}
	printf("Onto phase 2!\n");

	sos_process_create("test_child");
	sos_process_create("test_child");

	pid_t t = sos_process_wait(-1);

	printf(
		"Parent: Process %d has exited, mass murder in a little bit, but "
		"first, sleep\n",
		t);
	sos_sys_usleep(1000 * 5);

	printf("Okay! Now time for the mass murder!\n");

	for (int i = 0; i < NUM_CHILDREN; i++) {
		if (pids[i] > 0) sos_process_delete(pids[i]);
	}
	printf("All done. ps should be clear\n");
	return 0;
}
