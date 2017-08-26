#include <assert.h>
#include <cspace/cspace.h>
#include <frametable.h>
#include <mapping.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <string.h>
#include <ut_manager/ut.h>
#include <utils/page.h>
#include <vmem_layout.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/panic.h>

// #define vaddr_to_frame_cell(x) (PADDR_TO_FRAME_INDEX(x - FRAME_VSTART))
#define FRAME_INDEX_TO_VADDR(x) ((x << seL4_PageBits) + FRAME_VSTART);

#define FRAME_CACHE_SIZE 30
#define FRAME_CACHE_HIGH_WATER 20
/* #define FRAME_RESERVED \
// 	((1 << 17) + (1 << 16) + (1 << 15) + (0 << 14) + (0 << 13) + (0 << 12) + \
// 	 (0 << 11) + (0 << 10) + (0 << 9) + (0 << 8) + (0 << 7) + (0 << 6) +     \
// 	 (0 << 5) + (0 << 4) + (0 << 3) + (0 << 2) + (0 << 1))
*/

#define FRAME_RESERVED 0b10000000000000000
// staunched from ut_allocator.c
// #define FLOOR14(x) ((x) & ~((1 << 14) - 1))
// #define CEILING14(x) FLOOR14((x) + (1 << 14) - 1)

// TODO: Fix memory leak
// Fix memory reserved
// fix to use findmem
// Fix memory free
// Fix vmem layout

struct frame* frame_table = NULL;

uint32_t frame_count = 0;
seL4_Word ft_size;
seL4_Word ft_numframes = 0;
seL4_Word lowest_address;
seL4_Word highest_address;

inline int frame_to_index(struct frame* frame) { return frame - frame_table; }

uint32_t frame_cache_tail = 0;
void* frame_cache[FRAME_CACHE_SIZE];

static inline struct frame* paddr_to_frame_cell(seL4_Word paddr) {
	// remove the sub page index of the address
	seL4_Word phy_frame_start_num = (paddr >> seL4_PageBits);
	// subtract the lowest address's high bytes to get the index
	seL4_Word index = phy_frame_start_num - (lowest_address >> seL4_PageBits);
	// get the byte index into the struct
	seL4_Word offset_into_table = index * sizeof(struct frame);
	// get an actual pointer in memory
	seL4_Word frame_table_pointer = offset_into_table + (seL4_Word)frame_table;
	// these should both be valid ways of finding this pointer
	assert((struct frame*)(frame_table_pointer) == &frame_table[index]);
	return (struct frame*)frame_table_pointer;
}

static inline seL4_Word frame_cell_to_paddr(struct frame* frame_table_pointer) {
	// get the byte index into the frame_table
	seL4_Word offset_into_table =
		((seL4_Word)frame_table_pointer) - (seL4_Word)frame_table;
	// get the struct index
	seL4_Word index = offset_into_table / sizeof(struct frame);
	// index into the usable part of memory by adding the lowest address
	seL4_Word phy_frame_start_num = index + (lowest_address >> seL4_PageBits);
	// get an actual pointer in memory
	seL4_Word paddr = (phy_frame_start_num << seL4_PageBits);
	return paddr;
}

static inline seL4_Word vaddr_to_paddr(void* vaddr) {
	return ((seL4_Word)vaddr) - FRAME_VSTART;
}
static inline void* paddr_to_vaddr(seL4_Word paddr) {
	return (void*)(paddr + FRAME_VSTART);
}
static inline struct frame* vaddr_to_frame_cell(void* vaddr) {
	return paddr_to_frame_cell(vaddr_to_paddr(vaddr));
}
void* frame_cell_to_vaddr(struct frame* frame_cell) {
	return paddr_to_vaddr(frame_cell_to_paddr(frame_cell));
}
int frame_physical_alloc(void** vaddr) {
	if (ft_numframes - frame_count < FRAME_RESERVED) {
		return 1;
	}

	seL4_Word new_frame_addr = ut_alloc(seL4_PageBits);

	assert(paddr_to_frame_cell(new_frame_addr) < &frame_table[ft_numframes]);
	assert(paddr_to_frame_cell(new_frame_addr) > &frame_table[0]);
	assert(((new_frame_addr >> seL4_PageBits) << seL4_PageBits) ==
		   frame_cell_to_paddr(paddr_to_frame_cell(new_frame_addr)));
	assert(new_frame_addr == vaddr_to_paddr(paddr_to_vaddr(new_frame_addr)));
	assert(new_frame_addr ==
		   frame_cell_to_paddr(paddr_to_frame_cell(new_frame_addr)));
	assert(paddr_to_vaddr(new_frame_addr) ==
		   frame_cell_to_vaddr(paddr_to_frame_cell(new_frame_addr)));

	assert(new_frame_addr == vaddr_to_paddr(frame_cell_to_vaddr(
								 paddr_to_frame_cell(new_frame_addr))));
	seL4_ARM_Page new_frame;
	if (new_frame_addr == 0) {
		return 1;
	}
	/* Create the frame cap */
	int err = cspace_ut_retype_addr(new_frame_addr, seL4_ARM_SmallPageObject,
									seL4_PageBits, cur_cspace, &new_frame);

	conditional_panic(err, "Failed to retype UT memory into frame ?");

	err = map_page(new_frame, seL4_CapInitThreadPD,
				   FRAME_VSTART + new_frame_addr, seL4_AllRights, 0);

	conditional_panic(err, "Failed to map frame into SOS");
	assert(new_frame != 0);

	assert(((new_frame_addr >> seL4_PageBits) << seL4_PageBits) ==
		   frame_cell_to_paddr(paddr_to_frame_cell(new_frame_addr)));
	paddr_to_frame_cell(new_frame_addr)->status = FRAME_FREE;
	paddr_to_frame_cell(new_frame_addr)->cap = new_frame;

	*vaddr = (void*)FRAME_VSTART + new_frame_addr;

	frame_count++;
	return 0;
}

int frame_physical_free(struct frame* frame) {
	assert(frame_cell_to_paddr(frame) ==
		   vaddr_to_paddr(paddr_to_vaddr(frame_cell_to_paddr(frame))));
	assert((frame) == (paddr_to_frame_cell(frame_cell_to_paddr(frame))));
	assert(paddr_to_vaddr(frame_cell_to_paddr(frame)) ==
		   frame_cell_to_vaddr(frame));

	assert(frame_cell_to_paddr(frame) ==
		   vaddr_to_paddr(frame_cell_to_vaddr(
			   paddr_to_frame_cell(frame_cell_to_paddr(frame)))));

	assert(frame == vaddr_to_frame_cell(frame_cell_to_vaddr((frame))));
	assert(frame == paddr_to_frame_cell(frame_cell_to_paddr(frame)));

	assert(frame->cap != 0);
	cspace_delete_cap(cur_cspace, frame->cap);
	frame->cap = 0;
	frame->status = FRAME_UNTYPED;
	ut_free(frame_cell_to_paddr(frame), seL4_PageBits);
	frame_count--;
	return 1;
}

static void cache_frames() {
	dprintf(2, "Caching frames\n");
	while (frame_cache_tail < FRAME_CACHE_HIGH_WATER) {
		void* new_frame;
		int err = frame_physical_alloc(&new_frame);
		if (err) {
			dprintf(0, "Unable to cache frame\n");
			return;
		}
		assert(new_frame != NULL);
		frame_cache_tail++;
		// printf("tail %d\n", frame_cache_tail);
		frame_cache[frame_cache_tail] = new_frame;
		dprintf(2, "Frame cached\n");
	}
}

void* frame_alloc(void) {
	if (frame_cache_tail == 0) {
		cache_frames();
	}

	void* new_frame = frame_cache[frame_cache_tail];

	frame_cache[frame_cache_tail] = NULL;

	if (frame_cache_tail > 0) {
		frame_cache_tail--;
	}
	if (new_frame == NULL) {
		dprintf(0, "Unable to allocate frame\n");
		return NULL;
	}
	assert(vaddr_to_frame_cell(new_frame)->status == FRAME_FREE);
	assert(vaddr_to_frame_cell(new_frame)->cap != 0);

	dprintf(2, "Allocated frame %p, frame cache is now %u\n", new_frame,
			frame_cache_tail);

	vaddr_to_frame_cell(new_frame)->status = FRAME_INUSE;

	memset(new_frame, 0, PAGE_SIZE_4K);
	return new_frame;
}

struct frame* get_frame(void* addr) {
	struct frame* frame = ((vaddr_to_frame_cell(addr)));
	assert(addr == frame_cell_to_vaddr(frame));
	assert(frame == vaddr_to_frame_cell(addr));
	assert(frame_cell_to_paddr(frame) ==
		   vaddr_to_paddr(paddr_to_vaddr(frame_cell_to_paddr(frame))));
	assert((frame) == (paddr_to_frame_cell(frame_cell_to_paddr(frame))));
	assert(paddr_to_vaddr(frame_cell_to_paddr(frame)) ==
		   frame_cell_to_vaddr(frame));

	assert(frame_cell_to_paddr(frame) ==
		   vaddr_to_paddr(frame_cell_to_vaddr(
			   paddr_to_frame_cell(frame_cell_to_paddr(frame)))));

	assert(frame == vaddr_to_frame_cell(frame_cell_to_vaddr((frame))));
	assert(frame == paddr_to_frame_cell(frame_cell_to_paddr(frame)));
	return frame;
}

void frame_free(void* vaddr) {
	assert(vaddr_to_frame_cell(vaddr)->status == FRAME_INUSE);
	if (frame_cache_tail < FRAME_CACHE_SIZE) {
		frame_cache_tail++;
		frame_cache[frame_cache_tail] = vaddr;
		vaddr_to_frame_cell(vaddr)->status = FRAME_FREE;
	} else {
		frame_physical_free(vaddr_to_frame_cell(vaddr));
	}
	dprintf(2, "Freed frame %p, frame cache is now %u\n", vaddr,
			frame_cache_tail);
}

seL4_Word ft_early_initialize(const seL4_BootInfo* info) {
	ut_find_memory(&lowest_address, &highest_address);

	ft_numframes =
		((highest_address - lowest_address) / (1 << seL4_PageBits)) + 1;
	ft_size = ft_numframes * sizeof(struct frame);

	int bits = 1;

	while ((1 << bits) < ft_size) {
		bits++;
	}

	return ut_steal_mem(bits);
}
void ft_late_initialize(seL4_Word base) {
	dprintf(0, "Initializing frame table of %u size\n", ft_size);

	seL4_Word offset = 0;

	while (offset < ft_size) {
		seL4_Word new_frame;
		conditional_panic(FRAME_TABLE_VSTART + offset >= FRAME_TABLE_VEND,
						  "Frame table too big for allocated memory map");

		int err = cspace_ut_retype_addr(base + offset, seL4_ARM_SmallPageObject,
										seL4_PageBits, cur_cspace, &new_frame);
		conditional_panic(err, "Frametable failed to retype memory at init");

		err = map_page(new_frame, seL4_CapInitThreadPD,
					   FRAME_TABLE_VSTART + offset, seL4_AllRights, 0);

		conditional_panic(err, "Frametable failed to map memory at init");

		offset += (1 << seL4_PageBits);
	}
	frame_table = (struct frame*)FRAME_TABLE_VSTART;

	for (int i = 0; i < ft_numframes; ++i) {
		frame_table[i].status = FRAME_UNTYPED;
	}
}
void recursive_allocate(int counter);

void frame_test() {
	/* Allocate 10 pages and make sure you can touch them all */
	for (int i = 0; i < 10; i++) {
		/* Allocate a page */
		int* vaddr;
		vaddr = frame_alloc();
		assert(vaddr != NULL);

		/* Test you can touch the page */
		*vaddr = 0x37 + i;
		assert(*vaddr == 0x37 + i);

		printf("Page #%d allocated at %p\n", i, (void*)vaddr);
	}
	printf("Test 1 complete\n");
	/* Test that you never run out of memory if you always free frames. */
	for (int i = 0; i < 10000; i++) {
		/* Allocate a page */
		int* vaddr;
		vaddr = frame_alloc();
		assert(vaddr != NULL);

		/* Test you can touch the page */
		*vaddr = 0x37;
		assert(*vaddr == 0x37);

		/* print every 1000 iterations */
		if (i % 1000 == 0) {
			printf("Page #%d allocated at %p\n", i, vaddr);
		}

		frame_free(vaddr);
	}
	printf("Test 2 complete\n");

	/* Test that you eventually run out of memory gracefully,
	   and doesn't crash */
	// int counter = 0;
	recursive_allocate(0);
	// while (1) {
	// 	/* Allocate a page */
	// 	int* vaddr;
	// 	vaddr = frame_alloc();
	// 	if (vaddr == NULL) {
	// 		printf("Out of memory!\n");
	// 		break;
	// 	}

	// 	 Test you can touch the page
	// 	*vaddr = 0x37;
	// 	assert(*vaddr == 0x37);
	// 	counter++;

	// 	if (counter % 1000 == 0) {
	// 		printf("Page #%d allocated at %p\n", counter, vaddr);
	// 	}
	// }

	// free all the memory
}
#define NUM_PAGE_PER_HIT 1000
void recursive_allocate(int counter) {
	int** addrs = malloc(NUM_PAGE_PER_HIT * sizeof(int*));
	if (!addrs) {
		printf("MALLOC Error!\n");
		return;
	}
	int i = 0;
	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		int* vaddr;
		vaddr = frame_alloc();
		if (vaddr == NULL) {
			printf("Out of memory!\n");
			break;
		}

		/* Test you can touch the page */
		*vaddr = 0x37;
		assert(*vaddr == 0x37);
		addrs[i] = vaddr;

		if (counter % 1000 == 0) {
			printf("Page #%d allocated at %p\n", counter, vaddr);
			printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		}
		counter++;
	}

	if (i == NUM_PAGE_PER_HIT) {
		recursive_allocate(counter);
		// recursive_allocate(counter);
	}
	counter -= i;

	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		int* vaddr;
		vaddr = addrs[i];
		if (vaddr == NULL) {
			break;
		}
		if (counter % 1000 == 0) {
			printf("Page #%d freeing %p\n", counter, vaddr);
			printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		}
		counter++;
		frame_free(vaddr);
	}
	free(addrs);
}
