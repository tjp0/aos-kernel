#include <copy.h>
#include <devices/devices.h>
#include <process.h>
#include <sos.h>
#include <sos_coroutine.h>
#include <vm.h>
#define verbose 1
#include <sys/debug.h>
#include <sys/kassert.h>

uint32_t syscall_process_create(struct process* process, vaddr_t path) {
	trace(5);
	char filename[N_NAME + 1];
	if (copy_vspace2sos(path, filename, &process->vspace, N_NAME, 0) < 0) {
		trace(5);
		return -1;
	}

	struct process* new_process = process_create(filename);
	if (new_process == NULL) {
		return -1;
	}

	return new_process->pid;
}

uint32_t syscall_process_exit(struct process* process, uint32_t status) {
	trace(5);
	process_signal_kill(process);
	return 0;
}

/* Returns ID of caller's process. */
uint32_t syscall_process_my_id(struct process* process) { return process->pid; }

uint32_t syscall_process_status(struct process* process, vaddr_t processes,
								unsigned max) {

    region_list *rl = process->vspace.regions;
    if (!rl) { 
        return 0; 
    }
    region_node *r = find_region(rl, processes);
    if (!r) { 
        return 0; 
    }

    /* check ptr is in a writable region */
    if (!(r->perm & PAGE_WRITABLE)) { 
        return 0; 
    }

	unsigned int buf_size = max * sizeof(sos_process_t);

	if (max == 0) {
		return 0;
	}
	sos_process_t* things = malloc(buf_size);
	if (!things) {
		return 0;
	}
	memset(things, 0, buf_size);
	uint32_t pid = 1;
	uint32_t pid_count = 0;

	for (pid = 0; pid < MAX_PROCESSES; ++pid) {
		if (process_table[pid] &&
			process_table[pid]->status != PROCESS_ZOMBIE) {
			// popualate the things for each entry
			things[pid_count].pid = process_table[pid]->pid;
			things[pid_count].stime = process_table[pid]->start_time / 1000;
			things[pid_count].size =
				process_table[pid]->vspace.pagetable->num_ptes;
			strncpy(things[pid_count].command, process_table[pid]->name,
					N_NAME);

			dprintf(0, "pid command %s \n", process_table[pid]->name);
			pid_count++;
			if (pid_count == max) {
				break;
			}
		}
	}

	// pid_count++;
	int err = copy_sos2vspace(things, processes, &process->vspace,
							  pid_count * sizeof(sos_process_t), 0);
	free(things);
	if (err < 0) {
		trace(5);
		return -1;
	}
	return pid_count;
}

uint32_t syscall_process_kill(struct process* process, uint32_t pid) {
	if (pid == 0) {
		return -1;
	}

    struct process *p = current_process();
	if (pid == p->pid) {
		syscall_process_exit(process, 0);
	}

	dprintf(0, "Killing pid %d \n", pid);
	struct process* process_to_kill = get_process(pid);

	if (!process_to_kill) {
		return -1;
	}
	dprintf(0, "Killing process %d \n", process_to_kill);
	process_signal_kill(process_to_kill);
	return 0;
}

static uint32_t wait_pid(uint32_t pid) {
	if (pid == 0) {
		return -1;
	}
    if (pid == current_process()->pid) {
        return -1;
    }
	struct process* process_to_wait_on = get_process(pid);
	if (!process_to_wait_on) {
		return -1;
	}
	if (process_to_wait_on->status == PROCESS_ZOMBIE) {
		return pid;
	}
	pid_t ret_pid = (pid_t)wait(process_to_wait_on->event_exited);
	// sanity check that we have actually been notified of the correct process
	kassert(ret_pid == pid);
	return ret_pid;
}
static uint32_t wait_any(void) { return (uint32_t)wait(any_pid_exit_signal); }

uint32_t syscall_process_wait(struct process* process, uint32_t pid) {
	if (pid == -1) {
		return wait_any();
	}
	return wait_pid(pid);
}
/* A debugging syscall that just dumps a bunch of info about a process to the
 * serial console */
uint32_t syscall_process_debug(struct process* me, uint32_t pid) {
	if (process_table[pid] == NULL) {
		return -1;
	}
	struct process* process = process_table[pid];
	printf("*** %s ***\n", process->name);
	printf("PID: %u\n", process->pid);
	if (process->status != PROCESS_ALIVE) {
		printf("Dead process\n");
		return 0;
	}
	regions_print(process->vspace.regions);
	return 0;
}

/* TODO
						(  ) (@@) ( )  (@)  ()    @@    O     @     O     @
 (@@@               (                (@@@@ (   )
	   ====        ________                ___________
   _D _|  |_______/        \__I_I_____===__|_________|
	|(_)---  |   H\________/ |   |        =|___ ___|      _________________
	/     |  |   H  |  |     |   |         ||_| |_||     _| \_____A
   |      |  |   H  |__--------------------| [___] |   =| | |
   ________|___H__/__|_____/[][]~\_______|       |   -| |
   |/ |   |-----------I_____I [][] []  D
   |=======|____|________________________|_
 __/ =| o |=-~~\  /~~\  /~~\  /~~\
 ____Y___________|__|__________________________|_
  |/-=|___|=   O=====O=====O=====O|_____/~\___/          |_D__D__D_|
  |_D__D__D_|
   \_/      \__/  \__/  \__/  \__/      \_/               \_/   \_/    \_/ \_/
*/
