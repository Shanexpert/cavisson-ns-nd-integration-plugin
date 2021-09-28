echo "----"
/home/cavisson/work/bin/nsi_get_controller_name 2>/dev/null | sed '/^$/d'
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
  echo "ERROR running command nsi_get_controller name"
  exit 0
fi
echo "----"

/home/cavisson/work/bin/nsu_get_version -n -A 2>/dev/null |awk 'NR % 3 == 0'|cut -d " " -f 2,3,4 > .a.$$
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
  echo "ERROR running command nsu_get_version"
  exit 0
fi
/home/cavisson/work/bin/nsu_get_version -n -A 2>/dev/null |awk 'NR % 3 == 1' > .b.$$
paste .b.$$ .a.$$ -d "|" | sed -r 's/\s+//g'
echo "----"
KERNEL=`uname -r` 2>/dev/null
INTERFACE=`netstat -nr|grep -wm1 UG|awk '{print $8}'` 2>/dev/null
BANDWIDTH=`cat /sys/class/net/$INTERFACE/speed` 2>/dev/null
OS=`lsb_release -a | grep Description | awk '{print $3}'` 2>/dev/null
DISK_ROOT=`df -hPBG /| sed '2q;d' | awk '{print $4}'|sed 's/[!A-Z]*//g'` 2>/dev/null
DISK_HOME=`df -hPBG /home | sed '2q;d' | awk '/\/home/{print $4}'|sed 's/[!A-Z]*//g'` 2>/dev/null
CPU=`nproc` 2>/dev/null
TOTDISK=`df -h --total | grep total | awk '{print $2}'` 2>/dev/null
RAM=`free -g| sed -e's/  */ /g' | grep Mem | cut -d " " -f 2` 2>/dev/null
echo "$KERNEL|$BANDWIDTH|$OS|$DISK_ROOT|$DISK_HOME|$CPU|$TOTDISK|$RAM"
echo "----"
rm -f .*.$$
exit 0
