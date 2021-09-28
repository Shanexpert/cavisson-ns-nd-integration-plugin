#!/bin/sh

##########################********************************##############################*****************************##########################
# Name   : nsu_upload_alertHistory_data.sh
# Author : 
# Usage  : Create a migration tool to upload the alerthistory csv data for one partiton or no of partitions given
#  
##########################********************************##############################*****************************##########################
usage()
{
  tput bold
  echo ""
  echo "$*"
  echo "nsu_upload_alertHistory_data.sh -t <testrun> -s <start partition> -e <end partition>"
  echo "     -t <TR Number> Mandatory Argument"    
  echo "     -e <End partition> Mandatory Argument"    
  echo "     -s <Start partition> Mandatory Argument"    
  tput sgr0 # Normal text
  echo "   * This will load the data from start partition to end partition for alertHistory csv only"
  echo "   * For only one partition, user should give same partition in -s and -e "
  exit 1
}

check_mandatory_args()
{
  if [ "X$NS_WDIR" = "X" ]; then
    usage "Error: NS_WDIR is not set."
  fi

  if [ "X$TESTRUN" == "X" -o "X$START_PARTITION" == "X" -o "X$END_PARTITION" == "X" ]
  then
    usage "Error: Mandatory arg -t or -s or -e are missing."
  fi
}

get_partition_list()
{ 
  PARTITION="$START_PARTITION"
  while true
  do
    grep "^CurrentPartition" $NS_WDIR/logs/TR$TESTRUN/$PARTITION/.partition_info.txt|cut -d'=' -f2>>$NS_WDIR/logs/TR$TESTRUN/partitionList.txt
    PARTITION=`grep "^NextPartition" $NS_WDIR/logs/TR$TESTRUN/$PARTITION/.partition_info.txt|cut -d '=' -f 2`
    if [ "$PARTITION" == "$END_PARTITION" ];
    then
      echo "$PARTITION">>$NS_WDIR/logs/TR$TESTRUN/partitionList.txt
      break
    fi
  done
}

calculating_diff_between_cav_epoch()
{
  CAV_EPOCH=`cat $NS_WDIR/logs/TR$TESTRUN/.cav_epoch.diff`
  ALERT_HISTORY_CAV_EPOCH=`date --date="01-JAN-14" +%s`
  DIFF=`expr $CAV_EPOCH - $ALERT_HISTORY_CAV_EPOCH`
  DIFF=`expr $DIFF \* 1000`
} 

while getopts t:s:e:? c
do
  case $c in
    t)TESTRUN=$OPTARG;;
    s)START_PARTITION=$OPTARG;;
    e)END_PARTITION=$OPTARG;;
    *)usage "INVALID ARG $c";;
    ?)usage "INVALID ARG $c";;
  esac
done

check_mandatory_args

#getting partitions and put them in partitionList.txt file
if [ "X$START_PARTITION" == "X$END_PARTITION" ]
then
  echo $START_PARTITION >$NS_WDIR/logs/TR$TESTRUN/partitionList.txt
else
  get_partition_list
fi

calculating_diff_between_cav_epoch

delete_from_table()
{
  psql test cavisson <<+
  delete from alerthistory_${TESTRUN}_${PARTITION};
+

}

copying_table()
{
 #Make connection with DB and copy data from csv into database table
  psql test cavisson << +
  \copy alerthistory_${TESTRUN}_${PARTITION} from '$NS_WDIR/logs/TR$TESTRUN/$PARTITION/reports/csv/alertHistory.csv' WITH DELIMITER ',' NULL AS ''
+
  #if copy command failed
  if [ $? -ne 0 ]; then
    echo "Error: copy command to upload data in database failed"
    exit 1
  fi
}

get_part_time_in_ms()
{
  year=""
  month=""
  datee=""
  hour=""
  min=""
  sec=""
  part=`echo "$PARTITION"`
  i=0
  while [ $i -lt 14 ]
  do
    if [ $i -lt 4 ];then
      year+=`echo "${part:$i:1}"`
    elif [ $i -lt 6 ];then
      month+=`echo "${part:$i:1}"`
    elif [ $i -lt 8 ];then
      datee+=`echo "${part:$i:1}"`
    elif [ $i -lt 10 ];then
      hour+=`echo "${part:$i:1}"`
    elif [ $i -lt 12 ];then
      min+=`echo "${part:$i:1}"`
    elif [ $i -lt 14 ];then
      sec+=`echo "${part:$i:1}"`
    fi
    i=`expr $i + 1`
  done
  PART_TIME_IN_SEC=`date --date="$year-$month-$datee $hour:$min:$sec" +%s`
  CONST_TIME=`expr $PART_TIME_IN_SEC - $CAV_EPOCH - 3600`
  CONST_TIME=`expr $CONST_TIME \* 1000`
}

while read PARTITION
do
  #copy for backup
  cp $NS_WDIR/logs/TR$TESTRUN/$PARTITION/reports/csv/alertHistory.csv $NS_WDIR/logs/TR$TESTRUN/$PARTITION/reports/csv/alertHistory.csv_backup
  #get constraint time for comparing
  get_part_time_in_ms
  #changes in alert history timestamp column 
  awk -F',' '$2=$2-"'$DIFF'" {if($2<"'$CONST_TIME'"){$2="'$CONST_TIME'";print}else{$2=$2;print}}' OFS=, $NS_WDIR/logs/TR$TESTRUN/$PARTITION/reports/csv/alertHistory.csv>$NS_WDIR/logs/TR$TESTRUN/$PARTITION/reports/csv/output
  #move with changed file
  mv $NS_WDIR/logs/TR$TESTRUN/$PARTITION/reports/csv/output $NS_WDIR/logs/TR$TESTRUN/$PARTITION/reports/csv/alertHistory.csv
  #drop table and creating again
  delete_from_table
  copying_table
done<$NS_WDIR/logs/TR$TESTRUN/partitionList.txt
#removing file so that if someone run shell again partitionList.txt file should not be appended
rm $NS_WDIR/logs/TR$TESTRUN/partitionList.txt		
