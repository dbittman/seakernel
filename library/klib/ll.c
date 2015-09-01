/* ll.c - support functions for linked lists
 * These linked lists are stored as a linear doubly linked list, 
 * allowing us O(1) insert and remove.
 * 
 * WARNING - using the ll_for_each macros require a bit of extra work!
 * If using the 'safe' macros, and you remove a node from the list, you
 * may need to call ll_maybe_reset_loop before it reloops! */
#include <sea/kernel.h>
#include <sea/ll.h>
#include <sea/mutex.h>
#include <sea/tm/process.h>
#include <stdatomic.h>
#include <sea/mm/kmalloc.h>
struct llistnode *ll_do_insert(struct llist *list, struct llistnode *n, void *entry)
{
	if(!(list->flags & LL_LOCKLESS)) 
		rwlock_acquire(&list->rwl, RWL_WRITER); 
	assert(list->magic == LLIST_MAGIC);
	if(!list->head)
	{
		n->next = n->prev = 0;
	} else {
		n->next = list->head;
		n->prev = 0;
		list->head->prev = n;
	}
	list->head = n;
	n->entry = entry;
	n->memberof = list;
	n->magic = LLISTNODE_MAGIC;
	atomic_fetch_add_explicit(&list->num, 1, memory_order_release);
	if(!(list->flags & LL_LOCKLESS))
		rwlock_release(&list->rwl, RWL_WRITER);
	return n;
}

struct llistnode *ll_insert(struct llist *list, void *entry)
{
	assert(list && ll_is_active(list));
	struct llistnode *n = (struct llistnode *)kmalloc(sizeof(struct llistnode));
	return ll_do_insert(list, n, entry);
}

void *ll_do_remove(struct llist *list, struct llistnode *node, char locked)
{
	assert(list && node && ll_is_active(list));
	if(!(list->flags & LL_LOCKLESS) && !locked)
		rwlock_acquire(&list->rwl, RWL_WRITER);
	assert(list->magic == LLIST_MAGIC);
	assert(node->magic == LLISTNODE_MAGIC);
	if(node->memberof == list) {
		node->memberof = 0;
		atomic_fetch_sub_explicit(&list->num, 1, memory_order_release);
		if(list->head == node) {
			/* Now, is this the only node in the list? */
			if(list->head->next == 0) {
				list->head = 0;
				goto out;
			}
			list->head = node->next;
			if(node->next)
				assert(node->next->magic == LLISTNODE_MAGIC);
		}
		if(node->prev)
			assert(node->prev->magic == LLISTNODE_MAGIC);
		if(node->next)
			assert(node->next->magic == LLISTNODE_MAGIC);
		if(node->prev)
			node->prev->next = node->next;
		if(node->next)
			node->next->prev = node->prev;
		out:
		node->next = node->prev = 0;
	}
	if(!(list->flags & LL_LOCKLESS) && !locked)
		rwlock_release(&list->rwl, RWL_WRITER);
	return node;
}

void ll_remove(struct llist *list, struct llistnode *node)
{
	kfree(ll_do_remove(list, node, 0));
}

void ll_remove_entry(struct llist *list, void *search)
{
	struct llistnode *cur, *next;
	void *ent;
	if(!(list->flags & LL_LOCKLESS)) 
		rwlock_acquire(&list->rwl, RWL_WRITER);
	assert(list->magic == LLIST_MAGIC);
	ll_for_each_entry_safe(list, cur, next, void *, ent)
	{
		if(ent == search) {
			kfree(ll_do_remove(list, cur, 1));
			break;
		}
	}
	if(!(list->flags & LL_LOCKLESS)) 
		rwlock_release(&list->rwl, RWL_WRITER);
}

/* should list be null, we allocate one for us and return it. */
struct llist *ll_do_create(struct llist *list, char flags)
{
	if(list == 0) {
		list = (struct llist *)kmalloc(sizeof(struct llist));
		list->flags |= LL_ALLOC;
	} else
		memset(list, 0, sizeof(struct llist));
	rwlock_create(&list->rwl);
	list->head = 0;
	list->num = ATOMIC_VAR_INIT(0);
	list->magic = LLIST_MAGIC;
	list->flags |= (LL_ACTIVE | flags);
	return list;
}

void ll_destroy(struct llist *list)
{
	assert(list && !list->head);
	if(!ll_is_active(list))
		return;
	if(!(list->flags & LL_LOCKLESS)) 
		rwlock_acquire(&list->rwl, RWL_WRITER);
	list->flags &= ~LL_ACTIVE;
	if(!(list->flags & LL_LOCKLESS)) 
		rwlock_release(&list->rwl, RWL_WRITER);
	rwlock_destroy(&list->rwl);
	if(list->flags & LL_ALLOC)
		kfree(list);
}

void *ll_remove_head(struct llist *list)
{
	void *ent;
	if(!(list->flags & LL_LOCKLESS)) 
		rwlock_acquire(&list->rwl, RWL_WRITER);
	if(!list->num) {
		if(!(list->flags & LL_LOCKLESS)) 
			rwlock_release(&list->rwl, RWL_WRITER);
		return 0;
	}
	assert(list->magic == LLIST_MAGIC);
	assert(list->head);
	ent = list->head->entry;
	if(ent)
		kfree(ll_do_remove(list, (struct llistnode *)list->head, 1));
		
	if(!(list->flags & LL_LOCKLESS)) 
		rwlock_release(&list->rwl, RWL_WRITER);
	return ent;
}
