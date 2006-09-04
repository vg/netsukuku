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

#ifndef REQUEST_H
#define REQUEST_H

#include "misc.h"

#define REQUEST_TIMEOUT		300	/* The timeout in seconds for all the 
					   requests */
#ifdef DEBUG
#undef REQUEST_TIMEOUT
#define REQUEST_TIMEOUT		20
#endif

#if CONTINUE_HERE
/* TODO: CONTINUE HERE 
 *
 * SORT SORT SORT SORT SORT
 * SORT SORT SORT
 * SLRT */
enum errors
{
	/*Request errors*/
	E_INVALID_REQUEST = rqerr_add_error("E_INVALID_REQUEST", "Invalid request");
	E_ACCEPT_TBL_FULL = rqerr_add_error("E_ACCEPT_TBL_FULL", "Accept table full");

	E_QGROUP_FULL = rqerr_add_error("E_QGROUP_FULL", "Quadro Group full");
	E_NTK_FULL = rqerr_add_error("E_NTK_FULL", "No more cyberspace left");

	E_INVALID_SIGNATURE = rqerr_add_error("E_INVALID_SIGNATURE", "Invalid signature");
	E_CANNOT_FORWARD = rqerr_add_error("E_CANNOT_FORWARD", "Cannot forward the pkt");
	
	E_TOO_MANY_CONN = rqerr_add_error("E_TOO_MANY_CONN", "Too many connection");
};
#endif

/*
 * request.flags
 */
#define RQ_REPLY	1		/* This request is used as a 
				   	   reply to another request */
#define RQ_DROP		(1<<1)		/* If set, all the received requests of
					   the this same type will be dropped */
#define OP_FILTER_DROP	1
#define OP_FILTER_ALLOW	0

/*
 * request
 *
 * A request or a reply.
 */
typedef struct
{
	int		hash;		/* Hash of the request name */

	const char	*name;		/* Name of the request */

	const char	*desc;		/* Description */

	u_char		flags;

} request;

/*
 * request_err
 *
 * Error replied to a request that couldn't be fulfilled.
 * The error reply is generally sent with the pkts.c/pkt_err() function.
 *
 * The structure is the same of `request', but, in this case, the .name 
 * member is the string describing, in a form comprehensible to humans, the
 * error.
 */
typedef request request_err;


/* 
 * Request_table: It prevents requests flood and it is used in each connection.
 * Each element of the "rq" array corresponds to a request; it (the element)
 * keeps the number of requests served. If this number is equal
 * to [REQUEST]_MAXRQ, the maximum of simultaneous requests is reached.
 * 
 * Each element in rq_wait corresponds to a single request so it is formed by:
 * { [REQUEST 0]_MAXRQ elements | [REQUEST 1]_MAXRQ elements | ... };
 * rq_wait_idx keeps track of this but it must be initialized once with
 * rq_wait_idx_init().
 * Each element of rq_wait keeps the time when that request arrived. 
 * When the current time is >= [REQUEST]_WAIT+rq_wait, a new request is 
 * available and the corresponding request counter in "rq" is decremented. 
 */

#define TOTAL_MAXRQ	31
struct request_tbl
{
	u_char 	rq[TOTAL_REQUESTS];
	time_t	rq_wait[TOTAL_MAXRQ];
};
typedef struct request_tbl rq_tbl;

int rq_wait_idx[TOTAL_REQUESTS];

int update_rq_tbl_mutex;


/* 
 * Functions declaration starts here
 */

void rq_sort_requests(void);
int rq_add_request(const char *rq_name, u_char flags);
void rqerr_sort_errors(void);
int rqerr_add_error(const char *err_name, const char *err_desc);
request *rq_get_rqstruct(int rq_hash);
request_err *rqerr_get_rqstruct(int err_hash);

const u_char *rq_strerror(int err_hash);
const u_char *rq_to_str(int rq_hash);
const u_char *re_to_str(int rq_hash);
const u_char *re_strerror(int err_hash);

int op_verify(u_char op);
int op_filter_set(int rq_hash);
int op_filter_clr(int rq_hash);
int op_filter_test(int rq_hash);
void op_filter_reset_re(int bit);
void op_filter_reset_rq(int bit);
void op_filter_reset(int bit);

#endif /*REQUEST_H*/

