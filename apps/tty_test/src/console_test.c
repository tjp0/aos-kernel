#include <assert.h>
#include <sos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define SMALL_BUF_SZ 2
#define BUF_SZ 5000
char test_str[] = "Basic test string for read/write";
char small_buf[SMALL_BUF_SZ];

int test_buffers() {
	int console_fd = sos_sys_open("console", 0);
	/* test a small string from the code segment */
	int result = sos_sys_write(console_fd, test_str, strlen(test_str));
	assert(result == strlen(test_str));

	/* test reading to a small buffer */
	result = sos_sys_read(console_fd, small_buf, SMALL_BUF_SZ);
	/* make sure you type in at least SMALL_BUF_SZ */
	assert(result == SMALL_BUF_SZ);

	/* test reading into a large on-stack buffer */
	char stack_buf[BUF_SZ];
	/* for this test you'll need to paste a lot of data into
	   the console, without newlines */

	result = sos_sys_read(console_fd, stack_buf, BUF_SZ);
	printf("Got %u characters\n", result);
	assert(result == BUF_SZ);

	result = sos_sys_write(console_fd, stack_buf, BUF_SZ);
	assert(result == BUF_SZ);

	/* this call to malloc should trigger an brk */
	char *heap_buf = malloc(BUF_SZ);
	assert(heap_buf != NULL);

	/* for this test you'll need to paste a lot of data into
	   the console, without newlines */
	result = sos_sys_read(console_fd, heap_buf, BUF_SZ);
	assert(result == BUF_SZ);

	result = sos_sys_write(console_fd, heap_buf, BUF_SZ);
	assert(result == BUF_SZ);

	/* try sleeping */
	for (int i = 0; i < 5; i++) {
		time_t prev_seconds = time(NULL);
		sos_sys_usleep(1000);
		time_t next_seconds = time(NULL);
		assert(next_seconds > prev_seconds);
		printf("Tick\n");
	}

	return 0;
}
