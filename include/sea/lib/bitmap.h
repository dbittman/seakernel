#ifndef __SEA_LIB_BITMAP_H
#define __SEA_LIB_BITMAP_H

static inline void bitmap_assign(void *ptr, int bit, int val)
{
	int index = bit / 8;
	int offset = bit % 8;
	uint8_t *tmp = ptr;
	if(val)
		tmp[index] |= (1 << offset);
	else
		tmp[index] &= ~(1 << offset);
}

static inline int bitmap_test(void *ptr, int bit)
{
	int index = bit / 8;
	int offset = bit % 8;
	uint8_t *tmp = ptr;
	return (tmp[index] & (1 << offset));
}

static inline int bitmap_ffs(void *ptr, int numbits)
{
	uint8_t *tmp = ptr;
	for(int i=0;i<numbits;i++) {
		int index = i / 8;
		int offset = i % 8;
		if(tmp[index] & (1 << offset))
			return i;
	}
	return -1;
}

static inline int bitmap_ffr(void *ptr, int numbits)
{
	uint8_t *tmp = ptr;
	for(int i=0;i<numbits;i++) {
		int index = i / 8;
		int offset = i % 8;
		if(!(tmp[index] & (1 << offset)))
			return i;
	}
	return -1;
}

static inline int bitmap_ffr_start(void *ptr, int numbits, int start)
{
	uint8_t *tmp = ptr;
	for(int i=start;i<numbits;i++) {
		int index = i / 8;
		int offset = i % 8;
		if(!(tmp[index] & (1 << offset)))
			return i;
	}
	return -1;
}

#define bitmap_set(a,b) bitmap_assign(a,b,1)
#define bitmap_reset(a,b) bitmap_assign(a,b,0)

#endif

