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
        "sequence label malformed.",
        "sequence label too long.",
	"andns packet too long.",
	"malformed dns octet.",
	"too many pointers",
	"pointer outlaw.",
	"pointer to pointer.",
	"hname too long.",
	"ufo error!",
	"PTR query without suffix.",
	"PTR address malformed.",
	"hname dns incompatible.",
	"pkt_len breaked!",
	"question section too long.",
	"pkt questions unreadable.",
	"rdlength breaked.",
	"pkt A-sections unreadable.",
	"question name can not be packed.",
	"A-section rdata unwritable.",
	"question section can not be packed.",
	"A-sections can not be packed.",
	"andns packet too small.",
	"andns questions unreadable.",
	"andns answers unreadable.",
	"andns struct can not be packed.",
	"too many name servers.",
	"nameserver 127.0.0.1 discarded",
	"unstorable nameserver.",
	"invalid nameserver address.",
	"can not read resolv.conf error.",
	"no nameserver found.",
	"DNS forwarding disable.",
	"discarding andns packet.",
	"can not interpret andns packet",
	"processing andns pkt failed.",
	"malformed andns packet.",
	"nameservers unreachable",
	"error sending andns pkt.",
	"error receiving andns pkt.",
	"can not make andns connection.",
	"problem forwarding andns packet.",
	"andns_gethostbyname failed.",
	"dns PTR query resolving failed.",
	"andns A query resolving failed.",
	"andns PTR query resolving failed.",
	"dns A query resolving failed."
	
};

#define ERR_ESLBLM      	-1
#define ERR_ELBLEX		-2
#define ERR_EPKTEX		-3
#define ERR_OCTETM		-4
#define ERR_MAXPTR		-5
#define ERR_PTROUT		-6
#define ERR_PTRPTR		-7
#define ERR_HNAMEX		-8
#define	ERR_UFOERR		-9
#define ERR_PQWOSX		-10
#define ERR_PADDRM		-11
#define ERR_HNDNSM		-12
#define	ERR_PKTQST		-13
#define ERR_PKTLEN		-14
#define ERR_PKTQTS		-15
#define	ERR_ARDLEN		-16
#define ERR_PKTANS		-17
#define ERR_QSTPKT		-18
#define ERR_ARDATA		-19
#define ERR_QTSPKT		-20
#define ERR_ANSPKT		-21
#define ERR_APKLOW		-22
#define ERR_ANDQST		-23
#define ERR_ANDANS		-24
#define ERR_ANDPKT		-26
#define ERR_NSMAXX		-27
#define ERR_LOOPNS		-28
#define ERR_NSUNST		-29
#define ERR_NSINVL		-30
#define ERR_RSLCNF		-31
#define ERR_NONSFD		-32
#define ERR_DNSFWD		-33
#define ERR_PKTDCD		-34
#define ERR_ANDINT		-35
#define ERR_ANDFLT		-36
#define ERR_ANDMLF		-37
#define ERR_NSUNRC		-38
#define ERR_ANSUNR		-39
#define ERR_ANDSND		-40
#define ERR_ANDRCV		-41
#define ERR_ANDCCT		-42
#define ERR_ANDFWD		-43
#define ERR_GHBYNM		-44
#define ERR_DPTRRV		-45
#define ERR_A_A_RV		-46
#define ERR_APTRRV		-45
#define ERR_D_A_RV		-46

#define ERR_OVERFLOW    "Error number does not exist."

        /* END OF DEFS */




 /*
  * Core
  */
const char *err_func,*err_file;
#define ERR_NERR                sizeof(err_strings)/sizeof(char*)
#define err_seterrno(n)         errno=(n);err_func=__func__;	\
                                err_file=__FILE__
#define err_intret(n)           {err_seterrno(n);return -1;}
#define err_voidret(n)          {err_seterrno(n);return NULL;}
#define __err_strerror(n)                                       \
({                                                              \
        int __n=-((n)+1);                                       \
        __n>=ERR_NERR?                                          \
                ERR_OVERFLOW:                                   \
                err_strings[__n];                               \
})
#define err_strerror(e)                                         \
        ((e)>=0)?                                               \
                strerror(e):                                    \
                __err_strerror(e)
#define ERR_FORMAT      "In %s(): %s() returns -> %s\n"
#define err_str         ERR_FORMAT,__func__,                    \
                        err_func,__err_strerror(errno)

#endif /* ERR_ERRNO_H */

