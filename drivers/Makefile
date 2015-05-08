ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),config)
ifneq ($(MAKECMDGOALS),defconfig)
include $(shell realpath -m $(BUILDDIR)/../sea_defines.inc)
endif
endif
include ../make.inc
endif

MODULES-$(CONFIG_MODULE_LOOP)       += block/loop.m
MODULES-$(CONFIG_MODULE_CRYPTO_PART) += block/crypto-part.m
MODULES-$(CONFIG_MODULE_RAND)	    += char/rand.m
MODULES-$(CONFIG_MODULE_CRC32)      += library/crc32.m
MODULES-$(CONFIG_MODULE_ETHERNET)   += net/ethernet.m
MODULES-$(CONFIG_MODULE_I825XX)     += net/cards/i825xx.m
MODULES-$(CONFIG_MODULE_RTL8139)    += net/cards/rtl8139.m
MODULES-$(CONFIG_MODULE_I350)       += net/cards/i350.m
MODULES-$(CONFIG_MODULE_PARTITIONS) += partitions/partitions.m
MODULES-$(CONFIG_MODULE_NETLOOP)    += net/lo.m
MODULES-$(CONFIG_MODULE_UDP)        += net/udp.m

SUBDIRS-$(CONFIG_MODULE_AES)	    += aes
SUBDIRS-$(CONFIG_MODULE_AHCI)	    += ahci
SUBDIRS-$(CONFIG_MODULE_EXT2)	    += ext2
SUBDIRS-$(CONFIG_MODULE_ISO9660)    += iso9660
SUBDIRS-$(CONFIG_MODULE_FAT)	    += fat
SUBDIRS-$(CONFIG_MODULE_PSM)	    += psm
SUBDIRS-$(CONFIG_MODULE_SHIV)	    += shiv
SUBDIRS-$(CONFIG_MODULE_IPV4)	    += net/ipv4
SUBDIRS-$(CONFIG_MODULE_TCP)	    += net/tcp

ALLSUBDIRS = aes ext2 iso9660 fat ahci psm shiv net/ipv4 net/tcp

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
include ../arch/$(ARCH)/drivers/make.inc
endif
endif

MODULES += $(addprefix $(BUILDDIR)/, $(MODULES-y))
SUBDIRS = "$(SUBDIRS-y)"
VERSION=${KERNEL_VERSION}
DEPSFILE=$(BUILDDIR)/make.deps
# note the use of mcmodel=large
# I believe, but don't quote me on this, that
# this flag is needed if we want to access relocations
# that require 64bit fields...
MCFLAGS:=        -std=gnu11 \
		-D__KERNEL__ -mno-red-zone -ffreestanding -include `realpath $(BUILDDIR)/../sea_defines.h`\
		-nostdlib \
		-I`realpath ../arch/${ARCH}/include` \
		-I`realpath ../include` \
		-I`realpath $(BUILDDIR)/../arch/include`\
		-I`realpath $(BUILDDIR)/..`\
		-Wall -Wextra -Wformat-security -Wformat-nonliteral -Wshadow \
		-Wpointer-arith -Wcast-align -Wno-unused -Wnested-externs \
		-Winline -Wno-long-long -Wno-unused-parameter \
		-Wno-unused-but-set-parameter $(CFLAGS_ARCH) \
		-mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow \
		-fno-omit-frame-pointer \
		-fno-tree-loop-distribute-patterns \
		-fno-common

ifeq ($(ARCH), x86_64)
MCFLAGS += -mcmodel=large
endif

MCFLAGS += -O$(CONFIG_OPTIMIZATION_LEVEL)

ifeq ($(CONFIG_DEBUG),y)
	MCFLAGS += -g -D__DEBUG__
endif

ifeq ($(CONFIG_WERROR),y)
	MCFLAGS += -Werror
endif

export MCFLAGS
export CC
export LD
all: $(BUILDDIR) $(BUILDDIR)/built $(DEPSFILE) $(MODULES)
	@echo "Building modules, pass 2..."
	@for i in "$(SUBDIRS)"; do \
		if [ $$i != "" ]; then \
			export MCFLAGS="$(MCFLAGS)" ;\
			$(MAKE) -s -C $$i ;\
		fi ;\
	done

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/built:
	@mkdir -p $(BUILDDIR)/built

install: $(MODULES)
	@cp $(BUILDDIR)/built/* /sys/modules-${VERSION}
	@-cp modules.deps /sys/modules-${VERSION}/modules.deps

$(BUILDDIR)/%.m: %.c
	@mkdir -p $$(dirname $@)
	@echo -n -e "[CC] M \t$@                 \n"
	@$(CC) $(MCFLAGS) $< -c -o $@.o
	@$(CC) $(MCFLAGS) $(LDFLAGS_ARCH) $@.o -o $@ -Wl,-r,-Tlink.ld -lgcc -static
	@cp $@ $(BUILDDIR)/built/$(basename $(notdir $@))

clean:
	@-rm -rf $(BUILDDIR)

export OBJ_EXT=m
export DOBJS=${MODULES}
export DCFLAGS=${MCFLAGS}
include ../tools/make/deps.inc

