#include <kernel.h>
#include <fs.h>
#include <sys/stat.h>
#include "iso9660.h"

int iso9660_read_file(iso_fs_t *fs, struct iso9660DirRecord *file, char *buffer, int offset, int length)
{
	if((unsigned)offset > file->DataLen_LE)
		return 0;
	if((unsigned)(offset + length) > file->DataLen_LE)
		length = file->DataLen_LE-offset;
	unsigned char buf[2048];
	unsigned block=file->ExtentLocation_LE + offset / 2048;
	unsigned int i=0;
	while(length)
	{
		iso_read_block(fs, block, buf);
		int start = offset % 2048;
		int len = 2048-start;
		if(len > length)
			len = length;
		memcpy(buffer + i, buf + start, len);
		i += len;
		length -= len;
		offset=0;
		block++;
	}
	return i;
}
