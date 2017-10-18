#pragma once
#include <process.h>
#include <stdint.h>
/* Returns 0 on error, otherwise the previous sbrk location */
uint32_t syscall_sbrk(struct process* process, uint32_t size);
uint32_t syscall_mmap(struct process* process, uint32_t size, uint32_t perms);
uint32_t syscall_munmap(struct process* process, vaddr_t vaddr);
