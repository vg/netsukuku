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
 */

#define MAX_INTS_PER_STRUCT	64		/* The maximum number of short/int variables 
						   present in a struct */

#define IINFO_DYNAMIC_VALUE	-1		/* This define is used to fill part in a 
						   int_info struct that must be set each time. 
						   If that part is not set, -1 will remain, and
						   the int_info functions will call fatal(). 
						   Therefore this is useful to track bugs. */


/* flags for int_info.int_type */
#define INT_TYPE_32BIT		1		/* The int var is of 32 bits */
#define INT_TYPE_16BIT		(1<<1)		/* The int var is of 16 bits */
#define INT_TYPE_NETWORK 	(1<<2)		/* The int var is stored in network order */

/*
 * int_info: this struct is used to keep the information about the int/short
 * variables present in a struct. It is useful to convert all the int/short
 * vars in another endian format with a simple function. 
 * WARNING: There is a drawback: the struct must have the __packed__
 * attribute (but since we are using this for packet structs we don't care).
 *
 * Here there is an example which show how to use this int_info:
 * 	
 * given the struct s:
 *	struct 
 *	{
 *		u_char a;
 *		int b;
 *		short c;
 *		char d[23];
 *		int e[4];
 *	}__attribute__ ((__packed__)) s;
 *
 * its int_info struct should be filled in this way:
 * 
 * int_info s_int_info = { 3,
 *  			   {INT_TYPE_32BIT, INT_TYPE_16BIT, INT_TYPE_32BIT}, 
 *			   { sizeof(char), sizeof(char)+sizeof(int), 
 *			   	sizeof(char)+sizeof(int)+sizeof(short)+sizeof(char)*23},
 *		           { 1, 1, 4 }
 *			 };
 */
typedef struct
{
	/* The total int/short vars present in the struct */
	int 	total_ints;				

	/* Each member in the int_type array corresponds to a int/short var
	 * and it is set using the above INT_TYPE_ flags */
	char 	int_type[MAX_INTS_PER_STRUCT];

	/* Each member in the int_offset array specifies the amount of bytes
	 * to be added at the end of the struct to get the relative int/short
	 * var. */
	size_t	int_offset[MAX_INTS_PER_STRUCT];

	/* int_nmemb[x] is equal to the number of consecutive ints/shorts var,
	 * which start at the int_offset[x] offset. */
	size_t	int_nmemb[MAX_INTS_PER_STRUCT];

} int_info;

void ints_network_to_host(void *s, int_info iinfo);
void ints_host_to_network(void *s, int_info iinfo);
void ints_printf(void *s, int_info iinfo, void(*print_func(const char *, ...)));
