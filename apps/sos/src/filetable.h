#pragma once
#include <sos.h>
#include <typedefs.h>
struct vspace;

struct fd {
	int used;
	int (*dev_write)(void* data, struct vspace* vspace, vaddr_t buffer,
					 size_t length);
	int (*dev_read)(void* data, struct vspace* vspace, vaddr_t buffer,
					size_t length);
	int (*dev_close)(struct fd* fd);
	void* data;
	int flags;
};

struct fd_table {
	struct fd fds[PROCESS_MAX_FILES];
};

int fd_getnew(struct fd_table* fd_table);

inline int fd_inrange(int fd) { return fd < PROCESS_MAX_FILES && fd >= 0; }