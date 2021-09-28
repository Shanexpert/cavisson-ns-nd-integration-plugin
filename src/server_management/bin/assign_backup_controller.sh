#!/bin/bash
log_file=$NS_WDIR/server_management/logs/assignment.log
while getopts b:i:u: args
do
        case $args in
          b) BACKUP=$OPTARG;;
          i) TARGET_FILE=$OPTARG;;
          u) SESSUSER=$OPTARG;;
        esac
done
bkp_ctrl_name=$(echo $BACKUP|cut -d "." -f 1)
bkp_blade_name=$(echo $BACKUP|cut -d "." -f 2)
check=$(echo $BACKUP|tr '.' '+')
tmp=$(grep -o $check /tmp/$TARGET_FILE)
if [ $? -eq 0 ]; then
 echo " Controller $tmp cannot be backup of itself. " >&2
 echo "$SESSUSER|$(date)|Controller $tmp cannot be backup of itself" >> $log_file
 exit 1
fi
for i in $(cat /tmp/$TARGET_FILE); do
  name=$(echo $i|cut -d "+" -f 1)
  blade=$(echo $i|cut -d "+" -f 2)

  psql -d demo -U postgres << EOF 2>&1 >/dev/null
    UPDATE allocation SET
     bkp_ctrl= CASE WHEN bkp_ctrl = '$bkp_ctrl_name' THEN 'none' ELSE '$bkp_ctrl_name' END,
     bkp_blade= CASE WHEN bkp_blade = '$bkp_blade_name' THEN 'none' ELSE '$bkp_blade_name' END
    WHERE
      server_name='$name'
    AND
      blade_name='$blade'
EOF
if [ $? -ne 0 ]; then
 echo "Failed to assign backup controller $bkp_ctrl_name $bkp_blade_name" >&2
 echo "$SESSUSER|$(date)|Failed to assign backup controller $bkp_ctrl_name $bkp_blade_name to $name $blade" >> $log_file
 break;
 exit 1
else
 echo "Backup controller $bkp_ctrl_name $bkp_blade_name Assigned successfully"
 echo "$SESSUSER|$(date)|Backup controller $bkp_ctrl_name $bkp_blade_name Assigned to $name $blade successfully" >> $log_file
fi
done
rm -f /tmp/$TARGET_FILE
exit 0
