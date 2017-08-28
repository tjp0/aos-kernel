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

static inline struct frame* _paddr_to_frame_cell(seL4_Word paddr);
static inline seL4_Word _frame_cell_to_paddr(struct frame* frame_table_pointer);
static inline seL4_Word _vaddr_to_paddr(void* vaddr);
static inline void* _paddr_to_vaddr(seL4_Word paddr);
static inline struct frame* _vaddr_to_frame_cell(void* vaddr);
void* _frame_cell_to_vaddr(struct frame* frame_cell);
/*
 * Have simple wrappers for all of these functions that
 * do a bunch of asserts
 */
static inline void check_paddr(seL4_Word paddr);
static inline void check_vaddr(void* vaddr);
static inline void check_frame_cell(struct frame* frame_cell);

static inline struct frame* paddr_to_frame_cell(seL4_Word paddr);
static inline void* paddr_to_vaddr(seL4_Word paddr);
static inline seL4_Word vaddr_to_paddr(void* vaddr);
static inline struct frame* vaddr_to_frame_cell(void* vaddr);
static inline seL4_Word frame_cell_to_paddr(struct frame* frame_table_pointer);

void* frame_cell_to_vaddr(struct frame* frame_cell);

void sanity_check_frame_table(int id);
void sanity_check_frame_cell(struct frame* frame_cell);

uint32_t frame_cache_tail = 0;
void* frame_cache[FRAME_CACHE_SIZE];
static inline void check_stack_addr(void);

static inline struct frame* _paddr_to_frame_cell(seL4_Word paddr) {
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

static inline seL4_Word _frame_cell_to_paddr(
	struct frame* frame_table_pointer) {
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

static inline seL4_Word _vaddr_to_paddr(void* vaddr) {
	return ((seL4_Word)vaddr) - FRAME_VSTART;
}
static inline void* _paddr_to_vaddr(seL4_Word paddr) {
	return (void*)(paddr + FRAME_VSTART);
}
static inline struct frame* _vaddr_to_frame_cell(void* vaddr) {
	return _paddr_to_frame_cell(_vaddr_to_paddr(vaddr));
}
void* _frame_cell_to_vaddr(struct frame* frame_cell) {
	return _paddr_to_vaddr(_frame_cell_to_paddr(frame_cell));
}
/*
 * Have simple wrappers for all of these functions that
 * do a bunch of asserts
 */
static inline void check_paddr(seL4_Word paddr) {
	assert(((paddr >> seL4_PageBits) << seL4_PageBits) ==
		   _frame_cell_to_paddr(_paddr_to_frame_cell(paddr)));
	assert(paddr == _vaddr_to_paddr(_paddr_to_vaddr(paddr)));
	assert(paddr == _frame_cell_to_paddr(_paddr_to_frame_cell(paddr)));
	assert(_paddr_to_vaddr(paddr) ==
		   _frame_cell_to_vaddr(_paddr_to_frame_cell(paddr)));

	assert(paddr ==
		   _vaddr_to_paddr(_frame_cell_to_vaddr(_paddr_to_frame_cell(paddr))));
}

static inline void check_vaddr(void* vaddr) {
	assert(vaddr == _paddr_to_vaddr(_vaddr_to_paddr(vaddr)));

	if (vaddr != _frame_cell_to_vaddr(_vaddr_to_frame_cell(vaddr))) {
		printf("va %p\n", vaddr);
		printf("fk %p\n", _frame_cell_to_vaddr(_vaddr_to_frame_cell(vaddr)));
	}
	assert(vaddr == _frame_cell_to_vaddr(_vaddr_to_frame_cell(vaddr)));
	assert(_vaddr_to_paddr(vaddr) ==
		   _frame_cell_to_paddr(_vaddr_to_frame_cell(vaddr)));
	assert(vaddr ==
		   _paddr_to_vaddr(_frame_cell_to_paddr(_vaddr_to_frame_cell(vaddr))));
	assert((void*)vaddr >= (void*)FRAME_VSTART);
	assert((void*)vaddr <= (void*)FRAME_VEND);
}
static inline void check_frame_cell(struct frame* frame_cell) {
	assert(_frame_cell_to_paddr(frame_cell) ==
		   _vaddr_to_paddr(_paddr_to_vaddr(_frame_cell_to_paddr(frame_cell))));
	assert((frame_cell) ==
		   (_paddr_to_frame_cell(_frame_cell_to_paddr(frame_cell))));
	assert(_paddr_to_vaddr(_frame_cell_to_paddr(frame_cell)) ==
		   _frame_cell_to_vaddr(frame_cell));

	assert(_frame_cell_to_paddr(frame_cell) ==
		   _vaddr_to_paddr(_frame_cell_to_vaddr(
			   _paddr_to_frame_cell(_frame_cell_to_paddr(frame_cell)))));

	assert(frame_cell ==
		   _vaddr_to_frame_cell(_frame_cell_to_vaddr((frame_cell))));
	assert(frame_cell ==
		   _paddr_to_frame_cell(_frame_cell_to_paddr(frame_cell)));

	// if (frame_cell->cap == 0 && frame_cell->status != FRAME_UNTYPED) {
	// 	printf("addr %p, cap %x, status %d\n", _frame_cell_to_vaddr(frame_cell),
	// 		   frame_cell->cap, frame_cell->status);
	// }
	if (frame_cell->status == FRAME_INUSE || frame_cell->status == FRAME_FREE) {
		assert(frame_cell->cap != 3);
		assert(frame_cell->cap != 7);
		assert(frame_cell->cap != 0);
		assert(frame_cell->cap != CSPACE_NULL);
	}
	assert((void*)frame_cell >= (void*)FRAME_TABLE_VSTART);
	assert((void*)frame_cell <= (void*)FRAME_TABLE_VEND);
}

static inline struct frame* paddr_to_frame_cell(seL4_Word paddr) {
	check_paddr(paddr);
	return _paddr_to_frame_cell(paddr);
}

static inline void* paddr_to_vaddr(seL4_Word paddr) {
	check_paddr(paddr);
	return _paddr_to_vaddr(paddr);
}

static inline seL4_Word vaddr_to_paddr(void* vaddr) {
	check_vaddr(vaddr);
	return _vaddr_to_paddr(vaddr);
}

static inline struct frame* vaddr_to_frame_cell(void* vaddr) {
	check_vaddr(vaddr);
	return _vaddr_to_frame_cell(vaddr);
}

static inline seL4_Word frame_cell_to_paddr(struct frame* frame_table_pointer) {
	check_frame_cell(frame_table_pointer);
	return _frame_cell_to_paddr(frame_table_pointer);
}

void* frame_cell_to_vaddr(struct frame* frame_cell) {
	check_frame_cell(frame_cell);
	return _frame_cell_to_vaddr(frame_cell);
}

void sanity_check_frame_cell(struct frame* frame_cell) {
	check_frame_cell(frame_cell);
	assert(frame_cell->status != GARBAGE);
}

void sanity_check_frame_table(int id) {
	check_stack_addr();
	printf("Doing sanity check %d\n", id);
	int i = 0;
	// int is_bad = 0;
	for (i = 0; i < ft_numframes; i++) {
		sanity_check_frame_cell(&frame_table[i]);
		// // printf("%d\n", i);
		// if (frame_table[i].status == GARBAGE) {
		// 	if (!is_bad) {
		// 		printf("bad range. status == 0\n");
		// 		printf("start %p\n", &frame_table[i]);
		// 	}
		// 	is_bad = 1;
		// } else {
		// 	if (is_bad) {
		// 		printf("end %p\n", &frame_table[i]);
		// 	}
		// 	is_bad = 0;
		// }
	}
}

int frame_physical_alloc(void** vaddr) {
	// sanity_check_frame_table(13371337);
	if (ft_numframes - frame_count < FRAME_RESERVED) {
		return 1;
	}

	seL4_Word new_frame_addr = ut_alloc(seL4_PageBits);
	// printf("new untyped memory %p\n", (void*)new_frame_addr);

	// sanity_check_frame_table(13371338);
	assert(paddr_to_frame_cell(new_frame_addr) < &frame_table[ft_numframes]);
	assert(paddr_to_frame_cell(new_frame_addr) > &frame_table[0]);
	seL4_ARM_Page new_frame_cap;
	if (new_frame_addr == 0) {
		return 1;
	}
	// sanity_check_frame_table(13371339);
	/* Create the frame cap */
	int err = cspace_ut_retype_addr(new_frame_addr, seL4_ARM_SmallPageObject,
									seL4_PageBits, cur_cspace, &new_frame_cap);

	// sanity_check_frame_table(1337133901);

	assert(paddr_to_frame_cell(new_frame_addr) < &frame_table[ft_numframes]);
	assert(paddr_to_frame_cell(new_frame_addr) > &frame_table[0]);

	conditional_panic(err, "Failed to retype UT memory into frame ?");

	// sanity_check_frame_table(133713310);

	// rubbish vaddr FRAME_VSTART + new_frame_addr

	err =
		map_page(new_frame_cap, seL4_CapInitThreadPD,
				 (seL4_Word)paddr_to_vaddr(new_frame_addr), seL4_AllRights, 0);

	// sanity_check_frame_table(133713311);
	conditional_panic(err, "Failed to map frame into SOS");
	assert(new_frame_cap != 0);

	// sanity_check_frame_table(133713312);
	if (paddr_to_frame_cell(new_frame_addr)->status != FRAME_UNTYPED) {
		printf("frame cell address %p\n", paddr_to_frame_cell(new_frame_addr));
		printf(" BAAADDD -----------------\naddr %p, cap %x, status %d\n",
			   (void*)new_frame_addr, paddr_to_frame_cell(new_frame_addr)->cap,
			   paddr_to_frame_cell(new_frame_addr)->status);
	}
	assert(paddr_to_frame_cell(new_frame_addr)->status == FRAME_UNTYPED);
	paddr_to_frame_cell(new_frame_addr)->status = FRAME_FREE;
	paddr_to_frame_cell(new_frame_addr)->cap = new_frame_cap;
	check_frame_cell(paddr_to_frame_cell(new_frame_addr));

	*vaddr = (void*)FRAME_VSTART + new_frame_addr;

	frame_count++;

	// sanity_check_frame_table(0);
	return 0;
}

int frame_physical_free(struct frame* frame) {
	assert(frame->cap != 0);
	check_frame_cell(frame);
	cspace_delete_cap(cur_cspace, frame->cap);
	ut_free(
		frame_cell_to_paddr(frame),
		seL4_PageBits);  // this does a bunch of asserts in frame_cell_to_paddr
	frame->cap = 0;
	frame->status = FRAME_UNTYPED;
	frame_count--;
	// sanity_check_frame_table(0);
	return 1;
}

static void cache_frames() {
	// sanity_check_frame_table(2121121);
	dprintf(2, "Caching frames\n");
	while (frame_cache_tail < FRAME_CACHE_HIGH_WATER) {
		void* new_frame;
		// sanity_check_frame_table(3333);
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
	// void* new_frame;
	// int err = frame_physical_alloc(&new_frame);
	// if (err) {
	// 	dprintf(0, "Unable to cache frame\n");
	// 	return NULL;
	// }
	// vaddr_to_frame_cell(new_frame)->status = FRAME_INUSE;
	// assert(vaddr_to_frame_cell(new_frame)->cap != 0);
	// assert(new_frame != NULL);
	// sanity_check_frame_table(0);
	// return new_frame;
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

	if (vaddr_to_frame_cell(new_frame)->status != FRAME_FREE) {
		printf("addr %p, cap %x, status %d\n", new_frame,
			   vaddr_to_frame_cell(new_frame)->cap,
			   vaddr_to_frame_cell(new_frame)->status);
	}
	assert(vaddr_to_frame_cell(new_frame)->status == FRAME_FREE);
	assert(vaddr_to_frame_cell(new_frame)->cap != 0);

	dprintf(2, "Allocated frame %p, frame cache is now %u\n", new_frame,
			frame_cache_tail);

	vaddr_to_frame_cell(new_frame)->status = FRAME_INUSE;
	assert(vaddr_to_frame_cell(new_frame)->cap != 0);

	memset(new_frame, 3, PAGE_SIZE_4K);

	assert((void*)new_frame >= (void*)FRAME_VSTART);
	assert((void*)new_frame <= (void*)FRAME_VEND);
	return new_frame;
}

struct frame* get_frame(void* addr) {
	struct frame* frame = ((vaddr_to_frame_cell(addr)));
	assert(addr == frame_cell_to_vaddr(frame));
	assert(frame == vaddr_to_frame_cell(addr));
	return frame;
}

void frame_free(void* vaddr) {
	assert(vaddr_to_frame_cell(vaddr)->status == FRAME_INUSE);
	check_frame_cell(vaddr_to_frame_cell(vaddr));
	if (frame_cache_tail < FRAME_CACHE_SIZE) {
		frame_cache_tail++;
		frame_cache[frame_cache_tail] = vaddr;
		vaddr_to_frame_cell(vaddr)->status = FRAME_FREE;
		memset(vaddr, 7, PAGE_SIZE_4K);
	} else {
		frame_physical_free(vaddr_to_frame_cell(vaddr));
	}
	dprintf(2, "Freed frame %p, frame cache is now %u\n", vaddr,
			frame_cache_tail);
	// sanity_check_frame_table(0);
}
void ft_initialize(void) {
	seL4_Word offset = 0;

	while (offset < ft_size) {
		seL4_Word new_frame;

		seL4_Word paddr = ut_alloc(seL4_PageBits);

		void* vaddr = (void*)(FRAME_TABLE_VSTART + offset);
		conditional_panic(paddr == 0,
						  "Not enough memory to allocate frame table");
		// printf("paddr %p == %p\n", (void*)paddr,
		// (void*)_vaddr_to_paddr(vaddr)); printf("vaddr %p == %p\n",
		// (void*)vaddr, (void*)_paddr_to_vaddr(paddr)); assert(vaddr ==
		// paddr_to_vaddr(paddr)); assert(paddr == vaddr_to_paddr(vaddr));

		conditional_panic(FRAME_TABLE_VSTART + offset >= FRAME_TABLE_VEND,
						  "Frame table too big for allocated memory map");

		int err = cspace_ut_retype_addr(paddr, seL4_ARM_SmallPageObject,
										seL4_PageBits, cur_cspace, &new_frame);
		conditional_panic(err, "Frametable failed to retype memory at init");

		err = map_page(new_frame, seL4_CapInitThreadPD, (seL4_Word)vaddr,
					   seL4_AllRights, 0);

		conditional_panic(err, "Frametable failed to map memory at init");

		offset += (1 << seL4_PageBits);
	}
	frame_table = (struct frame*)FRAME_TABLE_VSTART;

	int i = 0;
	printf("Stack is at %p\n", &frame_table);
	printf("frame_table %p\n", frame_table);
	check_stack_addr();
	for (i = 0; i < ft_numframes; ++i) {
		check_stack_addr();
		// printf("%d\n", i);
		frame_table[i].status = FRAME_UNTYPED;
		check_frame_cell(&frame_table[i]);
		assert(frame_table[i].status == FRAME_UNTYPED);
	}
	printf("loop fnum %x\n", ft_numframes);

	for (i = 0; i < ft_numframes; ++i) {
		check_stack_addr();
		check_frame_cell(&frame_table[i]);
		if (frame_table[i].status != FRAME_UNTYPED) {
			printf("%x\n", i);
		}
		assert(frame_table[i].status == FRAME_UNTYPED);
	}

	for (i = 0; i < ft_numframes; ++i) {
		assert(frame_table[i].status == FRAME_UNTYPED);
	}

	printf("jk, u r awesome\n");

	printf("init sanity check\n");
	sanity_check_frame_table(4141);
	printf("DONE init\n");

	assert((void*)frame_table >= (void*)FRAME_TABLE_VSTART);
	assert((void*)&frame_table[ft_numframes] <= (void*)FRAME_TABLE_VEND);
}
static inline void check_stack_addr(void) {
	volatile int puppies;
	assert((void*)&puppies < (void*)FRAME_VSTART);
}

void recursive_allocate(int counter);

void frame_test() {
	printf("EVERYTHING MUST BE FINE HERE\n");
	sanity_check_frame_table(1337);
	printf("EVERYTHING MUST BE FINE HERE\n");
	/* Allocate 10 pages and make sure you can touch them all */
	for (int i = 0; i < 10; i++) {
		/* Allocate a page */
		int* vaddr;

		// sanity_check_frame_table((i * 4));
		vaddr = frame_alloc();
		// sanity_check_frame_table((i * 4) + 1);
		assert(vaddr != NULL);

		/* Test you can touch the page */
		*vaddr = 0x37 + i;
		assert(*vaddr == 0x37 + i);
		// sanity_check_frame_table((i * 4) + 2);

		printf("Page #%d allocated at %p\n", i, (void*)vaddr);
		// sanity_check_frame_table((i * 4) + 3);
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
	sanity_check_frame_table(0);

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
#define NUM_PAGE_PER_HIT 100
void recursive_allocate(int counter) {
	// sanity_check_frame_table(0);
	volatile int puppies;
	assert((void*)&puppies < (void*)FRAME_VSTART);

	int is_first = (counter == 0);
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
		// sanity_check_frame_table(0);
		if (vaddr == NULL) {
			printf("Out of memory!\n");
			break;
		}

		/* Test you can touch the page */
		*vaddr = 0x37;
		assert(*vaddr == 0x37);
		memset(vaddr, 3, PAGE_SIZE_4K);
		addrs[i] = vaddr;

		if (is_first || counter % 1000 == 0) {
			printf("Page #%d allocated at %p\n", counter, vaddr);
			printf("frame cell address %p\n", vaddr_to_frame_cell(vaddr));
			printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		}
		counter++;
	}
	printf("Pre\n");
	// something is killing some of my frame table entries
	counter -= i;
	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		int* vaddr;
		vaddr = addrs[i];
		if (vaddr == NULL) {
			break;
		}
		// if (is_first) {  //(counter % 1000 == 0) {
		// 	printf("Page #%d addr %p\n", counter, vaddr);
		// 	printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		// }
		counter++;
		if (vaddr_to_frame_cell(vaddr)->cap == 0) {
			printf("Page #%d BAD %p\n", counter, vaddr);
			printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		}
		if (vaddr_to_frame_cell(vaddr)->cap == 0) {
			printf("frame cell address %p\n", vaddr_to_frame_cell(vaddr));
			printf(" BAAADDD -----------------\naddr %p, cap %x, status %d\n",
				   vaddr, vaddr_to_frame_cell(vaddr)->cap,
				   vaddr_to_frame_cell(vaddr)->status);
		}
		assert(vaddr_to_frame_cell(vaddr)->cap != 0);
	}

	if (i == NUM_PAGE_PER_HIT) {
		recursive_allocate(counter);
		// recursive_allocate(counter);
	}

	printf("post\n");
	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		int* vaddr;
		vaddr = addrs[i];

		// sanity_check_frame_table(0);
		if (vaddr == NULL) {
			break;
		}
		memset(vaddr, 7, PAGE_SIZE_4K);
		// if (is_first) {  //(counter % 1000 == 0) {
		// 	printf("Page #%d freeing %p\n", counter, vaddr);
		// 	printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		// }
		// counter++;
		if (vaddr_to_frame_cell(vaddr)->cap == 0) {
			printf(" BAAADDD -----------------\naddr %p, cap %x, status %d\n",
				   vaddr, vaddr_to_frame_cell(vaddr)->cap,
				   vaddr_to_frame_cell(vaddr)->status);
		}
		// assert(vaddr_to_frame_cell(vaddr)->cap != 0);
	}
	counter -= i;

	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		int* vaddr;
		vaddr = addrs[i];
		if (vaddr == NULL) {
			break;
		}
		if (is_first || counter % 1000 == 0) {
			printf("Page #%d freeing %p\n", counter, vaddr);
			printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		}
		counter++;
		// sanity_check_frame_table(0);
		memset(vaddr, 3, PAGE_SIZE_4K);
		// sanity_check_frame_table(0);
		frame_free(vaddr);
		// sanity_check_frame_table(0);
	}
	// sanity_check_frame_table(0);
	free(addrs);
	// sanity_check_frame_table(0);
}
