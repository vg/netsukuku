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

#ifndef MODULE_H
#define MODULE_H

/* 
 * General defines 
 */

#define MOD_INIT_FUNC		"ntk_init_func"    /* the first function 
						      called from the module */

#define MODULE_INFO_NAME_STR(name)	"_ntk_mod_" #name
#define MODULE_INFO(name, info)		static const char _ntk_mod_##name[] = info

#define MODULE_DESCRIPTION(_desc)	MODULE_INFO(description, _desc)
#define MODULE_AUTHOR(_author)		MODULE_INFO(author, _author)
#define MODULE_VERSION(_ver)		MODULE_INFO(version, _ver)

/* TODO: PARAM ? */

#define MOD_DEFAULT_DESCRIPTION		"Just another module"
#define MOD_DEFAULT_AUTHOR		"Foo Bar <foofoo@org.org>"
#define MOD_DEFAULT_VERSION		"100"

/*
 * module
 * 
 * This struct keeps all the basic information of a loaded module.
 */
struct module
{
	LLIST_HDR	(struct module);

	char		*name;
	char		*desc;
	char		*author;
	int		version;

	char		*arg;		/* arguments passed to the module */

	struct module	**deps;		/* dependences */
};
typedef struct module module;



/*\
 *
 *  * * *  Globals  * * *
 *
\*/

module *ntk_modules;
int ntk_modules_counter;


#endif /*MODULE_H*/
