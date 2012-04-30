#include <kernel.h>
#include <fs.h>
#include <sys/stat.h>
#include "iso9660.h"

struct iso9660DirRecord *get_root_dir(iso_fs_t *fs)
{
	return (struct iso9660DirRecord *)fs->pvd->RootDirRecord;
}

int to_upper(char *str)
{
	while(str && *str)
	{
		if(*str >= 'a' && *str <= 'z')
			*str -= 32;
		str++;
	}
	return 0;
}

int to_lower(char *str)
{
	while(str && *str)
	{
		if(*str >= 'A' && *str <= 'Z')
			*str += 32;
		str++;
	}
	return 0;
}

int search_dir_rec(iso_fs_t *fs, struct iso9660DirRecord *dir, char *name, struct iso9660DirRecord *ret)
{
	char buf[2048];
	char search[strlen(name)+4];
	memset(search, 0, strlen(name)+4);
	strcpy(search, name);
	to_upper(search);
	unsigned int block=0, total=0;
	struct iso9660DirRecord *record;
	while(total < dir->DataLen_LE)
	{
		int res = iso9660_read_file(fs, dir, buf, block*2048, 2048);
		int off=0;
		while(off < res)
		{
			record = (struct iso9660DirRecord *)((char *)buf+off);
			if(!record->RecLen) {
				goto end;
			}
			/* Process the record */
			char flag=0;
			if(record->FileFlags & 0x2)
			{
				flag = (record->FileIdentLen == strlen((const char *)search) && !strncmp((const char *)record->FileIdent, (const char *)search, record->FileIdentLen));
			} else
			{
				record->FileIdentLen -= 2;
				record->FileIdent[record->FileIdentLen]=0;
				if(record->FileIdent[record->FileIdentLen-1] == '.')
					record->FileIdent[--record->FileIdentLen]=0;
				flag = (record->FileIdentLen == strlen(search) && !strncmp((const char *)record->FileIdent, (const char *)search, record->FileIdentLen));
			}
			if(flag)
			{
				/* Found! */
				memcpy(ret, record, sizeof(*ret));
				return 0;
			}
			off += record->RecLen;
		}
		if(res < 2048)
			break;
		block++;
		total += res;
	}
	end:
	return -1;
}

int read_dir_rec(iso_fs_t *fs, struct iso9660DirRecord *dir, int n, struct iso9660DirRecord *ret, char *name)
{
	char buf[2048];
	unsigned int block=0, total=0;
	struct iso9660DirRecord *record;
	while(total < dir->DataLen_LE)
	{
		int res = iso9660_read_file(fs, dir, buf, block*2048, 2048);
		int off=0;
		while(off < res)
		{
			record = (struct iso9660DirRecord *)((char *)buf+off);
			if(!record->RecLen)
				goto end;
			/* Process the record */
			if(!(record->FileFlags & 0x2))
			{
				record->FileIdentLen -= 2;
				record->FileIdent[record->FileIdentLen]=0;
				if(record->FileIdent[record->FileIdentLen-1] == '.')
					record->FileIdent[--record->FileIdentLen]=0;
			}
			if(!n--)
			{
				memcpy(ret, record, sizeof(*ret));
				strncpy(name, (const char *)record->FileIdent, record->FileIdentLen < 128 ? record->FileIdentLen : 127);
				return 0;
			}
			off += record->RecLen;
		}
		if(res < 2048)
			break;
		block++;
		total += res;
	}
	end:
	return -1;
}
