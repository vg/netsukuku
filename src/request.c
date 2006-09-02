/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

#include "includes.h"

#include "hash.h"
#include "request.h"
#include "xmalloc.h"
#include "log.h"

const static u_char request_str[][30]=
{ 
	{ "ECHO_ME" },
	{ "ECHO_REPLY" },
	{ "GET_FREE_NODES" },
	{ "GET_QSPN_ROUND" },

	{ "GET_INTERNET_GWS" },
	{ "SET_FOREIGN_ROUTE" },
	{ "DEL_FOREIGN_ROUTE"},
	{ "NEW_BACKROUTE"},
	{ "DELAYED_BROADCAST" },
	{ "SPLIT_ROUTE" },
	{ "SET_NO_IDENTITY" },
	{ "QSPN_CLOSE"},
	{ "QSPN_OPEN"},
	{ "QSPN_RFR"},
	{ "GET_DNODE_BLOCK" },
	{ "GET_DNODE_IP"},
	{ "TRACER_PKT" },
	{ "TRACER_PKT_CONNECT" },
	{ "DEL_SNODE" },
	{ "DEL_GNODE" },
	{ "GET_INT_MAP" },
	{ "GET_EXT_MAP" },
	{ "GET_BNODE_MAP" },

	{ "ANDNA_REGISTER_HNAME" },
	{ "ANDNA_CHECK_COUNTER"},
	{ "ANDNA_RESOLVE_HNAME"},
	{ "ANDNA_RESOLVE_IP"},
	{ "ANDNA_RESOLVE_MX"},
	{ "ANDNA_GET_ANDNA_CACHE" },
	{ "ANDNA_GET_SINGLE_ACACHE" },
	{ "ANDNA_SPREAD_SACACHE" },
	{ "ANDNA_GET_COUNT_CACHE"}

};

const static char 	unknown_reply[]="Unknow reply";
const static u_char	reply_str[][30]=
{
	{ "PUT_FREE_NODES" },
	{ "PUT_QSPN_ROUND" },
	{ "PUT_INTERNET_GWS" },
	{ "PUT_DNODEIP"	   },
	{ "EMPTY_REPLY_SLOT" },
	{ "EMPTY_REPLY_SLOT1" },
	{ "PUT_INT_MAP"	   },
	{ "PUT_EXT_MAP"     },
	{ "PUT_BNODE_MAP" },
	{ "ANDNA_RESOLVE_REPLY"   },
	{ "ANDNA_REV_RESOLVE_REPLY"   },
	{ "ANDNA_MX_RESOLVE_REPLY"   },
	{ "ANDNA_PUT_COUNT_CACHE" },
	{ "ANDNA_PUT_ANDNA_CACHE" },

	{ "ACK_AFFERMATIVE"},
	{ "ACK_NEGATIVE"   }
};

const static u_char error_str[][40]=
{	
	{ "Invalid request" },
	{ "Accept table full" },
	{ "Request table full" },
	{ "Quadro Group full" },
	{ "Netsukuku is full" },
	{ "Invalid signature" },
	{ "Cannot forward the pkt" },
	{ "Invalid hash_gnode" },
	{ "ANDNA cache queue full" },
	{ "Hostname update too early" },
	{ "Too many hostname registered" },
	{ "Hname updates counter mismatch" },
	{ "Inexistent host name" },
	{ "Counter check failed" },
	{ "Too many connection" },
};

/*Wait time*/
#define ECHO_ME_WAIT			5		/*(in seconds)*/
#define ECHO_REPLY_WAIT			5
#define GET_FREE_NODES_WAIT		10
#define GET_QSPN_ROUND_WAIT		10

#define GET_INTERNET_GWS_WAIT		5
#define SET_FOREIGN_ROUTE_WAIT		5
#define DEL_FOREIGN_ROUTE_WAIT		5
#define NEW_BACKROUTE_WAIT		10
#define DELAYED_BROADCAST_WAIT		5
#define SPLIT_ROUTE_WAIT		20
#define SET_NO_IDENTITY_WAIT		20

#define QSPN_CLOSE_WAIT			0
#define QSPN_OPEN_WAIT			0
#define QSPN_RFR_WAIT			5
#define GET_DNODEBLOCK_WAIT		20
#define GET_DNODEIP_WAIT	     	5
#define TRACER_PKT_WAIT			10
#define TRACER_PKT_CONNECT_WAIT		10

#define DEL_SNODE_WAIT			10
#define DEL_GNODE_WAIT			10

#define GET_INT_MAP_WAIT		10
#define GET_EXT_MAP_WAIT		10
#define GET_BNODE_MAP_WAIT		10

#define ANDNA_REGISTER_HNAME_WAIT	5
#define ANDNA_CHECK_COUNTER_WAIT	5
#define ANDNA_RESOLVE_HNAME_WAIT	2
#define ANDNA_RESOLVE_IP_WAIT		5
#define ANDNA_RESOLVE_MX_WAIT		5
#define ANDNA_GET_ANDNA_CACHE_WAIT	10
#define ANDNA_GET_SINGLE_ACACHE_WAIT	10
#define ANDNA_SPREAD_SACACHE_WAIT	10
#define	ANDNA_GET_COUNT_CACHE_WAIT	10


/*Max simultaneous requests*/ 
#define ECHO_ME_MAXRQ			0	/*NO LIMITS*/
#define ECHO_REPLY_MAXRQ		20
#define GET_FREE_NODES_MAXRQ		5
#define GET_QSPN_ROUND_MAXRQ		5

#define GET_INTERNET_GWS_MAXRQ		5
#define SET_FOREIGN_ROUTE_MAXRQ		30
#define DEL_FOREIGN_ROUTE_MAXRQ		30
#define NEW_BACKROUTE_MAXRQ		10
#define DELAYED_BROADCAST_MAXRQ		5
#define SPLIT_ROUTE_MAXRQ		1
#define SET_NO_IDENTITY_MAXRQ		1

#define QSPN_CLOSE_MAXRQ		0	/*NO LIMITS*/
#define QSPN_OPEN_MAXRQ			0	/*NO LIMITS*/
#define QSPN_RFR_MAXRQ			10
#define GET_DNODEBLOCK_MAXRQ		1
#define GET_DNODEIP_MAXRQ		10
#define TRACER_PKT_MAXRQ		20
#define TRACER_PKT_CONNECT_MAXRQ	10

#define DEL_SNODE_MAXRQ			20
#define DEL_GNODE_MAXRQ			5

#define GET_INT_MAP_MAXRQ		2
#define GET_EXT_MAP_MAXRQ		2
#define GET_BNODE_MAP_MAXRQ		2

#define ANDNA_REGISTER_HNAME_MAXRQ	30
#define ANDNA_CHECK_COUNTER_MAXRQ	0	/*NO LIMITS*/
#define ANDNA_RESOLVE_HNAME_MAXRQ	80
#define ANDNA_RESOLVE_IP_MAXRQ		40
#define ANDNA_RESOLVE_MX_MAXRQ		40
#define ANDNA_GET_ANDNA_CACHE_MAXRQ	5
#define ANDNA_GET_SINGLE_ACACHE_MAXRQ	10
#define ANDNA_SPREAD_SACACHE_MAXRQ	10
#define	ANDNA_GET_COUNT_CACHE_MAXRQ	5

const static u_char request_array[][2]=
{ 
	{ ECHO_ME_WAIT,        ECHO_ME_MAXRQ	    },
	{ ECHO_REPLY_WAIT,     ECHO_REPLY_MAXRQ	    },
	{ GET_FREE_NODES_WAIT, GET_FREE_NODES_MAXRQ },
	{ GET_QSPN_ROUND_WAIT, GET_QSPN_ROUND_MAXRQ },
	
	{ GET_INTERNET_GWS_WAIT,  GET_INTERNET_GWS_MAXRQ  },
	{ SET_FOREIGN_ROUTE_WAIT, SET_FOREIGN_ROUTE_MAXRQ },
	{ DEL_FOREIGN_ROUTE_WAIT, DEL_FOREIGN_ROUTE_MAXRQ },
	{ NEW_BACKROUTE_WAIT,     NEW_BACKROUTE_MAXRQ 	  },
	{ DELAYED_BROADCAST_WAIT, DELAYED_BROADCAST_MAXRQ },
	{ SPLIT_ROUTE_WAIT,       SPLIT_ROUTE_MAXRQ       },
	{ SET_NO_IDENTITY_WAIT,   SET_NO_IDENTITY_MAXRQ   },
	{ QSPN_CLOSE_WAIT,        QSPN_CLOSE_MAXRQ        },
	{ QSPN_OPEN_WAIT,         QSPN_OPEN_MAXRQ         },
	{ QSPN_RFR_WAIT,	  QSPN_RFR_MAXRQ	  },
	{ GET_DNODEBLOCK_WAIT,    GET_DNODEBLOCK_MAXRQ    },
	{ GET_DNODEIP_WAIT,       GET_DNODEIP_MAXRQ       },
	{ TRACER_PKT_WAIT,	  TRACER_PKT_MAXRQ	  },
	{ TRACER_PKT_CONNECT_WAIT,TRACER_PKT_CONNECT_MAXRQ},
	{ DEL_SNODE_WAIT,         DEL_SNODE_MAXRQ         },
	{ DEL_GNODE_WAIT,         DEL_GNODE_MAXRQ         },
	{ GET_INT_MAP_WAIT,	  GET_INT_MAP_MAXRQ	  },
	{ GET_EXT_MAP_WAIT,	  GET_EXT_MAP_MAXRQ	  },
	{ GET_BNODE_MAP_WAIT,	  GET_BNODE_MAP_MAXRQ	  },

	{ ANDNA_REGISTER_HNAME_WAIT,   ANDNA_REGISTER_HNAME_MAXRQ   },
	{ ANDNA_CHECK_COUNTER_WAIT,    ANDNA_CHECK_COUNTER_MAXRQ    },
	{ ANDNA_RESOLVE_HNAME_WAIT,    ANDNA_RESOLVE_HNAME_MAXRQ    },
	{ ANDNA_RESOLVE_IP_WAIT,       ANDNA_RESOLVE_IP_MAXRQ	    },
	{ ANDNA_RESOLVE_MX_WAIT,       ANDNA_RESOLVE_MX_MAXRQ	    },
	{ ANDNA_GET_ANDNA_CACHE_WAIT,  ANDNA_GET_ANDNA_CACHE_MAXRQ  },
	{ ANDNA_GET_SINGLE_ACACHE_WAIT,ANDNA_GET_SINGLE_ACACHE_MAXRQ},
	{ ANDNA_SPREAD_SACACHE_WAIT,   ANDNA_SPREAD_SACACHE_MAXRQ   },
	{ ANDNA_GET_COUNT_CACHE_WAIT,  ANDNA_GET_COUNT_CACHE_MAXRQ  }
};

/* 
 * Request_array indexes defines:
 * ex: request_array[SET_FOREIGN_ROUTE][RQ_WAIT]
 */
#define RQ_WAIT 	0
#define RQ_MAXRQ	1

const static u_char unknown_request[]="Unknow request";
const static u_char unknown_error[]="Unknow error";

request *ntk_request=0;
request_err *ntk_request_err=0;
int ntk_rq_counter=0, ntk_err_counter=0;


/*
 * rq_hash_name
 *
 * It return the 32bit hash of `rq_name'.
 * The returned hash is always != 0.
 */
int rq_hash_name(const char *rq_name)
{
	int hash;

	hash=fnv_32_buf((u_char *)rq_name, strlen(rq_name), FNV1_32_INIT);

	return !hash ? hash+1 : hash;
}

int hash_cmp(const void *a, const void *b)
{
	int *ai=(int *)a, *bi=(int *)b;

	return (*ai > *bi) - (*ai < *bi);
}

/*
 * rq_find_hash
 *
 * Returns the index number of the struct. contained in the `ntk_request'
 * array, which has the same `rq_hash'.
 *
 * If nothing was found, -1 is returned.
 */
int rq_find_hash(const int rq_hash)
{
	int i;

	for(i=0; i<ntk_rq_counter; i++)
		if(ntk_request[i].hash == rq_hash)
			return i;

	return -1;
}

/*
 * re_find_hash
 *
 * The same of rq_find_hash(), but searches only for replies.
 *
 * If the reply is found, its index number is returned.
 * If the reply was not found, -1 is returned.
 */
int re_find_hash(const int re_hash)
{
	int i;

	for(i=0; i<ntk_rq_counter; i++)
		if(ntk_request[i].flags & RQ_REPLY && 
				ntk_request[i].hash == re_hash)
			return i;
	return -1;
}

/*
 * rq_find_hash
 *
 * Returns the index number of the struct. contained in the `ntk_request'
 * array, which has its hash equivalent to the 32bit hash of `rq_name'.
 *
 * If nothing was found, -1 is returned.
 */
int rq_find_name(const char *rq_name)
{
	return rq_find_hash(rq_hash_name(rq_name));
}

/*
 * rq_find_hash
 *
 * Performs a bsearch(3) on the `ntk_request' array searching for a structure
 * which has the same hash of `rq_hash'.
 *
 * If the struct is found its index number is returned,
 * otherwise -1 is returned.
 */
int rq_bsearch_hash(const int rq_hash)
{
	request *rq;

	if((rq=bsearch(&rq_hash, ntk_request, ntk_rq_counter, 
			sizeof(request), hash_cmp)))
		return ((char *)rq-(char *)ntk_request)/sizeof(request);

	return -1;
}

/*
 * rq_sort_requests
 *
 * Sorts the `ntk_request' array using the 32bit hashes.
 *
 * Once the array has been sorted, it would be possible to use the
 * rq_bsearch_hash() function, which is far more efficient than
 * rq_find_hash().
 *
 * This function should be called after all the requests have been added to
 * the array with rq_add_request().
 */
void rq_sort_requests(void)
{
	qsort(ntk_request, ntk_rq_counter, sizeof(request), hash_cmp);
}

/*
 * rq_add_request
 *
 * Registers the request named `rq_name'. The name should be all in upper case.
 *
 * The hash of `rq_name' is returned.
 *
 * The hash must be saved for future use, because all the functions, which
 * deals with requests, will need it as argument.
 * The best way is to save it in a global variable, with the name all in
 * upper case, which has been declared in the header file (.h). In this way,
 * even the other source codes will be able to use it.
 *
 * If an error occurred fatal() is called, because this is a function which
 * must never fail.
 *
 * Warning: Remember to call rq_sort_requests() after all the requests have
 * 	    been registered.
 *
 * Example:
 * 	
 * 	In foo.h:
 *
 * 		int GET_NEW_MAP;
 *
 * 	In foo.c:
 *
 * 		void init_foo(void)
 * 		{
 * 			GET_NEW_MAP=rq_add_request("GET_NEW_MAP");
 * 			GET_NEW_MAP=rq_add_request("GET_NEW_FOO");
 * 			GET_NEW_MAP=rq_add_request("GET_NEW_BAR");
 * 			rq_sort_requests();
 *	 	}
 *	
 *	In bar.c
 *
 *		#include "foo.h"
 *	
 *		int func()
 *		{
 *			send_rq(pkt, 0, GET_NEW_MAP, 0, 0, 0);
 *		}
 *
 *	Note that init_foo() must be called only once.
 */
int rq_add_request(const char *rq_name, u_char flags)
{
	int hash;

	hash=rq_hash_name(rq_name);

	if(rq_find_hash(hash))
		fatal("The \"%s\" request has been already added or its hash "
		      "it's collinding with another request. "
		      "In the former case, avoid to register this request,"
		      "in the latter, change the name of the request",
		      rq_name);

	ntk_request=xrealloc(ntk_request, (ntk_rq_counter+1)*sizeof(request));

	ntk_request[ntk_rq_counter].hash=hash;
	ntk_request[ntk_rq_counter].name=rq_name;
	ntk_request[ntk_rq_counter].desc=0;
	ntk_request[ntk_rq_counter].flags=flags;
	
	ntk_rq_counter++;

	return hash;
}

/*\
 *
 * 	Request error functions
 *
 * They are the same of the above functions, but the are used for the request
 * errors.
\*/

int rqerr_hash_name(const char *rq_name)
{
	return rq_hash_name(rq_name);
}

int rqerr_find_hash(const int err_hash)
{
	int i;

	for(i=0; i<ntk_err_counter; i++)
		if(ntk_request_err[i].hash == err_hash)
			return i;

	return -1;
}

int rqerr_find_name(const char *err_name)
{
	return rqerr_find_hash(rqerr_hash_name(err_name));
}

int rqerr_bsearch_hash(const int err_hash)
{
	request_err *err;

	if((err=bsearch(&err_hash, ntk_request_err, ntk_err_counter, 
			sizeof(request_err), hash_cmp)))
		return ((char *)err-(char *)ntk_request_err)/sizeof(request_err);

	return -1;
}

/*
 * rqerr_sort_errors
 *
 * The equivalent of rq_sort_requests() for requests errors.
 *
 * Warning: Remember to call rqerr_sort_errors() after all the requests have 
 * 	    been registered.
 */
void rqerr_sort_errors(void)
{
	qsort(ntk_request_err, ntk_err_counter, sizeof(request_err), hash_cmp);
}

/*
 * rqerr_add_error
 *
 * The same of rq_add_request(), the only difference is `err_desc', which is
 * the short description, in a form comprehensible to humans, of the error.
 */
int rqerr_add_error(const char *err_name, const char *err_desc)
{
	int hash;

	hash=rqerr_hash_name(err_name);

	if(rqerr_find_hash(hash))
		fatal("The \"%s\" error has been already added or its hash "
		      "it's collinding with another error. "
		      "In the former case, avoid to register this error,"
		      "in the latter, change the name of the error",
		      err_name);

	ntk_request_err=xrealloc(ntk_request_err, 
					(ntk_err_counter+1)*sizeof(request));

	ntk_request_err[ntk_err_counter].hash=hash;
	ntk_request_err[ntk_err_counter].name=err_name;
	ntk_request_err[ntk_err_counter].desc=err_desc;
	ntk_request_err[ntk_err_counter].flags=0;
	
	ntk_err_counter++;

	return hash;
}


void rq_wait_idx_init(int *rq_wait_idx)
{
	int e, idx;
	
	for(e=0, idx=0; e<TOTAL_REQUESTS; e++) {
		rq_wait_idx[e]=idx;
		idx+=request_array[e][RQ_MAXRQ];
	}
}

/*
 * rq_strerror
 *
 * Returns the description of the error `err_hash'.
 */
const u_char *rq_strerror(int err_hash)
{
	int i=rqerr_find_hash(err_hash);

	if(i < 0)
		return unknown_error;
	return ntk_request_err[i].desc;
}

const u_char *re_strerror(int err_hash)
{
	return rq_strerror(err_hash);
}

/*
 * rq_to_str
 *
 * Returns the string of the name of the `rq_hash' request.
 */
const u_char *rq_to_str(int rq_hash)
{
	int i=rq_find_hash(rq_hash);

	if(i < 0)
		return unknown_request;
	return ntk_request[i].name;
}

/*
 * re_to_str
 *
 * Returns the string of the name of the `rq_hash' reply.
 */
const u_char *re_to_str(int rq_hash)
{
	int i=rq_find_hash(rq_hash);

	if(i < 0 || !(ntk_request[i].flags & RQ_REPLY))
		return (const u_char*)unknown_reply;
	return ntk_request[i].name;
}

void update_rq_tbl(rq_tbl *tbl)
{
	u_char i=0,e=0, idx=0;
	time_t cur_t;

	if(update_rq_tbl_mutex)
		return;
	else
		update_rq_tbl_mutex=1;

	time(&cur_t);

	for(; i<TOTAL_REQUESTS; i++) {
		for(e=0; e < request_array[i][RQ_MAXRQ]; e++) {
			if(tbl->rq_wait[idx] && (tbl->rq_wait[idx]+request_array[i][RQ_WAIT]) <= cur_t) {
				tbl->rq_wait[idx]=0;
				tbl->rq[i]--;
			}
			idx++;
		}
	}

	update_rq_tbl_mutex=0;
}
	
int is_rq_full(u_char rq, rq_tbl *tbl)
{
	if(rq_find_hash(rq))
		return E_INVALID_REQUEST;
	
	update_rq_tbl(tbl);
	
	if(tbl->rq[rq] >= request_array[rq][RQ_MAXRQ] && request_array[rq][RQ_MAXRQ])
		return E_REQUEST_TBL_FULL;
	else if(!request_array[rq][RQ_MAXRQ])
		return -1; /* No limits */
	else
		return 0;
}



int find_free_rq_wait(u_char rq, rq_tbl *tbl)
{
	int e, idx;
	
	for(e=0; e < request_array[rq][RQ_MAXRQ]; e++) {
		idx = rq_wait_idx[rq] + e;
		if(!tbl->rq_wait[idx])
			return idx;
	}
	
	return -1;	/*This happens if the rq_tbl is full for the "rq" request*/
}

int add_rq(u_char rq, rq_tbl *tbl)
{
	int err;
	time_t cur_t;
	
	/* TODO: XXX: Activate it and test it!!! */
	return 0;
	/* TODO: XXX: Activate it and test it!!! */
	
	if((err=is_rq_full(rq, tbl)) > 0)
		return err;
	else if(err < 0)
		return 0; /* no limits */
	
	time(&cur_t);
	
	tbl->rq[rq]++;
	tbl->rq_wait[find_free_rq_wait(rq, tbl)]=cur_t;	
	return 0;
}

/*
 * op_filter_set
 *
 * Sets the RQ_DROP flags to the request or reply `rq_hash', f.e.:
 *
 * 	op_filter_set(GET_NEW_MAP);
 *
 * If no request nor reply was found, -1 is returned.
 */
int op_filter_set(int rq_hash)
{
	int i;

	if((i=rq_find_hash(rq_hash)) >= 0)
		ntk_request[i].flags|=RQ_DROP;
	else if((i=rqerr_find_hash(rq_hash)) >= 0)
		ntk_request_err[i].flags|=RQ_DROP;
	else
			return -1;

	return 0;
}

/*
 * op_filter_clr
 *
 * Remove the RQ_DROP flags from the request or reply `rq_hash', f.e.:
 *
 * 	op_filter_clr(GET_NEW_MAP);
 *
 * If no request nor reply was found, -1 is returned.
 */
int op_filter_clr(int rq_hash)
{
	int i;

	if((i=rq_find_hash(rq_hash)) >= 0)
		ntk_request[i].flags&=~RQ_DROP;
	else if((i=rqerr_find_hash(rq_hash)) >= 0)
		ntk_request_err[i].flags&=~RQ_DROP;
	else
			return -1;

	return 0;
}

/*
 * op_filter_test
 *
 * Tests is the RQ_DROP flag is set for the request or reply `rq_hash', f.e.:
 *
 * 	op_filter_test(GET_NEW_MAP);
 *
 * If the flag is set 1 is returned, otherwise 0.
 * If no request nor reply was found, -1 is returned.
 *
 * Warning: it's not adviced to use this function as if(op_filter_test(...)), 
 *          because, in this way, you'll miss the eventual returned -1 value.
 */
int op_filter_test(int rq_hash)
{
	int i;

	if((i=rq_find_hash(rq_hash)) >= 0)
		return ntk_request[i].flags & RQ_DROP;
	else if((i=rqerr_find_hash(rq_hash)) >= 0)
		return ntk_request_err[i].flags & RQ_DROP;
	else
		return -1;

	return 0;
}


/*
 * op_filter_reset_re
 *
 * If `bit' is equal to OP_FILTER_ALLOW it removes the RQ_DROP flag from all
 * the error requests, 
 * if instead `bit' is equal to OP_FILTER_DROP, it sets the RQ_DROP to all the
 * error requests.
 */
void op_filter_reset_re(int bit)
{
	int i;

	if(bit == OP_FILTER_DROP)
		for(i=0; i<ntk_err_counter; i++)
			ntk_request_err[i].flags|=RQ_DROP;
	else
		for(i=0; i<ntk_err_counter; i++)
			ntk_request_err[i].flags&=~RQ_DROP;
}

/*
 * op_filter_reset_rq
 *
 * If `bit' is equal to OP_FILTER_ALLOW it removes the RQ_DROP flag from all
 * the requests, 
 * if instead `bit' is equal to OP_FILTER_DROP, it sets the RQ_DROP to all the
 * requests.
 */
void op_filter_reset_rq(int bit)
{
	int i;

	if(bit == OP_FILTER_DROP)
		for(i=0; i<ntk_rq_counter; i++)
			ntk_request[i].flags|=RQ_DROP;
	else
		for(i=0; i<ntk_rq_counter; i++)
			ntk_request[i].flags&=~RQ_DROP;
}

/*
 * op_filter_reset
 *
 * If `bit' is equal to OP_FILTER_ALLOW it removes the RQ_DROP flag from all
 * the requests and error requests, 
 * if instead `bit' is equal to OP_FILTER_DROP, it sets the RQ_DROP to all the
 * requests and error requests.
 */
void op_filter_reset(int bit)
{
	op_filter_reset_re(bit);
	op_filter_reset_rq(bit);
}
