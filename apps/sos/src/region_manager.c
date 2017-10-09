/*
 * Region manager for memory
 * AOS 2017
 * - Thomas 'n' Jordan
 * * * * * * * * * * * * * */

#include "region_manager.h"
#include <sel4/sel4.h>
#include <stdio.h>
#include <stdlib.h>
#include <vm.h>
#include <vmem_layout.h>

#define verbose 5
#include <sys/debug.h>

/* Leave a 4K page minimum between expanding regions */
#define REGION_SAFETY PAGE_SIZE_4K

// might want to memset memory to 0 but don't think it's needed
void default_load_page(region_node* reg, struct vspace* vspace, vaddr_t vaddr) {
	return;
}
void default_clean(region_node* reg) { return; }

static int create_initial_regions(region_list* reg_list) {
	region_node* ipc_buffer =
		add_region(reg_list, PROCESS_TOP, PAGE_SIZE_4K,
				   PAGE_READABLE | PAGE_WRITABLE | PAGE_PINNED, 0, 0, 0);
	if (ipc_buffer == NULL) {
		return REGION_FAIL;
	}

	region_node* stack =
		add_region(reg_list, PROCESS_TOP - PAGE_SIZE_4K, PAGE_SIZE_4K,
				   PAGE_READABLE | PAGE_WRITABLE, 0, 0, 0);
	if (stack == NULL) {
		return REGION_FAIL;
	}
	ipc_buffer->name = "IPC";
	stack->name = "STACK";
	reg_list->ipc_buffer = ipc_buffer;
	reg_list->stack = stack;

	return REGION_GOOD;
}

void region_list_destroy(region_list* reg_list) {
	assert(reg_list != NULL);
	region_node* node = reg_list->start;
	while (node) {
		region_node* next = node->next;
		free(node);
		node = next;
	}
	free(reg_list);
}

int init_region_list(region_list** reg_list) {
	region_list* list = malloc(sizeof(region_list));
	if (list == NULL) {
		return REGION_FAIL;
	};

	list->start = NULL;
	list->heap = NULL;
	int err = create_initial_regions(list);

	if (err == REGION_FAIL) {
		region_list_destroy(list);
		return REGION_FAIL;
	} else {
		*reg_list = list;
		return REGION_GOOD;
	}
}

/* Finds the place in the sorted linked list to insert a new chunk at vaddr */
static region_node* findspot(region_node* start, vaddr_t vaddr) {
	assert(start != NULL);

	while (start->next != NULL) {
		if (start->next->vaddr > vaddr) return start;

		start = start->next;
	}

	return start;
}

region_node* make_region_node(vaddr_t addr, unsigned int size,
							  unsigned int perm, void* load_page,
							  void* clean_page, void* data) {
	region_node* new_node = malloc(sizeof(region_node));
	if (new_node == NULL) {
		return NULL;
	}
	new_node->next = NULL;
	new_node->prev = NULL;
	new_node->vaddr = addr;
	new_node->size = size;
	new_node->perm = perm;
	new_node->name = "ELF";

	if (load_page == NULL) {
		new_node->load_page = default_load_page;
	} else {
		new_node->load_page = load_page;
	}

	if (clean_page == NULL) {
		new_node->clean = default_clean;
	} else {
		new_node->clean = clean_page;
	}

	new_node->data = data;

	return new_node;
}

region_node* add_region(region_list* reg_list, vaddr_t addr, unsigned int size,
						unsigned int perm, void* load_page, void* clean_page,
						void* data) {
	assert(IS_ALIGNED_4K(addr));
	assert(IS_ALIGNED_4K(size));
	assert(reg_list != NULL);

	region_node* cur = reg_list->start;

	region_node* new_node =
		make_region_node(addr, size, perm, load_page, clean_page, data);
	if (new_node == NULL) {
		return NULL;
	}

	if (!cur) {
		reg_list->start = new_node;
	} else if (cur->vaddr > addr) {
		region_node* prev = reg_list->start;
		new_node->next = prev;
		prev->prev = new_node;
		reg_list->start = new_node;
	} else {
		region_node* prev = findspot(reg_list->start, addr);
		new_node->next = prev->next;
		new_node->prev = prev;
		if (new_node->next) {
			new_node->next->prev = new_node;
		}
		prev->next = new_node;

		// kassert(prev->next->prev == prev);
		// kassert(prev->prev->next == prev);
	}

	return new_node;
}

region_node* find_region(region_list* list, vaddr_t addr) {
	region_node* start = list->start;

	while (start != NULL) {
		if (start->vaddr > addr) return NULL;

		if (start->vaddr + start->size >= addr) return start;

		start = start->next;
	}
	return NULL;
}

// Probably to expand the stack
int expand_left(region_node* node, vaddr_t address) {
	address = PAGE_ALIGN_4K(address);

	region_node* neighbor = node->prev;

	if (address < neighbor->vaddr + neighbor->size + REGION_SAFETY) {
		return REGION_FAIL;
	}

	node->vaddr -= address;
	return REGION_GOOD;
}

// Probably to expand the heap
int expand_right(region_node* node, uint32_t size) {
	assert(IS_ALIGNED_4K(size));
	region_node* neighbor = node->next;

	if (node->vaddr + node->size + size > neighbor->vaddr - REGION_SAFETY) {
		return REGION_FAIL;
	}
	node->size = node->size + size;
	return REGION_GOOD;
}

int in_stack_region(region_node* node, vaddr_t vaddr) {
	return (vaddr > node->prev->vaddr + node->prev->size + REGION_SAFETY);
}

/* Call this after the elf has been loaded to place the heap effectively */
region_node* create_heap(region_list* region_list) {
	region_node* stack = region_list->stack;
	assert(stack != NULL);
	region_node* prev = stack->prev;
	assert(prev != NULL);

	vaddr_t base_addr = prev->vaddr + prev->size + REGION_SAFETY;

	region_node* heap = add_region(region_list, base_addr, 0,
								   PAGE_READABLE | PAGE_WRITABLE, 0, 0, 0);

	if (heap == NULL) {
		return NULL;
	}
	region_list->heap = heap;
	heap->name = "HEAP";
	return heap;
}

void regions_print(region_list* regions) {
	region_node* cur = regions->start;
	// printf("----------------------\n");
	while (cur) {
		printf("0x%08x -> 0x%08x: perm: %08u, | %s", cur->vaddr,
			   cur->vaddr + cur->size, cur->perm, cur->name);
		if (cur->perm & PAGE_WRITABLE) {
			printf(" WRITABLE");
		}
		if (cur->perm & PAGE_READABLE) {
			printf(" READABLE");
		}
		if (cur->perm & PAGE_EXECUTABLE) {
			printf(" EXECUTABLE");
		}
		if (cur->perm & PAGE_PINNED) {
			printf(" PINNED");
		}
		printf("\n");

		// if (cur->prev && cur->prev->vaddr) {
		// 	printf("0x%08x -> 0x%08x: perm: %08u, | %s :0x%08x\n", cur->vaddr,
		// 		   cur->vaddr + cur->size, cur->perm, cur->name,
		// 		   cur->prev->vaddr);
		// } else {
		// 	printf("0x%08x -> 0x%08x: perm: %08u, | %s ;0x%08x\n", cur->vaddr,
		// 		   cur->vaddr + cur->size, cur->perm, cur->name, 0);
		// }
		cur = cur->next;
	}
	// printf("########################\n");
}
