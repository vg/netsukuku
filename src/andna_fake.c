#include "andna_fake.h"
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#ifdef DEBUG
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>


void inet_ntohl(u_int *data, int family)
{
        if(family==AF_INET) {
                data[0]=ntohl(data[0]);
        } else {
                if(BYTE_ORDER == LITTLE_ENDIAN) {
                        int i;
                        swap_ints(MAX_IP_INT, data, data);
                        for(i=0; i<MAX_IP_INT; i++)
                                data[i]=ntohl(data[i]);
                }
        }
}
void inet_htonl(u_int *data, int family)
{
        if(family==AF_INET) {
                data[0]=htonl(data[0]);
        } else {
                if(BYTE_ORDER == LITTLE_ENDIAN) {
                        int i;
                        swap_ints(MAX_IP_INT, data, data);
                        for(i=0; i<MAX_IP_INT; i++)
                                data[i]=htonl(data[i]);
                }
        }
}

int inet_setip_raw(inet_prefix *ip, u_int *data, int family)
{
        ip->family=family;
        memset(ip->data, '\0', sizeof(ip->data));

        if(family==AF_INET) {
                ip->data[0]=data[0];
                ip->len=4;
        } else if(family==AF_INET6) {
                memcpy(ip->data, data, sizeof(ip->data));
                ip->len=16;
        } else
                return -1;

        ip->bits=ip->len<<3; /* bits=len*8 */

        return 0;
}
int inet_setip(inet_prefix *ip, u_int *data, int family)
{
        inet_setip_raw(ip, data, family);
        inet_ntohl(ip->data, ip->family);
        return 0;
}


int str_to_inet(const char *src, inet_prefix *ip)
{
        struct in_addr dst;
        struct in6_addr dst6;
        int family;
        u_int *data;

        if(strstr(src, ":")) {
                family=AF_INET6;
                data=(u_int *)&dst6;
        } else {
                family=AF_INET;
                data=(u_int *)&dst;
        }

        if(inet_pton(family, src, (void *)data) < 0)
                return -1;

        inet_setip(ip, data, family);
        return 0;
}
/*
 * swap_array: swaps the elements of the `src' array and stores the result in
 * `dst'. The `src' array has `nmemb'# elements and each of them is `nmemb_sz'
 * big.
 */
void swap_array(int nmemb, size_t nmemb_sz, void *src, void *dst)
{
        int i, total_sz;

        total_sz = nmemb*nmemb_sz;

        char buf[total_sz], *z;

        if(src == dst)
                z=buf;
        else
                z=dst;

        for(i=nmemb-1; i>=0; i--)
                memcpy(z+(nmemb_sz*(nmemb-i-1)), (char *)src+(nmemb_sz*i),
                                nmemb_sz);

        if(src == dst)
                memcpy(dst, buf, total_sz);
}

/*
 * swap_ints: Swap integers.
 * It swaps the `x' array which has `nmemb' elements and stores the result it
 * in `y'.
 */
void swap_ints(int nmemb, unsigned int *x, unsigned int *y)
{
        swap_array(nmemb, sizeof(int), x, y);
}

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


char *__argv0;
int dbg_lvl;
int log_to_stderr;
static int log_facility=LOG_DAEMON;

void log_init(char *prog, int dbg, int log_stderr)
{
	__argv0=prog;
	dbg_lvl=dbg;
	log_to_stderr=log_stderr;
}

/* Life is fatal! */
void fatal(const char *fmt,...)
{
	char str[strlen(fmt)+3];
	va_list args;

	str[0]='!';
	str[1]=' ';
	strncpy(str+2, fmt, strlen(fmt));
	str[strlen(fmt)+2]=0;

	va_start(args, fmt);
	print_log(LOG_CRIT, str, args);
	va_end(args);

#ifdef DEBUG
	/* Useful to catch the error in gdb */
	kill(getpid(), SIGSEGV);
#endif
	exit(1);
}

/* Misc errors */
void error(const char *fmt,...)
{
	char str[strlen(fmt)+3];
	va_list args;

	str[0]='*';
	str[1]=' ';
	strncpy(str+2, fmt, strlen(fmt));
	str[strlen(fmt)+2]=0;
	
	va_start(args, fmt);
	print_log(LOG_ERR, str, args);
	va_end(args);
}

/* Let's give some news */
void loginfo(const char *fmt,...)
{
	char str[strlen(fmt)+3];
	va_list args;

	str[0]='+';
	str[1]=' ';
	strncpy(str+2, fmt, strlen(fmt));
	str[strlen(fmt)+2]=0;
	
	va_start(args, fmt);
	print_log(LOG_INFO, str, args);
	va_end(args);
}

/* "Debugging is twice as hard as writing the code in the first place.
 * Therefore, if you write the code as cleverly as possible, you are,
 * by definition, not smart enough to debug it." - Brian W. Kernighan
 * Damn!
 */

void debug(int lvl, const char *fmt,...)
{
	char str[strlen(fmt)+3];
	va_list args;

	if(lvl <= dbg_lvl) {
		str[0]='#';
		str[1]=' ';
		strncpy(str+2, fmt, strlen(fmt));
		str[strlen(fmt)+2]=0;

		va_start(args, fmt);
		print_log(LOG_DEBUG, str, args);
		va_end(args);
	}
}

void print_log(int level, const char *fmt, va_list args)
{
	
	if(log_to_stderr) {
		vfprintf(stderr, fmt, args);
		fprintf(stderr, "\n");
	} else {
		openlog(__argv0, LOG_PID, log_facility);
		vsyslog(level | log_facility, fmt, args);
		closelog();
	}
}
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

/* xmalloc.c: Shamelessly ripped from openssh and xcalloc added
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
 *
 * Changes:
 *
 * xstrndup() added. AlpT
 */


void *xmalloc(size_t size)
{
	void *ptr;

	if (size == 0)
		fatal("xmalloc: zero size");
	ptr = malloc(size);
	if (ptr == NULL)
		fatal("xmalloc: out of memory (allocating %lu bytes)", (u_long) size);
	return ptr;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	if (size == 0 || nmemb == 0)
		fatal("xcalloc: zero size");
	ptr=calloc(nmemb, size);
	if (ptr == NULL)
		fatal("xcalloc: out of memory (allocating %lu bytes * %lu blocks)",
				(u_long) size, (u_long) nmemb);
	return ptr;
}

void *xrealloc(void *ptr, size_t new_size)
{
	void *new_ptr;

	if (new_size == 0)
		fatal("xrealloc: zero size");
	if (ptr == NULL)
		new_ptr = malloc(new_size);
	else
		new_ptr = realloc(ptr, new_size);
	
	if (new_ptr == NULL)
		fatal("xrealloc: out of memory (new_size %lu bytes)", (u_long) new_size);
	return new_ptr;
}

void xfree(void *ptr)
{
	if (ptr == NULL)
		fatal("xfree: NULL pointer given as argument");
	free(ptr);
}

char *xstrndup(const char *str, size_t n)
{
	size_t len;
	char *cp;

	len=strlen(str) + 1;
	if(len > n && n > 0)
		len=n;
	cp=xmalloc(len);
	strncpy(cp, str, len);
	return cp;
}

char *xstrdup(const char *str)
{
	return xstrndup(str, 0);
}

int andna_reverse_resolve(inet_prefix ip, char ***hostnames)
{
        char **depo;

        depo=xmalloc(sizeof(char*));
        depo[0]=xmalloc(512);
        strcpy(depo[0],"abcde.it.it");
	printf("ANDNA COPIA %s\n",depo[0]);
        *hostnames=depo;
        return 1;
}
int andna_resolve_hname(char *hname, inet_prefix *resolved_ip)
{
        struct in_addr oo;
        resolved_ip->family=AF_INET;
        resolved_ip->len=4;
        resolved_ip->bits=0;
        inet_pton(AF_INET,"192.168.254.2",&oo);
        resolved_ip->data[0]=oo.s_addr;
        return 0;
}

void dp_print(dns_pkt *dp)
{
        dns_pkt_hdr *dph;
        dns_pkt_a *dpa;
	dns_pkt_qst *dpq;

        dph=&(dp->pkt_hdr);
        printf(" ID %d\n",dph->id);
        printf(" QR %d\n",dph->qr);
        printf(" opcode %d\n",dph->opcode);
        printf(" aa %d\n",dph->aa);
        printf(" tc %d\n",dph->tc);
        printf(" rd %d\n",dph->rd);
        printf(" ra %d\n",dph->ra);
        printf(" z %d\n",dph->z);
        printf(" rcode %d\n",dph->rcode);
        printf(" qdcount %d\n",dph->qdcount);
        printf(" ancount %d\n",dph->ancount);
        printf(" nscount %d\n",dph->nscount);
        printf(" arcount %d\n",dph->nscount);

	dpq=dp->pkt_qst;

	printf("QUESTION\n");
	printf("\tQNAME=%s\n",dpq->qname);
	printf("\tQNAME_nopref=%s\n",dpq->qname_nopref);
	printf("\tqtype=%d\n",dpq->qtype);
	printf("\tqclass=%d\n",dpq->qclass);
        dpa=dp->pkt_answ;
	printf("ANSWERS\n");
	if (!dpa) printf("Any!\n");
        while (dpa)
        {
                printf("\tname %s\n", dpa->name);
                //printf("\tname_nopref %s\n", dpa->name_nopref);
                printf("\ttype %d\n", dpa->type);
                printf("\tclass %d\n", dpa->class);
                printf("\tttl %d\n", dpa->ttl);
                printf("\trdlength %d\n", dpa->rdlength);
                printf("\trdata %s\n", dpa->rdata);
                dpa=dpa->next;
        }
	printf("AUTHS\n");
        dpa=dp->pkt_auth;
	if (!dpa) printf("Any!\n");
        while (dpa)
        {
                printf("\tname %s\n", dpa->name);
                //printf("\tname_nopref %s\n", dpa->name_nopref);
                printf("\ttype %d\n", dpa->type);
                printf("\tclass %d\n", dpa->class);
                printf("\tttl %d\n", dpa->ttl);
                printf("\trdlength %d\n", dpa->rdlength);
                printf("\trdata %s\n", dpa->rdata);
                dpa=dpa->next;
        }
	printf("ADD\n");
        dpa=dp->pkt_add;
	if (!dpa) printf("Any!\n");
        while (dpa)
        {
                printf("\tname %s\n", dpa->name);
                //printf("\tname_nopref %s\n", dpa->name_nopref);
                printf("\ttype %d\n", dpa->type);
                printf("\tclass %d\n", dpa->class);
                printf("\tttl %d\n", dpa->ttl);
                printf("\trdlength %d\n", dpa->rdlength);
                printf("\trdata %s\n", dpa->rdata);
                dpa=dpa->next;
        }



}



