echo "Loading modules..."
export PATH=$PATH:/:.:/usr/sbin
lmod /keyboard /pci /partitions /ata /ext2 /iso9660
if [[ "$1" = "/" ]]; then
	sh
else
	fsck -p -T -C $1
	printf "Mounting filesystems: / "
	mount $1 /mnt
	printf "dev "
	mount -t devfs \* /mnt/dev
	printf "proc "
	mount -t procfs \* /mnt/proc
	printf "tmp "
	mount -t tmpfs \* /mnt/tmp
	printf "done\n"
	chroot /mnt /bin/sh /config/rc/boot
fi
