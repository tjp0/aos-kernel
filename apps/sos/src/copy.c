#include <assert.h>
#include <copy.h>
#include <frametable.h>
#include <stdint.h>
#include <string.h>
#include <utils/page.h>
#include <vm.h>
#define verbose 5
#include <sys/debug.h>

/* This function is called when the copy happens inside a single page */
static int copy_sos2vspace_withinpage(void* src, vaddr_t dest_vaddr,
									  struct vspace* vspace, int64_t len,
									  uint32_t flags) {
	struct page_table_entry* pte = pd_getpage(vspace->pagetable, dest_vaddr);
	if (pte == NULL) {
		/* Pretend that we just vmfaulted on the page */
		if (vm_missingpage(vspace, dest_vaddr) < 0) {
			dprintf(
				0,
				"Unable to copy to userspace because vm_page didn't work ?\n");
			return -1;
		}

		pte = pd_getpage(vspace->pagetable, dest_vaddr);
		if (pte == NULL) {
			dprintf(0, "Failed to map in page for copying into userspace\n");
			return -1;
		}
	}

	assert(pte->frame != NULL);
	void* dst = frame_getvaddr(pte->frame) + (dest_vaddr - PAGE_ALIGN_4K(dest_vaddr));

	dprintf(0, "Copying from %p in KS to %p in US (%p in KS)\n", src,
			(void*)dest_vaddr, dst);
	memcpy(dst, src, len);

	if (flags & COPY_FLUSH) {
		seL4_ARM_Page_Unify_Instruction(pte->frame->cap, 0, PAGE_SIZE_4K);
	}

	return 0;
}

int64_t copy_sos2vspace(void* src, vaddr_t dest_vaddr, struct vspace* vspace,
						int64_t len, uint32_t flags) {
	int64_t start_len = len;
	/* Start by copying the part in the middle of the page */
	vaddr_t nearest_page = PAGE_ALIGN_4K(dest_vaddr + PAGE_SIZE - 1);
	vaddr_t alignbytes = nearest_page - dest_vaddr;
	int copy_len = alignbytes;
	if (alignbytes > len) {
		copy_len = len;
	}

	if (copy_sos2vspace_withinpage(src, dest_vaddr, vspace, alignbytes, flags) <
		0) {
		return -1;
	}
	len -= copy_len;
	src += copy_len;
	dest_vaddr += copy_len;

	while (len > 0) {
		copy_len = len > PAGE_SIZE_4K ? PAGE_SIZE_4K : len;
		if (copy_sos2vspace_withinpage(src, dest_vaddr, vspace, copy_len,
									   flags) < 0) {
			return -1;
		}
		len -= copy_len;
		src += copy_len;
		dest_vaddr += copy_len;
	}
	return start_len;
}