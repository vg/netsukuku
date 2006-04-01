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

#ifndef ANDNSLIB_H
#define ANDNSLIB_H

#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#define ANDNS_MAX_DATA_LEN	512
#define ANDNS_MAX_QST_LEN	512

struct andns_pkt_data
{
        uint16_t                rdlength;
        char                    rdata[ANDNS_MAX_DATA_LEN];
        struct andns_pkt_data   *next;
};
typedef struct andns_pkt_data andns_pkt_data;
#define ANDNS_PKT_DATA_SZ sizeof(andns_pkt_data)

typedef struct andns_pkt
{
        uint16_t        id;
        uint8_t         qr;
        uint8_t         qtype;
        uint8_t         ancount;
        uint8_t         nk;
        uint8_t         rcode;
        uint16_t        qstlength;
        char            qstdata[ANDNS_MAX_QST_LEN];
        andns_pkt_data  *pkt_answ;
} andns_pkt;
#define ANDNS_PKT_SZ sizeof(andns_pkt)

#define ANDNS_HDR_SZ	4
#define ANDNS_MAX_SZ 	ANDNS_HDR_SZ+ANDNS_MAX_QST_LEN+ANDNS_MAX_QST_LEN+4

/* ANDNS QUERY-TYPE */
#define AT_A            0 /* h->ip */
#define AT_PTR          1 /* ip->h */
#define AT_MX           2 /* h->mx */
#define AT_MXPTR        3 /* ip->mx */
/* RCODES: The rcodes are portable between ANDNS and DNS */
#define ANDNS_RCODE_NOERR     0       /* No error */
#define ANDNS_RCODE_EINTRPRT  1       /* Intepret error */
#define ANDNS_RCODE_ESRVFAIL  2       /* Server failure */
#define ANDNS_RCODE_ENSDMN    3       /* No such domain */
#define ANDNS_RCODE_ENIMPL    4       /* Not implemented */
#define ANDNS_RCODE_ERFSD     5       /* Refused */

size_t a_hdr_u(char *buf,andns_pkt *ap);
size_t a_qst_u(char *buf,andns_pkt *ap,int limitlen);
size_t a_answ_u(char *buf,andns_pkt *ap,int limitlen);
size_t a_answs_u(char *buf,andns_pkt *ap,int limitlen);
size_t a_u(char *buf,size_t pktlen,andns_pkt **app);
size_t a_hdr_p(andns_pkt *ap,char *buf);
size_t a_qst_p(andns_pkt *ap,char *buf,size_t limitlen);
size_t a_answ_p(andns_pkt_data *apd,char *buf,size_t limitlen);
size_t a_answs_p(andns_pkt *ap,char *buf, size_t limitlen);
size_t a_p(andns_pkt *ap, char *buf);
andns_pkt* create_andns_pkt(void);
andns_pkt_data* create_andns_pkt_data(void);
andns_pkt_data* andns_add_answ(andns_pkt *ap);
void destroy_andns_pkt(andns_pkt *ap);

#endif /* ANDNSLIB_H */

