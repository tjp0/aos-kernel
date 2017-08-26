#include <process.h>
#include <region_manager.h>
#include <stdint.h>
#include <syscall/syscall_memory.h>
#include <utils/page.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

uint32_t syscall_sbrk(struct process* process, uint32_t size) {
	dprintf(2, "sbrk called with size %08x\n", size);

	region_node* heap = process->vspace.regions->heap;

	if (heap == NULL) {
		heap = create_heap(process->vspace.regions);
		if (heap == NULL) {
			dprintf(2, "sbrk fail1\n");
			return 0;
		}
		process->vspace.sbrk = process->vspace.regions->heap->vaddr;
	}

	vaddr_t old_sbrk = process->vspace.sbrk;
	vaddr_t new_sbrk = old_sbrk + size;

	if (new_sbrk > heap->vaddr + heap->size) {
		uint32_t sizeup = PAGE_ALIGN_UP_4K(size);
		dprintf(2, "sbrk expanding by %08x\n", sizeup);
		int err = expand_right(heap, sizeup);

		if (err != REGION_GOOD) {
			dprintf(2, "sbrk fail2\n");
			return 0;
		}
	}
	process->vspace.sbrk = new_sbrk;
	dprintf(2, "sbrk returning 0x%08x\n", old_sbrk);

	return old_sbrk;
}