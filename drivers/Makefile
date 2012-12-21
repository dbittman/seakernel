include ../make.inc
include ../sea_defines.inc

MODULES-$(CONFIG_MODULE_LOOP) += block/loop.m
MODULES-$(CONFIG_MODULE_PCI)  += bus/pci.m
MODULES-$(CONFIG_MODULE_KEYBOARD) += char/keyboard.m
MODULES-$(CONFIG_MODULE_RAND)	+= char/rand.m
MODULES-$(CONFIG_MODULE_SYNC) 	+= other/sync.m
MODULES-$(CONFIG_MODULE_ETHERNET) += net/ethernet.m
MODULES-$(CONFIG_MODULE_IPV4)	+= net/ipv4.m
MODULES-$(CONFIG_MODULE_I825XX) += net/cards/i825xx.m
MODULES-$(CONFIG_MODULE_RTL8139) += net/cards/rtl8139.m

MODULES += $(MODULES-y)

VERSION=${KERNEL_VERSION}

CC=i586-pc-seaos-gcc
LD=i586-pc-seaos-ld

all:
	@$(MAKE) -s all_s

all_s: make.deps $(MODULES)
	echo "Building modules, pass 2..."
	if [ "$(CONFIG_MODULE_ATA)" != "" ]; then \
		$(MAKE) -s -C ata ;\
	fi
	if [ "$(CONFIG_MODULE_FAT)" != "" ]; then \
		$(MAKE) -s -C fat ;\
	fi
	if [ "$(CONFIG_MODULE_EXT2)" != "" ]; then \
		$(MAKE) -s -C ext2 ;\
	fi
	if [ "$(CONFIG_MODULE_ISO9660)" != "" ]; then \
		$(MAKE) -s -C iso9660 ;\
	fi
	@-cp -f iso9660/iso9660.m built/iso9660   2>/dev/null
	@-cp -f ext2/ext2.m built/ext2  2>/dev/null
	@-cp -f block/loop.m built/loop 2>/dev/null
	@-cp -f bus/pci.m built/pci 2>/dev/null
	@-cp -f char/keyboard.m built/keyboard 2>/dev/null
	@-cp -f char/rand.m built/rand 2>/dev/null
	@-cp -f ata/ata.m built/ata 2>/dev/null
	@-cp -f net/ethernet.m built/ethernet 2>/dev/null
	@-cp -f net/ipv4.m built/ipv4 2>/dev/null
	@-cp -f net/cards/i825xx.m built/i825xx 2>/dev/null
	@-cp -f net/cards/rtl8139.m built/rtl8139 2>/dev/null
	@-cp -f other/sync.m built/sync 2>/dev/null
	@-cp -f partitions/partitions.m built/partitions 2>/dev/null	
	@-cp -f fat/fat.m built/fat 2>/dev/null
	

install: $(MODULES)
	@cp -vf ata/ata.m /sys/modules-${VERSION}/ata  2>/dev/null
	@cp -vf block/loop.m /sys/modules-${VERSION}/loop  2>/dev/null
	@cp -vf bus/pci.m /sys/modules-${VERSION}/pci  2>/dev/null
	@cp -vf char/keyboard.m /sys/modules-${VERSION}/keyboard  2>/dev/null
	@cp -vf char/rand.m /sys/modules-${VERSION}/rand  2>/dev/null
	@cp -vf ext2/ext2.m /sys/modules-${VERSION}/ext2  2>/dev/null
	@cp -vf iso9660/iso9660.m /sys/modules-${VERSION}/iso9660  2>/dev/null
	@cp -vf net/ethernet.m /sys/modules-${VERSION}/ethernet  2>/dev/null
	@cp -vf net/ipv4.m /sys/modules-${VERSION}/ipv4  2>/dev/null
	@cp -vf net/cards/i825xx.m /sys/modules-${VERSION}/i825xx  2>/dev/null
	@cp -vf net/cards/rtl8139.m /sys/modules-${VERSION}/rtl8139  2>/dev/null
	@cp -vf other/sync.m /sys/modules-${VERSION}/sync  2>/dev/null
	@cp -vf partitions/partitions.m /sys/modules-${VERSION}/partitions  2>/dev/null

CFLAGS=-std=c99 -g -Wall -Wextra -fno-omit-frame-pointer\
           -Wformat-security -Wformat-nonliteral  -fno-rename-registers -fno-regmove\
	  -Wshadow -Wpointer-arith -Wcast-align -Wno-unused \
	  -Wnested-externs -Winline -Wno-long-long -mno-red-zone -D__KERNEL__ -mno-red-zone -nostdinc -c -O4 -m32 -I include -I../include -I./include -I../../include -I../../../include -ffreestanding -fno-common -nostdlib

%.m: %.c
	echo -n -e "[CC] M \t$<                 \n"
	$(CC) $(CFLAGS) $< -o $@.0
	echo -n -e "[LD] M \t$@                 \n"
	$(LD) $@.0 -o $@.1 -m seaos_i386 -r ../library/klib.a -T link.ld
	rm $@.0
	cat head head2 $@.1 > $@
	rm $@.1


clean:
	@-rm make.deps $(MODULES)
	@-$(MAKE) -C ext2 clean
	@-$(MAKE) -C ata clean
	@-$(MAKE) -C fat clean
	@-$(MAKE) -C iso9660 clean

deps:
	@-rm make.deps 2> /dev/null
	for i in $(MODULES) ; do \
		echo -n $$i >> make.deps ; \
		$(CC) $(CFLAGS) -MM `echo $$i | sed -e "s@^\(.*\)\.m@\1.c@"` | sed -e "s@^\(.*\)\.o:@:@" >> make.deps; \
	done

make.deps:
	@touch make.deps
	@${MAKE} -s deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif

