#pragma once
#include <autoconf.h>
#include <clock/clock.h>
#include <cspace/cspace.h>
#include <filetable.h>
#include <libcoro.h>
#include <region_manager.h>
#include <sel4/sel4.h>
#define MAX_PROCESSES CONFIG_SOS_PROCESS_MAX

extern struct semaphore* any_pid_exit_signal;

struct vspace {
	struct page_directory* pagetable;
	region_list* regions;

	vaddr_t sbrk;
};

enum process_status {
	PROCESS_ALIVE = 0,
	PROCESS_TO_DIE,
	PROCESS_DYING,
	PROCESS_ZOMBIE,
};

struct process {
	seL4_Word tcb_addr;
	seL4_TCB tcb_cap;
	cspace_t* croot;
	struct vspace vspace;
	vaddr_t elfentry;

	struct fd_table fds;
	uint32_t pid;
	char* name;
	timestamp_t start_time;

	enum process_status status;
	coro coroutine;
	struct semaphore* event_finished_syscall;
	struct semaphore* event_exited;
};

extern struct process* process_table[MAX_PROCESSES];

struct process* get_process(uint32_t pid);
struct process* process_create(char* app_name);
void process_kill(struct process* process, uint32_t status);
void process_signal_kill(struct process* process);
void process_init(void);
extern struct process sos_process;
