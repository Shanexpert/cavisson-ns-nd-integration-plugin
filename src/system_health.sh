#!/bin/bash
### BEGIN INIT INFO
# Provides:             system_health.sh
# Required-Start:       
# Required-Stop:        
# Should-Start:         
# Should-Stop:          
# Default-Start:        2 3 4 5
# Default-Stop:         0 1 6
# Short-Description:    system_health.sh
### END INIT INFO

DESC="System_health_check_monitor"
COMMAND="$NS_WDIR/system_health_monitor/bin/nsu_system_health_check"
LOGFILE="$NS_WDIR/system_health_monitor/logs/nsu_system_health_check.log"
PIDFILE="$NS_WDIR/system_health_monitor/bin/nsu_system_health_check.pid"
touch $PIDFILE
PARENTPIDFILE="$NS_WDIR/system_health_monitor/bin/nsu_check_health_monitor.pid"
touch $PARENTPIDFILE
NSU_CHECK_HEALTH_MONITOR="$NS_WDIR/system_health_monitor/bin/nsu_check_health_monitor"
CONF_FILE="$NS_WDIR/system_health_monitor/conf/system_health_monitor.conf"
CHK_HEALTH_MONITOR_LOG_MAX_SIZE=1          #1 MB (1024 * 1024 Bytes)
CHECK_HEALTH_MONITOR_INTERVAL=5            #5 minutes

get_running_pid_by_ps()
{
  PID=`cat $PIDFILE`
  if [ -n "$PID" ];then
    Status=`ps -p $PID`  
    if [ $? != 0 ];then
      PID=""
    fi
  fi
}

get_nsu_check_health_monitor_pid()
{
  nsu_check_health_monitor_running_pid=`cat $PARENTPIDFILE`
  #excluding grep and nsu_check_health_monitor.log processes
  if [ -n "$nsu_check_health_monitor_running_pid" ];then
    out=`ps -p $nsu_check_health_monitor_running_pid`
    if [ $? = 0 ];then
      #nsu_check_health_monitor_running_pid=`echo $out | cut -d ' ' -f 2`
      #nsu_check_health_monitor_running_pid=`cat $PARENTPIDFILE`
      kill_flag=1
      echo "Obtained nsu_check_health_monitor running pid is $nsu_check_health_monitor_running_pid"
    else
      nsu_check_health_monitor_running_pid=""
      echo "nsu_check_health_monitor is not running" 
    fi
  else
    echo "nsu_check_health_monitor is not running"
  fi
}


stop_nsu_check_health_monitor()
{
  kill_flag=0
  get_nsu_check_health_monitor_pid
  if [ $kill_flag -eq 1 ]; then
    echo "Stopping nsu_check_health_monitor with pid ($nsu_check_health_monitor_running_pid)"
    kill -9 $nsu_check_health_monitor_running_pid
    if [ "$?" = "0" ]; then
      echo "nsu_check_health_monitor is stopped"
    else
      echo "Error in stopping nsu_check_health_monitor"
    fi
  fi
}

start_nsu_check_health_monitor()
{
  echo "Starting nsu_check_health_monitor"
  nohup $NSU_CHECK_HEALTH_MONITOR -p $PID -i $CHECK_HEALTH_MONITOR_INTERVAL -s $CHK_HEALTH_MONITOR_LOG_MAX_SIZE_IN_BYTES >>$LOGFILE 2>&1 &
  echo $! >$PARENTPIDFILE
  get_nsu_check_health_monitor_pid
  sleep 1
  echo "Started nsu_check_health_monitor with pid ($nsu_check_health_monitor_running_pid)"
}

status_system_health()
{
  operation=$1
  get_running_pid_by_ps
  if [ "XX$PID" != "XX" ];then
    echo "$DESC is $operation with Pid($PID)."
  else
    if [ "XX$operation" == "XXstopped" ];then
      echo "$DESC is $operation."
    else
      echo "$DESC is not running"
    fi
  fi
}

stop_system_health()
{
  stop_nsu_check_health_monitor
  get_running_pid_by_ps
  if [ "XX$PID" != "XX" ]; then
   echo "Stopping $DESC with Pid($PID)."
   kill $PID
 
   count=0
   while true
   do
     sleep 1
     ps -p $PID >/dev/null 2>&1
     if [ "X$?" != "X0" -o "X$count" = "X60" ];then
       break
     fi
     count=`expr $count + 1`
   done
  
   if [ "$?" != 0 ]; then
     echo "Error in stopping $DESC Killing by sending signal 9"
     kill -9 $PID
   fi

   status_system_health "stopped"

  else
    echo "$DESC is not running."
  fi
}

get_nsu_check_health_monitor_args()
{
  #if system_health_monitor.conf file doesn't have these variables, default values will be used. 
  #If a variable is found more than once, then first occurance will be used.
  INTERVAL=`grep -m 1 "^CHECK_HEALTH_MONITOR_INTERVAL" $CONF_FILE | awk '{print $2}'`
  if [ -n "$INTERVAL" ]; then
    CHECK_HEALTH_MONITOR_INTERVAL=$INTERVAL
  fi
  SIZE=`grep -m 1 "^CHK_HEALTH_MONITOR_LOG_MAX_SIZE" $CONF_FILE | awk '{print $2}'`
  if [ -n "$SIZE" ]; then
    CHK_HEALTH_MONITOR_LOG_MAX_SIZE=$SIZE
  fi
  CHECK_HEALTH_MONITOR_INTERVAL=`expr $CHECK_HEALTH_MONITOR_INTERVAL \* 60`           #converting minutes to seconds.
  CHK_HEALTH_MONITOR_LOG_MAX_SIZE_IN_BYTES=`expr $CHK_HEALTH_MONITOR_LOG_MAX_SIZE \* 1024 \* 1024`    #Converting from MB to Bytes
  echo "CHECK_HEALTH_MONITOR_INTERVAL = $CHECK_HEALTH_MONITOR_INTERVAL;   CHK_HEALTH_MONITOR_LOG_MAX_SIZE = $CHK_HEALTH_MONITOR_LOG_MAX_SIZE_IN_BYTES"
}

start_system_health()
{
  get_nsu_check_health_monitor_args
  echo "Starting $DESC:"
  get_running_pid_by_ps
  if [ "XX$PID" != "XX" ]; then
    echo "$DESC is already running with Pid($PID)"
    exit -1
  fi
 
  nohup $COMMAND >$LOGFILE 2>&1 &
  echo $! >$PIDFILE
  sleep 1
  tail -1 $LOGFILE | grep "Error"
  if [ $? -eq 0 ]; then
    exit 1
  fi
  get_running_pid_by_ps
  if [ "XX$PID" == "XX" ];then
    echo "Error in Starting $DESC:"
    exit 255
  fi
  
  status_system_health "started"

  stop_nsu_check_health_monitor
  if [ $CHECK_HEALTH_MONITOR_INTERVAL -ne 0 ];then
    start_nsu_check_health_monitor
  fi
}

case "$1" in
   start)
    start_system_health
    ;;
   stop)
    stop_system_health
    ;;
   restart)
    echo "Restarting $DESC: "
    stop_system_health
    start_system_health
    ;;
   status)
    status_system_health "running"
    ;;
   *)
    N=$0
    echo "Usage: $N {start|stop|restart|status}" >&2
    exit 1;
    ;;
esac
