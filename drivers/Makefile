include ../make.inc
MODULES+=block/loop.m \
bus/pci.m \
char/keyboard.m \
char/rand.m \
net/ethernet.m \
net/ipv4.m \
net/cards/i825xx.m \
net/cards/rtl8139.m \
other/sync.m \
partitions/partitions.m

VERSION=${KERNEL_VERSION}

CC=i586-pc-seaos-gcc
LD=i586-pc-seaos-ld

all:
	@$(MAKE) -s all_s

all_s: make.deps $(MODULES)
	echo "Building modules, pass 2..."
	$(MAKE) -s -C ata
	@#$(MAKE) -s -C fat
	$(MAKE) -s -C ext2
	$(MAKE) -s -C iso9660
	@cp -f ata/ata.m built/ata
	@cp -f block/loop.m built/loop
	@cp -f bus/pci.m built/pci
	@cp -f char/keyboard.m built/keyboard
	@cp -f char/rand.m built/rand
	@cp -f ext2/ext2.m built/ext2
	@cp -f iso9660/iso9660.m built/iso9660
	@cp -f net/ethernet.m built/ethernet
	@cp -f net/ipv4.m built/ipv4
	@cp -f net/cards/i825xx.m built/i825xx
	@cp -f net/cards/rtl8139.m built/rtl8139
	@cp -f other/sync.m built/sync
	@cp -f partitions/partitions.m built/partitions	
	

install: $(MODULES)
	@cp -vf ata/ata.m /sys/modules-${VERSION}/ata
	@cp -vf block/loop.m /sys/modules-${VERSION}/loop
	@cp -vf bus/pci.m /sys/modules-${VERSION}/pci
	@cp -vf char/keyboard.m /sys/modules-${VERSION}/keyboard
	@cp -vf char/rand.m /sys/modules-${VERSION}/rand
	@cp -vf ext2/ext2.m /sys/modules-${VERSION}/ext2
	@cp -vf iso9660/iso9660.m /sys/modules-${VERSION}/iso9660
	@cp -vf net/ethernet.m /sys/modules-${VERSION}/ethernet
	@cp -vf net/ipv4.m /sys/modules-${VERSION}/ipv4
	@cp -vf net/cards/i825xx.m /sys/modules-${VERSION}/i825xx
	@cp -vf net/cards/rtl8139.m /sys/modules-${VERSION}/rtl8139
	@cp -vf other/sync.m /sys/modules-${VERSION}/sync
	@cp -vf partitions/partitions.m /sys/modules-${VERSION}/partitions

CFLAGS=-std=c99 -Wall -Wextra -fno-omit-frame-pointer\
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

