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
 * foo.c
 *
 * This source is just a basic example of a generic NTK module.
 * Since it is heavily commented, it is useful to understand how to code a
 * module.
 * 
 * You can also use this file as a start for your module:
 * copy foo/ to mymod/ and replace every 'foo' string with 'mymod', then
 * have fun coding ^_-
 */


/*
 * Mark this code as a NTK module
 * This define will also declare the global variables, which hold the infos
 * of the module. For this reason, it must be declared here.
 */
#define NETSUKUKU_MODULE

#include "includes.h"		/* This includes all the most common used 
				   sys headers (strings.h, time.h, 
				   errno.h, ...) */

/* You can put other sys headers here:
 *
 * #include <dmalloc.h> */

#include "module.h"		/* This is necessary to access to the internal 
				   structures and functions of modules */

#include "common.h"		/* Includes the common ntk headers: xmalloc.h, 
				   log.h, ... */


/*
 * Let's define the module's infos
 */

MODULE_FULL_NAME	("Foo Bar the conquerer");

MODULE_DESCRIPTION	("Foo Bar is a basic example of a generic NTK module."
			 "Since its source code is heavily commented, it is "
			 "useful to understand how to code a module.");

MODULE_AUTHOR		("AlpT <alpt@freaknet.org>");

MODULE_VERSION		("100");

/* 
 * Define the command line and config file options.
 */
MODULE_PARAM_BEGIN

	/* int value:	  foo speed=140 */
	MODULE_PARAM	("speed", "Forces Foo to run at the specified speed")

	/* string value:  foo lang=russian */
	MODULE_PARAM	("lang",  "Tells Foo what language it has to speak")

	/* boolean value: foo burn || foo burn=1 */
	MODULE_PARAM	("burn",  "If set, Foo will burn itself and all the"
				  " surrounding area")

	/* multi value:   foo goto=Moon, goto=Mars, goto=Sun */
	MODULE_PARAM	("goto",  "Tells Foo where to go")

MODULE_PARAM_END


/*
 * Force module.c to read and parse the `/etc/netsukuku/foo.conf' configuration
 * file when this module is loaded.
 *
 * The option of the configuration file are the same of the command line,
 * therefore they have been already defined above with MODULE_PARAM().
 *
 * The options specified in the command line ovveride the same options
 * specified in the configuration file.
 */
MODULE_LOAD_CONFIG_FILE();

/* 
 * If this module depends on the "bar" module, specify it with:
 *
 * 	MODULE_DEPEND_BEGIN
 *
 * 		MODULE_DEPEND   ("bar", "speed=341, lang=russian, burn")
 *
 * 	MODULE_DEPEND_END
 *
 & It is possible to use MODULE_DEPEND() multiple times.
 */


/*
 * ntk_module_init
 *
 * Initialization code. This function is called when this module is loaded.
 * 
 * On error returns -1, on success 0.
 */
int ntk_module_init(module *mod)
{
	int speed=0;
	char burn=0;
	char *lang, *wheretogo=0;
	size_t wheretogo_sz=0;

        /*
         * mod_load_config_file(mod);
         *
         */
	loginfo("FOO: I am the module Foo, bow before me.");

	/* 
	 * Retrieve the options 
	 */

	OPT_GET_INT_VALUE ("speed", mod->mod_opt, speed);
	
	OPT_GET_STRN_VALUE("lang",  mod->mod_opt, lang, 64);

	if(opt_get_value("burn", mod->mod_opt))
		burn=1;

	/***
	 * Retrieve the multi-value option "goto"
	 */
	OPT_GET_ARGZ_VALUE("goto", mod->mod_opt, wheretogo, wheretogo_sz);

	/*
	 * Unify the `wheretogo' argz vector into a unique string. All the
	 * values will be separated by the ',' character.
	 * If you want to iterate over the elements of the argz vector, you
	 * should use xargz_next (man argz_next)  */
	xargz_stringify(wheretogo, wheretogo_sz, ',');
	/**/

	loginfo("FOO: args line: \"%s\"", mod->args);
	loginfo("FOO: the final values of the options are:\n"
			"\tspeed\t= %d\n"
			"\tlang\t= %s\n"
			"\tburn\t= %s\n"
			"\tgoto\t= %s",
			speed, lang, burn ? "true" : "false", wheretogo);
		
	/*
	 * Return
	 */

	if(speed > 299790000) {
		error(ERROR_MSG "I cannot go faster than light", ERROR_FUNC);
		return -1; /* Error */
	}

	return 0; /* All OK */
}

int ntk_module_close(void)
{
	/* De-Initialization code */

	loginfo("I shall return");

	return 0;
}
