#include <clock/clock.h>
#include <stdio.h>
#include <cspace/cspace.h>
#include "dma.h"
#include "mapping.h"
#include "ut_manager/ut.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#define EPIT1_IRQ 88
#define EPIT_BASE (void*) 0x020D0000

#define EPIT_CR_CLKSRC_PERIPHERAL (1 << 24)
#define EPIT_CR_COMPAREENABLE (1 << 2)
#define EPIT_CR_ENMOD1 (1 << 1)
#define EPIT_CR_ENABLE (1 << 0)
#define TICKS_PER_MICROSECOND 66

static seL4_CPtr epit1_irq_cap;

struct EPIT_r {
	uint32_t EPIT1_CR;
	uint32_t EPIT1_SR;
	uint32_t EPIT1_LR;
	uint32_t EPIT1_CMPR;
	uint32_t EPIT1_CNR;
	uint32_t EPIT2_CR;
	uint32_t EPIT2_SR;
	uint32_t EPIT2_LR;
	uint32_t EPIT2_CMPR;
	uint32_t EPIT2_CNR;
};

static struct EPIT_r* epit_registers;

static seL4_CPtr
enable_irq(int irq, seL4_CPtr aep) {
    seL4_CPtr cap;
    int err;
    /* Create an IRQ handler */
    cap = cspace_irq_control_get_cap(cur_cspace, seL4_CapIRQControl, irq);
    /* Assign to an end point */
    err = seL4_IRQHandler_SetEndpoint(cap, aep);
    conditional_panic(err, "Failed to set interrupt endpoint");
    /* Ack the handler before continuing */
    err = seL4_IRQHandler_Ack(cap);
    conditional_panic(err, "Failure to acknowledge pending interrupts");
    return cap;
}

int start_timer(seL4_CPtr interrupt_ep) {
	printf("Starting timer\n");
	epit1_irq_cap = enable_irq(EPIT1_IRQ, interrupt_ep);
	epit_registers = map_device(EPIT_BASE, sizeof(struct EPIT_r));

	if(epit_registers == NULL) {
		printf("fail\n");
	}

	epit_registers->EPIT1_CR = (EPIT_CR_CLKSRC_PERIPHERAL | EPIT_CR_COMPAREENABLE | EPIT_CR_ENMOD1);
	epit_registers->EPIT1_CR |= EPIT_CR_ENABLE;

	epit_registers->EPIT1_CMPR = UINT32_MAX/2;
	printf("EPIT SET: %x\n",epit_registers->EPIT1_CR);
	printf("current epit: %x\n",epit_registers->EPIT1_CNR);

	return 0;
}

static inline uint32_t ts_getlower(void) {
	return UINT32_MAX-(epit_registers->EPIT1_CNR);
}

static uint32_t ts_upper = 0;
static uint32_t last_cr = 0;
static inline void time_stamp_wrap(void) {
	uint32_t cur = ts_getlower();
	if(last_cr > cur) {
		//ts_upper+= (UINT32_MAX + 1);
		ts_upper += 1;
	}
	last_cr = cur;
}

int timer_interrupt(void) {
	epit_registers->EPIT1_SR |= 1;
	if(epit_registers->EPIT1_CMPR == UINT32_MAX/2)
	{
		epit_registers->EPIT1_CMPR = 0;
	} else {
		epit_registers->EPIT1_CMPR = UINT32_MAX/2;
	}

	int err = seL4_IRQHandler_Ack(epit1_irq_cap);

	time_stamp_wrap();
	printf("ts upper: %u\n",ts_upper);
	printf("ts lower: %u\n",ts_getlower());
	printf("timestamp: %llu\n",time_stamp());
    assert(!err);
	return 0;
}

timestamp_t time_stamp(void) {
	time_stamp_wrap();
	return ((int64_t) ts_upper*UINT32_MAX + (uint64_t) ts_getlower()) / TICKS_PER_MICROSECOND;
}