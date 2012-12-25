LDFLAGS_ARCH =-Wl,-Tarch/x86/link.ld -Wl,-mseaos_i386
ASFLAGS_ARCH =-felf32
GASFLAGS_ARCH=--32
CFLAGS_ARCH  =-m32
include arch/x86/kernel/make.inc
