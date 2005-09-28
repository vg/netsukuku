/*
 *  Copyright (C) 2004-2005 Alo Sarv <madcat_@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ENDIAN_H__
#define __ENDIAN_H__

/**
 * \file endian.h
 * Defines global macros for byte-swapping. Whenever available, headers provided
 * by host platform are used, but falling back to generic code when none are
 * available.
 */

#include <byteswap.h>

#define SWAP16_ALWAYS(x) bswap_16(x)
#define SWAP32_ALWAYS(x) bswap_32(x)
#define SWAP64_ALWAYS(x) bswap_64(x)

// We don't want these to be defined
#ifdef LITTLE_ENDIAN
	#undef LITTLE_ENDIAN
#endif
#ifdef BIG_ENDIAN
	#undef BIG_ENDIAN
#endif

#if defined (__sparc__) || defined (__alpha__) || defined (__PPC__) \
|| defined (__mips__) || defined (__ppc__)
	#define __BIG_ENDIAN__
#endif

/**
 * Endianess identifier
 */
enum {
	LITTLE_ENDIAN = false,
	BIG_ENDIAN = true,
#ifdef __BIG_ENDIAN__
	HOST_ENDIAN = BIG_ENDIAN      //!< Host is big_endian system
#else
	HOST_ENDIAN = LITTLE_ENDIAN   //!< Host is little_endian system
#endif
};


#if defined(__BIG_ENDIAN__)
	#define SWAP16_ON_BE(val) SWAP16_ALWAYS(val)
	#define SWAP32_ON_BE(val) SWAP32_ALWAYS(val)
	#define SWAP64_ON_BE(val) SWAP64_ALWAYS(val)
	#define SWAP16_ON_LE(val) (val)
	#define SWAP32_ON_LE(val) (val)
	#define SWAP64_ON_LE(val) (val)
#else
	#define SWAP16_ON_BE(val) (val)
	#define SWAP32_ON_BE(val) (val)
	#define SWAP64_ON_BE(val) (val)
	#define SWAP16_ON_LE(val) SWAP16_ALWAYS(val)
	#define SWAP32_ON_LE(val) SWAP32_ALWAYS(val)
	#define SWAP64_ON_LE(val) SWAP64_ALWAYS(val)
#endif

#endif /* __ENDIAN_H__ */
