# Makefile for SeaKernel...
# Please use include files to add to things...
# All C files to be compiled and linked into the kernel---ish
CC=i586-pc-seaos-gcc
LD=i586-pc-seaos-ld
# Flags for programs
CFLAGS_NO = -m32 -nostdlib -nostdinc -fno-builtin -ffreestanding \
          -I../include -Iinclude -I ../../include -I ../../../include \
          -D__KERNEL__ -D__DEBUG__ -std=c99 -Wall -Wextra \
           -Wformat-security -Wformat-nonliteral\
	  -Wno-strict-aliasing -Wshadow -Wpointer-arith -Wcast-align -Wno-unused \
	  -Wnested-externs -Waddress -Winline -Wno-long-long -mno-red-zone -fno-omit-frame-pointer

CFLAGS = $(CFLAGS_NO) -O3 

LDFLAGS=-T kernel/link.ld -m seaos_i386
ASFLAGS=-felf32
GASFLAGS=--32
include make.inc
RAMFILES=data-initrd/usr/sbin/fsck usr/sbin/fsck data-initrd/usr/sbin/fsck.ext2 usr/sbin/fsck.ext2 \
	 data-initrd/preinit.sh /preinit.sh data-initrd/etc/fstab etc/fstab \
	 data-initrd/bin/bash /sh data-initrd/bin/lmod /lmod data-initrd/bin/mount /mount data-initrd/bin/chroot /chroot \
	 data-initrd/bin/cp /cp

# This is all the objects to be compiled and linked into the kernel
include kernel/make.inc

#For dependencies
DKOBJS=$(KOBJS)

os: can_build make.deps
	@#echo -n Calculating dependencies...
	@#$(MAKE) -s deps
	@#echo ready
	@echo Building kernel...
	@$(MAKE) -s os_s

include drivers/make.inc

deps_kernel:
	@echo Refreshing Dependencies...
	@-rm make.deps 2> /dev/null
	for i in $(DKOBJS) ; do \
		echo -n $$i >> make.deps ; \
		$(CC) $(CFLAGS) -MM `echo $$i | sed -e "s@^\(.*\)\.o@\1.c@"` | sed -e "s@^\(.*\)\.o:@:@" >> make.deps; \
	done

deps:
	@touch make.deps
	@${MAKE} -s deps_kernel
	@${MAKE} -s -C library deps
	@${MAKE} -s -C drivers deps

make.deps: #$(DKOBJS:.o=.c)
	@touch make.deps
	@$(MAKE) -s deps_kernel

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif

link: $(AOBJS) $(KOBJS) library/klib.a
	echo "[LD]	skernel"
	$(LD) $(LDFLAGS) -o skernel.1 $(AOBJS) $(KOBJS) library/klib.a

os_s: $(KOBJS) $(AOBJS) 
	$(MAKE) -s -C library
	$(MAKE) -s link
	echo Building modules, pass 1...
	$(MAKE) -C drivers
	echo "Building initrd..."
	-exec ./tools/mkird $(RAMFILES) > /dev/null
	mv skernel.1 skernel
all: make.deps
	@$(MAKE) -s os

install:
	@echo "installing kernel..."
	@cp -f skernel /sys/kernel
	@echo "installing initrd..."
	@cp -f initrd.img /sys/initrd
	@make -C drivers install

clean_s:
	@-rm  $(AOBJS) $(KOBJS) $(CLEAN) initrd.img skernel make.deps 2> /dev/null
	@-$(MAKE) -s -C library clean > /dev/null
	@-$(MAKE) -s -C drivers clean > /dev/null

clean:
	@-$(MAKE) -s clean_s > /dev/null 2>/dev/null

can_build:
	@echo -n "Checking for configuration (if this fails, please run ./configure)..."
	@test -e tools/confed
	@echo "All good"

love:
	@echo Not war

include ./tools/quietrules.make
