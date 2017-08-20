#include <assert.h>
#include <cspace/cspace.h>
#include <frametable.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ut_manager/ut.h>
#include <utils/page.h>
#include <vm.h>

#define verbose 2
#include <sys/debug.h>
#include <sys/panic.h>

#define PTES_PER_TABLE 256
#define PTS_PER_DIRECTORY 4096

static inline uint32_t vaddr_to_ptsoffset(vaddr_t address) {
	return ((address >> 20) & 0x000000FF);
}

static inline uint32_t vaddr_to_pteoffset(vaddr_t address) {
	return (address >> 24);
}

static struct page_table* create_pt(struct page_directory* pd,
									vaddr_t address) {
	struct page_table* pt = malloc(sizeof(struct page_table));
	memset(pt, 0, sizeof(struct page_table));

	seL4_Word pt_addr;
	seL4_ARM_PageTable pt_cap;
	int err;

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
		goto fail1;
	}

	pt->seL4_pt = pt_cap;

	return pt;

fail1:
	cspace_delete_cap(cur_cspace, pt_cap);
	return NULL;
fail2:
	ut_free(pt_addr, seL4_PageTableBits);
fail3:
	return NULL;
}

static struct page_table_entry* create_pte(vaddr_t address,
										   uint8_t permissions) {
	struct page_table_entry* pte = malloc(sizeof(struct page_table_entry));
	memset(pte, 0, sizeof(struct page_table));
	pte->address = address;
	pte->permissions = permissions;

	void* new_frame = frame_alloc();
	if (new_frame == NULL) {
		free(pte);
		return NULL;
	}

	struct frame* frame = get_frame(new_frame);
	pte->frame = frame;
	return pte;
}

struct page_directory* pd_create(seL4_ARM_PageDirectory seL4_pd) {
	struct page_directory* pd = malloc(sizeof(struct page_directory));
	if (!pd) {
		return NULL;
	}
	pd = memset(pd, 0, sizeof(struct page_directory));
	pd->seL4_pd = seL4_pd;
	return pd;
};

struct page_table_entry* pd_getpage(struct page_directory* pd,
									vaddr_t address) {
	address = PAGE_ALIGN_4K(address);
	struct page_table* pt = pd->pts[vaddr_to_ptsoffset(address)];
	if (pt == NULL) {
		return NULL;
	}
	struct page_table_entry* pte = pt->ptes[vaddr_to_pteoffset(address)];
	return pte;
}

struct page_table_entry* pd_createpage(struct page_directory* pd,
									   vaddr_t address, uint8_t permissions) {
	address = PAGE_ALIGN_4K(address);
	struct page_table* pt = pd->pts[vaddr_to_ptsoffset(address)];
	if (pt == NULL) {
		pt = create_pt(pd, address);
		if (pt == NULL) {
			return NULL;
		}
		pd->pts[vaddr_to_ptsoffset(address)] = pt;
	}
	struct page_table_entry* pte = pt->ptes[vaddr_to_pteoffset(address)];

	if (pte == NULL) {
		pte = create_pte(address, permissions);
	}
	return pte;
}

int pd_map_page(struct page_directory* pd, struct page_table_entry* page) {
	struct page_table* pt = pd->pts[vaddr_to_ptsoffset(page->address)];

	assert(pt != NULL);
	assert(pd->seL4_pd != 0);
	seL4_CPtr vcap;

	vcap = cspace_copy_cap(cur_cspace, cur_cspace, page->frame->cap,
						   seL4_AllRights);
	int err;
	if (vcap == 0) {
		err = 1;
		return err;
	}
	err = seL4_ARM_Page_Map(page->frame->cap, pd->seL4_pd, page->address,
							seL4_AllRights, 0);

	if (err) {
		return 1;
	}
	return 0;
}

int vm_missingpage(struct vspace* vspace, vaddr_t address) {
	printf("VM MISSING PAGE CALLED\n");

	// TODO
	return 0;
}