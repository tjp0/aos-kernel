/*
 * binary heap (beep) implementation 
 * by Jordan
 */

#include <stdio.h>
#include <stdlib.h>

#include "beep.h"

#define DEBUG 0

/* PUBLIC FUNCTIONS*/

/*
 * Given num_nodes, return the amount
 * of memory needed to malloc for the 
 * structure 
 * * * * * * * * * * * * * * * * * * */
unsigned int get_beep_size_for_n_nodes(unsigned int num_nodes){
	return sizeof(beep_struct) + sizeof(node_struct)*num_nodes;
}

/*
 * Make a new beep with a given memory segemnt
 * of size 'size'
 * you malloc me some space, tell me how many
 * nodes I can fit and i'll handle things
 * * * * * * * * * * * * * * * * * * * * * * */
void make_beep(beep_struct *mem, unsigned int node_capacity){
	assert(mem != NULL);
	beep_struct *beep = (beep_struct *)mem;
	beep->node_capacity = node_capacity;
	beep->nodes_in_use = 0;
	beep->node_array = (node_struct *)(((char *)mem) + sizeof(beep_struct));
}


/*
 * fill a beep from a list of nodes
 * this assumes that you've pretty much just made this beep
 * and you want to populate it with the given things
 * * * * * * * * * * * * * * * * * * * * * * */
void fill_beep(
		beep_struct *beep,
		node_struct *node_array,
		unsigned int num_nodes){

}


int is_empty(beep_struct *beep){
	return beep->nodes_in_use == 0;
}

int is_full(beep_struct *beep){
	return beep->nodes_in_use == beep->node_capacity;
}

int num_nodes(beep_struct *beep){
	return beep->nodes_in_use;
}


/*
 * Pop the highest priority thing off the heap
 * highest priority means smallest number
 * * * * * * * * * * * * * * * * * * * * * * */
node_data pop(beep_struct *beep){
	assert(beep != NULL);
	if (beep->nodes_in_use <= 0){
		if (DEBUG){
			printf("BEEP IS EMPTY\n");
		}
		return -1;
	}
	node_data data = beep->node_array[0].data;
	percolate_down(beep, 1);
	return data;
}

node_data peek(beep_struct *beep){
	assert(beep != NULL);
	if (beep->nodes_in_use <= 0){
		if (DEBUG){
			printf("BEEP IS EMPTY\n");
		}
		return -1;
	}
	return beep->node_array[0].data;
}

/*
 * Push a new entry to the heap
 * * * * * * * * * * * * * * * * * * * * * * */
int push(beep_struct *beep, priority_type priority, node_data data){
	assert(beep != NULL);
	if (beep->nodes_in_use >= beep->node_capacity){
		if (DEBUG){
			printf("CAN'T FIT ANY MORE NODES\n");
			printf("in use == %d, cap = %d\n", beep->nodes_in_use, beep->node_capacity);
		}
		return -1;
	}
	// We now have another node so increment our count
	beep->nodes_in_use += 1;
	// the hole starts at the end and percolates up
	unsigned int hole = beep->nodes_in_use;
	// percolate up
	while (hole > 1 && priority < beep->node_array[hole/2 - 1].priority){
		// move the parent node down the tree
		set_node_from_node(
			&(beep->node_array[hole - 1]),
			&(beep->node_array[hole/2 - 1])
		);
		// Dividing by 2 moves us up the tree
		hole = hole >> 1; 
	}
	// set where the hole ended up as the new data item
	set_node_from_data(
		&(beep->node_array[hole - 1]),
		priority,
		data
	);
	return data;
}


void print_beep(beep_struct *beep){
	assert(beep != NULL);
	int level = 1;
	int index = 0;
	printf("----------------------\n");
	while (level <= beep->nodes_in_use*2){
		int starting_index = index;
		while (index < level + starting_index && index < beep->nodes_in_use) {
			printf("%d ", beep->node_array[index].priority);
			index += 1;
		}
		printf("\n");
		level *= 2;
	}
	printf("#######################\n");
}

// make it go from a position

void delete_pos(beep_struct *beep, unsigned int pos){
	percolate_down(beep, pos);
}


// Return 0 on success
void delete_element(beep_struct *beep, node_data data){
	int i = 0;
	for (i = 0; i < beep->nodes_in_use; ++i) {
		if (beep->node_array[i].data == data){
			delete_pos(beep, i+1);
			return 0;
		}
	}
	if (DEBUG){
		printf("Didn't find %p\n", data);
		print_beep(beep);
	}
	assert(0); // Raise an error that we didn't find our element
}
void update_size(beep_struct *beep, unsigned int new_capacity){
	// This assumes you've like realloced or someting
	beep->node_capacity = new_capacity;
	beep->node_array = (node_struct *)(((char *)beep) + sizeof(beep_struct));
}

int node_capacity(beep_struct *beep){
	return beep->node_capacity;
}

/* PRIVATE FUNCTIONS*/
void percolate_down(beep_struct *beep, unsigned int pos){
	assert(beep != NULL);
	// Grab the last element
	unsigned int last_pos = beep->nodes_in_use;

	priority_type percolate_pri = beep->node_array[last_pos-1].priority;
	unsigned int percolate_dat = beep->node_array[last_pos-1].data;

	// last_pos -= 1;

	unsigned int cur_pos = pos;

	unsigned int n1_pos = 0;
	priority_type n1_pri = 0;

	unsigned int n2_pos = 0;
	priority_type n2_pri = 0;

	unsigned int next_pos = 0;
	priority_type next_pri = 0;



	while(1){

		if(DEBUG){
			printf("cur_pos: %d\n",cur_pos);
		}
		n1_pos = cur_pos * 2;

		// if there are no children, then you're done
		if (n1_pos > last_pos){
			break;
		}

		n2_pos = cur_pos * 2 + 1;

		// if n1 is the last node
		n1_pri = beep->node_array[n1_pos-1].priority;
		if (n2_pos > last_pos){
			if(DEBUG){
				printf("last one\n");
			}
			next_pos = n1_pos;
			next_pri = n1_pri;
		}else{
			// you have 2 children
			n2_pri = beep->node_array[n2_pos-1].priority;
			if (n1_pri < n2_pri){
				if(DEBUG){
					printf("smaller %d < %d\n",n1_pri, n2_pri);
				}
				next_pos = n1_pos;
				next_pri = n1_pri;
			}else{
				if(DEBUG){
					printf("bigger %d > %d\n",n1_pri, n2_pri);
				}
				next_pos = n2_pos;
				next_pri = n2_pri;
			}
		}

		if (next_pri < percolate_pri){
			beep->node_array[cur_pos-1] = beep->node_array[next_pos-1];
			set_node_from_node(
				&(beep->node_array[cur_pos-1]),
				&(beep->node_array[next_pos-1])
			);
			cur_pos = next_pos;
		}else{
			break;
		}
	}
	beep->node_array[cur_pos-1].priority = percolate_pri;
	beep->node_array[cur_pos-1].data = percolate_dat;
	beep->nodes_in_use -= 1;
	return ;
}

void set_node_from_node(node_struct *n1, node_struct *n2){
	n1->data = n2->data;
	n1->priority = n2->priority;
}

void set_node_from_data(
	node_struct *node,
	priority_type priority,
	node_data data){
	assert(node != NULL);

	node->data = data;
	node->priority = priority;
}
