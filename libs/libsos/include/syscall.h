#pragma once
#include <ipc.h>
#include <sos.h>

#define SOS_IPC_EP_CAP (0x1)

static inline int SYSCALL_ARG2(int syscall_no, int arg1, int arg2) {
	struct ipc_command ipc = ipc_create();
	ipc_packi(&ipc, syscall_no);
	ipc_packi(&ipc, arg1);
	ipc_packi(&ipc, arg2);
	return ipc_call(&ipc, SOS_IPC_EP_CAP);
}

static inline int SYSCALL_ARG1(int syscall_no, int arg1) {
	return SYSCALL_ARG2(syscall_no, arg1, 0);
}

static inline int SYSCALL_ARG0(int syscall_no) {
	return SYSCALL_ARG2(syscall_no, 0, 0);
}

#define SOS_SYSCALL_SBRK 45
#define SOS_SYSCALL_USLEEP 46
#define SOS_SYSCALL_SERIALWRITE 1001
#define SOS_SYSCALL_TERMINALSLEEP 1000