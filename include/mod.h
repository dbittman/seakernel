#ifndef MOD_H
#define MOD_H
#include <multiboot.h>
#include <task.h>

#define _MOD_FAIL  0
#define _MOD_GO    1
#define _MOD_AGAIN 2

typedef struct module_s {
	char *base;
	int length;
	addr_t entry;
	addr_t exiter;
	char name[128];
	char path[128];
	char deps[256];
	struct module_s *next;
} module_t;

#define add_kernel_symbol(x) {_add_kernel_symbol( (intptr_t)x, #x);}

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
void _add_kernel_symbol(const intptr_t func, const char * funcstr);
extern module_t *modules;
int remove_kernel_symbol(char *);
int sys_load_module(char *path, char *args, int flags);
int sys_unload_module(char *path, int flags);
intptr_t find_kernel_function(char * unres);
int load_deps_c(char *);

#endif
