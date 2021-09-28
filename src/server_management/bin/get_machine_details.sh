
check_cmon()
{
TIME=`timeout 5 nsu_server_admin -s "$SERVER_IP" -g -c 'date'`
if [ $? -ne 0 ]
then
        echo -n -e "ERROR:Cmon is not running on this server. Or it is taking more that 5 secs to connect to 7891 port.\n" 
	exit
else
        echo -n -e "$TIME\n"
fi
}

parse_generators()
{
echo -n -e "\n\e[4m${BLUE}Generator Name                        TR on Generator         Generator IP    Blade           IDd\n"
while read line 
do
GTR=`echo $line | cut -d "|" -f 1 | cut -d " " -f 2`
GNAME=`echo $line | cut -d "|" -f 2`
GIP=`echo $line | cut -d "|" -f 3`
GBLADE=`echo $line | cut -d "|" -f 5 | cut -d "/" -f 4`
ID=`echo $line | cut -d "|" -f 7`

echo -n -e "$GNAME              $GTR    $GIP            $GBLADE         $ID\n"
done < netclouddata.temp
echo -n -e "\n\n"
}


netstorm()
{
#search_server_package
#check_cmon
nsu_server_admin -g -s $SERVER_IP -c "nsu_show_all_netstorm" > netstorm.temp
sed -i '1d' netstorm.temp
sed -i '/^$/d' netstorm.temp
cat netstorm.temp  |sed ':a;N;$!ba;s/\n/:/g'
#echo -n -e "\e[4m${BLUE}Blade Name              Tomcat Portd\n"
COUNT=1
while read line 
do
        BLADE=`echo $line |cut -d "|" -f 1`
        TOMCAT=`echo $line |cut -d "|" -f 22`
        TEST=`echo $line | cut -d "|" -f 4`
        if [ "XX$TEST" != "XX-" ]
        then
                ENABLE=1
        fi
#        echo -n -e "[$COUNT]$BLADE              $TOMCAT\n"
        COUNT=$(( $COUNT+1 ))
done < netstorm.temp
if [ "xx$ENABLE" == "xx1" ]
then
#        echo -n -e "\nTest running status\n\n"
        while read line 
        do
        BLADE=`echo $line | cut -d "|" -f 1`
        TR=`echo $line | cut -d "|" -f 4`
        SCENARIO=`echo $line | cut -d "|" -f 2,3,6 | sed 's/|/\//g'`
        RUNTIME=`echo $line | cut -d "|" -f 8`
        STARTTIME=`echo $line | cut -d "|" -f 7`
        VUSERS=`echo $line | cut -d "|" -f 9`
        OWNER=`echo $line | cut -d "|" -f 21`
        #TOMCAT=`echo $line | cut -d "|" -f 22`
        if [ "xx$TR" != "xx-" ]
        then
#               echo -n -e "$BLADE | $SCENARIO | $TR | $RUNTIME | $STARTTIME | $VUSERS | $OWNER \n"
#                echo -n -e "Blade Name : $BLADE\n"
#                echo -n -e "Scenario Path : $SCENARIO\n"
#                echo -n -e "Test Run Number : $TR\n"
#                echo -n -e "Start Time : $STARTTIME\n"
#                echo -n -e "Duration : $RUNTIME\n"
#                echo -n -e "Number Of Virtual Users : $VUSERS\n"
#                echo -n -e "Owner : $OWNER\n"
                timeout 5 nsu_server_admin -s $SERVER_IP -g -c "cat /home/netstorm/$BLADE/logs/TR$TR/scenario" >scenario.temp  2>&1
                if [ $? -ne 0 ]
                then
                        echo -n -e "ERROR:Unable to fetch Scenario"
			break;
                else
                        if [ -e scenario.temp ]
                        then
                                TTYPE=`grep SGRP scenario.temp | grep -v "#" | sed -e's/  */ /g'|head -1| cut -d " " -f 3`
                                if [ "xx$TTYPE" == "xxNA" ]
                                then
                                        echo -n -e "NS\n"
                                else
                                        echo -n -e "NC\n"
                                        if [[ $TTYPE =~ [0-9] ]]
                                        then
                                                echo -n -e "NC:Generator:$TTYPE\n"
                                                timeout 5 nsu_server_admin -s $SERVER_IP -g -c "cat /home/netstorm/$BLADE/logs/TR$TR/global.dat" > global.temp
                                                TESTCONTROLLERIP=`grep "CONTROLLER_IP" global.temp | cut -d " " -f 2`
                                                TESTBLADE=`grep "CONTROLLER_ENV" global.temp| cut -d "/" -f 4`
                                                CONTROLLERTR=`grep "CONTROLLER_TESTRUN_NUMBER" global.temp | cut -d " " -f 2`
                                                echo -n -e "Controller:$TESTCONTROLLERIP:$TESTBLADE:$CONTROLLERTR\n"
                                        else
                                                timeout 5 nsu_server_admin -s $SERVER_IP -g -c "cat /home/netstorm/$BLADE/logs/TR$TR/NetCloud/NetCloud.data" > netclouddata.temp 2>&1
                                                echo -n -e "NC:Controller\n"

                                                if [ -e netclouddata.temp ]
                                                then
                                                        parse_generators
                                                else
                                                        echo -n -e "Not able to donwload Generators data"
                                                fi
                                        fi
                               fi
                        else
                                echo -n -e "Scenario file not available"
                        fi

                fi

        fi
        done < netstorm.temp
else
        echo -n -e "\nTest is not running on any controller.\n"
fi
unset ENABLE
}


tomcat()
{
#search_server_package
#check_cmon
nsu_server_admin -g -s $SERVER_IP -c "ps -ef" > ps.temp
nsu_server_admin -g -s $SERVER_IP -c "nsi_get_controller_name" > controllersd.temp
test=`cat ps.temp | grep -i "xms"`
if [ "xx$test" == "xx" ]
then
        echo -n -e "BLADE NAME          PROCESS ID      USER    JAVA PATH\n"
else
        echo -n -e "BLADE NAME          PROCESS ID      USER    JAVA PATH       XMS     XMX     NEWSIZE PERMSIZE\n"
fi
COUNT=1
while read line 
do
        BLADE=`echo $line`
        PID=`cat psd.temp | grep tomcat | grep -w "$BLADE"| sed -e's/  */ /g'| cut -d " " -f 2|sed '$!{:a;N;s/\n/,/;ta}'`
        USERNAME=`cat psd.temp | grep tomcat |grep -w "$BLADE"| grep "$BLADE"| sed -e's/  */ /g'| cut -d " " -f 1|sed '$!{:a;N;s/\n/,/;ta}'`
        JAVA=`cat psd.temp | grep tomcat |grep -w "$BLADE"| grep "$BLADE"| sed -e's/  */ /g'| cut -d " " -f 8|sed '$!{:a;N;s/\n/,/;ta}'`
        XMS=`cat psd.temp |grep tomcat |grep -w "$BLADE"| grep -o -i "xms[0-9]*[a-z]"|sed '$!{:a;N;s/\n/,/;ta}'`
        XMX=`cat psd.temp |grep tomcat |grep -w "$BLADE"| grep -o -i "xmx[0-9]*[a-z]"|sed '$!{:a;N;s/\n/,/;ta}'`
        NEWSIZE=`cat psd.temp|grep tomcat |grep -w "$BLADE"|grep -o -i "newsize[0-9]*[a-z]"|sed '$!{:a;N;s/\n/,/;ta}'`
        PERMSIZE=`cat psd.temp|grep tomcat |grep -w "$BLADE"|grep -o -i "permsize[0-9]*[a-z]"|sed '$!{:a;N;s/\n/,/;ta}'`
        if [ "xx$PID" != "xx" ]
        then
                echo -n -e "$BLADE              $PID    $USERNAME       $JAVA   $XMS    $XMX    $NEWSIZE        $PERMSIZE\n"
        else
                echo -n -e "$BLADE              dTomcat is not running for this controllerd\n"
        fi

done < controllersd.temp
termination_package
}

dnsmasq()
{
#search_server_package
#check_cmon
timeout 5 nsu_server_admin -g -s $SERVER_IP -c "ps -ef" > psd.temp
timeout 5 nsu_server_admin -g -s $SERVER_IP -c "cat /var/run/dnsmasq/resolv.conf" > isp_nameserverd.temp
timeout 5 nsu_server_admin -g -s $SERVER_IP -c "cat /etc/resolv.conf" > local_nameserverd.temp
USERNAME=`cat psd.temp | grep dnsmasq|grep -v "color"| sed -e's/  */ /g' | cut -d " " -f 1`
PID=`cat psd.temp | grep dnsmasq|grep -v "color"| sed -e's/  */ /g' | cut -d " " -f 2`
ISP_NAMESERVER=`cat isp_nameserverd.temp | grep -i nameserver`
LOCAL_NAMESERVER=`cat local_nameserverd.temp | grep -i nameserver`
if [ "xx$PID"  == "xx" ]
then
        echo -n -e "dDnsmasq is not running on this serverd\n"
else
        echo -n -e "\n\e[4md${BLUE}DNSMASQd\n"
        echo -n -e "User: $USERNAME\n"
        echo -n -e "Process ID: $PID\n\n"
fi
if [ "XX$ISP_NAMESERVER" == "XX" ]
then
        echo -n -e "dISP Nameservers not existingd\n\n"
else
        echo -n -e "\e[4mISP Nameserverd\n"
        echo -n -e "$ISP_NAMESERVER\n\n"
fi
if [ "xx$LOCAL_NAMESERVER" == "xx" ]
then
        echo -n -e "dLocal Nameservers not existingd\n\n"
else
        echo -n -e "\e[4mLocal Nameserverd\n"
        echo -n -e "$LOCAL_NAMESERVER\n"
fi
termination_package
}

socket()
{
#search_server_package
#check_cmon
timeout 5 nsu_server_admin -g -s $SERVER_IP -c "cat /proc/net/sockstat" > socketsd.temp
echo -n -e "SOCKET STATS\n"
cat socketsd.temp
termination_package
}

machine()
{
#search_server_package
#check_cmon
timeout 5 nsu_server_admin -g -s $SERVER_IP -c "df -h" > dfd.temp
timeout 5 nsu_server_admin -g -s $SERVER_IP -c "free -lg" > freed.temp
timeout 5 nsu_server_admin -g -s $SERVER_IP -c "uptime" > loadd.temp
echo -n -e "dDISK SPACEd\n\n"
cat dfd.temp
echo -n -e "\n"
echo -n -e "dMAIN MEMORYd\n\n"
cat freed.temp
echo -n -e "\n"
echo -n -e "dCORES:`timeout 5 nsu_server_admin -g -s $SERVER_IP -c "nproc"`\n\n"
echo -n -e "\n"
echo -n -e "dUPTIME and LOADd\n\n"
cat loadd.temp
echo -n -e "\n"
termination_package
}

postgres_process()
{
nsu_server_admin -g -s $SERVER_IP -c "ps -ef" > psd.temp
cat psd.temp| grep postgres | sed -e's/  */ /g' | grep -v "color" > postgres.temp
while read line
do
#       PID=`echo $line| sed -e's/  */ /g'| cut -d " " -f 2|sed '$!{:a;N;s/\n/,/;ta}'`
        #USERNAME=`cat psd.temp | grep postgres| sed -e's/  */ /g'| cut -d " " -f 2|sed '$!{:a;N;s/\n/,/;ta}'`
        PROCESS=`echo $line| sed -e's/  */ /g'| cut -d " " -f 2|sed '$!{:a;N;s/\n/,/;ta}'`
        COMMAND=`echo $line| sed -e's/  */ /g'| cut -d " " -f 9|sed '$!{:a;N;s/\n/,/;ta}'`
        echo -n -e "$PROCESS    $COMMAND\n"
done < postgresd.temp
termination_package
}
custom_command()
{
echo -n -e "Enter your command"
read COMMAND
menu_package $COMMAND
echo 
}


while getopts i:cn arg 
do
        case $arg in
                i) SERVER_IP=$OPTARG ;;
                c) check_cmon ;;
                n) netstorm ;;
                *)  echo -n -e "Use -s <start index number> -e <end index number> -n <index numbers> -v <except index numbers> -a \n"
                   echo -n -e "Use \",\" to seperate multiple indices in -v and -n options.\n" ;;

        esac
done
rm *.temp 2>/dev/null
