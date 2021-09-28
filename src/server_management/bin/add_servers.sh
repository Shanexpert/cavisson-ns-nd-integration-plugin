INPUT_SERVER_NAME=$1
INPUT_SERVER_IP=$2
INPUT_VENDOR=$3
INPUT_LOCATION=$4
INPUT_SERVER_TYPE=$5
INPUT_STATE=$6
INPUT_COUNTRY=$7
SESSUSER=$8
templatedir=$NS_WDIR/server_management/template

cleanup()
{
 rm -f /tmp/cmon.env.$$ a.temp.$$ b.temp.$$ c.temp.$$ blade_with_version.temp.$$ file.temp.$$
}
update_cmon_env()
{
  export server=$1
  export ip=$2
  export tier=`echo $server | rev | cut -d "-" -f2- | rev`
  export ND_IP
  envsubst < $templatedir/cmon.env.template > /tmp/cmon.env.$$
  nsu_server_admin -s $ip -g -F /tmp/cmon.env.$$ -D /home/cavisson/monitors/sys/
  nsu_server_admin -s $ip -g -c "mv /home/cavisson/monitors/sys/cmon.env.$$ /home/cavisson/monitors/sys/cmon.env"
  nsu_server_admin -s $ip -g -r >/dev/null
}

if [ $# -ne 8 ]
then
	echo "Invalid number of arguments"
	echo "add_servers.sh <server_name> <server ip> <vendor> <location> <server type> <State> <country> <session login username>"
	exit
fi

check_build()
{
  timeout 15s nsu_server_admin -s $INPUT_SERVER_IP -g -c 'nsu_get_version -n -A' | grep -v "tput" | grep -v "No such file or directory" > a.temp.$$ 2> check_build_err.log
  awk 'NR % 3 == 0' a.temp.$$ | cut -d " " -f 2,3,4  > b.temp.$$
  awk 'NR % 3 == 1' a.temp.$$ > c.temp.$$
  paste c.temp.$$ b.temp.$$ -d "|" |sed '$d' | sed -r 's/\s+//g' > blade_with_version.temp.$$
}

validate_server()
{
  ping -c 2 -W 10 $INPUT_SERVER_IP > /dev/null 2>&1
  if [ $? -eq 0 ]
  then
    timeout 6 nsu_server_admin -s $INPUT_SERVER_IP -g -c 'date' 2>&1 > /dev/null
    if [ $? -eq 0 ]
    then
      timeout 6 nsu_server_admin -s $INPUT_SERVER_IP -g -c 'free -g' > file.temp.$$ 2>/dev/null
      RAM=`cat file.temp.$$ | sed -e's/  */ /g' | grep Mem | cut -d " " -f 2`
      timeout 6 nsu_server_admin -s $INPUT_SERVER_IP -g -c 'lsblk -P' > file.temp.$$ 2>/dev/null
      DISK_DISPLAY=`cat file.temp.$$| grep 'TYPE="disk"' | cut -d " " -f 1,4`
      DISK=`echo $DISK_DISPLAY|cut -d '"' -f 4|sed '$!{:a;N;s/\n/ + /;ta}'`
      timeout 6 nsu_server_admin -s $INPUT_SERVER_IP -g -c 'nproc' > file.temp.$$ 2>/dev/null
      CPU=`cat file.temp.$$`
      check_build
      BLADES_DISPLAY=`cat blade_with_version.temp.$$ |sed ':a;N;$!ba;s/\n/+/g'`
      UBUNTU_VERSION=`nsu_server_admin -s 184.105.48.8 -i -c 'lsb_release -a'| grep Release | awk '{print $2}'`
      STATE=0
    else
      STATE=1
    fi
  else
    STATE=2
  fi
}

CHECK_SERVER_NAME=`psql  -X  -A -U postgres -d demo -t -c "select server_name from servers where LOWER(server_name)=LOWER('$INPUT_SERVER_NAME')"`
if [ "xx" == "xx$CHECK_SERVER_NAME" ]; then
  CHECK_SERVER_IP=`psql  -X  -A -U postgres -d demo -t -c "select server_ip from servers where LOWER(server_ip)=LOWER('$INPUT_SERVER_IP')"`
  if [[ "xx$INPUT_SERVER_IP" != "xx$CHECK_SERVER_IP" ]]; then
    validate_server
    if [[ $STATE -eq 0 ]]; then
      CHECK_VENDOR=`psql  -X  -A -U postgres -d demo -t -c "select vendor from vendor where vendor='$INPUT_VENDOR'"`
      if [[ "xx$INPUT_VENDOR" == "xx$CHECK_VENDOR" ]]; then
        CHECK_LOCATION=`psql  -X  -A -U postgres -d demo -t -c "select location, state, country from location where LOWER(location)=LOWER('$INPUT_LOCATION') AND LOWER(state)=LOWER('$INPUT_STATE') AND LOWER(country)=LOWER('$INPUT_COUNTRY')"`
        if [[ "xx" != "xx$CHECK_LOCATION" ]]; then
          if [ "XX$INPUT_SERVER_TYPE" == "XXCC" ] || [ "XX$INPUT_SERVER_TYPE" == "XXVP" ] || [ "XX$INPUT_SERVER_TYPE" == "XXVM" ]; then
            psql  -X  -A -U postgres -d demo -t -c "insert into servers(server_name, server_ip, vendor, location, zone, cpu, ram, total_disk_size, server_type, state, country) SELECT '$INPUT_SERVER_NAME','$INPUT_SERVER_IP','$INPUT_VENDOR','$INPUT_LOCATION', zone ,'$CPU','$RAM','$DISK','$INPUT_SERVER_TYPE', '$INPUT_STATE', '$INPUT_COUNTRY' from location where location='$INPUT_LOCATION' AND state='$INPUT_STATE' AND country='$INPUT_COUNTRY'" > /dev/null
            if [[ "XX$INPUT_SERVER_TYPE" == "XXVM" ]]; then
              psql -X -A -U postgres -d demo -t -c "insert into billing(server_name, server_ip, vendor, status, uptime, uptime_epoch) values('$INPUT_SERVER_NAME', '$INPUT_SERVER_IP', '$INPUT_VENDOR','t','$(date '+%D %T')','$(date +%s)')" > /dev/null 2>/dev/null
            fi
            while read UPLOAD
            do
              blade_name=`echo $UPLOAD | cut -d "|" -f 1`
              blade_version=`echo $UPLOAD | cut -d "|" -f 2`
              if [[ "XX$INPUT_SERVER_TYPE" != "CC" ]]; then
                if [[ "xx$blade_name" == "xxwork" ]]; then
                  MACHINE_TYPE="Generator"
                  ALLOCATION="Free"
                else
                  MACHINE_TYPE="Netstorm"
                  ALLOCATION="Reserved"
                fi
              else
                MACHINE_TYPE="Netstorm"
                ALLOCATION="Free"
              fi
              psql  -X  -A -U postgres -d demo -t -c "insert into allocation values ('$INPUT_SERVER_NAME','$INPUT_SERVER_IP','$blade_name','$UBUNTU_VERSION','$MACHINE_TYPE','t','NA','NA','NA','$ALLOCATION','$blade_version','$(date)','$INPUT_SERVER_TYPE')" > /dev/null 2>&1
            done < blade_with_version.temp.$$
            if [[ $? -eq 0 ]]; then
              echo "0"
              echo "Server $INPUT_SERVER_NAME successfully added."
              echo "$SESSUSER|$(date)|Server $INPUT_SERVER_NAME successfully added." >> $NS_WDIR/server_management/logs/addDelete.log
            else
              echo "1"
              echo "Error while adding server to database"
              echo "$SESSUSER|$(date) | Error while adding server $INPUT_SERVER_NAME to database" >> $NS_WDIR/server_management/logs/addDelete.log
              exit 1
            fi
            if [[ -e $NS_WDIR/server_management/conf/NCP.conf ]]; then
              ND_IP=$(grep -v "^#" $NS_WDIR/server_management/conf/NCP.conf | grep ND_IP |sed -r 's/\s+//g' | cut -d "=" -f 2)
              if [[ ! -z $ND_IP ]]; then
                update_cmon_env $INPUT_SERVER_NAME $INPUT_SERVER_IP
              fi
            fi
            echo -n -e "$INPUT_SERVER_NAME\n$INPUT_SERVER_IP\n$INPUT_VENDOR\n$INPUT_LOCATION\n$INPUT_ZONE\n$INPUT_SERVER_TYPE\n$CPU\n$RAM\n$DISK\n$UBUNTU_VERSION\n$BLADES_DISPLAY"
          else
            echo "1"
            echo "Error:Invalid Server Type"
            exit 1
          fi
        else
          echo "1"
          echo "Error:Location City: $INPUT_LOCATION, State: $INPUT_STATE, Country: $INPUT_COUNTRY does not exists"
          exit 1
        fi
      else
        echo "1"
        echo "Error:Vendor $INPUT_VENDOR does not exists"
        exit 1
      fi
    elif [[ $STATE -eq 2 ]]; then
      echo "1"
      echo "Error:Server is not reachable"
      exit 1
    else
      echo "1"
      echo "Error:Cmon is not running on the server"
      exit 1
    fi
  else
    echo "1"
    echo "Error:$INPUT_SERVER_IP already exists"
    exit 1
  fi
else
  echo "1"
  echo "Error:$INPUT_SERVER_NAME already exists"
  exit 1
fi
