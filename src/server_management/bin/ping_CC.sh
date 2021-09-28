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
errorlog=$NS_WDIR/server_management/logs/ping_CC_err.log
if [ ! -d $tmpdir ];
then
 echo "$tmpdir does not exist. Exiting" >$2
 exit 1
fi

cleanup()
{
  rm -f $tmpdir/.ping_cc.pid
  pkill -P $$
}

_hms()
{
    h=`expr $1 / 3600`
    m=`expr $1  % 3600 / 60`
    s=`expr $1 % 60`
    printf "%02d:%02d:%02d\n" $h $m $s
}

if [ "x$1" == "xstop" ]; then
  if [[ ! -e $tmpdir/.ping_cc.pid ]]; then
    echo "ERROR: ping_cc not running"
    exit 1
  else
    kill $(cat $tmpdir/.ping_cc.pid) && echo "stopping pid $(cat $tmpdir/.ping_cc.pid)" && cleanup
    exit 0
  fi
fi
if [ -e $tmpdir/.ping_cc.pid ]; then
  echo "ping_cc.sh is already running. Check pid $(cat $tmpdir/.ping_cc.pid)"
  exit 1
fi
######## Main function starts #############

echo $$ > $tmpdir/.ping_cc.pid
trap "cleanup ; exit 2" 1 2 3 6 9
check_details()
{
while read line
do
        IP=`echo $line | cut -d "|" -f 2`
        NAME=`echo $line| cut -d "|" -f 1`
        sh script.sh $IP $NAME &
done < $tmpdir/IP_CC.temp
sleep 180

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
echo -n -e "$NAME | $IP | $VENDOR | $STATUS\n" >> $tmpdir/ping_CC.log
#ping -c 1 -W 5 $IP > /dev/null
if [ "xx$STATUS" == "xxON" ]
then
    CURR_STATUS="t"
    if [ "xx$PREV_STATUS" != "xx$CURR_STATUS" ]
    then
  		psql -X -A -U postgres -d demo -c "update allocation set status='t' where server_ip='$IP'" >> $tmpdir/ping_CC.log 2>/dev/null
  		if [ $? -eq 0 ]
  		then
  			 echo -n -e "$NAME | $IP | $VENDOR | Status:ON\n" >> $tmpdir/ping_CC.log
   	  	 PETIME=`psql -X -A -U postgres -d demo -t -c "select downtime_epoch from billing_cc where status='f' and server_ip='$IP'"`
     		 NETIME=$(date +%s)
    		 EDURATION=`expr $NETIME - $PETIME`
         echo $EDURATION $NETIME $PETIME
      	 DURATION=`_hms $EDURATION`
         psql -X -A -U postgres -d demo -t -c "update billing_cc set status='t', uptime='$(date '+%D %T')', duration='$DURATION' where server_name='$NAME' and status='f'" >> $tmpdir/ping_CC.log
  		else
  			echo "$(date)|$NAME|Unable to upload to database." >> $tmpdir/ping_CC_err.log
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
  		echo -n -e  "$NAME | $IP | $VENDOR | Status:OFF\n" >> $tmpdir/ping_CC.log
  		psql -X -A -U postgres -d demo -c "update allocation set status='f' where server_ip='$IP'" >> $tmpdir/ping_CC.log 2>/dev/null
  		if [ $? -eq 0 ]
      then
  			psql -X -A -U postgres -d demo -t -c "insert into billing_cc(server_name, server_ip, vendor, status, downtime, downtime_epoch) values('$NAME', '$IP', '$VENDOR','f','$(date '+%D %T')','$(date +%s)')" >> $tmpdir/ping_CC.log
  		else
  			echo "$(date)|$NAME|Unable to upload to database." >> $tmpdir/ping_CC_err.log
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
	echo "$NAME | $IP | $VENDOR | Status:Ignore" >> $tmpdir/ping_CC.log
fi
done < $tmpdir/IP_CC.temp
}
while true
do
  echo -n -e "exection time : $(date '+%D %T')\n\n" >> $tmpdir/ping_CC.log
  psql -X -A -U postgres -d demo -t -c "select server_name, server_ip, vendor from servers where server_type!='VM'" > $tmpdir/IP_CC.temp 2>/dev/null
  if [ $? -ne 0 ]
  then
  	echo "$(date)|Postgresql not running. Waiting for 10 secs" >> $tmpdir/ping_CC_err.log
  	sleep 10
  	continue
  fi
  check_details
  echo -n -e  "***********************************************************************************************************************************\n\n\n" >> $tmpdir/ping_CC.log
rm -f $tmpdir/IP_CC.temp
done
