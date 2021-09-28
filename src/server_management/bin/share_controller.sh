#!/bin/bash
log_file=$NS_WDIR/server_management/logs/assignment.log
while getopts t:c:i:u: args
do
        case $args in
          u) SESSUSER=$OPTARG;;
          t) TEAM=$OPTARG;;
          c) CHANNEL=$OPTARG;;
          i) TARGET_FILE=$OPTARG;;
        esac
done
for i in $(cat /tmp/$TARGET_FILE); do
  name=$(echo $i|cut -d "+" -f 1)
  blade=$(echo $i|cut -d "+" -f 2)
  psql -d demo -U postgres << EOF 2>&1 >/dev/null
    UPDATE allocation SET
     shared= CASE WHEN '$CHANNEL' = ANY(shared) THEN array_remove(shared,'$CHANNEL') ELSE array_append(shared,'$CHANNEL') END
    WHERE
      server_name='$name'
    AND
      blade_name='$blade'
EOF
if [ $? -ne 0 ]; then
 echo "Failed to share Controller $name blade $blade" >&2
 echo "$SESSUSER|$(date)|Failed to share Controller $name blade $blade to Team $TEAM project $CHANNEL" >> $log_file
 break;
else
 echo "successfully shared controller $name blade $blade"
 echo "$SESSUSER|$(date)|successfully shared controller $name blade $blade to Team $TEAM project $CHANNEL" >> $log_file
fi
done

rm -f /tmp/$TARGET_FILE
