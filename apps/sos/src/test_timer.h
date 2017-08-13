#pragma once

#define BUILD_NUM 30

void simple_timer_callback(uint32_t id, void *data);

void death(uint32_t id, void *data);

void nest(uint32_t id, void *data);

void exponential(uint32_t id, void *data);



void test_timers(void);


void simple(void);

void count_down(void);

void build_n_kill(void);

void chain(void);

void test_exponential(void);
