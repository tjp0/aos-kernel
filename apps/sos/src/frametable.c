#include <cspace/cspace.h>
#include <frametable.h>
#include <mapping.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <string.h>
#include <sys/kassert.h>
#include <ut_manager/ut.h>
#include <utils/page.h>
#include <vmem_layout.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/panic.h>

#define FRAME_CACHE_SIZE 30
#define FRAME_CACHE_HIGH_WATER 20

#define FRAME_RESERVED 90000

// For demo purposes, this is approx 480k of userland frames
#define FRAME_TEST_MAX 0

#define DEBUG_VALUE 13210387

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
 * do a bunch of kasserts
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
	kassert((struct frame*)(frame_table_pointer) == &frame_table[index]);
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
	return ((seL4_Word)vaddr) - FRAME_VSTART + lowest_address;
}
static inline void* _paddr_to_vaddr(seL4_Word paddr) {
	return (void*)(paddr - lowest_address + FRAME_VSTART);
}
static inline struct frame* _vaddr_to_frame_cell(void* vaddr) {
	return _paddr_to_frame_cell(_vaddr_to_paddr(vaddr));
}
void* _frame_cell_to_vaddr(struct frame* frame_cell) {
	trace(5);
	return _paddr_to_vaddr(_frame_cell_to_paddr(frame_cell));
}
/*
 * Have simple wrappers for all of these functions that
 * do a bunch of kasserts
 */
static inline void check_paddr(seL4_Word paddr) {
	kassert(((paddr >> seL4_PageBits) << seL4_PageBits) ==
			_frame_cell_to_paddr(_paddr_to_frame_cell(paddr)));
	kassert(paddr == _vaddr_to_paddr(_paddr_to_vaddr(paddr)));
	kassert(paddr == _frame_cell_to_paddr(_paddr_to_frame_cell(paddr)));
	kassert(_paddr_to_vaddr(paddr) ==
			_frame_cell_to_vaddr(_paddr_to_frame_cell(paddr)));

	kassert(paddr ==
			_vaddr_to_paddr(_frame_cell_to_vaddr(_paddr_to_frame_cell(paddr))));
}

static inline void check_vaddr(void* vaddr) {
	kassert(vaddr == _paddr_to_vaddr(_vaddr_to_paddr(vaddr)));

	kassert(vaddr == _frame_cell_to_vaddr(_vaddr_to_frame_cell(vaddr)));
	kassert(_vaddr_to_paddr(vaddr) ==
			_frame_cell_to_paddr(_vaddr_to_frame_cell(vaddr)));
	kassert(vaddr ==
			_paddr_to_vaddr(_frame_cell_to_paddr(_vaddr_to_frame_cell(vaddr))));
	kassert((void*)vaddr >= (void*)FRAME_VSTART);
	if (vaddr > (void*)FRAME_VEND) {
		printf("%p", vaddr);
	}

	kassert((void*)vaddr < (void*)FRAME_VEND);
}
static inline void check_frame_cell(struct frame* frame_cell) {
	kassert(_frame_cell_to_paddr(frame_cell) ==
			_vaddr_to_paddr(_paddr_to_vaddr(_frame_cell_to_paddr(frame_cell))));
	kassert((frame_cell) ==
			(_paddr_to_frame_cell(_frame_cell_to_paddr(frame_cell))));
	kassert(_paddr_to_vaddr(_frame_cell_to_paddr(frame_cell)) ==
			_frame_cell_to_vaddr(frame_cell));

	kassert(_frame_cell_to_paddr(frame_cell) ==
			_vaddr_to_paddr(_frame_cell_to_vaddr(
				_paddr_to_frame_cell(_frame_cell_to_paddr(frame_cell)))));

	kassert(frame_cell ==
			_vaddr_to_frame_cell(_frame_cell_to_vaddr((frame_cell))));
	kassert(frame_cell ==
			_paddr_to_frame_cell(_frame_cell_to_paddr(frame_cell)));

	kassert((void*)frame_cell >= (void*)FRAME_TABLE_VSTART);
	kassert((void*)frame_cell < (void*)FRAME_TABLE_VEND);
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
	kassert(_paddr_to_frame_cell(_frame_cell_to_paddr(frame_table_pointer)) ==
			frame_table_pointer);
	return _frame_cell_to_paddr(frame_table_pointer);
}

void* frame_cell_to_vaddr(struct frame* frame_cell) {
	trace(5);
	check_frame_cell(frame_cell);
	kassert(_frame_cell_to_vaddr(frame_cell) < ((void*)FRAME_VEND));
	kassert(_frame_cell_to_vaddr(frame_cell) >= ((void*)FRAME_VSTART));
	trace(5);
	return _frame_cell_to_vaddr(frame_cell);
}

void sanity_check_frame_cell(struct frame* frame_cell) {
#ifdef HEAVY_VETTING
	check_frame_cell(frame_cell);
	kassert(frame_cell->debug_check == DEBUG_VALUE);
	kassert(frame_cell->status != GARBAGE);
#else
	(void)frame_cell;
#endif
}

void sanity_check_frame_table(int id) {
#ifdef HEAVY_VETTING
	check_stack_addr();
	dprintf(0, "Doing sanity check %d\n", id);
	int i = 0;
	for (i = 0; i < ft_numframes; i++) {
		sanity_check_frame_cell(&frame_table[i]);
	}
#else
	(void)id;
#endif
}

int frame_physical_alloc(void** vaddr) {
	if (ft_numframes - frame_count < FRAME_RESERVED) {
		return 1;
	}
	if (frame_count > FRAME_TEST_MAX && FRAME_TEST_MAX != 0) {
		return 1;
	}

	seL4_Word new_frame_addr = ut_alloc(seL4_PageBits);
	kassert(paddr_to_frame_cell(new_frame_addr) < &frame_table[ft_numframes]);
	kassert(paddr_to_frame_cell(new_frame_addr) > &frame_table[0]);
	seL4_ARM_Page new_frame_cap;
	if (new_frame_addr == 0) {
		return 1;
	}

	kassert(new_frame_addr < highest_address);
	/* Create the frame cap */
	int err = cspace_ut_retype_addr(new_frame_addr, seL4_ARM_SmallPageObject,
									seL4_PageBits, cur_cspace, &new_frame_cap);

	kassert(paddr_to_frame_cell(new_frame_addr) < &frame_table[ft_numframes]);
	kassert(paddr_to_frame_cell(new_frame_addr) > &frame_table[0]);

	conditional_panic(err, "Failed to retype UT memory into frame ?");

	err = map_page(new_frame_cap, seL4_CapInitThreadPD,
				   (seL4_Word)paddr_to_vaddr(new_frame_addr), seL4_AllRights,
				   seL4_ARM_Default_VMAttributes);

	conditional_panic(err, "Failed to map frame into SOS");
	kassert(new_frame_cap != 0);

	kassert(paddr_to_frame_cell(new_frame_addr)->status == FRAME_UNTYPED);
	paddr_to_frame_cell(new_frame_addr)->status = FRAME_FREE;
	paddr_to_frame_cell(new_frame_addr)->cap = new_frame_cap;
	check_frame_cell(paddr_to_frame_cell(new_frame_addr));

	*vaddr = paddr_to_vaddr(new_frame_addr);

	frame_count++;
	return 0;
}

int frame_physical_free(struct frame* frame) {
	kassert(frame->cap != 0);
	check_frame_cell(frame);
	cspace_delete_cap(cur_cspace, frame->cap);
	ut_free(frame_cell_to_paddr(frame), seL4_PageBits);

	frame->cap = 0;
	frame->status = FRAME_UNTYPED;
	frame_count--;
	return 1;
}

static void cache_frames() {
	dprintf(2, "Caching frames\n");
	while (frame_cache_tail < FRAME_CACHE_HIGH_WATER - 1) {
		void* new_frame;
		// sanity_check_frame_table(3333);
		int err = frame_physical_alloc(&new_frame);
		if (err) {
			dprintf(0, "Unable to cache frame\n");
			return;
		}
		kassert(new_frame != NULL);
		frame_cache_tail++;
		frame_cache[frame_cache_tail] = new_frame;
		dprintf(2, "Frame cached in addr %p\n", &frame_cache[frame_cache_tail]);
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

	kassert(vaddr_to_frame_cell(new_frame)->status == FRAME_FREE);
	kassert(vaddr_to_frame_cell(new_frame)->cap != 0);

	dprintf(3, "Allocated frame %p, frame cache is now %u\n", new_frame,
			frame_cache_tail);

	vaddr_to_frame_cell(new_frame)->status = FRAME_INUSE;
	kassert(vaddr_to_frame_cell(new_frame)->cap != 0);

	memset(new_frame, 0, PAGE_SIZE_4K);

	kassert((void*)new_frame >= (void*)FRAME_VSTART);
	kassert((void*)new_frame <= (void*)FRAME_VEND);
	return new_frame;
}

struct frame* get_frame(void* addr) {
	struct frame* frame = ((vaddr_to_frame_cell(addr)));
	kassert(addr == frame_cell_to_vaddr(frame));
	kassert(frame == vaddr_to_frame_cell(addr));
	return frame;
}

void frame_free(void* vaddr) {
	trace(5);
	kassert(vaddr_to_frame_cell(vaddr)->status == FRAME_INUSE);
	if (frame_cache_tail < FRAME_CACHE_SIZE - 1) {
		trace(5);
		frame_cache_tail++;
		frame_cache[frame_cache_tail] = vaddr;
		vaddr_to_frame_cell(vaddr)->status = FRAME_FREE;
	} else {
		trace(5);
		frame_physical_free(vaddr_to_frame_cell(vaddr));
	}
	dprintf(3, "Freed frame %p, frame cache is now %u\n", vaddr,
			frame_cache_tail);
	trace(5);
}
void ft_initialize(void) {
	seL4_Word offset = 0;

	while (offset < ft_size) {
		seL4_Word new_frame;

		seL4_Word paddr = ut_alloc(seL4_PageBits);

		void* vaddr = (void*)(FRAME_TABLE_VSTART + offset);
		kassert(vaddr < (void*)FRAME_TABLE_VEND);
		conditional_panic(paddr == 0,
						  "Not enough memory to allocate frame table");

		conditional_panic(FRAME_TABLE_VSTART + offset >= FRAME_TABLE_VEND,
						  "Frame table too big for allocated memory map");

		int err = cspace_ut_retype_addr(paddr, seL4_ARM_SmallPageObject,
										seL4_PageBits, cur_cspace, &new_frame);
		conditional_panic(err, "Frametable failed to retype memory at init");
		err = map_page(new_frame, seL4_CapInitThreadPD, (seL4_Word)vaddr,
					   seL4_AllRights, seL4_ARM_Default_VMAttributes);
		conditional_panic(err, "Frametable failed to map memory at init");

		offset += (1 << seL4_PageBits);
	}
	frame_table = (struct frame*)FRAME_TABLE_VSTART;

	int i = 0;
	for (i = 0; i < ft_numframes; ++i) {
		frame_table[i].status = FRAME_UNTYPED;
		frame_table[i].debug_check = DEBUG_VALUE;
	}

	kassert((void*)frame_table >= (void*)FRAME_TABLE_VSTART);
	kassert((void*)&frame_table[ft_numframes] <= (void*)FRAME_TABLE_VEND);

	sanity_check_frame_table(2020);
}
static inline void check_stack_addr(void) {
	volatile int puppies;
	kassert((void*)&puppies > ((void*)KERNEL_STACK_VSTART) &&
			((void*)&puppies < (void*)KERNEL_STACK_VEND));
}

void recursive_allocate(int counter);

void frame_test() {
	printf("Highest address of cell: %p with additional size 0x%x\n",
		   _paddr_to_frame_cell(highest_address), sizeof(struct frame));

	struct frame* highest = _paddr_to_frame_cell(highest_address);

	kassert((void*)_paddr_to_frame_cell(highest_address) <
			(void*)FRAME_TABLE_VEND);
	kassert((void*)_paddr_to_frame_cell(lowest_address) ==
			(void*)FRAME_TABLE_VSTART);

	/* This fials */
	printf("Highest vaddr %p\n", _frame_cell_to_vaddr(highest));
	kassert(_frame_cell_to_vaddr(highest) < (void*)FRAME_VEND);

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
		kassert(vaddr != NULL);

		/* Test you can touch the page */
		*vaddr = 0x37 + i;
		kassert(*vaddr == 0x37 + i);
		// sanity_check_frame_table((i * 4) + 2);

		printf("Page #%d allocated at (vaddr=%p, ft_addr=%p)\n", i,
			   (void*)vaddr, vaddr_to_frame_cell(vaddr));
		// sanity_check_frame_table((i * 4) + 3);
	}
	printf("Test 1 complete\n");
	/* Test that you never run out of memory if you always free frames. */
	for (int i = 0; i < 10000; i++) {
		/* Allocate a page */
		int* vaddr;
		vaddr = frame_alloc();
		kassert(vaddr != NULL);

		/* Test you can touch the page */
		*vaddr = 0x37;
		kassert(*vaddr == 0x37);

		/* print every 1000 iterations */
		if (i % 1 == 1000) {
			printf("Page #%d allocated at (vaddr=%p, ft_addr=%p)\n", i,
				   (void*)vaddr, vaddr_to_frame_cell(vaddr));
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
	// 		dprintf(20,"Out of memory!\n");
	// 		break;
	// 	}

	// 	 Test you can touch the page
	// 	*vaddr = 0x37;
	// 	kassert(*vaddr == 0x37);
	// 	counter++;

	// 	if (counter % 1000 == 0) {
	// 		dprintf(20,"Page #%d allocated at %p\n", counter, vaddr);
	// 	}
	// }

	// free all the memory
}
#define NUM_PAGE_PER_HIT 100
void recursive_allocate(int counter) {
	// sanity_check_frame_table(0);
	check_stack_addr();

	int is_first = (counter == 0);
	void** addrs = malloc(NUM_PAGE_PER_HIT * sizeof(void*));

	if (!addrs) {
		printf("MALLOC Error!\n");
		return;
	}
	int i = 0;
	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		void* vaddr;
		vaddr = frame_alloc();
		// sanity_check_frame_table(0);
		if (vaddr == NULL) {
			printf("Out of memory!\n");
			break;
		}

		/* Test you can touch the page */
		*(int*)vaddr = 0x37;
		kassert(*(int*)vaddr == 0x37);
		memset(vaddr, 3, PAGE_SIZE_4K);
		addrs[i] = vaddr;

		if (is_first || counter % 1000 == 0) {
			printf("Page #%d allocated at %p\n", counter, vaddr);
			printf("frame cell address %p\n", vaddr_to_frame_cell(vaddr));
			printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		}
		counter++;
	}
	// something is killing some of my frame table entries
	counter -= i;
	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		void* vaddr;
		vaddr = addrs[i];
		if (vaddr == NULL) {
			break;
		}
		// if (is_first) {  //(counter % 1000 == 0) {
		// 	dprintf(20,"Page #%d addr %p\n", counter, vaddr);
		// 	dprintf(20,"cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
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
		kassert(vaddr_to_frame_cell(vaddr)->cap != 0);
	}

	if (i == NUM_PAGE_PER_HIT) {
		recursive_allocate(counter);
		// recursive_allocate(counter);
	}

	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		void* vaddr;
		vaddr = addrs[i];

		// sanity_check_frame_table(0);
		if (vaddr == NULL) {
			break;
		}

		memset(vaddr, 7, PAGE_SIZE_4K);
		// if (is_first) {  //(counter % 1000 == 0) {
		// 	dprintf(20,"Page #%d freeing %p\n", counter, vaddr);
		// 	dprintf(20,"cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		// }
		// counter++;
		if (vaddr_to_frame_cell(vaddr)->cap == 0) {
			printf(" BAAADDD -----------------\naddr %p, cap %x, status %d\n",
				   vaddr, vaddr_to_frame_cell(vaddr)->cap,
				   vaddr_to_frame_cell(vaddr)->status);
		}
		// kassert(vaddr_to_frame_cell(vaddr)->cap != 0);
	}
	counter -= i;

	for (i = 0; i < NUM_PAGE_PER_HIT; ++i) {
		/* Allocate a page */
		void* vaddr;
		vaddr = addrs[i];
		if (vaddr == NULL) {
			break;
		}
		if (is_first || counter % 1000 == 0) {
			printf("Page #%d freeing %p\n", counter, vaddr);
			printf("cap == %x\n", vaddr_to_frame_cell(vaddr)->cap);
		}
		counter++;
		kassert((void*)vaddr < (void*)FRAME_VEND &&
				(void*)vaddr >= (void*)FRAME_VSTART);
		kassert(IS_ALIGNED_4K((unsigned long)vaddr));
		// sanity_check_frame_table(0);
		memset(vaddr, 4, PAGE_SIZE_4K);
		// sanity_check_frame_table(0);
		frame_free(vaddr);
		// sanity_check_frame_table(0);
	}
	// sanity_check_frame_table(0);
	free(addrs);
	// sanity_check_frame_table(0);
}
