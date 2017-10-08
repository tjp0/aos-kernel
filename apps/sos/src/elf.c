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
#include <devices/devices.h>
#include <mapping.h>
#include <process.h>
#include <ut_manager/ut.h>
#include <vm.h>
#include <vmem_layout.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/panic.h>

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

// TODO: move this to a .h file or somehwere better
typedef struct _nfs_file_reference {
	char *filename;

	uint64_t memory_seg_start;
	uint64_t file_seg_start;
	uint64_t size_of_segment_file;
	uint64_t size_of_segment_memory;

} nfs_file_reference;

// TODO: implement these bad boyz
void load_file_from_nfs(region_node *reg, struct vspace *vspace, vaddr_t vaddr);
void clean_file_things(region_node *reg);

static uint64_t max(uint64_t a, uint64_t b) { return a ? (a > b) : b; }
// static uint64_t min(uint64_t a, uint64_t b) { return a ? (a < b) : b; }
static uint64_t min3(uint64_t a, uint64_t b, uint64_t c) {
	return a ? (a <= b && a <= c) : b ? (b <= c) : c;
}

void load_file_from_nfs(region_node *reg, struct vspace *vspace,
						vaddr_t vaddr) {
	struct fd file;
	nfs_file_reference *ref = ((nfs_file_reference *)reg->data);
	nfs_dev_open(&file, ref->filename, FM_READ);

	uint64_t read_start = max(ref->memory_seg_start, PAGE_ALIGN_4K(vaddr));

	if (read_start >= ref->memory_seg_start &&
		read_start <= ref->memory_seg_start + ref->size_of_segment_file) {
		// I think there are some edge cases not accounted for here
		uint64_t size_to_read =
			min3(PAGE_SIZE_4K,
				 ref->memory_seg_start + ref->size_of_segment_file - read_start,
				 PAGE_ALIGN_UP_4K(read_start + 1) - ref->memory_seg_start);

		file.offset =
			ref->file_seg_start + (read_start - ref->memory_seg_start);

		file.dev_read(&file, vspace, read_start, size_to_read);
	} else {
		// fill with 0's
	}
}

void *setup_data_pointer(uint64_t memory_seg_start, uint64_t file_seg_start,
						 uint64_t size_of_segment_file,
						 uint64_t size_of_segment_memory, char *elf_file_name) {
	nfs_file_reference *elf_info = malloc(sizeof(struct _nfs_file_reference));
	if (!elf_info) {
		return NULL;
	}
	elf_info->memory_seg_start = memory_seg_start;
	elf_info->file_seg_start = file_seg_start;
	elf_info->size_of_segment_file = size_of_segment_file;
	elf_info->size_of_segment_memory = size_of_segment_memory;

	elf_info->filename = elf_file_name;
	return elf_info;
}

void clean_file_things(region_node *reg) { free(reg->data); }
/*
 * Inject data into the given vspace.
 * TODO: Don't keep these pages mapped in
 */
static int load_segment_into_vspace(struct vspace *vspace, char *src,
									unsigned long segment_size,
									unsigned long file_size, unsigned long dst,
									unsigned long permissions,
									char *elf_file_name,
									uint64_t segment_start_offset) {
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
	// use page align up
	unsigned long dst_region = PAGE_ALIGN_4K(dst);
	unsigned long region_size = PAGE_ALIGN_UP_4K(segment_size + dst_region);
	dprintf(0, "Creating region at %p to %p\n", (void *)dst_region,
			(void *)dst_region + region_size);

	//#pastie thoughts
	// do some setup and malloc things for data_pointer
	// the clean_elf_things funciton will free this
	// load_elf_from_nfs will use the data_pointer to get what it points to
	// which will be some offests and file names and it'll use this to
	// load the data over nfs into the region

	/* Start reading from segment_start_offset*/

	void *data_pointer = setup_data_pointer(
		dst, segment_start_offset, segment_size, file_size, elf_file_name);
	if (!data_pointer) {
		return -1;
	}

	region_node *node =
		add_region(vspace->regions, dst_region, region_size, permissions,
				   load_file_from_nfs, clean_file_things, data_pointer);

	dprintf(0, "Region at %p to %p created\n", (void *)dst_region,
			(void *)dst_region + region_size);

	// TODO
	// assert(node != NULL);
	// assert(file_size <= region_size);

	// 1. comment out elf.c crap
	if (copy_sos2vspace(src, dst, vspace, file_size, COPY_INSTRUCTIONFLUSH) <
		0) {
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

		uint64_t segment_start_offset = elf_getProgramHeaderOffset(elf_file, i);
		/* Fetch information about this segment. */
		source_addr = elf_file + segment_start_offset;  // not needed?
		file_size = elf_getProgramHeaderFileSize(elf_file, i);
		segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
		vaddr = elf_getProgramHeaderVaddr(elf_file, i);
		flags = elf_getProgramHeaderFlags(elf_file, i);

		/* Copy it across into the vspace. */
		dprintf(0, " * Loading segment %08x-->%08x\n", (int)vaddr,
				(int)(vaddr + segment_size));
		err = load_segment_into_vspace(
			&process->vspace, source_addr, segment_size, file_size, vaddr,
			get_vm_perms_from_elf(flags), process->name, segment_start_offset);
		conditional_panic(err != 0, "Elf loading failed!\n");
	}

	return 0;
}
