#include <string.h>
#include <phtread.h>
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

int main(int argc, char **argv)
{
	/*This shall be the main flow:*/

	/*Init stuff
	if_init();
	map_init();
	curme_init();
	*/

	init_radar();
	close_radar();

	/*Now we hook in the Netsukuku network
	netsukuku_hook();
	*/

	/*These are the main threads that keeps Netsukuku up & running.
	thread(connection_wrapper());
	thread(radar());
	thread(daemon_udp());
	daemon_tcp(); <<-- here we use this self process for the tcp_daemon
	*/
}
