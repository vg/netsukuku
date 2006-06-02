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
 * misc.c: some useful functions.
 */

#include "includes.h"
#include <dirent.h>
#include <sys/wait.h>

#include "misc.h"
#include "log.h"
#include "xmalloc.h"

/*
 * * * *  Hash functions  * * * *
 */


/*		Ripped 32bit Hash function 
 *
 * Fowler/Noll/Vo hash
 *
 * See  http://www.isthe.com/chongo/tech/comp/fnv/index.html
 * for more details as well as other forms of the FNV hash.
 *
 ***
 *
 * Use the recommended 32 bit FNV-1 hash, pass FNV1_32_INIT as the
 * u_long hashval argument to fnv_32_buf().
 *
 ***
 *
 * Please do not copyright this code.  This code is in the public domain.
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * By:
 *	chongo <Landon Curt Noll> /\oo/\
 *      http://www.isthe.com/chongo/
 * Share and Enjoy!	:-)
 *
 * fnv_32_buf - perform a 32 bit Fowler/Noll/Vo hash on a buffer
 * `hval'	- previous hash value or 0 if first call
 * returns:
 *	32 bit hash as a static hash type
 */
u_long fnv_32_buf(void *buf, size_t len, u_long hval)
{
    u_char *bp = (u_char *)buf;	/* start of buffer */
    u_char *be = bp + len;		/* beyond end of buffer */

    /*
     * FNV-1 hash each octet in the buffer
     */
    while (bp < be) {

	/* multiply by the 32 bit FNV magic prime mod 2^32 */
	hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);

	/* xor the bottom with the current octet */
	hval ^= (u_long)*bp++;
    }

    /* return our new hash value */
    return hval;
}


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
 * xor_int: XORs all the bytes of the `i' integer by merging them in a single
 * byte. It returns the merged byte.
 */
char xor_int(int i)
{
        char c;

        c = (i & 0xff) ^ ((i & 0xff00)>>8) ^ ((i & 0xff0000)>>16) ^ ((i & 0xff000000)>>24);

        return c;
}

/* 
 * hash_time: As the name says: hash time!
 * This function generates the hash of the timeval struct which refer
 * to the current time. 
 * If h_sec or h_usec are not null, it stores in them respectively the hash of
 * the second and the microsecond.
 */
int hash_time(int *h_sec, int *h_usec)
{
	struct timeval t;
	char str[sizeof(struct timeval)+1];
	u_int elf_hash;
	
	gettimeofday(&t, 0);
	memcpy(str, &t, sizeof(struct timeval));
	str[sizeof(struct timeval)]=0;

	elf_hash=dl_elf_hash((u_char *)str);
	
	if(h_sec)
		*h_sec=inthash(t.tv_sec);
	if(h_usec)
		*h_usec=inthash(t.tv_usec);

	return inthash(elf_hash);
}



/*
 * * * *  Swap functions  * * * *
 */


/*
 * swap_array: swaps the elements of the `src' array and stores the result in
 * `dst'. The `src' array has `nmemb'# elements and each of them is `nmemb_sz'
 * big.
 */
void swap_array(int nmemb, size_t nmemb_sz, void *src, void *dst)
{
	int i, total_sz;
	
	total_sz = nmemb*nmemb_sz;
	
	char buf[total_sz], *z;
	
	if(src == dst)
		z=buf;
	else
		z=dst;
	
	for(i=nmemb-1; i>=0; i--)
		memcpy(z+(nmemb_sz*(nmemb-i-1)), (char *)src+(nmemb_sz*i),
				nmemb_sz);
			
	if(src == dst)
		memcpy(dst, buf, total_sz);
}

/*
 * swap_ints: Swap integers.
 * It swaps the `x' array which has `nmemb' elements and stores the result it 
 * in `y'.
 */
void swap_ints(int nmemb, unsigned int *x, unsigned int *y) 
{
	swap_array(nmemb, sizeof(int), x, y);
}

void swap_shorts(int nmemb, unsigned short *x, unsigned short *y)
{
	swap_array(nmemb, sizeof(short), x, y);
}



/*
 * * * *  Random related functions  * * * *
 */

/* 
 * rand_range: It returns a random number x which is _min <= x <= _max
 */ 
inline int rand_range(int _min, int _max)
{
	return (rand()%(_max - _min + 1)) + _min;
}

/* 
 * xsrand
 *
 * It sets the random seed with a pseudo random number 
 */
void xsrand(void)
{
	FILE *fd;
	int seed;

	if((fd=fopen("/dev/urandom", "r"))) {
		fread(&seed, 4,1,fd);
		fclose(fd);
	} else
		seed=getpid() ^ time(0) ^ clock();

	srand(seed); 
}


/*
 * * * *  String functions  * * * *
 */

char *last_token(char *string, char tok)
{
	while(*string == tok)
		string++;
	return string;
}

/*
 * strip_char: Removes any occurrences of the character `char_to_strip' in the
 * string `string'.
 */
void strip_char(char *string, char char_to_strip)
{
	int i; 
	char *p;
	for(i=0; i<strlen(string); i++) {
		if(string[i]==char_to_strip) {
			p=last_token(&string[i], char_to_strip);
			strcpy(&string[i], p);
		}
	}
}



/*
 * * * *  Search functions  * * * *
 */

/*
 * split_string: splits the `str' strings at maximum in `max_substrings'#
 * substrings using as divisor the `div_str' string.
 * Each substring can be at maximum of `max_substring_sz' bytes.
 * The array of malloced substrings is returned and in `substrings' the number
 * of saved substrings is stored.
 * On error 0 is the return value.
 */
char **split_string(char *str, const char *div_str, int *substrings, 
		int max_substrings, int max_substring_sz)
{
	int i=0, strings=0, str_len=0, buf_len;
	char *buf, **splitted=0, *p;

	*substrings=0;
	
	str_len=strlen(str);

	buf=str-1;
	while((buf=strstr((const char *)buf+1, div_str)))
		strings++;
	if(!strings && !str_len)
		return 0;

	strings++;
	if(strings > max_substrings)
		strings=max_substrings;

	splitted=(char **)xmalloc(sizeof(char *)*strings);
	
	buf=str;
	for(i=0; i<strings; i++) {
		p=strstr((const char *)buf, div_str);
		if(p)
			*p=0;

		buf_len=strlen(buf);
		if(buf_len <= max_substring_sz && buf_len > 0)
			splitted[i]=xstrdup(buf);
		else {
			i--;
			strings--;
			buf=p+1;
		}

		if(!p) {
			i++;
			break;
		}
		buf=p+1;
	}
	
	if(i != strings)
		splitted=(char **)xrealloc(splitted, sizeof(char *)*i);

	*substrings=strings;
	return splitted;
}

/*
 * If `x' is present in the `ia' array, which has `nmemb' members, it returns
 * 1, otherwise 0.
 */
int find_int(int x, int *ia, int nmemb)
{
	int e;
	
	for(e=0; e<nmemb; e++)
		if(ia[e] == x)
			return 1;

	return 0;
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


/*
 *  *  *  *  Time functions  *  *  *  *
 */


/*
 * xtimer: It sleeps for `secs' seconds. 
 * `steps' tells to xtimer() how many "for" cycles it must run to sleep for
 * `secs' secons. At each cycle it updates `counter'.
 */
void xtimer(u_int secs, u_int steps, int *counter)
{
	static int i, ms_sleep;
	static int ms_step; /* how many ms it must sleep at every step */
	static int s_step;  /* seconds per step */
	static int mu_step; /* micro seconds per step */

	if(!steps)
		steps++;
	if(counter)
		*counter=0;
	ms_sleep=secs*1000;
	
	if(ms_sleep < secs) {
		/* We are waiting a LONG TIME, it's useless to have steps < of
		 * one second */
		ms_step=1000;
		steps=secs;
	} else {
		ms_step=ms_sleep/steps;
		
		if(!ms_step) {
			ms_step=1;
			steps=ms_sleep;
		}
	}

	s_step=ms_step/1000;
	mu_step=ms_step*1000;
	
	for(i=0; i<steps; i++) {
		if(ms_step >= 1000)
			sleep(s_step);
		else
			usleep(mu_step);
		if(counter)
			(*counter)++;
	}
}


/*
 *  *  *  *  File & Dir related functions  *  *  *  *
 */


/*
 * check_and_create_dir: tries to access in the specified `dir' directory and
 * if doesn't exist tries to create it.
 * On success it returns 0
 */
int check_and_create_dir(char *dir)
{
	DIR *d;
	
	/* Try to open the directory */
	d=opendir(dir);
	
	if(!d) {
		if(errno == ENOENT) {
			/* The directory doesn't exist, try to create it */
			if(mkdir(dir, S_IRUSR|S_IWUSR|S_IXUSR) < 0) {
				error("Cannot create the %d directory: %s", dir,
						strerror(errno));
				return -1;
			}
		} else {
			error("Cannot access to the %s directory: %s", dir, 
					strerror(errno));
			return -1;
		}
	}
	
	closedir(d);
	return 0;
}

/* 
 * file_exist
 *
 * returns 1 if `filename' is a valid existing file.
 */
int file_exist(char *filename)
{
	FILE *fd;

	if(!(fd=fopen(filename, "r")))
		return !(errno == ENOENT);
	fclose(fd);

	return 1;
}

/*
 * exec_root_script: executes `script' with the given `argv', but checks first if the
 * script is:
 * 	- suid
 * 	- it isn't owned by root
 * 	- it isn't writable by others than root
 * If one of this conditions is true, the script won't be executed.
 * On success 0 is returned.
 */
int exec_root_script(char *script, char *argv)
{
	struct stat sh_stat;
	int ret;
	char command[strlen(script)+strlen(argv)+2];
	
	if(stat(script, &sh_stat)) {
		error("Couldn't stat %s: %s", strerror(errno));
		return -1;
	}

	if(sh_stat.st_uid != 0 || sh_stat.st_mode & S_ISUID ||
	    sh_stat.st_mode & S_ISGID || 
	    (sh_stat.st_gid != 0 && sh_stat.st_mode & S_IWGRP) ||
	    sh_stat.st_mode & S_IWOTH) {
		error("Please adjust the permissions of %s and be sure it "
			"hasn't been modified.\n"
			"  Use this command:\n"
			"  chmod 744 %s; chown root:root %s",
			script, script, script);
		return -1;
	}
	
	sprintf(command, "%s %s", script, argv);
	loginfo("Executing \"%s\"", command);
	
	ret=system(command);
	if(ret == -1) {
		error("Couldn't execute %s: %s", script, strerror(errno));
		return -1;
	}
	
	if(!WIFEXITED(ret) || (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)) {
		error("\"%s\" didn't terminate correctly", command);
		return -1;
	}

	return 0;
}
	
	
/* This is the most important function */
void do_nothing(void)
{
	return;
}
