#include <kernel.h>
#include <pmap.h>
#include <mutex.h>
addr_t get_next_mm_device_page();
static addr_t get_virtual_address_page(struct pmap *m, addr_t p)
{
	addr_t masked = p & PAGE_MASK;
	for(int i=0;i<m->idx;i++)
	{
		if(masked == m->phys[i])
			return m->virt[i];
	}
	if(m->idx == m->idx_max)
	{
		m->idx_max *= 2;
		addr_t *tmp = kmalloc(m->idx_max * sizeof(addr_t));
		memcpy(tmp, m->phys, sizeof(addr_t) * m->idx);
		tmp = kmalloc(m->idx_max * sizeof(addr_t));
		memcpy(tmp, m->virt, sizeof(addr_t) * m->idx);
	}
	m->phys[m->idx] = masked;
	addr_t ret;
	m->virt[m->idx] = ret = get_next_mm_device_page();
	vm_map(ret, masked, PAGE_PRESENT | PAGE_WRITE, MAP_NOCLEAR);
	m->idx++;
	return ret;
}

addr_t pmap_get_mapping(struct pmap *m, addr_t p)
{
	assert(m->magic == PMAP_MAGIC);
	int offset = (p - (p & PAGE_MASK));
	mutex_acquire(&m->lock);
	addr_t v = get_virtual_address_page(m, p);
	mutex_release(&m->lock);
	return v + offset;
}

struct pmap *pmap_create(struct pmap *m, unsigned flags)
{
	if(!m) {
		m = kmalloc(sizeof(struct pmap));
		m->flags = PMAP_ALLOC | flags;
	} else
		m->flags = flags;
	m->magic = PMAP_MAGIC;
	mutex_create(&m->lock, 0);
	m->idx_max = PMAP_INITIAL_MAX;
	m->virt = kmalloc(sizeof(addr_t) * PMAP_INITIAL_MAX);
	m->phys = kmalloc(sizeof(addr_t) * PMAP_INITIAL_MAX);
	return m;
}

void pmap_destroy(struct pmap *m)
{
	assert(m->magic == PMAP_MAGIC);
	mutex_destroy(&m->lock);
	kfree(m->virt);
	kfree(m->phys);
	m->magic = 0;
	if(m->flags & PMAP_ALLOC)
		kfree(m);
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
