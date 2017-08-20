#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "region_manager.h"

void test_regions(void);

void test_regions(void) {
	region_list *reg_list;
	void *		 addr = 0;
	unsigned int size = 10;
	unsigned int perm = 0;

	init_region_list(&reg_list);

	int res;

	res = add_region(reg_list, addr, size, perm);
	print_list(reg_list);
	assert(res == REGION_GOOD);

	res = add_region(reg_list, addr, size, perm);
	print_list(reg_list);
	assert(res == REGION_FAIL);

	res = remove_region(reg_list, addr);
	print_list(reg_list);
	assert(res == REGION_GOOD);

	res = add_region(reg_list, addr, size, perm);
	print_list(reg_list);
	assert(res == REGION_GOOD);

	res = add_region(reg_list, addr, size, perm);
	print_list(reg_list);
	assert(res == REGION_FAIL);

	res = add_region(reg_list, addr, 1, perm);
	print_list(reg_list);
	assert(res == REGION_FAIL);

	res = add_region(reg_list, (void *)9, 1, perm);
	print_list(reg_list);
	assert(res == REGION_FAIL);

	res = add_region(reg_list, (void *)13, 1, perm);
	print_list(reg_list);
	assert(res == REGION_GOOD);

	res = add_region(reg_list, (void *)12, 1, perm);
	print_list(reg_list);
	assert(res == REGION_FAIL);

	res = add_region(reg_list, (void *)15, 5, perm);
	print_list(reg_list);
	assert(res == REGION_GOOD);

	res = remove_region(reg_list, (void *)13);
	print_list(reg_list);
	assert(res == REGION_GOOD);

	res = add_region(reg_list, (void *)12, 1, perm);
	print_list(reg_list);
	assert(res == REGION_GOOD);
}

int main(int argc, char const *argv[]) {
	test_regions();
	return EXIT_SUCCESS;
}
