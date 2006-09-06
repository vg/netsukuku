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

#include "conf.h"
#include "llist.c"

/*\
 *
 * 			Module API
 * 		      ==============
 *
 * The basic structure of a NTK module is:
 * 	
 * 	#define NETSUKUKU_MODULE
 * 	#include "module.h"
 * 	#include "common.h"
 * 	
 * 	int ntk_module_init(module *mod)
 * 	{
 * 		return 0;
 * 	}
 * 	
 * 	int ntk_module_close(void)
 * 	{
 * 		return 0;
 * 	}
 *
 * The ntk_module_init() function is called when the module is loaded, while
 * ntk_module_close() when the module is closed. They have to return 0 on
 * success, while -1 on error.
 *
 * The module should use the xmalloc.h functions (xmalloc, xrealloc, xfree,
 * ..., instead of their standard equivalent. The same applies for the xargz.h
 * functions.
 *
 * In modules/foo/ you can find a well commented example of a generic NTK 
 * module.
\*/


/*
 * General defines
 */
#define MAX_MOD_PARAMS			16	/* Max number of parameters */
#define MAX_MOD_DEPS			16	/* Max number of dependences */

#define NTK_MODULE_API_VERSION		100
#define NTK_MODULE_ID_STR		"ntk"


/*\
 *
 *	* * *  Macros used by the module  * * *
 *
\*/

/*
 * NETSUKUKU_MODULE 
 *
 * It must be defined at the very beginning of the module source, before the 
 * list of includes, f.e.:
 * 	
 * 	#define NETSUKUKU_MODULE
 *
 * 	#include <foo>
 * 	#include "module.h"
 *
 * 	... rest of the module code ...
 *
 * This define, declares all the constant variables keeping the module's
 * infos.
 */
#ifdef NETSUKUKU_MODULE
	const char   __ntk_module[]=NTK_MODULE_ID_STR;
	const char   __ntk_module_version=NTK_MODULE_API_VERSION;
#endif

#define MODULE_INFO_NAME_STR(name)	"_ntk_mod_" #name
#define MODULE_INFO_NAME(name)		_ntk_mod_##name
#define MODULE_INFO(name, info)		const char MODULE_INFO_NAME(name)[] = info

/*
 * MODULE_FULL_NAME
 *
 * It declares the full name of the module, ex:
 *
 * 	MODULE_FULL_NAME("Foo Bar the conquerer");
 */
#define MODULE_FULL_NAME(_fname)	MODULE_INFO(fullname, _fname)

/*
 * MODULE_DESCRIPTION
 *
 * It declares the full description of the module:
 * 	
 * 	MODULE_DESCRIPTION("Foo Bar is a very useful module."
 * 			   "It does foo and then bar");
 */
#define MODULE_DESCRIPTION(_desc)	MODULE_INFO(description, _desc)

/*
 * MODULE_AUTHOR
 *
 * MODULE_AUTHOR("Author Name <foofoo@bar.org>");
 */
#define MODULE_AUTHOR(_author)		MODULE_INFO(author, _author)

/*
 * MODULE_VERSION
 *
 * The version of the module. It is a string which represents an integer.
 * The integer is composed by 3 digits, the major number, the minor and the
 * sub-minor.
 *	
 *	MODULE_VERSION("100");
 *
 * When the major number changes, the older version of the module are no more
 * considered compatible with the current one. For example:
 * The module B depends on "A version 132". 
 * The module B will be considered compatible with "A version 1xx", but not 
 * with "A version 2xx" and any other later versions.
 */
#define MODULE_VERSION(_ver)		MODULE_INFO(version, _ver)

/*
 * MODULE_PARAM
 *
 * It declares a parameter, which can be specified in the agurments line of
 * the module. To declare more than one parameter, use MODULE_PARAM() multiple
 * times.
 *
 * `_param' is the name of the parameter.
 * `_desc' is its description.
 * 
 * Example:
 *
 * 	MODULE_PARAM_BEGIN
 * 	
 *	    MODULE_PARAM   ("server", "It specifies the IP of the Foo server")
 *	    MODULE_PARAM   ("ipv6",   "If it is set to a non zero value, the"
 *	    			      " Foo module will be ipv6 compatible")
 *	    MODULE_PARAM   ("xyz",    "")
 *
 *	MODULE_PARAM_END;
 */
#define MODULE_PARAM_BEGIN						\
		const char *MODULE_INFO_NAME(params)[] = {

#define MODULE_PARAM(_param, _desc)					\
	_param, _desc,

#define MODULE_PARAM_END						\
	0, 0 };

/*
 * MODULE_LOAD_CONFIG_FILE
 *
 * Forces module.c to read and parse the CONF_DIR/MOD_NAME.conf configuration
 * file when the module is loaded.
 *
 * The option of the configuration file are the same of the command line,
 * therefore they need to be defined just once with MODULE_PARAM().
 *
 * The options specified in the command line ovveride the same options 
 * specified in the configuration file.
 */
#define MODULE_LOAD_CONFIG_FILE()	MODULE_INFO(load_conf, "")

/*
 * MODULE_DEPEND
 * 
 * Each time MODULE_DEPEND() is called, it declares a new dependence of the
 * module.
 * `_dep' is the name of the dependence.
 * If `_dep' is loaded during the resolution of the module's dependences, 
 * `_dep_args' will be used as its arguments line.
 * 
 * Example:
 *	
 *	MODULE_DEPEND_BEGIN
 *
 * 		MODULE_DEPEND	("bar_mod", "arg1=341, xyz=1.2.3, enable_x")
 * 		MODULE_DEPEND	("another_mod", "")
 *
 * 	MODULE_DEPEND_END
 */
#define MODULE_DEPEND_BEGIN						\
	const char  	   *MODULE_INFO_NAME(deps)[] = {

#define MODULE_DEPEND(_dep, _dep_args)					\
		_dep, _dep_args,

#define MODULE_DEPEND_END						\
	0, 0 };


#define MOD_DEFAULT_FULLNAME		"Foo Bar the conquerer"
#define MOD_DEFAULT_DESCRIPTION		"Just another module"
#define MOD_DEFAULT_AUTHOR		"Foo Bar <foofoo@org.org>"
#define MOD_DEFAULT_VERSION		"100"


/*\
 *
 *	* * *  Functions used inside the module  * * *
 *
\*/

/*
 * MOD_INIT_FUNC, MOD_CLOSE_FUNC
 *
 * This two function must be declared inside the module's source code.
 *
 * ntk_module_init() is called when the module is loaded, while
 * ntk_module_close() when it is closed and freed.
 */
#define MOD_INIT_FUNC			"ntk_module_init"
#define MOD_CLOSE_FUNC			"ntk_module_close"


/*\
 *
 *	* * *  Structures  * * *
 *
\*/

/*
 * module
 * 
 * This struct keeps all the basic information of a loaded module.
 */
struct module
{
	LLIST_HDR	(struct module);

	char		*filename;

	/*
	 * General infos
	 */
	char		*name;
	const char	*fullname;
	const char	*desc;
	const char	*author;
	int		version;

	/**
	 * Parameters 
	 */
	u_char		params_counter;	/* number of parameters defined with
					   the MODULE_PARAM() macro */
	ntkopt		*mod_opt;	/* The parameters, the options of the 
					   config file and their relative 
					   values */
	char		*args;		/* argument line passed to the module */
	/**/


	/**
	 * Dependences
	 */
	u_char		deps_counter;	
	const char	**deps;		/* dependences declared with the 
					   MODULE_DEPEND() macro */
	const char	**deps_args;	/* arguments passed to the relative 
					   dependence */
	struct module	*mod_deps[MAX_MOD_DEPS];
	/**/

	void		*handle;	/* handle returned by dlopen */
	int		(*init_func)(struct module *);
	int		(*close_func)();
};
typedef struct module module;


/*\
 *   * * *  Globals  * * *
\*/

module *ntk_modules;
int ntk_modules_counter;


/*\
 *   * * *  Functions declaration  * * *
\*/

void module_init(void);
void module_close_all(void);

int mod_load_config_file(module *mod);
module *mod_find_name(const char *name);
int mod_is_loaded(const char *name);

module *load_module(const char *name, const char *path, const char *args);
int load_all_modules(char *argz, size_t argz_sz, const char *path);

#endif /*MODULE_H*/
