#include <filetable.h>
int fd_getnew(struct fd_table* fd_table) {
	for (int i = 0; i < PROCESS_MAX_FILES; i++) {
		if (fd_table->fds[i].used == 0) {
			return i;
		}
	}
	return -1;
}
void fd_table_close(struct fd_table* fd_table) {
	for (int i = 0; i < PROCESS_MAX_FILES; i++) {
		if (fd_table->fds[i].used != 0) {
			fd_table->fds[i].dev_close(&fd_table->fds[i]);
		}
	}
}
