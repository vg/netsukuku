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


/*
 * This function must be called before all.
 * Sets the default realm for domain name resolution
 * and stores infos about nameservers for dns query.
 * It removes from the structure _res the nameserver entry which
 * belongs to localhost. In this way, the dns_forwarding
 * won't finish in a infinite loop.
 */
int andns_init(int restricted)
{
	return andns_pkt_init(restricted);
}

/*
 * This is the main function for the resolution: the dns_wrapper receive the
 * buffer and rslv cares about building the answer.
 * `answer_buf' is the buffer where the answer will be stored, it must be at
 * least of `ANDNS_MAX_SZ' bytes. If it is null a new buffer will be
 * allocated.
 *
 * Returns:
 * 	NULL if the pkt has to be discarded.
 * 	A ptr to the answer to be sended if OK:
 * 		in this case, answ_len is filled with
 * 		the answer len.
 */
char *andns_rslv(char *msg, int msglen, 
		char *answer_buf, int *answ_len)
{
	andns_pkt *ap;
	dns_pkt *dp;
	int type, res;
	char *answer;

	answer=answer_buf;
	if(!answer)
		answer=xmalloc(ANDNS_MAX_SZ);
	memset(answer, 0, ANDNS_MAX_SZ);
		
	
	if (andns_proto(msg)==NK_NTK) {
		
		/*
		 * The query is formulated with ntk style AND
		 * the query is ntk-related.
		 */
		// Try to unpack msg
		if ((res=apkt(msg,msglen,&ap))==0)  // pkt malformed
			return NULL; // discard pkt!
		if (res==-1) // Cannot understand
			goto andns_eintrprt_return; // release *ap
		switch(ap->qtype) // query type
		{
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
		
	} else if (andns_proto(msg)==NK_INET) {
		
		/*
		 * The query is formulated with ntk style, BUT NOT
		 * ntk related, i.e. inet-related.
		 * We need to forward the query to some nameserver.
		 */
		
		if (!_dns_forwarding_)
			return NULL; // there is no nameserver to forward the query
		// see above
		if ((res=apkt(msg,msglen,&ap))==0)
			return NULL;
		if (res==-1)
			goto andns_eintrprt_return; // release *ap
		switch(ap->qtype)
		{
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
		// Prepare answer to be filled.
		// Now, the query is forwarded to soma nameserver.
		if((res=res_query(ap->qstdata,C_IN,type,(unsigned char*) answer,DNS_MAX_SZ))==-1) 
		{
			/*
			 * Query not sent.
			 */
			destroy_andns_pkt(ap);
			goto andns_esrvfail_return;
		}
		if ((res=dpkt(answer,res,&dp))==0)
		{
			/*
			 * The answer from nameserver is malformed
			 * In this case dp is not allocated.
			 */
			destroy_andns_pkt(ap);
			if(!answer_buf)
				xfree(answer);
			return NULL;
		}
		if (res==-1)
		{
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
		if ((res=danswtoaansw(dp,ap,answer))==-1)
		{
			// Translation failed.
			destroy_dns_pkt(dp);
			destroy_andns_pkt(ap);
			goto andns_esrvfail_return;
		}
	
		destroy_dns_pkt(dp);
		if((res=apktpack(ap,answer))==-1) // this call free ap
			goto andns_esrvfail_return;
		*answ_len=res;
		return answer;
	}
	else if (andns_proto(msg)==NK_OLDSTYLE)
	{
		/*
		 * The query is formulated with dns protocol
		 */
		if ((res=dpkt(msg,msglen,&dp))==0) // msg malformed
			return NULL;
		if (res==-1)// error interpreting msg
			goto dns_eintrprt_return; // release *ap
		switch((dp->pkt_qst)->qtype)
		{
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
		if (res==1)
		{ 	// Packet forwarding!
			debug(DBG_NOISE, "In rslv: forwarding packet....");
			destroy_dns_pkt(dp);
			if (!_dns_forwarding_)
			{
				error("In rslv: dns forwardind is disable.");
				goto dns_esrvfail_return;
			}
			debug(DBG_NOISE, "ID %d %d",msg[0],msg[1]);
			res=res_send((const unsigned char*)msg,msglen,(unsigned char*)answer,DNS_MAX_SZ);
			if (res == -1)
				goto dns_esrvfail_return;
			
			debug(DBG_NOISE, "ID %d %d",msg[0],msg[1]);
			debug(DBG_NOISE, "ID ANSW %d %d",answer[0],answer[1]);
			memcpy(answer,msg,2);
			*answ_len=res;
			return answer;
		}
		if((res=dpktpack(dp,answer))==-1) // this call free dp
			goto dns_esrvfail_return;
		*answ_len=res;
		return answer;
	} else // which protocol are you using?
		return NULL; // discard pkt plz

// copy original msg and fill whith appropriate
// rcode. I.e., answer is the question itself, but 
// whith rcode modified.
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
 * This function takes a query formulated with
 * the andns protocol and fills the andns_pkt *ap
 * with the answer.
 * The query is a A_TYPED query, i.e. host->ip.
 * On unsuccesful anserw, -1 is returned. 0 Otherwise.
 */
int a_a_resolve(andns_pkt *ap)
{
	int res;
	andns_pkt_data *apd;
	inet_prefix ipres;
		
	if ((res=andna_resolve_hname(ap->qstdata,&ipres))==-1)
	{
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

	if ((res=str_to_inet(ap->qstdata,&ipres))==-1)
	{
		ap->rcode=RCODE_EINTRPRT;
		ap->qr=1;
		return -1;
	}
	if ((res=andna_reverse_resolve(ipres,&hnames))==-1)
	{
		ap->rcode=RCODE_ENSDMN;
		ap->qr=1;
		return -1;
	}
	for (i=0;i<res;i++)
	{
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
	char *temp;
	int res;

	if (andns_realm((dp->pkt_qst)->qname)==INET_REALM)
		return 1;
	
	temp=rm_realm_prefix((dp->pkt_qst)->qname);
     	if ((res=andna_resolve_hname(temp, &ipres))==-1)
        {
                xfree(temp);
                (dp->pkt_hdr).rcode=RCODE_ENSDMN;
        	(dp->pkt_hdr).qr=1;
                return -1;
        }
        xfree(temp);
	
        (dp->pkt_hdr).rcode=RCODE_NOERR;
        (dp->pkt_hdr).qr=1;
        DP_ANCOUNT(dp)++;
        dpa=dns_add_a(&(dp->pkt_answ));
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
	char **hnames,*temp;
	int i, res;

	if (andns_realm((dp->pkt_qst)->qname)==INET_REALM)
		/* XXX: why ? */
		return 1;
	
	/* Alpt: isn't? (dp->pkt_qst)->qname just an IP ?
	 * temp=rm_realm_prefix((dp->pkt_qst)->qname); */
	temp=(dp->pkt_qst)->qname;	
	if ((res=str_to_inet(temp, &ipres))==-1)
	{	
		(dp->pkt_hdr).rcode=RCODE_EINTRPRT;
        	(dp->pkt_hdr).qr=1;
		return -1;
	}
	if ((res=andna_reverse_resolve(ipres, &hnames))==-1)
        {
        	(dp->pkt_hdr).rcode=RCODE_ENSDMN;
        	(dp->pkt_hdr).qr=1;
        	return -1;
        }
	for (i=0;i<res;i++)
	{
		dpa=dns_add_a(&(dp->pkt_answ));
		dpa->type=T_PTR;
		dpa->class=C_IN;
		dpa->ttl=DNS_TTL;
		strcpy(dpa->name,"localhost");
		if (nametolbl(hnames[i], dpa->rdata)==-1)
			memset(dpa->rdata,0,MAX_HNAME_LEN);

		xfree(hnames[i]);
	}
	xfree(hnames);

        (dp->pkt_hdr).rcode=RCODE_NOERR;
        (dp->pkt_hdr).qr=1;
	DP_ANCOUNT(dp)+=res;
	return 0;
}

int d_mx_resolve(dns_pkt *dp)
{
	(dp->pkt_hdr).rcode=RCODE_ENIMPL;
        (dp->pkt_hdr).qr=1;
	return 0;
}
