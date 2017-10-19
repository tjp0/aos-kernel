#include <assert.h>
#include <cspace/cspace.h>
#include <fault.h>
#include <frametable.h>
#include <sos_coroutine.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ut_manager/ut.h>
#include <utils/page.h>
#include <vm.h>

#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
#include <sys/panic.h>

static struct lock* paging_lock;
static struct page_table_entry* clock_pointer;

static struct frame* frame_alloc_swap();
void pte_free(struct page_table_entry* pte);
static int pd_map_page(struct page_directory* pd, struct page_table_entry* page,
					   seL4_ARM_Page pte_cap);
static int vm_updatepage(struct page_table_entry* pte);
static inline uint32_t vaddr_to_ptsoffset(vaddr_t address) {
	return ((address >> 20) & 0x00000FFF);
}

static inline uint32_t vaddr_to_pteoffset(vaddr_t address) {
	return ((address >> 12) & 0x000000FF);
}

static void clock_add(struct page_table_entry* pte) {
	kassert(pte != NULL);
	if (!(pte->flags & PAGE_PINNED)) {
		if (clock_pointer == NULL) {
			clock_pointer = pte;
			pte->next = pte;
			pte->prev = pte;
			return;
		}
		pte->next = clock_pointer;
		pte->prev = clock_pointer->prev;
		clock_pointer->prev = pte;
		pte->prev->next = pte;
	}
}

static void clock_remove(struct page_table_entry* pte) {
	kassert(pte != NULL);
	if (pte->next == NULL) {
		kassert(pte->flags & PAGE_PINNED);
		return;
	}
/* If we were pinned, we didn't need the swaplock */
#ifdef VM_HEAVY_VETTING
	kassert(pte->debug_check == VM_HEAVY_VETTING);
#endif
	kassert(pte->prev != NULL);
	kassert(pte->next != NULL);
	pte->prev->next = pte->next;
	pte->next->prev = pte->prev;
	clock_pointer = pte->next;
	pte->next = NULL;
	pte->prev = NULL;
	if (clock_pointer == pte) {
		clock_pointer = NULL;
	}
}

static struct page_table* create_pt(struct page_directory* pd, vaddr_t address,
									seL4_ARM_PageTable pt_cap) {
	struct page_table* pt = malloc(sizeof(struct page_table));
	if (pt == NULL) {
		goto fail3;
	}

	memset(pt, 0, sizeof(struct page_table));

	seL4_Word pt_addr;
	int err;

	if (pt_cap == 0) {
		/* Allocate a PT object */
		pt_addr = ut_alloc(seL4_PageTableBits);
		if (pt_addr == 0) {
			goto fail3;
		}
		/* Create the frame cap */
		err = cspace_ut_retype_addr(pt_addr, seL4_ARM_PageTableObject,
									seL4_PageTableBits, cur_cspace, &pt_cap);
		if (err) {
			goto fail2;
		}
		/* Tell seL4 to map the PT in for us */
		err = seL4_ARM_PageTable_Map(pt_cap, pd->seL4_pd, address,
									 seL4_ARM_Default_VMAttributes);
		if (err) {
			dprintf(2,
					"Failed creating page table, fail1, addr=0x%x, err=%u\n, "
					"reg=%d",
					address, err, seL4_GetMR(0));
			goto fail1;
		}
	}

	pt->seL4_pt = pt_cap;

	return pt;

fail1:
	cspace_delete_cap(cur_cspace, pt_cap);
	return NULL;
fail2:
	ut_free(pt_addr, seL4_PageTableBits);
fail3:
	dprintf(2, "Failed creating page table, fail2/3\n");
	return NULL;
}

static void pt_free(struct page_table* pt) {
	kassert(pt != NULL);
	kassert(pt->seL4_pt != 0);
	for (int i = 0; i < PTES_PER_TABLE; ++i) {
		if (pt->ptes[i] != NULL) {
			pte_free(pt->ptes[i]);
		}
	}
	cspace_delete_cap(cur_cspace, pt->seL4_pt);
	free(pt);
}
void pte_free(struct page_table_entry* pte) {
	kassert(pte);

	bool to_lock = !(pte->flags & PAGE_PINNED);
	if (to_lock) {
		trace(5);
		LOCK(paging_lock);
	} else {
		trace(5);
	}
	pte->pd->pts[vaddr_to_ptsoffset(pte->address)]
		->ptes[vaddr_to_pteoffset(pte->address)] = NULL;
	dprintf(3, "Destroying page %p at %p:%08x\n", pte, pte->pd, pte->address);
	pte->pd->num_ptes--;
	trace(5);
	if (!vm_pageisloaded(pte)) {
		trace(5);
		swapfree_frame(pte->disk_frame_offset);
	} else {
		trace(5);
		clock_remove(pte);
		/* If the page is "special", the cap is managed elsewhere */
		if (!(pte->flags & PAGE_SPECIAL)) {
			trace(5);
			kassert(pte->frame != NULL);
			frame_free(pte->frame);
			cspace_delete_cap(cur_cspace, pte->cap);
		}
	}
	trace(5);
	trace(5);
	if (to_lock) {
		trace(5);
		UNLOCK(paging_lock);
	}
	free(pte);
}

static struct page_table_entry* create_pte(struct page_directory* pd,
										   vaddr_t address, uint8_t flags,
										   seL4_ARM_Page pte_cap) {
	struct page_table_entry* pte = malloc(sizeof(struct page_table_entry));
	if (pte == NULL) {
		return NULL;
	}
	memset(pte, 0, sizeof(struct page_table_entry));
	pte->address = address;
	pte->flags = flags | PAGE_ACCESSED;
	pte->cap = pte_cap;
	pte->pd = pd;

	trace(5);
	if (pte_cap != 0) {
		pte->flags |= PAGE_PINNED;
		pte->flags |= PAGE_SPECIAL;
	}

	if (pte_cap == 0) {
		trace(5);
		struct frame* new_frame = frame_alloc_swap();
		trace(5);
		if (new_frame == NULL) {
			free(pte);
			return NULL;
		}
		pte->frame = new_frame;
		clock_add(pte);
		trace(5);
	}
#ifdef VM_HEAVY_VETTING
	pte->debug_check = VM_HEAVY_VETTING;
#endif
	return pte;
}

struct page_directory* pd_create(seL4_ARM_PageDirectory seL4_pd) {
	struct page_directory* pd = malloc(sizeof(struct page_directory));
	if (!pd) {
		return NULL;
	}
	pd = memset(pd, 0, sizeof(struct page_directory));
	pd->seL4_pd = seL4_pd;
	pd->num_ptes = 0;
	return pd;
};

void pd_free(struct page_directory* pd) {
	kassert(pd != NULL);
	kassert(pd->seL4_pd != 0);
	for (int i = 0; i < PTS_PER_DIRECTORY; ++i) {
		if (pd->pts[i] != NULL) {
			pt_free(pd->pts[i]);
		}
	}
	free(pd);
}

struct page_table_entry* pd_getpage(struct page_directory* pd,
									vaddr_t address) {
	trace(6);
	assert(pd != NULL);
	address = PAGE_ALIGN_4K(address);
	dprintf(6, "Finding page at: %p\n", (void*)address);
	dprintf(6, "Find page 1st index: %u\n", vaddr_to_ptsoffset(address));
	trace(6);
	struct page_table* pt = pd->pts[vaddr_to_ptsoffset(address)];
	trace(6);
	if (pt == NULL) {
		trace(6);
		return NULL;
	}
	trace(6);
	dprintf(6, "Find page 2nd index: %u\n", vaddr_to_pteoffset(address));
	struct page_table_entry* pte = pt->ptes[vaddr_to_pteoffset(address)];
	trace(6);

	return pte;
}

struct page_table_entry* pd_createpage(struct page_directory* pd,
									   vaddr_t address, uint8_t flags,
									   seL4_ARM_Page pte_cap) {
	assert(pd != NULL);
	address = PAGE_ALIGN_4K(address);
	if (address == 0) {
		return NULL;
	}
	dprintf(3, "Creating page at addr: 0x%x\n", address);
	struct page_table* pt = pd->pts[vaddr_to_ptsoffset(address)];
	dprintf(3, "New page 1st index: %u\n", vaddr_to_ptsoffset(address));
	if (pt == NULL) {
		pt = create_pt(pd, address, 0);
		if (pt == NULL) {
			dprintf(3, "Unable to create PT\n");
			return NULL;
		}
		pd->pts[vaddr_to_ptsoffset(address)] = pt;
	}
	dprintf(3, "New page 2nd index: %u\n", vaddr_to_pteoffset(address));
	struct page_table_entry* pte = pt->ptes[vaddr_to_pteoffset(address)];
	if (pte == NULL) {
		dprintf(2, "Creating new page table entry\n");
		pte = create_pte(pd, address, flags, pte_cap);

		if (pte == NULL) {
			dprintf(3, "Unable to create PTE\n");
			return NULL;
		}
		trace(5);
		dprintf(3, "Mapping page in\n");
		if (pd_map_page(pd, pte, pte_cap) < 0) {
			pte_free(pte);
			dprintf(3, "Unable to map PTE\n");
			return NULL;
		}
		pt->ptes[vaddr_to_pteoffset(address)] = pte;
		if (pte_cap == 0) {
			pd->num_ptes++;
		}
		dprintf(3, "New page created\n");
	}
	return pte;
}
struct page_table_entry* sos_map_page(struct page_directory* pd,
									  vaddr_t address, uint8_t permissions,
									  seL4_ARM_Page pte_cap) {
	if (pd == sos_process.vspace.pagetable) {
		dprintf(1, "Mapping 0x%08x into SOS with permissions: %u\n", address,
				permissions);
	}
	return pd_createpage(pd, address, permissions, pte_cap);
}

static int pd_map_page(struct page_directory* pd, struct page_table_entry* page,
					   seL4_ARM_Page pte_cap) {
	trace(4);
	struct page_table* pt = pd->pts[vaddr_to_ptsoffset(page->address)];
	bool cap_provided = (pte_cap != 0);

	kassert(pt != NULL);
	kassert(pd->seL4_pd != 0);
	if (pte_cap == 0) {
		kassert(page->frame->cap != 0);
		trace(5);
		pte_cap = cspace_copy_cap(cur_cspace, cur_cspace, page->frame->cap,
								  seL4_AllRights);
		trace(5);
		if (pte_cap == 0) {
			return VM_FAIL;
		}
	}
	page->cap = pte_cap;
	trace(4);
	int err;
	err = vm_updatepage(page);

	if (err) {
		trace(4);
		dprintf(0, "Failed to map page in, err code %d\n", err);
		if (!cap_provided) {
			cspace_delete_cap(cur_cspace, pte_cap);
			page->cap = 0;
		}
		return VM_FAIL;
	}
	return VM_OKAY;
}

/* called whenever a page doesn't exist in the pagetable */
int vm_missingpage(struct vspace* vspace, vaddr_t address) {
	dprintf(1, "VM MISSING PAGE CALLED AT ADDRESS %p\n", (void*)address);
	address = PAGE_ALIGN_4K(address);

	struct page_table_entry* pte;
	/* If the page exists, but is swapped out, swap it in */
	pte = pd_getpage(vspace->pagetable, address);
	if (pte != NULL) {
		int ret = vm_swapin(pte);
		return ret;
	}
	/* Otherwise check if we're in a valid region and start
	 * creating a new page */

	region_node* region = find_region(vspace->regions, address);
	if (region == NULL) {
		trace(5);
		dprintf(0, "Failed to find region for missing page\n");
		return VM_NOREGION;  // VM_NORWEGIAN lol
	}

	if ((pte = pd_createpage(vspace->pagetable, address, region->perm, 0)) ==
		NULL) {
		return VM_FAIL;
	}

	dprintf(2, "PTE is %p, getpage is %p\n", pte,
			pd_getpage(vspace->pagetable, address));

	assert(pd_getpage(vspace->pagetable, address) == pte);

	trace(5);
	region->load_page(region, vspace, address);
	trace(5);
	return VM_OKAY;
}

bool vm_pageisloaded(struct page_table_entry* pte) {
	kassert(pte != NULL);
	return ((pte->flags & PAGE_PINNED) || pte->frame != NULL);
}
/* Check if the page is set to inaccessible only for the purposes of keeping
 * track
 * of active pages */
bool vm_pageismarked(struct page_table_entry* pte) {
	return (!(pte->flags & PAGE_ACCESSED));
}
static int vm_swapout(struct page_table_entry* pte) {
	kassert(lock_owned(paging_lock));
	clock_remove(pte);
	trace(5);
	dprintf(1, "** pte %p\n", (void*)pte);
	kassert(pte != NULL);
	kassert(pte->frame != NULL);
	kassert(pte->cap != 0);
	kassert(!(pte->flags & PAGE_PINNED));
	dprintf(2, "Swapping out page %08x in addrspace %p\n", pte->address,
			pte->pd);

	seL4_ARM_Page_Unmap(pte->cap);
	cspace_delete_cap(cur_cspace, pte->cap);
	pte->cap = 0;
	struct frame* volatile temp_frame = pte->frame;
	pte->frame = NULL;

	pte->disk_frame_offset = swapout_frame(frame_to_vaddr(temp_frame));
	trace(5);
	dprintf(1, "** pte %p\n", (void*)pte);
	if (pte->disk_frame_offset < 0) {
		panic("Bad disk offset to swapout\n");
		return VM_FAIL;
	}

	dprintf(1, "Swapped out page %08x in addrspace %p to %08x on disk\n",
			pte->address, pte->pd, pte->disk_frame_offset);

	trace(5);
	dprintf(1, "** pte->frame: %p\n", (void*)temp_frame);
	frame_free(temp_frame);
	trace(5);
	return 0;
}
int vm_swapin(struct page_table_entry* pte) {
	trace(4);
	LOCK(paging_lock);
	kassert(pte != NULL);
	kassert(pte->frame == NULL);
	struct frame* volatile temp_frame = frame_alloc_swap();
	if (temp_frame == NULL) {
		dprintf(0, "Failed to allocate frame to swap in page\n");
		UNLOCK(paging_lock);
		return VM_FAIL;
	}
	dprintf(1, "Swapping in page %08x in addrspace %p from %08x on disk\n",
			pte->address, pte->pd, pte->disk_frame_offset);
	if (swapin_frame(pte->disk_frame_offset, frame_to_vaddr(temp_frame)) < 0) {
		goto fail3;
	}

	/* Create the frame cap */
	trace(5);
	pte->cap = cspace_copy_cap(cur_cspace, cur_cspace, temp_frame->cap,
							   seL4_AllRights);
	trace(5);
	if (pte->cap == 0) {
		goto fail3;
	}

	pte->flags |= PAGE_ACCESSED;
	trace(4);
	pte->frame = temp_frame;
	int err = vm_updatepage(pte);
	if (err) {
		trace(4);
		goto fail1;
	}
	seL4_ARM_Page_Unify_Instruction(pte->cap, 0, PAGE_SIZE_4K);
	seL4_ARM_Page_CleanInvalidate_Data(pte->cap, 0, PAGE_SIZE_4K);

	trace(4);
	clock_add(pte);
	trace(4);
	UNLOCK(paging_lock);
	return VM_OKAY;

fail1:
	trace(4);
	cspace_delete_cap(cur_cspace, pte->cap);
	pte->cap = 0;
	frame_free(temp_frame);
	trace(0);
	UNLOCK(paging_lock);
	return VM_FAIL;
fail3:
	frame_free((temp_frame));
	pte->frame = NULL;
	trace(0);
	UNLOCK(paging_lock);
	return VM_FAIL;
}

/* Could use a lookup table */
char* fault_getprintable(uint32_t reg) {
	if (fault_ispermissionfault(reg)) {
		return fault_iswritefault(reg) ? "permission fault (writing)"
									   : "permission fault (reading)";
	} else if (fault_isaccessfault(reg)) {
		return "access fault";
	} else if (fault_isalignmentfault(reg)) {
		return "alignment fault";
	}
	return "unknown fault";
}

void print_fault(struct fault f) {
	dprintf(0, "vm fault at 0x%08x, pc = 0x%08x, status=0x%x, %s %s\n", f.vaddr,
			f.pc, f.status, f.ifault ? "Instruction" : "Data",
			fault_getprintable(f.status));
	if (verbose > 0) {
		regions_print(current_process()->vspace.regions);
	}
}

struct syscall_return sos_handle_vmfault(struct process* process) {
	assert(process != NULL);
	trace(5);
	/* Page fault */
	struct fault fault = fault_struct();

	struct syscall_return retu = { };

	struct page_table_entry* pte =
		pd_getpage(process->vspace.pagetable, fault.vaddr);

	/* got page from pagetable */
	if (pte != NULL) {
		trace(5);
		if (!vm_pageisloaded(pte)) {
			trace(5);
			int err = vm_swapin(pte);
			if (err != VM_OKAY) {
				dprintf(0, "Out of memory\n");
				process_signal_kill(process);
				return retu;
			}
		} else if (vm_pageismarked(pte)) {
			trace(5);
			pte->flags |= PAGE_ACCESSED;
			vm_updatepage(pte);
			dprintf(2, "Unmarked page\n");
		} else {
			trace(5);
			dprintf(0, "Invalid memory permissions for process\n");
			dprint_fault(0, fault);
			trace(5);
			process_signal_kill(process);
			return retu;
		}
		trace(5);
		retu.valid = true;
		return retu;
	}

	/* If the page doesn't exist in the pagetable */
	if (fault_isaccessfault(fault.status)) {
		int err = vm_missingpage(&process->vspace, fault.vaddr);
		trace(5);
		if (err != VM_OKAY) {
			trace(5);
			dprintf(0, "Invalid memory access for process\n");
			dprint_fault(0, fault);
			trace(0);
			process_signal_kill(process);
			trace(0);
			return retu;
		}
		retu.valid = true;
		return retu;
	} else {
		trace(5);
		print_fault(fault);
		regions_print(process->vspace.regions);
		printf("Unable to handle process fault\n");
		process_signal_kill(process);
		return retu;
	}
}

static int vm_updatepage(struct page_table_entry* pte) {
	kassert(pte != NULL);
	kassert(pte->cap != 0);
	trace(4);
	seL4_ARM_Page_Unmap(pte->cap);
	seL4_Word permissions = 0;
	seL4_Word attributes = seL4_ARM_Default_VMAttributes;

	if (pte->flags & PAGE_READABLE) {
		permissions |= seL4_CanRead;
	}
	if (pte->flags & PAGE_WRITABLE) {
		permissions |= seL4_CanWrite;
	}
	if (!(pte->flags & PAGE_ACCESSED)) {
		permissions = 0;
	}
	if (pte->flags & PAGE_NOCACHE) {
		attributes = 0;
	}
	if (!(pte->flags & PAGE_EXECUTABLE)) {
		attributes |= seL4_ARM_ExecuteNever;
	}
	trace(4);
	kassert(pte->cap != 0);
	kassert(pte->pd != NULL);
	dprintf(4,
			"Trying to map page at 0x%08x with permissions 0x%08x and "
			"attributes 0x%08x\n",
			pte->address, permissions, attributes);
	int err = seL4_ARM_Page_Map(pte->cap, pte->pd->seL4_pd, pte->address,
								permissions, attributes);
	if (err) {
		dprintf(4, "Map mapping error: %u\n", err);
		trace(4);
		return VM_FAIL;
	}
	trace(4);
	return VM_OKAY;
}

int vm_init(void) {
	paging_lock = lock_create("Paging lock");
	conditional_panic(paging_lock == NULL, "Unable to create paging lock");
	return 0;
}

int vm_swappage(void) {
#ifdef CONFIG_SOS_DEBUG_NOSWAP
	return VM_FAIL;
#endif
	bool owned = lock_owned(paging_lock);
	if (!owned) LOCK(paging_lock);
	trace(4);
	bool found = false;
	if (clock_pointer == NULL) {
		UNLOCK(paging_lock);
		return VM_FAIL;
	}
	while (!found) {
		if (clock_pointer->flags & PAGE_ACCESSED) {
			trace(4);
			clock_pointer->flags &= ~PAGE_ACCESSED;
			vm_updatepage(clock_pointer);
			clock_pointer = clock_pointer->next;
		} else {
			break;
		}
	}
	trace(4);
	int ret = vm_swapout(clock_pointer);
	if (!owned) UNLOCK(paging_lock);
	return ret;
}

struct frame* frame_alloc_swap() {
	trace(5);
	struct frame* frame = frame_alloc();
	trace(5);
	while (frame == NULL) {
		if (vm_swappage() != VM_OKAY) {
			return NULL;
		}
		trace(5);
		frame = frame_alloc();
	}
	return frame;
}

extern void* __executable_start;
extern void* _end;

/* Ugly stuff below. Reader beware */

/* Removes the page from the VMM; this should only be done in very specific
 * circumstances to save memory where the CAPs are externally tracked (the frame
 * table allocator)
 * or never deleted
 */
void pte_untrack(struct page_table_entry* pte) {
	if (!(pte->flags & PAGE_SPECIAL && pte->flags & PAGE_PINNED)) {
		panic("Trying to untrack a normal page");
	}
	pte->pd->pts[vaddr_to_ptsoffset(pte->address)]
		->ptes[vaddr_to_pteoffset(pte->address)] = NULL;
	free(pte);
}
/* This is a bit ugly; used for initializing the page directory for SOS */
struct page_directory* pd_createSOS(seL4_CPtr pdcap,
									const seL4_BootInfo* boot) {
	struct page_directory* pd = pd_create(pdcap);
	kassert(pd != NULL); /* This function is only called at init,
							so we can panic if we fail */
	trace(5);
	uint32_t start = (uint32_t)&__executable_start;
	start = ALIGN_DOWN(start, 256 * PAGE_SIZE_4K);
	uint32_t end = (uint32_t)&_end;
	trace(5);
	/* For each ARM Page Table within our elf */
	seL4_CPtr pt_cap = boot->userImagePTs.start;
	seL4_CPtr pte_cap = boot->userImageFrames.start;
	for (uint32_t i = start; i < end; i += 256 * PAGE_SIZE_4K) {
		trace(5);
		if (pt_cap == boot->userImagePTs.end) {
			trace(5);
			break;
		}
		struct page_table* pt = create_pt(pd, i, pt_cap);
		kassert(pt != NULL);
		trace(5);
		pd->pts[vaddr_to_ptsoffset(i)] = pt;
		for (uint32_t j = i; i < 256 * PAGE_SIZE_4K; i += PAGE_SIZE_4K) {
			trace(5);
			if (pte_cap == boot->userImageFrames.end) {
				trace(5);
				break;
			}
			struct page_table_entry* pte = create_pte(
				pd, j,
				PAGE_EXECUTABLE | PAGE_PINNED | PAGE_READABLE | PAGE_WRITABLE,
				pt_cap);
			kassert(pte != NULL);
			trace(5);
			pt->ptes[vaddr_to_pteoffset(j)] = pte;
			pd->num_ptes++;
			pte_cap++;
			trace(5);
		}
		pt_cap++;
		trace(5);
	}
	trace(5);
	return pd;
}
