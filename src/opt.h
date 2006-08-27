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

#ifndef OPT_H
#define OPT_H

#include "xargz.h"
#include "llist.c"

#define MAX_OPT_SZ			(32+1)
#define MAX_OPT_DESC_SZ			(1024+1)


/*
 * OPT_GET_INT_VALUE
 *
 * This macro is used to retrieve the numeric value assigned to an option.
 * For example, let "speed" be one valid option defined with
 * add_option():
 *
 * 	int speed_value;
 *
 * 	OPT_GET_INT_VALUE("speed", my_conf_options, speed_value);
 */
#define OPT_GET_INT_VALUE(opt, opt_head, n)				\
({									\
	char *_val;							\
	_val=opt_get_value((opt), opt_head);				\
	if(_val)							\
		(n)=atoi(_val);						\
})

/*
 * OPT_GET_STRN_VALUE
 *
 * Sets the `str' pointer to the address of the string which contains
 * the value of the `_opt' option.
 * Fatal() is called if `string' of the value contains more than `maxbytes'
 * bytes.
 *
 * 	char *filename;
 *
 * 	OPT_GET_STRN_VALUE("pid_file", my_conf_options, filename, PATH_MAX);
 */
#define OPT_GET_STRN_VALUE(_opt, opt_head, str, maxbytes)		\
do {									\
	ntkopt *_o=opt_find_opt((_opt), (opt_head));			\
	if(_o && _o->value) {						\
 		if(_o->valsz > (maxbytes)+1)				\
			fatal("The value assigned to the \"%s\" option"	\
			      " is too long", _o->opt);			\
		(str) = _o->value;					\
	}								\
} while(0)

/*
 * OPT_GET_ARGZ_VALUE
 *
 * Sets the `str' pointer to the address of the argz vector which contains
 * the value of the `_opt' option. `str_sz' is set to the size of the argz
 * vector.
 *
 * 	char *modules;
 * 	size_t modules_sz;
 *
 * 	OPT_GET_ARGZ_VALUE("load_module", my_conf_options, modules, 
 * 				modules_sz);
 */
#define OPT_GET_ARGZ_VALUE(_opt, opt_head, str, str_sz)			\
do {									\
	ntkopt *_o=opt_find_opt((_opt), (opt_head));			\
	if(_o && _o->value) {						\
		(str) 	 = _o->value;					\
		(str_sz) = _o->valsz;					\
	}								\
} while(0)


/* 
 * ntkopt
 *
 * The linked list which contains the names and the values of registered 
 * options.
 */
struct ntk_opt
{
	LLIST_HDR	(struct ntk_opt);

	
	const char	*opt;		/* Option name */
	const char	*short_opt;	/* Option short name */
	const char	*desc;		/* Option description */

	char		*value;		/* It's an argz vector: all the strings
					   contained in `value' are separated by 
					   the character '\0'. Each string is a
					   value */
	size_t		valsz;		/* Size of `value' */
};
typedef struct ntk_opt ntkopt;


/*\
 *  * * *  Functions' declaration  * * *
\*/

void opt_add_option(const char *opt, const char *desc, ntkopt **opt_head);
ntkopt *opt_find_opt(const char *opt_name, ntkopt *opt_head);
char *opt_get_value(const char *opt_name, ntkopt *opt_head);
int opt_add_value(const char *opt_name, char *value, ntkopt *opt_head);
int opt_append_argz_value(const char *opt_name, char *value, size_t valsz,
				ntkopt *opt_head);
int opt_replace_value(const char *opt_name, char *value, ntkopt *opt_head);
void opt_free_value(ntkopt *opt);
void opt_clear_values(ntkopt *opt_head);
void opt_zero_values(ntkopt *opt_head);
void opt_close(ntkopt **opt_head);

#endif /*OPT_H*/
