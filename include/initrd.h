#ifndef INITRD_H
#define INITRD_H

typedef struct
{
	u32int nfiles;
} initrd_header_t;

typedef struct
{
	u8int magic;
	s8int name[256];
	u32int offset;
	u32int length; 
} initrd_file_header_t;

extern addr_t initrd_location;

#endif
