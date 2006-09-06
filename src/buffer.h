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
 * Various functions to manipulate buffers
 */

#ifndef BUFFER_H
#define BUFFER_H

/*
 * In bzero(3):
 * <<4.3BSD.  This function [bzero] is deprecated -- use memset in new 
 *   programs.>>
 */
#define setzero(_p, _sz)	memset((_p), 0, (_sz))

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
 *   * * *  Functions declaration  * * *
\*/
int is_bufzero(const void *a, int sz);

#endif /*BUFFER_H*/
