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

/*
 * Howto:
 *      You have to define your own error string
 *      just below. After that:
 *
 *      - Include this file
 *      - err_seterrno(ERR_SOMEERROR) sets errno.
 *      - err_intret(ERR_SOMERROR) sets errno and returns -1
 *      - err_voidret(ERR_SOMERROR) sets errno and returns NULL
 *      - err_strerror returns string describing error
 *      - err_str is a classic const char *format.
 *
 *      Note: when errno is set by lib functions, we have  the
 *              following equivalence:
 *
 *              err_strerror(errno):=strerror(errno).
 *
 */

        /*
         * Define here your error strings
         * If you want, and probably you want, you
         * should define here your symbols too.
         * Note: the symbols must be 'minus'-ed!
         */

        /* BEGIN DEFS */

static const char *err_strings[] = {
	"UFO error -o-",			/* ERR_UFOERR */
	"Malformed dns packet.",		/* ERR_MLFDPK */
	"Malformed andns packet.",		/* ERR_MLFAPK */
	"Dns forwarding is disable.",		/* ERR_DNSFWD */
	"Processing dns packet: fail!.",	/* ERR_DPKPRS */
	"Processing andns packet: fail!.",	/* ERR_APKPRS */
	"Nameservers can not be rached.",	/* ERR_NSUNRC */
	"Packet length break.",			/* ERR_PKTLEN */
	"Query interpretetion error.",		/* ERR_QINTRP */
	"Query processing error.",		/* ERR_QPROCS */
	"Error packing dns structures.",	/* ERR_PKTDST */
	"Error packing andns structures.",	/* ERR_PKTAST */
	"Error translating dns to andns.",	/* ERR_DTOATR */
	"Andna resolution failed.",		/* ERR_ANDNAR */
	"Invalid hostname.",			/* ERR_HNINVL */
	"Unknow (or not impl.) query type.",	/* ERR_UFOTOQ */
	"mark_init error!.",			/* ERR_MRKINI */
	"netfilter table not loadable.",	/* ERR_NETFIL */
	"error adding netfilter rules.",	/* ERR_NETRUL */
	"error committing netfilter rules.",	/* ERR_NETCOM */
	"error initializing ntk_mark_chain.",	/* ERR_NETCHA */
	"netfilter delete error.",		/* ERR_NETDEL */
	"error storing rules.",			/* ERR_NETSTO */
	"Nefilter was not restored.",		/* ERR_NETRST */
};
#define ERR_UFOERR	-1
#define ERR_MLFDPK	-2
#define ERR_MLFAPK	-3
#define ERR_DNSFWD	-4
#define ERR_DPKPRS	-5
#define ERR_APKPRS	-6
#define ERR_NSUNRC	-7
#define ERR_PKTLEN	-8
#define ERR_QINTRP	-9
#define ERR_QPROCS	-10
#define ERR_PKTDST	-11
#define ERR_PKTAST	-12
#define ERR_DTOATR	-13
#define ERR_ANDNAR	-14
#define ERR_HNINVL	-15
#define ERR_UFOTOQ	-16
#define ERR_MRKINI	-17
#define ERR_NETFIL	-18
#define ERR_NETRUL	-19
#define ERR_NETCOM	-20
#define ERR_NETCHA	-21
#define ERR_NETDEL	-22
#define ERR_NETSTO	-23
#define ERR_NETRST	-23

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

