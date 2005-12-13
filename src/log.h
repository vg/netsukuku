/* This file is part of Netsukuku system
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

/*
 * Use ERROR_MSG and ERROR_POS in this way:
 * 	printf(ERROR_MSG "damn! damn! damn!", ERROR_POS);
 */
#define ERROR_MSG  "%s:%d: "
#define ERROR_POS  __FILE__, __LINE__

/*Debug levels*/
#define DBG_NORMAL	1
#define DBG_SOFT	2
#define DBG_NOISE 	3
#define DBG_INSANE 	4

/* 
 * ERROR_FINISH:
 * A kind way to say all was messed up, take this example:
 *
 * int func(void) // returns -1 on errors
 * { 
 * 	int ret=0;
 *
 * 	,,,BLA BLA...
 * 	
 *	if(error_condition)
 *		ERROR_FINISH(ret, -1, finish);
 *
 * 	,,,BLA BLA...
 * 	
 *	finish:
 *		return ret;
 * }
 */
#define ERROR_FINISH(ret, err, label_finish)				\
do {									\
	void *_label_finish=&&label_finish;				\
	(ret)=(err); 							\
	goto *_label_finish;						\
} while(0)

#ifdef DEBUG
/* Colors used to highlights things while debugging ;) */
#define DEFCOL     "\033[0m"
#define BLACK      "\033[0;30m"
#define RED        "\033[0;31m"
#define GREEN      "\033[0;32m"
#define BROWN      "\033[0;33m"
#define BLUE       "\033[0;34m"
#define PURPLE     "\033[0;35m"
#define CYAN       "\033[0;36m"
#define LIGHTGRAY  "\033[0;37m"
#define DARKGRAY   "\033[1;30m"
#define LIGHTRED   "\033[1;31m"
#define LIGHTGREEN "\033[1;32m"
#define YELLOW     "\033[1;33m"
#define LIGHTBLUE  "\033[1;34m"
#define MAGENTA    "\033[1;35m"
#define LIGHTCYAN  "\033[1;36m"
#define WHITE      "\033[1;37m"
#endif

/* functions declaration */
void log_init(char *, int, int );

void fatal(const char *, ...);
void error(const char *, ...);
void loginfo(const char *, ...);
void debug(int lvl, const char *, ...);

void print_log(int level, const char *fmt, va_list args);

#endif /*LOG_H*/
