/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "mapping.h"

#include <cspace/cspace.h>
#include <process.h>
#include <region_manager.h>
#include <ut_manager/ut.h>
#include <vm.h>
#include "vmem_layout.h"
#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
#include <sys/panic.h>

extern const seL4_BootInfo* _boot_info;
/**
 * Maps a page table into the root servers page directory
 * @param vaddr The virtual address of the mapping
 * @return 0 on success
 */

void* map_device(char* name, void* paddr, int size) {
	trace(5);
	seL4_Word phys = (seL4_Word)paddr;

	region_node* node =
		create_mmap(sos_process.vspace.regions, size,
					PAGE_PINNED | PAGE_NOCACHE | PAGE_READABLE | PAGE_WRITABLE);
	kassert(node != NULL);
	node->name = name;

	vaddr_t vstart = node->vaddr;
	vaddr_t vend = node->vaddr + node->size;
	dprintf(1, "Mapping device memory 0x%x -> 0x%x (0x%x bytes)\n", phys,
			vstart, size);
	trace(5);
	while (vstart < vend) {
		seL4_Error err;
		seL4_ARM_Page frame_cap;
		/*		 Retype the untype to a frame */
		err = cspace_ut_retype_addr(phys, seL4_ARM_SmallPageObject,
									seL4_PageBits, cur_cspace, &frame_cap);
		conditional_panic(err, "Unable to retype device memory");
		/*		 Map in the page */
		struct page_table_entry* pte = sos_map_page(
			sos_process.vspace.pagetable, vstart,
			PAGE_PINNED | PAGE_NOCACHE | PAGE_READABLE | PAGE_WRITABLE,
			frame_cap);
		conditional_panic(pte == NULL, "Unable to map device");
		/*		 Next address */
		phys += (1 << seL4_PageBits);
		vstart += (1 << seL4_PageBits);
	}
	return (void*)node->vaddr;
}
