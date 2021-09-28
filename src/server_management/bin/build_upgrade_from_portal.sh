#!/bin/bash
sorted=/tmp/sorted_ip.$$
upgrade_log=$NS_WDIR/server_management/logs/build_upgrade.log
upgrade_shell=$NS_WDIR/server_management/bin/remote_upgrade.sh
key_file=$NS_WDIR/server_management/conf/pwless
portal_ip=$(ip -o -4 addr list $(netstat -nr|grep -wm1 UG|awk '{print $8}') | awk '{print $4}' | cut -d/ -f1|head -1)
usage()
{
  echo "$0  -n <BUILD Version> -i <Target file of Generators>"
  echo "Version : Should be like 4.1.15.54
Target file of Generators : It is a file containing generator ip and controller name. Seperated by pipe '|' "
}

cleanup()
{
  rm -f $sorted /tmp/$TARGET_FILE
}

change_key_file_permission(){
  if ! $(chmod 600 $key_file); then
    echo "Unable to change key file permissions" >2
    exit 1
  fi
}

trap 'FLAG=0 ; cleanup ; exit 1' 0

while getopts r:n:i:s:u: args
do
        case $args in
#          r) RELEASE=$OPTARG;;
          n) BUILD=$OPTARG;;
          i) TARGET_FILE=$OPTARG;;
          u) SESSUSER=$OPTARG;;
#          s) req_sess_id=$OPTARG;;
#       ?) usage;;
        esac
done
if [[ -z $BUILD ]] || [[ -z $TARGET_FILE ]] ; then
  echo "Arguments are not correct. Exiting" >&2
  usage
  exit 1
fi

#if [[ ! -e /home/cavisson/etc/build_hub.list ]] || [[ ! -s /home/cavisson/etc/build_hub.list ]]; then
#  echo "Build hub list file not found at /home/cavisson/etc/build_hub.list or its empty Downloading from 184.105.48.4"
#  nsu_server_admin -s 184.105.48.4 -G /home/cavisson/etc/build_hub.list
#  mv build_hub.list /home/cavisson/etc/
#  if [[ $? -ne 0 ]]; then
#    echo "Build hub file not found at 184.105.48.4. Exiting." >&2
#    exit 1
#  fi
#  if [[ ! -e remote_upgrade.sh ]] || [[ ! -s remote_upgrade.sh ]]; then
#    echo "remote_upgrade file not found. Exiting" >&2
#    exit 1
#  fi
#fi

RELEASE=$(echo $BUILD|cut -d "." -f 1,2,3)
VERSION=$(echo $BUILD|cut -d "." -f 4)
#thirdparty_build_file=thirdparty.${RELEASE}.${VERSION}_Ubuntu1604_64.bin
#netstorm_build_file=netstorm_all.${RELEASE}.${VERSION}?Ubuntu1604_64.bin

change_key_file_permission
#eval `ssh-agent` >/dev/null
#ssh-add key_file

check_ssh_access(){
  name=$(echo $i|cut -d "+" -f 1)
  ip=$(psql demo postgres -A -t -c "select server_ip from servers where server_name='$name'")
  blade=$(echo $i|cut -d "+" -f 2)
#  sess_id=$(echo $i|cut -d "|" -f 3)
#  if [[ $sess_id == $req_sess_id ]]; then
    if ssh -i $key_file -oStrictHostKeyChecking=no -oBatchMode=yes -p 1122 cavisson@$ip "echo >/dev/null 2>&1" 2>/dev/null; then
      echo "$ip|$blade" >> $sorted
    else
      echo "$SESSUSER|$ip|$blade|$(date)|Server is not accessible for upgrade"|tee -a $upgrade_log
    fi
#  fi
}


for i in `cat /tmp/$TARGET_FILE | grep -v "^#"`
do
  check_ssh_access &
done
wait

if [[ ! -e $sorted ]]; then
  cleanup
  echo "sorted file not created. Exiting" >&2
  exit 1
fi

#cp $NS_WDIR/bin/remote_upgrade.sh /tmp/remote_upgrade.$(whoami).sh && chmod +x /tmp/remote_upgrade.$(whoami).sh
cd /tmp
for i in `cat $sorted`
do
  ip=$(echo $i|cut -d "|" -f 1)
  blade=$(echo $i|cut -d "|" -f 2)
#  sess_id=$(echo $i|cut -d "|" -f 3)
#  if [[ $sess_id == $req_sess_id ]]; then
    ssh -i $key_file -oStrictHostKeyChecking=no -oBatchMode=yes -p 1122 cavisson@$ip "bash -s" < $NS_WDIR/server_management/bin/remote_upgrade.sh $blade $RELEASE $VERSION $ip $portal_ip 2>>/tmp/build_upgrade_tool_err.log &
    echo "$SESSUSER|$ip|$blade|$(date)|Request submitted successfully for $RELEASE $VERSION" >> $upgrade_log
#  fi
done
 wait
sleep 2
cleanup
