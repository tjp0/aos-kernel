#include <copy.h>
#include <devices/devices.h>
#include <fcntl.h>
#include <globals.h>
#include <process.h>
#include <sos.h>
#include <utils/math.h>

#define verbose 0
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
	if (fd->dev_write == NULL) {
		return -1;
	}
	return fd->dev_write(fd, &process->vspace, ptr, length);
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
	if (fd->dev_read == NULL) {
		return -1;
	}
	return fd->dev_read(fd, &process->vspace, ptr, length);
}

int syscall_stat(struct process* process, vaddr_t filebuf, vaddr_t statbuf) {
	trace(5);
	char filename[N_NAME + 1];
	filename[N_NAME] = '\0';
	if (copy_vspace2sos(filebuf, filename, &process->vspace, N_NAME,
						COPY_RETURNWRITTEN) < 0) {
		trace(5);
		return -1;
	}
	return nfs_dev_stat(&process->vspace, filename, statbuf);
}

int syscall_open(struct process* process, vaddr_t buf, int flags) {
	trace(5);
	char filename[N_NAME + 1];

	if (flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR) {
		return -1;
	}

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

	if (nfs_dev_open(fd, filename, flags) < 0) {
		return -1;
	}

	return fdnum;
}

int syscall_close(struct process* process, int fdnum) {
	if (!fd_inrange(fdnum)) {
		return -1;
	}
	struct fd* fd = &process->fds.fds[fdnum];
	if (fd->used != 1) {
		return -1;
	}
	if (fd->dev_close == NULL) {
		return -1;
	}
	return fd->dev_close(fd);
}

int syscall_getdirent(struct process* process, int pos, vaddr_t name,
					  size_t nbytes) {
	return nfs_dev_getdirent(&process->vspace, pos, name, nbytes);
}
