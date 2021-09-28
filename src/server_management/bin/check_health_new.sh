if [ "XX$NS_WDIR" == "xx"  ]
then
  echo "ERROR: NS_WDIR Not set. Exiting."
  exit 1
fi

ip=$1
pid=$$
blade_state_log="$NS_WDIR/server_management/logs/blade_state.log"
check_health_log="$NS_WDIR/server_management/logs/check_health.log"
check_health_err="$NS_WDIR/server_management/logs/check_health_err.log"
key=$NS_WDIR/server_management/conf/key
mon_script=$NS_WDIR/server_management/bin/server_health.sh
tmpdir=$NS_WDIR/server_management/tmp
if [ ! -d $tmpdir ];
then
 echo "$tmpdir does not exist. Exiting" >$2
 exit 1
fi

if [ -z "$ip" ]; then
  exit 7
fi

cleanup()
{
  rm -f $tmpdir/.*.$ip.$pid $tmpdir/.*.$ip.$pid* $tmpdir/splitted.$ip.$pid*
}


connect_and_execute()
{
  rechability()
  {
    p=$(ping -qc1 -W 2 $ip 2>&1 | awk -F'/' 'END{ print (/^rtt/? $6:"FAIL") }')
    if [[ "x$p" == "xFAIL" ]]; then
      echo "$(date)|$ip|ERROR: Not Reachable" >> $check_health_err
      cleanup
      c_param="Not Reachable"
      exit 1
    fi
  }
  connect_cmon()
  {
    timeout 5s nsu_server_admin -s $ip -g -F $mon_script -D /tmp >/dev/null 2>&1
    if [[ $? -ne 0 ]]; then
      echo "$(date)|$ip|ERROR: Server is not accessible from cmon and ssh" >> $check_health_err
      cleanup
      c_param="Not Accessible"
      exit 2
    else
      timeout 5s nsu_server_admin -s $ip -g -c "sh /tmp/server_health.sh" | grep -v "LSB" | sed '/^$/d' 2>/dev/null > $tmpdir/.result.$ip.$pid
      c_param="cmon"
    fi
  }
  connect_ssh()
  {
    timeout 5s ssh -i $key -oBatchMode=yes -oStrictHostKeyChecking=no cavisson@$ip -p 1122 /bin/bash < $mon_script > $tmpdir/.result.$ip.$pid 2>/dev/null
    if [[ $? -ne 0 ]] ; then
      timeout 5s sshpass -p 'cavisson' ssh -oStrictHostKeyChecking=no cavisson@$ip /bin/bash < $mon_script > $tmpdir/.result.$ip.$pid 2>/dev/null
      if [[ $? -ne 0 ]]; then
        rechability
        connect_cmon
      else
        c_param="ssh22"
      fi
    else
      c_param="ssh1122"
    fi
  }

  validate_error_in_result()
  {
    if grep -i "No space" $tmpdir/.result.$ip.$pid >/dev/null; then
      echo "$(date)|$ip|ERROR: No space left on the device" >> $check_health_err
      cleanup
      exit 3
    elif ERROR=$(grep -i "error" $tmpdir/.result.$ip.$pid) >/dev/null; then
      if [[ $connect_count -eq 0 ]]; then
        connect_cmon
        let "connect_count=$connect_count+1"
        validate_error_in_result
      else
        echo "$(date)|$ip|ERROR: $ERROR" >> $check_health_err
        cleanup
        exit 4
      fi
    elif [ $(grep -w work $tmpdir/.result.$ip.$pid | wc -l) != 2 ]; then
      if [[ $connect_count -eq 0 ]]; then
        connect_cmon
        let "connect_count=$connect_count+1"
        validate_error_in_result
      else
        echo "$(date)|$ip|ERROR: Incorrect fetched result file" >> $check_health_err
        cleanup
        exit 5
      fi
    fi
  }
  connect_count=0
  connect_ssh
  validate_error_in_result

  awk -v ip=$ip -v pid=$pid -v tmpdir=$tmpdir '{print $0 > tmpdir"/""splitted""."ip"."pid NR}' RS='----' $tmpdir/.result.$ip.$pid
  cat $tmpdir/splitted.$ip.${pid}2 | sed '/^$/d' > $tmpdir/.fetched_controller.${ip}.$pid
  cat $tmpdir/splitted.$ip.${pid}3 | sed '/^$/d' > $tmpdir/.build.${ip}.$pid
}



blade_compare()
{
  psql -X -A -U postgres -d demo -t -c "select blade_name, build_version from allocation where server_ip='${ip}'" > $tmpdir/.db_build.$ip.$pid 2>/dev/null || exit 1
  cat $tmpdir/.db_build.$ip.$pid | cut -d "|" -f1 > $tmpdir/.db_controller.${ip}.$pid
  grep -w -v -f $tmpdir/.fetched_controller.${ip}.$pid $tmpdir/.db_controller.${ip}.$pid > $tmpdir/.remove.${ip}.$pid
  grep -w -v -f $tmpdir/.db_controller.${ip}.$pid $tmpdir/.fetched_controller.${ip}.$pid > $tmpdir/.add.${ip}.$pid
  ADD_COUNT=`cat $tmpdir/.add.${ip}.$pid | wc -l`
  REMOVE_COUNT=`cat $tmpdir/.remove.${ip}.$pid | wc -l`
  if [ $ADD_COUNT -ne 0 ]
  then
    SERVER_NAME=`psql -X -A -U postgres -d demo -t -c "select distinct server_name from allocation where server_ip='$ip'"` || exit 1
    SERVER_TYPE=`psql -X -A -U postgres -d demo -t -c "select distinct server_type from allocation where server_ip='$ip'"` || exit 1
    while read BLADE
    do
      BUILD_VERSION=`grep -w $BLADE $tmpdir/.build.${ip}.$pid | cut -d "|" -f 2`
      if [ "xx$SERVER_TYPE" == "xxCC" ]
      then
        ALLOCATION="Free"
      else
        ALLOCATION="Reserved"
      fi
      psql -X -A -U postgres -d demo -t -c "insert into allocation(server_name, server_ip, blade_name, ubuntu_version, machine_type, status, team, channel, owner, allocation, build_version, build_upgradation_date, server_type, refresh_at) values('$SERVER_NAME','$ip','$BLADE','16.04','Netstorm','t','NA','NA','NA','$ALLOCATION','$BUILD_VERSION','$(date)','$SERVER_TYPE','$(date)')" > /dev/null
      if [ $? -eq 0 ]
      then
        echo "ADD|$(date)|$SERVER_NAME|$ip|$BLADE|$UVERSION|$BUILD_VERSION|$SERVER_TYPE|Netstorm|ON|NA|NA|NA|Free|$BANDWIDTH" >> $blade_state_log
      else
        echo "ADD|$(date)|$SERVER_NAME  $SERVER_IP Blade:$BLADE Distro:$UVERSION Build:$BUILD_VERSION Type:$SERVER_TYPE|Cannot upload to database" >> $check_health_err
      fi
    done < $tmpdir/.add.${ip}.$pid
  fi

  if [ $REMOVE_COUNT -ne 0 ]
  then
    while read BLADE
    do
      TEAM=`psql -X -A -U postgres -d demo -t -c "select team from allocation where server_ip='$ip' and blade_name='$BLADE'"`
      if [ "xx$TEAM" == "xxNA" ]
      then
        if [ "xx$BLADE" != "xxwork" ]
        then
          DETAILS=`psql -X -A -U postgres -d demo -t -c "select server_name,server_ip,blade_name,ubuntu_version,build_version,server_type,machine_type,status,team,channel,owner,allocation,bandwidth from allocation where server_ip='$ip' and blade_name='$BLADE'"`
          echo "REMOVE|$(date)|$DETAILS" >> $blade_state_log
          psql -X -A -U postgres -d demo -t -c "delete from allocation where server_ip='$ip' and blade_name='$BLADE'" > /dev/null
        else
          echo "REMOVE|$(date)|Cannot delete as controller work cannot be removed for $ip" >>$blade_state_log
        fi
      else
	psql  -X  -A -U postgres -d demo -t -c "update allocation set allocation='Missing' where server_ip='$ip' and blade_name='$BLADE'" > /dev/null
        echo "REMOVE|$(date)|Cannot delete as controller $BLADE is assigned to team $TEAM for $ip" >> $blade_state_log
      fi
    done < $tmpdir/.remove.${ip}.$pid
  fi
}



build_compare()
{
#  psql -X -A -U postgres -d demo -t -c "select blade_name, build_version from allocation where server_ip='$ip'" | sed -r 's/\s+//g' > .db_build.$ip.$pid
  while read CHECK
  do
    CURR_BLADE=`echo $CHECK| cut -d "|" -f 1`
    CURR_BUILD=`echo $CHECK|cut -d "|" -f 2`
    PREVIOUS_BUILD=`grep -w $CURR_BLADE $tmpdir/.db_build.$ip.$pid | cut -d "|" -f 2`
    if [ "xx$PREVIOUS_BUILD" != "xx$CURR_BUILD" ]
    then
            psql -X -A -U postgres -d demo -t -c "update allocation set build_version='$CURR_BUILD',build_upgradation_date='$(date)' where server_ip='$ip' and blade_name='$CURR_BLADE'" >/dev/null || echo "$(date)|$ip|Cannot update Blade $CURR_BLADE with $CURR_BUILD" >> $check_health_err
    fi
  done < $tmpdir/.build.${ip}.$pid
}



recursive_update()
{
  HEALTH=$(cat $tmpdir/splitted.$ip.${pid}4 | sed '/^$/d')
  KERNAL=`echo $HEALTH| cut -d "|" -f 1`
  BANDWIDTH=`echo $HEALTH| cut -d "|" -f 2`
  UVERSION=`echo $HEALTH| cut -d "|" -f 3`
  DISK_ROOT=`echo $HEALTH| cut -d "|" -f 4`
  DISK_HOME=`echo $HEALTH| cut -d "|" -f 5`
  if [[ "xx$DISK_HOME" == "xx" ]]; then
          DISK_HOME="NaN"
  fi
  CPU=`echo $HEALTH| cut -d "|" -f 6`
  TOTDISK=`echo $HEALTH| cut -d "|" -f 7`
  RAM=`echo $HEALTH| cut -d "|" -f 8`
  psql -X  -A -U postgres -d demo -t << EOF >> /dev/null
    update allocation set (refresh_at, bandwidth, ubuntu_version) = ('$(date)','$BANDWIDTH', '$UVERSION') where server_ip='$ip';
    update servers set (cpu, ram, total_disk_size, avail_disk_root, avail_disk_home, kernal, refresh_at, c_param) = ('$CPU', '$RAM', '$TOTDISK', '$DISK_ROOT', '$DISK_HOME', '$KERNAL', '$(date)', '$c_param') where server_ip='$ip';
EOF
if [[ $? -ne 0 ]]; then
  echo "$(date)|$ip|Cannot upload to database. Args Bandwith=$BANDWIDTH, OS=$UVERSION, CPU=$CPU, RAM=$RAM, Home=$DISK_HOME, Root=$DISK_ROOT, TotalDisk=$TOTDISK, Kernal=$KERNAL, Connect=$c_param" >> $check_health_err
  cleanup
fi
}


connect_and_execute
if ! pg_isready >/dev/null; then
  echo "$(date)|$ip|Postgresql not running. Skipping" >> $check_health_err
  exit 6
fi
blade_compare
build_compare
recursive_update

cleanup
exit 0
