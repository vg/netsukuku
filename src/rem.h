/* This file is part of Netsukuku
 * (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
 * rem.h
 *
 * Route Efficiency Measure
 */

#ifndef REM_H
#define REM_H

/*
 * Round Trip time
 * ---------------
 *
 * We can save the rtt, in millisec, using an unsigned integer of 8 bit or 32.
 * The 32 bit integer `y' is used when an accurate precision is needed or when
 * we just need to express the rtt in ms.
 * The 8 bit integer `x' is used when we don't want to waste memory.
 * The two are bounded by this law:
 *
 * 	y = 4*x^2
 * 
 * Note: a value stored in x isn't expressed in ms.
 *
 * You can use the :rem_rtt_8to32: and :rem_rtt_32to8: functions to convert a
 * value in the preferred format.
 *
 * The maximum rtt value is 4*255^2 ms = 260100 ms = 260 s
 */
typedef uint8_t rtt8_t;
typedef uint32_t rtt32_t;

/*
 * Bandwidth
 * ---------
 * 
 * We can save the bandwidth, in Kb/s, using an unsigned integer of 8 bit or 32.
 * The 32 bit integer `y' is used when an accurate precision is needed or when
 * we just need to express the bandwidth in Kb/s.
 * The 8 bit integer `x' is used when we don't want to waste memory.
 * The two are bounded by this law:
 *
 * 	y = f(x) = int(x/32+2)^2*x^int(x/128+2)+1
 *
 * Where the int(x) function returns the integer part of x.
 *
 * Note: a value stored in x isn't expressed in Kb/s.
 * 
 * You can use the :rem_bw_8to32: and :rem_bw_32to8: functions to convert a
 * value in the preferred format.
 *
 * The maximum bw value is f(255) = 1343091376 Kb/s = 1.25 Tb/s
 */
typedef uint8_t bw8_t;
typedef uint32_t bw32_t;


/*
 * TODO: define f(rtt, upbw, dwbw)
 */
typedef uint8_t  avg8_t;
typedef uint32_t avg32_t;


/*
 * rem_t
 * -----
 *
 * Route Efficiency Measure structure.
 * It is used to save the quality of a link or route.
 */
typedef struct
{
	rtt8_t		rtt;		/* Round trip time in ms */

	bw8_t		upbw;		/* Upload bandwidth */
	/* TODO: remember the bottleneck */
	bw8_t		dwbw;		/* Download */

	avg8_t		avg;		/* An ad-hoc average of the
					   previous metrics */
} rem_t;

#define REM_METRICS	4		/* Number of REM metrics, i.e. number 
					   of `rem_t' elements */

/*
 * Indexes of the various metrics placed in rem_t
 */
typedef int metric_t;
enum REM_indexes {
	REM_IDX_RTT=0,
	REM_IDX_UPBW,
	REM_IDX_DWBW,
	REM_IDX_AVG
};


/*\
 *  * * *  Exported functions  * * *
\*/

rtt32_t rem_rtt_8to32(rtt8_t x);
rtt8_t rem_rtt_32to8(rtt32_t y);

bw32_t rem_bw_8to32(bw8_t x);
bw8_t rem_bw_32to8(bw32_t y);

int rem_bw_cmp(bw32_t a, bw32_t b);
int rem_rtt_cmp(rtt32_t a, rtt32_t b);
int rem_avg_cmp(rem_t a, rem_t b);
int rem_metric_cmp(rem_t a, rem_t b, metric_t metric);

#endif /* REM_H */
