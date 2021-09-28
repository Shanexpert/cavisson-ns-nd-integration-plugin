#!/bin/bash

if [ "XX$NS_WDIR" == "xx"  ]
then
        echo "ERROR: NS_WDIR Not set. Exiting."
        exit 1
fi

cleanup()
{
  kill -9 $SSH_AGENT_PID 2>/dev/null
  rm -f $tmpdir/.check_health.pid
}


blade_state_log="$NS_WDIR/server_management/logs/blade_state.log"
check_health_log="$NS_WDIR/server_management/logs/check_health.log"
check_health_err="$NS_WDIR/server_management/logs/check_health_err.log"
batch=15 #check this number of servers in one go.
sleep_msec=10 #sleep for this number of seconds if pre_stats_check fails
tmpdir=$NS_WDIR/server_management/tmp
key=$NS_WDIR/server_management/conf/key

if [[ ! -f $key ]]; then
  echo "key file does not exists. Exiting." >&2
  exit 1
fi
pre_stats_check()
{
  if [[ ! -e $NS_WDIR/server_management/bin/check_health.sh ]]; then
    echo "$(date)|check_health.sh file does not exist. Skipping for $sleep_msec" >> $check_health_err
    sleep $sleep_msec
    continue
  fi
  if ! pg_isready >/dev/null; then
    echo "$(date)|Postgresql not running. Skipping for $sleep_msec" >> $check_health_err
    sleep $sleep_msec
    continue
  fi
}

if [ "x$1" == "xstop" ]; then
  if [[ ! -e $tmpdir/.check_health.pid ]]; then
    echo "ERROR: checkhealth not running"
    exit 1
  else
    kill $(cat $tmpdir/.check_health.pid) && echo "stopping pid $(cat $tmpdir/.check_health.pid)" && rm -rf $tmpdir/.check_health.pid
    exit 0
  fi

fi

if [ -e $tmpdir/.check_health.pid ]; then
  echo "check_health is already running. check pid $(cat $tmpdir/.check_health.pid)"
  exit 1
fi

#if [ -e key ]; then
#  eval $(ssh-agent)
#  ssh-add key
#else
#  echo "key file does not exist"
#  exit 1
#fi


echo $$ > $tmpdir/.check_health.pid
trap "cleanup ; exit 2" 1 2 3 6 9
while true; do
  start=0
  pre_stats_check
  psql -X -A -U postgres -d demo -t -c "select distinct server_ip from allocation where status='t'" > $tmpdir/server_ip.temp 2>/dev/null
  readarray -t ip < $tmpdir/server_ip.temp
  arr_len=${#ip[@]}
  let iterations=($arr_len+$batch-1)/$batch
  for (( i = 0; i < $iterations; i++ )); do
    for (( j = $start; j < $(expr $start + $batch); j++ )); do
      sh $NS_WDIR/server_management/bin/check_health_new.sh ${ip[$j]} &
    done
    wait
    start=$(expr $start + $batch)
  done
done
