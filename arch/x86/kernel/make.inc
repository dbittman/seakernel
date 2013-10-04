include arch/x86/kernel/cpu/make.inc
include arch/x86/kernel/drivers/make.inc
include arch/x86/kernel/init/make.inc
include arch/x86/kernel/loader/make.inc
include arch/x86/kernel/machine/make.inc
include arch/x86/kernel/mm/make.inc
include arch/x86/kernel/tm/make.inc

KOBJS+=arch/x86/kernel/atomic.o arch/x86/kernel/i386-stub.o
