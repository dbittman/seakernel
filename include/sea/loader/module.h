#ifndef __SEA_LOADER_MODULE_H
#define __SEA_LOADER_MODULE_H

#include <sea/types.h>
#include <sea/loader/elf.h>
#include <sea/lib/linkedlist.h>

#include <sea/loader/symbol.h>
#define _MOD_FAIL  0
#define _MOD_GO    1
#define _MOD_AGAIN 2

struct module {
	char *base;
	long length;
	addr_t entry;
	addr_t exiter;
	char name[128];
	char path[128];
	struct section_data sd;
	struct linkedentry listnode;
};

void loader_unload_all_modules();
void loader_init_modules();
struct module *loader_module_free_to_unload(struct module *i);
int sys_load_module(char *path, char *args, int flags);
int sys_unload_module(char *path, int flags);
bool loader_module_is_loaded(char *name);
const char *loader_lookup_module_symbol(addr_t addr, char **);
const char *arch_loader_lookup_module_symbol(struct module *, addr_t addr, char **);

#endif
