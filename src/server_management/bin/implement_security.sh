#!/bin/bash

nsu_server_admin -s $1 -i -c date 2>&1 > /dev/null
if [ $? -eq 0  ]
then
	nsu_server_admin -s $1 -i -F /home/netstorm/work/bin/sshd_config -D /etc/ssh/ 2>&1 > /dev/null
        nsu_server_admin -s $1 -i -c 'mkdir /etc/ssh/authorized_keys' 2>&1 > /dev/null
        nsu_server_admin -s $1 -i -c 'chmod 755 /etc/ssh/authorized_keys' 2>&1 > /dev/null
        nsu_server_admin -s $1 -i -F /home/netstorm/work/bin/iptables -D /etc/ssh/authorized_keys 2>&1 > /dev/null
        nsu_server_admin -s $1 -i -F /home/netstorm/work/bin/netstorm -D /etc/ssh/authorized_keys 2>&1 > /dev/null
        nsu_server_admin -s $1 -i -c 'service ssh restart' 2>&1 > /dev/null
        nsu_server_admin -s $1 -i -c 'sh /etc/ssh/authorized_keys/iptables' 2>&1 > /dev/null
	echo "Done";
else 
	echo "ERROR:Server not accessible or cmon not running"
fi
