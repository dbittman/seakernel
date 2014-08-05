SWAP-$(CONFIG_SWAP) =	kernel/mm/swapping/manager.o \
						kernel/mm/swapping/swap_in.o \
						kernel/mm/swapping/swap_out.o

KOBJS+= kernel/mm/area.o \
		kernel/mm/fault.o \
		kernel/mm/kmalloc.o \
		kernel/mm/map.o \
		kernel/mm/memory.o \
		kernel/mm/mmfile.o \
		kernel/mm/pager.o \
		kernel/mm/pmap.o \
		kernel/mm/pmm.o \
		kernel/mm/slab.o \
		kernel/mm/valloc.o \
		kernel/mm/vmm.o

KOBJS += $(SWAP-y)

