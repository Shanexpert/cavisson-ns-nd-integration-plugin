BLADE=$1
export CALLING_FROM_UPGRADE_SHELL=YES
if ! nsi_get_controller_name | grep -w $1 >/dev/null ; then
  read -r TEST_RUNNING BUILD CMON BANDWIDTH TOTDISK DISK_ROOT DISK_HOME CPU RAM DATE LOCAL_NAMESERVER ISP_NAMESERVER BLADE_EXIST RAM_AVAIL KERNAL<<< $(echo - - - - - - - - - - - - NO - -)
  echo "$BLADE|$BLADE_EXIST|$TEST_RUNNING|$BUILD|$CMON|$BANDWIDTH|$TOTDISK|$DISK_ROOT|$DISK_HOME|$CPU|$RAM|$RAM_AVAIL|$DATE|$LOCAL_NAMESERVER|$ISP_NAMESERVER|$KERNEL"
  exit 1
fi
BLADE_EXIST=YES
export NS_WDIR=/home/cavisson/$BLADE
#WDIR=/home/cavisson/$1
#if [ -f /home/cavisson/bin/nsi_controller_selection_profile ];then
#  . /home/cavisson/bin/nsi_controller_selection_profile $1
#else
#  . $WDIR/bin/nsi_controller_selection_profile $1
#fi
KERNEL=`uname -r` 2>/dev/null
INTERFACE=`netstat -nr|grep -wm1 UG|awk '{print $8}'` 2>/dev/null
BANDWIDTH=`cat /sys/class/net/$INTERFACE/speed` 2>/dev/null
DISK_ROOT=`df -hPBG /| sed '2q;d' | awk '{print $4}'|sed 's/[!A-Z]*//g'` 2>/dev/null
DISK_HOME=`df -hPBG /home | sed '2q;d' | awk '/\/home/{print $4}'|sed 's/[!A-Z]*//g'` 2>/dev/null
if [[ -z $DISK_HOME ]]; then
  DISK_HOME=NA
fi
CPU=`nproc` 2>/dev/null
TOTDISK=`df -BG --total | grep total | awk '{print $2}' | sed 's/[!A-Z]*//g'` 2>/dev/null
RAM=`free -g| sed -e's/  */ /g' | grep Mem | cut -d " " -f 2` 2>/dev/null
RAM_AVAIL=`free -g| sed -e's/  */ /g' | grep Mem | cut -d " " -f 7` 2>/dev/null
BUILD=`nsu_get_version  -n 2>/dev/null | awk '{print $2,$3,$4}' | sed -r 's/\s+//g'`
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
  BUILD="-"
fi
CMON=`nsu_get_version  -c 2>/dev/null | awk '{print $2,$3,$4}' | sed -r 's/\s+//g'`
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
  CMON="-"
fi
DATE=`date`
LOCAL_NAMESERVER=`cat /etc/resolv.conf 2>/dev/null | grep nameserver | awk '{print $2}' | sed '$!{:a;N;s/\n/:/;ta}'`
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
  LOCAL_NAMESERVER="-"
fi
ISP_NAMESERVER=`cat /var/run/dnsmasq/resolv.conf 2>/dev/null | grep nameserver | awk '{print $2}' | sed '$!{:a;N;s/\n/:/;ta}'`
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
  ISP_NAMESERVER="-"
fi
if ! nsu_show_netstorm >/tmp/a.$$ 2>&1; then
 TEST_RUNNING=NO
 SCENARIO="-"
else
 TR=`cat /tmp/a.$$ | sed '1d' | awk '{print $1}' | sed '$!{:a;N;s/\n/,/;ta}'`
 SCENARIO=`cat /tmp/a.$$ | sed '1d' | awk '{print $4}' | sed '$!{:a;N;s/\n/,/;ta}'`
 TEST_RUNNING="YES($TR)"
fi
rm /tmp/a.$$
echo "$BLADE|$BLADE_EXIST|$TEST_RUNNING|$SCENARIO|$BUILD|$CMON|$BANDWIDTH|$TOTDISK|${DISK_ROOT}|${DISK_HOME}|$CPU|$RAM|$RAM_AVAIL|$DATE|$LOCAL_NAMESERVER|$ISP_NAMESERVER|$KERNEL"
