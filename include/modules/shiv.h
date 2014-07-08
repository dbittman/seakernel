#ifndef __MOD_SHIV_H
#define __MOD_SHIV_H
#include <sea/config.h>
#if CONFIG_MODULE_SHIV

#if CONFIG_ARCH != TYPE_ARCH_X86_64
	#error "shiv only supports x86_64"
#endif

#include <sea/types.h>

struct vmcs {
	/* u32/64? */
	int a, b;
	char data[0];
};

struct vmachine {
	int id;
	struct vmcs *vmcs_phys;
};

#endif
#endif
