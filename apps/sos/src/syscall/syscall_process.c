#include <copy.h>
#include <process.h>
#include <sos.h>
#include <vm.h>
#define verbose 6
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
	process_kill(process, status);
	return 0;
}

/* Returns ID of caller's process. */
uint32_t syscall_process_my_id(struct process* process) { return process->pid; }

uint32_t syscall_process_status(struct process* process, vaddr_t processes,
								unsigned max) {
	unsigned int buf_size = max * sizeof(sos_process_t);

	sos_process_t* things = malloc(buf_size);
	if (!things) {
		return -1;
	}
	memset(things, 0, buf_size);
	uint32_t pid = 1;
	uint32_t pid_count = 0;

	for (pid = 1; pid < MAX_PROCESSES; ++pid) {
		if (process_table[pid]) {
			// popualate the things for each entry
			things[pid_count].pid = process_table[pid]->pid;
			things[pid_count].stime = process_table[pid]->start_time;
			things[pid_count].size =
				process_table[pid]->vspace.pagetable->num_ptes;
			strncpy(things[pid_count].command, process_table[pid]->name,
					N_NAME);

			dprintf(0, "pid command %s \n", process_table[pid]->name);
			pid_count++;
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
	dprintf(0, "Killing pid %d \n", pid);
	struct process* process_to_kill = get_process(pid);

	if (!process_to_kill) {
		return -1;
	}
	uint32_t status = -1;
	dprintf(0, "Killing process %d \n", process_to_kill);
	process_kill(process_to_kill, status);
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
