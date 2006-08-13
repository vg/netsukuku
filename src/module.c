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
 * module.c
 *
 * This code unifies the modules with the rest of the Netsukuku code.
 * It loads the modules and registers their functions.
 * The module API is explained in module.h
 */

#include "module.h"

int load_module(char *name, char *path, char *arg)
{
	char filename[NAME_MAX];
	module mod;

	int (*init_function)(char *);
	void *handle=0;
	char *info;

	char *error;
	int err;

	setzero(&mod, sizeof(mod));

	err=snprintf(filename, NAME_MAX, "%s/%s.so", path, name);
	if(err >= NAME_MAX)
		goto error;

	/**
	 * load the .so plugin
	 */
	handle = dlopen (filename, RTLD_NOW | RTLD_GLOBAL);
	if(!handle) {
		error(ERROR_MSG "Cannot load the \"%s\" module: %s", 
				ERROR_FUNC, dlerror());
		goto error;
	}
	/**/

	/*
	 * Retrieve the module infos (name, author, description, ...)
	 */

	/* name */
	mod.name = dlsym(handle, MODULE_INFO_NAME_STR(name));
	if(!mod.name)
		mod.name=xstrdup(name);

	/* description */
	mod.description = dlsym(handle, MODULE_INFO_NAME_STR(description));
	if(!mod.description)
		mod.description=MOD_DEFAULT_DESCRIPTION;

	/* author */
	mod.author = dlsym(handle, MODULE_INFO_NAME_STR(author));
	if(!mod.author)
		mod.author=MOD_DEFAULT_AUTHOR;

	/* version */
	info = dlsym(handle, MODULE_INFO_NAME_STR(version));
	if(!info)
		info=MOD_DEFAULT_VERSION;
	mod.version=atoi(info);

	/* TODO: CONTINUE HERE */
	/* TODO: CONTINUE HERE */
	/* TODO: CONTINUE HERE */

	/* 
	 * Load and call ntk_init_module() 
	 */

	debug(DBG_NOISE, "Calling the init function "
			 "of the \"%s\" module", name);

	dlerror();	/* Clear any existing error */
	*(void **) (&init_function) = dlsym(handle, MOD_INIT_FUNC);
	if ((error = dlerror()) != NULL)  {
		error(ERROR_MSG "Cannot load the init function for "
				"the \"%s\" module: %s\n", ERROR_FUNC, 
				name, error);
		goto error;
	}

	err=(*init_function)(arg);
	if(err < 0) {
		error("The initialization of "
		      "the \"%s\" module failed", name);
		goto error;
	}


	clist_add();
	return 0;

error:
	if(handle)
		dlclose(handle);
	goto error;
}
