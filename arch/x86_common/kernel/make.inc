include arch/x86_common/kernel/cpu/make.inc
include arch/x86_common/kernel/drivers/make.inc
include arch/x86_common/kernel/loader/make.inc
include arch/x86_common/kernel/machine/make.inc
include arch/x86_common/kernel/mm/make.inc

KOBJS += arch/x86_common/kernel/console.o arch/x86_common/kernel/debugger.o
