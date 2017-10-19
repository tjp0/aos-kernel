#pragma once
#include <process.h>
#include <stdint.h>
#include <typedefs.h>

/* Flags for the functions below*/
#define COPY_INSTRUCTIONFLUSH 0x1 /* Flush the instruction cache after */
#define COPY_VSPACE2SOS 0x2		  /* Reversal flag; used internally */
#define COPY_RETURNWRITTEN                                                   \
	0x4 /* Return how many bytes were written in the case of failure instead \
		   of -1 */

/* Copies data from sos into a vspace. Returns -1 if the copy is unsuccessful
 * (inc. partially) */
int64_t copy_sos2vspace(void* src, vaddr_t dest_vaddr, struct vspace* vspace,
						int64_t len, uint32_t flags);

/* vis versa */
int64_t copy_vspace2sos(vaddr_t src, void* dst, struct vspace* vspace,
						int64_t len, uint32_t flags);
