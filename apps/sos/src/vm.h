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

struct page_table_entry {
	uint8_t permissions;
	vaddr_t address;
	struct frame* frame;
	seL4_ARM_Page cap;
};

struct page_table {
	seL4_ARM_PageTable seL4_pt;
	struct page_table_entry* ptes[PTES_PER_TABLE];
};

struct page_directory {
	seL4_ARM_PageDirectory seL4_pd;
	struct page_table* pts[PTS_PER_DIRECTORY];
};

struct page_directory* pd_create(seL4_ARM_PageDirectory seL4_pd);

int vm_missingpage(struct vspace* vspace, vaddr_t address);

struct page_table_entry* pd_getpage(struct page_directory* pd, vaddr_t address);

struct page_table_entry* sos_map_page(struct page_directory* pd,
									  vaddr_t address, uint8_t permissions);

void sos_handle_vmfault(struct process* process);