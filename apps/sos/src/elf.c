/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "elf.h"
#include <assert.h>
#include <cspace/cspace.h>
#include <elf/elf.h>
#include <fcntl.h>
#include <sel4/sel4.h>
#include <string.h>
#include "region_manager.h"

#include <copy.h>
#include <devices/devices.h>
#include <mapping.h>
#include <process.h>
#include <ut_manager/ut.h>
#include <vm.h>
#include <vmem_layout.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
#include <sys/panic.h>

#define ELF_HEADER_SIZE 4096
/*
 * Convert ELF permissions into seL4 permissions.
 */
static inline seL4_Word get_vm_perms_from_elf(unsigned long permissions) {
	seL4_Word result = 0;

	if (permissions & PF_R) result |= PAGE_READABLE;
	if (permissions & PF_X) result |= PAGE_EXECUTABLE;
	if (permissions & PF_W) result |= PAGE_WRITABLE;

	return result;
}

struct nfs_elf_reference {
	char *filename;

	uint32_t memory_seg_start;
	uint32_t file_seg_start;
	uint32_t size_of_segment_file;
	uint32_t size_of_segment_memory;
};

static uint32_t min(int32_t a, uint32_t b) { return (a < b) ? a : b; }

static int load_file_from_nfs(region_node *reg, struct vspace *vspace,
							  vaddr_t vaddr) {
	dprintf(3, "Loading %08x from ELF file over NFS\n", vaddr);
	struct fd file;
	struct nfs_elf_reference *ref = ((struct nfs_elf_reference *)reg->data);

	uint32_t region_start = reg->vaddr;
	uint32_t memory_start = ref->memory_seg_start;
	uint32_t local_offset = memory_start - region_start;
	uint32_t first_offset = 0;
	uint32_t file_offset = 0;
	if (vaddr == reg->vaddr) {
		first_offset = local_offset;
		file_offset = ref->file_seg_start;
	} else {
		file_offset =
			(vaddr - region_start) + ref->file_seg_start - local_offset;
	}

	uint32_t max_space = PAGE_SIZE_4K - first_offset;
	uint32_t file_size_left = 0;

	dprintf(3, "segfile: %08x, curof: %08x, sstart: %08x, sizein: %08x\n",
			ref->size_of_segment_file, file_offset, ref->file_seg_start,
			file_offset - ref->file_seg_start);
	if (ref->size_of_segment_file >= (file_offset - ref->file_seg_start)) {
		file_size_left =
			(ref->size_of_segment_file - (file_offset - ref->file_seg_start));
	} else {
		dprintf(3, "%08x is a NULL page\n", vaddr);
		return 0;
	}

	dprintf(3,
			"File seg start: %08x, File seg vaddr start: %08x, File offset: "
			"%08x, Size of segment_file: %08x, file_size_left %08x, local "
			"offset: %08x\n",
			ref->file_seg_start, ref->memory_seg_start, file_offset,
			ref->size_of_segment_file, file_size_left, local_offset);
	dprintf(3, "Region start: %08x\n", region_start);

	uint32_t amount_to_write = min(max_space, file_size_left);
	dprintf(3,
			"Loading from file (<%s>:%08x) to page: %08x, interior: %08x, of "
			"size: %08x\n",
			ref->filename, file_offset, vaddr, vaddr + first_offset,
			amount_to_write);
	if (amount_to_write != 0) {
		nfs_dev_open(&file, ref->filename, O_RDONLY);
		file.offset = file_offset;
		if (file.dev_read(&file, vspace, vaddr + first_offset,
						  amount_to_write) != amount_to_write) {
			file.dev_close(&file);
			return -1;
		}
		file.dev_close(&file);
	}
	return 0;
}
static void clean_file_things(region_node *reg) { free(reg->data); }

static int load_segment_into_vspace(struct vspace *vspace, char *src,
									unsigned long segment_size,
									unsigned long file_size, unsigned long dst,
									unsigned long permissions,
									char *elf_file_name,
									unsigned long segment_start_offset) {
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
	*/

	// use page align up
	unsigned long dst_region = PAGE_ALIGN_4K(dst);
	unsigned long region_size =
		PAGE_ALIGN_UP_4K(segment_size + dst_region) - dst_region;
	dprintf(0,
			"Creating region at %p to %p, from file offset: %x to %x (file "
			"size is %x)\n",
			(void *)dst_region, (void *)dst_region + region_size,
			(uint32_t)segment_start_offset,
			(uint32_t)segment_start_offset + file_size, (uint32_t)file_size);

	void *alloc_func = NULL;
	void *clean_func = NULL;
	struct nfs_elf_reference *data_pointer = NULL;

	if (file_size > 0) {
		data_pointer = malloc(sizeof(struct nfs_elf_reference));
		if (!data_pointer) {
			return -1;
		}
		data_pointer->memory_seg_start = dst;
		data_pointer->file_seg_start = segment_start_offset;
		data_pointer->size_of_segment_file = file_size;
		data_pointer->size_of_segment_memory = region_size;
		data_pointer->filename = elf_file_name;
		alloc_func = load_file_from_nfs;
		clean_func = clean_file_things;
	}
	region_node *node =
		add_region(vspace->regions, dst_region, region_size, permissions,
				   alloc_func, clean_func, data_pointer);

	dprintf(0, "Region at %p to %p created\n", (void *)dst_region,
			(void *)dst_region + region_size);

	if (node == NULL || file_size > region_size) {
		return -1;
	}
	return 0;
}

int elf_load(struct process *process, char *elf_path) {
	trace(5);
	char *elf_file = malloc(ELF_HEADER_SIZE);
	if (elf_file == NULL) {
		trace(5);
		return -1;
	}
	trace(5);
	struct fd fd = {};
	dprintf(2, "Opening ELF from %s\n", elf_path);
	if (nfs_dev_open(&fd, elf_path, O_RDONLY) < 0) {
		free(elf_file);
		trace(5);
		return -1;
	}
	trace(5);
	if (fd.dev_read(&fd, NULL, (vaddr_t)elf_file, ELF_HEADER_SIZE) !=
		ELF_HEADER_SIZE) {
		free(elf_file);
		fd.dev_close(&fd);
		trace(5);
		return -1;
	}
	fd.dev_close(&fd);
	dprintf(2, "Elf headers read\n");
	trace(5);
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
		source_addr =
			elf_file + elf_getProgramHeaderOffset(elf_file, i);  // not needed?
		file_size = elf_getProgramHeaderFileSize(elf_file, i);
		segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
		vaddr = elf_getProgramHeaderVaddr(elf_file, i);
		flags = elf_getProgramHeaderFlags(elf_file, i);

		/* Copy it across into the vspace. */
		dprintf(0, " * Loading segment %08x-->%08x\n", (int)vaddr,
				(int)(vaddr + segment_size));
		err = load_segment_into_vspace(
			&process->vspace, source_addr, segment_size, file_size, vaddr,
			get_vm_perms_from_elf(flags), process->name,
			elf_getProgramHeaderOffset(elf_file, i));
		conditional_panic(err != 0, "Elf loading failed!\n");
	}
	process->elfentry = elf_getEntryPoint(elf_file);
	return 0;
}
