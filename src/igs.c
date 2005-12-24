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
 * igs.c:
 * Internet Gateway Search
 */

#include "includes.h"
#include <sys/wait.h>

#include "llist.c"
#include "libnetlink.h"
#include "inet.h"
#include "krnl_route.h"
#include "request.h"
#include "endianness.h"
#include "pkts.h"
#include "bmap.h"
#include "qspn.h"
#include "radar.h"
#include "netsukuku.h"
#include "route.h"
#include "libping.h"
#include "igs.h"
#include "xmalloc.h"
#include "log.h"


/*
 * bandwidth_in_8bit:
 * `x' is the bandwidth value expressed in Kb/s.
 * 
 * Since we consider `x' expressed in this form:
 * 	 x = y * 2^y; 
 * we can store just `y' in a u_char (8bit) variable.
 *
 * `bandwidth_in_8bit' returns `y' from `x'.
 *
 * `x' cannot be greater than 3623878656 (27*2^27), so if `x' is in Kb/s the
 * maximum bandwidth we can store in a byte is 3.6Tb/s.
 */
u_char bandwidth_in_8bit(u_int x)
{
	u_int i,z,a,b;
	u_int diff_2;

	for(z=27;z>=0;z--) {
		
		i=z<<z;
		if(i==x)
			/* x is exactly z*2^z */
			return (u_char)z;
	
		b=(z-1)<<(z-1);
		diff_2=(i-b)>>1;
		if(x >= i-diff_2 && x <=i)
			/* `x' is nearer to z*2^z than (z-1)*2^(z-1) */ 
			return z;

		a = z == 27 ? i : (z+1)<<(z+1);
		diff_2=(a-i)>>1;
		if(x <= i+diff_2 && x >= i)
			/* `x' is nearer to z*2^z than (z+1)*2^(z+1) */ 
			return z;
	}
	return 0;
}

/*
 * bandwidth_to_32bit: the inverse of bandwidth_in_8bit
 */
u_int bandwidth_to_32bit(u_char x)
{
	return (u_int)x<<x;
}

/*
 * str_to_inet_gw:
 * The syntax of `str' is IP:devname, i.e. 192.168.1.1:eth0.
 * str_to_inet_gw() stores the IP in `gw' and the device name in `dev'.
 * `dev' must be IFNAMSIZ big.
 * On error -1 is returned.
 */
int str_to_inet_gw(char *str, inet_prefix *gw, char *dev)
{
	char *buf;

	memset(dev, 0, IFNAMSIZ);

	/* Copy :devname in `dev' */
	if(!(buf=rindex(str, ':')))
		return -1;
	*buf=0;
	buf++;
	strncpy(dev, buf, IFNAMSIZ);
	dev[IFNAMSIZ-1]=0;

	/* Extract the IP from the first part of `str' */
	if(str_to_inet(str, gw))
		return -1;

	return 0;
}

/*
 * parse_internet_hosts: given a string which uses the following syntax:
 * 	"hostname1:hostname2:hostname3:..."
 * it stores each hostname in a new mallocated array and returns it.
 * The number of hostnames is written in `*hosts'
 * On error 0 is returned.
 */
char **parse_internet_hosts(char *str, int *hosts)
{
	char **hnames;
	
	hnames=split_string(str, ":", hosts, MAX_INTERNET_HNAMES,
			MAX_INTERNET_HNAME_SZ);
	return hnames;
}

void free_internet_hosts(char **hnames, int hosts)
{
	int i;
	for(i=0; i<hosts; i++)
		if(hnames[i])
			xfree(hnames[i]);
	if(hnames)
		xfree(hnames);
}

void init_igws(inet_gw ***igws, int **igws_counter, int levels)
{
	*igws=xmalloc(sizeof(inet_gw *) * levels);
	memset(*igws, 0, sizeof(inet_gw *) * levels);

	if(igws_counter) {
		*igws_counter=(int *)xmalloc(sizeof(int)*levels);
		memset(*igws_counter, 0, sizeof(int)*levels);
	}
}

void reset_igws(inet_gw **igws, int *igws_counter, int levels)
{
	int i;
	
	if(!igws)
		return;

	for(i=0; i<levels; i++) {
		list_destroy(igws[i]);
		igws_counter[i]=0;
	}
}

void free_igws(inet_gw **igws, int *igws_counter, int levels)
{
	if(!igws)
		return;

	reset_igws(igws, igws_counter, levels);

	if(igws)
		xfree(igws);
	if(igws_counter)
		xfree(igws_counter);
}

/* 
 * init_my_igws: initializses the `my_igws' llist. This list keeps inet_gw
 * structs which points to our (g)nodes, for example:
 * my_igws[0]->node == me.cur_node,
 * my_igws[1]->node == &me.cur_quadg.gnode[_EL(1)]->g.
 */
void init_my_igws(inet_gw **igws, int *igws_counter,
		inet_gw ***my_new_igws, u_char my_bandwidth, 
		map_node *cur_node, quadro_group *qg)
{
	inet_gw *igw, **my_igws;
	map_node *node;
	int i=0, e, bw, bw_mean;
	
	init_igws(&my_igws, 0, qg->levels);
	
	for(i=0; i<qg->levels; i++) {
		if(!i) {
			node=cur_node;
			bw_mean=my_bandwidth;
		} else {
			node=&qg->gnode[_EL(i)]->g;
			
			bw=e=0;
			igw=igws[i-1];
			list_for(igw) {
				bw_mean+=igw->bandwidth;
				e++;
			}
			bw_mean/=e;
			
			if(my_bandwidth && i==1)
				/* Add our bw in the average */
				bw_mean=(bw_mean*e+my_bandwidth)/(e+1);
		}
		
		igw=igw_add_node(igws, igws_counter, i, qg->gid[i],
				node, (u_char)bw_mean);
		my_igws[i]=igw;
	}
	
	*my_new_igws=my_igws;
}

void free_my_igws(inet_gw ***my_igs)
{
	if(*my_igs && *my_igs)
		xfree(*my_igs);
	*my_igs=0;
}

/*
 * init_internet_gateway_search: 
 * Initialization of the igs.c code.
 */
void init_internet_gateway_search(void)
{
	inet_prefix new_gw;
	char new_gw_dev[IFNAMSIZ];

	pthread_t ping_thread;
	pthread_attr_t t_attr;
	int i, ret;


        if(!server_opt.restricted || !server_opt.inet_connection)
		return;

	init_igws(&me.igws, &me.igws_counter, GET_LEVELS(my_family));
	
	if(!server_opt.inet_hosts)
		fatal("You didn't specified any Internet hosts in the "
			"configuration file. What hosts should I ping?");
	
	if(server_opt.share_internet)
                igw_exec_masquerade_sh(server_opt.ip_masq_script);
			
	ret=rt_get_default_gw(&new_gw, new_gw_dev);
	if(ret < 0) {
		loginfo("The retrieval of the default gw from the kernel failed.");

		if(!server_opt.inet_gw.data[0])
			fatal("The default gw isn't set in the kernel and you "
				"didn't specified it in netsukuku.conf. "
				"Cannot continue!");

		loginfo("Trying to set %s dev %s as the default gw",
			inet_to_str(server_opt.inet_gw), server_opt.inet_gw_dev);
		if(rt_replace_def_gw(server_opt.inet_gw_dev, server_opt.inet_gw))
			fatal("Cannot set the default gw to %s for the %s dev",
				inet_to_str(server_opt.inet_gw),
				server_opt.inet_gw_dev);
		
	} else if(strncmp(new_gw_dev, server_opt.inet_gw_dev, IFNAMSIZ) || 
		memcmp(new_gw.data, server_opt.inet_gw.data, MAX_IP_SZ)) {
		loginfo("Your specified Internet gateway doesn't match with "
			"the one currently stored in the kernel routing table."
			"I'm going to use the kernel gateway: %s dev %s",
			inet_to_str(new_gw), new_gw_dev);

		strncpy(server_opt.inet_gw_dev, new_gw_dev, IFNAMSIZ);
		memcpy(&server_opt.inet_gw, &new_gw, sizeof(inet_prefix));
	}

	for(i=0; i < me.cur_ifs_n; i++)
		if(!strcmp(me.cur_ifs[i].dev_name, server_opt.inet_gw_dev))
			fatal("You have selected the \"%s\" interface but your "
				"Internet connection uses it and you want the"
				" compatibilty with the Internet. This is not"
				" possible. Don't include \"%s\" in the list "
				"of interfaces utilised by the daemon",
				server_opt.inet_gw_dev, server_opt.inet_gw_dev);
		
	loginfo("Launching the first ping to the Internet hosts");
	me.inet_connected=igw_check_inet_conn();
	if(me.inet_connected)
		loginfo("The Internet connection is up & running");
	else
		loginfo("The Internet connection appears to be down");
	if(!me.inet_connected && server_opt.share_internet)
		fatal("We are not connected to the Internet, but you want to "
			"share your connection. Please check your options");

	debug(DBG_SOFT,   "Evocating the ping daemon.");
        pthread_attr_init(&t_attr);
        pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&ping_thread, &t_attr, igw_check_inet_conn_t, 0);
}

/*
 * igw_add_node: adds a new gw in the `igws[`level']' llist.
 * The pointer to the new inet_gw is returned.
 */
inet_gw *igw_add_node(inet_gw **igws, int *igws_counter,  int level,
		int gid, map_node *node, u_char bandwidth)
{
	inet_gw *igw;
	
	igw=xmalloc(sizeof(inet_gw));
	memset(igw, 0, sizeof(inet_gw));

	igw->node=node;
	igw->gid=gid;
	igw->bandwidth=bandwidth;
		
	clist_add(&igws[level], &igws_counter[level], igw);

	return igw;
}

/*
 * igw_find_node: finds an inet_gw struct in the `igws[`level']' llist which
 * points to the given `node'. The pointer to the found struct is
 * returned, otherwise 0 is the return value.
 */
inet_gw *igw_find_node(inet_gw **igws, int level, map_node *node)
{
	inet_gw *igw;

	igw=igws[level];
	list_for(igw)
		if(igw->node == node)
			return igw;
	return 0;
}

/*
 * igw_del_node: deletes, from the `igws[`level']' llist, the inet_gw struct
 * which points to `node'. On success 0 is returned.
 */
int igw_del_node(inet_gw **igws, int *igws_counter,  int level,
		map_node *node)
{
	inet_gw *igw;

	igw=igw_find_node(igws, level, node);
	if(!igw)
		return -1;

	clist_del(&igws[level], &igws_counter[level], igw);
	return 0;
}

/*
 * igw_update_gnode_bw: 
 * call this function _after_ adding and _before_ deleting the `igw->node' node
 * from the me.igws llist. This fuctions will update the `bandwidth' value of
 * the inet_gw which points to out (g)nodes.
 * Use `new'=1 if you are adding the node, otherwise use 0.
 */
void igw_update_gnode_bw(int *igws_counter, inet_gw **my_igws, inet_gw *igw,
		int new, int level, int maxlevels)
{
	int i, bw, old_bw=0;
	
	if(level >= maxlevels)
		return;

	if(new) {
		if(igws_counter[level] <= 0)
			return;
		
		bw = my_igws[level+1]->bandwidth * (igws_counter[level]-1);
		bw = (bw + igw->bandwidth) / igws_counter[level];
	} else {
		if(igws_counter[level] <= 1)
			return;

		bw = my_igws[level+1]->bandwidth * igws_counter[level];
		bw = (bw - igw->bandwidth) / (igws_counter[level]-1);
	}
	old_bw = my_igws[level+1]->bandwidth;
	my_igws[level+1]->bandwidth = bw;

	for(i=level+2; i<maxlevels; i++) {
		if(!my_igws[i] || igws_counter[i-1] <= 0)
			break;

		bw = my_igws[i]->bandwidth * igws_counter[i-1];
		bw = (bw - old_bw + my_igws[i-1]->bandwidth)/igws_counter[i-1];
		old_bw = my_igws[i]->bandwidth;
		my_igws[i]->bandwidth = bw;
	}
}


/*
 * igw_cmp: compares two inet_gw structs calculating their connection quality: 
 * bandwith - rtt/1000;
 */
int igw_cmp(const void *a, const void *b)
{
	inet_gw *gw_a=(inet_gw *)a;
	inet_gw *gw_b=(inet_gw *)b;

	u_int cq_a, cq_b, trtt;

	/* let's calculate the connection quality of both A and B */
	trtt = gw_a->node->links ? gw_a->node->r_node[0].trtt/1000 : 0;
	cq_a = bandwidth_to_32bit(gw_a->bandwidth) - trtt;
	trtt = gw_b->node->links ? gw_b->node->r_node[0].trtt/1000 : 0;
	cq_b = bandwidth_to_32bit(gw_b->bandwidth) - trtt;
	
	if(cq_a > cq_b)
		return 1;
	else if(cq_a == cq_b)
		return 0;
	else
		return -1;
}

/*
 * igw_order: orders in decrescent order the `igws[`level']' llist,
 * comparing the igws[level]->bandwidth and igws[level]->node->r_node[0].trtt 
 * values.
 * `my_igws[level]' will point to the inet_gw struct which refers to an our
 * (g)node.
 */
void igw_order(inet_gw **igws, int *igws_counter, inet_gw **my_igws, int level)
{
	inet_gw *igw, *igw_tmp;
	int i;
		
	if(!igws_counter[level] || !igws[level])
		return;
	
	igw_tmp=xmalloc(sizeof(inet_gw)*igws_counter[level]);
	
	/*
	 * Save a copy of the igws[leve] llist in the `igw_tmp' static buffer
	 * to let `qsort' sort it ._^
	 */
	i=0;
	igw=igws[level];
	list_for(igw) {
		memcpy(&igw_tmp[i], igw, sizeof(inet_gw));
		i++;
	}

	qsort(igw_tmp, i, sizeof(inet_gw), igw_cmp);

	/* 
	 * Restore igws[level] 
	 */
	i=0;
	igw=igws[level];
	list_for(igw) {
		memcpy(igw, &igw_tmp[i], sizeof(inet_gw));
		if(igw->node->flags & MAP_ME)
			my_igws[level]=igw;
		i++;
	}

	xfree(igw_tmp);
}


/*
 * igw_check_inet_conn: returns 1 if we are still connected to the Internet.
 * The check is done by pinging the `server_opt.inet_hosts'.
 */
int igw_check_inet_conn(void)
{
	int i, ret;

	for(i=0; server_opt.inet_hosts && server_opt.inet_hosts[i]; i++) {
		ret=pingthost(server_opt.inet_hosts[i], INET_HOST_PING_TIMEOUT);
		if(ret >= 1)
			return 1;
	}
	
	return 0;
}

/*
 * igw_check_inet_conn_t: checks if we are connected to the internet, then
 * waits, then checks if we are connected, then ...
 */
void *igw_check_inet_conn_t(void *null)
{
	inet_prefix new_gw;
	char new_gw_dev[IFNAMSIZ];
	int old_status, ret;
	
	for(;;) {
		old_status=me.inet_connected;
		me.inet_connected=igw_check_inet_conn();

		if(old_status && !me.inet_connected) {
			/* Connection lost, disable me.my_igws[0] */
			loginfo("Internet connection lost. Inet connection sharing disabled");
					
			me.my_igws[0]->bandwidth=0;
			igw_update_gnode_bw(me.igws_counter, me.my_igws,
				me.my_igws[0], 0, 0, me.cur_quadg.levels);
			clist_join(&me.igws[0], &me.igws_counter[0], me.my_igws[0]);
			
		} else if(!old_status && me.inet_connected) {
			if(server_opt.share_internet) {
				/* Maybe the Internet gateway is changed, it's
				 * better to check it */

				ret=rt_get_default_gw(&new_gw, new_gw_dev);
				if(ret < 0) {
					/* 
					 * Something's wrong, we can reach Inet
					 * hosts, but we cannot take the default 
					 * gw, thus consider ourself not connected.
					 */
					me.inet_connected=0;
					goto skip_it;
				}
				if(strncmp(new_gw_dev, server_opt.inet_gw_dev, IFNAMSIZ) || 
					memcmp(new_gw.data, server_opt.inet_gw.data, MAX_IP_SZ)) {
					
					/* New Internet gw (dialup connection ?)*/
					strncpy(server_opt.inet_gw_dev, new_gw_dev, IFNAMSIZ);
					memcpy(&server_opt.inet_gw, &new_gw, sizeof(inet_prefix));
					loginfo("Our Internet gateway changed, now it is: %s dev %s",
							inet_to_str(new_gw), new_gw_dev);
				} else
					loginfo("Internet connection is alive again. "
							"Inet connection sharing enabled");
			}

			/* Yay! We're connected, enable me.my_igws[0] */
			me.my_igws[0]->bandwidth=me.my_bandwidth;
			clist_ins(&me.igws[0], &me.igws_counter[0], me.my_igws[0]);
			igw_update_gnode_bw(me.igws_counter, me.my_igws,
				me.my_igws[0], 1, 0, me.cur_quadg.levels);

		}
skip_it:	
		sleep(INET_NEXT_PING_WAIT);
	}
}

/*
 * igw_exec_masquerade_sh: executes `script', which will do IP masquerade
 */
int igw_exec_masquerade_sh(char *script)
{
	int ret;
	
	loginfo("Executing %s", script);
	
	ret=system(script);
	if(ret == -1)
		fatal("Couldn't execute %s: %s", strerror(errno));
	if(!WIFEXITED(ret) || (WIFEXITED(ret) && WEXITSTATUS(ret) != 0))
		fatal("%s didn't terminate correctly. Aborting");

	return 0;
}

/*
 * igw_replace_default_gateways: sets the default gw route to reach the
 * Internet. The route utilises multipath therefore there are more than one
 * gateway which can be used to reach the Internet, these gateways are choosen
 * from the `igws' llist.
 * On error -1 is returned.
 */
int igw_replace_default_gateways(inet_gw **igws, int *igws_counter, 
		inet_gw **my_igws, int max_levels, int family)
{
	inet_gw *igw;
	inet_prefix to;

	struct nexthop *nh=0, *nehop;
	int ni, nexthops, level;

	/* to == 0.0.0.0 */
	inet_setip_anyaddr(&to, family);

	nh=xmalloc(sizeof(struct nexthop)*MAX_MULTIPATH_ROUTES);
	memset(nh, 0, sizeof(struct nexthop)*MAX_MULTIPATH_ROUTES);
	ni=0; /* nexthop index */

	/* 
	 * If we are sharing our Internet connection use, as the primary 
	 * gateway `me.internet_gw'.
	 */
	if(server_opt.share_internet && me.inet_connected) {
		memcpy(&nh[ni].gw, &server_opt.inet_gw, sizeof(inet_prefix));
		nh[ni].dev=server_opt.inet_gw_dev;
		nh[ni].hops=255-ni;
		ni++;
	}

	for(level=0; level<max_levels; level++) {
		
		/* Reorder igws[level] */
		igw_order(igws, igws_counter, my_igws, level);

		igw=igws[level];
		nexthops=MAX_MULTIPATH_ROUTES/max_levels;
		if(server_opt.share_internet)
			nexthops--;

		/* Take the first `nexthops'# gateways and add them in `nh' */
		list_for(igw) {
			if(ni >= nexthops)
				break;

			/* Skip gateways which have a bandwidth too small */
			if(igw->bandwidth < MIN_CONN_BANDWIDTH)
				continue;

			if(igw->node->flags & MAP_ME && !server_opt.share_internet) {
				if(level)
					continue;

				memcpy(&nh[ni].gw, &server_opt.inet_gw, sizeof(inet_prefix));
				nh[ni].dev=server_opt.inet_gw_dev;
				nh[ni].hops=255-ni;
			} else {
				nehop=rt_build_nexthop_gw(igw->node, (map_gnode *)igw->node, level, 1);
				if(!nehop) {
					debug(DBG_NORMAL, "igw:: Cannot get the gateway for "
							"the (g)node: %d of level: %d"
							, igw->gid, level);
					continue;
				}
				nehop->hops=255-ni;
				memcpy(&nh[ni], nehop, sizeof(struct nexthop));
				if(nehop)
					xfree(nehop);
			}
			ni++;
		}
	}
	nh[ni].dev=0;

	return 0;
}

char *pack_inet_gw(inet_gw *igw, char *pack)
{
	char *buf;

	buf=pack;

	memcpy(buf, &igw->gid, sizeof(u_char));
	buf+=sizeof(u_char);

	memcpy(buf, &igw->bandwidth, sizeof(u_char));
	buf+=sizeof(u_char);

	return pack;
}

inet_gw *unpack_inet_gw(char *pack, inet_gw *igw)
{
	char *buf=pack;

	memcpy(&igw->gid, buf, sizeof(u_char));
	buf+=sizeof(u_char);

	memcpy(&igw->bandwidth, buf, sizeof(u_char));
	buf+=sizeof(u_char);

	return igw;
}

/*
 * pack_igws: it packs the each `igws[`level']' llist and sets the package size
 * in `pack_sz'. The package is returned, otherwise, on error, NULL is the
 * value returned.
 */
char *pack_igws(inet_gw **igws, int *igws_counter, int levels, int *pack_sz)
{
	struct inet_gw_pack_hdr hdr;
	inet_gw *igw;
	
	int lvl;
	char *pack, *buf;

	memset(&hdr, 0, sizeof(struct inet_gw_pack_hdr));

	/* 
	 * Fill the pack header and calculate the total pack size 
	 */
	hdr.levels=levels;
	for(lvl=0; lvl<levels; lvl++)
		hdr.gws[lvl]=INET_GW_PACK_SZ*igws_counter[lvl];
	*pack_sz=IGWS_PACK_SZ(&hdr);

	buf=pack=xmalloc(*pack_sz);

	memcpy(buf, &hdr, sizeof(struct inet_gw_pack_hdr));
	buf+=sizeof(struct inet_gw_pack_hdr);

	/* Pack `igws' */
	for(lvl=0; lvl<levels; lvl++) {
		igw=igws[lvl];
		list_for(igw) {
			pack_inet_gw(igws[lvl], buf);
			buf+=INET_GW_PACK_SZ;
		}
	}

	return pack;
}

/*
 * unpack_igws: upacks what pack_igws() packed.
 * `pack' is the package which is `pack_sz' big.
 * The pointer to the unpacked igws are stored in `new_igws' and 
 * `new_igws_counter'. 
 * On error -1 is returned.
 */
int unpack_igws(char *pack, size_t pack_sz,
		map_node *int_map, map_gnode **ext_map, int levels,
		inet_gw ***new_igws, int **new_igws_counter)
{
	struct inet_gw_pack_hdr *hdr;
	inet_gw *igw, **igws;
	
	size_t sz;
	int i, lvl=0, *igws_counter;
	char *buf;

	hdr=(struct inet_gw_pack_hdr *)pack;
	sz=IGWS_PACK_SZ(hdr);

	/* Verify the package header */
	if(sz != pack_sz || sz > MAX_IGWS_PACK_SZ(levels) || 
			hdr->levels > levels) {
		debug(DBG_NORMAL, "Malformed igws package");
		return -1;
	}

	init_igws(&igws, &igws_counter, levels);

	buf=pack+sizeof(struct inet_gw_pack_hdr);
	for(lvl=0; lvl<hdr->levels; lvl++) {
		for(i=0; i<hdr->gws[lvl]; i++) {
			igw=xmalloc(sizeof(inet_gw));

			unpack_inet_gw(buf, igw);
			igw->node = node_from_pos(igw->gid, int_map);
			clist_add(&igws[lvl], &igws_counter[lvl], igw);

			buf+=INET_GW_PACK_SZ;
		}
	}
		
	*new_igws=igws;
	*new_igws_counter=igws_counter;
	return 0;
}
