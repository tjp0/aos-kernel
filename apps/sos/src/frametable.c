#include <autoconf.h>
#include <cspace/cspace.h>
#include <frametable.h>
#include <mapping.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <string.h>
#include <sys/kassert.h>
#include <ut_manager/ut.h>
#include <utils/page.h>
#include <vm.h>
#include <vmem_layout.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/panic.h>

#define FRAME_CACHE_SIZE 30
#define FRAME_CACHE_HIGH_WATER 20

#define FRAME_RESERVED 90000

struct frame* frame_table = NULL;

uint32_t frame_count = 0;

uint32_t frame_cache_tail = 0;
struct frame* frame_cache[FRAME_CACHE_SIZE];

static inline struct frame* paddr_to_frame(seL4_Word paddr) {
	// remove the sub page index of the address
	seL4_Word phy_frame_start_num = (paddr >> seL4_PageBits);
	// subtract the lowest address's high bytes to get the index
	seL4_Word index = phy_frame_start_num - (lowest_address >> seL4_PageBits);
	// get the byte index into the struct
	seL4_Word offset_into_table = index * sizeof(struct frame);
	// get an actual pointer in memory
	seL4_Word frame_table_pointer = offset_into_table + (seL4_Word)frame_table;
	// these should both be valid ways of finding this pointer
	kassert((struct frame*)(frame_table_pointer) == &frame_table[index]);
	return (struct frame*)frame_table_pointer;
}

static inline seL4_Word frame_to_paddr(struct frame* frame_table_pointer) {
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
	return ((seL4_Word)vaddr) - FRAME_VSTART + lowest_address;
}
static inline void* paddr_to_vaddr(seL4_Word paddr) {
	return (void*)(paddr - lowest_address + FRAME_VSTART);
}
struct frame* vaddr_to_frame(void* vaddr) {
	return paddr_to_frame(vaddr_to_paddr(vaddr));
}
void* frame_to_vaddr(struct frame* frame_cell) {
	return paddr_to_vaddr(frame_to_paddr(frame_cell));
}

struct frame* frame_physical_alloc(void) {
	trace(5);
	if (ft_numframes - frame_count < FRAME_RESERVED) {
		return NULL;
	}

	seL4_Word new_frame_addr = ut_alloc(seL4_PageBits);
	kassert(paddr_to_frame(new_frame_addr) < &frame_table[ft_numframes]);
	kassert(paddr_to_frame(new_frame_addr) > &frame_table[0]);
	trace(5);
	seL4_ARM_Page new_frame_cap;
	if (new_frame_addr == 0) {
		trace(5);
		return NULL;
	}

	kassert(new_frame_addr < highest_address);
	/* Create the frame cap */
	trace(5);
	int err = cspace_ut_retype_addr(new_frame_addr, seL4_ARM_SmallPageObject,
									seL4_PageBits, cur_cspace, &new_frame_cap);

	conditional_panic(err, "Failed to retype UT memory into frame ?");
	trace(5);
	struct page_table_entry* pte = sos_map_page(
		sos_process.vspace.pagetable, (vaddr_t)paddr_to_vaddr(new_frame_addr),
		PAGE_WRITABLE | PAGE_READABLE | PAGE_PINNED, new_frame_cap);
	if (pte == NULL) {
		panic("Failed to map frame into SOS");
	}
	trace(5);
	struct frame* newframe = vaddr_to_frame(paddr_to_vaddr(new_frame_addr));
	trace(5);
	newframe->cap = new_frame_cap;
	trace(5);
	frame_count++;
	return newframe;
}

int frame_physical_free(struct frame* frame) {
	kassert(frame->cap != 0);
	cspace_delete_cap(cur_cspace, frame->cap);
	ut_free(frame_to_paddr(frame), seL4_PageBits);

	frame->cap = 0;
	frame_count--;
	return 1;
}

static void cache_frames() {
	dprintf(2, "Caching frames\n");
	while (frame_cache_tail < FRAME_CACHE_HIGH_WATER - 1) {
		struct frame* new_frame;
		// sanity_check_frame_table(3333);
		trace(5);
		new_frame = frame_physical_alloc();
		trace(5);
		if (!new_frame) {
			dprintf(0, "Unable to cache frame\n");
			return;
		}
		kassert(new_frame != NULL);
		frame_cache_tail++;
		frame_cache[frame_cache_tail] = new_frame;
		dprintf(2, "Frame cached in addr %p\n", &frame_cache[frame_cache_tail]);
	}
}

struct frame* frame_alloc(void) {
	if (frame_cache_tail == 0) {
		trace(5);
		cache_frames();
	}

	trace(5);
	struct frame* new_frame = frame_cache[frame_cache_tail];

	frame_cache[frame_cache_tail] = NULL;

	if (frame_cache_tail > 0) {
		frame_cache_tail--;
	}
	trace(5);
	if (new_frame == NULL) {
		dprintf(0, "Unable to allocate frame\n");
		return NULL;
	}
	trace(5);
	kassert(new_frame->cap != 0);

	dprintf(3, "Allocated frame %p, frame cache is now %u\n", new_frame,
			frame_cache_tail);
	trace(5);
	memset(frame_to_vaddr(new_frame), 0, PAGE_SIZE_4K);

	return new_frame;
}

void frame_free(struct frame* frame) {
	trace(5);
	if (frame_cache_tail < FRAME_CACHE_SIZE - 1) {
		trace(5);
		frame_cache_tail++;
		frame_cache[frame_cache_tail] = frame;
	} else {
		trace(5);
		frame_physical_free(frame);
	}
	trace(5);
}
void ft_initialize(void) {
	trace(5);
	seL4_Word offset = 0;

	while (offset < ft_size) {
		trace(5);
		seL4_Word new_frame;

		seL4_Word paddr = ut_alloc(seL4_PageBits);

		void* vaddr = (void*)(FRAME_TABLE_VSTART + offset);
		kassert(vaddr < (void*)FRAME_TABLE_VEND);
		trace(5);
		conditional_panic(paddr == 0,
						  "Not enough memory to allocate frame table");

		conditional_panic(FRAME_TABLE_VSTART + offset >= FRAME_TABLE_VEND,
						  "Frame table too big for allocated memory map");
		trace(5);
		int err = cspace_ut_retype_addr(paddr, seL4_ARM_SmallPageObject,
										seL4_PageBits, cur_cspace, &new_frame);
		conditional_panic(err, "Frametable failed to retype memory at init");
		trace(5);
		struct page_table_entry* pte = sos_map_page(
			sos_process.vspace.pagetable, (vaddr_t)vaddr,
			PAGE_READABLE | PAGE_WRITABLE | PAGE_PINNED, new_frame);
		conditional_panic(pte == NULL,
						  "Frametable failed to map memory at init");

		offset += (1 << seL4_PageBits);
		trace(5);
	}
	frame_table = (struct frame*)FRAME_TABLE_VSTART;

	trace(5);
	kassert((void*)frame_table >= (void*)FRAME_TABLE_VSTART);
	kassert((void*)&frame_table[ft_numframes] <= (void*)FRAME_TABLE_VEND);
}
