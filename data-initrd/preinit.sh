echo -n "Loading modules..."
export PATH=$PATH:/:.:/usr/sbin
MODS="shiv keyboard pci partitions psm ata ahci ext2"
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
	if ! mount $1 /mnt; then
		echo
		echo "Failed to mount root filesystem, falling back to initrd shell..."
		sh
	else
		/mnt/bin/umount /dev
		chroot /mnt /bin/sh -c <<EOF "
			printf "dev"
			mount -t devfs /dev/null /dev
			printf "tmp"
			mount -t tmpfs /dev/null /tmp
			. /etc/rc/boot; exit 0"
EOF
		if [ $? != 0 ] ; then
			printf "** chroot failed, dropping to initrd shell **\n"
			sh
			exit 1
		fi
	fi
fi

