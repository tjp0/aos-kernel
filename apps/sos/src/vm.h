#pragma once
#include <process.h>
#include <sel4/sel4.h>
#include <stdint.h>
#include <typedefs.h>

/*
 *	Virtual Memory System
 */

//#define VM_HEAVY_VETTING 108392

#define PAGE_SIZE PAGE_SIZE_4K
#define PTES_PER_TABLE 256
#define PTS_PER_DIRECTORY 4096

#define VM_NOREGION -1
#define VM_FAIL -2
#define VM_OKAY 0

/* The cap is managed outside the virtual memory subsystem */
#define PAGE_SPECIAL (1 << 7)
/* Don't cache memory mapped from this page */
#define PAGE_NOCACHE (1 << 6)
/* The flag that defines if the page has been recently accessed */
#define PAGE_ZOMBIE (1 << 5)
/* Pinned pages are allocated immediately and always kept in memory */
#define PAGE_PINNED (1 << 4)
#define PAGE_ACCESSED (1 << 3)
#define PAGE_EXECUTABLE (1 << 2)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_READABLE (1 << 0)
/* Mask for the permissions section of the flags */
#define PAGE_PERMMASK 0b00000011

struct page_table_entry {
	uint8_t flags;
	vaddr_t address;
	struct frame* frame;
	union {
		seL4_ARM_Page cap;
		int32_t disk_frame_offset;
	};
	struct page_directory* pd;
	struct page_table_entry* next;
	struct page_table_entry* prev;
#ifdef VM_HEAVY_VETTING
	uint32_t debug_check;
#endif
};

struct page_table {
	seL4_ARM_PageTable seL4_pt;
	struct page_table_entry* ptes[PTES_PER_TABLE];
};

struct page_directory {
	seL4_ARM_PageDirectory seL4_pd;
	struct page_table* pts[PTS_PER_DIRECTORY];
	uint32_t num_ptes;
};

/* Creates and fills a page directory from SOS's bootinfo */
struct page_directory* pd_createSOS(seL4_ARM_PageDirectory seL4_pd,
									const seL4_BootInfo* boot);

struct page_directory* pd_create(seL4_ARM_PageDirectory seL4_pd);
void pd_free(struct page_directory* pd);
void pte_free(struct page_table_entry* pte);

int vm_missingpage(struct vspace* vspace, vaddr_t address);

struct page_table_entry* pd_getpage(struct page_directory* pd, vaddr_t address);

struct page_table_entry* sos_map_page(struct page_directory* pd,
									  vaddr_t address, uint8_t permissions,
									  seL4_ARM_Page pte_cap);

void sos_handle_vmfault(struct process* process);

bool vm_pageisloaded(struct page_table_entry* pte);
int vm_swapin(struct page_table_entry* pte);
int vm_swappage(void);
int vm_init(void);

int32_t swapout_frame(const void* src);
int32_t swap_init(void);
int32_t swapin_frame(int32_t disk_page_offset, void* dst);
void swapfree_frame(int32_t disk_page_offset);
void pte_untrack(struct page_table_entry* pte);
