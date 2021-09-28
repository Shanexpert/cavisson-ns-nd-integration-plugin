#!/bin/bash
logs_home=$NS_WDIR/server_management/logs
ccvp_billing()
{
  psql -X  -A -U postgres -d demo -t -c "select row_number() over(order by downtime_epoch desc), server_name, server_ip, vendor, status, uptime, downtime, duration from billing_cc"
}

vm_billing()
{
  psql -X  -A -U postgres -d demo -t -c "select row_number() over(order by uptime_epoch desc), server_name, server_ip, vendor, status, uptime, downtime, duration from billing order by row_number() over()"
}

assignment()
{
# cat -n /home/netstorm/work/server_management/logs/assignment.log | tac
tac $logs_home/assignment.log > file.temp
c=1
while read line
do
  echo "$c|$line"
  c=$(($c+1))
done < file.temp
rm file.temp 2>/dev/null
}

blade_state()
{
tac $logs_home/blade_state.log | grep -v -i "Cannot" > file.temp
c=1
while read line
do
  echo "$c|$line"
  c=$(($c+1))
done < file.temp
rm file.temp 2>/dev/null
}

blade_state_failure()
{
tac $logs_home/blade_state.log | head -100 | grep -i "Cannot" > file.temp
c=1
while read line
do
  echo "$c|$line"
  c=$(($c+1))
done < file.temp
rm file.temp 2>/dev/null
}

addDelete()
{
  tac $logs_home/addDelete.log | head -500 > file.temp
  c=1
  while read line
  do
    echo "$c|$line"
    c=$(($c+1))
  done < file.temp
  rm file.temp 2>/dev/null
}

while getopts cvadbf  arg
do
  case $arg in
    c) ccvp_billing;;
    v) vm_billing;;
    a) assignment;;
    d) addDelete;;
    b) blade_state;;
    f) blade_state_failure;;
    *) echo "Invalid option";;
  esac
done
