
/*This is q super global struct!*/
struct qspn_queue
{
	u_char 		send_qspn;	/*Has qspn to be sent? 0 or 1*/
	struct timeval  t;		/*When the send request was done*/
}qspn_q;
