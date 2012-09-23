#ifndef K_LL_H
#define K_LL_H

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
		for(curnode=0; (curnode != 0 ? (curnode != list->head) : (curnode=list->head)); curnode=curnode->next)

#define ll_for_each_entry(list,curnode,type,entry) \
		for(curnode=0; ((curnode != 0 ? (curnode != list->head) : (curnode=list->head)) && (entry=ll_entry(type, curnode))); curnode=curnode->next)

#define ll_for_each_safe(list,curnode,next) \
		for(curnode=0; ((curnode != 0 ? (curnode != list->head) : (curnode=list->head)) && (next=curnode->next)); curnode=next)

#define ll_for_each_entry_safe(list,curnode,next,type,entry) \
		for(curnode=0; ((curnode != 0 ? (curnode != list->head) : (curnode=list->head)) && (entry=ll_entry(type, curnode)) && next=curnode->next); curnode=next)

#endif
