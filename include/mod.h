#ifndef MOD_H
#define MOD_H
#include <multiboot.h>
#include <task.h>

#define _MOD_FAIL  0
#define _MOD_GO    1
#define _MOD_AGAIN 2

#include <types.h>
#include <task.h>

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

static inline int mod_fork(int *pid)
{
	int x = fork();
	if(x)
		*pid = x;
	return x;
}

int is_loaded(char *name);
int load_module(char *path, char *, int);
int unload_module(char *name);
void unload_all_modules();
void load_all_config_mods(char *);
module_t *canweunload(module_t *i);

extern module_t *modules;

int sys_load_module(char *path, char *args, int flags);
int sys_unload_module(char *path, int flags);

int load_deps_c(char *);

#endif
