#include <copy.h>
#include <filetable.h>
#include <globals.h>
#include <libcoro.h>
#include <process.h>
#include <sos.h>
#include <utils/ringbuffer.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
#define SERIAL_BUFFER_SIZE 512

#define AOS06_PORT (26706)

ringBuffer_typedef(char, charBuffer);

static charBuffer serial_buffer;
static charBuffer* serial_buffer_ptr = &serial_buffer;

static coro listening_thread = NULL;

static int reader = 0;
static void serial_handler(struct serial* serial, char c) {
	trace(4);
	if (isBufferFull(serial_buffer_ptr)) {
		trace(4);
		dprintf(2, "Dropping character\n");
		return;
	}
	trace(4);
	bufferWrite(serial_buffer_ptr, c);

	if (listening_thread != NULL) {
		trace(4);
		resume(listening_thread, &c);
	}
}

int serial_dev_init(void) {
	global_serial = serial_init(AOS06_PORT);
	if (global_serial == NULL) {
		return -1;
	}
	bufferInit(serial_buffer, SERIAL_BUFFER_SIZE, char);
	serial_register_handler(global_serial, serial_handler);
	return 0;
}

static int serial_read(struct fd* fd, struct vspace* vspace, vaddr_t procbuf,
					   size_t length) {
	if (listening_thread != NULL) {
		return -1;
	}
	char buf[SERIAL_BUFFER_SIZE];
	size_t index = 0;

	size_t written = 0;
	trace(4);
	while (written < length) {
		if (isBufferEmpty(serial_buffer_ptr)) {
			listening_thread = current_coro();
			trace(4);
			yield(NULL);
			trace(4);
			listening_thread = NULL;
		}
		/* bufferRead is a macro, we "pass" c directly */
		char c;
		bufferRead(serial_buffer_ptr, c);
		buf[index] = c;
		index++;
		written++;
		trace(4);
		if (c == '\n') {
			trace(4);
			break;
		}
		/*if(written % 10 == 0) {
			dprintf(2, "Serial read %u\n", written);
		}*/

		// dprintf(2, "%c", c);

		if (index == SERIAL_BUFFER_SIZE) {
			trace(4);
			if (copy_sos2vspace(buf, procbuf, vspace, index, 0) < 0) {
				goto serial_err;
			}
			index = 0;
			procbuf += index;
			trace(4);
		}
	}

	if (copy_sos2vspace(buf, procbuf, vspace, index, 0) < 0) {
		trace(4);
		goto serial_err;
	}
	trace(4);

	dprintf(2, "Serial read %u total\n", written);
	return written;

serial_err:
	trace(4);
	listening_thread = NULL;
	return -1;
}

int serial_write(struct fd* fd, struct vspace* vspace, vaddr_t procbuf,
				 size_t length) {
	char array[512];

	size_t written = 0;

	while (written < length) {
		size_t to_copy = MIN(sizeof(array), length - written);
		if (copy_vspace2sos(procbuf + written, array, vspace, to_copy, 0) < 0) {
			return -1;
		}
		size_t just = serial_send(global_serial, array, to_copy);
		if (just == 0) {
			return written;
		}
		written += just;
	}

	return written;
}

static int serial_close(struct fd* fd) {
	if (fd->flags & FM_READ) {
		reader = 0;
	}
	fd->used = 0;
	return 0;
}

int serial_open(struct fd* fd, int flags) {
	(void)flags;
	if (reader == 1 && (flags & FM_READ)) {
		return -1;
	}
	fd->used = 1;
	fd->dev_write = &serial_write;
	fd->dev_read = &serial_read;
	fd->dev_close = &serial_close;
	fd->flags = flags;

	if (flags & FM_READ) {
		reader = 1;
	}
	return 0;
}
