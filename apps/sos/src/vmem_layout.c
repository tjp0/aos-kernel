#include <frametable.h>
#include <mapping.h>
#include <process.h>
#include <sel4/sel4.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/kassert.h>
#include <ut_manager/ut.h>
#include <utils/page.h>
#include <vm.h>
#include <vmem_layout.h>

// should probably not do this but whatevs
seL4_Word ft_size;
seL4_Word ft_numframes;
seL4_Word lowest_address;
seL4_Word highest_address;

seL4_Word FRAME_VSTART;
seL4_Word FRAME_VEND;
seL4_Word FRAME_TABLE_VSTART;
seL4_Word FRAME_TABLE_VEND;
seL4_Word DMA_VSTART;
seL4_Word DMA_VEND;

/* The linker provides this handy symbol to find the end of the elf file */
extern char __end__;

void initialize_sos_memory(seL4_ARM_PageDirectory pd,
						   const seL4_BootInfo* boot) {
	conditional_panic(init_region_list(&sos_process.vspace.regions) < 0,
					  "Failed to init SOS regions");

	sos_process.vspace.regions->stack->name = "UNUSED";
	sos_process.vspace.regions->ipc_buffer->name = "UNUSED";

	ut_find_memory(&lowest_address, &highest_address);
	seL4_Word memory_range = (highest_address - lowest_address) + PAGE_SIZE_4K;

	region_node* node = add_region(
		sos_process.vspace.regions, 0x0, (uint32_t)&__end__,
		PAGE_READABLE | PAGE_WRITABLE | PAGE_EXECUTABLE | PAGE_PINNED, NULL,
		NULL, NULL);

	kassert(node);
	node->name = "ELF Reserved";

	node = create_heap(sos_process.vspace.regions);
	node->name = "UNUSED";
	kassert(node);

	node =
		create_mmap(sos_process.vspace.regions, memory_range,
					PAGE_READABLE | PAGE_WRITABLE | PAGE_PINNED | PAGE_SPECIAL);
	kassert(node);
	node->name = "Mapped frames";

	FRAME_VSTART = node->vaddr;
	FRAME_VEND = node->vaddr + node->size;

	uint32_t frame_table_size =
		(memory_range + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K * sizeof(struct frame);
	ft_numframes = ((memory_range + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K);
	ft_size = ft_numframes * sizeof(struct frame);

	node =
		create_mmap(sos_process.vspace.regions, frame_table_size,
					PAGE_READABLE | PAGE_WRITABLE | PAGE_PINNED | PAGE_SPECIAL);
	kassert(node);
	node->name = "Frame table";

	FRAME_TABLE_VSTART = node->vaddr;
	FRAME_TABLE_VEND = node->vaddr + node->size;

	node =
		create_mmap(sos_process.vspace.regions, (1uLL << DMA_SIZE_BITS),
					PAGE_READABLE | PAGE_WRITABLE | PAGE_PINNED | PAGE_NOCACHE);
	kassert(node);
	node->name = "DMA Mapping Area";

	DMA_VSTART = node->vaddr;
	DMA_VEND = node->vaddr + node->size;

	/* Create a pagedirectory based upon SOS's initially provided CAPS */
	sos_process.vspace.pagetable = pd_createSOS(pd, boot);
	kassert(sos_process.vspace.pagetable != NULL);
}

void* sos_allocate_stack_area(uint32_t size) {
	region_node* node = create_mmap(sos_process.vspace.regions, size,
									PAGE_READABLE | PAGE_WRITABLE);
	kassert(node);
	return NULL;
}

void print_vmem_layout(void) { regions_print(sos_process.vspace.regions); }
