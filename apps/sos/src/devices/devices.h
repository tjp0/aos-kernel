#pragma once
#include <filetable.h>
int serial_dev_init(void);
int nfs_dev_init(void);
int serial_open(struct fd* fd, int flags);
int serial_close(struct fd* fd);
/* This is special, since there's a dedicated syscall for serial_write */
int serial_write(void* data, struct vspace* vspace, vaddr_t procbuf,
				 int length);

int nfs_dev_open(struct fd* fd, char* name, int flags);
int nfs_dev_stat(struct vspace* vspace, char* name, vaddr_t sos_stat);
int nfs_dev_getdirent(struct vspace* vspace, int pos, vaddr_t name,
					  size_t nbytes);
