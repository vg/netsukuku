/*\
 * TODO.h
 * ------
 *
 * Detailed description of various TODOs sparse in the sources. 
 * Use cscope (or grep) to see where they are cited.
\*/

/*
 * Use bsearch(3) to search in the metrics.gw array (see {-MetricArrays-})
 */
#define TODO_BSEARCH_FOR_MAP_GW

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
 * Header for tha pack of the internal map.
 *
 * 	* The header must contain the MAX_METRIC_ROUTES variables
 */
#define TODO_PACK_MAP_HEADER


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
