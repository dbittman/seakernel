include arch/x86_64/kernel/cpu/make.inc
include arch/x86_64/kernel/drivers/make.inc
include arch/x86_64/kernel/init/make.inc
include arch/x86_64/kernel/loader/make.inc
include arch/x86_64/kernel/machine/make.inc
include arch/x86_64/kernel/mm/make.inc
include arch/x86_64/kernel/tm/make.inc

KOBJS += arch/x86_64/kernel/console.o arch/x86_64/kernel/debugger.o

