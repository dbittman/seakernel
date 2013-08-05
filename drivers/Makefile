ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),config)
ifneq ($(MAKECMDGOALS),defconfig)
include ../sea_defines.inc
endif
endif
include ../make.inc
endif
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
include ../arch/$(ARCH)/drivers/make.inc
endif
endif

# note the use of mcmodel=large
# I believe, but don't quote me on this, that
# this flag is needed if we want to access relocations
# that require 64bit fields...
MCFLAGS=-std=c99 -g -O3 -fno-rename-registers \
		-fno-regmove -D__KERNEL__ -mno-red-zone -nostdinc -ffreestanding \
		-fno-common -nostdlib \
		-I../arch/${ARCH}/include \
		-Iarch/${ARCH}/include \
		-I../../arch/${ARCH}/include \
		-I../include -I../../include \
		-I../../../include \
		-I../arch/$(ARCH)/include \
		-I../../arch/$(ARCH)/include \
		-Wall -Wextra -Wformat-security -Wformat-nonliteral -Wshadow \
		-Wpointer-arith -Wcast-align -Wno-unused -Wnested-externs \
		-fno-tree-loop-distribute-patterns -fno-tree-vectorize \
		-Winline -Wno-long-long -Wno-unused-parameter -Wno-unused-but-set-parameter $(CFLAGS_ARCH)

ifeq ($(ARCH), x86_64)
MCFLAGS += -mcmodel=large
endif


export MCFLAGS
export CC
export LD

MODULES-$(CONFIG_MODULE_LOOP)       += block/loop.m
MODULES-$(CONFIG_MODULE_PCI)        += bus/pci.m
MODULES-$(CONFIG_MODULE_KEYBOARD)   += char/keyboard.m
MODULES-$(CONFIG_MODULE_RAND)	    += char/rand.m
MODULES-$(CONFIG_MODULE_SYNC) 	    += other/sync.m
MODULES-$(CONFIG_MODULE_CRC32)      += library/crc32.m
MODULES-$(CONFIG_MODULE_ETHERNET)   += net/ethernet.m
MODULES-$(CONFIG_MODULE_IPV4)	    += net/ipv4.m
MODULES-$(CONFIG_MODULE_SOCKET)	    += net/socket.m
MODULES-$(CONFIG_MODULE_I825XX)     += net/cards/i825xx.m
MODULES-$(CONFIG_MODULE_RTL8139)    += net/cards/rtl8139.m
MODULES-$(CONFIG_MODULE_PARTITIONS) += partitions/partitions.m
MODULES += $(MODULES-y)

SUBDIRS-$(CONFIG_MODULE_ATA)	    += ata
SUBDIRS-$(CONFIG_MODULE_EXT2)	    += ext2
SUBDIRS-$(CONFIG_MODULE_ISO9660)    += iso9660
SUBDIRS-$(CONFIG_MODULE_FAT)	    += fat
SUBDIRS = "$(SUBDIRS-y)"
ALLSUBDIRS = "ata ext2 iso9660 fat"
VERSION=${KERNEL_VERSION}

all:
	@$(MAKE) -s all_s

all_s: make.deps $(MODULES)
	echo "Building modules, pass 2..."
	
	for i in "$(SUBDIRS)"; do \
		if [[ $$i != "" ]]; then \
		$(MAKE) -s -C $$i ;\
		fi ;\
	done
	@-mkdir -p built
	@-rm -rf built/*
	@-for i in $(MODULES); do \
		MOD=`basename $$i | sed -e "s@^\(.*\)\.m@\1@"` ; \
		cp -f $$i built/$$MOD ;\
	done
	@-for i in "$(SUBDIRS)"; do \
		if [[ $$i != "" ]]; then \
		cp -f $$i/$$i.m built/$$i ;\
		fi ;\
	done

install: $(MODULES)
	@-for i in $(MODULES); do \
		MOD=`basename $$i | sed -e "s@^\(.*\)\.m@\1@"` ; \
		cp -vf built/$$MOD /sys/modules-${VERSION}/$$MOD ;\
	done
	@-for i in "$(SUBDIRS)"; do \
		cp -f built/$$i /sys/modules-${VERSION}/$$i ;\
	done

%.m: %.c
	echo -n -e "[CC] M \t$<                 \n"
	$(CC) $(MCFLAGS) $< -c -o $@.0
	echo -n -e "[LD] M \t$@                 \n"
	$(CC) $(MCFLAGS) $(LDFLAGS_ARCH) $@.0 -o $@.1 -Wl,-r,-Tlink.ld ../library/klib.a -lgcc -static
	rm $@.0
	cat head $@.1 > $@


clean:
	@-find -name "*.m" | xargs rm -f
	@-find -name "*.o" | xargs rm -f
	@-rm -rf built/* make.deps
	@-for i in "$(ALLSUBDIRS)"; do \
		$(MAKE) -s -C $$i clean 2>/dev/null ;\
	done

export OBJ_EXT=m
export DOBJS=${MODULES}
export DCFLAGS=${MCFLAGS}
include ../tools/make/deps.inc
