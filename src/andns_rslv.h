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
#ifndef ANDNS_RSLV_H
#define ANDNS_RSLV_H


#include "andns.h"
#include "inet.h"

/* debug includes 
#include "andns.h"
#include "andna_fake.h" */

/* Globals */



// Functions
void char_print(char *buf, int len);
int store_ns(char *ns);
int collect_resolv_conf();
int andns_init(int restricted, char *resolv_conf);
char* andns_rslv(char *msg,int msglen, char *answer_buf, int *answ_len);
int ns_general_send(char *msg,int msglen,char *answer,int *anslen);
int ns_send(char *msg,int msglen, char *answer,int *anslen,struct sockaddr_in *ns,socklen_t nslen);
int dns_forward(dns_pkt *dp,char *msg,int msglen,char* answer);
int a_a_resolve(andns_pkt *ap);
int a_ptr_resolve(andns_pkt *ap);
int a_mx_resolve(andns_pkt *ap);
int d_a_resolve(dns_pkt *dp);
int d_ptr_resolve(dns_pkt *dp);
int d_mx_resolve(dns_pkt *dp);

#endif //ANDNS_RSLV_H
