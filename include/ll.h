#ifndef K_LL_H
#define K_LL_H
#include <types.h>
#include <rwlock.h>
struct llistnode {
	struct llistnode *next, *prev;
	void *entry;
};

struct llist {
	struct llistnode *head;
	rwlock_t rwl;
	char flags;
};

#define LL_ACTIVE   1
#define LL_ALLOC    2
#define LL_LOCKLESS 4
#define ll_is_active(list) ((list)->flags & LL_ACTIVE)
#define ll_is_empty(list) (!((list)->head))
#define ll_entry(type,node) ((type)((node)->entry))

#define ll_for_each(list,curnode) \
		for(curnode=0; (curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(list)->head)); curnode=curnode->next)

#define ll_for_each_entry(list,curnode,type,_entry) \
		for(curnode=0; ((curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(list)->head)) && (_entry=ll_entry(type, curnode))); curnode=curnode->next)

#define ll_for_each_safe(list,curnode,_next) \
		for(curnode=0; ((curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(list)->head)) && (addr_t)(_next=curnode->next)); curnode=_next)

#define ll_for_each_entry_safe(list,curnode,_next,type,_entry) \
		for(curnode=0; ((curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(list)->head)) && (_entry=ll_entry(type, curnode)) && (addr_t)(_next=curnode->next)); curnode=_next)

#define ll_maybe_reset_loop(list,cur,next) \
		if((list)->head == cur) next=0

#define ll_create(a) ll_do_create(a, 0)
#define ll_create_lockless(a) ll_do_create(a, LL_LOCKLESS)

struct llist *ll_do_create(struct llist *list, char flags);
void ll_destroy(struct llist *list);
void ll_do_remove(struct llist *list, struct llistnode *node, char);
void ll_remove(struct llist *list, struct llistnode *node);
void ll_remove_entry(struct llist *list, void *search);
struct llistnode *ll_insert(struct llist *list, void *entry);
struct llist *ll_create_lockless(struct llist *list);

#endif
