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

// should probably not do this but whatevs
extern seL4_Word ft_size;
extern seL4_Word ft_numframes;
extern seL4_Word lowest_address;
extern seL4_Word highest_address;
// where is the kernel loaded
// load after that
extern seL4_Word FRAME_VSTART;  // (0x08000000)

// //
extern seL4_Word FRAME_SIZE_BITS;  // (31)
extern seL4_Word FRAME_VEND;	   // ((unsigned int)(FRAME_VSTART + (1ull <<
// FRAME_SIZE_BITS)))

extern seL4_Word FRAME_TABLE_VSTART;	 // (FRAME_VEND + 0x1000)
extern seL4_Word FRAME_TABLE_SIZE_BITS;  // (24)
extern seL4_Word FRAME_TABLE_VEND;		 //
// 	((unsigned int)(FRAME_TABLE_VSTART + (1ull << FRAME_TABLE_SIZE_BITS)))

// /* Address where memory used for DMA starts getting mapped.
//  * Do not use the address range between DMA_VSTART and DMA_VEND */
extern seL4_Word DMA_VSTART;	 // (FRAME_TABLE_VEND + 0x1000)
extern seL4_Word DMA_SIZE_BITS;  // (22)
extern seL4_Word DMA_VEND;
extern seL4_Word PROCESS_TOP;
// ((unsigned int)(DMA_VSTART + (1ull << DMA_SIZE_BITS)))

// /* From this address onwards is where any devices will get mapped in
//  * by the map_device function. You should not use any addresses beyond
//  * here without first modifying map_device */
#define DEVICE_START (0xB0000000)

// #define ROOT_VSTART (0xC0000000)

// /* Constants for how SOS will layout the address space of any
//  * processes it loads up */

// #define PROCESS_TOP (0xD0000000)
void initialise_vmem_layout(void);
// seL4_Word get_FRAME_VSTART(void);

// seL4_Word get_FRAME_SIZE_BITS(void);
// seL4_Word get_FRAME_VEND(void);

// seL4_Word get_FRAME_TABLE_VSTART(void);
// seL4_Word get_FRAME_TABLE_SIZE_BITS(void);
// seL4_Word get_FRAME_TABLE_VEND(void);

// /* Address where memory used for DMA starts getting mapped.
//  * Do not use the address range between DMA_VSTART and DMA_VEND */
// seL4_Word get_DMA_VSTART(void);
// seL4_Word get_DMA_SIZE_BITS(void);
// seL4_Word get_DMA_VEND(void);

// /* From this address onwards is where any devices will get mapped in
//  * by the map_device function. You should not use any addresses beyond
//  * here without first modifying map_device */
// // seL4_Word get_DEVICE_START(void);

// seL4_Word get_ROOT_VSTART(void);

// /* Constants for how SOS will layout the address space of any
//  * processes it loads up */

// seL4_Word get_PROCESS_TOP(void);
// // #endif /* _MEM_LAYOUT_H_ */
