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

#ifndef AF_H
#define AF_H
#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "andns.h"

#define MAX_IP_INT      4

typedef struct
{
        u_char  family;              /* AF_INET or AF_INET6 */
        u_short len;                 /* IP length: 4 or 16 (bytes) */
        u_char  bits;                /* Number of used bits of the IP */
        u_int   data[MAX_IP_INT];    /* The address is kept in host long format,
                                       word ORDER 1 (most significant word first) */
}inet_prefix;
int str_to_inet(const char *src, inet_prefix *ip);
void swap_array(int nmemb, size_t nmemb_sz, void *src, void *dst);
void swap_ints(int nmemb, unsigned int *x, unsigned int *y);
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
#define DEFCOL		"\033[0m"
#define BLACK(x)	("\033[0;30m" (x) DEFCOL)
#define RED(x)		("\033[0;31m" (x) DEFCOL)
#define GREEN(x)	("\033[0;32m" (x) DEFCOL)
#define BROWN(x)	("\033[0;33m" (x) DEFCOL)
#define BLUE(x)		("\033[0;34m" (x) DEFCOL)
#define PURPLE(x)	("\033[0;35m" (x) DEFCOL)
#define CYAN(x)		("\033[0;36m" (x) DEFCOL)
#define LIGHTGRAY(x)	("\033[0;37m" (x) DEFCOL)
#define DARKGRAY(x)	("\033[1;30m" (x) DEFCOL)
#define LIGHTRED(x)	("\033[1;31m" (x) DEFCOL)
#define LIGHTGREEN(x)	("\033[1;32m" (x) DEFCOL)
#define YELLOW(x)	("\033[1;33m" (x) DEFCOL)
#define LIGHTBLUE(x)	("\033[1;34m" (x) DEFCOL)
#define MAGENTA(x)	("\033[1;35m" (x) DEFCOL)
#define LIGHTCYAN(x)	("\033[1;36m" (x) DEFCOL)
#define WHITE(x)	("\033[1;37m" (x) DEFCOL)
#endif

/* functions declaration */
void log_init(char *, int, int );

void fatal(const char *, ...);
void error(const char *, ...);
void loginfo(const char *, ...);
void debug(int lvl, const char *, ...);

void print_log(int level, const char *fmt, va_list args);

/* This file is part of Netsukuku system
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

/* xmalloc.h: Shamelessly ripped from openssh:
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Mon Mar 20 22:09:17 1995 ylo
 *
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
 */

void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);
void 	*xcalloc(size_t nmemb, size_t size);
void     xfree(void *);
char 	*xstrndup(const char *str, size_t n);
char	*xstrdup(const char *);

int andna_reverse_resolve(inet_prefix ip, char ***hostnames);
int andna_resolve_hname(char *hname, inet_prefix *resolved_ip);
void dp_print(dns_pkt *dp);
void inet_htonl(u_int *data, int family);

#endif
