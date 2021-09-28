#!/bin/bash
log_dir=$NS_WDIR/server_management/logs
usage()
{
  echo "-t to set Team"
  echo "-c to set Channel"
  echo "-o to set owner"
  echo "-d to delete"
  echo "-a to add team"
  echo "-l to list teams"
  echo "-o to list teams in other format"
  echo "-u to add session login user"
}

if [ $# -eq 0 ]
then
  echo "ERROR:This shell requires arguments"
  usage
  exit 1
fi

list_channels()
{
  psql  -X  -A -U postgres -d demo -t -c "select team, channel from clients"
}

add_channel()
{
  if [ "XX$TEAM" == "XX" ] || [ "XX$CHANNEL" == "XX" ] || [ "XX$OWNER" == "XX" ]
  then
    echo "1"
    echo "parameters are not correct"
    exit 1
  fi
  CHECK_TEAM=`psql  -X  -A -U postgres -d demo -t -c "select distinct team from clients where LOWER(team)=LOWER('$TEAM')"`
  if [ "xx$CHECK_TEAM" == "xx$TEAM" ]
  then
    CHECK_CHANNEL=`psql  -X  -A -U postgres -d demo -t -c "select distinct channel from clients where LOWER(team)=LOWER('$TEAM') and LOWER(channel)=LOWER('$CHANNEL')"`
    if [ "XX" == "XX$CHECK_CHANNEL" ]
    then
      psql  -X  -A -U postgres -d demo -t -c "insert into clients values ('$TEAM','$CHANNEL','$OWNER')" 2>&1 1>/dev/null
      if [ $? -eq 0 ]
      then
        echo "0"
        echo "Team : $TEAM Channel : $CHANNEL Owner : $OWNER is successfully added to the database"
        echo "$SESSUSER|$(date)|Team : $TEAM Channel : $CHANNEL Owner : $OWNER is successfully added to the database" >> $log_dir/addDelete.log
      else
        echo "1"
        echo "Error in updating database"
      fi
    else
      echo "1"
      echo "Entered channel already exists as $CHECK_CHANNEL for $TEAM"
      echo "$SESSUSER|$(date)|Entered channel $CHANNEL already exists as $CHECK_CHANNEL for $TEAM" >> $log_dir/addDelete.log
    fi
  else
    echo "1"
    echo "Team $TEAM is not found in the database"
  fi
}

delete_channel()
{
  if [ "xx$TEAM" == "xx" ] || [ "XX$CHANNEL" == "XX" ]
  then
    echo "1"
    echo "parameters are not correct"
    exit 1
  fi
  CHECK_CHANNEL=`psql  -X  -A -U postgres -d demo -t -c "select distinct channel from clients where LOWER(team)=LOWER('$TEAM') and LOWER(channel)=LOWER('$CHANNEL')"`

  if [ "xx$CHECK_CHANNEL" == "xx$CHANNEL" ]
  then
    COUNT=`psql -X -A -U postgres -d demo -t -c "select count(*) from allocation where team='$TEAM' and channel='$CHANNEL'"`
    if [ $COUNT -eq 0 ]
    then
      psql -X -A -U postgres -d demo -t -c "delete from clients where team='$TEAM' and channel='$CHANNEL'" 2>&1 1>/dev/null
      if [ $? -eq 0 ]
      then
        echo "0"
        echo "Channel $CHANNEL deleted successfully"
	echo "$SESSUSER|$(date)|Channel: $CHANNEL Team: $TEAM deleted successfully" >> $log_dir/addDelete.log
      else
        echo "1"
        echo "Error in updating database"
      fi
    else
      echo "1"
      echo "$COUNT servers are assigned to team $TEAM and channel $CHANNEL. Please remove them first"
      echo "$SESSUSER|$(date)|$COUNT servers are assigned to team $TEAM and channel $CHANNEL" >> $log_dir/addDelete.log
    fi
  else
    echo "1"
    echo "Channel $CHANNEL is not found in database for Team $TEAM"
    echo "$SESSUSER|$(date)|Channel $CHANNEL is not found in database for Team $TEAM" >> $log_dir/addDelete.log
  fi
}

while getopts t:c:o:dalOu: arg
do
	case $arg in
		t)TEAM=$OPTARG;;
		c)CHANNEL=$OPTARG;;
		o)OWNER=$OPTARG;;
    u)SESSUSER=$OPTARG;;
    d)delete_channel;;
    a)add_channel;;
		l)list_channels;;
		*)usage ;;
	esac
done
