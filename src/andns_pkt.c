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


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Official includes*/
#include "andns.h"
#include "andns_mem.h"
#include "andns_pkt.h"
#include "xmalloc.h"
#include "log.h"

#include <string.h>

#include <resolv.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h> 

#define LOOPBACK(x)     (((x) & htonl(0xff000000)) == htonl(0x7f000000)) 

/* Debug includes: DO NOT REMOVE 
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "andns.h"
#include "andns_mem.h"
#include "andns_pkt.h"
#include "andna_fake.h"

#include <string.h>

#include <resolv.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define LOOPBACK(x)     (((x) & htonl(0xff000000)) == htonl(0x7f000000))*/


/*
 * Takes a label: is there a ptr?
 * Returns:
 *  	-1  is a malformed label is finded
 *  	 0  if there's no pointer
 *  	<offset from start_pkt> if a pointer is found
 */
size_t getlblptr(char *buf)
{
        uint16_t dlbl;
	char c[2];

	memcpy(c,buf,2);

        if (!LBL_PTR(*c)) /* No ptr */ return 0;
        if (LBL_PTR(*c)!=LBL_PTR_MK) {
                error("In getlblptr: label sequence malformed");
                return -1;
        }
	(*c)&=0x3f;
        memcpy(&dlbl,c,2);
	dlbl=ntohs(dlbl);
        return dlbl; // offset
}

/*
 * Reads a contiguous octet-sequence-label.
 * Writes on dst.
 * read_yet and limit_len are counters: we must not exceede with reading.
 * Returns:
 * 	-1 On error
 * 	Bytes readed if OK
 */
int read_label_octet(const char *src,char *dst,int read_yet,int limit_len)
{
	int how;

	how=*src;
	if ( how > MAX_SQLBL_LEN) {
		error("In read_label_octet: exceeding label.");
		return -1;
	}
	if ( how+read_yet> limit_len ) {
		error("In read_label_octet: exceeding pkt. limit_len=%d,read_yet=%d,count=%d",limit_len,read_yet,how);
		return -1;
	}
	memcpy(dst,src+1,how);
	return how;
}

/*
 * 	It's trivial, but mathematciens do 
 * 	everything complex
 * 	OBSOLETE FUNCTION: SEE LATER
 *
 * The next function is a little complex.
 * It converts a hname from sequence_label format to str.
 * Returns 
 * 	bytes read if OK
 * 	-1 on error
 * 	1 (=len('\0')) on NULL-name
 *
 * -limit_len- is the maximum allowed space for hname.
 * it depends on pkt_len and others. Anyway, it's a limit that
 * i have to know.
 *
 * -buf- is the buffer to be parsed
 *
 * -start_pkt is the begin of pkt; it's useful: label-pointers
 * store offset from this point.
 *
 * -dst- is the destiny ~.~
 *
 * -count- is a counter: it shows how many bytes are readed yet..
 * It's useful bcz this function could recall itself, and it has to
 * remember the number of bytes readed.
 *
 * Tis function could go into recursion: in such case, recursion takes trace
 * about the recursion deep: more recursione than MAX_RECURSION_PTR are forbidded.
 *
 * Anyway,
 *      ***you have to call it with start_pkt=begin_pkt,count=0 and recursion=0***
 */

size_t old_lbltoname(char *buf,char *start_pkt,char *dst,int count,int limit_len,int recursion)
{
        size_t temp,offset;

	if (recursion==0)
	/* controls the pkt size */
	if (count>limit_len) {
		error("In lbltoname: exceeding pkt. LIMIT: %d",limit_len);
		return -1;
	}
	/* maybe we are at the last label octet */	
	if (*buf==0) {
		*dst=0;
		return 1;
	}
	/* maybe ther's a ptr: in such case, recursion is setted and temp
	 * stores offset from start_pkt */
	if ((temp=getlblptr(buf))) {
		if (temp==-1 || recursion>MAX_RECURSION_PTR) {
			error("In lbltoname: malformed pkt");
                        return -1;
		}
		recursion++;
		buf=start_pkt+temp;
	} else {
		if ((temp=read_label_octet(buf,dst,count,limit_len))==-1) 
			return -1;
		count+=temp+1; /* read also "." */
		buf+=temp+1;
		dst+=temp;
		if (*buf) 
			*dst++='.';
		else {
			*dst++=0;
			count++;
		}
	}
	if ((offset=old_lbltoname(buf,start_pkt,dst,count,limit_len,recursion))==-1)
		return -1;
	if (recursion) 
		return (recursion==1)?2:0;
	return offset+temp+1;
}

/*
 * Converts a dns compliant sequence label name to string.
 * Returns:
 * 	Bytes readed if OK
 * 	-1 on error
 */
int lbltoname(char *buf,char *start_pkt,char *dst,int limit)
{
        char *crow;
        int how,recursion=0;
        int ptr;
	int writed=0,readed=0;
	int new_limit=limit;

        crow=buf;

        while (*crow) {
                ptr=getlblptr(crow);
                if (ptr) { /* Got a pointer.... or got a error*/
			if (ptr==-1) {
                        	error("In lbltoname: malformed label!");
                        	return -1;
                	}
                        if (++recursion>MAX_RECURSION_PTR) {
                                error("In lbltoname: too many pointers.");
                                return -1;
                        }
			if (recursion==1) readed+=2; /* we read the pointer */
                        crow=start_pkt+ptr;
			new_limit=limit - (int)(crow - buf);
			if (new_limit<=0 || new_limit > (int)(buf-start_pkt)+limit) {
				error("In lbltoname: pointer to deep space!");
				return -1;
			}
                	if (getlblptr(crow)) {
                        	error("In lbltoname: pointer to pointer.");
                        	return -1;
                	}
                }

                how=*crow++;
                if (how>MAX_SQLBL_LEN) {
                        error("In lbltoname: exceeding label!");
                        return -1;
                }
		if (how>new_limit) {
                        error("In lbltoname: exceeding packet!");
                        return -1;
                }
		if (!recursion) 
			readed+=how+1;
		writed+=how+1;

                if (writed>MAX_DNS_HNAME_LEN) {
                        error("In lbltoname: hname too long.");
                        return -1;
                }
                memcpy(dst,crow,how);
                dst+=how;
                crow+=how;
                *dst++=(*crow)?'.':0;
        }
	if (!recursion) readed++;
        return readed;
}

/*
 * Returns the used protocol which packet belongs,
 * understanding it from NK bit in pkt headers.
 */
int andns_proto(char *buf)
{
        char c;
	
	c=*(buf+3);
	c=(c>>4)&0x03;
	return c;
}

/*
 * Returns the realm question.
 * 	INET_REALM if you are seraching something on internet.
 * 	NTK_REALM if you search something on ntk.
 * 	-1 on error
 *
 * If there is no suffix, returns _default_realm_, which is set
 * by andns_init.
 *
 * If prefixed is not NULL, and a prefix is found, *prefixed is
 * set to 1.
 *
 *  Note: in the case of a ptr_query, the suffix-realm has
 *  to be specified if you want a different behavior fron 
 *  the default.
 *  Do you want to know who is 1.2.3.4 in INET_REALM?
 *  ASk for 1.2.3.4.INT
 */
int andns_realm(dns_pkt_qst *dpq,int *prefixed)
{
	int slen;
	char *qst;

	qst=dpq->qname;

	if (!qst) 
	{
		error("In andns_realm: what appens?");
		return -1;	
	}
	slen=strlen(qst);
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
		
	// if qst is tto short, it's impossible to 
	// consider a prefix.
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
 * 	0 if the question does not have a suffix
 * 	1 if the question has suffix
 */
int is_prefixed(dns_pkt *dp)
{
	int prefix=0;

	andns_realm(dp->pkt_qst,&prefix);
	return prefix;
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
		 strcasestr(from+slen-REALM_PREFIX_LEN,NTK_REALM_PREFIX)) 
			strncpy(dst,from,slen-REALM_PREFIX_LEN);
	else
		strcpy(dst,from);
	return dst;
}
			
/*
 * DNS PTR query ask for 4.3.2.1.in-addr.arpa to know
 * who is 1.2.3.4.
 * This function reads this type of query transalting it
 * in the second form.
 * Writes result on *dst.
 * -1 on error.
 */
int swapped_straddr(char *src,char *dst)
{
        int family,count=0;
        int res,i;
        char atoms[4][4],*crow,*temp;

        if (!src) {
                error("In swapped_straddr: NULL argument!");
                return -1;
        }
        // If I cannot find any inverse prefix
        if( ! \
                ( (temp=(char*)strcasestr(src,DNS_INV_PREFIX))  ||\
                  (temp=(char*)strcasestr(src,DNS_INV_PREFIX6)) ||\
                  (temp=(char*)strcasestr(src,OLD_DNS_INV_PREFIX6)))) {
                error("In swapped_straddr: ptr query without suffix");
                return -1;
        }
        // IPV4 or IPV6?
        family=(strstr(temp,"6"))?AF_INET6:AF_INET;

        if (family==AF_INET) {
                while (src!=temp+1) {
                        crow=strstr(src,".");
                        if (!crow) return -1;
                        strncpy(atoms[count],src,crow-src);
                        atoms[count][crow-src]=0;
                        count++;
                        src=crow+1;
                }
                for (i=count-1;i>=0;i--) {
                        /*printf("ATom i %s\n", atoms[i]);*/
                        res=strlen(atoms[i]);
                        /* printf("Len is %d\n",res); */
                        strncpy(dst,atoms[i],res);
                        dst+=res;
                        *dst++=i==0?0:'.';
                }

        } else 
		while ((*temp--=*dst++));
        return 0;
}

/*
 * This function controls the integrity of a dns_pkt
 * in the case of a query ntk-related: in such case,
 * not alls dns-implementations are permitted.
 *
 * Returns 0 if OK. -1 if not.
 * If -1 is returned, the rcode has to be setted to 
 * E_NIMPL
 */

int dnslovesntk(dns_pkt *dp)
{
        dns_pkt_hdr *dph;

        dph=&(dp->pkt_hdr);
        if (dph->qr || dph->aa || dph->tc || dph->z) {
                error("In dnslovesntk: pkt_hdr with QR || AA || TC || Z");
                return -1; // We acept only queries, not answers o pkt trunc
        }                  // Z has to be 0 (see rfc)

        if (dph->opcode>=2) { // No Status server or unuesed values
                error("In dnslovesntk: opcode not supported in ntk realm");
                return -1;
        }
        if (DP_QDCOUNT(dp)!=1) {
                error("In dnslovesntk: i need one and only one query");
                return -1; // Only a query must exist.
        }
        if (dph->arcount || dph->ancount || dph->nscount) {
                error("In dnslovesntk: pkt has AN || NS || AR sections");
                return -1; // No Answ, Auth, Adds
        }
        /* Pkt_hdR controls ended */
	return 0;
}
/*
 * Converts a domain_name_string into a sequence label format,
 * dns compliant. Writes on dst.
 * -1 on error, number of bytes writed on succes
 */
size_t nametolbl(char *name,char *dst)
{
        char *crow;
        size_t offset=0,res;

        if (!name || !strcmp(name,"") || strlen(name)>MAX_DNS_HNAME_LEN) {
                error("In nametolbl: invalid name");
                return -1;
        }
        while ((crow=strstr(name+1,"."))) {
                res=crow-name;
                if (res+offset>MAX_SQLBL_LEN) {
                        error("In nametolbl: sequence label too long");
                        return -1;
                }
                *dst=(char)res; // write the octet length
		dst++;
		offset++;
                memcpy(dst,name,(size_t)res); // write label
                name+=res+1;dst+=res;offset+=res; // shift ptrs
        }
	if (!name) return offset;
	if((res=(char)strlen(name))>MAX_SQLBL_LEN) {
                error("In nametolbl: sequence label too long");
                return -1;
        }
	*dst++=(char)res;
	strcpy(dst,name);
	offset+=res+2;
        return offset;
}
/*
 * Disassembles DNS packet headers, writing a yet allocated
 * dns_pkt_hdr struct.
 * No controls on len, bcz <<--the min_pkt_len is controlled
 * by recv.-->>
 * Returns the number of bytes readed (always DNS_PKT_HDR_SZ).
 */
size_t dpkttohdr(char *buf,dns_pkt_hdr *dph)
{
        uint8_t c;
        uint16_t s;

                // ROW 1
        memcpy(&s,buf,sizeof(uint16_t));
        dph->id=ntohs(s);
                // ROW 2
        buf+=2;
        memcpy(&c,buf,sizeof(uint8_t));
        dph->qr= (c>>7)&0x01;
        dph->opcode=(c>>3)&0x0f;
        dph->aa=(c>>2)&0x01;
        dph->tc=(c>>1)&0x01;
        dph->rd=c&0x01;

        buf++;
        memcpy(&c,buf,sizeof(uint8_t));
	dph->ra=(c>>7)&0x01;
        dph->z=(c>>4)&0x07;
        dph->rcode=c&0x0f;

                // ROW 3
        buf++;
        memcpy(&s,buf,sizeof(uint16_t));
        dph->qdcount=ntohs(s);
                // ROW 4
        buf+=2;
        memcpy(&s,buf,sizeof(uint16_t));
        dph->ancount=ntohs(s);
                // ROW 5
        buf+=2;
        memcpy(&s,buf,sizeof(uint16_t));
        dph->nscount=ntohs(s);
                // ROW 6
        buf+=2;
        memcpy(&s,buf,sizeof(uint16_t));
        dph->arcount=ntohs(s);

        buf+=2;
        return DNS_HDR_SZ; // i.e. 12 :)
}
/*
 * This function alloc a new dns_pkt_qst to store a dns_question_section.
 * The new dns_pkt_qst is also added to the principal dp-struct
 * Returns bytes readed if OK. -1 otherwise.
 */
size_t dpkttoqst(char *start_buf,char *buf,dns_pkt *dp,int limit_len)
{
        size_t count;
        uint16_t s;
	dns_pkt_qst *dpq;

	dpq=dns_add_qst(dp);

        // get name
        if((count=lbltoname(buf,start_buf,dpq->qname,limit_len))==-1)
                return -1;
        buf+=count;
        // Now we have to write 2+2 bytes
        if (count+4>limit_len) {
                debug(DBG_NOISE, "In dpkttoqst: limit_len break!");
                return -1;
        }

        // shift to type and class
        memcpy(&s,buf,2);
        dpq->qtype=ntohs(s);
        count+=2;
        buf+=2;

        memcpy(&s,buf,2);
        dpq->qclass=ntohs(s);
        count+=2;

	rm_realm_prefix(dpq->qname,dpq->qname_nopref,dpq->qtype);
        return count;
}
/*
 * Disassembles a DNS qst_section_set.
 * Use the above function for each question section.
 * -1 on error. Number of bytes readed on success.
 *  If -1 is returned, rcode ha sto be set to E_INTRPRT
 */
size_t dpkttoqsts(char *start_buf,char *buf,dns_pkt *dp,int limit_len)
{
        size_t offset=0,res;
        int i,count;

        if (!(count=DP_QDCOUNT(dp)))
        	return 0; // No questions.

        for(i=0;i<count;i++) {
                if ( (res=dpkttoqst(start_buf,buf+offset,dp,limit_len-offset))==-1)
                        return -1;
                offset+=res;
        }
        return offset;
}

/*
 * The behavior of this function is in all similar to dpkttoqst.
 * Returns -1 on error. Bytes readed otherwise.
 */
size_t dpkttoa(char *start_buf,char *buf,dns_pkt_a **dpa_orig,int limit_len)
{
        size_t count,rdlen;
	dns_pkt_a *dpa;
        uint16_t s;
        uint32_t ui;

	dpa=dns_add_a(dpa_orig);

        // get name
        if((count=lbltoname(buf,start_buf,dpa->name,limit_len))==-1)
                return -1;
        buf+=count;
        // Now we have to write 2+2+4+2 bytes
        if (count+10>limit_len) {
                debug(DBG_NOISE, "In npkttoa: limit_len braek!");
                return -1;
        }

        memcpy(&s,buf,2);
        dpa->type=ntohs(s);
        count+=2;
        buf+=2;

        memcpy(&s,buf,2);
        dpa->class=ntohs(s);
        count+=2;
        buf+=2;

        memcpy(&ui,buf,4);
        dpa->ttl=ntohs(ui);
        count+=4;
        buf+=4;

        memcpy(&s,buf,2);
        dpa->rdlength=ntohs(s);
        count+=2;
        buf+=2;

        rdlen=dpa->rdlength;
        if (rdlen>MAX_DNS_HNAME_LEN) {
                debug(DBG_NOISE, "In dpkttoa: rdlen exceeds!");
                return -1;
        }
        // Now we have to write dpa->rdlength bytes
        if (count+rdlen>limit_len) {
                debug(DBG_NOISE, "In npkttoa: limit_len break!");
                return -1;
        }
	if (dpa->type==T_A)
        	memcpy(dpa->rdata,buf,rdlen);
	else 
		if ((ui=lbltoname(buf,start_buf,dpa->rdata,rdlen))==-1) {
			error("In dpkttpa: can not write rdata field.");
			return -1;
		}
        count+=rdlen;
        return count;
}

/*
 * See dpkttoqsts.
 * -1 on error.  Bytes readed otherwise.
 */
size_t dpkttoas(char *start_buf,char *buf,dns_pkt_a **dpa,int limit_len,int count)
{
        size_t offset=0,res;
        int i;

        if (!count) return 0;
        for(i=0;i<count;i++) {
                if ((res=dpkttoa(start_buf,buf+offset,dpa,limit_len-offset))==-1)
                        return -1;
                offset+=res;
        }
        return offset;
}

/*
 * This is a main function: takes the pkt-buf and translate
 * it in structured data.
 * It cares about dns_pkt allocations.
 *
 * Returns:
 * -1 on E_INTRPRT
 *  0 if pkt must be discarded.
 *  Number of bytes readed otherwise
 */
size_t dpkt(char *buf,size_t pktlen,dns_pkt **dpp)
{
	dns_pkt *dp;
	size_t offset=0,res;
	char *crow;

	crow=buf;
	// Controls pkt consistency: we must at least read pkt headers
	if (pktlen<DNS_HDR_SZ)
		return 0; // pkt MUST be discarded!
	*dpp=dp=create_dns_pkt();

	// Writes headers
	offset+=dpkttohdr(buf,&(dp->pkt_hdr));
	if (pktlen > DNS_MAX_SZ) // If pkt is too long: the headers are written,
				// so we can reply with E_INTRPRT
		return -1;
	crow+=offset;
	// Writes qsts
	if ((res=dpkttoqsts(buf,crow,dp,pktlen-offset))==-1)
		return -1;
	offset+=res;
	crow+=res;
	if ((res=dpkttoas(buf,crow,&(dp->pkt_answ),pktlen-offset,DP_ANCOUNT(dp)))==-1)
		return -1;
	offset+=res;
	/*crow+=res;
	if ((res=dpkttoas(buf,crow,&(dp->pkt_auth),pktlen-offset,DP_NSCOUNT(dp)))==-1)
		return -1;
	offset+=res;
	crow+=res;
	if ((res=dpkttoas(buf,crow,&(dp->pkt_add),pktlen-offset,DP_ARCOUNT(dp)))==-1)
		return -1;*/
	return offset;
}
/*
 * This function is the dptktohdr inverse.
 * Takes a dns_pkt struct and builds the
 * header pkt-buffer
 * Returns the number of bytes writd.
 */
size_t hdrtodpkt(dns_pkt *dp,char *buf)
{
        char *crow=buf;
        uint16_t u;
        dns_pkt_hdr *dph;

        dph=&(dp->pkt_hdr);
        u=htons(dph->id);
        memcpy(buf,&u,2);
        buf+=2;

        if (dph->qr) *buf|=0x80;
        *buf|=dph->opcode<<3;
        *buf|=dph->aa<<2;
        *buf|=dph->tc<<1;
        *buf|=dph->rd;

        buf++;
        *buf|=dph->ra<<7;
        *buf|=dph->z<<4;
        *buf|=dph->rcode;

        buf++;

        u=htons(dph->qdcount);
        memcpy(buf,&u,2);
        buf+=2;
        u=htons(dph->ancount);
        memcpy(buf,&u,2);
        buf+=2;
        u=htons(dph->nscount);
        memcpy(buf,&u,2);
        buf+=2;
        u=htons(dph->arcount);
        memcpy(buf,&u,2);
        buf+=2;
        return (size_t)(buf-crow);
}
/*
 * Translate a struct dns_pkt_qst in the dns-buffer buf.
 * Returns:
 * 	-1 On error
 * 	Bytes writed otherwise.
 *
 * If nopref, the name for the question is pkt-ized without
 * realm suffix.
 */
size_t qsttodpkt(dns_pkt_qst *dpq,char *buf, int limitlen,int nopref)
{
        size_t offset;
        uint16_t u;
	char *temp;

	temp=(nopref)?dpq->qname_nopref:dpq->qname;

        if((offset=nametolbl(temp,buf))==-1) {
		error("In qsttodpkt: error transalting name to sequence labels: name=%s",temp);
                return -1;
	}
        if (offset+4>limitlen) {
                error("In qsttodpkt: limitlen broken");
                return -1;
        }
        buf+=offset;
        u=htons(dpq->qtype);
        memcpy(buf,&u,2);
        buf+=2;offset+=2;
        u=htons(dpq->qclass);
        memcpy(buf,&u,2);
        buf+=2;offset+=2;
        return offset;
}
/*
 * Translates the question sections of a struct dns_pkt
 * into buf.
 * Returns:
 * 	-1 on error.
 *  	Number of bytes writed otherwise,
 *
 * If nopref, names are pkt-ized without prefix
 */
size_t qststodpkt(dns_pkt *dp,char *buf,int limitlen,int nopref)
{
        size_t offset=0,res;
        int i;
	dns_pkt_qst *dpq;
	dpq=dp->pkt_qst;

        for (i=0;dpq && i<DP_QDCOUNT(dp);i++) {
                if ((res=qsttodpkt(dpq,buf+offset,limitlen-offset,nopref))==-1)
                        return -1;
                offset+=res;
		dpq=dpq->next;
        }
        return offset;
}
size_t atodpkt(dns_pkt_a *dpa,char *buf,int limitlen)
{
        size_t offset,rdlen;
        uint16_t u;
        int i;

        if((rdlen=nametolbl(dpa->name,buf))==-1)
                return -1;
	offset=rdlen;
	if (offset+10>limitlen) {
                error("In atodpkt: limitlen broken");
                return -1;
        }
        buf+=offset;
        u=htons(dpa->type);
        memcpy(buf,&u,2);
        buf+=2;offset+=2;
        u=htons(dpa->class);
        memcpy(buf,&u,2);
        buf+=2;offset+=2;
        i=htonl(dpa->ttl);
        memcpy(buf,&i,4);
        buf+=4;offset+=4;

	if (dpa->type==T_A) {
		if (offset+dpa->rdlength>limitlen) {
                	error("In atodpkt: limitlen broken");
	                return -1;
        	}
        	memcpy(buf+2,dpa->rdata,dpa->rdlength);
		offset+=dpa->rdlength;
	} else {
		if ((rdlen=nametolbl(dpa->rdata,buf+2))==-1) {
			error("In atodpkt: can not write rdata field.");
			return -1;
		}
		offset+=rdlen;
		if (offset>limitlen) {
                	error("In atodpkt: limitlen broken");
	                return -1;
        	}
		dpa->rdlength=rdlen;
	}
        u=htons(dpa->rdlength);
        memcpy(buf,&u,2);
        offset+=2;
        return offset;
}
size_t astodpkt(dns_pkt_a *dpa,char *buf,int limitlen,int count)
{
        size_t offset=0,res;
        int i;
        for (i=0;dpa && i<count;i++) {
                if ((res=atodpkt(dpa,buf+offset,limitlen-offset))==-1)
                        return -1;
                offset+=res;
		dpa=dpa->next;
        }
        return offset;
}
/*
 * Transform a dns_pkt structure in char stream.
 *
 * Returns:
 * 	-1 on error
 * 	len(stream) if OK
 *
 * The stream has at least the header section writed.
 * `buf' must be at least of DNS_MAX_SZ bytes.
 *
 * nopref: do you want to pkt with name_realm_prefixed or not?
 * if nopref, name without prefix will be used
 *
 * DANGER: This function realeses *ALWAYS* the dns_pkt *dp!!!!
 * The stream buffer is allocated here.
 */
size_t dpktpack(dns_pkt *dp,char *buf,int nopref)
{
        size_t offset,res;

        memset(buf,0,DNS_MAX_SZ);

        offset=hdrtodpkt(dp,buf);
        buf+=offset;
        if((res=qststodpkt(dp,buf,DNS_MAX_SZ-offset,nopref))==-1)
                goto server_fail;
        offset+=res;
        buf+=res;
        if ( (res=astodpkt(dp->pkt_answ,buf,DNS_MAX_SZ-offset,DP_ANCOUNT(dp)))==-1)
		goto server_fail;
	offset+=res;
	/*buf+=res;
	if ( (res=astodpkt(dp->pkt_auth,buf,DNS_MAX_SZ-offset,DP_NSCOUNT(dp)))==-1) 
		goto server_fail;
	offset+=res;
	buf+=res;*/
        /*if ( (res=astodpkt(dp->pkt_add,buf,DNS_MAX_SZ-offset,DP_ARCOUNT(dp)))==-1)
		goto server_fail;
	offset+=res;*/
        destroy_dns_pkt(dp);
        return offset;
server_fail:
	destroy_dns_pkt(dp);
	return -1;
}
/*
 * Takes the buffer stream and translate headers to
 * andns_pkt struct. 
 * Returns ALWAYS 4. The pkt_len has to be controlled
 * elsewhere.
 */
size_t apkttohdr(char *buf,andns_pkt *ap)
{
        uint8_t c;
        uint16_t s;
        char *start_buf;

        start_buf=buf;

                // ROW 1
        memcpy(&s,buf,sizeof(uint16_t));
        ap->id=ntohs(s);
        buf+=2;

        memcpy(&c,buf,sizeof(uint8_t));
	ap->qr=(c>>7)&0x01;
	ap->qtype=(c>>3)&0x0f;
	ap->ancount=(c<<1)&0x0e;

        buf++;
	if (((*buf)|0x80)) ap->ancount++;

	ap->nk=(c>>4)&0x03;
	ap->rcode=c&0x0f;
        return ANDNS_HDR_SZ;
}
/*
 * Translate the andns_pkt question stream to andns_pkt struct.
 * -1 on error. Bytes readed otherwise.
 *  NOTE: The qst-data size is controlled: apkt won't be need
 *  this control.
 */
size_t apkttoqst(char *buf,andns_pkt *ap)
{
	uint16_t s;
	memcpy(&s,buf,sizeof(uint16_t));
	ap->qstlength=ntohs(s);
	if (ap->qstlength>=MAX_ANDNS_QST_LEN) {
		error("In apkttoqst: size exceeded");
		return -1;
	}
	buf+=2;
	memcpy(ap->qstdata,buf,ap->qstlength);
	rm_realm_prefix(ap->qstdata,ap->qstdata_nopref,ap->qtype);
	return ap->qstlength+2;
}

/*
 * This is a main function: takes the pkt-buf and translate
 * it in structured data.
 * It cares about andns_pkt allocation.
 * The apkt is allocate here.
 *
 * Returns:
 * -1 on E_INTRPRT
 *  0 if pkt must be discarded.
 *  Number of bytes readed otherwise
 */
size_t apkt(char *buf,size_t pktlen,andns_pkt **app)
{
	andns_pkt *ap;
	size_t offset,res;

	if (pktlen<ANDNS_HDR_SZ) {
		error("In apkt: pkt sz is less than pkt headers!");
		return 0;
	}
	*app=ap=create_andns_pkt();
	offset=apkttohdr(buf,ap);
	buf+=offset;
	if ((res=apkttoqst(buf,ap))==-1)
		return -1;
	return offset+res;
}

size_t hdrtoapkt(andns_pkt *ap,char *buf)
{
	uint16_t s;
	s=htons(ap->id);
	memcpy(buf,&s,sizeof(uint16_t));
	buf+=2;
	if (ap->qr) 
		(*buf)|=0x80;
	(*buf)|=( (ap->qtype)<<3);
	(*buf++)|=( (ap->ancount)>>1);
	(*buf)|=( (ap->ancount)<<7);
	(*buf)|=( (ap->nk)<<4);
	(*buf)|=(  ap->rcode);
	return ANDNS_HDR_SZ;
}
size_t qsttoapkt(andns_pkt *ap,char *buf,size_t limitlen)
{
	uint16_t s;

	if (ap->qstlength>MAX_ANDNS_QST_LEN || limitlen < ap->qstlength+2 ) {
		error("In qsttooapkt: size exceeded");
		return -1;
	}
	s=htons(ap->qstlength);
	memcpy(buf,&s,sizeof(uint16_t));
	buf+=2;
	memcpy(buf,ap->qstdata,ap->qstlength);
	return ap->qstlength+2;
}
size_t answtoapkt(andns_pkt_data *apd,char *buf,size_t limitlen)
{
	uint16_t s;
	if (apd->rdlength>MAX_ANDNS_ANSW_LEN || limitlen< apd->rdlength+2) {
		error("In answtoapkt: size exceeded");
		return -1;
	}
	s=htons(apd->rdlength);
	memcpy(buf,&s,sizeof(uint16_t));
	buf+=2;
	memcpy(buf,apd->rdata,apd->rdlength);
	return apd->rdlength+2;
}
size_t answstoapkt(andns_pkt *ap,char *buf, size_t limitlen)
{
	andns_pkt_data *apd;
	int i;
	size_t offset=0,res;

	apd=ap->pkt_answ;
	for (i=0;i<AP_ANCOUNT(ap) && apd;i++) {
		if((res=answtoapkt(apd,buf+offset,limitlen-offset))==-1)
			return -1;
		offset+=res;
		apd=apd->next;
	}
	return offset;
}

/*
 * apktpack: `buf' must be at least of ANDNS_MAX_SZ bytes.
 */
size_t apktpack(andns_pkt *ap, char *buf)
{
	size_t offset,res;

	memset(buf,0,ANDNS_MAX_SZ);

	offset=hdrtoapkt(ap,buf);
	buf+=offset;
	if ((res=qsttoapkt(ap,buf,ANDNS_MAX_SZ-offset))==-1)
		goto server_fail;
	offset+=res;
	if ((res=answstoapkt(ap,buf,ANDNS_MAX_SZ-offset))==-1)
		goto server_fail;
	offset+=res;
	destroy_andns_pkt(ap);

	return offset;

server_fail:
	destroy_andns_pkt(ap);
	return -1;
	//AANSWFAIL(crow,offset);
	//return crow;
}
/*
char* apkttodstream(andns_pkt *ap)
{
	dns_pkt *dp;
	dns_pkt_qst *dpq;
	char *bufstream;

	dp=create_dns_pkt();
	memset(dp,0,DNS_PKT_SZ);

	//writes headers
	(dp->pkt_hdr).id=1;
	(dp->pkt_hdr).rd=1;
	(dp->pkt_hdr).qdcount=1;

	dpq=dns_add_qst(dp);
	dpq->qtype=ap->qtype;
	dpq->qclass=C_IN;
	strncpy(dpq->qname,ap->qstdata,MAX_HNAME_LEN);
	dpktpack(dp,bufstream);
	return bufstream;
}*/
	
/*
 * In the case of a query whith ntk-protocol, but inet-related,
 * the apkt must be translated o a dns_pkt and the dns_pkt has to be 
 * forwarded to some nameserver.
 * This function translate the answer fron ntk style to dns style.
 * Returns: -1 on error, 0 if OK
 */
int danswtoaansw(dns_pkt *dp,andns_pkt *ap,char *msg)
{
	int acount,i,type,res;
	andns_pkt_data *apd;
	dns_pkt_a *dpa;

	acount=DP_ANCOUNT(dp);

	dpa=dp->pkt_answ;
	for(i=0;i<acount;i++) {
		apd=andns_add_answ(ap);
		if (!dpa) {
			error("In danswtoaansw: ancount is %d, but answers are not.",acount); 
			return -1;
		}
		type=ap->qtype;
		switch(type) {
			case AT_A:
				apd->rdlength=4;
				memcpy(apd->rdata,dpa->rdata,4);
				break;
			case AT_PTR:
				if ((res=lbltoname(dpa->rdata,msg,apd->rdata,dpa->rdlength))==-1)
					return -1;
				apd->rdlength=strlen(apd->rdata);
				break;
			case AT_MX:
				if ((res=lbltoname(dpa->rdata,msg,apd->rdata,dpa->rdlength))==-1)
					return -1;
				apd->rdlength=strlen(apd->rdata);
				break;
			default:
				error("In danswtoaansw: qtype error");
				break;
		}
		dpa=dpa->next;
	}
	return 0;
}

/*
 * A very stupid function for debugging
 */

void dp_print(dns_pkt *dp)
{
        dns_pkt_hdr *dph;
        dns_pkt_a *dpa;
        dns_pkt_qst *dpq;

        dph=&(dp->pkt_hdr);
        debug(DBG_NOISE, " ID %d",dph->id);
        debug(DBG_NOISE, " QR %d",dph->qr);
        debug(DBG_NOISE, " opcode %d",dph->opcode);
        debug(DBG_NOISE, " aa %d",dph->aa);
        debug(DBG_NOISE, " tc %d",dph->tc);
        debug(DBG_NOISE, " rd %d",dph->rd);
        debug(DBG_NOISE, " ra %d",dph->ra);
        debug(DBG_NOISE, " z %d",dph->z);
        debug(DBG_NOISE, " rcode %d",dph->rcode);
        debug(DBG_NOISE, " qdcount %d",dph->qdcount);
        debug(DBG_NOISE, " ancount %d",dph->ancount);
        debug(DBG_NOISE, " nscount %d",dph->nscount);
        debug(DBG_NOISE, " arcount %d",dph->nscount);

        dpq=dp->pkt_qst;

        debug(DBG_NOISE, "QUESTION");
        debug(DBG_NOISE, "\tQNAME=%s",dpq->qname);
        debug(DBG_NOISE, "\tQNAME_nopref=%s",dpq->qname_nopref);
        debug(DBG_NOISE, "\tqtype=%d",dpq->qtype);
        debug(DBG_NOISE, "\tqclass=%d",dpq->qclass);
        dpa=dp->pkt_answ;
        debug(DBG_NOISE, "ANSWERS");
        if (!dpa) debug(DBG_NOISE, "Any!");
        while (dpa) {
                debug(DBG_NOISE, "\tname %s", dpa->name);
                //debug(DBG_NOISE, "\tname_nopref %s", dpa->name_nopref);
                debug(DBG_NOISE, "\ttype %d", dpa->type);
                debug(DBG_NOISE, "\tclass %d", dpa->class);
                debug(DBG_NOISE, "\tttl %d", dpa->ttl);
                debug(DBG_NOISE, "\trdlength %d", dpa->rdlength);
                debug(DBG_NOISE, "\trdata %s", dpa->rdata);
                dpa=dpa->next;
        }
        debug(DBG_NOISE, "AUTHS");
        dpa=dp->pkt_auth;
        if (!dpa) debug(DBG_NOISE, "Any!");
        while (dpa) {
                debug(DBG_NOISE, "\tname %s", dpa->name);
                //debug(DBG_NOISE, "\tname_nopref %s", dpa->name_nopref);
                debug(DBG_NOISE, "\ttype %d", dpa->type);
                debug(DBG_NOISE, "\tclass %d", dpa->class);
                debug(DBG_NOISE, "\tttl %d", dpa->ttl);
                debug(DBG_NOISE, "\trdlength %d", dpa->rdlength);
                debug(DBG_NOISE, "\trdata %s", dpa->rdata);
                dpa=dpa->next;
        }
        debug(DBG_NOISE, "ADD");
        dpa=dp->pkt_add;
        if (!dpa) debug(DBG_NOISE, "Any!");
        while (dpa) {
                debug(DBG_NOISE, "\tname %s", dpa->name);
                //debug(DBG_NOISE, "\tname_nopref %s", dpa->name_nopref);
                debug(DBG_NOISE, "\ttype %d", dpa->type);
                debug(DBG_NOISE, "\tclass %d", dpa->class);
                debug(DBG_NOISE, "\tttl %d", dpa->ttl);
                debug(DBG_NOISE, "\trdlength %d", dpa->rdlength);
                debug(DBG_NOISE, "\trdata %s", dpa->rdata);
                dpa=dpa->next;
        }
}
