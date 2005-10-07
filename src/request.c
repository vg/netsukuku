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
#include "request.h"
#include "xmalloc.h"
#include "log.h"

void rq_wait_idx_init(int *rq_wait_idx)
{
	int e, idx;
	
	for(e=0, idx=0; e<TOTAL_REQUESTS; e++) {
		rq_wait_idx[e]=idx;
		idx+=request_array[e][RQ_MAXRQ];
	}
}

int op_verify(u_char op)
{
	return (op >= TOTAL_OPS) ? 1 : 0;
}

int rq_verify(u_char rq)
{
	return rq >= TOTAL_REQUESTS ? 1 : 0;
}

int re_verify(u_char re)
{
	return ((op_verify(re)) || (re < TOTAL_REQUESTS)) ? 1 : 0;
}

int err_verify(u_char err)
{
	return err >= TOTAL_ERRORS ? 1 : 0;
}

const u_char *rq_strerror(int err)
{
	if(err_verify(err))
		return unknown_error;
	return error_str[err];
}

const u_char *rq_to_str(u_char rq)
{
	if(rq_verify(rq))
		return unknown_request;
	return request_str[rq];
}

const u_char *re_to_str(u_char re)
{
	if(re_verify(re))
		return unknown_reply;
	return reply_str[re-TOTAL_REQUESTS];
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
	if(rq_verify(rq))
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
