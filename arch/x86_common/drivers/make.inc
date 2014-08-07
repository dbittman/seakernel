MODULES-$(CONFIG_MODULE_KEYBOARD)   += ../arch/x86_common/drivers/char/keyboard.m
MODULES-$(CONFIG_MODULE_PCI)        += ../arch/x86_common/drivers/bus/pci.m

SUBDIRS-$(CONFIG_MODULE_ATA)	    += ../arch/x86_common/drivers/ata

