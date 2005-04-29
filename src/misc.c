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

#include "includes.h"
#include "misc.h"

/* Robert Jenkins's 32 bit Mix Function */
unsigned int inthash(unsigned int key)
{
	key += (key << 12);
	key ^= (key >> 22);
	key += (key << 4);
	key ^= (key >> 9);
	key += (key << 10);
	key ^= (key >> 2);
	key += (key << 7);
	key ^= (key >> 12);
	return key;
}

/* 
 * Ripped from glibc.
 * This is the hashing function specified by the ELF ABI.  In the
 * first five operations no overflow is possible so we optimized it a
 * bit.  
 */
inline unsigned int dl_elf_hash (const unsigned char *name)
{
  unsigned long int hash = 0;
  if (*name != '\0') {
      hash = *name++;
      if (*name != '\0') {
	  hash = (hash << 4) + *name++;
	  if (*name != '\0') {
	      hash = (hash << 4) + *name++;
	      if (*name != '\0') {
		  hash = (hash << 4) + *name++;
		  if (*name != '\0') {
		      hash = (hash << 4) + *name++;
		      while (*name != '\0') {
			  unsigned long int hi;
			  hash = (hash << 4) + *name++;
			  hi = hash & 0xf0000000;

			  /* The algorithm specified in the ELF ABI is as
			     follows:

			     if (hi != 0)
			       hash ^= hi >> 24;

			     hash &= ~hi;

			     But the following is equivalent and a lot
			     faster, especially on modern processors.  */

			  hash ^= hi;
			  hash ^= hi >> 24;
			}
		    }
		}
	    }
	}
    }
  return hash;
}

/* 
 * hash_time: As the name says: hash time!
 * This function generates the hash of the timeval struct which refer
 * to the current time. 
 * The returned integer is a unique number, this function will never return the 
 * same number.
 * If h_sec or h_usec are not null, it stores in them respectively the hash of
 * the second and the microsecond.
 */
int hash_time(int *h_sec, int *h_usec)
{
	struct timeval t;
	char str[sizeof(struct timeval)+1];
	int elf_hash;
	
	gettimeofday(&t, 0);
	memcpy(str, &t, sizeof(struct timeval));
	str[sizeof(struct timeval)]=0;

	elf_hash=dl_elf_hash(str);
	
	if(h_sec)
		*h_sec=inthash(t.tv_sec);
	if(h_usec)
		*h_usec=inthash(t.tv_usec);

	return inthash(elf_hash);
}

/* 
 * rand_range: It returns a random number x which is _min <= x <= _max
 */ 
inline int rand_range(int _min, int _max)
{
	return (rand()%(_max - _min + 1)) + _min;
}

/* 
 * xsrand: It sets the random seed with a pseudo random number 
 */
void xsrand(void)
{
	srand(getpid() ^ time(0) ^ clock());
}

/* Is the buffer `a' filled with `sz'# of zeros?
 * If yes return 0. */
int is_bufzero(char *a, int sz)
{
	int i;
	for(i=0; i<sz; i++, a++)
		if(*a)
			return 1;
	return 0;
}
