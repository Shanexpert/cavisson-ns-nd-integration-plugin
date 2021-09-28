#!/bin/bash
if [ $# -ne 1  ]
then
	echo "Invalid number of arguments"
	exit
fi

	nsu_server_admin -s $1 -i -g -c 'sh nsi_get_controller_name' > name_temp 2>/dev/null
	if [ $? -ne 0  ]
	then
		echo "Cmon not running"
		exit
	fi

	while read name_temp
	do
	
       portNetstorm=` nsu_server_admin -s $1 -i -c "cat /home/netstorm/$name_temp/webapps/sys/config.ini"| grep "portNetstorm" |cut -d "=" -f 2 | sed -r 's/\s+//g'` 

			_nsu_generate_license -I $1:$portNetstorm -P 1 -t 1 -s 1
			nsu_server_admin -s $1 -i -c "mkdir /home/netstorm/$name_temp/.license" 2>&1 >/dev/null
			nsu_server_admin -s $1 -i -c "chown netstorm:netstorm /home/netstorm/$name_temp/.license" 2>&1 >/dev/null
			nsu_server_admin -s $1 -i -F license.nl1 -D /home/netstorm/$name_temp/.license 2>&1 >/dev/null
			nsu_server_admin -s $1 -i -c "chown netstorm:netstorm /home/netstorm/$name_temp/.license/license.nl1" 2>&1 >/dev/null
		                       echo "License uploaded for $name_temp"
		done < name_temp

rm name_temp
	
