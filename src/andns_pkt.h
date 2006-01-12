	         /**************************************
	        *     AUTHOR: Federico Tomassini        *
	       *     Copyright (C) Federico Tomassini    *
	      *     Contact effetom@gmail.com             *
	     ***********************************************
	     *******                                ********
*************************************************************************
*                                              				* 
*  This program is free software; you can redistribute it and/or modify	*
*  it under the terms of the GNU General Public License as published by	*
*  the Free Software Foundation; either version 2 of the License, or	*
*  (at your option) any later version.					*
*									*
*  This program is distributed in the hope that it will be useful,	*
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	*
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*
*  GNU General Public License for more details.				*
*									*
************************************************************************/
#ifndef ANDNS_PKT_H
#define ANDNS_PKT_H

#include "andns.h"

uint8_t _dns_forwarding_;

int andns_pkt_init(int restricted);
size_t getlblptr(char *buf);
size_t lbltoname(char *buf,char *start_pkt,char *dst,int count,int limit_len,int recursion);
int andns_proto(char *buf);
int andns_realm(char* qst);
char* rm_realm_prefix(char *from);
char* swapped_straddr(char *src);
int dnslovesntk(dns_pkt *dp);
size_t nametolbl(char *name,char *dst);

size_t dpkttohdr(char *buf,dns_pkt_hdr *dph);
size_t dpkttoqst(char *start_buf,char *buf,dns_pkt *dp,int limit_len);
size_t dpkttoqsts(char *start_buf,char *buf,dns_pkt *dp,int limit_len);
size_t dpkttoa(char *start_buf,char *buf,dns_pkt_a **dpa,int limit_len);
size_t dpkttoas(char *start_buf,char *buf,dns_pkt_a **dpa,int limit_len,int count);
size_t dpkt(char *buf,size_t pktlen,dns_pkt **dpp);

size_t hdrtodpkt(dns_pkt *dp,char *buf);
size_t qsttodpkt(dns_pkt_qst *dpq,char *buf, int limitlen);
size_t qststodpkt(dns_pkt *dp,char *buf,int limitlen);
size_t atodpkt(dns_pkt_a *dpa,char *buf,int limitlen);
size_t astodpkt(dns_pkt_a *dpa,char *buf,int limitlen,int count);
size_t dpktpack(dns_pkt *dp,char *buf);

size_t apkttohdr(char *buf,andns_pkt *ap);
size_t apkttoqst(char *buf,andns_pkt *ap);
size_t apkt(char *buf,size_t pktlen,andns_pkt **app);
size_t hdrtoapkt(andns_pkt *ap,char *buf);
size_t qsttoapkt(andns_pkt *ap,char *buf,size_t limitlen);
size_t answtoapkt(andns_pkt_data *apd,char *buf,size_t limitlen);
size_t answstoapkt(andns_pkt *ap,char *buf, size_t limitlen);
size_t apktpack(andns_pkt *ap,char *buf);

//char* apkttodstream(andns_pkt *ap);
int danswtoaansw(dns_pkt *dp,andns_pkt *ap,char *msg);


#endif //ANDNS_PKT_H
