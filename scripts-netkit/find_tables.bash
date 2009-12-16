#!/bin/bash

# refresh ARP table
FILE_ROUTE=$(mktemp)
ip route > ${FILE_ROUTE}
neighbours=$(cat ${FILE_ROUTE} | grep -E -v " (via|src)" | cut -d ' ' -f 1)
for i in $neighbours
do
 ping -c 1 $i &> /dev/null
done

# from ARP table lookup known MAC addresses
for an_ip in $(ip neigh | cut -d ' ' -f 1)
do
 an_mac=$(ip neigh | grep $an_ip | cut -d ' ' -f 5)
 mark_num=$(iptables -t mangle -L | grep $an_mac | grep "MARK xset" | cut -c 94-97)
 table_num=$(ip rule | grep $mark_num | cut -c 36-)
 echo From gateway $an_ip routing table $table_num
done

