MODULES-$(CONFIG_MODULE_KEYBOARD)   += ../arch/x86_64/drivers/char/keyboard.m
MODULES-$(CONFIG_MODULE_PCI)        += ../arch/x86_64/drivers/bus/pci.m

SUBDIRS-$(CONFIG_MODULE_ATA)	    += ../arch/x86_64/drivers/ata
CFLAGS_ARCH += -I `realpath ../arch/x86_64/include`

