#ifndef ANDNSNET_H
#define ANDNSNET_H

int w_socket(int family,int type, int proto,int die);
int w_connect(struct addrinfo *ai,int die) ;
int serial_connect(struct addrinfo *ai,int die);
int host_connect(const char *host,uint16_t port,int type,int die) ;
int ai_connect(struct addrinfo *ai,int die,int free_ai);
ssize_t w_send(int sk,const void *buf,size_t len,int die) ;
ssize_t w_recv(int sk,void *buf,size_t len,int die);
ssize_t squit(const char *host,uint16_t port,int type,void *buf,size_t buflen,void *anbuf,size_t anlen,int die);
ssize_t ai_squit(struct addrinfo *ai,void *buf,size_t buflen,void *anbuf,size_t anlen,int die,int free_ai);

#endif /* ANDNSNET_H */
