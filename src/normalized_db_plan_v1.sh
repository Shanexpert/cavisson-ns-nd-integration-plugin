shopt -s expand_aliases
alias sqlCredentials="psql -U cavisson test"
logFile=$NS_WDIR/mssqlMigration.log
normalizeTableExist=0
executionStatsTableExists=0

exitOnErr()
{
        # echo message
	echo "$1" 1>&2 
	echo " 	        Hence, aborting migration."

        echo "$1" 1>&2 >> $logFile 
	echo "		Hence, aborting migration." >> $logFile

	echo ""
        echo "" >> $logFile

	echo "====================== MIGRATION ABORTED ======================"
	echo "====================== MIGRATION ABORTED ======================" >> $logFile

	echo ""
	echo "" >> $logFile

        # exit using erro code
        exit $2
}

errorMessage()	
{
	echo "$1" 1>&2
	echo "$1" 1>&2 >> $logFile
}

message()
{
	#Echo message
	echo "$1"
	echo "$1" >> $logFile
}


messqge ""
message "======================DATA MIGRATION STARTED WITH 13 STEPS========================"
message ""
message "Database Migration logs will be created in [ $logFile ]"
message ""

#If user doesnot pass the test run number then will fetch test run number from config.ini
	
	message "STEP 1:"
	message "		Going to get Test Run Number."
	
	if [ -z $1 ]; then
		testNum=`cat $NS_WDIR/webapps/sys/config.ini | grep -v '#' |awk -F= '/nde.testRunNum/{print $2}'`
		message "		TestRun number fetched from config.ini [ $NS_WDIR/webapps/sys/config.ini ] - $testNum"
	else
		testNum=$1
		message "		TestRun number fetched from user - $testNum"
	fi

#Checking if TR fetched is valid or not
	if [ ${testNum:-0} -lt  1 ]; then
      		exitOnErr "		ERROR: TestRun number [$testNum] is neither fetched from $NS_WDIR/webapps/sys/config.ini nor given by user in argument."

	fi

	message "		COMPLETED."
	echo ""

#Checking if TR fetched is directory or not
	directory="$NS_WDIR/logs/TR$testNum"
	
	message "STEP 2:"
	message "		Checking if directory '$directory' exist or not."
	
	if [ ! -d "$directory" ]; then
        	exitOnErr "		Directory '$directory' does not exist."
	fi
	
	message "		Directory '$directory' exist."
	message "		COMPLETED."
	message ""

#Checking if mssqltable_<TR> exits in test or not. If does not exist, then no migration requiered."
	mssqlTableTR="mssqltable_$testNum"
	mssqlReportTR="mssqlreport_$testNum"

	message "STEP 3:"
	message "		Checking if table '$mssqlTableTR' exist in testRunNum - $testNum"

	sqlCredentials  -c "\d+ $mssqlTableTR;" >/dev/null 
	[[ $? != 0 ]] && exitOnErr "		ERROR:  Table '$mssqlTableTR' does not exist."

	message "		Table '$mssqlTableTR' exist. "
	message "		COMPLETED."
	message ""

#mssqltable_<TR> exits so, checking if mssqlestimatedexecutionplan_<TR> exists or not. If table is already present, then no migration requiered.
	mssqlEstimatedExecutionPlanTR="mssqlestimatedexecutionplan_$testNum"

	message "STEP 4:"
	message "		Checking if table '$mssqlEstimatedExecutionPlanTR' exist in testRunNum - $testNum"

	sqlCredentials  -c "\d+ $mssqlEstimatedExecutionPlanTR;" >/dev/null
	[[ $? = 0 ]] && errorMessage "		Table '$mssqlEstimatedExecutionPlanTR' exist. So, no migration requiered for table '$mssqlEstimatedExecutionPlanTR'."
	[[ $? = 0 ]] && normalizeTableExist=1 
	
	[[ $? != 0 ]] && message "		Table '$mssqlEstimatedExecutionPlanTR' does not exist."	
	
	message "		COMPLETED."
	message ""

#Checking if mssqlexecutionstats_<TR> exits.
	mssqlExecutionStatsTR="mssqlexecutionstats_$testNum"
	
	message "STEP 5:"
	message "		Checking if table '$mssqlExecutionStatsTR' exist in testRunNum - $testNum"
	
	sqlCredentials  -c "\d+ $mssqlExecutionStatsTR;" >/dev/null
	[[ $? = 0 ]] && errorMessage "		Table '$mssqlExecutionStatsTR' exist. So, no migration requiered for table '$mssqlExecutionStatsTR'"
 	[[ $? = 0 ]] && executionStatsTableExists=1
	[[ $? != 0 ]] && message "		Table '$mssqlExecutionStatsTR' does not exist."

        message "		COMPLETED."
        message ""


#Check if both mssqlexecutionstats_<TR> and mssqlestimatedexecutionplan_<TR> exits
	message "STEP 6:"
	message "		Checking if tables '$mssqlEstimatedExecutionPlanTR' and '$mssqlExecutionStatsTR' exist or not."

	if (( ($executionStatsTableExists != 0) && ($normalizeTableExist != 0) )); then
		exitOnErr "		Both destination tables '$mssqlEstimatedExecutionPlanTR' and '$mssqlExecutionStatsTR' are already existing. So, no migration requiered."
	fi

	if (( ($executionStatsTableExists != 1) )); then
		message "		Destination tables '$mssqlExecutionStatsTR' does not exist."
	fi

	if (( ($normalizeTableExist != 1) )); then
		message "		Destination tables '$mssqlEstimatedExecutionPlanTR' does not exist."
	fi

	message "		COMPLETED."

#If $normalizeTableExist is 1, means mssqlEstimatedExecutionStats_TR exist so will not create table. If value is 0 create mssqlEstimatedExecutionPlanTR_<TR> along with create primary key and indexes

	message "STEP 7:"
	
	if (( ($normalizeTableExist != 1)  )); then
		mssqlEstimatedExecutionPlanQuery="select distinct on (planhandle, startoffset, endoffset, sourcedb) planhandle, startoffset, endoffset, sourcedb as servername, queryplan into $mssqlEstimatedExecutionPlanTR from $mssqlReportTR;"
		
		message "		Going to create table '$mssqlEstimatedExecutionPlanTR'"
		message "		Query: $mssqlEstimatedExecutionPlanQuery"

		sqlCredentials  -c "$mssqlEstimatedExecutionPlanQuery" >/dev/null
		[[ $? != 0 ]] && exitOnErr "		ERROR: Getting error in creating table '$mssqlEstimatedExecutionPlanTR'."
	
		message "		Table '$mssqlEstimatedExecutionPlanTR' created successfully."
		message "		COMPLETED."
		message ""

    	#Creating primary key on mssqlEstimatedExecutionPlanTR
		mssqlEstimatedExecutionPlanPrimaryKeyQuery="alter table $mssqlEstimatedExecutionPlanTR add PRIMARY KEY (planhandle,startoffset,endOffset,servername);"
		
		message "STEP 8:"
		message "		Going to create primary key on table '$mssqlEstimatedExecutionPlanTR'."
		message "		Query : $mssqlEstimatedExecutionPlanPrimaryKeyQuery"

		sqlCredentials  -c "$mssqlEstimatedExecutionPlanPrimaryKeyQuery" >/dev/null
		[[ $? != 0 ]] && exitOnErr "		ERROR: Error in creating primary key on table '$mssqlEstimatedExecutionPlanTR'."
	
		message "		Primary Key created successfully on table '$mssqlEstimatedExecutionPlanTR'"
		message "		COMPLETED."
		message ""
	
	#Creating indexes on mssqlEstimatedExecutionPlanTR
		mssqlEstimatedExecutionPlanIndexQuery="create index mssqlestimatedexecutionplan_sdb_$testNum on $mssqlEstimatedExecutionPlanTR (servername);"

		message "STEP 9:"
		message "		Going to create index on table '$mssqlEstimatedExecutionPlanTR'."
		message "		Query :  $mssqlEstimatedExecutionPlanIndexQuery"
		 
		sqlCredentials  -c "$mssqlEstimatedExecutionPlanIndexQuery" >/dev/null
	
		[[ $? != 0 ]] && exitOnErr "		ERROR: Error in creating index on table $mssqlEstimatedExecutionPlanTR."

		message "		Created index on table on '$mssqlEstimatedExecutionPlanTR' successfully."
		message "		COMPLETED."
		message ""
	else
		message "		Skipping table '$mssqlEstimatedExecutionPlanTR' migration, as it already exist."
		message "		COMPLETED."

	fi

#If executionStatsTableExists = 1, means table exist so skip migration. If value is 0, then Creating mssqlExecutionStatsTR table, primary key and index

	message "STEP 10:"
	mssqlExecutionStatTR="mssqlexecutionstats_$testNum"

	if (( ($executionStatsTableExists != 1) )); then
		mssqlExecutionStatTRQuery="select normmssqlid, totalexecutioncount, totalworkertime, totalelapsedtime, totalphysicalreads, totallogicalreads, totallogicalwrites, totalclrtime, avgworkertime, avgphysicalreads, avglogicalwrites,avglogicalreads,avgclrtime,avgelapsedtime,totalwaittime,lastwaittime,minwaittime,maxwaittime, avgwaittime, planhandle, creationtime, timestamp, databasename, sourcedb as servername, startoffset,endoffset,serialno into $mssqlExecutionStatTR from $mssqlReportTR;"

		message "		Going to create table '$mssqlExecutionStatTR'."
		message "		Query: $mssqlExecutionStatTRQuery"

		sqlCredentials  -c "$mssqlExecutionStatTRQuery" >/dev/null
	
		[[ $? != 0 ]] && exitOnErr "		ERROR: Unable to create table '$mssqlExecutionStatTR'."
	
		message "		Created table '$mssqlExecutionStatTR' successfully."
		message "		COMPLETED."
		message ""

	#Creating primary key on mssqlexecutionstats_<TR>
		mssqlExecutionStatTRPrimaryKeyQuery="alter table $mssqlExecutionStatTR add PRIMARY KEY (serialno,timestamp);"

		message "STEP 11:"
		message "		Going to add primary key on table '$mssqlExecutionStatTR'."
		message "		Query : $mssqlExecutionStatTRPrimaryKeyQuery"
	
		sqlCredentials  -c "$mssqlExecutionStatTRPrimaryKeyQuery" >/dev/null

		[[ $? != 0 ]] && exitOnErr "		ERROR: Unable to add primary key on table '$mssqlExecutionStatTR'."

		message "		Primary key successfully created on table '$mssqlExecutionStatTR'."
		message "		COMPLETED."
		message ""

	#Creating indexes on mssqlexecutionstats_<TR>
		mssqlExecutionStatsIndexQuery="create index mssqlexecutionstats_index_$testNum on $mssqlExecutionStatTR (normmssqlid,timestamp,servername);"
	
		message "STEP 12:"
		message "		Going to create index on table '$mssqlExecutionStatTR'."
		message "         	Query : $mssqlExecutionStatsIndexQuery"
	
		sqlCredentials  -c "$mssqlExecutionStatsIndexQuery" >/dev/null
	
		[[ $? != 0 ]] && exitOnErr "		ERROR: Unable to create index on table '$mssqlExecutionStatTR'."
	
		message "          	Index successfully created on table '$mssqlExecutionStatTR'."
		message "		COMPLETED."
		message ""
	else
                message "		Skipping table '$mssqlExecutionStatTR' migration, as it already exist."
		message "		COMPLETED."
	fi

#Create hidden offset file
	message "STEP 13:"
	message "		Going to fetch partition number."

	partitionNum=`cat $directory/.curPartition | awk -F= '/CurPartitionIdx/{print $2}'`	
	message "		Fetched Partition Number - $partitionNum"
	
	offsetFilePath="$directory/$partitionNum/reports/csv"	
	message "		Going to create .MSSQLActualExecutionStats.csv.offset file in path - $offsetFilePath"
	
	`touch $offsetFilePath/.MSSQLActualExecutionStats.csv.offset`
	message "		COMPLETED."
	message ""

	message "============DATA MIGRATION COMPLETED=============="
	message ""
