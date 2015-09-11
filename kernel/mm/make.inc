KOBJS+= kernel/mm/fault.o \
		kernel/mm/kmalloc.o \
		kernel/mm/map.o \
		kernel/mm/memory.o \
		kernel/mm/mmfile.o \
		kernel/mm/pager.o \
		kernel/mm/pmap.o \
		kernel/mm/pmm.o \
		kernel/mm/pm_buddy.o \
		kernel/mm/reclaim.o \
		kernel/mm/newslab.o \
		kernel/mm/valloc.o \
		kernel/mm/vmm.o

KOBJS += $(SWAP-y)

