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

/*This struct is used as a header to handle all the linked list struct.
 * The only limitation is to put _EXACTLY_ struct bla_bla *next;
 * struct bla_bla *prev; at the beginning
 */
struct linked_list
{
	struct linked_list *next;
	struct linked_list *prev;
}linked_list;
typedef struct linked_list l_list;

#define list_init(list)							\
do {									\
	list=(typeof (list))xmalloc(sizeof(typeof(*(list)))); 		\
	list->prev=0; 							\
	list->next=0;							\
} while (0)
				     
#define list_last(list)							\
({									\
	 l_list *_i;							\
	 for(_i=(list); _i->next; _i=(l_list *)_i->next); 		\
 	 _i;								\
})
				     
#define list_new(size)							\
({									\
 	void *_new; 							\
 	_new=(void *)xmalloc(size); 					\
 	_new;								\
})
				     
#define list_free(list)							\
do { 									\
	xfree((list));							\
} while(0)
				     
#define list_ins(list, new)						\
do{ 									\
	l_list *_list=(l_list *)(list), *_new=(l_list *)(new);		\
	_list->next->prev=_new; 					\
	_list->prev->next=_new;						\
	_new->next=_list->next; 					\
	_new->prev=_list->prev; 	  				\
	list_free(_list); 						\
}while(0)
				
#define list_add(list, new)						\
do{									\
	l_list *_i; _i=list_last((list)); 				\
	_i->next=(new); 						\
	(new)->prev=_i; 						\
	(new)->next=0;							\
}while(0)

#define list_del(list)							\
do{ 									\
	l_list *_list=(l_list *)(list); 				\
	if(_list->prev)							\
		_list->prev->next=_list->next; 				\
	if(_list->next)							\
		_list->next->prev=_list->prev; 				\
        list_free(_list); 						\
}while(0)
				   
#define list_for(i)	for(; (i); (i)=(typeof (i))(i)->next)
				   
#define list_pos(list,pos)						\
({									\
	 int _i=0;							\
 	 l_list *_x=(list); 						\
 	 list_for(_x) { 						\
 	 	if(_i==(pos)) 						\
 			break;						\
		else 							\
 			_i++;						\
 	 } 								\
 	 _x;								\
})

#define list_destroy(list)						\
do{ 									\
	l_list *_x=(list), *_i, *_next; 				\
	_i=_x; 								\
	for(; _i; _i=next) {						\
		next=_x->next; 						\
		list_del(_x);						\
	}								\
}
