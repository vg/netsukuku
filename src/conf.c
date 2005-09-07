/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
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
 * conf.c:
 * Configuration file loader and parser. All the accepted option. which are
 * listed in conf.h, are put in the enviroment for later retrievement.
 */

#include "includes.h"
#include <ctype.h>

#include "log.h"
#include "xmalloc.h"
#include "conf.h"


/*
 * parse_config_line: it reads the `line' string and sees if it has a valid
 * option assignement that is in the form of "option = value".
 * On success it stores the option name with its value in the enviroment.
 * On failure fatal() is called, so it will never return ;)
 * `file' and `pos' are used by fatal() to tell where the corrupted `line' was.
 */
void parse_config_line(char *file, int pos, char *line)
{
	int i, e;
	char *value;
	
	if(!(value=strchr(line, '=')))
		fatal("The line %s:%d is invalid, it does not contain the '=' "
				"character. Aborting.", file, pos);

	for(i=0; config_str[i][0]; i++)
		if(strstr(line, config_str[i])) {
			e=1;
			break;
		}
	if(!e)
 	    fatal("The line %s:%d does not contain a valid option. Aborting.",
				file, pos);

	value++;
	while(isspace(*value)) 
		value++;
	
	if(setenv(config_str[i], value, 1))
		fatal("Error in line %s:%d: %s. Aborting.", file, pos, 
				strerror(errno));
}


/*
 * load_config_file: loads from `file' all the options that are written in it
 * and stores them in the enviroment. See parse_config_line() above.
 * If `file' cannot be opened -1 is returned, but if it is read and
 * parse_config_line() detects a corrupted line, fatal() is directly called.
 * On success 0 is returned.
 */
int load_config_file(char *file)
{
	FILE *fd;
	char buf[PATH_MAX+1], *p, *str;
	size_t slen;
	int i=0, e=0;

	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the configuration file from %s: %s", file, strerror(errno));
		return -1;
	}

	while(!feof(fd) && i < CONF_MAX_LINES) {
		memset(buf, 0, PATH_MAX+1);
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
			
			/* Don't include the newline and spaces of the end of 
			 * the string */
			slen=strlen(str);
			for(p=&str[slen-1]; isspace(*p); p--)
				*p='\0';
			

			parse_config_line(file, e, str);
			i++;
		}
	}

	return 0;
}
