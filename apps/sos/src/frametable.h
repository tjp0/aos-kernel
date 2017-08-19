#pragma once
#include <sel4/sel4.h>

enum frame_status { FRAME_INUSE, FRAME_UNTYPED, FRAME_FREE };

struct frame {
	enum frame_status status;
	seL4_ARM_Page cap;
};

struct frame* get_frame(void* addr);
void* frame_alloc(void);

seL4_Word ft_early_initialize(const seL4_BootInfo* bootinfo);
void ft_late_initialize(seL4_Word base);

void frame_test(void);