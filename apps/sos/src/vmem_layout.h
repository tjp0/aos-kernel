/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#pragma once
#include <sel4/sel4.h>

// #ifndef _MEM_LAYOUT_H_
// #define _MEM_LAYOUT_H_

extern seL4_Word ft_size;
extern seL4_Word ft_numframes;
extern seL4_Word lowest_address;
extern seL4_Word highest_address;
// where is the kernel loaded
// load after that

extern seL4_Word FRAME_VSTART;

extern seL4_Word FRAME_SIZE_BITS;
extern seL4_Word FRAME_VEND;
extern seL4_Word FRAME_TABLE_VSTART;
extern seL4_Word FRAME_TABLE_SIZE_BITS;
extern seL4_Word FRAME_TABLE_VEND;

/* Address where memory used for DMA starts getting mapped. */
extern seL4_Word DMA_VSTART;
#define DMA_SIZE_BITS 22
extern seL4_Word DMA_VEND;

#define PROCESS_TOP (0xD0000000)

void initialize_sos_memory(seL4_ARM_PageDirectory pd,
						   const seL4_BootInfo* bootinfo);
void print_vmem_layout(void);
