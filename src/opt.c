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
 * opt.c
 *
 * General library to handle options.
 *
 * A nice example is conf.c, which is built on top of this source.
 */

#include "includes.h"

#include "opt.h"
#include "xargz.h"
#include "common.h"

/*
 * opt_add_option
 * --------------
 *
 * registers the new option `opt', 
 * f.e. add_option("debug", "d");
 *
 * `opt' must not have more than 32 characters.
 *
 * `desc' is the description of the option, its size cannot exceed 1024 bytes.
 * If `desc' is NULL, no description will be assigned to the `opt' option.
 *
 * The new option is added in `*opt_head', which keeps the list of all the
 * valid options.
 */
void opt_add_option(const char *opt, const char *desc, ntkopt **opt_head)
{
	ntkopt *new;

	if(strlen(opt) >= MAX_OPT_SZ)
		fatal(ERROR_MSG "The `%s' option must be no more "
				"than %d characters", ERROR_FUNC,
				opt, MAX_OPT_SZ-1);
	if(desc && strlen(desc) >= MAX_OPT_DESC_SZ)
		fatal(ERROR_MSG "The description of the `%s' option"
				" must be no more than %d characters",
				ERROR_FUNC, opt, MAX_OPT_DESC_SZ-1);

	if(strchr(opt, ' '))
		fatal(ERROR_MSG "The name of the `%s' option must"
				" not contain the ' ' character", 
				ERROR_FUNC, opt);

	new=xzalloc(sizeof(ntkopt));
	new->opt=opt;
	new->desc=desc;
	*opt_head=list_add(*opt_head, new);
}

/*
 * opt_find_opt
 * ------------
 *
 * It returns the pointer to the structure of the option named `opt_name'.
 *
 * `opt_head' is the head of the llist which keeps all the valid options.
 *
 * If the option hasn't been found or if its value hasn't been set, NULL is
 * returned.
 */
ntkopt *opt_find_opt(const char *opt_name, ntkopt *opt_head)
{
	ntkopt *opt=opt_head;

	list_for(opt)
		if(opt->opt && !strcmp(opt->opt, opt_name))
			return opt;

	return 0;
}

/*
 * opt_get_value
 * -------------
 *
 * It returns the value assigned to the option named `opt_name'.
 *
 * `opt_head' is the head of the llist which keeps all the valid options.
 *
 * If the option hasn't been found or if its value hasn't been set, NULL is
 * returned.
 */
char *opt_get_value(const char *opt_name, ntkopt *opt_head)
{
	ntkopt *opt;
	
	opt=opt_find_opt(opt_name, opt_head);
	
	return opt ? opt->value : 0;
}

/*
 * opt_add_value
 * -------------
 *
 * It searches, in `opt_head', an option whose name is `opt_name',
 * if it is found, the string `value' is added to the opt->value argz vector.
 * 
 * If no option named `opt_name' is found, -1 is returned.
 */
int opt_add_value(const char *opt_name, char *value, ntkopt *opt_head)
{
	ntkopt *opt;

	if(!(opt=opt_find_opt(opt_name, opt_head)))
		return -1;

	xargz_add(&opt->value, &opt->valsz, value);

	return 0;
}

/*
 * opt_append_argz_value
 * ---------------------
 *
 * It searches, in `opt_head', an option whose name is `opt_name',
 * if it is found, the argz vector `value' is appended to the opt->value
 * argz vector.
 * `valsz' is the size of the `value' argz vector.
 * 
 * If no option named `opt_name' is found, -1 is returned.
 */
int opt_append_argz_value(const char *opt_name, char *value, size_t valsz,
				ntkopt *opt_head)
{
	ntkopt *opt;

	if(!(opt=opt_find_opt(opt_name, opt_head)))
		return -1;

	xargz_append(&opt->value, &opt->valsz, value, valsz);
	return 0;
}

/*
 * opt_replace_value
 * -----------------
 *
 * It searches, in `opt_head', an option whose name is `opt_name',
 * if it is found, the opt->value string is replaced by the duplicate of 
 * `value'.
 * 
 * If no option named `opt_name' is found, -1 is returned.
 */
int opt_replace_value(const char *opt_name, char *value, ntkopt *opt_head)
{
	ntkopt *opt;

	if(!(opt=opt_find_opt(opt_name, opt_head)))
		return -1;

	opt_free_value(opt);
	opt->value=xstrdup(value);
	return 0;
}

void opt_free_value(ntkopt *opt)
{
	if(opt->value)
		xfree(opt->value);
	opt->valsz=0;
}

/*
 * opt_clear_values
 * ----------------
 *
 * It cleans all the allocated values inside the `opt_head' llist.
 */
void opt_clear_values(ntkopt *opt_head)
{
	ntkopt *opt=opt_head;

	list_for(opt)
		opt_free_value(opt);
}

/*
 * opt_zero_values
 * ---------------
 *
 * Set to zero all the `value' and `valsz' variables contained in the
 * `opt_head' llist.
 */
void opt_zero_values(ntkopt *opt_head)
{
	ntkopt *opt=opt_head;

	list_for(opt) {
		opt->value=0;
		opt->valsz=0;
	}
}

/*
 * opt_close
 * ---------
 *
 * It destroys the `*opt_head' llist
 */
void opt_close(ntkopt **opt_head)
{
	opt_clear_values(*opt_head);
	list_destroy(*opt_head);
}
