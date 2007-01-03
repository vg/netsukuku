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
 * buffer.c
 *
 * Various functions to manipulate buffers and arrays (see :bufarr_t:).
 */

#ifndef BUFFER_H
#define BUFFER_H

/*
 * In bzero(3):
 * <<4.3BSD.  This function [bzero] is deprecated -- use memset in new 
 *   programs.>>
 */
#define setzero(_p, _sz)	memset((_p), 0, (_sz))

/*\
 *
 * Buffer macros
 * =============
 *
 * The following macros are used to put/get data in a buffer.
 *
\*/


/*
 * memput
 *
 * It copies `__sz' bytes from `__src' to `__dst' and then increments the `__dst'
 * pointer of `__sz' bytes.
 *
 * It incremented `__dst' pointer is returned.
 *
 * Note: you can also use side expression, f.e.	
 * 	 memput(buf++, src, (size+=sizeof(src));
 */
#define memput(__dst, __src, __sz)					\
({ 									\
 	void **_mp_pdst=(void **)&(__dst);				\
 	void *_mp_dst=*_mp_pdst, *_mp_src=(__src);			\
 	size_t _mp_sz=(size_t)(__sz);					\
 									\
	memcpy(_mp_dst, _mp_src, _mp_sz);				\
	(*_mp_pdst)+=(_mp_sz);						\
	_mp_dst;							\
})

/*
 * memget
 *
 * the same of memput(), but it increments `__src' instead.
 *
 * It incremented `__src' pointer is returned.
 */
#define memget(__dst, __src, __sz)					\
({ 									\
 	void **_mp_psrc=(void **)&(__src);				\
 	void *_mp_dst=(__dst), *_mp_src=*_mp_psrc;			\
 	size_t _mp_sz=(size_t)(__sz);					\
 									\
	memcpy(_mp_dst, _mp_src, _mp_sz);				\
	(*_mp_psrc)+=(_mp_sz);						\
	_mp_src;							\
})

/* use of hardcoded `_src' and `_dst' variable names */
#define bufput(_src, _sz)	(memput(buf, (_src), (_sz)))
#define bufget(_dst, _sz)	(memget((_dst), buf, (_sz)))



/*\
 *
 * Buffer Array
 * ============
 *
 * A buffer array is defined by these three elements:
 * 
 * 	atype *buf, int nmemb, int nalloc
 * 
 * `atype' is the type the elements of the array, f.e. it can be "int",
 * "double" or whatever.
 *
 * `buf' is the pointer to the start of the array, i.e. the first element.
 *
 * `nmemb' is the number of elements stored in the array. buf[nmemb-1] is the
 * last one.
 *
 * `nalloc' is the number of allocated elements of the array. It can be
 * greater than `nmemb'.
 *
 *
 * The following macros are used to manipulate a buffer array.
 * WARNING: do not use expression as arguments to these macros. For example,
 * DO NOT use array_replace(.., pos++, ...), but instead
 * array_replace(.., pos, ...); pos++;
 *
\*/
#define bufarr_t

/*
 * array_replace
 * ------------
 *
 * Copies the data pointed by _new to _buf[_pos] and returns &_buf[pos]
 * Note: `_pos' must be < `_nmemb', otherwise an overflow will occur.
 */
#define array_replace(_buf, _nmemb, _nalloc, _pos, _new)		\
({									\
	void *_ptr=(_buf)[(_pos)];					\
	memcpy(_ptr, (_new), sizeof(typeof(*(_new))));			\
	_ptr;								\
})

/*
 * array_insert
 * ------------
 *
 * The same of :array_replace:, but with an additional check.
 * :fatal(): is called is _pos is greater than _nmemb
 */
#define array_insert(_buf, _nmemb, _nalloc, _pos, _new)			\
({									\
 	if((_pos) >= (_nmemb) || (_pos) < 0)				\
		fatal(ERROR_MSG"Array overflow: _nmemb %d, _pos %d",	\
			ERROR_POS, (_nmemb), (_pos));			\
									\
	array_replace((_buf), (_nmemb), (_nalloc), (_pos), (_new));	\
})


/*
 * array_grow
 * ----------
 *
 * It enlarges the array by allocating `_count' elements and appending them at
 * its end.
 * _nalloc is incremented and the new pointer to the start of the array is
 * returned.
 *
 * Usage:
 *	buf_array_grow(buf, nmemb, nalloc, count);
 *
 * Note: if `_count' is a negative value, the array is shrinked (see
 * :array_shrink:)
 */
#define array_grow(_buf, _nmemb, _nalloc, _count)                       \
({									\
 	ssize_t _na=(_nalloc);						\
	(_nalloc) = ( _na+=(_count) );					\
 	if(_na < 0)							\
		fatal(ERROR_MSG"Array overflow: _count %d",		\
			ERROR_POS, _count);				\
	(_buf)=xrealloc((_buf), sizeof(typeof(*(_buf))) * _na);		\
)}

/*
 * array_shrink
 * ------------
 *
 * Deallocates and destroys the last `_count' elements of the array.
 * The new pointer to the start of the array is returned.
 *
 * Usage:
 *	buf_array_shrink(buf, nmemb, nalloc, count);
 */
#define array_shrink(_buf, _nmemb, _nalloc, _count)			\
		array_grow(_buf, _nmemb, _nalloc, -abs(_count))

/*
 * Private macro. See :array_add: and :array_add_more: below.
 */
#define array_add_grow(_buf, _nmemb, _nalloc, _new, _newalloc)		\
({									\
 	(_nmemb)++;							\
									\
	if((_nmemb) > (_nalloc))					\
		array_grow((_buf), (_nmemb), (_nalloc), (_newalloc));	\
									\
	array_insert((_buf), (_nmemb), (_nalloc), (_nmemb)-1, (_new));	\
)}


/*
 * array_add
 * ---------
 *
 * Adds a new element at the end of the array and copies in it
 * the data pointed by `_new'.
 *
 * _nmemb is incremented by one.
 * If necessary, a new element is allocated at the end of the
 * array and _nalloc is incremented by one.
 *
 * The pointer to the new inserted element is returned.
 */
#define array_add(_buf, _nmemb, _nalloc, _new)				\
		array_add_grow(_buf, _nmemb, _nalloc, _new, 1)

/*
 * array_add_more
 * --------------
 *
 * The same of :array_add:, but instead of allocating just one new element, it
 * allocates _nmemb/2+1 elements.
 * This is useful if array_add_more() is called many times.
 */
#define array_add_more(_buf, _nmemb, _nalloc, _new)			\
		array_add_grow(_buf, _nmemb, _nalloc, _new, (_nmemb)/2+1)


/*
 * array_del
 * ---------
 *
 * Deletes the element at position `_pos'.
 * The element is deleted by copying the last element of the array over it,
 * i.e. we copy _buf[_nmemb-1] over _buf[_pos]. In this way, the last element
 * of the array is reusable.
 *
 * Note that this macro doesn't deallocate anything, see :array_del_free: for
 * that.
 *
 * If `_pos' isn't a valid value, an array overflow occurs and :fatal(): is
 * called.
 */
#define array_del(_buf, _nmemb, _nalloc, _pos)				\
do {									\
	if((_pos) >= (_nmemb) || (_pos) < 0)				\
		fatal(ERROR_MSG"Array overflow: _nmemb %d, _pos %d",	\
			ERROR_POS, ((_nmemb)), ((_pos)));		\
	(_nmemb)--;							\
	if((_pos) < (_nmemb))						\
		memcpy( (_buf)[(_pos)], (_buf)[(_nmemb)], 		\
				sizeof(typeof(*((_buf)))) );		\
} while(0)

/*
 * array_del_free
 * --------------
 *
 * The same of :array_del:, but deallocates one element from the array.
 * The new pointer to the start of the array is returned.
 *
 * Usage:
 *	buf_del_free(buf, nmemb, nalloc, pos);
 */
#define array_del_free(_buf, _nmemb, _nalloc, _pos)			\
({									\
	array_del((_buf), (_nmemb), (_nalloc), (_pos));			\
	array_shrink((_buf), (_nmemb), (_nalloc), 1);			\
})

/*
 * array_destroy
 * -------------
 *
 * Deallocates the whole array, setting _nmemb and _nalloc to zero.
 */
#define array_destroy(_buf, _nmemb, _nalloc)				\
do {									\
	if((_buf) && (_nmemb))						\
		xfree((_buf));						\
	(_nmemb)=0;							\
	(_nalloc)=0;							\
} while(0)


/*\
 *   * * *  Functions declaration  * * *
\*/
int is_bufzero(const void *a, int sz);

#endif /*BUFFER_H*/
