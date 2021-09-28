#!/bin/bash
logs=$NS_WDIR/server_management/logs/addDelete.log
add_vendor()
{
	CHECK_VENDOR=`psql  -X  -A -U postgres -d demo -t -c "select * from vendor where LOWER(vendor)=LOWER('$VENDOR')"`
	if [ "xx" == "xx$CHECK_VENDOR" ]
	then
		psql  -X  -A -U postgres -d demo -t -c "insert into vendor values('$VENDOR')" 2>&1 > /dev/null
		if [ $? -eq 0 ]
		then
			echo "0"
		        echo "Database updated successfully."
			echo "$SESSUSER|$(date)|Vendor $VENDOR successfully added to database" >> $logs
		else
			echo "1"
			echo "Database not updated successfully."
		fi
	else
		echo "1"
	        echo "Entered Vendor already exists as $CHECK_VENDOR"
		echo "$SESSUSER|$(date)|Entered Vendor $VENDOR already exists as $CHECK_VENDOR" >> $logs
	fi
}

delete_vendor()
{
if [ "xx$VENDOR" == "xx" ]
then
        echo "1"
        echo "Parameters are not correct"
        exit 1
fi

CHECK_VENDOR=`psql  -X  -A -U postgres -d demo -t -c "select * from vendor where LOWER(vendor)=LOWER('$VENDOR')"`
if [ "xx$VENDOR" == "xx$CHECK_VENDOR" ]
then
	SERVERS=`psql -X -A -U postgres -d demo -t -c "select count(*) from servers where vendor='$VENDOR'"`
	if [ $SERVERS -ne 0 ]
	then
		echo "1"
		echo "$SERVERS Servers of this vendor does exist in master servers. Please remove those servers first"
		echo "$SESSUSER|$(date)|$SERVERS Servers of vendor $VENDOR does exist in master servers. " >> $logs
	else
	        psql  -X  -A -U postgres -d demo -t -c "delete from vendor where vendor='$VENDOR'" 2>&1 > /dev/null
		if [ $? -eq 0 ]
		then
			echo "0"
		        echo "Database updated successfully"
			echo "$SESSUSER|$(date)|Vendor $VENDOR deleted Successfully" >> $logs
		else
			echo "1"
			echo "Database not updated successfully"
		fi
	fi
else
	echo "1"
        echo "Entered Vendor $VENDOR does not exists."
	echo "$SESSUSER|$(date)|Entered Vendor $VENDOR does not exists." >> $logs
fi

}

list_vendors()
{
psql  -X  -A -U postgres -d demo -t -c "select vendor from vendor" > file.temp
C=1
while read VENDOR
do
	COUNT=`psql -X -A -U postgres -d demo -t -c "select count(*) from servers where vendor='$VENDOR'"`
	echo "$C|$VENDOR|$COUNT"
	C=$(( $C+1 ))
done < file.temp
rm *.temp 2>/dev/null
}

list_only_vendors()
{
psql  -X  -A -U postgres -d demo -t -c "select vendor from vendor"
}

usage()
{
echo "Use -a <Vendor Name> -p <Price> to add a vendor"
echo "Use -d <Vendor Name> to delete the vendor"
}

if [ $# -lt 1 ]
then
	usage
	exit
fi

while getopts u:a:d:lo arg
do
	case $arg in
		a)VENDOR=$OPTARG
#		p)PRICE=$OPTARG
		  add_vendor;;
		d)VENDOR=$OPTARG
		  delete_vendor;;
		u)SESSUSER=$OPTARG;;
		l)list_vendors;;
		o)list_only_vendors;;
		*)usage ;;
	esac
done
