/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <cspace/cspace.h>
#include <elf/elf.h>
#include <sel4/sel4.h>
#include <string.h>

#include "elf.h"
#include "region_manager.h"

#include <copy.h>
#include <mapping.h>
#include <process.h>
#include <ut_manager/ut.h>
#include <vmem_layout.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

/*
 * Convert ELF permissions into seL4 permissions.
 */
static inline seL4_Word get_sel4_rights_from_elf(unsigned long permissions) {
	seL4_Word result = 0;

	if (permissions & PF_R) result |= seL4_CanRead;
	if (permissions & PF_X) result |= seL4_CanRead;
	if (permissions & PF_W) result |= seL4_CanWrite;

	return result;
}

/*
 * Inject data into the given vspace.
 * TODO: Don't keep these pages mapped in
 */
static int load_segment_into_vspace(struct vspace *vspace, char *src,
									unsigned long segment_size,
									unsigned long file_size, unsigned long dst,
									unsigned long permissions) {
	/* Overview of ELF segment loading

	   dst: destination base virtual address of the segment being loaded
	   segment_size: obvious

	   So the segment range to "load" is [dst, dst + segment_size).

	   The content to load is either zeros or the content of the ELF
	   file itself, or both.

	   The split between file content and zeros is a follows.

	   File content: [dst, dst + file_size)
	   Zeros:        [dst + file_size, dst + segment_size)

	   Note: if file_size == segment_size, there is no zero-filled region.
	   Note: if file_size == 0, the whole segment is just zero filled.

	   The code below relies on seL4's frame allocator already
	   zero-filling a newly allocated frame.

	*/

	unsigned long dst_region = PAGE_ALIGN_4K(dst);
	segment_size = PAGE_ALIGN_4K(segment_size + PAGE_SIZE_4K);
	dprintf(0, "Creating region at %p to %p\n", (void *)dst_region,
			(void *)dst_region + segment_size);
	region_node *node =
		add_region(vspace->regions, dst_region, segment_size, permissions);
	dprintf(0, "Region at %p to %p created\n", (void *)dst_region,
			(void *)dst_region + segment_size);

	assert(node != NULL);
	assert(file_size <= segment_size);

	if (copy_sos2vspace(src, dst, vspace, file_size, 1) < 0) {
		return -1;
	}
	dprintf(0, "Initial data copied to region at %p\n", (void *)dst);
	return 0;
}

int elf_load(struct process *process, char *elf_file) {
	int num_headers;
	int err = 0;
	int i;
	/* Ensure that the ELF file looks sane. */
	if (elf_checkFile(elf_file)) {
		return seL4_InvalidArgument;
	}

	num_headers = elf_getNumProgramHeaders(elf_file);
	for (i = 0; i < num_headers; i++) {
		char *source_addr;
		unsigned long flags, file_size, segment_size, vaddr;
		/* Skip non-loadable segments (such as debugging data). */
		if (elf_getProgramHeaderType(elf_file, i) != PT_LOAD) continue;

		dprintf(0, " * Processing segment %d of %d\n", i + 1, num_headers);
		/* Fetch information about this segment. */
		source_addr = elf_file + elf_getProgramHeaderOffset(elf_file, i);
		file_size = elf_getProgramHeaderFileSize(elf_file, i);
		segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
		vaddr = elf_getProgramHeaderVaddr(elf_file, i);
		flags = elf_getProgramHeaderFlags(elf_file, i);

		/* Copy it across into the vspace. */
		dprintf(0, " * Loading segment %08x-->%08x\n", (int)vaddr,
				(int)(vaddr + segment_size));
		err = load_segment_into_vspace(
			&process->vspace, source_addr, segment_size, file_size, vaddr,
			get_sel4_rights_from_elf(flags) & seL4_AllRights);
		conditional_panic(err != 0, "Elf loading failed!\n");
	}

	return 0;
}
