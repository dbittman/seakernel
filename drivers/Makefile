include ../make.inc
include ../sea_defines.inc

MCFLAGS=-std=c99 -c -g -O3 -fno-rename-registers \
		-fno-regmove -D__KERNEL__ -mno-red-zone -nostdinc -ffreestanding \
		-fno-common -nostdlib -m32 \
		-I include -I../include -I./include -I../../include \
		-I../../../include \
		-Wall -Wextra -Wformat-security -Wformat-nonliteral -Wshadow \
		-Wpointer-arith -Wcast-align -Wno-unused -Wnested-externs \
		-Winline -Wno-long-long 

export MCFLAGS
export CC
export LD
MODULES-$(CONFIG_MODULE_LOOP)      += block/loop.m
MODULES-$(CONFIG_MODULE_PCI)       += bus/pci.m
MODULES-$(CONFIG_MODULE_KEYBOARD)  += char/keyboard.m
MODULES-$(CONFIG_MODULE_RAND)	   += char/rand.m
MODULES-$(CONFIG_MODULE_SYNC) 	   += other/sync.m
MODULES-$(CONFIG_MODULE_ETHERNET)  += net/ethernet.m
MODULES-$(CONFIG_MODULE_IPV4)	   += net/ipv4.m
MODULES-$(CONFIG_MODULE_I825XX)    += net/cards/i825xx.m
MODULES-$(CONFIG_MODULE_RTL8139)   += net/cards/rtl8139.m

MODULES += $(MODULES-y)

VERSION=${KERNEL_VERSION}

all:
	@$(MAKE) -s all_s

all_s: make.deps $(MODULES)
	echo "Building modules, pass 2..."
	if [ "$(CONFIG_MODULE_ATA)"     != "" ]; then \
		MCFLAGS='$(MCFLAGS)' $(MAKE) -s -C ata     ;\
	fi
	if [ "$(CONFIG_MODULE_FAT)"     != "" ]; then \
		MCFLAGS='$(MCFLAGS)' $(MAKE) -s -C fat     ;\
	fi
	if [ "$(CONFIG_MODULE_EXT2)"    != "" ]; then \
		MCFLAGS='$(MCFLAGS)' $(MAKE) -s -C ext2    ;\
	fi
	if [ "$(CONFIG_MODULE_ISO9660)" != "" ]; then \
		MCFLAGS='$(MCFLAGS)' $(MAKE) -s -C iso9660 ;\
	fi
	@-cp -f iso9660/iso9660.m       built/iso9660    2>/dev/null
	@-cp -f ext2/ext2.m             built/ext2       2>/dev/null
	@-cp -f block/loop.m            built/loop       2>/dev/null
	@-cp -f bus/pci.m               built/pci        2>/dev/null
	@-cp -f char/keyboard.m         built/keyboard   2>/dev/null
	@-cp -f char/rand.m             built/rand       2>/dev/null
	@-cp -f ata/ata.m               built/ata        2>/dev/null
	@-cp -f net/ethernet.m          built/ethernet   2>/dev/null
	@-cp -f net/ipv4.m              built/ipv4       2>/dev/null
	@-cp -f net/cards/i825xx.m      built/i825xx     2>/dev/null
	@-cp -f net/cards/rtl8139.m     built/rtl8139    2>/dev/null
	@-cp -f other/sync.m            built/sync       2>/dev/null
	@-cp -f partitions/partitions.m built/partitions 2>/dev/null	
	@-cp -f fat/fat.m               built/fat        2>/dev/null
	

install: $(MODULES)
	@-cp -vf built/ata           /sys/modules-${VERSION}/ata        2>/dev/null
	@-cp -vf built/loop          /sys/modules-${VERSION}/loop       2>/dev/null
	@-cp -vf built/pci           /sys/modules-${VERSION}/pci        2>/dev/null
	@-cp -vf built/keyboard      /sys/modules-${VERSION}/keyboard   2>/dev/null
	@-cp -vf built/rand          /sys/modules-${VERSION}/rand       2>/dev/null
	@-cp -vf built/ext2          /sys/modules-${VERSION}/ext2       2>/dev/null
	@-cp -vf built/iso9660       /sys/modules-${VERSION}/iso9660    2>/dev/null
	@-cp -vf built/ethernet      /sys/modules-${VERSION}/ethernet   2>/dev/null
	@-cp -vf built/ipv4          /sys/modules-${VERSION}/ipv4       2>/dev/null
	@-cp -vf built/cards/i825xx  /sys/modules-${VERSION}/i825xx     2>/dev/null
	@-cp -vf built/cards/rtl8139 /sys/modules-${VERSION}/rtl8139    2>/dev/null
	@-cp -vf built/sync          /sys/modules-${VERSION}/sync       2>/dev/null
	@-cp -vf built/partitions    /sys/modules-${VERSION}/partitions 2>/dev/null

%.m: %.c
	echo -n -e "[CC] M \t$<                 \n"
	$(CC) $(MCFLAGS) $< -o $@.0
	echo -n -e "[LD] M \t$@                 \n"
	$(LD) $@.0 -o $@.1 -m seaos_i386 -r ../library/klib.a -T link.ld
	rm $@.0
	cat head $@.1 > $@
	rm $@.1

clean:
	@-rm make.deps $(MODULES) 2>/dev/null
	@-$(MAKE) -C ext2 clean 2>/dev/null
	@-$(MAKE) -C ata clean 2>/dev/null
	@-$(MAKE) -C fat clean 2>/dev/null
	@-$(MAKE) -C iso9660 clean 2>/dev/null

deps:
	@-rm make.deps 2> /dev/null
	for i in $(MODULES) ; do \
		echo -n $$i >> make.deps ; \
		$(CC) $(MCFLAGS) -MM `echo $$i | sed -e "s@^\(.*\)\.m@\1.c@"` | sed -e "s@^\(.*\)\.o:@:@" >> make.deps; \
	done

make.deps:
	@touch make.deps
	@${MAKE} -s deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif
