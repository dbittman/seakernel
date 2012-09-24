#ifndef K_LL_H
#define K_LL_H
#include <types.h>
struct llistnode {
	struct llistnode *next, *prev;
	void *entry;
};

struct llist {
	struct llistnode *head;
	mutex_t lock;
	char mallocd;
};

#define ll_entry(type,node) ((type)node->entry)

#define ll_for_each(list,curnode) \
		for(curnode=0; (curnode != 0 ? (curnode != list->head) : (addr_t)(curnode=list->head)); curnode=curnode->next)

#define ll_for_each_entry(list,curnode,type,entry) \
		for(curnode=0; ((curnode != 0 ? (curnode != list->head) : (addr_t)(curnode=list->head)) && (entry=ll_entry(type, curnode))); curnode=curnode->next)

#define ll_for_each_safe(list,curnode,next) \
		for(curnode=0; ((curnode != 0 ? (curnode != list->head) : (addr_t)(curnode=list->head)) && (addr_t)(next=curnode->next)); curnode=next)

#define ll_for_each_entry_safe(list,curnode,next,type,entry) \
		for(curnode=0; ((curnode != 0 ? (curnode != list->head) : (addr_t)(curnode=list->head)) && (entry=ll_entry(type, curnode)) && (addr_t)(next=curnode->next)); curnode=next)

#warning "ll_for_each* does not zero out entry or curnode after a completely traversal of the list!!!"

struct llist *ll_create(struct llist *list);
void ll_destroy(struct llist *list);
void ll_remove(struct llist *list, struct llistnode *node);
struct llistnode *ll_insert(struct llist *list, void *entry);

#endif
