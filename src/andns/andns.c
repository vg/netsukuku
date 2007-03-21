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



#include <stdlib.h>

#include "andns.h"
#include "andns_lib.h"
#include "andns_net.h"
#include "andns_shared.h"

andns_pkt *ntk_query(andns_query *query)
{
    andns_pkt *ap;

    ap= create_andns_pkt();

    if (andns_set_question(query, ap))
        return NULL;

    ap->qtype   = query->type;
    ap->nk      = query->realm;
    ap->p       = query->proto;
    ap->service = query->service;
    ap->r       = query->recursion;
    ap->id      = query->id;

    if (andns_dialog(query, ap))
        return NULL;

    return ap;
}

void free_andns_pkt(andns_pkt *ap)
{
    destroy_andns_pkt(ap);
    return;
}
