#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>

void add_mmf(task_t *t, mmf_t *mf)
{
	mmf_t *o = t->mm_files;
	t->mm_files = mf;
	mf->prev=0;
	mf->next = o;
	if(o)
		o->prev=mf;
}

void remove_mmf(mmf_t *mf)
{
	task_t *t = (task_t *)current_task;
	if(mf->prev)
		mf->prev->next = mf->next;
	else
		t->mm_files=mf->next;
	if(mf->next)
		mf->next->prev = mf->prev;
	if(!(mf->flags & MAP_SHARED) || !mf->count)
		kfree(mf->count);
	kfree(mf);
}

unsigned sys_mmap(void *addr, void *str, int prot, int flags, int fildes)
{
	if(!str)
		return EINVAL;
	struct mmapblock {
		unsigned len;
		unsigned off;
	} *blk = str;
	if(flags & MAP_ANON || (addr && flags&MAP_FIXED))
		return ENOTSUP;
	struct file *fil = get_file_pointer((task_t *)current_task, fildes);
	if(!fil)
		return EBADF;
	if(!(fil->flags & _FREAD))
		return EACCES;
	if(flags&PROT_WRITE && !(fil->flags&_FWRITE))
		return EACCES;
	/* Ok, we can attempt to allocate the area */
	vma_t **v = (vma_t **)((flags&MAP_SHARED) ? &((task_t *)current_task)->mmf_share_space : &((task_t *)current_task)->mmf_priv_space);
	if(!*v) {
		*v = (vma_t *)kmalloc(sizeof(vma_t));
		init_vmem_area(*v, (flags&MAP_SHARED) ? MMF_SHARED_START : MMF_PRIV_START,
			       (flags&MAP_SHARED) ? MMF_SHARED_END : MMF_PRIV_END, A_NI);
		unsigned a = (*v)->addr, e = (*v)->addr + A_NI*PAGE_SIZE;
		while(a < e) {
			vm_map(a, pm_alloc_page(), PAGE_PRESENT | PAGE_USER | PAGE_WRITE, MAP_CRIT);
			a += PAGE_SIZE;
		}
	}
	int np = blk->len/PAGE_SIZE + 1;
	vnode_t *node = insert_vmem_area(*v, np);
	if(!node)
		return ENOMEM;
	int i=0;
	/* Make sure that nothing is mapped there */
	while(i < np) {
		if(vm_getmap(node->addr + i*PAGE_SIZE, 0))
			vm_unmap(node->addr + i*PAGE_SIZE);
		i++;
	}
	/* And create the entry */
	mmf_t *mf = (mmf_t *)kmalloc(sizeof(mmf_t));
	mf->flags=flags;
	mf->prot=prot;
	mf->node=node;
	mf->sz=blk->len;
	mf->off=blk->off;
	mf->count=(unsigned *)kmalloc(sizeof(unsigned));
	*mf->count=1;
	mf->fd = fildes;
	add_mmf((task_t *)current_task, mf);
	return node->addr;
}

mmf_t *find_mmf(task_t *t, vnode_t *n)
{
	mmf_t *m = t->mm_files;
	while(m && m->node != n)
		m=m->next;
	return m;
}

void flush_mmf(mmf_t *m, int un_map, unsigned off, unsigned addr, unsigned end)
{
	struct file *fil = get_file_pointer((task_t *)current_task, m->fd);
	if(!fil)
		return;
	int sz = end-addr;
	int i=0;
	while(addr < end)
	{
		if(vm_getmap(addr, 0))
		{
			int length = sz-i;
			if(length > PAGE_SIZE)
				length = PAGE_SIZE;
			do_sys_write_flags(fil, i+off, (char *)addr, length);
			if(un_map)
				vm_unmap(addr);
		}
		i += PAGE_SIZE;
		addr += PAGE_SIZE;
	}
}

int change_count(mmf_t *mf, int ch)
{
	lock_scheduler();
	*mf->count += ch;
	int r = *mf->count;
	unlock_scheduler();
	return r;
}

int sys_munmap(void *ptr, unsigned sz)
{
	if(!ptr)
		return -EINVAL;
	ptr = (void *)((unsigned)ptr & PAGE_MASK);
	vma_t *v   = current_task->mmf_priv_space;
	vnode_t *n = find_vmem_area(v, (unsigned)ptr);
	if(!n)
		n = find_vmem_area((v=current_task->mmf_share_space), (unsigned)ptr);
	if(!n)
		return -EINVAL;
	mmf_t *mf = find_mmf((task_t *)current_task, n);
	if(!mf)
		return -EINVAL;
	vnode_t *vn = mf->node;
	int clear=0;
	if((unsigned)ptr == vn->addr && (sz / PAGE_SIZE) == vn->num_pages)
		clear=1;
	flush_mmf(mf, clear, ((unsigned)ptr - vn->addr)+mf->off, (unsigned)ptr, (unsigned)ptr + sz);
	if(clear)
	{
		remove_mmf(mf);
		remove_vmem_area(v, vn);
	}
	return 0;
}

int sys_msync()
{
	return -ENOSYS;
}

int sys_mprotect()
{
	return -ENOSYS;
}

void check_mmf_and_flush(task_t *t, int fd)
{
	mmf_t *mf = t->mm_files, *nex;
	while(mf)
	{
		nex = mf->next;
		if(((int)mf->fd == fd) || (fd == -1))
			flush_mmf(mf, 0, mf->off, mf->node->addr, mf->sz);
		mf=nex;
	}
}

void copy_mmf(task_t *old, task_t *new)
{
	/* This will increase the count of all shared MMFs */
	mmf_t *mf = old->mm_files;
	while(mf)
	{
		mmf_t *n = (mmf_t *)kmalloc(sizeof(mmf_t));
		memcpy(n, mf, sizeof(mmf_t));
		if(mf->flags & MAP_SHARED)
			change_count(mf, 1);
		else {
			n->count = (unsigned *)kmalloc(sizeof(unsigned));
			*n->count=1;
		}
		add_mmf(new, n);
		mf = mf->next;
	}
}

int pfault_mmf_check(unsigned err, unsigned addr)
{
	mmf_t *mf = current_task->mm_files;
	vma_t *v   = current_task->mmf_priv_space;
	vnode_t *n = find_vmem_area(v, addr);
	if(!n)
		n = find_vmem_area((v=current_task->mmf_share_space), addr);
	if(!n)
		return 0;
	while(mf && mf->node != n)
		mf=mf->next;
	/* If we tried to write and we aren't allowed to do so */
	if((err & 0x2) && !(mf->prot & PROT_WRITE))
		return 0;
	/* If its already present */
	if(err & 0x1)
		return 0;
	unsigned p = pm_alloc_page();
	unsigned attr = PAGE_PRESENT | PAGE_USER;
	if(mf->prot & PROT_WRITE)
		attr |= PAGE_WRITE;
	vm_map(addr & PAGE_MASK, p, attr, MAP_CRIT);
	memset((void *)(addr & PAGE_MASK), 0, PAGE_SIZE);
	struct file *fil = get_file_pointer((task_t *)current_task, mf->fd);
	if(!fil)
		return 1;
	int off = (addr&PAGE_MASK) - n->addr;
	do_sys_read_flags(fil, mf->off + off, (char *)(addr&PAGE_MASK), PAGE_SIZE);
	return 1;
}

void clear_mmfiles(task_t *t, int exiting)
{
	/* If we are exiting, we decrease the count of shared MMFs
	 * and we always remove private MMFs */
	/* Then free the mmf_*_space */
	mmf_t *mf = t->mm_files;
	unsigned int *pd = (unsigned *)t->pd;
	/* Unlink memory */
	lock_scheduler();
	task_t *q = kernel_task;
	while(q && q->mmf_share_space != t->mmf_share_space) q=q->next;
	if(!q)
		kfree(t->mmf_share_space);
	lock_scheduler();
	int S = PAGE_DIR_IDX(MMF_SHARED_START/PAGE_SIZE);
	int E = PAGE_DIR_IDX(MMF_SHARED_END/PAGE_SIZE);
	int i;
	for(i=S;i<E;i++) {
		if(!q && pd[i])
		{
			self_free_table(i);
			pm_free_page(pd[i]&PAGE_MASK);
		}
		pd[i]=0;
	}
	t->mmf_share_space=0;
	unlock_scheduler();
	while(mf)
	{
		change_count(mf, -1);
		mmf_t *n = mf->next;
		remove_mmf(mf);
		mf=n;
	}
	kfree(t->mmf_priv_space);
}

void mmf_sync()
{
	task_t *t = (task_t *)kernel_task;
	while(t)
	{
		check_mmf_and_flush(t, -1);
		t=t->next;
	}
}
