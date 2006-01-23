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


/*
 * andns_mem.c: memory library for andns structs
 */


#include "andns.h"
#include "andns_mem.h"
#include "xmalloc.h"
#include <stdio.h>
#include <string.h>

/* debug includes 
#include "andns.h"
#include "andns_mem.h"
#include "andna_fake.h"
#include <stdio.h>
#include <string.h> */


/*
 * DNS FUNCTIONS
 */
dns_pkt* create_dns_pkt()
{
	dns_pkt *dp;
	dp=xmalloc(DNS_PKT_SZ);
	dp->pkt_qst=NULL;
	dp->pkt_answ=NULL;
	dp->pkt_add=NULL;
	dp->pkt_auth=NULL;
	return dp;
}

dns_pkt_qst* create_dns_pkt_qst()
{
	dns_pkt_qst *dpq;
	dpq=xmalloc(DNS_PKT_QST_SZ);
	dpq->next=NULL;
	memset(dpq->qname,0,MAX_HNAME_LEN+REALM_PREFIX_LEN);
	memset(dpq->qname_nopref,0,MAX_HNAME_LEN);
	return dpq;
}
dns_pkt_a* create_dns_pkt_a()
{
	dns_pkt_a *dpa;
	dpa=xmalloc(DNS_PKT_A_SZ);
	memset(dpa->name,0,MAX_HNAME_LEN);
	memset(dpa->rdata,0,MAX_HNAME_LEN);
	dpa->next=NULL;
	return dpa;
}

dns_pkt_qst* dns_add_qst(dns_pkt *dp)
{
	dns_pkt_qst *dpq,*temp;
	dpq=create_dns_pkt_qst();
	temp=dp->pkt_qst;
	if (!temp) { dp->pkt_qst=dpq;return dpq;}
	while (temp->next) temp=temp->next;
	temp->next=dpq;
	return dpq;
}
void dns_del_last_qst(dns_pkt *dp)
{
	dns_pkt_qst *dpq=dp->pkt_qst;
	if (!dpq) return;
	if (!(dpq->next)){
		xfree(dpq);
		dp->pkt_qst=NULL;
		return;
	}
	while ((dpq->next)->next);
	xfree(dpq->next);
	dpq->next=NULL;
	return;
}
	
dns_pkt_a* dns_add_a(dns_pkt_a **dpa)
{
	dns_pkt_a *dpa_add,*a;
	int count=0;

	a=*dpa;
	dpa_add=create_dns_pkt_a();
	if (!a) {
		(*dpa)=dpa_add;
	}
	else {
		while (a->next)
		{
			a=a->next;
			count++;
		}
		a->next=dpa_add;
	}
	return dpa_add;
}

void dpktacpy(dns_pkt *dst,dns_pkt *src,const char *prefix)
{
	dns_pkt_a *dpas,*dpad;
	int slen;
	int yet_pref=0;

	dpas=src->pkt_answ;
	while(dpas) {
		dpad=DP_ADD_ANSWER(dst);
		memcpy(dpad,dpas,sizeof(dns_pkt_a));
		dpad->next=NULL;
		if (prefix && !yet_pref) {
			slen=strlen(dpad->name);
			memcpy(dpad->name+slen,prefix,REALM_PREFIX_LEN);
			*(dpad->name+slen+REALM_PREFIX_LEN)=0;
			yet_pref=1;
		}
		dpas=dpas->next;
	}
	dpas=src->pkt_auth;
	while(dpas) {
		dpad=DP_ADD_AUTH(dst);
		memcpy(dpad,dpas,sizeof(dns_pkt_a));
		dpad->next=NULL;
/*		if (prefix) {
			slen=strlen(dpad->name);
			memcpy(dpad->name+slen,prefix,REALM_PREFIX_LEN);
			*(dpad->name+slen+REALM_PREFIX_LEN)=0;
		}*/
		dpas=dpas->next;
	}
	dpas=src->pkt_add;
	while(dpas) {
		dpad=DP_ADD_ADD(dst);
		memcpy(dpad,dpas,sizeof(dns_pkt_a));
		dpad->next=NULL;
/*		if (prefix) {
			slen=strlen(dpad->name);
			memcpy(dpad->name+slen,prefix,REALM_PREFIX_LEN);
			*(dpad->name+slen+REALM_PREFIX_LEN)=0;
		}*/
		dpas=dpas->next;
	}
}
	

dns_pkt* dpktcpy(dns_pkt *src)
{
	dns_pkt *dst;
	dns_pkt_qst *dpq,*dpq_src;

	dst=create_dns_pkt();
	memcpy(dst,src,sizeof(dns_pkt));
	dst->pkt_qst=NULL;
	dst->pkt_answ=NULL;
	dst->pkt_auth=NULL;
	dst->pkt_add=NULL;

	dpq_src=src->pkt_qst;
	while (dpq_src) {
		dpq=dns_add_qst(dst);
		memcpy(dpq,dpq_src,sizeof(dns_pkt_qst));
		dpq_src=dpq_src->next;
	}
	dpktacpy(dst,src,NULL);
	return dst;
}

void destroy_dns_pkt(dns_pkt *dp)
{
	dns_pkt_a *dpa,*dpa_t;
	dns_pkt_qst *dpq,*dpq_t;

	if (dp->pkt_qst)
	{
		dpq=dp->pkt_qst;
		while (dpq)
		{
			dpq_t=dpq->next;
			xfree(dpq);
			dpq=dpq_t;
		}
	}
	if (dp->pkt_answ)
	{
		dpa=dp->pkt_answ;
		while (dpa)
		{
			dpa_t=dpa->next;
			xfree(dpa);
			dpa=dpa_t;
		}
	}
	if (dp->pkt_add)
	{
		dpa=dp->pkt_add;
		while (dpa)
		{
			dpa_t=dpa->next;
			xfree(dpa);
			dpa=dpa_t;
		}
	}
	if (dp->pkt_auth)
	{
		dpa=dp->pkt_auth;
		while (dpa)
		{
			dpa_t=dpa->next;
			xfree(dpa);
			dpa=dpa_t;
		}
	}
	xfree(dp);
	return;
}
	

/*
 * ANDNS FUNCTIONS
 */

andns_pkt* create_andns_pkt()
{
	andns_pkt *ap;
	ap=xmalloc(ANDNS_PKT_SZ);
	ap->pkt_answ=NULL;
	memset(ap->qstdata,0,MAX_ANDNS_QST_LEN+REALM_PREFIX_LEN);
	memset(ap->qstdata_nopref,0,MAX_ANDNS_QST_LEN);
	return ap;
}

andns_pkt_data* create_andns_pkt_data()
{
	andns_pkt_data *apd;
	apd=xmalloc(ANDNS_PKT_DATA_SZ);
	apd->next=NULL;
	memset(apd->rdata,0,MAX_ANDNS_ANSW_LEN);
	return apd;
}
andns_pkt_data* andns_add_answ(andns_pkt *ap)
{
	andns_pkt_data *apd,*a;
	
	apd=create_andns_pkt_data();
	a=ap->pkt_answ;
	if (!a) {ap->pkt_answ=apd;return apd;}
	while (a->next) a=a->next;
	a->next=apd;
	return apd;;
}
	
void destroy_andns_pkt(andns_pkt *ap)
{
	/*
	if (ap->pkt_qst)
	{
		apd=ap->pkt_qst;
		while (apd)
		{
			apd_t=apd->next;
			xfree(apd);
			apd=apd_t;
		}
	}*/
	if (ap->pkt_answ)
	{
		andns_pkt_data *apd,*apd_t;
		apd=ap->pkt_answ;
		while (apd)
		{
			apd_t=apd->next;
			xfree(apd);
			apd=apd_t;
		}
	}
	xfree(ap);
}
