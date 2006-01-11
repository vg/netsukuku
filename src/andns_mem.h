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
#ifndef ANDNS_MEM_H
#define ANDNS_MEM_H

dns_pkt* create_dns_pkt();
dns_pkt_qst* create_dns_pkt_qst();
dns_pkt_a* create_dns_pkt_a();
dns_pkt_qst* dns_add_qst(dns_pkt *dp);
void dns_del_last_qst(dns_pkt *dp);
dns_pkt_a* dns_add_a(dns_pkt_a **dpa);
void destroy_dns_pkt(dns_pkt *dp);

andns_pkt* create_andns_pkt();
andns_pkt_data* create_andns_pkt_data();
andns_pkt_data* andns_add_answ(andns_pkt *ap);
void destroy_andns_pkt(andns_pkt *ap);

#endif // ANDNS_MEM_H
