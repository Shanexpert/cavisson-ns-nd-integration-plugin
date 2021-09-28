ALL=" server_name = ANY (select server_name from allocation) "
TEAM=" ANY (select team from allocation) "
PROJECT=" ANY (select channel from allocation) "
OWNER=" ANY (select owner from allocation) "
UBUNTU_VERSION=" ANY (select ubuntu_version from allocation) "
STATUS=" ANY (select status from allocation) "
MACHINE_TYPE=" ANY (select machine_type from allocation) "
ALLOCATION=" ANY (select allocation from allocation) "
BUILD_VERSION=" ANY (select build_version from allocation) "
SERVER_TYPE=" ANY (select server_type from allocation) "
SERVER_NAME=" ANY (select server_name from allocation) "
SERVER_IP=" ANY (select server_ip from allocation) "



while getopts t:p:o:u:s:m:a:b:c:n:i:ASD arg 
do
        case $arg in
                t) TEAM="'$OPTARG'"
                   if [ "$OPTARG" == "all" ]
                   then
                   	TEAM=" ANY (select team from allocation) "
                   fi;;
                p) PROJECT="'$OPTARG'"
                   if [ "$OPTARG" == "all" ]
                   then
                   	PROJECT=" ANY (select channel from allocation) "
                   fi;;
                o) OWNER="'$OPTARG'"
                   if [ "$OPTARG" == "all" ]
                   then
                   	OWNER=" ANY (select owner from allocation) "
                   fi;;
                u) UBUNTU_VERSION="'$OPTARG'"
                   if [ "$OPTARG" == "all" ]
                   then
                   UBUNTU_VERSION=" ANY (select ubuntu_version from allocation) "
                   fi;;
		s) STATUS="'$OPTARG'"
		   if [ "$OPTARG" == "all" ]
                   then
                   	STATUS=" ANY (select status from allocation) "
                   fi;;
                m) MACHINE_TYPE="'$OPTARG'"
                   if [ "$OPTARG" == "all" ]
                   then
                   	MACHINE_TYPE=" ANY (select machine_type from allocation) "
                   fi;;
                a) ALLOCATION="'$OPTARG'"
                   if [ "$OPTARG" == "all" ]
                   then
                   	ALLOCATION=" ANY (select allocation from allocation) "
                   fi;;
                b) BUILD_VERSION="'$OPTARG'"
                
                   if [ "$OPTARG" == "all" ]
                   then
                   	BUILD_VERSION=" ANY (select build_version from allocation) "
                   fi;;
                c) SERVER_TYPE="'$OPTARG'"
                   if [ "$OPTARG" == "all" ]
                   then
                   	SERVER_TYPE=" ANY (select server_type from allocation) "
                   fi;;
		n) SERVER_NAME="'$OPTARG'"
		   if [ "$OPTARG" == "all" ]
                   then
                   	SERVER_NAME=" ANY (select server_name from allocation) "
                   fi;;
		i) SERVER_IP="'$OPTARG'"
		   if [ "$OPTARG" == "all" ]
                   then
                   	SERVER_IP=" ANY (select server_ip from allocation) "
                   fi;;
		A) TOGGLE=1;;
		S) TOGGLE=2;;
		D) TOGGLE=3;;
                *) echo -n -e "Please use correct option. \n"
                   echo -n -e "Use -s <start index number> -e <end index number> -n <index numbers> -v <except index numbers> -a \n"
                   echo -n -e "Use \",\" to seperate multiple indices in -v and -n options.\n" 
			exit;;

        esac
done

if [ "xx$TOGGLE" == "xx1"  ]
then
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (), blade_name, machine_type, team, channel, owner, allocation, build_version, build_upgradation_date, controller_ip, controller_blade from allocation where team=$TEAM AND channel=$PROJECT AND owner=$OWNER AND ubuntu_version=$UBUNTU_VERSION AND machine_type=$MACHINE_TYPE AND allocation=$ALLOCATION and build_version=$BUILD_VERSION AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP order by server_name"|sed ':a;N;$!ba;s/\n/+/g'
psql -X  -A -U postgres -d demo -t -c "select distinct server_name, server_ip, ubuntu_version, status, server_type from allocation where team=$TEAM AND channel=$PROJECT AND owner=$OWNER AND ubuntu_version=$UBUNTU_VERSION AND machine_type=$MACHINE_TYPE AND allocation=$ALLOCATION and build_version=$BUILD_VERSION AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP order by server_name"|sed ':a;N;$!ba;s/\n/+/g'
psql -X  -A -U postgres -d demo -t -c "select vendor,location,zone,cpu,ram,total_disk_size,case when security_group is null or security_group='' then 'generator' else security_group end from servers where server_ip=$SERVER_IP order by server_name "|sed ':a;N;$!ba;s/\n/+/g'
elif [ "xx$TOGGLE" == "xx2"  ]
then
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (order by server_name nulls last) as rownum , * from servers order by server_name"
elif [ "xx$TOGGLE" == "xx3" ]
then
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (order by server_name nulls last) as rownum, server_name, server_ip, blade_name, ubuntu_version, machine_type, status, team, channel, owner, allocation, build_version, build_upgradation_date, refresh_at, bandwidth from allocation where team=$TEAM AND channel=$PROJECT AND owner=$OWNER AND ubuntu_version=$UBUNTU_VERSION AND machine_type=$MACHINE_TYPE AND allocation=$ALLOCATION and build_version=$BUILD_VERSION AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP AND status=$STATUS order by server_name" 
#cat file.temp | sed ':a;N;$!ba;s/|/,/g' > /home/netstorm/work/webapps/domainadmin/admin/master_blades.csv	
#rm -f file.temp
else
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (order by server_name nulls last) as rownum, * from allocation where team=$TEAM AND channel=$PROJECT AND owner=$OWNER AND ubuntu_version=$UBUNTU_VERSION AND machine_type=$MACHINE_TYPE AND allocation=$ALLOCATION and build_version=$BUILD_VERSION AND server_type=$SERVER_TYPE AND server_name=$SERVER_NAME AND server_ip=$SERVER_IP order by server_name"
#|sed ':a;N;$!ba;s/\n/|/g'
fi
