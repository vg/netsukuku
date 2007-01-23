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
 * rem.c
 *
 * Route Efficiency Measure routines
 */

#include "includes.h"
#include <math.h>

#include "rem.h"
#include "log.h"


/*
 * See {-rtt8_t-}
 */
rtt32_t rem_rtt_8to32(rtt8_t x)
{
	/* 
	 * y = 4*x^2 
	 */
	return (rtt32_t) ( (x*x) << 2 );
}

/*
 * The inverse of {-rem_rtt_8to32-}
 */
rtt8_t rem_rtt_32to8(rtt32_t y)
{
	if(y > 260100) /* > rem_rtt_8to32(255) */
		fatal(ERROR_MSG "Unsupported value", ERROR_FUNC);

	/* 
	 * x = sqrt( y/4 )
	 */
	return (rtt8_t) sqrt(y >> 2);
}

/*
 * See {-bw8_t-}
 */
bw32_t rem_bw_8to32(bw8_t x)
{
	/* 
	 * y = int(x/32+2)^2 * x^int(x/128+2)+1
	 *
	 * It is equivalent to:
	 *
	 * 	if  x < 32*1:
	 * 		y = 4*x^2 + 1
	 * 	if  x < 32*2:
	 * 		y = 9*x^2 + 1
	 * 	if  x < 32*3:
	 * 		y = 16*x^2 + 1
	 * 	if  x < 32*4:
	 * 		y = 25*x^2 + 1
	 *
	 * 	if  x < 32*5:
	 * 		y = 36*x^3 + 1
	 * 	if  x < 32*6:
	 * 		y = 49*x^3 + 1
	 * 	if  x < 32*7:
	 * 		y = 64*x^3 + 1
	 * 	if  x < 32*8:
	 * 		y = 81*x^3 + 1
	 * 
	 * And we stop here, because the maximum value of x is 255.
	 */

	return (bw32_t) ( ((x>>5)+2)*((x>>5)+2)*((int)pow(x, (x>>7)+2)) + 1 );
}

/*
 * The inverse of {-rem_bw_8to32-}
 */
bw8_t rem_bw_32to8(bw32_t y)
{
	/* 
	 * The int() function used in {-rem_bw_8to32-} creates 8 different
	 * cases when  x  is in [0,255]. We just need to take the inverse of
	 * each case.
	 *
	 * Let f(x)=rem_bw_8to32(x)
	 */

	if(y <= 3845) /* <= f(31) */
		/* x = sqrt( (y-1) / 4) */
		return (bw8_t) sqrt( (y-1) >> 2 );

	else if(y <= 35722) /* <= f(63) */ 
		return (bw8_t) sqrt( (y-1) / 9);

	else if(y <= 144401) /* <= f(95) */
		/* x = sqrt( (y-1) / 16) */
		return (bw8_t) sqrt( (y-1) >> 4);

	else if(y <= 403226) /* <= f(127) */ 
		return (bw8_t) sqrt( (y-1) / 25);
	
	else if(y <= 144708445) /* <= f(159) */
		return (bw8_t) cbrt( (y-1) / 36);

	else if(y <= 341425680) /* <= f(191) */
		return (bw8_t) cbrt( (y-1) / 49);

	else if(y <= 709732289) /* <= f(223) */
		return (bw8_t) cbrt( (y-1) / 64);
	
	else if(y <= 1343091376) /* <= f(255) */
		return (bw8_t) cbrt( (y-1) / 81);

	else
		fatal(ERROR_MSG "Unsupported value", ERROR_FUNC);

	return 0; /* Shut up the compiler */
}

/*
 * rem_bw_cmp
 * -----------
 *
 * It returns an integer less than, equal to, or greater than zero if `a'
 * is considered to be respectively less than, equal to, or greater than
 * `b'.
 */
int rem_bw_cmp(bw32_t a, bw32_t b)
{
        /*
         * a < b   -1	 -->  The bw `a' is worse  than `b'
         * a > b    1    -->  The bw `a' is better than `b'
         * a = b    0	 -->  They are the same
         */
	return (a > b) - (a < b);
}

/*
 * rem_rtt_cmp
 * -----------
 *
 * It returns an integer less than, equal to, or greater than zero if `a'
 * is considered to be respectively *greater* than, equal to, or *less* than
 * `b'.
 *
 * This is the reverse of the standard comparison. The rtt `b' is
 * considered more efficient if it is less than `a'!
 */
int rem_rtt_cmp(rtt32_t a, rtt32_t b)
{
        /*
         * a < b    1	 -->  The rtt `a' is better than `b'
         * a > b   -1    -->  The rtt `a' is worse  than `b'
         * a = b    0	 -->  They are the same
         */
	return (a < b) - (a > b);
}

/*
 * rem_avg_cmp
 * -----------
 */
int rem_avg_cmp(avg32_t a, avg32_t b)
{
	/*
	 * TODO: 
	 */
	fatal("CODE ME!!!");
}

/*
 * rem_metric_cmp
 * --------------
 *
 * Wrapper function. It does a comparison of `a' and `b' using the specified
 * `metric' (see {-metric_t-})
 *
 * It returns an integer less than, equal to, or greater than zero if `a'
 * is considered to be respectively  *more efficient*  than, equal to, 
 * or  *less efficient*  than `b'.
 *
 * {-fatal-} is called is no valid metric has been specified
 */
int rem_metric_cmp(rem_t a, rem_t b, metric_t metric)
{
	int ret;

	switch(metric) {
		case REM_IDX_RTT:
			ret=rem_rtt_cmp((rtt32_t)a.rtt, (rtt32_t)b.rtt);
			break;
		case REM_IDX_UPBW:
			ret=rem_bw_cmp((bw32_t)a.upbw, (bw32_t)b.upbw);
			break;
		case REM_IDX_DWBW:
			ret=rem_bw_cmp((bw32_t)a.dwbw, (bw32_t)b.dwbw);
			break;
		case REM_IDX_AVG:
			ret=rem_avg_cmp((avg32_t)a.avg, (avg32_t)b.avg);
			break;
		default:
			fatal(ERROR_MSG "Unknown metric %d\n",
					ERROR_POS, metric);
			break;
	}
	
	return ret;
}
