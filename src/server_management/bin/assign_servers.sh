#!/bin/bash
log_file=$NS_WDIR/server_management/logs/assignment.log

usage()
{
	echo "Error: This requires argument. $0 <Server Name+Blade Name> <team> <channel> <allocation>"
}

if [ $# -ne 2 ]
then
	usage
	exit 1
fi


main_func()
{
	CHECK_SERVER_NAME=`psql  -X  -A -U postgres -d demo -t -c "select distinct server_name from allocation where LOWER(server_name)=LOWER('$SERVER_NAME')"`
	if [ "xx$CHECK_SERVER_NAME" == "xx$SERVER_NAME" ]
	then
		CHECK_BLADE_NAME=`psql  -X  -A -U postgres -d demo -t -c "select blade_name from allocation where LOWER(blade_name)=LOWER('$BLADE_NAME') and server_name='$SERVER_NAME'"`
		if [ "xx$CHECK_BLADE_NAME" == "xx$BLADE_NAME" ]
		then
			CHECK_TEAM=`psql  -X  -A -U postgres -d demo -t -c "select distinct team from clients where LOWER(team)=LOWER('$TEAM')"`
			if [ "xx$CHECK_TEAM" == "xx$TEAM" ] || [ "xx$TEAM" == "xxNA" ]
			then
				CHECK_CHANNEL=`psql  -X  -A -U postgres -d demo -t -c "select distinct channel from clients where LOWER(channel)=LOWER('$CHANNEL') and team='$TEAM'"`
				if [ "xx$CHECK_CHANNEL" == "xx$CHANNEL" ] || [ "xx$CHANNEL" == "xxNA" ]
				then
					if [ "xx$ALLOCATION" == "xxFree" ] || [ "xx$ALLOCATION" == "xxDedicated" ] || [ "xx$ALLOCATION" == "xxAdditional" ] || [ "xx$ALLOCATION" == "xxReserved" ]
					then
						CURR_ALLOC=`psql  -X  -A -U postgres -d demo -t -c "select distinct allocation from allocation where server_name='$SERVER_NAME' and blade_name='$BLADE_NAME'"`
						if [ "xx$ALLOCATION" == "xxFree" ]
						then
							if [[ "xx$CURR_ALLOC" == "xxReserved" ]]; then
								ALLOCATION="Reserved"
							fi
							TEAM="NA"
							CHANNEL="NA"
							OWNER="NA"
							psql -X -A -U postgres -d demo -c "update allocation set bkp_ctrl='none' , bkp_blade='none', shared=null where server_name='$SERVER_NAME' and blade_name='$BLADE_NAME'"
						else
							if [[ "xx$CURR_ALLOC" == "xxReserved" ]]; then
								ALLOCATION="Reserved"
							fi
							OWNER=`psql  -X  -A -U postgres -d demo -t -c "select distinct owner from clients where team='$TEAM' and channel='$CHANNEL'"`
						fi
						psql  -X  -A -U postgres -d demo -t -c "update allocation set team='$TEAM', channel='$CHANNEL', owner='$OWNER' , allocation='$ALLOCATION' where server_name='$SERVER_NAME' and blade_name='$BLADE_NAME'" > /dev/null
						if [ $? -eq 0 ]
						then
							echo "Allocation of $SERVER_NAME and $BLADE_NAME changed to team = $TEAM channel = $CHANNEL and owner = $OWNER"
							echo "$SESSUSER|$(date)|Allocation of $SERVER_NAME and $BLADE_NAME changed to team = $TEAM channel = $CHANNEL and owner = $OWNER" >> $log_file
							return 0
						else
							echo "Database Error"
							echo "$SESSUSER|$(date)|Database error while assigning $SERVER_NAME and $BLADE_NAME changed to team = $TEAM channel = $CHANNEL and owner = $OWNER" >> $log_file
							return 1
						fi
					else
						echo "Allocation can be only Additional, Dedicated or Free"
						echo "$SESSUSER|$(date)|Allocation $ALLOCATION is neither Additional, Dedicated or Free for $SERVER_NAME and $BLADE_NAME" >> $log_file
						return 1
					fi
				else
					echo "Channel $CHANNEL not found for $TEAM"
					echo "$SESSUSER|$(date)|Channel $CHANNEL not found for $TEAM" >> $log_file
					return 1
				fi
			else
				echo "Team $TEAM not found"
				echo "$SESSUSER|$(date)|Team $TEAM not found" >> $log_file
				return 1
			fi
		else
			echo "Blade $BLADE_NAME does not exist for $SERVER_NAME"
			echo "$SESSUSER|$(date)|Blade $BLADE_NAME does not exist for $SERVER_NAME" >> $log_file
			return 1
		fi
	else
		echo "Server $SERVER_NAME does not exist"
		echo "$SESSUSER|$(date)|Server $SERVER_NAME does not exist" >> $log_file
		return 1
	fi
}

TEMP=$1
SESSUSER=$2
while read var; do
	SERVER_NAME=$(echo $var | cut -d "+" -f 1)
	BLADE_NAME=$(echo $var | cut -d "+" -f 2)
	TEAM=$(echo $var | cut -d "+" -f 3)
	CHANNEL=$(echo $var | cut -d "+" -f 4)
	ALLOCATION=$(echo $var | cut -d "+" -f 5)
	if ! main_func ; then
	 exit 1
	fi
done < /tmp/$TEMP
rm /tmp/$TEMP
