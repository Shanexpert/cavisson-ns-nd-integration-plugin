#ifndef NS_ERROR_MSG_H
#define NS_ERROR_MSG_H

/********************************************************************
 * Name            : ns_error_msg.h  
 * Purpose         : Definition file of error messages 
 * Initial Version : 
 * Modification    : 
 * Document        : cavisson/docs......

 * Error Classifications

   0 System Errors                    : CavErr100xxxx
  
   1 Parsing Related Errors           : CavErr101xxxx
       a) Scenario Errors             : CavErr1011xxx
       b) Script Errors               : CavErr1012xxx
       c) GDF Errors                  : CavErr1013xxx
       d) Machine Config              : CavErr1014xxx 
       e) Misc Error                  : CavErr1019xxx

   2 Generator Related Errors         : CavErr102xxxx
       a) Health Check Error          : CavErr1021xxx
       b) Data Upload Error           : CavErr1022xxx
       c) Generator Failue Error      : CavErr1023xxx
       d) Misc Error                  : CavErr1029xxx 

   3 Test Related Errors              : CavErr103xxxx
       a) Test Start Error            : CavErr1031xxx
       b) License Error               : CavErr1032xxx
       c) Parameterisation Error      : CavErr1033xxx
       d) SyncPoint Error             : CavErr1034xxx
       e) CVM Failue Error            : CavErr1035xxx
       f) Message Communication Error : CavErr1036xxx
       g) End TestRun Error           : CavErr1037xxx
       h) Misc Error                  : CavErr1039xxx 

   4 RBU Error                        : CavErr104xxxx

   5 RTC Error                        : CavErr105xxxx
       a) Manage Vuser Error          : CavErr1051xxx 
       b) Manage Generator Error      : CavErr1052xxx
       c) Quantity RTC Error          : CavErr1053xxx
       d) Scheduling RTC Error        : CavErr1054xxx
       e) SyncPoint RTC               : CavErr1055xxx
       f) Monitor RTC Error           : CavErr1056xxx

   6 Monitor Error                    : CavErr106xxxx

   7 Miscellaneous Error              : CavErr107xxxx
 ********************************************************************/


#define CAV_ERR_HDR_LEN 17
#define CAV_ERR_HDR_LEN_ONLY 15

#define CAV_ERR_MSG_1 "Invalid number of arguments."
#define CAV_ERR_MSG_2 "Value must be numeric."
#define CAV_ERR_MSG_3 "Invalid option."
#define CAV_ERR_MSG_4 "Value cannot be 0."
#define CAV_ERR_MSG_5 "Maximum value must be greater than minimum value."
#define CAV_ERR_MSG_6 "Percentage value range should be in between 0 and 100."
#define CAV_ERR_MSG_7 "Incorrect Group name '%s' provided."
#define CAV_ERR_MSG_8 "Value cannot be less than 0."
#define CAV_ERR_MSG_9 "Value must be greater than 0."
#define CAV_ERR_MSG_10 "Value can range in between 0 and 65535"
#define CAV_ERR_MSG_11 "Value must be in the range 0.001 <= value <= 2147483.647"
#define CAV_ERR_MSG_12 "Value cannot be other than 0 or 1."
#define CAV_ERR_MSG_13 "Data Directory '%s' is not present."

//===============================================================================
/* ----- || Start: System Errors (ErrCode: 100xxxx) || ------ */

// Fun: alloc()
#define CAV_ERR_1000001    "CavErr[1000001]: Failed to allocate memory of size %luB for 'DS: %s'"\
                              ".\nSysErr(%d): %s."
// Fun: realloc()
#define CAV_ERR_1000002    "CavErr[1000002]: Failed to re-allocate memory of new size %dB against "\
                              "old size %dB for 'DS: %s'.\nSysErr(%d): %s."
// Fun: getsockname() 
#define CAV_ERR_1000003    "CavErr[1000003]: Failed to get SocketName bounded by fd '%d'.\nSysErr(%d): %s."

#define CAV_ERR_1000004    "CavErr[1000004]: Failed to remove directory: %s. \nSysErr(%d): %s."
#define CAV_ERR_1000005    "CavErr[1000005]: Failed to create directory: %s. \nSysErr(%d): %s."
#define CAV_ERR_1000006    "CavErr[1000006]: Failed to open file: %s. \nSysErr(%d): %s."
#define CAV_ERR_1000007    "CavErr[1000007]: Failed in taking lock for file: %s. \nSysErr(%d): %s"
#define CAV_ERR_1000008    "CavErr[1000008]: Disk running out of space, so can not start new test\n"\
                           "Please release some space and try again."
#define CAV_ERR_1000009    "CavErr[1000009]: %s/webapps/logs do not have write permission to create test run directory\n"\
                           "Please correct permission attributes, and try again" 
#define CAV_ERR_1000010    "CavErr[1000010]: Failed to fetch interface address."
#define CAV_ERR_1000011    "CavErr[1000011]: Failed to copy string."
#define CAV_ERR_1000012    "CavErr[1000012]: Failed in rename file '%s' with file '%s'. \nSysErr(%d): %s"
#define CAV_ERR_1000013    "CavErr[1000013]: Failed to initialize process image for the process '%s'.\nSysErr(%d): %s."

// Fun: fcntl()
#define CAV_ERR_1000014    "CavErr[1000014]: Failed to set fd '%d' as no-blocking. SysErr(%d): %s."
                           

#define CAV_ERR_1000015    "CavErr[1000015]: Product type '%s' is not valid type of machine, "\
                              "please ensure that product type should be anyone of"\
                              "NS>NO|NS+NO|NS|NC|NDE|SM of machine in /home/cavisson/etc/cav.conf"

#define CAV_ERR_1000016    "CavErr[1000016]: File '%s' does not exists."

#define CAV_ERR_1000017    "CavErr[1000017]: File '%s' is of zero size."

#define CAV_ERR_1000018    "CavErr[1000018]: Failed to copy string '%s' into the memory."

#define CAV_ERR_1000019    "CavErr[1000019]: Failed to run '%s' command. \nSysErr(%d): %s."

#define CAV_ERR_1000020    "CavErr[1000020]: Failed to read '%s' file."

#define CAV_ERR_1000021    "CavErr[1000021]: Failed to close file: %s. \nSysErr(%d): %s."

#define CAV_ERR_1000022    "CavErr[1000022]: Failed to compile regular expression.\nSysErr: %s."

#define CAV_ERR_1000023    "CavErr[1000023]: Failed to compress script archive '%s'. \nSysErr(%d):%s."

#define CAV_ERR_1000024    "CavErr[1000024]: Failed to send file '%s' to controller '%s'"

#define CAV_ERR_1000025    "CavErr[1000025]: Failed to change directory to '%s'. \nSysErr(%d): %s."

#define CAV_ERR_1000026    "CavErr[1000026]: Failed to get user name. \nSysErr(%d): %s."

#define CAV_ERR_1000027    "CavErr[1000027]: Failed to get value of environment variable '%s'."

#define CAV_ERR_1000028    "CavErr[1000028]: '%s/logs/TR%d/%lld/ns_logs/' do not have write permission to create directory\n"\
                           "Please correct permission attributes, and try again." 

#define CAV_ERR_1000029    "CavErr[1000029]: '%s/%s' do not have write permission to create directory\n"\
                           "Please correct permission attributes, and try again." 

#define CAV_ERR_1000030    "CavErr[1000030]: Failed to close standard I/O stream which was opened for command '%s'. \nSysErr(%d): %s."

#define CAV_ERR_1000031    "CavErr[1000031]: Failed to open standard I/O stream for command '%s'. \nSysErr(%d): %s."

#define CAV_ERR_1000032    "CavErr[1000032]: Failed to write into file: %s. \nSysErr(%d): %s."

#define CAV_ERR_1000033    "CavErr[1000033]: Failed to copy '%s' into '%s'. \nSysErr(%d): %s."

#define CAV_ERR_1000034    "CavErr[1000034]: Failed to find path '%s'. \nSysErr(%d): %s."

#define CAV_ERR_1000035    "CavErr[1000035]: Failed to get current working directory. \nSysErr(%d): %s."

#define CAV_ERR_1000036    "CavErr[1000036]: Failed to get socket name. \nSysErr(%d): %s."

#define CAV_ERR_1000037    "CavErr[1000037]: Failed to start nsa_log_mgr for '%s'. \nSysErr(%d): %s."

#define CAV_ERR_1000038    "CavErr[1000038]: Failed to get file '%s' status. \nSysErr(%d): %s."

#define CAV_ERR_1000039    "CavErr[1000039]: Failed to create a new epoll instance. \nSysErr(%d): %s."

#define CAV_ERR_1000040    "CavErr[1000040]: Failed to join thread for DB tables creation."

#define CAV_ERR_1000041    "CavErr[1000041]: Failed to detach shared memory segment. \nSysErr(%d): %s."

#define CAV_ERR_1000042    "CavErr[1000042]: Failed to initialize an unnamed semaphore. \nSysErr(%d): %s."

#define CAV_ERR_1000043    "CavErr[1000043]: Failed to initialize db_aggregator. \nSysErr(%d): %s."

#define CAV_ERR_1000044    "CavErr[1000044]: Failed to create a new process using fork system call. \nSysErr(%d): %s."

#define CAV_ERR_1000045    "CavErr[1000045]: File '%s' is not a regular file."
/* ----- || End: System Errors (ErrCode: 100xxxx) || ------ */
//===============================================================================

/* ----- || Start: Parsing Related Errors (ErrCode: 101xxxx) || ------ */

//G_AUTO_FETCH_EMBEDDED:

#define CAV_ERR_1011001 "CavErr[1011001]: Inline resource fetching for all scenario groups cannot be specified for a page.\n"\
                        "In the scenario, it is configured for Page '%s'.\n\n"\
			"Message: %s\n" \
                        "To enable inline resource fetching for specific page(s), configure it for the scenario group using Group Based Settings -> Inline Resources."

#define CAV_ERR_1011002 "CavErr[1011002]: Page name '%s' configured for inline resource fetching for scenario group '%s' is not valid page in the script '%s'.\n\n"\
			"Message: %s\n"\
                        "Configure it by selecting correct page name using Group Based Settings -> Inline Resources."

#define CAV_ERR_1011003 "CavErr[1011003]: Incorrect Group name '%s' is provided for Inline resources fetching.\n\n"\
                        "Message: %s\n"\
                        "Configure it by selecting correct page name using Group Based Settings -> Inline Resources."

#define CAV_ERR_1011004 "CavErr[1011004]: Inline resource option is not configured properly.\n\n"\
			"Message: %s\n"\
                        "Reconfigure it using Group Based Settings -> Inline Resources."

//G_PAGE_THINK_TIME

#define CAV_ERR_1011005 "CavErr[1011005]: Think Time option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Reconfigure it using Group Based Settings -> Think Time."

#define CAV_ERR_1011006 "CavErr[1011006]: Think Time for all scenario groups cannot be specified for a page.\n"\
                        "In the scenario, it is configured for Page '%s'.\n\n"\
                        "Message: %s\n"\
                        "To enable Think Time for specific page(s), configure it for the scenario group using Group Based Settings -> Think Time."

#define CAV_ERR_1011007 "CavErr[1011007]: Page name '%s' configured for think time for scenario group '%s' is not valid page in the script '%s'.\n\n"\
                        "Message: %s\n"\
                        "Configure it by selecting correct page name for scenario group using Group Based Settings -> Think Time."

#define CAV_ERR_1011008 "CavErr[1011008]: Incorrect Group name '%s' is provided for Think time configuration.\n\n"\
                        "Message: %s\n"\
                        "Configure it using Group Based Settings -> Think Time."

//G_SESSION_PACING

#define CAV_ERR_1011009 "CavErr[1011009]: Session pacing option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Reconfigure it using Group Based Settings -> Session Pacing."

#define CAV_ERR_1011010 "CavErr[1011010]: Incorrect Group name '%s' is provided for Session Pacing configuration.\n\n"\
                        "Message: %s\n"\
                        "Configure it using Group Based Settings -> Session Pacing."

//G_INLINE_DELAY

#define CAV_ERR_1011011 "CavErr[1011011]: Inline delay option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Inline delay."

#define CAV_ERR_1011012 "CavErr[1011012]: Inline delay for all scenario groups cannot be specified for a page.\n"\
                        "In the scenario, it is configured for Page %s.\n\n"\
                        "Message: %s\n" \
                        "To enable inline delay for specific page(s), configure it for the scenario group using Group Based Settings -> Inline delay."

#define CAV_ERR_1011013 "CavErr[1011013]: Page name '%s' configured for inline delay for scenario group '%s' is not valid page in the script '%s'.\n\n"\
                        "Message: %s\n"\
                        "Configure it by selecting correct page name for scenario group using Group Based Settings -> Inline delay."

#define CAV_ERR_1011014 "CavErr[1011014]: Incorrect Group name '%s' is provided for inline delay configuration.\n\n"\
                        "Message: %s\n"\
                        "Configure it using Group Based Settings -> Inline delay."

#define CAV_ERR_1011015 "CavErr[1011015]: Maximum value must be greater than 4 times of median value(should be greater than minimum value).\n\n"\
                        "Message: %s\n"\
                        "Configure it using Group Based Settings -> Inline delay."

//G_VUSER_TRACE

#define CAV_ERR_1011016 "CavErr[1011016]: Virtual user trace option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Logs And Reports."

//G_TRACING

#define CAV_ERR_1011017 "CavErr[1011017]: Tracing option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Logs And Reports -> Tracing."

#define CAV_ERR_1011018 "CavErr[1011018]: Percentage value of session for trace session limit should be upto 2 decimal place.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Logs And Reports -> Tracing."

//G_REPORTING

#define CAV_ERR_1011019 "CavErr[1011019]: Reporting option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Logs And Reports -> Reporting."

//G_SHOW_RUNTIME_RUNLOGIC_PROGRESS

#define CAV_ERR_1011020 "CavErr[1011020]: Runtime runlogic progress option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Logs And Reports -> Advanced."

//PROGRESS_MSECS

#define CAV_ERR_1011021 "CavErr[1011021]: Sample Interval cannot be less than 1 sec.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports."

//DEBUG_TRACE

#define CAV_ERR_1011022 "CavErr[1011022]: %s configuration is supported only for scenario in which debug logging is enabled."

//ENABLE_SYNC_POINT

#define CAV_ERR_1011023 "CavErr[1011023]: Syncpoint enabling option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> SyncPoints." 

//AUTO_COOKIE

#define CAV_ERR_1011024 "CavErr[1011024]: Cookie option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> HTTP -> Advanced." 

//AUTO_REDIRECT

#define CAV_ERR_1011025 "CavErr[1011025]: Auto Redirect option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> HTTP -> Advanced."

//G_HTTP_MODE

#define CAV_ERR_1011026 "CavErr[1011026]: HTTP mode option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

//G_ENABLE_REFERER

#define CAV_ERR_1011027 "CavErr[1011027]: Referer enabling option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

//G_M3U8_SETTING

#define CAV_ERR_1011028 "CavErr[1011028]: HTTP Live Streaming(HLS) protocol option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

//G_HTTP_AUTH_NTLM

#define CAV_ERR_1011029 "CavErr[1011029]: HTTP authentication using 'NTLM' option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Authentication."

//G_HTTP_AUTH_KERB

#define CAV_ERR_1011030 "CavErr[1011030]: Kerberos Authentication option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Authentication."

//G_HTTP_CACHING

#define CAV_ERR_1011031 "CavErr[1011031]: HTTP caching option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Caching."

//G_HTTP_CACHE_TABLE_SIZE

#define CAV_ERR_1011032 "CavErr[1011032]: HTTP cache table size option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Caching."

//G_HTTP_CACHE_MASTER_TABLE

#define CAV_ERR_1011033 "CavErr[1011033]: HTTP master cache table option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Caching."

#define CAV_ERR_1011034 "CavErr[1011034]: Master Cache table name must be start with alpha and only alphanumeric and '_' are allowed.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Caching."

//G_MAX_CON_PER_VUSER

#define CAV_ERR_1011035 "CavErr[1011035]: Connection Setting option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Connection(s) Setting."

#define CAV_ERR_1011036 "CavErr[1011036]: Maximum connection per user can't be greater than 1024.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Connection(s) Setting."

#define CAV_ERR_1011297 "CavErr[1011297]: Invalid browser settings for %s.\n"\
			"Here maximum proxy connection per server is %d (HTTP 1.0),"\
			"which cannot be greater than maximum connection %d per virtual users."

#define CAV_ERR_1011298 "CavErr[1011298]: Invalid browser settings for %s.\n"\
                        "Here maximum proxy connection per server is %d (HTTP 1.1),"\
                        "which cannot be greater than maximum connection %d per virtual users."

//G_ENABLE_NETWORK_CACHE_STATS

#define CAV_ERR_1011037 "CavErr[1011037]: Network Cache Stats option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Advanced Settings."

//G_END_TX_NETCACHE

#define CAV_ERR_1011038 "CavErr[1011038]: Change transaction name option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Advanced Settings."

//G_SEND_NS_TX_HTTP_HEADER

#define CAV_ERR_1011039 "CavErr[1011039]: Option for sending transaction name in HTTP request header is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Advanced Settings."

//G_SAVE_LOCATION_HDR_ON_ALL_RSP_CODE

#define CAV_ERR_1011040 "CavErr[1011040]: Save location header value on any response code option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Advanced Settings."

//G_KA_PCT

#define CAV_ERR_1011041 "CavErr[1011041]: Keep alive option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Keep Alive Settings."

//G_NUM_KA

#define CAV_ERR_1011042 "CavErr[1011042]: Average Keep Alive Requests Per Connection option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [HTTP] -> [Keep Alive Settings] -> [Average Keep Alive Requests Per Connection]."

//G_KA_TIME_MODE

#define CAV_ERR_1011043 "CavErr[1011043]: Keep alive timeout option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Keep Alive Settings."


//G_PROXY_AUTH

#define CAV_ERR_1011044 "CavErr[1011044]: Proxy option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Proxy -> Proxy authentication credential."

//G_SSL_CERT_FILE_PATH

#define CAV_ERR_1011045 "CavErr[1011045]: SSL Certificate Setting option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL."

#define CAV_ERR_1011046 "CavErr[1011046]: SSL Certificate file should be present at path %s/cert.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL."

//G_SSL_KEY_FILE_PATH

#define CAV_ERR_1011047 "CavErr[1011047]: SSL Key file option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL."

#define CAV_ERR_1011048 "CavErr[1011048]: SSL Key file should be present at path %s/cert.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL."

//G_CIPHER_LIST

#define CAV_ERR_1011049 "CavErr[1011049]: Cipher Suites option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL." 

//G_AVG_SSL_REUSE

#define CAV_ERR_1011050 "CavErr[1011050]: SSL Reuse option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL -> Advanced Settings."

//G_SSL_CLEAN_CLOSE_ONLY

#define CAV_ERR_1011051 "CavErr[1011051]: SSL Clean Close option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL -> Advanced Settings."

//G_SSL_SETTINGS

#define CAV_ERR_1011052 "CavErr[1011052]: TLS SNI option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL -> Advanced Settings."

//G_SSL_RENEGOTIATION

#define CAV_ERR_1011053 "CavErr[1011053]: SSL Renegotiation option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL -> Advanced Settings."

//G_TLS_VERSION

#define CAV_ERR_1011054 "CavErr[1011054]: SSL Version option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> SSL -> Advanced Settings."


//URL_PDF

#define CAV_ERR_1011055 "CavErr[1011055]: URL Percentile Definition File option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."


//PAGE_PDF

#define CAV_ERR_1011056 "CavErr[1011056]: Page Percentile Definition File option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."

//SESSION_PDF

#define CAV_ERR_1011057 "CavErr[1011057]: Session Percentile Definition File option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."

//TRANSACTION_TIME_PDF

#define CAV_ERR_1011058 "CavErr[1011058]: Individual transaction Percentile Definition File option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."

//TRANSACTION_RESPONSE_PDF_PDF

#define CAV_ERR_1011059 "CavErr[1011059]: Transactions response Percentile Definition File option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."

//ENABLE_LOG_MGR

#define CAV_ERR_1011060 "CavErr[1011060]: Event logging using log manager option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Event Log."

//EVENT_LOG

#define CAV_ERR_1011061 "CavErr[1011061]: Event logging option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Event Log."

//SHOW_GROUP_DATA

#define CAV_ERR_1011062 "CavErr[1011062]: Scenario group based test Metric(s) option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Advanced Settings."

//ENABLE_PAGE_BASED_STATS

#define CAV_ERR_1011063 "CavErr[1011063]: Page Based Stats option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Advanced Settings."

//SHOW_SERVER_IP_DATA

#define CAV_ERR_1011064 "CavErr[1011064]: Server IP address based metrics option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Advanced Settings."

//SYNC_POINT_TIME_OUT

#define CAV_ERR_1011065 "CavErr[1011065]: SyncPoint timeout option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> SyncPoints."

//NUM_NVM

#define CAV_ERR_1011066 "CavErr[1011066]: Virtual Machine(s) setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced."

//PARTITION_SETTINGS

#define CAV_ERR_1011067 "CavErr[1011067]: Partition Setting option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced."

//AUTO_SCALE_CLEANUP_SETTING

#define CAV_ERR_1011068 "CavErr[1011068]: Cleanup indices on partition switch option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced."

//DISABLE_SCRIPT_COPY_TO_TR

#define CAV_ERR_1011069 "CavErr[1011069]: Copy scripts in test run option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced."

//OPTIMIZE_ETHER_FLOW

#define CAV_ERR_1011070 "CavErr[1011070]: Optimize ethernet packet flow option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced."

//NVM_DISTRIBUTION

#define CAV_ERR_1011071 "CavErr[1011071]: Users/Session distribution over NVMs option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Users/Session distribution over NVMs."

//VUSER_THREAD_POOL

#define CAV_ERR_1011072 "CavErr[1011072]: Virtual User Thread Pool option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

#define CAV_ERR_1011073 "CavErr[1011073]: Thread stack size value should be in between 32 and 8192.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

//CAV_EPOCH_YEAR

#define CAV_ERR_1011074 "CavErr[1011074]: Epoch year option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

//MAX_DYNAMIC_HOST

#define CAV_ERR_1011075 "CavErr[1011075]: Dynamic hosts option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

//ENABLE_FCS_SETTINGS

#define CAV_ERR_1011076 "CavErr[1011076]: Concurrent sessions setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

//SRC_PORT_MODE

#define CAV_ERR_1011077 "CavErr[1011077]: TCP Connection Port Selection Settings is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

//IO_VECTOR_SIZE

#define CAV_ERR_1011078 "CavErr[1011078]: IO Vector Settings is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

#define CAV_ERR_1011079 "CavErr[1011079]: Initial Vector Size for IO Vector settings cannot be less than 100.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

#define CAV_ERR_1011080 "CavErr[1011080]: Initial Vector Size for IO Vector settings cannot be greater than Maximum Vector Size.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

#define CAV_ERR_1011081 "CavErr[1011081]: Sum of 'Initial Vector Size' and 'Increment by size' value cannot be greater than 'Maximum Vector Size'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced -> Advanced Settings."

//G_RTE_SETTINGS

#define CAV_ERR_1011082 "CavErr[1011082]: Remote Terminal Emulation(RTE) option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> RTE Settings."

//G_JAVA_SCRIPT

#define CAV_ERR_1011083 "CavErr[1011083]: Java Script option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

//G_USER_CLEANUP_MSECS

#define CAV_ERR_1011084 "CavErr[1011084]: Old user cleanup time option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

//G_USE_DNS

#define CAV_ERR_1011085 "CavErr[1011085]: DNS Lookup option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

#define CAV_ERR_1011086 "CavErr[1011086]: Cache Time(TTL) for DNS Lookup option cannot be greater than 60 minutes.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

//G_DEBUG

#define CAV_ERR_1011087 "CavErr[1011087]: Debug Logging option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

//G_SESSION_RETRY

#define CAV_ERR_1011088 "CavErr[1011088]: Session Retry on Page Failure option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

//G_MODULEMASK

#define CAV_ERR_1011089 "CavErr[1011089]: Provided Modulemask '%s' is not a valid modulemask.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced -> Debug Logging."

//PAGE_AS_TRANSACTION

#define CAV_ERR_1011090 "CavErr[1011090]: Pages As Transaction option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Transactions."

//G_HTTP2_SETTINGS

#define CAV_ERR_1011091 "CavErr[1011091]: HTTP/2.0 protocol option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

#define CAV_ERR_1011092 "CavErr[1011092]: Maximum Concurrent Streams for HTTP/2.0 protocol should be greater than 1 and less than 500.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

#define CAV_ERR_1011093 "CavErr[1011093]: Initial window size for HTTP/2.0 protocol should be greater than 1(MB) and less than 2048(MB).\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

#define CAV_ERR_1011094 "CavErr[1011094]: Max frame size for HTTP/2.0 protocol should be greater than 16(KB) and less than 16384(KB).\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

#define CAV_ERR_1011095 "CavErr[1011095]: Header table size for HTTP/2.0 protocol should be greater than 4(KB) and less than 64(KB).\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP."

//PERCENTILE_REPORT

#define CAV_ERR_1011096 "CavErr[1011096]: Percentile Data option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."

#define CAV_ERR_1011097 "CavErr[1011097]: Percentile data interval should be multiple of Sample interval.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."


#define CAV_ERR_1011098 "CavErr[1011098]: Percentile mode of 'at the end of test' and 'for run phase only' are not allowed in continuos monitoring mode.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reportsi -> Percentile Data."

//G_PROXY_EXCEPTIONS

#define CAV_ERR_1011099 "CavErr[1011099]: Proxy exceptions options is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Proxy."

#define CAV_ERR_1011100 "CavErr[1011100]: Proxy address exception list must be separated by semicolon(;).\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Proxy."

#define CAV_ERR_1011101 "CavErr[1011101]: Proxy address exception list should have valid subnet value.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Proxy."

//G_MAX_URL_RETRIES

#define CAV_ERR_1011102 "CavErr[1011102]: Max retries on connection option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

#define CAV_ERR_1011103 "CavErr[1011103]: Number of Retries on connection cannot be less than 0 or greater than 255.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced."

//G_CONTINUE_ON_PAGE_ERROR

#define CAV_ERR_1011104 "CavErr[1011104]: Continue Session on Page Failure option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced -> Continue Session on Page Failure."

#define CAV_ERR_1011105 "CavErr[1011105]: Continue session on page failure for all scenario groups cannot be specified for a page.\n"\
                        "In the scenario, it is configured for Page '%s'.\n\n"\
                        "Message: %s\n" \
                        "To enable continue session on page failure for specific page(s), configure it for the scenario group using Group Based Settings -> Advanced -> Continue Session on Page Failure."

#define CAV_ERR_1011106 "CavErr[1011106]: Page name '%s' configured for continue session on page failure for scenario group '%s' is not valid page in the script '%s'.\n\n"\
                        "Message: %s\n"\
                        "Configure it by selecting correct page name using Group Based Settings -> Advanced -> Continue Session on Page Failure."

//G_BODY_ENCRYPTION

#define CAV_ERR_1011107 "CavErr[1011107]: Request Body Encryption option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced -> Request Body Encryption."

#define CAV_ERR_1011108 "CavErr[1011108]: Encryption Key and Encryption Initialization Vector(IVec) size should be as per encryption algorithm .\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Advanced -> Request Body Encryption."

//G_PAGE_RELOAD

#define CAV_ERR_1011109 "CavErr[1011109]: Page Reload option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Page Reload."

#define CAV_ERR_1011110 "CavErr[1011110]: Page name '%s' configured for page reload option for scenario group '%s' is not valid page in the script '%s'.\n\n"\
                        "Message: %s\n"\
                        "Configure it by selecting correct page name using Group Based Settings ->Page Reload."

//READER_RUN_MODE

#define CAV_ERR_1011111 "CavErr[1011111]: Generation of Drill down reports, Page dumps reports option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Advanced Settings."

#define CAV_ERR_1011112 "CavErr[1011112]: Maximum delay for generation of drill down reports, page dumps reports cannot be less than 10sec(s).\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Advanced Settings."

#define CAV_ERR_1011113 "CavErr[1011113]: In NDE continous monitoring mode only Online reporting is supported.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports -> Advanced Settings."

//G_HTTP_BODY_CHECKSUM_HEADER

#define CAV_ERR_1011114 "CavErr[1011114]: MD5 checksum HTTP header setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> HTTP -> Advanced Settings."

//URI_ENCODING

#define CAV_ERR_1011115 "CavErr[1011115]: URL Encoding option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> HTTP -> URL Encoding."

#define CAV_ERR_1011116 "CavErr[1011116]: Encoding of query parameter cannot be same as that of URL.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> HTTP -> URL Encoding."

//G_STATIC_HOST

#define CAV_ERR_1011117 "CavErr[1011117]: Static mapping option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Server Mapping -> Static Mapping."

#define CAV_ERR_1011118 "CavErr[1011118]: Duplicate entry of hostname is provided for static mapping option.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Server Mapping -> Static Mapping."

#define CAV_ERR_1011119 "CavErr[1011119]: Invalid IP entry is provided for static mapping option.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Server Mapping -> Static Mapping."

//G_SERVER_HOST
#define CAV_ERR_1011120 "CavErr[1011120]: Server mapping option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Server Mapping -> Mapping mode."

//G_SMTP_TIMEOUT_GREETING,G_SMTP_TIMEOUT_RCPT,G_SMTP_TIMEOUT_DATA_INIT,G_SMTP_TIMEOUT_DATA_BLOCK,G_SMTP_TIMEOUT_DATA_TERM,_SMTP_TIMEOUT

#define CAV_ERR_1011121 "CavErr[1011121]: SMTP Settings option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using SMTP Settings -> SMTP Timeout."
//NJVM_STD_ARGS

#define CAV_ERR_1011122 "CavErr[1011122]: JAVA settings option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings."

#define CAV_ERR_1011123 "CavErr[1011123]: Max heap size should not exceed 12288 MB of memory.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings ->  JVM Max Heap Size."

//NJVM_CUSTOM_ARGS

#define CAV_ERR_1011124 "CavErr[1011124]: JVM Custom Arguments option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings -> JVM Custom Arguments."

//NJVM_SYSTEM_CLASS_PATH

#define CAV_ERR_1011125 "CavErr[1011125]: System Java Class Path option is not configured properly.\n\n"\
                        "Message:  %s\n"\
                        "Configure it correctly using JAVA settings -> System Java Class Path."
//NJVM_JAVA_HOME

#define CAV_ERR_1011126 "CavErr[1011126]: Java Home Path option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings -> Java Home Path."

//NJVM_CLASS_PATH

#define CAV_ERR_1011127 "CavErr[1011127]: Scenario Specific Class Path  option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings -> Scenario Specific Class Path"

//NJVM_VUSER_THREAD_POOL

#define CAV_ERR_1011128 "CavErr[1011128]: JVM Virtual User Thread Pool option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings -> JVM Virtual User Thread Pool."

//NJVM_MSG_TIMEOUT

#define CAV_ERR_1011129 "CavErr[1011129]: Message Timeout option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings -> Message Timeout."

//NJVM_CONN_TIMEOUT

#define CAV_ERR_1011130 "CavErr[1011130]: Connection Timeout option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings -> Connection Timeout."

//NET_DIAGNOSTICS_SERVER

#define CAV_ERR_1011131 "CavErr[1011131]: NetDiagnostics Settings option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using NetDiagnostics Settings."

//G_ENABLE_CORRELATION_ID

#define CAV_ERR_1011132 "CavErr[1011132]: APM Integration for Correlation-ID option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using APM Integration -> General Settings."

#define CAV_ERR_1011133 "CavErr[1011133]: %s should not exceed 32 characters and only special character allowed is '-'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using APM Integration -> General Settings."

//G_ENABLE_DT

#define CAV_ERR_1011134 "CavErr[1011134]: APM Integration for dynaTrace option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using APM Integration -> dynaTrace."

//HEALTH_MONITOR_DISK_FREE

#define CAV_ERR_1011135 "CavErr[1011135]: System Health Setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Health Check Settings ->  System Health ->Disk"

#define CAV_ERR_1011136 "CavErr[1011136]: Each of Threshold Free Space i.e. Critical, Major, Minor should not be greater than 1000.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Health Check Settings -> System Health ->Disk"

//HEALTH_MONITOR_INODE_FREE

#define CAV_ERR_1011137 "CavErr[1011137]: System Health Setting is not properly configured.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Health Check Settings -> System Health -> Inode."

#define CAV_ERR_1011138 "CavErr[1011138]: Each of Threshold Free Space i.e. Critical, Major, Minor should not be greater than 1000.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Health Check Settings -> System Health ->Inode."

//HEALTH_MONITOR

#define CAV_ERR_1011139 "CavErr[1011139]: Connection limit option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Health Check Settings -> Advanced Settings."

#define CAV_ERR_1011140 "CavErr[1011140]: Connection limit value should not be greater than 65000\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Health Check Settings -> Advanced Settings."

//HIERARCHICAL_VIEW

#define CAV_ERR_1011141 "CavErr[1011141]: Topology name option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Topology name."


//DISABLE_NS_MONITORS,DISABLE_NO_MONITORS

#define CAV_ERR_1011142 "CavErr[1011142]: The exclude list for enabling NetStorm monitors/NetOcean monitors is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Enable NetStorm monitors/NetOcean monitors."

//LPS_SERVER

#define CAV_ERR_1011143 "CavErr[1011143]: Log Processing System(LPS) option is not configured properly.\n\n"\
		        "Message: %s\n"\
		        "Configure it correctly using Monitors -> Log Processing System(LPS)"

//ENABLE_OUTBOUND_CONNECTION

#define CAV_ERR_1011144 "CavErr[1011144]: Enable Outbound Connection option is not configured properly.\n\n"\
		        "Message: %s\n"\
		        "Configure it correctly using Log Processing System(LPS) -> Enable Outbound Connection"

//MONITOR_PROFILE

#define CAV_ERR_1011145 "CavErr[1011145]: Scenario Associated Monitor Group(s) option is not configured properly.\n\n"\
 		        "Message: %s\n"\
		        "Configure it correctly using Log Processing System(LPS) ->Scenario Associated Monitor Group(s)"

//CONTINUE_ON_MONITOR_ERROR
  
#define CAV_ERR_1011146 "CavErr[1011146]: Continue on Monitor/dynamic monitor/pre test check monitors failure\n\n"\
   		        "Message: %s\n"\
		        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_CMON_AGENT
 
#define CAV_ERR_1011147 "CavErr[1011147]: CavMonAgent Process configuration option is not configured properly\n\n"\
		        "Message: %s\n"\
		        "Configure it correctly using Monitors -> Advance Settings." 

//PRE_TEST_CHECK_TIMEOUT

#define CAV_ERR_1011148 "CavErr[1011048]: Check monitors/Batch Job pre test timeout option is not configured properly\n\n"\
		        "Message: %s\n"\
		        "Configure it correctly using Monitors -> Advance Settings."


//POST_TEST_CHECK_TIMEOUT

#define CAV_ERR_1011149 "CavErr[1011149]: Check monitors/Batch Job post test timeout option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_AUTO_SERVER_SIGNATURE

#define CAV_ERR_1011150 "CavErr[1011150]: Server signature option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_DATA_CONN_HB
 
#define CAV_ERR_1011151 "CavErr[1011151]: Heartbeat on monitors data connection option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_MONITOR_DR

#define CAV_ERR_1011152 "CavErr[1011152]: Monitors connection option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_CHECK_MONITOR_DR

#define CAV_ERR_1011153 "CavErr[1011153]: Check monitors connection option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_JAVA_PROCESS_SERVER_SIGNATURE

#define CAV_ERR_1011154 "CavErr[1011054]: Server signature option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//DYNAMIC_VECTOR_TIMEOUT

#define CAV_ERR_1011155 "CavErr[1011155]: Dynamic vector timeout option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//DYNAMIC_VECTOR_MONITOR_RETRY_COUNT

#define CAV_ERR_1011156 "CavErr[1011156]: Dynamic vector monitor option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//COHERENCE_NID_TABLE_SIZE

#define CAV_ERR_1011157 "CavErr[1011157]: Coherence monitor NodeID option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//SKIP_UNKNOWN_BREADCRUMB

#define CAV_ERR_1011158 "CavErr[1011158]: Skipping vector option[either tier/server/instance] is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_HEROKU_MONITOR

#define CAV_ERR_1011159 "CavErr[1011159]: Heroku Monitors option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//ENABLE_HML_GROUPS

#define CAV_ERR_1011160 "CavErr[1011160]: HML Group feature option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Advance Settings."

//SAVE_NVM_FILE_PARAM_VAL

#define CAV_ERR_1011161 "CavErr[1011161]: Save nvm file param option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Logs And Reports."

//ENABLE_NO_MONITORS

#define CAV_ERR_1011162 "CavErr[1011162]: NetOcean monitor enabling option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Enable NetOcean monitors."

//ENABLE_NS_MONITORS

#define CAV_ERR_1011163 "CavErr[1011163]: NetStorm monitor enabling option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Monitors -> Enable NetStorm monitors."

//G_FIRST_SESSION_PACING

#define CAV_ERR_1011164 "CavErr[1011164]: Delay introduction before first session option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Session Pacing."

//G_NEW_USER_ON_SESSION

#define CAV_ERR_1011165 "CavErr[1011165]: New user simulation option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Session Pacing."
//G_RBU

#define CAV_ERR_1011166 "CavErr[1011166]: Run virtual user as real browser user option is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Advance Settings."

#define CAV_ERR_1011167 "CavErr[1011167]: Scenario name cannot have a comma(,) in it."

//NS_GENERATOR
#define CAV_ERR_1011168 "CavErr[1011168]: Generator is not configured properly\n\n" \
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Generators]"

//PROF_PCT_MODE
#define CAV_ERR_1011169 "CavErr[1011169]: Distribution Mode is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Distribution Mode]"

#define CAV_ERR_1011170 "CavErr[1011170]: Incorrect format of the scenario file. must be <TestCaseName>.conf"

#define CAV_ERR_1011171 "CavErr[1011171]: <Project>/<Subproject>/<ScenarioName>' is not given in proper format"

//STYPE
#define CAV_ERR_1011172 "CavErr[1011172]: Scenario Type option is not configured properly.\n"\
                        "Message: %s\n"\
                        "Configure it using Schedule Setting -> Scenario Type" 

//SCENARIO_SETTING_PROFILE part of CavErr CAV_ERR_1011177
#define CAV_ERR_1011174 "CavErr[1011174]: Scenario Setting Profile is not configure properly.\n\n"\
                        "Message: Scenario profile file '%s' does not Exists.\n"\
                        "Re-Configure it using -\n\t[ScenarioGUI] -> [Scenario Profile]"

//TARGET_RATE
#define CAV_ERR_1011173 "CavErr[1011173]: Target rate is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-configure it using -\n\t"\
                        "[Schedule Setting] -> [Scenario Type(FSR)]\n\t"\
                        "[Schedule Setting] -> [Distribution Mode(PCT)]\n\t"\
                        "[Schedule Setting] -> [Global Schedule > Total(Session/Minute)]"

//SCHEDULE_TYPE
#define CAV_ERR_1011175 "CavErr[1011175]: Schedule Type option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configured it using -\n\t[Schedule Setting] -> [Schedule Type]"

//SCHEDULE_BY
#define CAV_ERR_1011176 "CavErr[1011176]: Schedule By option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Schedule By]"

//SCENARIO_SETTING_PROFILE 
#define CAV_ERR_1011177 "CavErr[1011177]: Scenario Setting Profile is not configure properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[ScenarioGUI] -> [Scenario Profile]"

//ENABLE_FCS_SETTING
#define CAV_ERR_1011178 "CavErr[1011178]: Maximum number of concurrent sessions is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-conigure it using -\n\t[Global Setting] -> [Advance] -> [Advance Setting] -> " \
                        "[Maximum Number of Concurrent Sessions]"

//ENABLE_NS_CHROME

#define CAV_ERR_1011179 "CavErr[1011179]: Browser enabling option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Browser]"

//G_RBU_THROTTLING_SETTING

#define CAV_ERR_1011180 "CavErr[1011180]: Network Throttle option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Lighthouse Settings]"

//G_RBU_CAPTURE_CLIPS

#define CAV_ERR_1011181 "CavErr[1011181]: Video clips of resolution option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Video Clip Settings]"

#define CAV_ERR_1011182 "CavErr[1011182]: Clip capturing frequency should not be less than 5(ms) or greater than 10000(ms).\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Video Clip Settings]"

//DOM Threshold should be passed for %s
//Onload Threshold should be passed for %s
#define CAV_ERR_1011183 "CavErr[1011183]: %s should be greater than 0 or less than 120000(ms).\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Video Clip Settings]"

#define CAV_ERR_1011184 "CavErr[1011184]: %s should not be less than 0 or greater than 100(ms).\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Video Clip Settings]"

//G_RBU_SETTINGS

#define CAV_ERR_1011185 "CavErr[1011185]: Capture rendering metrice option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Video Clip Settings]"

#define CAV_ERR_1011186 "CavErr[1011186]: Capture rendering time should not be less than 100(ms) or greater than 10000(ms).\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [Video Clip Settings]"

//G_RBU_CACHE_DOMAIN

#define CAV_ERR_1011187 "CavErr[1011187]: DN offload option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Real Browser User Settings] -> [CDN Offload Settings]"

//G_RBU_USER_AGENT

#define CAV_ERR_1011188 "CavErr[1011188]: User Agent string option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [User Agent Settings]"

//G_RBU_CAPTURE_PERFORMANCE_TRACE

#define CAV_ERR_1011189 "CavErr[1011189]: Enable JSProfiler option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ JS Profiler Settings]"
//G_RBU_CACHE_SETTING

#define CAV_ERR_1011190 "CavErr[1011190]: Browser caching option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_ADD_HEADER

#define CAV_ERR_1011191 "CavErr[1011191]: HTTP Headers option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_CLEAN_UP_PROF_ON_SESSION_START

#define CAV_ERR_1011192 "CavErr[1011192]: Browser profile setting option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"
//G_RBU_HAR_SETTING

#define CAV_ERR_1011193 "CavErr[1011193]: HTTP response body option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_ALERT_SETTING

#define CAV_ERR_1011194 "CavErr[1011194]: Alert due to script runtime error option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_SCREEN_SIZE_SIM

#define CAV_ERR_1011195 "CavErr[1011195]: Browser screen size simulation option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_ENABLE_TTI_MATRIX

#define CAV_ERR_1011196 "CavErr[1011196]: Time To Interact(TTI) metrics option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_RM_PROF_SUB_DIR

#define CAV_ERR_1011197 "CavErr[1011197]: Removal of specified directory option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"
//G_RBU_HAR_TIMEOUT

#define CAV_ERR_1011198 "CavErr[1011198]: Timeout for capturing page option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

#define CAV_ERR_1011199 "CavErr[1011199]: Value for timeout must be greater than or equal to 65.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_BLOCK_URL_LIST

#define CAV_ERR_1011200 "CavErr[1011200]: Block URL option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_DOMAIN_IGNORE_LIST

#define CAV_ERR_1011201 "CavErr[1011201]: Exclude domain option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//RBU_DOMAIN_STAT

#define CAV_ERR_1011202 "CavErr[1011202]: Domain based performance metric option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"
//RBU_POST_PROC

#define CAV_ERR_1011203 "CavErr[1011203]: Prefix to add in HAR option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"
//G_RBU_ADD_ND_FPI

#define CAV_ERR_1011204 "CavErr[1011204]: NetDiagnostics integration option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//RBU_MARK_MEASURE_MATRIX

#define CAV_ERR_1011205 "CavErr[1011205]: Mark and Measure option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"
//RBU_ENABLE_CSV

#define CAV_ERR_1011206 "CavErr[1011206]: Creation of per page CSV files option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//RBU_BROWSER_COM_SETTINGS

#define CAV_ERR_1011207 "CavErr[1011207]: Time interval for connection with browser extension option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

#define CAV_ERR_1011208 "CavErr[1011208]: Frequency value should vary from 1 to 256.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

#define CAV_ERR_1011209 "CavErr[1011209]: Time interval should be greater than 3000(ms) or less than 120000(ms) \n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"
//RBU_ENABLE_DUMMY_PAGE

#define CAV_ERR_1011210 "CavErr[1011210]: Default webpage hit option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

//G_RBU_PAGE_LOADED_TIMEOUT

#define CAV_ERR_1011211 "CavErr[1011211]: Sleep interval option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

#define CAV_ERR_1011212 "CavErr[1011212]: Timeout should be positive value and must be greater than 500 ms.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

#define CAV_ERR_1011213 "CavErr[1011213]: Phase interval should be integer value and must be greater than 2000 ms.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

#define CAV_ERR_1011214 "CavErr[1011214]: Source IP selection mode is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Group Based Settings] -> [Source IP selection mode]"

//WAN_ENV

#define CAV_ERR_1011215 "CavErr[1011215]: Network Simulation option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Global Settings] -> [Network Simulation]"
//ADVERSE_FACTOR

#define CAV_ERR_1011216 "CavErr[1011216]: Adverse Factor Latency option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Global Settings] -> [Adverse Factor Latency]"
//WAN_JITTER

#define CAV_ERR_1011217 "CavErr[1011217]: Network Jitter option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Global Settings] -> [Network Jitter]"

//G_MAX_PAGES_PER_TX

#define CAV_ERR_1011218 "CavErr[1011218]: Maximum page instances option for one transaction is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Group Based Settings] -> [Transactions]"

#define CAV_ERR_1011219 "CavErr[1011219]: Maximum page instances allowed in one transaction should not be less than 1 or greater than 64000.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Transactions]"

//ENABLE_TRANSACTION_CUMULATIVE_GRAPHS

#define CAV_ERR_1011220 "CavErr[1011220]: Transaction cummulative metric setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Global Settings] -> [Transactions]"

//MAX_USERS

#define CAV_ERR_1011221 "CavErr[1011221]: Limit Maximum Users is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Schedule Setting] -> [Scenario Type(Fix Session Rate)] -> [Schedule Advance Settings] -> [Limit Maximum Users]"

//URI_ENCODING
#define CAV_ERR_1011222 "CavErr[1011222]: Only special characters[!@#$%^&*()-=_+`~[{]}|;:'\",\\<.>/?*] can be encoded.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> HTTP -> URL Encoding."

//NJVM_STD_ARGS
#define CAV_ERR_1011223 "CavErr[1011223]: Min heap size should be more than 1 Mb of memory.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using JAVA settings ->  JVM Max Heap Size."

//SHOW_IP_DATA

#define CAV_ERR_1011224 "CavErr[1011224]: IP settings option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> IP Settings."

//G_USE_SRC_IP

#define CAV_ERR_1011225 "CavErr[1011225]: Source IP settings option is not configured properly.\n\n"\
                      "Message: %s\n"\
                      "Configure it correctly using Group Based Settings -> IP Settings."

//NJVM_SYSTEM_CLASS_PATH

#define CAV_ERR_1011226 "CavErr[1011226]: System Java Class Path should be an absolute path.\n\n"\
                        "Message:  %s\n"\
                        "Configure it correctly using JAVA settings -> System Java Class Path."

//G_ENABLE_CORRELATION_ID

#define CAV_ERR_1011227 "CavErr[1011227]: Source ID(SI) option should not exceed 32 characters and special characters allowed for this option are [-+>].\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using APM Integration -> General Settings."

#define CAV_ERR_1011228 "CavErr[1011228]: Source ID(SI) option value is missing.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using APM Integration -> %s."

//G_INLINE_EXCLUDE_DOMAIN_PATTERN G_INLINE_EXCLUDE_URL_PATTERN G_INLINE_INCLUDE_DOMAIN_PATTERN G_INLINE_INCLUDE_URL_PATTERN

#define CAV_ERR_1011229 "CavErr[1011229]: Inline Patteren for either Domain or URL option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [Inline Resources] -> [Inline Pattern]"

//G_USE_SAME_NETID_SRC

#define CAV_ERR_1011230 "CavErr[1011230]: Source IP selection mode is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [IP Settings] -> [Source IP Selection mode]"

//G_TRACING_LIMIT
#define CAV_ERR_1011231 "CavErr[1011231]: Trace limit option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [Logs And Reports] -> [Tracing]"

//G_INLINE_MIN_CON_REUSE_DELAY

#define CAV_ERR_1011232  "CavErr[1011232]: Minimum connection reuse delay option is not configured properly.\n\n"\
                         "Message: %s\n"\
                         "Configure it correctly using [Group Based Settings] -> [Inline delay] -> [Advanced Setting]"

//G_SCRIPT_MODE

#define CAV_ERR_1011233 "CavErr[1011233]: Run script mode option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [Virtual user] -> [Run script mode]"

//ULOCATION

#define CAV_ERR_1011234 "CavErr[1011234]: Location setting option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Location]"

//UACCESS

#define CAV_ERR_1011235 "CavErr[1011235]: User Network Access option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Access]"

//UBROWSER

#define CAV_ERR_1011236 "CavErr[1011236]: User Browser Type option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Browser]"

//UPSCREEN_SIZE

#define CAV_ERR_1011237 "CavErr[1011237]: User Profile Default Screen Size Setting option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Screen size]"

//UPLOCATION

#define CAV_ERR_1011238 "CavErr[1011238]: User location option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Profile]"

//UPACCESS

#define CAV_ERR_1011239 "CavErr[1011239]: User Access option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Profile]"

//UPBROWSER

#define CAV_ERR_1011240 "CavErr[1011240]: User Browser option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Profile]"

//STYPE part of CavErr CAV_ERR_1011172
#define CAV_ERR_1011241 "CavErr[1011241]: Scenario Type option is not configured properly.\n"\
                        "Message: Scenario Type '%s' is not a valid scenario type.\n"\
                        "Configure it using Schedule Setting -> Scenario Type" 

//NS_GENERATOR
#define CAV_ERR_1011242 "CavErr[1011242]: Generator is not configured properly\n\n" \
                        "Message: Generator '%s' is having duplicate entry in scenario.\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Generators]"

#define CAV_ERR_1011243 "CavErr[1011243]: Invalid number of arguments provided. Atleast 2 arguments required with %s"
#define CAV_ERR_1011244 "CavErr[1011244]: Scenario Group name '%s' length should be less than or equal to 32"
#define CAV_ERR_1011245 "CavErr[1011245]: Scenario group name should contain alphanumeric character, '_' '-' ',' '.' and first character should be an alphabet."

//G_IP_VERSION_MODE

#define CAV_ERR_1011246 "CavErr[1011246]: IP Address Version Mode setting is not configured properly.\n"\
                        "Message: %s\n"\
                        "Reconfigure it correctly using Group Based Settings -> IP Settings -> Advanced Settings"

//G_OVERRIDE_RECORDED_THINK_TIME

//CHECK_GENERATOR_HEALTH
#define CAV_ERR_1011247 "CavErr[1011247]: Check generator health settings is not configured properly.\n"\
                        "Message: %s\n"\
                        "Reconfigure it correctly using -\n\t[Health Check Settings] -> [System Health] -> [Generator Health]"

//RBU
#define CAV_ERR_1011248 "CavErr[1011248]: Provided user Agent string(%s) is of length(%d). It cannot be more than 1024 characters.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using [Real Browser User Settings] -> [User Agent Settings]"

//G_RBU
#define CAV_ERR_1011249 "CavErr[1011249]: Lighthouse Reporting is only supported for Chrome Browser.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Real Browser User Settings] -> [Enable Lighthouse Reporting]"

#define CAV_ERR_1011250 "CavErr[1011250]: Lighthouse Reporting is only supported for Chrome Browser version 68 or above.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Real Browser User Settings] -> [Enable Lighthouse Reporting]"

//SYNC_POINT
#define CAV_ERR_1011251 "CavErr[1011251]: SyncPoints setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011252 "CavErr[1011252]: Participating VUsers value should be in float.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011253 "CavErr[1011253]: 'Release Type' option value and 'Absolute Time/Time Period' option value should be 'NA' as 'Release Mode' option is set to 'Manual'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011254 "CavErr[1011254]: 'Absolute Time/Time Period' option value should be 'NA' as 'Release Type' option is set to 'Target VUsers'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011255 "CavErr[1011255]: 'Time Period' option value '%s' is not provided in proper time format(HH:MM:SS) for Release Type 'Period' option.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011256 "CavErr[1011256]: 'Absolute Time' option value '%s' should be greater than current time.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011257 "CavErr[1011257]: 'Release within Duration/Release VUsers At Rate' option value should be 'NA' as 'Release Schedule' option is set to 'Immediate'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011258 "CavErr[1011258]: 'Release VUsers At Rate' value cannot be less than 0.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011259 "CavErr[1011259]: SyncPoint type and SyncPoint name combination cannot be same for all scenario groups and a specific group.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011260 "CavErr[1011260]: 'Release Target VUsers' option can either have numeric value or '*'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011261 "CavErr[1011261]: '*' value for 'Release Target VUsers' option can only come at end position.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011262 "CavErr[1011262]: Participating VUsers option cannot have a value greater than 100.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

//CONTINUE_TEST_ON_GEN_FAILURE

#define CAV_ERR_1011263 "CavErr[1011263]: Continue test on generator failures option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [Advanced] -> [Advanced Settings] -> [Continue test on generator failures]."

#define CAV_ERR_1011264 "CavErr[1011264]: Generator to Controller connection establishment timeout value cannot be less than 0 secs or greater than 60 mins.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [Advanced] -> [Advanced Settings] -> [Continue test on generator failures]."

//HOST_TLS_VERSION

#define CAV_ERR_1011265 "CavErr[1011265]: Override SSL version for specified hosts option is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SSL]."

//SyncPoint
#define CAV_ERR_1011266 "CavErr[1011266]: 'Frequency' option value '%s' is not provided in proper time format(HH:MM:SS).\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011267 "CavErr[1011267]: 'Duration in step' option value is not provided correctly. It should be in format [HH:MM:SS,Percentage of users]\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

#define CAV_ERR_1011268 "CavErr[1011268]: Sum of percentage of all step users to be removed from SyncPoint for Release schedule - 'Duration in Step' should not be greater than 100.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Global Settings] -> [SyncPoints]."

//NUM_USERS
#define CAV_ERR_1011269 "CavErr[1011269]: Number of Virtual Users(NUM_USERS) for Scenario type 'Fix Concurrent User' and Distribution Mode 'By Percentage' is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-configure it using -\n\t"\
                        "[Schedule Setting] -> [Scenario Type(Fix Concurrent Users)]\n\t"\
                        "[Schedule Setting] -> [Distribution Mode(PCT)]\n\t"\
                        "[Schedule Setting] -> [Global Schedule > Total(Session/Minute)]"

#define CAV_ERR_1011270 "CavErr[1011270]: Number of Virtual Users(NUM_USERS) for Scenario type 'Fix Mean Users' and Distribution Mode 'By Percentage' is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-configure it using -\n\t"\
                        "[Schedule Setting] -> [Scenario Type(Fix Mean Users)]\n\t"\
                        "[Schedule Setting] -> [Mean Concurrent User]"

#define CAV_ERR_1011271 "CavErr[1011271]: Failed to merge and sort scenario file '%s'\n"\
                        "Message: %s"

#define CAV_ERR_1011272 "CavErr[1011135]: System Health Setting is not configured properly.\n\n"\
                        "Message: Filesystem %s not a valid Filesystem\n"\
                        "Configure it correctly using Health Check Settings ->  System Health ->Disk"

#define CAV_ERR_1011273 "CavErr[1011137]: System Health Setting is not properly configured.\n\n"\
                        "Message: Filesystem %s not a valid Filesystem\n"\
                        "Configure it correctly using Health Check Settings -> System Health -> Inode."

//G_CLICK_AWAY
#define CAV_ERR_1011274 "CavErr[1011274]: Next Page Name value cannot be 'ALL' for click away setting.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Click Away]"

#define CAV_ERR_1011275 "CavErr[1011275]: For group name 'ALL' and page name 'ALL' next page name value can only be '-1' for click away setting.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Click Away]"

#define CAV_ERR_1011276 "CavErr[1011276]: Incorrect scenario group name '%s' provided for click away setting.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Click Away]"

#define CAV_ERR_1011277 "CavErr[1011277]: Page name '%s' specified for next page name option in click away setting is not present in scenario group '%s' script.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Click Away]"

#define CAV_ERR_1011278 "CavErr[1011278]: Page name '%s' specified in click away setting is not a valid page name in scenario group '%s'.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Click Away]"

#define CAV_ERR_1011279 "CavErr[1011279]: Ramp down setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Ramp Down Settings]"

//G_KA_TIME
#define CAV_ERR_1011280 "CavErr[1011280]: Keep Alive Timeout setting is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [HTTP] -> [Keep Alive Settings] -> [Keep Alive Timeout]"

#define CAV_ERR_1011281 "CavErr[1011281]: Unknown browser attribute '%s' is provided for User Browser option.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [User Profile] -> [Profile]"

#define CAV_ERR_1011282 "CavErr[1011282]: JMeter JVM Setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [JMeter Settings]"

#define CAV_ERR_1011283 "CavErr[1011283]: Min Heap size value cannot be less than 512(MB)\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [JMeter Settings]"

#define CAV_ERR_1011284 "CavErr[1011284]: Keyword '%s' has been deprecated from NS Release 4.1.7 Build 31, instead of this, use keyword: G_RBU_SETTINGS <grp-name> <mode(0/100).\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Real Browser User Settings] -> [Video Clip Settings]"

#define CAV_ERR_1011285 "CavErr[1011285]: Manual proxy settings options is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Group Based Settings -> Proxy -> Use manual proxy settings."

//G_PAGE_RELOAD
#define CAV_ERR_1011286 "CavErr[1011286]: Incorrect scenario group name '%s' provided for page reload setting.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [Page Reload]"

#define CAV_ERR_1011287 "CavErr[1011287]: Keyword '%s' has been deprecated from NS Release 4.1.7 Build 31, instead of this, use keyword: G_RBU_ENABLE_TTI_MATRIX <grp> <mode>.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -[Real Browser User Settings] -> [ Advanced Settings]"

#define CAV_ERR_1011288 "CavErr[1011288]: Domain list can have maximum of 50 domains for 'Enable CDN offload for additional domain(s)' option.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Real Browser User Settings] -> [Content Delivery Network (CDN) Offload Settings]"

#define CAV_ERR_1011289 "CavErr[1011289]: Click away setting is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Group Based Settings] -> [Click Away]"

#define CAV_ERR_1011290 "CavErr[1011290]: Script '%s' is used in both RBU and non-RBU mode"

#define CAV_ERR_1011291 "CavErr[1011291]: Syncpoint 'Release policy' cannot be modified as Syncpoint '%s' is in Active state."

//NH_SCENARIO
#define CAV_ERR_1011292 "CavErr[1011292]: NH_SCENARIO_INTEGRATION keyword is not configured properly\n\n"\
                        "Message: Wrong NH_SCENARIO_INTEGRATION entry in %s\n"\
                        "Re-Configure it using - [Admin] -> [NetHavoc] -> [NetHavoc scenario configuration]"

#define CAV_ERR_1011293 "CavErr[1011293]: PERCENTILE_REPORT mode 1 (generate percentile report for run phase only) is not supported "\
                        "with phase duration and mode sessions"

#define CAV_ERR_1011294 "CavErr[1011294]: PERCENTILE_REPORT mode 1 (generate percentile report for run phase only) is only supported "\
                        "in Schedule type Simple and Schedule by Scenario"

#define CAV_ERR_1011295 "CavErr[1011295]: Test Monitor Config is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using Global Settings -> Advanced"

#define CAV_ERR_1011296 "CavErr[1011296]: NH_SCENARIO keyword is not configured properly\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using - [Schedule Setting] -> [Global Schedule] -> [Nethavoc Scenario Settings]"

//SGRP
#define CAV_ERR_1011300 "CavErr[1011300]: Scenario Group is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group]" 

#define CAV_ERR_1011301 "CavErr[1011301]: Scenario Group is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Group Name]"

#define CAV_ERR_1011302 "CavErr[1011302]: Scenario Group is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Generator Name]"

#define CAV_ERR_1011303 "CavErr[1011303]: Scenario Group is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Scenario Type]"

#define CAV_ERR_1011304 "CavErr[1011304]: Scenario Group is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [User Profile]"

#define CAV_ERR_1011305 "CavErr[1011305]: Scenario Group is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Script Name]"

#define CAV_ERR_1011306 "CavErr[1011306]: Scenario Group is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Users/Session/Percentage]"

#define CAV_ERR_1011307 "CavErr[1011307]: Scenario Group is not configured properly.\n\n"\
                        "Message: Generator '%s' does not exist in used generator list or Discarded.\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Generator]"

//SCHEDULE
#define CAV_ERR_1011308 "CavErr[1011308]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule]"

#define CAV_ERR_1011309 "CavErr[1011309]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Add Group]"

//START PHASE
#define CAV_ERR_1011310 "CavErr[1011310]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Start]"

#define CAV_ERR_1011311 "CavErr[1011311]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Start] -> [After]"

#define CAV_ERR_1011312 "CavErr[1011312]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Start] -> [After Time(HH:MM:SS)]"

#define CAV_ERR_1011313 "CavErr[1011313]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Start] -> [After Group]"

#define CAV_ERR_1011314 "CavErr[1011314]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up]"

#define CAV_ERR_1011315 "CavErr[1011315]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up] -> [Step]"

#define CAV_ERR_1011316 "CavErr[1011316]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up] -> [Rate]"

#define CAV_ERR_1011317 "CavErr[1011317]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up] -> [Time]"

#define CAV_ERR_1011318 "CavErr[1011318]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up] -> [Time Session]"

#define CAV_ERR_1011319 "CavErr[1011319]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Duration]"

#define CAV_ERR_1011320 "CavErr[1011320]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Duration] -> [Session]"

#define CAV_ERR_1011321 "CavErr[1011321]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Duration] -> [Time]"

#define CAV_ERR_1011322 "CavErr[1011322]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Stabilization]"

#define CAV_ERR_1011323 "CavErr[1011323]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Stabilization] -> [Time]" 

#define CAV_ERR_1011324 "CavErr[1011324]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Down]"

#define CAV_ERR_1011325 "CavErr[1011325]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Down] -> [Step]"

#define CAV_ERR_1011326 "CavErr[1011326]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Down] -> [Time]"

#define CAV_ERR_1011327 "CavErr[1011327]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Down] -> [Time]"

#define CAV_ERR_1011328 "CavErr[1011328]: Global Schedule is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Down] -> [Time Session]"
//SGRP  
#define CAV_ERR_1011329 "CavErr[1011329]: Scenario Group is not configured properly.\n\n"\
                        "Message: In group '%s' one or more generators are provided without distribution percentage\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Generator Name]"

//SGRP  
#define CAV_ERR_1011330 "CavErr[1011330]: Scenario Group is not configured properly.\n\n"\
                        "Message: Scenario group '%s', generator percentage should add up to total 100 to run Netcloud test\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Generator Name]"

#define CAV_ERR_1011331 "CavErr[1011331]: Scenario Group is not configured properly.\n\n"\
                        "Message: Invalid scenario Type '%s'. ScenType can only be 'NA' in sceario group, if Scenario Type is not Mixed Mode\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Scenario Type]"

#define CAV_ERR_1011332 "CavErr[1011332]: Scenario Group is not configured properly.\n\n"\
                        "Message: Invalid Scenario Type '%s', it can only be NA, FIX_CONCURRENT_USERS or FIX_SESSION_RATE.\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Scenario Type]"

#define CAV_ERR_1011333 "CavErr[1011333]: Scenario Group is not configured properly.\n\n"\
                        "Message: Number of %s (%.02f) cannot be less than total number of generators (%d) used in "\
                        "group (%s), please increase number of %s and re-run the test\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Users/Session/Percentage]"

#define CAV_ERR_1011334 "CavErr[1011334]: Scenario Group is not configured properly.\n\n"\
                        "Message: Invalid format of project and sub-project '%s' please provide correct format '<proj>/<sub-proj>'\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Script Name]"

#define CAV_ERR_1011335 "CavErr[1011335]: Scenario Group is not configured properly.\n\n"\
                        "Message: Invalid script path '%s', please provide valid script path\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Script Name]"

//SCHEDULE 
#define CAV_ERR_1011336 "CavErr[1011336]: Global Schedule is not configured properly.\n\n"\
                        "Message: Phase '%s' given in Global Scenario is not a valid phase.\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule]"

#define CAV_ERR_1011337 "CavErr[1011337]: Global Schedule is not configured properly.\n\n"\
                        "Message: Ramp Up pattern '%s' is not a valid pattern for 'RATE' mode. It can be LINEARLY or RANDOMLY.\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up] -> [Rate]"

//SCHEDULE_TYPE
#define CAV_ERR_1011338 "CavErr[1011338]: Schedule Type option is not configured properly.\n\n"\
                        "Message: '%s' is not a valid Schedule Type mode.\n"\
                        "Configured it using -\n\t[Schedule Setting] -> [Schedule Type]"

//SCHEDULE_BY
#define CAV_ERR_1011339 "CavErr[1011339]: Schedule By option is not configured properly.\n\n"\
                        "Message: Invalid value of Schedule By '%s'. It can be only 'Scenario' or 'Group'\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Schedule By]"
//SCHEDULE
#define CAV_ERR_1011340 "CavErr[1011340]: Global Schedule is not configured properly.\n\n"\
                        "Message: Ramp Up mode '%s' selected for Fixed Concurrent Users is not a valid mode. It can be IMMEDIATELY, STEP, RATE or TIME.\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up]"

#define CAV_ERR_1011341 "CavErr[1011341]: Global Schedule is not configured properly.\n\n"\
                        "Message: Ramp Up mode '%s' selected for Fixed Sessions Rate scenarios. It can be IMMEDIATELY or TIME_SESSIONS.\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up]"

#define CAV_ERR_1011342 "CavErr[1011342]: Global Schedule is not configured properly.\n\n"\
                        "Message: Ramp Up pattern '%s' is not a valid pattern for 'RATE' mode. It can be LINEARLY or RANDOMLY.\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up] -> [Rate]"

#define CAV_ERR_1011343 "CavErr[1011343]: Global Schedule is not configured properly.\n\n"\
                        "Message: Ramp Up pattern '%s' is not a valid pattern for 'TIME' mode. It can be LINEARLY or RANDOMLY\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Up] -> [Time]"

#define CAV_ERR_1011344 "CavErr[1011344]: Global Schedule is not configured properly.\n\n"\
                        "Message: Ramp Down mode '%s' selected for Fixed Concurrent Users is not a valid mode. It can be IMMEDIATELY, STEP or TIME\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Down]"

#define CAV_ERR_1011345 "CavErr[1011345]: Global Schedule is not configured properly.\n\n"\
                        "Message: Ramp Down pattern '%s' is not a valid pattern for 'TIME' mode. It can be LINEARLY or RANDOMLY\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Ramp Down] -> [Time]"

#define CAV_ERR_1011346 "CavErr[1011346]: Scenario Group is not configured properly.\n\n"\
                        "Message: Users (%d) are unavailable to run test on generators (%d) used in a group\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Users/Session/Percentage]"

#define CAV_ERR_1011347 "CavErr[1011347]: Scenario Group is not configured properly.\n\n"\
                        "Message: Selected script is not JMeter type script.\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group]" 

#define CAV_ERR_1011348 "CavErr[1011348]: Scenario Group is not configured properly.\n\n"\
                        "Message: In NetStorm mode generator name should be NA, but given generator name is '%s'.\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Generator Name]"

#define CAV_ERR_1011349 "CavErr[1011349]: Scenario Group is not configured properly.\n\n"\
                        "Message: Number of users '%s' is not valid, it must be numeric\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Users/Session/Percentage]"

#define CAV_ERR_1011350 "CavErr[1011350]: Scenario Group is not configured properly.\n\n"\
                        "Message: Group name '%s' is already defined in Scenario Group.\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Group Name]"

#define CAV_ERR_1011351 "CavErr[1011351]: Scenario Group is not configured properly.\n\n"\
                        "Message: In NetCloud mode generator count should be numeric, but given generator count is '%s' for Scenario Group '%s'.\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Generator Name]"

#define CAV_ERR_1011352 "CavErr[1011352]: Scenario Group is not configured properly.\n\n"\
                        "Message: Provided script path '%s' doesn't exist. Please provide a valid script path\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group] -> [Script Name]"

#define CAV_ERR_1011353 "CavErr[1011353]: Global Schedule is not configured properly.\n\n"\
                        "Message: Construction of phases is incorrect. phase id %d (%s), active users left = %d\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule]"

#define CAV_ERR_1011354 "CavErr[1011354]: Global Schedule is not configured properly.\n\n"\
                        "Message: No run phase given for Group id %d.\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule]"

#define CAV_ERR_1011355 "CavErr[1011355]: Global Schedule is not configured properly.\n\n"\
                        "Message: Invalid phase name '%s'.\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule]"

#define CAV_ERR_1011356 "CavErr[1011356]: Global Schedule is not configured properly.\n\n"\
                        "Message: Number of sessions '%d' are not enough to run number of vusers '%d'\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Duration] -> [Session]"

#define CAV_ERR_1011357 "CavErr[1011357]: Provided Modulemask '%s' is not a valid modulemask."

#define CAV_ERR_1011358 "CavErr[1011358]: 'CLUSTER_VARS' can be used once in scenario configuration."\

#define CAV_ERR_1011359 "CavErr[1011359]: Too few/more arguments for %s keywords.\n"

#define CAV_ERR_1011360 "CavErr[1011360]: ENABLE_HEROKU_MONITORS must have argument as 0 or 1 only.\n"

#define CAV_ERR_1011361 "CavErr[1011361]: Alert setting is not configured properly.\n\n"\
                        "Message: %s\n"\

#define CAV_ERR_1011362 "CavErr[1011361]: Alert setting is not configured properly.\n\n"\
                        "Message: Provided type %s is not supported\n"\

#define CAV_ERR_1011363 "CavErr[1011363]: Scenario Group is not configured properly.\n\n"\
                        "Message: Failed to create URL Based Script [%s], Command Argument [%s]\n"\
                        "Re-Configure it using -\n\t[Schedule Setting] -> [Add Group]" 

#define CAV_ERR_1011364 "CavErr[1011364]: Global Schedule is not configured properly.\n\n"\
                        "Message: Group %s and %s belong to different generators\n"\
                        "Configure it using -\n\t[Schedule Setting] -> [Global Schedule] -> [Start]"

#define CAV_ERR_1011365 "CavErr[1011365]: Additional HTTP Header settings is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [HTTP]"

#define CAV_ERR_1011366 "CavErr[1011366]: For Addtional HTTP Header Mode can be 0(MAIN),1(INLINE),2(ALL).\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [HTTP]"

#define CAV_ERR_1011367 "CavErr[1011367]: Additional HTTP Header settings for all scenario groups cannot be specified for a page.\n"\
                        "In the scenario, it is configured for Page '%s'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [HTTP]"

#define CAV_ERR_1011368 "CavErr[1011368]: Page name '%s' configured in Additional HTTP Header Settings for scenario group '%s' is not valid page in the script '%s'.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [HTTP]"

#define CAV_ERR_1011370 "CavErr[1011370]: Jmeter CSV File Split Option is not configured properly\n\n"\
                        "message: %s\n"\
                        "Configure it correctly using [Scripts] -> [Run Jmeter Scripts]"

#define CAV_ERR_1011371 "CavErr[1011371]: Jmeter Vusers Split Option is not configured properly\n\n"\
                        "message: %s\n"\
                        "Configure it correctly using [Scripts] -> [Run Jmeter Scripts]"

#define CAV_ERR_1011372 "CavErr[1011372]: Absolute path of data file is not supported in Jmeter"

#define CAV_ERR_1011373 "CavErr[1011373]: In JMETER Setting Additonal argument length shouldn't be greater than 1024"

#define CAV_ERR_1011374 "CavErr[1011374]: Jmeter Schedule Setting Option is not configured properly\n\n"\
                        "message: %s\n"\ 
                        "Configure it correctly using [Scripts] -> [Run Jmeter Scripts]"

#define CAV_ERR_1011375 "CavErr[1011375]: JMeter Addtional Argument is not configured properly.\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [JMeter Settings]"

#define CAV_ERR_1011376 "CavErr[1011376]: Generate Jmeter Report can have value 0 or 1\n\n"\
                        "Message: %s\n"\
                        "Configure it correctly using [Group Based Settings] -> [JMeter Settings]"


//Script Parsing Error
#define CAV_ERR_1012001 "CavErr[1012001]: "
#define CAV_ERR_1012002_ID "CavErr[1012002]: "
#define CAV_ERR_1012003_ID "CavErr[1012003]: "
#define CAV_ERR_1012003_MSG "Start transaction can only have one input i.e. transaction name."
#define CAV_ERR_1012004_ID "CavErr[1012004]: "
#define CAV_ERR_1012004_MSG "Failed to get flow file list."
#define CAV_ERR_1012005_ID "CavErr[1012005]: "
#define CAV_ERR_1012005_MSG "End transaction as different name can have three inputs, start transaction name, transaction status and end transaction name."
#define CAV_ERR_1012006_ID "CavErr[1012006]: "
#define CAV_ERR_1012006_MSG "End transaction can only have two inputs, transaction name and transaction status."
#define CAV_ERR_1012007_ID "CavErr[1012007]: "
#define CAV_ERR_1012007_MSG "Syncpoint can only have one input i.e. syncpoint name"
#define CAV_ERR_1012008_ID "CavErr[1012008]: "
#define CAV_ERR_1012008_MSG "Start timer can only have one input i.e. timer name"
#define CAV_ERR_1012009_ID "CavErr[1012009]: "
#define CAV_ERR_1012009_MSG "Invalid variable '%s'. It should contain only alphanumeric character and '_'. Also, first character should be an alphabet."
#define CAV_ERR_1012010_ID "CavErr[1012010]: "
#define CAV_ERR_1012010_MSG "Variable '%s' cannot be declared more than once."
#define CAV_ERR_1012011_ID "CavErr[1012011]: "
#define CAV_ERR_1012011_MSG "Failed to generate hash table for 'variable_names.txt' due to %s."
#define CAV_ERR_1012012_ID "CavErr[1012012]: "
#define CAV_ERR_1012012_MSG "For Java type script jar files size(%d) exceeds provided size(%d) for jar files."
#define CAV_ERR_1012013_ID "CavErr[1012013]: "
#define CAV_ERR_1012013_MSG "For Java type script class path size(%d) exceeds provided size(%d) for class path."
#define CAV_ERR_1012014_ID "CavErr[1012014]: "
#define CAV_ERR_1012014_MSG "Failed to compile script."
#define CAV_ERR_1012015_ID "CavErr[1012015]: "
#define CAV_ERR_1012015_MSG "Failed to get runlogic function in 'runlogic.c' file."
#define CAV_ERR_1012016_ID "CavErr[1012016]: "
#define CAV_ERR_1012016_MSG "Failed to get '%s' function in '%s.c' file."
#define CAV_ERR_1012017_ID "CavErr[1012017]: "
#define CAV_ERR_1012017_MSG "Failed to get '%s' function in provided script."
#define CAV_ERR_1012018_ID "CavErr[1012018]: "
#define CAV_ERR_1012018_MSG "Page name '%s' configured for %s feature is not a valid page. Please provide valid page name."
#define CAV_ERR_1012019_MSG "CavErr[1012019]: Variable '%s' used in Checkpoint for 'Save Count' is not declared for script '%s'."
#define CAV_ERR_1012020_ID "CavErr[1012020]: "
#define CAV_ERR_1012020_MSG "In Search Param Regex Group can have value range from 0-10"

//Start of Error code for parse_api
#define CAV_ERR_1012021_ID  "CavErr[1012021]: "
#define CAV_ERR_1012021_MSG "'%s' bracket is missing.\nRegistration.spec Line = "
#define CAV_ERR_1012022_ID  "CavErr[1012022]: "
#define CAV_ERR_1012022_MSG "Unexpected characters '%s' after closing bracket. After end bracket only space, tabs and semicolon are allowed.\nRegistration.spec Line = "
#define CAV_ERR_1012023_ID  "CavErr[1012023]: " 
#define CAV_ERR_1012023_MSG "Argument name '%s' length (%d) is greater than max length (%d).\nRegistration.spec Line = "
#define CAV_ERR_1012024_ID  "CavErr[1012024]: "
#define CAV_ERR_1012024_MSG "Unexpected character '%s' after closing double quote in field '%d'. After closing double quote only spaces, tabs, ',', and ')' are allowed and after equal sign only one pair of double quote is allowed.\nRegistration.spec Line = "
#define CAV_ERR_1012025_ID  "CavErr[1012025]: "
#define CAV_ERR_1012025_MSG "Starting and ending double quote is mismatched in argument value.\nRegistration.spec Line = "
#define CAV_ERR_1012026_ID  "CavErr[1012026]: "
#define CAV_ERR_1012026_MSG "API argument value '%s' length (%d) is greater than max length (%d).\nRegistration.spec Line = "
#define CAV_ERR_1012028_ID  "CavErr[1012028]: "
#define CAV_ERR_1012028_MSG "API line length (%d) is greater than max length (%d).\nRegistration.spec Line = "
#define CAV_ERR_1012029_ID  "CavErr[1012029]: "
#define CAV_ERR_1012029_MSG "API name length (%d) is greater than max length (%d).\nRegistration.spec Line = "
//End of Error code for parse_api

#define CAV_ERR_1012027_ID  "CavErr[1012027]: "
#define CAV_ERR_1012027_MSG "Unknown '%s' option provided for '%s' in %s parameter."
#define CAV_ERR_1012030_ID  "CavErr[1012030]: "
#define CAV_ERR_1012030_MSG "RedirectionDepth option supports only when AUTO_REDIRECT feature is enabled\n."\
                            "Configure it using-\n\t[Global Settings] -> [HTTP] -> [Advanced]"
#define CAV_ERR_1012031_ID  "CavErr[1012031]: "
#define CAV_ERR_1012031_MSG "Invalid field number '%d' of VAR_VALUE in file parameter. Total fields in file are '%d'"
#define CAV_ERR_1012032_ID  "CavErr[1012032]: "
#define CAV_ERR_1012032_MSG "Invalid value of RedirectionDepth. It must be in between 1 and 32."
#define CAV_ERR_1012033_ID  "CavErr[1012033]: "
#define CAV_ERR_1012033_MSG "Value of '%s' option cannot be less than zero in %s parameter."
#define CAV_ERR_1012034_ID  "CavErr[1012034]: "
#define CAV_ERR_1012034_MSG "'LBMATCH' option can have numeric value or 'CLOSEST' or 'FIRST'."
#define CAV_ERR_1012036_ID  "CavErr[1012036]: "
#define CAV_ERR_1012036_MSG "'%s' option can only be specified once in %s parameter."
#define CAV_ERR_1012037_ID  "CavErr[1012037]: "
#define CAV_ERR_1012037_MSG "Invalid VAR_VALUE type '%s' in file parameter. It can be FILE, FILE_PARAM or VALUE."
#define CAV_ERR_1012055_ID  "CavErr[1012055]: "
#define CAV_ERR_1012055_MSG "'%s' option must be specified in %s parameter."

//Search Parameter Start
#define CAV_ERR_1012041_ID  "CavErr[1012041]: "
#define CAV_ERR_1012041_MSG "'ORD' option value can not be zero in %s parameter."
#define CAV_ERR_1012043_ID  "CavErr[1012043]: "
#define CAV_ERR_1012043_MSG "'%s' option can only have numeric value."
#define CAV_ERR_1012046_ID  "CavErr[1012046]: "
#define CAV_ERR_1012046_MSG "Parameter '%s' should be define before declaring parameter '%s' because later is dependent on first."
#define CAV_ERR_1012050_ID  "CavErr[1012050]: "
#define CAV_ERR_1012050_MSG "Specific page and ALL pages(*) cannot be specified together for '%s' option."
#define CAV_ERR_1012052_ID  "CavErr[1012052]: "
#define CAV_ERR_1012052_MSG "Search option must be set to 'Variable' if 'LB' AND/OR 'RB' value is specified through parameter."
#define CAV_ERR_1012053_ID  "CavErr[1012053]: "
#define CAV_ERR_1012053_MSG "%s parameter name is missing. It should be at very first position in API."
#define CAV_ERR_1012054_ID  "CavErr[1012054]: "
#define CAV_ERR_1012054_MSG "In Search Parameter ORD value must be '1' when either of LB and/Or RB arguments are not specified OR provide RE argument."
//Search Parameter End

//Declare Array Parameter Start
#define CAV_ERR_1012126_ID  "CavErr[1012126]: "
#define CAV_ERR_1012126_MSG "Invalid argument is provided in Declare Parameter."
#define CAV_ERR_1012127_ID  "CavErr[1012127]: "
#define CAV_ERR_1012127_MSG "In Declare Array Parameter, variable name is not provided."
#define CAV_ERR_1012128_ID  "CavErr[1012128]: "
#define CAV_ERR_1012128_MSG "'Size' option provided in declare array param should be numeric."
#define CAV_ERR_1012129_ID  "CavErr[1012129]: "
#define CAV_ERR_1012129_MSG "Invalid range of 'Size' option in declare array parameter. Range should be in between 1 to 10000."
#define CAV_ERR_1012130_ID  "CavErr[1012130]: "
#define CAV_ERR_1012130_MSG "Option %s is invalid in %s parameter."
#define CAV_ERR_1012131_ID  "CavErr[1012131]: "
#define CAV_ERR_1012131_MSG "'Size' option is not provided in declare array parameter."
//Declare Array Parameter End

//Tag var parameter start
#define CAV_ERR_1012132_ID  "CavErr[1012132]: "
#define CAV_ERR_1012132_MSG "'%s' option value is not provided in %s parameter."
#define CAV_ERR_1012133_ID  "CavErr[1012133]: "
#define CAV_ERR_1012133_MSG "Invalid format of xml parameter is provided."
#define CAV_ERR_1012134_ID  "CavErr[1012134]: "
#define CAV_ERR_1012134_MSG "Invalid tag format is provided for '%s' option in XML parameter."
#define CAV_ERR_1012135_ID  "CavErr[1012135]: "
#define CAV_ERR_1012135_MSG "Invalid attribute field provided in XML parameter. Attribute field must contain an '=' symbol."
#define CAV_ERR_1012136_ID  "CavErr[1012136]: "
#define CAV_ERR_1012136_MSG "'ORD' option value can either have numeric digits or %s in %s parameter."
#define CAV_ERR_1012137_ID  "CavErr[1012137]: " 
#define CAV_ERR_1012137_MSG "EncodeMode option value can only be provided 'Specified' with CharstoEncode option in %s parameter."
#define CAV_ERR_1012138_ID  "CavErr[1012138]: "
#define CAV_ERR_1012138_MSG "CharstoEncode option '%s' is not provided properly. Only special characters are allowed in %s parameter."
#define CAV_ERR_1012139_ID  "CavErr[1012139]: "
#define CAV_ERR_1012139_MSG "EncodeSpaceBy option supports only for '+' and '%20' character in %s parameter."
#define CAV_ERR_1012140_ID  "CavErr[1012140]: "
#define CAV_ERR_1012140_MSG "Can have only value qualifier in XML parameter"
#define CAV_ERR_1012141_ID  "CavErr[1012141]: "
#define CAV_ERR_1012141_MSG "Invalid node value '%s' is provided for XML parameter."

//JSON param start
#define CAV_ERR_1012035_ID  "CavErr[1012035]: "
#define CAV_ERR_1012035_MSG "Search option must be set to 'Variable' if value to be search is specified through parameter in JSON parameter."
#define CAV_ERR_1012184_ID  "CavErr[1012184]: "
#define CAV_ERR_1012184_MSG "OBJECT_PATH should start with root element. Please add a prefix root in it."
//JSON param end

//Random number start
#define CAV_ERR_1012039_ID  "CavErr[1012039]: "
#define CAV_ERR_1012039_MSG "Invalid value for 'Format' option is provided in %s parameter."
#define CAV_ERR_1012044_ID  "CavErr[1012044]: "
#define CAV_ERR_1012044_MSG "Minimum option value should be less than Maximum option value in %s parameter."
//Random number end

//Unique Number Start
#define CAV_ERR_1012045_ID  "CavErr[1012045]: "
#define CAV_ERR_1012045_MSG "Format option value should start with '0' in Unique Number parameter."
#define CAV_ERR_1012047_ID  "CavErr[1012047]: "
#define CAV_ERR_1012047_MSG "Format value digit length cannot be more than three in Unique Number parameter."
#define CAV_ERR_1012048_ID  "CavErr[1012048]: "
#define CAV_ERR_1012048_MSG "Format value digit range should be in between '08' and '032' in Unique Number parameter."
//Unique Number End

//Global cookie start
#define CAV_ERR_1012051_ID  "CavErr[1012051]: "
#define CAV_ERR_1012051_MSG "Disable 'AUTO_COOKIE' settings from scenario[Global Settings -> HTTP -> Advanced], to use 'Cookie' parameter. Both cannot be use at a time."
#define CAV_ERR_1012056_ID  "CavErr[1012056]: "
#define CAV_ERR_1012056_MSG "Provided cookie value '%s' is of length(%d), which is greater than the expected length(%d)."
//GLobal cookie end

//UniqueRangevar Start
#define CAV_ERR_1012038_ID  "CavErr[1012038]: "
#define CAV_ERR_1012038_MSG "Unique Range parameter is not supported for 'Fix Session Rate' Scenario Type option."
//UniqueRangevar End

//Random String start
#define CAV_ERR_1012177_ID "CavErr[1012177]: "
#define CAV_ERR_1012177_MSG "Must have variables for the declaration in Random String parameter."
//Random string end

//Checkpoint Start
#define CAV_ERR_1012122_ID "CavErr[1012122]: "
#define CAV_ERR_1012122_MSG "Both TEXT and TextPfx/TextSfx option cannot be provided together in Checkpoint."
#define CAV_ERR_1012305_ID "CavErr[1012305]: "
#define CAV_ERR_1012305_MSG "Checksum value '%s'(contains %d character). Its length should not exceed 32 characters."
#define CAV_ERR_1012306_ID "CavErr[1012306]: "
#define CAV_ERR_1012306_MSG "Invalid value '%s' of Checksum option provided. It value must be in hexadecimal number."
#define CAV_ERR_1012310_ID "CavErr[1012310]: "
#define CAV_ERR_1012310_MSG "Any of the option from TEXT, (TextPfx and TextSfx), CompareFile, Checksum, ChecksumCookie must be specified."
//Checkpoint End

//File Parameter start
#define CAV_ERR_1012087_ID   "CavErr[1012087]: "
#define CAV_ERR_1012087_MSG  "Must have variables for the declaration in File parameter."
#define CAV_ERR_1012091_ID   "CavErr[1012091]: "
#define CAV_ERR_1012091_MSG  "Column index for a variable cannot be set to 1 as MODE value is set to 'WEIGHTED_RANDOM' so first column of data file is fixed for Weight."
#define CAV_ERR_1012098_ID   "CavErr[1012098]: "
#define CAV_ERR_1012098_MSG  "Either give all variable name with column number or don't specify column number for any variable name in File parameter."
#define CAV_ERR_1012100_ID   "CavErr[1012100]: "
#define CAV_ERR_1012100_MSG  "File parameter variable name cannot have 0 as the column index = %s:%s."
#define CAV_ERR_1012088_ID   "CavErr[1012088]: "
#define CAV_ERR_1012088_MSG  "File parameter variable '%s' is of size '%d'. It should be in between 1 and 4096."
#define CAV_ERR_1012101_ID   "CavErr[1012101]: "
#define CAV_ERR_1012101_MSG  "Variable name '%s' on line %d is already provided. Please provide different variable name."
#define CAV_ERR_1012102_ID   "CavErr[1012102]: "
#define CAV_ERR_1012102_MSG  "Failed in entering variable name '%s' into hash_table ."
#define CAV_ERR_1012107_ID   "CavErr[1012107]: "
#define CAV_ERR_1012107_MSG  "FirstDataLine option value(%d) should be greater than headerLine(%d)."
#define CAV_ERR_1012108_ID   "CavErr[1012108]: "
#define CAV_ERR_1012108_MSG  "Weigted Random option cannot be read-write (specified by /size) in File parameter."
#define CAV_ERR_1012089_ID   "CavErr[1012089]: "
#define CAV_ERR_1012089_MSG  "'%s' option must be specified."
#define CAV_ERR_1012110_ID   "CavErr[1012110]: "
#define CAV_ERR_1012110_MSG  "CopyFileToTR option is used with YES argument but FILE option doesn't contain absolute file path. Please provide absolute file path."
#define CAV_ERR_1012090_ID   "CavErr[1012090]: "
#define CAV_ERR_1012090_MSG  "CopyFileToTR option is used with YES argument but IsSaveIntoFile option is used with NO. Please correct it"
#define CAV_ERR_1012092_ID   "CavErr[1012092]: "
#define CAV_ERR_1012092_MSG  "Failed to run '%s' command in Command Var parameter."
#define CAV_ERR_1012118_ID   "CavErr[1012118]: "
#define CAV_ERR_1012118_MSG  "Data file '%s' is used more than once in Parameter with 'Use Once Mode'. For 'Use Once' Mode same data file cannot be use in more than one parameter."
#define CAV_ERR_1012119_ID   "CavErr[1012119]: "
#define CAV_ERR_1012119_MSG  "Data file '%s', extension '%s' and mode '%s' are mismatched in script '%s' api having first parameter '%s'. As given mode is '%s',hence file extension must be '%s'."
#define CAV_ERR_1012093_ID   "CavErr[1012093]: "
#define CAV_ERR_1012093_MSG  "File name provided of size '%d' exceeds its size range. File name size should be maximum of 1024 characters."
#define CAV_ERR_1012094_ID   "CavErr[1012094]: "
#define CAV_ERR_1012094_MSG  "Total number of columns in %s data file for a row exceeded the maximum limit of 200."
#define CAV_ERR_1012095_ID   "CavErr[1012095]: "
#define CAV_ERR_1012095_MSG  "None of the given lines of the %s data file contains the maximum column index %d."
#define CAV_ERR_1012096_ID   "CavErr[1012096]: "
#define CAV_ERR_1012096_MSG  "For 'WEIGHTED_RANDOM' mode first field should be numeric value."
#define CAV_ERR_1012097_ID   "CavErr[1012097]: "
#define CAV_ERR_1012097_MSG  "Column index provided for a variable does not have the corresponding value present in the %s data file."
#define CAV_ERR_1012086_ID   "CavErr[1012086]: "
#define CAV_ERR_1012086_MSG  "Invalid value format provided. %d or more values provided. Must have %d values."
#define CAV_ERR_1012085_ID   "CavErr[1012085]: "
#define CAV_ERR_1012085_MSG  "Variable '%s' has specified max size of '%d', canot have larger sized value for '%s'"
#define CAV_ERR_1012112_ID   "CavErr[1012112]: "
#define CAV_ERR_1012112_MSG  "Format of 'Host' or 'Port' is invalid."
#define CAV_ERR_1012113_ID   "CavErr[1012113]: "
#define CAV_ERR_1012113_MSG  "Bad Family or Bad IP found."
#define CAV_ERR_1012084_ID   "CavErr[1012084]: "
#define CAV_ERR_1012084_MSG  "Port is not provided."
#define CAV_ERR_1012083_ID   "CavErr[1012083]: "
#define CAV_ERR_1012083_MSG  "Driver name '%s' does't not exist."
#define CAV_ERR_1012099_MSG  "CavErr[1012099]: Insufficient number of variable values specified in data file '%s' used in file parameter with "\
                             "mode Unique in script '%s'. Total users for the scenario group using this script are '%d' and total number "\
                             "of variable values in this file are '%d'"
#define CAV_ERR_1012120_ID   "CavErr[1012120]: "
#define CAV_ERR_1012120_MSG  "Absolute data file path '%s' is invalid. Absolute data file must start with '/home/cavisson'"
#define CAV_ERR_1012121_ID   "CavErr[1012121]: "
#define CAV_ERR_1012121_MSG  "Insufficient number of variable values specified in data file '%s' used in file parameter with mode "\
                             "%s in script '%s'. Total users for the scenario group using this script are '%d' "\
                             "and total number of variable values in this file are '%d'."
#define CAV_ERR_1012123_ID   "CavErr[1012123]: "
#define CAV_ERR_1012123_MSG  "File extension '%s' and mode '%s' are mismatched. As given mode is '%s',hence file extension must be '%s'."

#define CAV_ERR_1012124_ID   "CavErr[1012124]: "
#define CAV_ERR_1012124_MSG  "Data file '%s' used in script '%s' does not exist."
//File parameter end

//Data time parameter
#define CAV_ERR_1012103_ID   "CavErr[1012103]: "
#define CAV_ERR_1012103_MSG  "%s"

//API's start
#define CAV_ERR_1012057_ID  "CavErr[1012057]: "
#define CAV_ERR_1012057_MSG "BODY must be terminated by quotes, optionally comma and then new line."
#define CAV_ERR_1012058_ID  "CavErr[1012058]: "
#define CAV_ERR_1012058_MSG "Single comma(,) should be there after termination of BODY."
#define CAV_ERR_1012059_ID  "CavErr[1012059]: "
#define CAV_ERR_1012059_MSG "'=' not found in %s boundary"
#define CAV_ERR_1012060_ID  "CavErr[1012060]: "
#define CAV_ERR_1012060_MSG "Boundary is started with quotes but in the end, quotes is missing."
#define CAV_ERR_1012061_ID  "CavErr[1012061]: "
#define CAV_ERR_1012061_MSG "Found header with boundary %s."
#define CAV_ERR_1012062_ID  "CavErr[1012062]: "
#define CAV_ERR_1012062_MSG "Exceeded max limit of boundaries (parts of multipart body) in multipart message."
#define CAV_ERR_1012063_ID  "CavErr[1012063]: "
#define CAV_ERR_1012064_ID  "CavErr[1012064]: "
#define CAV_ERR_1012064_MSG "Correct input is not provided (ns_protobuf_segment_line)."
#define CAV_ERR_1012065_ID  "CavErr[1012065]: "
#define CAV_ERR_1012065_MSG "Post content length size exceeds its limit of %d."
#define CAV_ERR_1012066_ID  "CavErr[1012066]: "
#define CAV_ERR_1012066_MSG "cavd variable has no name."
#define CAV_ERR_1012067_ID  "CavErr[1012067]: "
#define CAV_ERR_1012067_MSG "HTTP headers size is greater than expected size(%d)."
#define CAV_ERR_1012068_ID  "CavErr[1012068]: "
#define CAV_ERR_1012068_MSG "Pagename '%s' is provided multiple times in script '%s'."
#define CAV_ERR_1012069_ID  "CavErr[1012069]: "
#define CAV_ERR_1012069_MSG "Provided URL '%s' is not a valid URL."
#define CAV_ERR_1012070_ID  "CavErr[1012070]: " 
#define CAV_ERR_1012070_MSG "Invalid format or API Name is not provided in Line."
#define CAV_ERR_1012071_ID  "CavErr[1012071]: "
#define CAV_ERR_1012071_MSG "Failed to write into File."
#define CAV_ERR_1012072_ID  "CavErr[1012072]: "
#define CAV_ERR_1012072_MSG "Provided URL is not valid. Expecting one of either 'http://' or 'https://' or 'xhttp://'."
#define CAV_ERR_1012073_ID  "CavErr[1012072]: "
#define CAV_ERR_1012073_MSG "Failed to add hostname to server table"
#define CAV_ERR_1012074_ID  "CavErr[1012074]: "
#define CAV_ERR_1012074_MSG "Provided fully_qualified_url value '%s' is not a valid value. It can be YES or NO."
#define CAV_ERR_1012075_ID  "CavErr[1012075]: "
#define CAV_ERR_1012075_MSG "HTTP version '%s' is not supported. Please provide version as 1.0 or 1.1"
#define CAV_ERR_1012076_ID  "CavErr[1012076]: "
#define CAV_ERR_1012076_MSG "<100-continue> header can be given only with POST method.\n"\
                            "Reconfigure header method using [Insert] -> [Action API] -> [POST]."
#define CAV_ERR_1012077_ID  "CavErr[1012077]: "
#define CAV_ERR_1012077_MSG "Pipelining feature cannot be enabled with <100-continue> header." 
#define CAV_ERR_1012142_ID  "CavErr[1012142]: "
#define CAV_ERR_1012142_MSG "'%s' is not provided in compression ratio."
#define CAV_ERR_1012143_ID  "CavErr[1012143]: "
#define CAV_ERR_1012143_MSG "'%s' is not a numeric value in compression ratio."
#define CAV_ERR_1012144_ID  "CavErr[10120144]: "
#define CAV_ERR_1012144_MSG "'%s' Url Callback funtion name is not provided correctly."
#define CAV_ERR_1012145_ID  "CavErr[1012145]: "
#define CAV_ERR_1012145_MSG "Request type provided is not valid. It can be HTTP or HTTPS."
#define CAV_ERR_1012146_ID  "CavErr[1012146]: "
#define CAV_ERR_1012146_MSG "Location is not defined for redirected url."
#define CAV_ERR_1012147_ID  "CavErr[1012147]: "
#define CAV_ERR_1012147_MSG "End of File found but BODY closing quote not found. Body close quote should be present."
#define CAV_ERR_1012148_ID  "CavErr[1012148]: "
#define CAV_ERR_1012148_MSG "Unexpected end of file."
#define CAV_ERR_1012149_ID  "CavErr[1012149]: "
#define CAV_ERR_1012149_MSG "Body for multipart body should start from a new line."
#define CAV_ERR_1012150_ID  "CavErr[1012150]: "
#define CAV_ERR_1012150_MSG "Starting quote not found"
#define CAV_ERR_1012151_ID  "CavErr[1012151]: "
#define CAV_ERR_1012151_MSG "segment start value is less than 0 or tmpPostTable->num_entries value is less than or equals to 0."
#define CAV_ERR_1012152_ID  "CavErr[1012152]: "
#define CAV_ERR_1012152_MSG "Failed to encode URL '%s'"
#define CAV_ERR_1012153_ID  "CavErr[1012153]: "
#define CAV_ERR_1012153_MSG "Passed tag is not valid. Expecting tag is '%s'."
#define CAV_ERR_1012154_ID  "CavErr[1012154]: "
#define CAV_ERR_1012154_MSG "'%s' not provided for name-value pair."
#define CAV_ERR_1012155_ID  "CavErr[1012155]: "
#define CAV_ERR_1012155_MSG "End of File found but ITEMDATA closing mark not found."
#define CAV_ERR_1012156_ID  "CavErr[1012156]: "
#define CAV_ERR_1012156_MSG "BODY can not be provided with '%s'."
#define CAV_ERR_1012158_ID  "CavErr[1012158]: "
#define CAV_ERR_1012158_MSG "'%s' value should be greater than 60 sec"
#define CAV_ERR_1012159_ID  "CavErr[1012159]: "
#define CAV_ERR_1012159_MSG "%s (%s) is not valid."
#define CAV_ERR_1012160_ID  "CavErr[1012160]: "
#define CAV_ERR_1012160_MSG "Unexpected data received"
#define CAV_ERR_1012161_ID  "CavErr[1012161]: "
#define CAV_ERR_1012161_MSG "Timeout value '%d' should be in between 10000 msec and 120000 msec."
#define CAV_ERR_1012162_ID  "CavErr[1012162]: "
#define CAV_ERR_1012162_MSG "Number of arguments provided with '%s' is incorrect."
#define CAV_ERR_1012164_ID  "CavErr[1012164]: "
#define CAV_ERR_1012164_MSG "Boundary not found in content type."
#define CAV_ERR_1012165_ID  "CavErr[1012165]: "
#define CAV_ERR_1012165_MSG "MULTIPART_BOUNDARY is missing."
#define CAV_ERR_1012167_ID  "CavErr[1012167]: "
#define CAV_ERR_1012167_MSG "Inline url transaction name is not present"
#define CAV_ERR_1012168_ID  "CavErr[1012168]: "
#define CAV_ERR_1012168_MSG "URL transaction Name is not valid, Inline URL Transaction name should be either string or in <TxName[start-end range]> format."
#define CAV_ERR_1012169_ID  "CavErr[1012169]: "
#define CAV_ERR_1012169_MSG "Space is found in between '%s' and '%s'."
#define CAV_ERR_1012170_ID  "CavErr[1012170]: "
#define CAV_ERR_1012170_MSG "Cannot have two main url in a single page."
#define CAV_ERR_1012171_ID  "CavErr[1012171]: "
#define CAV_ERR_1012171_MSG "'%s' Callback is not supported for Java Type Script"
#define CAV_ERR_1012172_ID  "CavErr[1012172]: "
#define CAV_ERR_1012172_MSG "Found MULTIPART_BODY_BEGIN but main header didnt supply a boundary."
#define CAV_ERR_1012173_ID  "CavErr[1012173]: "
#define CAV_ERR_1012173_MSG "URL option is not provided."
#define CAV_ERR_1012174_ID  "CavErr[1012174]: "
#define CAV_ERR_1012174_MSG "MULTIPART_BODY_BEGIN and END sections dont match."
#define CAV_ERR_1012175_ID  "CavErr[1012175]: "
#define CAV_ERR_1012175_MSG "INLINE URL's  and END_INLINE dont match."
#define CAV_ERR_1012176_ID  "CavErr[1012176]: "
#define CAV_ERR_1012176_MSG "Unexpected script_line read."
#define CAV_ERR_1012178_ID  "CavErr[1012178]: "
#define CAV_ERR_1012178_MSG "END_INLINE not found."
#define CAV_ERR_1012180_ID  "CavErr[1012180]: "
#define CAV_ERR_1012180_MSG "Unexpected end of file found."
#define CAV_ERR_1012500_ID  "CavErr[1012500]: "
#define CAV_ERR_1012500_MSG "= symbol not found after '%s' option."
#define CAV_ERR_1012501_ID  "CavErr[1012501]: "
#define CAV_ERR_1012501_MSG "Starting and ending double quote is mismatched in argument '%s' value."
#define CAV_ERR_1012502_ID  "CavErr[1012502]: "
#define CAV_ERR_1012502_MSG "'%s' not found for value part."
#define CAV_ERR_1012503_ID  "CavErr[1012503]: "
#define CAV_ERR_1012503_MSG "'%s' not found for value of '%s'."
#define CAV_ERR_1012504_ID  "CavErr[1012504]: "
#define CAV_ERR_1012504_MSG "Body Should Start on a new line."
#define CAV_ERR_1012505_ID  "CavErr[1012505]: "
#define CAV_ERR_1012505_MSG "END_INLINE keyword is missing."
#define CAV_ERR_1012506_ID  "CavErr[1012506]: "
#define CAV_ERR_1012506_MSG "Failed to find quotes after first MULTIPART_BODY_BEGIN"
#define CAV_ERR_1012507_ID  "CavErr[1012507]: "
#define CAV_ERR_1012507_MSG "Unknown argument in MULTIPART_BODY found [%s]"
#define CAV_ERR_1012508_ID  "CavErr[1012508]: "
#define CAV_ERR_1012508_MSG "Invalid Inline url transaction name"
#define CAV_ERR_1012509_ID  "CavErr[1012509]: "
#define CAV_ERR_1012509_MSG "Unknown argument provided [%s]."
#define CAV_ERR_1012510_ID  "CavErr[1012510]: "
#define CAV_ERR_1012510_MSG "Comma not found before start of next argument."
#define CAV_ERR_1012511_ID  "CavErr[1012511]: "
#define CAV_ERR_1012511_MSG "Error in parsing API argument"
#define CAV_ERR_1012247_ID  "CavErr[1012247]: "
#define CAV_ERR_1012247_MSG "Failed to write the data.\nSysErr(%d): %s."
#define CAV_ERR_1012328_ID  "CavErr[1012328]: "
#define CAV_ERR_1012328_MSG "Invalid Value received parsing Argument Name = %s."
#define CAV_ERR_1012329_ID  "CavErr[1012329]: "
#define CAV_ERR_1012329_MSG "Parameter value is not terminated by quotes. Parameter name = %s."
#define CAV_ERR_1012330_ID  "CavErr[1012330]: "
#define CAV_ERR_1012330_MSG "Value is not provided for option '%s'."
#define CAV_ERR_1012327_ID  "CavErr[1012327]: "
#define CAV_ERR_1012327_MSG "No comma found after %s."
#define CAV_ERR_1012332_ID  "CavErr[1012332]: "
#define CAV_ERR_1012332_MSG "Starting quote is not provided."
#define CAV_ERR_1012333_ID  "CavErr[1012333]: "
#define CAV_ERR_1012333_MSG "Quotes is missing in the argument."
#define CAV_ERR_1012334_ID  "CavErr[1012334]: "
#define CAV_ERR_1012334_MSG "= not found after argument name."
#define CAV_ERR_1012335_ID  "CavErr[1012335]: "
#define CAV_ERR_1012335_MSG "Argument name is not provided."
#define CAV_ERR_1012243_ID  "CavErr[1012243]: "
#define CAV_ERR_1012243_MSG "Only comma & white space are allowed in between the quotes."
#define CAV_ERR_1012189_ID  "CavErr[1012189]: "
#define CAV_ERR_1012189_MSG "'%s' option is  not provided."
#define CAV_ERR_1012190_ID  "CavErr[1012190]: "
#define CAV_ERR_1012190_MSG "Closing Quotes not found for Pagename option."
#define CAV_ERR_1012245_ID  "CavErr[1012245]: "
#define CAV_ERR_1012245_MSG "No argument is provided for End timer API."
#define CAV_ERR_1012246_ID  "CavErr[1012246]: "
#define CAV_ERR_1012246_MSG "Cannot provide end timer for '%s' as timer was not started for it. Configure it correctly."
#define CAV_ERR_1012192_ID  "CavErr[1012192]: "
#define CAV_ERR_1012192_MSG "Error in parsing %s of '%s' API."
#define CAV_ERR_1012194_ID  "CavErr[1012194]: "
#define CAV_ERR_1012194_MSG "Unexpected '%s' found in click API."
#define CAV_ERR_1012195_ID  "CavErr[1012195]: "
#define CAV_ERR_1012195_MSG "Failed to parse Click script file, comma not found between arguments."
#define CAV_ERR_1012196_ID  "CavErr[1012196]: "
#define CAV_ERR_1012196_MSG "Failed to parse Click script file, start quote not found for argument."
#define CAV_ERR_1012198_ID  "CavErr[1012198]: "
#define CAV_ERR_1012198_MSG "Failed to read script file. Length of argument string is negative."
#define CAV_ERR_1012199_ID  "CavErr[1012199]: "
#define CAV_ERR_1012199_MSG "Failed to parse click script file. Not in \"browserurl=...\" format."
#define CAV_ERR_1012200_ID  "CavErr[1012200]: "
#define CAV_ERR_1012200_MSG "Failed to parse argument of click API, semicolon not followed by closing parenthesis."
#define CAV_ERR_1012206_ID  "CavErr[1012206]: "
#define CAV_ERR_1012206_MSG "Failed to parse argument of click apiname = %s."
#define CAV_ERR_1012207_ID  "CavErr[1012207]: "
#define CAV_ERR_1012207_MSG "Failed to parse argument of click API [%s], with click api BrowserUserProfile, HarLogDir and VncDisplayId option cannot be passed."
#define CAV_ERR_1012208_ID  "CavErr[1012208]: "
#define CAV_ERR_1012208_MSG "CLIPINTERVAL value should be greater or equal to 5 ms."
#define CAV_ERR_1012212_ID  "CavErr[1012212]: "
#define CAV_ERR_1012212_MSG "Can't resolve the url for apiname = '%s()'; Neither specified in API (URL or HREF attributes) nor found last_url."
#define CAV_ERR_1012213_ID  "CavErr[1012213]: "
#define CAV_ERR_1012213_MSG "URL '%s' is not in 'fully qualified' format for apiname = '%s'."
#define CAV_ERR_1012214_ID  "CavErr[1012214]: "
#define CAV_ERR_1012214_MSG "Failed to add entry in click action table for apiname = %s"
#define CAV_ERR_1012216_ID  "CavErr[1012216]: "
#define CAV_ERR_1012216_MSG "Failed to create entry for page or url."
//FTP API start
#define CAV_ERR_1012209_ID  "CavErr[1012209]: "
#define CAV_ERR_1012209_MSG "Start quotes not found for %s API."
#define CAV_ERR_1012210_ID  "CavErr[1012210]: "
#define CAV_ERR_1012210_MSG "Unexpected Data '%s' found after start quotes."
#define CAV_ERR_1012211_ID  "CavErr[1012211]: "
#define CAV_ERR_1012211_MSG "'%s' option must be specified."
#define CAV_ERR_1012215_ID  "CavErr[1012215]: "
#define CAV_ERR_1012215_MSG "Unknown value '%s' provided for %s option."
#define CAV_ERR_1012217_ID  "CavErr[1012217]: "
#define CAV_ERR_1012217_MSG "'%s' option must be specified for '%s' page."
#define CAV_ERR_1012218_ID  "CavErr[1012218]: "
#define CAV_ERR_1012218_MSG "%s option can be given only once."
#define CAV_ERR_1012219_ID  "CavErr[1012219]: "
#define CAV_ERR_1012219_MSG "Unknown/Unsupported argument '%s' provided."
#define CAV_ERR_1012220_ID  "CavErr[1012220]: "
#define CAV_ERR_1012220_MSG "Unknown value '%s' provided for %s option. It can be '%s' or '%s'."
#define CAV_ERR_1012221_ID  "CavErr[1012221]: "
#define CAV_ERR_1012221_MSG "Maximum Random bytes value cannot be less than Minimum Random bytes value in CAVINCLUDE."
#define CAV_ERR_1012222_ID  "CavErr[1012222]: "
#define CAV_ERR_1012222_MSG "Minimum Random bytes value cannot be less than zero in CAVINCLUDE."
#define CAV_ERR_1012235_ID  "CavErr[1012235]: "
#define CAV_ERR_1012235_MSG "Request BODY is too large in file %s."
#define CAV_ERR_1012239_ID  "CavErr[1012239]: "
#define CAV_ERR_1012239_MSG "Unable to allocate memory for file %s."
#define CAV_ERR_1012240_ID  "CavErr[1012240]: "
#define CAV_ERR_1012240_MSG "Minimum and Maximum MESSAGE_COUNT value should be greater than 0."
#define CAV_ERR_1012241_ID  "CavErr[1012241]: "
#define CAV_ERR_1012241_MSG "Maximum MESSAGE_COUNT value should be greater than Minimum MESSAGE_COUNT value."
#define CAV_ERR_1012242_ID  "CavErr[1012242]: "
#define CAV_ERR_1012242_MSG "Either count value is allowed or minimum count and maximum count value is allowed."
//FTP API end
//LDAP API start
#define CAV_ERR_1012223_ID  "CavErr[1012223]: "
#define CAV_ERR_1012223_MSG "Invalid URL '%s' is provided. Only ldap or ldaps:// option can be provided."
#define CAV_ERR_1012224_ID  "CavErr[1012224]: "
#define CAV_ERR_1012224_MSG "Invalid argument '%s' is provided. Expecting 'URL' as an attribute."
#define CAV_ERR_1012225_ID  "CavErr[1012225]: "
#define CAV_ERR_1012225_MSG "URL option must be specified for LDAP operation."
#define CAV_ERR_1012452_ID  "CavErr[1012452]: "
#define CAV_ERR_1012452_MSG "'%s' option can only have 0 or 1 value in LDAP operation."
//LDAP API end
//IMAP STart
#define CAV_ERR_1012459_ID  "CavErr[1012459]: "
#define CAV_ERR_1012459_MSG "Request type is not imap(s) but still we are in an imap state."
//IMAP end
//Websocket API start
#define CAV_ERR_1012312_ID  "CavErr[1012312]: "
#define CAV_ERR_1012312_MSG "Buffer option should start on a new line."
#define CAV_ERR_1012313_ID  "CavErr[1012313]: "
#define CAV_ERR_1012313_MSG "END_INLINE keyword is missing."
#define CAV_ERR_1012315_ID  "CavErr[1012315]: "
#define CAV_ERR_1012315_MSG "ID doesn't exists in ns_web_websocket_send API."
#define CAV_ERR_1012317_ID  "CavErr[1012317]: "
#define CAV_ERR_1012317_MSG "Invalid URL is provided. Only ws:// or wss:// option can be provided."
#define CAV_ERR_1012321_ID  "CavErr[1012321]: "
#define CAV_ERR_1012321_MSG "Invalid ID provided for websocket API."
#define CAV_ERR_1012324_ID  "CavErr[1012324]: "
#define CAV_ERR_1012324_MSG "ID [%s] is invalid as there is no WebSocket connect for provided ID."
#define CAV_ERR_1012326_ID  "CavErr[1012326]: "
#define CAV_ERR_1012326_MSG "'ID' value should be unique in websocket script."
#define CAV_ERR_1012336_ID  "CavErr[1012336]: "
#define CAV_ERR_1012336_MSG "'Buffer' option is mandatory."
//Websocket API end
//FC2 start
#define CAV_ERR_1012249_ID  "CavErr[1012249]: "
#define CAV_ERR_1012249_MSG "%s is not provided."
#define CAV_ERR_1012250_ID  "CavErr[1012250]: "
#define CAV_ERR_1012250_MSG "Url host is provided empty with query paramters."
#define CAV_ERR_1012226_ID  "CavErr[1012226]: "
#define CAV_ERR_1012226_MSG "Invalid URL '%s' is provided for FC2 API."
#define CAV_ERR_1012109_ID  "CavErr[1012109]: "
#define CAV_ERR_1012109_MSG "Unexpected line end of ns_web_url API. ');' should be there at the end of ns_web_url API"
//fc2 end
//API's end

//GENERATOR ERROR
#define CAV_ERR_1014001 "CavErr[1014001]: Maximum limit of generators '255' exceeded. Can not run test more than '255' Generators"

#define CAV_ERR_1014002 "CavErr[1014002]: File '%s/etc/.netcloud/generators.dat' doesn't exist"

#define CAV_ERR_1014003 "CavErr[1014003]: Invalid format of generator list in '%s/etc/.netcloud/generators.dat' file"

#define CAV_ERR_1014004 "CavErr[1014004]: Generator '%s' is containing dot(.), generator name can not have dot(.)"

#define CAV_ERR_1014005 "CavErr[1014005]: CaMonAgentPort can not be zero for generator '%s'"\
                        "CavMonAgentPort can have only integer value."

#define CAV_ERR_1014006 "CavErr[1014006]: Error: Generator IP <%s> specified is not a valid generator IP. "\
                        "It can be due to wrong generator entry either in /etc/hosts file or in generator file. "\
                        "Please check generator entry in /etc/hosts file and generator file."

#define CAV_ERR_1014007 "CavErr[1014007]: While using both internal and external controller then internal generatos can not be "\
                        "mention within external controller, please remove '%s' generator from scenario group"

#define CAV_ERR_1014008 "CavErr[1014008]: Users (%d) are unavailable to run test on generators (%d)used in a group"

#define CAV_ERR_1014009 "CavErr[1014009]: Scenario group '%s', generator percentage should add up to total 100 to run Netcloud test"

// ###### Test Init Miss Errors (ErrCode: 1019xxx) ######
#define CAV_ERR_1019001 "CavErr[1019001]: Failed to distribute Sessions over VUsers.\n"\
                           "Number of Sessions(%d) must be greater than or equal to number of VUsers(%d)."   
#define CAV_ERR_1019002 "CavErr[1019002]: Failed to distribute Sessions over VUsers.\n"\
                              "For Group '%s' number of Sessions(%d) must be greater than or equal to number of VUsers(%d)."   

//#define CAV_ERR_1014010 "CavErr[10140010]: Failed to run test on %d out of %d generators, Active [%d], Killed [%d]."

#define CAV_ERR_1014011 "CavErr[1014011]: Failed to create partition inside generator %s directory on controller %s."

#define CAV_ERR_1014012 "CavErr[1014012]: Failed to change ownership of generator %s directory on controller %s."

#define CAV_ERR_1014013 "CavErr[1014013]: Failed to start test.\n" \
                        "Available generators(%d) are less than minimum generators(%d) required to start the test."

#define CAV_ERR_1014014 "CavErr[1014014]: Failed to create threads while uploading generator data."

#define CAV_ERR_1014015 "CavErr[1014015]: Failed to transfer generator %s file to controller. User may not able to see generator logs."

#define CAV_ERR_1014016 "CavErr[1014016]: In netcloud test generator file [%s] doesn't exist."

#define CAV_ERR_1014017 "CavErr[1014017]: Invalid format of generator details in generator file '%s'."

#define CAV_ERR_1014018 "CavErr[1014018]: Generator '%s' have duplicate entry in generator file as '%s'."

#define CAV_ERR_1014019 "CavErr[1014019]: Generator name '%s' not allocated to Controller ip '%s', Controller '%s' in Generator conf file."

#define CAV_ERR_1014020 "CavErr[1014020]: Generator name '%s' does not exist in used generator list."\
                        " Please add this generator in Generator conf file."

#define CAV_ERR_1014021 "CavErr[1014021]: External generators are not specified, while using both internal and external controller."\
                        " Please select external generators in scenario group."

#define CAV_ERR_1014022 "CavErr[1014022]: Internal generators are not specified, while using both internal and external controller."\
                        " Please select internal generators in scenario group."

#define CAV_ERR_1014023 "CavErr[1014023]: Number of users %d of thread group %d is less than number of generators %d."

//#define CAV_ERR_1014024 "CavErr[1014024]: Failed to run %d%% of generators, Active/Expected Generator [%d/%d]"

#define CAV_ERR_1014025 "CavErr[1014025]: All generators got terminated. Please check logs and reconfigure to start test\n"

//#define CAV_ERR_1014026 "CavErr[1014013]: Failed to process generator %s\n%s."

#define CAV_ERR_1014027 "CavErr[1014027]: Invalid generator entry found for Controller '%s' in generator conf file. Value of Controller IP '%s' in file 'cav_%s.conf' or Controller path '%s' does not match in generator conf file.\n"

#define CAV_ERR_1014029 "CavErr[1014027]: Invalid generator entry found for Controller '%s' in generator conf file. Value of Controller IP '%s' in file 'cav.conf' or Controller path '%s' does not match in generator conf file.\n"

#define CAV_ERR_1014028 "CavErr[1014028]: No generator entry found for Controller '%s' in file '%s'\n"


/* ----- || End: Start: Parsing Related Errors (ErrCode: 101xxxx) || ------ */
//===============================================================================

/* ----- || Start: Generators Errors (ErrCode: 103xxxx) || ------ */

/* ----- || End: Generators Errors (ErrCode: 103xxxx) || ------ */
//===============================================================================

/* ----- || Start: Running Test/Session Errors (ErrCode: 103xxxx) || ------ */

#define CAV_ERR_1031001  "CavErr[1031001]: Test is triggered with '%s' user. It can run with 'cavisson' user only."
#define CAV_ERR_1031002  "CavErr[1031002]: %s (%s IP) in file (%s) is not in 'IP/netbits' format.\n"\
                         "Reconfigure and start the test again."
#define CAV_ERR_1031003  "CavErr[1031003]: '%s' option can be specified only once to start test."
#define CAV_ERR_1031004  "CavErr[1031004]: -s (gui_server_address) option must be in format 'SERVER:PORT'" 
#define CAV_ERR_1031005 "CavErr[1031005]: -m option must be in format 'SERVER:PORT:GENERATOR_ID:EventLoggerPortOfMaster:PartitionIdx:DH_PORT'" 
#define CAV_ERR_1031006  "CavErr[1031006]: Retry Count value passed with '-R' option should be greater than 0."
#define CAV_ERR_1031007  "CavErr[1031007]: Retry Interval value passed with '-I' option should be greater than 0."
#define CAV_ERR_1031008  "CavErr[1031008]: Timeout value passed with '-T' option should be greater than 0."
#define CAV_ERR_1031009  "CavErr[1031009]: Invalid argument '%c' provided to netstorm binary."
#define CAV_ERR_1031010  "CavErr[1031010]: Failed to allocate shared memory for NS parent and logging writer."
#define CAV_ERR_1031011  "CavErr[1031011]: '%s' CSV file is not present in generator. It should not happen for '%s', please re-run the test."
#define CAV_ERR_1031012  "CavErr[1031012]: Failed to allocate memory for 'DS: %s' AND/OR 'DS: %s'."
#define CAV_ERR_1031013  "CavErr[1031013]: Failed to allocate memory for 'DS: %s'."
#define CAV_ERR_1031014  "CavErr[1031014]: Failed to allocate memory for user entries to store information regarding test run."
#define CAV_ERR_1031015  "CavErr[1031015]: Unknown '%s' option provided for getting error code."
#define CAV_ERR_1031016  "CavErr[1031016]: Event logging must be enabled if debug tracing is enabled.\n"\
                         "Configure 'Event logging' option using [Global Settings] -> [Logs And Reports] -> [Event Log]."
#define CAV_ERR_1031017  "CavErr[1031017]: Postgresql is not running, please start postgresql and re-run the test."
#define CAV_ERR_1031018  "CavErr[1031018]: Failed to create database tables, please configure and re-run the test."
#define CAV_ERR_1031019  "CavErr[1031019]: Incorrect Event. Hashcode for event '%s' is out of range (%d). It should be in between '0' and '%d'"
#define CAV_ERR_1031020  "CavErr[1031020]: %s version is not available to run RBU test."
#define CAV_ERR_1031021  "CavErr[1031021]: Unable to create profile for given %s '%s' version, as this version is not supported in RBU."
#define CAV_ERR_1031022  "CavErr[1031022]: Unknown Browser mode provided to run RBU test."
#define CAV_ERR_1031023  "CavErr[1031023]: RBU is not installed/configured but RBU feature is enabled. Please install RBU and re-run the test."
#define CAV_ERR_1031024  "CavErr[1031024]: Host <%s> specified by Host header in <%s> group is not valid.\n %s"
#define CAV_ERR_1031025  "CavErr[1031025]: Unknown Server location '%s' is provided. Please add a valid server location and re-run the test."
#define CAV_ERR_1031026  "CavErr[1031026]: 'dnsmasq' is not running OR default nameserver entry not set. First start the dnsmasq"\
                         " then run the test. If you want to run test without dnsmasq then disable STOP_TEST_IF_DNSMASQ_NOT_RUNNING"\
                         " keyword in scenario file. Running test without dnsmsq can change the test results."
#define CAV_ERR_1031027  "CavErr[1031027]: Failed to check whether dnsmasq is running or not through command '%s'"
#define CAV_ERR_1031028  "CavErr[1031028]: Same percentile definition file '%s' is provided for '%s' and '%s'.\n"\
                         "Configure it correctly using Global Settings -> Logs And Reports -> Percentile Data."
#define CAV_ERR_1031029  "CavErr[1031029]: duplicates in %s_write.c file"
#define CAV_ERR_1031030  "CavErr[1031030]: Page name '%s' configured for %s parameter '%s' is not valid page in the script '%s'."
#define CAV_ERR_1031031  "CavErr[1031031]: Runtime runlogic progress option is configured in the scenario. Script ‘%s’ used in"\
                         " scenario group ‘%s’ does not have metadata needed for runtime progress. Either disable runlogic runtime"\
                         " progress or edit the runlogic of this script and save it to create metadata."
#define CAV_ERR_1031032  "CavErr[1031032]: Failed to validate script."
#define CAV_ERR_1031033  "CavErr[1031033]: For Replay Access Logs scenario type only one scenario group is allowed."
#define CAV_ERR_1031034  "CavErr[1031034]: Failed to initialize initial name server address and default domain name."
#define CAV_ERR_1031035  "CavErr[1031035]: Health Check Fail. %s"
#define CAV_ERR_1031036  "CavErr[1031036]: Index Key var '%s' is not declared for session %s."
#define CAV_ERR_1031037  "CavErr[1031037]: Index key var (%s) can not be a type of 'STATIC' or 'COOKIE VAR' for session %s."
#define CAV_ERR_1031038  "CavErr[1031038]: Size of DS: %s(%d) and DS: %s(%d) are not same. It can cause 'MEMORY DAMAGE'"
#define CAV_ERR_1031039  "CavErr[1031039]: Variable name '%s' used for search in search variable name '%s' is not a valid variable."
#define CAV_ERR_1031040  "CavErr[1031040]: Variable name '%s' used for search in json variable name '%s' is not a valid variable."
#define CAV_ERR_1031041  "CavErr[1031041]: Cluster variable values are not defined for cluster id %d."
#define CAV_ERR_1031042  "CavErr[1031042]: Group variable values are not defined for Scenario group %s."
#define CAV_ERR_1031043  "CavErr[1031043]: Variable '%s' used in Checkreply size is not declared for script '%s'."
#define CAV_ERR_1031044  "CavErr[1031044]: C method '%s' defined for applying 'Custom think time' is not available in script. \nSysErr: %s."
#define CAV_ERR_1031045  "CavErr[1031045]: Inline resource fetching is enabled and no validation is also enabled for scenario group '%s'. No validation cannot be used with inline resource fetching enabled."
#define CAV_ERR_1031046  "CavErr[1031046]: Failed to allocate memory for 'string_buffer' of size '%d'"
#define CAV_ERR_1031047  "CavErr[1031047]: For 'Fix %s Rate (%s/Minute)' scenario type, '%s hits per minute' value cannot be zero.\n\n"\
                         "Configure '%s hits per minute' using Global Settings -> Schedule Settings -> %s hits per minute"
#define CAV_ERR_1031048  "CavErr[1031048]: NetDiagnostic mode is enabled but NetDiagnostics data collector port is not provided.\n"\
                         "Configure the collector port using [NetDiagnostics Settings] in scenario UI."
#define CAV_ERR_1031049  "CavErr[1031049]: NetDiagnostic mode is enabled but 'Topology Name' is not provided.\n"\
                         "Configure topology name using [Monitors] -> [Topology Name]"
#define CAV_ERR_1031050  "CavErr[1031050]: Failed to connect to NDCollector server ip: %s, port: %d due to %s"
#define CAV_ERR_1031051  "CavErr[1031051]: Failed to send message to ndcollector."
#define CAV_ERR_1031052  "CavErr[1031052]: Failed to make NDC connection as non blocking. \nSysErr: %s."
#define CAV_ERR_1031053  "CavErr[1031053]: Number of users (%d) cannot be less than Concurrent Session Limit (%d)."
#define CAV_ERR_1031054  "CavErr[1031054]: Schedule By option cannot be provided as 'Group' if Users/Session distribution over NVMs option is provided as 'Distribution for maximum isolation among scripts'.\n"\
                         "Configure it correctly using Global Settings -> Advanced -> Users/Session distribution over NVMs."
#define CAV_ERR_1031055  "CavErr[1031055]: Total numbers of values (%d) in data file (%s) provided for file parameter in script (%s) is less"\
                         "than the total number of CVMs (%d).\n"\
                         "Configure values provided in data file correctly or reduce number of CVM's."
#define CAV_ERR_1031056  "CavErr[1031056]: Number of CPUs given (%d) with 'CPU_AFFINITY' configuration can not be less than Number of Process (%d)"
#define CAV_ERR_1031057  "CavErr[1031057]: Invalid argument provided to netstorm binary '%s'."
#define CAV_ERR_1031058  "CavErr[1031058]: Failed to get controller test run number" 
#define CAV_ERR_1031059  "CavErr[1031059]: Host <%s> specified by Host header is not valid.\n %s"
// ###### License Errors (ErrCode: 1032xxx) ######
#define CAV_ERR_1032001    "CavErr[1032001]: Currently you don't have Netstorm License. Contact Cavisson Account Representative (US +1-800-701-6125) to get license for your product."
#define CAV_ERR_1032002    "CavErr[1032002]: Seems your Netstorm license file is tampered. Contact Cavisson Account Representative (US +1-800-701-6125) to get license for your product."
#define CAV_ERR_1032003    "CavErr[1032003]: License is not valid for product '%s'. Contact Cavisson Account Representative (US +1-800-701-6125) to renew your license.%s"
#define CAV_ERR_1032004    "CavErr[1032004]: Machine IP '%s' is Not Licensed to use Product '%s'. Contact Cavisson Account Representative (US +1-800-701-6125) to renew your license.%s"
#define CAV_ERR_1032005    "CavErr[1032005]: Port '%d' is not a valid '%s' Licensed port. Contact Cavisson Account Representative (US +1-800-701-6125) to renew your license.%s"
#define CAV_ERR_1032006    "CavErr[1032006]: Not Licensed to use protocol '%s' for Product '%s'. Contact Cavisson Account Representative (US +1-800-701-6125) to renew your license.%s"
#define CAV_ERR_1032007    "CavErr[1032007]: Not Licensed to use feature '%s' for Product '%s'. Contact Cavisson Account Representative (US +1-800-701-6125) to renew your license.%s"
#define CAV_ERR_1032008    "CavErr[1032008]: NetStorm License expired on '%s' and grace period of '%d' days is also over. Contact Cavisson Account Representative (US +1-800-701-6125) to renew your license.%s"
#define CAV_ERR_1032009    "CavErr[1032009]: License type provided for product '%s' is '%s'. It can be either 'PERMANENT' or 'LEASE'. Contact Cavisson Account Representative (US +1-800-701-6125).%s"
#define CAV_ERR_1032010    "CavErr[1032010]: VUsers license limit exceeded for %u VUsers, "\
                           "limit was %u VUsers(including grace VUsers) only.\n"\
                           "Contact Cavisson Account Representative (US +1-800-701-6125) "\
                           "to increase VUsers license limit. \n%s" 

/* ----- || End: Running Test/Session Errors (ErrCode: 103xxxx) || ------ */
//===============================================================================

/* ----- || Start: RBU Errors (ErrCode: 104xxxx) || ------ */

/* ----- || End: RBU Errors (ErrCode: 104xxxx) || ------ */
//===============================================================================

/* ----- || Start: RTC Errors (ErrCode: 105xxxx) || ------ */

/* ----- || End: RTC Errors (ErrCode: 105xxxx) || ------ */
//===============================================================================

/* ----- || Start: Alert Msg (ErrCode: 1017xxx) || ------ */

#define ALERT_MSG_1017001 "AlertMsg[10170001]: Generator '%s', ip '%s' for %s got failed"
#define ALERT_MSG_1017002 "AlertMsg[10170002]: CVM id '%d', ip '%s' for %s got failed"
#define ALERT_MSG_1017003 "AlertMsg[10170003]: CVM id '%d' of Generator '%s', ip '%s' for %s got failed"
#define ALERT_MSG_1017004 "AlertMsg[10170004]: Ignoring generator '%s', ip '%s' for %s due to delay in '%d' progress report"
#define ALERT_MSG_1017005 "AlertMsg[10170005]: Getting delay from generator %s, ip %s, delay_time %s for data connection"
#define ALERT_MSG_1017006 "AlertMsg[10170006]: Enqueuing progress report# %d to queue:%d, due to delay of progress report# %d"

/* ----- || End: RTC Errors (ErrCode: 1017xxx) || ------ */
//===============================================================================

//===============================================================================

/* ----- || Start: GDF Errors (ErrCode: 1013xxx) || ------ */
  
#define CAV_ERR_1013001 "CavErr[1013001]: dataType {%d} in GDF is not correct.\n"

#define CAV_ERR_1013002 "CavErr[1013002]: Cannot open tmp gdf file %s .\n"
 
#define CAV_ERR_1013003 "CavErr[1013003]: Cannot open GDF file '%s' .\n"

#define CAV_ERR_1013004 "CavErr[1013004]: Number of fields are not correct in Info line in GDF, line = %s.\n"

#define CAV_ERR_1013005 "CavErr[1013005]: groupType is not scaler for rpGroupID = %d.\n"

#define CAV_ERR_1013006 "CavErr[1013006]: groupType is not vector for rpGroupID = %d.\n"

#define CAV_ERR_1013007 "CavErr[1013007]: popen failed for command %s. Error = %s .\n"

#define CAV_ERR_1013008 "CavErr[1013008]: pclose failed for command nsu_get_errors. %s.\n"

#define CAV_ERR_1013009 "CavErr[1013009]: This graph should not be vector\n rptid = %d. \n"

#define CAV_ERR_1013010 "CavErr[1013010]: This group should not be vector\n rptid = %d. \n"

#define CAV_ERR_1013011 "CavErr[1013011]: groupType {%s} in GDF is not correct.\n "

#define CAV_ERR_1013012 "CavErr[1013012]: Could not create table entry for Group_Info.\n"

#define CAV_ERR_1013013 "CavErr[1013013]: Could not create table entry for Graph_Info.\n"

#define CAV_ERR_1013014 "CavErr[1013014]: Numbers of fields in Graph line is not 14. Line = %s .\n"

#define CAV_ERR_1013015 "CavErr[1013015]: Number of Graphs (%d) in gdf is more than %d.\nExiting..\n"

#define CAV_ERR_1013016 "CavErr[1013016]: Invalid line Between Graphs lines {%s} in GDF ID %d .\n"

#define CAV_ERR_1013017 "CavErr[1013017]: Number of graphs %d not equals to total Number of graphs %d defined in GDF ID %d.\n"

#define CAV_ERR_1013018 "CavErr[1013018]: (set_rtg_index_in_cm_info): This group should not be vector\n rptid = %d.\n"

#define CAV_ERR_1013019 "CavErr[1013019]: Cannot read Group line: number of fields are not correct.\n"

#define CAV_ERR_1013020 "CavErr[1013020]: Numbers of graphs should not be 0 in GDF ID %d.Hence,exiting from the test\n"

#define CAV_ERR_1013021 "CavErr[1013021]: Found duplicate Group Id [%d] for Group [%s] and Group [%s].\n"

#define CAV_ERR_1013022 "CavErr[1013022]: Report Group Id is not Correct line = %s .\n"

#define CAV_ERR_1013023 "CavErr[1013023]: Group Type is not Correct in line = %s .\n"

#define CAV_ERR_1013024 "CavErr[1013024]: Invalid line in GDF. No group or graph is found in line: %s\n"

#define CAV_ERR_1013025 "CavErr[1013025]: Unable to open diff GDF file %s. \n "

#define CAV_ERR_1013026 "CavErr[1013026]: Can not create new rtgmessage.dat due to Error in runing command [%s]. \n"

#define CAV_ERR_1013027 "CavErr[1013027]: Group ID %u is present in both Custom Monitor (%s) and Dynamic Vector Monitor (%s).\n"

#define CAV_ERR_1013028  "CavErr[1013028]: Unable to distribute of data file '%s' over CVM. Format of control file '%s' is not valid."

#define CAV_ERR_1013029  "CavErr[1013029]: Unable to distribute of data file '%s' over CVM. Format of control file '%s' is not valid." \
                         " Remove control file"

#define CAV_ERR_1013030  "CavErr[1013030]: Unable to distribute of data file '%s' over CVM. Format of last file '%s' is not valid."

/* ----- || End: GDF Errors (ErrCode: 1013xxx) || ------ */
//===============================================================================



//===============================================================================

/* ----- || Start: Monitor Errors (ErrCode: 105xxxx) || ------ */

//------------------------------------------------ns_monitoring.c Error--------------------------------------------------------------------------

#define CAV_ERR_1060001 "CavErr[1060001]: Cannot send a signal to logging_writer .\n"

#define CAV_ERR_1060002 "CavErr[1060002]: Unable to create rbu_logs directories - snap_shots, screen_shot and harp_files.\n "

#define CAV_ERR_1060003 "CavErr[1060003]: Unable to create lighthouse directory.\n"

#define CAV_ERR_1060004 "CavErr[1060004]: Unable to create performance_trace directory.\n"

#define CAV_ERR_1060005 "CavErr[1060005]: Cannot creat logging files.\n"

#define CAV_ERR_1060006 "CavErr[1060006]: mktime method failed. Couldn't get partition_start_time = %ld or partition_midnight_epoch = %ld.\n"

#define CAV_ERR_1060007 "CavErr[1060007]: Cannot open file = %s.\n"

#define CAV_ERR_1060008 "CavErr[1060008]: Cannot open file '%s', Error = %s .\n"

#define CAV_ERR_1060009 "CavErr[1060009]: Unable to create partition  directory in NetCloud directory.\n"

#define CAV_ERR_1060010 "CavErr[1060010]: Cannot open rtgMessage file pointer for generator idx = %d, File = %s, error = %s.\n"

#define CAV_ERR_1060011 "CavErr[1060011]: Cannot create testrun.gdf file in new partiton. Error = %s.\n" 

#define CAV_ERR_1060012 "CavErr[1060012]: Invaid number of arguments.\n"\
                                         "Usage: ENABLE_PROGRESS_REPORT <reporting mode>\n"

//--------------------------------------------ns_dynamic_vector_monitor.c Error------------------------------------------------------------------

#define CAV_ERR_1060013 "CavErr[1060013]: DYNAMIC_VECTOR_MONITOR is not configured properly.\n"

//--------------------------------------------ns_custom_monitor.c Error -------------------------------------------------------------------------                                            
#define CAV_ERR_1060014 "CavErr[1060014]: Connection Error for Custom/Standard monitor - %s(%s) failed. Test Run Canceled.\n"

#define CAV_ERR_1060015 "CavErr[1060015]: Received Error on NDC data connection. Message = %s .\n"


//---------------------------------------------ns_runtime_changes_monitor.c & Others----------------------------------------------------------------------

#define CAV_ERR_1060017 "CavErr[1060017]: Cannot open diff file %s .\n"

#define CAV_ERR_1060018 "CavErr[1060018]: Cannot open gdf file %s for monitor %s .\n"

#define CAV_ERR_1060019 "CavErr[1060019]: No Tunnels are defind in Scenario file .\n" 

#define CAV_ERR_1060020 "CavErr[1060020]: Could not allocate memory for the report table for data connection.\n"

#define CAV_ERR_1060021 "CavErr[1060021]: [Control Connection]: Progress report size(%d) from client ip(%s), is not same as master progress report size(%d). \nScenario file of the client is not compatible with master Scenario file.\n"

#define CAV_ERR_1060022 "CavErr[1060022]: Failed to run %d of generators out of %d generators .\n"

#define CAV_ERR_1060023 "CavErr[1060023]: Cannot write start message in rtgMessage.dat file.\n"

#define CAV_ERR_1060024 "CavErr[1060024]: All generators got terminated. Hence stopping the test...\n"

#define CAV_ERR_1060025 "CavErr[1060025]: %s:%d Malloc failed for control connection. So, exiting. \n"


#define CAV_ERR_1060026 "CavErr[1060026]: Failed in adding monitor to epoll.\n"

#define CAV_ERR_1060027 "CavErr[1060027]: Connection making failed for monitor %s(%s). So exitting.\n"

#define CAV_ERR_1060028 "CavErr[1060028]: cav.conf is not properly set. \n"

#define CAV_ERR_1060029 "CavErr[1060029]: Duplicate vector name(%s) in a monitor whose graph definition file (GDF) is %s.\n"

#define CAV_ERR_1060030 "CavErr[1060030]: %s keyword must have argument as 0,1 and 2  only.\n"

#define CAV_ERR_1060031 "CavErr[1060031]: Error in adding cmon agent server [%s].\n"

#define CAV_ERR_1060032 "CavErr[1060032]: Cannot open '%s.mprof' file or file is not present in mprof directory.\n"

#define CAV_ERR_1060033 "CavErr[1060033]: Error in creation of auto mon table.\n"

#define CAV_ERR_1060034 "CavErr[1060034]: Server name %s not found in Server.conf.\n"

#define CAV_ERR_1060035 "CavErr[1060035]: Error in adding auto monitor server where server is 127.0.0.1 .\n" 

#define CAV_ERR_1060036 "CavErr[1060036]: Error in adding auto monitor server where server is %s .\n"

#define CAV_ERR_1060037 "CavErr[1060037]: Cannot create auto monitor table .\n"

#define CAV_ERR_1060038 "CavErr[1060038]: Cannot Add auto mon server where server is %s .\n"

#define CAV_ERR_1060039 "CavErr[1060039]: Unknown mode on Generators .\n"

#define CAV_ERR_1060040 "CavErr[1060040]: Invalid arguments. \n"\
                                         "Usage: %s <0/1>.\n"

#define CAV_ERR_1060041 "CavErr[1060041]: CMON_SETTINGS <Server> HB_MISS_COUNT=6;CAVMON_MON_TMP_DIR=/tmp;CAVMON_DEBUG=on; \n"

#define CAV_ERR_1060042 "CavErr[1060042]: No monitor is running on server %s .\n"
 
#define CAV_ERR_1060043 "CavErr[1060043]: HIERARCHICAL mode can not be disable. It is mandatory .\n"

#define CAV_ERR_1060044 "CavErr[1060044]: monitor list is missing for keyword %s. At leat one monitor should be in list.\n"

#define CAV_ERR_1060045 "CavErr[1060045]: All generators got terminated. Hence stopping the test...\n"

#define CAV_ERR_1060046 "CavErr[1060046]: Failed to run %d%% of generators out of %d generators. \n"

#define CAV_ERR_1060047 "CavErr[1060047]: Error in writing start message in rtgMessage.dat file.\n"

#define CAV_ERR_1060048 "CavErr[1060048]: [Control Connection]: Progress report size(%d) from client ip(%s), is not same as master progress report size(%d). \n"\
                        "Scenario file of the client is not compatible with master Scenario file.\n" 

#define CAV_ERR_1060049 "CavErr[1060049]: Cannot find JSON file at %s path \n"

#define CAV_ERR_1060050 "CavErr[1060050]: LPS Port is not given with Keyword LPS_SERVER.\n"

#define CAV_ERR_1060051 "CavErr[1060051]: Keyword LPS_SERVER is not found in lps.conf File.\n"

#define CAV_ERR_1060052 "CavErr[1060052]: Value of keyword LPS_SERVER is not Numeric.\n"

#define CAV_ERR_1060053 "CavErr[1060053]: Invalid Mode for LPS_SERVER.\n"

#define CAV_ERR_1060054 "CavErr[1060054]: nd_port is not numeric for NET_DIAGNOSTICS_SERVER Keyword.\n"

#define CAV_ERR_1060055 "CavErr[1060055]: Invalid ND profile name for NET_DIAGNOSTICS_SERVER Keyword.\n"

#define CAV_ERR_1060056 "CavErr[1060056]: mode is empty for NET_DIAGNOSTICS_SERVER Keyword.\n"

#define CAV_ERR_1060057 "CavErr[1060057]: Keyword for NET_DIAGNOSTICS_SERVER not found in conf file.\n"

#define CAV_ERR_1060058 "CavErr[1060058]: nd_port is empty for NET_DIAGNOSTICS_SERVER Keyword.\n"

#define CAV_ERR_1060059 "CavErr[1060059]: Server (%s) not present in topolgy, for the Monitor (%s).\n"

#define CAV_ERR_1060060 "CavErr[1060060]: Wrong option for Custom Monitor \n%s:\n%s \n"\
                                          "Usage:\n"\
                                          "CUSTOM_MONITOR <Create Server IP {NO | NS | Any IP}> <GDF FileName> <Vector Name> <Option {Run Every Time (1) | Run Once (2)}> <Program Path> [Program Arguments]\n"\
                                          "Example:\n"\
                                          "CUSTOM_MONITOR 192.168.18.104 cm_vmstat.gdf VMStat 2 /opt/cavisson/monitors/samples/cm_vmstat .\n"

#define CAV_ERR_1060061 "CavErr[1060061]: Wrong print_mode for Custom Monitor \n%s:\n%s \n"\
                                          "Usage:\n"\
                                          "CUSTOM_MONITOR <Create Server IP {NO | NS | Any IP}> <GDF FileName> <Vector Name> <Option {Run Every Time (1) | Run Once (2)}> <Program Path> [Program Arguments]\n"\
                                          "Example:\n"\
                                          "CUSTOM_MONITOR 192.168.18.104 cm_vmstat.gdf VMStat 2 /opt/cavisson/monitors/samples/cm_vmstat .\n"

#define CAV_ERR_1060062 "CavErr[1060062]: Wrong access for Custom Monitor \n%s:\n%s \n"\
                                          "Usage:\n"\
                                          "CUSTOM_MONITOR <Create Server IP {NO | NS | Any IP}> <GDF FileName> <Vector Name> <Option {Run Every Time (1) | Run Once (2)}> <Program Path> [Program Arguments]\n"\
                                          "Example:\n"\
                                          "CUSTOM_MONITOR 192.168.18.104 cm_vmstat.gdf VMStat 2 /opt/cavisson/monitors/samples/cm_vmstat .\n"

#define CAV_ERR_1060063 "CavErr[1060063]: GET LOG FILE MONITOR AND LOG_MONITOR can be used when lps_mode is 1.\n"

#define CAV_ERR_1060064 "CavErr[1060064]: Too few arguments for Custom Monitor : %s \n"\
                                          "Usage:\n"\
                                          "CUSTOM_MONITOR <Create Server IP {NO | NS | Any IP}> <GDF FileName> <Vector Name> <Option {Run Every Time (1) | Run Once (2)}> <Program Path> [Program Arguments]\n"\
                                          "Example:\n"\
                                          "CUSTOM_MONITOR 192.168.18.104 cm_vmstat.gdf VMStat 2 /opt/cavisson/monitors/samples/cm_vmstat .\n"

#define CAV_ERR_1060065 "CavErr[1060065]: Duplicate monitor name(%s) whose graph definition file (GDF) is %s.\n "

#define CAV_ERR_1060066 "CavErr[1060066]: Could not create table entry for Monitor List Table .\n"     

#define CAV_ERR_1060067 "CavErr[1060067]: Syntax error in standard_monitors.dat file. At Line %d on '%s'.\n "

#define CAV_ERR_1060068 "CavErr[1060068]: In standard_monitor.dat, monitor type '%s' is incorrect for monitor %s.\n"

#define CAV_ERR_1060069 "CavErr[1060069]: Could not create table entry for Standard Monitor .\n " 

#define CAV_ERR_1060070 "CavErr[1060070]: Cannot open standard monitor.dat %s .\n"

#define CAV_ERR_1060071 "CavErr[1060071]: Mandatory field -M matric is missing for matric buffer.\n"

#define CAV_ERR_1060072 "CavErr[1060072]: Invalid Metric.\n"\
                                          "%s ."\
                                          "Valid Metrics for AppDynamics: ExternalCalls, OAP, BTP.\nFor OracleStats: SQL_REPORT, CACHE_SIZES_STATS, INSTANCE_EFFICIENCY_STATS, LOAD_PROFILE_STATS, MEMORY_STATS, SHARED_POOL_STATS, SYSSTAT, TIME_MODEL_STATS.\n "  

#define CAV_ERR_1060073 "CavErr[1060073]: Too few arguments for Standard Monitor .\n"\
                                          "Monitor buf %s .\n"\
                                          "Usage:\n"\
                                          "STANDARD_MONITOR <Server Name> <Vector Name> <Monitor Name> <Arguments>\n"\
                                          "Where: \n"\
                                          "Server Name is the name of the server where standard monitor will run\n"\
                                          "Vector Name is vector name for the monitor. It should be unique\n"\
                                          "Monitor Name is the name of the Standard Monitor\n"\
                                          "Arguments as per standard monitor. This is optional\n"\
                                          "Examples: \n"\
                                          "        STANDARD_MONITOR 192.168.18.104 VecServer1 SystemStatsLinux 10 .\n" 

#define CAV_ERR_1060074 "CavErr[1060074]: Length of monitor %s is %d which is greater than max %d buffer length.\n"                                          
#define CAV_ERR_1060075 "CavErr[1060075]: In standard_monitor.dat, standard monitor type '%s' is incorrect for vector %s.\n"\
                                         "Please provide valid standard monitor name\n"\
                                         "Please provide valid standard monitor type.\n"\
                                         "Valid standard monitor types are:\n"\
                                         "CM -> For custom monitor.\n"\
                                         "DVM -> For dynamic vector monitor.\n"  

#define CAV_ERR_1060076 "CavErr[1060076]: Server (%s) not present in topolgy.\n"

#define CAV_ERR_1060077 "CavErr[1060077]: Wrong Signature Type for Server Signature '%s'. It should be 'Command' or 'File'.\n"\
                                         " Usage:\n"\
                                         "SERVER_SIGNATURE <Server Name> <Signature Name> <Signature Type {Command | File}> <Command or File Name>\n"\
                                         "Example:\n"\
                                         "SERVER_SIGNATURE 192.168.18.104 DBVerAppServer1 Command get_db_version\n"\
                                         "SERVER_SIGNATURE NS ServerXmlM02AppServer1 File /tmp/server.xml.\n " 

#define CAV_ERR_1060078 "CavErr[1060078]: Could not create table entry for Server Signature Table\n"                

#define CAV_ERR_1060079 "CavErr[1060079]: Too few fields for Server Signature '%s'.\n"\
                                         " Usage:\n"\
                                         "SERVER_SIGNATURE <Server Name> <Signature Name> <Signature Type {Command | File}> <Command or File Name>\n"\
                                         "Example:\n"\
                                         "SERVER_SIGNATURE 192.168.18.104 DBVerAppServer1 Command get_db_version\n"\
                                         "SERVER_SIGNATURE NS ServerXmlM02AppServer1 File /tmp/server.xml.\n "


#define CAV_ERR_1060080 "CavErr[1060080]:Got error from NDC. Closing connection. Error = %s"
#define CAV_ERR_1060081 "CavErr[1060081]:Unable to open file %s. Error: = %s \n"
#define CAV_ERR_1060082 "CavErr[1060082]:TSDB is not configured properly.Error = %s"

//do not use these error as using these in shell as hard coded.
//#define CAV_ERR_1060083 "CavErr[1060083]: Cmon is not running on controller. Please start cmon and try again."

#define CAV_ERR_1060084 "CavErr[1060084]: Multidisk path [%s] had no write permission." 

#define CAV_ERR_1060085 "CavErr[1060085]: G_DATADIR setting is not configured properly.\n"\
                        "Message: %s\n"
    



/* ----- || End: Monitor Errors (ErrCode: 105xxxx) || ------ */
//==============================================================================

#endif
