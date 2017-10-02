#pragma once
#include <clock/clock.h>
#include <cspace/cspace.h>
#include <filetable.h>
#include <region_manager.h>
#include <sel4/sel4.h>
#include <utils/picoro.h>
#define MAX_PROCESSES (256)

extern struct semaphore* any_pid_exit_signal;

struct vspace {
	struct page_directory* pagetable;
	region_list* regions;

	vaddr_t sbrk;
};

enum process_status {
	PROCESS_ALIVE = 0,
	PROCESS_ZOMBIE,
};

struct process {
	seL4_Word tcb_addr;
	seL4_TCB tcb_cap;
	seL4_Word vroot_addr;
	seL4_Word ipc_buffer_addr;
	seL4_CPtr ipc_buffer_cap;
	cspace_t* croot;
	struct vspace vspace;

	struct fd_table fds;
	uint32_t pid;
	char* name;
	timestamp_t start_time;

	enum process_status status;
	coro current_coroutine;
	struct semaphore* event_finished_syscall;
	struct semaphore* event_exited;
};

extern struct process* process_table[MAX_PROCESSES];

struct process* get_process(int32_t pid);
struct process* process_create(char* app_name);
void process_kill(struct process* process, uint32_t status);
