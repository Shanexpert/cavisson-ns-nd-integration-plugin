build_hub=~/etc/build_hub.list
min_hubs=6

cleanup(){
  rm -f /tmp/*.$$
}

check_build_on_hub(){
  if timeout 10s nsu_server_admin -s $ip -g -c "ls /home/cavisson/work/logs/HUB" > /tmp/builds.$ip.$$ 2>/dev/null ; then
    grep "thirdparty.*.bin" /tmp/builds.$ip.$$ | cut -d "." -f 2,3,4,5 | cut -d "_" -f 1 > /tmp/thirdparty.$ip.$$
    grep "netstorm_all.*.bin" /tmp/builds.$ip.$$ | cut -d "." -f 2,3,4,5 | cut -d "_" -f 1 > /tmp/netstorm.$ip.$$
    comm -1 -2 /tmp/thirdparty.$ip.$$ /tmp/netstorm.$ip.$$ | sed 's/[[:space:]]//g' > /tmp/available_version.$ip.$$
  else
    echo "$ip|unable to connect"
  fi
}

for ip in `cat $build_hub|grep -v "^#"`
do
  check_build_on_hub &
done
wait
cat /tmp/available_version.*.$$ | sort | uniq -dc | awk -v min_hubs=$min_hubs '{ if ($1 >= min_hubs) print $2 }'
#echo "4.2.0.95"
cleanup
