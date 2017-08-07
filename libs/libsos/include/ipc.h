#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sel4/sel4.h>
#include <string.h>

struct ipc_command {
	int length;
	seL4_Uint32 label;
	int err;
};

#define DIV_UP(x,y) ((x+(y-1))/y)

static inline
struct ipc_command ipc_create(void) {
	struct ipc_command ipc;
	ipc.length = 0;
	ipc.label = 0;
	ipc.err = 0;
	return ipc;
}

static inline
int ipc_packi(struct ipc_command* ipc, seL4_Word word) {

	if(ipc->length + 1 > seL4_MsgMaxLength) {
		ipc->err = 1;
		return 0;
	}
	seL4_SetMR(ipc->length, word);
	ipc->length += 1;
	return 1;
}
static inline
int ipc_packs(struct ipc_command* ipc, seL4_Word length, void* string) {

	if(!ipc_packi(ipc,length)) {
		ipc->err = 1;
		return 0;
	}

	if(ipc->length + DIV_UP(length,4) > seL4_MsgMaxLength) {
		ipc->err = 1;
		return 0;
	}
	void* target = &(seL4_GetIPCBuffer()->msg[ipc->length]);
	memcpy(target,string,length);
	ipc->length += DIV_UP(length,4);
	return 1;
}
static inline
int ipc_unpacki(struct ipc_command* ipc, seL4_Word* integer) {
	if(ipc->length > seL4_MsgMaxLength) {
		ipc->err = 1;
		return 0;
	}
	if(integer != NULL) {
		*integer = seL4_GetMR(ipc->length);
	}
	ipc->length += 1;

	return 1;
}
static inline
int ipc_unpackb(struct ipc_command* ipc, seL4_Word* length, void** array) {
	if(!ipc_unpacki(ipc,length)) {
		ipc->err = 1;
		return 0;
	}

	size_t sel4len = DIV_UP(*length, 4);

	if(ipc->length + sel4len > seL4_MsgMaxLength) {
		ipc->err = 1;
		return 0;
	}

	*array = &(seL4_GetIPCBuffer()->msg[ipc->length]);
	ipc->length += sel4len;

	return 1;
}
static inline
void ipc_call(struct ipc_command* ipc, seL4_CPtr dest) {
	seL4_MessageInfo_t tag = seL4_MessageInfo_new(ipc->label, 0, 0, ipc->length);
    seL4_SetTag(tag);
    seL4_Call(dest,tag);
    return;
}
static inline
size_t ipc_maxs(struct ipc_command* ipc) {
	return (seL4_MsgMaxLength - ipc->length - 1)*sizeof(seL4_Word);
}
static inline
int ipc_err(struct ipc_command* ipc) {
	return ipc->err;
}