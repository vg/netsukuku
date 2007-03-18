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
