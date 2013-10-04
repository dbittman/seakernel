#ifndef K_LL_H
#define K_LL_H
#include <types.h>
#include <rwlock.h>

#define LLISTNODE_MAGIC 0x77755533
#define LLIST_MAGIC     0x33355577

struct llistnode {
	unsigned magic;
	volatile struct llistnode *next, *prev;
	void *entry;
	struct llist *memberof;
};

struct llist {
	unsigned magic;
	volatile struct llistnode *head;
	rwlock_t rwl;
	char flags;
	unsigned num;
};

#define LL_ACTIVE   1
#define LL_ALLOC    2
#define LL_LOCKLESS 4
#define ll_is_active(list) ((list)->flags & LL_ACTIVE)
#define ll_is_empty(list) (!((list)->head))
#define ll_entry(type,node) ((type)((node)->entry))

#define ll_for_each(list,curnode) \
		for(curnode=0; (list)->head && (curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(struct llistnode *)(list)->head)); curnode=(struct llistnode *)curnode->next)

#define ll_for_each_entry(list,curnode,type,_entry) \
		for(curnode=0; (list)->head && ((curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(struct llistnode *)(list)->head)) && (_entry=ll_entry(type, curnode))); curnode=(struct llistnode *)curnode->next)

#define ll_for_each_safe(list,curnode,_next) \
		for(curnode=_next=0; (list)->head && ((curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(struct llistnode *)(list)->head)) && (addr_t)(_next=(struct llistnode *)curnode->next)); curnode=_next)

#define ll_for_each_entry_safe(list,curnode,_next,type,_entry) \
		for(curnode=_next=0; (list)->head && ((curnode != 0 ? (curnode != (list)->head) : (addr_t)(curnode=(struct llistnode *)(list)->head)) && (_entry=ll_entry(type, curnode)) && (addr_t)(_next=(struct llistnode *)curnode->next)); curnode=_next)

#define ll_maybe_reset_loop(list,cur,next) \
		if((list)->head == cur) next=0

#define ll_create(a) ll_do_create(a, 0)
#define ll_create_lockless(a) ll_do_create(a, LL_LOCKLESS)

struct llistnode *ll_do_insert(struct llist *list, struct llistnode *, void *entry);
struct llist *ll_do_create(struct llist *list, char flags);
void ll_destroy(struct llist *list);
void *ll_do_remove(struct llist *list, struct llistnode *node, char);
void ll_remove(struct llist *list, struct llistnode *node);
void ll_remove_entry(struct llist *list, void *search);
struct llistnode *ll_insert(struct llist *list, void *entry);
void *ll_remove_head(struct llist *list);
#endif
