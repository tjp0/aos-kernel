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
#include <serial/serial.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char debug_buf[1024];
void plogf(const char* msg, ...) {
	va_list alist;
	va_start(alist, msg);
	//	if (global_debug_serial) {
	//		vsprintf(debug_buf, msg, alist);
	//		serial_send(global_debug_serial, debug_buf, strlen(debug_buf));
	//	}
	vprintf(msg, alist);
	va_end(alist);
}
void _tracer(const char* file, const int line, const char* function) {
	printf("trace: <%s>:%u | %s\n", function, line, file);
}
