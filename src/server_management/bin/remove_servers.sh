logs_dir=$NS_WDIR/server_management/logs
usage()
{
	echo "Error: This requires argument. $0 <Server Name>"
}

if [ $# -ne 2 ]
then
	echo "1"
	usage
	exit 1
fi
SERVER=$1
SESSUSER=$2
CHECK_SERVER_NAME=`psql  -X  -A -U postgres -d demo -t -c "select server_name from servers where LOWER(server_name)=LOWER('$SERVER')"`
if [ "xx$CHECK_SERVER_NAME" != "xx"  ]
then
	psql  -X  -A -U postgres -d demo -t -c "select blade_name, team, channel, allocation from allocation where server_name='$SERVER'" > file.temp 2>/dev/null
	while read line
	do
		BLADE=`echo $line | cut -d "|" -f 1`
		TEAM=`echo $line | cut -d "|" -f 2`
		CHANNEL=`echo $line | cut -d "|" -f 3`
		ALLOCATION=`echo $line | cut -d "|" -f 4`
		if [ "xx$TEAM" != "xxNA" ]
		then
			echo "Blade $BLADE is allocated to $TEAM. Change it to NA to delete this server" >> file1.temp
			echo "$SESSUSER|$(date)|Blade $BLADE is allocated to $TEAM. Change it to NA to delete this server" >> $logs_dir/addDelete.log
			VAR="n"
		fi
		if [ "xx$ALLOCATION" != "xxFree" ]
		then
			echo "Blade $BLADE is assigned as $ALLOCATION. Change it to Free to delete this server" >> file1.temp
			echo "$SESSUSER|$(date)|Blade $BLADE is assigned as to $ALLOCATION. Change it to Free to delete this server" >> $logs_dir/addDelete.log
			VAR="n"
		fi
	done < file.temp
	if [ "xx$VAR" != "xxn" ]
	then
		psql  -X  -A -U postgres -d demo -t <<+ 2>&1 > /dev/null
		delete from allocation where server_name = '$SERVER';
		delete from servers where server_name = '$SERVER';
		delete from billing where status='f' and server_name='$SERVER';
		delete from billing_cc where status='f' and server_name='$SERVER';
+
		if [ $? -eq 0 ]
		then
			echo "0"
			echo "Server $SERVER deleted"
			echo "$SESSUSER|$(date)|Server $SERVER deleted" >> $logs_dir/addDelete.log
		else
			echo "1"
			echo "Database Error"
		fi
	else
		echo "1"
		cat file1.temp
	fi
else
	echo "1"
	echo "Server Name $SERVER Does not exist"
fi

rm *.temp 2>/dev/null
