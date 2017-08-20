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
#include "elf.h"
#include "network.h"

#include "mapping.h"
#include "ut_manager/ut.h"
#include "vmem_layout.h"

#include <autoconf.h>
#include <frametable.h>
#include <process.h>
#include <region_manager.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "test_timer.h"

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

/**
 * NFS mount point
 */
extern fhandle_t mnt_point;

void handle_syscall(seL4_Word badge, int num_args) {
	seL4_Word syscall_number;
	seL4_CPtr reply_cap;

	struct ipc_command ipc = ipc_create();
	ipc_unpacki(&ipc, &syscall_number);

	/* Save the caller */
	reply_cap = cspace_save_reply_cap(cur_cspace);
	assert(reply_cap != CSPACE_NULL);

	/* Process system call */
	switch (syscall_number) {
		case SOS_SYSCALL0:
			dprintf(0, "syscall: thread made syscall 0!\n");

			seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
			seL4_SetMR(0, 0);
			seL4_Send(reply_cap, reply);

			break;
		case SOS_SERIALWRITE:;
			size_t length;
			void* array;
			if (!ipc_unpackb(&ipc, &length, &array)) {
				break;
			}
			serial_send(global_serial, array, length);

			break;

		default:
			printf("%s:%d (%s) Unknown syscall %d\n", __FILE__, __LINE__,
				   __func__, syscall_number);
			/* we don't want to reply to an unknown syscall */
	}

	/* Free the saved reply cap */
	cspace_free_slot(cur_cspace, reply_cap);
}

void syscall_loop(seL4_CPtr ep) {
	while (1) {
		seL4_Word badge;
		seL4_Word label;
		seL4_MessageInfo_t message;

		message = seL4_Wait(ep, &badge);
		label = seL4_MessageInfo_get_label(message);
		if (badge & IRQ_EP_BADGE) {
			/* Interrupt */
			if (badge & IRQ_BADGE_NETWORK) {
				network_irq();
			}
			if (badge & IRQ_BADGE_EPIT1) {
				timer_interrupt_epit1();
			}
			if (badge & IRQ_BADGE_EPIT2) {
				timer_interrupt_epit2();
			}

		} else if (label == seL4_VMFault) {
			/* Page fault */
			dprintf(0, "vm fault at 0x%08x, pc = 0x%08x, status=%08x %s\n", seL4_GetMR(1),
					seL4_GetMR(0),
					seL4_GetMR(3),
					seL4_GetMR(2) ? "Instruction Fault" : "Data fault");

			assert(!"Unable to handle vm faults");
		} else if (label == seL4_NoFault) {
			/* System call */
			handle_syscall(badge, seL4_MessageInfo_get_length(message) - 1);

		} else {
			printf("Rootserver got an unknown message\n");
		}
	}
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

static void _sos_init(seL4_CPtr* ipc_ep, seL4_CPtr* async_ep) {
	seL4_Word dma_addr;
	seL4_Word low, high;
	int err;

	/* Retrieve boot info from seL4 */
	_boot_info = seL4_GetBootInfo();
	conditional_panic(!_boot_info, "Failed to retrieve boot info\n");
	if (verbose > 0) {
		print_bootinfo(_boot_info);

		printf("\nFRAME_VSTART:\t %p\n", (void*)FRAME_VSTART);
		printf("FRAME_VEND:\t %p\n", (void*)FRAME_VEND);
		printf("FRAME_TABLE_VSTART:\t %p\n", (void*)FRAME_TABLE_VSTART);
		printf("FRAME_TABLE_VEND:\t %p\n", (void*)FRAME_TABLE_VEND);
		printf("DMA_VSTART:\t %p\n", (void*)DMA_VSTART);
		printf("DMA_VEND:\t %p\n", (void*)DMA_VEND);
	}

	/* Initialise the untyped sub system and reserve memory for DMA */
	err = ut_table_init(_boot_info);
	conditional_panic(err, "Failed to initialise Untyped Table\n");
	/* DMA uses a large amount of memory that will never be freed */
	dma_addr = ut_steal_mem(DMA_SIZE_BITS);
	conditional_panic(dma_addr == 0, "Failed to reserve DMA memory\n");

	seL4_Word ft_base = ft_early_initialize(_boot_info);

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

	ft_late_initialize(ft_base);

	/* Initialiase other system compenents here */

	_sos_ipc_init(ipc_ep, async_ep);
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

	_sos_init(&_sos_ipc_ep_cap, &_sos_interrupt_ep_cap);

	/* Initialise the network hardware */
	network_init(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_NETWORK));

	global_serial = serial_init();

	start_timer(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_EPIT1),
				badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_EPIT2));

	// test_timers();
	// register_timer(100,&simple_timer_callback,NULL);

	// frame_test();

	/* Start the user application */
	start_first_process("First Process", _sos_ipc_ep_cap);

	/* Wait on synchronous endpoint for IPC */
	dprintf(0, "\nSOS entering syscall loop\n");
	syscall_loop(_sos_ipc_ep_cap);

	/* Not reached */
	return 0;
}
