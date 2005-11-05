/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * llist.c: Various functions to handle linked lists
 */

#ifndef LLIST_C
#define LLIST_C

/*
 * This struct is used as a header to handle all the linked list struct.
 * The only limitation is to put _EXACTLY_ struct bla_bla *next;
 * struct bla_bla *prev; at the beginning.
 */

struct linked_list
{
	struct linked_list *next;
	struct linked_list *prev;
}linked_list;
typedef struct linked_list l_list;

#define is_list_zero(list)						\
({									\
 	int _i, _r=1;							\
	char *_a=(char *)(list);					\
	for(_i=0; _i<sizeof(typeof(*(list))); _i++, _a++)		\
		if(*_a)	{						\
			_r=0;						\
			break;						\
		}							\
	_r;								\
})

#define list_copy(list, new)						\
do{									\
	l_list _o, *_l=(l_list *)(list);				\
	_o.prev=_l->prev;						\
	_o.next=_l->next;						\
	memcpy((list), (new), sizeof(typeof(*(list))));			\
	_l->prev=_o.prev;						\
	_l->next=_o.next;						\
} while(0)


#define list_init(list, new)						\
do {									\
	l_list *_l;							\
	if((new))							\
		(list)=(new);						\
	else								\
		(list)=(typeof (list))xmalloc(sizeof(typeof(*(list))));	\
	_l=(l_list *)(list);						\
	_l->prev=0; 							\
	_l->next=0;							\
	if((new))							\
		list_copy((list), (new));				\
	else								\
		memset((list), 0, sizeof(typeof(*(list)))); 		\
} while (0)
				     
#define list_last(list)							\
({									\
	 l_list *_i;							\
	 for(_i=(l_list *)(list); _i->next; _i=(l_list *)_i->next); 	\
 	 _i;								\
})

#define list_add(list, new)						\
do{									\
	l_list *_i, *_n;						\
	_i=(l_list *)list_last((list)); 				\
	_n=(l_list *)(new);						\
	_i->next=_n;							\
	_n->prev=_i; 							\
	_n->next=0;							\
}while(0)
/*	if(is_list_zero((list)))				 	\
		list_copy((list), (new));				\
	else {								\
	}								\
*/

#define list_free(list)							\
do {									\
	memset((list), 0, sizeof(typeof(*(list)))); 			\
	xfree((list));							\
} while(0)


/* It returns the new head of the linked list, so it must be called in this
 * way: head=list_del(head, list); */
#define list_del(head, list)						\
({									\
	l_list *_list=(l_list *)(list), *_next=0;			\
	l_list *_head=(l_list *)(head); 				\
	if(_list->prev)							\
		_list->prev->next=_list->next; 				\
	if(_list->next)	{						\
		_next=_list->next; 					\
		_next->prev=_list->prev; 				\
	}								\
        list_free(_list); 						\
	if(_head == _list)						\
		_next;							\
	else 								\
		(_next=_head);						\
	(typeof((list)))_next;						\
})

#define list_ins(list, new)						\
do {									\
	l_list *_l=(l_list *)(list), *_n=(l_list *)(new);		\
	if(_l->next)							\
		_l->next->prev=_n;					\
	_n->next=_l->next;						\
	_l->next=_n;							\
	_n->prev=_l;							\
} while (0)
	
#define list_for(i) for(; (i); (i)=(typeof (i))(i)->next)

/*
 * list_safe_for: Use this for if you want to do something like:
 * 	list_for(list)
 * 		list_del(list);
 * If you are trying to do that, the normal list_for isn't safe! That's
 * because when list_del will free the current `list', list_for will use a
 * wrong list->next pointer, which doesn't exist at all.
 * list_for_del is the same as list_for but it takes a second parameter, which
 * is a list pointer. This pointer will be used to store, before executing the
 * code inside the for, the list->next pointer.
 * So this is a safe for.
 */
#define list_safe_for(i, next)						\
for((i) ? (next)=(typeof (i))(i)->next : 0; (i); 			\
		(i)=(next), (i) ? (next)=(typeof (i))(i)->next : 0)


/* 
 * list_pos: returns the `pos'-th struct present in `list'
 */
#define list_pos(list, pos)						\
({									\
	 int _i=0;							\
 	 l_list *_x=(l_list *)(list);					\
 	 list_for(_x) { 						\
 	 	if(_i==(pos)) 						\
 			break;						\
		else 							\
 			_i++;						\
 	 } 								\
 	 (typeof((list)))_x;						\
})

/*
 * list_get_pos: returns the position of the `list' struct in the `head'
 * linked list, so that list_pos(head, list_get_pos(head, list)) == list
 * If `list' is not present in the linked list, -1 is returned.
 */
#define list_get_pos(head, list)					\
({									\
 	int _i=0, _e=0;							\
	l_list *_x=(l_list *)(head);					\
									\
	list_for(_x) {							\
		if(_x == (l_list *)(list)) {				\
			_e=1;						\
			break;						\
		} else							\
			_i++;						\
	}								\
	_e ? _i : -1;							\
})

#define list_destroy(list)						\
do{ 									\
	l_list *_x=(l_list *)(list), *_i, *_next;			\
	_i=_x;								\
	_next=_i->next;							\
	for(; _i; _i=_next) {						\
		_next=_i->next; 					\
		list_del(_x, _i);					\
	}								\
	(list)=0;							\
}while(0)


/*
 * Here below there are the defines for the linked list with a counter.
 * The arguments format is:
 * l_list **_head, int *_counter, l_list *list
 */

#define clist_add(_head, _counter, _list)				\
do{									\
	if(!(*(_counter)) || !(*(_head)))				\
		list_init(*(_head), (_list));				\
	else								\
		list_add(*(_head), (_list));				\
	(*(_counter))++;						\
}while(0)

#define clist_del(_head, _counter, _list)				\
do{									\
	if((*(_counter)) > 0) {						\
		*((_head))=list_del(*(_head), (_list));			\
		(*(_counter))--;					\
	}								\
}while(0)

/* 
 * Zeros the `counter' and set the head pointer to 0.
 * usage: head=clist_init(&counter);
 */
#define clist_init(_counter)						\
({									\
	*(_counter)=0;							\
	0;								\
})

#endif /*LLIST_C*/
