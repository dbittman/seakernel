LDFLAGS_ARCH =-Wl,-melf_i386_seaos
ASFLAGS_ARCH =-felf32
GASFLAGS_ARCH=--32
CFLAGS_ARCH  =-m32 -I `realpath ../arch/x86_common/include`

include ../arch/x86_common/drivers/make.inc

