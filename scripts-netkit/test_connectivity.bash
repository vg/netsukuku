#!/bin/bash

# Add to PATH the directory where this script resides.
SCRIPT_IS_HERE=$(dirname $(which $0))
if [ $SCRIPT_IS_HERE = '.' ]
then
 SCRIPT_IS_HERE=$(pwd)
fi
PATH=${PATH}:${SCRIPT_IS_HERE}

FILE_INFO=$(mktemp)
collect_ips.bash > ${FILE_INFO}
echo "Testing "$(cat ${FILE_INFO} | wc -l)" nodes:"
for i in $(cat ${FILE_INFO}); do ping -c 1 &>/dev/null $i && echo $i ok || echo $i error; done

