#pragma once
#include <sos.h>
#include <typedefs.h>
struct vspace;

struct fd {
	int used;
	int (*dev_write)(struct fd* fd, struct vspace* vspace, vaddr_t buffer,
					 size_t length);
	int (*dev_read)(struct fd* fd, struct vspace* vspace, vaddr_t buffer,
					size_t length);
	int (*dev_close)(struct fd* fd);
	/* Opaque data used by the underlying device */
	void* data;
	int flags;
	/* Current offset in the file */
	uint64_t offset;
};

struct fd_table {
	struct fd fds[PROCESS_MAX_FILES];
};

void fd_table_close(struct fd_table* fd_table);

/* Return a new unused FD */
int fd_getnew(struct fd_table* fd_table);

inline int fd_inrange(int fd) { return fd < PROCESS_MAX_FILES && fd >= 0; }
