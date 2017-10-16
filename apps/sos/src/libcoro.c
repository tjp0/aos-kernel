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

/* Ideally each coroutine struct should be located
 * in it's own mapped area for debugging in the event
 * of an overflow */
struct coroutine {
	jmp_buf context;
	struct coroutine* next;
	int counter;
} first;

struct coroutine* idle = NULL;
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
int current_coro_num(void) { return current->counter; }

int resumable(struct coroutine* coro) { return (coro && coro->next == NULL); }
/* Used for argument passing between routines */
static void* volatile pass_arg;

static void* pass(struct coroutine* me, void* arg) {
	kassert(me != NULL);
	kassert(current != NULL);
	pass_arg = arg;
	trace(5);
	if (!setjmp(me->context)) {
		trace(5);
		dprintf(5, "Jumping from stack: %d\n", me->counter);
		longjmp(current->context, 1);
		trace(5);
	}
	dprintf(5, "Jumped to stack: %d\n", me->counter);
	trace(5);
	return pass_arg;
}

void* resume(struct coroutine* c, void* arg) {
	if (!resumable(c)) {
		printf("Stack %d is busted\n", c->counter);
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
	dprintf(5, "Resuming coro: %p\n", self);
	void* retfun = func(pass_arg);
	dprintf(5, "Coro going idle: %p\n", self);

	pass_arg = retfun;
	push(&idle, pop(&current));
	longjmp(current->context, 1);

	__builtin_unreachable();
}

struct coroutine* coroutine(void* function(void* arg)) {
	static int counter = 1;
	trace(5);
	struct coroutine* ret_co = NULL;
	if (idle == NULL) {
		dprintf(2, "Allocating new coroutine: %d\n", counter);
		void* stack =
			(void*)syscall_mmap(&sos_process, STACK_SIZE,
								PAGE_WRITABLE | PAGE_READABLE | PAGE_PINNED);
		trace(5);
		dprintf(5, "Stack is at %p\n", stack);
		if (stack == NULL) {
			trace(5);
			return NULL;
		}
		trace(5);
		ret_co = (struct coroutine*)ALIGN_DOWN(
			(uintptr_t)stack + STACK_SIZE - sizeof(struct coroutine),
			STACK_ALIGNMENT);
		ret_co->counter = counter;
		counter++;
	} else {
		trace(5);
		ret_co = pop(&idle);
	}
	if (ret_co == NULL) {
		return NULL;
	}
	struct coarg c;
	c.coro = ret_co;
	c.func = function;
	trace(5);
	utils_run_on_stack(ret_co, coroutine_run, &c);
	trace(5);
	dprintf(5, "Returning new coroutine: %p\n", ret_co);
	return ret_co;
}
