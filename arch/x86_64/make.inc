LDFLAGS_ARCH =-Wl,-Tarch/x86_64/link.ld -Wl,-mseaos_x86_64
ASFLAGS_ARCH =-felf64
GASFLAGS_ARCH=--64
CFLAGS_ARCH  =-m64
include arch/x86_64/kernel/make.inc
