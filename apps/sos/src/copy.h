#pragma once
#include <process.h>
#include <stdint.h>
#include <typedefs.h>
#define COPY_FLUSH 0x1

int64_t copy_sos2vspace(void* src, vaddr_t dest_vaddr, struct vspace* vspace,
						int64_t len, uint32_t flags);