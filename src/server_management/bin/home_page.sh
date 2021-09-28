#!/bin/bash
tmpdir=$NS_WDIR/server_management/tmp
cards()
{
	psql -X  -A -U postgres -d demo -t << EOF
	select count(*) from servers;
	select count(*) from allocation where machine_type='Generator';
	select count(*) from allocation;
	select count(*) from allocation where machine_type='Controller';
	select count(*) from allocation where allocation='Dedicated';
	select count(*) from allocation where allocation='Additional';
	select count(*) from allocation where allocation='Free';
	select count(*) from allocation where machine_type='Generator';
	select count(*) from allocation where machine_type='Netstorm';
	select count(*) from allocation where machine_type='Controller';
	select count(*) from allocation where machine_type='NO';
	select count(status) from (select distinct server_name, status from allocation where server_type!='VM') as abc where status='t' group by status order by status;
EOF
	OFF=`psql -X  -A -U postgres -d demo -t -c "select COUNT(*) from (select distinct server_name, status from allocation where server_type!='VM') as abc where status='f' group by status order by status"`
	if [ -z $OFF ]
	then
		OFF=0
	fi
	echo $OFF
	psql -X  -A -U postgres -d demo -t -c "select team, machine_type, count(machine_type) as count  from allocation group by team, machine_type order by team, machine_type" | sed ':a;N;$!ba;s/\n/+/g'
	if [[ -e $tmpdir/.security_scan.pid ]]; then
		pid=$(cat $tmpdir/.security_scan.pid)
		if  kill -0 $pid 2>/dev/null && [[ -r /proc/$pid/cmdline ]] && xargs -0l echo < /proc/$pid/cmdline | grep -q "security_scan.sh"
		then
			echo "security scan|running|$pid"
		else
			echo "security scan|notrunning"
		fi
	else
		echo "security scan|notrunning"
	fi
	if [[ -e $tmpdir/.mailer.pid ]]; then
		pid=$(cat $tmpdir/.mailer.pid)
		if  kill -0 $pid 2>/dev/null && [[ -r /proc/$pid/cmdline ]] && xargs -0l echo < /proc/$pid/cmdline | grep -q "mailer.py"
		then
			echo "mailer|running|$pid"
		else
			echo "mailer|notrunning"
		fi
	else
		echo "mailer|notrunning"
	fi
	if [[ -e $tmpdir/.check_health.pid ]]; then
		pid=$(cat $tmpdir/.check_health.pid)
		if  kill -0 $pid 2>/dev/null && [[ -r /proc/$pid/cmdline ]] && xargs -0l echo < /proc/$pid/cmdline | grep -q "trigger_check_health.sh"
		then
			echo "healthcheck|running|$pid"
		else
			echo "healthcheck|notrunning"
		fi
	else
		echo "healthcheck|notrunning"
	fi
	if [[ -e $tmpdir/.ping_cc.pid ]]; then
		pid=$(cat $tmpdir/.ping_cc.pid)
		if  kill -0 $pid 2>/dev/null && [[ -r /proc/$pid/cmdline ]] && xargs -0l echo < /proc/$pid/cmdline | grep -q "ping_CC.sh"
		then
			echo "statuscheck|running|$pid"
		else
			echo "statuscheck|notrunning"
		fi
	else
		echo "statuscheck|notrunning"
	fi
}
controllers()
{
psql -X  -A -U postgres -d demo -t -c "select row_number() over(),  server_name, server_ip, blade_name, team, channel from allocation where machine_type='Controller' order by team"
}

team_with_blades()
{
psql -X  -A -U postgres -d demo -t << EOF
    SELECT server_name, blade_name, allocation, machine_type,
    CASE WHEN servers.security_group is null
      OR servers.security_group=''
    THEN 'generator'
    ELSE servers.security_group
    END
    FROM allocation JOIN servers USING(server_name)
    ORDER BY server_name;
EOF
}

graphs()
{
	echo "Not an option right now"
}

while getopts abcd arg
do
  case $arg in
    a) cards ;;
    b) graphs ;;
    c) controllers ;;
		d) team_with_blades ;;
  	*) echo "Invalid option";;
  esac
done
