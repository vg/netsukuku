#ifndef ANDNS_NET_H
#define ANDNS_NET_H

int idp_inet_ntop(int family,struct sockaddr *addr,char *buf,int buflen);
int w_socket(int family,int type, int proto,int die);
int w_connect(struct addrinfo *ai,int die) ;
int serial_connect(struct addrinfo *ai,int die);
int host_connect(const char *host,uint16_t port,int type,int die) ;
int ai_connect(struct addrinfo *ai,int die,int free_ai);
ssize_t w_send(int sk,const void *buf,size_t len,int die) ;
ssize_t w_recv(int sk,void *buf,size_t len,int die);
ssize_t hn_send_recv_close(const char *host,uint16_t port,int type,void *buf,size_t buflen,void *anbuf,size_t anlen,int die);
ssize_t ai_send_recv_close(struct addrinfo *ai,void *buf,size_t buflen,void *anbuf,size_t anlen,int die,int free_ai);

#endif /* ANDNS_NET_H */
