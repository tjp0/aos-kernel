
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <elf/elf.h>
#include <serial/serial.h>

#include <clock/clock.h>
#include <autoconf.h>
#include <sys/debug.h>


#include "test_timer.h"


void simple_timer_callback(uint32_t id, void *data) {
    printf("I am Timer %d, who was created with %p\n", id, data);
    printf("It has been %lld milliseconds since boot\n",time_stamp());
    // register_timer(1000,&simple_timer_callback,NULL);
}


void death(uint32_t id, void *data) {
    printf("I am a DEAD!!!!!!!!!! Timer %d, who was created with %p\n", id, data);
    assert(0);
    // printf("It has been %lld milliseconds since boot\n",time_stamp()/1000);
    // register_timer(1000,&simple_timer_callback,NULL);
}


void nest(uint32_t id, void *data) {
    printf("I am nested Timer %d, who was created with %p\n", id, data);
    if (data) {
        register_timer(1000,&nest,data-1);
    }
}


void exponential(uint32_t id, void *data) {
    int i = 0;
    for (i = 0; i < data; ++i){
        printf("-------");
    }
    printf("Timer[%d] -: %p\n", id, data);

    if (data>0){
        register_timer(1000 + 100,&exponential, (void *)(((int)data) - 1));
        register_timer(1000,&exponential, (void *)(((int)data) - 1));
    }
}


void test_timers(void){
    simple();
    // count_down();
    // chain();
    // build_n_kill();
    // test_exponential();
}

void test_exponential(void){
    void *data = 5;
    uint32_t delay = 1000;
    printf("TESTING exponential\n");

    uint32_t id1 = register_timer(delay,&simple_timer_callback, data);
    printf("I just registered timer %d with data:%p delay:%u\n", id1, data, delay);

    delay = 2000;
    uint32_t id2 = register_timer(delay,&exponential, data);
    printf("I just registered timer %d with data:%p delay:%u\n", id2, data, delay);

    printf("TESTING exponential %u %u\n", id1, id2);

}


void simple(void){
    // Count down 10 seconds in 100 increments
    void *data = 0;
    uint32_t id;
    uint32_t delay = 1000;

    delay = 1000;
    id = register_timer(delay, &simple_timer_callback, data);
    printf("I just registered timer %d with data:%p delay:%u\n", id, data, delay);

    // delay = 2000;
    // id = register_timer(delay, &simple_timer_callback, data);
    // printf("I just registered timer %d with data:%p delay:%u\n", id, data, delay);

    // delay = 3000;
    // id = register_timer(delay, &simple_timer_callback, data);
    // printf("I just registered timer %d with data:%p delay:%u\n", id, data, delay);

    // delay = 4000;
    // id = register_timer(delay, &simple_timer_callback, data);
    // printf("I just registered timer %d with data:%p delay:%u\n", id, data, delay);
}



void count_down(void){
    // Count down 10 seconds in 100 increments
    void *data = 0;
    int i = 0;
    uint32_t id;
    int max_counters = 5;
    for (i = 0; i < max_counters; ++i) {
        data = (void *)i;
        uint32_t delay = 5000;// 1000*i + 1000; //1000*max_counters +

        id = register_timer(delay,&simple_timer_callback, data);
        printf("I just registered timer %d with data:%p delay:%u\n", id, data, delay);
    }
}
void build_n_kill(void){
    // Count down 10 seconds in 100 increments
    void *data = 0;
    int i = 0;
    uint32_t id;
    int max_counters = BUILD_NUM;
    unsigned int ids[BUILD_NUM];
    for (i = 0; i < max_counters; ++i) {
        data = (void *)i;
        id = register_timer(1000,&death, data);
        printf("I just registered a death timer %d with %p\n", id, data);
        ids[i] = id;
    }
    for (i = 0; i < max_counters; ++i) {
        unsigned int res = remove_timer(ids[i]);
        printf("res = %u\n", res);
    }
    printf("killed all the timers\n");
}

void chain(void){
    // Start a list of nested timers
    uint32_t id = register_timer(1000,&nest, (void *)100);
    printf("I just registered a nest timer %d\n", id);
}

