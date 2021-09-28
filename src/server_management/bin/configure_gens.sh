
ip=$1
blade=$2
mode=$3
scenario=/tmp/QAValidation.conf.$$
gen_name=/tmp/gen_name.dat.$$
gen_blade=/tmp/generators.dat.$$
gen_blade_bkp=/tmp/generators.dat.$$
generators=/tmp/generators.$$
portal_logs=$NS_WDIR/server_management/logs/portal.log
show_data=/tmp/show_data.$$
cleanup()
{
	rm -f $scenario $gen_name $gen_blade $generators $show_data
}

if [ $# -ne 2 ]
then
	echo "Number of arguments must be 3"
	echo "Usage: $0 <Controller IP> <Controller Blade> Mode<1/2>"
	exit 1
fi

results(){
	a=`psql -X -A -U postgres -d demo -t << EOF
		select server_name, build_version, bkp_ctrl, bkp_blade from allocation where server_ip='$ip' and blade_name='$blade';
EOF`
  echo "Controller Name:$(echo $a|cut -d'|' -f1)"
  echo "Controller IP:$ip"
  echo "Controller Blade:$blade"
  echo "Controller Build:$(echo $a|cut -d'|' -f2)"
	echo "Backup Controller:$(echo $a|cut -d'|' -f3)"
	echo "Backup Controller Blade:$(echo $a|cut -d'|' -f4)"
  echo "Generator File : /home/cavisson/$blade/etc/.netcloud/generators.dat"
	cat $show_data | sed ':a;N;$!ba;s/\n/!@/g'
}

validate_generators()
{
	check()
	{
		local ip=$1
		local name=$2
		local blade=$3
		if timeout 7s nsu_server_admin -s $ip -g -F $NS_WDIR/server_management/bin/gen_data.sh -D /tmp >/dev/null 2>&1; then
		 timeout 7s nsu_server_admin -s $ip -g -c "sh /tmp/gen_data.sh $blade" | sed "1s/^/${name}|${ip}|/" | grep -v error | sed '/^$/d' 2>/dev/null
		 if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
		  echo "$name|$ip|Unable to execute validation script|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-"
		 fi
		else
		 echo "$name|$ip|CMON Not Reachable|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-"
		fi
	}
	while read line
	do
		local server_name=$(echo $line|cut -d "|" -f 1)
		local server_ip=$(echo $line|cut -d "|" -f 2)
		local gen_blade=$(echo $line|cut -d "|" -f 3)
		check $server_ip $server_name $gen_blade >> $show_data &
	done < $generators
	wait
}

config_bkp()
{
	p=$(psql -X -A -U postgres -d demo -t -c "select bkp_ctrl, bkp_blade from allocation where server_ip='$ip' and blade_name='$blade'")
	local name=$(echo $p|cut -d'|' -f1)
	if [ "x$name" == "xnone" ]; then
		return 0
	fi
	local blade=$(echo $p|cut -d'|' -f2)
	local ip=$(get_server_ip_from_name.sh $name)
	making_gen_file $ip $blade $gen_blade_bkp
}

validation_scenario()
{
	echo "SCHEDULE ALL Start START IMMEDIATELY
SCHEDULE ALL RampUp RAMP_UP ALL TIME 00:00:00 LINEARLY
SCHEDULE ALL Stabilize STABILIZATION TIME 0
SCHEDULE ALL Duration DURATION TIME 00:01:00
SCHEDULE ALL RampDown RAMP_DOWN ALL TIME 00:00:00 LINEARLY
G_PAGE_THINK_TIME ALL ALL 2 1000
PROF_PCT_MODE NUM
G_OVERRIDE_RECORDED_THINK_TIME ALL ALL 1
CHECK_GENERATOR_HEALTH 1 10 50 2 50
SGRP g1 ALL NA Internet 0 default/default/GoogleKoPenny 100
TNAME QAValidationTest" > $scenario
for i in `cat $gen_name` ; do
	echo "NS_GENERATOR $i" >> $scenario
done
timeout 5s nsu_server_admin -s $ip -g -F $scenario -D /home/cavisson/$blade/scenarios/default/default/
if [[ $? -ne 0 ]]; then
	echo "$(whoami)|$(date)|ERROR: CMON is not running on $ip or unable to send scenario for unknown reason"
	exit 2
fi
if [[ -f $NS_WDIR/server_management/GoogleKoPenny.tar.gz ]]; then
	nsu_server_admin -s $ip -g -F $NS_WDIR/server_management/GoogleKoPenny.tar.gz -D /home/cavisson/$blade/scripts/default/default/
fi
nsu_server_admin -s $ip -g -c "mv /home/cavisson/$blade/scenarios/default/default/QAValidation.conf.$$ /home/cavisson/$blade/scenarios/default/default/QAValidation.conf"
}

making_gen_file()
{
ctrl_ip=$1
ctrl_blade=$2
gen_blade=$3
touch $gen_blade
echo "#GeneratorName|IP|CavMonAgentPort|Location|Work|Type|ControllerIp|ControllerName|ControllerWork|Team|NameServer|DataCenter|Future1|Future2|Future3|Future4|Future5|Future6|Future7|Comments" > $gen_blade
while read VAR
do
	ALLOC=$(echo $VAR | cut -d "|" -f 4)
	if [[ "xx$ALLOC" != "xx$PREV_ALLOC"  ]]; then
		echo "############################## Generator Configuration for $ctrl_blade as $ALLOC on $(date +'%d-%h-%y %H:%M:%S') by $(whoami) ##########################################" >> $gen_blade
	fi
	PREV_ALLOC=$ALLOC
  GEN_NAME=$(echo $VAR | cut -d "|" -f 1)
  GEN_IP=$(echo $VAR | cut -d "|" -f 2)
  GEN_BLADE=$(echo $VAR | cut -d "|" -f 3)
  LOCATION=$(echo $VAR | cut -d "|" -f 5)
	TEAM=$(echo $VAR | cut -d "|" -f 6)
	CHANNEL=$(echo $VAR | cut -d "|" -f 7)
  echo "$GEN_NAME|$GEN_IP|7891|$LOCATION|/home/cavisson/$GEN_BLADE|Internal|$ctrl_ip|$ctrl_blade|/home/cavisson/$ctrl_blade|${TEAM}${CHANNEL}|NA|NA|NA|NA|NA|NA|NA|NA|NA|NA" >> $gen_blade
done < $generators

cat $gen_blade | grep -v "^#" | cut -d "|" -f 1 > $gen_name

nsu_server_admin -s $ctrl_ip -g -F $gen_blade -D /home/cavisson/$ctrl_blade/etc/.netcloud >/dev/null 2>&1
if [ $? -ne 0 ]
then
  echo "$(whoami)|$(date)|ERROR: Unable to upload generator conf file to controller $ctrl_ip  Server may not be accessible or Cmon not running on the server"
  exit 1
else
	nsu_server_admin -s $ctrl_ip -g -c "mv /home/cavisson/$ctrl_blade/etc/.netcloud/generators.dat.$$ /home/cavisson/$ctrl_blade/etc/.netcloud/generators.dat"
	nsu_server_admin -s $ctrl_ip -g -c "cp /home/cavisson/$ctrl_blade/etc/.netcloud/generators.dat /home/cavisson/etc/.netcloud/Gen_$ctrl_blade.dat" >/dev/null 2>&1
fi
}

trap "cleanup ; exit 2" 1 2 3 6

psql -X -A -U postgres -d demo -t << EOF | sort -t "|" --key=4 -r > $generators
  SELECT allocation.server_name, allocation.server_ip, blade_name, allocation, servers.location, team, channel
  FROM allocation JOIN servers using(server_name)
  WHERE team=(
    SELECT team
    FROM allocation
    WHERE server_ip='$ip' AND blade_name='$blade'
    )
    AND (channel=(
    SELECT distinct channel
    FROM allocation
    WHERE server_ip='$ip' AND blade_name='$blade'
    )
    OR channel in (SELECT unnest(shared) FROM allocation WHERE server_ip='$ip' AND blade_name='$blade'))
    AND machine_type='Generator' and status='t';
EOF

#psql -X -A -U postgres -d demo -t << EOF | sort -t "|" --key=4 -r > $generators
#	SELECT allocation.server_name, allocation.server_ip, blade_name, allocation, servers.location
#	FROM allocation JOIN servers using(server_name)
#	WHERE team=(
#		SELECT team
#		FROM allocation
#		WHERE server_ip='$ip' AND blade_name='$blade'
#		)
#		AND channel=(
#		SELECT distinct channel
#		FROM allocation
#		WHERE server_ip='$ip' AND blade_name='$blade'
#		)
#		AND machine_type='Generator' and status='t';
#EOF
if [[ $? -ne 0 ]]; then
	echo "$(whoami)|$(date)|ERROR: Unable to get generators for IP:$IP and blade:$blade" | tee -a $portal_logs 1>&2
	exit 1
fi
validate_generators & 
making_gen_file $ip $blade $gen_blade
validation_scenario &
config_bkp
if [ $? -eq 0 ]
then
	results
fi
wait
cleanup
exit 0
