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

#include "includes.h"
#include <ctype.h>
#include <dlfcn.h>

#include "config.h"
#include "module.h"
#include "common.h"

void close_module(module *mod);

void module_init(void)
{
	ntk_modules=(module *)clist_init(&ntk_modules_counter);
}

void module_close_all(void)
{
	module *mod=ntk_modules, *next;

	list_safe_for(mod, next)
		close_module(mod);

	ntk_modules=0;
	ntk_modules_counter=0;
}

/*
 * mod_find_name
 *
 * It searches in the loaded modules llist a modules whose name is equal to
 * `name'. 
 *
 * A pointer to the found module is returned, otherwise
 * if it isn't found NULL is returned.
 */
module *mod_find_name(const char *name)
{
	module *m=ntk_modules;

	list_for(m)
		if(m->name && !strcmp(m->name, name))
			return m;

	return 0;
}

/*
 * mod_load_config_file
 *
 * It loads all the options defined in `mod'->mod_opt from the
 * configuration file of the module `mod'.
 * The file is located at "CONF_DIR/`mod'->name.conf".
 *
 * On success 0 is returned.
 */
int mod_load_config_file(module *mod)
{
	char filename[PATH_MAX+1];
	
	snprintf(filename, PATH_MAX, "%s/%s.conf", CONF_DIR, mod->name);
	return load_config_file(filename, mod->mod_opt);
}


/*
 * mod_parse_args
 *
 * It parses the `mod'->args arguments line and saves the values of the parsed
 * parameters in `mod'->mod_opt.
 *
 * The arguments format is:
 * 	param [= value], param2 [= value], ...
 * ex:
 * 	zero=0, foo=bar and pascal, pi = 3 + 0.14 , null=void, enableX
 *
 * Note that mod_parse_args() will override the colliding options which have
 * been previously set in `mod'->mod_opt.
 *
 * On success, zero is returned. 
 * On error, -1 is returned.
 */
int mod_parse_args(module *mod)
{
	ntkopt *opt_head, *tmp_opt, *o;
	char *value, *arg_line=0, *next_arg, *arg, *p;
	int ret=0;

	if(!mod->args || !*mod->args || !mod->mod_opt)
		return -1;

	/* Work on a safe copy of mod->args */
	next_arg = arg = arg_line = xstrdup(mod->args);

	/* Copy mod->mod_opt in opt_head and clean all the values, in this way
	 * we'll be able to recognize the colliding options */
	opt_head=list_copy_all(mod->mod_opt);
	opt_zero_values(opt_head);

	do {
		arg=next_arg;
		if((next_arg=strchr(arg, ','))) {
			*next_arg=0;
			next_arg++;
		} else
			next_arg=0;

		/* remove final newline, spaces and tabs */
		p=next_arg-2;
		while(next_arg && isspace(*p)) {
			*p=0;
			p--;
		}

		/* remove the initial spaces and tabs */
		while(*arg == ' ' || *arg == '\t')
			arg++;

		if(!(value=strchr(arg, '='))) {
			/* 
			 * No value has been assigned to this param 
			 */

			if(!opt_add_value(arg, "1", opt_head))
				continue;
			else {
				error("%s: \"%s\" is not a valid parameter", 
						mod->name, arg);
				return -1;
			}
		} else {
			/* 
			 *  param = value
			 */

			*value=0;
			value++;

			if(arg == value-1) {
				/* We are in this this case:
				 * 	=value
				 * missing parameter
				 */
				error("%s: syntax error: a parameter"
					" is missing", mod->name);
				ERROR_FINISH(ret, -1, finish);
			}

			/* remove final newline, spaces and tabs before
			 * `value' */
			p=value-2;
			while(isspace(*p)) {
				*p=0;
				p--;
			}

			/* remove the initial spaces after `value' */
			while(*value == ' ' || *value == '\t')
				value++;

			if(!*value) {
				/* We are in this this case:
				 * 	param=
				 * no assiged value to the param
				 */
				error("%s: syntax error: no value has been "
					"assigned to the \"%s\" parameter", 
					mod->name, arg);
				ERROR_FINISH(ret, -1, finish);
			}


			if(opt_add_value(arg, value, opt_head)) {
				error("%s: \"%s\" is not a valid parameter", 
						mod->name, arg);
				ERROR_FINISH(ret, -1, finish);
			}
		}

	} while(next_arg);

	/* 
	 * Save the new `opt_head' llist in mod->mod_opt, overriding any 
	 * colliding options
	 */
	o=opt_head;
	list_for(opt_head) {
		if(opt_head->value) {
			tmp_opt=opt_find_opt(opt_head->opt, mod->mod_opt);
			opt_free_value(tmp_opt);
			opt_append_argz_value(opt_head->opt, opt_head->value, 
						opt_head->valsz, mod->mod_opt);
		}
	}
	opt_close(&o);

finish:
	arg_line && xfree(arg_line);

	return ret;
}

/*
 * mod_resolve_deps
 *
 * It resolves all the dependences of the module `mod':
 * 	- if the dependence X is a module which hasn't been loaded yet, it is
 * 	  loaded with load_module(), using `path' and `mod'->deps_args[X] as
 * 	  arguments.
 * 	- the pointer to the module struct of X is added in the
 * 	  `mod'->mod_deps array.
 * 
 * On success, zero is returned. 
 * On error, -1 is returned.
 */
int mod_resolve_deps(module *mod, const char *path)
{
	module **deps=mod->mod_deps;
	int i;

	for(i=0; i < mod->deps_counter; i++)
		if(!(deps[i]=mod_find_name(mod->deps[i])) &&
			!(deps[i]=load_module(mod->deps[i], path, 
					      mod->deps_args[i])) < 0)
			return -1;

	return 0;
}

/*
 * load_module
 *
 * It loads the module residing in the `path'/`name'.so shared object file.
 * 
 * If the module is valid, its initialization function is called and the 
 * arguments line `args' is passed to it.
 * 
 * It returns a pointer to the module struct, which describes the loaded
 * module.
 * On error NULL is returned.
 */
module *load_module(const char *name, const char *path, const char *args)
{
	char filename[PATH_MAX+1];
	module *mod=0;

	char *info, *retstr;
	int err;

	err=snprintf(filename, PATH_MAX, "%s/%s.so", path, name);
	if(err >= PATH_MAX)
		goto _err;

	/* Check if the module has been already loaded */
	if(mod_find_name(name)) {
		error("The \"%s\" module has been already loaded", name);
		goto _err;
	}

	/* Add it in the modules list, and do it now, in this way we can check
	 * if we are, at the same time, double loading it. */
	mod=xzalloc(sizeof(module));
	mod->name=xstrdup(name);
	mod->filename=xstrdup(filename);
	clist_add(&ntk_modules, &ntk_modules_counter, mod);

	/**
	 * load the .so plugin
	 */

	/* Change dir to `path', because the modules are compiled with
	 * DT_RUNPATH set to "./". See dlopen(3) */
	if(chdir(path)) {
		error(ERROR_MSG "Cannot chdir to \"%s\": %s", ERROR_FUNC,
				path, strerror(errno));
		goto _err;
	}

	mod->handle = dlopen (filename, RTLD_NOW | RTLD_GLOBAL);
	if(!mod->handle) {
		error(ERROR_MSG "Cannot load the \"%s\" module: %s", 
				ERROR_FUNC, name, dlerror());
		goto _err;
	}
	/**/

	/*
	 * Check if it is a valid NTK module 
	 */
	info = dlsym(mod->handle, "__ntk_module");
	if(!info || strcmp(info, NTK_MODULE_ID_STR)) {
		error("%s isn't a valid Netsukuku module", filename);
		goto _err;
	}
	
	/*
	 * Check the module API version
	 */
	info = dlsym(mod->handle, "__ntk_module_version");
	if(!info || abs(atoi(info) - NTK_MODULE_API_VERSION)%100 >= 1) {
		error("The version of the API used by "
			"the \"%s\" module isn't compatible "
			"with your version of ntkd", name);
		goto _err;
	}

	/*
	 * Retrieve the module infos (name, author, description, ...)
	 */

	/* full name */
	mod->fullname = dlsym(mod->handle, MODULE_INFO_NAME_STR(fullname));
	if(!mod->fullname)
		mod->fullname=MOD_DEFAULT_FULLNAME;

	/* description */
	mod->desc = dlsym(mod->handle, MODULE_INFO_NAME_STR(description));
	if(!mod->desc)
		mod->desc=MOD_DEFAULT_DESCRIPTION;

	/* author */
	mod->author = dlsym(mod->handle, MODULE_INFO_NAME_STR(author));
	if(!mod->author)
		mod->author=MOD_DEFAULT_AUTHOR;

	/* version */
	info = dlsym(mod->handle, MODULE_INFO_NAME_STR(version));
	if(!info)
		info=MOD_DEFAULT_VERSION;
	mod->version=atoi(info);

	/*
	 * Load and resolve module dependences
	 */
	info = dlsym(mod->handle, MODULE_INFO_NAME_STR(deps));
	if(info && *info) {
		const char **deps=(const char **)info;
		int i;

		for(i=0; deps[i]; )
			i+=2;

		if(( mod->deps_counter = i>>1 )) {
			mod->deps	= xmalloc(sizeof(char *)*mod->deps_counter);
			mod->deps_args	= xmalloc(sizeof(char *)*mod->deps_counter);

			for(i=0; deps[i]; ) {
				mod->deps[i>>1]      = deps[i];
				mod->deps_args[i>>1] = deps[i+1];
				i+=2;
			}

			mod_resolve_deps(mod, path);
		}
	}

	/**
	 *
	 * Load and parse the parameters passed to the module
	 */
	mod->args = args ? xstrdup(args) : 0;

	info = dlsym(mod->handle, MODULE_INFO_NAME_STR(params));
	if(info && *info) {
		const char **params=(const char **)info;
		int i;

		for(i=0; params[i]; )
			i+=2;
		
		if(( mod->params_counter = i>>1 )) {
			for(i=0; params[i]; ) {
				opt_add_option(params[i], params[i+1], &mod->mod_opt);
				i+=2;
			}

			if(dlsym(mod->handle, MODULE_INFO_NAME_STR(load_conf)))
				if(mod_load_config_file(mod)) {
					error("Loading of %s.so aborted: "
						"configuration file cannot"
						" be loaded.", name);
					goto _err;
				}

			if(mod->args && *mod->args)
				if(mod_parse_args(mod) < 0)
					goto _err;
		}
	}
	/**/


	/***
	 *
	 * Load and call ntk_module_init() 
	 */

	debug(DBG_NOISE, "Calling the init function "
			 "of the %s.so module", name);

	dlerror();	/* Clear any existing error */
	*(void **)(&mod->init_func) = dlsym(mod->handle, MOD_INIT_FUNC);
	if ((retstr = dlerror()) != NULL)  {
		error(ERROR_MSG "%s.so: cannot load the %s function: %s",
				ERROR_FUNC, name, MOD_INIT_FUNC, retstr);
		goto _err;
	}

	err=mod->init_func(mod);
	if(err < 0) {
		error("The initialization of "
		      "the %s.so module failed", name);
		goto _err;
	}
	/**/

	/*
	 * Load ntk_module_close() for future usage
	 */
	dlerror();
	*(void **)(&mod->close_func) = dlsym(mod->handle, MOD_CLOSE_FUNC);
	if ((retstr = dlerror()) != NULL)  {
		error(ERROR_MSG "%s.so: cannot load the %s function: %s",
				ERROR_FUNC, name, MOD_CLOSE_FUNC, retstr);
		goto _err;
	}

	loginfo("Module \"%s\" loaded", name);

	return mod;

_err:
	close_module(mod);
#ifdef MOD_DEBUG
	fatal("sigh");
#endif
	return 0;
}

/*
 * load_all_modules
 *
 * Loads all the modules specified in the `argz' vector.
 * The format of `argz' is:
 * 	
 * 	"mod_name mod_args\0mod1_name mod1_args\0...\0"
 * 
 * `argz_sz' is the size of the `argz' vector.
 *
 * `path' is the directory where the modules reside.
 *
 * On error -1 is returned
 */
int load_all_modules(char *argz, size_t argz_sz, const char *path)
{
	char *entry = 0, *value, module[NAME_MAX+1];
	size_t sz;
	int ret=0;

	while ((entry = xargz_next (argz, argz_sz, entry))) {

		if((value=strchr(entry, ' ')))
			value++;
		
		sz = value ? value-entry-1 : strlen(entry);
		if(sz > NAME_MAX) {
			error(ERROR_MSG "\"%s\" module name too long", 
					ERROR_FUNC, entry);
			ERROR_FINISH(ret, -1, finish);;
		}

		strncpy(module, entry, sz);
		module[sz]=0;

		if(!load_module(module, path, value))
			ERROR_FINISH(ret, -1, finish);;
	}

finish:
	return ret;
}

void module_free(module *mod)
{
	if(mod->mod_opt)
		opt_close(&mod->mod_opt);
	mod->name && xfree(mod->name);
	mod->filename && xfree(mod->filename);
	mod->args && xfree(mod->args);
	mod->deps && xfree(mod->deps);
	mod->deps_args && xfree(mod->deps_args);
	clist_del(&ntk_modules, &ntk_modules_counter, mod);
}

void close_module(module *mod)
{
	if(!mod)
		return;

	mod->close_func && mod->close_func();
	mod->handle && dlclose(mod->handle);

	module_free(mod);
}
