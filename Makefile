# seaos kernel makefile 
include sea_defines.inc
include make.inc
ARCH=x86
export ARCH
CFLAGS_NOARCH = -O3 -g -std=c99 -nostdlib -nostdinc \
		 -fno-builtin -ffreestanding \
         -I../include -Iinclude -I ../../include -I ../../../include \
         -D__KERNEL__ -D__DEBUG__ \
         -Wall -Wextra -Wformat-security -Wformat-nonliteral \
	     -Wno-strict-aliasing -Wshadow -Wpointer-arith -Wcast-align \
	     -Wno-unused -Wnested-externs -Waddress -Winline \
	     -Wno-long-long -mno-red-zone -fno-omit-frame-pointer 

include arch/${ARCH}/make.inc

CFLAGS  = ${CFLAGS_NOARCH} ${CFLAGS_ARCH}
LDFLAGS = ${LDFLAGS_ARCH}
ASFLAGS = ${ASFLAGS_ARCH}
GASFLAGS= ${GASFLAGS_ARCH}

include kernel/make.inc
include drivers/make.inc

os: can_build make.deps
	@echo Building kernel...
	@$(MAKE) -s os_s

DOBJS=$(KOBJS)
DCFLAGS=$(CFLAGS)
export OBJ_EXT=o
include deps.inc

deps:
	@touch make.deps
	@${MAKE} -s do_deps
	@${MAKE} -s -C library deps
	@${MAKE} -s -C drivers deps

link: $(AOBJS) $(KOBJS) library/klib.a
	echo "[LD]	skernel"
	$(LD) $(LDFLAGS) -o skernel.1 $(AOBJS) $(KOBJS) library/klib.a

os_s: $(KOBJS) $(AOBJS) 
	$(MAKE) -s -C library
	$(MAKE) -s link
	echo Building modules, pass 1...
	$(MAKE) -C drivers
	echo "Building initrd..."
	-./tools/ird.rb initrd.conf > /dev/null
	mv skernel.1 skernel

all: make.deps
	@$(MAKE) -s os

install:
	@echo "installing kernel..."
	@cp -f skernel /sys/kernel
	@echo "installing initrd..."
	@cp -f initrd.img /sys/initrd
	@make -C drivers install VERSION=${KERNEL_VERSION}

clean_s:
	@-rm  $(AOBJS) $(KOBJS) $(CLEAN) initrd.img skernel make.deps 2> /dev/null
	@-$(MAKE) -s -C library clean &> /dev/null
	@-$(MAKE) -s -C drivers clean &> /dev/null

clean:
	@-$(MAKE) -s clean_s > /dev/null 2>/dev/null

config:
	@tools/conf.rb config.cfg
	@echo post-processing configuration...
	@tools/config.rb .config.cfg
	
defconfig:
	@tools/conf.rb -d config.cfg
	@echo post-processing configuration...
	@tools/config.rb .config.cfg

can_build:
	@echo -n "Checking for configuration (if this fails, please run ./configure)..."
	@test -e tools/confed
	@echo "All good"

love:
	@echo Not war
	
help:
	@echo "make [target]"
	@echo "Useful targets:"
	@echo -e " config:\truns the configuration utility"
	@echo -e " defconfig:\tcreates a default configuration"
	@echo -e " clean:\t\tremoves compiled objects from the source tree"
	@echo -e " deps:\t\trecalculates dependencies for the source tree"
	@echo -e " install:\tcopies the compiled binaries to their proper locations in the file system"
	@echo -e " all,os:\tcompiles the kernel"
	@echo

include ./tools/quietrules.make
