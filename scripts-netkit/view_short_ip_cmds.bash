#!/bin/bash

# call this script like this:
#     view_short_ip_cmds.bash guest1.log guest2.log
# to produce (at stdout) the logs of "ip" commands for
# guest1 and guest2, sorted.

# Add to PATH the directory where this script resides.
SCRIPT_IS_HERE=$(dirname $(which $0))
if [ $SCRIPT_IS_HERE = '.' ]
then
 SCRIPT_IS_HERE=$(pwd)
fi
PATH=${PATH}:${SCRIPT_IS_HERE}

num=$#

OUTFILE=$(mktemp)
TEMPFILE=$(mktemp)

# produce logs to level 2 (INFO)
view_logs.bash 2 $* >${OUTFILE}

# grep only ip commands
cat ${OUTFILE} | grep ": \/sbin\/ip\|my netid is now [0-9]" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}

# remove time and source-file
cat ${OUTFILE} | sed "s/2009.*\/sbin\///g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}
cat ${OUTFILE} | sed "s/2009.*my netid is now/NETID/g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}

# remove lots of "protocol ntk"
cat ${OUTFILE} | sed "s/ protocol ntk//g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}

# remove lines not useful
cat ${OUTFILE} | grep -v "link set" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}
cat ${OUTFILE} | grep -v "addr show" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}

# abbreviations
cat ${OUTFILE} | sed "s/ip addr flush dev/FLUSH/g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}
cat ${OUTFILE} | sed "s/ip addr add/SETADDR/g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}
cat ${OUTFILE} | sed "s/ip route add/R/g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}
cat ${OUTFILE} | sed "s/dev eth. via/VIA/g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}
cat ${OUTFILE} | sed "s/ip route del/DEL R/g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}
cat ${OUTFILE} | sed "s/ip route change/CHG R/g" >${TEMPFILE}
mv ${TEMPFILE} ${OUTFILE}

for ((i=${num}-1;i>0;i=i-1))
do
 indent=""
 for ((j=0;j<i;j=j+1))
 do
  indent=$indent"   "
 done
 cat ${OUTFILE} | sed "s/^$indent/$indent                        \|  /g" >${TEMPFILE}
 mv ${TEMPFILE} ${OUTFILE}
done

cat ${OUTFILE}

