
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

#define MAX_ALIVE_TIME_SECONDS 10
#define MALLOC_PAGES 10

#include <sos.h>

int main(int argc, char const *argv[]) {

	pid_t myid = sos_my_id();
	srand(myid);

	uint64_t sleep_time = (rand()%(MAX_ALIVE_TIME_SECONDS))*1000;

	printf("Child process: %u. Sleeping for %llu milliseconds\n",myid,sleep_time);
	for(int i=0;i<MALLOC_PAGES;i++) {
		int* inta = malloc(4096);
		inta[0] = 1000;
	}
	sos_sys_usleep(sleep_time);
	printf("Child process %u exiting\n",myid);
	return 0;
}
