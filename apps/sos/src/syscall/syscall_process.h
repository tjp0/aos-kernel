#pragma once
#include <process.h>
#include <stdint.h>
#include <typedefs.h>
/* Returns the pid of the newly created process */
uint32_t syscall_process_create(struct process* process, vaddr_t path);
uint32_t syscall_process_exit(struct process* process, uint32_t status);
