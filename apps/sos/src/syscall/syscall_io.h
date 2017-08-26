#include <process.h>

int syscall_serialwrite(struct process* process, vaddr_t ptr, size_t length,
						size_t* written);