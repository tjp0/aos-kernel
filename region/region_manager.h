
#include "list.h"

#define REGION_FAIL 0
#define REGION_GOOD 1

#define IS_ALIGNED_4K(addr) IS_ALIGNED(addr, PAGE_BITS_4K)
typedef struct _region_node {
	void* addr;
	unsigned int size;
	unsigned int perm;
} region_node;

typedef struct _region_list { list_t* regions; } region_list;

/* malloc the thing and get things set up*/
int init_region_list(region_list** reg_list);

/* malloc the thing and get things set up*/
region_node* make_region_node(void* addr, unsigned int size, unsigned int perm);

/* Returns 0 on success */
int add_region(region_list* reg_list, void* addr, unsigned int size,
			   unsigned int perm);

/* Returns 0 if it's ok, -1 if that region is occupied */
int does_region_overlap(region_list* reg_list, void* addr, unsigned int size);

region_node* find_region(region_list* reg_list, void* addr);

void print_list(region_list* reg_list);
