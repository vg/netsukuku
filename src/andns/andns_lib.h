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

#include "andns.h"

/*
 * Functions
 */
int andns_compress(char *src, int srclen);
char* andns_uncompress(char *src, int srclen, int *dstlen);
int a_hdr_u(char *buf,andns_pkt *ap);
int a_qst_u(char *buf, andns_pkt *ap, int limitlen);
int a_answ_u(char *buf, andns_pkt *ap, int limitlen);
int a_answs_u(char *buf,andns_pkt *ap,int limitlen);
int a_u(char *buf, int pktlen, andns_pkt **app);
int a_hdr_p(andns_pkt *ap, char *buf);
int a_qst_p(andns_pkt *ap, char *buf, int limitlen);
int a_answ_p(andns_pkt *ap, andns_pkt_data *apd, char *buf, int limitlen);
int a_answs_p(andns_pkt *ap, char *buf, int limitlen);
int a_p(andns_pkt *ap, char *buf);
andns_pkt* create_andns_pkt(void);
andns_pkt_data* create_andns_pkt_data(void);
andns_pkt_data* andns_add_answ(andns_pkt *ap);
void destroy_andns_pkt_data(andns_pkt_data *apd);
void andns_del_answ(andns_pkt *ap);
void destroy_andns_pkt_datas(andns_pkt *ap);
void destroy_andns_pkt(andns_pkt *ap);
void align_andns_question(andns_pkt *ap, int len);


#endif /* ANDNS_LIB_H */
