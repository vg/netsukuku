/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
 */

#include "log.h"
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>

extern char *__argv0;
extern int dbg_lvl;
extern int log_to_stderr;
static log_facility=LOG_DAEMON;

/* Life is fatal! */
void fatal(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	print_log(LOG_CRIT, fmt, args);
	va_end(args);
	exit(1);
	/*TODO: safe_exit(255); We must save the maps before exit*/
}

/* Misc errors */
void error(const char *fmt,...)
{
	va_list args;
	
	va_start(args, fmt);
	print_log(LOG_ERR, fmt, args);
	va_end(args);
}

/* Let's give some news */
void loginfo(const char *fmt,...)
{
	va_list args;
	
	va_start(args, fmt);
	print_log(LOG_INFO, fmt, args);
	va_end(args);
}

/* "Debugging is twice as hard as writing the code in the first place.
 * Therefore, if you write the code as cleverly as possible, you are,
 * by definition, not smart enough to debug it." - Brian W. Kernighan
 * Damn!
 */

void debug(int lvl, const char *fmt,...)
{
	va_list args;

	if(lvl <= dbg_lvl) {
		va_start(args, fmt);
		print_log(LOG_DEBUG, fmt, args);
		va_end(args);
	}
}

void print_log(int level, const char *fmt,...)
{
	va_list args;
	
	if(log_to_stderr) 
		vfprintf(stderr, fmt, args);
	else {
		openlog(__argv0, LOG_PID, log_facility);
		vsyslog(level | log_facility, fmt, args);
		closelog();
	}
}
