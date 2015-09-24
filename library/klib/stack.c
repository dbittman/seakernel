#include <sea/lib/stack.h>
#include <sea/kobj.h>
#include <sea/spinlock.h>
#include <sea/kernel.h>
#include <sea/spinlock.h>
static struct stack_elem __sentry;

struct stack *stack_create(struct stack *stack, int flags)
{
	KOBJ_CREATE(stack, flags, STACK_KMALLOC);
	spinlock_create(&stack->lock);
	memset(&__sentry, 0, sizeof(__sentry));
	stack->top = &__sentry;
	return stack;
}

void stack_destroy(struct stack *stack)
{
	spinlock_destroy(&stack->lock);
	KOBJ_DESTROY(stack, STACK_KMALLOC);
}

void stack_push(struct stack *stack, struct stack_elem *elem, void *obj)
{
	if(!(stack->flags & STACK_LOCKLESS))
		spinlock_acquire(&stack->lock);
	
	stack->top->next = elem;
	elem->obj = obj;
	elem->next = &__sentry;
	elem->prev = stack->top;
	stack->top = elem;
	stack->count++;

	if(!(stack->flags & STACK_LOCKLESS))
		spinlock_release(&stack->lock);
}

void stack_delete(struct stack *stack, struct stack_elem *elem)
{
	if(!(stack->flags & STACK_LOCKLESS))
		spinlock_acquire(&stack->lock);
	
	assert(stack->count > 0);
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	if(stack->top == elem)
		stack->top = elem->prev;
	stack->count--;
	
	if(!(stack->flags & STACK_LOCKLESS))
		spinlock_release(&stack->lock);
}

void *stack_pop(struct stack *stack)
{
	if(!(stack->flags & STACK_LOCKLESS))
		spinlock_acquire(&stack->lock);

	if(!stack->count) {
		if(!(stack->flags & STACK_LOCKLESS))
			spinlock_release(&stack->lock);
		return NULL;
	}
	void *obj = stack->top->obj;
	stack->top->prev->next = &__sentry;
	stack->top = stack->top->prev;
	stack->count--;

	if(!(stack->flags & STACK_LOCKLESS))
		spinlock_release(&stack->lock);
	return obj;
}

