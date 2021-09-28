#!/bin/bash
read_config()
{
  conf_dir=$NS_WDIR/server_management/conf
  config_file=$conf_dir/NCP.conf
  tmpdir=$NS_WDIR/server_management/tmp
  nmap_args=$(awk '/\[nmap\]/{flag=1;next}/\[*\]/{flag=0}flag' $config_file|awk -F'=' '/nmap_args/{ sub(/#.*/,""); print $2}')
  pg_database=demo
  pg_user=postgres
  pg_port=5432
  batch=$(awk '/\[nmap\]/{flag=1;next}/\[*\]/{flag=0}flag' $config_file|awk -F'=' '! /^#/ && ! /^$/ && /batch/{gsub("\\s+", "");sub(/#.*/,""); print $2}')
  timeout=$(awk '/\[nmap\]/{flag=1;next}/\[*\]/{flag=0}flag' $config_file|awk -F'=' '! /^#/ && ! /^$/ && /timeout/{gsub("\\s+", "");sub(/#.*/,""); print $2}')
  [[ "x$timeout" == "x" ]] && timeout=1800s
  nmapdir=$NS_WDIR/server_management/nmap
  nmap_scan_report=$nmapdir/reports
  nmap_scan_report_html=$NS_WDIR/logs/nmap/reports
  nmap_logs=$NS_WDIR/server_management/logs/nmap.log
}


compare()
{
  local host_ip=$1
  security_group=$2
  local rules=$(awk '/\[securityGroups\]/{flag=1;next}/\[*\]/{flag=0}flag' $config_file|awk '! /^#/ && ! /^$/{gsub("\\s+", "");sub(/#.*/,""); print}'|grep -w $security_group)
  local scanned_ports=( $(awk -v ip=$host_ip '$0 ~ ip && $0 ~ /Ports:/ && ! /timeout/ {
    FS=":"
    gsub("\\s+", "")
    len=split($3, data, ",")
    for (i=1; i <= len; i++) {
      split(data[i], ports, "/")
      printf ("%d\n", ports[1])
    }
  }' $nmap_scan_report/nmap_scan_report.${epoch}.gnmap) )
  if [[ ${#scanned_ports[@]} -eq 0 ]]; then
    return
  fi
  declare -a declared_ports
  local range=$(echo ${rules}|cut -d'=' -f2)
  local t=($(echo $range | tr ',' "\n"))
  for (( k = 0; k < ${#t[@]}; k++ )); do
    if [[ ${t[$k]} =~ "-" ]]; then
      local start=$(echo ${t[$k]}|cut -d'-' -f 1)
      local end=$(echo ${t[$k]}|cut -d'-' -f 2)
      for l in `seq $start $end`; do
        declared_ports+=("$l")
      done
    else
      declared_ports+=("${t[$k]}")
    fi
  done

  for m in ${scanned_ports[@]}; do
    if [[ " ${declared_ports[*]} " != *" ${m} "* ]]; then
      vulnerable_port_details=$(awk -v ip=$host_ip -v vulnerable_port=${m} '$0 ~ ip && $0 ~ /Ports:/ && ! /timeout/ {
        FS=":"
        gsub("\\s+", "")
        len=split($3, data, ",")
        for (i=1; i <= len; i++) {
          split(data[i], ports, "/")
          if (ports[1]==vulnerable_port) {
            printf ("%d|%s|%s|%s|%s\n", ports[1], ports[2], ports[3], ports[5],ports[7])
          }
        }
      }' $nmap_scan_report/nmap_scan_report.${epoch}.gnmap)
      vport=$(echo $vulnerable_port_details|cut -d'|' -f1)
      vport_state=$(echo $vulnerable_port_details|cut -d'|' -f2)
      vport_proto=$(echo $vulnerable_port_details|cut -d'|' -f3)
      vport_service=$(echo $vulnerable_port_details|cut -d'|' -f4)
      vport_service_version=$(echo $vulnerable_port_details|cut -d'|' -f4)
      until psql -X -A -t -d $pg_database -U $pg_user << EOF >/dev/null 2>&1
      UPDATE security_scan SET
      vport= ARRAY_APPEND(vport,'$vport'),
      vport_state= ARRAY_APPEND(vport_state, '$vport_state'),
      vport_proto= ARRAY_APPEND(vport_proto, '$vport_proto'),
      vport_service= ARRAY_APPEND(vport_service, '$vport_service'),
      vport_service_version= ARRAY_APPEND(vport_service_version, '$vport_service_version')
      WHERE server_ip='$host_ip' AND start_time='$(date -d @$epoch)';
EOF
      do
        echo "$(date)|PG not ready" >> $nmap_logs
        sleep 1
      done
    fi
  done
}

read_config

if [ "x$1" == "xstop" ]; then
  if [ -e $tmpdir/.security_scan.pid ]; then
    kill $(cat $tmpdir/.security_scan.pid) && echo "stopping pid $(cat $tmpdir/.security_scan.pid)" && rm -f $tmpdir/.security_scan.pid
    exit 0
  else
    echo "ERROR: security_scan is not running"
    exit 1
  fi
fi
if [ "x$1" == "xstatus" ]; then
  if [ -e $tmpdir/.security_scan.pid ]; then
    echo "security scan is running."
    ps p $(cat $tmpdir/.security_scan.pid)
    exit 0
  else
    echo "security scan is not running."
    exit 0
  fi
fi
if [ -e $tmpdir/.security_scan.pid ]; then
  echo "security_scan is already running. Check pid $(cat $tmpdir/.security_scan.pid)"
  exit 1
else
  echo $$ > $tmpdir/.security_scan.pid
fi

while true; do
  start=0
  hosts=( `psql -X -A -t -d $pg_database -U $pg_user << EOF
  SELECT distinct servers.server_name,
    servers.server_ip,
    CASE WHEN servers.security_group is null
      OR servers.security_group=''
    THEN 'generator'
    ELSE servers.security_group
    END
  FROM servers JOIN allocation USING (server_name)
  WHERE allocation.status='t'
  ORDER BY servers.server_name;
EOF` )
  if [[ $? -ne 0 ]]; then
    sleep 1 && continue
  fi
  arr_len=${#hosts[@]}
  let iterations=($arr_len+$batch-1)/$batch
  for (( i = 0; i < $iterations; i++ )); do
    epoch=$(date '+%s')
    unset ip
    for (( j = $start; j < $(expr $start + $batch); j++ )); do
      ip+=$(echo ${hosts[$j]}|cut -d'|' -f 2) ; ip+=" "
    done
    timeout $timeout nmap -oA $nmap_scan_report/nmap_scan_report.${epoch} $nmap_args $ip 2>&1 >/dev/null
    for (( j = $start; j < $(expr $start + $batch); j++ )); do
      if xsltproc $nmap_scan_report/nmap_scan_report.${epoch}.xml -o $nmap_scan_report_html/nmap_scan_report.${epoch}.html 2>/dev/null; then
        name=$(echo ${hosts[$j]}|cut -d'|' -f 1)
        ip=$(echo ${hosts[$j]}|cut -d'|' -f 2)
        security_group=$(echo ${hosts[$j]}|cut -d'|' -f 3)
        if [[ "x$name" != "x" ]]; then
          until psql -X -A -t -d $pg_database -U $pg_user << EOF >/dev/null 2>&1
            INSERT into security_scan(server_name, server_ip, start_time, report_file, security_group, ismailsent)
            VALUES('$name','$ip','$(date -d @$epoch)','nmap_scan_report.${epoch}.html','$security_group','false');
EOF
          do
            echo "$(date)|PG not ready" >> $nmap_logs
            sleep 5
          done
          compare $ip $security_group
        fi
      fi
    done
    start=$(expr $start + $batch)
  done
done
