/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
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

#include <time.h>
#include <sys/types.h>

/*WARNING* Keep it up to date!! *WARNING*/
#define TOTAL_REQUESTS          24
#define TOTAL_REPLIES		6
#define TOTAL_ERRORS		6

/*Requests*/
enum pkt_op
{
	ECHO_ME,			/*The node requests to be echoed by the dst_node*/
	ECHO_REPLY,			/*Yep, this isn't really a reply*/
	GET_FREE_IPS,			/*it means: <<Get the list of free ips in your gnode, plz>>*/
	NEW_DNODE,
	NEW_SNODE,
	NEW_GNODE,

	SET_ROUTE,
	DEL_ROUTE,
	SET_FOREIGN_ROUTE,		/*Pkt to send to a border_node, to set the route in the foreign	groupnode*/
	DEL_FOREIGN_ROUTE,
	NEW_BACKROUTE,			/*Tells the dst_node to use a different route to reply*/

	DELAYED_BROADCAST,		/*Broadcast packet to be spread only in the dst groupnode*/
	SPLIT_ROUTE,			/*This pkt advices the src_node to split the socket in two route*/
	SET_NO_IDENTITY,		/*Pkt that specify to the last node in the route to change the src ip
					  of the future incoming pkts*/

	QSPN_REQUEST,			/*The qspn_pkt used to trace the entire g_node*/
	QSPN_REPLY,			/*The qspn_pkt sent by the e_node*/
	QSPN_RFR,			/*RequestForRoute: This is used to get additional routes*/
	GET_DNODEBLOCK ,
	GET_DNODEIP,
	TRACER_PKT_CONNECT,		/*This is the tracer_pkt used to connect to the dst_node.
					  In the first entry of the tcr_pkt there's the src node, in the
					  second the dst_node, the remaining are as usual*/

	DEL_SNODE,
	DEL_GNODE,

	GET_INTMAP,
	GET_EXTMAP,

	/*  *  *  Replies  *  *  */
	QSPN_RFR_REPLY,
	PUT_FREE_IPS,			/*it means: <<Here it is the list of free ips in your gnode, cya>>*/
	PUT_DNODEBLOCK,
	PUT_DNODEIP,
	PUT_INTMAP,
	PUT_EXTMAP,

	/*Acks*/
	ACK_AFFERMATIVE,				/*Ack affermative. Everything is fine.*/
	ACK_NEGATIVE					/*The request is rejected. The error is in the pkt body*/

};

static u_char reply_array[]=
{
	QSPN_RFR_REPLY,
	PUT_FREE_IPS,
	PUT_DNODEBLOCK,
	PUT_DNODEIP, 	  
	PUT_INTMAP, 	
	PUT_EXTMAP,	 

	ACK_AFFERMATIVE,
	ACK_NEGATIVE
};

static char unknown_reply[]="Unknow reply";
static u_char reply_str[][20]=
{
	{ "QSPN_RFR_REPLY"   },
	{ "PUT_FREE_IPS" },
	{ "PUT_DNODEBLOCK" },
	{ "PUT_DNODEIP"	   },
	{ "PUT_INTMAP"	   },
	{ "PUT_EXTMAP"     },

	{ "ACK_AFFERMATIVE"},
	{ "ACK_NEGATIVE"   }
};


enum errors
{
	/*Request errors*/
	E_INVALID_REQUEST=1,
	E_ACCEPT_TBL_FULL,
	E_REQUEST_TBL_FULL,
	E_GNODE_FULL,
	E_TOO_MANY_CONN
};

static u_char error_array[]=
{	
	 NULL ,
	 E_INVALID_REQUEST ,
	 E_ACCEPT_TBL_FULL ,
	 E_REQUEST_TBL_FULL,
	 E_GNODE_FULL,
	 E_TOO_MANY_CONN   ,
};

static u_char error_str[][20]=
{	{ "Unknown error!" },
	{ "Invalid request" },
	{ "Accept table full" },
	{ "Request table full" },
	{ "Gnode full" },
	{ "Too many connection" },
};

/*Wait time*/
#define ECHO_ME_WAIT			5		/*(in seconds)*/
#define ECHO_REPLY_WAIT			5
#define GET_FREE_IPS_WAIT		10
#define NEW_DNODE_WAIT			10
#define NEW_SNODE_WAIT			10
#define NEW_GNODE_WAIT			10

#define SET_ROUTE_WAIT			5
#define DEL_ROUTE_WAIT			5
#define SET_FOREIGN_ROUTE_WAIT		5
#define DEL_FOREIGN_ROUTE_WAIT		5
#define NEW_BACKROUTE_WAIT		10

#define DELAYED_BROADCAST_WAIT		5

#define SPLIT_ROUTE_WAIT		20
#define SET_NO_IDENTITY_WAIT		20

#define QSPN_REQUEST_WAIT		10
#define QSPN_REPLY_WAIT			10
#define QSPN_RFR_WAIT		5
#define GET_DNODEBLOCK_WAIT		20
#define GET_DNODEIP_WAIT	     	5
#define TRACER_PKT_CONNECT_WAIT		10

#define DEL_SNODE_WAIT			10
#define DEL_GNODE_WAIT			10

#define GET_INTMAP_WAIT			10
#define GET_EXTMAP_WAIT			10

/*Max simultaneous requests*/ 
#define ECHO_ME_MAXRQ			20
#define ECHO_REPLY_MAXRQ		20
#define GET_FREE_IPS_MAXRQ		5
#define NEW_DNODE_MAXRQ			5
#define NEW_SNODE_MAXRQ			5
#define NEW_GNODE_MAXRQ			5

#define SET_ROUTE_MAXRQ			10
#define DEL_ROUTE_MAXRQ			10
#define SET_FOREIGN_ROUTE_MAXRQ		30
#define DEL_FOREIGN_ROUTE_MAXRQ		30
#define NEW_BACKROUTE_MAXRQ		10

#define DELAYED_BROADCAST_MAXRQ		5

#define SPLIT_ROUTE_MAXRQ		1
#define SET_NO_IDENTITY_MAXRQ		1

#define QSPN_REQUEST_MAXRQ		1
#define QSPN_REPLY_MAXRQ		1
#define QSPN_RFR_MAXRQ		10
#define GET_DNODEBLOCK_MAXRQ		1
#define GET_DNODEIP_MAXRQ		10
#define TRACER_PKT_CONNECT_MAXRQ	10

#define DEL_SNODE_MAXRQ			20
#define DEL_GNODE_MAXRQ			5

#define GET_INTMAP_MAXRQ		2
#define GET_EXTMAP_MAXRQ		2

static char unknown_request[]="Unknow request";
static u_char request_array[][2]=
{ 
	{ ECHO_ME_WAIT,        ECHO_ME_MAXRQ},
	{ ECHO_REPLY_WAIT,     ECHO_REPLY_MAXRQ},
	{ GET_FREE_IPS_WAIT, GET_FREE_IPS_MAXRQ },

	{ NEW_DNODE_WAIT, NEW_DNODE_MAXRQ },
	{ NEW_SNODE_WAIT, NEW_SNODE_MAXRQ},
	{ NEW_GNODE_WAIT, NEW_GNODE_MAXRQ},
	{ SET_ROUTE_WAIT, SET_ROUTE_MAXRQ},
	{ DEL_ROUTE_WAIT, DEL_ROUTE_MAXRQ },

	{ SET_FOREIGN_ROUTE_WAIT, SET_FOREIGN_ROUTE_MAXRQ },
	{ DEL_FOREIGN_ROUTE_WAIT, DEL_FOREIGN_ROUTE_MAXRQ},
	{ NEW_BACKROUTE_WAIT,     NEW_BACKROUTE_MAXRQ },
	{ DELAYED_BROADCAST_WAIT, DELAYED_BROADCAST_MAXRQ },
	{ SPLIT_ROUTE_WAIT,       SPLIT_ROUTE_MAXRQ       },
	{ SET_NO_IDENTITY_WAIT,   SET_NO_IDENTITY_MAXRQ   },
	{ QSPN_REQUEST_WAIT,      QSPN_REQUEST_MAXRQ      },
	{ QSPN_REPLY_WAIT,        QSPN_REPLY_MAXRQ        },
	{ QSPN_RFR_WAIT,	  QSPN_RFR_MAXRQ	  },
	{ GET_DNODEBLOCK_WAIT,    GET_DNODEBLOCK_MAXRQ    },
	{ GET_DNODEIP_WAIT,       GET_DNODEIP_MAXRQ       },
	{ TRACER_PKT_CONNECT_WAIT,TRACER_PKT_CONNECT_MAXRQ},
	{ DEL_SNODE_WAIT,          DEL_SNODE_MAXRQ        },
	{ DEL_GNODE_WAIT,         DEL_GNODE_MAXRQ         },
	{ GET_INTMAP_WAIT,        GET_INTMAP_MAXRQ        },
	{ GET_EXTMAP_WAIT,        GET_EXTMAP_MAXRQ        }
};
static u_char request_str[][20]=
{ 
	{ "ECHO_ME" },
	{ "ECHO_REPLY" },
	{ "GET_FREE_IPS" },

	{ "NEW_DNODE" },
	{ "NEW_SNODE"},
	{ "NEW_GNODE"},
	{ "SET_ROUTE"},
	{ "DEL_ROUTE" },

	{ "SET_FOREIGN_ROUTE" },
	{ "DEL_FOREIGN_ROUTE"},
	{ "NEW_BACKROUTE"},
	{ "DELAYED_BROADCAST" },
	{ "SPLIT_ROUTE" },
	{ "SET_NO_IDENTITY" },
	{ "QSPN_REQUEST"},
	{ "QSPN_REPLY"},
	{ "QSPN_RFR"},
	{ "GET_DNODE_BLOCK" },
	{ "GET_DNODE_IP"},
	{ "TRACER_PKT_CONNECT" },
	{ "DEL_SNODE" },
	{ "DEL_GNODE" },
	{ "GET_INTMAP" },
	{ "GET_EXTMAP" }
};
/*Request_array indexes defines:
 * ex: request_array[SET_FOREIGN_ROUTE][RQ_WAIT]
 */
#define RQ_WAIT 	0
#define RQ_MAXRQ	1

#define TOTAL_MAXRQ	31	

/* Request_table: It prevents requests flood and it is used in each connection.
 * Each element of the "rq" array corresponds to a request; it (the element)
 * keeps the number of requests served. If this number is equal
 * to [REQUEST]_MAXRQ, the maximum of simultaneous requests is reached.
 * Each element in rq_wait corresponds to a single request; it
 * keeps the time when that request arrived. When the current time
 * is >= [REQUEST]_WAIT+rq_wait, a new request is available and the corresponding
 * request counter in "rq" is decremented.
 */

struct request_tbl
{
	u_char 	rq[TOTAL_REQUESTS];
	time_t	rq_wait[TOTAL_MAXRQ];
};

typedef struct request_tbl rq_tbl;

int update_rq_tbl_mutex;

/*Functions declaration starts here*/
char *rq_strerror(u_char err);
#define re_strerror(err) (rq_strerror((err)))
char *rq_to_str(u_char );
char *re_to_str(u_char );
int op_verify(u_char );
int rq_verify(u_char );
int re_verify(u_char );
void update_rq_tbl(rq_tbl *);
int is_rq_full(u_char , rq_tbl *);
int find_free_rq_wait(u_char , rq_tbl *);
int add_rq(u_char , rq_tbl *);