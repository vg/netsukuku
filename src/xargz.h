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

#ifndef XARGZ_H
#define XARGZ_H

error_t xargz_add(char **argz, size_t *argz_len, const char *str);
error_t xargz_add_sep(char **argz, size_t *argz_len, const char *str, int delim);
error_t xargz_append(char **argz, size_t *argz_len, const char *buf, size_t buf_len);
error_t xargz_create(char * const argv[], char **argz, size_t *argz_len);
error_t xargz_create_sep(const char *str, int sep, char **argz, size_t *argz_len);
error_t xargz_delete(char **argz, size_t *argz_len, char *entry);
error_t xargz_insert (char **argz, size_t *argz_len, char *before, const char *entry);
error_t xargz_replace(char **argz, size_t *argz_len, const char *str, 
			const char *with, unsigned int *replace_count);

size_t xargz_count(const char *argz, size_t argz_len)
		__attribute__ ((weak, alias ("argz_count")));

void xargz_extract(char *argz, size_t argz_len, char  **argv)
		__attribute__ ((weak, alias ("argz_extract")));

char * xargz_next(char *argz, size_t argz_len, const char *entry)
		__attribute__ ((weak, alias ("argz_next")));

void xargz_stringify(char *argz, size_t len, int sep)
		__attribute__ ((weak, alias ("argz_stringify")));

#endif /*XARGZ_H*/
