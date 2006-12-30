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

/*
 * TODO: y=f(x)=4*x^2
 * 	 radar.c won't use anymore RTT_DELTA, it will just observes the
 * 	 changes of rtt8_t
 */
typedef uint8_t rtt8_t;
typedef uint32_t rtt32_t;

/*
 * TODO:
 * y=f(x)=int(x/32+2)^2*x^int(x/128+2)+1; }
 *
 */
typedef uint8_t bw8_t;
typedef uint32_t bw32_t;


/*
 * TODO: define f(rtt, upbw, dwbw)
 */
typedef uint8_t  avg8_t;
typedef uint32_t avg32_t;


/*
 * TODO: 
 * 	 - link_id
 * 	 - REM
 */


typedef struct
{
	rtt_t		rtt;		/* Round trip time in ms */

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
enum REM_indexes {
	REM_IDX_RTT=0,
	REM_IDX_UPBW,
	REM_IDX_DWBW,
	REM_IDX_AVG
};
