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

#include "includes.h"

#include "llist.c"
#include "inet.h"
#include "endianness.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "route.h"
#include "request.h"
#include "pkts.h"
#include "tracer.h"
#include "qspn.h"
#include "radar.h"
#include "netsukuku.h"
#include "daemon.h"
#include "crypto.h"
#include "andna_cache.h"
#include "andna.h"

#include "andns.h"
#include "andns_rslv.h"
#include "andns_mem.h"
#include "andns_pkt.h"
#include "xmalloc.h"
#include "log.h"

#include <resolv.h>

/* To remove after integration with ntk codes 
 * For now, DO NOT REMOVE! 
#include "andns.h"
#include "andns_rslv.h"
#include "andns_mem.h"
#include "andns_pkt.h"
#include "andna_fake.h"
#include <stdio.h>
#include <string.h>
#include <resolv.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> */


/* Globals */

static uint8_t _dns_forwarding_;
static struct sockaddr_in _andns_ns_[MAXNSSERVERS];
static uint8_t _andns_ns_count_;

/*
 * A very stupid function for debugging
 */
void char_print(char *buf, int len)
{
        int i,count=0;

        printf("Printing %d bytes\n",len);
        for (i=0;i<len;i++) {
                printf("%02X ", (unsigned char)(buf[i]));
                count++;
                if ((count%16)==0) printf("\n");
        }
        printf("\n");
        return;
}

/*
 * Saves on `nsbuf' and `ns_count' the ip 
 * address ns: these infos will be used for DNS
 * forwarding.
 *
 * Returns:
 * 	-1 on error
 * 	 0 if OK
 */
int store_ns(char *ns, struct sockaddr_in *nsbuf, uint8_t *ns_count)
{
        int res;
	struct sockaddr_in *saddr;

        if (*ns_count >= MAXNSSERVERS)
                return -1;
        if (strstr(ns, "127.0.0."))
                return -1;
	
	saddr=nsbuf+(*ns_count);
	saddr->sin_family=AF_INET;
	if ((res=inet_pton(AF_INET, ns, &(saddr->sin_addr)))<0) {
		error("In store_ns: error converting str to sockaddr-> %s\n", strerror(errno));
		return -1;
	} else if (res==0) {
		error("In store_ns: invalid address\n");
		return -1;
	}
	saddr->sin_port=htons(53);
        (*ns_count)++;
        return 0;
}

/*
 * Reads resolv.conf, searching nameserver lines.
 * Takes the ip address from these lines and calls store_ns
 * "nameserver 127.0.0.1" is discraded to remove looping beahviors
 * The valid nameservers are stored in `nsbuf' array which must have at least
 * of `MAXNSSERVERS' members. The number of stored nameservers is written in
 * `*ns_count' and it is returned.
 * If an error occurred or no hostnames are available -1 is returned.
 */
int collect_resolv_conf(char *resolve_conf, struct sockaddr_in *nsbuf, uint8_t *ns_count)
{
        FILE *erc;
        char buf[64],*crow,tbuf[64];
        int i=0;

        if (!(erc=fopen(resolve_conf,"r"))) {
                error("In collect_resolv_conf: error -> %s.", strerror(errno));
                return -1;
        }
        while ((crow=fgets((char*)buf,64,erc))) {
		if((*buf)=='#' || (*buf)=='\n' || !(*buf))
			/* Strip off the comment lines */
			continue;
			
                if (!(crow=strstr(buf,"nameserver ")))
			continue;
		crow+=11;
		while (*(crow+i) && *(crow+i)!='\n') {
                        *(tbuf+i)=*(crow+i);
                        i++;
                }
                *(tbuf+i)=0;
                store_ns(tbuf, nsbuf, ns_count);
                i=0;
        }
        if (fclose(erc)!=0) {
                error("In collect_resolv_conf: closing resolv.conf -> %s",strerror(errno));
                return -1;
        }
        if (!(*ns_count)) {
                error("In collect_resolv_conf: no dns server was found.");
                return -1;
        }
        return *ns_count;
}

/*
 * This function must be called before all.
 * Sets the default realm for domain name resolution
 * and stores infos about nameservers for dns query.
 * On error -1 is returned.
 */
int andns_init(int restricted, char *resolv_conf)
{
        int i,res;
        char msg[(INET_ADDRSTRLEN+2)*MAXNSSERVERS];
	char buf[INET_ADDRSTRLEN];
	struct sockaddr_in *saddr;

        _default_realm_=(restricted)?INET_REALM:NTK_REALM;
        _andns_ns_count_=0;

	memset(msg,0,(INET_ADDRSTRLEN+2)*MAXNSSERVERS);

	res=collect_resolv_conf(resolv_conf, _andns_ns_, &_andns_ns_count_);
        if (res == -1) {
                debug(DBG_NORMAL, "ALERT: DNS forwarding disable");
                _dns_forwarding_=0;
                return -1;
        }

	/* 
	 * Debug message 
	 */
        for (i=0;i<_andns_ns_count_;i++) {
		saddr=_andns_ns_+i;
                if(inet_ntop(saddr->sin_family,(void*)&((saddr)->sin_addr),buf,INET_ADDRSTRLEN)) {
			strncat(msg,buf,INET_ADDRSTRLEN);
			strncat(msg,i==_andns_ns_count_-1?". ":", ",2);
		} else 
			error("In andns_init: error converting sockaddr -> %s.",strerror(errno));
	}
	debug(DBG_NORMAL, "Andns init: DNS query inet-related will be forwarded to: %s",msg);
	
	_dns_forwarding_=_andns_ns_count_?1:0;
        return 0;
}

/*
 * This function is a copy of andns_rslv.
 * It is called by andns_rslv in the case of:
 *
 * 	- NTK protocol query
 * 		*AND*
 * 	- NTK realm query
 */
char* andns_rslv_ntk_ntk(char *msg,int msglen,
		char *answer,int *answ_len)
{
	andns_pkt *ap;
	int res;

	if ((res=apkt(msg,msglen,&ap))==0)  // pkt malformed
        	return NULL; // discard pkt!
        if (res==-1) // Cannot understand
                goto andns_eintrprt_return; // release *ap
        switch(ap->qtype) { // query type
                        /*
                         * This calls fills *ap with appropriate values.
                         * After these calls, *ap could be pkt-ized and sended
                         */
        	case AT_A:
                	res=a_a_resolve(ap);
                        break;
                case AT_PTR:
                        res=a_ptr_resolve(ap);
                        break;
                case AT_MX:
                        res=a_mx_resolve(ap);
                        break;
                default:
                        goto andns_eintrprt_return; // release *ap
	}
        if((res=apktpack(ap,answer))==-1) // this call free ap
        	goto andns_esrvfail_return; // Error packing *ap. In this case
                                                      // answer is not allocated.
        *answ_len=res;
        return answer; // all is OK. res is the answer len.

andns_eintrprt_return:
        destroy_andns_pkt(ap);
        memcpy(answer,msg,msglen);
        AANSW(answer,RCODE_EINTRPRT);
        *answ_len=msglen;
        return answer;
andns_esrvfail_return:
        memcpy(answer,msg,msglen);
        AANSW(answer,RCODE_ESRVFAIL);
        *answ_len=msglen;
        return answer;
}

/*
 * This function is a copy of andns_rslv.
 * It is called by andns_rslv in the case of:
 *
 * 	- NTK protocol query
 * 		*AND*
 * 	- INET realm query
 */
char* andns_rslv_ntk_inet(char *msg,int msglen,
		char *answer,int *answ_len)
{
	andns_pkt *ap;
	dns_pkt *dp;
	int res,type;

        if (!_dns_forwarding_)
        	return NULL; /* there is no nameserver to forward the query */
        if ((res=apkt(msg,msglen,&ap))==0)
        	return NULL; /* discard pkt */
        if (res==-1)
        	goto andns_eintrprt_return; /* release *ap */
        switch(ap->qtype) {
                case AT_A:
                        type=T_A;
                        break;
                case AT_PTR:
                        type=T_PTR;
                        break;
                case AT_MX:
                        type=T_MX;
                        break;
                default:
                        goto andns_eintrprt_return; // release *ap
	}
        /* Prepare answer to be filled.
         * Now, the query is forwarded to some nameserver.
	 */
        if((res=res_query(ap->qstdata,C_IN,type,(unsigned char*) answer,DNS_MAX_SZ))==-1) {
                        /*
                         * Query not sent.
                         */
                destroy_andns_pkt(ap);
		goto andns_esrvfail_return;
	}
	if ((res=dpkt(answer,res,&dp))==0) {
        /*
         * The answer from nameserver is malformed
         * In this case dp is not allocated.
         */
		destroy_andns_pkt(ap);
                return NULL;
	}
	if (res==-1) {
        /*
         * The answer from nameserver is not
         * interpretable.
         */
		destroy_dns_pkt(dp);
		goto andns_eintrprt_return; // release *ap
	}
        /*
         * Now, we need to translate the dns answer to an andns answer.
         */
	if ((res=danswtoaansw(dp,ap,answer))==-1) {
		/* Translation failed.*/
		destroy_dns_pkt(dp);
		destroy_andns_pkt(ap);
		goto andns_esrvfail_return;
	}
                
	destroy_dns_pkt(dp);
	if((res=apktpack(ap,answer))==-1) // this call free ap
		goto andns_esrvfail_return;
	*answ_len=res;
	return answer;

andns_eintrprt_return:
        destroy_andns_pkt(ap);
        memcpy(answer,msg,msglen);
        AANSW(answer,RCODE_EINTRPRT);
        *answ_len=msglen;
        return answer;
andns_esrvfail_return:
        memcpy(answer,msg,msglen);
        AANSW(answer,RCODE_ESRVFAIL);
        *answ_len=msglen;
        return answer;
}

/*
 * This function is a copy of andns_rslv.
 * It is called by andns_rslv in the case of:
 *
 * 	- DNS protocol query
 */
char* andns_rslv_inet(char *msg,int msglen,
		char *answer,int *answ_len)
{
	dns_pkt *dp;
	int res;

	if ((res=dpkt(msg,msglen,&dp))==0) /* msg malformed */
		return NULL;
	if (res==-1)/* error interpreting msg */
		goto dns_eintrprt_return; /* release *ap */
	switch((dp->pkt_qst)->qtype) {
		/*
		 * The d_*_resolve family functions return 1
		 * if the query has to be forwarded to some nameserver,
		 * i.e., if the query is inet-related.
		 * Otherwise, the answer is made in andna and these
		 * functions fills *dp whith apropriate values,
		 * making it ready to be pkt-ized.
		 */
		case T_A:
			res=d_a_resolve(dp);
			break;
		case T_PTR:
			res=d_ptr_resolve(dp);
			break;
		case T_MX:
			res=d_mx_resolve(dp);
			break;
		default:
			goto dns_eintrprt_return; // release *dp
	}
	if (res==1) {
	       /* Packet forwarding! */
		if ((res=dns_forward(dp,msg,msglen,answer))==-1)
			goto dns_esrvfail_return;
	} else if((res=dpktpack(dp,answer,0))==-1) 
		/* this call free dp */
        	goto dns_esrvfail_return;
        *answ_len=res;
        return answer;

dns_eintrprt_return:
        destroy_dns_pkt(dp);
        memcpy(answer,msg,msglen);
        DANSW(answer,RCODE_EINTRPRT);
        *answ_len=msglen;
        return answer;
dns_esrvfail_return:
        memcpy(answer,msg,msglen);
        DANSW(answer,RCODE_ESRVFAIL);
        *answ_len=msglen;
        return answer;
}


/*
 * This is the main function for the resolution: the dns_wrapper receive the
 * buffer and rslv cares about building the answer.
 * `answer' is the buffer where the answer will be stored, it must be at
 * least of `ANDNS_MAX_SZ' bytes. 
 *
 * Returns:
 * 	NULL if the pkt has to be discarded.
 * 	A ptr to the answer to be sended if OK:
 * 		in this case, answ_len is filled with
 * 		the answer len.
 */
char *andns_rslv(char *msg, int msglen, 
		char *answer, int *answ_len)
{
	memset(answer, 0, ANDNS_MAX_SZ);
	
	if (andns_proto(msg)==NK_NTK) 
		return andns_rslv_ntk_ntk(msg,msglen,answer,answ_len); 
	else if (andns_proto(msg)==NK_INET) 
		return andns_rslv_ntk_inet(msg,msglen,answer,answ_len);
	else if (andns_proto(msg)==NK_OLDSTYLE) 
		return andns_rslv_inet(msg,msglen,answer,answ_len);
	else // which protocol are you using?
		return NULL; // discard pkt plz
}
	
/*
 * This function takes a query formulated with
 * the andns protocol and fills the andns_pkt *ap
 * with the answer.
 * The query is a A_TYPED query, i.e. host->ip.
 * On unsuccesful anserw, -1 is returned. 0 Otherwise.
 */

int ns_general_send(char *msg,int msglen,char *answer,int *anslen)
{
        int res,i;

        for (i=0;i<MAXNSSERVERS && i<_andns_ns_count_;i++) {
                res=ns_send(msg,msglen,answer,anslen,_andns_ns_+i,sizeof(struct sockaddr_in));
                if (res==-1) continue;
		else break;
        }
	return res==-1?res:0;
//        if (res==-1) return -1;
  //      return 0;
}

int ns_send(char *msg,int msglen, char *answer,int *anslen,struct sockaddr_in *ns,socklen_t nslen)
{
        int s;
	ssize_t len;

	if ((s=new_dgram_socket(AF_INET))==-1) {
		error("In ns_send: can not create socket.");
		return -1;
	}
	if ((connect(s,(struct sockaddr*)ns,nslen))) {
		error("In ns_send: error connecting socket -> %s.",strerror(errno));
		goto close_return;
	}
	len=inet_send(s,msg,msglen,0);
        if (len==-1) {
                error("In ns_send. Pkt not forwarded. %s",strerror(errno));
		goto close_return;
        }
        len=inet_recv(s,(void*)answer,DNS_MAX_SZ,0);
        if (len==-1) {
                error("In ns_send. Pkt not received.");
		goto close_return;
        }
        *anslen=len;
	close(s);
        return 0;
close_return:
	close(s);
	return -1;
}


int dns_forward(dns_pkt *dp,char *msg,int msglen,char* answer)
{
	dns_pkt *dp_forward;
	char fwdbuf[DNS_MAX_SZ];
	int res,len;

	if (!_dns_forwarding_) {
		error("In rslv: dns forwardind is disable.");
		goto failing;
	}
	debug(DBG_INSANE, "DNS FORWARDING!");
	if (!is_prefixed(dp)) {
		/*res=res_send((const unsigned char*)msg,msglen,(unsigned char*)answer,DNS_MAX_SZ);*/
		if(ns_general_send(msg,msglen,answer,&res)) {
			error("In dns_forwarding: forward fail.");
			goto failing;
		}
		destroy_dns_pkt(dp);
		return res;
	}
	/* prepare to re-format query without prefix */
	dp_forward=dpktcpy(dp);
	memset(fwdbuf,0,DNS_MAX_SZ);
	if ((res=dpktpack(dp_forward,fwdbuf,1))==-1) { /* dp_foward is destroyed */
		error("In rslv: error packing forwarding buffer.");
		goto failing;
	}
/*	res=res_send((const unsigned char*)fwdbuf,res,(unsigned char*)answer,DNS_MAX_SZ);
	if (res == -1) {
		error("DNS Forwarding error.");
		printf("Error forwarding!\n");
		goto failing;
	}*/
	if (ns_general_send(fwdbuf,res,answer,&len)) {
		error("DNS Forwarding error.");
		goto failing;
	}
	if ((res=dpkt(answer,len,&dp_forward))==-1) {
		error("In rslv: can not unpack msg from nameserver.");
		goto failing;
	}
	dpktacpy(dp,dp_forward,INET_REALM_PREFIX);
	destroy_dns_pkt(dp_forward);
	dp->pkt_hdr.nscount=0;
	dp->pkt_hdr.arcount=0;
	if ((res=dpktpack(dp,answer,0))==-1) {
		error("In rslv: can not pack prefixed pkt.");
		return -1;
	}
	return res;
failing:
	destroy_dns_pkt(dp);
	return -1;
}
					

int a_a_resolve(andns_pkt *ap)
{
	int res;
	andns_pkt_data *apd;
	inet_prefix ipres;
		
	if ((res=andna_resolve_hname(ap->qstdata_nopref,&ipres))==-1) {
		ap->rcode=RCODE_ENSDMN;
		ap->qr=1;
		return -1;
	}

	ap->rcode=RCODE_NOERR;
	ap->qr=1;
	ap->ancount++;
	apd=andns_add_answ(ap);
	apd->rdlength=ipres.len;
	inet_htonl(ipres.data, ipres.family);
	if (ipres.family==AF_INET)
       	        memcpy(apd->rdata,ipres.data,4);
       	else
       		memcpy(apd->rdata,ipres.data,16);
	return 0;
}
/*
 * Idem. The query is PTR_TYPED query.
 */
int a_ptr_resolve(andns_pkt *ap)
{
	andns_pkt_data *apd;

	inet_prefix ipres;
	char **hnames;
	int i;
	int res;

	if ((res=str_to_inet(ap->qstdata_nopref,&ipres))==-1) {
		ap->rcode=RCODE_EINTRPRT;
		ap->qr=1;
		return -1;
	}
	if ((res=andna_reverse_resolve(ipres,&hnames))==-1) {
		ap->rcode=RCODE_ENSDMN;
		ap->qr=1;
		return -1;
	}
	for (i=0;i<res;i++) {
		apd=andns_add_answ(ap);
		apd->rdlength=strlen(hnames[i]);
		strcpy(apd->rdata,hnames[i]);
		xfree(hnames[i]);
		ap->ancount++;
	}
	ap->rcode=RCODE_NOERR;
	ap->qr=1;
	return 0;
}
/*
 * Idem. The query is MX_TYPED query.
 */
int a_mx_resolve(andns_pkt *ap)
{
	ap->rcode=RCODE_ENIMPL;
	ap->qr=1;
	return 0;
}

int d_a_resolve(dns_pkt *dp)
{
	dns_pkt_a *dpa;
	inet_prefix ipres;
	int res;

	if (andns_realm(dp->pkt_qst,NULL)==INET_REALM)
		return 1;
	
     	if ((res=andna_resolve_hname(dp->pkt_qst->qname_nopref, &ipres))==-1) {
                (dp->pkt_hdr).rcode=RCODE_ENSDMN;
        	(dp->pkt_hdr).qr=1;
                return -1;
        }
	
        (dp->pkt_hdr).rcode=RCODE_NOERR;
        (dp->pkt_hdr).qr=1;
        dpa=DP_ADD_ANSWER(dp);
	dpa->type=T_A;
	dpa->class=C_IN;
	dpa->ttl=DNS_TTL;
	strcpy(dpa->name,dp->pkt_qst->qname);
        dpa->rdlength=ipres.len;
	inet_htonl(ipres.data, ipres.family);
        if (ipres.family==AF_INET)
                memcpy(dpa->rdata,ipres.data,4);
        else
                memcpy(dpa->rdata,ipres.data,16);
        return 0;
}

int d_ptr_resolve(dns_pkt *dp)
{
	dns_pkt_a *dpa;
	inet_prefix ipres;
	char **hnames;
	int i, res;
	char addr[INET_ADDRSTRLEN];

	if (andns_realm(dp->pkt_qst,NULL)==INET_REALM)
		return 1;
	
	if (swapped_straddr(dp->pkt_qst->qname_nopref,addr)) {
		error("In d_ptr_resolve: can not swap address.");
		(dp->pkt_hdr).rcode=RCODE_EINTRPRT;
        	(dp->pkt_hdr).qr=1;
		return -1;
	}
	if ((res=str_to_inet(addr, &ipres))==-1) {	
		(dp->pkt_hdr).rcode=RCODE_EINTRPRT;
        	(dp->pkt_hdr).qr=1;
		return -1;
	}
	if ((res=andna_reverse_resolve(ipres, &hnames))==-1) {
        	(dp->pkt_hdr).rcode=RCODE_ENSDMN;
        	(dp->pkt_hdr).qr=1;
        	return -1;
        }
	for (i=0;i<res;i++) {
		dpa=DP_ADD_ANSWER(dp);
		dpa->type=T_PTR;
		dpa->class=C_IN;
		dpa->ttl=DNS_TTL;
		strcpy(dpa->name,dp->pkt_qst->qname);
		strcpy(dpa->rdata,hnames[i]);
//		if (nametolbl(hnames[i], dpa->rdata)==-1)
//			memset(dpa->rdata,0,MAX_HNAME_LEN);

		xfree(hnames[i]);
	}
	xfree(hnames);

        (dp->pkt_hdr).rcode=RCODE_NOERR;
        (dp->pkt_hdr).qr=1;
	return 0;
}

int d_mx_resolve(dns_pkt *dp)
{
	(dp->pkt_hdr).rcode=RCODE_ENIMPL;
        (dp->pkt_hdr).qr=1;
	return 0;
}
