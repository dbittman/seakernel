#ifndef __SEA_KOBJ_H
#define __SEA_KOBJ_H

#include <sea/mm/kmalloc.h>
#include <sea/string.h>
/* TODO: roll this out to all kernel objects */
#define KOBJ_CREATE(obj,flags,alloc_flag) do {\
	if(!obj) {\
		obj = kmalloc(sizeof(*obj)); \
		obj->flags = flags | alloc_flag; \
	} else {\
		memset(obj, 0, sizeof(*obj)); \
		obj->flags = flags; \
	} \
	} while(0)

#define KOBJ_DESTROY(obj,alloc_flag) do {\
	if(obj->flags & alloc_flag)\
		kfree(obj);\
	} while(0)

#endif

