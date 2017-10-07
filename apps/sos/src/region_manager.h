#pragma once
// #include "list.h"

#include <typedefs.h>
#include <utils/list.h>
#include <utils/page.h>

#define REGION_FAIL 0
#define REGION_GOOD 1

// #define IS_ALIGNED_4K(addr) IS_ALIGNED(addr, PAGE_BITS_4K)

typedef void (*load_page_func)(region_node* reg, struct vspace* vspace,
							   vaddr_t vaddr);
typedef void (*clean_func)(region_node* reg);

typedef struct _region_node {
	vaddr_t vaddr;
	uint32_t size;
	uint32_t perm;
	char* name;
	struct _region_node* next;
	struct _region_node* prev;
	load_page_func load_page;
	clean_page_func clean;
	void* data;
} region_node;

typedef struct _region_list {
	region_node* start;
	region_node* stack;
	region_node* ipc_buffer;
	region_node* heap;
} region_list;

/* malloc the thing and get things set up*/
int init_region_list(region_list** reg_list);

/* malloc the thing and get things set up*/
region_node* make_region_node(vaddr_t addr, unsigned int size,
							  unsigned int perm, load_page_func load_page,
							  clean_func clean, void* data);

/* Returns 0 on success */
region_node* add_region(region_list* reg_list, vaddr_t addr, unsigned int size,
						unsigned int perm, load_page_func load_page,
						clean_func clean, void* data);

void region_list_destroy(region_list* reg_list);

/* Remove the region corresponding to addr
 * Returns -1 if that region couldn't be found
 *			0 if it worked
 * * * * * * * * * * * * * * * * * * * * * * * */
int remove_region(region_list* reg_list, vaddr_t addr);

region_node* find_region(region_list* reg_list, vaddr_t addr);

void regions_print(region_list* reg_list);

// Probably to expand the stack
int expand_left(region_node* node, vaddr_t address);

// Probably to expand the heap
int expand_right(region_node* node, uint32_t size);

int in_stack_region(region_node* node, vaddr_t vaddr);

region_node* create_heap(region_list* region_list);
