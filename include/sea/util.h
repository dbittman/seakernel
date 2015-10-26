#ifndef __SEA_UTIL_H
#define __SEA_UTIL_H

static inline uint32_t bithack_round_nearest_power2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

#endif

