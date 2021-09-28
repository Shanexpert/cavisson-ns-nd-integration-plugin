#!/bin/sh
#Name: Status
#Purpose: To update servers in the Database with current status and also maintain the logs of servers OFF and ON Timings.
#Author: Gyanendra

########## Convert epoch time into duration #############
#_hms()
##{
# local S=${1}
# ((h=S/3600))
# ((m=S%3600/60))
# ((s=S%60))
# printf "%d:%d:%d\n" $h $m $s
#}
tmpdir=$NS_WDIR/server_management/tmp
errorlog=$NS_WDIR/server_management/logs/ping_VM_err.log
if [ ! -d $tmpdir ];
then
 echo "$tmpdir does not exist. Exiting" >$2
 exit 1
fi
_hms()
{
    h=`expr $1 / 3600`
    m=`expr $1  % 3600 / 60`
    s=`expr $1 % 60`
    printf "%02d:%02d:%02d\n" $h $m $s
}

cleanup()
{
  rm -f $tmpdir/.ping_vm.pid
  pkill -P $$
}

if [ "x$1" == "xstop" ]; then
  if [[ ! -e $tmpdir/.ping_vm.pid ]]; then
    echo "ERROR: ping_vm not running"
    exit 1
  else
    kill $(cat $tmpdir/.ping_vm.pid) && echo "stopping pid $(cat $tmpdir/.ping_vm.pid)" && cleanup
    exit 0
  fi
fi
if [ -e $tmpdir/.ping_vm.pid ]; then
  echo "ping_vm.sh is already running. Check pid $(cat $tmpdir/.ping_vm.pid)"
  exit 1
fi
######## Main function starts #############


echo $$ > $tmpdir/.ping_vm.pid
trap "cleanup; exit 1" 1 2 3 6 9
#echo -n -e "exection time : $(date '+%D %T')\n\n" >> ping.log
check_details()
{
while read line
do
        IP=`echo $line | cut -d "|" -f 2`
        NAME=`echo $line| cut -d "|" -f 1`
        sh script.sh $IP $NAME &
done < $tmpdir/IP.temp
sleep 200

while read line
do

NAME=`echo "$line" | cut -d "|" -f 1`
IP=`echo "$line" | cut -d "|" -f 2`
VENDOR=`echo "$line" | cut -d "|" -f 3`
#PREV_STATUS=`psql -X -A -U postgres -d demo -t -c "select distinct status from allocation where server_ip='$IP'"`
PREV_STATUS=`cat $tmpdir/.$NAME.log.bk` 2>/dev/null
if [ $? -ne 0 ]
then
  PREV_STATUS=`psql -X -A -U postgres -d demo -t -c "select distinct status from allocation where server_ip='$IP'"` 2>/dev/null
  if [ $? -ne 0 ]
  then
    continue
  fi
fi
if [ "xx$PREV_STATUS" == "xxON" ]
then
  PREV_STATUS="t"
elif [ "xx$PREV_STATUS" == "xxOFF" ]
then
  PREV_STATUS="f"
fi

STATUS=`cat $tmpdir/.${NAME}.log`
mv $tmpdir/.${NAME}.log $tmpdir/.${NAME}.log.bk
echo -n -e "$NAME | $IP | $VENDOR | $STATUS\n" >> $tmpdir/ping.log
if [ "xx$STATUS" == "xxON" ]
then
        CURR_STATUS="t"
        if [ "xx$PREV_STATUS" != "xx$CURR_STATUS" ]
        then
# 	      	echo "ON|$(date '+%T %D')|$(date +%s)" >> status/$NAME
      		psql -X -A -U postgres -d demo -c "update allocation set status='t' where server_ip='$IP'" >> $tmpdir/ping.log
          if [ $? -eq 0 ]
      		then
        		echo -n -e "$NAME | $IP | $VENDOR | Status:ON\n" >> $tmpdir/ping.log
        		psql -X -A -U postgres -d demo -t -c "insert into billing(server_name, server_ip, vendor, status, uptime, uptime_epoch) values('$NAME', '$IP', '$VENDOR','t','$(date '+%D %T')','$(date +%s)')" >> $tmpdir/ping.log
          else
            echo "$(date)|$NAME|Unable to upload to database." >> $tmpdir/ping_VM_err.log
            if [ "xx$PREV_STATUS" == "xxt" ]
            then
              echo "ON" > $tmpdir/.${NAME}.log.bk
            elif [ "xx$PREV_STATUS" == "xxf" ]
            then
              echo "OFF" > $tmpdir/.${NAME}.log.bk
            fi
          fi
        fi
elif [ "xx$STATUS" == "xxOFF" ]
then
        CURR_STATUS="f"
        if [ "xx$PREV_STATUS" != "xx$CURR_STATUS" ]
        then
          echo -n -e  "$NAME | $IP | $VENDOR | Status:OFF\n" >> $tmpdir/ping.log
          psql -X -A -U postgres -d demo -c "update allocation set status='f' where server_ip='$IP'" >> $tmpdir/ping.log
          if [ $? -eq 0 ]
          then
            PETIME=`psql -X -A -U postgres -d demo -t -c "select uptime_epoch from billing where status='t' and server_ip='$IP'"`
            NETIME=$(date +%s)
            EDURATION=`expr $NETIME - $PETIME`
            echo $EDURATION $NETIME $PETIME
            DURATION=`_hms $EDURATION`
            psql -X -A -U postgres -d demo -t -c "update billing set status='f', downtime='$(date '+%D %T')', duration='$DURATION' where server_name='$NAME' and status='t'" >> $tmpdir/ping.log
          else
            echo "$(date)|$NAME|Unable to upload to database." >> $tmpdir/ping_VM_err.log
            if [ "xx$PREV_STATUS" == "xxt" ]
            then
              echo "ON" > $tmpdir/.${NAME}.log.bk
            elif [ "xx$PREV_STATUS" == "xxf" ]
            then
              echo "OFF" > $tmpdir/.${NAME}.log.bk
            fi
          fi
        fi
else
	echo "$NAME | $IP | $VENDOR | Status:Ignore" >> $tmpdir/ping.log
fi
done < $tmpdir/IP.temp
}
while true
do
echo -n -e "exection time : $(date '+%D %T')\n\n" >> $tmpdir/ping.log
psql -X -A -U postgres -d demo -t -c "select server_name, server_ip, vendor from servers where server_type='VM'" > $tmpdir/IP.temp
if [ $? -ne 0 ]
then
        echo "$(date)|Postgresql not running. Waiting for 10 secs" >> $tmpdir/ping_VM_err.log
        sleep 10
        continue
fi
check_details 1
echo -n -e  "***********************************************************************************************************************************\n\n\n" >> $tmpdir/ping.log
rm -f $tmpdir/IP.temp
done
