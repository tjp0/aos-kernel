#include <sys/kassert.h>
// #include <cspace/cspace.h>
#include <frametable.h>
#include <mapping.h>
#include <sel4/sel4.h>
#include <stdlib.h>
// #include <string.h>
#include <stdio.h>
#include <ut_manager/ut.h>
#include <utils/page.h>
#include <vmem_layout.h>
// #define verbose 0
// #include <sys/debug.h>
// #include <sys/panic.h>

// should probably not do this but whatevs
seL4_Word ft_size;
seL4_Word ft_numframes;
seL4_Word lowest_address;
seL4_Word highest_address;

seL4_Word FRAME_VSTART;
seL4_Word FRAME_SIZE_BITS;
seL4_Word FRAME_VEND;
seL4_Word FRAME_TABLE_VSTART;
seL4_Word FRAME_TABLE_SIZE_BITS;
seL4_Word FRAME_TABLE_VEND;
seL4_Word DMA_VSTART;
seL4_Word DMA_SIZE_BITS;
seL4_Word DMA_VEND;
seL4_Word KERNEL_STACK_VSTART;
seL4_Word KERNEL_STACK_VEND;

/* The linker provides this handy symbol to find the end of the elf file */
extern char __end__;

static seL4_Word log_2(seL4_Word num);
static seL4_Word log_2(seL4_Word num) {
	seL4_Word bits = 0;
	while ((1 << bits) < num) {
		bits++;
	}
	return bits;
}

void initialise_vmem_layout(void) {
	ut_find_memory(&lowest_address, &highest_address);
	seL4_Word memory_range = (highest_address - lowest_address) + PAGE_SIZE_4K;
	FRAME_VSTART = PAGE_ALIGN_UP_4K((unsigned int)&__end__ + ELF_PADDING);
	FRAME_SIZE_BITS = log_2(memory_range);
	FRAME_VEND = ((unsigned int)(FRAME_VSTART + (1ull << FRAME_SIZE_BITS)));
	FRAME_TABLE_VSTART = (FRAME_VEND + VADDR_PADDING);
	FRAME_TABLE_SIZE_BITS = FRAME_SIZE_BITS;
	// calculate the number of pages of size 2 ^ seL4_PageBits
	FRAME_TABLE_SIZE_BITS -= seL4_PageBits;
	// account for how many bits are needed for the frame struct
	// multiply by 8 to get the number of bits since
	// size of returns the number of bytes in the struct
	FRAME_TABLE_SIZE_BITS += log_2(8 * sizeof(struct frame));
	FRAME_TABLE_VEND =
		((unsigned int)(FRAME_TABLE_VSTART + (1ull << FRAME_TABLE_SIZE_BITS)));
	DMA_VSTART = (FRAME_TABLE_VEND + VADDR_PADDING);
	DMA_SIZE_BITS = (24);
	DMA_VEND = ((unsigned int)(DMA_VSTART + (1ull << DMA_SIZE_BITS)));

	ft_numframes = (memory_range / PAGE_SIZE_4K) + 1;
	ft_size = ft_numframes * sizeof(struct frame);

	KERNEL_STACK_VSTART = DMA_VEND + VADDR_PADDING;
	KERNEL_STACK_VEND = KERNEL_STACK_VSTART + (1ull << KERNEL_STACK_BITS);

	conditional_panic(KERNEL_STACK_VEND > DEVICE_START,
					  "Failed to allocate virtual memory layout");
	// seL4_Word frame_bits = FRAME_SIZE_BITS;
	// subtract the number of bits used for each page
	// add on the number of bits it takes to
	// account for the frame struct
	// return frame_bits - seL4_PageBits + log_2(8 * sizeof(struct frame));
}

void print_vmem_layout(void) {
	printf("\n---------- -- ---------- | --------------\n");
	printf("0x00000000 -> 0x%08x | ELF Reserved\n", (unsigned int)&__end__);
	printf("0x%08x -> 0x%08x | Heap\n", (unsigned int)heap_vstart(),
		   (unsigned int)heap_vend());
	printf("0x%08x -> 0x%08x | Frame mapping area\n", FRAME_VSTART, FRAME_VEND);
	printf("0x%08x -> 0x%08x | Frame table\n", FRAME_TABLE_VSTART,
		   FRAME_TABLE_VEND);
	printf("0x%08x -> 0x%08x | DMA Area\n", DMA_VSTART, DMA_VEND);
	printf("0x%08x -> 0x%08x | Kernel stack\n", KERNEL_STACK_VSTART,
		   KERNEL_STACK_VEND);
	printf("---------- -- ---------- | --------------\n");
}