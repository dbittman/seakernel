LDFLAGS_ARCH =-Wl,-Tarch/x86_64/link.ld -Wl,-melf_x86_64_seaos "-Wl,-z max-page-size=0x1000"
ASFLAGS_ARCH =-felf64
GASFLAGS_ARCH=--64
CFLAGS_ARCH  =-m64 -mcmodel=large -I arch/x86_common/include
include arch/x86_64/kernel/make.inc
include arch/x86_common/make.inc
