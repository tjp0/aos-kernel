#include <cpio/cpio.h>
#include <elf/elf.h>
#include <mapping.h>
#include <process.h>
#include <sel4/sel4.h>
#include <string.h>
#include <vm.h>
#include "cpio.h"
#include "elf.h"
#include "ut_manager/ut.h"
#include "vmem_layout.h"

#define verbose 2
#include <sys/debug.h>
#include <sys/panic.h>

extern char _cpio_archive[];

/* This is the index where a clients syscall enpoint will
 * be stored in the clients cspace. */
#define USER_EP_CAP (1)

// Fix these up later to support multiple processes
#define TTY_PRIORITY (0)
#define TTY_EP_BADGE (101)

struct process* process_create(char* app_name, seL4_CPtr fault_ep) {
	int err;

	dprintf(0, "*** CREATING PROCESS (%s) ***\n", app_name);
	//seL4_Word stack_addr;
	//seL4_CPtr stack_cap;
	seL4_CPtr user_ep_cap;

	/* These required for setting up the TCB */
	seL4_UserContext context;

	/* These required for loading program sections */
	char* elf_base;
	unsigned long elf_size;

	struct process* process = malloc(sizeof(struct process));
	if (process == NULL) {
		return NULL;
	}

	/* Create a VSpace */
	process->vroot_addr = ut_alloc(seL4_PageDirBits);
	conditional_panic(!process->vroot_addr, "No memory for new Page Directory");

	seL4_CPtr pd_cap;

	err =
		cspace_ut_retype_addr(process->vroot_addr, seL4_ARM_PageDirectoryObject,
							  seL4_PageDirBits, cur_cspace, &pd_cap);
	conditional_panic(err, "Failed to allocate page directory cap for client");

	process->vspace.pagetable = pd_create(pd_cap);
	init_region_list(&process->vspace.regions);
	dprintf(0, "*** Regions allocated\n");

	/* Create a simple 1 level CSpace */
	process->croot = cspace_create(1);
	assert(process->croot != NULL);

	/* Create an IPC buffer */
	process->ipc_buffer_addr = ut_alloc(seL4_PageBits);
	conditional_panic(!process->ipc_buffer_addr, "No memory for ipc buffer");
	err = cspace_ut_retype_addr(process->ipc_buffer_addr,
								seL4_ARM_SmallPageObject, seL4_PageBits,
								cur_cspace, &process->ipc_buffer_cap);
	conditional_panic(err, "Unable to allocate page for IPC buffer");

	/* Copy the fault endpoint to the user app to enable IPC */
	user_ep_cap =
		cspace_mint_cap(process->croot, cur_cspace, fault_ep, seL4_AllRights,
						seL4_CapData_Badge_new(TTY_EP_BADGE));
	/* should be the first slot in the space, hack I know */
	assert(user_ep_cap == 1);
	assert(user_ep_cap == USER_EP_CAP);

	dprintf(0, "*** IPC ALLOCATED ***\n");

	/* Create a new TCB object */
	process->tcb_addr = ut_alloc(seL4_TCBBits);
	conditional_panic(!process->tcb_addr, "No memory for new TCB");
	err = cspace_ut_retype_addr(process->tcb_addr, seL4_TCBObject, seL4_TCBBits,
								cur_cspace, &process->tcb_cap);
	conditional_panic(err, "Failed to create TCB");

	dprintf(0, "*** TCB ALLOCATED ***\n");
/* Provide a logical name for the thread -- Helpful for debugging */
#ifdef SEL4_DEBUG_KERNEL
	seL4_DebugNameThread(process->tcb_cap, app_name);
#endif

	/* parse the cpio image */
	dprintf(1, "\nStarting \"%s\"...\n", app_name);
	elf_base = cpio_get_file(_cpio_archive, app_name, &elf_size);
	conditional_panic(!elf_base, "Unable to locate cpio header");

	/* load the elf image */

	dprintf(0, "*** Loading ELF ***\n");
	err = elf_load(process, elf_base);
	conditional_panic(err, "Failed to load elf image");
	dprintf(0, "*** ELF Loaded ***\n");


/* Stack should be automagically mapped in as the process requires it.
	stack_addr = ut_alloc(seL4_PageBits);
	conditional_panic(!stack_addr, "No memory for stack");
	err = cspace_ut_retype_addr(stack_addr, seL4_ARM_SmallPageObject,
								seL4_PageBits, cur_cspace, &stack_cap);
	conditional_panic(err, "Unable to allocate page for stack");

	err = map_page(stack_cap, process->vspace.pagetable->seL4_pd,
				   process->vspace->regions->stack, seL4_AllRights,
				   seL4_ARM_Default_VMAttributes);
	conditional_panic(err, "Unable to map stack IPC buffer for user app");
*/

	/* Map in the IPC buffer for the thread */

	region_node* ipc_region = process->vspace.regions->ipc_buffer;
	conditional_panic(ipc_region == NULL, "No IPC region defined. WTF");

	dprintf(0, "Mapping IPC buffer at vaddr 0x%x\n",ipc_region->vaddr);
	struct page_table_entry* ipc_pte = sos_map_page(process->vspace.pagetable,ipc_region->vaddr, seL4_CanWrite|seL4_CanRead);
	conditional_panic(ipc_pte == NULL, "Failed to map IPC buffer");

		/* Configure the TCB */
	err = seL4_TCB_Configure(process->tcb_cap, user_ep_cap, TTY_PRIORITY,
							 process->croot->root_cnode, seL4_NilData,
							 process->vspace.pagetable->seL4_pd, seL4_NilData,
							 ipc_region->vaddr, ipc_pte->cap);
	conditional_panic(err, "Unable to configure new TCB");


	region_node* stack_region = process->vspace.regions->stack;
	/* Start the new process */
	memset(&context, 0, sizeof(context));
	context.pc = elf_getEntryPoint(elf_base);
	context.sp = stack_region->vaddr + stack_region->size - 1;

	dprintf(0, "ELF ENTRY POINT IS %x\n", context.pc);
	dprintf(0, "*** PROCESS STARTING ***\n");
	seL4_TCB_WriteRegisters(process->tcb_cap, 1, 0, 2, &context);

	return process;
}