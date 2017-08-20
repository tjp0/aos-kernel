#include <assert.h>
#include <frametable.h>
#include <stdint.h>
#include <string.h>
#include <utils/page.h>
#include <vm.h>

#define verbose 5
#include <sys/debug.h>

/* This function is called when the copy happens inside a single page */
static int copy_sos2vspace_withinpage(void* src, vaddr_t dest_vaddr,
									  struct vspace* vspace, int64_t len) {
	struct page_table_entry* pte = pd_getpage(vspace->pagetable, dest_vaddr);
	if (pte == NULL) {
		/* Pretend that we just vmfaulted on the page */
		vm_missingpage(vspace, dest_vaddr);

		pte = pd_getpage(vspace->pagetable, dest_vaddr);
		if (pte == NULL) {
			return -1;
		}
	}
	assert(pte->frame != NULL);
	void* dst = frame_getvaddr(pte->frame);

	dprintf(0, "Copying from %p in KS to %p in US (%p in KS)\n", src,
			(void*)dest_vaddr, dst);
	memcpy(src, dst, len);
	return 0;
}

int64_t copy_sos2vspace(void* src, vaddr_t dest_vaddr, struct vspace* vspace,
						int64_t len) {
	int64_t start_len = len;
	/* Start by copying the part in the middle of the page */
	vaddr_t nearest_page = PAGE_ALIGN_4K(dest_vaddr + PAGE_SIZE);
	vaddr_t alignbytes = nearest_page - dest_vaddr;
	int copy_len = alignbytes;
	if (alignbytes > len) {
		copy_len = len;
	}

	if (copy_sos2vspace_withinpage(src, dest_vaddr, vspace, alignbytes) < 0) {
		return -1;
	}
	len -= copy_len;
	src += copy_len;
	dest_vaddr += copy_len;

	while (len > 0) {
		copy_len = len > PAGE_SIZE_4K ? PAGE_SIZE_4K : len;
		if (copy_sos2vspace_withinpage(src, dest_vaddr, vspace, copy_len) < 0) {
			return -1;
		}
		len -= copy_len;
		src += copy_len;
		dest_vaddr += copy_len;
	}
	return start_len;
}