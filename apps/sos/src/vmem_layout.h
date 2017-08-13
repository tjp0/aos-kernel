/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _MEM_LAYOUT_H_
#define _MEM_LAYOUT_H_

#define FRAME_VSTART		(0x08000000)
#define FRAME_SIZE_BITS		(31)
#define FRAME_VEND			((unsigned int) (FRAME_VSTART + (1ull << FRAME_SIZE_BITS)))

#define FRAME_TABLE_VSTART (FRAME_VEND+0x1000)
#define FRAME_TABLE_SIZE_BITS (24)
#define FRAME_TABLE_VEND ((unsigned int) (FRAME_TABLE_VSTART + (1ull << FRAME_TABLE_SIZE_BITS)))

/* Address where memory used for DMA starts getting mapped.
 * Do not use the address range between DMA_VSTART and DMA_VEND */
#define DMA_VSTART          (FRAME_TABLE_VEND+0x1000)
#define DMA_SIZE_BITS       (22)
#define DMA_VEND            ((unsigned int) (DMA_VSTART + (1ull << DMA_SIZE_BITS)))

/* From this address onwards is where any devices will get mapped in
 * by the map_device function. You should not use any addresses beyond
 * here without first modifying map_device */
#define DEVICE_START        (0xB0000000)

#define ROOT_VSTART         (0xC0000000)

/* Constants for how SOS will layout the address space of any
 * processes it loads up */
#define PROCESS_STACK_TOP   (0x90000000)
#define PROCESS_IPC_BUFFER  (0xA0000000)
#define PROCESS_VMEM_START  (0xC0000000)

#define PROCESS_SCRATCH     (0xD0000000)


#endif /* _MEM_LAYOUT_H_ */
