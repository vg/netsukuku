#include "t2.h"

/*
 * tp_mask_cmp
 * -----------
 *
 * Returns the hamming distance between the {-tpmask_t-} `a' and `b'.
 */
int tp_mask_cmp(tpmask_t a, tpmask_t b)
{
	int i, hamming=0;

	for(i=0; i<MAX_TP_HOPS/8; i++)
		hamming+=count_bits(a.mask[i] ^ b.mask[b]);

	return hamming;
}

/*
 * tp_is_similar
 * -------------
 *
 * Returns 1 if `X' is similar to `Y', 0 otherwise.
 */
int tp_is_similar(tpmask_t X, tpmask_t Y)
{
	int bX, bY;
	int i, hamming, n;

	bX=bY=0;

	for(i=0; i<MAX_TP_HOPS/8; i++) {
		bX+=count_bits(X.mask[i]);
		bY+=count_bits(Y.mask[i]);
	}

	n = bX > bY ? bY : bX;

	hamming = tp_mask_cmp(X, Y);
	
	return hamming <= HAMD_MIN_EQ(n);
}

/*
 * tp_mask_set
 * -----------
 *
 * Sets the `bitpos'-th bit of the tpmask `mask' to `bit'.
 * `bit' can be zero or a non zero value. In the latter case it is
 * interpreted as one.
 */
void tp_mask_set(tpmask_t *mask, int bitpos, u_char bit)
{
#ifdef DEBUG
	if(bitpos >= MAX_TP_HOPS)
		fatal(ERROR_MSG "Too many bits", ERROR_FUNC);
#endif

	if(bit)
		SET_BIT(mask->mask, bitpos)
	else
		CLR_BIT(mask->mask, bitpos);
}

/*
 * tp_mask_test
 * ------------
 *
 * Returns one if the `bitpos'-th bit of `mask' is set, otherwise zero is
 * returned.
 */
int tp_mask_test(tpmask_t *mask, int bitpos)
{
#ifdef DEBUG
	if(bitpos >= MAX_TP_HOPS)
		fatal(ERROR_MSG "Too many bits", ERROR_FUNC);
#endif

	return TEST_BIT(mask->mask, bitpos);
}
