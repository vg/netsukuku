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


#include "andns_net.h"


int idp_inet_ntop(struct sockaddr *addr,char *buf,int buflen)
{
    const char *res;
    struct sockaddr_in *saddr;
    struct sockaddr_in6 *saddr6;

    saddr= (struct sockaddr_in*)addr;
    res= inet_ntop(AF_INET, (void*)(&(saddr->sin_addr)), buf, buflen);
    if (res) return 0;
            
    saddr6= (struct sockaddr_in6*)addr;
    res= inet_ntop(AF_INET6, (void*)(&(saddr6->sin6_addr)), buf, buflen);
    if (res) return 0;

    return -1;
}

/* Connection Layer */

int w_connect(struct addrinfo *ai) 
{
    int sk,res;
    if((sk= socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol))==-1)
        return -1;
    
    res= connect(sk,ai->ai_addr,ai->ai_addrlen);
    if (!res) 
        return sk;

    close(sk);
    return -1;
}

int serial_connect(struct addrinfo *ai)
{
    int res;
    struct addrinfo *temp;

    temp= ai;
    if (!temp) return -1;

    do {
        res= w_connect(temp);
        temp= temp->ai_next;
    } while (res==-1 && temp);

    if (res==-1) return -1;

    return res;
}
    
/*
 * host_connect returns a connected socket to (host,port)
 * endpoint. It is protocol independent.
 * -1 on error.
 */
int host_connect(const char *host,uint16_t port,int type) 
{
    int res;
    char portstr[6];
    struct addrinfo *ai,filter;

    if (!host)
        return -1;

    memset(&filter, 0, sizeof(struct addrinfo));
    filter.ai_socktype= type;
    memset(portstr, 0, 6);

    res= snprintf(portstr, 6, "%d", port);
    if (res<0 || res>=6) return -1;

    res= getaddrinfo(host, portstr, &filter, &ai);
    if (res!=0) return -1;
    
    res= serial_connect(ai);
    freeaddrinfo(ai);
    return res;
}

int ai_connect(struct addrinfo *ai,int free_ai)
{
    int res;

    res=serial_connect(ai);
    if (free_ai)
        freeaddrinfo(ai);
    return res;
}

/* Communication Layer */

ssize_t w_send(int sk,const void *buf,size_t len) 
{
    return send(sk, buf, len, 0);
}

ssize_t w_recv(int sk,void *buf,size_t len)
{
    return recv(sk,buf,len,0);
}


/* 
 * These two functions and the MACRO are
 * almost VERBATIM copied from inet.c and inet.h.
 * Functions by AlpT, Andrea Lo Pumo.
 */

#define MILLISEC_TO_TV(x,t)                                             \
do{                                                                     \
        (t).tv_sec=(x)/1000;                                            \
        (t).tv_usec=((x) - ((x)/1000)*1000)*1000;                       \
}while(0)

ssize_t w_send_timeout(int s,const void *buf,size_t len,int timeout)
{
    struct timeval timeout_t;
    fd_set fdset;
    int ret;

    MILLISEC_TO_TV(timeout*1000, timeout_t);

    FD_ZERO(&fdset);
    FD_SET(s, &fdset);

    ret = select(s+1, NULL, &fdset, NULL, &timeout_t);
    return ret;

    if(FD_ISSET(s, &fdset))
        return w_send(s, buf, len);

    return -1;
}

ssize_t w_recv_timeout(int s,void *buf,size_t len,int timeout)
{
    struct timeval timeout_t;
    fd_set fdset;
    int ret;

    MILLISEC_TO_TV(timeout*1000, timeout_t);

    FD_ZERO(&fdset);
    FD_SET(s, &fdset);

    ret = select(s+1, &fdset, NULL, NULL, &timeout_t);
    if (ret == -1) return -1;

    if(FD_ISSET(s, &fdset))
        return w_recv(s, buf, len);
    return -1;
}


    
/* Dialog Layer */

/* "Botta e risposta" */
ssize_t hn_send_recv_close(const char *host,uint16_t port,int type,void *buf,
        size_t buflen,void *anbuf,size_t anlen,int timeout)
{
    ssize_t ret;
    int res;

    res=host_connect(host,port,type);
    if (res==-1) 
        return -1;
    if (timeout)
        ret=w_send_timeout(res,buf,buflen,timeout);
    else
        ret=w_send(res,buf,buflen);
    if (ret==-1) 
        return -2;
    if (timeout)
        ret=w_recv_timeout(res,anbuf,anlen,timeout);
    else
        ret=w_recv(res,anbuf,anlen);
    if (ret==-1)
        return -3;
    close(res);
    return ret;
}
/* "Botta e risposta" */
ssize_t ai_send_recv_close(struct addrinfo *ai,void *buf,size_t buflen,
        void *anbuf,size_t anlen,int free_ai,int timeout)
{
    ssize_t ret;
    int res;

    res=ai_connect(ai,free_ai);
    if (res==-1) 
        return -1;
    if (timeout)
        ret=w_send_timeout(res,buf,buflen,timeout);
    else
        ret=w_send(res,buf,buflen);
    if (ret==-1) 
        return -2;
    if (timeout)
        ret=w_recv_timeout(res,anbuf,anlen,timeout);
    else
        ret=w_recv(res,anbuf,anlen);
    if (ret==-1) 
        return -3;
    close(res);
    return ret;
}
    
void char_print(char *buf, int len)
{
        int i,count=0;

        printf("Printing %d bytes\n",len);
        for (i=0;i<len;i++) {
                printf("%02X ", (unsigned char)(buf[i]));
                count++;
                if ((count%16)==0) printf("\n");
        }
        printf("\n");
        return;
}

