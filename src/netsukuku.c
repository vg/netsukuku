#include <string.h>
#include <phtread.h>
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

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

void init_netsukuku(void)
{
	memset(&me, 0, sizeof(struct current));
	srand(time(0));	

	/*TODO:
	void log_init(char *prog, int dbg, int log_stderr): 
	signal();
	*/
	maxgroupnode_level_init();
	

	my_family=server_opt.family;
	strncpy(me.cur_dev, server_opt.dev, IFNAMSIZ);
	if(!if_init(server_opt.dev, &me.cur_dev_idx))
		fatal("Cannot initialize the %s device", server_opt.dev);

	qspn_init(GET_LEVELS(my_family));
	init_radar();

	init_load_maps();

	init_accept_tbl(MAX_CONNECTIONS, MAX_ACCEPTS, FREE_ACCEPT_TIME);
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
	/*
	 * The main flow shall never be stopped, and the sand of time will be
	 * revealed.
	 */

#ifdef QSPN_EMPIRIC
	error("QSPN_EMPIRIC is activated!!!!");
	exit(1);
#endif

	init_netsukuku();
	
	/* Now we hook in the Netsukuku network */
	netsukuku_hook(me.cur_dev);

	/*These are the main threads that keeps Netsukuku up & running.
	thread(connection_wrapper());
	thread(radar());
	thread(daemon_udp());
	daemon_tcp(); <<-- here we use this self process for the tcp_daemon
	*/

	destroy_netsukuku();
}
