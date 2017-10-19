#include "libcoro.h"
#include <process.h>
#include <setjmp.h>
#include <stdlib.h>
#include <syscall/syscall_memory.h>
#include <utils/arith.h>
#include <utils/stack.h>
#include <vm.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>
#define STACK_ALIGNMENT 8 /* Stacks must be aligned to 8 bytes */
#define STACK_SIZE 4096 * 3

#pragma GCC push_options
#pragma GCC optimize("O0")

#define DEBUG_VAL 0x132193
/* Ideally each coroutine struct should be located
 * in it's own mapped area for debugging in the event
 * of an overflow */
struct coroutine first = {
	.process = &sos_process, .idle = false, .debug = DEBUG_VAL};

struct coroutine* current = &first;

static void push(struct coroutine** list, struct coroutine* c) {
	c->next = *list;
	*list = c;
}

static struct coroutine* pop(struct coroutine** list) {
	struct coroutine* nxt = *list;
	*list = nxt->next;
	nxt->next = NULL;
	return nxt;
}

struct coroutine* current_coro(void) {
	return current;
}

int resumable(struct coroutine* coro) { kassert(coro->debug == DEBUG_VAL);return (coro && coro->next == NULL); }
/* Used for argument passing between routines */
static void* volatile pass_arg;

static void* pass(struct coroutine* me, void* arg) {
	kassert(me != NULL);
	kassert(current != NULL);
	kassert(me->debug == DEBUG_VAL);
	pass_arg = arg;
	trace(5);
	if (!setjmp(me->context)) {
		trace(5);
		dprintf(5, "Jumping from stack: %s\n", me->process->name);
		longjmp(current->context, 1);
		trace(5);
	}
	kassert(me->debug == DEBUG_VAL);
	kassert(me->process != NULL);
	dprintf(5, "Jumped to stack: %s\n", me->process->name);
	trace(5);
	return pass_arg;
}

void* resume(struct coroutine* c, void* arg) {
	if (!resumable(c)) {
		printf("Stack <%s:%u> is busted\n", c->process->name, c->process->pid);
		kassert(resumable(c));
	}
	trace(5);
	push(&current, c);
	return pass(c->next, arg);
}
void* yield(void* arg) {
	trace(5);
	return pass(pop(&current), arg);
}

/* A struct used as a standin for a tuple */
struct coarg {
	struct coroutine* coro;
	void* (*func)(void* arg);
};

static void* coroutine_run(void* arg) {
	trace(5);
	struct coarg* c = (struct coarg*)arg;
	struct coroutine* self = c->coro;
	void* (*func)(void* arg) = c->func;
	trace(5);
	if (setjmp(self->context) == 0) {
		trace(5);
		return NULL;
	}
	dprintf(5, "Resuming coro: %p to call func: %p\n", self, func);
	void* retfun = func(pass_arg);
	dprintf(5, "Coro going idle: %p, <%s:%u>\n", self, self->process->name,
			self->process->pid);
	pass_arg = retfun;
	pop(&current);
	self->idle = true;
	kassert(self->debug == DEBUG_VAL);
	longjmp(current->context, 1);

	__builtin_unreachable();
}
struct coroutine* coroutine_create(struct process* process) {
	kassert(process != NULL);
	void* stack = (void*)syscall_mmap(
		&sos_process, STACK_SIZE, PAGE_WRITABLE | PAGE_READABLE | PAGE_PINNED);
	if (stack == NULL) {
		return NULL;
	}

	region_node* node = find_region(sos_process.vspace.regions, (vaddr_t)stack);
	if (node == NULL) {
		printf("%p\n", stack);
		panic(
			"We called mmap, but our returned pointer isn't in a region ? WTF");
	}
	node->name = process->name;

	struct coroutine* ret_co = (struct coroutine*)ALIGN_DOWN(
		(uint32_t)stack + STACK_SIZE - sizeof(struct coroutine), 8);
	ret_co->debug = DEBUG_VAL;
	ret_co->process = process;
	ret_co->idle = true;
	dprintf(3, "Created new coro at %p assigned to <%s:%u>\n", ret_co,
			process->name, process->pid);
	return ret_co;
}
bool coro_idle(struct coroutine* coro) { kassert(coro->debug == DEBUG_VAL);return coro->idle; }
struct process* current_process(void) {
	return current->process;
}
void coroutine_free(struct coroutine* coro) {
	kassert(coro->idle == true);
	coro->debug = 0;
	syscall_munmap(&sos_process, (vaddr_t)coro);
}

struct coroutine* coroutine_start(struct coroutine* coro,
								  void* function(void* arg)) {
	struct coarg c;
	kassert(coro != NULL);
	kassert(coro->idle == true);
	kassert(coro->process != NULL);
	kassert(coro->debug == DEBUG_VAL);
	c.coro = coro;
	c.func = function;
	coro->idle = false;
	trace(5);
	utils_run_on_stack(coro, coroutine_run, &c);
	trace(5);
	dprintf(5, "Returning new coroutine: %p\n", coro);
	return coro;
}

#pragma GCC pop_options
