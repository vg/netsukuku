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
 * use mrand48() and the rest of the 48bit RNG family.
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
 * get_rand_interval
 *
 * It returns a real random integer.
 *
 * The idea is simple: it counts how many microseconds passes from the start
 * to the end of a simple loop that writes for 262144 times on a buffer.
 * The interval is then returned.
 *
 * The interval is influenced chaotically by all the running programs on the
 * OS and by the kernel itself. The computer is then influenced by the user
 * interference, the attached devices, the generated interrupts, etc...
 * For this reason, a same function, will always require different time
 * intervals to solve its computation.
 *
 * You should not use get_rand_interval() to get a random number, because the
 * returned intervals are not (luckily) uniformly distributed, moreover it
 * requires a lot of computation.
 * The correct use of this function is to set the random seed using srand(),
 * seed48(), _only if_ /dev/urandom is not present in the OS.
 *
 * Thanks to /dev/random for the inspiration (it measures the interval between
 * interrupts, etc...)
 */
int get_rand_interval(void)
{
	struct timeval t1, t2;
	int i;

	gettimeofday(&t1, 0);
	for(i=0; i<512; i++) {
		int e=0;
		char buf[512];

		for(e=0; e<512; e++)
			buf[e]=((e*i*e*i)/( !i ? 7 : i+1))+e-i;
	}
	gettimeofday(&t2, 0);

	return abs((t2.tv_usec-t1.tv_usec)+(t2.tv_sec-t1.tv_sec));
}

void _xsrand(void)
{
	unsigned int seed;

	if(_rnd_urandom_fd)
		fread(&seed, sizeof(unsigned int), 1, _rnd_urandom_fd);
	else
		seed=getpid() ^ time(0) ^ clock() ^ get_rand_interval();

	srand(seed); 
}

void _xsrand48(void)
{
	if(_rnd_urandom_fd)
		fread(_rnd_rng_state, sizeof(_rnd_rng_state), 1, _rnd_urandom_fd);
	else {
		double la[3];
		struct timeval cur_t;
		long sc;

#ifdef _SC_AVPHYS_PAGES
		sc=sysconf(_SC_AVPHYS_PAGES);
#endif
		
		getloadavg(la, 3);
		gettimeofday(&cur_t, 0);
		_rnd_rng_state[0]=(int)la[0] ^ clock() ^ getpid() ^ cur_t.tv_sec ^ cur_t.tv_usec;
		gettimeofday(&cur_t, 0);
		_rnd_rng_state[1]=(int)la[1] ^ clock() ^ cur_t.tv_usec ^ get_rand_interval() ^ sc;
		gettimeofday(&cur_t, 0);
		_rnd_rng_state[2]=(int)la[2] ^ clock() ^ cur_t.tv_usec ^ cur_t.tv_sec ^ 
					get_rand_interval();
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
	return jrand48(_rnd_rng_state);
}

long int xrand(void)
{
	long int r;

	if(_rnd_urandom_fd)
		fread(&r, sizeof(long int), 1, _rnd_urandom_fd);
	else
		r=xrand_fast();

	return r;
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
