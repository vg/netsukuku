/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
 * misc.c: Miscellaneous functions, nothing else.
 */

#define MILLISEC(x)	(((x).tv_sec*1000)+((x).tv_usec/1000))

#define FNV_32_PRIME ((u_long)0x01000193)
#define FNV1_32_INIT ((u_long)0x811c9dc5)

/* Bit map related macros.  */
#define SET_BIT(a,i)     ((a)[(i)/CHAR_BIT] |= 1<<((i)%CHAR_BIT))
#define CLR_BIT(a,i)     ((a)[(i)/CHAR_BIT] &= ~(1<<((i)%CHAR_BIT)))
#define TEST_BIT(a,i)    (((a)[(i)/CHAR_BIT] & (1<<((i)%CHAR_BIT))) ? 1 : 0)

u_long fnv_32_buf(void *buf, size_t len, u_long hval);
unsigned int inthash(unsigned int key);
inline unsigned int dl_elf_hash (const unsigned char *name);
int hash_time(int *h_sec, int *h_usec);
void swap_array(int nmemb, size_t nmemb_sz, void *src, void *dst);
void swap_shorts(int nmemb, unsigned short *x, unsigned short *y);
void swap_ints(int nmemb, unsigned int *x, unsigned int *y);
int rand_range(int, int);
void xsrand(void);
void strip_char(char *string, char char_to_strip);
char *last_token(char *string, char tok);
int is_bufzero(char *a, int sz);
void do_nothing(void);
