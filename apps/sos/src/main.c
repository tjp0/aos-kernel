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
#include <ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cspace/cspace.h>

#include <cpio/cpio.h>
#include <elf/elf.h>
#include <nfs/nfs.h>
#include <serial/serial.h>

#include <clock/clock.h>
#include <vm.h>
#include "elf.h"
#include "network.h"

#include "mapping.h"
#include "ut_manager/ut.h"
#include "vmem_layout.h"

#include <autoconf.h>
#include <copy.h>
#include <devices/devices.h>
#include <frametable.h>
#include <process.h>
#include <region_manager.h>
#include <syscall.h>
#include <syscall/syscall_io.h>
#include <syscall/syscall_memory.h>
#include <syscall/syscall_time.h>
#include <utils/arith.h>
#include <utils/endian.h>
#include <utils/picoro.h>
#include <utils/stack.h>
#include "test_timer.h"
#define verbose 3
#include <sys/debug.h>
#include <sys/panic.h>

/* This is the index where a clients syscall enpoint will
 * be stored in the clients cspace. */
#define USER_EP_CAP (1)
/* To differencient between async and and sync IPC, we assign a
 * badge to the async endpoint. The badge that we receive will
 * be the bitwise 'OR' of the async endpoint badge and the badges
 * of all pending notifications. */
#define IRQ_EP_BADGE (1 << (seL4_BadgeBits - 1))
/* All badged IRQs set high bet, then we use uniq bits to
 * distinguish interrupt sources */
#define IRQ_BADGE_NETWORK (1 << 0)
#define IRQ_BADGE_EPIT1 (1 << 1)
#define IRQ_BADGE_EPIT2 (1 << 2)

#define INIT_NAME CONFIG_SOS_STARTUP_APP
/* The linker will link this symbol to the start address  *
 * of an archive of attached applications.                */
extern char _cpio_archive[];

const seL4_BootInfo* _boot_info;

struct process* tty_test_process;
/*
 * A dummy starting syscall
 */

struct serial* global_serial;

#define SOS_SYSCALL0 0
#define SOS_SERIALWRITE 1

seL4_CPtr _sos_ipc_ep_cap;
seL4_CPtr _sos_interrupt_ep_cap;

static void _sos_ipc_init(seL4_CPtr* ipc_ep, seL4_CPtr* async_ep);
static void sos_main(void);
static void _sos_early_init(seL4_CPtr* ipc_ep, seL4_CPtr* async_ep);
static void* _sos_late_init(void* unusedarg);

/**
 * NFS mount point
 */
extern fhandle_t mnt_point;

void handle_syscall(seL4_Word badge, int num_args) {
	seL4_Word syscall_number;
	seL4_CPtr reply_cap;
	struct process* process = tty_test_process;

	struct ipc_command ipc = ipc_create();
	ipc_unpacki(&ipc, &syscall_number);

	unsigned int arg1;
	unsigned int arg2;
	unsigned int arg3;
	unsigned int arg4;
	unsigned int err;
	unsigned int ret1 = 0;
	unsigned int ret2 = 0;
	unsigned int ret3 = 0;
	unsigned int ret4 = 0;
	int64_t temp64 = 0;
	ipc_unpacki(&ipc, &arg1);
	ipc_unpacki(&ipc, &arg2);
	ipc_unpacki(&ipc, &arg3);
	ipc_unpacki(&ipc, &arg4);

	/* Save the caller */
	reply_cap = cspace_save_reply_cap(cur_cspace);
	assert(reply_cap != CSPACE_NULL);
	/* Process system call */
	dprintf(2, "SYSCALL: %d with args: (%08x %08x %08x %08x)\n", syscall_number,
			arg1, arg2, arg3, arg4);
	switch (syscall_number) {
		case SOS_SYSCALL_SERIALWRITE: {
			err = syscall_serialwrite(process, arg1, arg2, &ret1);
		} break;
		case SOS_SYSCALL_SBRK: {
			err = syscall_sbrk(process, arg1);
		} break;
		case SOS_SYSCALL_USLEEP: {
			err = syscall_usleep(process, arg1);
		} break;
		case SOS_SYSCALL_TIMESTAMP: {
			err = syscall_time_stamp(process, &temp64);
			split64to32(temp64, &ret1, &ret2);
		} break;
		case SOS_SYSCALL_OPEN: {
			err = syscall_open(process, arg1, arg2);
		} break;
		case SOS_SYSCALL_READ: {
			err = syscall_read(process, arg1, arg2, arg3);
		} break;
		case SOS_SYSCALL_WRITE: {
			err = syscall_write(process, arg1, arg2, arg3);
		} break;
		default: {
			printf("%s:%d (%s) Unknown syscall %d\n", __FILE__, __LINE__,
				   __func__, syscall_number);
			/* we don't want to reply to an unknown syscall, so short circuit
			 * here */
			cspace_free_slot(cur_cspace, reply_cap);
			return;
		}
	}
	dprintf(2,
			"SYSCALL RET: %d with values: (err: %08x, args %08x %08x %08x)\n",
			syscall_number, err, ret1, ret2, ret3, ret4);

	/* Respond to the syscall */
	ipc = ipc_create();
	ipc_packi(&ipc, err);
	ipc_packi(&ipc, ret1);
	ipc_packi(&ipc, ret2);
	ipc_packi(&ipc, ret3);
	ipc_packi(&ipc, ret4);
	ipc_send(&ipc, reply_cap);

	/* Free the saved reply cap */
	cspace_free_slot(cur_cspace, reply_cap);
}

struct event {
	seL4_Word badge;
	seL4_Word label;
	seL4_MessageInfo_t message;
};

void* handle_event(void* e) {
	struct event* event = (struct event*)e;
	seL4_Word badge = event->badge;
	seL4_Word label = event->label;
	seL4_MessageInfo_t message = event->message;

	dprintf(3, "SOS activated\n");
	if (badge & IRQ_EP_BADGE) {
		/* Interrupt */
		if (badge & IRQ_BADGE_NETWORK) {
			dprintf(2, "Network IRQ\n");
			network_irq();
		}
		if (badge & IRQ_BADGE_EPIT1) {
			dprintf(2, "Timer IRQ\n");
			timer_interrupt_epit1();
		}
		if (badge & IRQ_BADGE_EPIT2) {
			dprintf(2, "Timer IRQ\n");
			timer_interrupt_epit2();
		}

	} else if (label == seL4_VMFault) {
		dprintf(2, "VMFAULT\n");
		sos_handle_vmfault(tty_test_process);

	} else if (label == seL4_NoFault) {
		/* System call */
		handle_syscall(badge, seL4_MessageInfo_get_length(message) - 1);

	} else {
		printf("Rootserver got an unknown message\n");
	}
	dprintf(3, "Coro complete\n");
	return 0;
}

void* event_loop(void* void_ep) {
	seL4_CPtr ep = (seL4_CPtr)void_ep;
	while (1) {
		seL4_Word badge;
		seL4_Word label;
		seL4_MessageInfo_t message;
		message = seL4_Wait(ep, &badge);
		label = seL4_MessageInfo_get_label(message);
		struct event e = {badge, label, message};
		coro event_handler = coroutine(&handle_event);
		resume(event_handler, &e);
	}
	return NULL;
}

static void print_bootinfo(const seL4_BootInfo* info) {
	int i;

	/* General info */
	dprintf(1, "Info Page:  %p\n", info);
	dprintf(1, "IPC Buffer: %p\n", info->ipcBuffer);
	dprintf(1, "Node ID: %d (of %d)\n", info->nodeID, info->numNodes);
	dprintf(1, "IOPT levels: %d\n", info->numIOPTLevels);
	dprintf(1, "Init cnode size bits: %d\n", info->initThreadCNodeSizeBits);

	/* Cap details */
	dprintf(1, "\nCap details:\n");
	dprintf(1, "Type              Start      End\n");
	dprintf(1, "Empty             0x%08x 0x%08x\n", info->empty.start,
			info->empty.end);
	dprintf(1, "Shared frames     0x%08x 0x%08x\n", info->sharedFrames.start,
			info->sharedFrames.end);
	dprintf(1, "User image frames 0x%08x 0x%08x\n", info->userImageFrames.start,
			info->userImageFrames.end);
	dprintf(1, "User image PTs    0x%08x 0x%08x\n", info->userImagePTs.start,
			info->userImagePTs.end);
	dprintf(1, "Untypeds          0x%08x 0x%08x\n", info->untyped.start,
			info->untyped.end);

	/* Untyped details */
	dprintf(1, "\nUntyped details:\n");
	dprintf(1, "Untyped Slot       Paddr      Bits\n");
	for (i = 0; i < info->untyped.end - info->untyped.start; i++) {
		dprintf(1, "%3d     0x%08x 0x%08x %d\n", i, info->untyped.start + i,
				info->untypedPaddrList[i], info->untypedSizeBitsList[i]);
	}

	/* Device untyped details */
	dprintf(1, "\nDevice untyped details:\n");
	dprintf(1, "Untyped Slot       Paddr      Bits\n");
	for (i = 0; i < info->deviceUntyped.end - info->deviceUntyped.start; i++) {
		dprintf(
			1, "%3d     0x%08x 0x%08x %d\n", i, info->deviceUntyped.start + i,
			info->untypedPaddrList[i +
								   (info->untyped.end - info->untyped.start)],
			info->untypedSizeBitsList[i + (info->untyped.end -
										   info->untyped.start)]);
	}

	dprintf(1, "-----------------------------------------\n\n");

	/* Print cpio data */
	dprintf(1, "Parsing cpio data:\n");
	dprintf(1, "--------------------------------------------------------\n");
	dprintf(1, "| index |        name      |  address   | size (bytes) |\n");
	dprintf(1, "|------------------------------------------------------|\n");
	for (i = 0;; i++) {
		unsigned long size;
		const char* name;
		void* data;

		data = cpio_get_entry(_cpio_archive, i, &name, &size);
		if (data != NULL) {
			dprintf(1, "| %3d   | %16s | %p | %12d |\n", i, name, data, size);
		} else {
			break;
		}
	}
	dprintf(1, "--------------------------------------------------------\n");
}

void start_first_process(char* app_name, seL4_CPtr fault_ep) {
	/* Run the init process... if we actually used an init, actually just a
	 * shell */
	struct process* process = process_create(CONFIG_SOS_STARTUP_APP, fault_ep);
	tty_test_process = process;
}

static inline seL4_CPtr badge_irq_ep(seL4_CPtr ep, seL4_Word badge) {
	seL4_CPtr badged_cap =
		cspace_mint_cap(cur_cspace, cur_cspace, ep, seL4_AllRights,
						seL4_CapData_Badge_new(badge | IRQ_EP_BADGE));
	conditional_panic(!badged_cap, "Failed to allocate badged cap");
	return badged_cap;
}

/*
 * Main entry point - called by crt.
 */
int main(void) {
#ifdef SEL4_DEBUG_KERNEL
	seL4_DebugNameThread(seL4_CapInitThreadTCB, "SOS:root");
#endif

	dprintf(0, "\nSOS Starting...\n");

	_sos_early_init(&_sos_ipc_ep_cap, &_sos_interrupt_ep_cap);
	panic("This should never be reached");
	/* Not reached */
	return 0;
}

/* Early initialization, before we switch stacks */
static void _sos_early_init(seL4_CPtr* ipc_ep, seL4_CPtr* async_ep) {
	seL4_Word dma_addr;
	seL4_Word low, high;
	int err;

	/* Retrieve boot info from seL4 */
	_boot_info = seL4_GetBootInfo();
	conditional_panic(!_boot_info, "Failed to retrieve boot info\n");

	/* Initialise the untyped sub system and reserve memory for DMA */
	err = ut_table_init(_boot_info);

	conditional_panic(err, "Failed to initialise Untyped Table\n");

	initialise_vmem_layout();
	if (verbose > 0) {
		print_bootinfo(_boot_info);
		print_vmem_layout();
	}
	/* DMA uses a large amount of memory that will never be freed */
	dma_addr = ut_steal_mem(DMA_SIZE_BITS);
	conditional_panic(dma_addr == 0, "Failed to reserve DMA memory\n");

	/* find available memory */
	ut_find_memory(&low, &high);

	/* Initialise the untyped memory allocator */
	ut_allocator_init(low, high);

	/* Initialise the cspace manager */
	err = cspace_root_task_bootstrap(ut_alloc, ut_free, ut_translate, malloc,
									 free);
	conditional_panic(err, "Failed to initialise the c space\n");

	/* Initialise DMA memory */
	err = dma_init(dma_addr, DMA_SIZE_BITS);
	conditional_panic(err, "Failed to intiialise DMA memory\n");

	/* Initialise the new kernel stack */
	for (seL4_Word i = KERNEL_STACK_VSTART; i < KERNEL_STACK_VEND;
		 i += PAGE_SIZE_4K) {
		seL4_Word new_frame;
		seL4_Word paddr = ut_alloc(seL4_PageBits);
		conditional_panic(paddr == 0,
						  "Not enough memory to allocate kernel stack");
		int err = cspace_ut_retype_addr(paddr, seL4_ARM_SmallPageObject,
										seL4_PageBits, cur_cspace, &new_frame);
		conditional_panic(err, "Kernel stack failed to retype memory at init");
		err = map_page(new_frame, seL4_CapInitThreadPD, i, seL4_AllRights,
					   seL4_ARM_ExecuteNever | seL4_ARM_PageCacheable);
		conditional_panic(err, "Kernel stack failed to map memory at init");
	}
	utils_run_on_stack((void*)KERNEL_STACK_VEND, _sos_late_init, NULL);
}

/* After the stack is moved, finish initialization */
static void* _sos_late_init(void* unusedarg) {
	(void)unusedarg;

	ft_initialize();

	/* Initialiase other system compenents here */
	_sos_ipc_init(&_sos_ipc_ep_cap, &_sos_interrupt_ep_cap);
	sos_main();  // Should never return
	panic("We should not be here");
	return NULL;
}

static void _sos_ipc_init(seL4_CPtr* ipc_ep, seL4_CPtr* async_ep) {
	seL4_Word ep_addr, aep_addr;
	int err;

	/* Create an Async endpoint for interrupts */
	aep_addr = ut_alloc(seL4_EndpointBits);
	conditional_panic(!aep_addr, "No memory for async endpoint");
	err = cspace_ut_retype_addr(aep_addr, seL4_AsyncEndpointObject,
								seL4_EndpointBits, cur_cspace, async_ep);
	conditional_panic(err, "Failed to allocate c-slot for Interrupt endpoint");

	/* Bind the Async endpoint to our TCB */
	err = seL4_TCB_BindAEP(seL4_CapInitThreadTCB, *async_ep);
	conditional_panic(err, "Failed to bind ASync EP to TCB");

	/* Create an endpoint for user application IPC */
	ep_addr = ut_alloc(seL4_EndpointBits);
	conditional_panic(!ep_addr, "No memory for endpoint");
	err = cspace_ut_retype_addr(ep_addr, seL4_EndpointObject, seL4_EndpointBits,
								cur_cspace, ipc_ep);
	conditional_panic(err, "Failed to allocate c-slot for IPC endpoint");
}
static void sos_main(void) {
	/* Initialise the network hardware */
	network_init(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_NETWORK));

	start_timer(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_EPIT1),
				badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_EPIT2));

	conditional_panic(serial_dev_init() < 0, "Serial init failed");
	// test_timers();
	// register_timer(1000 * 1000, &simple_timer_callback, NULL);

	// printf("Running frame test\n");
	// frame_test();

	/* Start the user application */
	start_first_process("First Process", _sos_ipc_ep_cap);

	/* Wait on synchronous endpoint for IPC */
	dprintf(0, "\nSOS entering syscall loop\n");

	coro coro_event_loop = coroutine(event_loop);
	resume(coro_event_loop, (void*)_sos_ipc_ep_cap);
}