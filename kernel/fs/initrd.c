#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/fs/initrd.h>
#include <sea/fs/inode.h>
#include <sea/boot/multiboot.h>
#include <sea/fs/ramfs.h>
#include <sea/vsprintf.h>

static addr_t initrd_location=0;
static addr_t initrd_end=0;

void fs_initrd_load(struct multiboot *mb)
{
	if(mb->mods_count > 0)
	{
		initrd_location = *((u32int*)(addr_t)mb->mods_addr);
		initrd_end = *(u32int*)((addr_t)mb->mods_addr+4);
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
	struct ustar_header *uh = (struct ustar_header *)initrd_location;
	while((addr_t)uh < initrd_end) {
		/* tar is retarded. It stores field data as octal encoded ASCII because reasons.
		 * All of which are stupid */
		size_t len = strtoint_oct(uh->size);
		size_t recordlen = (len + 511) & ~511;
		addr_t datastart = (addr_t)uh + 512;
		struct inode *q = 0;
		int err;
		if(strncmp(uh->magic, "ustar", 5)) {
			break;
		}
		printk(1, "\t* Loading '%s': %d bytes...\n", 
				uh->name, len);
		switch(uh->typeflag[0]) {
			case '5':
				uh->name[strlen(uh->name) - 1] = 0;
				q = fs_path_resolve_create(uh->name, 0, S_IFDIR | 0777, &err);
				break;
			case '0': case '7':
				q = fs_path_resolve_create(uh->name, 0, S_IFREG | 0777, &err);
				fs_inode_write(q, 0, len, (char *)datastart);
				break;
			default:
				panic(0, "initrd: unknown file type %c", uh->typeflag[0]);
		}

		if(!q)
			panic(0, "initrd: failed to create entry %s (%d)", uh->name, err);
		vfs_icache_put(q);

		uh = (struct ustar_header *)((addr_t)uh + 512 /* header length */ + recordlen);
	}

	fs_path_resolve_create("/dev", 0, S_IFDIR | 0777, 0);
	fs_path_resolve_create("/mnt", 0, S_IFDIR | 0777, 0);
}

