#include <copy.h>
#include <process.h>

#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
int syscall_process_create(struct process* process, vaddr_t path) {
	trace(5);
	char filename[N_NAME + 1];
	if (copy_vspace2sos(path, filename, &process->vspace, N_NAME,
						COPY_RETURNWRITTEN) < 0) {
		trace(5);
		return -1;
	}

	struct process* new_process = process_create(filename);
	if (new_process == NULL) {
		return -1;
	}

	return new_process->pid;
}
