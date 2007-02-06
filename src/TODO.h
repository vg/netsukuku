/*\
 * TODO.h
 * ------
 *
 * Detailed description of various TODOs sparse in the sources. 
 * Use cscope (or grep) to see where they are cited, or better, use {-WoC-}.
\*/

/*
 * Use an ad-hoc struct for each root node (of any level).
 *
 * This struct will be accessed by the radar, the map functions, qspn.c.
 *
 * We can store detailed info for each link.
 *
 * The ext_rnodes struct can be merged here.
 */
#define TODO_ROOT_NODE_STRUCT

/*
 * Use {-counter_cmp-} for all the counters, f.e. in andna.c there's the
 * hnames_updates counter.
 */
#define TODO_COUNTER_CMP

/*
 * Observe the changes of a rtt8_t variable instead of using RTT_DELTA.
 */
#define TODO_RTT_DELTA_RTT8BIT

/*
 * Use me.my_upbw, me.my_dwbw, instead of me.my_bandwidth.
 * More generally, split bandwidth in upload and downlod in igs.c
 */
#define TODO_MY_UPBW

/*
 * |{no brdcast_hdr}|
 *
 * Wipe brdcast_hdr from the Netsukuku sources
 */

/*
 * |{TODO_rnl_nnet}|
 *
 * Integrate rnode_list.nnet in the existing radar.c code.
 */

/*
 * |{TODO_NODE_PRIV_KEY}|
 *
 * RSA *node_priv_key[levels];
 *
 * node_priv_key[0] is the priv_key of localhost.
 * node_priv_key[1] is the priv_ket of the gnode of level 1, where localhost
 * belongs.
 * ...
 */


/*\
 *
 * 	Long term TODOs 
 *
\*/

/*
 * Find a hash function H() such that, given two similar tpmask 
 * (see tpmask_t) A and B, asb(H(A)-H(B)) is a small number.
 *
 * Note: H(A)-H(B) can also be the hamming distance between H(A) and H(B).
 *
 * If such hash exists, then we can save a lot of space in {-map_gw-}, because
 * instead of storing the whole tpmask (which is 32byte large), we'll just
 * save the H(A) and H(B).
 *
 * Note: sizeof(H(A)) < 32 byte
 */
#define TODO_FUZZY_HASH

/*
 * |{TODO_gcount}|
 *
 * map_gnode.gcount is an unsigned int (32 bit).
 * If we are dealing with ipv6, it should be an 128 bit number, because
 * if there are more than 4294967295 nodes in Netsukuku, map_gnode.gcount will
 * overflow.
 *
 * The same problem persists for {-qspn_gnode_count-}, and
 * for {-NODES_PER_LEVEL-}.
 *
 * It should be easy to fix this: use uint128_t. However, for now, we don't
 * care.
 */

/*
 * |{TODO rand_valid_ip}|
 *
 * Find a cleaner way to generate a valid random ip
 */
