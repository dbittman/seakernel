/* Defines functions for virtual memory management */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>

#include <sea/cpu/interrupt.h>
#include <sea/boot/init.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/loader/symbol.h>

addr_t *kernel_directory=0;
pml4_t *kernel_dir_phys=0;
int id_tables=0;


/* This function will setup a paging environment with a basic page dir, 
 * enough to process the memory map passed by grub */
static void early_mm_vm_map(pml4_t *pml4, addr_t addr, addr_t map)
{
	pdpt_t *pdpt;
	page_dir_t *pd;
	page_table_t *pt;
	
	if(!pml4[PML4_IDX(addr/0x1000)])
		pml4[PML4_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	pdpt = (addr_t *)(pml4[PML4_IDX(addr/0x1000)] & PAGE_MASK);
	if(!pdpt[PDPT_IDX(addr/0x1000)])
		pdpt[PDPT_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	pd = (addr_t *)(pdpt[PDPT_IDX(addr/0x1000)] & PAGE_MASK);
	if(!pd[PAGE_DIR_IDX(addr/0x1000)])
		pd[PAGE_DIR_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	/* passing map as zero allows us to map in all the tables, but leave the
	 * true mapping null. This is handy for the page stack and heap */
	pt = (addr_t *)(pd[PAGE_DIR_IDX(addr/0x1000)] & PAGE_MASK);
	pt[PAGE_TABLE_IDX(addr/0x1000)] = map;
}

extern void *boot_pml4;
alignas(0x1000) static uint64_t physmap_pages [513][512];
void arch_mm_vm_init(struct vmm_context *context)
{
 	context->root_physical = (addr_t)&boot_pml4;
 	context->root_virtual = kernel_context.root_physical + MEMMAP_KERNEL_START;
 	mutex_create(&kernel_context.lock, MT_NOSCHED);
 	/* map in all physical memory */
 	pml4_t *pml4 = (pml4_t *)context->root_virtual;
 	addr_t address = 0;
 	memset(physmap_pages, 0, sizeof(physmap_pages));
	pdpt_t *pdpt = (pdpt_t *)((addr_t)physmap_pages[0]);
	for(int i=0;i<512;i++) {
		pdpt[i] = ((addr_t)physmap_pages[i+1] - MEMMAP_KERNEL_START) | PAGE_PRESENT | PAGE_WRITE;
		page_dir_t *pd = (page_dir_t *)((pdpt[i] & PAGE_MASK) + MEMMAP_KERNEL_START);
		for(int j=0;j<512;j++) {
			pd[j] = address | PAGE_PRESENT | PAGE_WRITE | (1 << 7);
			address += 0x200000;
		}
	}
 	pml4[PML4_INDEX(PHYS_PAGE_MAP)] = ((addr_t)pdpt - MEMMAP_KERNEL_START) | PAGE_PRESENT | PAGE_WRITE;
	/* Enable paging */
	set_ksf(KSF_PAGING);
}

addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked)
{
	addr_t vpage = (v&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	addr_t ret=0;
	if(pd_cur_data && !locked)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((pd_cur_data) ? pd_cur_data->root_virtual : kernel_context.root_virtual);
	if(!pml4[vp4])
		goto out;
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		goto out;
	if(pdpt[vpdpt] & PAGE_LARGE)
	{
		ret = pdpt[vpdpt] & PAGE_MASK;
		goto out;
	}
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		goto out;
	if(pd[vdir] & PAGE_LARGE)
	{
		ret = pd[vdir] & PAGE_MASK;
		goto out;
	}
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	ret = pt[vtbl] & PAGE_MASK;
	out:
	if(p)
		*p = ret;
	if(pd_cur_data && !locked)
		mutex_release(&pd_cur_data->lock);
	return ret;
}

void arch_mm_vm_set_attrib(addr_t v, short attr)
{
	addr_t vpage = (v&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	if(pd_cur_data)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((pd_cur_data) ? pd_cur_data->root_virtual : kernel_context.root_virtual);
	if(!pml4[vp4])
		pml4[vp4] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(pdpt[vpdpt] & PAGE_LARGE)
	{
		pdpt[vpdpt] &= PAGE_MASK;
		pdpt[vpdpt] |= (attr | PAGE_LARGE);
		goto out;
	}
	if(!pdpt[vpdpt])
		pdpt[vpdpt] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(pd[vdir] & PAGE_LARGE)
	{
		pd[vdir] &= PAGE_MASK;
		pd[vdir] |= (attr | PAGE_LARGE);
		goto out;
	}
	if(!pd[vdir])
		pd[vdir] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	
	pt[vtbl] &= PAGE_MASK;
	pt[vtbl] |= attr;
	out:
	asm("invlpg (%0)"::"r" (v));
#if CONFIG_SMP && 0
	if(pd_cur_data) {
		if(IS_KERN_MEM(v))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(v) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	if(pd_cur_data)
		mutex_release(&pd_cur_data->lock);
}

unsigned int arch_mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked)
{
	addr_t vpage = (v&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	unsigned ret=0;
	if(pd_cur_data && !locked)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((pd_cur_data) ? pd_cur_data->root_virtual : kernel_context.root_virtual);
	if(!pml4[vp4])
		goto out;
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		goto out;
	if(pdpt[vpdpt] & PAGE_LARGE)
	{
		ret = pdpt[vpdpt] & ATTRIB_MASK;
		goto out;
	}
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		goto out;
	if(pd[vdir] & PAGE_LARGE)
	{
		ret = pd[vdir] & ATTRIB_MASK;
		goto out;
	}
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	ret = pt[vtbl] & ATTRIB_MASK;
	out:
	if(p)
		*p = ret;
	if(pd_cur_data && !locked)
		mutex_release(&pd_cur_data->lock);
	return ret;
}
