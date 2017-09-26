#pragma once
#include <cspace/cspace.h>
#include <filetable.h>
#include <region_manager.h>
#include <sel4/sel4.h>
struct vspace {
	struct page_directory* pagetable;
	region_list* regions;

	vaddr_t sbrk;
};

struct process {
	seL4_Word tcb_addr;
	seL4_TCB tcb_cap;
	seL4_Word vroot_addr;
	seL4_Word ipc_buffer_addr;
	seL4_CPtr ipc_buffer_cap;
	cspace_t* croot;
	struct vspace vspace;

	struct fd_table fds;
	uint32_t pid;
	char* name;
};
struct process* get_process(int32_t pid);
struct process* process_create(char* app_name);
void process_kill(struct process* process);
