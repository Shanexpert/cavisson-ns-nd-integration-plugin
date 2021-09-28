#!/bin/bash
templatedir=$NS_WDIR/server_management/template
hostfile=$1
service_name=$2
pid=$$
if [[ $# -ne 2 ]]; then
  echo "usage: ncp_update_config <host file> <service name>"
  echo "service name: cmon"
  exit 1
fi
if [[ ! -d $templatedir ]]; then
  echo "Template directory does not exist" >&2
  exit 1
fi
if [[ ! -f $hostfile ]]; then
  echo "Hostfile does not exist." >&2
  exit 1
fi
cmon_env()
{
  timeout 7s nsu_server_admin -s $ip -g -c "mv /tmp/$templatename.${ip}.$pid /home/cavisson/monitors/sys/$templatename"
  if [[ $? -ne 0 ]]; then
    echo "Unable to update cmon.env file" >&2
    continue
  else
    timeout 40s nsu_server_admin -s $ip -g -r >/dev/null
    if [[ $? -eq 0 ]]; then
      echo "$ip|cmon.env updated and cmon restarted successfully."
    else
      echo "$ip|cmon.env updated but unable to restart cmon in 40 sec"
    fi
  fi
}
for i in $(ls $templatedir/*.template)
do
  templatename=$(echo $(basename $i)|rev|cut -d'.' -f2-|rev)
  for j in $(cat $hostfile)
  do
    IFS=,
    export $j
    if [[ -z $ip ]]; then
      echo "Variable ip is missing in host file" >&2
      continue
    fi
    envsubst < $i > /tmp/$templatename.${ip}.$pid
    timeout 7s nsu_server_admin -s $ip -g -F /tmp/$templatename.${ip}.$pid -D /tmp 2>/dev/null
    if [[ $? -ne 0 ]]; then
      echo "$ip|Unable to post template file" >&2
      continue
    else
      if [[ "x$service_name" == "xcmon" ]]; then
        cmon_env
      fi
    fi
    rm -f /tmp/$templatename.${ip}.$pid
  done
done
