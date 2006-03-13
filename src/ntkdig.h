#ifndef NTK_DIG_H
#define NTK_DIG_H

#include <errno.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define VERSION			"0.1"

#define NTKDIG_PORT		53
#define NTKDIG_PORT_STR		"53"
#define MAX_NS			3
#define LOCALHOST		"localhost"
#define ANDNS_MAX_SZ    1024

#define min(x,y)		(x)<(y)?(x):(y)

#define REALM_NTK		0
#define REALM_INT		1
#define REALM_NTK_STR		"ntk"
#define REALM_INT_STR		"inet"

#define PROTO_ANDNS		0
#define PROTO_DNS		1
#define PROTO_ANDNS_STR		"andns"
#define PROTO_DNS_STR		"dns"

#define QTYPE_A			0
#define QTYPE_PTR		1
#define QTYPE_MX		2
#define QTYPE_MXPTR		3
#define QTYPE_A_STR		"a"
#define QTYPE_MX_STR		"mx"
#define QTYPE_PTR_STR		"ptr"
#define QTYPE_MXPTR_STR		"mxptr"

/* NK BIT */
#define NK_NTK                  1
#define NK_INET                 2

char *QTYPE_STR_LIST[]={QTYPE_A_STR,QTYPE_PTR_STR,QTYPE_MX_STR,QTYPE_MXPTR_STR};
int QT_LEN=4;
int QTP_LEN=4;

#define QTFROMPREF(s)							\
({									\
 	int __n,__res=-1;						\
	for (__n=0;__n<QT_LEN;__n++) 					\
		if (!strncasecmp(s,QTYPE_STR_LIST[__n],strlen(s))) 	\
 			{__res=__n;break;}				\
	__res; })			
		
			
typedef struct ntkdig_opts {
	struct sockaddr_in	ns[MAX_NS];
	uint8_t			ns_len;
	uint8_t			ns_lhost;
	uint8_t			qt;
	uint8_t			pt;
	uint8_t			realm;
	uint8_t			silent;
} ntkdig_opts;

#define NTKDIG_OPTS_SZ	sizeof(ntkdig_opts)

ntkdig_opts globopts;


#endif /* NTK_DIG_H */
