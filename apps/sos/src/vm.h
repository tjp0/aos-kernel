#pragma once
#include <process.h>
#include <sel4/sel4.h>
#include <stdint.h>
#include <typedefs.h>

/*
 *	Virtual Memory System
 */

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
/* Pinned pages are allocated immediately and always kept in memory */
#define PAGE_PINNED (1 << 4)
/* The flag that defines if the page has been recently accessed */
#define PAGE_ACCESSED (1 << 3)
#define PAGE_EXECUTABLE (1 << 2)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_READABLE (1 << 0)
/* Mask for the permissions section of the flags */
#define PAGE_PERMMASK 0b00000011

struct page_table_entry {
	uint8_t flags;
	vaddr_t address;
	/* if frame is NULL, we're paged out *
	 * or we're special (and pinned) */
	struct frame* frame;
	union {
		/* We either have a frame or a token to
		 * pass into swap.c */
		seL4_ARM_Page cap;
		int32_t disk_frame_offset;
	};
	/* The page directory that contains this PTE */
	struct page_directory* pd;
	/* Pointers used in the clock linked list */
	struct page_table_entry* next;
	struct page_table_entry* prev;
#ifdef VM_HEAVY_VETTING
	uint32_t debug_check;
#endif
};

struct page_table {
	seL4_ARM_PageTable seL4_pt;
	seL4_Word paddr;
	struct page_table_entry* ptes[PTES_PER_TABLE];
};

struct page_directory {
	seL4_ARM_PageDirectory seL4_pd;
	seL4_Word paddr;
	struct page_table* pts[PTS_PER_DIRECTORY];
	uint32_t num_ptes;
};

/* Creates and fills a page directory from SOS's bootinfo */
struct page_directory* pd_createSOS(seL4_ARM_PageDirectory seL4_pd,
									const seL4_BootInfo* boot);

/* Creates a new PD (pdcap is only used for bootstrapping SOS. It should
 * otherwise be set to 0 */
struct page_directory* pd_create(seL4_ARM_PageDirectory pdcap);

void pd_free(struct page_directory* pd);
void pte_free(struct page_table_entry* pte);

/* Called by copy.c incase trying to copy to page that isn't
 * mapped in yet, or it's currently swapped out */
int vm_missingpage(struct vspace* vspace, vaddr_t address);

/* Returns the PTE to a corresponding vaddr */
struct page_table_entry* pd_getpage(struct page_directory* pd, vaddr_t address);

/* Maps in a new page at <address>. pte_cap should be zero unless the cap is
 * externally
 * managed */
struct page_table_entry* sos_map_page(struct page_directory* pd,
									  vaddr_t address, uint8_t permissions,
									  seL4_ARM_Page pte_cap);

/* Handles a process VMfault and returns the parameters for IPC to return to the
 * process */
struct syscall_return sos_handle_vmfault(struct process* process);

/* Returns if a PTE is currently loaded in memory */
bool vm_pageisloaded(struct page_table_entry* pte);

int vm_init(void);

/* These are from swap.c. They are in charge of handling paging to disk */

/* Copies 4096 bytes from src and stores it in the pagefile; returns an
 * identifying token */
int32_t swapout_frame(const void* src);

int32_t swap_init(void);

/* Given an identifying token from swapout_frame; retrieved the stored data */
int32_t swapin_frame(int32_t disk_page_offset, void* dst);

/* Tells swap.c that the stored data is no longer required; and it is able to
 * free it without paging it in */
void swapfree_frame(int32_t disk_page_offset);

/* Special function for memory saving; if a PTE will never be unmapped or
 * touched;
 * removes it from the PD. Used for parts of SOS's initial memory (ELF segments)
 */
void pte_untrack(struct page_table_entry* pte);
