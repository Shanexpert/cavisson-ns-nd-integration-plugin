#!/bin/bash

FILE=$1

echo "`date` ----------------------------------------------" >> $FILE
echo "mpstat command output" > $FILE
echo "----------------------------------------------" >> $FILE
mpstat -P ALL 5 5 >> $FILE

echo "`date` ----------------------------------------------" >> $FILE
echo "ps -lef command output" >> $FILE
echo "----------------------------------------------" >> $FILE
ps -lef >> $FILE

echo  >> $FILE
# echo "----------------------------------------------" >> $FILE
# echo "cat /var/log/messages last 200 lines output" >> $FILE
# echo "----------------------------------------------" >> $FILE
#can not use sudo because it requires password
#sudo tail -200 /var/log/messages >> $FILE

echo "`date` ----------------------------------------------" >> $FILE
echo "dmesg output" >> $FILE
echo "----------------------------------------------" >> $FILE
dmesg >> $FILE

echo "`date` ----------------------------------------------" >> $FILE
echo "netstat -s output" >> $FILE
echo "----------------------------------------------" >> $FILE
netstat -s >> $FILE 2>&1

echo "`date` ----------------------------------------------" >> $FILE
echo "netstat -na output" >> $FILE
echo "----------------------------------------------" >> $FILE
netstat -na >> $FILE 2>&1

echo "`date` ----------------------------------------------" >> $FILE
echo "free -ml output" >> $FILE
echo "----------------------------------------------" >> $FILE
free -ml  >> $FILE

echo "`date` ----------------------------------------------" >> $FILE
echo "File: /proc/net/sockstat" >> $FILE
echo "----------------------------------------------" >> $FILE
cat /proc/net/sockstat  >> $FILE

echo "`date` ----------------------------------------------" >> $FILE
echo "vmstat output" >> $FILE
echo "----------------------------------------------" >> $FILE
vmstat 1 5  >> $FILE

echo "`date` ----------------------------------------------" >> $FILE
echo "Number of open files" >> $FILE
echo "----------------------------------------------" >> $FILE
# Need to remove /usr/sbin, since in ubuntu machine lsof is not installed in this directory
cat /proc/sys/fs/file-nr  >> $FILE 2>&1

echo "`date` ----------------------------------------------" >> $FILE
echo "Sleeping for 5 seconds ..." >>$FILE
sleep 5

echo "`date` ----------------------------------------------" >> $FILE
echo "ps -lef command output" >> $FILE
echo "----------------------------------------------" >> $FILE
ps -lef >> $FILE

