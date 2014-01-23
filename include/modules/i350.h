#ifndef __MODULES_I350_H
#define __MODULES_I350_H
#include <types.h>
#include <modules/pci.h>
#include <mutex.h>

struct i350_receive_descriptor {
	uint64_t buffer;
	uint64_t length:16;
	uint64_t frag_check:16;
	uint64_t status:8;
	uint64_t error:8;
	uint64_t vlan_tag:16;
};

struct i350_transmit_descriptor {
	uint64_t buffer;
	uint64_t length:16;
	uint64_t cso:8;
	uint64_t cmd:8;
	uint64_t sta:4;
	uint64_t __reserved:4;
	uint64_t css:8;
	uint64_t vlan:16;
};

struct i350_device {
	struct pci_device *pci;
	addr_t mem, pcsmem;
	
	addr_t receive_list_physical;
	addr_t transmit_list_physical;
	struct i350_receive_descriptor *receive_ring;
	struct i350_transmit_descriptor *transmit_ring;
	uint32_t rx_list_count;
	uint32_t rx_buffer_len;
	uint32_t tx_list_count;
	uint32_t tx_buffer_len;
	
	mutex_t *tx_queue_lock[1];
	mutex_t *rx_queue_lock[1];
	
};



#define E1000_CTRL     0x00000
#define E1000_STATUS   0x00008
#define E1000_CTRL_EXT 0x00018
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RDTR1    0x02820  /* RX Delay Timer (1) - RW */
#define E1000_RDBAL1   0x02900  /* RX Descriptor Base Address Low (1) - RW */
#define E1000_RDBAH1   0x02904  /* RX Descriptor Base Address High (1) - RW */
#define E1000_RDLEN1   0x02908  /* RX Descriptor Length (1) - RW */
#define E1000_RDH1     0x02910  /* RX Descriptor Head (1) - RW */
#define E1000_RDT1     0x02918  /* RX Descriptor Tail (1) - RW */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_IMC      0x000D8  /* Interrupt Mask Clear - WO */
#define E1000_ICR      0x01500

#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RDTR     0x02820  /* RX Delay Timer - RW */
#define E1000_RDBAL0   E1000_RDBAL /* RX Desc Base Address Low (0) - RW */
#define E1000_RDBAH0   E1000_RDBAH /* RX Desc Base Address High (0) - RW */
#define E1000_RDLEN0   E1000_RDLEN /* RX Desc Length (0) - RW */
#define E1000_RDH0     E1000_RDH   /* RX Desc Head (0) - RW */
#define E1000_RDT0     E1000_RDT   /* RX Desc Tail (0) - RW */
#define E1000_RDTR0    E1000_RDTR  /* RX Delay Timer (0) - RW */
#define E1000_RXDCTL   0x02828  /* RX Descriptor Control queue 0 - RW */
#define E1000_SRRCTL0  0x0C00C


#define E1000_TCTL     0x0400
#define E1000_TDBAL0   0xE000
#define E1000_TDBAH0   0xE004
#define E1000_TDLEN0   0xE008
#define E1000_TDH0     0xE010
#define E1000_TDT0     0xE018
#define E1000_TXDCTL   0xE028



#define E1000_GPRC     0x04074
#define E1000_GPTC     0x04080
#define E1000_RXERR    0x0400C
#define E1000_MPC      0x04010

#define E1000_PCS_LCTL    0x04208  /* PCS Link Control - RW */



#define E1000_PCS_LCTL_AN_ENABLE (1 << 16)
#define E1000_PCS_LCTL_AN_RESTART (1 << 17)

#define E1000_CTRL_EXT_DRVLOADED (1 << 28)

#define E1000_CTRL_RESET (1 << 26)
#define E1000_CTRL_GIO_MASTER_DISABLE (1 << 2)
#define E1000_CTRL_SLU (1 << 6)
#define E1000_CTRL_ILOS (1 << 7)
#define E1000_CTRL_RXFC (1 << 27)

#define E1000_STATUS_RESET_DONE (1 << 21)
#define E1000_STATUS_GIO_MASTER_ENABLE (1 << 19)

#define E1000_ICR_RXDW    (1 << 7)
#define E1000_ICR_LSC     (1 << 2)
#define E1000_ICR_TXDW    (1 << 0)
#define E1000_ICR_RXMISS  (1 << 6)
#define E1000_ICR_FER     (1 << 22)
#define E1000_ICR_PCIEX   (1 << 24)

#endif
