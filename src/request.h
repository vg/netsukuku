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

/*\
 *
 *			Request API
 *		      ===============
 *
 * Defines
 * -------
 *
 * A `request' is a packet sent to a remote host. The packet asks to the host
 * some information or tells it to do something.
 *
 * A `reply' is the request sent back by the remote host as an answer to a
 * request. Generally for each reply exists the relative request.
 *
 * A `request error' is a reply which contains just an error code.
 *
 *
 * Registering a new request/reply/error
 * -------------------------------------
 *
 * The name of the request/reply/error has to be in this format:
 *
 * 		ID_REQUESTNAME
 *
 * where ID is the short name of your module (in upper case), and
 * REQUESTNAME is the name of the request (always in upper case). 
 * F.e:
 * 		FOO_GET_NEW_MAP
 *
 * The rq_add_request() and rqerr_add_error() are used to register
 * respectively a new request/reply or a request error. 
 * Their returned value is the numeric id assigned to the request/reply/error.
 * It must be saved in a global variable (non static), in this way, even other
 * modules we'll be able to use it. Use the RQ_ADD_REQUEST() and
 * RQERR_ADD_ERROR() macros to facilitate this assignment.
 * The variable name is the same of the request name (always in upper case).
 * All the other functions of the NTK code which deals with requests, will
 * take as arguments this numeric id.
 * See request.c for the complete description rq_add_request().
 *
 * They must be called only once per request, therefore the best strategy is
 * to put them inside initialization functions.
 * 
 * If you are implementing a module, you should use the rq_del_request() and
 * rqerr_del_error() functions in the de-initialization function, otherwise if
 * the module will be closed and then loaded a second time, fatal() will be
 * called, because request.c would think that a name collision happened.
 *
 * Example:
 *
 * 	In foo.h:
 *
 * 		rq_t 	FOO_GET_NEW_MAP,
 * 			FOO_PUT_NEW_MAP,
 * 			FOO_GET_PASS;
 *
 * 		rqerr_t E_INVALID_UNIVERSE;
 *
 * 	In foo.c:
 *
 * 		void init_foo(void)
 * 		{
 * 			RQ_ADD_REQUEST( FOO_GET_NEW_MAP, 0 );
 * 			RQ_ADD_REQUEST( FOO_PUT_NEW_MAP, RQ_REPLY );
 * 			RQ_ADD_REQUEST( FOO_GET_PASS, 0 );
 *
 * 			RQERR_ADD_ERROR( E_INVALID_UNIVERSE, 0 );
 *	 	}
 *
 *	 	void close_foo(void)
 *	 	{
 *	 		rq_del_request(FOO_GET_NEW_MAP);
 *	 		rq_del_request(FOO_PUT_NEW_MAP);
 *	 		rq_del_request(FOO_GET_PASS);
 *	 		rqerr_del_error(E_INVALID_UNIVERSE);
 *	 	}
 *	
 *	In bar.c
 *
 *		#include "foo.h"
 *	
 *		int func()
 *		{
 *			send_rq(pkt, 0, FOO_GET_NEW_MAP, 0, 0, 0);
 *		}
 *
 *  Note that init_foo() must be called only once.
 *
\*/

#include "misc.h"

#define REQUEST_TIMEOUT		300	/* The timeout in seconds for all the 
					   requests */
#ifdef DEBUG
#undef REQUEST_TIMEOUT
#define REQUEST_TIMEOUT		20
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


typedef int32_t rq_t;
#define re_t	rq_t
typedef int32_t rqerr_t;

/*
 * request
 *
 * A request or a reply.
 */
typedef struct
{
	rq_t		hash;		/* Hash of the request name */

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


/*\
 *
 * Functions declaration starts here
 *
\*/

rq_t rq_hash_name(const char *rq_name);
void rq_sort_requests(void);

rqerr_t rqerr_hash_name(const char *rq_name);
void rqerr_sort_errors(void);

#define RQ_ADD_REQUEST(_rq, _flags)	_rq = rq_add_request(#_rq , (_flags))
int rq_add_request(const char *rq_name, u_char flags);
void rq_del_request(int rq_hash);
request *rq_get_rqstruct(int rq_hash);

#define RQERR_ADD_ERROR(_err, _flags)	_err = rqerr_add_error(#_err , (_flags))
int rqerr_add_error(const char *err_name, const char *err_desc);
void rqerr_del_error(int rq_hash);
request_err *rqerr_get_rqstruct(int err_hash);

const u_char *rq_strerror(int err_hash);
const u_char *re_strerror(int err_hash);
const u_char *rq_to_str(int rq_hash);
const u_char *re_to_str(int rq_hash);
const u_char *rqerr_to_str(rqerr_t err_hash);
const u_char *rq_rqerr_to_str(rq_t rq_hash);

int op_filter_set(int rq_hash);
int op_filter_clr(int rq_hash);
int op_filter_test(int rq_hash);
void op_filter_reset_re(int bit);
void op_filter_reset_rq(int bit);
void op_filter_reset(int bit);

#endif /*REQUEST_H*/
