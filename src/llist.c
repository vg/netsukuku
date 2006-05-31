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
 * --
 *
 * llist.c: Various functions to handle (doubled) linked lists
 * 
 * Why did I used macros for all these functions?
 * Well, I just love the blue-smurf-like syntax highlighting of Vim ^_-
 *
 * Moreover using `typeof' and other nice tricks each macro can recognize the
 * size of the arguments, in this way we can have functions like `list_copy',
 * `list_init'.
 * Finally, performance is optimised. I don't trust gcc and its -O2, (and they
 * don't trust me).
 */

#ifndef LLIST_C
#define LLIST_C

/*
 * This struct is used as a header to handle all the linked list struct.
 * The only limitation is to put _EXACTLY_ 
 * 	struct bla_bla *next;
 * 	struct bla_bla *prev; 
 * at the beginning.
 * You can also use the LLIST_HDR() macro.
 */

#define LLIST_HDR(_struct)	_struct *next, *prev

struct linked_list
{
	LLIST_HDR	(struct linked_list);
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

/*
 * list_copy
 *
 * It copies just the contents of `new' in `list' without overwriting the
 * ->prev and ->next pointers.
 */
#define list_copy(list, new)						\
do{									\
	l_list _o, *_l=(l_list *)(list);				\
	_o.prev=_l->prev;						\
	_o.next=_l->next;						\
	memcpy((list), (new), sizeof(typeof(*(list))));			\
	_l->prev=_o.prev;						\
	_l->next=_o.next;						\
} while(0)

/*
 * list_init
 *
 * If `new' is 0, it stores in `list' a pointer to a newly mallocated 
 * and zeroed struct.
 * Its type is the same of `list'.
 * If `new' is non zero, then `list' is simply set to `new'.
 * The ->prev and ->next pointers are always set to zero.
 */
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
	if(!(new))							\
		memset((list), 0, sizeof(typeof(*(list)))); 		\
} while (0)

/*
 * list_last
 *
 * It returns the last element of the `list' llist
 */
#define list_last(list)							\
({									\
	 l_list *_i=(l_list *)(list);					\
	 for(; _i && _i->next; _i=(l_list *)_i->next);			\
 	 (typeof((list)))_i;						\
})

/*
 * list_append
 *
 * It appends the `_new' struct at the end of the `_head' llist.
 * If `_tail' is not 0, then it is considered as the end of the llist and the
 * `_head' argument isn't required.
 * If `_tail' is 0, the end will be reached by traversing the entire llist,
 * starting from `_head'.
 * If both `_tail' and `_head' are 0, `new->next' and `new->prev' will be set
 * to 0.
 * The new tail is always returned.
 * Example: tail=list_append(0, tail, new);
 * 	    or
 * 	    list_append(head, 0, new);
 */
#define list_append(_head, _tail, _new)					\
({									\
	l_list *_t, *_n;						\
	_t=(_tail) ? (_tail) : (l_list *)list_last((_head)); 		\
	_n=(l_list *)(_new);						\
 	if(_n != _t) {							\
 		if(_t)							\
			_t->next=_n;					\
		_n->prev=_t; 						\
		_n->next=0;						\
	}								\
	_new;								\
})

/*
 * list_add
 *
 * It adds the `_new' struct at the start of the `_head' llist.
 * The new head of the llist is returned.
 * 
 * Example: 
 * 		head=list_add(head, new);
 */
#define list_add(_head, _new)						\
({									\
 	l_list *_h, *_n;						\
 	_h=(l_list *)(_head);						\
 	_n=(l_list *)(_new);						\
 	if(_h != _n) {							\
		_n->next=_h;						\
		_n->prev=0;						\
 		if(_h)							\
			_h->prev=_n;					\
	}								\
	(typeof((_head)))_n;						\
})
 
/*
 * list_join: 
 * before list_join(list):
 * 	... <-> A <-> list <-> C <-> ...
 * after:
 * 	... <-> A <-> C <-> ...
 * Note that `list' is not freed!
 * It returns the head of the list, so call it in this way:
 * 	head=list_join(head, list);
 */
#define list_join(head, list)						\
({									\
	l_list *_l=(l_list *)(list), *_h=(l_list *)(head), *_ret;	\
	if(_l->next)							\
		_l->next->prev=_l->prev;				\
	if(_l->prev)							\
		_l->prev->next=_l->next;				\
	_ret = _l == _h ? _l->next : _h;				\
	(typeof((head)))_ret;						\
})

#define list_free(list)							\
do {									\
	memset((list), 0, sizeof(typeof(*(list)))); 			\
	xfree((list));							\
} while(0)

/* 
 * list_del
 * 
 * It's the same of list_join() but it frees the `list'.
 * It returns the new head of the linked list, so it must be called in
 * this way: head=list_del(head, list); 
 */
#define list_del(head, list)						\
({									\
	l_list *_list=(l_list *)(list), *_head=(l_list *)(head); 	\
 									\
 	_head=(l_list *)list_join((head), _list);			\
        list_free(_list); 						\
	(typeof((head)))_head;						\
})

/*
 * list_ins
 *
 * It inserts the `new' struct between the `list' and `list'->next 
 * structs.
 */
#define list_ins(list, new)						\
do {									\
	l_list *_l=(l_list *)(list), *_n=(l_list *)(new);		\
	if(_l->next)							\
		_l->next->prev=_n;					\
	_n->next=_l->next;						\
	_l->next=_n;							\
	_n->prev=_l;							\
} while (0)
	
/* 
 * list_substitute
 *
 * It removes from the llist the `old_list' struct and inserts in its
 * position the `new_list' struct
 * Note that `old_list' isn't freed, it is just unlinked from the llist.
 */
#define list_substitute(old_list, new_list)				\
do{									\
	l_list *_n, *_o;						\
	_n=(l_list *)(new_list);					\
	_o=(l_list *)(old_list);					\
	if(_o->next != _n)						\
		_n->next=_o->next;					\
	if(_o->prev != _n)						\
		_n->prev=_o->prev;					\
	if(_n->next)							\
		_n->next->prev=_n;					\
	if(_n->prev)							\
		_n->prev->next=_n;					\
}while(0)

/*
 * list_swap
 *
 * Before:	H -> Y <-> [A] <-> I <-> [B] <-> P <-> T
 * list_swap(A, B);
 * After:	H -> Y <-> [B] <-> I <-> [A] <-> P <-> T	
 */
#define list_swap(a, b)							\
do{									\
	l_list _ltmp, *_a, *_b;						\
	_a=(l_list *)(a);						\
	_b=(l_list *)(b);						\
	if(_a->next == _b) {						\
		list_substitute(_a, _b);				\
		list_ins(_b, _a);					\
	} else if(_a->prev == _b) {					\
		list_substitute(_b, _a);				\
		list_ins(_a, _b);					\
	} else {							\
		_ltmp.next=(l_list *)_b->next;				\
		_ltmp.prev=(l_list *)_b->prev;				\
		list_substitute(_a, _b);				\
		list_substitute(&_ltmp, _a);				\
	}								\
}while(0)

/*
 * list_moveback
 *
 * It swaps `list' with its previous struct
 */
#define list_moveback(list)						\
do{									\
	l_list *_l=(l_list *)(list);					\
	if(_l->prev)							\
		list_swap(_l->prev, _l);				\
}while(0)

/*
 * list_movefwd
 *
 * It swaps `list' with its next struct
 */
#define list_movefwd(list)						\
do{									\
	l_list *_l=(l_list *)(list);					\
	if(_l->next)							\
		list_swap(_l->next, _l);				\
}while(0)
 
/* 
 * list_moveontop
 *
 * `_list' must be already present in the `_head' llist, otherwise this
 * function is just equal to list_add().
 * 
 * list_moveontop() moves `_list' on top of the `_head' llist, thus:
 * 	- `_list' is joined (see list_join() above)
 * 	- `_list' becomes the new head
 * 	- `_head' goes in `_list'->next
 * The new head of the llist is returned.
 * 
 * Example:	head=list_moveontop(head, list);
 */
#define list_moveontop(_head, _list)					\
({									\
 	l_list *_h=(l_list *)(_head), *_l=(l_list *)(_list);		\
 									\
 	if(_h != _l) {							\
		_h=(l_list *)list_join((typeof((_head)))_h, _l);	\
		_h=(l_list *)list_add((typeof((_head)))_h, _l);		\
	}								\
	(typeof((_head)))_h;						\
})

/*
 * list_for
 *
 * Pretty simple, use it in this way:
 *	 my_llist_ptr *index;
 *	 index=head_of_the_llist;
 *	 list_for(index) {
 *	 	do_something_with(index);
 *	 }
 *
 * WARNING: do not use expressions!! For example:
 *    DO NOT THIS
 * 	list_for(index=head_of_the_llist) {
 * 		...
 * 	}
 *    DO NOT THIS
 * In this case `index' will be set to `head_of_the_llist' each time and
 * you'll get an infinite loop;
 */
#define list_for(i) for(; (i); (i)=(typeof (i))(i)->next)

/*
 * list_count
 *
 * Returns the number of structs present in the llist
 */
#define list_count(_head)						\
({									\
	l_list *_l=(l_list *)(_head);					\
	int _i=0; 							\
									\
	list_for(_l)							\
		_i++;							\
	_i;								\
})


/*
 * list_safe_for
 *
 * Use this for if you want to do something like:
 * 	list_for(list)
 * 		list_del(list);
 * If you are trying to do that, the normal list_for isn't safe! That's
 * because when list_del will free the current `list', list_for will use a
 * wrong list->next pointer, which doesn't exist at all.
 * list_for_del is the same as list_for but it takes a second parameter, which
 * is a list pointer. This pointer will be used to store, before executing the
 * code inside the for, the list->next pointer.
 * So this is a safe for.
 *
 * WARNING: do not use expressions in the arguments, (see the warning above
 * for the normal list_for)
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

/* 
 * list_destroy
 *
 * It traverse the entire `list' llist and calls list_del() on each 
 * struct.
 */
#define list_destroy(list)						\
do{ 									\
	l_list *_x=(l_list *)(list), *_i, *_next;			\
	_i=_x;								\
	if(_i)								\
		_next=_i->next;						\
	for(; _i; _i=_next) {						\
		_next=_i->next; 					\
		list_del(_x, _i);					\
	}								\
	(list)=0;							\
}while(0)


/*
 * list_copy_some
 *
 * It calls the `check_func' function for each struct of the `list' llist,
 * passing to it, as first argument, a pointer to the struct itself.
 * The other parameters to list_copy_some are passed to `check_func', for
 * example by calling:
 * 	list_copy_some(list, my_check_func, arg1, arg2, arg3);
 * the `my_check_func' will be called in this way:
 * 	my_check_func(arg1, arg2, arg3);
 * 
 * If the `check_func' function returns a non zero value, the struct is 
 * copied in a new allocated space.
 * All the copied structs form the replicated llist.
 * Its head is returned.
 * 
 * The `list' llist is not modified.
 */
#define list_copy_some(list, check_func, func_args...)			\
({									\
 	l_list *_new=0, *_head=0, *_tail=0, *_l=(l_list *)(list);	\
									\
	list_for(_l) {							\
 		if(!check_func(((typeof((list)))_l), ## func_args ))	\
 			continue;					\
 									\
		_new=xmalloc(sizeof(typeof(*(list))));			\
 		if(!_head) _head=_new;					\
 									\
		memcpy(_new, _l, sizeof(typeof(*(list))));		\
		_tail=list_append(0, _tail, _new);			\
	}								\
 									\
 	(typeof((list)))_head;						\
})

/*
 * list_copy_all
 *
 * It copies the entire `list' llist in a new allocated one.
 * It returns the head to the replicated llist.
 * The `list' llist is not modified.
 */
#define list_copy_all_yes(_p)	(1)
#define list_copy_all(list)	list_copy_some((list), list_copy_all_yes)
 
/*
 * Here below there are the definitions for the linked list with a counter.
 * The arguments format is:
 * l_list **_head, int *_counter, l_list *list
 */

#define clist_add(_head, _counter, _list)				\
do{									\
	if(!(*(_counter)) || !(*(_head)))				\
		list_init(*(_head), (_list));				\
	else								\
		*((_head))=list_add(*(_head), (_list));			\
	(*(_counter))++;						\
}while(0)

#define clist_append(_head, _tail, _counter, _list)			\
do{									\
									\
	l_list *_t=0, **_targv=(l_list **)(_tail);			\
	if(_targv)							\
		_t=*_targv;						\
	if(!(*(_counter)) || !(*(_head)))				\
		list_init(*(_head), (_list));				\
	else {								\
		_t=(l_list *)list_append(*(_head), _t, (_list));	\
		if(_targv)						\
			(*_targv)=_t;					\
	}								\
	(*(_counter))++;                                                \
}while(0)

#define clist_del(_head, _counter, _list)				\
do{									\
	if((*(_counter)) > 0) {						\
		*((_head))=list_del(*(_head), (_list));			\
		(*(_counter))--;					\
	}								\
}while(0)

#define clist_ins(_head, _counter, _list)				\
do{									\
        if(!(*(_counter)) || !(*(_head)))				\
		clist_add((_head), (_counter), (_list));		\
        else {								\
                list_ins(*(_head), (_list));                            \
        	(*(_counter))++;					\
	}								\
}while(0)

#define clist_join(_head, _counter, _list)                              \
do{                  							\
	if((*(_counter)) > 0) {						\
		*((_head))=list_join((*(_head)), _list);		\
		(*(_counter))--;					\
	}								\
} while(0)
	

/* 
 * Zeros the `counter' and set the head pointer to 0.
 * usage: head=clist_init(&counter);
 */
#define clist_init(_counter)						\
({									\
	*(_counter)=0;							\
	0;								\
})

#define clist_destroy(_head, _counter)					\
({									\
 	list_destroy(*((_head)));					\
 	(*(_head))=0;							\
 	(*(_counter))=0;						\
 	0;								\
})

/*
 * clist_qsort
 *
 * It qsorts the `_head' llist, which has `_counter' elements.
 * The `_cmp_func' function will be used to compare two llist.
 * The new head of the llist is returned. Example:
 * 	head=clist_qsort(head, counter, my_cmp_func);
 * `counter' can be also 0, but it's better if you've counted already.
 * 
 * Btw, this function just calls qsort(3), this is how it works:
 * 	- first of all it counts how many elements there are in the llist.
 * 	  This is done only if `_counter' is 0.
 * 	- it uses a temporary callocated array to store all the pointers to the
 * 	  elements of the llist. 
 * 	  tmp[0]=_head; tmp[1]=_head->next; tmp[..]=_head->next->..
 * 	- it calls qsort(3) on the tmp array. Note that qsort gives the
 *        address of &tmp[x] and &tmp[y] to `_cmp_func()', thus `_cmp_func()'
 *        should be aware of this.
 * 	- it follows the new order of `tmp' and appends each array element in
 * 	  a new llist.
 * 	- the head of the new llist is returned.
 */
#define clist_qsort(_head, _counter, _cmp_func)				\
({									\
	l_list *_h=(l_list *)(_head), *_n, *_nhead, *_ntail;		\
	int _i=0, _c;							\
									\
	_c = !(_counter) ? list_count(_h) : (_counter);			\
	l_list *_tmp_list[_c];						\
									\
	_n=_h;								\
	list_for(_n) {							\
		_tmp_list[_i]=_n;					\
		_i++;							\
	}								\
									\
	qsort(_tmp_list, _c, sizeof(l_list *), (_cmp_func));		\
									\
	_ntail=0;							\
	_nhead=_tmp_list[0];						\
	for(_i=0; _i<_c; _i++)						\
		_ntail=(l_list *)list_append(0, _ntail, _tmp_list[i]);	\
									\
	(typeof((_head)))_nhead;					\
})									\

#endif /*LLIST_C*/
