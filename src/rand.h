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


void init_rand(void);
void close_rand(void);
int get_time_xorred(void);
void xsrand(void);
inline long int xrand_fast(void);
long int xrand(void);
inline int rand_range(int _min, int _max);
inline int rand_range_fast(int _min, int _max);
void get_rand_bytes(void *garbage, size_t sz);
