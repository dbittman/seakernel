include drivers/net/make.inc
include drivers/video/make.inc

RAMFILES+=drivers/char/keyboard.m keyboard drivers/block/loop.m loop \
drivers/bus/pci.m pci drivers/partitions/partitions.m partitions \
drivers/ext2/ext2.m ext2 \
drivers/ata/ata.m ata \
drivers/iso9660/iso9660.m iso9660
