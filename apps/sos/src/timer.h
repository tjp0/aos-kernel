#pragma once
#include <clock/clock.h>

void timer_init(void);
int timer_interrupt(void);
void epit2_sleepto(timestamp_t time);