#include <assert.h>
// #include <cspace/cspace.h>
#include <frametable.h>
#include <mapping.h>
#include <sel4/sel4.h>
#include <stdlib.h>
// #include <string.h>
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
seL4_Word PROCESS_TOP;

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
	seL4_Word memory_range = (highest_address - lowest_address);

	FRAME_VSTART = (0x08000000);

	// FRAME_SIZE_BITS = (31);
	FRAME_SIZE_BITS = log_2(memory_range);

	FRAME_VEND = ((unsigned int)(FRAME_VSTART + (1ull << FRAME_SIZE_BITS)));

	FRAME_TABLE_VSTART = (FRAME_VEND + 0x1000);

	FRAME_TABLE_SIZE_BITS = FRAME_SIZE_BITS;
	// calculate the number of pages of size 2 ^ seL4_PageBits
	FRAME_TABLE_SIZE_BITS -= seL4_PageBits;
	// account for how many bits are needed for the frame struct
	// multiply by 8 to get the number of bits since
	// size of returns the number of bytes in the struct
	FRAME_TABLE_SIZE_BITS += log_2(8 * sizeof(struct frame));

	FRAME_TABLE_VEND =
		((unsigned int)(FRAME_TABLE_VSTART + (1ull << FRAME_TABLE_SIZE_BITS)));

	DMA_VSTART = (FRAME_TABLE_VEND + 0x1000);

	DMA_SIZE_BITS = (22);
	DMA_VEND = ((unsigned int)(DMA_VSTART + (1ull << DMA_SIZE_BITS)));

	PROCESS_TOP = 0xD0000000;

	ft_numframes = (memory_range / PAGE_SIZE_4K);

	ft_size = ft_numframes * sizeof(struct frame);

	// seL4_Word frame_bits = FRAME_SIZE_BITS;
	// subtract the number of bits used for each page
	// add on the number of bits it takes to
	// account for the frame struct
	// return frame_bits - seL4_PageBits + log_2(8 * sizeof(struct frame));
}
