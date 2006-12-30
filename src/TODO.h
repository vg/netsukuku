

/*
 * Find a hash function H() such that, given two similar tpmask 
 * (see tpmask_t) A and B, asb(H(A)-H(B)) is a small number.
 *
 * Note: H(A)-H(B) can also be the hamming distance between H(A) and H(B).
 *
 * If such hash exists, then we can save a lot of space in :map_gw:, because
 * instead of storing the whole tpmask (which is 32byte large), we'll just
 * save the H(A) and H(B).
 *
 * Note: sizeof(H(A)) < 32 byte
 */
#define TODO_FUZZY_HASH

/*
 * Use bsearch(3) to search in the metrics.gw array (see :MetricArrays:)
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
