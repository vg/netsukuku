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

char *QTYPE_STR_LIST[]={QTYPE_A_STR,QTYPE_PTR_STR,\
			QTYPE_MX_STR,QTYPE_MXPTR_STR};
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
	int8_t			ns_len;
	int8_t			ns_lhost;
	int8_t			port;
	int8_t			qt;
	int8_t			pt;
	int8_t			realm;
	int8_t			silent;
	char*			question;
} ntkdig_opts;

#define NTKDIG_OPTS_SZ	sizeof(ntkdig_opts)

ntkdig_opts globopts;


#define QR_STR(ap)	((ap)->qr==0)?"QUERY":"ANSWER"
#define QTYPE_STR(ap)						\
({								\
 	char *__c;						\
 	switch((ap)->qtype) {					\
 		case AT_A:					\
			__c="Host2Ip";				\
			break;					\
 		case AT_PTR:					\
			__c="Ip2Host";				\
			break;					\
 		case AT_MX:					\
			__c="Host2MX";				\
			break;					\
 		case AT_MXPTR:					\
			__c="Ip2MX";				\
			break;					\
		default:					\
			__c="Unknow";				\
 			break;					\
			}					\
		__c;})						
#define NK_STR(ap)						\
({								\
	char *__d;						\
	switch((ap)->nk) {					\
		case NK_OLDSTYLE:				\
			__d="DNS";				\
			break;					\
		case NK_NTK:					\
			__d="ANDNA";				\
			break;					\
		case NK_INET:					\
			__d="ANDNA";				\
			break;					\
		default:					\
			__d="UNKNOW";				\
 			break;					\
			}					\
 		__d;})						

#define RCODE_STR(ap)						\
({								\
 	char *__e;						\
	switch((ap)->rcode) {					\
		case RCODE_NOERR:				\
			__e="NOERR";				\
			break;					\
		case RCODE_EINTRPRT:				\
			__e="INTRPRT";				\
			break;					\
		case RCODE_ESRVFAIL:				\
			__e="SRFFAIL";				\
			break;					\
		case RCODE_ENSDMN:				\
			__e="NOXHOST";			\
			break;					\
		case RCODE_ENIMPL:				\
			__e="NOTIMPL";				\
			break;					\
		case RCODE_ERFSD:				\
			__e="REFUSED";				\
			break;					\
		default:					\
			__e="UNKNOW";				\
			break;					\
	}							\
	__e;})



void print_usage();
void print_version();
void init_opts();
int opt_set_ns(char *s,int limit);
int opt_set_qtype(char *s);
int opt_set_ptype(char *s);
int opt_set_realm(char *s);
int ns_init(const char *hostname,int nslimit);
int ask_query(char *q,int qlen,char *an,int *anlen,struct sockaddr_in *saddr);
void print_question(andns_pkt *ap);
void print_answer_name(andns_pkt_data *apd);
void print_answer_addr(andns_pkt_data *apd);
andns_pkt* andns_pkt_from_opts();
int do_command();
int handle_answer(char *answ,int alen);
int main(int argc,char **argv);
int imain(int argc,char **argv);

#endif /* NTK_DIG_H */
