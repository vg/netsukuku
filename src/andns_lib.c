                 /**************************************
                *     AUTHOR: Federico Tomassini        *
               *     Copyright (C) Federico Tomassini    *
              *     Contact effetom@gmail.com             *
             ***********************************************
             *******          BEGIN 3/2006          ********
*************************************************************************
*                                                                       *
*  This program is free software; you can redistribute it and/or modify *
*  it under the terms of the GNU General Public License as published by *
*  the Free Software Foundation; either version 2 of the License, or    *
*  (at your option) any later version.                                  *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
************************************************************************/

#include "andns_lib.h"
#include "log.h"
#include "err_errno.h"
#include "xmalloc.h"

#include <arpa/inet.h>

/*
 * Takes the buffer stream and translate headers to
 * andns_pkt struct.
 * Returns ALWAYS 4. The pkt_len has to be controlled
 * elsewhere.
 */
size_t a_hdr_u(char *buf,andns_pkt *ap)
{
        uint8_t c;
        uint16_t s;
        char *start_buf;

        start_buf=buf;

                // ROW 1
        memcpy(&s,buf,sizeof(uint16_t));
        ap->id=ntohs(s);
        buf+=2;

        memcpy(&c,buf,sizeof(uint8_t));
        ap->qr=(c>>7)&0x01;
        ap->p=c&0x40?1:0;
        ap->qtype=(c>>3)&0x07;
        ap->ancount=(c<<1)&0x0e;

        buf++;
        memcpy(&c,buf,sizeof(uint8_t));
        if (((*buf)&0x80)) ap->ancount++;

	ap->r=(c>>6)&0x01;
        ap->nk=(c>>4)&0x03;
        ap->rcode=c&0x0f;
        return ANDNS_HDR_SZ;
}
/*
 * Translate the andns_pkt question stream to andns_pkt struct.
 * -1 on error. Bytes readed otherwise.
 *  NOTE: The qst-data size is controlled: apkt won't need
 *  this control.
 */
size_t a_qst_u(char *buf,andns_pkt *ap,int limitlen)
{
	size_t ret;
	uint16_t s;
	uint8_t c;
	if (limitlen<4)
		err_ret(ERR_ANDMAP,-1);
	switch(ap->qtype) {
		case AT_A:
			memcpy(&s,buf,2);
			ap->service=ntohs(s);
			buf+=2;
			if (ap->nk==NTK_REALM) {
				ap->qstlength=16;
				if (ap->qstlength>limitlen-2)
                			err_ret(ERR_ANDPLB,-1);
				AP_ALIGN(ap);
				memcpy(ap->qstdata,buf,16);
				ret=18;
			} else {
				memcpy(&s,buf,2);
				ap->qstlength=ntohs(s);
				buf+=2;
        			if (ap->qstlength>=ANDNS_MAX_QST_LEN || 
					ap->qstlength>limitlen-4)
                			err_ret(ERR_ANDPLB,-1);
				AP_ALIGN(ap);
        			memcpy(ap->qstdata,buf,ap->qstlength);
				ret=ap->qstlength+4;
			}
			break;
		case AT_PTR:
			c=*buf;
			if (c!=0 && c!=1)
				err_ret(ERR_ANDMAP,-1)
			ap->qstlength=c?16:4;
			if (ap->qstlength>limitlen-1)
				err_ret(ERR_ANDMAP,-1)
			AP_ALIGN(ap);
			memcpy(ap->qstdata,buf,ap->qstlength);
			ret=ap->qstlength+1;
			break;
		default:
			debug(DBG_INSANE,"In a_qst_u: unknow query type.");
			err_ret(ERR_ANDMAP,-1)
	}
	return ret;
}
size_t a_answ_u(char *buf,andns_pkt *ap,int limitlen)
{
        uint16_t alen;
        andns_pkt_data *apd;

	if (limitlen<5)
		err_ret(ERR_ANDMAP,-1);
        memcpy(&alen,buf+2,sizeof(uint16_t));
        alen=ntohs(alen);
        if (alen+4>limitlen)
                err_ret(ERR_ANDPLB,-1);
        if (alen>ANDNS_MAX_DATA_LEN)
                err_ret(ERR_ANDPLB,-1);

        apd=andns_add_answ(ap);
	if (*buf&0x80)
		apd->r=1;
	apd->wg=(*buf&0x7f);
	apd->prio=(*(buf+1));
        apd->rdlength=alen;
	APD_ALIGN(apd);
        memcpy(apd->rdata,buf+4,alen);
        return alen+4;
}
size_t a_answs_u(char *buf,andns_pkt *ap,int limitlen)
{
        int ancount,i;
        size_t offset=0,res;

        ancount=ap->ancount;
        for (i=0;i<ancount;i++) {
                res=a_answ_u(buf+offset,ap,limitlen-offset);
                if (res==-1) {
                        error(err_str);
                        err_ret(ERR_ANDMAD,-1);
                }
                offset+=res;
        }
        return offset;
}
/*
 * This is a main function: takes the pkt-buf and translate
 * it in structured data.
 * It cares about andns_pkt allocation.
 * The apkt is allocate here.
 *
 * Returns:
 * -1 on E_INTRPRT
 *  0 if pkt must be discarded.
 *  Number of bytes readed otherwise
 */
size_t a_u(char *buf,size_t pktlen,andns_pkt **app)
{
        andns_pkt *ap;
        size_t offset,res;
        int limitlen;

        if (pktlen<ANDNS_HDR_SZ)
                err_ret(ERR_ANDPLB,0);
        *app=ap=create_andns_pkt();
        offset=a_hdr_u(buf,ap);
        buf+=offset;
        limitlen=pktlen-offset;
        if ((res=a_qst_u(buf,ap,limitlen))==-1) {
                error(err_str);
                err_ret(ERR_ANDMAP,-1);
        }
	offset+=res;
	buf+=res;
	limitlen-=res;
	if ((res=a_answs_u(buf,ap,limitlen))==-1) {
		error(err_str);
		err_ret(ERR_ANDMAP,-1);
	}
        return offset+res;
}

size_t a_hdr_p(andns_pkt *ap,char *buf)
{
        uint16_t s;
        s=htons(ap->id);
        memcpy(buf,&s,sizeof(uint16_t));
        buf+=2;
        if (ap->qr)
                (*buf)|=0x80;
	if (ap->p)
		(*buf)|=0x40;
        (*buf)|=( (ap->qtype)<<3);
        (*buf++)|=( (ap->ancount)>>1);
        (*buf)|=( (ap->ancount)<<7);
	if (ap->r)
		*buf|=0x40;
        (*buf)|=( (ap->nk)<<4);
        (*buf)|=(  ap->rcode);
        return ANDNS_HDR_SZ;
}
size_t a_qst_p(andns_pkt *ap,char *buf,size_t limitlen)
{
	size_t ret;
        uint16_t s;
	switch(ap->qtype){
		case AT_A:
			if (ap->qstlength+4>limitlen)
				err_ret(ERR_ANDMAD,-1);
			s=htons(ap->service);
			memcpy(buf,&s,2);
			buf+=2;
			ret=ap->qstlength+2;
			if (ap->nk==INET_REALM) {
				s=htons(ap->qstlength);
				memcpy(buf,&s,2);
				buf+=2;
				ret+=2;
			}
			memcpy(buf,ap->qstdata,ap->qstlength);
        		memcpy(buf,ap->qstdata,ap->qstlength);
			break;
		case AT_PTR:
			if (ap->qstlength+1>limitlen)
				err_ret(ERR_ANDMAD,-1);
			if (ap->qstlength==16) 
				*buf=0x01;
			else if (ap->qstlength!=4)
				err_ret(ERR_ANDMAD,-1);
			buf++;
			memcpy(buf,ap->qstdata,ap->qstlength);
			ret=ap->qstlength+1;
			break;
		default:
			debug(DBG_INSANE,"In a_qst_p: unknow query type.");
			err_ret(ERR_ANDMAD,-1);
	}
	return ret;
}
size_t a_answ_p(andns_pkt_data *apd,char *buf,size_t limitlen)
{
        uint16_t s;
        if (apd->rdlength>ANDNS_MAX_DATA_LEN || 
			limitlen< apd->rdlength+4)
                err_ret(ERR_ANDPLB,-1);
	if (apd->r)
		*buf|=0x80;
	*buf++|= (apd->wg&0x7f);
	*buf++|=apd->prio;
        s=htons(apd->rdlength);
        memcpy(buf,&s,sizeof(uint16_t));
        buf+=2;
        memcpy(buf,apd->rdata,apd->rdlength);
        return apd->rdlength+4;
}
size_t a_answs_p(andns_pkt *ap,char *buf, size_t limitlen)
{
        andns_pkt_data *apd;
        int i;
        size_t offset=0,res;

        apd=ap->pkt_answ;
        for (i=0;i<ap->ancount && apd;i++) {
                if((res=a_answ_p(apd,buf+offset,limitlen-offset))==-1) {
                        error(err_str);
                        err_ret(ERR_ANDMAD,-1);
                }
                offset+=res;
                apd=apd->next;
        }
        return offset;
}
size_t a_p(andns_pkt *ap, char *buf)
{
        size_t offset,res;

        memset(buf,0,ANDNS_MAX_SZ);

        offset=a_hdr_p(ap,buf);
        buf+=offset;
        if ((res=a_qst_p(ap,buf,ANDNS_MAX_SZ-offset))==-1)
                goto server_fail;
        offset+=res;
        buf+=res;
        if ((res=a_answs_p(ap,buf,ANDNS_MAX_SZ-offset))==-1)
                goto server_fail;
        offset+=res;
        destroy_andns_pkt(ap);
        return offset;
server_fail:
        destroy_andns_pkt(ap);
        error(err_str);
        err_ret(ERR_ANDMAD,-1);
}


/* MEM */

	/* Borning functions */
andns_pkt* create_andns_pkt(void)
{
        andns_pkt *ap;
        ap=xmalloc(ANDNS_PKT_SZ);
	memset(ap,0,ANDNS_PKT_SZ);
        return ap;
}

andns_pkt_data* create_andns_pkt_data(void)
{
        andns_pkt_data *apd;
        apd=xmalloc(ANDNS_PKT_DATA_SZ);
	memset(apd,0,ANDNS_PKT_DATA_SZ);
        return apd;
}
andns_pkt_data* andns_add_answ(andns_pkt *ap)
{
        andns_pkt_data *apd,*a;

        apd=create_andns_pkt_data();
        a=ap->pkt_answ;
        if (!a) {
                ap->pkt_answ=apd;
                return apd;
        }
        while (a->next) a=a->next;
        a->next=apd;
        return apd;;
}
	/* Death functions */
void destroy_andns_pkt_data(andns_pkt_data *apd)
{
	if (apd->rdata)
		xfree(apd->rdata);
	xfree(apd);
}
void destroy_andns_pkt_datas(andns_pkt *ap)
{
	andns_pkt_data *apd,*apd_t;
	apd=ap->pkt_answ;
	while(apd) {
		apd_t=apd->next;
		destroy_andns_pkt_data(apd);
		apd=apd_t;
	}
}
void destroy_andns_pkt(andns_pkt *ap)
{
	if (ap->qstdata)
		xfree(ap->qstdata);
	destroy_andns_pkt_datas(ap);
        xfree(ap);
}
