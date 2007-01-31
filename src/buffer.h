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
 * Various functions to manipulate buffers and arrays (see {-bufarr_t-}).
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
 * The incremented `__dst' pointer is returned.
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
 * The incremented `__src' pointer is returned.
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
 * Buffer Array							|{bufarr_t}|
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
 * `nmemb' is the number of elements stored in the array. buf[*nmemb-1] is the
 * last one.
 *
 * `nalloc' is the number of allocated elements of the array. It can be
 * greater than `nmemb'. 
 *
 *
 * The macros defined below are used to manipulate a buffer array. Their
 * generic prototype is:
 *
 * 	atype **_buf, int *_nmemb, int *_nalloc
 *
 * Where `_buf' is a pointer to a `buf' pointer described above.
 * `_nmemb' is a pointer to a `nmemb' variable,
 * `_nalloc' is a pointer to a `nalloc' variable.
 * Note that a macro may modify `*_buf', `*_nmemb' or `*_nalloc'.
 * 
 *
 * WARNING: do not use expression as arguments to these macros. For example,
 * 	    DO NOT use array_replace(.., pos++, ...), but instead
 * 	    array_replace(.., pos, ...); pos++;
 *
 * Optional arguments
 * ------------------
 *
 * `_nmemb' and `_nalloc' must to two different variables.
 * If you don't want to specify `_nalloc', set it to 0.
 * If the `nalloc' argument passed to a macro is set to 0, then
 * the macro will assume that  *nmemb == *nalloc  and it will only 
 * modify `*nmemb'.
 *
 * Notes on loops and buffer deletion
 * ----------------------------------
 *
 * Consider the following loop:
 *
 * 	for(i=0; i<nmembs; i++)
 *		* Remove buffer[i] 
 * 		array_del(&buffer, &nmembs, i);
 *
 * What will happen?
 * The loop will skip over some elements. That's why:
 *
 * 	the loop starts;
 * 	i=0;
 * 	buffer[0] is removed:
 *
 *	the array is shifted to the left by one:
 *		buffer[1] replaces buffer[0],
 *		buffer[2] replaces buffer[1],
 *		and so on... until
 *   	        buffer[nmembs-1] replaces buffer[nmembs-2]
 *
 *      nmembs is decremented by one
 *
 * 	the loop continues;
 * 	i=1;
 *  [!]	buffer[1] is removed, however buffer[1] was substituted with
 *      buffer[2]! Thus, the original buffer[1] is not removed.
 *
 *      ...
 *
 * The correct loop is:
 *
 * 	
 * 	for(i=0; i<nmembs; i++) {
 *		* Remove buffer[i] 
 * 		array_del(&buffer, &nmembs, i);
 * 		* Adjust the index
 * 		i--; 
 * 	}
 *
 * Note also that `nmembs' is modified during the call to `array_del'.
 *
 * If you want to simply destroy the array, use {-array_destroy-}
\*/

/*
 * array_replace
 * -------------
 *
 * Copies the data pointed by _new to (*_buf)[_pos] and returns &(*_buf)[pos]
 * Note: `_pos' must be < `*nalloc', otherwise an overflow will occur.
 *
 * Usage:
 * 	array_replace(&buf_array_ptr, pos, new_element_ptr);
 */
#define array_replace(_buf, _pos, _new)					\
({									\
	void *_ar_ptr=(*(_buf))[(_pos)];				\
	memcpy(_ar_ptr, (_new), sizeof(typeof(*(_new))));		\
	(typeof(*(_buf)) _ar_ptr;					\
})

/*
 * array_insert
 * ------------
 *
 * The same of {-array_replace-}, but with an additional check.
 * :fatal(): is called if _pos is greater than *_nalloc
 *
 * Usage:
 * 	array_insert(&buf_array_ptr, &nalloc_var, new_elem_ptr);
 */
#define array_insert(_buf, _nalloc, _pos, _new)				\
({									\
 	if(_pos >= *(_nalloc) || _pos < 0)				\
		fatal(ERROR_MSG"Array overflow: _nalloc %d, _pos %d",	\
			ERROR_POS, _nalloc, _pos);			\
									\
	array_replace(_buf, _pos, _new);				\
})


/*
 * array_grow
 * ----------
 *
 * It enlarges the array by allocating `_count' elements and appending them at
 * its end.
 * `*_nalloc' is incremented and `*_buf' is set to point to the new start of the 
 * array (since :xrealloc(): is used).
 *
 * If the `_nalloc' argument is set to 0, then this macro will ignore the
 * `_count' argument assuming that `*_nmemb' == `*_nalloc'+`*_count' and it 
 * won't increment anything.
 *
 * Note: if `_count' is a negative value, the array is shrinked (see
 * {-array_shrink-})
 *
 * Usage:
 * 	array_grow(&buf_array_ptr, &nmemb_var, &nalloc_var, count);
 * or
 * 	array_grow(&buf_array_ptr, &nmemb_var_already_incremented, 0, 0);
 */
#define array_grow(_buf, _nmemb, _nalloc, _count)                       \
do {									\
 	typeof(_nmemb) _ag_na = (_nalloc);				\
	typeof(*(_nmemb)) _ag_fake_na;					\
	if(!_ag_na) {							\
		_ag_fake_na = *(_nmemb);				\
		_ag_na = &_ag_fake_na;					\
	}								\
	(*_ag_na)+=(_count);						\
 	if(*_ag_na < 0)							\
		fatal(ERROR_MSG"Array underflow: _count %d",		\
			ERROR_POS, _count);				\
	*(_buf)=xrealloc(*(_buf), sizeof(typeof(**(_buf))) * (*_ag_na));\
} while(0)

/*
 * array_shrink
 * ------------
 *
 * Deallocates and destroys the last `_count' elements of the array.
 * `*_buf' is set to point to the new start of the array (since :xrealloc():
 * is used).
 *
 * If the `_nalloc' argument is set to 0, then this macro will assume that 
 * `*_nmemb' == `*_nalloc'-`*_count' and it won't increment anything.
 *
 * Usage:
 * 	array_shrink(&buf_array_ptr, &nmemb_var, &nalloc_var, count);
 * or
 * 	array_shrink(&buf_array_ptr, &nmemb_var_already_decremented, 0, 0);
 */
#define array_shrink(_buf, _nmemb, _nalloc, _count)			\
		array_grow(_buf, _nmemb, _nalloc, -abs(_count))

/*
 * Private macro. See {-array_add-} and {-array_add_more-} below.
 */
#define array_add_grow(_buf, _nmemb, _nalloc, _new, _newalloc)		\
({									\
 	(*(_nmemb))++;							\
									\
	if(!(_nalloc) || *(_nmemb) > *(_nalloc))			\
		array_grow(_buf, _nmemb, _nalloc, _newalloc);		\
									\
	array_replace(_buf, (*(_nmemb))-1, _new);			\
)}


/*
 * array_add
 * ---------
 *
 * Adds a new element at the end of the array and copies in it
 * the data pointed by `_new'.
 *
 * `*_nmemb' is always incremented by one. If necessary, a new 
 * element is allocated at the end of the array and `*_nalloc' is incremented
 * by one.
 *
 * The pointer to the new inserted element is returned.
 *
 * Exception:
 * if the `_nalloc' argument is set to 0, then this macro will assume that 
 * `*_nmemb' == `*_nalloc', and only `*_nmemb' will be incremented.
 *
 * Usage:
 * 	array_add(&buf_array_ptr, &nmemb_var, &nalloc_var, new_elem_ptr);
 * or
 * 	array_add(&buf_array_ptr, &nmemb_var, 0, new_element_ptr);
 */
#define array_add(_buf, _nmemb, _nalloc, _new)				\
		array_add_grow(_buf, _nmemb, _nalloc, _new, 1)

/*
 * array_add_more
 * --------------
 *
 * The same of {-array_add-}, but instead of allocating just one new element, it
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
 * i.e. we copy (*_buf)[*_nmemb-1] over (*_buf)[_pos]. In this way, the last 
 * element of the array is reusable.
 * `*_nmemb' is decremented by one.
 *
 * Note that this macro doesn't deallocate anything, see {-array_del_free-} for
 * that.
 *
 * If `_pos' isn't a valid value, an array overflow occurs and :fatal(): is
 * called.
 */
#define array_del(_buf, _nmemb, _pos)					\
do {									\
	if(_pos >= *(_nmemb) || _pos < 0)				\
		fatal(ERROR_MSG "Array overflow: _nmemb %d, _pos %d",	\
			ERROR_POS, *(_nmemb), *(_pos));			\
	(*(_nmemb))--;							\
	if(_pos < *(_nmemb))						\
		memcpy( (*(_buf))[(_pos)], (*(_buf))[*(_nmemb)], 	\
				sizeof(typeof(**(_buf))) );		\
} while(0)

/*
 * array_del_free
 * --------------
 *
 * The same of {-array_del-}, but deallocates one element from the array.
 * `*_buf' is set to point to the new start of the array (since :xrealloc():
 * is used).
 *
 * Exception:
 * if the `_nalloc' argument is set to 0, then this macro will assume that 
 * `*_nmemb' == `*_nalloc', and only `*_nmemb' will be decremented.
 *
 * Usage:
 * 	array_del_free(&buf_array_ptr, &nmemb_var, &nalloc_var, pos);
 * or
 * 	array_del_free(&buf_array_ptr, &nmemb_var, 0, pos);
 */
#define array_del_free(_buf, _nmemb, _nalloc, _pos)			\
do {									\
	array_del(_buf, _nmemb, _pos);					\
	array_shrink(_buf, _nmemb, _nalloc, 1);				\
} while(0)

/*
 * array_rem
 * ---------
 *
 * Deletes the element at position `_pos'.
 * The element is deleted by shifting to the left all its successive elements.
 * In this way, the order of the array is preserved.
 * This is useful if the array has been ordered with qsort(3), however this is
 * less efficient than {-array_del-}.
 *
 * Note that this macro doesn't deallocate anything, see {-array_rem_free-} for
 * that. Moreover, once the array is shifted, the data stored in 
 * (*_buf)[*_nmemb] is not touched. You may want to zero it. Example:
 *
 * 	array_rem(&buf_array_ptr, &nmemb_var, pos);
 * 	setzero( buf_array_ptr[nmemb_var], sizeof(typeof(*buf_array_ptr)) );
 *
 * If `_pos' isn't a valid value, an array overflow occurs and :fatal(): is
 * called.
 */
#define array_rem(_buf, _nmemb, _pos)					\
do {									\
	if(_pos >= *(_nmemb) || _pos < 0)				\
		fatal(ERROR_MSG"Array overflow: _nmemb %d, _pos %d",	\
			ERROR_POS, *(_nmemb), _pos);			\
	/* Shift the array */						\
	if(_pos < (*(_nmemb))-1)					\
                memmove( &(*(_buf))[_pos], &(*(_buf))[_pos+1],		\
                      sizeof(typeof(**(_buf))) * ((*(_nmemb))-_pos-1));	\
	(*(_nmemb))--;							\
} while(0)

/*
 * array_rem_free
 * --------------
 *
 * The same of {-array_rem-}, but deallocates one element from the array.
 * `*_buf' is set to point to the new start of the array (since :xrealloc():
 * is used).
 */
#define array_rem_free(_buf, _nmemb, _nalloc, _pos)			\
do {									\
	array_rem(_buf, _nmemb, _pos);					\
	array_shrink(_buf, _nmemb, _nalloc, 1);				\
} while(0)

/*
 * array_destroy
 * -------------
 *
 * Deallocates the whole array, setting `*_nmemb' and `*_nalloc' to zero.
 * `_nmemb' or `_nalloc' can be 0.
 * 
 * Usage:
 * 	array_destroy(&buf_array_ptr, &nmemb_var, &nalloc_var);
 * or
 * 	array_destroy(&buf_array_ptr, 0, 0);
 */
#define array_destroy(_buf, _nmemb, _nalloc)				\
do {									\
	if( *(_buf) && (!(_nmemb) || *(_nmemb) > 0) )			\
		xfree(*(_buf));						\
	_nmemb && *(_nmemb)=0;						\
	_nalloc && *(_nalloc)=0;					\
} while(0)

/*
 * array_bzero
 * -----------
 *
 * Sets `_count' elements of the array to 0. 
 * If `_count' == -1, then the whole array is zeroed.
 * If `_count' > `*_nalloc', :fatal(): is called.
 *
 * Usage:
 * 	array_bzero(&buf_array_ptr, &nalloc_var, count);
 * or
 * 	array_bzero(&buf_array_ptr, &nalloc_var, -1);
 */
#define array_bzero(_buf, _nalloc, _count)				\
do {									\
	if((_count) > *(_nalloc))					\
		fatal(ERROR_MSG "Array overflow: _count %d",		\
			ERROR_POS, (_count));				\
									\
	setzero(*(_buf), sizeof(typeof(**(_buf))) * 			\
				(_count == -1 ? *(_nalloc) : _count) );	\
} while(0)


/*\
 *   * * *  Functions declaration  * * *
\*/
int is_bufzero(const void *a, int sz);

#endif /*BUFFER_H*/
