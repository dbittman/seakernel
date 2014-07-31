#ifndef __SEA_FS_INITRD_H
#define __SEA_FS_INITRD_H

#include <sea/boot/multiboot.h>
#include <sea/types.h>

typedef struct __attribute__((packed))
{
	u32int nfiles;
} initrd_header_t;

typedef struct __attribute__((packed))
{
	u8int magic;
	s8int name[256];
	char pad[3];
	u32int offset;
	u32int length; 
} initrd_file_header_t;

void fs_initrd_load(struct multiboot *mb);
void fs_initrd_parse();

#endif
