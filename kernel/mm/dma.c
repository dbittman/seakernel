#include <kernel.h>
#include <memory.h>
#include <task.h>

/* allocates a physically contiguous section of memory of length length, 
 * which does not cross a 64K boundary.
 */
char ata_dma_buf[0x1000 * 32];
unsigned ata_off=0;
int allocate_dma_buffer(size_t length, addr_t *virtual, addr_t *physical)
{
	if(ata_off + length > 0x1000 * 32) return -1;
	*virtual = *physical = (addr_t)(ata_dma_buf + ata_off);
	ata_off += length;
	return 0;
}
