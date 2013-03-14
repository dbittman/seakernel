KOBJS += arch/x86/kernel/cpu/acpi.o \
		 arch/x86/kernel/cpu/apic.o \
		 arch/x86/kernel/cpu/cpu.o \
		 arch/x86/kernel/cpu/engage.o \
		 arch/x86/kernel/cpu/features.o \
		 arch/x86/kernel/cpu/fpu.o \
		 arch/x86/kernel/cpu/mptables.o \
		 arch/x86/kernel/cpu/smp.o

ifeq ($(CONFIG_SMP),y)
	AOBJS += arch/x86/kernel/cpu/bootstrap.o 
endif
