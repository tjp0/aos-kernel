#pragma once
#include <clock/clock.h>

void timer_init(void);
int timer_interrupt(void);
int epit2_sleepto(timestamp_t time);