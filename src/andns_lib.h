                 /**************************************
                *     AUTHOR: Federico Tomassini        *
               *     Copyright (C) Federico Tomassini    *
              *     Contact effetom@gmail.com             *
             ***********************************************
             *******          BEGIN 3/2006          ********
*************************************************************************
*                                                                       *
*  This program is free software; you can redistribute it and/or modify *
*  it under the terms of the GNU General Public License as published by *
*  the Free Software Foundation; either version 2 of the License, or    *
*  (at your option) any later version.                                  *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
************************************************************************/

#ifndef ANDNS_LIB_H
#define ANDNS_LIB_H

#include <string.h>
#include <stdint.h>
#include <sys/types.h>

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

/* Compression */
#define ANDNS_COMPR_THRESHOLD   1000
#define ANDNS_COMPR_LEVEL   Z_BEST_COMPRESSION

/* Misc */
#define ANDNS_MAX_ANSWERS   255
#define ANDNS_TIMEOUT       15

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
 * Functions
 */
int andns_compress(char *src,int srclen);
char* andns_uncompress(char *src,int srclen,int *dstlen) ;
int a_hdr_u(char *buf,andns_pkt *ap);
int a_qst_u(char *buf,andns_pkt *ap,int limitlen);
int a_answ_u(char *buf,andns_pkt *ap,int limitlen);
int a_answs_u(char *buf,andns_pkt *ap,int limitlen);
int a_u(char *buf,int pktlen,andns_pkt **app);
int a_hdr_p(andns_pkt *ap,char *buf);
int a_qst_p(andns_pkt *ap,char *buf,int limitlen);
int a_answ_p(andns_pkt *ap,andns_pkt_data *apd,char *buf,int limitlen);
int a_answs_p(andns_pkt *ap,char *buf, int limitlen);
int a_p(andns_pkt *ap, char *buf);
andns_pkt* create_andns_pkt(void);
andns_pkt_data* create_andns_pkt_data(void);
andns_pkt_data* andns_add_answ(andns_pkt *ap);
void destroy_andns_pkt_data(andns_pkt_data *apd);
void andns_del_answ(andns_pkt *ap);
void destroy_andns_pkt_datas(andns_pkt *ap);
void destroy_andns_pkt(andns_pkt *ap);

#endif /* ANDNS_LIB_H */
