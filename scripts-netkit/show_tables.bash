#!/bin/bash

# Add to PATH the directory where this script resides.
SCRIPT_IS_HERE=$(dirname $(which $0))
if [ $SCRIPT_IS_HERE = '.' ]
then
 SCRIPT_IS_HERE=$(pwd)
fi
PATH=${PATH}:${SCRIPT_IS_HERE}

FILE_INFO=$(mktemp)
find_tables.bash | cut -d ' ' -f 3,6 > ${FILE_INFO}

echo "     Table main"
ip route list table main
for neigh_ip in $(cat ${FILE_INFO} | cut -d ' ' -f 1)
do
 neigh_tab=$(cat ${FILE_INFO} | grep $neigh_ip | cut -d ' ' -f 2)
 echo "     Table $neigh_tab (packets from $neigh_ip)"
 ip route list table $neigh_tab
done

