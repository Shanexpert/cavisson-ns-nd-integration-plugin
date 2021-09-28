#ifndef NS_CAHCE_INCLUDE
#define NS_CACHE_INCLUDE



#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/prctl.h>
#ifdef SLOW_CON
#include <linux/socket.h>
#include <netinet/tcp.h>
#define TCP_BWEMU_REV_DELAY 16
#define TCP_BWEMU_REV_RPD 17
#define TCP_BWEMU_REV_CONSPD 18
#endif
#ifdef NS_USE_MODEM
#include <linux/socket.h>
//#include <linux/cavmodem.h>
#include <netinet/tcp.h>
#include <regex.h>

#include "nslb_dyn_hash.h"
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "user_tables.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "cavmodem.h"
#include "ns_wan_env.h"

#endif
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef USE_EPOLL
//#include <asm/page.h>
// This code has been commented for FC8 PORTING
//#include <linux/linkage.h>
#include <linux/unistd.h>
#include <sys/epoll.h>
#include <asm/unistd.h>
#endif
#include <math.h>
#include "runlogic.h"
#include "uids.h"
#include "cookies.h"
//#include "logging.h"
#include <gsl/gsl_randist.h>
#include "weib_think.h"
#include "netstorm.h"
#include <pwd.h>
#include <stdarg.h>
#include <sys/file.h>

#include "decomp.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "util.h" 
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "eth.h"
#include "timing.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_master_agent.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "server_stats.h"
#include "ns_trans.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
#include "ns_parent.h"
#include "ns_child_msg_com.h"
#include "ns_http_hdr_states.h"
#include "ns_url_resp.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_auto_fetch_embd.h"
#include "ns_parallel_fetch.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_replay_access_logs.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_page.h"
#include "ns_vuser.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_global_dat.h"
#include "ns_smtp_send.h"
#include "ns_smtp.h"
#include "ns_pop3_send.h"
#include "ns_pop3.h"
#include "ns_ftp_send.h"
#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_http_status_codes.h"

#include "ns_server_mapping.h"
#include "ns_event_log.h"
#include "ns_event_id.h"


#include "ns_http_hdr_states.h"
#include "ns_http_cache_table.h"
#include "ns_alloc.h"
#include "ns_http_cache_store.h"
#include "ns_http_cache_hdr.h"
#include "ns_http_cache.h"
#include "nslb_date.h"
#include "ns_vuser_ctx.h"
#include "ns_data_types.h"
#include "ns_parse_scen_conf.h"

#endif  //NS_CACHE_INCLUDE
