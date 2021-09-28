add_vendor()
{
  CHECK_VENDOR=`psql  -X  -A -U postgres -d demo -t -c "select * from vendor where LOWER(vendor)=LOWER('$INPUT_VENDOR')"`
  if [ "xx" == "xx$CHECK_VENDOR" ]
  then
    psql  -X  -A -U postgres -d demo -t -c "insert into vendor values('$INPUT_VENDOR')" 2>&1 > /dev/null
    if [ $? -eq 0 ]
    then
      echo "SUCCESS:Vendor $INPUT_VENDOR added successfully"
    else
      echo "ERROR:Database error"
    fi
  else
    echo "ERROR:Vendor already exists as $CHECK_VENDOR"
  fi
}

add_location()
{
  CHECK_LOCATION=`psql  -X  -A -U postgres -d demo -t -c "select * from location where LOWER(location)=LOWER('$INPUT_LOCATION')"`
  if [ "xx" == "xx$CHECK_LOCATION" ]
  then
    psql  -X  -A -U postgres -d demo -t -c "insert into location values('$INPUT_LOCATION')" 2>&1 > /dev/null

    if [ $? -eq 0 ]
    then
      echo "SUCCESS:Location $INPUT_LOCATION added successfully"
    else
      echo "ERROR:Database error"
    fi
  else
    echo "ERROR:Location already exists as $CHECK_LOCATION"
  fi

}
remove_vendor()
{
  CHECK_VENDOR=`psql  -X  -A -U postgres -d demo -t -c "select * from vendor where LOWER(vendor)=LOWER('$INPUT_VENDOR')"`
  if [ "xx" == "xx$CHECK_VENDOR" ]
  then
    echo "ERROR:Vendor name does not exist"
  else
    psql  -X  -A -U postgres -d demo -t -c "delete from vendor where vendor='$CHECK_VENDOR'" 2>&1 > /dev/null
    psql  -X  -A -U postgres -d demo -t -c "update servers set vendor = 'NA' where vendor = '$CHECK_VENDOR'" 2>&1 > /dev/null
    if [ $? -eq 0 ]
    then
      echo "SUCCESS:Vendor $INPUT_VENDOR removed successfully"
    else
      echo "ERROR:Database error"
    fi
  fi
}

remove_location()
{
  CHECK_LOCATION=`psql  -X  -A -U postgres -d demo -t -c "select * from location where LOWER(location)=LOWER('$INPUT_LOCATION')"`
  if [ "xx" == "xx$CHECK_LOCATION" ]
  then
    echo "ERROR:Location does not exists"
  else
    psql  -X  -A -U postgres -d demo -t -c "delete from location where location='$CHECK_LOCATION'" 2>&1 > /dev/null
    psql  -X  -A -U postgres -d demo -t -c "update servers set location = 'NA' where location = '$CHECK_LOCATION'" 2>&1 > /dev/null
    if [ $? -eq 0 ]
    then
      echo "SUCCESS:Location $INPUT_LOCATION removed successfully"
    else
      echo "ERROR:Database error"
    fi
  fi
}

while getopts v:l:o: arg
do
  case $arg in
    v) INPUT_VENDOR="$OPTARG"
       VENDOR_FLAG="1";;
    l) INPUT_LOCATION="$OPTARG"
       LOCATION_FLAG="1";;
    o) OPERATION="$OPTARG";;
  esac
done

if [ "xx$OPERATION" == "xxadd" ] && [ "xx$VENDOR_FLAG" == "xx1" ]
then
  add_vendor
elif [ "xx$OPERATION" == "xxadd" ] && [ "xx$LOCATION_FLAG" == "xx1" ]
then
  add_location
elif [ "xx$OPERATION" == "xxremove" ] && [ "xx$VENDOR_FLAG" == "xx1" ]
then
  remove_vendor
elif [ "xx$OPERATION" == "xxremove" ] && [ "xx$LOCATION_FLAG" == "xx1" ]
then
  remove_location
fi
