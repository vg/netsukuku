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
 * General configuration file loader and parser. To understand how to use it, 
 * see {-conf-usage-}.
 */

#include "includes.h"
#include <ctype.h>

#include "conf.h"
#include "common.h"

/*
 * parse_config_line
 *
 * it reads the `line' string and sees if it has a valid option assignment 
 * that is in the form of "option = value".
 * On success it stores the option name with its value in the environment.
 * On failure fatal() is called, so it will never return ;)
 * `file' and `pos' are used by fatal() to tell where the corrupted `line' was.
 *
 * `opt_head' is the head of the llist which keeps all the valid options.
 */
void parse_config_line(char *file, int pos, char *line, ntkopt *opt_head)
{
	ntkopt *opt=opt_head;
	size_t optlen;
	int e=0;
	char *value;
	
	while(isspace(*line)) 
		if(!*(++line))
			/* It's just an empty line */
			return;

	/* Check if `line' contains a valid option */
	list_for(opt) {
		optlen=strlen(opt->opt);
		if(!memcmp(line, opt->opt, optlen)) {
			e=1;
			break;
		}
	}
	if(!e)
 	    fatal("The line %s:%d does not contain a valid option. Aborting.",
				file, pos);

	/* Eat the remaining spaces */
	value=line+optlen+1;
	while(isspace(*value)) 
		value++;

	if(!*value) {
		/* 
		 * Consider this option as boolean and set it to "1" 
		 */
		opt_add_value(opt->opt, "1", opt);
		return;
	} else if(*value == '=')
		value++;

	while(isspace(*value)) 
		value++;
	if(!*value) {
		/*
		 * We are in this case:
		 * 	opt =
		 * missing value after the '='
		 */
		fatal("%s:%d: syntax error: no value has been assigned "
				"to the \"%s\" option", 
				file, pos, opt->opt);
	}

	opt_add_value(opt->opt, value, opt);
}

/*
 * load_config_file
 *
 * loads from `file' all the options that are written in it and stores them
 * in the environment. See parse_config_line() above.
 * If `file' cannot be opened -1 is returned, but if it is read and
 * parse_config_line() detects a corrupted line, fatal() is directly called.
 *
 * `opt_head' is the head of the llist which keeps all the valid options.
 *
 * On success 0 is returned.
 */
int load_config_file(char *file, ntkopt *opt_head)
{
	FILE *fd;
	char buf[PATH_MAX+1], *p, *str;
	size_t slen;
	int e=0;

	if(!(fd=fopen(file, "r"))) {
		fatal("Cannot load the configuration file from %s: %s",
			file, strerror(errno));
		return -1;
	}

	while(!feof(fd)) {
		setzero(buf, PATH_MAX+1);
		fgets(buf, PATH_MAX, fd);
		e++;

		if(feof(fd))
			break;

		str=buf;
		while(isspace(*str))
			str++;
		if(*str=='#' || !*str) {
			/* Strip off any comment or null lines */
			continue;
		} else {
			/* Remove the last part of the string where a side
			 * comment starts, 	#a comment like this.
			 */
			if((p=strrchr(str, '#')))
				*p='\0';
			
			/* Don't include the newline and spaces at the end of 
			 * the string */
			slen=strlen(str);
			for(p=&str[slen-1]; isspace(*p); p--)
				*p='\0';
			

			parse_config_line(file, e, str, opt_head);
		}
	}

	fclose(fd);

	return 0;
}
