include sea_defines.inc

CFLAGS = -O3 -g -std=c99 -nostdlib -nostdinc \
		 -fno-builtin -ffreestanding \
         -I../include -Iinclude -I ../../include -I ../../../include \
         -D__KERNEL__ -D__DEBUG__ \
         -Wall -Wextra -Wformat-security -Wformat-nonliteral \
	     -Wno-strict-aliasing -Wshadow -Wpointer-arith -Wcast-align \
	     -Wno-unused -Wnested-externs -Waddress -Winline \
	     -Wno-long-long -mno-red-zone -fno-omit-frame-pointer 

LDFLAGS=-T kernel/link.ld -m seaos_i386
ASFLAGS=-felf32
GASFLAGS=--32
include make.inc

include kernel/make.inc
include drivers/make.inc

#For dependencies
DKOBJS=$(KOBJS)

os: can_build make.deps
	@echo Building kernel...
	@$(MAKE) -s os_s

deps_kernel:
	@echo refreshing dependencies...
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
	@-$(MAKE) -s -C library clean > /dev/null
	@-$(MAKE) -s -C drivers clean > /dev/null

clean:
	@-$(MAKE) -s clean_s > /dev/null 2>/dev/null

config:
	tools/conf.rb config.cfg
	@echo post-processing configuration...
	@tools/config.rb .config.cfg
	
defconfig:
	tools/conf.rb -d config.cfg
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
