echo -n "Loading modules..."
export PATH=$PATH:/:.:/usr/sbin
MODS="keyboard pci partitions ata ext2"
err=0
for i in $MODS; do
	if ! modprobe -d / $i ; then
		err=1
	fi
done
if [[ $err == 0 ]]; then
	echo " ok"
else
	echo " FAIL"
fi

if [[ "$1" = "/" ]]; then
	# the user instructed us to use the initrd as root. Just start a shell
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
