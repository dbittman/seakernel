LDFLAGS_ARCH =-T arch/x86/link.ld -m seaos_i386
ASFLAGS_ARCH =-felf32
GASFLAGS_ARCH=--32

include arch/x86/kernel/make.inc
