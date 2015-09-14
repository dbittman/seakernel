#ifndef __SEA_LIB_STACK_H
#define __SEA_LIB_STACK_H

#include <sea/mutex.h>
#include <stdbool.h>
#include <sea/mm/kmalloc.h>
#define STACK_KMALLOC 1
#define STACK_LOCKLESS 2

struct stack_elem {
	void *obj;
	struct stack_elem *next, *prev;
};

struct stack {
	int flags;
	mutex_t lock;
	size_t count;
	struct stack_elem *top;
};

static inline bool stack_is_empty(struct stack *stack)
{
	return stack->count == 0;
}

struct stack *stack_create(struct stack *stack, int flags);
void stack_destroy(struct stack *stack);
void stack_push(struct stack *stack, struct stack_elem *elem, void *obj);
void *stack_pop(struct stack *stack);
void stack_delete(struct stack *stack, struct stack_elem *elem);

#endif

