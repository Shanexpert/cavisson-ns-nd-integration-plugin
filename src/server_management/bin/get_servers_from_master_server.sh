ALL=" server_name = ANY (select server_name from servers) "
STATUS=" ANY (select status from allocation) "
SERVER_TYPE=" ANY (select server_type from servers) "
SERVER_NAME=" ANY (select server_name from servers) "
SERVER_IP=" ANY (select server_ip from servers) "
VENDOR=" ANY (select vendor from servers) "
LOCATION=" ANY (select location from servers) "
ZONE=" ANY (select zone from servers) "

while getopts s:z:l:v:t:n:i:ASD arg
do
  case $arg in
      s) if [ "$OPTARG" != "all" ]
        then
         STATUS="'$OPTARG'"
        fi;;
      z) if [ "$OPTARG" != "all" ]
        then
         ZONE="'$OPTARG'"
        fi;;
      l) if [ "$OPTARG" != "all" ]
        then
         LOCATION="'$OPTARG'"
        fi;;
      v) if [ "$OPTARG" != "all" ]
        then
         VENDOR="'$OPTARG'"
        fi;;
      t) if [ "$OPTARG" != "all" ]
        then
         SERVER_TYPE="'$OPTARG'"
        fi;;
      n) if [ "$OPTARG" != "all" ]
        then
          SERVER_NAME="'$OPTARG'"
        fi;;
      i) if [ "$OPTARG" != "all" ]
        then
         SERVER_IP="'$OPTARG'"
        fi;;
      A) TOGGLE=1;;
      S) TOGGLE=2;;
      D) TOGGLE=3;;
      *)  echo -n -e "Please use correct option. \n"
          echo -n -e "Use -s <start index number> -e <end index number> -n <index numbers> -v <except index numbers> -a \n"
          echo -n -e "Use \",\" to seperate multiple indices in -v and -n options.\n"
          exit;;

  esac
done

if [ "xx$TOGGLE" == "xx1"  ]
then
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (), blade_name, machine_type, team, channel, owner, allocation, build_version, build_upgradation_date, controller_ip, controller_blade from allocation where team=$TEAM AND channel=$PROJECT AND owner=$OWNER AND ubuntu_version=$UBUNTU_VERSION AND machine_type=$MACHINE_TYPE AND allocation=$ALLOCATION and build_version=$BUILD_VERSION AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP order by server_name"|sed ':a;N;$!ba;s/\n/+/g'
psql -X  -A -U postgres -d demo -t -c "select distinct server_name, server_ip, ubuntu_version, status, server_type from allocation where team=$TEAM AND channel=$PROJECT AND owner=$OWNER AND ubuntu_version=$UBUNTU_VERSION AND machine_type=$MACHINE_TYPE AND allocation=$ALLOCATION and build_version=$BUILD_VERSION AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP order by server_name"|sed ':a;N;$!ba;s/\n/+/g'
psql -X  -A -U postgres -d demo -t -c "select vendor,location,zone,cpu,ram,total_disk_size from servers where server_ip=$SERVER_IP order by server_name "|sed ':a;N;$!ba;s/\n/+/g'
elif [ "xx$TOGGLE" == "xx2"  ]
then
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (order by server_name nulls last) as rownum , * from servers order by server_name"
elif [ "xx$TOGGLE" == "xx3" ]
then
	psql -X  -A -U postgres -d demo -t -c "SELECT row_number() over (ORDER BY server_name nulls last) AS rownum, server_name, server_ip, vendor, location, state, country, zone, cpu, ram, total_disk_size, avail_disk_root, avail_disk_home, kernal, refresh_at, CASE WHEN security_group is null OR security_group='' THEN 'generator' ELSE security_group END, (SELECT report_file FROM security_scan WHERE server_name=servers.server_name ORDER BY start_time DESC LIMIT 1), (SELECT vport FROM security_scan WHERE server_name=servers.server_name ORDER BY start_time DESC LIMIT 1), model, cpu_freq, spec_rating FROM servers WHERE vendor=$VENDOR AND location=$LOCATION AND zone=$ZONE AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP AND server_name in (select server_name from allocation where status=$STATUS) order by server_name"|sed 's/,/ /g'
#cat file.temp.$$ | sed ':a;N;$!ba;s/|/,/g' > $NS_WDIR/webapps/domainadmin/admin/master_servers.csv

else
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (order by server_name nulls last) as rownum, * from servers where vendor=$VENDOR AND location=$LOCATION AND zone=$ZONE AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP AND server_name in (select server_name from allocation where status=$STATUS)order by server_name"
#|sed ':a;N;$!ba;s/\n/|/g'
fi
