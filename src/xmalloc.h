/* This file is part of Netsukuku system
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
 */

#ifndef XMALLOC_H
#define XMALLOC_H

#ifdef USE_DMALLOC

#include "dmalloc.h"

#else

/* 
 * xmalloc.h: Derived from openssh
 *
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Mon Mar 20 22:09:17 1995 ylo
 *
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
 *
 * --
 *  
 * xfree, zfree macros wrapper added. AlpT
 */

/*
 * xfree
 *
 * It calls _xfree(__pptr) and then sets `__pptr' to 0.
 * It is safe, you can use it also with expressions like
 * 	xfree(a++);
 */
#define xfree(__pptr)							\
({									\
	char **_p=(char **)&(__pptr); 					\
	_xfree(*_p);							\
	*_p=0;								\
})

/*
 * zfree
 *
 * Calls :xfree: only if _zPtr is non zero
 */
#define zfree(_zPtr)							\
({									\
	char **_zP=(char **)&(_zPtr); 					\
	if(*_zP)							\
		xfree(*_zP);						\
})

/* Functions declaration */
void	*xmalloc(size_t);
void    *xzalloc(size_t size);
void	*xrealloc(void *, size_t);
void 	*xcalloc(size_t nmemb, size_t size);
void    _xfree(void *);
char 	*xstrndup(const char *str, size_t n);
char	*xstrdup(const char *);

#ifndef _USE_GLIBC_MALLOC

/* Just to be sure that all the Netsukuku code uses xmalloc */

#undef 	malloc
#define	malloc	xmalloc

#undef 	free
#define free	xfree

#undef 	realloc
#define realloc	xrealloc

#undef 	calloc
#define calloc	xcalloc

#undef 	strdup
#define strdup	xstrdup

#undef 	strndup
#define strndup	xstrndup

#endif /* _USE_GLIBC_MALLOC */

#endif /* USE_DMALLOC */

#endif /*XMALLOC_H*/
