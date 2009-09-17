#!/bin/bash

# call this script like this:
#     view_logs 3 guest1.log guest2.log
# to produce (at stdout) the logs for guest1
# and guest2 up to level 3 of loggings, sorted.

lev=$1
shift

OUTFILE=$(mktemp)
TEMPFILE=$(mktemp)

indent=""
for i in $*
do
 sed "s/^/${indent}/g" ${i} >>${OUTFILE}
 indent=$indent"   "
done

if [ $lev -lt 4 ]
then
 cat ${OUTFILE} | grep -v "ULTRADEBUG:" > ${TEMPFILE}
 cat ${TEMPFILE} > ${OUTFILE}
fi

if [ $lev -lt 3 ]
then
 cat ${OUTFILE} | grep -v "DEBUG:" > ${TEMPFILE}
 cat ${TEMPFILE} > ${OUTFILE}
fi

if [ $lev -lt 2 ]
then
 cat ${OUTFILE} | grep -v "INFO:" > ${TEMPFILE}
 cat ${TEMPFILE} > ${OUTFILE}
fi

if [ $lev -lt 1 ]
then
 cat ${OUTFILE} | grep -v "WARNING:" > ${TEMPFILE}
 cat ${TEMPFILE} > ${OUTFILE}
fi

sort ${OUTFILE}
rm ${OUTFILE}
rm ${TEMPFILE}

