#pragma once
#include <stdint.h>

inline void join32to64(uint32_t x1, uint32_t x2, uint64_t *y2) {
	*y2 = (uint64_t)x1 | ((uint64_t)x2 << 32);
}

inline void split64to32(uint64_t x, uint32_t *y1, uint32_t *y2) {
	*y1 = x & 0xffffffff;
	*y2 = x >> 32;
}
