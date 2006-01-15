#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "andns.h"
#include "andns_pkt.h"
#include "andns_rslv.h"
#include "andns_mem.h"
#include "resolv.h"
int main()
{
	int sock;
	struct addrinfo  hints,*res_addrs;
	int s_addrinfo_len=sizeof(struct addrinfo);
	int s_addr_len=sizeof(struct sockaddr);
	char buf[512],*crow;
	int bytes,yy;
	struct sockaddr fromaddr;
	socklen_t fromlen=s_addr_len;
	dns_pkt *dp;
	dns_pkt_a *dpa;
	
	andns_pkt_init(1);
	sock=socket(PF_INET,SOCK_DGRAM,0);
	if (sock==1)
		fprintf(stdout,"Sock error: %s\n", strerror(errno));
	system("./hh");

	memset(&hints,0,s_addrinfo_len);
	hints.ai_family=PF_INET;
	hints.ai_socktype=SOCK_DGRAM;
	hints.ai_flags=AI_PASSIVE;
	//
/*	yy=nametolbl("g",buf);
	printf("BUUUUU %d\n",yy);
	for (bytes=0;bytes<yy;bytes++)
		printf("CHAR %c oppure INYCHAR %d\n",buf[bytes],buf[bytes]);
*/
	if (getaddrinfo("localhost","53",&hints,&res_addrs)!=0)
		printf("getaddrinfo error: %s\n", gai_strerror(errno));

	if (bind(sock,res_addrs->ai_addr,s_addr_len)!=0)
		printf("bind error: %s\n", gai_strerror(errno));
	dp=create_dns_pkt();
	dpa=dns_add_a(&(dp->pkt_answ));


	while(1)
	{	
		bytes=recvfrom(sock,buf,512,0,&fromaddr,&fromlen);
		if (bytes<0)
		{
			printf("recvfrom error: %s\n",strerror(errno));
			//continue;
			return -1;
		}
		printf("Recv %d bytes\n",bytes);
		crow=andns_rslv(buf,bytes,NULL,&yy);
		if (!crow)
		{
			printf("Cio è NULL\n");
			return 0;
		}
		/*
		yy=res_send(buf,bytes,answer,512);
		printf("RES_send sended. Bytes: %d. Now calling dpkt\n",yy);
		dpkt(answer,yy,&dp);
		printf("RCODE %d\n",(dp->pkt_hdr).rcode);
		printf("QDCOUNT %d\n",(dp->pkt_hdr).qdcount);
		printf("ANCOUNT %d\n",(dp->pkt_hdr).ancount);
		printf("QR %d\n",(dp->pkt_hdr).qr);
		printf("DMANDA: %s\n",(dp->pkt_qst)->qname);
		printf("RISPOSTA cE? %c\n",(dp->pkt_answ)?'y':'n');*/
		if ((bytes=sendto(sock,crow,yy,0,&fromaddr,fromlen))==-1)
		{
			printf("sendto error: %s\n",strerror(errno));
			//continue;
			return -1;
		}
		if (bytes!=yy)
			printf("Bytes resolved differs from bytes sent\n");

		
	}
	return 0;
}
