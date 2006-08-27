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
 * conf.c
 *
 * General configuration file loader and parser.
 */

#ifndef CONF_H
#define CONF_H

/*\
 *	
 *		       * * *  Conf.c usage  * * *
 *
 *
 * First of all, it is necessary to register all the valid options with
 * add_option(), f.e.:
 * 	
 * 	ntkopt *myoptions=0;
 *	
 *	int main() 
 *	{
 *		add_option("map_file", 	&myoptions);
 *		add_option("speed",	&myoptions);
 *		add_option("load_module",  &myoptions);
 *
 *		...
 *	}
 * 
 * You can then load the options from the configuration file:
 *
 *	load_config_file("/etc/my.conf", myoptions);
 * 
 * In the configuration file the options must be written in this format:
 * 	
 * 	opt_name [[=] value]
 *
 * For example:
 *
 * 	speed		= 140 Km/h
 * 	load_module	andna	arg1=foo, arg2=bar
 * 	load_module	Viphilama    es=netsukuku.org
 *
 * All the parsed values are stored in the `myoptions' linked list.
 * If load_config_file() encounters a syntax error, it calls immediately
 * fatal(). 
 *
 * To retrieve the value of a specific option use opt_get_value():
 *
 * 	filename=opt_get_value("map_file", myoptions);
 *
 * Alternatively you can use the OPT_GET_INT_VALUE(),
 * OPT_GET_STRN_VALUE() and OPT_GET_ARGZ_VALUE() macros. Their description is
 * in opt.h.
 * 
 * Do not free the strings containing the retrieved value, because they are 
 * stored in the `myoptions' llist and they will be freed when 
 * close_conf() will be called.
 *
 *
 * The values of an option, which has been specified multiple times in the same
 * configuration file, are stored in a unique string and they are separated by
 * the '\0' character. In other words, they are unified in a argz vector.
 * In the example above, "load_module" has been specified two times, therefore
 * the string returned by 
 *
 * 	opt_get_value("load_module", myoptions);
 *
 * will be: 
 *
 * 	"andna	arg1=foo, arg2=bar\0Viphilama	es=netsukuku.org"
 *
 * The argz functions (man args_add(3)) are a nice way to handle this type of 
 * strings. You should use the local xargz.h functions, f.e.:
 *	
 *	char *argz=0, *entry=0;
 *	size_t argz_sz=0;
 *
 *	OPT_GET_ARGZ_VALUE("load_module", myoptions, argz, argz_sz);
 *
 * 	while ((entry = xargz_next (argz, argz_sz, entry))) {
 * 		
 * 		... code here ...
 *
 * 	}
 *
 * 	xfree(argz);
 *
 *
 * Finally, at the end of your program you can free all the allocated space:
 *	 
 *	 opt_close(&myoptions);
\*/

#include "opt.h"

/*\
 *  * * *  Functions' declaration  * * *
\*/

int load_config_file(char *file, ntkopt *opt_head);

#endif /*CONF_H*/
