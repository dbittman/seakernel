echo -n "Loading modules..."
export PATH=$PATH:/:.:/usr/sbin
MODS="keyboard pci partitions ata sata ext2"
err=0
failed_mods=""
for i in $MODS; do
	if !  modprobe -d / $i ; then
		err=1
		failed_mods="$failed_mods $i"
	fi
	printf "."
done
if [[ $err == 0 ]]; then
	echo " ok"
else
	echo " FAIL"
fi

for i in $failed_mods; do
	echo "loading failed for module $i: see kernel log for details"
done

if [[ "$1" = "/" ]]; then
	# the user instructed us to use the initrd as root. Just start a shell
	sh
else
	fsck -p -T -C $1
	printf "Mounting filesystems: $1 -> / "
	mount $1 /mnt
	printf "dev "
	mount -t devfs \* /mnt/dev
	printf "proc "
	mount -t procfs \* /mnt/proc
	printf "tmp "
	mount -t tmpfs \* /mnt/tmp
	printf "done\n"
	if ! chroot /mnt /bin/sh /etc/rc/boot ; then
		printf "** chroot failed, dropping to initrd shell **\n"
		sh
	fi
fi
