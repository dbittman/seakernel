#ifndef __SEA_LOADER_MODULE_H
#define __SEA_LOADER_MODULE_H

#include <types.h>

#define _MOD_FAIL  0
#define _MOD_GO    1
#define _MOD_AGAIN 2

typedef struct module_s {
	char *base;
	long length;
	addr_t entry;
	addr_t exiter;
	char name[128];
	char path[128];
	char deps[256];
	struct module_s *next;
} module_t;

static inline void write_deps(char *b, char *str)
{
	int i=0;
	while(b && str && *(str+i))
	{
		*(b+i) = *(str+i);
		i++;
	}
	*(b+i)=0;
}

void loader_unload_all_modules();
void loader_init_modules();
module_t *loader_module_free_to_unload(module_t *i);
int sys_load_module(char *path, char *args, int flags);
int sys_unload_module(char *path, int flags);
int loader_module_is_loaded(char *name);
extern module_t *modules;

#endif
