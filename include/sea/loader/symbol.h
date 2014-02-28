#ifndef __SEA_LOADER_SYMBOL_H
#define __SEA_LOADER_SYMBOL_H

#include <types.h>
#include <mutex.h>

#define MAX_SYMS 600

typedef struct { 
	const char *name; 
	addr_t ptr; 
	int flag;
} kernel_symbol_t;

#define loader_add_kernel_symbol(x) loader_do_add_kernel_symbol((addr_t)x, #x)

extern mutex_t sym_mutex;
extern kernel_symbol_t export_syms[MAX_SYMS];

void loader_do_add_kernel_symbol(const intptr_t func, const char * funcstr);
intptr_t loader_find_kernel_function(char * unres);
void loader_init_kernel_symbols(void);
int loader_remove_kernel_symbol(char * unres);


#endif
