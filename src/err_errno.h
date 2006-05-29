                 /**************************************
                *     AUTHOR: Federico Tomassini        *
               *     Copyright (C) Federico Tomassini    *
              *     Contact effetom@gmail.com             *
             ***********************************************
               *****                                ******
*************************************************************************
*                                                                       *
*  This program is free software; you can redistribute it and/or modify *
*  it under the terms of the GNU General Public License as published by *
*  the Free Software Foundation; either version 2 of the License, or    *
*  (at your option) any later version.                                  *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
************************************************************************/


#ifndef ERR_ERRNO_H
#define ERR_ERRNO_H

#include <errno.h>
#include <string.h>
#include <stdlib.h>

static const char *err_strings[] = {
	"UFO error -o-",			/* ERR_UFOERR */
	"Malformed Label Octet.",		/* ERR_DNSMLO */
	"Malformed Sequence Label.",		/* ERR_DNSMSL */
	"Malformed Dns Packet.",		/* ERR_DNSMDP */
	"Malformed Dns Data.",			/* ERR_DNSMDD */
	"Too many Recursive Pointers.",		/* ERR_DNSTRP */
	"Dns Packet Len Break.",		/* ERR_DNSPLB */
	"Pointer To Pointer error.",		/* ERR_DNSPTP */
	"Malformed Data.",			/* ERR_DNSMDA */
	"Error Packing Dns Struct.",		/* ERR_DNSPDS */
/**/	
	"Malformed Andna Packet.",		/* ERR_ANDMAP */
	"Andns Packet Len Break.",		/* ERR_ANDPLB */
	"Malformed Andns Data.",		/* ERR_ANDMAD */
	"Andna Not Compatbile Query.", 		/* ERR_ANDNCQ */
/**/
	"Error reading resolv.conf.",		/* ERR_RSLERC */
	"Andns init error.",			/* ERR_RSLAIE */
	"There isn't No NameServer.",		/* ERR_RSLNNS */
	"Error Forwarding DNS Query.",		/* ERR_RSLFDQ */
	"Resolution Error.",			/* ERR_RSLRSL */
	"Andns Query Discarded.", 		/* ERR_RSLAQD */
/**/
	"mark_init error!.",			/* ERR_NETINI */
	"netfilter table not loadable.",	/* ERR_NETFIL */
	"error adding netfilter rules.",	/* ERR_NETRUL */
	"error committing netfilter rules.",	/* ERR_NETCOM */
	"error initializing ntk_mark_chain.",	/* ERR_NETCHA */
	"netfilter delete error.",		/* ERR_NETDEL */
	"error storing rules.",			/* ERR_NETSTO */
	"Nefilter was not restored.",		/* ERR_NETRST */
/**/	
	"SNSD main record not found.",		/* ERR_SNDMRF */
	"SNSD recursion failed.",		/* ERR_SNDRCS */
/**/	
	"Zlib Compression Fail.",		/* ERR_ZLIBCP */
	"Zlib Uncompression Fail.",		/* ERR_ZLIBUP */
	"Zlib compression is unuseful.",	/* ERR_ZLIBNU */
};

#define ERR_UFOERR	-1
#define ERR_DNSMLO	-2
#define ERR_DNSMSL	-3
#define	ERR_DNSMDP	-4
#define ERR_DNSMDD	-5
#define ERR_DNSTRP	-6
#define ERR_DNSPLB	-7
#define ERR_DNSPTP	-8
#define ERR_DNSMDA	-9
#define ERR_DNSPDS	-10

#define ERR_ANDMAP	-11
#define ERR_ANDPLB	-12
#define ERR_ANDMAD	-13
#define ERR_ANDNCQ	-14

#define ERR_RSLERC	-15
#define ERR_RSLAIE	-16
#define ERR_RSLNNS	-17
#define ERR_RSLFDQ	-18
#define ERR_RSLRSL	-19
#define ERR_RSLAQD	-20

#define ERR_MRKINI	-21
#define ERR_NETFIL	-22
#define ERR_NETRUL	-23
#define ERR_NETCOM	-24
#define ERR_NETCHA	-25
#define ERR_NETDEL	-26
#define ERR_NETSTO	-27
#define ERR_NETRST	-28

#define ERR_SNDMRF	-29
#define ERR_SNDRCS	-30

#define ERR_ZLIBCP	-31
#define ERR_ZLIBUP	-32
#define ERR_ZLIBNU	-33

#define ERR_OVERFLOW    "Error number does not exist."

        /* END OF DEFS */


 /*
  * Core
  */
const char *err_func,*err_file;
#define ERR_NERR                sizeof(err_strings)/sizeof(char*)
#define err_seterrno(n)         errno=(n);err_func=__func__;	\
                                err_file=__FILE__
#define err_ret(n,ret)		{err_seterrno(n);return ret;}
#define err_intret(n)           {err_seterrno(n);return -1;}
#define err_voidret(n)          {err_seterrno(n);return NULL;}
#define __err_strerror(n)                                       \
({                                                              \
        int __n=-((n)+1);                                       \
        (__n>=ERR_NERR || __n<0)?                               \
                ERR_OVERFLOW:                                   \
                err_strings[__n];                               \
})
#define err_strerror(e)                                         \
        ((e)>=0)?                                               \
                strerror(e):                                    \
                __err_strerror(e)
#define ERR_FORMAT      "In %s(): %s() returns -> %s"
#define err_str         ERR_FORMAT,__func__,                    \
                        err_func,__err_strerror(errno)

#endif /* ERR_ERRNO_H */

