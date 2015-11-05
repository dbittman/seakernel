#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/fs/initrd.h>
#include <sea/fs/inode.h>
#include <sea/boot/multiboot.h>
#include <sea/fs/ramfs.h>
#include <sea/vsprintf.h>

addr_t initrd_location=0;
addr_t initrd_end=0;

addr_t initrd_start_page, initrd_end_page;

void fs_initrd_load(struct multiboot *mb)
{
	if(mb->mods_count > 0)
	{
		initrd_location = *(uint32_t*)(mb->mods_addr + MEMMAP_KERNEL_START);
		initrd_end = *(uint32_t*)(mb->mods_addr+4 + MEMMAP_KERNEL_START);
		initrd_start_page = initrd_location & ~(PAGE_SIZE-1);
		initrd_end_page = (initrd_end & ~(PAGE_SIZE-1)) + PAGE_SIZE;
		/* Place the start of temporary usable memory to the end of the initrd */
		return;
	}
	not_found:
	kprintf("[initrd]: Couldn't find an initial ramdisk.\n[initrd]: No known way to start the system.\n");
	panic(0, "no initrd");
}

void ramfs_point_to_data(struct inode *node, void *data, size_t len);
void fs_initrd_parse(void)
{
	struct ustar_header *uh = (struct ustar_header *)(initrd_location + PHYS_PAGE_MAP);
	int err;
	while((addr_t)uh < initrd_end + PHYS_PAGE_MAP) {
		if(!*uh->name)
			break;
		/* tar is retarded. It stores field data as octal encoded ASCII because reasons.
		 * All of which are stupid */
		size_t len = strtoint_oct(uh->size);
		size_t recordlen = (len + 511) & ~511;
		addr_t datastart = (addr_t)uh + 512;
		struct inode *q = 0;
		if(strncmp(uh->magic, "ustar", 5)) {
			break;
		}
		printk(1, "\t* Loading '%s': %d bytes...\n", 
				uh->name, len);
		switch(uh->typeflag[0]) {
			int r;
			case '2':
				printk(1, "\t\t- symlink -> %s\n", uh->linkname);
				if((r = sys_symlink(uh->linkname, uh->name))) {
					panic(0, "failed to create symbolic link: %s -> %s: %d", uh->name, uh->linkname, r);
				}
				break;
			case '5':
				uh->name[strlen(uh->name) - 1] = 0;
				q = fs_path_resolve_create(uh->name, 0, S_IFDIR | 0777, &err);
				break;
			case '0': case '7':
				q = fs_path_resolve_create(uh->name, 0, S_IFREG | 0777, &err);
				ramfs_point_to_data(q, (void *)datastart, len);
				//fs_inode_write(q, 0, len, (char *)datastart);
				break;
			default:
				panic(0, "initrd: unknown file type %c", uh->typeflag[0]);
		}

		if(!q && uh->typeflag[0] != '2')
			panic(0, "initrd: failed to create entry %s (%d)", uh->name, err);
		if(q)
			vfs_icache_put(q);

		uh = (struct ustar_header *)((addr_t)uh + 512 /* header length */ + recordlen);
	}
	fs_path_resolve_create("/dev", 0, S_IFDIR | 0777, &err);
	fs_path_resolve_create("/mnt", 0, S_IFDIR | 0777, &err);
}

