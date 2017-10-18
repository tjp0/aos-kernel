#pragma once
#include <ipc.h>
#include <sos.h>

#define SOS_IPC_EP_CAP (0x1)

static inline int SYSCALL_ARG3(int syscall_no, int arg1, int arg2, int arg3) {
	struct ipc_command ipc = ipc_create();
	ipc_packi(&ipc, syscall_no);
	ipc_packi(&ipc, arg1);
	ipc_packi(&ipc, arg2);
	ipc_packi(&ipc, arg3);
	return ipc_call(&ipc, SOS_IPC_EP_CAP);
}

static inline int SYSCALL_ARG2(int syscall_no, int arg1, int arg2) {
	return SYSCALL_ARG3(syscall_no, arg1, arg2, 0);
}

static inline int SYSCALL_ARG1(int syscall_no, int arg1) {
	return SYSCALL_ARG3(syscall_no, arg1, 0, 0);
}

static inline int SYSCALL_ARG0(int syscall_no) {
	return SYSCALL_ARG3(syscall_no, 0, 0, 0);
}

#define SOS_SYSCALL_SBRK 0
#define SOS_SYSCALL_USLEEP 1
#define SOS_SYSCALL_TIMESTAMP 2
#define SOS_SYSCALL_TERMINALSLEEP 4
#define SOS_SYSCALL_OPEN 5
#define SOS_SYSCALL_WRITE 6
#define SOS_SYSCALL_READ 7
#define SOS_SYSCALL_CLOSE 8
#define SOS_SYSCALL_STAT 9
#define SOS_SYSCALL_GETDIRENT 10
#define SOS_SYSCALL_PROCESS_CREATE 11
#define SOS_SYSCALL_EXIT 12
#define SOS_SYSCALL_PROCESS_MY_ID 13
#define SOS_SYSCALL_PROCESS_STATUS 14
#define SOS_SYSCALL_PROCESS_KILL 15
#define SOS_SYSCALL_PROCESS_WAIT 16
#define SOS_SYSCALL_MMAP 17
#define SOS_SYSCALL_MUNMAP 18
#define SOS_SYSCALL_PROCESS_DEBUG 19
