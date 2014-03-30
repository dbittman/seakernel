#ifndef PCI_H
#define PCI_H
#include <sea/config.h>
#ifdef CONFIG_MODULE_PCI
#include <sea/types.h>
#include <sea/fs/inode.h>

struct pci_config_space
{
    /* 0x00 */
    uint16_t vendor_id;
    uint16_t device_id;
    /* 0x04 */
    uint16_t command;
    uint16_t status;
    /* 0x08 */
    uint16_t revision;
    uint8_t  subclass;
    uint8_t  class_code;
    /* 0x0C */
    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;
    /* 0x10 */
    uint32_t bar0;
    /* 0x14 */
    uint32_t bar1;
    /* 0x18 */
    uint32_t bar2;
    /* 0x1C */
    uint32_t bar3;
    /* 0x20 */
    uint32_t bar4;
    /* 0x24 */
    uint32_t bar5;
    /* 0x28 */
    uint32_t cardbus_cis_pointer;
    /* 0x2C */
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    /* 0x30 */
    uint32_t expansion_rom_base_address;
    /* 0x34 */
    uint32_t reserved0;
    /* 0x38 */
    uint32_t reserved1;
    /* 0x3C */
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  min_grant;
    uint8_t  max_latency;
}__attribute__((packed));

#define PCI_DRIVEN  0x1 /* Device has a driver that has loaded it - set by the driver */
#define PCI_ENGAGED 0x2 /* Driver is using the device (usually on if 0x1 is on) */
#define PCI_ERROR   0x4 /* The driver encountered an error while loading the device: Sets error value */

struct pci_device
{
    uint16_t bus, dev, func;
    struct pci_config_space *pcs;
    
    unsigned char flags;
    unsigned error;
    char pad;
    struct inode *node;
    struct pci_device *next, *prev;
};
struct pci_device *pci_locate_device(unsigned short vendor, unsigned short device);
unsigned pci_get_base_address(struct pci_device *device);
struct pci_device *pci_locate_devices(unsigned short vendor, 
	unsigned short device, int i);
void pci_write_dword(const uint16_t bus, const uint16_t dev, 
	const uint16_t func, const uint32_t reg, unsigned data);
uint32_t pci_read_dword(const uint16_t bus, const uint16_t dev, 
	const uint16_t func, const uint32_t reg);
struct pci_device *pci_locate_class(unsigned short class, unsigned short _subclass);
#endif
#endif
