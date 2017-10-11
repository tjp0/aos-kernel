/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <libcoro.h>
#include <stdio.h>
void plogf(const char* msg, ...);
#define _dprintf(v, col, args...)               \
	do {                                        \
		if ((v) < verbose) {                    \
			printf(col);                        \
			printf("%u: ", current_coro_num()); \
			plogf(args);                        \
			printf("\033[0;0m");                \
		}                                       \
	} while (0)

#define dprintf(v, ...) _dprintf(v, "\033[22;33m", __VA_ARGS__)

#define dprint_fault(a, b)               \
	{                                    \
		if (a < verbose) print_fault(b); \
	}

#define WARN(...) _dprintf(-1, "\033[1;31mWARNING: ", __VA_ARGS__)

#define NOT_IMPLEMENTED()                                                 \
	printf("\033[22;34m %s:%d -> %s not implemented\n\033[;0m", __FILE__, \
		   __LINE__, __func__);

void _tracer(const char* file, const int line, const char* function);
#define trace(v)                                   \
	do {                                           \
		if ((v) < verbose) {                       \
			_tracer(__FILE__, __LINE__, __func__); \
		}                                          \
	} while (0)
#endif /* _DEBUG_H_ */
