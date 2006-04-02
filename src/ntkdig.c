	         /**************************************
	        *     AUTHOR: Federico Tomassini        *
	       *     Copyright (C) Federico Tomassini    *
	      *     Contact effetom@gmail.com	          *
	     ***********************************************
	     *******          BEGIN 3/2006          ********
*************************************************************************
*                                              				* 
*  This program is free software; you can redistribute it and/or modify	*
*  it under the terms of the GNU General Public License as published by	*
*  the Free Software Foundation; either version 2 of the License, or	*
*  (at your option) any later version.					*
*									*
*  This program is distributed in the hope that it will be useful,	*
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	*
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*
*  GNU General Public License for more details.				*
*									*
************************************************************************/
#include <getopt.h>  
#include <stdio.h>  
#include <unistd.h>  
#include <fcntl.h>

#include "andnslib.h"
#include "ntkdig.h"
#include "log.h"
#include "err_errno.h"

static int n_answers=1;
static ntkdig_opts globopts;

void print_usage() 
{
	printf("Usage:\n" 
		"\tntkdig [OPTIONS] host\n\n"
		" -v --version		print version, then exit.\n"
		" -n --nameserver=ns	use nameserver `ns' instead of localhost.\n"
		" -p --port=port		nameserver port, default 53.\n"
		" -t --query-type=qt	query type (default A).\n"
		" -r --realm=realm	inet or netsukuku (default) realm to scan.\n"
		" -s --silent		ntkdig will be not loquacious.\n"
		" -h --help		display this help, then exit.\n\n");
}
void print_version()
{
	printf("ntkdig version %s (Netsukuku tools)\n\n",VERSION);
	printf("Copyright (C) 2006.\n"
		"This is free software.  You may redistribute copies of it under the terms of\n"
		"the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n\n"
		"Written by Federico Tomassini.\n");

}
void init_opts()
{
	memset(&globopts,0,NTKDIG_OPTS_SZ);
	globopts.port=htons(NTKDIG_PORT);
	/*res=ns_init(LOCALHOST,1);
	if (res) {
		printf("Internal error initializing options (is andna running on localhost?).\n");
		exit(1);
	}*/
	globopts.ns_len=0;
	globopts.ns_lhost=1;
	globopts.qt=QTYPE_A;
	globopts.pt=PROTO_ANDNS;
	globopts.realm=REALM_NTK;
}
	
int opt_set_ns(char *s,int limit)
{
	int res;
	res=ns_init(s,(limit)?1:MAX_NS);
	globopts.ns_lhost=0;
	return res;
}
int opt_set_qtype(char *s)
{
	int res;
	res=QTFROMPREF(s);
	globopts.qt=res;
	return res;
}
int opt_set_ptype(char *s)
{
	if (!strncasecmp(PROTO_ANDNS_STR,s,strlen(s)))
		globopts.realm=PROTO_ANDNS;
	else if (!strncasecmp(PROTO_DNS_STR,s,strlen(s)))
		globopts.realm=PROTO_DNS;
	else return -1;
	return 0;
}
int opt_set_realm(char *s)
{
	if (!strncasecmp(REALM_NTK_STR,s,strlen(s)))
		globopts.realm=REALM_NTK;
	else if (!strncasecmp(REALM_INT_STR,s,strlen(s)))
		globopts.realm=REALM_INT;
	else return -1;
	return 0;
}
int opt_set_port(char *s)
{
	int port;
	port=atoi(s);
	if (!port)
		return -1;
	globopts.port=htons(port);
	if (globopts.ns_len) 
		for (port=0;port<globopts.ns_len;port++)
			(globopts.ns+port)->sin_port=globopts.port;
	return 0;
}
int ns_init(const char *hostname,int nslimit)
{
	int res;
	struct addrinfo	*ailist,*aip,filter;

	memset(&filter,0,sizeof(struct addrinfo));
	filter.ai_family=AF_INET;
	filter.ai_socktype=SOCK_DGRAM;

	res=getaddrinfo(hostname,NTKDIG_PORT_STR,&filter,&ailist);
	if (res) {
		printf("Invalid address: %s\n",gai_strerror(errno));
		return -1;
	}
	res=min(MAX_NS,nslimit);
	for (aip=ailist;aip && globopts.ns_len<res;aip=aip->ai_next) {
		if (aip->ai_family!=AF_INET)
			continue;
		memcpy(globopts.ns+globopts.ns_len,aip->ai_addr,
				sizeof(struct sockaddr_in));
		(globopts.ns+globopts.ns_len)->sin_port=globopts.port;
		globopts.ns_len++;
	}
	if (!globopts.ns_len) {
		printf("No nameserver found for %s.",hostname);
		return -1;
	}
	//printf("Found %d nameservers.\n",globopts.ns_len);
	freeaddrinfo(ailist);
	return 0;
}

int ask_query(char *q,int qlen,char *an,int *anlen,struct sockaddr_in *saddr)
{
	int len,skt;
	skt=socket(PF_INET,SOCK_DGRAM,0);

	if (skt==-1) {
		printf("Internal error opening socket.\n");
		return -1;
	}
			
	/*if (fcntl(skt, F_SETFL, O_NONBLOCK) < 0) {
		printf("set_nonblock_sk(): cannot set O_NONBLOCK: %s",strerror(errno));
		goto close_return;
	}*/
	if ((connect(skt,(struct sockaddr*)saddr,sizeof(struct sockaddr_in)))) {
                printf("In ask_query: error connecting socket -> %s.",strerror(errno));
		goto close_return;
        }
	len=send(skt,q,qlen,0);
	if (len==-1) {
		printf("In ask_query: error sending pkt -> %s.\n",strerror(errno));
		goto close_return;
	}
	len=recv(skt,an,ANDNS_MAX_SZ,0);
	if (len==-1) {
		printf("In ask_query: error receiving pkt -> %s.\n",strerror(errno));
		goto close_return;
	}
	*anlen=len;
	return 0;
close_return:
	close(skt);
	return -1;
}

void print_question(andns_pkt *ap)
{
	printf("\n\t# Question Headers: #\n");
	printf("# id: %d\tqr: %s\tqtype: %s\n",ap->id,QR_STR(ap),QTYPE_STR(ap));
	printf("# answers: %d\tnk: %s\trcode: %s\n",ap->ancount,NK_STR(ap),RCODE_STR(ap));
	printf("# \t\tRealm: %s\n",GET_OPT_REALM);
}
void print_answer_name(andns_pkt_data *apd)
{
//	printf("\n\t# Answer Section: #\n");
	printf("~ Hostname:\t%s\n",apd->rdata);
}
void print_banner_answer()
{
	printf("\n\t# Answer Section: #\n");
}

void print_answer_addr(andns_pkt_data *apd)
{
	struct in_addr a;
	memcpy(&a,apd->rdata,sizeof(struct in_addr));
//	printf("\n\t# Answer Section %d: #\n",n_answers);
	n_answers++;
	printf("~ Ip Address:\t%s\n",inet_ntoa(a));
}
andns_pkt* andns_pkt_from_opts()
{
	andns_pkt *ap;

	ap=create_andns_pkt();
	ap->id=rand()>>16;
	ap->qtype=globopts.qt;
	ap->nk=(globopts.realm==REALM_NTK)?NK_NTK:NK_INET;
	ap->qstlength=strlen(globopts.question);
	memcpy(ap->qstdata,globopts.question,ap->qstlength);
	return ap;
}

int do_command()
{
	int res,msglen,answlen;
	andns_pkt *ap;
	char msg[ANDNS_MAX_SZ],answ[ANDNS_MAX_SZ];
	int i;

	printf("Quering for %s...\n",globopts.question);
	if (globopts.ns_lhost) {
		res=ns_init(LOCALHOST,1);
		if (res==-1) {
			printf("Where is the ANDNA server?.\n");
			exit(1);
		}
	}
	ap=andns_pkt_from_opts();
	msglen=a_p(ap,msg);
	if (msglen==-1) {
		printf("Internal error building packet.");
		exit(1);
	}
	res=0;
	for (i=0;i<globopts.ns_len;i++) {
		res=ask_query(msg,msglen,answ,&answlen,globopts.ns+i);
		if (res==-1)
			continue;
		else 
			break;
	}
	if (res==-1) {
		printf("Sending packet failed.\n");
		exit(1);
	}
	
	res=handle_answer(answ,answlen);
	if (res==-1) {
		printf(err_str);
		exit(1);
	}
	return 0;
}
int handle_answer(char *answ,int alen)
{
	int i;
	void (*printer)(andns_pkt_data *);
	int offset;
	andns_pkt *ap;
	andns_pkt_data *apd;

	offset=a_u(answ,alen,&ap);
	if (offset==-1) {
		printf("Answer interpretation error.\n");
		exit(1);
	}
	if (offset!=alen) 
		printf("Packet length differs from packet contents: %d vs %d.",alen,offset);
	if (ap->rcode!=ANDNS_RCODE_NOERR) {
		print_question(ap);
		exit(1);
	}

	if (!ap->ancount) {
		printf("Received answer contains no data.\n");
		exit(1);
	}
	print_question(ap);
	switch(ap->qtype) {
		case AT_A:
			printer=print_answer_addr;
			break;
		case AT_PTR || AT_MX || AT_MXPTR:
			printer=print_answer_name;
			break;
		default:
			printf("Unable to print answer");
			exit(1);
	}
	print_banner_answer();
	apd=ap->pkt_answ;
	for (i=0;i<ap->ancount;i++) {
		printer(apd);
		apd=apd->next;
	}
	destroy_andns_pkt(ap);
	return 0;
			
}

int imain(int argc,char **argv)
{
	andns_pkt *ap;
	andns_pkt_data *apd;
	char a[1024];
	int o;
	
	log_init(argv[0],0,1);
	memset(&globopts,0,sizeof(ntkdig_opts));
	ap=create_andns_pkt();
	ap->id=rand()>>16;
	ap->qtype=AT_A;
	ap->nk=NK_NTK;
	ap->ancount=2;
	ap->qr=1;
	ap->qstlength=strlen("CICCIO");
	memcpy(ap->qstdata,"CICCIO",ap->qstlength);
	apd=andns_add_answ(ap);
	apd->rdlength=4;
	inet_aton("1.2.3.4",(struct in_addr*)apd->rdata);
//	apd->rdlength=strlen("PLUTO");
//	strcpy(apd->rdata,"PLUTO");
	apd=andns_add_answ(ap);
	apd->rdlength=4;
	inet_aton("11.12.13.14",(struct in_addr*)apd->rdata);
//	apd->rdlength=strlen("MINNIE");
//	strcpy(apd->rdata,"MINNIE");

	o=a_p(ap,a);
	handle_answer(a,o);
//	print_question(ap);
//	print_answer_name(apd);
	return 0;
}
	
void consistency_control(void)
{
	int len,limit;

	len=strlen(globopts.question);
	limit=(globopts.realm==REALM_NTK)?ANDNS_MAX_QST_LEN:ANNDS_DNS_MAZ_QST_LEN;
	if (len>limit) {
		printf("\nNo. request object is too long.\n");
		exit(1);
	}
}
int main(int argc,char **argv) 
{
	int c,res;
	extern int optind, opterr, optopt;
	extern char *optarg;

	init_opts();
	struct option longopts[]= {
		{"version",0,0,'v'},
		{"nameserver",1,0,'n'},
		{"port",1,0,'p'},
		{"query-type",1,0,'t'},
		{"realm",1,0,'r'},
		{"silent",0,0,'s'},
		{"help",0,0,'h'},
		{0,0,0,0}
	};

	while (1) {
		int option_index=0;
		c=getopt_long(argc, argv, "vn:t:p:r:sh", longopts, &option_index);
		if (c==-1) 
			break;
		switch (c) {
			case 'v':
				print_version();
				exit(1);
			case 's':
				globopts.silent=1;
				break;
			case 'h':
				print_usage();
				exit(1);
			case 'n':
				res=opt_set_ns(optarg,0);
				if (res) {
					printf("Nameserver `%s' is not friendly.\n",optarg);
					exit(1);
					break;
				}
				break;
			case 't':
				res=opt_set_qtype(optarg);
				if (res==-1) {
					printf("Query type `%s' is not valid.\n\n"
						"Valid query types are:\n"
						" a\thost -> ip\n"
						" mx\thost -> mx\n"
						" ptr\tip -> host\n"
						" ptrmx\tip ->mx\n"
						"(or univoque abbreviation)\n\n",optarg);
					exit(1);
					break;
				}
				break;
			case 'p':
				res=opt_set_port(optarg);
				if (res) {
					printf("Port `%s' is not a valid port.\n",optarg);
					exit(1);
					break;
				}
				break;
			case 'r':
				res=opt_set_realm(optarg);
				if (res) {
					printf("%s is not a valid realm.\n\n"
						"Valid realms are:\n"
						" ntk\tnetsukuku realm\n"
						" inet\tinternet realm\n"
						"(or univoque abbreviation)\n\n",optarg);
					exit(1);
					break;
				}
				break;

			/*case '?':
				printf("111%s: option `-%c' is invalid: ignored\n",argv[0], optopt);
				break;*/
			default:
				print_usage();
				exit(1);
				break;
		}
	}
	if (optind==argc) {
		print_usage();
		exit(1);
	}
	globopts.question=argv[optind];
	consistency_control();
	res=do_command();
	return 0;
}
