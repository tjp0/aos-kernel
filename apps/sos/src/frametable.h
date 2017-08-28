#pragma once
#include <sel4/sel4.h>

// FRAME_RELEASED is the same as frame untyped but just for debug i need to call
// it somehting different
// garbage is to see if things get set to 0 or inuse
enum frame_status { GARBAGE, FRAME_INUSE, FRAME_UNTYPED, FRAME_FREE };

struct frame {
	enum frame_status status;
	seL4_ARM_Page cap;
};

struct frame* get_frame(void* addr);

void* frame_cell_to_vaddr(struct frame* frame_cell);

void* frame_alloc(void);

void ft_initialize(void);

void frame_test(void);
