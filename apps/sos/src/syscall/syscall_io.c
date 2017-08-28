#include <copy.h>
#include <devices/devices.h>
#include <globals.h>
#include <process.h>
#include <sos.h>
#include <utils/math.h>

#define verbose 10
#include <sys/debug.h>
#include <sys/kassert.h>

int syscall_serialwrite(struct process* process, vaddr_t ptr, int length,
						size_t* written) {
	return serial_write(NULL, &process->vspace, ptr, length);
}

int syscall_write(struct process* process, int fdnum, vaddr_t ptr, int length) {
	kassert(process != NULL);
	if (!fd_inrange(fdnum)) {
		return -1;
	}
	if (process->fds.fds[fdnum].used != 1) {
		return -1;
	}
	struct fd* fd = &process->fds.fds[fdnum];
	return fd->dev_write(fd->data, &process->vspace, ptr, length);
}

int syscall_read(struct process* process, int fdnum, vaddr_t ptr, int length) {
	trace(5);
	kassert(process != NULL);
	if (!fd_inrange(fdnum)) {
		return -1;
	}
	if (process->fds.fds[fdnum].used != 1) {
		return -1;
	}
	struct fd* fd = &process->fds.fds[fdnum];
	return fd->dev_read(fd->data, &process->vspace, ptr, length);
}

int syscall_open(struct process* process, vaddr_t buf, int flags) {
	trace(5);
	char filename[N_NAME + 1];
	if (copy_vspace2sos(buf, filename, &process->vspace, N_NAME,
						COPY_RETURNWRITTEN) < 0) {
		trace(5);
		return -1;
	}
	filename[N_NAME] = '\0';
	int fdnum = fd_getnew(&process->fds);
	if (fdnum < 0) {
		trace(5);
		return -1;
	}
	struct fd* fd = &process->fds.fds[fdnum];

	/* This is just the "open" codepath, so this is allowed */
	if (!strcmp(filename, "console")) {
		trace(5);
		if (serial_open(fd, flags) < 0) {
			return -1;
		}
		return fdnum;
	}
	trace(5);
	return -1;
}