/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
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
	(list)=(typeof (list))xmalloc(sizeof(typeof(*(list))));		\
	_l=(l_list *)(list);						\
	_l->prev=0; 							\
	_l->next=0;							\
	memset((list), 0, sizeof(typeof(*(list)))); 			\
	if((new))							\
		list_copy((list), (new));				\
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
		_i=(l_list *)list_last((list)); 			\
		_n=(l_list *)(new);					\
		_i->next=_n;						\
		_n->prev=_i; 						\
		_n->next=0;						\
}while(0)
/*	if(is_list_zero((list)))				 	\
		list_copy((list), (new));				\
	else {								\
	}								\
*/

#define list_free(list)							\
do {									\
	xfree((list));							\
} while(0)

#define list_del(list)							\
do{ 									\
	l_list *_list=(l_list *)(list); 				\
	if(_list->prev)							\
		_list->prev->next=_list->next; 				\
	if(_list->next)							\
		_list->next->prev=_list->prev; 				\
        list_free(_list); 						\
}while(0)

#define list_ins(list, new)						\
do {									\
	l_list *_l=(l_list *)(list), *_n=(l_list *)(new);		\
	if(_l->next)							\
		_l->next->prev=_n;					\
	if(_l->prev)							\
		_l->prev->next=_n;					\
	_n->next=_l->next;						\
	_n->prev=_l->prev;						\
	list_del((list));						\
} while (0)
	
#define list_for(i) for(; (i); (i)=(typeof (i))(i)->next)
				   
#define list_pos(list,pos)						\
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

#define list_destroy(list)						\
do{ 									\
	l_list *_x=(l_list *)(list), *_i, *_next;			\
	_i=_x;								\
	_next=_i->next;							\
	for(; _i; _i=_next) {						\
		_next=_i->next; 					\
		list_del(_i);						\
	}								\
}while(0)
