#!/bin/bash
logs=$NS_WDIR/server_management/logs/addDelete.log
key=N9zAAFkhz9jdguTTNwt35N7I35T15AuJ
add_location()
{
	LOCATION=$(echo $FQL|cut -d':' -f1)
	COUNTRY=$(echo $FQL|cut -d':' -f2)
	STATE=$(echo $FQL|cut -d':' -f3)
	ZONE=$(echo $FQL|cut -d':' -f4)
	if [[ "XX$ZONE" == "XXEast" ]] || [[ "XX$ZONE" == "XXWest" ]] || [[ "XX$ZONE" == "XXCentral" ]]; then
		res=$(curl "https://www.mapquestapi.com/geocoding/v1/address?key=${key}&location=${LOCATION}+${STATE}+${COUNTRY}" 2>/dev/null)
		if [[ $? -ne 0 ]]; then
			echo "Unable to get Latitude and longitude for $LOCATION" >&2
			exit 1
		fi
		LAT=$(echo $res|jq '.results[0].locations[0].latLng.lat')
		LON=$(echo $res|jq '.results[0].locations[0].latLng.lng')
		CCODE=$(echo $res|jq '.results[0].locations[0].adminArea1'|sed -e 's/^"//' -e 's/"$//')
		CHECK_LOCATION=`psql  -X  -A -U postgres -d demo -t -c "select * from location where LOWER(location)=LOWER('$LOCATION') AND LOWER(country)=LOWER('$COUNTRY') AND LOWER(state)=LOWER('$STATE')"`
		if [ "xx" == "xx$CHECK_LOCATION" ]
		then
			psql  -X  -A -U postgres -d demo -t -c "insert into location (location, country, state, lat, lon, ccode, zone) values ('$LOCATION', '$COUNTRY', '$STATE', '$LAT', '$LON', '$CCODE', '$ZONE');" 2>&1 > /dev/null
			if [ $? -eq 0 ]
			then
			  echo "Database updated successfully."
				echo "$SESSUSER|$(date)|Location $LOCATION added successfully" >> $logs
				exit 0
			else
				echo "Database not updated successfully."
				exit 1
			fi
		else
		  echo "Entered Location already exists as $CHECK_LOCATION"
			exit 1
		fi
	else
		echo "Zone should be East, West Or Central" >&2
		exit 1
	fi
}

delete_location()
{
	if [ "xx$FQL" == "xx" ]
	then
	  echo "Parameters are not correct"
	  exit 1
	fi

	LOCATION=$(echo $FQL|cut -d':' -f1)
	COUNTRY=$(echo $FQL|cut -d':' -f2)
	STATE=$(echo $FQL|cut -d':' -f3)

	CHECK_LOCATION=`psql  -X  -A -U postgres -d demo -t -c "select location from location where LOWER(location)=LOWER('$LOCATION') AND LOWER(country)=LOWER('$COUNTRY') AND LOWER(state)=LOWER('$STATE')"`
	if [ "xx" != "xx$CHECK_LOCATION" ]
	then
		SERVERS=`psql -X -A -U postgres -d demo -t -c "select count(*) from servers where location='$LOCATION' AND country='$COUNTRY' AND state='$STATE'"`
		if [ $SERVERS -ne 0 ]
		then
			echo "$SERVERS Servers of this location does exist in master servers. Please remove those servers first"
			echo "$SESSUSER|$(date)|$SERVERS Servers of $LOCATION location does exist in master servers" >> $logs
			exit 1
		else
		  psql  -X  -A -U postgres -d demo -t -c "delete from location where location='$LOCATION' AND state='$STATE' AND country='$COUNTRY'" 2>&1 > /dev/null
			if [ $? -eq 0 ]
			then
			  echo "Database updated successfully"
				echo "$SESSUSER|$(date)|Location $LOCATION deleted successfully" >> $logs
				exit 0
			else
				echo "Database not updated successfully"
				exit 1
			fi
		fi
	else
	  echo "Entered Location does not exists."
		exit 1
	fi
}

list_location()
{
psql  -X  -A -U postgres -d demo -t << EOF
	SELECT
		row_number() over (), location.location, location.state, location.country, location.lat, location.lon, location.ccode, location.zone, count(servers.location)
	FROM
		servers right outer join location using(location)
	GROUP BY
		location.location, location.state, location.country, location.lat, location.lon, location.ccode, location.zone
EOF
}

list_only_locations()
{
psql  -X  -A -U postgres -d demo -t -c "select location from location"
}

usage()
{
echo "Use -a <Vendor Name> -p <Price> to add a vendor"
echo "Use -d <Vendor Name> to delete the vendor"
exit 1
}

if [ $# -lt 1 ]
then
	usage
	exit 1
fi

while getopts a:d:lou: arg
do
	case $arg in
		a)FQL=$OPTARG
			add_location;;
		d)FQL=$OPTARG
			delete_location;;
		u)SESSUSER=$OPTARG;;
		l)list_location;;
		o)list_only_locations;;
		*)usage ;;
	esac
done

exit 0
