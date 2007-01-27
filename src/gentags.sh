#!/bin/bash
#
# gentags.sh
# 
# Generate every possible tag file by examining only the files residing in the
# CVS repository.
#

exclude_files='~$|\<tags\.woc$|\<tags$|\<TAGS$|\<tags\.rev\.woc$|\<cscope.out$|\<CVS/$'

list_files() {
	local from=$1
	local to=$2

	for i in `cat $from/CVS/Entries | grep -v "$exclude_files" |  cut -d "/" -f 2`
	do
		if [ -d $from/$i ]
		then
			list_files $from/$i
		elif [ -f $from/$i ]
		then
			echo $from/$i
		fi
	done
}

tfile=`tempfile`
export WOC_FILE_LIST="$tfile"
list_files . > $tfile
echo "-- Cscope"
cscope -b -i$tfile
echo "-- Ctags"
ctags -L $tfile
echo "-- Woctags"
woctags.sh
rm -f $tfile
