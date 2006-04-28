	         /**************************************
	        *     AUTHOR: Federico Tomassini        *
	       *     Copyright (C) Federico Tomassini    *
	      *     Contact effetom@gmail.com             *
	     ***********************************************
	     *******          BEGIN 3/2006          ********
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

#define _GNU_SOURCE
#include <string.h>

#include "includes.h"
#include "misc.h"
#include "andns.h"
#include "log.h"
#include "err_errno.h"
#include "xmalloc.h"
#include "andna.h"
#include "andnslib.h"
#include "dnslib.h"


static uint8_t _dns_forwarding_;
static uint8_t _andns_ns_count_;
static uint8_t _default_realm_;

static struct addrinfo _ns_filter_;
static struct addrinfo *_andns_ns_[MAXNSSERVERS];



			/* INIT FUNCTIONS */

int debug_andna_resolve_hname(char *s,inet_prefix *addr)
{
        char *ciccio="111.222.123.123";
        str_to_inet(ciccio,addr);
        return 0;
}
int debug_andna_reverse_resolve(inet_prefix addr,char ***hnames)
{
        char **crow;
        crow=(char**)xmalloc(2*sizeof(char));
        crow[0]=(char*)xmalloc(40);
        crow[1]=(char*)xmalloc(40);
        strcpy(crow[0],"ciaomamma");
        strcpy(crow[1],"depausceve");
        *hnames=crow;
        return 2;
}

/*
 * Saves on `nsbuf' and `ns_count' the ip
 * address ns: these infos will be used for DNS
 * forwarding.
 *
 * Returns:
 *      -1 on error
 *       0 if OK
 */
int store_ns(char *ns)
{
        int res;
	struct addrinfo *ai;

        if (strstr(ns, "127.0.0.")) /* TODO: make it proto independent  */
                return -1;

	ai=_andns_ns_+_andns_ns_count_;
	res=getaddrinfo(ns,DNS_PORT_STR,_ns_filter_,&ai);
	if (!res) {
		debug(DBG_NORMAL,"In store_ns(): gai %s -> %s",ns,gai_strerror(errno));
		return -1;
	}
	_andns_ns_count_++;
        return 0;
}
/*
 * Reads resolv.conf, searching nameserver lines.
 * Takes the ip address from these lines and calls store_ns.
 * "nameserver 127.0.0.1" is discarded to remove looping behaviors.
 * The number of stored nameservers is written in
 * `*ns_count' and it is returned.
 * If an error occurred or no hostnames are available, -1 is returned.
 */
int collect_resolv_conf(char *resolve_conf)
{
        FILE *erc;
        char buf[512],*crow,*t;
        int i=0;

        if (!(erc=fopen(resolve_conf,"r"))) {
                error("In collect_resolv_conf: "
			"error -> %s.", strerror(errno));
		err_ret(ERR_RSLERC,-1);
        }
        while ((crow=fgets(buf,512,erc)) && *_andns_ns_count_<MAXNSSERVERS) {
                if (!(crow=strstr(buf,"nameserver "))) /* is a good line? */
                        continue;
		
		/* Skip if the line is commented */
		*crow=0;
		if(strchr(buf, "#"))
			continue;
		
                crow+=11;
	
		/* remove unwanted chars */
		strip_char(crow, '\t');
		strip_char(crow, ' ');
		strip_char(crow, '\n');
			
                store_ns(crow); /* finally store nameserver */
        }
        if (fclose(erc)!=0) {
                error("In collect_resolv_conf: closing "
			"resolv.conf -> %s",strerror(errno));
		err_ret(ERR_RSLERC,-1);
        }
        if (!_andns_ns_count_) 
		err_ret(ERR_RSLNNS,-1);
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
        char msg[(INET6_ADDRSTRLEN+2)*MAXNSSERVERS];
        char buf[INET6_ADDRSTRLEN];
        struct sockaddr_in *saddr;

	memset(_ns_filter_,0,sizeof(struct addrinfo));
	_ns_filter_->ai_socktype=SOCK_DGRAM;

        _default_realm_=(restricted)?INET_REALM:NTK_REALM;
        _andns_ns_count_=0;

        memset(msg,0,(INET_ADDRSTRLEN+2)*MAXNSSERVERS);

        res=collect_resolv_conf(resolv_conf);
        if (res <=0) {
                _dns_forwarding_=0;
		debug(DBG_NORMAL,err_str);
                err_ret(ERR_RSLAIE,-1);
        }

        /*
         * Debug message
         */
        for (i=0;i<_andns_ns_count_;i++) {
                saddr=(_andns_ns_+i)->ai_addr;
                if(inet_ntop(saddr->sin_family,
			(void*)&((saddr)->sin_addr),buf,INET6_ADDRSTRLEN)) {
                        strncat(msg,buf,INET_ADDRSTRLEN);
                        strncat(msg,i==_andns_ns_count_-1?". ":", ",2);
                } else
                        error("In andns_init: error "
				"converting sockaddr -> %s.",\
				strerror(errno));
        }
        debug(DBG_NORMAL,"Andns init: DNS query inet-related "
			"will be forwarded to: %s",msg);

        _dns_forwarding_=1;
        return 0;
}

			/* NET FUNCTIONS */

int ns_general_send(char *msg,int msglen,char *answer,int anslen)
{
        int res,i;

        for (i=0;i<MAXNSSERVERS && i<_andns_ns_count_;i++) {
		res=ai_squit(_andns_ns_+i,msg,msglen,answer,anslen,0,0);
/*                res=ns_send(msg,msglen,answer,
			anslen,_andns_ns_+i,sizeof(struct sockaddr_in));*/
                if(res != -1)
                        return res;
        }

        err_ret(ERR_RSLFDQ,-1);
}
/*
int ns_send(char *msg,int msglen, char *answer,int *anslen,struct sockaddr_in *ns,socklen_t nslen)
{
        int s;
        ssize_t len;

        if ((s=new_dgram_socket(AF_INET))==-1) {
                error("In ns_send: can not create socket.");
                return -1;
        }
        if ((connect(s,(struct sockaddr*)ns,nslen))) {
                error("In ns_send: error connecting "
			"socket -> %s.",strerror(errno));
                goto close_return;
        }
	len=set_nonblock_sk(s);
	if (len==-1) {
                error("In ns_send: error setting nonblock socket.");
                goto close_return;
        }

        len=inet_send(s,msg,msglen,0);
        if (len==-1) {
                error("In ns_send. Pkt not forwarded.");
                goto close_return;
        }
	if (len!=msglen) {
		error("In ns_send: have to send "
			"%d bytes, sended %d.",msglen,len);
                goto close_return;
        }

        len=inet_recv_timeout(s,(void*)answer,DNS_MAX_SZ,0, DNS_REPLY_TIMEOUT);
        if (len==-1) {
                error("In ns_send. Pkt not received.");
                goto close_return;
        }
        *anslen=len;
        close(s);
        return 0;
close_return:
        close(s);
        err_ret(ERR_RSLFDQ,-1);
}*/

			/* UTILS FUNCTIONS */

void dpktacpy(dns_pkt *dst,dns_pkt *src,const char *prefix)
{
        dns_pkt_a *dpas,*dpad;
        int slen;
        int yet_pref=0;
        char temp[257];

        dpas=src->pkt_answ;
        while(dpas) {
                dpad=DP_ADD_ANSWER(dst);
                memcpy(dpad,dpas,sizeof(dns_pkt_a));
                dpad->next=NULL;
                if (prefix && !yet_pref) { /* TODO: yet_pref better */
                        slen=strlen(dpad->name);
                        if (dpas->type!=T_PTR)
                                memcpy(dpad->name+slen,prefix,REALM_PREFIX_LEN);
                        else {
                                strcpy(temp,dpad->name);
                                memcpy(dpad->name,prefix+1,REALM_PREFIX_LEN-1);
                                dpad->name[REALM_PREFIX_LEN-1]='.';
                                strcpy(dpad->name+REALM_PREFIX_LEN,temp);
                        }
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
                dpas=dpas->next;
        }
        dpas=src->pkt_add;
        while(dpas) {
                dpad=DP_ADD_ADD(dst);
                memcpy(dpad,dpas,sizeof(dns_pkt_a));
                dpad->next=NULL;
                dpas=dpas->next;
        }
}
dns_pkt* dpktcpy(dns_pkt *src,const char *prefix)
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
        dpktacpy(dst,src,prefix);
        return dst;
}

/*
 * Remove the suffix realm, if any.
 * Writes the result on dst.
 */
char* rm_realm_prefix(char *from,char *dst,int type)
{
        int slen;
        slen=strlen(from);

        if (slen<5)
                strcpy(dst,from);
        else if (type==T_PTR) {
                if (strcasestr(from,PTR_INET_REALM_PREFIX)==from ||
                    strcasestr(from,PTR_NTK_REALM_PREFIX)==from)
                        strcpy(dst,from+REALM_PREFIX_LEN);
                else
                        strcpy(dst,from);

        } else if (strcasestr(from+slen-REALM_PREFIX_LEN,INET_REALM_PREFIX) ||
                 strcasestr(from+slen-REALM_PREFIX_LEN,NTK_REALM_PREFIX)) {
                        strncpy(dst,from,slen-REALM_PREFIX_LEN);
			dst[slen-REALM_PREFIX_LEN]=0;
	} else
                strcpy(dst,from);
        return dst;
}

dns_pkt* dpktcpy_rm_pref(dns_pkt *src)
{
	dns_pkt *dst;
	dns_pkt_qst *dpq;
	char temp[DNS_MAX_HNAME_LEN];
	dst=dpktcpy(src,NULL);
	dpq=dst->pkt_qst;
	rm_realm_prefix(dpq->qname,temp,dpq->qtype);
	strcpy(dpq->qname,temp);
	return dst;
}
int andns_realm(dns_pkt_qst *dpq,int *prefixed)
{
        int slen;
        char *qst;

        qst=dpq->qname;

        if (!qst)
                err_ret(ERR_UFOERR,-1);
        slen=strlen(qst);

        /* if qst is tto short, it's impossible to
           consider a prefix. */
        if (slen<5) return _default_realm_;

        if (dpq->qtype==T_PTR) {
                if (strcasestr(qst,PTR_INET_REALM_PREFIX)==qst) {
                        if (prefixed) *prefixed=1;
                        return INET_REALM;
                }
                if (strcasestr(qst,PTR_NTK_REALM_PREFIX)==qst) {
                        if (prefixed) *prefixed=1;
                        return NTK_REALM;
                }
                if (prefixed) *prefixed=0;
                return _default_realm_;
        }
        if (strcasestr(qst+slen-REALM_PREFIX_LEN,INET_REALM_PREFIX)) {
                if (prefixed) *prefixed=1;
                return INET_REALM;
        }
        if (strcasestr(qst+slen-REALM_PREFIX_LEN,NTK_REALM_PREFIX)) {
                if (prefixed) *prefixed=1;
                return NTK_REALM;
        }
        if (prefixed) *prefixed=0;
        return _default_realm_;
}
/*
 * Returns:
 *      0 if the question does not have a suffix
 *      1 if the question has suffix
 */
int is_prefixed(dns_pkt *dp)
{
        int prefix=0;

        andns_realm(dp->pkt_qst,&prefix);
        return prefix;
}

/*
 * A very stupid function that converts 
 * ANDNS code to DNS code.
 */
int qtype_a_to_d(int qt) 
{
	switch (qt) {
		case AT_A:
			return T_A;
		case AT_PTR:
			return T_PTR;
		/*case AT_MX:
			return T_MX;*/
		default:
			return -1;
	}
}
int apqsttodpqst(andns_pkt *ap,dns_pkt **dpsrc)
{
	dns_pkt *dp;
	dns_pkt_hdr *dph;
	dns_pkt_qst *dpq;
	int res,qt;
	int qlen,family;

	qt=qtype_a_to_d(ap->qtype);
	if (qt==-1)
		err_ret(ERR_ANDNCQ,-1);

	*dpsrc=create_dns_pkt();
	dp=*dpsrc;
	dph=&(dp->pkt_hdr);
	dpq=dns_add_qst(dp);

	if (qt==T_A) {
		qlen=strlen(ap->qstdata);
		if (qlen>DNS_MAX_HNAME_LEN) 
			goto incomp_err;
		strcpy(dpq->qname,ap->qstdata);
	}
	else if (qt==T_PTR) {
		qlen=ap->qstlength;
		if (strstr(ap->qstdata,":")) 
			family=AF_INET6;
		else 
			family=AF_INET;
		/*crow=inet_ntop(family,(const void*)ap->qstdata,
					temp,DNS_MAX_HNAME_LEN);
		if (!crow) 
			goto incomp_err; */
		res=swapped_straddr_pref(ap->qstdata,dpq->qname,family);
		if (res==-1) {
			debug(DBG_INSANE,err_str);
			goto incomp_err;
		}
	}
	else if (qt==T_MX) {
		destroy_dns_pkt(dp);
		return -1;
	}
	else {/* if (qt==AT_MXPTR) {*/
		destroy_dns_pkt(dp);
		return -1;
	}
	dph->id=ap->id;
	dph->rd=1;
	dph->qdcount++;
	dpq->qtype=qt;
	dpq->qclass=C_IN;
	return 0;
incomp_err:
	destroy_dns_pkt(dp);
	err_ret(ERR_ANDNCQ,-1);
}
int dpanswtoapansw(dns_pkt *dp,andns_pkt *ap)
{
	int i,rcode,qt,ancount;
	dns_pkt_a *dpa;
	andns_pkt_data *apd;

	ancount=DNS_GET_ANCOUNT(dp);
	rcode=DNS_GET_RCODE(dp);
	ap->rcode=rcode;
	ap->qr=1;
	
	if (rcode!=DNS_RCODE_NOERR) 
		return 0;
	qt=dp->pkt_qst->qtype;
	dpa=dp->pkt_answ;
	for (i=0;i<ancount;i++) {
		if (!dpa) 
			break;
		apd=andns_add_answ(ap);
		if (qt==T_A) {
			memcpy(apd->rdata,dpa->rdata,4);
			apd->rdlength=4;
		} 
		else if (qt==T_PTR) {
			strcpy(apd->rdata,dpa->rdata);
			apd->rdlength=strlen(apd->rdata);
		}
		else if (qt==T_MX) {
			strcpy(apd->rdata,"Ntkrules");
			apd->rdlength=8;
		}
		else {
			strcpy(apd->rdata,"Ntkrules");
			apd->rdlength=8;
		}
		dpa=dpa->next;
	}
	if (i!=ancount)
		debug(DBG_INSANE,"In dpanswtoapansw: "
				  "ancount=%d, answers=%d",\
				DNS_GET_ANCOUNT(dp),i);
	ap->ancount=i;
	return 0;
}

		/* FINALLY RESOLVING FUNCTIONS */

/*
 * His goal is trivial.
 * DO NOT USE suffixes query, i.e. query with ".INT" or ".NTK".
 * NXDOMAIN otherwise.
 *
 * Returns:
 *      -1 on error
 *      0 if OK
 */
int andns_gethostbyname(char *hname, inet_prefix *ip)
{
        dns_pkt *dp;
        dns_pkt_hdr *dph;
        dns_pkt_qst *dpq;
        int res,anslen;
        char msg[DNS_MAX_SZ],answ[DNS_MAX_SZ];
        uint32_t addr;

        dp=create_dns_pkt();
        dph=&(dp->pkt_hdr);

        dph->id=(rand() >> 16) ^ (rand() >> 16);
        dph->rd=1;

        dpq=dns_add_qst(dp);
	rm_realm_prefix(hname,dpq->qname,T_A);
        dpq->qtype=T_A;
        dpq->qclass=C_IN;

        DP_QDCOUNT(dp)++;

        memset(msg,0,DNS_MAX_SZ);
        memset(answ,0,DNS_MAX_SZ);
        if ((res=d_p(dp,msg))==-1) {
                error(err_str);
		err_ret(ERR_RSLRSL,-1);
        }
        if ((res=ns_general_send(msg,res,answ,DNS_MAX_SZ))==-1) {
                error(err_str);
		err_ret(ERR_RSLRSL,-1);
        }
        if ((res=d_u(answ,res,&dp))==-1) {
                error(err_str);
		err_ret(ERR_RSLRSL,-1);
        }
        memcpy(&addr, dp->pkt_answ->rdata, sizeof(uint32_t));
        addr=ntohl(addr);
        if ((res=inet_setip_raw(ip,&addr, AF_INET))==-1) {
                error("In andns_gethostbyname: can not fill inet_prefix.");
		err_ret(ERR_RSLRSL,-1);
        }
        destroy_dns_pkt(dp);
	return 0;
}


/* There is a DNS query, internet realm.
 * I'm going to forward it, but first I have 
 * to control suffix presence.
 * 
 * After this function, `answer` is the answer to be 
 * sent to the client.
 *
 * Returns:
 * 	answer len
 */ 
int dns_forward(dns_pkt *dp,char *msg,int msglen,char* answer)
{
        dns_pkt *dp_forward;
        char fwdbuf[DNS_MAX_SZ];
        int res,len;

        if (!_dns_forwarding_) {
                error("In rslv: dns forwardind is disable.");
                goto safe_failing;
        }
        debug(DBG_INSANE, "Forwarding dns query to inet nameservers...");
        if (!is_prefixed(dp)) {
                if((res=ns_general_send(msg,msglen,answer,ANDNS_MAX_SZ))==-1) {
                        error(err_str);
                        goto safe_failing;
                }
                destroy_dns_pkt(dp);
                return res;
        }
        /* prepare to re-format query without prefix */
	dp_forward=dpktcpy_rm_pref(dp);
        memset(fwdbuf,0,DNS_MAX_SZ);
        if ((res=d_p(dp_forward,fwdbuf))==-1) { /* dp_foward is destroyed */
                error(err_str);
                goto safe_failing;
        }
        if ((res=ns_general_send(fwdbuf,res,answer,ANDNS_MAX_SZ)==-1)) {
                error(err_str);
                goto safe_failing;
        }
        if ((res=d_u(answer,res,&dp_forward))==-1) {
                error(err_str);
                goto safe_failing;
        }
        dpktacpy(dp,dp_forward,INET_REALM_PREFIX);
        destroy_dns_pkt(dp_forward);
	DNS_SET_NSCOUNT(dp,0);
	DNS_SET_ARCOUNT(dp,0);
        if ((res=d_p(dp,answer))==-1) {
                error(err_str);
		goto failing;
        }
        return res;
safe_failing:
	destroy_dns_pkt(dp);
	goto failing;
failing:
	memcpy(answer,msg,msglen);
	ANDNS_SET_RCODE(answer,RCODE_ESRVFAIL);
	ANDNS_SET_QR(answer);
	res=msglen;
        err_ret(ERR_RSLFDQ,res);
}

/* There is a DNS query, netsukuku realm.
 *
 * I'm going to resolve it in ANDNA.
 *
 * After this function, `answer` is the answer to be 
 * sent to the client.
 *
 * Returns:
 * 	answer len
 */

/* snsd */
int inet_rslv(dns_pkt *dp,char *msg,int msglen,char *answer)
{
	inet_prefix addr;
	int res,qt,i,rcode;
	char temp[DNS_MAX_HNAME_LEN];
	dns_pkt_a *dpa;

	qt=dp->pkt_qst->qtype;
	rm_realm_prefix(dp->pkt_qst->qname,temp,qt);
	if (qt==T_A) {
		res=debug_andna_resolve_hname(temp,&addr);
		if (res==-1) {
			rcode=RCODE_ENSDMN;
			goto safe_return_rcode;
		}
		dpa=DP_ADD_ANSWER(dp);
		dpa->type=T_A;
		dpa->cl=C_IN;
		dpa->ttl=DNS_TTL;
		strcpy(dpa->name,dp->pkt_qst->qname);
		dpa->rdlength=addr.len;
       		inet_htonl(addr.data,addr.family);
	        if (addr.family==AF_INET)
        	        memcpy(dpa->rdata,addr.data,4);
        	else
                	memcpy(dpa->rdata,addr.data,16);
	} else if (qt==T_PTR) {
		char tomp[DNS_MAX_HNAME_LEN];
		char **hnames;
		res=swapped_straddr(temp,tomp);
		if (res==-1) {
			rcode=RCODE_EINTRPRT;
			goto safe_return_rcode;
		}
		res=str_to_inet(tomp,&addr);
		if (res==-1) {
			rcode=RCODE_ESRVFAIL;
			goto safe_return_rcode;
		}
		res=debug_andna_reverse_resolve(addr, &hnames);
		if (res==-1) {
			rcode=RCODE_ENSDMN;
			goto safe_return_rcode;
		}
		for (i=0;i<res;i++) {
			dpa=DP_ADD_ANSWER(dp);
			dpa->type=T_PTR;
			dpa->cl=C_IN;
			dpa->ttl=DNS_TTL;
			strcpy(dpa->name,dp->pkt_qst->qname);
			strcpy(dpa->rdata,hnames[i]);
			xfree(hnames[i]);
		}
		xfree(hnames);
	} else if (qt==T_MX) {
		rcode=RCODE_ENIMPL;
		goto safe_return_rcode;
	} else {
		rcode=RCODE_ENIMPL;
		goto safe_return_rcode;
	}
	DNS_SET_QR(dp,1);
	res=d_p(dp,answer);
	if (res==-1) {
		rcode=RCODE_ESRVFAIL;
		goto return_rcode;
	}
	return res;
safe_return_rcode:
	destroy_dns_pkt(dp);
	goto return_rcode;
return_rcode:
	memcpy(answer,msg,msglen);
	ANDNS_SET_RCODE(answer,rcode);
	ANDNS_SET_QR(answer);
	return msglen;
}
	
/* Rewrite this function for snsd */
int nk_rslv(andns_pkt *ap,char *msg,int msglen,char *answer)
{
	int qt,res,i,rcode;
	andns_pkt_data *apd;
	inet_prefix ipres;

	qt=ap->qtype;
	if (qt==AT_A) {
		rm_realm_prefix(ap->qstdata,temp,AT_A);
		res=debug_andna_resolve_hname(ap->qstdata,&ipres);
		if (res==-1) {
			rcode=RCODE_ENSDMN;
			goto safe_return_rcode;
		}
		ap->ancount++;
		apd=andns_add_answ(ap);
        	apd->rdlength=ipres.len;
        	inet_htonl(ipres.data, ipres.family);
        	if (ipres.family==AF_INET)
                	memcpy(apd->rdata,ipres.data,4);
        	else
                	memcpy(apd->rdata,ipres.data,16);
	} 
	else if (qt==AT_PTR) {
		char **hnames;
		res=str_to_inet(ap->qstdata,&ipres);
		if (res==-1) {
			rcode=RCODE_EINTRPRT;
			goto safe_return_rcode;
		}
		res=debug_andna_reverse_resolve(ipres,&hnames);
		if (res==-1) {
			rcode=RCODE_ENSDMN;
			goto safe_return_rcode;
		}
		for (i=0;i<res;i++) {
			apd=andns_add_answ(ap);
			ap->ancount++;
			apd->rdlength=strlen(hnames[i]);
			strcpy(apd->rdata,hnames[i]);
			xfree(hnames[i]);
		}
	}
	else if (qt==AT_MX) {
		rcode=RCODE_ENIMPL;
		goto safe_return_rcode;
	}
	else if (qt==AT_MXPTR) {
		rcode=RCODE_ENIMPL;
		goto safe_return_rcode;
	}
	else {
		rcode=RCODE_EINTRPRT;
		goto safe_return_rcode;
	}
	ap->qr=1;
	ap->rcode=RCODE_NOERR;
	res=a_p(ap,answer);
	if (res==-1) {
		rcode=RCODE_ESRVFAIL;
		goto return_rcode;
	}
	return res;
safe_return_rcode:
	destroy_andns_pkt(ap);
	goto return_rcode;
return_rcode:
	memcpy(answer,msg,msglen);
	ANDNS_SET_RCODE(answer,rcode);
	ANDNS_SET_QR(answer);
	return msglen;
}
int nk_forward(andns_pkt *ap,char *msg,int msglen,char *answer)
{
	int res,rcode;
	dns_pkt *dp;
	char new_answ[DNS_MAX_SZ];

	res=apqsttodpqst(ap,&dp);
	if (res==-1) {
		rcode=RCODE_EINTRPRT;
		goto safe_return_rcode;
	}
	res=d_p(dp,answer);
	if (res==-1) {
		rcode=RCODE_EINTRPRT;
		goto safe_return_rcode;
	}
	res=ns_general_send(answer,res,new_answ,ANDNS_MAX_SZ);
	if (res==-1) {
		rcode=RCODE_ESRVFAIL;
		goto safe_return_rcode;
	}
	res=d_u(new_answ,res,&dp);
	if (res==-1) {
		rcode=RCODE_ESRVFAIL;
		goto safe_return_rcode;
	}
	res=dpanswtoapansw(dp,ap);
	if (res==-1) {
		rcode=RCODE_ESRVFAIL;
		destroy_dns_pkt(dp);
		goto safe_return_rcode;
	}
	destroy_dns_pkt(dp);
	res=a_p(ap,answer);
	if (res==-1) {
		rcode=RCODE_EINTRPRT;
		goto safe_return_rcode;
	}
	return res;
safe_return_rcode:
	debug(DBG_INSANE,err_str);
	destroy_andns_pkt(ap);
	memcpy(answer,msg,msglen);
	ANDNS_SET_QR(answer);
	ANDNS_SET_RCODE(answer,rcode);
	return msglen;
}
/*
 * This is the main function for the resolution: the dns_wrapper receive the
 * buffer and rslv builds the answer.
 * `answer' is the buffer where the answer will be stored, it must be at
 * least of `ANDNS_MAX_SZ' bytes.
 *
 * Returns:
 *      NULL if the pkt has to be discarded.
 *      A ptr to the answer to be sended if OK:
 *              in this case, answ_len is filled with
 *              the answer len.
 */
char *andns_rslv(char *msg, int msglen,char *answer, int *answ_len)
{
	int proto,res,r;
	dns_pkt *dp;
	andns_pkt *ap;

	proto=GET_NK_BIT(msg);
        if (proto==NK_DNS) 
		res=d_u(msg,msglen,&dp);
	else if (proto==NK_INET || proto ==NK_NTK)
		res=a_u(msg,msglen,&ap);
	else {
		debug(DBG_INSANE,"andns_rslv(): "
				 "Which language are you speaking?");
		return NULL;
	}
	if (res==0) 
		goto discard;
        memset(answer, 0, ANDNS_MAX_SZ);
	if (res==-1) 
		goto intrprt;
        if (proto==NK_DNS) {
		r=andns_realm(dp->pkt_qst,NULL);
		if (r==INET_REALM)
			res=dns_forward(dp,msg,msglen,answer);
		else
			res=inet_rslv(dp,msg,msglen,answer);
	} 
	else if (proto==NK_NTK)
		res=nk_rslv(ap,msg,msglen,answer);
	else if (proto==NK_INET)
		res=nk_forward(ap,msg,msglen,answer);
	*answ_len=res;
	return answer;
discard:
	debug(DBG_INSANE,err_str);
	err_ret(ERR_RSLAQD,NULL);
intrprt:
	debug(DBG_INSANE,err_str);
	memcpy(answer,msg,msglen);
	ANDNS_SET_RCODE(answer,1);
	//ANDNS_SET_RCODE(answer,RCODE_EINTRPRT);
	ANDNS_SET_QR(answer);
	*answ_len=msglen;
	return answer;
}




