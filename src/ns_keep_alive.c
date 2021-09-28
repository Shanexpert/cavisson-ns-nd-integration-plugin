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

//#include "ns_cache_include.h"
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

#include "netstorm.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "src_ip.h"
#include "ns_common.h"
#include "ns_http_cache.h"

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
//#include <asm/poll.h>
#include <linux/unistd.h>
#include <sys/epoll.h>
#include <asm/unistd.h>
#endif
#include <math.h>
#include "runlogic.h"
#include "uids.h"
#include "cookies.h"
#include <gsl/gsl_randist.h>
#include "weib_think.h"
#include <pwd.h>
#include <stdarg.h>
#include <sys/file.h>

#include "decomp.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "ns_sock_list.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "amf.h"
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
//#include "ns_handle_read.h"
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
#include "ns_group_data.h"

//#include "ns_auto_redirect.h"
//#include "ns_url_req.h"
//#include "ns_replay_access_logs.h"
//#include "ns_replay_access_logs_parse.h"
//#include "ns_page.h"
//#include "ns_vuser.h"
//#include "ns_schedule_ramp_down_fcu.h"
//#include "ns_schedule_ramp_up_fcu.h"
#include "ns_global_dat.h"
//#include "ns_smtp_send.h"
//#include "ns_smtp.h"
//#include "ns_pop3_send.h"
//#include "ns_pop3.h"
//#include "ns_ftp_send.h"
//#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_http_status_codes.h"

#include "ns_server_mapping.h"
#include "ns_keep_alive.h"
#include "ns_common.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"


void ka_timeout_handle_cb( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  NSDL2_HTTP(vptr, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);
  
  if(SHOW_GRP_DATA)
    set_grp_based_counter_for_keep_alive(vptr, (now - client_data.timer_started_at));

  close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
}

/*This function will get call from on_new_session_start.
 * On new session start we are filling KA timeout value*/
void fill_ka_time_out(VUser *vptr, u_ns_ts_t now)
{
   if(runprof_table_shr_mem[vptr->group_num].gset.ka_mode == KA_MODE_NONE)
   {
      NSDL2_HTTP(vptr, NULL, "Method Called, Mode = KA_MODE_NONE, no timer will set. returning");
      return;
   }
   else if(runprof_table_shr_mem[vptr->group_num].gset.ka_mode == KA_MODE_BROWSER)
   {
     vptr->ka_timeout = vptr->browser->ka_timeout;
     NSDL2_HTTP(vptr, NULL, "Method Called, Browser based mode, setting ka timeout = %d", vptr->ka_timeout);
   }
   else if(runprof_table_shr_mem[vptr->group_num].gset.ka_mode == KA_MODE_GROUP)
   {
     vptr->ka_timeout = runprof_table_shr_mem[vptr->group_num].gset.ka_timeout;
     NSDL2_HTTP(vptr, NULL, "Method Called, Group based mode, setting ka timeout = %d", vptr->ka_timeout);
   }
}


/*This function is for adding timer*/
void check_and_add_ka_timer(connection *cptr, VUser *vptr, u_ns_ts_t now)
{
  ClientData client_data;
  client_data.p = cptr;

  NSDL2_HTTP(vptr, cptr, "Method Called, conn time_out = %d", vptr->ka_timeout);
  if(vptr->ka_timeout != -1)
  { 
    cptr->timer_ptr->actual_timeout = vptr->ka_timeout;
    client_data.timer_started_at = now;
    dis_timer_add_ex(AB_TIMEOUT_KA, cptr->timer_ptr, now, ka_timeout_handle_cb, client_data, 0, global_settings->ka_timeout_all_flag);
  }
}

int kw_set_ka_time_mode(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num;
  int mode;

  num = sscanf(buf, "%s %s %d", keyword, grp, &mode);
  if (num != 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_KA_TIME_MODE_USAGE, CAV_ERR_1011043, CAV_ERR_MSG_1);
  }
  else if ((mode < 0 ) || (mode > 2))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_KA_TIME_MODE_USAGE, CAV_ERR_1011043, CAV_ERR_MSG_3);
  }
  else
  {
  *to_change = mode;
   NSDL2_HTTP(NULL, NULL, "Method Called, Mode = %d", *to_change);
  }
  return 0;
}

int kw_set_ka_timeout(char *buf, int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num;
  int time_value;
  num = sscanf(buf, "%s %s %d", keyword, grp, &time_value);
  if (num != 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_KA_TIME_USAGE, CAV_ERR_1011280, CAV_ERR_MSG_1);
  } else {
    *to_change = time_value;
   NSDL2_HTTP(NULL, NULL, "Method Called, Time out = %d", *to_change);
  }
  return 0;
}

/*This function is to set global flag.
 * We are doing this because, on adding timer it
 * checks for sorted value. but if we ghave same 
 * timeout then no need to sorting just add timer.*/
void check_and_set_flag_for_ka_timer ()
{
  int idx;
  int last_value = -1;
  char flag = 1; //  ALL Equal
  int i;
  int max;

  RunProfTableEntry_Shr *rstart;
  UserProfIndexTableEntry_Shr *userindex_ptr;
  UserProfTableEntry_Shr *ustart;

  for(i = 0; i < total_runprof_entries; i++)
  {
     if(runprof_table_shr_mem[i].gset.ka_mode == 1) {
       rstart = &runprof_table_shr_mem[i];
       userindex_ptr = rstart->userindexprof_ptr;
       max = userindex_ptr->length;
       ustart = userindex_ptr->userprof_start;
  
       for (idx = 0; idx < max; idx ++, ustart ++) 
       {
         if(last_value == -1)
           last_value = ustart->browser->ka_timeout;
         else 
         {
           if(last_value != ustart->browser->ka_timeout)
           {
             flag = 0; 
             break;
           } 
           else
             last_value = ustart->browser->ka_timeout;
         }
       }
       if(!flag) break;
     } 
     else if ( runprof_table_shr_mem[i].gset.ka_mode == 2) 
     {
       if(last_value != -1) 
       {
         if(last_value != runprof_table_shr_mem[i].gset.ka_timeout)
         {
           flag = 0;
           break;
         } 
         else 
           last_value = runprof_table_shr_mem[i].gset.ka_timeout; 
       } 
       else
         last_value = runprof_table_shr_mem[i].gset.ka_timeout; 
     }
  } //For loop
  global_settings->ka_timeout_all_flag = flag;
}
