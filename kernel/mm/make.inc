SWAP-$(CONFIG_SWAP) =	kernel/mm/swapping/manager.o \
						kernel/mm/swapping/swap_in.o \
						kernel/mm/swapping/swap_out.o

KOBJS+=kernel/mm/area.o \
kernel/mm/kmalloc.o \
kernel/mm/memory.o \
kernel/mm/mmfile.o \
kernel/mm/pager.o \
kernel/mm/slab.o

KOBJS += $(SWAP-y)
