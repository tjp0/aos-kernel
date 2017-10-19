#pragma once
#include <sel4/sel4.h>

struct frame {
	seL4_ARM_Page cap;
};

/* Given an address in the frametable mapping area;
 * returns the frame struct that manages it */
struct frame* vaddr_to_frame(void* addr);

/* Given a frame struct, find's the frame's address in
 * the frametable mapping area */
void* frame_to_vaddr(struct frame* frame);

/* Allocates a new frame */
struct frame* frame_alloc(void);
/* Frees an allocated frame */
void frame_free(struct frame* frame);

/* Initializes the frametable (called at boot) */
void ft_initialize(void);
