#include <clock/clock.h>
#include <stdio.h>
#include <cspace/cspace.h>
#include "dma.h"
#include "mapping.h"
#include "ut_manager/ut.h"
#include "timer.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#define EPIT1_IRQ 88
#define EPIT2_IRQ 89
#define EPIT1_BASE (void*) 0x020D0000
#define EPIT2_BASE (void*) 0x020D4000

#define EPIT_CR_CLKSRC_PERIPHERAL (1 << 24)
#define EPIT_CR_COMPAREENABLE (1 << 2)
#define EPIT_CR_ENMOD1 (1 << 1)
#define EPIT_CR_ENABLE (1 << 0)
#define EPIT_CR_RLD (1 << 3)
#define EPIT_CR_IOVW (1 << 17)
#define TICKS_PER_MICROSECOND 66
#define EPIT_PRESCALER_SHIFT 4
#define MICROSECONDS_PER_MILISECOND 1000
#define MICROSECONDS_PER_SECOND 1000*1000
static seL4_CPtr epit1_irq_cap;
static seL4_CPtr epit2_irq_cap;

struct EPIT_r {
	uint32_t EPIT_CR;
	uint32_t EPIT_SR;
	uint32_t EPIT_LR;
	uint32_t EPIT_CMPR;
	uint32_t EPIT_CNR;
};

static struct EPIT_r* epit1_r;
static struct EPIT_r* epit2_r;

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

int start_timer(seL4_CPtr epit1_ep, seL4_CPtr epit2_ep) {
	//printf("Starting timer\n");
	epit1_irq_cap = enable_irq(EPIT1_IRQ, epit1_ep);
	epit2_irq_cap = enable_irq(EPIT2_IRQ, epit2_ep);

	epit1_r = map_device(EPIT1_BASE, sizeof(struct EPIT_r));
	epit2_r = map_device(EPIT2_BASE, sizeof(struct EPIT_r));
	conditional_panic(epit1_r == NULL || epit2_r == NULL, "Failed to map EPIT registers");

	/* Start EPIT1 timer (microsecond timestamps) */
	epit1_r->EPIT_CR = (EPIT_CR_CLKSRC_PERIPHERAL | EPIT_CR_COMPAREENABLE | EPIT_CR_ENMOD1);
	epit1_r->EPIT_CMPR = UINT32_MAX/2;
	epit1_r->EPIT_LR = UINT32_MAX;
	epit1_r->EPIT_SR |= 1;
	epit1_r->EPIT_CR |= EPIT_CR_ENABLE;
	timer_init();

	return 0;
}

static inline uint32_t ts_getlower(void) {
	return UINT32_MAX-(epit1_r->EPIT_CNR);
}

static uint32_t ts_upper = 0;
static uint32_t last_cr = 0;
static inline void time_stamp_wrap(void) {
	uint32_t cur = ts_getlower();
	if(last_cr > cur) {
		ts_upper += 1;
	}
	last_cr = cur;
}

int timer_interrupt_epit1(void) {
	epit1_r->EPIT_SR |= 1;
	if(epit1_r->EPIT_CMPR == UINT32_MAX/2)
	{
		epit1_r->EPIT_CMPR = 0;
	} else {
		epit1_r->EPIT_CMPR = UINT32_MAX/2;
	}

	int err = seL4_IRQHandler_Ack(epit1_irq_cap);

	time_stamp_wrap();
	printf("ts upper: %u\n",ts_upper);
	printf("ts lower: %u\n",ts_getlower());
	printf("timestamp: %llu\n",time_stamp());
    assert(!err);
	return 0;
}

void epit2_sleepto(timestamp_t timestamp) {
	int64_t curtime = time_stamp();
	int64_t diff = timestamp - curtime;
	if(diff > UINT32_MAX*4096*TICKS_PER_MICROSECOND) {
		/* Fail */
		printf("TOO BIG SO FAIL\n");
	}

	if(diff < 0) {
		printf("IMEEDIATE RUN\n");
		/* TODO: Handle immediately */
	}

	diff *= TICKS_PER_MICROSECOND;

	uint32_t scaler_bit = 1;
	while(diff > UINT32_MAX)
	{
		diff /= 2;
		scaler_bit*=2;
	}
	scaler_bit -= 1;

	uint32_t scaler_bitshift = (scaler_bit << EPIT_PRESCALER_SHIFT);
	epit2_r->EPIT_CR = (EPIT_CR_CLKSRC_PERIPHERAL | EPIT_CR_COMPAREENABLE | EPIT_CR_ENMOD1 | scaler_bitshift | EPIT_CR_RLD | EPIT_CR_IOVW);
	epit2_r->EPIT_LR = diff;
	epit2_r->EPIT_CMPR = 0;
	epit2_r->EPIT_SR |= 1;
	epit2_r->EPIT_CR |= EPIT_CR_ENABLE;

	printf("Sleeping until %lld. Current time is %lld\n",timestamp,curtime);
	printf("(%lld) ticks, (%u) scaler\n",diff,scaler_bit+1);
}

int timer_interrupt_epit2(void) {
	epit2_r->EPIT_SR |= 1;
	epit2_r->EPIT_CR &= ~EPIT_CR_ENABLE;
	int err = seL4_IRQHandler_Ack(epit2_irq_cap);
	assert(!err);

	timer_interrupt();

	return 0;
	//Handle event timer
}

timestamp_t time_stamp(void) {
	time_stamp_wrap();
	return ((int64_t) ts_upper*UINT32_MAX + (uint64_t) ts_getlower()) / TICKS_PER_MICROSECOND;
}