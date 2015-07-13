#ifndef K_LL_H
#define K_LL_H
#include <sea/types.h>
#include <sea/rwlock.h>

#define LLISTNODE_MAGIC 0x77755533
#define LLIST_MAGIC     0x33355577

struct llistnode {
	unsigned magic;
	struct llistnode *next, *prev;
	void *entry;
	struct llist *memberof;
};

struct llist {
	unsigned magic;
	struct llistnode *head;
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
		for(curnode=(struct llistnode *)((list)->head); !(_entry=0) && (curnode); curnode=(struct llistnode *)curnode->next)

#define ll_for_each_entry(list,curnode,type,_entry) \
		for(curnode=(struct llistnode *)((list)->head); !(_entry=0) && (curnode) && (_entry=ll_entry(type, curnode)); curnode=(struct llistnode *)curnode->next)

#define ll_for_each_safe(list,curnode,_next) \
		for(curnode=(struct llistnode *)((list)->head); !(_entry=0) && (curnode) && ((_next=(struct llistnode *)(curnode->next))||1); curnode=(struct llistnode *)_next)

#define ll_for_each_entry_safe(list,curnode,_next,type,_entry) \
		for(curnode=(struct llistnode *)((list)->head); !(_entry=0) && (curnode) && (_entry=ll_entry(type, curnode)) && ((_next=(struct llistnode *)(curnode->next))||1); curnode=(struct llistnode *)_next)

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
