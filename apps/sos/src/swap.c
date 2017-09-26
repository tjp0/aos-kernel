#include <devices/devices.h>
#include <filetable.h>
#include <sos_coroutine.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vm.h>

#define verbose 10
#include <sys/debug.h>
#include <sys/kassert.h>
int32_t swapout_frame(const void* src);
int32_t swapin_frame(int32_t disk_page_offset, void* dst);

/* Not really used, but useful for conceptual purposes */
struct disk_page {
	union {
		int32_t next_diskpage;
		char frame[PAGE_SIZE_4K];
	};
};

int32_t current_diskpage = -1;
int32_t current_offset = 0;
struct fd swapfile;
struct lock* swap_lock;

/* Current problems: Error checking
 * Locking/Unlocking needs to be written
 * dev read/write should directly take an offset parameter
 */

int32_t swap_init() {
	trace(4);
	swap_lock = lock_create();
	nfs_dev_open(&swapfile, "pagefile", FM_WRITE | FM_READ);
	return 0;
}

int32_t swapout_frame(const void* src) {
	trace(4);
	bool new_swap = false;
	lock(swap_lock);
	if (current_diskpage < 0) {
		trace(4);
		new_swap = true;
		current_diskpage = current_offset;
		current_offset += sizeof(struct disk_page);
	}
	trace(4);
	int32_t swapped_diskpage = current_diskpage;
	current_diskpage = -1;
	swapfile.offset = swapped_diskpage;
	/* Todo: Error handling */

	if (new_swap == false) {
		trace(4);
		kassert(swapfile.dev_read(&swapfile, NULL, (vaddr_t)&current_diskpage,
								  sizeof(int32_t)) == sizeof(int32_t));
		swapfile.offset = swapped_diskpage;
	}
	trace(4);
	kassert(swapfile.dev_write(&swapfile, NULL, (vaddr_t)src, PAGE_SIZE_4K) ==
			PAGE_SIZE_4K);
	unlock(swap_lock);
	trace(4);
	return swapped_diskpage;
}

int32_t swapin_frame(int32_t disk_page_offset, void* dst) {
	trace(4);
	lock(swap_lock);
	swapfile.offset = disk_page_offset;
	trace(4);
	kassert(swapfile.dev_read(&swapfile, NULL, (vaddr_t)dst, PAGE_SIZE_4K) ==
			PAGE_SIZE_4K);
	swapfile.offset = disk_page_offset;
	/* Race condition here: Multiple swapping in frames may corrupt pointers in
	 * LL*/
	trace(4);
	kassert(swapfile.dev_write(&swapfile, NULL, (vaddr_t)&current_diskpage,
							   sizeof(int32_t)) == sizeof(int32_t));
	current_diskpage = disk_page_offset;
	unlock(swap_lock);
	trace(4);
	return VM_OKAY;
}

/* The page is no longer needed, mark as unused */
void swapfree_frame(int32_t disk_page_offset) {
	lock(swap_lock);
	swapfile.offset = disk_page_offset;
	kassert(swapfile.dev_write(&swapfile, NULL, (vaddr_t)&current_diskpage,
							   sizeof(int32_t)) == sizeof(int32_t));
	current_diskpage = disk_page_offset;
	unlock(swap_lock);
}
