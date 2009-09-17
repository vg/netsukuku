#!/bin/bash

out_prefix=$1

# Add to PATH the directory where this script resides.
SCRIPT_IS_HERE=$(dirname $(which $0))
if [ $SCRIPT_IS_HERE = '.' ]
then
 SCRIPT_IS_HERE=$(pwd)
fi
PATH=${PATH}:${SCRIPT_IS_HERE}

node=$(uname -n)
outdir=${out_prefix}/${node}
mkdir -p ${outdir}

launch(){
 # Launches a prg in background. Saves output in outdir.
 # Saves in variables the PID of the prg and the file used as output.
 prg_name=${1}
 if ( echo ${1} | grep 'python' >/dev/null )
 then
  prg_name=${2}
 fi
 prg_name=$(basename ${prg_name})
 output=${outdir}/${prg_name}.log
 $* >${output} 2>&1 &
 LAUNCH_PID=$!
 LAUNCH_OUTPUT=${output}
}

nics=$(net_info.bash | grep ^nic | cut -d ' ' -f 2)
cd /hosthome/netsukuku/pyntk
launch /opt/stackless/bin/python2.6 ntkd -i ${nics} -vvvv
NTKD_PID=${LAUNCH_PID}
NTKD_OUTPUT=${LAUNCH_OUTPUT}
echo 'Gone...'

