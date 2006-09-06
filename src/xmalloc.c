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
 */

/*\
 * xmalloc.c
 *
 * Derived from openssh
 *
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
\*/

#include <stdlib.h>
#include <string.h>

#define _USE_GLIBC_MALLOC
#include "xmalloc.h"
#include "log.h"

#ifndef USE_DMALLOC

void *xmalloc(size_t size)
{
	void *ptr;

	if (!size)
		fatal("xmalloc: zero size");
	ptr = malloc(size);
	if (!ptr)
		fatal("xmalloc: out of memory (allocating %lu bytes)", (u_long) size);
	return ptr;
}

/*
 * xzalloc
 *
 * Mallocs `size' bytes and sets them to zero
 */
void *xzalloc(size_t size)
{              
	void *ptr;

	ptr=xmalloc(size);
	memset(ptr, 0, size);
	return ptr;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	if (!size || !nmemb)
		fatal("xcalloc: zero size");
	ptr=calloc(nmemb, size);
	if (!ptr)
		fatal("xcalloc: out of memory (allocating %lu bytes * %lu blocks)",
				(u_long) size, (u_long) nmemb);
	return ptr;
}

void _xfree(void *ptr)
{
	if (!ptr)
		fatal("xfree: NULL pointer given as argument");
	free(ptr);
}

void __xrealloc(void *ptr, size_t new_size)
{
	void *new_ptr;

	if (!(new_ptr=realloc(ptr, new_size)))
		fatal("xrealloc: out of memory "
			"(new_size %lu bytes)", (u_long) new_size);
	return new_ptr;
}

/*
 * xrealloc
 *
 * acts as the glibc realloc(3) function
 */
void *xrealloc(void *ptr, size_t new_size)
{
	void *new_ptr;

	if (!new_size && !ptr)
		fatal("xrealloc: NULL ptr and zero size");

	if (!ptr)
		new_ptr = xmalloc(new_size);
	else if(!new_size)
		new_ptr = xfree(ptr);
	else
		new_ptr = __xrealloc(ptr, new_size);
	
	return new_ptr;
}

char *xstrndup(const char *str, size_t n)
{
	size_t len;
	char *cp;

	len=strlen(str);
	if(len > n && n > 0)
		len=n;
	
	cp=xmalloc(len+1);
	strncpy(cp, str, len);
	cp[len]=0;

	return cp;
}

char *xstrdup(const char *str)
{
	return xstrndup(str, 0);
}

#endif /*USE_DMALLOC*/
