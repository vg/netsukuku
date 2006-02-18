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
	
};

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

