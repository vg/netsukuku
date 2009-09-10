#!/bin/bash

if echo "$@" | sed -e 's/\///g' | grep -q '[^a-zA-Z0-9\-\.\ _]'
then
	exit 1
fi

o=`grep "$1" | sed -e "s/$1//" | grep -v '\.info$\|\.pdf$' | sed -e 's/\.//'`

for i in $o; do
	printf "<a href=\"index.php?pag=documentation&amp;file=$2$1.$i\">[$i]</a> "
done
