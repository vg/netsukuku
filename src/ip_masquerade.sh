#!/bin/bash
#
# ip_masquerade.sh: sets IP masquerading.
# This script is executed by NetsukukuD at its start (if in restricted mode).
# If all went ok the exit status is 0, otherwise NetsukukuD will stop.

OS=`uname`

if [ $OS == "Linux" ]
then
	# Flush all the NAT rules
	iptables -F POSTROUTING -t nat  

	# Masquerade
	iptables -A POSTROUTING -t nat -j MASQUERADE
	exit $?
fi

exit 1
