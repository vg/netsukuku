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
 * xargz.c
 *
 * Wrappers to the argz functions
 */

#include <argz.h>

#include "xargz.h"
#include "common.h"

error_t xargz_add(char **argz, size_t *argz_len, const char *str)
{
	if(argz_add(argz, argz_len, str))
		fatal(ERROR_MSG "out of memory", ERROR_FUNC);

	return 0;
}

error_t xargz_add_sep(char **argz, size_t *argz_len, const char *str, int delim)
{
	if(argz_add_sep(argz, argz_len, str, delim))
		fatal(ERROR_MSG "out of memory", ERROR_FUNC);

	return 0;
}

error_t xargz_append(char **argz, size_t *argz_len, const char *buf, size_t buf_len)
{
	if(argz_append(argz, argz_len, buf, buf_len))
		fatal(ERROR_MSG "out of memory", ERROR_FUNC);

	return 0;
}

error_t xargz_create(char * const argv[], char **argz, size_t *argz_len)
{
	if(argz_create(argv, argz, argz_len))
		fatal(ERROR_MSG "out of memory", ERROR_FUNC);

	return 0;
}

error_t xargz_create_sep(const char *str, int sep, char **argz, size_t *argz_len)
{
	if(argz_create_sep(str, sep, argz, argz_len))
		fatal(ERROR_MSG "out of memory", ERROR_FUNC);

	return 0;
}

error_t xargz_insert (char **argz, size_t *argz_len, char *before, const char *entry)
{
	if(argz_insert(argz, argz_len, before, entry))
		fatal(ERROR_MSG "out of memory", ERROR_FUNC);

	return 0;
}

error_t xargz_replace(char **argz, size_t *argz_len, const char *str, 
			const char *with, unsigned int *replace_count)
{
	if(argz_replace(argz, argz_len, str, with, replace_count))
		fatal(ERROR_MSG "out of memory", ERROR_FUNC);

	return 0;
}

inline size_t xargz_count(const char *argz, size_t argz_len)
{
	return argz_count(argz, argz_len);
}

inline void xargz_extract(char *argz, size_t argz_len, char  **argv)
{
	return argz_extract(argz, argz_len, argv);
}

inline char * xargz_next(char *argz, size_t argz_len, const char *entry)
{
	return argz_next(argz, argz_len, entry);
}

inline void xargz_stringify(char *argz, size_t len, int sep)
{
	return argz_stringify(argz, len, sep);
}

inline void xargz_delete(char **argz, size_t *argz_len, char *entry)
{
	return argz_delete(argz, argz_len, entry);
}
