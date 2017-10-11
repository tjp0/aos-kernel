#pragma once
#include <libcoro.h>
#include <stdio.h>
#include <sys/panic.h>
#undef kassert
#ifdef NDEBUG
#define kassert(x) ((void)0)
#else
static inline void __kassert_fail(const char* expr, const char* file, int line,
								  const char* func) {
	fprintf(stderr, "%u Assertion failed: (%s: %s: %d):%s\n",
			current_coro_num(), file, func, line, expr);
	fflush(NULL);
	sel4_abort();
}

#define kassert(x) \
	((x) ? ((void)0) : (__kassert_fail(#x, __FILE__, __LINE__, __func__)))
#endif
