#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>

/* allocates a physically contiguous section of memory of length length, 
 * which does not cross a 64K boundary.
 */
static char ata_dma_buf[0x1000 * 128];
static unsigned ata_off=0;
int arch_mm_allocate_dma_buffer(size_t length, addr_t *virt, addr_t *physical)
{
	if(ata_off + length > 0x1000 * 128) return -1;
	*virt = *physical = (addr_t)(ata_dma_buf + ata_off);
	ata_off += length;
	return 0;
}
