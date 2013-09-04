#ifndef _SYMBOL_H
#define _SYMBOL_H

#include <types.h>

#define MAX_SYMS 600

typedef struct { 
	const char *name; 
	addr_t ptr; 
	int flag;
} kernel_symbol_t;

#define add_kernel_symbol(x) _add_kernel_symbol((addr_t)x, #x)

extern mutex_t sym_mutex;
extern kernel_symbol_t export_syms[MAX_SYMS];

void _add_kernel_sym_user(const addr_t func, const char * funcstr);
void _add_kernel_symbol(const addr_t func, const char * funcstr);
addr_t find_kernel_function_user(char * unres);
addr_t find_kernel_function(char * unres);
void init_kernel_symbols(void);
int remove_kernel_symbol(char * unres);


#endif
