
#include "map.h"

map_node *int_map;

/* gen_rnd_map: Generate Random Map*/
void gen_rnd_map(int start_node) 
{
	int i=start_node, r, e, rnode_rnd, ms_rnd;
	map_rnode rtmp;
	
	if(i > MAXGROUPNODES)
		i=(rand()%(MAXGROUPNODES-0+1))+0;
	
	for(;;) {
		
		r=(rand()%(MAXLINKS-0+1))+0;          /*rnd_range algo: (rand()%(max-min+1))+min*/
		int_map[i].flags!=MAP_SNODE;
		for(e=0; e<=r; e++) {
			memset(&rtmp, '\0', sizeof(map_node));
			rnode_rnd=(rand()%(MAXGROUPNODES-0+1))+0;
			rtmp.r_node=nr;
			ms_rnd=(rand()%((MAXRTT*1000)-0+1))+0;
			rtmp.rtt.tv_usec=ms_rnd*1000;
			map_rnode *rnode_add(int_map[i], &rtmp);

			if(int_map[rnode_rnd] & ~MAP_SNODE)
				gen_rnd_map(rnode_rnd);
		}
			
		
	}
}

int main()
{
	int i=(rand()%(MAXGROUPNODES-0+1))+0;
	
	int_map=init_map(0);
	gen_ran_map(i);


	
}

/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
/****/
