#nsu_server_admin  -s $1 -g -c "df -h";

tomcat()
{
  timeout 10s nsu_server_admin  -s $IP -g -c "nsi_get_controller_name" > file.temp
  timeout 10s nsu_server_admin  -s $IP -g -c "ps -ef" > file1.temp
  while read line
  do
    echo -n -e "\n\n$line\n\n"
    cat file1.temp | grep tomcat |grep $line
  done < file.temp
}

while getopts i:abcdefghi arg
do
  case $arg in
    i) IP=$OPTARG;;
    a) timeout 10s nsu_server_admin  -s $IP -g -c "df -h";;
    b) timeout 10s nsu_server_admin  -s $IP -g -c "ps -ef" | grep "postgres";;
    c) timeout 10s nsu_server_admin  -s $IP -g -c "free -lg";;
    d) tomcat ;;
    e) timeout 10s nsu_server_admin  -s $IP -g -c "top -b -n 1";;
    f) timeout 10s nsu_server_admin  -s $IP -g -c "ifconfig";;
    g) timeout 10s nsu_server_admin  -s $IP -g -c "netstat -ntp";;
    h) timeout 10s nsu_server_admin  -s $IP -g -c "lscpu";;
    *) echo "Invalid Option";;
  esac
done
