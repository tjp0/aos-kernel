#include <copy.h>
#include <globals.h>
#include <process.h>
#include <serial/serial.h>
#include <utils/math.h>

#define verbose 0
#include <sys/debug.h>

int syscall_serialwrite(struct process* process, vaddr_t ptr, size_t length,
						size_t* written) {
	dprintf(2, "syscall: thread made syscall serialwrite!\n");
	char array[512];

	length = MIN(sizeof(array) - 1, length);
	copy_vspace2sos(ptr, array, &process->vspace, length, 0);
	array[length] = '\0';
	dprintf(2, "Vaddr is %08x\n", ptr);
	dprintf(2, "Length is %u\n", length);
	dprintf(2, "Printing string %s\n", array);
	serial_send(global_serial, array, length);

	*written = length;
	return 0;
}