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
 * 
 * --
 * request.c
 *
 * The code that flowa from this source preserves the database of the
 * Netsukuku requests, replies and errors.
 *
 * It's structure is completely modular: it's possible to register at any
 * moment a new request, therefore if a module registers its own requests,
 * they will be accessible by all the other modules and sources of NTK.
 * 
 * The API is described in request.h
 */

#include "includes.h"

#include "hash.h"
#include "request.h"
#include "xmalloc.h"
#include "log.h"

const static u_char unknown_request[]="Unknow request";
const static char unknown_reply[]="Unknow reply";
const static u_char unknown_error[]="Unknow error";

request *ntk_request=0;
request_err *ntk_request_err=0;
int ntk_rq_counter=0, ntk_err_counter=0;
u_char ntk_request_sorted=0, ntk_request_err_sorted=0;

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

	if(!ntk_request_sorted)
		rq_sort_requests();

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
 * Once the array has been sorted, it is possible to use the
 * rq_bsearch_hash() function, which is far more efficient than
 * rq_find_hash().
 *
 * This function should be called after all the requests have been added to
 * the array with rq_add_request().
 */
void rq_sort_requests(void)
{
	qsort(ntk_request, ntk_rq_counter, sizeof(request), hash_cmp);
	ntk_request_sorted=1;
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
 * To avoid conflicts, the name of the variable and of the request must be
 * prefixed with a unique identifier.
 * It's adviced to use the name of the relative source code.
 *
 * If an error occurred fatal() is called, because this is a function which
 * must never fail.
 *
 *  :Warning: 
 * 	 
 * 	 * Use rq_del_request() when the request won't be used anymore
 *
 *  :Warning:
 *
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
	ntk_request_sorted=0;

	return hash;
}

/*
 * rq_del_request
 *
 * Removes the `rq_hash' request (or reply) from the ntk_request array
 */
void rq_del_request(int rq_hash)
{
	int idx;

	if((idx=rq_find_hash(rq_hash)) < 0)
		return;

	if(idx < ntk_rq_counter-1)
		/* Shifts all the succesive elements of `idx', in this way,
		 * the order of the array isn't changed */
		memmove(&ntk_request[idx], 
			  &ntk_request[idx+1],
				sizeof(request)*(ntk_rq_counter-idx-1));
	ntk_rq_counter--;
	ntk_request=xrealloc(ntk_request, ntk_rq_counter*sizeof(request));
}

/*
 * rq_get_rqstruct
 *
 * Returns the pointer to the request structure which contains `rq_hash'.
 * If no structure is found, 0 is returned.
 */
request *rq_get_rqstruct(int rq_hash)
{
	int i=rq_find_hash(rq_hash);
	return i < 0 ? 0 : &ntk_request[i];
}


/*\
 *
 *   * * *   Request error functions   * * *
 *
 * They are the same of the above functions, but the are used for the request
 * errors.
 *
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

	if(!ntk_request_err_sorted)
		rqerr_sort_errors();

	if((err=bsearch(&err_hash, ntk_request_err, ntk_err_counter, 
			sizeof(request_err), hash_cmp)))
		return ((char *)err-(char *)ntk_request_err)/sizeof(request_err);

	return -1;
}

/*
 * rqerr_sort_errors
 *
 * The equivalent of rq_sort_requests() for requests errors.
 */
void rqerr_sort_errors(void)
{
	qsort(ntk_request_err, ntk_err_counter, sizeof(request_err), hash_cmp);
	ntk_request_err_sorted=1;
}

/*
 * rqerr_add_error
 *
 * The same of rq_add_request(), the only difference is `err_desc', which is
 * the short description, in a form comprehensible to humans, of the error.
 *
 *  :Warning: 
 * 	    
 * 	    * Use rqerr_del_error() when the request error won't be used
 * 	      anymore.
 *
 *  :Warning:
 *
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
	ntk_request_err_sorted=0;

	return hash;
}

/*
 * rqerr_del_error
 *
 * Removes the `rqerr_hash' request error from the ntk_request_err array
 */
void rqerr_del_error(int rqerr_hash)
{
	int idx;

	if((idx=rqerr_find_hash(rqerr_hash)) < 0)
		return;

	if(idx < ntk_err_counter-1)
		/* Shifts all the succesive elements of `idx', in this way,
		 * the order of the array isn't changed */
		memmove(&ntk_request_err[idx], 
			  &ntk_request_err[idx+1],
				sizeof(request_err)*(ntk_err_counter-idx-1));
	ntk_err_counter--;
	ntk_request_err=xrealloc(ntk_request_err, ntk_err_counter*sizeof(request_err));
}


/*
 * rqerr_get_rqstruct
 *
 * Returns the pointer to the request_err structure which contains `err_hash'.
 * If no structure is found, 0 is returned.
 */
request_err *rqerr_get_rqstruct(int err_hash)
{
	int i=rqerr_find_hash(err_hash);
	return i < 0 ? 0 : &ntk_request_err[i];
}

/*\
 *
 *   * * *   String conversion functions   * * *
 *
\*/

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

/*\
 *
 * 	* * *   OP filter functions   * * *
 *
 *
 * These functions set or remove the RQ_DROP flag of the specified request or
 * reply.
 *
\*/

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
