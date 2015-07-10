#include <sea/kernel.h>
#if (CONFIG_MODULES)
#include <sea/fs/inode.h>
#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/mm/dma.h>
#include <sea/loader/module.h>
#include <sea/loader/elf.h>
#include <sea/loader/symbol.h>
#include <sea/loader/module.h>
#include <sea/sys/fcntl.h>
#include <sea/errno.h>
#include <sea/cpu/cpu-io.h>
#include <sea/vsprintf.h>
#include <sea/fs/kerfs.h>
#include <sea/mm/kmalloc.h>
#include <sea/ll.h>

struct llist module_list;
static mutex_t sym_mutex;
static kernel_symbol_t export_syms[MAX_SYMS];

void loader_init_kernel_symbols(void)
{
	uint32_t i;
	for(i = 0; i < MAX_SYMS; i++)
		export_syms[i].ptr = 0;
	ll_create(&module_list);
	mutex_create(&sym_mutex, 0);
	/* symbol functions */
	loader_add_kernel_symbol(loader_find_kernel_function);
	loader_add_kernel_symbol(loader_remove_kernel_symbol);
	loader_add_kernel_symbol(loader_do_add_kernel_symbol);
	/* basic kernel functions */
	loader_add_kernel_symbol(panic_assert);
	loader_add_kernel_symbol(panic);
	loader_do_add_kernel_symbol((addr_t)&kernel_state_flags, "kernel_state_flags");
	loader_add_kernel_symbol(printk);
	loader_add_kernel_symbol(kprintf);
	loader_add_kernel_symbol(snprintf);
	loader_add_kernel_symbol(memset);
	loader_add_kernel_symbol(memcpy);
	loader_add_kernel_symbol(memmove);
	loader_add_kernel_symbol(strlen);
	loader_add_kernel_symbol(strncpy);
	loader_add_kernel_symbol(strncmp);
	loader_add_kernel_symbol(_strcpy);
	loader_add_kernel_symbol(ll_insert);
	loader_add_kernel_symbol(ll_remove_entry);
	loader_add_kernel_symbol(ll_do_create);
	loader_add_kernel_symbol(ll_remove);
	loader_add_kernel_symbol(ll_destroy);
	loader_add_kernel_symbol(queue_create);
	loader_add_kernel_symbol(queue_dequeue);
	loader_add_kernel_symbol(queue_enqueue);
	loader_add_kernel_symbol(queue_destroy);
	loader_add_kernel_symbol(inb);
	loader_add_kernel_symbol(outb);
	loader_add_kernel_symbol(inw);
	loader_add_kernel_symbol(outw);
	loader_add_kernel_symbol(inl);
	loader_add_kernel_symbol(outl);
	loader_add_kernel_symbol(mutex_create);
	loader_add_kernel_symbol(mutex_destroy);
	loader_add_kernel_symbol(__mutex_release);
	loader_add_kernel_symbol(__mutex_acquire);
	loader_add_kernel_symbol(__rwlock_acquire);
	loader_add_kernel_symbol(rwlock_release);
	loader_add_kernel_symbol(__rwlock_escalate);
	loader_add_kernel_symbol(rwlock_create);
	loader_add_kernel_symbol(rwlock_destroy);

	/* these systems export these, but have no initialization function */
	loader_add_kernel_symbol(time_get_epoch);
}

const char *loader_lookup_module_symbol(addr_t addr, char **modname)
{
	rwlock_acquire(&module_list.rwl, RWL_READER);
	struct llistnode *node;
	module_t *mq, *found = 0;
	ll_for_each_entry(&module_list, node, module_t *, mq) {
		if((addr_t)addr >= (addr_t)mq->base && (addr_t)addr < ((addr_t)mq->base + mq->length)) {
			found = mq;
			break;
		}
	}
	/* Determine if are being depended upon or if we can unload */
	rwlock_release(&module_list.rwl, RWL_READER);
	if(!found || found->sd.strtab == -1 || found->sd.symtab == -1)
		return 0;
	return arch_loader_lookup_module_symbol(found, addr, modname);
}

void loader_do_add_kernel_symbol(const intptr_t func, const char * funcstr)
{
	uint32_t i;
	if(func < (addr_t)&kernel_start)
		panic(0, "tried to add invalid symbol %x:%s\n", func, funcstr);
	mutex_acquire(&sym_mutex);
	for(i = 0; i < MAX_SYMS; i++)
	{
		if(!export_syms[i].ptr)
			break;
	}
	if(i >= MAX_SYMS)
		panic(0, "ran out of space on symbol table");
	export_syms[i].name = funcstr;
	export_syms[i].ptr = func;
	mutex_release(&sym_mutex);
}

intptr_t loader_find_kernel_function(char * unres)
{
	uint32_t i;
	mutex_acquire(&sym_mutex);
	for(i = 0; i < MAX_SYMS; i++)
	{
		if(export_syms[i].ptr &&
			strlen(export_syms[i].name) == strlen(unres) &&
			!memcmp((uint8_t*)export_syms[i].name, (uint8_t*)unres,
					(int)strlen(unres))) {
			mutex_release(&sym_mutex);
			return export_syms[i].ptr;
		}
	}
	mutex_release(&sym_mutex);
	return 0;
}

int loader_remove_kernel_symbol(char * unres)
{
	uint32_t i;
	mutex_acquire(&sym_mutex);
	for(i = 0; i < MAX_SYMS; i++)
	{
		if(export_syms[i].ptr &&
			strlen(export_syms[i].name) == strlen(unres) &&
			!memcmp((uint8_t*)export_syms[i].name, (uint8_t*)unres,
					(int)strlen(unres)))
		{
			export_syms[i].ptr=0;
			mutex_release(&sym_mutex);
			return 1;
		}
	}
	mutex_release(&sym_mutex);
	return 0;
}

int loader_module_is_loaded(char *name)
{
	struct llistnode *node;
	module_t *m;
	rwlock_acquire(&module_list.rwl, RWL_READER);
	ll_for_each_entry(&module_list, node, module_t *, m) {
		if(!strcmp(m->name, name))
		{
			rwlock_release(&module_list.rwl, RWL_READER);
			return 1;
		}
	}
	rwlock_release(&module_list.rwl, RWL_READER);
	return 0;
}

static int load_module(char *path, char *args, int flags)
{
	if(!path)
		return -EINVAL;
	if(!(flags & 2)) printk(KERN_DEBUG, "[mod]: Loading Module '%s'\n", path);
	int i, pos=-1;
	module_t *tmp = (module_t *)kmalloc(sizeof(module_t));
	char *r = strrchr(path, '/');
	if(r) r++; else r = path;
	strncpy(tmp->name, r, 128);
	strncpy(tmp->path, path, 128);
	if(loader_module_is_loaded(tmp->name)) {
		kfree(tmp);
		return -EEXIST;
	}
	if(flags & 2) {
		kfree(tmp);
		return 0;
	}
	/* Open and test */
	int desc=sys_open(path, O_RDWR);
	if(desc < 0)
	{
		kfree(tmp);
		return -ENOENT;
	}
	/* Determine the length */
	struct stat sf;
	sys_fstat(desc, &sf);
	int len = sf.st_size;
	/* Allocate the space and read into it */
	char *mem = (char *)kmalloc(len);
	sys_read(desc, 0, mem, len);
	sys_close(desc);
	/* Fill out the slot info */
	tmp->exiter=0;
	/* Call the elf parser */
	void *res = loader_parse_elf_module(tmp, (unsigned char *)mem);
	kfree(mem);
	if(!res)
	{
		kfree(tmp);
		return -ENOEXEC;
	}
	ll_do_insert(&module_list, &tmp->listnode, tmp);
	printk(0, "[mod]: loaded module '%s' @[%x - %x]\n", path, tmp->base, tmp->base + len);
	return ((int (*)(char *))tmp->entry)(args);
}

static int do_unload_module(char *name, int flags)
{
	/* Is it going to work? */
	struct llistnode *node;
	module_t *m, *module = 0;
	rwlock_acquire(&module_list.rwl, RWL_READER);
	ll_for_each_entry(&module_list, node, module_t *, m) {
		if(!strcmp(m->name, name)) {
			module = m;
			break;
		}
	}
	rwlock_release(&module_list.rwl, RWL_READER);

	if(!module) {
		return -ENOENT;
	}
	/* Determine if are being depended upon or if we can unload */
	/* Call the unloader */
	printk(KERN_INFO, "[mod]: Unloading Module '%s'\n", name);
	int ret = 0;
	if(module->exiter)
		ret = ((int (*)())module->exiter)();
	/* Clear out the resources */
	kfree(module->base);

	ll_do_remove(&module_list, &module->listnode, 0);

	kfree(module);
	return ret;
}

static int unload_module(char *name)
{
	if(!name)
		return -EINVAL;
	return do_unload_module(name, 0);
}

void loader_unload_all_modules(void)
{
	/* Unload all loaded modules */
	int todo=1;
	int pass=1;
	while(todo--) {
		if(pass == 10) {
			kprintf("[mod]: Unloading modules...pass 10...fuck it.\n");
			return;
		}
		kprintf("[mod]: Unloading modules pass #%d...\n", pass++);
		int i;
		/* okay, so this only ever gets called during the shutdown of the system, so
		 * don't both locking the module list */
		struct llistnode *node, *next;
		module_t *m;
		ll_for_each_entry_safe(&module_list, node, next, module_t *, m) {
			int r = do_unload_module(m->name, 0);
			if(r < 0 && r != -ENOENT)
				todo++;
		}
	}
}

void loader_init_modules(void)
{
}

int sys_load_module(char *path, char *args, int flags)
{
	if(current_process->effective_uid)
		return -EPERM;
	return load_module(path, args, flags);
}

int sys_unload_module(char *path, int flags)
{
	if(current_process->effective_uid)
		return -EPERM;
	return do_unload_module(path, flags);
}

int kerfs_module_report(size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"    MODULE SIZE(KB) ADDRESS\n");
	struct llistnode *node;
	module_t *m;
	rwlock_acquire(&module_list.rwl, RWL_READER);
	ll_for_each_entry(&module_list, node, module_t *, m) {
		KERFS_PRINTF(offset, length, buf, current,
				"%10s %8d %x\n",
				m->name, m->length / 1024, (addr_t)m->base);
	}
	rwlock_release(&module_list.rwl, RWL_READER);

	return current;
}

#endif
