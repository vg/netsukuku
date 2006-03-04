
/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
 *
 * --
 * This code derives from iproute2/iprule.c, it was slightly modified to fit
 * in Netsukuku.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 * Changes:
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 * Rani Assaf <rani@magic.metawire.com> 980930:	do not allow key for ipip/sit
 * Phil Karn <karn@ka9q.ampr.org>	990408:	"pmtudisc" flag
 */

#include "includes.h"

#include <linux/ip.h>
#include <linux/if_tunnel.h>

#include "libnetlink.h"
#include "inet.h"
#include "iptunnel.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "xmalloc.h"
#include "log.h"

static int do_add(int cmd, inet_prefix *remote, inet_prefix *local, char *dev,
                int tunl_number);
int do_del(inet_prefix *remote, inet_prefix *local, char *dev, int tunl_number);

int tunnel_add(inet_prefix *remote, inet_prefix *local, char *dev,
		int tunl_number)
{
	return do_add(SIOCADDTUNNEL, remote, local, dev, tunl_number);
}

int tunnel_change(inet_prefix *remote, inet_prefix *local, char *dev,
		int tunl_number)
{
	return do_add(SIOCCHGTUNNEL, remote, local, dev, tunl_number);
}

int tunnel_del(inet_prefix *remote, inet_prefix *local, char *dev,
		int tunl_number)
{
	return do_del(remote, local, dev, tunl_number);
}

static int do_ioctl_get_ifindex(const char *dev)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err) {
		error(ERROR_MSG "ioctl: %s",ERROR_POS, strerror(errno));
		return 0;
	}
	close(fd);
	return ifr.ifr_ifindex;
}

#if 0
static int do_ioctl_get_iftype(const char *dev)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCGIFHWADDR, &ifr);
	if (err) {
		error(ERROR_MSG "ioctl: %s",ERROR_POS, strerror(errno));
		return -1;
	}
	close(fd);
	return ifr.ifr_addr.sa_family;
}

static char * do_ioctl_get_ifname(int idx)
{
	static struct ifreq ifr;
	int fd;
	int err;

	ifr.ifr_ifindex = idx;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCGIFNAME, &ifr);
	if (err) {
		error(ERROR_MSG "ioctl: %s",ERROR_POS, strerror(errno));
		return NULL;
	}
	close(fd);
	return ifr.ifr_name;
}
#endif

static int do_get_ioctl(const char *basedev, struct ip_tunnel_parm *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, basedev, IFNAMSIZ);
	ifr.ifr_ifru.ifru_data = (void*)p;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCGETTUNNEL, &ifr);
	if (err)
		error(ERROR_MSG "ioctl: %s",ERROR_POS, strerror(errno));
	close(fd);
	return err;
}

static int do_add_ioctl(int cmd, const char *basedev, struct ip_tunnel_parm *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	if (cmd == SIOCCHGTUNNEL && p->name[0])
		strncpy(ifr.ifr_name, p->name, IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, basedev, IFNAMSIZ);
	ifr.ifr_ifru.ifru_data = (void*)p;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, cmd, &ifr);
	if (err)
		error(ERROR_MSG "ioctl: %s",ERROR_POS, strerror(errno));
	close(fd);
	return err;
}

static int do_del_ioctl(const char *basedev, struct ip_tunnel_parm *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	if (p->name[0])
		strncpy(ifr.ifr_name, p->name, IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, basedev, IFNAMSIZ);
	ifr.ifr_ifru.ifru_data = (void*)p;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCDELTUNNEL, &ifr);
	if (err)
		error(ERROR_MSG "ioctl: %s",ERROR_POS, strerror(errno));
	close(fd);
	return err;
}

/*
 * fill_tunnel_parm: fills the `p' struct.
 * `remote' and `local' must be in host order
 */
static int fill_tunnel_parm(int cmd, inet_prefix *remote, inet_prefix *local, 
		char *dev, int tunl_number, struct ip_tunnel_parm *p)
{
	char medium[IFNAMSIZ];

	memset(p, 0, sizeof(*p));
	memset(&medium, 0, sizeof(medium));

	p->iph.version = 4;
	p->iph.ihl = 5;
#ifndef IP_DF
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#endif
	p->iph.frag_off = htons(IP_DF);
	p->iph.protocol = IPPROTO_IPIP;

	if(remote)
		p->iph.daddr = htonl(remote->data[0]);
	if(local)
		p->iph.saddr = htonl(local->data[0]);
	if(dev)
		strncpy(medium, dev, IFNAMSIZ-1);

	sprintf(p->name, "tunl%d", tunl_number);
	if (cmd == SIOCCHGTUNNEL) {
		/* Change the old tunnel */
		struct ip_tunnel_parm old_p;
		memset(&old_p, 0, sizeof(old_p));
		if (do_get_ioctl(p->name, &old_p))
			return -1;
		*p = old_p;
	}

	if (p->iph.protocol == IPPROTO_IPIP)
		if ((p->i_flags & GRE_KEY) || (p->o_flags & GRE_KEY))
			fatal("Keys are not allowed with ipip and sit.");

	if (medium[0]) {
		p->link = do_ioctl_get_ifindex(medium);
		if (p->link == 0)
			return -1;
	}

	if (IN_MULTICAST(ntohl(p->iph.daddr)) && !p->iph.saddr)
		fatal("Broadcast tunnel requires a source address.");

	return 0;
}


static int do_add(int cmd, inet_prefix *remote, inet_prefix *local, char *dev,
                int tunl_number)
{
	struct ip_tunnel_parm p;

	if (fill_tunnel_parm(cmd, remote, local, dev, tunl_number, &p) < 0)
		return -1;

	if (p.iph.ttl && p.iph.frag_off == 0)
		fatal("ttl != 0 and noptmudisc are incompatible");

	switch (p.iph.protocol) {
		case IPPROTO_IPIP:
			return do_add_ioctl(cmd, "tunl0", &p);
		default:	
			fatal("cannot determine tunnel mode (ipip, gre or sit)\n");
	}
	return -1;
}

int do_del(inet_prefix *remote, inet_prefix *local, char *dev, int tunl_number)
{
	struct ip_tunnel_parm p;

	if (fill_tunnel_parm(SIOCDELTUNNEL, remote, local, dev, 
				tunl_number, &p) < 0)
		return -1;

	switch (p.iph.protocol) {
		case IPPROTO_IPIP:
			return do_del_ioctl("tunl0", &p);
		default:	
			return do_del_ioctl(p.name, &p);
	}
	return -1;
}

/*
 * tun_add_tunl: it adds in the `ifs' array a new struct which refers to 
 * the tunnel "tunlX", where X is a number equal to `tunl'.
 */
int tun_add_tunl(interface *ifs, u_char tunl)
{
	char tunl_name[IFNAMSIZ];
	struct rtnl_handle rth;

	if (rtnl_open(&rth, 0) < 0) {
		error(ERROR_MSG "Cannot open the rtnetlink socket",ERROR_POS);
		return -1;
	}
	ll_init_map(&rth);

	sprintf(tunl_name, "tunl%d", tunl);
	strncpy(ifs->dev_name, tunl_name, IFNAMSIZ);
	if(!(ifs->dev_idx=ll_name_to_index(tunl_name)))
		return -1;

	rtnl_close(&rth);
	return 0;
}

/* 
 * tun_del_tunl: it removes from the `ifs' array, which must have at least
 * MAX_TUNNEL_IFS members, the struct which refers the tunnel "tunlX", where X
 * is a number equal to `tunl'.
 * If no such struct is found, -1 is returned.
 */
int tun_del_tunl(interface *ifs, u_char tunl)
{
	char tunl_name[IFNAMSIZ];
	int i;
	
	sprintf(tunl_name, "tunl%d", tunl);

	for(i=0; i<MAX_TUNNEL_IFS; i++)
		if(!strncmp(ifs[i].dev_name, tunl_name, IFNAMSIZ)) {
			memset(&ifs[i], 0, sizeof(interface));
			return 0;
		}
	
	return -1;
}



void init_tunnels_ifs(void)
{
	memset(tunnel_ifs, 0, sizeof(interface)*MAX_TUNNEL_IFS);
}

/*
 * first_free_tunnel_if: returns the position of the first member of the 
 * `tunnel_ifs' array which isn't used yet.
 * If the whole array is full, -1 is returned.
 */
int first_free_tunnel_if(void)
{
	int i;

	for(i=0; i<MAX_TUNNEL_IFS; i++)
		if(!*tunnel_ifs[i].dev_name && !tunnel_ifs[i].dev_idx)
			return i;
	return -1;
}

/*
 * set_tunnel_ip: it brings down and up and set the `tunl_ip' IP to the
 * "tunl`tunl_number'" tunnel device
 */
int set_tunnel_ip(int tunl_number, inet_prefix *tunl_ip)
{
	const char *ntop;
	ntop=inet_to_str(*tunl_ip);

	set_all_ifs(&tunnel_ifs[tunl_number], 1, set_dev_down);
	set_all_ifs(&tunnel_ifs[tunl_number], 1, set_dev_up);
	if(set_all_dev_ip(*tunl_ip, &tunnel_ifs[tunl_number], 1) < 0) {
		error("Cannot set the %s ip to tunl%d",
				ntop, tunl_number);
		return -1;
	}
	return 0;
}

/*
 * add_tunnel_if: creates a new tunnel, adds it in the `tunnel_ifs' array, and
 * if `tunl_ip' isn't null, sets to the tunnel the IP `tunl_ip'.
 */
int add_tunnel_if(inet_prefix *remote, inet_prefix *local, char *dev,
		int tunl_number, inet_prefix *tunl_ip)
{
	if(!tunl_number) {
		if(tunnel_change(remote, local, dev, tunl_number) < 0) {
			error("Cannot modify the \"tunl%d\" tunnel",
					tunl_number);
			return -1;
		}
	} else {
		if(tunnel_add(remote, local, dev, tunl_number) < 0) {
			error("Cannot add the \"tunl%d\" tunnel", tunl_number);
			return -1;
		}
	}

	if(tun_add_tunl(&tunnel_ifs[tunl_number], tunl_number) < 0)
		return -1;

	if(tunl_ip) {
		if(set_tunnel_ip(tunl_number, tunl_ip) < 0)
			return -1;
	}

	return 0;
}

/*
 * del_tunnel_if: the inverse of add_tunnel_if() (see above)
 */
void del_tunnel_if(inet_prefix *remote, inet_prefix *local, char *dev,
		int tunl_number)
{
	if(tunl_number)	{
		if(tunnel_del(remote, local, dev, tunl_number) < 0) {
			error("Cannot delete the \"tunl%d\" tunnel", 
					tunl_number);
			return -1;
		}
	}

	tun_del_tunl(tunnel_ifs, tunl_number);
}
