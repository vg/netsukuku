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
 * --
 * 128bit-gmp.c: I made this to handle the HUGE ipv6 numbers
 */

#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <gmp.h>
#include <stdlib.h>

#define MAX32		4294967295
#define ZERO128		{0,0,0,0}

int sum_int(unsigned int , unsigned int *);
int sum_128(unsigned int *, unsigned int *);
int sub_int(unsigned int *, unsigned int);
int sub_128(unsigned int *, unsigned int *);
int htonl_128(unsigned int *, unsigned int *, int );
int ntohl_128(unsigned int *, unsigned int *, int );

/*y=x+y*/
int sum_128(unsigned int *x, unsigned int *y)
{
	mpz_t xx, yy, res;
	size_t count;
	
	mpz_init(res);
	mpz_init(xx);
	mpz_init(yy);
	mpz_import (xx, 4, 1, sizeof(x[0]), 0, 0, x);
	mpz_import (yy, 4, 1, sizeof(y[0]), 0, 0, y);

	mpz_add(res, xx, yy);
	memset(y, '\0', sizeof(y[0])*4);
	mpz_export(y, &count, 1, sizeof(x[0]), 0,0, res);
	
	mpz_clear(xx);
	mpz_clear(yy);
	mpz_clear(res);
	return 0;	
}

/*y=x+y*/
int sum_int(unsigned int x, unsigned int *y)
{
	unsigned int z[4]=ZERO128;
	
	z[3]=x;
	return sum_128(z, y);
}

/*y=x-y*/
int sub_128(unsigned int *x, unsigned int *y)
{
	mpz_t xx, yy, res;
	size_t count;
	
	mpz_init(res);
	mpz_init(xx);
	mpz_init(yy);
	mpz_import(xx, 4, 1, sizeof(x[0]), 0, 0, x);
	mpz_import(yy, 4, 1, sizeof(y[0]), 0, 0, y);

	mpz_sub(res, xx, yy);
	memset(y, '\0', sizeof(y[0])*4);
	mpz_export(y, &count, 1, sizeof(x[0]), 0,0, res);
	
	mpz_clear(xx);
	mpz_clear(yy);
	mpz_clear(res);
	return 0;	
}

/* y=y-x */
int sub_int(unsigned int *y, unsigned int x)
{
	unsigned int z[4]=ZERO128;

	z[3]=x;
	return sub_128(z, y);
}

/* "ORDER can be 1 for most significant word first or -1 for least significant first." */
int htonl_128(unsigned int *x, unsigned int *y, int order)
{
	mpz_t xx;
	size_t count;
	
	if(!order)
		order=1;
	mpz_init(xx);
	mpz_import(xx, 4, order, sizeof(x[0]), -1, 0, x);
	memset(y, '\0', sizeof(y[0])*4);
	mpz_export(y, &count, order, sizeof(x[0]), 1,0, xx);
	mpz_clear(xx);
	return 0;
}

int ntohl_128(unsigned int *x, unsigned int *y, int order)
{
	mpz_t xx;
	size_t count;
	
	if(!order)
		order=1;
	mpz_init(xx);
	mpz_import(xx, 4, order, sizeof(x[0]), 1, 0, x);
	memset(y, '\0', sizeof(y[0])*4);
	mpz_export(y, &count, order, sizeof(x[0]), -1,0, xx);	
	mpz_clear(xx);

	return 0;
}
