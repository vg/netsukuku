#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>

#include "ntkdig.h"
#include "xmalloc.h"
#include "andns_net.h"
#include "crypto.h"
#include "log.h"

static ntkdig_opts globopts;
static struct timeval time_start,time_stop;

void version(void)
{
        say("ntk-dig version %s (Netsukuku tools)\n\n"
            "Copyright (C) 2006.\n"
            "This is free software.  You may redistribute copies of it under the terms of\n"
            "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
            "There is NO WARRANTY, to the extent permitted by law.\n\n",VERSION);
	exit(1);
}


void usage(void)
{
        say("Usage:\n"
                "\tntk-dig [OPTIONS] host\n\n"
                " -v --version          print version, then exit.\n"
                " -n --nameserver=ns    use nameserver `ns' instead of localhost.\n"
                " -P --port=port        nameserver port, default 53.\n"
                " -t --query-type=qt    query type (default A).\n"
                " -r --realm=realm      inet or netsukuku (default) realm to scan.\n"
                " -s --service=service  SNSD service.\n"
                " -p --protocolo=proto  SNSD protocol (udp/tcp).\n"
                " -S --silent           ntk-dig will be not loquacious.\n"
                " -h --help             display this help, then exit.\n\n");
	exit(1);
}
void qt_usage(char *arg)
{
	say("Bad Query Type %s\n"
	    "Valid query types are:\n"
            " snsd\thost:port -> ip\n"
            " ptr\tip -> host\n"
            "(you can also use univoque abbreviation)\n\n",arg);
	exit(1);
}
void realm_usage(char *arg)
{
	say("Bad Realm %s\n"
	    "Valid realms are:\n"
            " ntk\tnetsukuku realm\n"
            " inet\tinternet realm\n"
            "(you can also use univoque abbreviation)\n\n",arg);
	exit(1);
}
void proto_usage(char *arg)
{
	say("Bad Protocol %s\n"
	    "Valid protocols are:\n"
            " udp\n"
            " tcp\n"
            "(you can also use univoque abbreviation)\n\n",arg);
	exit(1);
}

double diff_time(struct timeval a,struct timeval b)
{
        double res;
        res=(double)(b.tv_sec-a.tv_sec);
        if (b.tv_usec<a.tv_usec)
                res+=(100.0-a.tv_usec+b.tv_usec);
        else
                res+=(b.tv_usec-a.tv_usec);
	res/=TIME_SCALE;
        return res;
}


void opts_init(void)
{
	memset(&GOP,0,NTKDIG_OPTS_SZ);
	strcpy(GOP.nsserver,LOCALHOST);
	GOP.port=NTKDIG_PORT;
	GQT=create_andns_pkt();
	GQT->nk=REALM_NTK;
	srand((unsigned int)time(NULL));
}

void opts_set_silent(void)
{
	GOP.silent=1;
}

void opts_set_port(char *arg)
{
	int res;
	uint16_t port;

	res=atoi(arg);
	port=(uint16_t)res;

	if (port!=res) {
		say("Bad port %s.",arg);
		exit(1);
	}
	GOP.port=port;
}

void opts_set_ns(char *arg)
{
	int slen;

	slen=strlen(arg);
	if (slen>=MAX_HOSTNAME_LEN) {
		say("Server hostname too long.");
		exit(1);
	}
	strcpy(GOP.nsserver,arg);
	GOP.nsserver[slen]=0;
}

void opts_set_qt(char *arg)
{
	int res;

	res=QTFROMPREF(arg);
	if (res==-1) 
		qt_usage(arg);
	GQT->qtype=res;
}

void opts_set_realm(char *arg)
{
	uint8_t res;

	res=REALMFROMPREF(arg);
	if (!res) 
		realm_usage(arg);
	GQT->nk=res;
}
void opts_set_service(char *arg)
{
	int res;
	uint16_t service;

	res=atoi(arg);
	service=(uint16_t)res;

	if (service!=res) {
		say("Bad service %s.",arg);
		exit(1);
	}
	GQT->service=service;
}
void opts_set_proto(char *arg) 
{
	uint8_t p;

	p=PROTOFROMPREF(arg);
	if (p==2) 
		proto_usage(arg);
	GQT->p=p;
}
void hname_hash(char *dst,char *src)
{
	u_char hashm5[16];
	u_char *bp,*be;
	u_int hval=0;
	
	hash_md5(src, strlen(src), hashm5);
	bp = (u_char *)hashm5; 
        be = bp + 16;         
    	while (bp < be) {
        	hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
        	hval ^= (u_long)*bp++;
    	}
	hval=htonl(hval);
	memcpy(dst,&hval,ANDNS_HASH_H);
}

void opts_set_question(char *arg)
{
	struct in_addr ia;
	struct in6_addr i6a;
	int res;
	
	switch(GQT->qtype) {
		case QTYPE_A:
			if (GQT->nk==REALM_NTK) {
				G_ALIGN(ANDNS_HASH_H);
				hname_hash(GQT->qstdata,arg);
			} else {
				res=strlen(arg);
				if (res>255) {
					say("Hostname %s is too long for DNS standard.",arg);
					exit(1);
				}
				G_ALIGN(res+1);
				strcpy(GQT->qstdata,arg);
			}
			return;
		case QTYPE_PTR:
			res=inet_pton(AF_INET,arg,&ia);
			if (res) {
				G_ALIGN(ANDNS_HASH_H);
				memcpy(GQT->qstdata,&ia.s_addr,4);
				return;
			}
			res=inet_pton(AF_INET6,arg,&i6a);
			if (!res) {
				say("Bad address `%s'\n",arg);
				exit(1);
			}
			G_ALIGN(16);
			memcpy(GQT->qstdata,&i6a.in6_u,16);
			GQT->ipv=ANDNS_IPV6;
			return;
		default:
			say("Unknow Query Type.\n");
			return;
	}
}
void opts_finish(char *arg)
{
	int r;

	r=rand();
	GQT->id=r>>16;
	opts_set_question(arg);
}


void do_command(void)
{
	char buf[ANDNS_MAX_SZ];
	char answer[ANDNS_MAX_PK_LEN];
	size_t res;
	char *crow;
	struct in_addr ia;

	res=a_p(GQT,buf);
	if (res==-1) {
		say("Error building question.\n");
		exit(1);
	}
	res=hn_send_recv_close(GOP.nsserver,GOP.port,
			SOCK_DGRAM,buf,res,answer,ANDNS_MAX_PK_LEN,0);
	if (res==-1) {
		say("Communication failed with %s.\n",GOP.nsserver);
		exit(1);
	}
	say("RECV %d\n",res);
	res=a_u(answer,res,&GQT);
	if (res<=0) {
		say("Error interpreting server answer.\n");
		exit(1);
	}
	say("An: %d\n",GQT->ancount);
	say("RCODE : %s\n",RCODE_STR(GQT));
	memcpy(&(ia.s_addr),GQT->pkt_answ->rdata,4);
	crow=inet_ntoa(ia);
	say("ANSWER is %s .\n",crow);
	say("Ale\n");
}
int main(int argc, char **argv)
{
        int c;
        extern int optind, opterr, optopt;
        extern char *optarg;

	log_init("",5,1);
	gettimeofday(&time_start,NULL);

	opts_init();
	struct option longopts[]= {
                {"version",0,0,'v'},
                {"nameserver",1,0,'n'},
                {"port",1,0,'P'},
                {"query-type",1,0,'t'},
                {"realm",1,0,'r'},
                {"service",1,0,'s'},
                {"proto",1,0,'p'},
                {"silent",0,0,'S'},
                {"help",0,0,'h'},
                {0,0,0,0}
        };

	while(1) {
		int oindex=0;
		c=getopt_long(argc, argv, 
			"vn:P:t:r:s:p:Sh", longopts, &oindex);
		if (c==-1)
			break;
		switch(c) {
			case 'v':
				version();
			case 'n':
				opts_set_ns(optarg);
				break;
			case 'P':
				opts_set_port(optarg);
				break;
			case 't':
				opts_set_qt(optarg);
				break;
			case 'r':
				opts_set_realm(optarg);
				break;
			case 's':
				opts_set_service(optarg);
				break;	
			case 'p':
				opts_set_proto(optarg);
				break;
			case 'h':
				usage();
			case 'S':
				opts_set_silent();
				break;
			default:
				usage();
		}
	}
	if (optind==argc)
		usage();
	opts_finish(argv[optind]);
	do_command();
	time_report;
	bye;
	return 0;
}
