echo "Loading modules..."
export PATH=$PATH:/:.:/usr/sbin
lmod /keyboard /pci /partitions /ata /ext2 /iso9660
fsck -p -T -C /dev/hda1
printf "Mounting filesystems: / "
mount /dev/hda1 /mnt
printf "dev "
mount -t devfs \* /mnt/dev
printf "proc "
mount -t procfs \* /mnt/proc
printf "tmp "
mount -t tmpfs \* /mnt/tmp
printf "done\n"
cp /sh /mnt/tmp/sh

chroot /mnt /tmp/sh /config/rc/boot
