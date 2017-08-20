/*
 * Region manager for memory
 * AOS 2017
 * - Thomas 'n' Jordan
 * * * * * * * * * * * * * */

#include "region_manager.h"
#include <stdio.h>
#include <stdlib.h>

static int compare_nodes(void* data1, void* data2);
static int print_node(void* data);
/* malloc the thing and get things set up*/
int init_region_list(region_list** reg_list) {
	*reg_list = (region_list*)malloc(sizeof(struct _region_list));

	if (!*reg_list) {
		printf("MALLOC FAILED\n");
		return REGION_FAIL;
	}
	(*reg_list)->regions = (list_t*)malloc(sizeof(list_t));
	list_init((*reg_list)->regions);

	return REGION_GOOD;
}

/* malloc the thing and get things set up*/
region_node* make_region_node(void* addr, unsigned int size,
							  unsigned int perm) {
	region_node* region = (region_node*)malloc(sizeof(struct _region_node));

	region->addr = addr;
	region->size = size;
	region->perm = perm;

	return region;
}

/* Returns 0 on success */
int add_region(region_list* reg_list, void* addr, unsigned int size,
			   unsigned int perm) {
	assert(IS_ALIGNED_4K(addr));
	// If it overlaps then that's not ok
	if (does_region_overlap(reg_list, addr, size) == REGION_FAIL) {
		return REGION_FAIL;
	}
	// Make a new region
	region_node* new_region = make_region_node(addr, size, perm);
	// append it to the list
	list_append(reg_list->regions, new_region);
	return REGION_GOOD;
}

/* Returns 0 if it's ok, -1 if that region is occupied */
int does_region_overlap(region_list* reg_list, void* addr, unsigned int size) {
	// test all the regions
	struct list_node* n = reg_list->regions->head;
	while (n != NULL) {
		region_node* node = n->data;
		// x1 <= y2 AND y1 <= x2  --> overlap
		if (addr <= node->addr + node->size && node->addr <= addr + size) {
			return REGION_FAIL;
		}
		n = n->next;
	}
	return REGION_GOOD;
}

/* Remove the region corresponding to addr
 * Returns -1 if that region couldn't be found
 *			0 if it worked
 * * * * * * * * * * * * * * * * * * * * * * * */
int remove_region(region_list* reg_list, void* addr) {
	// find the node first... grr not efficient
	struct list_node* n = reg_list->regions->head;
	while (n != NULL) {
		region_node* node = n->data;
		if (node->addr == addr) {
			break;
		}
		n = n->next;
	}
	int res = list_remove(reg_list->regions, n->data, compare_nodes);
	printf("%d\n", res);
	return REGION_GOOD;
}

static int compare_nodes(void* data1, void* data2) {
	void* addr1 = ((region_node*)(data1))->addr;
	void* addr2 = ((region_node*)(data2))->addr;
	int   val   = (addr1 != addr2);
	return val;
}

region_node* find_region(region_list* reg_list, void* addr) {
	struct list_node* n = reg_list->regions->head;
	while (n != NULL) {
		region_node* node = n->data;
		// x1 <= y2 AND y1 <= x2  --> overlap
		if (addr >= node->addr && addr <= node->addr + node->size) {
			return node;
		}
		n = n->next;
	}
	return REGION_FAIL;
}

void print_list(region_list* reg_list) {
	list_foreach(reg_list->regions, print_node);
}

static int print_node(void* data) {
	region_node* node = (region_node*)data;
	printf("node && %p\n", data);
	printf("addr %p\n", node->addr);
	printf("size %d\n", node->size);
	return 0;
}
