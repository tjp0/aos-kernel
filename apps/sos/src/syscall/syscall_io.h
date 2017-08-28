#include <process.h>

int syscall_serialwrite(struct process* process, vaddr_t ptr, size_t length,
						size_t* written);

int syscall_open(struct process* process, vaddr_t buf, int flags);

int syscall_read(struct process* process, int fdnum, vaddr_t ptr, int length);

int syscall_write(struct process* process, int fdnum, vaddr_t ptr, int length);