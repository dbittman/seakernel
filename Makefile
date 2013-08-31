# seakernel makefile
ARCH=__none__
ifneq ($(MAKECMDGOALS),config)
ifneq ($(MAKECMDGOALS),defconfig)
ifneq ($(MAKECMDGOALS),clean)
include sea_defines.inc
endif
endif
endif

# process the arch config, and set the arch string
ifeq ($(CONFIG_ARCH), 2)
	ARCH=x86_64
	ARCH_TC=x86_64
endif

ifeq ($(CONFIG_ARCH), 1)
	ARCH=x86
	ARCH_TC=i586
endif

export ARCH_TC
export ARCH
export CC
export LD
export AR

CFLAGS_NOARCH = -std=c99 -nostdlib -nostdinc -nodefaultlibs \
                -fno-builtin -ffreestanding \
                -mno-red-zone -fno-omit-frame-pointer \
                -mpush-args -mno-accumulate-outgoing-args \
	        -fno-tree-loop-distribute-patterns -fno-tree-vectorize \
                -Iarch/${ARCH}/include \
                -I../include -Iinclude -I ../../include -I ../../../include \
                -D__KERNEL__ \
                -Wall -Wextra -Wformat-security -Wformat-nonliteral \
	        -Wno-strict-aliasing -Wshadow -Wpointer-arith -Wcast-align \
	        -Wno-unused -Wnested-externs -Waddress -Winline \
	        -Wno-long-long -Wno-unused-parameter -Wno-unused-but-set-parameter\
	        
CFLAGS_NOARCH += -O$(CONFIG_OPTIMIZATION_LEVEL)

ifeq ($(CONFIG_DEBUG),y)
	CFLAGS_NOARCH += -g -D__DEBUG__
endif

include make.inc
	        
ifneq ($(ARCH),__none__)
include arch/${ARCH}/make.inc
endif

export CFLAGS  = ${CFLAGS_NOARCH} ${CFLAGS_ARCH}
export LDFLAGS = ${LDFLAGS_ARCH}
export ASFLAGS = ${ASFLAGS_ARCH}
export GASFLAGS= ${GASFLAGS_ARCH}

include kernel/make.inc
include drivers/make.inc

all: can_build make.deps
	$(MAKE) -s kernel

ifneq ($(MAKECMDGOALS),config)
ifneq ($(MAKECMDGOALS),defconfig)
ifneq ($(MAKECMDGOALS),clean)
DOBJS=$(KOBJS)
DCFLAGS=$(CFLAGS)
export OBJ_EXT=o
include tools/make/deps.inc
endif
endif
endif

deps:
	@touch make.deps
	@${MAKE} -s do_deps
	@${MAKE} -s -C library deps
	@${MAKE} -s -C drivers deps
.PHONY: library/klib.a
library/klib.a:
	$(MAKE) -s -C library

skernel.1: $(AOBJS) $(KOBJS) library/klib.a
	echo "[LD]	skernel"
	$(CC) $(CFLAGS) $(LDFLAGS) -o skernel.1 $(AOBJS) $(KOBJS) library/klib.a -lgcc -static-libgcc -static

skernel: skernel.1
	cp skernel.1 skernel
	if [ "${ARCH}" = "x86_64" ]; then \
		objcopy -I elf64-x86-64 -O elf32-i386 skernel ;\
	fi

initrd.img: modules
	echo "Building initrd..."
	./tools/ird.rb initrd-${ARCH_TC}.conf > /dev/null ;\

modules: library/klib.a
	echo Building modules, pass 1...
	$(MAKE) -C drivers

kernel: make.deps skernel initrd.img


install: kernel
	@echo "installing kernel..."
	@cp -f skernel /sys/kernel
	@echo "installing initrd..."
	@cp -f initrd.img /sys/initrd
	@make -C drivers install VERSION=${KERNEL_VERSION}

clean:
	@-rm -f $(AOBJS) $(KOBJS) $(CLEAN) initrd.img skernel make.deps skernel.1
	@-$(MAKE) -s -C library clean &> /dev/null
	@-$(MAKE) -s -C drivers clean &> /dev/null

distclean: 
	@-$(MAKE) -s clean
	@-rm -f sea_defines.{h,inc} 
	@-rm -f initrd.conf make.inc
	@-rm -f tools/{confed,mkird}
	@-rm -f make.deps drivers/make.deps

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

doc:
	make -s -C documentation pdf
	make -s -C documentation aux_clean

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

include tools/make/rules.inc
