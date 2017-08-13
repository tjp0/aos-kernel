/*
 * Binary heap implementation
 * beep struct contains meta data about the heap
 * node struct contains a priority and a pointer/data element
 * by jordan
 * Licenced under the 'do whatever you want with it' Licence
*/


#include <stdint.h>
#include <assert.h>
typedef int32_t node_data;
typedef unsigned int priority_type;

typedef struct _node {
	priority_type priority;
	node_data data;
} node_struct;


typedef struct _beep {
	unsigned int node_capacity;
	unsigned int nodes_in_use;
	node_struct *node_array;
} beep_struct;


/* PUBLIC FUNCTIONS*/

/*
 * Given num_nodes, return the amount
 * of memory needed to malloc for the 
 * structure 
 * * * * * * * * * * * * * * * * * * */
unsigned int get_beep_size_for_n_nodes(unsigned int num_nodes);

/*
 * Make a new beep with a given memory segemnt
 * of size 'size'
 * you malloc me some space, tell me how many
 * nodes I can fit and i'll handle things
 * * * * * * * * * * * * * * * * * * * * * * */
void make_beep(beep_struct *mem, unsigned int node_capacity);

/*
 * fill a beep from a list of nodes
 * this assumes that you've pretty much just made this beep
 * and you want to populate it with the given things
 * * * * * * * * * * * * * * * * * * * * * * */
void fill_beep(
	beep_struct *beep,
	node_struct *node_array,
	unsigned int num_nodes
);

int is_full(beep_struct *beep);
int is_empty(beep_struct *beep);

// Return the number of nodes in the struct
int num_nodes(beep_struct *beep);

// Return the number of nodes in the struct
int node_capacity(beep_struct *beep);
/*
 * Pop the highest priority thing off the heap
 * highest priority means smallest number
 * * * * * * * * * * * * * * * * * * * * * * */
node_data pop(beep_struct *beep);
node_data peek(beep_struct *beep);

/*
 * Push a new entry to the heap
 * * * * * * * * * * * * * * * * * * * * * * */
int push(beep_struct *beep, priority_type priority, node_data data);


void delete_pos(beep_struct *beep, unsigned int pos);
void delete_element(beep_struct *beep, node_data data);
void update_size(beep_struct *beep, unsigned int new_capacity);


void print_beep(beep_struct *beep);

/* PRIVATE FUNCTIONS*/
void percolate_down(beep_struct *beep, unsigned int pos);
void set_node_from_node(node_struct *n1, node_struct *n2);
void set_node_from_data(
	node_struct *node,
	priority_type priority,
	node_data data
);
