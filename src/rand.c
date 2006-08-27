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
 * rand.c
 *
 * Pseudo Number Generator Functions
 * These functions use "/dev/urandom" if present on the system, otherwise they
 * use nrand48() and the rest of the 48bit RNG family.
 * The state of the 48bit RNG functions is kept on a global variable, in this
 * way, they are thread safe.
 */

#include "includes.h"

#include "rand.h"

#define URANDOM_DEVICE			"/dev/urandom"

unsigned short int _rnd_rng_state[3];
FILE *_rnd_urandom_fd=0;

/*
 * init_rand
 *
 * Initialize this code
 */
void init_rand(void)
{
	if(!_rnd_urandom_fd)
		_rnd_urandom_fd=fopen(URANDOM_DEVICE, "r");
	xsrand();
}

void close_rand(void)
{
	if(_rnd_urandom_fd)
		fclose(_rnd_urandom_fd);
}

/*
 * get_time_xorred
 *
 * It returns a real random seed.
 *
 * The idea is simple: the microseconds of the current time returned by
 * gettimeofday() mess up everything.
 * It is impossible to know what `tv_usec' will be in a given instant.
 * The returned value is the `tv_usec' xorred with `tv_sec'.
 *
 * You should not use get_time_xorred() to get a random number, because the
 * returned values are not uniformly distributed.
 * The correct use of this function is to set the random seed using srand(),
 * seed48(), _only if_ /dev/urandom is not present in the OS.
 */
int get_time_xorred(void)
{
	struct timeval t;
	gettimeofday(&t, 0);

	return t.tv_usec ^ t.tv_sec;
}


void _xsrand(void)
{
	unsigned int seed;

	if(_rnd_urandom_fd)
		fread(&seed, sizeof(unsigned int), 1, _rnd_urandom_fd);
	else
		seed=getpid() ^ clock() ^ get_time_xorred();

	srand(seed); 
}

void _xsrand48(void)
{
	if(_rnd_urandom_fd)
		fread(_rnd_rng_state, sizeof(_rnd_rng_state), 1, _rnd_urandom_fd);
	else {
		struct timeval cur_t;
		long sc=0;

#ifdef _SC_AVPHYS_PAGES
		sc=sysconf(_SC_AVPHYS_PAGES);
#endif

		_rnd_rng_state[0]=clock() ^ getpid() ^ get_time_xorred();
		_rnd_rng_state[1]=clock() ^ sc ^ get_time_xorred();
		_rnd_rng_state[2]=get_time_xorred();
	}

	seed48(_rnd_rng_state);
}

/* 
 * xsrand
 *
 * It sets the random seed with a pseudo random number 
 */
void xsrand(void)
{
	_xsrand();
	_xsrand48();
}

inline long int xrand_fast(void)
{
	return nrand48(_rnd_rng_state);
}

/*
 * xrand
 *
 * Returns a pseudo-random non-negative number.
 */
long int xrand(void)
{
	long int r;

	if(_rnd_urandom_fd)
		fread(&r, sizeof(long int), 1, _rnd_urandom_fd);
	else
		r=xrand_fast();

	return abs(r);
}

/* 
 * rand_range: It returns a random number x which is _min <= x <= _max
 */ 
inline int rand_range(int _min, int _max)
{
	return (xrand()%(_max - _min + 1)) + _min;
}

inline int rand_range_fast(int _min, int _max)
{
	return (xrand_fast()%(_max - _min + 1)) + _min;
}

/*
 * get_rand_bytes
 *
 * Copies in `garbages' `sz'# pseudo-random bytes
 */
void get_rand_bytes(void *garbage, size_t sz)
{
	size_t i,e,d;
	void *p;

	if(_rnd_urandom_fd) {
		fread(garbage, sz, 1, _rnd_urandom_fd);
		return;
	}

	if(sz <= sizeof(int)) {
		e = xrand();
		memcpy(garbage, &e, sz);
		return;
	}

	/* let `sz' be divisible by `sizeof(int)' */
	d = (sz/sizeof(int))*sizeof(int); 
	for(i=0, p=garbage; i<d; ) {
		e = xrand();
		memcpy(p, &e, sizeof(int));
		i+=sizeof(int);
		p+=sizeof(int);
	}

	if((d = sz-d)) {
		/* d is always < sizeof(int) */
		e = xrand();
		memcpy(p, &e, d);
	}
}
