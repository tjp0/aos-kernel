#include <assert.h>
#include <cspace/cspace.h>
#include <mapping.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <ut_manager/ut.h>
#include <vmem_layout.h>

#define verbose 0
#include <sys/debug.h>
#include <sys/panic.h>

#define PADDR_TO_FRAME_INDEX(x) (((unsigned int)x) >> seL4_PageBits)
#define VADDR_TO_FRAME_INDEX(x) (PADDR_TO_FRAME_INDEX(x - FRAME_VSTART))
#define FRAME_INDEX_TO_VADDR(x) (x << seL4_PageBits);

#define FRAME_CACHE_SIZE 30
#define FRAME_CACHE_HIGH_WATER 20
#define FRAME_RESERVED 120

struct frame* frame_table = NULL;

uint32_t frame_count = 0;
seL4_Word ft_size;
seL4_Word ft_numframes = 0;

enum frame_status { FRAME_INUSE, FRAME_UNTYPED, FRAME_FREE };

struct frame {
	enum frame_status status;
	seL4_ARM_Page cap;
};

inline int frame_to_index(struct frame* frame) {
	return ((frame - frame_table) / sizeof(struct frame));
}

uint32_t frame_cache_tail = 0;
void* frame_cache[FRAME_CACHE_SIZE];

int frame_physical_alloc(void** vaddr) {
	if (ft_numframes - frame_count < FRAME_RESERVED) {
		return 1;
	}

	seL4_Word new_frame_addr = ut_alloc(seL4_PageBits);

	assert(PADDR_TO_FRAME_INDEX(new_frame_addr) < ft_numframes);

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

	struct frame frame;
	frame.status = FRAME_FREE;
	frame.cap = new_frame;

	frame_table[PADDR_TO_FRAME_INDEX(new_frame_addr)] = frame;
	*vaddr = (void*)FRAME_VSTART + new_frame_addr;

	frame_count++;
	return 0;
}

int frame_physical_free(struct frame* frame) {
	cspace_delete_cap(cur_cspace, frame->cap);
	frame->cap = 0;
	frame->status = FRAME_UNTYPED;

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

	dprintf(2, "Allocated frame %p, frame cache is now %u\n", new_frame,
			frame_cache_tail);
	frame_table[VADDR_TO_FRAME_INDEX(new_frame)].status = FRAME_INUSE;

	return new_frame;
}

void frame_free(void* vaddr) {
	if (frame_cache_tail < FRAME_CACHE_SIZE) {
		frame_cache_tail++;
		frame_cache[frame_cache_tail] = vaddr;
		frame_table[VADDR_TO_FRAME_INDEX(vaddr)].status = FRAME_FREE;
	} else {
		frame_physical_free(&frame_table[VADDR_TO_FRAME_INDEX(vaddr)]);
	}
	dprintf(2, "Freed frame %p, frame cache is now %u\n", vaddr,
			frame_cache_tail);
}

seL4_Word ft_early_initialize(const seL4_BootInfo* info) {
	uint32_t max_paddr = 0;
	for (int i = 0; i < info->untyped.end - info->untyped.start; i++) {
		uint32_t paddr =
			info->untypedPaddrList[i] + (1 << info->untypedSizeBitsList[i]);
		if (paddr > max_paddr) {
			max_paddr = paddr;
		}
	}
	ft_numframes = (max_paddr) / (1 << seL4_PageBits) + 1;
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

void frame_test() {
	/* Allocate 10 pages and make sure you can touch them all */
	for (int i = 0; i < 10; i++) {
		/* Allocate a page */
		int* vaddr;
		vaddr = frame_alloc();
		assert(vaddr != NULL);

		/* Test you can touch the page */
		*vaddr = 0x37;
		assert(*vaddr == 0x37);

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
	int counter = 0;
	while (1) {
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
		counter++;

		if (counter % 1000 == 0) {
			printf("Page #%d allocated at %p\n", counter, vaddr);
		}
	}
}