#pragma once
#include <stdint.h>
/* Returns 0 on error, otherwise the previous sbrk location */
uint32_t syscall_sbrk(struct process* process, uint32_t size);