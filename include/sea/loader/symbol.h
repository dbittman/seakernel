#ifndef __SEA_LOADER_SYMBOL_H
#define __SEA_LOADER_SYMBOL_H

#include <stdint.h>
#include <sea/types.h>
#include <sea/mutex.h>

#define MAX_SYMS 600

#define MAX_SECTIONS 32
struct section_data {
	addr_t vbase[MAX_SECTIONS];
	int num;
	int strtab, shstrtab, symtab, symlen;
};

typedef struct { 
	const char *name; 
	addr_t ptr; 
	int flag;
} kernel_symbol_t;

extern struct section_data kernel_sections;

#define loader_add_kernel_symbol(x) loader_do_add_kernel_symbol((addr_t)x, #x)

void loader_do_add_kernel_symbol(const addr_t func, const char * funcstr);
const char *arch_loader_symbol_lookup(addr_t addr, struct section_data *sd);
intptr_t loader_find_kernel_function(char * unres);
void loader_init_kernel_symbols(void);
int loader_remove_kernel_symbol(char * unres);


#endif
