#!/bin/bash

find_dir_script()
{
 # Find the directory where this script resides.
 SCRIPT=$1
 RET=$(dirname $(which $1))
 if [ $RET = '.' ]
 then
  RET=$(pwd)
 fi
}

make_hosthome_path()
{
 # The passed path is accessible from host.
 # Make it the way it will be accessible from guest.
 PATH_AS_HOST=$1
 HOME_ESCAPED=$(echo $HOME | sed "s/\//\\\\\//g")
 RET=$(echo ${PATH_AS_HOST} | sed "s/^$HOME_ESCAPED/\/hosthome/g")
}

