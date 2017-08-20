#pragma once
#include <cspace/cspace.h>
#include <sel4/sel4.h>
#include <vm.h>

struct process {
	seL4_Word tcb_addr;
	seL4_TCB tcb_cap;
	seL4_Word vroot_addr;
	seL4_Word ipc_buffer_addr;
	seL4_CPtr ipc_buffer_cap;
	cspace_t* croot;

	struct page_directory* pagetable;
};