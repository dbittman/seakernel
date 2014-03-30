/* crc32.c: provides a kernel API for calculating crc32.
 * Copyright (C) 2013 Daniel Bittman
 */
#include <kernel.h>
#include <modules/crc32.h>
#include <sea/loader/symbol.h>
uint32_t crc32_table[256];

uint32_t calculate_crc32(uint32_t crc, const unsigned char *buf, size_t len)
{
	unsigned char tmp;
	const unsigned char *p;
	crc = ~crc;
	for (p = buf; p < (buf + len); p++) {
		tmp = *p;
		crc = (crc >> 8) ^ crc32_table[(crc & 0xff) ^ tmp];
	}
	return ~crc;
}

void init_crc32_table()
{
	for(int i=0;i<256;i++)
	{
		uint32_t remainder = i;
		for(int j=0;j<8;j++)
		{
			if(remainder & 0x1) {
				remainder = remainder >> 1;
				remainder ^= 0xedb88320;
			} else {
				remainder = remainder >> 1;
			}
			crc32_table[i] = remainder;
		}
	}
}

int module_install()
{
	init_crc32_table();
	loader_add_kernel_symbol(calculate_crc32);
	return 0;
}

int module_tm_exit()
{
	loader_remove_kernel_symbol("calculate_crc32");
	return 0;
}
