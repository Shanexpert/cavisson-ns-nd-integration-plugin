#ifndef _NS_KW_USAGE_H_
#define _NS_KW_USAGE_H_

#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_trace_level.h"
#include "ns_exit.h"

#define NS_KW_PARSING_ERR(kw, is_runtime, err_buff, usage_msg, ...) \
{\
  NSTL1(NULL, NULL, "Scenario Settings: %s", kw);\
  NSTL1(NULL, NULL, "%s", usage_msg);\
  NS_RUNTIME_RETURN_OR_INIT_ERROR(is_runtime, err_buff, __VA_ARGS__);\
}

#define AUTO_FETCH_EMBEDDED_USAGE "Usage: G_AUTO_FETCH_EMBEDDED <group_name> <page_name> <fetch_option>\n"\
                                  "Group name can be ALL or any group name used in scenario group\n"\
                                  "Page name can be ALL or name of page for which auto fetch is to be enabled.\n"\
                                  "ALL and particular page combination is not a valid combination (e.g. ALL page1)\n"\
                                  "Fetch option is 0 (default) for disabling and 1 for enabling auto fetching\n"

#define G_PAGE_THINK_TIME_USAGE "Usage: G_PAGE_THINK_TIME <group_name> <page_name> <think_mode> <value1> <value2>\n"\
                                "Group name can be ALL or any group name used in scenario group\n"\
                                "Page name can be ALL or name of page for which page think time is to be enabled.\n"\
                                "ALL and particular page combination is not a valid combination (e.g. ALL page1)\n"\
                                "Think mode:\n"\
                                "0: No think time, value1 and value2 not applicable (Default)\n"\
                                "1: Internet type random, value1 is median time and value2 not applicable.\n"\
                                "2: Constant time, value1 is constant time and value2 not applicable.\n"\
                                "3: Uniform random, value1 is the minimum time and value2 is maximum time.\n"\
                                "4: Custom Delay, value1 is the name of Callback Method.\n"\
                                "Note: Time values (value1 and value2) should be specified in milliseconds.\n"

#define SESSION_PACING_USAGE "Usage: G_SESSION_PACING <group_name> <pacing_mode> <pacing_time_mode> <value1> [<value2>]\n"\
                             "<group_name> It can be ALL or any valid group name\n"\
                             "<pacing_modes> Different Session pacing modes: "\
                             "<0(No pacing)>"\
                             "<1(Pacing after completion of previous session)>"\
                             "<2(Pacing every interval)>\n"\
                             "<time_modes> Different session pacing time mode:"\
                             "<0(Constant time)> <fixed delay of (value1) seconds>" \
                             "<1(Random time (Internet type distribution))> <average intervals of (value1) seconds>"\
                             "<2(Uniform random time)> <delay of (value1(MinValue)) to (value2(MaxValue)) seconds>\n"\
                             "For example to enable constant pacing of 10 seconds for all groups, use following keyword:\n"\
                             "G_SESSION_PACING ALL 1 0 10000\n"

#define INLINE_DELAY_USAGE "Usage: G_INLINE_DELAY <group_name> <page_name> <delay_mode> <value1> <value2>\n"\
                           "Group name can be ALL or any group name used in scenario group\n"\
                           "Page name can be ALL or name of page for which inline delay time is to be enabled.\n"\
                           "ALL and particular page combination is not a valid combination (e.g. ALL page1)\n"\
                           "DELAY mode:\n"\
                           "0: No delay time, value1 and value2 not applicable (Default)\n"\
                           "1: Internet type random, value1 is median time and value2 is constant delay and value3 for min_limit_time and value4 for max_limit_time.\n"\
                           "2: Constant time, value1 is constant time and value2 not applicable.\n"\
                           "3: Uniform random, value1 is the minimum time and value2 is maximum time.\n"\
                           "4: Custom Delay, value1 is the name of Callback Method.\n"\
                           "Note: Time values (value1 and value2) should be specified in milliseconds.\n"

#define G_VUSER_TRACE_USAGE "Usage: G_VUSER_TRACE <group name> <mode>"\
                            "This keyword is used to enable or disable the virtual user trace for the user."\
                            "Group name can be ALL or any group name used in scenario group"\
                            "Mode: Mode for enable and disable the user tracing. It can only be 0 or 1"\
                            "1 - Tracing is enabled (default)"\
                            "0 - Tracing is disabled"

#define G_TRACING_USAGE "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n"\
                    "Here trace-session-limit is used to define how many sessions, one need to be dump\n"\
                    "0 : unlimited(default)\n"\
                    "1 : Limit defined in percentage\n"\
                    "2 : Limit defined in number\n"\
                    "Where trace-session-limit-value is used to define pct in case of trace-session-limit 1 and number of sessions for trace-session-limit value is 2\n"

#define G_REPORTING_USAGE "Usage: G_REPORTING <group name> <report-level(1/2)> <ddr percentage limit>\n"\
                          "Where report level is:\n"\
                          "1 - Summary, Progress report and Runtime graphs (Default)\n"\
                          "2 - Summary, Progress, Runtime graphs and Drill Down report\n"\
                          "ddr percentage limit(optional):\n"\
                          "Specifies ddr percentage to be capture\n"

#define G_SHOW_RUNTIME_RUNLOGIC_PROGRESS_USAGE "Usage: G_SHOW_RUNTIME_RUNLOGIC_PROGRESS <group_name> <mode>\n"\
                                               "This keyword is used to show the flow of Virtual Users across the flows used in test.\n"\
                                               "Group-Name: Provided group name (Default ALL)\n"\
                                               "Mode: Mode for enable/disable the G_SHOW_RUNTIME_RUNLOGIC_PROGRESS. It can only be 0, 1\n"\
                                               "0 - Disable virtual user flow graph data.(default)\n"\
                                               "1 - Enable virtual user flow graph data.\n"

#define PROGRESS_MSECS_USAGE "Usage: PROGRESS_MSECS <progress interval>\n"

#define DEBUG_TRACE_USAGE "Usage: DEBUG_TRACE <enable/disable>\n"

#define G_SSL_CERT_FILE_PATH_USAGE "Usages:\n"\
                                 "Synatx - G_SSL_CERT_FILE_PATH <Grp> <CERT_File> \n"\
                                 "Where:\n"\
                                 "Grp          : Any valid Group or ALL \n"\
                                 "Cert_file    : Cert file will have 'certificate file' stored at 'cert' directory or relative path from cert directory\n"

#define G_SSL_KEY_FILE_PATH_USAGE "Usages:\n"\
                                  "Synatx - G_SSL_KEY_FILE_PATH <Grp> <KEY_File> \n"\
                                  "Where:\n"\
                                  "Grp          : Any valid Group or ALL \n"\
                                  "key_file    : Key file will have 'key file' stored at 'cert' directory or relative path from cert directory\n"

#define G_CIPHER_LIST_USAGE "Usages: G_CIPHER_LIST <grp_name> <cipher_list name>"

#define G_AVG_SSL_REUSE_USAGE "Usages: G_AVG_SSL_REUSE <grp_name> <Average ssl reuse%>"

#define G_SSL_CLEAN_CLOSE_ONLY_USAGE "Usages: G_SSL_CLEAN_CLOSE_ONLY <grp_name> <enable/disable>"

#define G_SSL_SETTINGS_USAGE "Usages: G_SSL_SETTINGS <grp_name> <enable/disable>"

#define G_SSL_RENEGOTIATION_USAGE "Usage: unable to start_ssl_renegotiation for  <mode>\n"\
                                  "Where:\n"\
                                  "group : Any valid group\n"\
                                  "mode : Is used to specify whether user want to start G_SSL_RENEGOTIATION  or do not want G_SSL_RENEGOTIATION:\n"

#define G_TLS_VERSION_USAGE "Usage: G_TLS_VERSION <grp> <version>\n"\
                            "Where:\n"\
                            "group can be any valid group\n"\
                            "version can be given according to the version need to be used as sslv2_3\n"

#define SAVE_NVM_FILE_PARAM_VAL_USAGE "Usages:\n"\
                                      "Syntax - SAVE_NVM_FILE_PARAM_VAL <mode>\n"\
                                      "Where:\n"\
                                      "  mode - 0 or 1\n"\
                                      "         0 - disabled (Default)\n"\
                                      "         1 - enabled, This will save distributed data over the nvm at path"\
                                      "             ~/TRxx/partition_idx/scripts/<script_name>/<Data File>.<first_paramter>.<nvm_id>.<mode>\n"

#define PERCENTILE_REPORT_USAGE "Usage: PERCENTILE_REPORT <enable/disable> <mode> <interval>\n"\
                                "Percentile_report: This keyword is used to genrerate enable or disable the Percentile Report.\n"\
                                "0 - Disable the <percentile report>\n"\
                                "1 - Enable the <percentile report> (default).\n"\
                                "Mode: <Mode> for  It can only be 0, 1 & 2\n"\
                                "0 - This mode is to generate for total run\n"\
                                "1 - This mode is to generate for all phases\n"\
                                "2 - This mode is to generate for interval (default)\n"\
                                "Interval: default interval is 300000 ms\n"

#define URL_PDF_USAGE "Usage: URL_PDF <url_pdf name>"

#define PAGE_PDF_USAGE "Usage: PAGE_PDF <page_pdf name>"

#define SESSION_PDF_USAGE "Usage: SESSION_PDF <session_pdf name>"

#define TRANSACTION_TIME_PDF_USAGE "Usage: TRANSACTION_TIME_PDF <transaction_time_pdf name>"

#define TRANSACTION_RESPONSE_PDF_USAGE "Usage: TRANSACTION_RESPONSE_PDF <transaction_response_pdf name>"

#define ENABLE_LOG_MGR_USAGE "Usage: ENABLE_LOG_MGR <enable/disable> <port>"

#define EVENT_LOG_USAGE "Usage: EVENT_LOG <Severity> <Filter mode>"

#define G_PROXY_EXCEPTIONS_USAGE "Usage:\n"\
                                 "G_PROXY_EXCEPTIONS <GroupName|ALL> <Bypass> <Exception_List>\n"\
                                 "\tGroup Name: This field can have a valid group name or ALL.\n"\
                                 "\tBypass: Can have two valid values:\n"\
                                 "\t\t0: Do not bypass proxy server for local addresses\n"\
                                 "\t\t1: Bypass proxy server for local addresses\n"\
                                 "\tException List: Can have a valid semicolon separated exceptions.\n\tExceptions can be any of the following \n"\
                                 "\t\ti.e 1) Domain name/Host name (www.cavisson.com)\n"\
                                 "\t\t    2) Ip address of host names that is resolved (67.218.96.251)\n"\
                                 "\t\t    3) Port number (:8800)\n"\
                                 "\t\t    4) IP & Port combination(67.218.96.251:8800)\n"\
                                 "\t\t    5) Subnet (192.168.1.0/24 or 2001::10:10:10:10/64)\n"


#define AUTO_SCALE_CLEANUP_SETTING_USAGE "Usage: AUTO_SCALE_CLEANUP_SETTING <Enable/disable> <cleanup indices time>"

#define G_DEBUG_USAGE "Usage: G_DEBUG <grp_name> <debug level>"

#define G_MODULEMASK_USAGE "Usage: G_MODULEMASK <grp_name> <modulemask name>" 

#define G_KA_TIME_MODE_USAGE "Usage: G_KA_TIME_MODE <grp_name> <Keep Alive Timeout option number>"

#define G_NUM_KA_USAGE "Usage: G_NUM_KA <grp_name> <Average Keep Alive Requests minimum> <Average Keep Alive Requests maximum>" 

#define G_ON_EURL_ERR_USAGE "Usage: G_ON_EURL_ERR <grp_name> <enable>"

#define G_USE_RECORDED_HOST_IN_HOST_HDR_USAGE "Usage: G_USE_RECORDED_HOST_IN_HOST_HDR <grp_name> <enable>"

#define G_DISABLE_ALL_HEADER_USAGE "Usage: G_DISABLE_ALL_HEADER <grp_name> <enable>"

#define G_USE_SRC_IP_USAGE "Usage: G_USE_SRC_IP <grp_name> <ip_mode> <ip_list_mode_file>"

#define AUTO_REDIRECT_USAGE "Usage: AUTO_REDIRECT <Enable redirection upto depth> <Enable/Disable>"

#define G_HTTP2_SETTINGS_USAGE "Usages: G_HTTP2_SETTINGS <grp_name> <enable_push> <max_concurrent_streams> <initial_window_size> <max_frame_size> <header_table_size"\
                               "grp_name give any group name or ALL .\n"\
                               "<enable_push> is to enable(1)/disable(0) Server Push\n"\
                               "<max_concurrent_streams> number of streams that the sender permits the receiver to create\n"\
                               "<initial_window_size> Indicates the sender's initial window size (in octets) for stream-level flow control\n"\
                               "<max_frame_size> the size of the largest frame payload that the sender is willing to receive\n"\
                               "<header_table_size> the maximum size of the header compression table used to decode header blocks, in octets\n"

#define G_HTTP_MODE_USAGE "Usages: G_HTTP_MODE <grp_name> <mode>"\
                          "  This keyword is used to set HTTP Mode.\n"\
                          "    in grp_name give any group name or ALL .\n"\
                          "    Mode: Mode for G_HTTP_MODE 0, 1 or 2\n"\
                          "      0 - HTTP mode AUTO \n"\
                          "      1 - HTTP mode HTTP1\n"\
                          "      2 - HTTP mode HTTP2\n"

#define G_ENABLE_REFERER_USAGE "Usage: G_ENABLE_REFERER <grp_name> <enable/disable> <change referer on redirection>."

#define G_M3U8_SETTING_USAGE "Usages:\n Syntax - G_M3U8_SETTING <GRP_NAME> <MODE> <BANDWIDTH>\n"\
                             "Where:\n"\
                             "group_name    - G1, G2 etc \n"\
                             "mode          - 0 Disable (Default) \n"\
                             "              - 1 Enable \n"\
                             "bandwidth     - 0 or desire bandwidth\n"

#define G_HTTP_AUTH_NTLM_USAGE "Usage:\n"\
                               "   G_HTTP_AUTH_NTLM <group_name> <enable_ntlm> [<ntlm_version>] [<domain>] [<workstation>]\n"\
                               "   enable_ntlm:\n"\
                               "     0: Disabled\n"\
                               "     1: Enabled (default)\n"\
                               "   ntlm_version:\n"\
                               "     0: NTLM\n"\
                               "     1: NTLM2\n"\
                               "     2: NTLMv2 (default)\n"\
                               "   domain: Client domain name\n"\
                               "   workstation: Client workstation name\n"

#define G_HTTP_AUTH_KERB_USAGE "Usage:\n"\
                               "   G_HTTP_AUTH_KERB <group_name> <enable_kerb> \n"\
                               "    enable_kerb:\n"\
                               "     0: Disabled\n"\
                               "     1: Enabled (default)\n"

#define G_HTTP_CACHING_USAGE "Usage: G_HTTP_CACHING <group name> <pct-user>  <mode> <client-freshness-constraints>\n"\
                             "  Group name can be ALL or any group name used in scenario group\n"\
                             "  Pct-users can only from 0 to 100\n"\
                             "  Mode can only be 0 or 1\n"\
                             "  Client Freshness Constraint can be ON(1) or OFF(0)\n"

#define G_HTTP_CACHE_TABLE_SIZE_USAGE "  Usage: G_HTTP_CACHE_TABLE_SIZE <group name> <mode> <value>\n"\
                                      "  This keyword is used to tune the size of cache table size for a user\n"\
                                      "  for optimizing memory usage.\n"\
                                      "  Smaller value will reduce the size of table but may cause more collisions"\
                                      "  so use value as per the application\n"\
                                      "    Group name can be ALL or any group name used in scenario group\n"\
                                      "    Mode: Mode for allocation of cache table size for a user. It can only be 0 or 1\n"\
                                      "      0 - Cache table size is fixed using the value field (default)\n"\
                                      "      1 - Cache table size is based on previus sessions cache entries\n"\
                                      "    Value should be an integer between 1 and 65536. Default is 512. Value should be 2^n. If not netstorm will round up to the next 2^n value. For example, if value give is 120 then netstorm will use 128 (i.e. 2^7)\n"

#define G_HTTP_CACHE_MASTER_TABLE_USAGE "  Usage: G_HTTP_CACHE_MASTER_TABLE <group name> <mode> <master table name> <master table size>\n"\
                                        "  This keyword is used to enable the master table mode in cache\n"\
                                        "  for optimizing memory usage.\n"\
                                        "    Group name can be ALL or any group name used in scenario group\n"\
                                        "    Group name can be ALL or any group name used in scenario group\n"\
                                        "      0 - Dont use master table to save the response\n"\
                                        "      1 - Use master table to save the response\n"\
                                        "    Master table name: This is any valid name\n"\
                                        "Master table size: This should be an integer between 1 and 65536. Default is 512. Value should be 2^n. If not netstorm will round up to the next 2^n value. For example, if value give is 120 then netstorm will use 128 (i.e. 2^7)\n"

#define G_ENABLE_NETWORK_CACHE_STATS_USAGE "  Usage: G_ENABLE_NETWORK_CACHE_HEADERS <group name> <mode> <Consider Refresh Hit As Hit flag> <Type>\n"\
                                           "  This keyword is used to enable or disable the network cache graphs for the users of particular group.\n"\
                                           "  This keyword is used to enable or disable the network cache graphs for the users of particular group.\n"\
                                           "    Group name can be ALL or any group name used in scenario group\n"\
                                           "    Mode: Mode for enable/disable the network cache graphs. It can only be 0 or 1\n"\
                                           "      1 - Network Cache is enabled\n"\
                                           "      0 - Network Cache is disabled (default)\n"\
                                           "    Refresh Hit Flag : Can have two values 0/1.\n"\
                                           "      0 - Consider TCP_REFRESH_HIT state as a HIT.\n"\
                                           "      1 - Consider TCP_REFRESH_HIT state as a MISS.\n"

#define G_MAX_CON_PER_VUSER_USAGE "  Usage: G_MAX_CON_PER_VUSER <group> <mode> <max_con_per_svr_http_1.0> <max_con_per_svr_http_1.1> <max_proxy_per_svr_http_1.0> <max_proxy_per_svr_http_1.1> <max_con_per_vuser>\n"\
                                  "  This keyword is use to set connection settings\n"\
                                  "  here: \n"\
                                  "    group: Group-name can be ALL or any group name used in scenario group\n"\
                                  "    mode: Mode for deciding connection settings. It can only be 0 or 1\n"\
                                  "          0 - Default connection settings(default)\n"\
                                  "          1 - Browser settings, remaining fields becomes invalid in mode 1\n"\
                                  "    max_con_per_svr_http_1.0: Maximum number of connection per server for HTTP 1.0\n"\
                                  "    max_con_per_svr_http_1.1: Maximum number of connection per server for HTTP 1.1\n"\
                                  "    max_proxy_per_svr_http_1.0: Maximum number of proxy connection per server for HTTP 1.0\n"\
                                  "    max_proxy_per_svr_http_1.1: Maximum number of proxy connection per server for HTTP 1.1\n"\
                                  "    max_con_per_vuser: Use to set number of virtual connections per users\n"

#define G_END_TX_NETCACHE_USAGE "Usage:\n"\
                                "   G_END_TX_NETCACHE  <group-name> <mode>\n"\
                                "   mode:\n"\
                                "     0: Do not end transaction based on NetCache hit (Default)\n"\
                                "     1: End transaction based on NetCache hit\n"

#define G_SEND_NS_TX_HTTP_HEADER_USAGE "Usage:\n"\
                                       "   G_SEND_NS_TX_HTTP_HEADER <group_name> <mode> [Tx Variable] [<HTTP Header name>]\n"\
                                       "   mode:\n"\
                                       "     0: Do not send\n"\
                                       "     1: Send for Main URL only\n"\
                                       "     2: Send for both Main and Inline Urls\n"\
                                       "   Tx Variable: NS variable containing transaction name to be used in http header. Default value will be last started transaction name.\n"\
                                       "   HTTP Header name: HTTP header that will be send in request. Default value of http header name is CavTxName\n"

#define G_SAVE_LOCATION_HDR_ON_ALL_RSP_CODE_USAGE "Usage: G_SAVE_LOCATION_HDR_ON_ALL_RSP_CODE <group name> <0|1>\n"\
                                                  "0: To disable the keyword.(default value)\n"\
                                                  "1: To enable saving location url value on all response code\n"

#define G_PROXY_SERVER_USAGE "Group Name:This field can have a valid group name or ALL.\n"\
                             " Mode: Mode can have three values:\n"\
                             "Mode 0: No proxy\n"\
                             "Mode 1: Use the proxy from system settings\n"\
                             "Mode 2: Use manual proxy configuration\n"\
                             "Address List: Can have a valid proxy address and is used for mode 2\n"\
                             "1) HTTP/HTTPS protocol (e.g http=192.168.1.66;https=cavissonproxy.com)\n"\
                             "2) We can specify all standalone(e.g all=192.168.1.68)\n"

#define G_PROXY_AUTH_USAGE  "Usage:\n"\
                            "G_PROXY_SERVER <Group Name|ALL> <Mode> <Address List>\n"\
                            "Group Name:This field can have a valid group name or ALL.\n"\
                            "mode: Mode can have three values:\n"\
                            "Mode 0: No proxy\n"\
                            "Mode 1: Use the proxy from system settings\n"\
                            "Mode 2: Use manual proxy configuration\n"\
                            "Address List: Can have a valid proxy address and is used for mode 2\n"\
                            "1) HTTP/HTTPS protocol (e.g http=192.168.1.66;https=cavissonproxy.com)\n"\
                            "2) We can specify all standalone(e.g all=192.168.1.68)\n"

#define SHOW_IP_DATA_USAGE  " Usage: SHOW_IP_DATA <mode>\n"\
                            " This keyword is used to show no of requests sent for perticular IP.\n"\
                            " Mode: Mode for enable/disable the IP data. It can only be 0, 1\n"\
                            " 2 - Disable IP graph data for any IP.(default)\n"\
                            " 1 - Enable IP graph data.\n"

#define G_HTTP_BODY_CHECKSUM_HEADER_USAGE "Usages:\n"\
                                          "G_HTTP_BODY_CHECKSUM_HEADER <Group>  <mode> <add header in case of empty body> <add prefix and suffix> <header name> <prefix for the http_body> <suffix for the http_body>\n"\
                                          "Where:\n"\
                                          "  group_name                    - group name can be ALL or any valid group\n"\
                                          "  mode                            0 - disable keyword(Default)\n"\
                                          "                                  1 - enable keyword\n"\
                                          "  if_body_empty                 - disabled.\n"\
                                          "  if_pfx_sfx                    - provide prefix_suffix to checksum string.\n"\
                                          "                                  0 - disable prefix_suffix\n"\
                                          "                                  1 - enabled, for sending headers \n"\
                                          "  header_name                   - X-Payload-Confirmation\n"\
                                          "  body_pfx                      - '@'\n"\
                                          "  body_sfx                      - '@'\n"

#define SHOW_GROUP_DATA_USAGE "Usage: SHOW_GROUP_DATA <mode>\n"\
                              " This keyword is used to enable or disable the GROUP graph data.\n"\
                              " Mode: Mode for enable/disable the GROUP data. It can only be 0, 1\n"\
                              "  0 - Disable GROUP graph data for any group.(default)\n"\
                              "  1 - Enable GROUP graph data.\n"

#define ENABLE_PAGE_BASED_STATS_USAGE "Usage: ENABLE_PAGE_BASED_STATS <mode>\n"\
                                      "This keyword is used to enable or disable the  Page Based Stat graph data.\n"\
                                      " Mode: Mode for enable/disable the Page Based Stat. It can only be 0, 1\n"\
                                      "0 - Disable Page based Stat.(default)\n"\
                                      "1 - Enable Page based Stat.\n"

#define READER_RUN_MODE_USAGE "Usage: READER_RUN_MODE <mode> <time>\n"\
                              "This keyword is used to change READER_RUN_MODE.\n"\
                              "Mode: Mode for READER_RUN_MODE 0,1,3. 2 is for future use not using right now.\n"\
                              "0 - \n"\
                              "1 - \n"\
                              "3 - \n"\
  "Time: Time is in seconds. This is optional, default value is 10 secs. This is frequency at which reader need to write data in csv file.\n"

#define SHOW_SERVER_IP_DATA_USAGE "Usage: SHOW_SERVER_IP_DATA <mode>\n"\
                                  "This keyword is used to enable or disable the dynamic server ip graph data.\n"\
                                  "Mode: Mode for enable/disable the SHOW_SERVER_IP_DATA. It can only be 0, 1\n"\
                                  "0 - Disable SERVER IP graph data.(default)\n"\
                                  "1 - Enable SERVER IP graph data.\n"

#define PAGE_AS_TRANSACTION_USAGE "Usage: PAGE_AS_TRANSACTION <value> <transaction name format> <parent sample mode>\n"\
                                  "Where value is:\n"\
                                  "0 - Do not consider page as transaction (Default)\n"\
                                  "1 - Consider all page status as one transaction (Default)\n"\
                                  "2 - Consider success and failure as two different transactions\n"\
                                  "3 - Consider diferent page status as different transactions\n"\
                                  "transaction name format is an optional field:\n"\
                                  "0 - The name of transaction begin with 'tx_<page_name>'(Default)\n"\
                                  "1 - The name of transaction will be page name\n"\
                                  "parent sample mode is an optional field(used for jmeter script):\n"\
                                  "0 - Transaction has all the sample data as sub samples(pages) (Default Option)\n"\
                                  "1 - Transaction do not have sample data as sub samples(pages)\n"

#define URI_ENCODING_USAGE "Usage: URI_ENCODING <encode_pipe> <encode_char_in_uri> <encode_char_in_query>\n"\
                           "<encode_pipe> can be 0\n <encode_pipe> can be 1\n <encode_pipe> can be 2\n <encode_pipe> can be 3\n"\
                           "<encode_char_in_uri> has any character which user want to encode in uri.\n It has value NONE or any character\n"\
                           "<encode_char_in_query> has any character which user want to encode in query param.\n It has value NONE or SAME or any character\n"

#define AUTO_COOKIE_USAGE  "Usage: AUTO_COOKIE <mode> <expires_mode>\n"\
                           "Where mode is:\n"\
                           "0 - Disable auto cookie, i.e. do not use Auto cookie (Default)\n"\
                           "1 - Use auto cookie with name, path and domain\n"\
                           "2 - Use auto cookie with name only\n"\
                           "3 - Use auto cookie with name and path only\n"\
                           "4 - Use auto cookie with name and domain only\n"\
                           "Where expires_mode is:\n"\
                           "0 - Do not use expires attribute.(Default)\n"\
                           "1 - If expiry is in the past then delete cookie"

#define PARTITION_SETTINGS_USAGE "Usage: PARTITION_SETTINGS <PARTITION_CREATION_MODE[1/2]> <SYNC_ON_FIRST_SWITCH[0/1]> <PARTITION_SWITCH_DURATION><Hh/Mm>\n"\
                                 "PARTITION_CREATION_MODE:\n\t1 - Create partition based on time;\n\t2 - For auto mode.\n"\
                                 "SYNC_ON_FIRST_SWITCH:\n\t0 - Don't sync partition duration with midnight;\n\t1 - Sync partition duration\n"\
                                 "PARTITION_SWITCH_DURATION:\n\t<duration in mins><M/m>\n\t<duration in hours><H/h>\n"

#define G_SCRIPT_MODE_USAGE  "Usage: G_SCRIPT_MODE <group name> <mode> <stack_size> <free_stack>\n"\
                           "  Group name can be ALL or any group name used in scenario group\n"\
                           "  Mode: Mode of script execution\n"\
                           "    0 - Legacy Script mode execution  - \n"\
                           "    1 - Run script in user context - \n"\
                           "    2 - Run script in Thread Per User - \n"\
                           "  Stack_size is the size of stack allocate for virtual used.\n"\
                           "  It is in KB. It is used only in the case of mode 1\n"\
                           "  free_stack is to provide the control for freeing the stack. It is used only in the case of mode 1\n"

#define G_SESSION_RETRY_USAGE  "Usages:\n"\
                               "G_SESSION_RETRY <group> <num_retrires> <retry_interval> \n"\
                               "Where:\n"\
                               "  group_name         - provide group_name (Defult ALL)\n"\
                               "  num_retries        - provide number of retries\n"\
                               "                       0       - disabled(Default)\n"\
                               "                       1,2,... - enabled\n"\
                               "  retry_interval     - time interval between retries (milliseconds)\n"\
                               "  Eg: G_SESSION_RETRY ALL 3 2000\n"

#define G_BODY_ENCRYPTION_USAGE  "Usages:\n"\
                                 "G_BODY_ENCRYPTION <Group> <encryption_algo> <base64_encode_option> <key> <ivec>\n"\
                                 "Where:\n"\
                                 "  group_name                        - group name can be ALL or any valid group\n"\
                                 "  encryption_algo                   - Encryption algo can be any one of these\n"\
                                 "                                      - NONE \n"\
                                 "                                      - AES_128_CBC \n"\
                                 "                                      - AES_128_CTR \n"\
                                 "                                      - AES_192_CBC \n"\
                                 "                                      - AES_192_CTR \n"\
                                 "                                      - AES_256_CBC \n"\
                                 "                                      - AES_256_CTR \n"\
                                 "  base64_encode_option              - Encoding options are \n"\
                                 "                                      - NONE \n"\
                                 "                                      - KEY_IVEC \n"\
                                 "                                      - BODY \n"\
                                 "                                      - KEY_IVEC_BODY \n"\
                                 "  key                               - For encryption \n"\
                                 "  ivec                              - For encryption \n"

#define G_CONTINUE_ON_PAGE_ERROR_USAGE "Usage: G_CONTINUE_ON_PAGE_ERROR <group_name> <page_name> <value>\n"\
                                       "Group name can be ALL or any group name used in scenario group\n"\
                                       "Page name can be ALL or name of page for which continue on page error to be enabled.\n"\
                                       "ALL and particular page combination is not a valid combination (e.g. ALL page1)\n"\
                                       "Continue on page error value is 0 (default) for disabling and 1 for enabling continue on page error\n"\

#define G_RTE_SETTINGS_USAGE  "Usages:\n"\
                              "Syntax - G_RTE_SETTING <GRP_NAME> <MODE> <PROTO> <GUI>\n"\
                              "Where:\n"\
                              "group_name    - G1, G2 etc \n"\
                  	      "mode          - 0 Disable (Default) \n"\
                  	      "              - 1 Enable \n"\
                  	      "protocol      - 1 for SSH\n"\
                  	      "              - 2 for TN3270\n"\
                  	      "terminal      - 0 for disable\n"\
                  	      "              - 1 for enable\n"

#define ENABLE_SYNC_POINT_USAGE "Usage: ENABLE_SYNC_POINT <0/1> \n"

#define SYNC_POINT_TIME_OUT_USAGE "Usage: SYNC_POINT_TIME_OUT <Overall time out in hh:mm:ss> <Interarrival time out in hh:mm:ss> \n"\
                                  "Overall time out      : default value of overall time out is 10 mins \n"\
                                  "Interarrival time out : default value of inter arrival time out is 60 secs \n"

#define HIERARCHICAL_VIEW_USAGE "Usage: HIERARCHICAL_VIEW <Enable (1)> [Configuration File Name] [vector seperator]\n"\
                                "Example:\n"\
                                "HIERARCHICAL_VIEW 1 /home/cavisson/work/xyz >\n OR \nHIERARCHICAL_VIEW 1\n\n"

#define DISABLE_NS_NO_MONITORS_USAGE "Usage: DISABLE_NS_MONITORS <mon1> <mon2> <mon3> ..."

#define ENABLE_NO_MONITORS_USAGE "Usage: ENABLE_NO_MONITORS  <mode> <server_name> <controller_name>\n"\
                                 "\tmode: 1 - enable, 0 - disable"




#define ENABLE_NS_MONITORS_USAGE "Usage: ENABLE_NS_MONITORS <mode> <iperf monitoring>\n"\
                                      "mode - 1 : active all the monitors [default]\n"\
                                             "0 : inactive all the monitors" \
                                            " 2:active all monitor and start Tcp states monitoring on all generators if Machine Type  NC\n" \
                                                 "Iperf Monitoring : 1-> Apply NetworkBandwidth on all Generator if Machine Type NC\n" \
                                                                    "0-> Disable (ByDefault)"

#define TSDB_SERVER_USAGE "Usage : ENABLE_TSDB_CONFIGURATION <0/1/2>\n" \
                             "This keyword is use to set the TSDB Configuration\n"\
                           "0-Disable, Used RTG for dumping data for in RTG\n"\
                           "1-Enable,Used TSDB for dumping data in TSDB\n"\
                           "2-BOTH,Used both TSDB and RTG for dumping data\n"



#define LPS_SERVER_USAGE "Usage: LPS_SERVER <SERVER> <PORT> <MODE>\n"\
                         "This keyword is use to set the LPS Agent IP address, Port no.and Mode. Mode is optional field, Default value is 2.\n"\
                         "Mode:\n"\
                         "0 -> Disable. will run only Service monitor by setting default ip port for Service monitor.\n"\
                         "1 -> Enable.  will run Service monitor, Special monitor and Log monitor.\n"\
                         "2 -> Enable (default). will run only service monitor. \n"

#define ENABLE_OUTBOUND_CONNECTION_USAGE "Usage: ENABLE_OUTBOUND_CONNECTION <Enable/disable>"

#define MONITOR_PROFILE_USAGE "Usage: MONITOR_PROFILE <mprof_name>"

#define CONTINUE_ON_MONITOR_ERROR_USAGE "Usage: CONTINUE_ON_MONITOR_ERROR <custom/std data monitors mode>  <dynamic vector monitors mode>  <pre test check monitors mode>\n"\
                                        "Where:\n"\
                                        "Dynamic Vector Monitors mode : Is used to specify whether user want to continue test if Dynamic vector monitor fails\n"\
                                        "Pre Test Check Monitors mode : Is used specify whether user want to continue test if Pre test check monitor fails\n"

#define PRE_TEST_CHECK_TIMEOUT_USAGE "Usage: PRE_TEST_CHECK_TIMEOUT <timeout for running pre test check monitors>"

#define POST_TEST_CHECK_TIMEOUT_USAGE "Usage: POST_TEST_CHECK_TIMEOUT <timeout for running post test check monitors"

#define ENABLE_CMON_AGENT_USAGE "Usage: ENABLE_CMON_AGENT <enable/disable>"

#define ENABLE_AUTO_SERVER_SIGNATURE_USAGE "Usage: ENABLE_AUTO_SERVER_SIGNATURE <enable/disable>"

#define ENABLE_DATA_CONN_HB_USAGE "Usage:\n"\
                                  "DATA_CONN_HEART_BEAT <Enable (1)/Disable (0)> <Interval in secs>\n"\
                                  "Example:\n"\
                                  "DATA_CONN_HEART_BEAT 1 900 \n"

#define ENABLE_MONITOR_DR_USAGE "Usage: ENABLE_MONITOR_DR <enable/disable>"

#define ENABLE_CHECK_MONITOR_DR_USAGE "Usage: ENABLE_CHECK__MONITOR_DR <enable/disable>"

#define ENABLE_JAVA_PROCESS_SERVER_SIGNATURE_USAGE "Usage: ENABLE_JAVA_PROCESS_SERVER_SIGNATURE <enable/disable> <retry count> <threshold in secs> <recovery threshold in secs>"

#define DYNAMIC_VECTOR_TIMEOUT_USAGE "Usage: DYNAMIC_VECTOR_TIMEOUT <dynamic vector timeout in seconds>"

#define DYNAMIC_VECTOR_MONITOR_RETRY_COUNT_USAGE "Usage: DYNAMIC_VECTOR_MONITOR_RETRY_COUNT <dynamic vector monitor retry count in seconds>"

#define COHERENCE_NID_TABLE_SIZE_USAGE "Usage: COHERENCE_NID_TABLE_SIZE <coherence monitor NodeID Table size in MB>"

#define SKIP_UNKNOWN_BREADCRUMB_USAGE "Usage: SKIP_UNKNOWN_BREADCRUMB <enable/disable>"

#define ENABLE_HEROKU_MONITOR_USAGE "Usage: ENABLE_HEROKU_MONITOR <enable/disable>"

#define ENABLE_HML_GROUPS_USAGE "Usage: ENABLE_HML_GROUPS <enable/disable>"

#define ULOCATION_USAGE "Usage: ULOCATION <from_location> <to_location> <fw_latency in msec> <bw_latency in msec> <fw_loss in %> <bw_loss in %>"

#define UACCESS_USAGE "Usage: UACCESS <access_name> <upload_bw in kbps> <download_bw in kbps> <compression>"

#define G_USE_DNS_USAGE "Usage: G_USE_DNS <groupname or ALL> <Enable 0/1/2> <DNS Caching Mode 0/1/2> <DNS Log Mode 0/1> <DNS Connection Type<0/1> <DNS Cache ttl (in milliseconds)>\n"

#define IO_VECTOR_SIZE_USAGE "Usage: IO_VECTOR_SIZE <init_io_vector_size> <io_vector_delta_size> <io_vector_max_size>"

#define G_JAVA_SCRIPT_USAGE "Usage:G_JAVA_SCRIPT <grp_name> <js_mode> <js_all>\n"\
                            "where js_mode has 3 options\n"\
                            "0 -disable\n"\
                            "1 -enable\n"\
                            "2 -Use updated DOM for parameters\n"\
                            "and where js_all has 2 options\n"\
                            "0 -Save java script enabled URL responses\n"\
                            "1 -Save all enabled URL responses\n"

#define G_KA_PCT_USAGE "Usage: G_KA_PCT <grp_name> <keep alive connections in %>"

#define MAX_DYNAMIC_HOST_USAGE "Usage: MAX_DYNAMIC_HOST <number of host> <threshold time>\n"\
                               "Where number of host is:\n"\
                               "Maximum numbers of dynamic host, here 0 is default value\n"\
                               "threshold time :\n"\
                               "Threshold time(in ms) set for DNS lookup used for reporting purpose. This is an optional field, default value is 2 seconds\n"

#define G_FIRST_SESSION_PACING_USAGE "Usage: G_FIRST_SESSION_PACING <group_name> <first_session_pacing_option (0/1)> \n"\
                                     "Where\n"\
                                     "group_name: Scenario group name. It can be ALL or any valid group name\n"\
                                     "first_session_pacing_option: Apply pacing for first session of the user or not\n"\
                                     "For example to enable pacing for first session for all groups, use following keyword:\n"\
                                     "G_FIRST_SESSION_PACING ALL 1\n"\
                                     "Note: Pacing time for first session is by randomized mean pacing time\n"


#define G_NEW_USER_ON_SESSION_USAGE "Usage: G_NEW_USER_ON_SESSION <group_name> <refresh_user (0/1)> <refresh_values (0/1)> <refresh_cookie (0/1)>\n"\
                                    "Where\n"\
                                    "group_name: Scenario group name. It can be ALL or any valid group name\n"\
                                    "refresh_user: Create new user on every new session or not\n"\
                                    "refresh_value: Refresh value of paraeters on every session if diasbled. This will only fesiable if refresh_user = 0\n"\
                                    "For example to create new used on every session for groups, use following keyword:\n"\
                                    "G_NEW_USER_ON_SESSION ALL 1\n"\
                                    "This will simulate a new user on each session"

#define G_MAX_PAGES_PER_TX_USAGE "Usage: G_MAX_PAGES_PER_TX <group_name> <count>\n"\
                                 "Where:\n"\
                                 "<group_name> It can be ALL or any valid group name\n"\
                                 "<count> - Maximum number of page instances allowed in one transaction. It should be >= 1 and <= 64000\n"


#define ENABLE_TRANSACTION_CUMULATIVE_GRAPHS_USAGE "Usage: ENABLE_TRANSACTION_CUMULATIVE_GRAPHS <enable/disable>"

#define G_DATADIR_USAGE "Usage: G_DATATDIR_USAGE <group_name> <mode(0/1)> <data directory>\n"\
                        "Where\n"\
                        "group_name: Scenario group name. It can be ALL or any valid group name\n"\
                        "mode: '0' means ignore keyword, '1' means us data directory mentioned in 3rd argument of this keyword."

#define MAX_USERS_USAGE "Usage: MAX_USERS <limit maximum users>"

#define G_INLINE_EXCLUDE_DOMAIN_URL_PATTERN_USAGE "Usage: %s <group_name> <pattern_list>\n"\
                                                  "Group name can be ALL or any group name used in scenario group\n"\
                                                  "Pattern list can be any ragular expression seperated by ,\n"

#define G_INLINE_MIN_CON_REUSE_DELAY_USAGE "Usage: G_INLINE_MIN_CON_REUSE_DELAY <group_name> <min_value> <max_value>\n"\
                                           "Group name can be ALL or any group name used in scenario group\n"\
                                           "Note: Time values (MIN and MAX) should be specified in milliseconds."

#define G_STATIC_HOST_USAGE "Usages: \nG_STATIC_HOST <grp-name> <Host_name> <ip>\n"

#define G_SERVER_HOST_USAGE "Usages:\n"\
                            "Syntax - G_SERVER_HOST <GRP_NAME> <RECORDED_SERVER_IP> <ACTUAL_SERVER_IP> <LOCATION> <ACTUAL_SERVER_IP> <LOCATION> ...\n"\
                            "Where:\n"\
                            "group_name           - ALL, G1, G2 etc \n"\
                            "recorded_server_ip   - This can any host or domain or ip \n"\
                            "actual_server_ip     - This can any host or domain or ip \n"\
                            "location             - Any location name from VendorData.default or - (Default is SanFrancisco)\n"

#define G_SMTP_TIMEOUT_USAGE "Usage: G_SMTP_TIMEOUT <grp_name> <timeout in milliseconds>"

#define NJVM_STD_ARGS_USAGE "Usage: NJVM_STD_ARGS <njvm_min_heap_size> <njvm_max_heap_size> <njvm_gc_logging_mode>\n"\
                            "This keyword is to set standard njvm arguments."

#define NJVM_VUSER_THREAD_POOL_USAGE "Usage: NJVM_VUSER_THREAD_POOL <init thread pool size> <incremental size> <max thread pool size> <threshold percentage>\n"\
                                     "This keyword is use to No of njvm_thread in system\n"

#define NJVM_CUSTOM_ARGS_USAGE "Usage: NJVM_CUSTOM_ARGS <arg>\n"\
                               "Where:\n"\
                               "path : Is used to set the njvm custom configurations."


#define NJVM_CLASS_PATH_USAGE "Usage: NJVM_CLASS_PATH <path>\n"\
                              "Where:\n"\
                              "path : Is used to set the njvm class path"

#define NJVM_JAVA_HOME_USAGE "Usage: NJVM_JAVA_HOME <path>\n"\
                             "Where:\n"\
                             "path : Is used to set the njvm java home path"


#define NJVM_CONN_TIMEOUT_USAGE "Usage: NJVM_CONN_TIMEOUT <timeout>\n"\
                                "Where:\n"\
                                "timeout : Is used to set the njvm connection timeout, and it is in seconds"

#define NJVM_MESSAGE_TIMEOUT_USAGE "Usage: NJVM_MSG_TIMEOUT <timeout>\n"\
                                   "Where:\n"\
                                   "timeout : Is used to set the njvm msg timeout, and it is in seconds"

#define NET_DIAGNOSTICS_SERVER_USAGE "Usage: NET_DIAGNOSTICS_SERVER <SERVER> <PORT> <ND_PROFILE NAME> <MODE>\n"\
                                     "This keyword is use to set the ND Agent IP address and Port no."

#define HEALTH_MONITOR_DISK_FREE_USAGE "Usage: HEALTH_MONITOR_DISK_FREE <enable/disable> <file_system> <critical> <major> <minor> <start_test/stop_test>"

#define G_ENABLE_CORRELATION_ID_USAGE "Usage:G_ENABLE_CORRELATION_ID <Group> <Mode> <Header&QueryParameter> <HeaderName> <QueryParameterName> <prefix> <config> <suffix>"

#define ENABLE_NS_CHROME_USAGE  "Usages:\n"\
                                "Syntax - ENABLE_NS_CHROME <mode> <path>\n"\
                                "Where:\n"\
                                "  mode - provide netstorm chrome is enable or not.\n"\
                                "         0 - disabled (Default)\n"\
                                "         1 - enabled\n"\
                                "  path - provide netstorm chrome binary path.\n"\
                                "       - applicable only with mode 1\n"\
                                "       - Eg:    /home/cavisson/thirdparty/chrome/chrome_60 (Default path).\n"

#define G_RBU_USAGE "Usages:\n"\
                    "G_RBU <group_name> <page_name> <mode> <ss_mode> <param_mode> <stop_browser_on_sess_end_flag> <browser_mode>\n"\
                    "Where:\n"\
                    "  group_name                    - group name can be ALL or any valid group\n"\
                    "  page_name                     - page name can be ALL or any valid group\n"\
                    "  mode                          - provide script behaviour.\n"\
                    "                                  0 - script behave as Normal script (Default)\n"\
                    "                                  1 - script behave as RBU script\n"\
                    "                                  2 - script behave as RBU script(Node Mode)\n"\
                    "  ss_mode                       - provide screen shot flag.\n"\
                    "                                  0 - screen shot disabled (Default)\n"\
                    "                                  1 - screen shot is enabled \n"\
                    "  param_mode                    - provide auto parameter flag.\n"\
                    "                                  0 - disabled (Default)\n"\
                    "                                  1 - enabled \n"\
                    "  stop_browser_on_sess_end_flag - provide enable stop_browser_api on session end.\n"\
                    "                                  0 - disabled \n"\
                    "                                  1 - enabled \n"\
                    "  browser_mode                  - provide browser_mode\n"\
                    "                                  0 - for firefox browser (Default)\n"\
                    "                                  1 - for chrome browser\n"\
                    "  lighthouse_mode               - enable lighthouse plugin\n"\
                    "                                  0 - disabled \n"\
                    "                                  1 - enabled \n"

#define G_RBU_THROTTLING_SETTING_USAGE "Usages:\n"\
                                       "G_RBU_THROTTLING_SETTING <Group> <Mode> <Download Throughput(kbps)> <Upload Throughput(kbps)> <Request Latency(ms)>\n"\
                                       "Where:\n"\
                                       "Group                 - Group name can be ALL or any valid group\n"\
                                       "                        - Default is ALL\n"\
                                       "Mode                  - It may be 0 or 1\n"\
                                       "                        - 0 - Disabled(Default)\n"\
                                       "                        - 1 - Enabled\n"\
                                       "Download Throughput   - Set Download Throughput in kbps\n"\
                                       "Upload Throughput     - Set Upload Throughput in kbps\n"\
                                       "Request Latency       - Set Request Latency in milliseconds(ms)\n"

#define G_RBU_CPU_THROTTLING_USAGE "Usages:\n"\
                                       "G_RBU_CPU_THROTTLING <Group> <Mode> <slowdown multiplier>\n"\
                                       "Where:\n"\
                                       "Group                 - Group name can be ALL or any valid group\n"\
                                       "                        - Default is ALL\n"\
                                       "Mode                  - It may be 0 or 1\n"\
                                       "                        - 0 - Disabled(Default)\n"\
                                       "                        - 1 - Enabled\n"\
                                       "Slowdown Multiplier   -  any number greater than 1\n"


#define RBU_ENABLE_CSV_USAGE  "Usages: \nRBU_ENABLE_CSV <mode(0/1)>"

#define G_RBU_CAPTURE_CLIPS_USAGE  "Usages:\n"\
                                   "G_RBU_CAPTURE_CLIPS <group_name> <mode> [<frequency>] [<quality>] [domload_threshold] [onload_threshold]\n"\
                                   "Where:\n"\
                                   "  group_name         - provide group_name (Defult ALL)\n"\
                                   "  mode               - provide mode of capture_clips\n"\
                                   "                       0   - disabled(Default)\n"\
                                   "                       1,2 - enabled\n"\
                                   "  frequency          - provide frequency (at how many interval it will take clips, range is 5 to 10000)\n"\
                                   "  quality            - provide quality (Range is 0 to 100)\n"\
                                   "  domload_threshold  - provide value for domload_threshold (milliseconds, range is 0 to 120000), enabled when mode is 2\n"\
                                   "  onload_threshold   - provide value for onload_threshold (milliseconds, range is 0 to 120000), enabled when mode is 2\n"\

#define G_RBU_SETTINGS_USAGE "Usages: G_RBU_SETTINGS <grp-name> <mode(0 or (>=100 and <=10000))>"

#define G_RBU_CACHE_DOMAIN_USAGE "Usages:\n"\
                                 "Syntax - G_RBU_CACHE_DOMAIN <GRP_NAME> <MODE> [DOMAIN_LIST] \n"\
                                 "Where:\n"\
                                 "group_name    - G1, RBU etc \n"\
                                 "mode          - 0 Disable (Default) \n"\
                                 "              - 1 Enable \n"\
                                 "domain_list   - ALL or www.jcpenney.com;m.jcpenney.com (optional)"

#define G_RBU_USER_AGENT_USAGE "Usages:\n"\
                               "G_RBU_USER_AGENT <grp-name> <mode> <user_agent_string>\n"\
                               "Where:\n"\
                               "Group Name - group name can be ALL or any valid group\n"\
                               "mode       - provide script behaviour.\n"\
                               "   0 - provide Firefox default user agent\n"\
                               "   1 - provide user agent from Internet user profile.\n"\
                               "     Eg: G_RBU_USER_AGENT ALL 1 2G_GSM_CSD_2G_SINGLE_LOCATION\n"\
                               "   2 - provide user agent string\n"\
                               "     Eg: G_RBU_USER_AGENT ALL 2 Mozilla/5.0 (Linux; U; Android 2.3.4; en-us; HTC_Amaze_4G Build/GRJ22) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1"

#define G_RBU_CACHE_SETTING_USAGE  "Usages:\n"\
                                   "Syntax - G_RBU_CACHE_SETTING <group_name> [<mode>] [<cache_mode>] [<cookie_mode>]\n"\
                                   "Where:\n"\
                                   "  group_name - provide ALL or specified group name\n"\
                                   "  mode - provide flag to enable and disable cache\n"\
                                   "         0 - disable cache (Default)\n"\
                                   "         1 - enable cache\n"\
                                   "  cache_mode - provide flag to clear cache\n"\
                                   "         0 - don't clear cache (Default)\n"\
                                   "         1 - clear cache on session start\n"\
                                   "         2 - clear cache on page start\n"\
                                   "         3 - clear cache on Click start\n"\
                                   "  cookie_mode - provide flag to clear cookie on start of session\n"\
                                   "         0 - don't clear cookie (Default)\n"\
                                   "         1 - clear cookie on session start"

#define G_RBU_ADD_HEADER_USAGE  "Usages:\n"\
                                "Syntax - G_RBU_ADD_HEADER <group_name> <mode>\n"\
                                "Where:\n"\
                                "  mode - provide header in request\n"\
                                "  Eg: HEADER = header_name : header_value"

#define G_RBU_HAR_SETTING_USAGE "Usages:\n"\
                                "Syntax - G_RBU_HAR_SETTING <group_name> <mode> <compression> <request> <response> \n"\
                                "Where:\n"\
                                "group_name    - G1, RBU etc \n"\
                                "mode          - 0 Disable (Default) \n"\
                                "              - 1 Enable \n"\
                                "compression   - 0 Disable (This will not compress HAR) \n"\
                                "              - 1 Enable (This will compress HAR) \n"\
                                "request       - 0 Disable (This will not dump body in HAR in request) \n"\
                                "              - 1 Enable (This will dump body in HAR in request) \n"\
                                "response      - 0 Disbale (This will not dump body in HAR in response) \n"\
                                "              - 1 Enable (This will dump body in HAR in response) \n"\
                                "JS processing time and data URI  - 0 Disable (This will not dump JS processing time and data URI in HAR) \n"\
                                "                                 - 1 Enable (This will dump JS processing time) \n"\
                                "                                 - 2 Enable (This will data URI in HAR) \n"\
                                "                                 - 3 Enable (This will dump both JS processing time and data URI in HAR)"

#define G_RBU_ALERT_SETTING_USAGE "Usages:\n"\
                                  "Syntax - G_RBU_ALERT_SETTING <GRP_NAME> <MODE> \n"\
                                  "Where:\n"\
                                  "group_name    - G1, RBU etc \n"\
                                  "mode          - 0 Disable alerts (Default) \n"\
                                  "              - 1 Enable alerts"

#define G_RBU_ENABLE_TTI_MATRIX_USAGE "Usages:\n"\
                                      "Syntax - G_RBU_ENABLE_TTI_MATRIX <grp> <mode>\n"\
                                      "Where:\n" \
                                      "Group Name     - group name can be ALL or any valid group\n"\
                                      "mode           - 0 Disable (Default)\n"\
                                      "               - 1 Enable"

#define G_RBU_CLEAN_UP_PROF_ON_SESSION_START_USAGE "Usages:\n"\
                                                   "Syntax - G_RBU_CLEAN_UP_PROF_ON_SESSION_START <group_name> <mode> <sample_profile> \n"\
                                                   "Where:\n"\
                                                   "  group_name      - Name of group (eg: G1,G2 or ALL(default))\n"\
                                                   "  mode            - 0 Disable (Default)\n"\
                                                   "                  - 1 Enable (clean profile before the start of the session)\n"\
                                                   "  sample_profile  - to provide sample profile in which client shoud have their specific settings.\n "\
                                                   "                    Note - 1) sample profile must be registered in profile.in\n"\
                                                   "                    Note - 2) sample profile path must be absolute\n"\
                                                   "                    Eg: /home/<user>/.rbu/.mozilla/firefox/prfoile/<sample_profile>\n"\
                                                   "                    This is mandatory option"

#define G_RBU_RM_PROF_SUB_DIR_USAGE "Usages:\n"\
                                    "Syntax - G_RBU_RM_PROF_SUB_DIR <Grp> <Mode> <profile relative path> \n"\
                                    "Where:\n"\
                                    "Grp          : Any valid Group or ALL \n"\
                                    "Mode         : mode for logging access_log\n"\
                                    "                0 : Disable\n"\
                                    "                1 : Enable, delete directory on session start\n"\
                                    "                2 : Enable, delete directory on page start\n"\
                                    "profile relative path : Provide relative path of directory to be deleted from profile path.\n"\
                                    "                Example : If need to delete Service Worker directory which exist at path - \n"\
                                    "                         /home/cavisson/.rbu/.chrome/profiles/new_shikha_satish/Default/Service Worker\n"\
                                    "                         then provide Default/Service Worker."

#define G_RBU_HAR_TIMEOUT_USAGE "Usages:\n"\
                                "G_RBU_HAR_TIMEOUT <group_name> <time (Sec)>\n"\
                                "Where:\n"\
                                "group_name            - group name can be ALL or any valid group\n"\
                                "                      - Default is ALL\n"\
                                "time                  - provide time to wait for HAR file, in seconds\n"\
                                "                      - Default is [%d] Sec."

#define G_RBU_BLOCK_URL_LIST_USAGE "Usages:\n"\
                                   "Syntax     -  G_RBU_BLOCK_URL_LIST <GRP_NAME> <URL_LIST> \n"\
                                   "Where:\n"\
                                   "GRP_NAME   -  G1, RBU etc \n"\
                                   "URL_LIST   -  Provide Urls, e.g:   https://www.walgreens.com/,https://js.kohls.com/media/javascript/omnilinks-latest.js"

#define G_RBU_DOMAIN_IGNORE_LIST_USAGE "Usages:\n"\
                                       "Syntax - G_RBU_DOMAIN_IGNORE_LIST <GRP_NAME> <DOMAIN_LIST> \n"\
                                       "Where:\n"\
                                       "GRP_NAME      - G1, RBU etc \n"\
                                       "DOMAIN_LIST   -Provide Domains, e.g., www.facebook.com,google"

#define RBU_DOMAIN_STAT_USAGE "Usages:\n"\
                              "Syntax - RBU_DOMAIN_STAT <mode> \n"\
                              "Where:\n"\
                              "mode    - mode for enabling/disabling dynamic domain stats\n"\
                              "        0 : Disable\n"\
                              "        1 : Enable"

#define RBU_POST_PROC_USAGE "Usages: \nRBU_POST_PROC <mode(0/1/2)> <[har_rename_info_file]>"

#define RBU_MARK_MEASURE_MATRIX_USAGE "Usages:\n"\
                                      "Syntax - RBU_MARK_MEASURE_MATRIX <mode> \n"\
                                      "Where:\n"\
                                      "mode          - mode for enabling/disabling dynamic domain stats\n"\
                                      "                0 : Disable\n"\
                                      "                1 : Enable"

#define RBU_BROWSER_COM_SETTINGS_USAGE "Usages:\n"\
                                       "RBU_BROWSER_COM_SETTINGS <mode> <frequency> <interval>\n"\
                                       "Where:\n"\
                                       "mode       - enable/disable the feature\n"\
                                       "             0 --> disable\n"\
                                       "             1 --> enable.\n"\
                                       "Frequency    - Integer number less then 256\n"\
                                       "Interval     - Time in milli seconds to reconnect\n"\
                                       "          Eg - RBU_BROWSER_COM_SETTINGS 1 20 3000"

#define RBU_ENABLE_DUMMY_PAGE_USAGE  "Usages:\n"\
                                     "RBU_ENABLE_DUMMY_PAGE <mode> <url>\n"\
                                     "Where:\n"\
                                     "mode - provide facility to add dummy internaly. (Mandatory)\n"\
                                     "       1 - means  add default page automatically.\n"\
                                     "            Eg: RBU_DEFAUTL_PAGE 1 http://www.google.com\n"\
                                     "            Eg: RBU_DEFAUTL_PAGE 1\n"\
                                     "       0 - means don't add default page automatically.\n"\
                                     "url  - If mode is 0 then provide default page. (Optional)"


#define G_RBU_PAGE_LOADED_TIMEOUT_USAGE "Usages:\n"\
                                        "Syntax - G_RBU_PAGE_LOADED_TIMEOUT <group_name> <time> <phase_interval>\n"\
                                        "Where:\n"\
                                        "  time           - provide time to wait page load, in millisecond\n"\
                                        "  phase_interval - provide phase interval time for page load time calculation, in millisecond"

#define G_RBU_SCREEN_SIZE_SIM_USAGE "Error: keyword '%s' has been deprecated from NS Release 4.1.7 Build 31, instead of this, use following keyword:-\n"\
                                    "  G_RBU_SCREEN_SIZE_SIM <grp-name> <mode>\n"\
                                    "    Group Name     - group name can be ALL or any valid group\n"\
                                    "    mode             0 - Disable (Default)\n"\
                                    "                     1 - enable\n"\
                                    "                         Eg: G_RBU_SCREEN_SIZE_SIM ALL 1"

#define UBROWSER_USAGE " Usage: UBROWSER <BrowerName>  <MaxConPerServerHTTP1.0> <MaxConPerServerHTTP1.1> <MaxProxyPerServerHTTP1.0> <MaxProxyPerServerHTTP1.1> <MaxConPerUser> <Keep Alive Timeout in ms> <User Agent String>"

#define NVM_DISTRIBUTION_USAGE "Usage: NVM_DISTRIBUTION <mode>\n"\
                               "where it has 2 modes\n"\
                               "1-Distribution for maximum isolation among scripts.All scenario group using same script will be executed only by one NVM\n"\
                               "2-User specifies NVM Id for each scenario group"

#define DISABLE_SCRIPT_COPY_TO_TR_USAGE "Usage: DISABLE_SCRIPT_COPY_TO_TR <mode>\n"\
                                        "Where:\n"\
                                        "mode : Is used to specify whether user want to copy files with/without subdirectories or do not want to copy"

#define OPTIMIZE_ETHER_FLOW_USAGE "Usage: OPTIMIZE_ETHER_FLOW <HandshakeMergeAck> <DataMergeAck> <Fin/Reset >\n"\
                                  "Where\n"\
                                  "HandshakeMergeAck : This field is used to control merging on ACK during handshake (connection).\n"\
                                  "\t0: This is default. In this case, no merging is done to reduce number of packets.\n"\
                                  "\t1: This will result in piggybacking of ACK with other packets as and when possible during handshake.\n"\
                                  "DataMergeAck: This field is used to control merging on ACK during data communication.\n"\
                                  "\t0: This is default. In this case, no merging is done to reduce number of packets.\n"\
                                  "\t1: This will result in piggybacking of ACK with other packets as and when possible during data communication.\n"\
                                  "Fin/Reset : \n"\
                                  "\t0: This is default. In this case, connection will be closed gracefully using FIN.\n"\
                                  "\t1: This value will close the connection by sending TCP Reset (RST) instead of FIN. No packets will be send back from other side. This will result in reduction of packets by 2 if close is done by the side where this option is used."

#define VUSER_THREAD_POOL_USAGE "Usage: VUSER_THREAD_POOL <init thread pool size> <incremental size> <max thread pool size> <threshold percentage>\n"\
                                "This keyword is use to set number of njvm_thread in system"

#define CAV_EPOCH_YEAR_USAGE "Usage: CAV_EPOCH_YEAR <set epoch year>"

#define ENABLE_FCS_SETTINGS_USAGE "Usage: ENABLE_FCS_SETTINGS <mode> <concurrent sessions limit> <queue size>\n"\
                                  "Mode:  Concurrent session mode\n"\
                                  "0 -  Disable concurrent session\n"\
                                  "1 -  Enable concurrent session\n"\
                                  "Limit: concurrent session limit on machine\n"\
                                  "Pool Size: maximum number of users to be saved (optional argument)"

#define G_MAX_URL_RETRIES_USAGE "Usage: G_MAX_URL_RETRIES_USAGE <grp_name> <retry time on connection> <mode>\n"\
                                "where mode has 2 options\n"\
                                "1- Only if request method are HEAD, CONNECT, GET\n"\
                                "2- Always"

#define G_USER_CLEANUP_MSECS_USAGE "Usage: G_USER_CLEANUP_MSECS_USAGE <grp_name> <time in msecs, user connection remains open for after an old user completes a session>"

#define NS_GENERATOR_USAGE "Usage: NS_GENERATOR <generator_name>\n"\
                           "Where generator_name:\n" \
                           "Name of generator used in load balancing."

#define  PROF_PCT_MODE_USAGE  "Usage: PROF_PCT_MODE <NUM or PCT or NUM_AUTO>"

#define SGRP_USAGE  "Usage: SGRP <GroupName> <GeneratorName> <ScenType> <user-profile>\n"\
                    "<type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>"

#define STYPE_USAGE  "Usage: STYPE <ScenarioType>\n"\
                     "Where ScenarioType: (Fix Concurrent User or Mix Mode or Fix Session Rate)"

#define TARGET_RATE_USAGE "Usage: TARGET_RATE <Target Rate>"\
                          "Where Target Rate is total number sessions per minutes"

#define SCHEDULE_USAGE  "Usage: SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>\n"\
                        "Phase Type:\n"\
                        "\t START\n"\
                        "\t RAMP_UP\n"\
                        "\t DURATION\n"\
                        "\t STABILIZATION\n"\
                        "\t RAMP_DOWN\n"

#define SCHEDULE_TYPE_USAGE "Usage: SCHEDULE_TYPE <SIMPLE or ADVANCED>" 

#define NH_SCENARIO_USAGE "Usage: NH_SCENARIO <nh scenario name> <ns phase name> <delay> <is_stop_scenario_at_phase_end>" 

#define NH_SCENARIO_INTEGRATION_USAGE "Usage: NH_SCENARIO_INTEGRATION <server loacation> <url> <token> <topology> <pattern>" 

#define SCHEDULE_BY_USAGE "Usage: SCHEDULE_BY <SCENARIO or GROUP>"

#define SCENARIO_SETTING_PROFILE_USAGE "Usage: SCENARIO_SETTING_PROFILE <File Name>" 
 
#define ENABLE_FCS_SETTING_USAGE "Usage: ENABLE_FCS_SETTINGS <mode> <session limit> <queue size>"

#define HEALTH_MONITOR_USAGE "Usage: HEALTH_MONITOR <enable/disable> <wait for connections to clear if reached to limit>"

#define G_TRACING_LIMIT_USAGE "Usage: G_TRACING_LIMIT <group name> <tracing scratch buffer size> <max trace param entries> <max trace param value length>"

#define REPLAY_FILE_USAGE "Usage: REPLAY_FILE <log type> <replay directory>"

#define REPLAY_FACTOR_USAGE "Usage: REPLAY_FACTOR <user playback factor in %> <users arrival time factor in %> <inter page time factor in %> <replay iteration count>"

#define HEALTH_MONITOR_INODE_FREE_USAGE "Usage: HEALTH_MONITOR_INODE_FREE <enable/disable> <file_system> <critical> <major> <minor> <start_test/stop_test>"

#define G_USE_SAME_NETID_SRC_USAGE "Usage: G_USE_SAME_NETID_SRC <grp_name> <mode>\n"\
                                           "where it has 3 options\n"\
                                           "0 - default\n"\
                                           "1 - Server address (actual servers to be hit) need not be a NetOcean address\n"\
                                           "2 - Server address (actual servers to be hit) must be NetOcean address"

#define NJVM_SYSTEM_CLASS_PATH_USAGE "Usage: NJVM_SYSTEM_CLASS_PATH_USAGE <system java class path>"

#define G_ENABLE_DT_USAGE "Usage: G_ENABLE_DT <grp_name> <enable/disable> <include fields in the x-dynaTrace Header>"

#define NUM_NVM_USAGE "Usage: NUM_NVM_USAGE <number of virtual machine(s)> <cpu/machine> <minimum users/session per nvm(in case of CPU)>"

#define TEST_MONITOR_TYPE_USAGE "Usage: TEST_MONITOR_CONFIG <Monitor Type> <Monitor Index> <Monitor Name> <Tier> <Server> <TR Number> <PartitionId> <File Upload URL>\n"\
                      "where Monitor Type have 3 options\n"\
                      "0 - None\n"\
                      "1 - HTTP API\n"\
                      "2 - Web Page Audit\n"\
                      "All Arguments are mandatory for HTTP API and Web Page Audit\n"

#define G_IP_VERSION_MODE_USAGE "Usage: G_IP_VERSION_MODE_USAGE <group name> <IP Address Version Mode>\n"\
                                "where Version Mode can have 3 values:\n"\
                                "0 - Auto\n"\
                                "1 - IPV4\n"\
                                "2 - IPV6"

#define CHECK_GENERATOR_HEALTH_USAGE "Usage: CHECK_GENERATOR_HEALTH <mode> <minDiskAvailability> <minCpuAvailability> "\
                                     "<minMemAvailability> <minBandwidthAvailability>"\
                                     "\tThis keyword is used to stop test if generator is violating any of following availability:"\
                                     "\tMode: Mode for enable/disable. It can only be 0 (Disable), 1 (Enable - default)\n"\
                                     "\tminDiskAvailability: Specify threshold disk space availability between [0-1024] (GB)\n"\
                                     "\tminCpuAvailability: Specify threshold user cpu pct between [0-INT_MAX] (%)\n"\
                                     "\tminMemAvailability: Specify threshold memory availability between [0-1024] (GB)\n"\
                                     "\tminBandwidthAvailability: Specify threshold bandwidth availability between [0-1024] (Mbps)\n"




#define ENABLE_MONITOR_REPORT_USAGE "Invalid number of arguments.\n"\
                                    "Usage: ENABLE_PROGRESS_REPORT <reporting mode>\n"\
                                    "Where:\n"\
                                    "reporting mode: To configure reporting mode in progress report. Default value is 2\n"   

#define ENABLE_ALERT_LOG_MONITOR_USAGE "Usage: ENABLE_ALERT_LOG_MONITOR <option> <log_name>.\n"\
                                       "Expected options are: 0->Disable, 1->AutoMode (enable only if continuous mode), 2->Enable. \n"

#define ND_ENABLE_METHOD_MONITOR_EX_USAGE "Usage: ND_ENABLE_METHOD_MONITOR_EX <enable/disable> .\n"
 
#define ND_ENABLE_HTTP_HEADER_CAPTURE_MONITOR_USAGE "Usage: ND_ENABLE_HTTP_HEADER_CAPTURE_MONITOR <enable/disable>.\n"

#define ND_ENABLE_EXCEPTIONS_MONITOR_USAGE "Usage: ND_ENABLE_EXCEPTIONS_MONITOR <enable/disable>.\n"

#define ND_ENABLE_ENTRY_POINT_MONITOR_USAGE "Usage: ND_ENABLE_ENTRY_POINT_MONITOR <enable/disable>.\n"

#define ND_ENABLE_BACKEND_CALL_MONITOR_USAGE "Usage: ND_ENABLE_BACKEND_CALL_MONITOR <enable/disable>.\n"
 
#define ND_ENABLE_NODE_GC_MONITOR_USAGE "Usage: ND_ENABLE_NODE_GC_MONITOR <enable/disable>.\n"

#define ND_ENABLE_EVENT_LOOP_MONITOR_USAGE "Usage: ND_ENABLE_EVENT_LOOP_MONITOR <enable/disable>.\n"

#define ND_ENABLE_FP_STATS_MONITOR_EX_USAGE "Usage: ND_ENABLE_FP_STATS_MONITOR_EX <enable/disable>.\n"

#define ND_ENABLE_BUSINESS_TRANS_STATS_MONITOR_USAGE "Usage: ND_ENABLE_BUSINESS_TRANS_STATS_MONITOR_USAGE <enable/disable>.\n"

#define ND_ENABLE_JVM_THREAD_MONITOR_USAGE "Usage: ND_ENABLE_JVM_THREAD_MONITOR <enable/disable>.\n"

#define ND_ENABLE_DB_CALL_MONITOR_USAGE "Usage: ND_ENABLE_DB_CALL_MONITOR <enable/disable>.\n"

#define ND_ENABLE_BT_MONITOR_USAGE "Usage: ND_ENABLE_BT_MONITOR <enable/disable>.\n"

#define ND_ENABLE_BT_IP_MONITOR_USAGE "Usage: ND_ENABLE_BT_IP_MONITOR <enable/disable>.\n"

#define NV_MONITOR_USAGE "Usage: NV MONITOR <enable/disable>.\n"

#define DYNAMIC_VECTOR_MONITOR_USAGE "DYNAMIC_VECTOR_MONITOR is not configured properly.\n"\
                                     "Configure it as following \n"\
                                     "DYNAMIC_VECTOR_MONITOR <Server Name> <Dynamic Vector Monitor Name> <GDF Name> <Run Option> <Command or Program Name with arguments for getting data> EOC <Program Name with arguments for getting Vector List>.\n"\
                                     "Example:\n"\
                                     "DYNAMIC_VECTOR_MONITOR 192.168.18.104 dvm_df cm_df.gdf 2 cm_df EOC cm_df -vectors.\n" 

#define SYNC_POINT_USAGE "Usage: SYNC_POINT <Group> <Type> <Name> <Active/Inactive> <Pct user> <Release target user> <Scripts> <Release Mode> <Release Type> <Target/Time/Period> <Release Forcefully> <Release Schedule> <Immediate/Duration/Rate>\n"\
                         " Where:\n"\
                         " Type :\n"\
                         "   0: start transaction \n"\
                         "   1: start page \n"\
                         "   2: start script \n"\
                         "   3: custom sync point \n"\
                         " Active/Inactive :\n"\
                         "   0: inactive \n"\
                         "   1: active (Default)\n"\
                         " pct_user : default value of active user percentage is 100.00 \n"\
                         " Release Mode :\n"\
                         "   0: Auto\n"\
                         "   1: Manual\n"\
                         " Release Type :\n"\
                         "   0: Target (if specified next field should be NA)\n"\
                         "   1: Time (Format - MM/DD/YYYY HH:MIN:SS)\n"\
                         "   2: Period (Format - HH:MM:SS)\n"\
                         " Release forcefully on target reached if release type is Time or Period:\n"\
                         "   0: False\n"\
                         "   1: True\n"\
                         " Release Schedule :\n"\
                         "   0: Immediate (if specified next field should be NA)\n"\
                         "   1: Duration (Format - HH:MM:SS)\n"\
                         "   2: Rate (Format - XX)"

#define ADVERSE_FACTOR_USAGE "Usage: ADVERSE_FACTOR <LATENCY_FACTOR> <LOSS_FACTOR>"

#define WAN_JITTER_USAGE     "Usage: WAN_JITTER <FW_JITTER> <RV_JITTER>"

#define CONTINUE_TEST_ON_GEN_FAILURE_USAGE "Usage: CONTINUE_TEST_ON_GEN_FAILURE <mode> <max allowed delay samples> <continue with percentage of gen started>\n"\
                               "  <continue with percentage of gen running> <gen test start timeout>\n"\
                               "  Where following used to specify whether user want to continue test on controller:\n"\
                               "  mode : <Enable/Disable> if generators fails then continue (1) or stop (0)\n"\
                               "  max delay samples allowed : generator fails if given number of samples delayed consecutively\n"\
                               "  continue with percentage of gen started : generator fails if given percentage of gen not started then stop test\n"\
                               "  continue with percentage of gen running : generator fails if given percentage of gen not running then stop test\n"\
                               "  gen test start timeout : generator fails if gen not started before given timeout then stop test"

#define HOST_TLS_VERSION_USAGE "Usage: HOST_TLS_VERSION <hostname> <tls version> \n"\
                               "Where : \n"\
                               "hostname    : Any Recorded host \n"\
                               "tls_version : ssl3, tls1, tls1_1, tls1_2\n"\
                               "Default ssl version 2_3  is supported. A TLS/SSL connection established with these methods may "\
                               "understand the SSLv3, TLSv1, TLSv1.1 and TLSv1.2 protocols. In this case client will send out highest "\
                               "ssl method supported . SSLv2_3 also permits a fallback to lower ssl version. "



#define ND_DATA_VALIDATION_USAGE "USAGE: ND_DATA_VALIDATION <0/1> <0/1> <tps max> <resp max> <count max>.\n"

#define GROUP_HIERARCHY_CONFIG_USAGE "Usage: GROUP_HIERARCHY_CONFIG <0/1>.\n"

#define CMON_SETTINGS_USAGE "Usage: CMON_SETTINGS <Server> HB_MISS_COUNT=6;CAVMON_MON_TMP_DIR=/tmp;CAVMON_DEBUG=on; .\n"

#define ENABLE_AUTO_JSON_MONITOR_USAGE  "USAGE: ENABLE_AUTO_JSON_MONITOR <Mode> <filepath> \n"\
					"Mode 0 -Disable the ENABLE_AUTO_JSON_MONITOR keyword. \n"\
					"Mode 1 -Enable  ENABLE_AUTO_JSON_MONITOR 1 <JSON file> \n" \
					"Filepath will have name of JSON file.\n"\
					"Mode 2 -Enable  ENABLE_AUTO_JSON_MONITOR 2 <JSON DIRECTORY_NAME> \n" 

#define COH_CLUSTER_AND_CACHE_VECTORS_USAGE "USAGE: COH_CLUSTER_AND_CACHE_VECTORS <value> <Cluster vector filepath> <Cache vector file path> .\n"

#define ENABLE_ALERT_USAGE  "Usage: ENABLE_ALERT <mode> <Type> <Policy> <RateLimit> <RetryCount> <RetryTime> <ThreadInitSize> <ThreadInitMax> <QueueInitSize> <QueMaxSize> <AlertServerConfig>\n" \
                            "Where:\n"   \
                            "  mode - alert is enable or not.\n" \
                            "         0 - disabled (Default)\n"\
                            "         1 - enabled\n" 
 
#define G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE "Usage: G_RBU_CAPTURE_PERFORMANCE_TRACE <Group> <Mode> <Timeout> <Enable Memory> <Enable Screenshot>"

#define G_KA_TIME_USAGE "Usage:  G_KA_TIME <grpname> <keepalive time value>"

#define G_RAMP_DOWN_METHOD_USAGE "Usage: G_RAMP_DOWN_METHOD <group_name> <ramp_down_method> <mode based fields>"

#define G_PAGE_RELOAD_USAGE "Usage: G_PAGE_RELOAD <Group Name> <Page> <Min Reloads> <Max Reloads> <Timeout in ms>"

#define UPBROWSER_USAGE "Usage: UPBROWSER <Userprofile name>"

#define G_JMETER_JVM_SETTINGS_USAGE "Usages: G_JMETER_JVM_SETTINGS <GRP_NAME> <MIN_HEAP_SIZE> <MAX_HEAP_SIZE> <OPT_JVM_ARGS>\n"\
                                "Where:\n"\
                                "group_name    - G1, G2, ALL etc \n"\
                                "min_heap_size(MB) - 512, 1024 etc\n"\
                                "max_heap_size(MB) - 512, 1024 etc\n"\
                                "opt_jvm_args  - additonal jvm arguments provided to jmeter(optional)"

#define G_JMETER_ADD_ARGS_USAGE "Usages: G_JMETER_ADD_ARGS <GRP_NAME> <GEN_JMETER_REPORT> <OPT_JMETER_ARGS>\n"\
                                "Where:\n"\
                                "group_name    - G1, G2, ALL etc \n"\
                                "gen_jmeter_report - 0, 1\n"\
                                "opt_jmeter_args  - additonal jmeter arguments(optional)"

#define JMETER_CSV_DATA_SET_SPLIT_USAGE "Usages: JMETER_CSV_DATA_SET_SPLIT_USAGE <CSV_SPLIT_MODE> <CSV_SPLIT_STRING>\n"\
                                "Where:\n"\
                                "csv_split_mode    - 0/1/2 \n"\
                                "csv_split_string  - csv file containing specific pattern in name.\n"\
                                "                    Applicable only when csv_split_mode is 2"
#define JMETER_VUSERS_SPLIT_USAGE "Usages: JMETER_VUSERS_SPLIT <VUSERS_SPLIT_MODE>\n"\
                                "Where:\n"\
                                "vusers_split_mode    - 0/1"

#define G_JMETER_SCHEDULE_SETTING_USAGE "Usages: G_JMETER_SCHEDULE_SETTINGS <GRP_NAME> <THREADNUM> <RAMPUP> <DURATION>\n"\
                                "Where:\n"\
                                "group_name    - G1, G2, ALL etc \n"\
                                "threadnum     - Jmeter Threads  \n"\
                                "rampup time   - Time in seconds for Jmeter user ramp up\n"\
                                "duration      - Time in seconds of Jmeter test duration(should be > 0)"

#define G_HTTP_HDR_USAGE "Usages: G_HTTP_HDR <Group> <Page> <Mode> <HeaderName> <HeaderValue>\n"\
				"Where:\n"\
				"Group       - G1, G2, ALL etc \n"\
				"Page        - Name of page of the script running in this group. Page can be ALL(applied on ALL Pages)\n"\
				"Mode        - Specifies where to add the new header(0-Main, 1 - Inline, 2-ALL)\n"\
				"HeaderName  - Name of Header as per HTTP specification. It should not contains any space.\n"\
				"HeaderValue - Header Value. It can have in the string but not leading and trailing.\n"


#define G_CLICK_AWAY_USAGE "G_PAGE_CLICK_AWAY <Group Name> <Page> <Next page> <pct> <Time in ms> <Call check page or not> <transaction status>"

#define CAVMON_INBOUND_PORT_USAGE "Usage: CAVMON_INBOUND_PORT_USAGE <port> \n"
#endif
