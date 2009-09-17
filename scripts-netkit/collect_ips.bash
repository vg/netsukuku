#!/bin/bash

# Add to PATH the directory where this script resides.
SCRIPT_IS_HERE=$(dirname $(which $0))
if [ $SCRIPT_IS_HERE = '.' ]
then
 SCRIPT_IS_HERE=$(pwd)
fi
PATH=${PATH}:${SCRIPT_IS_HERE}

FILE_INFO=$(mktemp)
net_info.bash > ${FILE_INFO}
addr=$(cat ${FILE_INFO} | grep ^addr | cut -d ' ' -f 2)
neighs=$(cat ${FILE_INFO} | grep ^neighbour | cut -d ' ' -f 2)
gnodes=$(cat ${FILE_INFO} | grep ^gnode | cut -d ' ' -f 2)
(
 for i in ${addr}
 do
  echo $i
 done
 for i in ${neighs}
 do
  echo $i
 done
 for i in ${gnodes}
 do
  traceroute -a -n -q 1 -w 2 $i 2>/dev/null | grep -v traceroute | grep -v '\*' | sed 's/^.../xxx/g' | cut -d ' ' -f 2
 done
) | sort -u

rm ${FILE_INFO}

