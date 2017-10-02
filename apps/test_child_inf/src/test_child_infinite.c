
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
#include <string.h>
#define MALLOC_PAGES 1000
int main(int argc, char const *argv[]) {

	pid_t myid = sos_my_id();
	srand(myid);
	void* pointers[MALLOC_PAGES];
	for(int i=0;i<MALLOC_PAGES;i++) {
		pointers[i] = malloc(4096);
		if(pointers[i] == NULL) {
			printf("test_child_infinite:%u, malloc failed\n",myid);
			return 0;
		}
	}
	while(1) {
		for(int i=0;i<MALLOC_PAGES;i++) {
			memset(pointers[i],rand()%255,4096);
		}
	}
	return 0;
}
