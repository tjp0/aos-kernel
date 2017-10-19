#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef uint32_t vaddr_t;

struct syscall_return {
	bool valid;
	uint32_t err;
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;
	uint32_t arg4;
};


