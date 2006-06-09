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

#ifndef MISC_H
#define MISC_H

/*
 * NMEMB: returns the number of members of the `x' array
 */
#define NMEMB(x)        	(sizeof((x))/sizeof(typeof((x)[0])))

/*
 * In bzero(3):
 * <<4.3BSD.  This function [bzero] is deprecated -- use memset in new 
 *   programs.>>
 */
#define setzero(_p, _sz)	memset((_p), 0, (_sz))

/*
 * memput
 *
 * It copies `__sz' bytes from `__src' to `__dst' and then increments the `__dst'
 * pointer of `__sz' bytes.
 *
 * *WARNING* 
 * Do NOT put expression in `__dst', and `__sz', f.e.
 * 	*WRONG CODE*
 * 	memput(buf++, src, (size+=sizeof(src));
 *
 * 	*CORRECT CODE*
 * 	buf++; size+=sizeof(src);
 * 	memput(buf, src, size);
 * *WARNING*
 */
#define memput(__dst, __src, __sz)					\
({ 									\
	void *_bufret=memcpy((__dst), (__src), (__sz));			\
	(__dst)+=(__sz);						\
	_bufret;							\
})

/*
 * memget
 *
 * the same of memput(), but it increments `__src' instead.
 */
#define memget(__dst, __src, __sz)					\
({ 									\
	void *_bufret=memcpy((__dst), (__src), (__sz));			\
	(__src)+=(__sz);						\
	_bufret;							\
})

/* use of hardcoded `_src' and `_dst' variable names */
#define bufput(_src, _sz)	(memput(buf, (_src), (_sz)))
#define bufget(_dst, _sz)	(memget((_dst), buf, (_sz)))

/* 
 * MILLISEC: converts a timeval struct to a int. The time will be returned in
 * milliseconds.
 */
#define MILLISEC(x)		(((x).tv_sec*1000)+((x).tv_usec/1000))

/*
 * MILLISEC_TO_TV: Converts `x', which is an int into `t', a timeval struct
 */
#define MILLISEC_TO_TV(x,t) 						\
do{									\
	(t).tv_sec=(x)/1000; 						\
	(t).tv_usec=((x) - ((x)/1000)*1000)*1000; 			\
}while(0)

#define FNV_32_PRIME ((u_long)0x01000193)
#define FNV1_32_INIT ((u_long)0x811c9dc5)

/* 
 * Bit map related macros.  
 */
#define SET_BIT(a,i)     ((a)[(i)/CHAR_BIT] |= 1<<((i)%CHAR_BIT))
#define CLR_BIT(a,i)     ((a)[(i)/CHAR_BIT] &= ~(1<<((i)%CHAR_BIT)))
#define TEST_BIT(a,i)    (((a)[(i)/CHAR_BIT] & (1<<((i)%CHAR_BIT))) ? 1 : 0)

/*
 * FIND_PTR:
 * Given an array of pointers `a' of `n' members, it searches for a member
 * equal to the pointer `p'. If it is found its position is returned,
 * otherwise -1 is the value returned.
 */
#define FIND_PTR(p, a, n)						\
({									\
 	int _i, _ret;							\
									\
	for(_i=0, _ret=-1; _i<(n); _i++)				\
		if((a)[_i] == (p)) {					\
			_ret=_i;					\
			break;						\
		}							\
	_ret;								\
})

/* 
 *  *  Functions declaration  *  *
 */
u_long fnv_32_buf(void *buf, size_t len, u_long hval);
unsigned int inthash(unsigned int key);
inline unsigned int dl_elf_hash (const unsigned char *name);
char xor_int(int i);
int hash_time(int *h_sec, int *h_usec);

void swap_array(int nmemb, size_t nmemb_sz, void *src, void *dst);
void swap_ints(int nmemb, unsigned int *x, unsigned int *y) ;
void swap_shorts(int nmemb, unsigned short *x, unsigned short *y);

inline int rand_range(int _min, int _max);
void xsrand(void);

char *last_token(char *string, char tok);
void strip_char(char *string, char char_to_strip);
char **split_string(char *str, const char *div_str, int *substrings,
		int max_substrings, int max_substring_sz);

		
int find_int(int x, int *ia, int nmemb);
int is_bufzero(const void *a, int sz);

void xtimer(u_int secs, u_int steps, int *counter);

int check_and_create_dir(char *dir);
int file_exist(char *filename);
int exec_root_script(char *script, char *argv);

void do_nothing(void);

#endif /*MISC_H*/
