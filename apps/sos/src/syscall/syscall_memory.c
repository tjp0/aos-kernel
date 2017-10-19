#include <process.h>
#include <region_manager.h>
#include <sos_coroutine.h>
#include <stdint.h>
#include <syscall/syscall_memory.h>
#include <utils/page.h>
#include <vm.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
#include <sys/panic.h>

uint32_t syscall_sbrk(struct process* process, uint32_t size) {
	dprintf(3, "process addr in sbrk: %08x\n", (unsigned int)process);
	assert(process != NULL);
	dprintf(2, "sbrk called with size %08x\n", size);
	trace(3);
	region_node* heap = process->vspace.regions->heap;
	dprintf(3, "regions: %p\n", process->vspace.regions);
	dprintf(3, "heap: %p\n", heap);
	trace(3);
	vaddr_t old_sbrk = process->vspace.sbrk;
	vaddr_t new_sbrk = old_sbrk + size;
	bool overflow = __builtin_add_overflow(old_sbrk, size, &new_sbrk);
	if (overflow) {
		dprintf(1, "** overflow\n");
		return 0;
	}
	trace(3);
	if (new_sbrk > heap->vaddr + heap->size) {
		trace(3);
		uint32_t sizeup = PAGE_ALIGN_UP_4K(size);
		dprintf(2, "sbrk expanding by %08x\n", sizeup);
		int err = expand_right(heap, sizeup);
		trace(3);
		if (err != REGION_GOOD) {
			dprintf(0, "sbrk fail\n");
			dprintf(0, "Tried to sbrk 0x%08x bytes\n", size);
			regions_print(process->vspace.regions);
			return 0;
		}
	}
	trace(3);
	process->vspace.sbrk = new_sbrk;
	dprintf(2, "sbrk returning 0x%08x\n", old_sbrk);

	return old_sbrk;
}

uint32_t syscall_mmap(struct process* process, uint32_t size,
					  uint32_t permissions) {
	trace(5);
	region_node* mapped_region =
		create_mmap(process->vspace.regions, size, permissions);
	if (!mapped_region) {
		dprintf(1, "Failed to find space to mmap or something\n");
		return 0;
	}
	trace(5);
	if (permissions & PAGE_PINNED) {
		for (vaddr_t vaddr = mapped_region->vaddr;
			 vaddr < mapped_region->vaddr + size; vaddr += PAGE_SIZE_4K) {
			trace(5);
			struct page_table_entry* pte =
				sos_map_page(process->vspace.pagetable, vaddr, permissions, 0);
			if (pte == NULL) {
				dprintf(1, "Pte was null\n");
				region_remove(process->vspace.regions, process, vaddr);
				return 0;
			}
		}
	}
	trace(5);
	dprintf(4, "Allocated new mmap region: 0x%08x\n", mapped_region->vaddr);
	return mapped_region->vaddr;
}

uint32_t syscall_munmap(struct process* process, vaddr_t vaddr) {
	kassert(vaddr != 0);
	dprintf(0, "Unmapping region at %08x\n", vaddr);
	return region_remove(process->vspace.regions, process, vaddr);
}
