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
#ifndef ANDNS_H
#define ANDNS_H

#include <sys/types.h>
#include <stdint.h>


// PREFIX TO QUERY THE INET REALM
#define INET_REALM_PREFIX 	".INT"
#define NTK_REALM_PREFIX 	".NTK"
#define PTR_INET_REALM_PREFIX 	"INT."
#define PTR_NTK_REALM_PREFIX 	"NTK."
#define REALM_PREFIX_LEN 	4

	/*
	 * STRUCTURES
 	*/


/* 
 * DNS STRUCTS
 */
#define MAX_HNAME_LEN 		255
#define MAX_DNS_HNAME_LEN 	255
#define MAX_DNS_LL		2
#define DNS_HDR_SZ		12
#define DNS_MAX_SZ		512
typedef struct dns_pkt_hdr {
	uint16_t       id;
        uint8_t        qr;
        uint8_t        opcode;
        uint8_t        aa;
        uint8_t        tc;
        uint8_t        rd;
        uint8_t        ra;
        uint8_t        z;
        uint8_t        rcode;
        uint8_t        qdcount;
        uint8_t        ancount;
        uint8_t        nscount;
        uint8_t        arcount;
} dns_pkt_hdr;
#define DNS_PKT_HDR_SZ sizeof(dns_pkt_hdr)

//DNS_HDR MACROS
#define DP_QDCOUNT(dp)  ((dp)->pkt_hdr).qdcount
#define DP_ANCOUNT(dp)  ((dp)->pkt_hdr).ancount
#define DP_NSCOUNT(dp)  ((dp)->pkt_hdr).nscount
#define DP_ARCOUNT(dp)  ((dp)->pkt_hdr).arcount

#define DP_ADD_ANSWER(dp)	dns_add_a(&((dp)->pkt_answ));DP_ANCOUNT(dp)+=1;
#define DP_ADD_AUTH(dp)		dns_add_a(&((dp)->pkt_auth));DP_NSCOUNT(dp)+=1;
#define DP_ADD_ADD(dp)		dns_add_a(&((dp)->pkt_add));DP_ARCOUNT(dp)+=1;

#define LBL_PTR_MK              0xC0 // Network byte order
#define LBL_PTR_OFF_MK          0x3fff // N.b. order
#define LBL_PTR(c)      ((c)&LBL_PTR_MK) // AND whith 0xC000
#define MAX_RECURSION_PTR	20

#define MAX_SQLBL_LEN		63

#define DANSWFAIL(c,offset)    *((c)+3)=(((*((c)+3))&0xf0)|0x02);*((c)+offset)='\0'
#define DANSW(c,rcode)		*((c)+3)=(((*((c)+3))&0xf0)|rcode)

struct dns_pkt_qst {
	char            	qname[MAX_HNAME_LEN+REALM_PREFIX_LEN];
	char            	qname_nopref[MAX_HNAME_LEN];
	uint16_t       		qtype;
	uint16_t       		qclass;
	struct dns_pkt_qst 	*next;
};
typedef struct dns_pkt_qst dns_pkt_qst;
#define DNS_PKT_QST_SZ sizeof(dns_pkt_qst)

struct dns_pkt_a
{
        char            	name[MAX_HNAME_LEN];
        uint16_t       		type;
        uint16_t       		class;
        uint32_t       		ttl;
        uint16_t      		rdlength;
        char            	rdata[MAX_HNAME_LEN];
	struct dns_pkt_a	*next;
};
typedef struct dns_pkt_a dns_pkt_a;
#define DNS_PKT_A_SZ sizeof(dns_pkt_a)
#define DNS_TTL	86400;

typedef struct dns_pkt
{
        dns_pkt_hdr     pkt_hdr;
        dns_pkt_qst     *pkt_qst;
        dns_pkt_a       *pkt_answ;
        dns_pkt_a       *pkt_auth;
        dns_pkt_a       *pkt_add;
} dns_pkt;
#define DNS_PKT_SZ sizeof(dns_pkt)

/* 
 * ANDNS STRUCTS
 */
#define MAX_ANDNS_QST_LEN 255
#define MAX_ANDNS_ANSW_LEN 255
#define ANDNS_HDR_SZ	4
#define ANDNS_MAX_SZ	1024
struct andns_pkt_data
{
        uint16_t	       	rdlength;
        char            	rdata[MAX_ANDNS_ANSW_LEN];
	struct andns_pkt_data	*next;
};
typedef struct andns_pkt_data andns_pkt_data;
#define ANDNS_PKT_DATA_SZ sizeof(andns_pkt_data)

typedef struct andns_pkt
{
        uint16_t       	id;
        uint8_t        	qr;
        uint8_t        	qtype;
        uint8_t       	ancount;
        uint8_t        	nk;
        uint8_t        	rcode;
	uint16_t       	qstlength;
	char	       	qstdata[MAX_ANDNS_QST_LEN+REALM_PREFIX_LEN];	
	char	       	qstdata_nopref[MAX_ANDNS_QST_LEN];	
        andns_pkt_data  *pkt_answ;
} andns_pkt;
#define ANDNS_PKT_SZ sizeof(andns_pkt)
#define AP_ANCOUNT(ap)	((ap)->ancount)

#define AANSWFAIL(msg)    	*((msg)+3)=(((*((msg)+3))&0xf0)|0x02);*((msg)+offset)='\0'
#define AANSW(c,rcode)		*((c)+3)=(((*((c)+3))&0xf0)|rcode)



/*
 * INET 
 */

// DNS QUERY-TYPE: others type will be discarded
#define T_AAAA  28      // h->ip IPV6 
#define T_A     1	// h->ip IPV4
#define T_PTR   12	// ip->h
#define T_MX    15	// h->mx
// ANDNS QUERY-TYPE
#define AT_A    	0 // h->ip
#define AT_PTR  	1 // ip->h
#define AT_MX   	2 // h->mx
#define AT_MXPTR        3 // ip->mx

// CLASSES
#define C_IN    1 // internet class, others are discarded

// RCODES: The rcodes are portable between ANDNS and DNS
#define RCODE_NOERR     0 	// No error
#define RCODE_EINTRPRT  1	// Intepret error
#define RCODE_ESRVFAIL  2	// Server failure
#define RCODE_ENSDMN    3	// No such domain
#define RCODE_ENIMPL    4	// Not implemented
#define RCODE_ERFSD     5	// Refused

// PREFIXES FOR PTR QUERY
#define DNS_INV_PREFIX          ".IN-ADDR.ARPA"
#define DNS_INV_PREFIX6         ".IP6.ARPA"
#define OLD_DNS_INV_PREFIX6     ".IP6.INT" // For backward compatibility

// REALMS TO SEARCH
#define NTK_REALM 		0
#define INET_REALM		1
#define DEFAULT_REALM	

// NK BIT
#define NK_OLDSTYLE 		0
#define NK_NTK			1
#define NK_INET			2

// PROTOCOLS
#define ANDNS_NTK_PROTO		0
#define ANDNS_DNS_PROTO		1




#endif //ANDNS_H
