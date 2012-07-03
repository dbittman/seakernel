#include <kernel.h>
#include <elf.h>
#include <fs.h>
#include <task.h>
#include <memory.h>
#include <mod.h>
module_t *modules=0;
int load_deps(char *);
mutex_t mod_mutex;
int load_module(char *path, char *args, int flags)
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
	
	mutex_on(&mod_mutex);
	module_t *m = modules;
	while(m) {
		if(!strcmp(m->name, tmp->name))
		{
			mutex_off(&mod_mutex);
			kfree(tmp);
			return -EEXIST;
		}
		m=m->next;
	}
	mutex_off(&mod_mutex);
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
	tmp->base=mem;
	tmp->length=len;
	tmp->exiter=0;
	tmp->deps[0]=0;
	/* Time to decode the module header */
	if(!(*mem == 'M' && *(mem + 1) == 'O' && *(mem+2) == 'D'))
	{
		kfree(tmp);
		kfree(mem);
		return -EINVAL;
	}
	/* Call the elf parser */
	int res = parse_elf_module(tmp, (unsigned char *)mem+4, path);
	if(res == _MOD_FAIL || res == _MOD_AGAIN)
	{
		kfree(mem);
		kfree(tmp);
		/* try again? Maybe we loaded a dependency and need to retry */
		if(res == _MOD_AGAIN)
			return load_module(path, args, flags);
		return -ENOEXEC;
	}
	mutex_on(&mod_mutex);
	module_t *old = modules;
	modules = tmp;
	tmp->next = old;
	mutex_off(&mod_mutex);
	return ((int (*)(char *))tmp->entry)(args);
}

int load_deps_c(char *d)
{
	if(!*d) return 0;
	char *mnext, *current;
	current = d;
	int count=0;
	while(current)
	{
		mnext = strchr(current, ',');
		if(mnext)
		{
			*mnext=0;
			mnext++;
		}
		if(*current == ':')
			break;
		
		module_t *mq = modules;
		char found=0;
		while(mq) {
			if(!strcmp(current, mq->name) || !strcmp(mq->path, current))
				found=1;
			mq=mq->next;
		}
		if(!found) {
			int res;
			res = load_module(current, (char *)mtboot->cmdline, 0);
			if(res)
			{
				char ver[64];
				char tmp[strlen(current) + 64];
				get_kernel_version(ver);
				sprintf(tmp, "/sys/modules-%s/%s", ver, current);
				res = load_module(tmp, (char *)mtboot->cmdline, 0);
				if(res)
					return -1;
			}
			count++;
		}
		current = mnext;
	}
	return count;
}


/* This checks if module 'm' depends on module 'yo' */
int do_it_depend_on(module_t *yo, module_t *m)
{
	char *d = m->deps;
	if(!*d) return 0;
	char *mnext, *current;
	current = d;
	int count=0;
	while(current)
	{
		mnext = strchr(current, ',');
		if(mnext)
		{
			*mnext=0;
			mnext++;
		}
		if(*current == ':')
			break;
		if(!strcmp(yo->name, current))
			return 1;
		current = mnext;
	}
	return 0;
}

/* This makes sure module 'i' can unload and not break dependencies */
module_t *canweunload(module_t *i)
{
	module_t *mq = modules;
	while(mq) {
		if(mq != i && do_it_depend_on(i, mq))
			return mq;
		mq=mq->next;
	}
	return 0;
}

int do_unload_module(char *name)
{
	/* Is it going to work? */
	mutex_on(&mod_mutex);
	module_t *mq = modules;
	while(mq) {
		if((!strcmp(mq->name, name) || !strcmp(mq->path, name)))
			break;
		mq = mq->next;
	}
	
	if(!mq)
		return -ENOENT;
	/* Determin if are being depended upon or if we can unload */
	module_t *mo;
	if((mo = canweunload(mq))) {
		mutex_off(&mod_mutex);
		return -EINVAL;
	}
	mutex_off(&mod_mutex);
	/* Call the unloader */
	printk(KERN_INFO, "[mod]: Unloading Module '%s'\n", name);
	int ret = 0;
	if(mq->exiter)
		ret = ((int (*)())mq->exiter)();
	/* Clear out the resources */
	kfree(mq->base);
	mutex_on(&mod_mutex);
	module_t *a = modules;
	if(a == mq)
		modules = mq->next;
	else
	{
		while(a && a->next != mq)a=a->next;
		assert(a);
		a->next = mq->next;
	}
	mutex_off(&mod_mutex);
	kfree(mq);
	return ret;
}

int unload_module(char *name)
{
	if(!name)
		return -EINVAL;
	return do_unload_module(name);
}

void unload_all_modules()
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
		module_t *mq = modules;
		while(mq) {
			int r = do_unload_module(mq->name);
			if(r < 0 && r != -ENOENT) {
				todo++;
				mq = mq->next;
			} else
				mq = modules;
		}
	}
}

void init_module_system()
{
	create_mutex(&mod_mutex);
	init_kernel_symbols();
}

int sys_load_module(char *path, char *args, int flags)
{
	return load_module(path, args, flags);
}

int sys_unload_module(char *path, int flags)
{
	return unload_module(path);
}
