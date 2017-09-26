#include <copy.h>
#include <cpio/cpio.h>
#include <devices/devices.h>
#include <elf/elf.h>
#include <mapping.h>
#include <process.h>
#include <sel4/sel4.h>
#include <string.h>
#include <vm.h>
#include "cpio.h"
#include "elf.h"
#include "globals.h"
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
#define MAX_PROCESSES (256)

/* PID 0 is reserved, not used for anything */
struct process* process_table[MAX_PROCESSES];

/* Returns 0 if no PIDs free */
static uint32_t get_new_pid(void) {
	static uint32_t curpid = 1;
	for (uint32_t i = curpid + 1; i < MAX_PROCESSES; ++i) {
		if (process_table[i] == NULL) {
			curpid = i;
			return i;
		}
	}
	for (uint32_t i = 1; i < curpid; ++i) {
		if (process_table[i] == NULL) {
			curpid = i;
			return i;
		}
	}
	return 0;
}

struct process* get_process(int32_t pid) {
	if (pid > MAX_PROCESSES) {
		return NULL;
	}
	return process_table[pid];
}

struct process* process_create(char* app_name) {
	int err;

	dprintf(0, "*** CREATING PROCESS (%s) ***\n", app_name);
	// seL4_Word stack_addr;
	// seL4_CPtr stack_cap;
	seL4_CPtr user_ep_cap;

	/* These required for setting up the TCB */
	seL4_UserContext context;

	/* These required for loading program sections */
	char* elf_base;
	unsigned long elf_size;

	struct process* process = malloc(sizeof(struct process));
	if (process == NULL) {
		goto err0;
	}

	memset(process, 0, sizeof(struct process));

	process->pid = get_new_pid();
	if (process->pid == 0) {
		goto err1;
	}
	process_table[process->pid] = process;

	process->name = strdup(app_name);

	/* Create a VSpace */
	process->vroot_addr = ut_alloc(seL4_PageDirBits);
	if (process->vroot_addr == 0) {
		goto err1_5;
	}

	seL4_CPtr pd_cap;

	err =
		cspace_ut_retype_addr(process->vroot_addr, seL4_ARM_PageDirectoryObject,
							  seL4_PageDirBits, cur_cspace, &pd_cap);
	if (err) {
		ut_free(process->vroot_addr, seL4_PageBits);
		goto err2;
	}

	process->vspace.pagetable = pd_create(pd_cap);
	if (process->vspace.pagetable == NULL) {
		goto err3;
	}

	err = init_region_list(&process->vspace.regions);
	if (err != REGION_GOOD) {
		goto err4;
	}
	dprintf(0, "*** Regions allocated: %p\n", process->vspace.regions);

	/* Create a simple 1 level CSpace */
	process->croot = cspace_create(1);
	if (process->croot == NULL) {
		goto err5;
	}

	/* Create an IPC buffer */
	process->ipc_buffer_addr = ut_alloc(seL4_PageBits);
	if (process->ipc_buffer_addr == 0) {
		goto err6;
	}

	err = cspace_ut_retype_addr(process->ipc_buffer_addr,
								seL4_ARM_SmallPageObject, seL4_PageBits,
								cur_cspace, &process->ipc_buffer_cap);
	if (err) {
		ut_free(process->ipc_buffer_addr, seL4_PageBits);
		goto err7;
	}

	/* Copy the fault endpoint to the user app to enable IPC */
	user_ep_cap =
		cspace_mint_cap(process->croot, cur_cspace, sos_syscall_cap,
						seL4_AllRights, seL4_CapData_Badge_new(process->pid));
	/* should be the first slot in the space, hack I know */
	assert(user_ep_cap == 1);
	assert(user_ep_cap == USER_EP_CAP);

	dprintf(0, "*** IPC ALLOCATED ***\n");

	/* Create a new TCB object */
	process->tcb_addr = ut_alloc(seL4_TCBBits);
	if (process->tcb_addr == 0) {
		goto err8;
	}
	err = cspace_ut_retype_addr(process->tcb_addr, seL4_TCBObject, seL4_TCBBits,
								cur_cspace, &process->tcb_cap);
	if (err) {
		ut_free(process->tcb_addr, seL4_TCBBits);
		goto err9;
	}

	dprintf(0, "*** TCB ALLOCATED ***\n");
/* Provide a logical name for the thread -- Helpful for debugging */
#ifdef SEL4_DEBUG_KERNEL
	seL4_DebugNameThread(process->tcb_cap, app_name);
#endif

	/* parse the cpio image */
	dprintf(1, "\nStarting \"%s\"...\n", app_name);
	elf_base = cpio_get_file(_cpio_archive, app_name, &elf_size);
	if (elf_base == NULL) {
		goto err10;
	}

	/* load the elf image */

	dprintf(0, "*** Loading ELF ***\n");
	err = elf_load(process, elf_base);
	if (err) {
		goto err11;
	}
	dprintf(0, "*** ELF Loaded ***\n");

	/* Stack should be automagically mapped in as the process requires it */

	/* Map in the IPC buffer for the thread */

	region_node* ipc_region = process->vspace.regions->ipc_buffer;
	if (ipc_region == NULL) {
		goto err12;
	}

	dprintf(0, "Mapping IPC buffer at vaddr 0x%x\n", ipc_region->vaddr);
	struct page_table_entry* ipc_pte =
		sos_map_page(process->vspace.pagetable, ipc_region->vaddr,
					 PAGE_WRITABLE | PAGE_READABLE | PAGE_PINNED);

	if (ipc_pte == NULL) {
		goto err13;
	}
	/* Configure the TCB */
	err = seL4_TCB_Configure(process->tcb_cap, user_ep_cap, 0,
							 process->croot->root_cnode, seL4_NilData,
							 process->vspace.pagetable->seL4_pd, seL4_NilData,
							 ipc_region->vaddr, ipc_pte->cap);
	if (err) {
		goto err14;
	}

	region_node* stack_region = process->vspace.regions->stack;

	if (stack_region == NULL) {
		goto err15;
	}

	/* Start the new process */
	memset(&context, 0, sizeof(context));
	context.pc = elf_getEntryPoint(elf_base);
	/* The stack pointer must be aligned for data access. ARM uses 8 byte stack
	 * alignment
	 */
	context.sp = ALIGN_DOWN(stack_region->vaddr + stack_region->size - 1, 8);

	dprintf(0, "ELF ENTRY POINT IS %x\n", context.pc);
	dprintf(0, "*** PROCESS STARTING (PID: %u) ***\n", process->pid);
	seL4_TCB_WriteRegisters(process->tcb_cap, 1, 0, 2, &context);

	return process;

/* Woo, error handling. Do everything above, but freeing in reverse */
err15:
err14:
err13:
err12:
err11:
err10:
	cspace_delete_cap(cur_cspace, process->tcb_cap);
err9:
err8:
	cspace_delete_cap(cur_cspace, process->ipc_buffer_cap);
	cspace_delete_cap(cur_cspace, user_ep_cap);
err7:
err6:
	cspace_destroy(process->croot);
err5:
	region_list_destroy(process->vspace.regions);
err4:
	pd_free(process->vspace.pagetable);
err3:
	cspace_delete_cap(cur_cspace, pd_cap);
err2:
err1_5:
	free(process->name);
	process_table[process->pid] = NULL;
err1:
	free(process);
err0:
	return NULL;
}

void process_coredump(struct process* process) {
	printf("*************\n DUMPING CORE ************\n");
	struct fd coredump;
	nfs_dev_open(&coredump, "coredump.bin", FM_WRITE);

	region_node* r = process->vspace.regions->start;
	char* buffer = malloc(PAGE_SIZE_4K);
	if (buffer == NULL) {
		printf("Error dumping core\n");
		return;
	}
	while (r != NULL) {
		printf("Dumping region: %s\n", r->name);
		for (uint32_t i = r->vaddr; i < r->vaddr + r->size; i += PAGE_SIZE_4K) {
			coredump.offset = i;
			coredump.dev_write(&coredump, &process->vspace, r->vaddr, r->size);
		}
		r = r->next;
	}
	printf("Process dumped\n");
}
void process_kill(struct process* process) {
	regions_print(process->vspace.regions);
	process_coredump(process);
	panic("Killing processes not implemented yet");
}
