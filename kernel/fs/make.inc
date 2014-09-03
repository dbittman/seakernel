KOBJS+= kernel/fs/close.o \
kernel/fs/devfs.o \
kernel/fs/fcntl.o \
kernel/fs/file.o \
kernel/fs/flock.o \
kernel/fs/initrd.o \
kernel/fs/mminode.o \
kernel/fs/mount.o \
kernel/fs/open.o \
kernel/fs/ramfs.o \
kernel/fs/read_write.o \
kernel/fs/socket.o \
kernel/fs/stat.o \
kernel/fs/sync.o \
kernel/fs/sys.o 

include kernel/fs/proc/make.inc
include kernel/fs/vfs/make.inc
