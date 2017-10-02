
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

int main(int argc, char const *argv[]) {
	printf("yay\n");

	sos_process_create("test_child");
	sos_process_create("test_child");
	return 0;
}
