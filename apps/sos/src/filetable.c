#include <filetable.h>
int fd_getnew(struct fd_table* fd_table) {
	for (int i = 3; i < PROCESS_MAX_FILES; i++) {
		if (fd_table->fds[i].used == 0) {
			return i;
		}
	}
	return -1;
}