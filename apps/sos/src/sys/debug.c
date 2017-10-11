/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "debug.h"
#include <globals.h>
#include <libcoro.h>
#include <serial/serial.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
void plogf(const char* msg, ...) {
	va_list alist;
	va_start(alist, msg);
	vprintf(msg, alist);
	va_end(alist);
}
void _tracer(const char* file, const int line, const char* function) {
	printf("%u trace: <%s>:%u | %s\n", current_coro_num(), function, line,
		   file);
}
