#!/bin/bash
FILE_LINK=$(mktemp)
FILE_ADDR=$(mktemp)
FILE_ROUTE=$(mktemp)
ip link > ${FILE_LINK}
ip addr show eth0 > ${FILE_ADDR}
ip route > ${FILE_ROUTE}
nics=$(cat ${FILE_LINK} | grep '^...eth' | cut -d ' ' -f 2 | cut -d ':' -f 1)
my_addr=$(cat ${FILE_ADDR} | grep 'inet ' | cut -d ' ' -f 6 | cut -d '/' -f 1)
neighbours=$(cat ${FILE_ROUTE} | grep -E -v " (via|src)" | cut -d ' ' -f 1)
gnode1=$(cat ${FILE_ROUTE} | grep -E " (via|src) " | cut -d ' ' -f 1 | grep -v /)
gnode2=$(cat ${FILE_ROUTE} | grep -E " (via|src) " | cut -d ' ' -f 1 | grep /24 | cut -d / -f 1)
gnode3=$(cat ${FILE_ROUTE} | grep -E " (via|src) " | cut -d ' ' -f 1 | grep /16 | cut -d / -f 1)
gnode4=$(cat ${FILE_ROUTE} | grep -E " (via|src) " | cut -d ' ' -f 1 | grep /8 | cut -d / -f 1)

for i in $nics
do
 echo nic $i
done

if [ $my_addr ]
then
 echo addr $my_addr
fi

for i in $neighbours
do
 echo neighbour $i
done

for i in $gnode1
do
 echo gnode_1 $i
done

for i in $gnode2
do
 echo gnode_2 $i
done

for i in $gnode3
do
 echo gnode_3 $i
done

for i in $gnode4
do
 echo gnode_4 $i
done

rm ${FILE_LINK}
rm ${FILE_ADDR}
rm ${FILE_ROUTE}

