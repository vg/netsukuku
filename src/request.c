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

#include "request.h"
#include "xmalloc.h"
#include "log.h"

int op_verify(u_char op)
{
	if(op > TOTAL_REQUEST+TOTAL_REPLIES)
		return 1;
	return 0;
}

int rq_verify(u_char rq)
{
	if(rq > TOTAL_REQUEST)
		return 1;
	return 0;
}

int re_verify(u_char re)
{
	if(re > TOTAL_REPLIES && re < TOTAL_REQUEST)
		return 1;
	return 0;
}

char *rq_strerror(int err)
{
	return error_str[err];
}

char *rq_to_str(int rq)
{
	if(rq_verify)
		return unknown_request;
	
	return request_str[rq];
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

	for(; i<TOTAL_REQUEST; i++) {
		for(; e < request_array[i][RQ_MAXRQ]; e++) {
			if(tbl->rq_wait[idx] && tbl->rq_wait[idx]+request_array[i][RQ_WAIT] <= cur_t) {
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
	
	if(tbl->rq[rq] >= request_array[rq][RQ_MAXRQ])
		return E_REQUEST_TBL_FULL;
	
	return 0;
}

int find_free_rq_wait(u_char rq, rq_tbl *tbl)
{
	int e, idx=0;
	
	for(e=0; e<rq; e++) {
		idx+=request_array[e][RQ_MAXRQ];
	}
	
	for(e=idx; e < request_array[rq][RQ_MAXRQ]; e++) {
		if(!tbl->rq_wait[e])
			return e;
	}
	
	return -1;	/*This happens if the rq_tbl is full for the "rq" request*/
}

int add_rq(u_char rq, rq_tbl *tbl)
{
	int err;
	time_t cur_t;
	
	if((err=is_rq_full(rq, tbl)))
		return err;
	
	time(&cur_t);
	
	tbl->rq[rq]++;
	tbl->rq_wait[find_free_rq_wait(rq, tbl)]=cur_t;	
}

