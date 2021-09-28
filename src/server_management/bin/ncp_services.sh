#!/bin/bash
tmpdir=$NS_WDIR/server_management/tmp
key=$NS_WDIR/server_management/conf/key
pid_check_health=$tmpdir/.check_health.pid
pid_ping_cc=$tmpdir/.ping_cc.pid
pid_ping_vm=$tmpdir/.ping_vm.pid
pid_mailer=$tmpdir/.mailer.pid
declare -a valid_actions
declare -a valid_services
valid_actions=(start stop status restart)
valid_services=(ping checkhealth mail scan all)
if [[ $# -ne 2 ]]; then
  echo "ERROR: Insufficient arguments."
  echo -n -e "\nUsage: ncp_services.sh <action> <service name>\nAction: start, stop, status, restart\nServices: ping, checkhealth, mail, scan, all\n\n"
  exit 1
fi
if [[ ! " ${valid_actions[@]} " =~ " $1 " ]]; then
  echo "ERROR: Invalid Argument. Please use start, stop, status or restart"
  exit 1
fi
if [[ ! " ${valid_services[@]} " =~ " $2 " ]]; then
  echo "ERROR: Invalid Service name. Valid services are ping, checkhealth and mail"
  exit 1
fi
if [ ! -d $tmpdir ];
then
 echo "$tmpdir does not exist. Exiting" >$2
 exit 1
fi
if [ ! -f $key ]; then
  echo "key file does not exists. Exiting." >&2
  exit 1
fi

action=$1

start(){
  local service_name=$1
  p_start()
  {
    ping_CC.sh &
    ping_VM.sh &
    echo "Starting status monitoring" && sleep 0.5
    status ping
  }
  c_start()
  {
    if [[ -e $tmpdir/.ssh_agent_check_health.pid ]]; then
      echo "ssh agent running with pid $(cat $tmpdir/.ssh_agent_check_health.pid). checkhealth might be already running. Exiting"
    else
      eval $(ssh-agent -s)
      ssh-add $key
      echo $SSH_AGENT_PID > $tmpdir/.ssh_agent_check_health.pid
    fi
    trigger_check_health.sh &
    echo "Starting checkhealth"
    sleep 0.5 && status checkhealth
  }
  m_start()
  {
    mailer.py &
    echo "starting mail service" && sleep 0.5
    status mail
  }
  s_start()
  {
    security_scan.sh &
    echo "starting service security scan" && sleep 0.5
    status scan
  }
  if [[ "x$service_name" == "xping" ]]; then
    cd $tmpdir & p_start
  elif [[ "x$service_name" == "xcheckhealth" ]]; then
    cd $tmpdir & c_start
  elif [[ "x$service_name" == "xmail" ]]; then
    cd $tmpdir & m_start
  elif [[ "x$service_name" == "xscan" ]]; then
    cd $tmpdir & s_start
  elif [[ "x$service_name" == "xall" ]]; then
    cd $tmpdir
    p_start ; c_start ; m_start ; s_start
  fi
}

stop(){
  local service_name=$1
  p_stop()
  {
    ping_CC.sh stop
    ping_VM.sh stop
  }
  c_stop()
  {
    trigger_check_health.sh stop
    [[ -e $tmpdir/.ssh_agent_check_health.pid ]] && kill -9 $(cat $tmpdir/.ssh_agent_check_health.pid) 2>/dev/null
    rm -f $tmpdir/.ssh_agent_check_health.pid
  }
  m_stop()
  {
    if [[ -e $pid_mailer ]] ; then
      kill $(cat $pid_mailer) && rm -f $pid_mailer && echo "stopping mailer service"
    else
      echo "ERROR: service mailer not running"
    fi
  }
  s_stop()
  {
    security_scan.sh stop
  }
  if [[ "x$service_name" == "xping" ]]; then
    p_stop
  elif [[ "x$service_name" == "xcheckhealth" ]]; then
    c_stop
  elif [[ "x$service_name" == "xmail" ]]; then
    m_stop
  elif [[ "x$service_name" == "xscan" ]]; then
    s_stop
  elif [[ "x$service_name" == "xall" ]]; then
    p_stop ; c_stop ; m_stop ; s_stop
  fi
}

status() {
  local service_name=$1
  p_status()
  {
    if [[ ! -e $pid_ping_cc ]]; then
      echo "service ping_cc not running"
    else
      ps p $(cat $pid_ping_cc)
    fi
    if [[ ! -e $pid_ping_vm ]]; then
      echo "service ping_vm not running"
    else
      ps p $(cat $pid_ping_vm)
    fi
  }
  c_status()
  {
    if [[ ! -e $pid_check_health ]]; then
      echo "service check health not running"
    else
      ps p $(cat $pid_check_health)
    fi
  }
  m_status()
  {
    if [[ ! -e $pid_mailer ]]; then
      echo "service mailer not running"
    else
      ps p $(cat $pid_mailer)
    fi
  }
  s_status()
  {
    security_scan.sh status
  }
  if [[ "x$service_name" == "xping" ]]; then
    p_status
  elif [[ "x$service_name" == "xcheckhealth" ]]; then
    c_status
  elif [[ "x$service_name" == "xmail" ]]; then
    m_status
  elif [[ "x$service_name" == "xscan" ]]; then
    s_status
  elif [[ "x$service_name" == "xall" ]]; then
    p_status ; c_status ; m_status ; s_status
  fi
}

if [[ $1 == 'start' ]]; then
  start $2
elif [[ $1 == 'stop' ]]; then
  stop $2
elif [[ $1 == 'status' ]]; then
  status $2
elif [[ $1 == 'restart' ]]; then
  stop $2
  start $2
fi
