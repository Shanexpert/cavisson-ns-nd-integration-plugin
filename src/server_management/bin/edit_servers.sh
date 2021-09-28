usage()
{
    echo "Error:This shell requires arguments"
    echo "$0 <Server Name> <Blade Name> <Allocation> <Machine Type>"
}
conf_dir=$NS_WDIR/server_management/conf
config_file=$conf_dir/NCP.conf

if [ $# -lt 5 ]
then
  echo "1"
  usage
  exit 1
fi

SERVER=$1
BLADE=$2
ALLOCATION=$3
MACHINE_TYPE=$4
security_group=$5
SESSUSER=$6
rules=( $(awk '/\[securityGroups\]/{flag=1;next}/\[*\]/{flag=0}flag' $config_file|awk -F'=' '! /^#/ && ! /^$/{gsub("\\s+", "");sub(/#.*/,""); print $1 }') )
if [ "xx$ALLOCATION" == "xxDedicated" ] || [ "xx$ALLOCATION" == "xxAdditional" ] || [ "xx$ALLOCATION" == "xxFree" ] || [ "xx$ALLOCATION" == "xxReserved" ]
then
  var="1"
else
  echo "1"
  echo "Allocation can be only Dedicated, Additional, Reserved or Free"
  exit 1
fi

if [ "xx$MACHINE_TYPE" == "xxGenerator" ] || [ "xx$MACHINE_TYPE" == "xxController" ] || [ "xx$MACHINE_TYPE" == "xxNetstorm" ] || [ "xx$MACHINE_TYPE" == "xxNO" ]
then
  var="1"
else
  echo "1"
  echo "Machine Type can be only Generator, Controller, Netstorm or Free"
  exit 1
fi

if ! [[ " ${rules[*]} " != *" ${security_group} "* ]]; then
  echo "1"
  echo -n -e "Invalid security group.\nValid security groups are ${rules[*]}\n"
  exit 1
fi

CHECK_SERVER_NAME=`psql  -X  -A -U postgres -d demo -t -c "select server_name from allocation where LOWER(server_name)=LOWER('$SERVER')"`
if [ "xx$CHECK_SERVER_NAME" != "xx" ]
then
  CHECK_BLADE_NAME=`psql  -X  -A -U postgres -d demo -t -c "select blade_name from allocation where LOWER(blade_name)=LOWER('$BLADE')"`
  if [ "xx$CHECK_BLADE_NAME" != "xx" ]
  then
    CHECK_ALLOCATION=`psql  -X  -A -U postgres -d demo -t -c "select allocation from allocation where server_name='$SERVER' and blade_name='$BLADE'"`
    CHECK_MACHINE_TYPE=`psql  -X  -A -U postgres -d demo -t -c "select machine_type from allocation where server_name='$SERVER' and blade_name='$BLADE'"`
    if [ "xx$CHECK_ALLOCATION" == "xx$ALLOCATION" ] && [ "xx$CHECK_MACHINE_TYPE" == "xx$MACHINE_TYPE" ]
    then
      echo "0"
      echo "No change in allocation and machine type status"
      echo "$SESSUSER|$(date)|No change in allocation and machine type status" >> $NS_WDIR/server_management/logs/addDelete.log
    else
      if [ "xx$CHECK_ALLOCATION" != "xx$ALLOCATION" ] && [ "xx$CHECK_MACHINE_TYPE" == "xx$MACHINE_TYPE" ]
      then
        psql  -X  -A -U postgres -d demo -t -c "update allocation set allocation='$ALLOCATION' where server_name='$SERVER' and blade_name='$BLADE'" > /dev/null
        if [ $? -eq 0 ]
        then
          echo "0"
          echo "Allocation status of $SERVER and $BLADE is changed from $CHECK_ALLOCATION to $ALLOCATION"
          echo "$SESSUSER|$(date)|Allocation status of $SERVER and $BLADE is changed from $CHECK_ALLOCATION to $ALLOCATION" >> $NS_WDIR/server_management/logs/addDelete.log
        else
          echo "1"
          echo "Error in updating database"
          exit 1
        fi
      elif [ "xx$CHECK_ALLOCATION" == "xx$ALLOCATION" ] &&  [ "xx$CHECK_MACHINE_TYPE" != "xx$MACHINE_TYPE" ]
      then
        psql  -X  -A -U postgres -d demo -t -c "update allocation set machine_type='$MACHINE_TYPE' where server_name='$SERVER' and blade_name='$BLADE'" > /dev/null
        if [ $? -eq 0 ]
        then
          echo "0"
          echo "Machine Type of $SERVER and $BLADE is changed from $CHECK_MACHINE_TYPE to $MACHINE_TYPE"
          echo "$SESSUSER|$(date)|Machine Type of $SERVER and $BLADE is changed from $CHECK_MACHINE_TYPE to $MACHINE_TYPE" >> $NS_WDIR/server_management/logs/addDelete.log
        else
          echo "1"
          echo "Error in updating database"
          exit 1
        fi
      elif [ "xx$CHECK_ALLOCATION" != "xx$ALLOCATION" ] &&  [ "xx$CHECK_MACHINE_TYPE" != "xx$MACHINE_TYPE" ]
      then
        psql  -X  -A -U postgres -d demo -t -c "update allocation set machine_type='$MACHINE_TYPE', allocation='$ALLOCATION'  where server_name='$SERVER' and blade_name='$BLADE'" > /dev/null
        if [ $? -eq 0 ]
        then
          echo "0"
          echo  "Server $SERVER and Blade $BLADE\n Allocation changed from $CHECK_ALLOCATION to $ALLOCATION and Machine Type changed from $CHECK_MACHINE_TYPE to $MACHINE_TYPE"
          echo "$SESSUSER|$(date)|Server $SERVER and Blade $BLADE\n Allocation changed from $CHECK_ALLOCATION to $ALLOCATION and Machine Type changed from $CHECK_MACHINE_TYPE to $MACHINE_TYPE" >> $NS_WDIR/server_management/logs/addDelete.log
        else
          echo "1"
          echo "Error in updating database"
          exit 1
        fi
      fi
    fi
  else
    echo "1"
    echo "Blade name $BLADE does not exist for $SERVER"
    echo "$SESSUSER|$(date)|Blade name $BLADE does not exist for $SERVER" >> $NS_WDIR/server_management/logs/addDelete.log
    exit 1
  fi
else
  echo "1"
  echo "Server Name $SERVER not found in the database."
  echo "$SESSUSER|$(date)|Server Name $SERVER not found in the database." >> $NS_WDIR/server_management/logs/addDelete.log
  exit 1
fi
