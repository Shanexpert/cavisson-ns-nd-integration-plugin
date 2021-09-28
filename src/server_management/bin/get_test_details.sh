if [ $# -ne 1 ]
then
        echo "ERROR:Invalid number of arguments. "
        exit 1
else
        SERVER_IP=$1
fi
check_cmon ()
{       
        timeout 5 nsu_server_admin -s "$SERVER_IP" -g -c 'date' > time${MY_PID}.temp
        if [ $? -ne 0 ]
        then
                echo "ERROR:Cmon is not running on the server"
                exit 1
        fi
}

check_cmon
nsu_server_admin -g -s $SERVER_IP -c "nsu_show_all_netstorm" > netstorm.temp 2>/dev/null
if [ $? -eq 0 ]
then
        COUNT=1
        sed -i '1d' netstorm.temp
        sed -i '/^$/d' netstorm.temp
        while read line 
        do
                BLADE=`echo $line |cut -d "|" -f 1`
                TOMCAT=`echo $line |cut -d "|" -f 22`
                TEST=`echo $line | cut -d "|" -f 4`
                if [ "XX$TEST" != "XX-" ]
                then
                        ENABLE=1
                fi
		echo -n -e "$BLADE|$TOMCAT\n"
                COUNT=$(( $COUNT+1 ))
        done < netstorm.temp
	if [ "xx$ENABLE" == "xx1" ]
then
        echo -n -e "\nTest running status\n\n"
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
                echo -n -e "${BOLD}Blade Name${RESET} : $BLADE\n"
                echo -n -e "${BOLD}Scenario Path${RESET} : $SCENARIO\n"
                echo -n -e "${BOLD}Test Run Number${RESET} : $TR\n"
                echo -n -e "${BOLD}Start Time${RESET} : $STARTTIME\n"
                echo -n -e "${BOLD}Duration${RESET} : $RUNTIME\n"
                echo -n -e "${BOLD}Number Of Virtual Users${RESET} : $VUSERS\n"
                echo -n -e "${BOLD}Owner${RESET} : $OWNER\n"
                timeout 5 nsu_server_admin -s $SERVER_IP -g -c "cat /home/netstorm/$BLADE/logs/TR$TR/scenario" >scenario${MY_PID}.temp  2>&1
                if [ $? -ne 0 ]
                then
                        echo -n -e "${RED}Unable to fetch Scenario${RESET}"
                else
                        if [ -e scenario${MY_PID}.temp ]
                        then
                                TTYPE=`grep SGRP scenario${MY_PID}.temp | grep -v "#" | sed -e's/  */ /g'|head -1| cut -d " " -f 3`
                                if [ "xx$TTYPE" == "xxNA" ]
                                then
                                        echo -n -e "${BOLD}Test Type${RESET} : NS\n\n\n"
                                else
                                        echo -n -e "${BOLD}Test Type${RESET} : NC\n"
                                        if [[ $TTYPE =~ [0-9] ]]
                                        then
                                                echo -n -e "${BOLD}Running test as${RESET}: Generator\n"
                                                echo -n -e "${BOLD}Generator ID${RESET}: $TTYPE\n"
                                                timeout 5 nsu_server_admin -s $SERVER_IP -g -c "cat /home/netstorm/$BLADE/logs/TR$TR/global.dat" > global${MY_PID}.temp
                                                TESTCONTROLLERIP=`grep "CONTROLLER_IP" global${MY_PID}.temp | cut -d " " -f 2`
                                                TESTBLADE=`grep "CONTROLLER_ENV" global${MY_PID}.temp| cut -d "/" -f 4`
                                                CONTROLLERTR=`grep "CONTROLLER_TESTRUN_NUMBER" global${MY_PID}.temp | cut -d " " -f 2`
                                                echo -n -e "${BOLD}Controller IP${RESET}: $TESTCONTROLLERIP\n"
                                                echo -n -e "${BOLD}Controller Blade${RESET}: $TESTBLADE\n"
                                                echo -n -e "${BOLD}Controller Testrun Number${RESET}: $CONTROLLERTR\n\n\n"
                                        else
                                                timeout 5 nsu_server_admin -s $SERVER_IP -g -c "cat /home/netstorm/$BLADE/logs/TR$TR/NetCloud/NetCloud.data" > netclouddata${MY_PID}.temp 2>&1
                                                echo -n -e "Running Test as Controller\n"

                                                if [ -e netclouddata${MY_PID}.temp ]
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
        done < netstorm${MY_PID}.temp
else
        echo -n -e "\nTest is not running on any controller.\n"
fi
unset ENABLE
}
    
fi
