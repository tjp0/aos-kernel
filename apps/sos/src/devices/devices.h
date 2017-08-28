#pragma once
#include <filetable.h>
int serial_dev_init(void);
int serial_open(struct fd* fd, int flags);
int serial_close(struct fd* fd);
/* This is special, since there's a dedicated syscall for serial_write */
int serial_write(void* data, struct vspace* vspace, vaddr_t procbuf,
				 int length);