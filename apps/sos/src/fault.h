#pragma once
#include <stdint.h>
#include <sel4/sel4.h>
/* A bunch of defs and inline functions for ARM status register parsing */

#define FSR_ALIGNMENTFAULT 0b1
#define FSR_TRANSLATION_FAULT_FIRSTLEVEL 0b101
#define FSR_TRANSLATION_FAULT_SECONDLEVEL 0b111
#define FSR_ACCESSFAULT_FIRSTLEVEL 0b11
#define FSR_ACCESSFAULT_SECONDLEVEL 0b110
#define FSR_PERMISSIONFAULT_FIRSTLEVEL 0b1101
#define FSR_PERMISSIONFAULT_SECONDLEVEL 0b1111
#define FSR_DOMAINFAULT_FIRSTLEVEL 0b1001
#define FSR_DOMAINFAULT_SECONDLEVEL 010011

#define DF_WRITEERROR					(1<<11)

static inline uint32_t fault_maskfault(uint32_t reg) {
	return (reg & 0b1111);
}

static inline int fault_ispermissionfault(uint32_t reg) {
	reg = fault_maskfault(reg);
	return (reg == FSR_PERMISSIONFAULT_SECONDLEVEL || reg == FSR_PERMISSIONFAULT_FIRSTLEVEL);
}

static inline int fault_isaccessfault(uint32_t reg) {
	reg = fault_maskfault(reg);
		return (reg == FSR_ACCESSFAULT_FIRSTLEVEL || reg == FSR_ACCESSFAULT_SECONDLEVEL
			|| reg == FSR_TRANSLATION_FAULT_FIRSTLEVEL || reg == FSR_TRANSLATION_FAULT_SECONDLEVEL);
}

/* Must check if it is actually a permission fault first */
static inline int fault_iswritefault(uint32_t reg) {
	return ((reg & DF_WRITEERROR) > 0);
}

static inline int fault_isalignmentfault(uint32_t reg) {
	reg = fault_maskfault(reg);
	return reg == FSR_ALIGNMENTFAULT;
}

char* fault_getprintable(uint32_t reg);

struct fault {
	uint32_t status;
	uint32_t ifault;
	uint32_t pc;
	uint32_t vaddr;
};

static inline struct fault fault_struct(void) {
	struct fault f;
	f.pc = seL4_GetMR(0);
	f.vaddr = seL4_GetMR(1);
	f.ifault = seL4_GetMR(2);
	f.status = seL4_GetMR(3);
	return f;
}

void print_fault(struct fault f);