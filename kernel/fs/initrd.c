#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/fs/initrd.h>
#include <sea/fs/inode.h>
#include <sea/boot/multiboot.h>
#include <sea/fs/ramfs.h>
#include <sea/vsprintf.h>

static int initrd_version=0;
static addr_t initrd_location=0;
static initrd_header_t *initrd_header;
static initrd_file_header_t *file_headers;

void fs_initrd_load(struct multiboot *mb)
{
	addr_t initrd_end;
	if(mb->mods_count > 0)
	{
		initrd_location = *((u32int*)(addr_t)mb->mods_addr);
		initrd_end = *(u32int*)((addr_t)mb->mods_addr+4);
		addr_t q = initrd_location;
		if(!(*(unsigned char *)(q) == 'I' 
				&& *(unsigned char *)(q+1) == 'R' 
				&& *(unsigned char *)(q+2) == 'D'))
			goto not_found;
		initrd_version = *(unsigned char *)(q+3);
		if(initrd_version != '2')
			goto not_found;
		initrd_location += 4;
		/* Place the start of temporary usable memory to the end of the initrd */
		placement = initrd_end;
		return;
	}
	not_found:
	kprintf("[initrd]: Couldn't find an initial ramdisk.\n[initrd]: No known way to start the system.\n");
	panic(0, "no initrd");
}

void fs_initrd_parse()
{
	unsigned int i;
	printk(5, "[vfs]: Processing initrd...");
	addr_t location = initrd_location;
	initrd_header = (initrd_header_t *)location;
	file_headers = (initrd_file_header_t *) (location+sizeof(initrd_header_t));
	struct inode *q;
	/* Temporarily set the FS indicators to the ramfs root so we can use the 
	 * VFS. This makes parsing directories much easier. */
	printk(1, "\n");
	int count=0, size=0;
	fs_resolve_path_create("/usr", 0, S_IFDIR | 0777, 0);
	fs_resolve_path_create("/usr/sbin", 0, S_IFDIR | 0777, 0);
	fs_resolve_path_create("/etc", 0, S_IFDIR | 0777, 0);
	fs_resolve_path_create("/dev", 0, S_IFDIR | 0777, 0);
	fs_resolve_path_create("/mnt", 0, S_IFDIR | 0777, 0);
	for (i = 0; i < initrd_header->nfiles; i++)
	{
		printk(1, "\t* Loading '%s': %d bytes...\n", 
				(char *)&file_headers[i].name, file_headers[i].length);
		file_headers[i].offset += location;
		char name[128];
		snprintf(name, 128, "/%s", (char *)&file_headers[i].name);
		q = fs_resolve_path_create(name, 0, S_IFREG | 0777, 0);
		if(q) {
			fs_inode_write(q, 0, file_headers[i].length, (char *)((addr_t)file_headers[i].offset));
			vfs_icache_put(q);
		}
		count++;
		size += file_headers[i].length / 1024;
	}
	printk(5, "\r[vfs]: Initrd loaded (%d files, %d KB: ok)\n", count, size);
	/* Reset the indicators to FS-less system */
}

