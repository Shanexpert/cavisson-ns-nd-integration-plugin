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
  echo "-u to add session login name"
}

if [ $# -eq 0 ]
then
  echo "ERROR:This shell requires arguments"
  usage
  exit 1
fi

list_teams()
{
#psql  -X  -A -U postgres -d demo -t -c "select team, channel , owner, count (machine_type) as count from allocation group by team, channel, owner order by team;"
psql -X  -A -U postgres -d demo -t << EOF
  SELECT clients.team, clients.channel, clients.owner, count(allocation.machine_type) as TOT
  FROM allocation right outer join clients using(team,channel)
  GROUP BY clients.team, clients.channel, clients.owner
  ORDER BY team;
EOF
}

list_only_teams()
{
  psql  -X  -A -U postgres -d demo -t -c "select distinct team from clients order by team"
}

#need variables TEAM, CHANNEL and OWNER to be set
add_team()
{
  if [ "xx$TEAM" == "xx" ] || [ "xx$CHANNEL" == "xx" ] || [ "xx$OWNER" == "xx" ]
  then
    echo "1"
    echo "parameters are not correct"
    exit 1
  fi
  CHECK_TEAM=`psql  -X  -A -U postgres -d demo -t -c "select distinct team from clients where LOWER(team)=LOWER('$TEAM')"`
  if [ "xx$CHECK_TEAM" == "xx" ]
  then
    psql  -X  -A -U postgres -d demo -t << EOF 2>&1 1>/dev/null
    insert into clients values ('$TEAM','$CHANNEL','$OWNER');
    insert into clients values ('$TEAM','default','$OWNER');
EOF
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
    echo "Entered team already exists as $CHECK_TEAM"
    echo "$SESSUSER|$(date)|Entered team $TEAM already exists as $CHECK_TEAM" >> $log_dir/addDelete.log
  fi
}

delete_team()
{
  if [ "xx$TEAM" == "xx" ]
  then
    echo "1"
    echo "parameters are not correct"
    exit 1
  fi
  CHECK_TEAM=`psql  -X  -A -U postgres -d demo -t -c "select distinct team from clients where LOWER(team)=LOWER('$TEAM')"`
  if [ "xx$CHECK_TEAM" == "xx$TEAM" ]
  then
    COUNT=`psql -X -A -U postgres -d demo -t -c "select count(*) from allocation where team='$TEAM'"`
    if [ $COUNT -eq 0 ]
    then
      psql -X -A -U postgres -d demo -t -c "delete from clients where team='$TEAM'" 2>&1 1>/dev/null
      if [ $? -eq 0 ]
      then
        echo "0"
        echo "Team $TEAM deleted successfully"
        echo "$SESSUSER|$(date)|Team $TEAM deleted successfully" >> $log_dir/addDelete.log
      else
        echo "1"
        echo "Error in updating database"
      fi
    else
      echo "1"
      echo "$COUNT servers are assigned to this team. Please remove them first"
      echo "$SESSUSER|$(date)|$COUNT servers are assigned to $TEAM team." >> $log_dir/addDelete.log
    fi
  else
    echo "1"
    echo "Team $TEAM is not found in database"
  fi
}



while getopts t:c:o:dalOu: arg
do
	case $arg in
		t)TEAM=$OPTARG;;
		c)CHANNEL=$OPTARG;;
		o)OWNER=$OPTARG;;
    u)SESSUSER=$OPTARG;;
    d)delete_team;;
    a)add_team;;
		l)list_teams;;
		O)list_only_teams;;
		*)usage ;;
	esac
done
