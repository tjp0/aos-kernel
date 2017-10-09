#include <clock/clock.h>
#include <copy.h>
#include <cpio/cpio.h>
#include <devices/devices.h>
#include <elf/elf.h>
#include <mapping.h>
#include <process.h>
#include <sel4/sel4.h>
#include <string.h>
#include <utils/picoro.h>
#include <vm.h>
#include "cpio.h"
#include "elf.h"
#include "globals.h"
#include "sos_coroutine.h"
#include "ut_manager/ut.h"
#include "vmem_layout.h"
#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
#include <sys/panic.h>

#define ELF_HEADER_SIZE 4096
struct semaphore* any_pid_exit_signal;

void process_zombie_reap(struct process* process);
/* This is the index where a clients syscall enpoint will
 * be stored in the clients cspace. */
#define USER_EP_CAP (1)

/* PID 0 is reserved, not used for anything */
struct process* process_table[MAX_PROCESSES];

/* return a pid if free or if it represents a zombie process
 * Returns 0 if no PIDs free */
static uint32_t get_new_pid(void) {
	static uint32_t curpid = 1;
	for (uint32_t i = curpid + 1; i < MAX_PROCESSES; ++i) {
		if (process_table[i] == NULL ||
			process_table[i]->status == PROCESS_ZOMBIE) {
			curpid = i;
			return i;
		}
	}

	for (uint32_t i = 1; i < curpid; ++i) {
		if (process_table[i] == NULL ||
			process_table[i]->status == PROCESS_ZOMBIE) {
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
	trace(5);
	/* These required for setting up the TCB */
	seL4_UserContext context;

	struct process* process = malloc(sizeof(struct process));
	if (process == NULL) {
		goto err0;
	}
	trace(5);
	memset(process, 0, sizeof(struct process));

	process->pid = get_new_pid();
	if (process->pid == 0) {
		goto err1;
	}
	trace(5);
	struct process* existing_process;
	if ((existing_process = process_table[process->pid]) != NULL) {
		trace(5);
		kassert(existing_process->status == PROCESS_ZOMBIE);
		process_zombie_reap(existing_process);
	}
	trace(5);
	process->name = strdup(app_name);

	/* Create semaphores for if we are in a syscall*/
	process->event_finished_syscall = semaphore_create();
	if (process->event_finished_syscall == NULL) {
		goto err1_3;
	}
	trace(5);

	process->event_exited = semaphore_create();
	if (process->event_exited == NULL) {
		goto err1_4;
	}

	/* Create a VSpace */
	process->vroot_addr = ut_alloc(seL4_PageDirBits);
	if (process->vroot_addr == 0) {
		goto err1_5;
	}
	trace(5);
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
	trace(5);
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
	trace(5);

	/* Copy the fault endpoint to the user app to enable IPC */
	user_ep_cap =
		cspace_mint_cap(process->croot, cur_cspace, sos_syscall_cap,
						seL4_AllRights, seL4_CapData_Badge_new(process->pid));

	if (user_ep_cap != USER_EP_CAP) {
		goto err7_5;
	}

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
	trace(5);

	dprintf(0, "*** TCB ALLOCATED ***\n");
/* Provide a logical name for the thread -- Helpful for debugging */
#ifdef SEL4_DEBUG_KERNEL
	seL4_DebugNameThread(process->tcb_cap, app_name);
#endif

	dprintf(0, "*** Loading ELF ***\n");
	err = elf_load(process, app_name);
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
	trace(5);
	dprintf(0, "Mapping IPC buffer at vaddr 0x%x\n", ipc_region->vaddr);
	struct page_table_entry* ipc_pte =
		sos_map_page(process->vspace.pagetable, ipc_region->vaddr,
					 PAGE_WRITABLE | PAGE_READABLE | PAGE_PINNED);

	if (ipc_pte == NULL) {
		goto err13;
	}
	unlock(ipc_pte->lock);
	/* Configure the TCB */
	err = seL4_TCB_Configure(process->tcb_cap, user_ep_cap, 0,
							 process->croot->root_cnode, seL4_NilData,
							 process->vspace.pagetable->seL4_pd, seL4_NilData,
							 ipc_region->vaddr, ipc_pte->cap);
	if (err) {
		goto err14;
	}
	trace(5);
	region_node* stack_region = process->vspace.regions->stack;

	trace(5);
	if (stack_region == NULL) {
		goto err15;
	}

	trace(5);
	/* Register the process in the process table */
	process->status = PROCESS_ALIVE;
	process_table[process->pid] = process;
	/* Start the new process */
	memset(&context, 0, sizeof(context));
	context.pc = process->elfentry;
	/* The stack pointer must be aligned for data access. ARM uses 8 byte stack
	 * alignment
	 */
	context.sp = ALIGN_DOWN(stack_region->vaddr + stack_region->size - 1, 8);

	dprintf(0, "ELF ENTRY POINT IS %x\n", context.pc);
	dprintf(0, "*** PROCESS STARTING (PID: %u) ***\n", process->pid);
	process->start_time = time_stamp();
	seL4_TCB_WriteRegisters(process->tcb_cap, 1, 0, 2, &context);
	trace(5);
	return process;

/* Woo, error handling. Do everything above, but freeing in reverse */
err15:
err14:
err13:
err12:
err11:
	trace(5);
	cspace_delete_cap(cur_cspace, process->tcb_cap);
err9:
err8:
	trace(5);
	cspace_delete_cap(process->croot, user_ep_cap);
err7_5:
	trace(5);
	trace(5);
	cspace_destroy(process->croot);
err5:
	trace(5);
	region_list_destroy(process->vspace.regions);
err4:
	trace(5);
	pd_free(process->vspace.pagetable);
err3:
	trace(5);
	cspace_delete_cap(cur_cspace, pd_cap);
err2:
err1_5:
	trace(5);
	semaphore_destroy(process->event_exited);
err1_4:
	trace(5);
	semaphore_destroy(process->event_finished_syscall);
err1_3:
	trace(5);
	free(process->name);
	process_table[process->pid] = NULL;
err1:
	trace(5);
	free(process);
err0:
	trace(5);
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
void process_kill(struct process* process, uint32_t status) {
	kassert(process != NULL);
	trace(1);
	if (process->status == PROCESS_ZOMBIE) {
		return;
	}

	/* wait until a syscall is finished (if needed), unless it is called
	 * exit */
	if (process->current_coroutine != NULL &&
		process->current_coroutine != current_coro()) {
		wait(process->event_finished_syscall);
	}

	/* While we waited, something else might have killed us */
	if (process->status == PROCESS_ZOMBIE) {
		return;
	}
	process->status = PROCESS_ZOMBIE;
	trace(1);
	cspace_delete_cap(cur_cspace, process->tcb_cap);
	trace(1);
	pd_free(process->vspace.pagetable);
	trace(1);
	fd_table_close(&process->fds);
	trace(1);
	cspace_delete_cap(process->croot,
					  USER_EP_CAP); /* The copied syscall IPC cap */
	trace(1);
	cspace_destroy(process->croot);
	trace(1);
	region_list_destroy(process->vspace.regions);
	trace(1);

	process->tcb_cap = 0;
	process->vspace.pagetable = NULL;
	process->croot = 0;
	process->vspace.regions = NULL;
	trace(1);
	// Signal that *this* process has ended
	signal(process->event_exited, (void*)process->pid);
	trace(1);
	// Signal that *a* process has ended
	signal(any_pid_exit_signal, (void*)process->pid);
	trace(1);
}
void process_zombie_reap(struct process* process) {
	kassert(process != NULL);
	kassert(process->status == PROCESS_ZOMBIE);
	semaphore_destroy(process->event_finished_syscall);
	semaphore_destroy(process->event_exited);
	process_table[process->pid] = NULL;
	free(process->name);
	free(process);
}
