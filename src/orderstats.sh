shopt -s expand_aliases

#############################  CONFIGURATION  ############################################

#Details for OrderDB
ORDERDB_USER="rptuser"
ORDERDB_PASSWORD="Rw2fKREcDKzvfHAS"
ORDERDB_DBNAME="orderdb"

#Details for CartDB
CARTDB_USER="checkoutapp"
CARTDB_PASSWORD="ofkP3MnOSk3IuKbn"
CARTDB_DBNAME="checkoutdb"
##########################################################################################

#Data Limit for ViewBy Option (Time in Hours)
MAX_DATA_FETCH_LIMIT_HOURLY="25"
MAX_DATA_FETCH_LIMIT_MINUTES="25"
MAX_DATA_FETCH_LIMIT_SECONDS="2"

#Constants
HOUR="Hour"
MIN="Min"
SEC="Sec"
ORDERDB_SUBMITTED_ORDERS="Submitted_Orders(OrderDb)"
ORDER_INLINE_ITEMS="Order_Inline_Items(OrderDb)"
SUBMITTED_ORDERS_VIA_KOHLS_CASH="KohlsCash_Orders(OrderDb)"
CARTDB_SUBMITTED_ORDER="Submitted_Orders(CartDB)"
ORDER_ORDERS_BY_PAYMENT_MODE="Orders_by_PaymentMode(OrderDb)"
ORDERDB_ORDERS_OVERALL_DETAILS="Orders_OverALL_Details(OrderDb)"
ORDERS_REPORT="Orders_Report(OrderDb)"

#Min Date format
DATE_FORMATTING="%m/%d/%Y %H:%i:00" 

statsType=$1
HOST=$2
startDate=$3
startTime=$4
endDate=$5
endTime=$6
groupBy=$7
viewBy=$8

#Check for Statstype
	if [ $statsType == "" ]; then
		echo "Statstype should be either '$ORDERDB_SUBMITTED_ORDERS' or '$ORDER_INLINE_ITEMS' or '$SUBMITTED_ORDERS_VIA_KOHLS_CASH' or '$CARTDB_SUBMITTED_ORDER' or '$ORDER_ORDERS_BY_PAYMENT_MODE' or '$ORDERDB_ORDERS_OVERALL_DETAILS' or '$ORDERS_REPORT'"
		exit 0
	fi
	
	if [ $statsType != "$ORDERDB_SUBMITTED_ORDERS" ] && [ $statsType != "$ORDER_INLINE_ITEMS" ] && [ $statsType != "$SUBMITTED_ORDERS_VIA_KOHLS_CASH" ] && [ $statsType != "$CARTDB_SUBMITTED_ORDER" ] && [ $statsType != "$ORDER_ORDERS_BY_PAYMENT_MODE" ] && [ $statsType != "$ORDERDB_ORDERS_OVERALL_DETAILS" ] && [ $statsType != "$ORDERS_REPORT" ]; then
		echo "Statstype should be either '$ORDERDB_SUBMITTED_ORDERS' or 'ORDER_INLINE_ITEMS' or '$SUBMITTED_ORDERS_VIA_KOHLS_CASH' or '$CARTDB_SUBMITTED_ORDER' or '$ORDER_ORDERS_BY_PAYMENT_MODE' or '$ORDERDB_ORDERS_OVERALL_DETAILS' or '$ORDERS_REPORT'"
		exit 0
	fi


#Check for start date time & end date time format
	startDateTime="$startDate $startTime"
	endDateTime="$endDate $endTime"

	startDateTimeStamp=`date -d "$startDateTime" +%s`
	endDateTimeStamp=`date -d "$endDateTime" +%s`

	startDateLen=${#startDate}
	startTimeLen=${#startTime}
	endDateLen=${#endDate}
	endTimeLen=${#endTime}

	if [ ${startDateLen:-0} != 10 ]; then
		echo "Start Date Time and End Date Time should be in mm/dd/yyyy hh24:mm:ss format"
		exit 0
	fi

	if [ ${startTimeLen:-0} != 8 ]; then
		echo "Start Date Time and End Date Time should be in mm/dd/yyyy hh24:mm:ss format"
		exit 0
	fi

	if [ ${endDateLen:-0} != 10 ]; then
		echo "Start Date Time and End Date Time should be in mm/dd/yyyy hh24:mm:ss format"
		exit 0
	fi

	if [ ${endTimeLen:-0} != 8 ]; then
		echo "Start Date Time and End Date Time should be in mm/dd/yyyy hh24:mm:ss format"
		exit 0
	fi

	if [ "$endDateTime" \< "$startDateTime" ]; then
	        echo "End Date Time ($endDateTime) should be greater than or equal to Start date Time ($startDateTime)"
	        exit 0
	fi

#Check for diff bet end date time and start date time is less than 25 hours
	diff=`expr $endDateTimeStamp - $startDateTimeStamp`
#echo "$MAX_DATA_FETCH_LIMIT_MINUTES  , Diff - $diff"
	
	if [ $viewBy == $HOUR ]; then	
		MAX_DATA_FETCH_LIMIT_HOURLY=`expr $MAX_DATA_FETCH_LIMIT_HOURLY \* 60 \* 60`
		
		if [ "$diff" -gt "$MAX_DATA_FETCH_LIMIT_HOURLY" ]; then
			echo "Maximum data fetch for $MAX_DATA_FETCH_LIMIT_HOURLY hours"
			exit 0
		fi	

	elif [ $viewBy == $MIN ]; then			
		MAX_DATA_FETCH_LIMIT_MINUTES=`expr $MAX_DATA_FETCH_LIMIT_MINUTES \* 60 \* 60`

		if [ "$diff" -gt "$MAX_DATA_FETCH_LIMIT_MINUTES" ]; then	
			echo "Maximum data fetch for $MAX_DATA_FETCH_LIMIT_MINUTES minutes"
			exit 0
		fi
	
	elif [ $viewBy == $SEC ]; then		
		MAX_DATA_FETCH_LIMIT_SECONDS=`expr $MAX_DATA_FETCH_LIMIT_SECONDS \* 60 \* 60`	
		
		if [ "$diff" -gt "$MAX_DATA_FETCH_LIMIT_SECONDS" ]; then
			echo "Maximum data fetch for $MAX_DATA_FETCH_LIMIT_SECONDS seconds"
			exit 0
		fi
	fi

#Create connection string
	if [ $HOST == "NA" ]; then
		if [ "$statsType" == "$CARTDB_SUBMITTED_ORDER" ]; then
			alias sqlCredentials="mysql -u $CARTDB_USER -p\"$CARTDB_PASSWORD\" -D $CARTDB_DBNAME --ssl-mode=DISABLED"
		else
			alias sqlCredentials="mysql -u $ORDERDB_USER -p\"$ORDERDB_PASSWORD\" -D $ORDERDB_DBNAME --ssl-mode=DISABLED"
		fi
	else
		if [ "$statsType" == "$CARTDB_SUBMITTED_ORDER" ]; then
			alias sqlCredentials="mysql -h $HOST -u $CARTDB_USER -p\"$CARTDB_PASSWORD\" -D $CARTDB_DBNAME"
		else
			alias sqlCredentials="mysql -h $HOST -u $ORDERDB_USER -p\"$ORDERDB_PASSWORD\" -D $ORDERDB_DBNAME"
		fi
	fi
			
			
			
			
#Creating Query based on statsType, viewBy and groupBy

if [ $viewBy == "$SEC" ]; then
	DATE_FORMATTING="%m/%d/%Y %H:%i:%s"
elif [ $viewBy == "$HOUR" ]; then
	DATE_FORMATTING="%m/%d/%Y %H:00:00"	
fi

startTime="$startTime:000000"
endTime="$endTime:999999"

#echo "start - $startTime , End - $endTime"

#ORDERDB
if [ "$statsType" == "$ORDERDB_SUBMITTED_ORDERS" ]; then
	if [ $groupBy == "Channel" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders, channel_code Channel, SUM(dai.Merchandise_Offer_Price) as 'Revenue($)' FROM orderdb.est_order eo INNER JOIN est_order_price dai on eo.order_id = dai.order_id WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') AND eo.perf_user = 0 GROUP BY eo.channel_code,DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"
	
	elif [ $groupBy == "None" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders, SUM(dai.Merchandise_Offer_Price) as ' Revenue($)' FROM orderdb.est_order eo INNER JOIN est_order_price dai on eo.order_id = dai.order_id   WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') AND eo.perf_user = 0 GROUP BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"
	fi
fi


#CARTDB
if [ $statsType == "$CARTDB_SUBMITTED_ORDER" ]; then
	if [ $groupBy == "Channel" ]; then
			query="SELECT DATE_FORMAT(eo.created_ts,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders, last_updated_by channel FROM checkoutdb.est_failed_orders eo WHERE eo.created_ts BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') GROUP BY eo.last_updated_by,DATE_FORMAT(eo.created_ts,'$DATE_FORMATTING') ORDER BY DATE_FORMAT(eo.created_ts,'$DATE_FORMATTING');"	
	elif [ $groupBy == "None" ]; then
			query="SELECT DATE_FORMAT(eo.created_ts,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders FROM checkoutdb.est_failed_orders eo WHERE eo.created_ts BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') GROUP BY DATE_FORMAT(eo.created_ts,'$DATE_FORMATTING') ORDER BY DATE_FORMAT(eo.created_ts,'$DATE_FORMATTING');"	
	fi
fi


#CARTSIZE
if [ $statsType == "$ORDER_INLINE_ITEMS" ]; then
	if [ $groupBy == "Channel" ]; then
 		#query="select DATE_FORMAT(est_order.submitted_tmst,'$DATE_FORMATTING') Submitted_Time, est_order.channel_code, est_order.order_id, est_item.sku_id, count(est_item.sku_id) countID From est_order est_order JOIN est_item est_item ON est_item.order_id=est_order.order_id where est_order.submitted_tmst >= STR_TO_DATE('$startDate $startTime', '%m/%d/%Y %H:%i:%s:%f') and est_order.submitted_tmst <= STR_TO_DATE('$endDate $endTime', '%m/%d/%Y %H:%i:%s:%f') AND est_order.perf_user = 0 GROUP BY  est_item.sku_id, est_order.order_id, est_order.channel_code,DATE_FORMAT(est_order.submitted_tmst,'$DATE_FORMATTING') ORDER BY est_order.submitted_tmst;"
		query="select t1.Submitted_Time as 'Submitted Time', t1.channel_code Channel,  max(t1.NUM_OF_Items) as 'Max Cart Size', min(t1.NUM_OF_Items) as 'Min Cart Size', avg(t1.NUM_OF_Items) as 'Avg Cart Size' from ( select count(*) as NUM_OF_Items, channel_code, DATE_FORMAT(est_order.submitted_tmst,'$DATE_FORMATTING') Submitted_Time, est_order.order_id From est_order est_order JOIN est_item est_item ON est_item.order_id=est_order.order_id where est_order.submitted_tmst >= STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') and est_order.submitted_tmst <= STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') AND est_order.perf_user = 0 GROUP BY est_order.channel_code,est_order.order_id, DATE_FORMAT(est_order.submitted_tmst,'$DATE_FORMATTING')) t1 group by channel_code, t1.Submitted_Time ORDER BY t1.Submitted_Time;"
	elif [ $groupBy == "None" ]; then
			query="select t1.Submitted_Time as 'Submitted Time', max(t1.NUM_OF_Items) as 'Max Cart Size', min(t1.NUM_OF_Items) as 'Min Cart Size', avg(t1.NUM_OF_Items) as 'Avg Cart Size' from ( select count(*) as NUM_OF_Items, DATE_FORMAT(est_order.submitted_tmst,'$DATE_FORMATTING') Submitted_Time, est_order.order_id From est_order est_order JOIN est_item est_item ON est_item.order_id=est_order.order_id where est_order.submitted_tmst >= STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') and est_order.submitted_tmst <= STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') AND est_order.perf_user = 0 GROUP BY est_order.order_id, DATE_FORMAT(est_order.submitted_tmst,'$DATE_FORMATTING')) t1 group by t1.Submitted_Time ORDER BY t1.Submitted_Time;"
	fi
fi

#KOHLS_CASH
if [ "$statsType" == "$SUBMITTED_ORDERS_VIA_KOHLS_CASH" ]; then
	if [ $groupBy == "Channel" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) as 'KohlsCash Orders', channel_code Channel FROM orderdb.est_order eo INNER JOIN est_payment dai on eo.order_id = dai.order_id WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') and dai.pay_instrument_type ='kohlsCashCoupon' AND eo.perf_user = 0 GROUP BY eo.channel_code,DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"

	elif [ $groupBy == "None" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) as 'KohlsCash Orders' FROM orderdb.est_order eo INNER JOIN est_payment dai on eo.order_id = dai.order_id  WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') and dai.pay_instrument_type ='kohlsCashCoupon' AND eo.perf_user = 0  GROUP BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"
	fi
fi

#OrderDb-Orders_by_PaymentMode
if [ "$statsType" == "$ORDER_ORDERS_BY_PAYMENT_MODE" ]; then
	if [ $groupBy == "Channel" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders, channel_code Channel,dai.pay_instrument_type as 'Payment Mode' FROM orderdb.est_order eo, est_payment dai WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') and eo.order_id = dai.order_id AND eo.perf_user = 0  GROUP BY eo.channel_code,DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING'),dai.pay_instrument_type ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"
	
	elif [ $groupBy == "None" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders, dai.pay_instrument_type as 'Payment Mode' FROM orderdb.est_order eo, est_payment dai WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') and eo.order_id = dai.order_id AND eo.perf_user = 0 GROUP BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING'),dai.pay_instrument_type ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"
	fi
fi

#OrderDb-Orders_OverALL_Details
if [ "$statsType" == "$ORDERDB_ORDERS_OVERALL_DETAILS" ]; then
	if [ $groupBy == "Channel" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders, channel_code Channel, SUM(dai.Merchandise_Offer_Price) as 'Revenue($)', pi.pay_instrument_type as 'Payment Mode' FROM orderdb.est_order eo INNER JOIN est_order_price dai on eo.order_id = dai.order_id INNER JOIN est_payment pi on pi.order_id=dai.order_id WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') AND eo.perf_user = 0 GROUP BY eo.channel_code,DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING'),pi.pay_instrument_type ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"
	
	elif [ $groupBy == "None" ]; then
		query="SELECT DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING') as 'Submitted Time', COUNT(1) Orders, SUM(dai.Merchandise_Offer_Price) as 'Revenue($)', pi.pay_instrument_type as 'Payment Mode' FROM orderdb.est_order eo INNER JOIN est_order_price dai on eo.order_id = dai.order_id INNER JOIN est_payment pi on pi.order_id=dai.order_id WHERE eo.submitted_tmst BETWEEN STR_TO_DATE('$startDate $startTime','%m/%d/%Y %H:%i:%s') AND STR_TO_DATE('$endDate $endTime','%m/%d/%Y %H:%i:%s') AND eo.perf_user = 0 GROUP BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING'),pi.pay_instrument_type ORDER BY DATE_FORMAT(eo.submitted_tmst,'$DATE_FORMATTING');"
	fi
fi

#OPM (ORDERS_REPORT)
if [ "$statsType" == "$ORDERS_REPORT" ]; then
	#query="SELECT tab.Order_Time, SUM(tab.Order_Count) All_Channel_Order_Count, SUM(CASE WHEN Channel_Code = 'WEB' THEN Order_Count ELSE 0 END ) WEB_Order_Count, SUM(CASE WHEN Channel_Code = 'MOBILE' THEN Order_Count ELSE 0 END ) MOBILE_Order_Count, SUM(CASE WHEN Channel_Code = 'kiosk' THEN Order_Count ELSE 0 END ) kiosk_Order_Count, SUM(CASE WHEN Channel_Code = 'android' THEN Order_Count ELSE 0 END) android_Order_Count,  SUM(CASE WHEN Channel_Code = 'ipad' THEN Order_Count ELSE  0 END ) ipad_Order_Count, SUM(CASE  WHEN Channel_Code = 'tablet' THEN Order_Count ELSE  0 END ) tablet_Order_Count, SUM(CASE WHEN Channel_Code = 'iphone' THEN Order_Count ELSE 0 END ) iphone_Order_Count, SUM( CASE WHEN Channel_Code = 'contactCenter' THEN Order_Count ELSE  0 END ) contactCenter_Order_Count, SUM(CASE WHEN Channel_Code = 'endless_aisle' THEN  Order_Count ELSE  0 END ) endless_aisle_Order_Count, SUM(CASE WHEN Channel_Code = 'channelx' THEN Order_Count ELSE 0  END )     channelx_Order_Count, SUM(CASE  WHEN Channel_Code = 'channely' THEN Order_Count ELSE 0 END ) channely_Order_Count , SUM(tab.Revenue) All_Channel_Revenue, SUM(CASE WHEN Channel_Code = 'WEB' THEN Revenue ELSE 0 END ) WEB_Revenue, SUM(CASE WHEN Channel_Code = 'MOBILE' THEN Revenue ELSE 0 END ) MOBILE_Revenue, SUM(CASE WHEN Channel_Code = 'kiosk' THEN  Revenue ELSE 0 END ) kiosk_Revenue, SUM(CASE WHEN Channel_Code = 'android' THEN Revenue ELSE 0 END ) android_Revenue, SUM(CASE WHEN Channel_Code = 'ipad' THEN Revenue ELSE  0 END ) ipad_Revenue, SUM(CASE WHEN Channel_Code = 'tablet' THEN Revenue ELSE  0 END ) tablet_Revenue, SUM(CASE WHEN Channel_Code = 'iphone' THEN  Revenue ELSE 0 END ) iphone_Revenue, SUM(CASE WHEN Channel_Code = 'contactCenter' THEN Revenue ELSE  0 END ) contactCenter_Revenue, SUM(CASE WHEN Channel_Code = 'endless_aisle' THEN Revenue ELSE 0 END ) endless_aisle_Revenue, SUM(CASE WHEN Channel_Code = 'channelx' THEN Revenue ELSE 0 END) channelx_Revenue, SUM(CASE WHEN Channel_Code = 'channely' THEN Revenue ELSE 0 END ) channely_Revenue FROM ( SELECT DATE_FORMAT(ord.Submitted_Tmst, '$DATE_FORMATTING') Order_Time, ord.Channel_Code , COUNT(1) Order_Count, SUM(CASE WHEN ord.Status_Code NOT IN ('CANCELLED','FAILED','REJECTED') THEN orp.Merchandise_Offer_Price ELSE 0 END ) Revenue FROM EST_ORDER ord , EST_ORDER_PRICE orp WHERE ord.Order_Id = orp.Order_Id AND ord.Submitted_Tmst BETWEEN STR_TO_DATE('$startDate $startTime', '%m/%d/%Y %H:%i:%s:%f') AND STR_TO_DATE('$endDate $endTime', '%m/%d/%Y %H:%i:%s:%f') AND perf_user = 0 GROUP BY DATE_FORMAT(ord.Submitted_Tmst, '$DATE_FORMATTING'), ord.Channel_Code) tab GROUP BY Order_Time ORDER BY Order_Time;"
	query="SELECT tab.Order_Time, SUM(tab.Order_Count) All_Channel_Order_Count, SUM(CASE WHEN Channel_Code = 'WEB' THEN Order_Count ELSE 0 END ) WEB_Order_Count, SUM(CASE WHEN Channel_Code = 'MOBILE' THEN Order_Count ELSE 0 END ) MOBILE_Order_Count, SUM(CASE WHEN Channel_Code = 'kiosk' THEN Order_Count ELSE 0 END ) kiosk_Order_Count, SUM(CASE WHEN Channel_Code = 'android' THEN Order_Count ELSE 0 END) android_Order_Count,  SUM(CASE WHEN Channel_Code = 'ipad' THEN Order_Count ELSE  0 END ) ipad_Order_Count, SUM(CASE  WHEN Channel_Code = 'tablet' THEN Order_Count ELSE  0 END ) tablet_Order_Count, SUM(CASE WHEN Channel_Code = 'iphone' THEN Order_Count ELSE 0 END ) iphone_Order_Count, SUM( CASE WHEN Channel_Code = 'contactCenter' THEN Order_Count ELSE  0 END ) contactCenter_Order_Count, SUM(CASE WHEN Channel_Code = 'endless_aisle' THEN  Order_Count ELSE  0 END ) endless_aisle_Order_Count, SUM(CASE WHEN Channel_Code = 'channelx' THEN Order_Count ELSE 0  END )     channelx_Order_Count, SUM(CASE  WHEN Channel_Code = 'channely' THEN Order_Count ELSE 0 END ) channely_Order_Count , SUM(tab.Revenue) All_Channel_Revenue, SUM(CASE WHEN Channel_Code = 'WEB' THEN Revenue ELSE 0 END ) WEB_Revenue, SUM(CASE WHEN Channel_Code = 'MOBILE' THEN Revenue ELSE 0 END ) MOBILE_Revenue, SUM(CASE WHEN Channel_Code = 'kiosk' THEN  Revenue ELSE 0 END ) kiosk_Revenue, SUM(CASE WHEN Channel_Code = 'android' THEN Revenue ELSE 0 END ) android_Revenue, SUM(CASE WHEN Channel_Code = 'ipad' THEN Revenue ELSE  0 END ) ipad_Revenue, SUM(CASE WHEN Channel_Code = 'tablet' THEN Revenue ELSE  0 END ) tablet_Revenue, SUM(CASE WHEN Channel_Code = 'iphone' THEN  Revenue ELSE 0 END ) iphone_Revenue, SUM(CASE WHEN Channel_Code = 'contactCenter' THEN Revenue ELSE  0 END ) contactCenter_Revenue, SUM(CASE WHEN Channel_Code = 'endless_aisle' THEN Revenue ELSE 0 END ) endless_aisle_Revenue, SUM(CASE WHEN Channel_Code = 'channelx' THEN Revenue ELSE 0 END) channelx_Revenue, SUM(CASE WHEN Channel_Code = 'channely' THEN Revenue ELSE 0 END ) channely_Revenue FROM ( SELECT DATE_FORMAT(ord.Submitted_Tmst, '$DATE_FORMATTING') Order_Time, ord.Channel_Code , COUNT(1) Order_Count, SUM(orp.Merchandise_Offer_Price) Revenue FROM EST_ORDER ord , EST_ORDER_PRICE orp WHERE ord.Order_Id = orp.Order_Id AND ord.Submitted_Tmst BETWEEN STR_TO_DATE('$startDate $startTime', '%m/%d/%Y %H:%i:%s:%f') AND STR_TO_DATE('$endDate $endTime', '%m/%d/%Y %H:%i:%s:%f') AND perf_user = 0 GROUP BY DATE_FORMAT(ord.Submitted_Tmst, '$DATE_FORMATTING'), ord.Channel_Code) tab GROUP BY Order_Time ORDER BY Order_Time;"
fi

#echo "$query"

sqlCredentials  -e "$query" -w 2>/dev/null
exit 0
