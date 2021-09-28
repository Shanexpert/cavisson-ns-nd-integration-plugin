
if [ $# -ne 1 ]
then
  echo "Error: This shell requires arguments."
  echo "$0 <Csv file name>"
  exit 1
fi
#if [ "xx$NS_WDIR" == "xx" ]
#then
#  echo "NS_WDIR not set. Use bash -l"
#  exit 1
#fi

PATH=$1
if [ ! -e "$PATH" ]
then
  echo "CSV file not found."
  exit 1
else
  while read line
  do
    var=`echo $line | /usr/bin/rev | /usr/bin/cut -d "," -f 1`
    if [ "xx$var" != "xx" ]
    then
       echo $line | /bin/sed 's/$/,/' >> temp.temp
    else
      echo $line  >> temp.temp
    fi
  done < $PATH
  /bin/mv temp.temp $PATH
fi
/bin/rm temp.temp 2>/dev/null
