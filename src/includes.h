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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/time.h>

/*socket*/
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <linux/socket.h>
#include <netinet/ip.h>
#include <linux/in_route.h>

#include <net/if.h>

#include <time.h>
#include <asm/types.h>
#include <sys/types.h>

#include <netdb.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/ioctl.h>
#include <fcntl.h>

#include <gmp.h>
#include <pthread.h>

#include <linux/limits.h>
#include <signal.h>
#include <linux/byteorder/little_endian.h>


#define _PACKED_ __attribute__ ((__packed__))
