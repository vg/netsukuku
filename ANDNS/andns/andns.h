/*
 * (c) Copyright 2006, 2007 Federico Tomassini aka efphe <effetom@gmail.com>
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


#ifndef ANDNS_H
#define ANDNS_H

#include <zlib.h>
#include <stdint.h>
#include <arpa/inet.h>

#define ANDNS_SHARED_VERSION "0.0.1"

/* Compression */
#define ANDNS_COMPR_THRESHOLD   1000
#define ANDNS_COMPR_LEVEL   Z_BEST_COMPRESSION

/* Misc */
#define ANDNS_MAX_ANSWERS   255
#define ANDNS_TIMEOUT       15
#define ANDNS_STRERROR      128

/* Hostnames */
#define ANDNS_MAX_INET_HNAME_LEN    255
#define ANDNS_MAX_NTK_HNAME_LEN     512
#define ANDNS_HASH_HNAME_LEN        16

/* Realms */
#define ANDNS_NTK_REALM     1
#define ANDNS_INET_REALM    2

/* Query Type */
#define AT_A    0 /* h->ip */
#define AT_PTR  1 /* ip->h */
#define AT_G    2 /* global */

/* Andns Proto */
#define ANDNS_PROTO_TCP     0
#define ANDNS_PROTO_UDP     1

/* Ipv */
#define ANDNS_IPV4      0
#define ANDNS_IPV6      1

/* Answers info */
#define ANDNS_APD_MAIN_IP 1<<0
#define ANDNS_APD_IP      1<<1
#define ANDNS_APD_TCP     1<<2
#define ANDNS_APD_UDP     1<<3

/* Macros */
#define ANDNS_SET_RCODE(s,c)    *((s)+3)=(((*((s)+3))&0xf0)|c)
#define ANDNS_SET_QR(s)         (*((s)+2))|=0x80
#define ANDNS_SET_ANCOUNT(s,n)  *(s+2)|=((n)>>1);*(s+3)|=((n)<<7);
#define ANDNS_SET_Z(s)          *(s+3)|=0x20;
#define ANDNS_UNSET_Z(s)        *(s+3)&=0xdf;
#define APD_ALIGN(apd)          (apd)->rdata=(char*)malloc((apd)->rdlength+1); \
                                memset((apd)->rdata,0,(apd)->rdlength+1)
#define AP_ALIGN(ap)            (ap)->qstdata=(char*)malloc((ap)->qstlength)

/* Pkts */
#define ANDNS_PKT_HDR_SZ    4
#define ANDNS_PKT_HDRZ_SZ   4
#define ANDNS_PKT_QST_SZ    ANDNS_MAX_NTK_HNAME_LEN + 4 
#define ANDNS_PKT_QUERY_SZ  ANDNS_PKT_HDR_SZ + ANDNS_PKT_QST_SZ
#define ANDNS_PKT_ANSW_SZ   20
#define ANDNS_PKT_ANSWS_SZ  ANDNS_PKT_ANSW_SZ * ANDNS_MAX_ANSWERS
#define ANDNS_PKT_TOT_SZ    ANDNS_PKT_QUERY_SZ + ANDNS_PKT_ANSWS_SZ

/* RCODES: The rcodes are portable between ANDNS and DNS */
#define ANDNS_RCODE_NOERR     0       /* No error */
#define ANDNS_RCODE_EINTRPRT  1       /* Intepret error */
#define ANDNS_RCODE_ESRVFAIL  2       /* Server failure */
#define ANDNS_RCODE_ENSDMN    3       /* No such domain */
#define ANDNS_RCODE_ENIMPL    4       /* Not implemented */
#define ANDNS_RCODE_ERFSD     5       /* Refused */

/* Structs size */
#define ANDNS_PKT_DATA_SZ   sizeof(andns_pkt_data)
#define ANDNS_PKT_SZ        sizeof(andns_pkt)

/*
 * Structs
 */

typedef struct andns_query
{
    char        question[ANDNS_MAX_NTK_HNAME_LEN];
    int         id;
    int     	hashed;
    int     	recursion;
    int     	type;
    int     	realm;
    int     	proto;
    int    	    service;
    char        errors[ANDNS_STRERROR];
    char        andns_server[INET6_ADDRSTRLEN];
    int         port;
} andns_query;


/*
 * Structures
 */
struct andns_pkt_data
{
    uint8_t                 m;          /* main ip      */
    uint8_t                 wg;         /* snsd weight  */
    uint8_t                 prio;       /* snsd prio    */
    uint16_t                rdlength;   /* answer len   */
    uint16_t                service;    /* snsd service */
    char                    *rdata;     /* answer       */
    struct andns_pkt_data   *next;      /* next answer  */
};
typedef struct andns_pkt_data andns_pkt_data;

typedef struct andns_pkt
{
    uint16_t        id;         /* id                   */
    uint8_t         r;          /* recursion            */
    uint8_t         qr;         /* question or answer?  */
    uint8_t         z;          /* compression          */
    uint8_t         qtype;      /* query type           */
    uint16_t        ancount;    /* answers number       */
    uint8_t         ipv;        /* ipv4 ipv6            */
    uint8_t         nk;         /* ntk Bit              */
    uint8_t         rcode;      /* response code        */
    uint8_t         p;          /* snsd protocol        */
    uint16_t        service;    /* snsd service         */
    uint16_t        qstlength;  /* question lenght      */
    char            *qstdata;   /* question             */
    andns_pkt_data  *pkt_answ;  /* answres              */
} andns_pkt;

/*
 * Functions
 */

andns_pkt *ntk_query(andns_query *query);
void free_andns_pkt(andns_pkt *ap);

#endif /* ANDNS_LIB_H */
