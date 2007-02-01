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
 * qspn.c
 *
 * Implementation of the Quantum Shortest Path Netsukuku v2 algorithm.
 * For more information on the Q^2 see {-CITE_Q2_DOC-}
 */


/* TODO: 
 * remember: 
 * The rnodes of the root_node of 0 level are updated by the radar(), 
 * instead the root_nodes of greater levels are updated by the qspn.
 */
