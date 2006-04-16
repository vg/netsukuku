/* This file is part of Netsukuku
 * (c) Copyright 2006 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

#ifndef SNSD_H
#define SNSD_H

#include "inet.h"
#include "crypto.h"
#include "endianness.h"
#include "llist.c"

/*
 * SNSD definitions
 */

#define SNSD_MAX_RECORDS		256	/* Number of maximum SNSD records
						   which can be stored in an
						   andna_cache */
#define SNSD_MAX_QUEUE_RECORDS		1	/* There can be only one snsd 
						   record for the queued hnames */
#define SNSD_DEFAULT_SERVICE		0
#define SNSD_DEFAULT_PRIO		16
#define SNSD_DEFAULT_WEIGHT		1

#define SNSD_WEIGHT(x)		((x) & 0x7f) 	/* The snsd weight has to 
						   be <= 127 */

/* * snsd_node flags * */
#define SNSD_NODE_HNAME		1		/* A hname is associated in the 
					 	   snsd record */
#define SNSD_NODE_IP		(1<<1)		/* An IP is associated in the 
					   	   snsd record */


/*
 * snsd_node, snsd_service, snsd_prio
 *
 * They are three linked list. They are all orthogonal to each other.
 * The snsd_node llist is inside each snsd_prio struct which is inside each
 * snsd_service struct:
 * || service X          <->   service Y          <->   service Z  <-> ... ||
 *        |		           |			    |
 *        V		           V			    V
 *    snsd_prio_1-->node       snsd_prio_1-->node          ...-->...
 *        |		           |
 *        V		           V
 *    snsd_prio_2-->node	  ...-->node
 *    	  |
 *    	  V
 *    	 ...-->node
 *
 * Using this schema, we don't have to sort them, ever. The nodes are already
 * grouped by service and in each service by priority.
 * 
 * These llist are directly embedded in the andna_cache, lcl_cache and
 * rh_cache.
 * 
 * The andna_cache keeps all the SNSD nodes associated to the registered
 * hostname. The andna_cache doesn't need `snsd_node->pubkey'.
 */
struct snsd_node
{ 
	LLIST_HDR	(struct snsd_node);
	
	u_int		record[MAX_IP_INT];	/* It can be the IP or the md5
						   hash of the hname of the 
						   SNSD node */
	RSA		*pubkey;		/* pubkey of the snsd_node */
	char		flags;			/* This will tell us what 
						   `record' is */
	
	u_char		weight;
};
typedef struct snsd_node snsd_node;
/* In the pack of a snsd_node we don't save the `pubkey' */
#define SNSD_NODE_PACK_SZ		(MAX_IP_SZ+sizeof(char)*2)

struct snsd_prio
{
	LLIST_HDR	(struct snsd_prio);
	
	u_char		prio;			/* Priority of the SNSD node */
	
	snsd_node	*node;
};
typedef struct snsd_prio snsd_prio;
#define SNSD_PRIO_PACK_SZ		(sizeof(char))

struct snsd_service
{
	LLIST_HDR	(struct snsd_service);

	u_short		service;		/* Service number */
	
	snsd_prio	*prio;
};
typedef struct snsd_service snsd_service;
#define SNSD_SERVICE_PACK_SZ		(sizeof(char))


/*
 *  * * * snsd structs package * * *
 */

struct snsd_node_llist_hdr
{
	u_short		count;		/* # of snsd_node structs packed 
					   in the body */
};
INT_INFO snsd_node_llist_hdr_iinfo = { 1, { INT_TYPE_16BIT }, { 0 }, { 1 } };
/*
 * the body of the pkt is:
 * 
 * struct snsd_node_pack {
 *	u_int           record[MAX_IP_INT];
 *	char            flags;
 *	u_char          weight;
 * } pack[hdr.nodes];
 */
#define SNSD_NODE_LLIST_PACK_SZ(head) 	(list_count((head))*SNSD_NODE_PACK_SZ  \
					  + sizeof(struct snsd_node_llist_hdr))
		
struct snsd_prio_llist_hdr
{
	u_short		count;		/* number of structs packed in 
					   the body */
};
INT_INFO snsd_prio_llist_hdr_iinfo = { 1, { INT_TYPE_16BIT }, { 0 }, { 1 } };
/*
 * the body is:
 *
 * snsd_prio_pack {
 * 	u_char		prio;
 * 	char		snsd_node_llist_pack[SNSD_NODE_LLIST_PACK_SZ];
 * } pack[hdr.count];
 */

struct snsd_service_llist_hdr
{
	u_short		count;
};
INT_INFO snsd_service_llist_hdr_iinfo = { 1, { INT_TYPE_16BIT }, { 0 }, { 1 } };
/*
 * the body is:
 * 	u_short		service;
 * 	char		snsd_prio_llist_pack[SNSD_PRIO_LLIST_PACK_SZ];
 */


/*
 * * * Functions' declaration * * *
 */

snsd_service *snsd_find_service(snsd_service *sns, u_short service);
snsd_service *snsd_add_service(snsd_service **head, u_short service);
snsd_prio *snsd_find_prio(snsd_prio *snp, u_char prio);
snsd_prio *snsd_add_prio(snsd_prio **head, u_char prio);
snsd_node *snsd_find_node_by_record(snsd_node *snd, u_int record[MAX_IP_INT]);
snsd_node *snsd_choose_wrand(snsd_node *head);
snsd_node *snsd_add_node(snsd_node **head, u_short *counter, 
			 u_short max_records, u_int record[MAX_IP_INT]);

#endif /*SNSD_H*/
