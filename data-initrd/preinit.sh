echo "Loading modules..."
export PATH=$PATH:/:.:/usr/sbin
modprobe -d / /keyboard 
modprobe -d / /pci 
modprobe -d / /partitions 
modprobe -d / /ata 
modprobe -d / /ext2 
#modprobe -d / /iso9660
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
	chroot /mnt /bin/sh /etc/rc/boot
fi
