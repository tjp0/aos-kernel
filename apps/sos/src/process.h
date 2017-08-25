#pragma once
#include <cspace/cspace.h>
#include <region_manager.h>
#include <sel4/sel4.h>
struct vspace {
	struct page_directory* pagetable;
	region_list* regions;

	region_node* stack;
	region_node* heap;
};

struct process {
	seL4_Word tcb_addr;
	seL4_TCB tcb_cap;
	seL4_Word vroot_addr;
	seL4_Word ipc_buffer_addr;
	seL4_CPtr ipc_buffer_cap;
	cspace_t* croot;
	struct vspace vspace;
};

struct process* process_create(char* app_name, seL4_CPtr fault_ep);

void process_kill(struct process* process);