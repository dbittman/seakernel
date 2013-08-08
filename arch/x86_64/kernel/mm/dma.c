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

addr_t mmdev_addr = 0;
mutex_t mmd_lock;
addr_t get_next_mm_device_page()
{
	if(!mmdev_addr) {
		mutex_create(&mmd_lock, 0);
		mmdev_addr = DEVICE_MAP_START;
	}
	mutex_acquire(&mmd_lock);
	if(mmdev_addr >= DEVICE_MAP_END)
		panic(0, "ran out of mmdev space");
	addr_t ret = mmdev_addr;
	mmdev_addr += 0x1000;
	mutex_release(&mmd_lock);
	return ret;
}
