#include <string.h>
#include <phtread.h>
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

#include <unistd.h>
#include <errno.h>
#include <getopt.h>
	      
extern int errno;
extern char *optarg;
extern int optind, opterr, optopt;


int init_load_maps(void)
{
	if(!(me.int_map=load_map(server_opt.int_map_file, &me.cur_node)))
		me.int_map=init_map(0);
		
	if(!(me.ext_map=load_extmap(server_opt.ext_map_file, &me.cur_quadg)))
		me.ext_map=init_extmap(GET_LEVELS(my_family), 0);

	if(!(me.bnode_map=load_bmap(server_opt.bnode_map_file, me.ext_map, &me.bmap_nodes)))
		bmap_level_init(GET_LEVELS(my_family), &me.bnode_map, &me.bmap_nodes);
}

int save_maps(void)
{
	save_map(me.int_map, me.cur_node, server_opt.int_map_file);
	save_bmap(me.bnode_map, me.bmap_nodes, me.ext_map, me.cur_quadg, server_opt.bnode_map_file);
	save_extmap(me.ext_map, MAXGROUPNODE, me.cur_quadg, server_opt.ext_map_file);
}

int free_maps(void)
{
	bmap_level_free(me.bnode_map, me.bmap_nodes);
	free_extmap(me.ext_map, GET_LEVELS(my_family), 0);
	free_map(me.int_map, 0);
}

int fill_default_options(void)
{
	server_opt.dev=0;
	server_opt.family=AF_INET;
	strncpy(server_opt.int_map_file, INT_MAP_FILE, NAME_MAX);
	strncpy(server_opt.ext_map_file, EXT_MAP_FILE, NAME_MAX);
	strncpy(server_opt.bnode_map_file, BNODE_MAP_FILE, NAME_MAX);

	server_opt.daemon=1;
	server_opt.dbg_lvl=0;

	server_opt.max_connections=MAX_CONNECTIONS;
	server_opt.max_accepts_per_host=MAX_ACCEPTS;
	server_opt.max_accepts_per_host_time=FREE_ACCEPT_TIME;
}

void parse_options(int argc, char **argv)
{
	int c;
	int digit_optind = 0;

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"dev", 1, 0, 'd'},
			{"ipv6", 0, 0, '6'},
			{"ipv4", 0, 0, '4'},
			{"int_map", 1, 0, 'I'},
			{"ext_map", 1, 0, 'E'},
			{"bnode_map", 1, 0, 'B'},
			{"no-daemon", 0, 0, 'D'},
			{"debug", 0, 0, 'd'},
			{"version", 0, 0, 'v'},
			{0, 0, 0, 0}
		};

		c = getopt_long (argc, argv,"d:I:E:B:hvd64D", long_options, &option_index);
		if (c == -1)
			break;

		switch(c)
		{
			case 'v':
				printf("%s\n",VERSION);
				exit(0); 
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case '4':
				server_opt.family=AF_INET;
				break;
			case '6':
				server_opt.family=AF_INET6;
				break;
			case 'I': 
				strncpy(server_opt.int_map_file, optarg, NAME_MAX); 
				break;
			case 'E': 
				strncpy(server_opt.ext_map_file, optarg, NAME_MAX);
				break;
			case 'B': 
				strncpy(server_opt.bnode_map_file, optarg, NAME_MAX); 
				break;
			case 'D':
				server_opt.daemon=0;
				break;
			case 'd':
				server_opt.dbg_lvl++;
				break;
			default:
				break;
		}
	}

	if (optind < argc) {
		usage();
		exit(0);
	}

}

void init_netsukuku(void)
{
	char *dev;
	memset(&me, 0, sizeof(struct current));
	srand(time(0));	

	log_init(argv[0], server_opt.dbg_lvl, 1);
	/*TODO:
	signal();
	*/
	maxgroupnode_level_init();
	

	my_family=server_opt.family;
	ntk_udp_port=DEFAULT_NTK_UDP_PORT;
	ntk_tcp_port=DEFAULT_NTK_TCP_PORT;
	if(!dev=if_init(server_opt.dev, &me.cur_dev_idx))
		fatal("Cannot initialize the %s device", server_opt.dev);
	else
		strncpy(me.cur_dev, dev, IFNAMSIZ);

	qspn_init(GET_LEVELS(my_family));

	init_radar();

	init_load_maps();

	debug(DBG_NORMAL, "ACPT: Initializing the accept_tbl: \n"
			"	max_connections: %d,\n"
			"	max_accepts_per_host: %d,\n"
			"	max_accept_per_host_time: %d", 
			server_opt.max_connections, 
			server_opt.max_accepts_per_host, 
			server_opt.max_accepts_per_host_time);
	init_accept_tbl(server_opt.max_connections, 
			server_opt.max_accepts_per_host, 
			server_opt.max_accepts_per_host_time);
}

void destroy_netsukuku(void)
{
	save_maps();
	free_maps();
	maxgroupnode_level_free();
	close_radar();
	destroy_accept_tbl();
}

int main(int argc, char **argv)
{
	pthread_t daemon_radar_thread, daemon_udp_thread, daemon_tcp_thread;
	pthread_attr_t t_attr;
	
	/*
	 * The main flow shall never be stopped, and the sand of time will be
	 * revealed.
	 */

#ifdef QSPN_EMPIRIC
	error("QSPN_EMPIRIC is activated!!!!");
	exit(1);
#endif

	fill_default_options();
	parse_options(argc, argv);
	init_netsukuku();
	
	/* Now we hook in the Netsukuku network */
	netsukuku_hook(me.cur_dev);

	log_init(argv[0], server_opt.dbg_lvl, 0);

	if(server_opt.daemon)
		if(daemon(0, 0) == -1) {
			error("Impossible to daemonize: %s.", strerror(errno));
		}

	pthread_attr_init(&t_attr);
	pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

	/* 
	 * These are the daemons, the main threads that keeps Netsukuku 
	 * up & running. 
	 */
	pthread_create(&daemon_radar_thread, &t_attr, radar_daemon, NULL);
	pthread_create(&daemon_udp_thread,   &t_attr, udp_daemon,   NULL);
	tcp_daemon(); /* We use this self process for the tcp_daemon */

	pthread_attr_destroy(&t_attr);
	destroy_netsukuku();
}
