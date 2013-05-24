#ifndef ATA_H
#define ATA_H
#include <pci.h>
#include <mod.h>

/* Most of these definitions and support functions have been borrowed from CDI */
#define PCI_CLASS_ATA           0x01
#define PCI_SUBCLASS_ATA        0x01
#define PCI_VENDOR_VIA          0x1106

#define ATA_PRIMARY_CMD_BASE    0x1F0
#define ATA_PRIMARY_CTL_BASE    0x3F6
#define ATA_PRIMARY_IRQ         14

#define ATA_SECONDARY_CMD_BASE  0x170
#define ATA_SECONDARY_CTL_BASE  0x376
#define ATA_SECONDARY_IRQ       15

#define ATA_DELAY(c) do { ata_reg_inb(c, REG_ALT_STATUS); \
    ata_reg_inb(c, REG_ALT_STATUS);\
    ata_reg_inb(c, REG_ALT_STATUS); \
    ata_reg_inb(c, REG_ALT_STATUS); \
    ata_reg_inb(c, REG_ALT_STATUS);} while (0)
#define ATA_IDENTIFY_TIMEOUT    5000
#define ATA_READY_TIMEOUT       500
#define ATA_IRQ_TIMEOUT         500
#define ATA_SECTOR_SIZE         512

#define REG_DATA                0x0
#define REG_ERROR               0x1
#define REG_FEATURES            0x1
#define REG_SEC_CNT             0x2
#define REG_LBA_LOW             0x3
#define REG_LBA_MID             0x4
#define REG_LBA_HIG             0x5
#define REG_DEVICE              0x6
#define REG_STATUS              0x7
#define REG_COMMAND             0x7


#define REG_CONTROL             0x10
#define REG_ALT_STATUS          0x10



#define DEVICE_DEV(x)           (x << 4)

#define STATUS_BSY              (1 << 7)
#define STATUS_DRDY             (1 << 6)
#define STATUS_DF               (1 << 5)
#define STATUS_DRQ              (1 << 3)
#define STATUS_ERR              (1 << 0)
#define STATUS_MASK             (STATUS_BSY | STATUS_DRDY | STATUS_DF | \
                                STATUS_DRQ | STATUS_ERR)

#define COMMAND_IDENTIFY        0xEC
#define COMMAND_IDENTIFY_ATAPI  0xA1


#define CONTROL_HOB             (1 << 7)
#define CONTROL_SRST            (1 << 2)
#define CONTROL_NIEN            (1 << 1)


#define BMR_COMMAND             0x0
#define BMR_STATUS              0x2
#define BMR_PRDT                0x4

#define BMR_CMD_START           (1 << 0)
#define BMR_CMD_WRITE           (1 << 3)

#define BMR_STATUS_ERROR        (1 << 1)
#define BMR_STATUS_IRQ          (1 << 2)

#define ATA_DMA_MAXSIZE         (64 * 1024)

#define F_ATAPI    0x1
#define F_DMA      0x2
#define F_LBA28    0x4
#define F_LBA48    0x8
#define F_EXIST   0x10
#define F_SATA    0x20
#define F_SATAPI  0x40
#define F_ENABLED 0x80
/* Fuck your extended partitions. */
struct partition {
	char flag;
	char ext;
	char i_dont_care[2];
	char sysid;
	char again_dont_care[3];
	unsigned int start_lba;
	unsigned int length;
};

struct ata_device {
	struct ata_controller *controller;
	unsigned char flags;
	int id;
	unsigned long long length;
	struct partition ptable[64];
};

struct ata_controller {
	uint8_t						enabled;
    uint8_t                     id;
    uint16_t                    port_cmd_base;
    uint16_t                    port_ctl_base;
    uint16_t                    port_bmr_base;
    uint16_t                    irq;
    int                         dma_use;
    volatile unsigned           irqwait;
    unsigned                    prdt_phys;
    uint64_t*                   prdt_virt;
    unsigned                    dma_buf_phys[512];
    unsigned                    dma_buf_virt[512];
    struct ata_device           devices[2];
    mutex_t*                    wait;
    struct ata_device *         selected;
};

struct dev_rec
{
	struct inode *node;
	struct dev_rec *next;
};
extern struct dev_rec *nodes;
static inline uint16_t ata_reg_base(struct ata_controller* controller,
    uint8_t reg)
{
    // Fuer alle Register die ueber die ctl_base angesprochen werden muessen
    // setzen wir Bit 4.
    if ((reg & 0x10) == 0) {
        return controller->port_cmd_base;
    } else {
        return controller->port_ctl_base;
    }
}

/**
 * Byte aus Kontrollerregister lesen
 */
static inline uint8_t ata_reg_inb(struct ata_controller* controller,
    uint8_t reg)
{
    uint16_t base = ata_reg_base(controller, reg);
    return inb(base + (reg & 0xF));
}

/**
 * Byte in Kontrollerregister schreiben
 */
static inline void ata_reg_outb(struct ata_controller* controller, 
    uint8_t reg, uint8_t value)
{
    uint16_t base = ata_reg_base(controller, reg);
    outb(base + (reg & 0xF), value);
}

/**
 * Word aus Kontrollerregister lesen
 */
static inline uint16_t ata_reg_inw(struct ata_controller* controller,
    uint8_t reg)
{
    uint16_t base = ata_reg_base(controller, reg);
    return inw(base + (reg & 0xF));
}

/**
 * Byte in Kontrollerregister schreiben
 */
static inline void ata_reg_outw(struct ata_controller* controller,
    uint8_t reg, uint16_t value)
{
    uint16_t base = ata_reg_base(controller, reg);
    outw(base + (reg & 0xF), value);
}



/**
 * Mehrere Words von einem Port einlesen
 *
 * @param port   Portnummer
 * @param buffer Puffer in dem die Words abgelegt werden sollen
 * @param count  Anzahl der Words
 */
static inline void ata_insw(uint16_t port, void* buffer, uint32_t count)
{
    asm("rep insw" : "+D"(buffer), "+c"(count) : "d"(port) : "memory");
}

/**
 * Mehrere Words auf einen Port schreiben
 *
 * @param port   Portnummer
 * @param buffer Puffer aus dem die Words gelesen werden sollen
 * @param count  Anzahl der Words
 */
static inline void ata_outsw(uint16_t port, void* buffer, uint32_t count)
{
    asm("rep outsw" : "+S"(buffer), "+c"(count) : "d"(port) : "memory");
}

static inline void outsw(unsigned long addr, const void *buffer, int count)
{
	if (count) {
		const uint16 *buf = buffer;
		do {
			outw(addr, *buf++);
		} while (--count);
	}
}

static inline void insw(unsigned long addr, void *buffer, int count)
{
	if (count) {
		uint16 *buf = (uint16 *)buffer;
		do {
			uint16 x = inw(addr);
			*buf = x;
			buf++;
		} while (--count);
	}
}
int atapi_rw_main(int rw, int dev, u64 blk_, char *buf);
int ioctl_atapi(int min, int cmd, int arg);
struct ata_device *get_ata_device(int min, int *part);
int ata_dma_rw(struct ata_controller *cont, struct ata_device *dev, int rw, 
	u64 blk, unsigned char *buf, int count);
void remove_devices();
extern volatile char dma_busy;
int ata_pio_rw(struct ata_controller *cont, struct ata_device *dev, int rw, 
	unsigned long long blk, unsigned char *buffer, unsigned);
struct pci_device *pci_locate_class(unsigned short class, 
	unsigned short subclass);
extern struct ata_controller *primary, *secondary;
extern struct pci_device *ata_pci;
extern mutex_t *dma_mutex;
extern int api;
void remove_devices();
int ata_wait_irq(struct ata_controller *cont);
void ata_irq_handler(registers_t *regs);
int ata_disk_sync(struct ata_controller *cont);
int init_ata_controller(struct ata_controller *cont);
int init_ata_device();
extern int __a, __b, __c, __d;

#define ATA_DMA_ENABLE 1
#endif
