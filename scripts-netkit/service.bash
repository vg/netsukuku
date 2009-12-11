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

monitor_network()
{
 /hosthome/netsukuku/scripts-netkit/monitor.py &
}

launch_ntkd()
{
 # Launches ntkd in background. All nics are managed. Logs are saved.
 # $1 = Where to place logs;
 #      This is a dir. Inside it we create a dir named after the machine name.
 #      Inside this one we place our logs.
 #      If needed, all the directory structure is created.
 # $2 = Delay before to start;
 #      When we actually launch ntkd, a message appears on the console.
 out_prefix=$1
 DELAY=$2
 node=$(uname -n)
 PATH=${PATH}:/hosthome/netsukuku/scripts-netkit
 nics=$(net_info.bash | grep ^nic | cut -d ' ' -f 2)
 cd /hosthome/netsukuku/pyntk
 run_command="/opt/stackless/bin/python2.6 ntkd -i ${nics} -vvvv"
 compose_output_filename ${out_prefix}/${node} $run_command
 delay_warn_then_log_do $DELAY "ntkd daemon launched." ${outdir} $run_command &
 NTKD_PID=$!
 NTKD_OUTPUT=${output}
}

compose_output_filename()
{
 # Composes a path to a file. Places the path in variable ${output}.
 # If needed, the directory structure is created.
 # $1 is the path to the directory.
 # The rest of the arguments, is the command whose output will be saved
 # in the file. It is used to name the file after the command.
 # E.g.
 #    compose_output_filename /home/joe/logs python ntkd
 # will put /home/joe/logs/ntkd.log in ${output}
 outdir=$1
 prg_name=${2}
 if ( echo ${2} | grep 'python' >/dev/null )
 then
  prg_name=${3}
 fi
 prg_name=$(basename ${prg_name})
 output=${outdir}/${prg_name}.log
 mkdir -p ${outdir}
}

delay_warn_then_log_do()
{
 # Waits the given amount of seconds, writes a message on the console,
 # then executes a command and redirects output.
 # $1 = Delay in seconds.
 # $2 = Message to be display.
 # $3 = Redirection.
 # The rest of the arguments, is the command to be executed
 DELAY=$1
 WARN=$2
 OUTPUT=$3
 shift 3
 sleep $DELAY
 echo $WARN
 $* >> ${output} 2>&1
}

kill_generation()
{
 local myp=$1
 local my_gen_p
 for my_gen_p in $(ps -o pid= --ppid $myp)
 do
  kill_generation $my_gen_p
 done
 kill -HUP $myp
}

