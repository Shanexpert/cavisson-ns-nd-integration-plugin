
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_server.h"
#include "ns_error_codes.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "nslb_sock.h"
#include "poi.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_replay_access_logs.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "wait_forever.h"
#include "ns_event_log.h"
#include "ns_data_types.h"
#include "ns_common.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_js.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "rc4.h"
#include "ns_netstorm_diagnostics.h"
#include "tr069/src/ns_tr069_lib.h"
#include "ns_url_hash.h"
#include "ns_vuser.h"
#include "ns_proxy_server_reporting.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_group_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_ip_data.h"
#include "ns_trans_parse.h"
#include "ns_mongodb_api.h"
#include "ns_replay_db_query.h"
#include "ns_runtime_runlogic_progress.h" 
#include "ns_server_ip_data.h" 
#include "ns_trace_level.h"
#include "ns_trans.h"
#include "ns_dynamic_avg_time.h"
#include "ns_exit.h"
#include "ns_error_msg.h"


#ifndef CAV_MAIN
int user_group_table_size;
int user_cookie_table_size;
int user_dynamic_vars_table_size;
int user_var_table_size;
int user_order_table_size;
int user_svr_table_size;
int usr_table_size = 0;
int * my_runprof_table;

PerProcVgroupTable * my_vgroup_table;
#else
__thread int user_group_table_size;
__thread int user_cookie_table_size;
__thread int user_dynamic_vars_table_size;
__thread int user_var_table_size;
__thread int user_order_table_size;
__thread int user_svr_table_size;
__thread int usr_table_size = 0;
__thread int * my_runprof_table;

__thread PerProcVgroupTable * my_vgroup_table;
#endif

#ifdef RMI_MODE
int user_byte_vars_table_size;
#endif
int ultimate_max_vusers;
int parent_fd = -1;
int parent_dh_fd = -1;
int event_logger_fd = -1;
int event_logger_dh_fd = -1;
//unsigned short parent_port_number;
#ifndef CAV_MAIN
VUser* gFreeVuserHead = NULL;
int gFreeVuserCnt=0;
int gFreeVuserMinCnt=0;
int *scen_group_adjust;
int *seq_group_next;
#else
__thread VUser* gFreeVuserHead = NULL;
__thread int gFreeVuserCnt=0;
__thread int gFreeVuserMinCnt=0;
__thread int *scen_group_adjust;
__thread int *seq_group_next;
#endif
/**
timer_type global_timers[3];
timer_type* ramp_tmr = &global_timers[0];
timer_type* end_tmr = &global_timers[1];
timer_type* progress_tmr = &global_timers[2];
*/
timer_type global_timers[1];
timer_type* progress_tmr = &global_timers[0];
int num_connections;
int smtp_num_connections;
int pop3_num_connections;
int ftp_num_connections;
int dns_num_connections;
int ldap_num_connections;
int imap_num_connections;
int jrmi_num_connections;

#ifndef CAV_MAIN
avgtime *average_time = NULL; //used bt children to send progress report to parent
#else
__thread avgtime *average_time = NULL; //used bt children to send progress report to parent
#endif
//int max_parallel;
int total_badchecksums;
//int ramping_done;
unsigned int rp_handle, sp_handle, up_handle, gen_handle;

//global id key for use in DNS queries
rc4_key dns_global_id_key;
unsigned short child_idx;

void child_init()
{
  child_data_init();
  #ifndef CAV_MAIN
  test_data_init();
  #endif

}

void test_data_init()
{

    NSDL2_CHILD(NULL, NULL, "Method called");
    // Get the parent socket address allocated by the system
    // This was added for virutal user trace so that we find NVM0 address 
    // Can be used for other purposes in future
  
    struct sockaddr_in sockname;
    socklen_t socksize = sizeof (sockname);
    if (getsockname(parent_fd, (struct sockaddr *)&sockname, &socksize) < 0)
    {
       NS_EXIT(-1, CAV_ERR_1000003, parent_fd, errno, nslb_strerror(errno));
    }
    v_port_table[my_port_index].sockname = sockname;

    v_port_entry = &v_port_table[my_port_index];
    v_port_entry->pid = getpid();

    #ifndef CAV_MAIN
    if(run_mode == NORMAL_RUN )
       init_nvm_logging_shr_mem();
    #endif
    if(SHOW_GRP_DATA)
    {
       NSDL2_CHILD(NULL, NULL, "total_runprof_entries = %d, local group avgtime size = %d", total_runprof_entries, GROUP_VUSER_SIZE);
       MY_MALLOC_AND_MEMSET(grp_vuser, GROUP_VUSER_SIZE, "Malloc grp_vuser memory for show group data counters", -1);
    }

    if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
    {
       NSDL2_CHILD(NULL, NULL, "Allocating memory for per_grp_sess_inst_num, total_runprof_entries = %d", total_runprof_entries);
       MY_MALLOC_AND_MEMSET(per_grp_sess_inst_num, (sizeof(int) * total_runprof_entries), "Allocating memory for per_grp_sess_inst_num", -1);
    }

    my_runprof_table = &per_proc_runprof_table[my_port_index*total_runprof_entries];
        
    my_vgroup_table = &per_proc_vgroup_table[my_port_index*total_group_entries];

    NSDL3_CHILD(NULL, NULL, "my_port_index:%d mypid:%d vusers:%d fetches:%d", 
                             my_port_index, v_port_entry->pid, ultimate_max_vusers, v_port_entry->num_fetches);

    create_unique_group_tables();

    //Initialize Pacing entries for scenrio groups
    MY_MALLOC(scen_group_adjust, total_runprof_entries * sizeof(int), "scen_group_adjust", -1);
    bzero(scen_group_adjust, total_runprof_entries * sizeof(int));
    MY_MALLOC_AND_MEMSET(seq_group_next, (total_group_entries * sizeof(int)), "seq_group_next", -1);
	
    user_group_table_size = total_group_entries * sizeof(UserGroupEntry); 
    user_cookie_table_size = max_cookie_hash_code * sizeof(UserCookieEntry);
    user_dynamic_vars_table_size = max_dynvar_hash_code * sizeof(UserDynVarEntry);

    #ifdef RMI_MODE
    user_byte_vars_table_size = max_bytevar_hash_code * sizeof(UserByteVarEntry);
    #endif
    user_var_table_size = max_var_table_idx * sizeof(UserVarEntry);
    user_order_table_size = max_var_table_idx * sizeof(int); 
    user_svr_table_size = total_svr_entries * sizeof(UserSvrEntry);

  //if dns cache enabled 
  if(global_settings->protocol_enabled & DNS_CACHE_ENABLED)
    usr_table_size = total_svr_entries * sizeof(UserServerResolveEntry); 

  if (global_settings->load_key)
     gFreeVuserHead = allocate_user_tables(v_port_table[my_port_index].num_vusers);
  else
     gFreeVuserHead = allocate_user_tables(USER_CHUNK_SIZE);

  gFreeVuserMinCnt = gFreeVuserCnt;

  NSDL3_CHILD(NULL, NULL, "Init: Free Vuser Count =%d, ultimtate vusers=%d", gFreeVuserCnt, ultimate_max_vusers);

  cur_ip_entry = 0;


  tr069_nvm_init(v_port_entry->num_vusers);
  //Create dynamic URL hash table for each NVM in case dynamic url hash table is not disabled
  // and static urls are not using dynamic hash table
  //In case static urls are using dynamic hash table, it will get created by parent
  if(global_settings->static_url_hash_mode != NS_STATIC_URL_HASH_USING_DYNAMIC_HASH && 
       global_settings->dynamic_url_hash_mode != NS_DYNAMIC_URL_HASH_DISABLED)
       url_hash_dynamic_table_init();
  #ifdef NS_DEBUG_ON
  else
    dynamic_url_dump();
#endif
     #ifdef ENABLE_SSL
    /* Initialize the SSL stuff */
    ssl_main_init();
     #endif 

}
void child_data_init()
{
  u_ns_ts_t now;
  unsigned int seed;
  char *ip;
  char child_str[64 + 1];
  int cpid = getpid();

  NSDL2_CHILD(NULL, NULL, "Method called");
  
  
#ifndef NS_PROFILE
        /*NetCloud: DDR changes.
         * 
         * To support DDR in Netcloud, making unique child index across the test 
         * Therefore we need to combine generator id and NVM id
         * Format: 2 bytes(16 bits)
         * Generator id =(B1) 1byte(8 bits)
         * NVM Id =(B0) 1byte(8 bits)
         * _______
         * |B1|B0|
         * |__|__|
         * 
         * eg:  For generator id 1 and nvm id 1
         * child_idx: 0000000100000001
         */
  child_idx = (g_generator_idx>0?(g_generator_idx << 8):0) + my_child_index;
  NSDL2_CHILD(NULL, NULL, "child index = %hd, Hexa Representation = %016hx", child_idx, child_idx);

  if ((parent_fd = connect_to_parent()) < 0)  {
      NS_EXIT(-1, "");
  }
   fcntl(parent_fd, F_SETFD, FD_CLOEXEC);
   #ifndef CAV_MAIN
   if ((parent_dh_fd = connect_to_parent_dh()) < 0)  { /*bug 92660 */
      NS_EXIT(-1, "");
   }
   //Bug 84920: FDs are getting inherited to child processes. Need to avoid fd inheritance. So, set FD_CLOEXEC flag on FDs.
   fcntl(parent_dh_fd, F_SETFD, FD_CLOEXEC);
   #endif
   /*Do not connect as process is not FORKED*/
   if(global_settings->enable_event_logger) {
     if(!send_events_to_master) {
	    ip = "127.0.0.1";
     } else {
	    ip = master_ip;
   }
   /*Child can send save data API msg so we need to connect & init socket communication
     event if filter mode is set to DO_NOT_LOG_EVENT*/
     /*Connect to Event Logger*/
     if ((event_logger_fd = connect_to_event_logger(ip, event_logger_port_number)) < 0)  {
             snprintf(child_str, 64, "%s%d", (!send_events_to_master)? "NVM" : "Generator NVM", my_port_index);
   	     fprintf( stderr, "\n%s:  Error in creating the TCP socket to " 
	 				   "communicate with the event logger (%s:%d)."
					   "  Aborting...\n", child_str, ip, event_logger_port_number);
	     end_test_run();
   	  }
          sprintf(g_el_subproc_msg_com_con.conn_owner, "NVM_TO_EVENT_LOGGER");
    
        }
#endif


	srand(cpid);  /* get a different random sequence for each child process */
   #ifndef CAV_MAIN
        realloc_avgtime_and_set_ptrs(g_avgtime_size, 0, NEW_OBJECT_DISCOVERY_NONE);
   #endif

        /*Craete runtime for Each this NVMs it JAVA SCRIPT Engine is Enabled for any of GROUP*/
        js_init();

        /* If mongodb is enabled then initialised mongo client */
        ns_mongodb_init(); 


	progress_tmr->timer_type = -1;
	progress_tmr->next = NULL;
	progress_tmr->prev = NULL;

	num_connections = 0;
	smtp_num_connections = 0;
	//max_parallel = 0;

	/* Initialize the statistics. */
	//ramping_done = 0;
	/* Initialize the rest. */
	now = get_ms_stamp();
	//start_time = now;
	seed = now;
	seed ^= cpid;
	seed ^= getppid();
	srandom( seed );
        
	rp_handle = ns_rand_init(cpid, TEN_MILLION);
  
        // sp_handle is used in replay mode REPLAY_DB_QUERY_USING_FILE, in this case user profile will not be used
        if(global_settings->db_replay_mode == REPLAY_DB_QUERY_USING_FILE)
          sp_handle = ns_rand_init(cpid*5, cum_query_pct);
        else
	  sp_handle = ns_rand_init(cpid*5, 100);
	//up_handle = ns_rand_init(v_port_entry->pid, 100000000);
	up_handle = ns_rand_init(cpid, 1000000);
	gen_handle = ns_rand_init(cpid, 0x0fffffff);

        if(global_settings->replay_mode)
          ns_parse_usr_info_file(my_port_index);

        //used only for DNS - not checking return value - this will be logged from init_id_key()
        init_id_key(&dns_global_id_key, ARES_ID_KEY_LEN);
   #ifndef CAV_MAIN
        // sets pointer to the num_nvm_users_shm for this child
        init_user_num_shm();
   #endif
}
