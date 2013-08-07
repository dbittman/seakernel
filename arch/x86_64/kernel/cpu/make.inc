KOBJS += arch/x86_64/kernel/cpu/apic.o \
		 arch/x86_64/kernel/cpu/cpu.o \
		 arch/x86_64/kernel/cpu/engage.o \
		 arch/x86_64/kernel/cpu/features.o \
		 arch/x86_64/kernel/cpu/ipi.o \
		 arch/x86_64/kernel/cpu/mptables.o \
		 arch/x86_64/kernel/cpu/smp.o

ifeq ($(CONFIG_SMP),y)
	AOBJS += arch/x86_64/kernel/cpu/bootstrap.o 
endif
