#pragma once
#include <process.h>
#include <stdint.h>
#include <typedefs.h>
#define COPY_INSTRUCTIONFLUSH 0x1
#define COPY_VSPACE2SOS 0x2
#define COPY_RETURNWRITTEN 0x4

int64_t copy_sos2vspace(void* src, vaddr_t dest_vaddr, struct vspace* vspace,
						int64_t len, uint32_t flags);

int64_t copy_vspace2sos(vaddr_t src, void* dst, struct vspace* vspace,
						int64_t len, uint32_t flags);