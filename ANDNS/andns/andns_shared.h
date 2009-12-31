/*
 * (c) Copyright 2006, 2007 Federico Tomassini aka efphe <effetom@gmail.com>
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


#ifndef ANDNS_SHARED_H
#define ANDNS_SHARED_H

#include "andns.h"
#include "andns_lib.h"
#include "andns_net.h"

void andns_set_error(const char *err, andns_query *q);
void hstrcpy(char *d, char *s);
int andns_set_ntk_hname(andns_query *q, andns_pkt *p);
int andns_set_question(andns_query *q, andns_pkt *p);
int andns_dialog(andns_query *q, andns_pkt *ap);

#endif
