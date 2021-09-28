#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include <curl/curl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <v1/topolib_structures.h>
#include "url.h"
#include "../../base/topology/topolib_v1/topolib_init.c"
#include "../../base/topology/topolib_v1/topolib_ns_methods.c"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_gdf.h"
#include "ns_msg_com_util.h"
#include "ns_log.h"
#include "ns_user_monitor.h"
#include "ns_custom_monitor.h"
#include "ns_batch_jobs.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_child_msg_com.h"
#include "ns_percentile.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_mon_log.h"
#include "ns_event_log.h"
#include "ns_server_admin_utils.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "nslb_sock.h"
#include "nslb_alloc.h"
#include "netstorm.h"

#include "ns_get_log_file_monitor.h"
#include "ns_monitor_profiles.h"
#include "ns_monitoring.h"
#include "ns_coherence_nid_table.h"
#include "ns_trace_level.h"
#include "ns_compress_idx.h"
#include "ns_nv_tbl.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_parent.h"
#include "ns_ndc.h"
#include "ns_ndc_outbound.h"
#include "ns_standard_monitors.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_exit.h"
#include "wait_forever.h"
#include "nslb_mon_registration_util.h"
#include "ns_appliance_health_monitor.h"
#include "nslb_mon_registration_con.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_monitor_metric_priority.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_tsdb.h"
#include "cav_tsdb_interface.h"
#include "ns_monitor_init.h"
extern int generate_aggregate_vector_list(CM_info *cus_mon_ptr, char *vector_line, char **overall_list);
#define DELTA_CUSTOM_MONTABLE_ENTRIES 5
#define MAX_CM_MSG_SIZE               4096 
#define MAX_NAME_SIZE                 1024 
#define MAX_FIELDS_FOR_MSSQL		7       // FIelds in mssql_monitors.dat file.

#define TIMESTAMP_NEEDED 	0x01
#define TIMESTAMP_NOT_NEEDED 	0x02
#define DELTA_MON_ENTRIES   5
#define MAX_CSV_COLUMN     128
char g_enable_tcp_keepalive_opt;
int g_tcp_conn_idle;
int g_tcp_conn_keepintvl;
int g_tcp_conn_keepcnt;

int g_vector_runtime_changes;
int g_monitor_runtime_changes;
int g_monitor_runtime_changes_NA_gdf;
int new_monitor_first_idx;

int total_deleted_vectors = 0;
int total_waiting_deleted_vectors = 0;
int total_deleted_servers = 0;
int total_deleted_instances = 0;

//Deleting vector frequency handle
bool g_enable_delete_vec_freq = false;
int g_delete_vec_freq_cntr = 0 ;
char g_delete_vec_freq = 5;           //default frequency is 5 for delete frequency

//MSSQL_table *mssql_table_ptr = NULL;
//Taken for NODE_POD Discovery
NormObjKey *node_id_key;
NodeList *node_list; 
char norm_tbl_init_done_for_node = 0;
NormObjKey *dyn_cm_rt_hash_key = NULL;

Monitor_list *monitor_list_ptr = NULL;
Monitor_list *rtgRewrt_monitor_list_ptr = NULL;

CM_info *cm_bt_percentile_ptr = NULL;
CM_info *cm_ip_percentile_ptr = NULL;
HiddenFileInfo *hidden_file_ptr = NULL;

int total_monitor_list_entries = 0;     //total entries of monitors
int max_monitor_list_entries = 0;

int total_hidden_file_entries = 0;
int max_hiddlen_file_entries = 0;

int total_mssql_row = 0;
int max_mssql_row = 0;
int node_list_entries = 0;
int max_node_list_entries = 0;

int is_mssql_monitor_applied = 0;
int init_size_for_dr_table = 0;
int delta_size_for_dr_table = 500;
int max_size_for_dr_table = 10000;
int max_dr_table_entries;
int max_dr_table_tmp_entries;

//Global buffer for cm_update_monitor request if we will not send the msg to ndc then we will store this in buffer and retry again
int total_cm_update_monitor_entries;
int max_cm_update_monitor_entries;
char **cm_update_monitor_buf;

DvmVectorIdxMappingTbl **dvm_idx_mapping_ptr = NULL;

char *auto_json_monitors_ptr = NULL;	//ptr to json buffer
char *auto_json_monitors_diff_ptr = NULL;	//ptr to json diff buffer
Mon_from_json *json_monitors_ptr = NULL;	//table to store standard  monitors which are applied through json at runtime

char skip_unknown_breadcrumb = 0;

int total_rtgRewrt_monitor_list_entries = 0;
char g_rtg_rewrite = 0 ;

int total_mapping_tbl_row_entries = 0; // total number of monitors in 2-d matrix
int max_mapping_tbl_row_entries = 0; // max number of monitors in 2-d matrix
int total_dummy_dvm_mapping_tbl_row_entries = 0; // This is to realloc dvm_idx_mapping_table

int total_json_monitors = 0; //total no of rows occupied in table storing standard  monitors which are applied through json at runtime
int max_json_monitors = 0; //max no of rows of table storing standard  monitors which are applied through json at runtime

//int monitor_log_fd = -1; //monitor.log fd
  
int new_vector;    //Flag to indicate the new vector 

char ns_version[11];

/* Global variable for Custom monitor Disaster Recovery */
CM_info **cm_dr_table;        // This table will contain index of those custom monitor which are exited during the test due
static CM_info **cm_dr_table_tmp;    // This is a temprary table, used to store exited custom monitor entry for next retry 

int cm_retry_flag = 0;                    // custom monitor retry flag, will retry connection once closed or not
int max_cm_retry_count = 0;               //custom monitor retry count
int g_total_aborted_cm_conn = 0; 
int num_aborted_cm_non_recoved = 0;
inline void cm_update_dr_table(CM_info *cus_mon_ptr);
char *strptime(const char *s, const char *format, struct tm *tm);
int create_vector_and_delete_percentile(char *field,struct CM_info *tmp_cm_ptr);
int conn_timeout;

int monitor_runtime_changes_applied = 0; //For any runtime change ADD/DELETE MONITOR/VECTOR we have to set this flag, because we are applying runtime changes on basis this flag

int monitor_deleted_on_runtime = 0;     // ONLY FOR DELETE
int monitor_added_on_runtime = 0;

int runtime_dynamic_vector_list_found = 0;

//int nv_monitor_start_idx = -1; // initialize with '-1', after first progress interval setting this to '-2'
int nv_vector_format = 0;

int nc_ip_data_mon_idx = -1;
GenVectorIdx **genVectorPtr = NULL;

/*-------------------------------------*/

extern void cm_info_runtime_dump();
CM_info *save_cm_info_runtime_ptr =  NULL;
int total_save_cm_runtime_entries = 0; 

extern Group_Info *group_data_ptr;
extern Graph_Info *graph_data_ptr;

//flag to dump the monitors_tables
int dump_monitor_tables = 0;

int auto_json_mon_first_time_flag = 0;

int warning_no_vectors = 0;

//serial_no field for mssql monitors
int serial_no;

char g_validate_nd_vector = 0;
char g_log_mode = 0;
char gdf_log_pattern[1024] = {0}; 
FILE *g_nd_monLog_fd = NULL;

Long_long_data nan_buff[128] = {0};
int is_norm_init_done = 0;

int is_outbound_connection_enabled = 0;
int g_all_mon_id = 1;
int g_mon_id = 0;
int g_chk_mon_id = 0;
int max_mon_id_entries = 0;
MonIdMapTable *mon_id_map_table = NULL;

MBeanMonInfo *mbean_mon_ptr = NULL;
int total_mbean_mon = 0;
int max_mbean_mon = 0;
int mbean_monitor_offset = 0;
char mbean_mon_rtc_applied = 0;
char flag_for_non_enc_file=0;

int g_remove_trailing_char;
char g_trailing_char[256] = {0};


struct id_pool mon_id_pool = {NULL, 0, 0};

//get norm_id_for_vectors
int get_norm_id_for_vectors(char *vector_name, char *gdf_name, int *new_flag);

//Heart Beat
int make_nb_cntrl_connection(ServerInfo *svr_list, int do_not_add_fd_in_epoll);
//-------------------------- Usage -----------------------
long long calculate_timestamp_from_date_string(char * buffer);
int enable_store_config = 0;

//int read_mssql_file(char *);
//int convert_buf_to_array(char *, int *);
//int fill_data_in_csv_for_mssql_new(CM_info * ,char *);
//int delete_node_pod(int, char*, char*);

#if 0
void initialize_reused_deleted_vector()
{
  MY_MALLOC_AND_MEMSET(deleted_vector, (sizeof(CM_info *) * CM_DR_ARRAY_SIZE), "deleted_vector", -1);
  max_deleted_vectors = CM_DR_ARRAY_SIZE;
  MY_MALLOC_AND_MEMSET(reused_vector, (sizeof(CM_info *) * CM_DR_ARRAY_SIZE), "reused_vector", -1);
  max_reused_vectors = CM_DR_ARRAY_SIZE;

  //DR tables
  MY_MALLOC_AND_MEMSET(cm_dr_table, (sizeof(CM_info *) * init_size_for_dr_table), "DR table", -1);
  max_dr_table_entries = init_size_for_dr_table;
  MY_MALLOC_AND_MEMSET(cm_dr_table_tmp, (sizeof(CM_info *) * init_size_for_dr_table), "Tmp DR table", -1);
  max_dr_table_tmp_entries = init_size_for_dr_table;
}
#endif

void create_entry_in_reused_or_deleted_structure(int **input, int *max_entries)
{
  MY_REALLOC_AND_MEMSET(*input, (sizeof(int) * (*max_entries +  5)), (sizeof(int) * (*max_entries)), "create_entry_in_reused_or_deleted_structure", -1);
  *max_entries = *max_entries +  5;
}

int check_allocation_for_reused(CM_info *cus_mon_ptr)
{
  if(cus_mon_ptr->total_reused_vectors >= cus_mon_ptr->max_reused_vectors)
  {
    MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Monitor name = %s, total_reused_vectors = %d and max_reused_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_reused_vectors, cus_mon_ptr->max_reused_vectors); 
   create_entry_in_reused_or_deleted_structure(&cus_mon_ptr->reused_vector, &cus_mon_ptr->max_reused_vectors);
   MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "After create_entry_in_reused_or_deleted_structure Now total_entries = %d max_entries = %d", cus_mon_ptr->total_reused_vectors, cus_mon_ptr->max_reused_vectors);
  }
  return 0;
}

/*static void usage(char *error, char *mon_name, char *mon_args, int runtime_flag, char *err_msg)
{
//TODO: msg is overwriting in err_msg.....check??

  NSDL2_MON(NULL, NULL, "Method called");
  sprintf(err_msg, "\n%s:\n%s %s\n", error, mon_name, mon_args);
  strcat(err_msg, "Usage:\n");

  strcat(err_msg, "CUSTOM_MONITOR <Create Server IP {NO | NS | Any IP}> <GDF FileName> <Vector Name> <Option {Run Every Time (1) | Run Once (2)}> <Program Path> [Program Arguments]\n");
  //printf("Where: \n"); // need to write help enteries
  strcat(err_msg, "Example:\n");
  strcat(err_msg, "CUSTOM_MONITOR 192.168.18.104 cm_vmstat.gdf VMStat 2 /opt/cavisson/monitors/samples/cm_vmstat\n\n");
}*/

//-------------------------- End Usage -----------------------

//--------------------------- Config part --------------------

/******************************************************************
 *  * Purpose  : This function will return Netstorm version with build 
 *               Like - 3.9.0.10
 *               Here - first three digit shows Netstorm version i.e. 3.9.0 shows netstorm version
 *                   and last 4th digit show netstorm build i.e 10 show netstorm build 
 *           
 * Input    : version - this is output buffer   
 *****************************************************************/


//***************************************************DVM Indexing Code Starts**********************************************************
void reset_scratch_buffer_length(int reset_length)
{
  monitor_scratch_buf_len = reset_length;
}

void copy_to_scratch_buffer_for_monitor_request(char *buffer, int length, char reset_flag, int reset_length)
{
  int pending_length;

  if(reset_flag == 1)
  {
    if(reset_length > 0)
      monitor_scratch_buf_len = reset_length;
    else
      monitor_scratch_buf_len = 0;
  }

  pending_length = monitor_scratch_buf_size - (monitor_scratch_buf_len + length + 1);

  if(pending_length < 0)
  {
      if(length < INITIAL_CM_INIT_BUF_SIZE)
      {
         monitor_scratch_buf_size = monitor_scratch_buf_size + INITIAL_CM_INIT_BUF_SIZE;
      }
      else
      {
         monitor_scratch_buf_size = monitor_scratch_buf_size + length;
      }
      MY_REALLOC(monitor_scratch_buf, monitor_scratch_buf_size + 1, "monitor_scratch_buf", -1);
   }
  
  monitor_scratch_buf_len += sprintf(monitor_scratch_buf + monitor_scratch_buf_len, "%s", buffer);
  NSTL2(NULL, NULL, "Scratch Buffer length = %d, Scratch Buffer = %s", monitor_scratch_buf_len, monitor_scratch_buf);
}


//Maninder: Function added for dvm indexing

int create_mapping_table_row_entries()  //create rows of DVM Mapping table
{ 
  NSDL4_MON(NULL, NULL, "Method called. total = %d, max = %d, ptr = %p, size = %d, name = DVM Vector Idx Mapping Table", total_mapping_tbl_row_entries, max_mapping_tbl_row_entries, dvm_idx_mapping_ptr, sizeof(DvmVectorIdxMappingTbl));  
 
  //MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(dvm_idx_mapping_ptr, ((max_mapping_tbl_row_entries + DELTA_DVM_CM_IDX_MAPPING_TABLE_ENTRIES + num_passed_runtime_dyn_mon) * sizeof(void *)), (max_mapping_tbl_row_entries * sizeof(void *)), "DVM Vector Idx Mapping Table Row", -1);
  MY_REALLOC(dvm_idx_mapping_ptr, ((max_mapping_tbl_row_entries + DELTA_DVM_CM_IDX_MAPPING_TABLE_ENTRIES + total_monitor_list_entries - new_monitor_first_idx ) * sizeof(void *)), "DVM Vector Idx Mapping Table Row", -1);

  max_mapping_tbl_row_entries += (DELTA_DVM_CM_IDX_MAPPING_TABLE_ENTRIES + total_monitor_list_entries - new_monitor_first_idx);
  
  return 0;
}

int create_mapping_table_col_entries(int row_no, int vec_count, int max_mapping_tbl_vectors_entries)  //create columns of DVM Mapping table
{ 
  NSDL4_MON(NULL, NULL, "Method called. total = %d, max = %d, ptr = %p, size = %d, name = DVM Vector Idx Mapping Table", total_mapping_tbl_row_entries, max_mapping_tbl_row_entries, dvm_idx_mapping_ptr[row_no], sizeof(DvmVectorIdxMappingTbl));

  if(max_mapping_tbl_vectors_entries != 0)
  {
    MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(dvm_idx_mapping_ptr[row_no], ((vec_count + DELTA_DVM_CM_IDX_MAPPING_TABLE_ENTRIES) * sizeof(DvmVectorIdxMappingTbl)), (max_mapping_tbl_vectors_entries * sizeof(DvmVectorIdxMappingTbl)), "DVM Vector Idx Mapping Table Row", -1);
  }
  else
  {
    NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(dvm_idx_mapping_ptr[row_no], ((vec_count + DELTA_DVM_CM_IDX_MAPPING_TABLE_ENTRIES) * sizeof(DvmVectorIdxMappingTbl)), "DVM Vector Idx Mapping Table Row", -1, NULL);
  }

  return (vec_count + DELTA_DVM_CM_IDX_MAPPING_TABLE_ENTRIES);
} 

int handle_mapping_table_for_data_filling(char *vector_name, CM_info *cus_mon_ptr, int id)
{
    int i;
    int norm_id;
    int total_vectors;
    CM_vector_info *vector_list = cus_mon_ptr->vector_list;

    NSDL4_MON(NULL, NULL, "Method Called. Monitor_name: %s, Vector_name: %s, dvm_cm_mapping_tbl_row_idx = %d", cus_mon_ptr->monitor_name, vector_name, cus_mon_ptr->dvm_cm_mapping_tbl_row_idx);
    // found new vector add in runtime table & set flag 'monitor_runtime_changes_applied'
    // refer to section : Addition of new vector
    if(dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx == -1)
    {
      //to handle case if any old vector comes with new vector id and relative_dyn_idx is not set in dvm_idx_mapping_ptr
      //TSDBKJ remove for TSDB
      if (!(g_tsdb_configuration_flag & TSDB_MODE))
      {
        if(total_vector_gdf_hash_entries > 0 && (cus_mon_ptr->nd_norm_table.size > 0))
        {
          norm_id = nslb_get_norm_id(&cus_mon_ptr->nd_norm_table, vector_name, strlen(vector_name));
          if(norm_id >= 0)
            i = norm_id;
          else
          {
            dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].is_data_filled = 1;
            return -1;
          }
          total_vectors = i+1;
        }
        else
        {
          i=0;
          total_vectors = cus_mon_ptr->total_vectors;
        }
        MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "handle_mapping_table_ i=%d, total_vectors=%d, gdf=%s",
                                                i, total_vectors,cus_mon_ptr->gdf_name_only);

        for (; i < total_vectors; i++)
        {
          if(!strcmp(vector_list[i].vector_name, vector_name))
          {
            MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "vector_list vector name : %s", vector_list[i].vector_name);

          //dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx = i;

            dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].is_data_filled = 1;
          
          //vector_list[i].vectorIdx = id;
 
          //vector_list[i].vector_state = CM_INIT;

            if(vector_list[i].vector_state == CM_DELETED || (vector_list[i].flags & WAITING_DELETED_VECTOR))
            {
              MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Deleted custom monitor whose vector is (%s) has been added successfully\n", vector_list[i].vector_name);
              if(!(vector_list[i].flags & WAITING_DELETED_VECTOR))
              {
                if(!(cus_mon_ptr->flags & NA_GDF))
                {
                  MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "calling g_vector_runtime_chnage %s", cus_mon_ptr->gdf_name);
                  g_vector_runtime_changes = 1;
                }
                else
                {
                  MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "calling g_monitor_runtime_chnage %s", cus_mon_ptr->gdf_name);
                  g_monitor_runtime_changes_NA_gdf = 1;
                }
                check_allocation_for_reused(cus_mon_ptr);
                cus_mon_ptr->reused_vector[cus_mon_ptr->total_reused_vectors] = i;
                cus_mon_ptr->total_reused_vectors++;
              }
              else
              {
                MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "waiting deleted vector-------");
                total_waiting_deleted_vectors--;
              }
              set_reused_vector_counters(vector_list, i, cus_mon_ptr, cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port, cus_mon_ptr->pgm_path, cus_mon_ptr->pgm_args, MON_REUSED_NORMALLY);
              vector_list[i].vector_state = CM_INIT; 
            }
          //CM_VECTOR_RESET is only marked when connection error is received on monitor connection. It means, we willnot be updating mapping table for it. It will always be -1. So it will come only in this check. Need to remove RESET_VECTOR if vector is received. 
            else if(vector_list[i].vector_state == CM_VECTOR_RESET)
            {
              MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "cm_vector_reset ---------- vector_state");
              vector_list[i].vector_state = CM_INIT;
            }
            else
            {
              MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Data is received for same vector =%s with different vector id. Hence, marking the relative_dyn_idx for old vector id as -1 Old dvm_id= %d New dvm_id = %d", vector_list[i].vector_name, vector_list[i].vectorIdx, id);
              dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vector_list[i].vectorIdx].relative_dyn_idx = -1;
              dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vector_list[i].vectorIdx].is_data_filled = -1;
            }
            vector_list[i].vectorIdx = id;
            dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx = i;
 
            return (dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx);
          }

        }
      }	
      //TSDBKJ remove for TSDB end	 

      //setting is_data_filled 1 here, because when vector is received for the first time, it does not traverse the above block of strcmp() because it does not have that vector yet in its structure and hence its is_data_filled is not set for the first time. When data is received for 2nd time, is_data_filled will be set from above block. But if we talk about the case where from 2nd sample onwards vector name is not sent. Then is_data_filled will never be set as it wont be going in above block because it doesnot have vecctor name to compare with. So data was ignored and a log was logged after returning from this function.
      dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].is_data_filled = 1;

      return -1;
    }
    else
    {
      // old vector has been reset 
      //Compare vector name in CM_Info table at index :cm_info_ptr[dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx ][ id ].dyn_idx ]
      //if(!strcmp(cm_info_ptr[dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx].vector_name, vector_name))
      if(!strcmp(vector_list[dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx].vector_name, vector_name))
      {
        //set is_data_filled to 1 in mapping table
        dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].is_data_filled = 1;

        if((vector_list[dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx].vector_state == CM_DELETED) || (vector_list[dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx].flags & WAITING_DELETED_VECTOR))
        { 
          MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Deleted custom monitor whose vector is (%s) has been added successfully\n", vector_list[dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx].vector_name);
          if(!(vector_list[dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx].flags & WAITING_DELETED_VECTOR))
          {
             MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "calling g_vector_runtime_chnage %s", cus_mon_ptr->gdf_name); 
            g_vector_runtime_changes = 1;
 
            check_allocation_for_reused(cus_mon_ptr);
            cus_mon_ptr->reused_vector[cus_mon_ptr->total_reused_vectors] = dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx;
            cus_mon_ptr->total_reused_vectors++;
          }
          else
            total_waiting_deleted_vectors--;
          
          set_reused_vector_counters(vector_list, dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx, cus_mon_ptr, cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port, cus_mon_ptr->pgm_path, cus_mon_ptr->pgm_args, MON_REUSED_NORMALLY);
          
        }

        return (dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx);
      }
      else
      {   
        // compare vector name in all the vectors
	//TSDBKJ remove for TSDB
	if (!(g_tsdb_configuration_flag & TSDB_MODE))
        {
          if((total_vector_gdf_hash_entries > 0) && (cus_mon_ptr->nd_norm_table.size > 0))
          {
            norm_id = nslb_get_norm_id(&cus_mon_ptr->nd_norm_table, vector_name, strlen(vector_name));
            if(norm_id >= 0)
              i = norm_id;
            else
              return -1;
            total_vectors = i+1;
          }
          else
          {
            i=0;
            total_vectors = cus_mon_ptr->total_vectors;
          }
          MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "handle_mapping_table else case i=%d, total_vectors=%d, gdf=%s",
                                                              i, total_vectors,cus_mon_ptr->gdf_name_only);

          for (; i < total_vectors; i++)
          {
            if(!strcmp(vector_list[i].vector_name, vector_name))
            {
            // update idx & set is_data_filled to 1 in mapping
            // table & break the loop
            //dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx = cus_mon_ptr->parent_idx + i;
            //dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx = i;

              dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].is_data_filled = 1;

            //vector_list[i].vectorIdx = id;

              if((vector_list[i].vector_state == CM_DELETED) || (vector_list[i].flags & WAITING_DELETED_VECTOR))
              { 
                MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Deleted custom monitor whose vector is (%s) has been added successfully\n", vector_list[i].vector_name);
                if(!(vector_list[i].flags & WAITING_DELETED_VECTOR))
                {
                   MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "calling g_vector_runtime_chnage %s", cus_mon_ptr->gdf_name);
                   g_vector_runtime_changes = 1;
           
                   check_allocation_for_reused(cus_mon_ptr);
                   cus_mon_ptr->reused_vector[cus_mon_ptr->total_reused_vectors] = i;
                   cus_mon_ptr->total_reused_vectors++;
                }
                else
                  total_waiting_deleted_vectors--;
              
                set_reused_vector_counters(vector_list, i, cus_mon_ptr, cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port, cus_mon_ptr->pgm_path, cus_mon_ptr->pgm_args, MON_REUSED_NORMALLY);

              } 
              else
              {
                MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Data is received for same vector= %s with different vector id. Hence, marking the relative_dyn_idx for old vector id as -1 Old dvm_id= %d New dvm_id = %d", vector_list[i].vector_name, vector_list[i].vectorIdx, id);
                dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vector_list[i].vectorIdx].relative_dyn_idx = -1;
                dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vector_list[i].vectorIdx].is_data_filled = -1;
              }

              dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx = i;
              vector_list[i].vectorIdx = id;


              return (dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx);
            }
          }
	}
	//TSDBKJ remove for TSDB end
      }       
    }
  return -1;   
}
//****************************************************DVM Indexing Code Ends*************************************************************


char *encode_message(char *msg_buf, int msg_len)
{
  char *ptr;
  
  ptr = curl_escape(msg_buf, msg_len);
  return ptr;
}


void get_ns_version_with_build_number()
{
  FILE *app = NULL;

  NSDL2_MON(NULL, NULL, "Method called");

  if((app = popen("nsu_get_version -v", "r")) == NULL)
  {
    printf("ERROR: popen failed for command nsu_get_version -v. Error = %s\n" ,nslb_strerror(errno));
    return;
  }

  fgets(ns_version, 11, app);

  ns_version[strlen(ns_version) - 1] = '\0';

  //Fixed nsu_get_version defunct issue
  if(pclose(app) == -1)
    printf("ERROR : pclose() failed for nsu_get_version\n");

  NSDL3_MON(NULL, NULL, "ns_version = [%s]", ns_version);
}


void stop_mon_config_monitor(CM_info *cus_mon_ptr, int reason)
{
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  int norm_id;

  if(cus_mon_ptr->any_server)
  {
    //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
    if(total_mon_config_list_entries > 0)
    {
      if(cus_mon_ptr->instance_name == NULL)
         sprintf(tmp_buf, "%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name,
                                     global_settings->hierarchical_view_vector_separator, cus_mon_ptr->g_mon_id);
      else
         sprintf(tmp_buf, "%s%c%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->instance_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->g_mon_id);
    }

    else
    {
      if(cus_mon_ptr->instance_name == NULL)
        sprintf(tmp_buf, "%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name);
      else
        sprintf(tmp_buf, "%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator,
                                     cus_mon_ptr->instance_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name);
    }

    nslb_delete_norm_id_ex(specific_server_id_key, tmp_buf, strlen(tmp_buf), &norm_id);

    NSDL1_MON(NULL, NULL, " Monitor tmp_buf '%s' deleted from hash table for tier_name '%s'\n", tmp_buf, cus_mon_ptr->tier_name);
  }

  if(stop_one_custom_monitor(cus_mon_ptr, reason) == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                            "Error in sending end_monitor to mon whose vector name is [%s]. Therefore going to close fd and mark deleted",
                                 cus_mon_ptr->monitor_name);
    handle_monitor_stop(cus_mon_ptr, reason);
  }
}
/*
TODO:
//status=OK\n
status=ERROR;msg=error message\n
status=SUCCESS;msg=Monitor started successfully\n
//status=STOPPED;msg=Monitor stopped successfully\n
*/
static int update_mon_config_status(CM_info *cus_mon_ptr)
{
  MonConfig *mon_config;

  char *buffer = cus_mon_ptr->data_buf;
  char status[128] = "\0";
  int num_fields, id, i;
  char *field[5];

  NSDL3_MON(NULL, NULL, "Method called. buffer = %s", buffer);

  mon_config = mon_config_list_ptr[cus_mon_ptr->mon_info_index].mon_config;
  num_fields = get_tokens(buffer, field,";", 2);
  for(id =0; id < mon_config->total_mon_id_index; id++)
  {
    NSDL1_MON(NULL, NULL, "Going to process status of %s G_MON_ID with buffer %s", mon_config->g_mon_id, buffer);
    //and find out the mon_id
    if(cus_mon_ptr->mon_id == mon_config->mon_id_struct[id].mon_id)
    {
      for(i = 0; i<num_fields; i++)
      {
        if(field[i])
        {
          if(!strncmp(field[i],"status=",2))
          {
            if(strlen(field[i] + 7) > 0)
              strcpy(status, (field[i] + 7));

            if(!strncmp(status,"ERROR",5))
            {
              mon_config->mon_id_struct[id].status = MJ_FAILURE;
              mon_config->mon_err_count +=1;
              if(cus_mon_ptr->sm_mode == SM_RUN_ONCE_MODE)
                stop_mon_config_monitor(cus_mon_ptr, SM_DELETE_ON_RECEIVE_ERROR);
              else
                stop_mon_config_monitor(cus_mon_ptr, MON_STOP_ON_RECEIVE_ERROR);
            }
            else if(!strncmp(status,"STOPPED",7))
            {
              mon_config->mon_id_struct[id].status = MJ_STOPPED;
              //mon_config->mon_err_count +=1;
              stop_mon_config_monitor(cus_mon_ptr, SM_DELETE_ON_RECEIVE_ERROR);
            }
            else if(!strncmp(status,"SUCCESS",7))
            {
              mon_config->mon_id_struct[id].status = MJ_SUCCESS;
            }
            else
            {
              MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Received Invalid status for monitor. Status = %s", status);
            }
          }
          else if(!strncmp(field[i],"msg=",4))
          {
            if(strlen(field[i] + 4) > 0)
              strcpy(mon_config->mon_id_struct[id].message, (field[i] + 4));
          }
        }
      }
      break;
    }
  }
  return 1;
}

static int validate_data(CM_info *cus_mon_ptr)
{
  //int i = 0;
  char *buffer = cus_mon_ptr->data_buf;
  NSDL3_MON(NULL, NULL, "Method called. buffer = %s", buffer);
 
  //Error pattern can be inside event, therefore moving this check above.
  //log to event if EVENT:X.Y:..is there
  if(!ns_cm_monitor_event_command(cus_mon_ptr, buffer, _FLN_))
    return 1;
    
  if(strstr(buffer, "Error:"))
  {
    MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_CRITICAL, "%s", buffer);
    return 1;
  }
  return 0;
}

static char *cm_to_str(CM_info *cus_mon_ptr)
{
  static char cm_buf[4*1024];
  snprintf(cm_buf, 4*1024, "MonitorName = %s, ProgramName with args = (%s %s), gdf = %s, ServerIP = %s, ServerPort = %d, fd = %d, dindex = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->pgm_path, cus_mon_ptr->pgm_args, cus_mon_ptr->gdf_name?cus_mon_ptr->gdf_name:"NULL", cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port, cus_mon_ptr->fd, cus_mon_ptr->dindex);
  return(cm_buf);
}

static char *cm_event_msg(CM_info *cus_mon_ptr)
{
  static char cm_event_msg_buf[3 * 1024] = "\0";
  char src_add[1024] = "\0";

  strcpy(src_add, nslb_get_src_addr(cus_mon_ptr->fd));

  /* In cm_get_log_file & log event monitor -> we have dummy gdf that's why we skip their gdf parsing.
   *                                           that's the reason we can't access structure 'Group_Info' in below log message.
   *                                           hence skipping calling of get_gdf_group_name() for both these monitors.
   *
   * In log parser -> '-M' option is for LPS LOG EVENT Monitor. */
  if((cus_mon_ptr->gdf_flag == CM_GET_LOG_FILE) || (cus_mon_ptr->flags & ORACLE_STATS) || (cus_mon_ptr->flags & ALL_VECTOR_DELETED) ||
             ((strstr(cus_mon_ptr->pgm_args, "cm_log_parser") != NULL) && (strstr(cus_mon_ptr->pgm_args, " -M ") != NULL)))
  {
    sprintf(cm_event_msg_buf, "monitor name(%s), source address '%s',  destination address '%s:%d'.", 
                               cus_mon_ptr->monitor_name,
                               (!strcmp(src_add, "Unknown or Not Connected") || (src_add == NULL))?"NSAppliance":src_add, 
                               cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
  }
  else
  {
    sprintf(cm_event_msg_buf, "monitor %s(%s), source address '%s',  destination address '%s:%d'.", 
                               get_gdf_group_name(cus_mon_ptr->gp_info_index), cus_mon_ptr->monitor_name, 
                               (!strcmp(src_add, "Unknown or Not Connected") || (src_add == NULL))?"NSAppliance":src_add, 
                               cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
  }
  return(cm_event_msg_buf);
}

//remove
void check_cavinfo_value(char *value)
{
  NSDL2_MON(NULL, NULL, "Method called, value = [%s]", value);
  if(!value)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: cav.conf is not properly set\n");
    NS_EXIT(-1,CAV_ERR_1060028);
  }
}

int validate_and_fill_data(CM_info *cus_mon_ptr, int max, char *buffer, Long_long_data *data, void *metrics_idx, char *metric_id_found, char *vector_name)
{
  int i = 0, j = 0;
  char *ptr = NULL;
  char *end_ptr = NULL;
  int check_for_excluded_graph = 0;
  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;
 
  //Long_long_data *data = cus_mon_ptr->data;

 MLTL3(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                    "validate_and_fill_data , Received vector name  %s for monitor_name = %s ", vector_name, cus_mon_ptr->monitor_name);


  if(buffer[0] == 'E' && buffer[1] == 'r' && buffer[2] == 'r' && buffer[3] == 'o' && buffer[4] == 'r' && buffer[5] == ':') 
  {
    MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                    "Received Error (%s) for monitor %s", buffer, cus_mon_ptr->monitor_name);

    return -1;
  }

  if(max == 0) //parent has '0' no_of_elements means gdf is yet not parsed
    return 0;
  ptr = strtok_r(buffer," ",&buffer);

  if(cus_mon_ptr->gp_info_index >= 0)
  {
    //This handling is done to exclude data 
    local_group_data_ptr = group_data_ptr + cus_mon_ptr->gp_info_index;
    local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;
 
    if(local_group_data_ptr->num_actual_graphs[MAX_METRIC_PRIORITY_LEVELS] != local_group_data_ptr->num_graphs)
      check_for_excluded_graph = 1;
  }
  
  while(ptr)
  {
    ++i;
    //if (i > max)

    NSDL2_MON(NULL, NULL, "i = %d, max = %d", i, max); 
    
    if(check_for_excluded_graph)
    {
      if(local_graph_data_ptr->graph_state == GRAPH_EXCLUDED)
      {
        if(IS_TIMES_GRAPH(local_graph_data_ptr->data_type) || (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_2B_100_COUNT_4B) ||
           IS_TIMES_STD_GRAPH(local_graph_data_ptr->data_type))
          j = i + 4;
        else
          j = i + 1;

        while((ptr) && (i<j)) 
        { 
          ++i;
          ptr = strtok_r(buffer," ",&buffer);
        }
        //To revert the increament which occured previously. As i is being increased in the outer loop.
        i--;
        local_graph_data_ptr++;
        j = 0;
        continue;
      }

      //This block is to skip times graph data element. If data count reaches 4 for that graph we move graph pointer to next graph. And if it reaches 1 
      j++;  //acts as count to skip data element for times graph.
      if(IS_TIMES_GRAPH(local_graph_data_ptr->data_type) || (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_2B_100_COUNT_4B) ||
           IS_TIMES_STD_GRAPH(local_graph_data_ptr->data_type))
      {
        if(j == 4)
        {
          local_graph_data_ptr++;
          j = 0;
        }
      }
      else
      {
        local_graph_data_ptr++;
        j = 0;
      }
    }
 
    if (i > max)
    {
      if(!(cus_mon_ptr->no_log_OR_vector_format & NO_LOGGING))
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
                                    "Got more than expected (%d) data elements. Received data %s", max, buffer);
   
        if(cus_mon_ptr->no_log_OR_vector_format & LOG_ONCE)
          cus_mon_ptr->no_log_OR_vector_format |= NO_LOGGING;
      }
  
      update_health_monitor_sample_data(&hm_data->data_more_than_expected);
      break;
    }
    
    //data[i - 1] = atof(ptr);

    /*We are using strtod insted of atof because it covers more cases rather than atof fun ex:
      It converts hexadecimal number, scientific notational number like 232e+3, 34E+3, 44e-2, 45E-4 into decimal notation.
    */
    data[i - 1] = strtod(ptr, &end_ptr);

    //if end_ptr is equal to ptr, it mean data is invalid
    if(ptr == end_ptr)
    {
      //we assuming - is a valid data 
      if(ptr[0] != '-')
      {
        MLTL1(EL_D, DM_METHOD, MM_MON, _FLN_, cus_mon_ptr,
                              EID_DATAMON_INV_DATA, EVENT_CRITICAL,
                              "Received invalid data from custom monitor: %s", buffer);
      
       return -1;
      }
    }

    //if data starts from sign '-' then we have to fill zero. Ex: negative number -123 and no number i.e. -
    if(ptr[0] == '-')
    {
      if(strncmp(ptr, "-nan", 4))//if data is -nan then don't fill zero
        data[i - 1] = 0;
    }
    ptr = strtok_r(buffer," ",&buffer);
  }

  if (i < max)
  {
    //this is case for old agent in cm_nd_bt.gdf data is
   // --------------------------------------------------------------------------
              // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
              // avg min max count
              // 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
              // r t       s r t         t            t          t           pct 
    //this is case for new agent in cm_nd_bt.gdf data is
   // --------------------------------------------------------------------------
              // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
              // avg min max count
              // 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
              // t       t       s r t         t            t          t           pct 

    //this change is temperory this code is removed after new agent is installed in all machine.
    //TODO:remove
    //we are shifting values ahead in case of old agent because in old agent case data we will receive less.
    //So we make data itself avg(0) min(1) max(2) value will be same as old value(0) and count(3) will be the next graph count(7) value
    if((i == (max -3)) && cus_mon_ptr->flags & ND_BT_GDF)
    {
      memmove(data + 3, data, 8*(cus_mon_ptr->no_of_element - 3));
      data[0] = data[3];
      data[1] = data[3];
      data[2] = data[3];
      data[3] = data[7]; 
    }
    else if((i == (max -3)) && cus_mon_ptr->flags & ND_BACKEND_CALL_STATS_GDF)
    {
      memmove(data + 4, data + 1, 8*(cus_mon_ptr->no_of_element - 4));
      data[1] = data[4];
      data[2] = data[4];
      data[3] = data[4];
      data[4] = data[8];
    } 
    else
    {
      if(!(cus_mon_ptr->no_log_OR_vector_format & NO_LOGGING))
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
                                "Got (%d) less than expected (%d) data elements. Received data %s", i, max, buffer);

        if(cus_mon_ptr->no_log_OR_vector_format & LOG_ONCE)
          cus_mon_ptr->no_log_OR_vector_format |= NO_LOGGING;

        update_health_monitor_sample_data(&hm_data->data_less_than_expected);
      }
      return -1;
    }
  }
  // long cur_time;
 // cur_time = time(NULL);
  if(!(g_tsdb_configuration_flag & RTG_MODE))
    ns_tsdb_insert(cus_mon_ptr, data, metrics_idx, metric_id_found, 0, vector_name); //Need to discuss
  return 0;
}

void free_vec_lst_row(CM_vector_info * vector_info)
{
  CM_info *cm_info = vector_info->cm_info_mon_conn_ptr;
  
  if(!(cm_info->flags & ND_MONITOR) && (!strstr(cm_info->gdf_name,"cm_nd_integration_point_status.gdf")) && (!strstr(cm_info->gdf_name,"cm_nd_entry_point_stats.gdf")) && (!strstr(cm_info->gdf_name,"cm_nd_db_query_stats.gdf")))
    FREE_AND_MAKE_NULL(vector_info->data, "vector_info->data", -1);
 
  FREE_AND_MAKE_NOT_NULL(vector_info->vector_name, "vector_info->vector_name", -1);
  FREE_AND_MAKE_NOT_NULL(vector_info->mon_breadcrumb, "vector_info->mon_breadcrumb", -1);
  if(vector_info->kube_info != NULL)
    FREE_AND_MAKE_NOT_NULL(vector_info->kube_info->app_name, "vector_info->kube_info->app_name", -1);
  vector_info->flags &= 0;
  
  if(!(g_tsdb_configuration_flag & RTG_MODE))
  {
     ns_tsdb_free_metric_id_buffer(vector_info->metrics_idx);
  }
  return;
}

FILE *check_and_open_csv_fp(FILE *fp, char *file_path)
{
  if(! fp)
  {
    if((fp = fopen(file_path, "a+")) == NULL)
    {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
               "Could not open file %s ERROR: %s.", file_path, nslb_strerror(errno));
      return NULL;
    }
  }

  return fp;
}


void reload_normid_from_metadata_csv(NormObjKey *sql_id_key, char *csv_file_path, char *csv_file_name, int name_col, int norm_id_col)
{
  int planhandle_newflag = 0;
  /* Normalized Table Recovery Map (ntrmap)*/
  struct 
  {
   NormObjKey *key; 
   char *csvname;
   int name_fieldnum;
   int len_fieldnum;
   int normid_fieldnum;
  } ntrmap[] =

  { 
    //metadata csv format: NormSQLID,SQLID,query
    //needed sqlid, len, normsql id
    //name_col -> the column in csv for normalised entry
    //norm_id_col -> the column in csv where norm_id is stored
    {sql_id_key, csv_file_name, name_col, -1, norm_id_col},
  };
  
   MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method called. csv_file_path = %s", csv_file_path); 
 int num_obj_types = sizeof(ntrmap) / sizeof(ntrmap[0]); // Num of object types
  int obj_iter;
  for(obj_iter = 0; obj_iter < num_obj_types; obj_iter++)
  {
       /* Open csv file */
       char filename[1024];
       snprintf(filename, 1024, "%s", csv_file_path);
 
       FILE *fp = fopen(filename, "r");
       if(!fp)
       {
         MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Could not open file %s,"
                      " for reading while creating in memory normalised table\n", filename);
         continue;
       }

       /* Find the max number of fields to be read */ 
       int max = 0;
       if(ntrmap[obj_iter].name_fieldnum > max) max = ntrmap[obj_iter].name_fieldnum;
       if(ntrmap[obj_iter].len_fieldnum > max) max = ntrmap[obj_iter].len_fieldnum;
       if(ntrmap[obj_iter].normid_fieldnum > max) max = ntrmap[obj_iter].normid_fieldnum;
       max++; //add 1 to max as max is a number and field num is index
     
       if(norm_id_col == -1) //MSSQLPlanHandle.csv Needs to tokenize on four fields
	 max = 4;

       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "max = %d", max);

       /* Read line by line */
       char line[64*1024];
       while(fgets(line, 64*1024, fp))
       {
         /* Split the fields of line */
         char *fields[max];
         int num_fields;
         num_fields = get_tokens_with_multi_delimiter(line, fields, ",", max);  
         if(num_fields < max)
         {
           MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in reading file %s,"
                           "while creating in memory normalised table. "
                           "Number of fields are less than expected in the read line '%s'\n", filename, line);
           continue; // Skip this line
         }

         /* Now set the normalized ID */
         /*unsigned int nslb_set_norm_id(NormObjKey *key, char *in_str, int in_strlen, unsigned int normid)*/

         char *newline_ptr = NULL;
         newline_ptr = strstr(fields[(int)(ntrmap[obj_iter].name_fieldnum)], "\n");
         if(newline_ptr != NULL)
           *newline_ptr = '\0'; 

         if(ntrmap[obj_iter].len_fieldnum == -1) //Since len field is not there, use strlen 
         {
           //if norm_id_col is -1, this means it is for MSSQLPlanHandle.csv and we need to generate norm_id because this csv does not contain any norm_id field in it. 
           if(norm_id_col < 0)
           {
	     char plan_buffer[2048];
             int planhandle_newflag = 0;
	     sprintf(plan_buffer, "%s,%s,%s,%s", fields[0], fields[1], fields[2], fields[3]); 
             nslb_get_or_gen_norm_id(ntrmap[obj_iter].key, plan_buffer, strlen(plan_buffer), &planhandle_newflag);
           }
           else
           {
             nslb_set_norm_id(ntrmap[obj_iter].key, 
                              fields[(int)(ntrmap[obj_iter].name_fieldnum)], 
                              strlen(fields[(int)(ntrmap[obj_iter].name_fieldnum)]), 
                              atoi(fields[(int)(ntrmap[obj_iter].normid_fieldnum)]));  
           }
         }
         else
         {
           //if norm_id_col is -1, this means it is for MSSQLPlanHandle.csv and we need to generate norm_id because this csv does not contain any norm_id field in it. 
           if(norm_id_col < 0)
           {
              //ntrmap[obj_iter].normid_fieldnum = nslb_get_or_gen_norm_id(plan_handle_sql_id_key, fields[(int)(ntrmap[obj_iter].name_fieldnum)], strlen(fields[(int)(ntrmap[obj_iter].name_fieldnum)]), &planhandle_newflag);
              nslb_get_or_gen_norm_id(ntrmap[obj_iter].key, fields[(int)(ntrmap[obj_iter].name_fieldnum)], strlen(fields[(int)(ntrmap[obj_iter].name_fieldnum)]), &planhandle_newflag);
              /*nslb_set_norm_id(ntrmap[obj_iter].key, 
                               fields[(int)(ntrmap[obj_iter].name_fieldnum)], 
                               strlen(fields[(int)(ntrmap[obj_iter].name_fieldnum)]), 
                               atoi(fields[(int)(ntrmap[obj_iter].normid_fieldnum)]));  */
           }
           nslb_set_norm_id(ntrmap[obj_iter].key, 
                           fields[(int)(ntrmap[obj_iter].name_fieldnum)], 
                           atoi(fields[(int)(ntrmap[obj_iter].len_fieldnum)]), 
                           atoi(fields[(int)(ntrmap[obj_iter].normid_fieldnum)])); 
         }
       }
         CLOSE_FP(fp); 
  }
}

int check_for_csv_file(CM_info* cus_mon_ptr)
{
  char csv_filepath[1024];
/*
  if(cus_mon_ptr->csv_stats_ptr->metadata_csv_file)
  {
    cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp = check_and_open_csv_fp(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp, cus_mon_ptr->csv_stats_ptr->metadata_csv_file);
    if(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp == NULL)
      return -1;
  }
  if(cus_mon_ptr->csv_stats_ptr->planhandle_csv_file)
  {
    cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp = check_and_open_csv_fp(cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp, cus_mon_ptr->csv_stats_ptr->planhandle_csv_file);
    if(cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp == NULL)
      return -1;
  }
*/
  if(strchr(cus_mon_ptr->csv_stats_ptr->csv_file, '/')  == NULL)
    sprintf(csv_filepath, "%s/logs/%s/reports/csv/%s", g_ns_wdir, global_settings->tr_or_partition, cus_mon_ptr->csv_stats_ptr->csv_file);
  else
    sprintf(csv_filepath, "%s", cus_mon_ptr->csv_stats_ptr->csv_file);

  cus_mon_ptr->csv_stats_ptr->csv_file_fp = check_and_open_csv_fp(cus_mon_ptr->csv_stats_ptr->csv_file_fp, csv_filepath);
  if(cus_mon_ptr->csv_stats_ptr->csv_file_fp == NULL)
    return -1;

  return 0;
}

/*static void close_fp(FILE *fp)
{
    CLOSE_FP(fp);
}*/

void get_breadcrumb(char *cs_ip, int cs_port, char *mon_breadcrumb)
{
  strcpy(mon_breadcrumb, "T1>S1");
}

void create_breadcrumb_path(int vector_list_row, int runtime_flag, char *err_msg, CM_vector_info *vector_table_ptr, CM_info *parent_ptr)
{
  NSDL3_MON(NULL, NULL, "Method Called.");

  char mon_breadcrumb_path[MAX_CM_MSG_SIZE] = {0};
  char vector[MAX_NAME_SIZE + 1] = {0};
  char instance[MAX_NAME_SIZE + 1] = {0};
  char server[MAX_NAME_SIZE + 1] = {0};
  char display_server_name[512 + 1] = {0};
  char tiername[512 + 1] = {0};
  char temp_tiername[MAX_NAME_SIZE + 1] = {0};
  char *ptr;
  int total_flds = 0;
  int received_server_len = 0;
  char *field[50];
  char hv_separator[2] = {0};
  char make_breadcrumb_type;
  hv_separator[0] = global_settings->hierarchical_view_vector_separator;

  //memset(field, 0, 50);
  
  if(parent_ptr)
    make_breadcrumb_type = parent_ptr->tier_server_mapping_type;

  if(strstr(parent_ptr->pgm_args, "cm_weblogic_monitor") || strstr(parent_ptr->pgm_path, "cm_weblogic_monitor"))  
  {
    strncpy(vector, vector_table_ptr[vector_list_row].vector_name, MAX_NAME_SIZE);

    //NEW FORMAT: SERVER>INSTANCE , OLD FORMAT: INSTANCE
    if(strstr(parent_ptr->gdf_name, "cm_weblogic_jvm_stats.gdf") || strstr(parent_ptr->gdf_name, "cm_weblogic8_thread_pool_stats.gdf") || strstr(parent_ptr->gdf_name, "cm_weblogic_thread_pool_stats.gdf") || strstr(parent_ptr->gdf_name, "cm_weblogic_thread_pool_stats_v2.gdf"))
    {
      total_flds = get_tokens(vector, field, hv_separator, 50);
      
      if(total_flds == 1) //old format
        strncpy(instance, field[0], MAX_NAME_SIZE);
      else if(total_flds > 1) //new format    
      {
        strncpy(instance, field[1], MAX_NAME_SIZE);
        strncpy(server, field[0], MAX_NAME_SIZE);
        received_server_len = strlen(server) + 1;
      }
    }
    //NEW FORMAT: SERVER>INSTANCE>... , OLD FORMAT: INSTANCE>...
    else if(strstr(parent_ptr->gdf_name, "cm_weblogic8_jdbc_stats.gdf") || strstr(parent_ptr->gdf_name, "cm_weblogic_jdbc_stats.gdf") || strstr(parent_ptr->gdf_name, "cm_weblogic_session_stats.gdf"))
    {
      total_flds = get_tokens(vector, field, hv_separator, 50);
      
      if(total_flds == 2) //old format
        strncpy(instance, field[0], MAX_NAME_SIZE);
      else if(total_flds > 2) //new format    
      {
        strncpy(instance, field[1], MAX_NAME_SIZE);
        strncpy(server, field[0], MAX_NAME_SIZE);
        received_server_len = strlen(server) + 1;
      }
    }
    else
    {
      strncpy(instance, vector, MAX_NAME_SIZE);
      ptr = strchr(instance, global_settings->hierarchical_view_vector_separator); 
      if(ptr)
        *ptr = '\0';
    }

    //By Default make_breadcrumb_type will be as STANDARD_TYPE. But for weblogic monitor we will be setting by default as CUSTOM_TYPE because if someone apllied weblogic monitor via mprof, then it will not not look into topology for breadcrumb, it will directly create breadcrumb with the vectors passed along with mprof. So we need to set CUSTOM_TYPE as default for WEBLOGIC, so it will work as it was earlier by looking into topology tables for breadcrumb.
    if(make_breadcrumb_type & STANDARD_TYPE)
    {
      if(topolib_get_tier_and_server_disp_name_from_instance(instance, tiername, display_server_name, err_msg, server, topo_idx) == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s", err_msg);
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Creating custom breadcrumb for vector [%s].", vector_table_ptr[vector_list_row].vector_name);
        if(parent_ptr->tier_name)
        {
          strcpy(tiername, parent_ptr->tier_name);
          strcpy(display_server_name, parent_ptr->server_name);
        }
      }
    }
    else if(make_breadcrumb_type & CUSTOM_TYPE)
    {
      if(parent_ptr->tier_name)
      {
        strcpy(tiername, parent_ptr->tier_name);
        strcpy(display_server_name, parent_ptr->server_name);
      }
    }

    REPLACE_INSTANCE_CAME_WITH_INSTANCE_PROVIDED_IN_JSON(vector_table_ptr[vector_list_row].vector_name + received_server_len);

    if(enable_store_config)
    {
      if(!strcmp(tiername, "Cavisson"))
      {
        sprintf(temp_tiername, "%s!%s", g_store_machine, tiername);
        strcpy(tiername, temp_tiername);
      }
    }

    snprintf(mon_breadcrumb_path, MAX_CM_MSG_SIZE, "%s%c%s%c%s", tiername, global_settings->hierarchical_view_vector_separator,
                                                 display_server_name, global_settings->hierarchical_view_vector_separator,
                                                 instance);
  }
  else if(strstr(parent_ptr->pgm_path, "cm_oracle_stats"))
  {
    sprintf(tiername, "%s", parent_ptr->tier_name);
    sprintf(display_server_name, "%s", parent_ptr->server_name);
 
    REPLACE_INSTANCE_CAME_WITH_INSTANCE_PROVIDED_IN_JSON(vector_table_ptr[vector_list_row].vector_name + received_server_len);

    if(enable_store_config)
    {
      if(!strcmp(tiername, "Cavisson"))
      {
        sprintf(temp_tiername, "%s!%s", g_store_machine, tiername);
        strcpy(tiername, temp_tiername);
      }
    }

    snprintf(mon_breadcrumb_path, MAX_CM_MSG_SIZE, "%s%c%s%c%s", tiername, global_settings->hierarchical_view_vector_separator,
                                                 display_server_name, global_settings->hierarchical_view_vector_separator,
                                                 instance);

  }
  else if(((parent_ptr->flags & ND_MONITOR) || (strstr(parent_ptr->gdf_name, "cm_nd_db_query_stats.gdf")) || (strstr(parent_ptr->gdf_name, "cm_nd_entry_point_stats.gdf")) || (strstr(parent_ptr->gdf_name, "cm_nd_integration_point_status.gdf"))) || (strstr(parent_ptr->pgm_path, "cm_rum_stats") != NULL) || (parent_ptr->skip_breadcrumb_creation == 1))
  { //doing strstr instead of strncmp becoz of gdf name absolute path '/home/cavisson/work/sys/cm_nd_http_header_stats.gdf'
    strcpy(mon_breadcrumb_path, vector_table_ptr[vector_list_row].vector_name);
  }

  else if(parent_ptr->cs_port == -1)  //Only in case of NetCloud monitor
  {
    sprintf(mon_breadcrumb_path, "Controller%c%s", global_settings->hierarchical_view_vector_separator, vector_table_ptr[vector_list_row].vector_name);
  }
  //Add namespace and pod name before vector name. Breadcrumb will be T>S>Namespace>Pod>vector
  else if((strstr(parent_ptr->gdf_name, "cm_redis_db_stats_ex.gdf")) || (strstr(parent_ptr->gdf_name,"cm_redis_replication_stats_ex.gdf")))
  {
    if(parent_ptr->namespace)
    {
      sprintf(instance, "%s%c%s%c%s", parent_ptr->namespace, global_settings->hierarchical_view_vector_separator, parent_ptr->pod_name, global_settings->hierarchical_view_vector_separator, vector_table_ptr[vector_list_row].vector_name);
    }
    else
    {
      sprintf(instance, "default%c%s%c%s", global_settings->hierarchical_view_vector_separator, parent_ptr->pod_name, global_settings->hierarchical_view_vector_separator, vector_table_ptr[vector_list_row].vector_name);
    }

    snprintf(mon_breadcrumb_path, MAX_CM_MSG_SIZE, "%s%c%s%c%s", parent_ptr->tier_name, global_settings->hierarchical_view_vector_separator,
                                                    parent_ptr->server_name, global_settings->hierarchical_view_vector_separator, instance);
 } 
  else
  {
    if ((strstr(parent_ptr->gdf_name, "cm_kubernetes_pod_network_stats.gdf") != NULL) || (strstr(parent_ptr->gdf_name, "cm_kubernetes_container_df_stats.gdf") != NULL))
    {
      //For kubernetes monitor we only need netstorm to add Tier name because server>instance comes from the monitor for these two monitor.
      if(topolib_get_tier_from_server(parent_ptr->cs_ip, parent_ptr->cs_port, tiername, err_msg,topo_idx) == -1)
      {
        if(!runtime_flag)
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s", err_msg);
      }

      REPLACE_INSTANCE_CAME_WITH_INSTANCE_PROVIDED_IN_JSON(vector_table_ptr[vector_list_row].vector_name);
      if(enable_store_config)
      {
        if(!strcmp(tiername, "Cavisson"))
        {
          sprintf(temp_tiername, "%s!%s", g_store_machine, tiername);
          strcpy(tiername, temp_tiername);
        }
      }

      snprintf(mon_breadcrumb_path, MAX_CM_MSG_SIZE, "%s%c%s", tiername, global_settings->hierarchical_view_vector_separator,
                                                 instance);
    }
    else
    {
      if(parent_ptr->tier_name)
      {
        strcpy(tiername, parent_ptr->tier_name);
        strcpy(display_server_name, parent_ptr->server_name);
      }
      else
      {
        if(topolib_get_tier_and_server_disp_name_from_server(parent_ptr->cs_ip, parent_ptr->cs_port, tiername, display_server_name, err_msg,topo_idx) == -1)
        {
          if(!runtime_flag)
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s", err_msg);
        }
      }

      REPLACE_INSTANCE_CAME_WITH_INSTANCE_PROVIDED_IN_JSON(vector_table_ptr[vector_list_row].vector_name);

      if(enable_store_config)
      {
        if(!strcmp(tiername, "Cavisson"))
        {
          sprintf(temp_tiername, "%s!%s", g_store_machine, tiername);
          strcpy(tiername, temp_tiername);
        }
      }

      snprintf(mon_breadcrumb_path, MAX_CM_MSG_SIZE, "%s%c%s%c%s", tiername, global_settings->hierarchical_view_vector_separator,
                                                 display_server_name, global_settings->hierarchical_view_vector_separator,
                                                 instance);
    }
  }

  //save received 'breadcrumb' into structure
  MY_MALLOC(vector_table_ptr[vector_list_row].mon_breadcrumb, strlen(mon_breadcrumb_path) + 1, "Custom Monitor BreadCrumb", -1);
  strcpy(vector_table_ptr[vector_list_row].mon_breadcrumb, mon_breadcrumb_path);

  //set flag
  vector_table_ptr[vector_list_row].flags |= MON_BREADCRUMB_SET;

  NSDL3_MON(NULL, NULL, "vector_table_ptr[vector_list_row].mon_breadcrumb = %s, vector_table_ptr[vector_list_row].is_mon_breadcrumb_set = %d", vector_table_ptr[vector_list_row].mon_breadcrumb, (vector_table_ptr[vector_list_row].flags & MON_BREADCRUMB_SET));
}


//TODO: in 4.2.0 log bug
//We will mark the vector index as -1
//We will write the group line after checking the total deleted vectors
void check_deleted_vector(CM_info *cus_mon_ptr, int idx)
{
  int i, j;

  for(i = 0; i < cus_mon_ptr->total_deleted_vectors; i++)
  {
    if(cus_mon_ptr->deleted_vector[i] == idx)
    {
      for(j = i; j < (cus_mon_ptr->total_deleted_vectors - 1); j++)
        cus_mon_ptr->deleted_vector[j] = cus_mon_ptr->deleted_vector[j+1];
      cus_mon_ptr->total_deleted_vectors --;
      return;
    }
  }
}

//Search reused entry in deleted list and remove its entry.
/*void check_and_remove_entry_from_deleted_list(CM_info *cus_mon_ptr, int index)
{
  int i;

  for(i = 0; i < cus_mon_ptr->total_deleted_vectors; i++)
  {
    if(cus_mon_ptr->deleted_vector[i] == -1)  //Skip already removed entries
      continue;
    else if(cus_mon_ptr->deleted_vector[i] == index)
    {
      cus_mon_ptr->deleted_vector[i] = -1;
      cus_mon_ptr->flags &= ~WAITING_DELETED_VECTOR;
      break;
    }
  }
}*/


void set_reused_vector_counters(CM_vector_info *vector_list, int i, CM_info *cm_info_mon_conn_ptr, char *cs_ip, int cs_port, char *pgm_path, char *pgm_args, int instant_flag)
{
 
  if(vector_list[i].flags & WAITING_DELETED_VECTOR) 
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Waiting Deleted Vector (%s) has been added.\n", vector_list[i].vector_name);
  else
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Deleted custom monitor whose vector is (%s) has been added successfully\n", vector_list[i].vector_name);

  //Adding node pod to list if the reused vector case comes for NA_KUBER
  if(cm_info_mon_conn_ptr->flags & NA_KUBER)
  {
    add_node_and_pod_in_list(vector_list[i].vector_name, &vector_list[i], cm_info_mon_conn_ptr, vector_list[i].vectorIdx, REUSE_POD);
  }

  if(cs_ip)
  {
    MY_REALLOC(cm_info_mon_conn_ptr->cs_ip, (strlen(cs_ip) + 1), "vector_list[i].cs_ip", -1 );
    strcpy(cm_info_mon_conn_ptr->cs_ip, cs_ip);
    cm_info_mon_conn_ptr->cs_port = cs_port;

    MY_REALLOC(cm_info_mon_conn_ptr->cavmon_ip, (strlen(cs_ip) + 1), "vector_list[i].cavmon_ip", -1 );
    strcpy(cm_info_mon_conn_ptr->cavmon_ip, cs_ip);
    cm_info_mon_conn_ptr->cavmon_port = cs_port;
  }

  if(pgm_path && pgm_path[0] != '\0')
  {
    REALLOC_AND_COPY(pgm_path, cm_info_mon_conn_ptr->pgm_path, strlen(pgm_path) + 1, "pgm path", -1)
  }
  //else
    //strcpy(cm_info_mon_conn_ptr->pgm_path, " ");

  if(pgm_args && pgm_args[0] != '\0')
  {
    REALLOC_AND_COPY(pgm_args, cm_info_mon_conn_ptr->pgm_args, strlen(pgm_args) + 1, "pgm args", -1)
  }
  //else
   //strcpy(cm_info_mon_conn_ptr->pgm_args, " ");
  
  cm_info_mon_conn_ptr->cm_retry_attempts = max_cm_retry_count;

  if(!(vector_list[i].flags & WAITING_DELETED_VECTOR))
  { 
    vector_list[i].flags &= ~RUNTIME_DELETED_VECTOR;
    vector_list[i].vector_state = CM_REUSED;
    monitor_runtime_changes_applied = 1;
  }
  else
    vector_list[i].flags &= ~WAITING_DELETED_VECTOR;

  vector_list[i].cm_info_mon_conn_ptr = cm_info_mon_conn_ptr;

  if(cm_info_mon_conn_ptr->conn_state == CM_DELETED)
    cm_info_mon_conn_ptr->conn_state = CM_REUSED;

  if(!(cm_info_mon_conn_ptr->flags & ALL_VECTOR_DELETED) && (instant_flag != MON_REUSED_INSTANTLY))
    check_deleted_vector(cm_info_mon_conn_ptr, i);

}

int verify_vector_format(CM_vector_info *cm_ptr, CM_info *parent_ptr)
{
  if(parent_ptr->no_log_OR_vector_format & FORMAT_NOT_DEFINED)      //vector format not set. Whatever may be the format of first vector, that will be set.
  {
    if(cm_ptr->vectorIdx >= 0)          //Id non-negative i.e. set new format vector
    {
      parent_ptr->no_log_OR_vector_format &= ~FORMAT_NOT_DEFINED;
      parent_ptr->no_log_OR_vector_format |= NEW_VECTOR_FORMAT;
    }
    else                       //Set old format vector
    {
      parent_ptr->no_log_OR_vector_format &= ~FORMAT_NOT_DEFINED;
      parent_ptr->no_log_OR_vector_format |= OLD_VECTOR_FORMAT;
    }
  }
  else if((parent_ptr->no_log_OR_vector_format & OLD_VECTOR_FORMAT) && cm_ptr->vectorIdx >= 0)  //first vector was old format and afterwards vector is new format, so ignoring
  {
    MLTL1(EL_DF, 0, 0, _FLN_, parent_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Ignoring this vector (%s) as first vector is an old format vector and now we are expecting only old format vectors. Old format is 'vector_name', new format is 'id:vector_name'", cm_ptr->vector_name);
    return -1;
  }
  else if((parent_ptr->no_log_OR_vector_format & NEW_VECTOR_FORMAT) && cm_ptr->vectorIdx == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, parent_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Ignoring this vector (%s) as first vector is a new format vector and now we are expecting only new format vectors. Old format is 'vector_name', new format is 'id:vector_name'", cm_ptr->vector_name);
    return -1;
  }

return 0;
}
/*
//This method will convert NA_mssql_abc_xyz.gdf to MSSQLAbcXyz.csv
void generate_csv_file_name(char *gdf_name, char *csv_name)
{
  char *ptr = NULL, *ptr1 = NULL;
  char gdf[1024];

  strcpy(gdf, gdf_name);
  strcpy(csv_name, "MSSQL");

  ptr = gdf + 9;  //(9 = length of "NA_mssql_") 
  while((ptr1 = strchr(ptr, '_')) != NULL)
  {
    *ptr1 = '\0';
    *ptr = *ptr - 32;
    sprintf(csv_name, "%s%s", csv_name, ptr);
    //strcat(csv_name, ptr);
    ptr = ptr1 + 1;
  }

  ptr1 = strrchr(ptr, '.');
  *ptr1 = '\0';
  *ptr = *ptr - 32;
  sprintf(csv_name + (strlen(csv_name)), "%s.csv", ptr);
}
*/
// This replaces the string into a new buffer (MAX buffer lenth must be < 10240)
// AA:AA ==> AA%3AAA
// Where buf =  AA:AA 
// from = :
// to = %3A
/*static void replace_strings(char *buf, char *new_buf, int max_len, char* from, char* to) {

  NSDL2_MON(NULL, NULL, "Method called, Replacing '%s' by '%s'", from, to); 

  new_buf[0] = '\0';
  char *p;
  char *q = buf;
  int from_len = strlen(from);
  int to_len = strlen(to);
  int total_len = 0;

  while((p = strstr(q, from)) != NULL) {
    total_len += (p - q) + to_len; 
    if(total_len > max_len) {
     NSDL2_MON(NULL, NULL, "Not Replacing '%s' by '%s' as too small buffer. Total = %d, Max = %d",
                            from, to, total_len, max_len); 
     strncpy(new_buf, buf, max_len);
     return; 
    }
    strncat(new_buf, q, p - q);
    strcat(new_buf, to);
    q = p + from_len;
  }
  strcat(new_buf, q);
}

//created this function just to set use_lps flag using its return value.
static int cm_set_use_lps(char *mon_name, char *mon_args)
{
  if(((strstr(mon_name, "cm_access_log_stats") != NULL) || (strstr(mon_args, "cm_access_log_stats") != NULL)) || 
    ((strstr(mon_name, "cm_file") != NULL) || (strstr(mon_args, "cm_file") != NULL)) ||
    ((strstr(mon_name, "cm_access_log_status_stats") != NULL) || (strstr(mon_args, "cm_access_log_status_stats") != NULL)))
    return 1;
  else
    return 0;  
}*/


void create_accesslog_id(char *origin_cmon_and_ip_port, char *pgm_args, char *vector_name, char *mon_name) 
{
  int i, value; 
  char *ptr1;
  char *ptr2;
  char *ptr3;
  char buffer[MAX_MONITOR_BUFFER_SIZE];
  char save[64] = "\0";
  ptr1 = strstr(pgm_args, "-N");
  ptr2 = strstr(pgm_args, "-n ");
  if(ptr1 && !ptr2)
  {
    ptr1 = ptr1+2;
    if(!ptr1)
      return;

    while(ptr1[0] == ' ')
    {
      ptr1++;
    }
    ptr3 = strstr(ptr1, " ");
    if(!ptr3)
      strcpy(save, ptr1);
    else
      strncpy(save, ptr1, (int)(ptr3-ptr1));
    if(ns_is_numeric(save))
    {
      value = atoi(save);
    }
    for(i=1; i< value; i++) 
    {
      char err_msg[MAX_DATA_LINE_LENGTH]={0};
      sprintf(buffer, "STANDARD_MONITOR %s %s%d %s %s -O %d", origin_cmon_and_ip_port, vector_name,i, mon_name, pgm_args, i);
      kw_set_standard_monitor("STANDARD_MONITOR", buffer, 0, 0,err_msg, NULL); 
    }
    strcat(pgm_args, "-O 0");
  }
}

//--------------------------- End Config part --------------------


//---------------------------- Processing GDF Part ----------------

//TM: get_next_cm_gdf_name (removed) and set_cm_gp_info_index (removed)
void set_nd_backend_mon_data_ptrs(int monitor_index, int group_num_monitors)
{
  int idx, instanceIdx, vec_idx;
  int vectorIdx, tierIdx;
  CM_info *cm_info_mon_conn_ptr = NULL;
  CM_vector_info *vector_list = NULL;

  for(idx = monitor_index; idx < (monitor_index + group_num_monitors); idx++)
  { 
    cm_info_mon_conn_ptr = monitor_list_ptr[idx].cm_info_mon_conn_ptr;
    vector_list = cm_info_mon_conn_ptr->vector_list;
    for(vec_idx = 0; vec_idx < cm_info_mon_conn_ptr->total_vectors; vec_idx++)
    {
      if(vector_list[vec_idx].flags & OVERALL_VECTOR)
        vectorIdx = vector_list[vec_idx].vectorIdx;
      else
        vectorIdx = vector_list[vec_idx].mappedVectorIdx;

      instanceIdx = vector_list[vec_idx].instanceIdx;
      tierIdx = vector_list[vec_idx].tierIdx;

      if((vectorIdx < 0) || (instanceIdx < 0) || (tierIdx < 0))
        continue;

      if(vector_list[vec_idx].flags & OVERALL_VECTOR)
      { 
        if((cm_info_mon_conn_ptr->tierNormVectorIdxTable[vectorIdx][tierIdx]) != NULL)
          vector_list[vec_idx].data = (cm_info_mon_conn_ptr->tierNormVectorIdxTable[vectorIdx][tierIdx])->aggrgate_data;
      }
      else
      { 
        if((cm_info_mon_conn_ptr->instanceVectorIdxTable[vectorIdx][instanceIdx]) != NULL) 
          vector_list[vec_idx].data = (cm_info_mon_conn_ptr->instanceVectorIdxTable[vectorIdx][instanceIdx])->data;
      }
    }
  }
}

//TODO: monitor_index can be removed from the arguments
void set_nv_mon_data_ptrs(int monitor_index, CM_info *cm_ptr)
{
  int idx;
  int vectorIdx; 
  int num_vectors = cm_ptr->total_vectors;

  CM_vector_info *vector_list = cm_ptr->vector_list;

  for(idx = 0; idx < num_vectors; idx++)
  {
    vectorIdx = vector_list[idx].vectorIdx;

    if(vectorIdx < 0)
    {    
      vector_list[idx].flags |= DATA_FILLED;
      continue;
    }

    if(cm_ptr->nv_map_data_tbl)
    {
      if((cm_ptr->nv_map_data_tbl)->data)
      {
        if((cm_ptr->nv_map_data_tbl)->data[vectorIdx])
          vector_list[idx].data = (cm_ptr->nv_map_data_tbl)->data[vectorIdx];
      }
    }
    
    // TODO : THIS NEED TO BE FIXED, SETTING THIS ALWAYS
    vector_list[idx].flags |= DATA_FILLED;
  }
}
 
int set_cm_info_values(int monitor_index, int num_group, int num_element, int group_num_vectors, int group_index)
{
  //int dbuf_len = 0;  // for size of buffer which will hold the msg from Create Server.
  int cnt = 0;
  int idx, i;
  int m = 0;
  int save_no_of_elements = 0;
  CM_info *cm_ptr = monitor_list_ptr[monitor_index].cm_info_mon_conn_ptr;
  CM_vector_info *vector_list = cm_ptr->vector_list;

  get_no_of_elements(cm_ptr, &save_no_of_elements);

  NSDL2_MON(NULL, NULL, "Method called, monitor_index = %d, num_group = %d, num_element = %d, group_num_vectors = %d, group_index = %d", monitor_index, num_group, num_element, group_num_vectors, group_index);

  for (idx = monitor_index; idx < (monitor_index + monitor_list_ptr[monitor_index].no_of_monitors); idx++)
  {
    cm_ptr = monitor_list_ptr[idx].cm_info_mon_conn_ptr;
    vector_list = cm_ptr->vector_list;

    cm_ptr->gp_info_index = group_index;
    cm_ptr->num_group = num_group;      // set num of group in Group Info Buffer.;

    if(cm_ptr->no_of_element <= 0)
      cm_ptr->no_of_element = num_element;// set num of element in Custom Monitor

    if(num_element == 0)
      cm_ptr->gp_info_index = -1;       //setting gp_info_index equal to -1 intentially, where gdf is NA because it has no graph enteries so we can't set any gp_info_idx for this monitor but it is taking value which is set previously.

    if(cm_ptr->no_of_element == 0)
    {
      get_no_of_elements(cm_ptr, (int *)&(cm_ptr->no_of_element));
    }

    NSDL2_MON(NULL, NULL, "idx = [%d], cm_ptr->gp_info_index = [%d], cm_ptr->num_group = [%d], cm_ptr->no_of_element  = [%d]", idx, cm_ptr->gp_info_index, cm_ptr->num_group, cm_ptr->no_of_element);
 
    //We set dbuf_len to a big size because there may come a big error message from custom monitor
    //This is move to function where we initialize cm info node members
    /*if(cm_ptr->data_buf == NULL)
    {
      if(!strcmp(cm_ptr->gdf_name,"NA_mssql_query_execution_stats.gdf"))
        dbuf_len = g_mssql_data_buf_size + 1;
      else
        dbuf_len = MAX_MONITOR_BUFFER_SIZE + 1;
      MY_MALLOC(cm_ptr->data_buf, dbuf_len, "Custom Monitor data", idx);
      memset(cm_ptr->data_buf, 0, dbuf_len);
    }*/

    for (i = 0; i < cm_ptr->total_vectors; i++)
    {
      if(vector_list[i].data == NULL)
      {
        NSDL2_MON(NULL, NULL, " FOUND NULL DOING MALLOC ");
        MY_MALLOC(vector_list[i].data, (cm_ptr->no_of_element * sizeof(double)), "Custom Monitor data", i);

        // set "nan" on monitor data connection break
        for(m = 0; m < cm_ptr->no_of_element; m++)
        {
          vector_list[i].data[m] = 0.0/0.0;
        }
      }
    
       if((!(g_tsdb_configuration_flag & RTG_MODE)) && (vector_list[i].metrics_idx == NULL))
      {
        // MY_MALLOC(vector_list[i].metrics_idx, (cm_ptr->no_of_element * sizeof(long)), "Custom Monitor data", i);
        ns_tsdb_malloc_metric_id_buffer(&vector_list[i].metrics_idx , cm_ptr->no_of_element);
      }
    
      NSDL2_MON(NULL, NULL, "vector_list[%d].data = [%p]", vector_list[i].data);

    }

    NSDL2_MON(NULL, NULL, "idx = [%d], cm_ptr->gp_info_index = [%d], cm_ptr->num_group = [%d], "
                          "cm_ptr->no_of_element  = [%d]", 
                           idx, cm_ptr->gp_info_index, cm_ptr->num_group, 
                           cm_ptr->no_of_element);
    cnt++;
  }

  NSDL2_MON(NULL, NULL, " cnd = [%d]", cnt);
  return cnt;
}
//---------------------------- End Processing GDF Part ----------------

//---------------------------- Filling Part ---------------------------
//This function will return index of dynamic monitor vector
static int find_dyn_vct_idx(CM_info *cus_mon_ptr, char *vct_name, int expected_idx)
{
  int i;
  int dyn_num_vectors = cus_mon_ptr->total_vectors;
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;

  NSDL2_MON(NULL, NULL, "Method Called, cus_mon_ptr = %p, vct_name = [%s], dyn_num_vectors = %d, expected_idx = %d", 
                         cus_mon_ptr, vct_name?vct_name:NULL, dyn_num_vectors, expected_idx);

  if(!vector_list)
  {
    NSDL3_MON(NULL, NULL, "Vector list is not allocated for monitor gdf %s, vector_name = %s. So returning", 
	     cus_mon_ptr->gdf_name, cus_mon_ptr->monitor_name);

    return -1;
  }

  //this is the case when old format vector is received and when expected idx (starts from 0) is more than the number of vectors received, we will return -1. so that it will add it as a new vector. 
  //This case was generated when any duplicate vector was found, and after that it is ignored and vector count is reduced. But if again in next sample duplicate vector comes then at the expected index that vector was found but that expected index was out of vector count limit.
  if(expected_idx >= (cus_mon_ptr->total_vectors))
    return -1;  

  //Firstly check vector name at their expected position or not???
  //If it is on their expexted position then then return from here
  if(strcmp(vct_name, vector_list[expected_idx].vector_name) == 0)
  {
    NSDL3_MON(NULL, NULL, "vector %s is found on their expected position %d", vct_name, expected_idx);
    return expected_idx;
  }

  //If vector not found on their expected position then we have to search it over whole array
  for(i = 0; i < dyn_num_vectors; i++)
  {
    NSDL3_MON(NULL, NULL, "i = %d, Comparing vector name vct_name = [%s] to cus_mon_ptr->vector_name = [%s]", 
                           i, vct_name?vct_name:NULL, vector_list[i].vector_name);
    if(strcmp(vct_name, vector_list[i].vector_name) == 0) 
      return i;
  } 

  NSDL3_MON(NULL, NULL, "vector name %s not found in Custom Mon array", vct_name);
  return -1; //Failure
}

//This function will return index of dynamic monitor vector by comparing breadcrumb 
static int find_dyn_vct_idx_using_breadcrumb(CM_info *cus_mon_ptr, char *breadcrumb, int expected_idx)
{
  int i;
  int dyn_num_vectors = cus_mon_ptr->total_vectors;
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;

  NSDL2_MON(NULL, NULL, "Method Called, cus_mon_ptr = %p, breadcrumb = [%s], dyn_num_vectors = %d, expected_idx = %d", 
                         cus_mon_ptr, breadcrumb?breadcrumb:NULL, dyn_num_vectors, expected_idx);

  //Firstly check breadcrumb at their expected position or not???
  //If it is on their expexted position then then return from here
  if(strcmp(breadcrumb, vector_list[expected_idx].mon_breadcrumb) == 0)
  {
    NSDL3_MON(NULL, NULL, "breadcrumb %s is found on their expected position %d", breadcrumb, expected_idx);
    return expected_idx;
  }

  //If breadcrumb not found on their expected position then we have to search it over whole array
  for(i = 0; i < dyn_num_vectors; i++)
  {
    NSDL3_MON(NULL, NULL, "i = %d, Comparing breadcrumb = [%s] to cus_mon_ptr->mon_breadcrumb = [%s]", 
                           i, breadcrumb?breadcrumb:NULL, vector_list[i].mon_breadcrumb);
    if(strcmp(breadcrumb, vector_list[i].mon_breadcrumb) == 0) 
      return i;
  } 

  MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              " breadcrumb %s not found in Custom Mon array", breadcrumb);
  return -1; //Failure
}

void get_no_of_elements(CM_info *cus_mon_ptr, int *num_data)
{
  FILE *read_gdf_fp;
  char *cm_gdf_name = cus_mon_ptr->gdf_name;  
  char line[MAX_LINE_LENGTH];
  char tmpbuff[MAX_LINE_LENGTH];
  char *buffer[GDF_MAX_FIELDS];
  char data_type;
  int i = 0, idx = 0;
  int size = 0;
  int graph_priority;

  NSDL2_MON(NULL, NULL, "Method called, cus_mon_ptr = %p, cm_gdf_name = %s", 
                         cus_mon_ptr, cm_gdf_name?cm_gdf_name:"NULL");

  //reset
  element_count = 0;
 
  for(i = 0; i <= MAX_METRIC_PRIORITY_LEVELS; i++)
    cus_mon_ptr->group_element_size[i] = 0;

  memset(cus_mon_ptr->save_times_graph_idx, -1, sizeof(int)*128);

  /* Open GDF file if file name provided */
  if(strncmp(cus_mon_ptr->gdf_name,"NA", 2))
    read_gdf_fp = open_gdf(cm_gdf_name);
  else
    return;
    
  while (fgets(line, MAX_LINE_LENGTH, read_gdf_fp) !=NULL)
  {     
    line[strlen(line) - 1] = '\0';

    if((sscanf(line, "%s", tmpbuff) == -1) || line[0] == '#' || line[0] == '\0' || 
    //if(line[0] == '#' || line[0] == '\0' || 
       !(strncasecmp(line, "info|", strlen("info|"))) || !(strncasecmp(line, "group|", strlen("group|"))))
      continue;
    else if(!(strncasecmp(line, "graph|", strlen("graph|"))))
    {
      strcpy(tmpbuff, line);
      i = get_tokens(line, buffer, "|", GDF_MAX_FIELDS);

      if(i != 14)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, 
                  "Error: No. of field in Graph line is not 14. Line = %s\n", tmpbuff);
        continue;
      }
    
      data_type = data_type_to_num(buffer[4]);

      //saving index of min graph of times and timesStd 
      if(IS_TIMES_GRAPH(data_type) || IS_TIMES_STD_GRAPH(data_type))
      {
        if(idx > 127)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, 
                    "Total expected no. of times and timesStd graphs is 128. Exceeding this limit.\n");
        }
        else
        {
          cus_mon_ptr->save_times_graph_idx[idx] = element_count + 1;  
          idx++;
        }
      } 
 
      size = getSizeOfGraph(data_type, &element_count);

      /* Calculate total group size - buffer[11] => graph priority, buffer[8] => graph is exculded or not*/
      if((graph_priority = is_graph_excluded(buffer[11], buffer[8], cus_mon_ptr->metric_priority)) != GRAPH_EXCLUDED)
        cus_mon_ptr->group_element_size[graph_priority] += size; 
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Error: Invalid line = %s\n", tmpbuff);
      continue;
    }
  }
  close_gdf(read_gdf_fp);

  *num_data = element_count;
  
  //If HML feature is enable then total element size 
  if(cus_mon_ptr->metric_priority != MAX_METRIC_PRIORITY_LEVELS)
    for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++)
      cus_mon_ptr->group_element_size[MAX_METRIC_PRIORITY_LEVELS] += cus_mon_ptr->group_element_size[i];
}

void get_breadcrumb_level(CM_info *cus_mon_ptr)
{
  FILE *read_gdf_fp;
  char line[MAX_LINE_LENGTH];
  char *fields[10];
  char breadcrumb[VECTOR_NAME_MAX_LEN + 1] = {0};
  int num_fields = -1;
  

  read_gdf_fp = open_gdf(cus_mon_ptr->gdf_name);
  while (fgets(line, MAX_LINE_LENGTH, read_gdf_fp) !=NULL)
  {
    line[strlen(line) - 1] = '\0';
    if(line[0] == '#' || line[0] == '\0')
      continue;
    if(!(strncasecmp(line, "group|", strlen("group|"))))
    {
      get_tokens_with_multi_delimiter(line, fields, "|", 10);
      strcpy(breadcrumb,fields[7]);
      num_fields = get_tokens_with_multi_delimiter(breadcrumb, fields, ">", 10);
      cus_mon_ptr->breadcrumb_level = num_fields;
      CLOSE_FP(read_gdf_fp);
      break;
    }
  }
}

//generate breadcrumb, search for this breadcrumb in dyn_num_vectors, if found get and set mapped index & data ptr, if not found do add new vector
int search_and_set_values(char *buffer, CM_info *cus_mon_ptr)
{
  int k = 0;
  char vector[VECTOR_NAME_MAX_LEN] = {0};
  int vectorid = -1;
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;

  if(parse_nv_vector(buffer, vector, &vectorid) == -2) 
    return -1; //vector format not correct expecting new format 'id:vector'

  for (k = 0; k < cus_mon_ptr->total_vectors; k++)
  {
    if(!strcmp(vector_list[k].vector_name, vector)) //matched
    {
      vector_list[k].vectorIdx = nv_get_id_value(cus_mon_ptr->vectorIdxmap, vectorid, NULL);
      vector_list[k].data = (cus_mon_ptr->nv_map_data_tbl)->data[vector_list[k].vectorIdx];
      vector_list[k].flags |= DATA_FILLED;  //changed
      return 1;
    }
  }

  return 0;
}

/*************** MIGRATION SECTION STARTS *************************/
/*
  check if numeric

  if(numeric)
  {
    check in mapping table & update data 
    if (new)
    {
      check if old vector comes in new format by looping dyn_num_vectors
      if(old vector comes in new format)
      {
        mark migration flag to new format in cm table  
        set data ptrs to point now mapping table data 
        set id's in table

        add overall in table , if already present then duplicate msg will come else it will get added
      }
      else means completely new vector
      {
        add new vector
        mark migration flag to new
        data ptrs will get set automatically during gdf processing
      }
    }
    else 
    {
      update data only
    } 
  }
  else
  {
    update data         
    mark migration flag to new
    clear mapping table means set flag to 1 only
  }
*/

int mon_migration_for_alert(CM_info * cm_ptr, int id, char * vector_name)
{ 
  int i;
  CM_vector_info *vector_list = cm_ptr->vector_list;

  for(i=0; i<cm_ptr->total_vectors; i++)
  {
    if((!strcmp(vector_name, vector_list[i].vector_name)))
    {
      //if(cm_ptr->vectorIdx != -1)
       // return i;

      vector_list[i].vectorIdx = id;
      MLTL1(EL_DF, 0, 0, _FLN_, cm_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       " Migration: Received old format vector %s in new format", vector_name);

      cm_ptr->no_log_OR_vector_format &= ~OLD_VECTOR_FORMAT;
      cm_ptr->no_log_OR_vector_format |= NEW_VECTOR_FORMAT;

      if(cm_ptr->dvm_cm_mapping_tbl_row_idx < 0)
      {
        cm_ptr->dvm_cm_mapping_tbl_row_idx = total_mapping_tbl_row_entries;
        total_mapping_tbl_row_entries++;
        total_dummy_dvm_mapping_tbl_row_entries --;
      }

      //we need to allocate dvm mapping table for this vector also.
      if(id >= cm_ptr->max_mapping_tbl_vector_entries)
      {
        cm_ptr->max_mapping_tbl_vector_entries = create_mapping_table_col_entries(cm_ptr->dvm_cm_mapping_tbl_row_idx, id, cm_ptr->max_mapping_tbl_vector_entries);
      }

      dvm_idx_mapping_ptr[cm_ptr->dvm_cm_mapping_tbl_row_idx][id].relative_dyn_idx = i;

      return i;
    }
  }
  return -1;
}

/*************** MIGRATION SECTION ENDS *************************/

int parse_netcloud_data_buf(char *buffer, int *vector_id, int *gen_id, char *vector_name)
{
/* Data Format

  First sample : 
    GeneratorID:VectorID:GeneratorName>IP|Data\n
  Rest sample : 
    GeneratorID:VectorID|Data\n */

  int num_field;
  char *field[10];

  num_field = get_tokens_with_multi_delimiter(buffer, field, ":|", 4);

  if(num_field == 4)      //First data sample
  {
    *gen_id = atoi(field[0]);
    *vector_id = atoi(field[1]);
    strcpy(vector_name, field[2]);
    return 1;
  }
  else if(num_field == 3)  //Data after first sample
  {
    *gen_id = atoi(field[0]);
    *vector_id = atoi(field[1]);
    return 2;
  }
  else
  {
    NSDL3_MON(NULL, NULL, "Data line for NetCloud IP Data monitor is not coming as expected");
    return -1;
  }
}

long long read_cav_epoch_diff_in_secs()
{
  FILE *fp = NULL;
  char tmpstr[1024] = "";
  long long cav_epoch_diff_in_secs = 0;
 
  sprintf(tmpstr, "cat %s/logs/TR%d/.cav_epoch.diff 2>/dev/null", g_ns_wdir, testidx);
  if((fp = popen(tmpstr, "r")) != NULL)
  {
    if((fgets(tmpstr, 1023, fp)) != NULL) // Used the same string
    {
      tmpstr[1023] = '\0';
      cav_epoch_diff_in_secs = atoll(tmpstr);
    } 
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: could not open '%s', could not get cav_epoch_diff", tmpstr);
  }

  if(fp)
    pclose(fp);
  
  return cav_epoch_diff_in_secs;
}


long long caluculate_timestamp_from_date_string(char * buffer)
{
  char tmp_buf[1024];
  struct tm tm;
  long long seconds_from_cav_epoch;
  long long cav_epoch_diff_in_secs;

  memset(&tm, 0, sizeof(struct tm));
  cav_epoch_diff_in_secs = global_settings->unix_cav_epoch_diff;
  //if -123456789 is received in date
  if(strstr(buffer,"-123456789"))
  {
   strcpy(tmp_buf, buffer); 
  }
  else
  {
    //Format of Date  : 2017-04-08 16:44:13
    //2017-04-08 16:44:13
    tm.tm_isdst = -1;   //auto find daylight saving
    strptime(buffer, "%Y-%m-%d %H:%M:%S", &tm);
    strftime(tmp_buf, 1024, "%s", &tm);
  }
  seconds_from_cav_epoch = (atoll(tmp_buf) - cav_epoch_diff_in_secs);
  return seconds_from_cav_epoch;
}

/*
Format of data received from CMON will be like: 

sql handle (varbinary), total execution count (bigint), total worker time (bigint),total elapsed time(bigint),total physical reads(bigint),total logical reads(bigint),total logical writes(bigint),total clr time (bigint),avg worker time(bigint),avg physical reads(bigint),avg logical writes(bigint),avg logical reads(bigint),avg clr time(bigint),avg elapsed time(bigint),total wait time(bigint),last wait time(bigint),min wait time(bigint),max wait time(bigint),avg wait time(bigint),plan handle (varbinary),creation time (double),last execution time (date),db name(string),serverName (string),start Offset(int),end offset (int),query, query Plan

The queryPlan sent by cmon is very long. It can be more than 64K. That is the only reason we wish to keep that column at last, so that netstorm do not need to parse that field.

Format of data that will be dumped in csv will be like:

norm_sql_id, total execution count (bigint), total worker time (bigint),total elapsed time(bigint),total physical reads(bigint),total logical reads(bigint),total logical writes(bigint),total clr time (bigint),avg worker time(bigint),avg physical reads(bigint),avg logical writes(bigint),avg logical reads(bigint),avg clr time(bigint),avg elapsed time(bigint),total wait time(bigint),last wait time(bigint),min wait time(bigint),max wait time(bigint),avg wait time(bigint),plan handle (varbinary),creation time (double),last execution time (date),db name(string),serverName (string),start Offset(int),end offset (int),  query Plan (text)

*/
/*
static int fill_data_in_csv_for_query_execution_stats(char *buffer, CM_info *cus_mon_ptr)
{
  char *ptr, *sql_query = NULL, *sql_handle = NULL, *server_name = NULL, *plan_handle = NULL, *start_offset = NULL, *end_offset = NULL;
  int norm_sql_id = 0;
  int newflag = 0;
  int i = 0;
  long long time_stamp_in_sec;
  char *pointer;
  char hyphen_buf[2] = "-";
  int planhandle_newflag = 0;
  char planhandle_buffer[2048];

  if((cus_mon_ptr->csv_stats_ptr->csv_file_fp == NULL) || (cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp == NULL) || (cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp == NULL))
  {
    if(check_for_csv_file(cus_mon_ptr) < 0)
      return -1;
  }

  ptr = strstr(buffer, ",");
  if(!ptr)
    return 0;

  *ptr = '\0';

  //We are taking sql handle and taking its normalisation to find an id. 
  norm_sql_id = nslb_get_or_gen_norm_id(query_execution_sql_id_key, buffer, strlen(buffer), &newflag);
  *ptr = ',';

  MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              " norm_sql_id = %d, newflag = %d",norm_sql_id, newflag);

  if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d,", norm_sql_id) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file MSSQLReport.csv. Data line: %s ERROR: %s", buffer, strerror(errno));
    close_fp(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
    return -1;
  }

  while((ptr = strstr(buffer, ",")) != NULL)
  {
    *ptr = '\0';
    i++;

    //We donot need to write sql handle in MSSQLReport.csv. We will be entering sql handle in that file instead. That will be used for mapping sql query and sql handle.
    if(i == 1)
    {
      sql_handle = buffer;
      buffer = ptr + 1;
      continue;
    }

   //This field contain sql_query and it wont be entered in MSSQLReport.csv 
    else if(i == 27)
    {
      sql_query = buffer;
      buffer = ptr + 1;
      break;
    }
    else if(i == 20)
    {
      plan_handle = buffer;
    } 
    else if(i == 25)
    {
      start_offset = buffer;
    }
    else if(i == 26)
    {
      end_offset = buffer;
    }
    //This field contain last execution point which is saved as a string. We will be converting that date format into epoch secs.
    else if(i == 22)
    {
      pointer = strrchr(buffer, '.');
       if(pointer)
        *pointer = '\0';

      time_stamp_in_sec = caluculate_timestamp_from_date_string(buffer);

      if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%lld,", time_stamp_in_sec) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file MSSQLReport.csv. Data line: %s ERROR: %s", buffer, strerror(errno));
        close_fp(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
        return -1;
      }
      buffer = ptr + 1;
      continue;
    }
    else if(i == 24)
    {
      server_name = buffer;
    }

    if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%s,", buffer) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file MSSQLReport.csv. Data line: %s ERROR: %s", buffer, strerror(errno));
      close_fp(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
      return -1;
    }

    buffer = ptr + 1;

  }
  //query_plan = buffer;
  if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d\n", serial_no) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in writing data in file MSSQLReport.csv. Data line: %s ERROR: %s", buffer, strerror(errno));
    close_fp(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
    return -1;
  }
 
  
  fflush(cus_mon_ptr->csv_stats_ptr->csv_file_fp);

  //creating buffer to check uniqueness of the respective fields.
  sprintf(planhandle_buffer, "%s,%s,%s,%s", plan_handle, start_offset, end_offset, server_name);

  nslb_get_or_gen_norm_id(plan_handle_sql_id_key, planhandle_buffer, strlen(planhandle_buffer), &planhandle_newflag);

  if(planhandle_newflag)
  {
    //if(fprintf(cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp, "%s,%s,%s,%s,%s\n", plan_handle, start_offset, end_offset, server_name, buffer) < 0)
    if(fprintf(cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp, "%s,%s\n", planhandle_buffer, buffer) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file MSSQLPlanHandle.csv. NormSqlId = %d, ERROR: %s", norm_sql_id, strerror(errno));
      NSDL2_MON(NULL, NULL, "Error in writting data in meta data csv file MSSQLPlanHandle.csv. norm_sql_id = %d", norm_sql_id);
      close_fp(cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp);
      return -1;
    }
    fflush(cus_mon_ptr->csv_stats_ptr->planhandle_csv_file_fp);
  }

  if(!newflag)
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Already have normalised id %d.", norm_sql_id);
    SERIAL_NO_INCREMENT(serial_no); 
    return 0;
  }
  
  if(!sql_handle)	sql_handle = hyphen_buf;
  if(!sql_query)	sql_query = hyphen_buf;
  if(!server_name)	server_name = hyphen_buf;

  if(fprintf(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp, "%d,%s,%s,%d,%s\n", norm_sql_id, sql_handle, sql_query, serial_no, server_name) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file MSSQLMetadata.csv. NormSqlId = %d, ERROR: %s", norm_sql_id, strerror(errno));
    NSDL2_MON(NULL, NULL, "Error in writting data in meta data csv file MSSQLMetadata.csv. norm_sql_id = %d", norm_sql_id);
    close_fp(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp);
    return -1;
  }
  SERIAL_NO_INCREMENT(serial_no);

  fflush(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp);

  return 0;
}
*/
/*
static int fill_data_in_csv_for_temp_db(char *buffer, CM_info *cus_mon_ptr, int norm_field, int handle_field)
{
  int i = 0;
  int norm_sql_id = -1;
  int newflag = 0;
  char *ptr; 
  char csv_filepath[1024];
  time_t now = time(NULL);
  long long time_stamp_in_sec;
  char *sql_query = NULL, *sql_handle = NULL;
  char hyphen_buf[2] = "-";
  struct tm tm_struct;

  if((cus_mon_ptr->csv_stats_ptr->csv_file_fp == NULL) || (cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp == NULL))
  {
    if(check_for_csv_file(cus_mon_ptr) < 0)
      return -1;
  }

  while((ptr = strstr(buffer, ",")) != NULL)
  {
    *ptr = '\0';
    i++;
    
    if(i == norm_field)
    {
      norm_sql_id = nslb_get_or_gen_norm_id(temp_db_sql_id_key, buffer, strlen(buffer), &newflag);
      sql_query = buffer;
      if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d,", norm_sql_id) < 0)
      {
        //buffer logged here will not be exact data line
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file: %s. Data line: %s ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, buffer, strerror(errno)); 
        close_fp(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
        return -1;
      }
      buffer = ptr + 1;
      continue;
    }
    else if(i == handle_field)
    {
      sql_handle = buffer;
      buffer = ptr + 1;
      continue;
    }
   
    if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%s,", buffer) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file: %s. Data line: %s ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, buffer, strerror(errno));
      close_fp(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
      return -1;
    }
   
    buffer = ptr + 1;
  }

  //code will come here in case it didnt find field in above loop. If field is the last entry.
  //i == (field-1)  ->  its done when field of which we need to do normalisation is the last one. In the above loop, it wont traverse the last index because there isnt any comma at the end of line.
  //i == field  ->    Its done if we find field in the middle of traversing and we will dump rest of the data as it is.

  //Dumping buffer in both the csv files. This is because buffer will contain the last value of data line in case of temp db monitor. And we need to dump last value i.e. server_name in both the tables becasue it is a composite key in normalised table.
  if(newflag)
  {
    if(!sql_handle)	sql_handle = hyphen_buf;
    if(!sql_query)	sql_query = hyphen_buf;

    if(fprintf(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp, "%d,%s,%s,%s\n", norm_sql_id, sql_handle, sql_query, buffer) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file: %s. Data line: %s ERROR: %s", cus_mon_ptr->csv_stats_ptr->metadata_csv_file, buffer, strerror(errno));
      close_fp(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp);
      return -1;
    }
  }

  //buffer = ptr + 1;

  strftime(csv_filepath, 20, "%Y-%m-%d %H:%M:%S", nslb_localtime(&now, &tm_struct, 1));
  time_stamp_in_sec = caluculate_timestamp_from_date_string(csv_filepath);

  if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%s,%d,%lld\n", buffer, serial_no, time_stamp_in_sec) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in file: %s. ERROR: %s", cus_mon_ptr->csv_stats_ptr->metadata_csv_file, strerror(errno));
    close_fp(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp);
    return -1;
  }

  SERIAL_NO_INCREMENT(serial_no);
  //cus_mon_ptr->csv_stats_ptr->serial_no++;

  fflush(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp);
  fflush(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
  return 1;

  fflush(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
  return 1;
}
*/

int check_if_alredy_deleted(CM_info *cus_mon_ptr, int idx)
{
  int i;
  for(i = 0; i < cus_mon_ptr->total_deleted_vectors; i++)
  {
    if(cus_mon_ptr->deleted_vector[i] == idx)
    {
      MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              "[ %s ] is already added in deleted_vector list therefore we are not going add this vector in deleted_vector list", cus_mon_ptr->vector_list[idx].vector_name);
      return -1;
    }
  }
  return 0;
}

//This will change file when date changes.
int check_and_change_file_wrt_date(CM_info *cus_mon_ptr)
{
  time_t now = time(NULL);
  char file[1024] = {0}, temp_file[1024] = {0}, buffer[50] = {0};
  char *ptr = NULL;
  int date_format;
  struct tm tm_struct;
  
  strftime(buffer, 50, "%Y%m%d", nslb_localtime(&now, &tm_struct, 1));
  
  date_format = atoi(buffer);
  if(date_format > cus_mon_ptr->csv_stats_ptr->date_format)
  {
    strcpy(file, cus_mon_ptr->csv_stats_ptr->csv_file);

    ptr = strrchr(file, '_');
    if(ptr == NULL)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                          "Invalid file path saved. Could not find '_' in file path (%s). Hence not storing data.", file); 
      return -1;
    }

    strncpy(temp_file, file, (ptr - file));
    int temp_file_len = strlen(temp_file);
    sprintf(temp_file + temp_file_len, "_%s", buffer);
  
    if(cus_mon_ptr->csv_stats_ptr->csv_file_fp)
      fclose(cus_mon_ptr->csv_stats_ptr->csv_file_fp);

    cus_mon_ptr->csv_stats_ptr->csv_file_fp = NULL;
    REALLOC_AND_COPY(temp_file, cus_mon_ptr->csv_stats_ptr->csv_file, (strlen(temp_file) + 1), "Changing file name as date has been changed", -1);
  }
  return 0;
}


//Generalised function for monitors whose data is to be directly dumped in csv without any manipulation.
int dump_data_in_csv(char *buffer, CM_info *cus_mon_ptr, char flag)
{
  char csv_filepath[1024];
  time_t now = time(NULL);
  long long time_stamp_in_sec;
  struct tm tm_struct;
  time_t   sec_from_epoch;
  char local_time_buff[1024];

  if((cus_mon_ptr->csv_stats_ptr->csv_file_fp == NULL))
  {
    if(check_for_csv_file(cus_mon_ptr) < 0)
      return -1; 
  }
  sec_from_epoch = time(NULL);
  struct tm *localtime_breakDown = localtime(&sec_from_epoch);
  localtime_breakDown->tm_isdst = -1;
   strftime(local_time_buff, 1024, "%s", localtime_breakDown);
  if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%s|%lld", buffer, atoll(local_time_buff) * 1000) < 0) 
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump into file. File: %s, Data: %s ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, buffer, nslb_strerror(errno));
    return -1;
  }

  if(flag & TIMESTAMP_NOT_NEEDED)
  {
    //As we are returning from here, we need to put a new line to the file.
    fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "\n");
    fflush(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
    return 0;
  }
  
  strftime(csv_filepath, 20, "%Y-%m-%d %H:%M:%S", nslb_localtime(&now, &tm_struct, 1));
  time_stamp_in_sec = caluculate_timestamp_from_date_string(csv_filepath);

  if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, ",%d,%lld\n", serial_no,time_stamp_in_sec) < 0)
  //if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d,%lld\n", cus_mon_ptr->csv_stats_ptr->serial_no,time_stamp_in_sec) < 0)
  //if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%lld\n", time_stamp_in_sec) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump timestamp into file. File: %s, Data: %lld ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, time_stamp_in_sec, nslb_strerror(errno));
    return -1;
  }
  //cus_mon_ptr->csv_stats_ptr->serial_no++;
  SERIAL_NO_INCREMENT(serial_no);
 
  fflush(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
  return 0;
} 
/*
int fill_data_in_csv_for_mssql(int timestamp_idx[], int count, char *buffer, CM_info *cus_mon_ptr)
{
  char *ptr, *pointer;
  int i = 0, j = 0;
  long long time_stamp_in_sec;
  char timestamp[50];
  time_t now = time(NULL);
  struct tm tm_struct;

  if((cus_mon_ptr->csv_stats_ptr->csv_file_fp == NULL))
  {
    if(check_for_csv_file(cus_mon_ptr) < 0)
      return -1;
  }
 
  while((ptr = strstr(buffer, ",")) != NULL)
  {
    *ptr = '\0';
    i++;
    
    if(i == timestamp_idx[j])
    {
      if((buffer[0] == '\0') || (!strcmp(buffer, "NULL")) || (!strcmp(buffer, "null")) || (!strcmp(buffer, "0")))
      {
        time_stamp_in_sec = 0;
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              " Received Invalid field on date time column '%d' for monitor %s. Buffer = %s. So setting data for this column to 0.", i, cus_mon_ptr->gdf_name, buffer);
      }
      else
      {
        pointer = strrchr(buffer, '.');
        if(pointer)
          *pointer = '\0';
       
        time_stamp_in_sec = caluculate_timestamp_from_date_string(buffer);
      }

      if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%lld,", time_stamp_in_sec) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump timestamp into file. File: %s, Data: %lld ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, time_stamp_in_sec, strerror(errno));
        return -1;
      }

      j++;
      buffer = ptr + 1;

      if(j == count)
        break;

      continue;
    }
   
    if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%s,", buffer) < 0)
    {
       MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump timestamp into file. File: %s, Data: %s ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, buffer, strerror(errno));
      return -1;
    }

    buffer = ptr + 1;
  }

  if(j != count)   //In case last field is date-time field.
  {
    pointer = strrchr(buffer, '.');
    if(pointer)
      *pointer = '\0';

    time_stamp_in_sec = caluculate_timestamp_from_date_string(buffer);
    if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%lld,", time_stamp_in_sec) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump timestamp into file. File: %s, Data: %lld ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, time_stamp_in_sec, strerror(errno));
      return -1;
    }
  }
  else
  { 
    if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%s,", buffer) < 0)
    { 
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump timestamp into file. File: %s, Data: %s ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, buffer, strerror(errno));
      return -1;
    }
  }
  
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", nslb_localtime(&now, &tm_struct, 1));
  time_stamp_in_sec = caluculate_timestamp_from_date_string(timestamp);

  if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d,%lld\n", serial_no,time_stamp_in_sec) < 0)
  //if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d,%lld\n", cus_mon_ptr->csv_stats_ptr->serial_no,time_stamp_in_sec) < 0)
  //if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%lld\n", time_stamp_in_sec) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump timestamp into file. File: %s, Data: %lld ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, time_stamp_in_sec, strerror(errno));
    return -1;
  }
  SERIAL_NO_INCREMENT(serial_no);
  //cus_mon_ptr->csv_stats_ptr->serial_no++;

  fflush(cus_mon_ptr->csv_stats_ptr->csv_file_fp);

  return 0;
}
*/
//This method is to fill data 
int filldata(CM_info *cus_mon_ptr,int buff_length)
{
  int i = 0, k = 0, inst_id = -1; 
  int is_numeric = 0;
  int  max = cus_mon_ptr->no_of_element;
  Long_long_data *data = NULL;
  char *buffer = cus_mon_ptr->data_buf;
  int dyn_idx = -1;
  char *vector_name = NULL, *tok_ptr = NULL;
  char *field[MAX_DYNAMIC_VECTORS];
  char breadcrumb[1024];
  breadcrumb[0] = '\0';
  int total_flds = 0;
  char *overall_vectors = NULL, *inst_ptr = NULL, *colon_ptr = NULL;
  char vector[VECTOR_NAME_MAX_LEN] = {0};
  int vectorid = -1;
  int generator_id = -1;
  int is_new_format=0;
  int ret,num;
  char temp_buf[MAX_MONITOR_BUFFER_SIZE + 1];
  int val_ret = -1;
  char *tmp_pipe_ptr = NULL;
  char *data_ptr;
  char nc_temp_buf[1024];
  char save = '\0';     //This is to temporary store the character where null is placed, so that we can undo the change.
  int m = 0;
  int z = 0;
  int tieridx, instanceidx, vectoridx ;
  char backend_name[VECTOR_NAME_MAX_LEN +1];
  CM_vector_info *vector_list = NULL;
  backend_name[0] = '\0';

  /* To save monitor output temporarily, in order to give complete output in event.log 
   * In case monitor output is: Received invalid data from custom monitor: Interval can not be less than 10000 ms forcing                                 to default 60000 ms (60 seconds).
   * Earlier in event.log: Received invalid data from custom monitor: Interval
   * Now in event.log: Received invalid data from custom monitor: Interval can not be less than 10000 ms forcing to defa                         ult 60000 ms (60 seconds).*/
  char save_buf[MAX_MONITOR_BUFFER_SIZE + 1]; 
  memcpy(save_buf, buffer, buff_length+1);  //saving here because below 'strtok(buffer, " ");' will tokenize the buffer.
  save_buf[MAX_MONITOR_BUFFER_SIZE] = '\0';
  memcpy(temp_buf, buffer,buff_length+1); //saving here because below 'strtok(buffer, " ");' will tokenize the buffer.
  temp_buf[MAX_MONITOR_BUFFER_SIZE] = '\0';
  int flag =0;
  // For SM test
  if((total_mon_config_list_entries > 0) && (buffer[0] == 's' || buffer[0] == 'S'))
  {
    if((update_mon_config_status(cus_mon_ptr)) == 1)
      return SUCCESS;
  }

  // Here we are not going to dump data in rtg in case of SM when monitor is run once.
  if((total_mon_config_list_entries > 0) && (cus_mon_ptr->sm_mode == SM_RUN_ONCE_MODE))
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
                              "Received data for gdf = %s. Not going to dump data in RTG file", cus_mon_ptr->gdf_name, cus_mon_ptr->g_mon_id);
    return SUCCESS;
  }
  if(buffer[0] == 'E')
  {
    if(validate_data(cus_mon_ptr))
      return SUCCESS;
  }

  NSDL2_MON(NULL, NULL, "Method called. Buffer = [%s]", buffer);
  // We are maintaing one row of CM_info per vector of dynamic vector monitor
  // So based on which vector data came, we need to point to that row

  //Here we are checking if vector format is new or not. All the monitor has bee converted to new format but only cohenrence monitor send vectors in old format. Need to remove this check if that also gets converted in new format.
  //Handling of Error case which has been sent from monitor on its connection. Error format be like:
  //   E:<Error code>:<Error message>
  //   ex:     E:01:Could not find BC command.            E:02:curl command not available.
  if(strchr(buffer, ':'))   //New format vector
  {
    if(buffer[0] == 'E')
    {
      num=get_tokens(buffer, field, ":", 5);
 
      if(num < 2)
      {
         MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Received Invalid data from monitor. Data = %s", save_buf);
        return SUCCESS;
      }
   
      if(!strcmp(field[1], "01"))
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                          "Received error from monitor. Command not found on cmon. Error code: %s, Error Msg: %s", field[1], field[2]);
      }
      else if(!strcmp(field[1], "02"))
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                          "Received error from monitor. There has been some error with thirdparty. Error code: %s, Error Msg: %s", field[1], field[2]);
        update_health_monitor_sample_data(&hm_data->mon_with_thirdparty_issues); 
      }

      return SUCCESS;
    }
  }

  if (cus_mon_ptr->num_dynamic_filled >= cus_mon_ptr->total_vectors)
  {
    NSDL2_MON(NULL, NULL, "All data received. Resetting num_dynamic_filled");
    cus_mon_ptr->num_dynamic_filled = 0;
  }
/*
  if(strncmp(cus_mon_ptr->gdf_name, "NA_mssql_query_execution_stats.gdf", 34) == 0)
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              " received data for MSSQL stats monitor GDF: %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
    NSDL2_MON(NULL, NULL, "received data for MSSQL stats monitor GDF: %s. Going to dump data in csv. Data: %s", cus_mon_ptr->gdf_name, buffer);
    fill_data_in_csv_for_query_execution_stats(buffer, cus_mon_ptr);
    return 0;
  }
  else if(!strncmp(cus_mon_ptr->gdf_name, "NA_mssql_monitor_services.gdf", 29))
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received data for mssql monitor service stats monitor. GDF: %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
    NSDL2_MON(NULL, NULL, "Received data for mssql monitor service stats monitor. GDF: %s. Going to dump data in csv. Data: %s", cus_mon_ptr->gdf_name, buffer);
    fill_data_in_csv_for_mssql(cus_mon_ptr->csv_stats_ptr->date_time_idx, cus_mon_ptr->csv_stats_ptr->date_time_col_count, buffer, cus_mon_ptr);
    return 0;
  }
  else if(!strncmp(cus_mon_ptr->gdf_name, "NA_mssql_jobs_stats.gdf", 23))
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received data for mssql jobs stats monitor. GDF: %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
    NSDL2_MON(NULL, NULL, "Received data for mssql jobs stats monitor. GDF: %s. Going to dump data in csv. Data: %s", cus_mon_ptr->gdf_name, buffer);
    fill_data_in_csv_for_mssql(cus_mon_ptr->csv_stats_ptr->date_time_idx, cus_mon_ptr->csv_stats_ptr->date_time_col_count, buffer, cus_mon_ptr);
    return 0;
  }
  else if(!strncmp(cus_mon_ptr->gdf_name, "NA_mssql_session_stats.gdf", 26))
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received data for mssql session stats monitor. GDF: %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
    NSDL2_MON(NULL, NULL, "Received data for mssql session stats monitor. GDF: %s. Going to dump data in csv. Data: %s", cus_mon_ptr->gdf_name, buffer);
    fill_data_in_csv_for_mssql(cus_mon_ptr->csv_stats_ptr->date_time_idx, cus_mon_ptr->csv_stats_ptr->date_time_col_count, buffer, cus_mon_ptr);
    return 0;
  }
  else if(!strncmp(cus_mon_ptr->gdf_name, "NA_mssql_job_history_stats.gdf", 29))
  {
     MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received data for mssql job history stats monitor. GDF: %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
     NSDL2_MON(NULL, NULL, "Received data for mssql job history stats monitor. GDF: %s. Going to dump data in csv. Data: %s", cus_mon_ptr->gdf_name, buffer);
    fill_data_in_csv_for_mssql(cus_mon_ptr->csv_stats_ptr->date_time_idx, cus_mon_ptr->csv_stats_ptr->date_time_col_count, buffer, cus_mon_ptr);
    return 0;
  }
  else if(!strncmp(cus_mon_ptr->gdf_name, "NA_mssql_mirroring.gdf", 22))
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received data for mssql monitor. GDF: %s. Data: %s. Going to dump data in csv.", cus_mon_ptr->gdf_name, buffer);
    fill_data_in_csv_for_mssql(cus_mon_ptr->csv_stats_ptr->date_time_idx, cus_mon_ptr->csv_stats_ptr->date_time_col_count, buffer, cus_mon_ptr);
    return 0;
  }
  else if(!strncmp(cus_mon_ptr->gdf_name, "NA_mssql_temp_db.gdf", 20))
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received data for mssql monitor. GDF: %s. Data: %s. Going to dump data in csv.", cus_mon_ptr->gdf_name, buffer);
    NSDL2_MON(NULL, NULL, "Received data for mssql temp db monitor. GDF: %s. Going to dump data in csv. Data: %s", cus_mon_ptr->gdf_name, buffer);
    fill_data_in_csv_for_temp_db(buffer, cus_mon_ptr, 6, 5);
    return 0;
  }
  else if(!strcmp(cus_mon_ptr->gdf_name, "NA_mssql_blocking_session_stats.gdf"))
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "received data for gdf = %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
    dump_data_in_csv(buffer, cus_mon_ptr, TIMESTAMP_NEEDED);
    return 0;
  }
  else if(strncmp(cus_mon_ptr->gdf_name, "NA_mssql_", 9) == 0)
  { 
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "received data for gdf = %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
    fill_data_in_csv_for_mssql_new( cus_mon_ptr , buffer );
  }
*/
  //TODO Set flag for below gdf 
  if(cus_mon_ptr->flags & NA_WMI_PERIPHERAL_MONITOR)
  {
    MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "received data for gdf = %s. Going to dump data in csv.", cus_mon_ptr->gdf_name);
    NSDL2_MON(NULL, NULL, "received data for gdf = %s. Going to dump data in csv. Data: %s", cus_mon_ptr->gdf_name, buffer);
    if(check_and_change_file_wrt_date(cus_mon_ptr) < 0)
      return -1;

    dump_data_in_csv(buffer, cus_mon_ptr, TIMESTAMP_NOT_NEEDED);
    return 0;
  }

  if (monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic) {
    NSDL3_MON(NULL, NULL, "Monitor is dynamic vector and num_dynamic_filled = %d, monitor name = %s, total_vectors = %d", 
                           cus_mon_ptr->num_dynamic_filled, cus_mon_ptr->monitor_name, cus_mon_ptr->total_vectors);

     if(cus_mon_ptr->flags & ND_MONITOR)  
     {
       if(!strncmp(buffer, "nd_control_req:action=monitor_instance_reset", 44))
       {
         MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Got instance reset message on data connection for monitor = %s, gdf = %s, Message: %s", cus_mon_ptr->monitor_name, cus_mon_ptr->gdf_name, buffer);
         inst_ptr = strstr(buffer, "instanceid=");
         inst_ptr += 11;
         if(inst_ptr)
         {
           colon_ptr = strchr(inst_ptr, ';');
           *colon_ptr = '\0';

           inst_id = atoi(inst_ptr);

           NSDL3_MON(NULL, NULL, "\n inst_id = [ %d] \n", inst_id);
           vector_list = cus_mon_ptr->vector_list;
           for (k = 0; k < cus_mon_ptr->total_vectors; k++)
           {
             NSDL3_MON(NULL, NULL, "vector_list[k].vector_name = [ %s] , vector_list[k].inst_actual_id  = [ %d ] \n", vector_list[k].vector_name, vector_list[k].inst_actual_id );

             if((vector_list[k].inst_actual_id > -1) && (vector_list[k].inst_actual_id == inst_id) && !(vector_list[k].flags & OVERALL_VECTOR))
             {
               NSDL3_MON(NULL, NULL, "MATCHED: vector_list[k].vector_name = [ %s] , inst_id = [ %d] \n", vector_list[k].vector_name, inst_id);
               delete_entry_for_nd_monitors(cus_mon_ptr, k, 1); //sending 1 to set flag in case of instance
             } 
           }
         }
         return 0;
       }
       //DELETE_VECTOR|T>S>I>BT1 T>S>I>BT2
       else if(!strncmp(buffer, DELETE_VECTOR_MSG, DELETE_VECTOR_MSG_LEN))
       {
         MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                               " Received Delete vector format for ND monitor: %s", buffer);

         tok_ptr = strchr(buffer, '|');
         if(!tok_ptr)
         { 
           MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                               " Received invalid Delete vector format: %s", buffer);
           return 0;
         }
         
         //tokenize and save vector
         total_flds = get_tokens((tok_ptr + 1), field, " ", MAX_DYNAMIC_VECTORS);
         NSDL3_MON(NULL, NULL, "total_flds for deleting ND vectors = [%d]", total_flds);
     
         CM_info *tmp_cm_ptr = NULL;
         tmp_cm_ptr = set_or_get_nd_percentile_ptrs(cus_mon_ptr);

         for(i = 0; i < total_flds; i++)
         {
           NSDL3_MON(NULL, NULL, "Going to delete ND vectors = [%s]", field[i]);
           if(strchr(field[i], ':'))     //If New format, need to match ID
           {
             if(parse_vector_new_format(field[i], vector, &tieridx, &instanceidx, &vectoridx, backend_name, (char*)data, NULL, 4, cus_mon_ptr->gdf_name) < 0)
             {
               NSDL3_MON(NULL, NULL, "Received Invalid Delete Request for vector = [%s]", vector);
               MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                "Received Invalid Delete Request for vector : %s", vector);
               continue;
             }
         
             fetch_hash_index_and_delete_nd_vector(vector, cus_mon_ptr);   //compare with vector when ids are removed using hash table
           }
           else
             fetch_hash_index_and_delete_nd_vector(field[i], cus_mon_ptr);        //Compare with Vector name using hash table
           // delete percentile vector
           if(tmp_cm_ptr)
             create_vector_and_delete_percentile(field[i], tmp_cm_ptr);
         }
         return 0;
       }
       else
       {
         //check if first byte is numeric or char.In old format tier, its assumed that tier 
         //will not start with number.
         if(isdigit(buffer[0]))
           is_numeric = 1;
    
         if(is_numeric)
         {
           //check & do mapping table work if new else update data only
           dyn_idx = generate_aggregate_vector_list(cus_mon_ptr, temp_buf, &overall_vectors); 
         }
         else
	 {
           MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                              " Received invalid vector '%s': old format for gdf %s is not supported.", buffer, cus_mon_ptr->gdf_name);
	   return 0;
	 }
         //found new vector add in structure
         if(dyn_idx > 0 || overall_vectors != NULL)
         {
           //check if vector format changes or not
           //Since old format vector are expected to come only at the begining, but not as vector list but as init_vector_list, which is saved when a ND monitor is enabled with the file path which contain the vector in old format. vector from that file is added in the structure. Data will always come in new format. So we need to migrate those vector already added in structure in old format to new format if data is coming for those vectors, otherwise vector coming with data will be added in the structure as new vectors.
   
           //null terminating because we need complete vector string here , not data
           //Already data processed in generate_aggregate_vector_list(), hence we do not need data anymore  
           if((tok_ptr = strrchr(save_buf, '|')) != NULL)
             *tok_ptr = '\0';
          

           //if(add_new_vector(cus_mon_ptr, save_buf, save_buf, NULL, max, breadcrumb) < 0) //new vector with data
           //val_ret = add_new_vector(cus_mon_ptr, temp_buf,  save_buf, NULL, max, breadcrumb); //new vector with data
           val_ret = add_cm_vector_info_node(cus_mon_ptr->monitor_list_idx, save_buf, NULL, max, breadcrumb, 1, 1);//new vector with data
           if(val_ret >= 0)
             monitor_runtime_changes_applied = 1;

           //return val_ret;
           if (overall_vectors != NULL)
           {
             //adding overall vectors in CM_Info structure
             total_flds = get_tokens(overall_vectors, field, " ", MAX_DYNAMIC_VECTORS);
             NSDL3_MON(NULL, NULL, "total_flds = [%d]", total_flds);
  
             if(total_flds > 0)
               monitor_runtime_changes_applied = 1;

             for(i = 0; i < total_flds; i++)
             {

               sprintf(save_buf, "%s", field[i]);
               //if(add_new_vector(cus_mon_ptr, save_buf, field[i], NULL, max, breadcrumb) < 0) //new vector with data
               if(add_cm_vector_info_node(cus_mon_ptr->monitor_list_idx, field[i], NULL, max, breadcrumb, 1, 1) < 0) //new vector with data
                 continue;
               // this code will be called at runtime so cus_mon_ptr will be pointing to dyn_cm_info_runtime_ptr
               //Newly added entry will be at end of dyn_cm_info_runtime_ptr. So go to last element  total_dyn_cm_runtime_entries
               // and check
               //if breadcrumb matches with cus_mon_ptr[total_dyn_cm_runtime_entries -1].mon_breadcrumb
               if (strstr(save_buf, cus_mon_ptr->vector_list[cus_mon_ptr->total_vectors - 1].mon_breadcrumb) != NULL)
               {
                 cus_mon_ptr->vector_list[cus_mon_ptr->total_vectors - 1].flags |= OVERALL_VECTOR;
                 MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           " Adding overall vector breadcrumb is %s", cus_mon_ptr->vector_list[cus_mon_ptr->total_vectors - 1].mon_breadcrumb);
               }
             }
           }
         } 
         else if( dyn_idx < 0)
         {
           if(is_numeric == 1)
           {
             //dyn_idx = -2 -----------> For data validation, logging has been done in generate_aggregate_vector_list(), so ignoring here.
             if(dyn_idx != -2)
               MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                          " Received invalid vector or space/pipe/colon in vector name %s.", save_buf);
           }
         }
       }
       FREE_AND_MAKE_NULL(overall_vectors, "overall_vectors", -1);
       //found data in new vector format, already processed hence returing,  old vector format will be processed below
       if(is_numeric == 1)
         return 0;
     }
     //set flag for cm_nv_ ... no need to strstr KJ
     else if((cus_mon_ptr->flags & NV_MONITOR) &&  strncmp(buffer, DELETE_VECTOR_MSG, DELETE_VECTOR_MSG_LEN) != 0)
     {  
       if(!strncmp(buffer, "nv_req_cmd=nv_monitor_reset", 27))
       {
         if(cus_mon_ptr->dummy_data != NULL)
         {
           vector_list = cus_mon_ptr->vector_list;
           for (k = 0; k < cus_mon_ptr->total_vectors; k++)
           {
            // cm_ptr->data = cus_mon_ptr->dummy_data;

             // set "nan" on monitor data connection break
             for(m = 0; m < cus_mon_ptr->no_of_element; m++)
             {
               vector_list[k].data[m] = 0.0/0.0;
             }

             vector_list[k].vectorIdx = -1;
           }
           if(cus_mon_ptr->vectorIdxmap->idxMap){
             FREE_AND_MAKE_NULL(cus_mon_ptr->vectorIdxmap->idxMap, "cus_mon_ptr->vectorIdxmap->idxMap", -1); 
             cus_mon_ptr->vectorIdxmap->mapSize = 0;
             cus_mon_ptr->vectorIdxmap->maxAssValue = 1;
           }
         }
         return 0;
       }
       else
       {
         // here buffer will have data in format :       Id:VectorName|data   

         // call function ; parse_and_fill_nv_data , if this returns '1' then means new vector, we need to add this vector in CM_info table
         new_vector = parse_and_fill_nv_data(buffer, cus_mon_ptr);

         if(new_vector != -2) // -2 means old format
         {
           if( new_vector )
           {

             //monitor_runtime_changes_applied = 1;

             //null terminating because we need complete vector string here , not data
             //Already data processed in generate_aggregate_vector_list(), hence we do not need data anymore  
             if((tok_ptr = strrchr(save_buf, '|')) != NULL)
               *tok_ptr = '\0';
	      
             snprintf(temp_buf, MAX_MONITOR_BUFFER_SIZE + 1, "%s", save_buf); //saving here because below we cant use same buffer in both search_and_set_values and add_new_vector function as search_and_set_values is setting buffer null.
	     temp_buf[1024] = '\0'; 
              
             //compare vector in dyn_num_vectors, if found get and set mapped index & data ptr, if not found then add as new vector
             if(search_and_set_values(temp_buf, cus_mon_ptr) == 0) //not matched, add as new vector
             {
               //if(add_new_vector(cus_mon_ptr, save_buf, save_buf, NULL, max, breadcrumb) < 0) //new vector with data
               //val_ret = add_new_vector(cus_mon_ptr, save_buf, save_buf, NULL, max, breadcrumb); //new vector with data
               val_ret = add_cm_vector_info_node(cus_mon_ptr->monitor_list_idx, save_buf, NULL, max, breadcrumb, 1, 1);
               if(val_ret >= 0)
                 monitor_runtime_changes_applied = 1;

               return val_ret;
             }
           }
           return 0;
         }
       }
     }
     else if(cus_mon_ptr->cs_port == -1)       //Assuming only for netcloud monitor, port will be -1.
     {
       if(cus_mon_ptr->no_of_element == 0)
       {
         get_no_of_elements(cus_mon_ptr, &max);
         cus_mon_ptr->no_of_element = max;
       }
       else
         max = cus_mon_ptr->no_of_element;

      /*First sample : 
          GeneratorID:VectorID:GeneratorName>IP|Data\n
        Rest sample : 
          GeneratorID:VectorID|Data\n */

       strcpy(nc_temp_buf, buffer);

       data_ptr = strchr(buffer, '|');
       if(data_ptr)
       {
         *data_ptr = '\0';                  //Now buffer will be containing only vector without data.
         data_ptr++;
       }
       else
       {
         MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                          " Received data line doesnot contain '|' before data. Data line: %s.", buffer);
         return -1;
       }

       ret = parse_netcloud_data_buf(nc_temp_buf, &vectorid, &generator_id, vector);
       if(ret == -1)
         return -1;
       else if(ret == 1) //new vector
       {
         if(vectorid >= genVectorPtr[generator_id][0].max_vectors)
         {
           MY_REALLOC_AND_MEMSET(genVectorPtr[generator_id], ((genVectorPtr[generator_id][0].max_vectors + DELTA_CUSTOM_MONTABLE_ENTRIES) * sizeof(GenVectorIdx)), (genVectorPtr[generator_id][0].max_vectors * sizeof(GenVectorIdx)), "GeneratorVectorIndex Column allocation", -1);
           genVectorPtr[generator_id][0].max_vectors += DELTA_CUSTOM_MONTABLE_ENTRIES;
         }
	  
	 MY_MALLOC_AND_MEMSET(genVectorPtr[generator_id][vectorid].data, (max * sizeof(double)), "GeneratorVectorIndex allocation for data", -1);

         if(!(g_tsdb_configuration_flag & RTG_MODE))
	 {
            //MY_MALLOC_AND_MEMSET(genVectorPtr[generator_id][vectorid].metrics_idx, (max * sizeof(long)), "GeneratorVectorIndex allocation for data", -1);
	      ns_tsdb_malloc_metric_id_buffer(&genVectorPtr[generator_id][vectorid].metrics_idx, max);
	 }
         NSDL3_MON(NULL, NULL, "Data pointer in the 2-D table is malloced to fill data.");
         validate_and_fill_data(cus_mon_ptr, max, data_ptr, genVectorPtr[generator_id][vectorid].data,genVectorPtr[generator_id][vectorid].metrics_idx, &genVectorPtr[generator_id][vectorid].metrics_idx_found, vector);
   
         genVectorPtr[generator_id][0].num_vector += 1;

         if(genVectorPtr[generator_id][0].gen_fd == 0)
         {
           genVectorPtr[generator_id][0].gen_fd = cus_mon_ptr->fd;
           cus_mon_ptr->fd = -1;
         }


         //if(add_new_vector(cus_mon_ptr, save_buf, vector, buffer, max, breadcrumb) < 0)
         if(add_cm_vector_info_node(cus_mon_ptr->monitor_list_idx, vector, buffer, max, breadcrumb, 1, 1) <  0)
           return -1;

         monitor_runtime_changes_applied = 1;
       }
       else   //old vector
       {
         cus_mon_ptr->fd = -1;
          validate_and_fill_data(cus_mon_ptr, max, data_ptr, genVectorPtr[generator_id][vectorid].data,genVectorPtr[generator_id][vectorid].metrics_idx, &genVectorPtr[generator_id][vectorid].metrics_idx_found, vector);
       }

       return 0;
     }

    //Manish: It may be possible that monitor data will come in zigzaz sequence 
    //So we need to sure data is mapped with their own vector name
    //Eg: vector1|data1 data2 data3 ...
    //    vector2|data1 data2 data3 ... 
    tok_ptr = strchr(buffer, '|'); 
    if(tok_ptr != NULL) //If Data is comming with vector name 
    {
      NSDL3_MON(NULL, NULL, "Since data is comening with vector name so seraching dynamic vector index in custom monitor array");

      vector_name = buffer; 
     //ADD_VECTOR|<vector name> <>
     //KJTSDB : exclude this for TSDB
        if(strncmp(vector_name, ADD_VECTOR_MSG, ADD_VECTOR_MSG_LEN) == 0)
        {
          int failed_vectors = 0;
          flag = 1;
          NSDL3_MON(NULL, NULL, "vector_name = [%s], buffer = [%s]", vector_name?vector_name:NULL, buffer);
          tok_ptr++; 
      //KJ if nd backend moitor. This will eliminate dublicate. for duplicate log message
     // if (nd_backend_monitor)
      //{
        //generate_aggregate_vector_list();
      //}
      //If 
      //tokenize and save vector
          total_flds = get_tokens(tok_ptr, field, " ", MAX_DYNAMIC_VECTORS);
          NSDL3_MON(NULL, NULL, "total_flds = [%d]", total_flds);
      
          for(i = 0; i < total_flds; i++)
          {
            NSDL3_MON(NULL, NULL, "field[%d] = [%s]", i, field[i]);
            //KJ check return value, and set monitor_runtime_changes_applied if any vector added

           // TODO : HANDLE INVALID VECTOR FORMAT eg "1:" id without vector name
        //add_new_vector(cus_mon_ptr, save_buf, field[i], NULL, max, breadcrumb); //new vector without data
           if(add_cm_vector_info_node(cus_mon_ptr->monitor_list_idx, field[i], NULL, max, breadcrumb, 1, 1) < 0)
            failed_vectors++;
          }
          if(total_flds != failed_vectors)
            monitor_runtime_changes_applied = 1;
          return 0;
      }
      else if(strncmp(vector_name, DELETE_VECTOR_MSG, DELETE_VECTOR_MSG_LEN) == 0)
      {
	flag = 1;
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                             " Received delete vectors msg: '%s'.", buffer);
   
        NSDL3_MON(NULL, NULL, "vector_name = [%s], buffer = [%s]", vector_name?vector_name:NULL, buffer);
        tok_ptr++; 
      //tokenize and save vector
        total_flds = get_tokens(tok_ptr, field, " ", MAX_DYNAMIC_VECTORS);
        NSDL3_MON(NULL, NULL, "total_flds = [%d]", total_flds);

      //check field[0] is new format or old if total fields is greter than 0 and set flag of new vector
      //if((total_flds > 0) && (strstr(cus_mon_ptr->gdf_name, "cm_nv_") != NULL))
        if(total_flds > 0)
        {
          if((strchr(field[0], ':')) != NULL)
          is_new_format = 1;
        }
        for(i = 0; i < total_flds; i++)
        {
          //reset TODO THIS IS NOT REQUIRED, ONCE TESTED THROUGHLY REMOVE THIS RESET LINE  Mani
          dyn_idx = -1; // for new format vectors this is not needed, this is for old format vectors
       
          NSDL3_MON(NULL, NULL, "field[%d] = [%s]", i, field[i]);

          if(is_new_format == 1) // Found vectors in new format 'id:vectorname'
          {
            //extract id & vector name
            parse_nv_vector(field[i],vector,&vectorid);

          //because NV vectors has separate handling, we need to differentitate
            if((vectorid < cus_mon_ptr->max_mapping_tbl_vector_entries) && (cus_mon_ptr->dvm_cm_mapping_tbl_row_idx != -1)) // other than netvision monitors vectors
            {
              dyn_idx = dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].relative_dyn_idx;
   
            //check in vector table for a case:  Vector is received and deleted in the same sample
            if(dyn_idx < 0)
              dyn_idx = find_dyn_vct_idx(cus_mon_ptr, vector, cus_mon_ptr->num_dynamic_filled);
          }
          else 
          { 
            // NV vector
            //from mapping table .... TODO  //Maninder  // ADDITION OF ALREADY DELETED VECTOR IS NOT SUPPORTED YET
            dyn_idx = find_dyn_vct_idx(cus_mon_ptr, vector, cus_mon_ptr->num_dynamic_filled); // TODO THIS NEED TO BE OPTIMIZE FOR NV
          }
        }
        else //vectors in old format
        {
          dyn_idx = find_dyn_vct_idx(cus_mon_ptr, field[i], cus_mon_ptr->num_dynamic_filled);
        }

        if(dyn_idx  < 0)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR, "Requested vector %s not found.", vector_name);
        }
        else
        {
          // cm_tbl_idx is CM_Info table row num & dyn_idx is vector index with respect to its parent i.e dyn_idx always ranges from (0 - dyn_num_vectors) while cm_tbl_idx ranges from (0 - total_cm_entries)

          vector_list = cus_mon_ptr->vector_list;

          //Mark vector for deletion
          vector_list[dyn_idx].flags |= RUNTIME_DELETED_VECTOR;
          vector_list[dyn_idx].flags |= RUNTIME_RECENT_DELETED_VECTOR;
          
          if(cus_mon_ptr->flags & ND_MONITOR)  
          {
            if((vector_list[dyn_idx].vectorIdx >= 0) && (vector_list[dyn_idx].instanceIdx >= 0))
            {
              (cus_mon_ptr->instanceVectorIdxTable)[vector_list[dyn_idx].mappedVectorIdx][vector_list[dyn_idx].instanceIdx]->reset_and_validate_flag |= ND_RESET_FLAG;
              (cus_mon_ptr->instanceVectorIdxTable)[vector_list[dyn_idx].mappedVectorIdx][vector_list[dyn_idx].instanceIdx]->reset_and_validate_flag &= ~GOT_ND_DATA;

              vector_list[dyn_idx].vectorIdx = -1;
              vector_list[dyn_idx].mappedVectorIdx = -1;
              vector_list[dyn_idx].instanceIdx = -1;
              vector_list[dyn_idx].tierIdx = -1;
            }
          }
          else if(cus_mon_ptr->flags & NA_KUBER)
          {
            add_node_and_pod_in_list(vector, &vector_list[dyn_idx], cus_mon_ptr, vector_list[dyn_idx].vectorIdx, DELETE_POD);
          } 
          // set "nan" on vector deletion
          for(z = 0; z < cus_mon_ptr->no_of_element; z++)
          {
            vector_list[dyn_idx].data[z] = 0.0/0.0;
          }

          //saving parent ptr only for diff file
          //saving deleted vector pointer into global array for diff file creation
          if((vector_list[dyn_idx].vector_state != CM_DELETED) && !(cus_mon_ptr->flags & ALL_VECTOR_DELETED) && !(vector_list[dyn_idx].flags & WAITING_DELETED_VECTOR))
          {
            if(cus_mon_ptr->total_deleted_vectors >= cus_mon_ptr->max_deleted_vectors)
            {
              MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                               "Monitor name = %s, total_deleted_vectors = %d and max_deleted_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_deleted_vectors,cus_mon_ptr->max_deleted_vectors);               
 
              create_entry_in_reused_or_deleted_structure(&cus_mon_ptr->deleted_vector, &cus_mon_ptr->max_deleted_vectors);
              MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                               "After create_entry_in_reused_or_deleted_structure Now total_entries = %d max_entries = %d", cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);   
            }
            cus_mon_ptr->deleted_vector[cus_mon_ptr->total_deleted_vectors] = dyn_idx;
            vector_list[dyn_idx].vector_state = CM_DELETED;
	      if(!(g_tsdb_configuration_flag & RTG_MODE) && (vector_list[dyn_idx].metrics_idx_found == 1))
	      {
                ns_tsdb_delete_metrics_by_id(cus_mon_ptr, vector_list[dyn_idx].metrics_idx);
                vector_list[dyn_idx].metrics_idx_found = 0;
	      }
            //deleted vectors handling
            vector_list[dyn_idx].flags |= RUNTIME_RECENT_DELETED_VECTOR;
         
            //Mark vector for deletion
            //We will not mark the vector waiting deleted for NA_KUBER, as these vector are not written in testrun.gdf & diff
            //We will not enable monitor_deleted_on_runtime and monitor_runtime_changes_applied for NA_KUBER otherwise it write empty diff
            //if only NA_KUBER vector in deleted in progress interval and no other change occured in same interval
            if(!g_enable_delete_vec_freq || (!strncmp(cus_mon_ptr->gdf_name, "NA_",3)))   //Feature disabled
            {
              cus_mon_ptr->vector_list[dyn_idx].flags |= RUNTIME_DELETED_VECTOR;
              cus_mon_ptr->vector_list[dyn_idx].vector_state = CM_DELETED;
	        if(!(g_tsdb_configuration_flag & RTG_MODE) && (vector_list[dyn_idx].metrics_idx_found == 1))
                {
                  ns_tsdb_delete_metrics_by_id(cus_mon_ptr, cus_mon_ptr->vector_list[dyn_idx].metrics_idx);
                  cus_mon_ptr->vector_list[dyn_idx].metrics_idx_found = 0;
                }
              if(strncmp(cus_mon_ptr->gdf_name, "NA_",3)) 
              {
                monitor_deleted_on_runtime = 1;
                monitor_runtime_changes_applied = 1;
                total_deleted_vectors++;
              }
            }
            else
            {
              total_waiting_deleted_vectors++;
              vector_list[dyn_idx].flags |= WAITING_DELETED_VECTOR;
            }
            cus_mon_ptr->total_deleted_vectors++;
          }

          if((cus_mon_ptr->dvm_cm_mapping_tbl_row_idx != -1) && (vectorid >= 0))
          {
	    dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].is_data_filled = -1;
            dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].relative_dyn_idx = -1;
            // NEED TO CHECK TODO .... dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].relative_dyn_idx = -1;   // ............check whether required or not
          }
          
          //set variable so that we should process vector deletion in deliver report

        }
      }
      return 0;
     }
     //KJTSDB : exclude this for TSDB end
     if(flag == 0)
     {
      *tok_ptr = 0; //Remove |
     
     //KJTSDB : exclude this for TSDB 
       if(!(g_tsdb_configuration_flag & TSDB_MODE))
       {
         CLEAR_WHITE_SPACE(vector_name);
         CLEAR_WHITE_SPACE_FROM_END(vector_name);     
     //KJTSDB : exclude this for TSDB end
       } 
      buffer = tok_ptr + 1; // Point buffer to after pipe
      CLEAR_WHITE_SPACE(buffer);
      CLEAR_WHITE_SPACE_FROM_END(buffer);

      NSDL3_MON(NULL, NULL, "vector_name = [%s], buffer = [%s]", vector_name?vector_name:NULL, buffer);

      /* In Coherence cluster & Coherence cache, we consider new vector only if obtained breadcrumb not present in cm info structure.
         If for any vector we are not able to generate breadcrumb immediately (i.e. started server signature) then we will ignore that particular vector, but in next progress report again we will follow this complete procedure & if in this progress report we get breadcrumb then will add this as new vector.  */
      //KJTSDB set bit for cm_coherence_cluster
      if((cus_mon_ptr->flags & NEW_FORMAT) && (cus_mon_ptr->flags & COHERENCE_CLUSTER))
      {
        MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                               "Received cohrence cluster vector %s. Going for processing.", vector_name); 
        process_coh_cluster_mon_vector(vector_name, breadcrumb, START_SERVER_SIG, cus_mon_ptr->nid_table_row_idx, cus_mon_ptr->server_index);
        if(breadcrumb[0] != '\0')
        {
          MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                               "For coherence cluster vector %s obtained breadcrumb %s.", vector_name, breadcrumb);
          //compare bredcrumb in custom monitor table, if not found then add this as new vector
          dyn_idx = find_dyn_vct_idx_using_breadcrumb(cus_mon_ptr, breadcrumb, cus_mon_ptr->num_dynamic_filled);
        }

        if((dyn_idx < 0) && (breadcrumb[0] == '\0'))        
          return 0;
      }
      //KJTSDB set single bit for all of below coherence monitors
      else if(((cus_mon_ptr->flags & COHERENCE_OTHERS) && (strstr(vector_name, "AllInstances") == NULL)) && (cus_mon_ptr->nid_table_row_idx >= 0))
      {
        MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                              " Received coherence cache, service or storage vector %s. Going for processing.", vector_name); 
        process_coh_cache_service_storage_mon_vector(vector_name, breadcrumb, cus_mon_ptr->gdf_name, cus_mon_ptr->nid_table_row_idx);
        if(breadcrumb[0] != '\0')
        {
          MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                              " For coherence cache or service vector %s obtained breadcrumb %s.", vector_name, breadcrumb);    
          //compare bredcrumb in custom monitor table, if not found then add this as new vector
          dyn_idx = find_dyn_vct_idx_using_breadcrumb(cus_mon_ptr, breadcrumb, cus_mon_ptr->num_dynamic_filled);
        }

        if((dyn_idx < 0) && (breadcrumb[0] == '\0'))
          return 0;
      }
      else
      { 
        char *ptr;
        //Data coming in format:   "Id:Vector_name|Data"
        if((ptr=strchr(vector_name,':'))!= NULL)
        { 
          if(*(ptr+1) == '\0' || vector_name[0] == ':')
          {
            MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                        " Received invalid vector %s", save_buf);
            return -1;
          }
          save=*ptr;
          *ptr = '\0';   //we needed only id to compare in ns_is_numeric().
        }

        if(ns_is_numeric((vector_name)))
          vectorid = atoi(vector_name);
	//KJTSDB Remove for all, as now LPS based monitor send vector id
        /*else
        {
          //For LPS based monitors data comes in old format, so need to handle it.
          if(cus_mon_ptr->flags & USE_LPS)
          {
            vectorid = -1;
            dyn_idx = find_dyn_vct_idx(cus_mon_ptr, vector_name, cus_mon_ptr->num_dynamic_filled);
          }
          else
          {
            MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         " Received invalid vector %s. Only a numeric value can be id before ':' in vector name.", save_buf);
            
            return -1;
          }
        }*/

        //if(vectorid > cus_mon_ptr->max_mapping_tbl_vector_entries)
        //check_and_realloc_dvm_mapping(vectorid, &(cus_mon_ptr->max_mapping_tbl_vector_entries), cus_mon_ptr->dvm_cm_mapping_tbl_row_idx);
	//KJTSDBif (!cus_mon_ptr->flags & DONTADDVECTOR)
        if(vectorid >= 0 && dvm_idx_mapping_ptr) 
        {
          if (vectorid >= cus_mon_ptr->max_mapping_tbl_vector_entries)
          {
            cus_mon_ptr->max_mapping_tbl_vector_entries = create_mapping_table_col_entries(cus_mon_ptr->dvm_cm_mapping_tbl_row_idx, vectorid, cus_mon_ptr->max_mapping_tbl_vector_entries);
          }

          if(dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].is_data_filled == 1) //old vector
            dyn_idx = dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].relative_dyn_idx;
          else
          {
           //This case will only come if vector gets deleted. But when it comes again it will be coming with id:vector_name|data. So it will have 'ptr'. Below statement will be executed only if we have ptr.
            if(! ptr)
            {
              MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Received vector for the first time or received vector first time after it is being deleted. So, expecting a vector_name to come along with the data.Hence, going to close the monitor connection. Vector Id = %d. Buffer = %s", vectorid ,save_buf);
              //if(cus_mon_ptr->fd != -1)
                //handle_monitor_stop(cus_mon_ptr, MON_STOP_ON_RECEIVE_ERROR);  
              return INVALID_VECTOR;
            }
            dyn_idx = handle_mapping_table_for_data_filling(ptr+1, cus_mon_ptr, vectorid);
          }
        }

        //Check if "init-vector-file" is used in json, so that migration can be done for old vector format that are present in init-vector-file with the new format vectors which will come during test.
	//TSDBKJ Set flag for cm_alert_log_stats.gdf
        if((dyn_idx == -1) && ((cus_mon_ptr->flags & ALERT_LOG_MONITOR) || cus_mon_ptr->init_vector_file_flag))
        {
          dyn_idx = mon_migration_for_alert(cus_mon_ptr, vectorid, ptr+1);
        }

        if(ptr)
          *ptr = save;
      }

      if(dyn_idx < 0) //This is the case when a new vector is found.
      {
        //monitor_runtime_changes_applied = 1;
        //return(add_new_vector(cus_mon_ptr, save_buf, vector_name, save_buf, max, breadcrumb)); //new vector with data
        //if(add_new_vector(cus_mon_ptr, save_buf, vector_name, save_buf, max, breadcrumb) >= 0); //new vector with data
        if(!(cus_mon_ptr->flags & USE_LPS) && (save != ':'))   //This case is not for LPS based monitors, as it comes in old format.
        {  
          MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Received invalid vector %s. Expected ':' for the vectors received for the first time."
                           "Going to close monitor connection.", save_buf);

          //if(cus_mon_ptr->fd != -1)
            //handle_monitor_stop(cus_mon_ptr, MON_STOP_ON_RECEIVE_ERROR);
          return INVALID_VECTOR;
        }
	//TSDBKJ we should not call this
	//TSDBKJ add flag in cm_ptr, initally of , on first add_cm_vector_info set the flag on
        val_ret = add_cm_vector_info_node(cus_mon_ptr->monitor_list_idx, vector_name, NULL, max, breadcrumb, 1, 1);
 
        if(val_ret >= 0)
        {
          monitor_runtime_changes_applied = 1;
          if(cus_mon_ptr->dvm_cm_mapping_tbl_row_idx > -1 && vectorid > -1)
            dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].relative_dyn_idx = cus_mon_ptr->total_vectors - 1 ;
        
          dyn_idx = cus_mon_ptr->total_vectors - 1;
        }
        else
          return val_ret;
      }
      NSDL3_MON(NULL, NULL, "vector %s found at dyn_idx %d", vector_name?vector_name:NULL, dyn_idx);
      //KJTSDBif (!cus_mon_ptr->flags & DONTADDVECTOR) ends
    }
    } 
    else //If data is coming without vector name
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Received invalid vector %s. Expected '|' for the vectors received", save_buf);
      return -1;
    }
    data = cus_mon_ptr->vector_list[dyn_idx].data; //Pointing after |
  }
  else
  //TSDBKJ, Need to move buffer lines below in else
  {
    dyn_idx=0;

  //cus_mon_ptr, max, buffer, 
  tmp_pipe_ptr = strchr(buffer, '|');
  if(tmp_pipe_ptr)
    buffer = tmp_pipe_ptr + 1; 
  
  }
  //TSDBKJ Buffer now pointing to Pipe(|) + 1

  //reset deleted flag
  if((dyn_idx >= 0) && (cus_mon_ptr->vector_list[dyn_idx].flags & RUNTIME_DELETED_VECTOR))
  {
    cus_mon_ptr->vector_list[dyn_idx].flags &= ~RUNTIME_DELETED_VECTOR;
    cus_mon_ptr->vector_list[dyn_idx].vector_state = CM_REUSED;
    if(!(cus_mon_ptr->flags & ALL_VECTOR_DELETED))
      check_deleted_vector(cus_mon_ptr, dyn_idx);

    monitor_runtime_changes_applied = 1;
  }


  //nd monitor migration change
  /*if(dyn_idx >= 0 && ((cus_mon_ptr + dyn_idx)->nd_mon_migration_flag == NEW_FORMAT_VECTOR))
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       " Migration: Received existing new format vector %s in old format", save_buf);
    //clear mapping
    (cus_mon_ptr->instanceVectorIdxTable)[(cus_mon_ptr + dyn_idx)->mappedVectorIdx][(cus_mon_ptr + dyn_idx)->instanceIdx]->reset_and_validate_flag |= ND_RESET_FLAG;
    //(cus_mon_ptr + dyn_idx)->vectorIdx = -1;
    //(cus_mon_ptr + dyn_idx)->instanceIdx = -1;
    //(cus_mon_ptr + dyn_idx)->tierIdx = -1;

    // NOTE: DATA OF NEW->OLD FORMAT VECTORS WILL BE FILLED IN MAPPING TABLE ONLY AND WE ARE NOT RESETTING ID'S BECAUSE WE ARE USING MAPPING TABLE DATA PTRS
   
    //set migration flag to OLD
  }*/

  if((monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic))
   val_ret = validate_and_fill_data(cus_mon_ptr, max, buffer, data,cus_mon_ptr->vector_list[dyn_idx].metrics_idx, &cus_mon_ptr->vector_list[dyn_idx].metrics_idx_found, (monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic) ? cus_mon_ptr->vector_list[dyn_idx].mon_breadcrumb: cus_mon_ptr->vector_list[dyn_idx].vector_name);
  else
    val_ret = validate_and_fill_data(cus_mon_ptr, max, buffer, cus_mon_ptr->vector_list[dyn_idx].data,cus_mon_ptr->vector_list[dyn_idx].metrics_idx, &cus_mon_ptr->vector_list[dyn_idx].metrics_idx_found, (monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic) ? cus_mon_ptr->vector_list[dyn_idx].mon_breadcrumb: cus_mon_ptr->vector_list[dyn_idx].vector_name);

  if(val_ret < 0)
  {
    NSDL2_MON(NULL, NULL, "validate_and_fill_data return -1");
  }
  else
  {
    // At this point, we have processed one line
    if (monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic) 
    {
      // We cannot add this check as we do not clear is_data_filled after we process it as we want to repeat same value in
      // progress report. Need to review this and change later
      // if((cus_mon_ptr + cus_mon_ptr->num_dynamic_filled)->is_data_filled)
      // MLTL1(EL_DE, 0, 0, _FLN_, cus_mon_ptr, 10167, "Duplicate data received. Latest data will be used"); more details
      // else
      {
        //(cus_mon_ptr + cus_mon_ptr->num_dynamic_filled)->is_data_filled = 1;
        cus_mon_ptr->vector_list[dyn_idx].flags |= DATA_FILLED;
        cus_mon_ptr->num_dynamic_filled++;
        // For dynamic vector monitor, once we get all lines, we reset num_dynamic_filled
        if (cus_mon_ptr->num_dynamic_filled >= cus_mon_ptr->total_vectors)
        {
          NSDL2_MON(NULL, NULL, "All data received. Resetting num_dynamic_filled");
          cus_mon_ptr->num_dynamic_filled = 0;
        }
      }
    } 
    else 
    {
      // if(cus_mon_ptr->is_data_filled)
      // MLTL1(EL_DE, 0, 0, _FLN_, cus_mon_ptr, 10167, "Duplicate data received. Latest data will be used");
      // else
      cus_mon_ptr->vector_list[0].flags |= DATA_FILLED;
    }
  }
  return 0;
}

//This function is used to check 'CAV_DATA_START' and 'CAV_DATA_END' header.
static int check_nv_mon_data_header(char *RecvMsg, CM_info *cus_mon_ptr)
{
  int ret = 0;
  int idx = 0;
  int vectorIdx = 0;
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;    

  if(strncmp(RecvMsg,"CAV_DATA_START",14) == 0)
  {
    ret=1;
    MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Method callled Start Receive Message = %s, GDF Name = %s", RecvMsg, cus_mon_ptr->gdf_name);
    if(cus_mon_ptr->nv_map_data_tbl)  
      cus_mon_ptr->nv_data_header |= NV_START_HEADER; //We sets the 2nd bit of nv_data_header if we get CAV_DATA_START header.
  } 
  else if(strncmp(RecvMsg,"CAV_DATA_END",12) == 0)
  {
    ret=1; 
    MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Method callled End Receive Message = %s, GDF Name = %s, Version - %s", RecvMsg,  cus_mon_ptr->gdf_name, (char*)(RecvMsg + 12));
    if(cus_mon_ptr->nv_map_data_tbl)    
    {
      if(cus_mon_ptr->nv_data_header & NV_START_HEADER)
      {
        cus_mon_ptr->nv_data_header &= ~(NV_START_HEADER); //We resets the 2nd  bit of nv_data_header if we get CAV_DATA_END header
        for(idx = 0; idx < cus_mon_ptr->total_vectors; idx++) 
        {   
          vectorIdx = vector_list[idx].vectorIdx;
          if(vectorIdx != -1)
            memcpy(cus_mon_ptr->nv_map_data_tbl->data[vectorIdx], cus_mon_ptr->nv_map_data_tbl->temp_data[vectorIdx], (cus_mon_ptr->no_of_element * sizeof(double)));
        }
      }
    }
  }
  return ret;  
}

//check the Recived msg and check for new line character.
static int
checkfornewline(CM_info *cus_mon_ptr)
{
  int ret; //, done = 0;
  char *data_ptr = cus_mon_ptr->data_buf;
  char *save_ptr = cus_mon_ptr->data_buf;
  char *line_ptr = data_ptr;
  int data_exceeded=0;

  NSDL2_MON(NULL, NULL, "Method called. RecvMsg = %s", data_ptr);
  MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
	                                        "cus_mon_ptr->dindex %d", cus_mon_ptr->dindex);
  // Process all lines received so far
  while( *data_ptr != '\0' )
  {
    // done = 0;
    if(*data_ptr == '\n') // Found one full line
    {
      *data_ptr = '\0'; // Replace new line by empty
      if (line_ptr != data_ptr) // Handle empty lines
      {
        cus_mon_ptr->data_buf=line_ptr;
        
       //Here we check for NV monitors. 
        NSDL3_MON(NULL, NULL, "Data Send to filldata  (%s) with length %d", line_ptr, data_ptr - line_ptr);
        if(cus_mon_ptr->nv_data_header & NV_INITIALIZE)
        {
          if(check_nv_mon_data_header(data_ptr, cus_mon_ptr) == 0)
          {
            //fill data.

	    ret = filldata(cus_mon_ptr, data_ptr - line_ptr);
            if(ret == INVALID_VECTOR)
	    {
              cus_mon_ptr->data_buf = save_ptr;
              return FAILURE;
	    }
          }     
        }  
        else 
	{
          ret = filldata (cus_mon_ptr, data_ptr - line_ptr);
	}

        if(ret == INVALID_VECTOR)
	{
              cus_mon_ptr->data_buf = save_ptr;
              return FAILURE;
	}
        // done = 1;
      }
      else
      {
        NSDL3_MON(NULL, NULL, "Received line is empty");
      }
     
      cus_mon_ptr->dindex = data_ptr - line_ptr;
      line_ptr=data_ptr+1;
    }
    data_ptr++;
  }
  //TODO move leftover in the filldata 

  //For NetCloud Monitors skip the below check
  if(cus_mon_ptr->cs_port != -1)
  {
    if(cus_mon_ptr->flags & NA_MSQL)
    {
      if ( cus_mon_ptr->dindex == g_mssql_data_buf_size) // We got msg with max size without any new line
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
                                  "Got much longer data (%d) than expected (%d). Ignoring this data",
				   cus_mon_ptr->dindex, g_mssql_data_buf_size);
        //cus_mon_ptr->dindex = 0; // Ignore this message
	data_exceeded =1;
        // return FAILURE;
      }
    }
    else
    {
      if ( cus_mon_ptr->dindex == MAX_MONITOR_BUFFER_SIZE) // We got msg with max size without any new line
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
  			   	  "Got much longer data (%d) than expected (%d). Ignoring this data",
				  cus_mon_ptr->dindex, MAX_MONITOR_BUFFER_SIZE);
        //cus_mon_ptr->dindex = 0; // Ignore this message
	data_exceeded =1;
        // return FAILURE;
      } /*  else  */
    }
  }

  if(*line_ptr != '\0' && data_exceeded == 0)
  {
    cus_mon_ptr->dindex  = data_ptr - line_ptr;
    NSDL3_MON(NULL, NULL, "Partital data Receieved data_ptr %s line_ptr %s", data_ptr, line_ptr);
    data_ptr=line_ptr;
    MLTL3(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
  			   	  "Partital data Receieved data_ptr  (%s) data_ptr(%s) d_index (%d)", save_ptr, data_ptr, cus_mon_ptr->dindex);
    cus_mon_ptr->data_buf = save_ptr;
    strcpy(cus_mon_ptr->data_buf, line_ptr);
  }
  else
  {
    cus_mon_ptr->data_buf = save_ptr;
    cus_mon_ptr->dindex  = 0;
  }


/*     return NO_LINE_PROCESSED; */

/*   /\* will not reach *\/ */
  return SUCCESS;
}

//Return SUCCESS if all OK else returns FAILURE
//Received msg from CS.
static int
receive(CM_info *cus_mon_ptr)
{ 
  char *RecvMsg;
  char nd_monitor_log_path[1024];
  nd_monitor_log_path[0] = '\0';
  char msg_buff[1024*1024 + 200];
  msg_buff[0] = '\0';
  char curr_time_buffer[100];
  //fd_set fdRcv;
  //struct timeval timeout;
  int  maxlen, log_mon_data = 0;
  int lengthrecv;
  //char *buffer = NULL;

  NSDL2_MON(NULL, NULL, "Method called. %s, source address = %s", cm_to_str(cus_mon_ptr), nslb_get_src_addr(cus_mon_ptr->fd));
  //maxlen is taken large because of big error msg

  //debug_log(DM_EXECUTION, MM_MESSAGES, _FL_, "receive", "Before receive data: fd is %d", cus_mon_ptr->fd);
  //printf("no_of_element = %d\n", cus_mon_ptr->no_of_element);
  //we are reading data till EAGAIN is coming, but if data is continuously comming then not handled this condition now.
  while(1)
  {
    if((!strncmp(cus_mon_ptr->gdf_name,"NA_mssql_", 9)))
      maxlen = g_mssql_data_buf_size - cus_mon_ptr->dindex;
    else
      maxlen =  MAX_MONITOR_BUFFER_SIZE - cus_mon_ptr->dindex;

    MLTL3(EL_DF, DM_METHOD, MM_MON, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
					 "cus_mon_ptr->dindex %d maxlen %d cus_mon_ptr->gdf_name %s", cus_mon_ptr->dindex, maxlen, cus_mon_ptr->gdf_name); 

    RecvMsg = cus_mon_ptr->data_buf + cus_mon_ptr->dindex; // Must set in loop as checkfornewline() set dindex
    NSDL3_MON(NULL, NULL, "Calling recv to read data");
    lengthrecv = recv(cus_mon_ptr->fd, RecvMsg, maxlen, 0);
    if(lengthrecv == 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_CRITICAL,
				     "Connection closed for %s",
				      cm_event_msg(cus_mon_ptr));

      return FAILURE;
    }

    if(lengthrecv < 0)
    {
      if (errno == EAGAIN) 
      {
        NSDL2_MON(NULL, NULL, "Received EAGAIN for Custom Monitor Vector Name '%s'",  cus_mon_ptr->monitor_name);
        return SUCCESS;
      }
      else if (errno == EINTR)
      {   /* this means we were interrupted */
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        continue;
      }
      else 
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_CRITICAL,
				       "Error in reading data from server for monitor %s"
				       "Error: recv(): errno %d (%s)",
				       cm_event_msg(cus_mon_ptr), errno, nslb_strerror(errno));
      
        return FAILURE;
      }
    }

    RecvMsg[lengthrecv] = 0;
    cus_mon_ptr->dindex += lengthrecv;
    MLTL3(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
	                                        "cus_mon_ptr->dindex %d lengthrecv %d", cus_mon_ptr->dindex, lengthrecv);

    //We are only calculating data throughput for non-LPS and non-ND monitors. LPS and ND are calculating their data throughput at their ends.
    if((global_settings->enable_health_monitor) && (!(cus_mon_ptr->flags & ND_MONITOR || cus_mon_ptr->flags & USE_LPS)))
      hm_data->total_data_throughput += lengthrecv;

    //Fixed bug 5690
    if(group_default_settings->module_mask & MM_MON)
    {
      MLTL1(EL_DF, DM_METHOD, MM_MON, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
					 "Received data in this read is (%s)", RecvMsg); 

      MLTL1(EL_DF, DM_METHOD, MM_MON, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
					 "Received data so far is (%s)", cus_mon_ptr->data_buf); 
    }

    if(g_log_mode)
    {
      if(strcasecmp(gdf_log_pattern, "all") == 0)
      {
        if( (cus_mon_ptr->flags & ND_MONITOR) && (!strstr(cus_mon_ptr->gdf_name, "cm_nd_db_query_stats.gdf")) && (!strstr(cus_mon_ptr->gdf_name, "cm_nd_entry_point_stats.gdf")) && (!strstr(cus_mon_ptr->gdf_name, "cm_nd_integration_point_status.gdf")))
          log_mon_data = 1;
      }
      else{
        if(strstr(cus_mon_ptr->gdf_name,gdf_log_pattern))
          log_mon_data = 1;
      }

      if(log_mon_data)
      {
        sprintf(msg_buff,"%s|%s|Receive Length: %d|%s",nslb_get_cur_date_time(curr_time_buffer, 1), cus_mon_ptr->gdf_name, lengthrecv, RecvMsg);
        if(g_nd_monLog_fd > 0)
          fprintf(g_nd_monLog_fd, "%s", msg_buff);
        else
        {
          sprintf(nd_monitor_log_path,"%s/logs/%s/ns_logs/monitor_data.log",g_ns_wdir, global_settings->tr_or_partition);
          g_nd_monLog_fd = fopen(nd_monitor_log_path, "a+");
          if(g_nd_monLog_fd)
          {
            fprintf(g_nd_monLog_fd, "%s", msg_buff);
	  }
          else
            MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Unable to open %s", nd_monitor_log_path);
         }
         fflush(g_nd_monLog_fd);
         log_mon_data = 0;
      }
    }
    

    int ret;
    ret = checkfornewline(cus_mon_ptr);
    if (ret == FAILURE) // Currently it is not returned but keep it for future
      return ret;
    continue;
  }
}

void close_custom_monitor_connection(CM_info *cus_mon_ptr)
{ 
  int close_ret = -2;
  int close_errno = -1;

  NSDL2_MON(NULL, NULL, "Method called. CustomMonitor => %s", cm_to_str(cus_mon_ptr));
  
  if(cus_mon_ptr->fd >= 0)
  {
    REMOVE_SELECT_MSG_COM_CON(cus_mon_ptr->fd, DATA_MODE);
    char tmp_buf[128];
    errno = 0;  //For testing only
    strcpy(tmp_buf, nslb_get_dest_addr(cus_mon_ptr->fd));
    NSDL3_MON(NULL, NULL, "Before calling close() - errno = %d, MonInfo = %s , src = [%s], des = [%s], tcp_info = [%s]", 
                           errno, cm_to_str(cus_mon_ptr), nslb_get_src_addr(cus_mon_ptr->fd), tmp_buf,
                           nslb_get_tcpinfo(cus_mon_ptr->fd));

    if((close_ret = close(cus_mon_ptr->fd)) < 0)
    {
      close_errno = errno;

      switch(close_errno)
      {
        case EBADF:
          NSDL3_MON(NULL, NULL, "fd isn't a valid open file descriptor. close() - errno = %d, MonInfo = %s , src = [%s],"
                                "des = [%s], tcp_info = [%s]", close_errno, cm_to_str(cus_mon_ptr), 
                                 nslb_get_src_addr(cus_mon_ptr->fd), tmp_buf, nslb_get_tcpinfo(cus_mon_ptr->fd));
          break;
        case EINTR:
          NSDL3_MON(NULL, NULL, "The close() call was interrupted by a signal. close() - errno = %d, MonInfo = %s ,"                                       "src = [%s], des = [%s], tcp_info = [%s]", close_errno, cm_to_str(cus_mon_ptr), 
                                 nslb_get_src_addr(cus_mon_ptr->fd), tmp_buf, nslb_get_tcpinfo(cus_mon_ptr->fd));
          break;
        case EIO: 
          NSDL3_MON(NULL, NULL, "An I/O error occurred. close() - errno = %d, MonInfo = %s , src = [%s],"
                                "des = [%s], tcp_info = [%s]", close_errno, cm_to_str(cus_mon_ptr), 
                                 nslb_get_src_addr(cus_mon_ptr->fd), tmp_buf, nslb_get_tcpinfo(cus_mon_ptr->fd));
          break;
        default: 
          NSDL3_MON(NULL, NULL, "Error: connection close by NS is failed for monitor name '%s' and for fd %d",
                                 cus_mon_ptr->monitor_name, cus_mon_ptr->fd);
          break;
      }
      MLTL1(EL_DF, DM_WARN1, MM_MON, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_CRITICAL,
                                          "Error: connection close by NS is failed for monitor name '%s' and for fd %d",
                                          cus_mon_ptr->monitor_name, cus_mon_ptr->fd); 
    }
    else if(close_ret == 0) 
    {
       NSDL3_MON(NULL, NULL, "Connection closed successfully by NS for monitor name '%s' and for fd %d",
                              cus_mon_ptr->monitor_name, cus_mon_ptr->fd);
    }

    strcpy(tmp_buf, nslb_get_dest_addr(cus_mon_ptr->fd));
    NSDL3_MON(NULL, NULL, "After calling close() - errno = %d, MonInfo = %s , src = [%s], des = [%s], tcp_info = [%s]", 
                           close_errno, cm_to_str(cus_mon_ptr), nslb_get_src_addr(cus_mon_ptr->fd), tmp_buf,
                           nslb_get_tcpinfo(cus_mon_ptr->fd));
    cus_mon_ptr->fd = -1;
  }
  else if(monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic)
  {
    //NSTL1_OUT(NULL, NULL, "This should not happen. FD wont be negative in this piece of code. cus_mon_ptr = %p, Gdf_name = %s, Vector_name = %s, Dyn_num_vectors = %d", &cus_mon_ptr, cus_mon_ptr->gdf_name, cus_mon_ptr->vector_name, cus_mon_ptr->dyn_num_vectors);
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "This should not happen. FD wont be negative in this piece of code. cus_mon_ptr = %p, Gdf_name = %s, Monitor_nators = %s,  = %d", &cus_mon_ptr, cus_mon_ptr->gdf_name, cus_mon_ptr->monitor_name, cus_mon_ptr->total_vectors);
  }
}


//Added By Manish for reseting data to 0 if monitor is stoping.
//Date:Fri Nov 11 16:56:10 IST 2011
int handle_monitor_stop(CM_info *cus_mon_ptr, int reason)
{
  int i,k, m = 0;
  int num_dynamic_vector;
  CM_info *save_parent_ptr = cus_mon_ptr;
  CM_info *cm_ptr = cus_mon_ptr;
  NSDL2_MON(NULL, NULL, "Method called, cus_mon_ptr = %p, reason = %d", cus_mon_ptr, reason);
  
  if(!(cus_mon_ptr->flags & OUTBOUND_ENABLED))
  {
    close_custom_monitor_connection(cus_mon_ptr);
  }
  else
  {
    //MS->cm_index will be -1 only if there is no entry for corresponding monitor is not in cm_info_ptr
    //mon_id_map_table[cus_mon_ptr->mon_id].cm_index = -1;
    if(mon_id_map_table[cus_mon_ptr->mon_id].state != DELETED_MONITOR && (reason == MON_DELETE_AFTER_SERVER_INACTIVE))
    {
      //for any_server we are marking state DELETED_MONITOR because when we get instance_up then we also get node discovery msg from ndc.then we can skip the monitors whose any_server is true.
      if(cus_mon_ptr->any_server)
        mon_id_map_table[cus_mon_ptr->mon_id].state = DELETED_MONITOR;
      else
        mon_id_map_table[cus_mon_ptr->mon_id].state = INACTIVE_MONITOR;
    }
    else
      mon_id_map_table[cus_mon_ptr->mon_id].state = DELETED_MONITOR;
  }

  //only in this case we will retry connection.
  if(reason == MON_STOP_ON_RECEIVE_ERROR && (!(cus_mon_ptr->flags & OUTBOUND_ENABLED)))
  {
    NSDL2_MON(NULL, NULL, "Adding cus_mon_ptr->monitor_name = %s into CM DR table", cus_mon_ptr->monitor_name);
    //not stopped normally hence setting conn state to abort.
    //make cm_id table point to index of CM_Info table for this monitor.
    update_health_monitor_sample_data(&hm_data->num_monitors_exited);
    cm_update_dr_table(cus_mon_ptr);
  }
  else if(reason == MON_STOP_ON_REQUEST)
    cus_mon_ptr->conn_state = CM_STOPPED;
  else if(reason == MON_DELETE_ON_REQUEST || reason == MON_DELETE_INSTANTLY || reason == MON_DELETE_AFTER_SERVER_INACTIVE)
  {
    cus_mon_ptr->conn_state = CM_DELETED;
    cus_mon_ptr->cm_retry_attempts = 0;
  
    //Need to take care when multiple deletion of same monitor occurs in same progress interval. It will be increased multiple times for a same monitor.
    update_health_monitor_sample_data(&hm_data->monitors_deleted);
  }

  cus_mon_ptr->no_log_OR_vector_format &= 0;
  cus_mon_ptr->no_log_OR_vector_format |= FORMAT_NOT_DEFINED;
  
  cus_mon_ptr->no_log_OR_vector_format |= LOG_ONCE;
  
  if (!(monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic)) //Case1: Monitor is not dynamic  
  {
    cus_mon_ptr->vector_list[0].flags &= ~DATA_FILLED; // Make is 0 so that progress report is not generated

    if(cus_mon_ptr->vector_list[0].data != NULL)
    {
      // set "nan" on monitor data connection break
      for(m = 0; m < cus_mon_ptr->no_of_element; m++)
      {
        cus_mon_ptr->vector_list[0].data[m] = 0.0/0.0;
      }
    }

    if(cus_mon_ptr->flags & ND_MONITOR)
    {
      //memset(cus_mon_ptr->data, 0, (cus_mon_ptr->no_of_element * sizeof(double))); //56 is 7(no of graphs) * sizeof(double)
      if(!(cus_mon_ptr->vector_list[0].flags & OVERALL_VECTOR) && (save_parent_ptr->instanceVectorIdxTable) && (cus_mon_ptr->vector_list[0].mappedVectorIdx >= 0) && (((save_parent_ptr->instanceVectorIdxTable)[cus_mon_ptr->vector_list[0].mappedVectorIdx][cus_mon_ptr->vector_list[0].instanceIdx]) != NULL))
      {
        (save_parent_ptr->instanceVectorIdxTable)[cus_mon_ptr->vector_list[0].mappedVectorIdx][cus_mon_ptr->vector_list[0].instanceIdx]->reset_and_validate_flag |= ND_RESET_FLAG;
        (save_parent_ptr->instanceVectorIdxTable)[cus_mon_ptr->vector_list[0].mappedVectorIdx][cus_mon_ptr->vector_list[0].instanceIdx]->reset_and_validate_flag &= ~GOT_ND_DATA;
      }
    }
    else   
    {
      //memset(cus_mon_ptr->data, 0, MAX_MONITOR_BUFFER_SIZE + 1);
    }

    if(reason == MON_DELETE_ON_REQUEST || reason == MON_DELETE_INSTANTLY || reason == MON_DELETE_AFTER_SERVER_INACTIVE)
    {
      if((cus_mon_ptr->vector_list[0].vector_state != CM_DELETED) && !(cus_mon_ptr->vector_list[0].flags & WAITING_DELETED_VECTOR) && !(cus_mon_ptr->flags & ALL_VECTOR_DELETED) && ((reason == MON_DELETE_ON_REQUEST) || (reason == MON_DELETE_AFTER_SERVER_INACTIVE)))
      {
        if(cus_mon_ptr->total_deleted_vectors >= cus_mon_ptr->max_deleted_vectors)
        {
          MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Monitor Name = %s, total_deleted_vectors = %d and max_deleted_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors); 
          create_entry_in_reused_or_deleted_structure(&cus_mon_ptr->deleted_vector, &cus_mon_ptr->max_deleted_vectors);
          MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "After create_entry_in_reused_or_deleted_structure Now total_entries = %d max_entries = %d", cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);
        }
        cus_mon_ptr->deleted_vector[cus_mon_ptr->total_deleted_vectors] = 0;
        cus_mon_ptr->total_deleted_vectors++; 
        if(!g_enable_delete_vec_freq)    //Feature disabled
        {
          cus_mon_ptr->vector_list[0].flags |= RUNTIME_DELETED_VECTOR;
          cus_mon_ptr->vector_list[0].vector_state = CM_DELETED;
	    if(!(g_tsdb_configuration_flag & RTG_MODE) && (cus_mon_ptr->vector_list[0].metrics_idx_found == 1))
	    {
              ns_tsdb_delete_metrics_by_id(cus_mon_ptr, cus_mon_ptr->vector_list[0].metrics_idx);
              cus_mon_ptr->vector_list[0].metrics_idx_found = 0;
	    }
        }
        else
        {
          total_waiting_deleted_vectors++;
          cus_mon_ptr->vector_list[0].flags |= WAITING_DELETED_VECTOR;
        }
          
        total_deleted_vectors++;
        cus_mon_ptr->vector_list[0].flags |= RUNTIME_RECENT_DELETED_VECTOR;
      }
    }
  }
  else  //Case2: Monitor is dynamic
  {
    num_dynamic_vector = cus_mon_ptr->total_vectors;
    CM_vector_info *vector_list = cus_mon_ptr->vector_list;
    //changed
    if(cus_mon_ptr->flags & NV_MONITOR)
    {
      if(cus_mon_ptr->dummy_data != NULL)
      {
        cus_mon_ptr->no_log_OR_vector_format |= DEFAULT_LOG;
        for (k = 0; k < num_dynamic_vector; k++, cm_ptr++)
        {
          // set "nan" on monitor data connection break
          for(m = 0; m < cus_mon_ptr->no_of_element; m++)
          {
            vector_list[k].data[m] = 0.0/0.0;            
          }

          vector_list[k].vectorIdx = -1;

          if(reason == MON_DELETE_ON_REQUEST || reason == MON_DELETE_AFTER_SERVER_INACTIVE)
          {
            if((vector_list[k].vector_state != CM_DELETED) && !(cus_mon_ptr->flags & ALL_VECTOR_DELETED) && !(vector_list[k].flags & WAITING_DELETED_VECTOR))
            {
              if(cus_mon_ptr->total_deleted_vectors >= cus_mon_ptr->max_deleted_vectors )
              {
                MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "Monitor Name: %s, total_deleted_vectors = %d and max_deleted_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);                 

                create_entry_in_reused_or_deleted_structure(&(cus_mon_ptr->deleted_vector), &cus_mon_ptr->max_deleted_vectors);
                MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "After create_entry_in_reused_or_deleted_structure Now total_entries = %d max_entries = %d", cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);
              }
              cus_mon_ptr->deleted_vector[cus_mon_ptr->total_deleted_vectors] = k;
              cus_mon_ptr->total_deleted_vectors++;
              vector_list[k].flags |= RUNTIME_RECENT_DELETED_VECTOR;
              if(!g_enable_delete_vec_freq)    //Feature disabled
      	      {
                vector_list[k].flags |= RUNTIME_DELETED_VECTOR;  
                vector_list[k].vector_state = CM_DELETED;
		  if(!(g_tsdb_configuration_flag & RTG_MODE) && (vector_list[k].metrics_idx_found == 1))
		  {
                    ns_tsdb_delete_metrics_by_id(cus_mon_ptr, vector_list[k].metrics_idx);
                    vector_list[k].metrics_idx_found = 0; 
		  }
              }
              else
              {
                total_waiting_deleted_vectors++;
                vector_list[k].flags |= WAITING_DELETED_VECTOR;
              }
	      
              total_deleted_vectors++;
            }
          }
        }

        if(reason == MON_DELETE_ON_REQUEST)
          cus_mon_ptr->conn_state = CM_DELETED;

        FREE_AND_MAKE_NULL(cus_mon_ptr->vectorIdxmap->idxMap, "cus_mon_ptr->vectorIdxmap->idxMap", -1);
        cus_mon_ptr->vectorIdxmap->mapSize = 0;
        cus_mon_ptr->vectorIdxmap->maxAssValue = 1;
      }
    }
    else
    {
      cus_mon_ptr->no_log_OR_vector_format &= 0;
      cus_mon_ptr->no_log_OR_vector_format |= FORMAT_NOT_DEFINED;

      cus_mon_ptr->no_log_OR_vector_format |= LOG_ONCE;


      for(i = 0; i <num_dynamic_vector ; i++)
      {
        vector_list[i].flags &= ~DATA_FILLED;
        if(vector_list[i].data != NULL)
        {
          // set "nan" on monitor data connection break
          for(m = 0; m < save_parent_ptr->no_of_element; m++)
          {
            vector_list[i].data[m] = 0.0/0.0;            
          }

          if(cus_mon_ptr->flags & ND_MONITOR)
          {
            //memset(cus_mon_ptr->data, 0, (cus_mon_ptr->no_of_element * sizeof(double)));
            if(!(vector_list[i].flags & OVERALL_VECTOR) && (save_parent_ptr->instanceVectorIdxTable) && (vector_list[i].mappedVectorIdx >= 0) && (((save_parent_ptr->instanceVectorIdxTable)[vector_list[i].mappedVectorIdx][vector_list[i].instanceIdx]) != NULL))
            {
              (save_parent_ptr->instanceVectorIdxTable)[vector_list[i].mappedVectorIdx][vector_list[i].instanceIdx]->reset_and_validate_flag |= ND_RESET_FLAG;
              (save_parent_ptr->instanceVectorIdxTable)[vector_list[i].mappedVectorIdx][vector_list[i].instanceIdx]->reset_and_validate_flag &= ~GOT_ND_DATA;
          
              cus_mon_ptr->no_log_OR_vector_format |= LOG_ONCE;
              //Data for this monitor is generated by NDC. So if connection from NDC is closed, after re-connecting ids may change. So need to reset ids and table. 
              if(strstr(cus_mon_ptr->gdf_name, "cm_nd_bt_percentile.gdf"))
              {
                vector_list[i].data = nan_buff;
                vector_list[i].vectorIdx = -1;
                vector_list[i].instanceIdx = -1;
                vector_list[i].tierIdx = -1;
                vector_list[i].mappedVectorIdx = -1;
              }         
            }
          }
          else  
          {
            //memset(cus_mon_ptr->data, 0, MAX_MONITOR_BUFFER_SIZE + 1);
          }
        }

        if(reason == MON_DELETE_ON_REQUEST || reason == MON_DELETE_INSTANTLY || reason == MON_DELETE_AFTER_SERVER_INACTIVE)
        {
          if((vector_list[i].vector_state != CM_DELETED) && !(vector_list[i].flags & WAITING_DELETED_VECTOR) && !(cus_mon_ptr->flags & ALL_VECTOR_DELETED) && ((reason == MON_DELETE_ON_REQUEST) || (reason == MON_DELETE_AFTER_SERVER_INACTIVE)))
          {
            if(cus_mon_ptr->total_deleted_vectors >= cus_mon_ptr->max_deleted_vectors)
            {
              MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "Monitor Name: %s, total_deleted_vectors = %d and max_deleted_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);
              create_entry_in_reused_or_deleted_structure(&(cus_mon_ptr->deleted_vector), &cus_mon_ptr->max_deleted_vectors);
              MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "Monitor Name: %s, total_deleted_vectors = %d and max_deleted_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);
            }
            cus_mon_ptr->deleted_vector[cus_mon_ptr->total_deleted_vectors] = i;
            cus_mon_ptr->total_deleted_vectors++;
            vector_list[i].flags |= RUNTIME_RECENT_DELETED_VECTOR;
            if(!g_enable_delete_vec_freq)    //Feature disabled
            {
              vector_list[i].flags |= RUNTIME_DELETED_VECTOR;
              vector_list[i].vector_state = CM_DELETED;
	        if(!(g_tsdb_configuration_flag & RTG_MODE) && (vector_list[i].metrics_idx_found == 1))
                {
                  ns_tsdb_delete_metrics_by_id(cus_mon_ptr, vector_list[i].metrics_idx);
                  vector_list[i].metrics_idx_found = 0;
                }
            }
            else
            {
              vector_list[i].flags |= WAITING_DELETED_VECTOR;
              total_waiting_deleted_vectors++; 
            }

	    total_deleted_vectors++;
          }
          cus_mon_ptr->conn_state = CM_DELETED;
        }
        else
        {
          //Marked reset so that mapping table will not be filled. It willbe filled when data comes. 
          //No need to mark it RESET if it is already deleted. Refer BUG: 65236
          if((reason == MON_STOP_ON_RECEIVE_ERROR) && (vector_list[i].vector_state != CM_DELETED) && !(vector_list[i].flags & WAITING_DELETED_VECTOR)) 
            vector_list[i].vector_state = CM_VECTOR_RESET;

          if(vector_list[i].vectorIdx == -1)
            MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "vectorIdx is -1 for cus_mon_ptr[ %p ] with vector name [ %s ]",&(vector_list[i]), vector_list[i].vector_name); 
          //reset
          if((save_parent_ptr->dvm_cm_mapping_tbl_row_idx != -1) && (vector_list[i].vectorIdx != -1))
            dvm_idx_mapping_ptr[save_parent_ptr->dvm_cm_mapping_tbl_row_idx][vector_list[i].vectorIdx].is_data_filled = -1;
        }        
      }

      if((reason == MON_DELETE_ON_REQUEST || reason == MON_DELETE_INSTANTLY || reason == MON_DELETE_AFTER_SERVER_INACTIVE || reason == MON_STOP_ON_RECEIVE_ERROR) && (save_parent_ptr->dvm_cm_mapping_tbl_row_idx != -1))
      { 
        memset(dvm_idx_mapping_ptr[save_parent_ptr->dvm_cm_mapping_tbl_row_idx], -1, save_parent_ptr->max_mapping_tbl_vector_entries * sizeof(DvmVectorIdxMappingTbl));
          
        if(strstr(cm_ptr->gdf_name,"cm_hm_ns_tier_server_count.gdf") != NULL)
           g_tier_index_send = -1;


        MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "gdf_name = %s monitor name = %s , bytes_memset = %d max mapping table entried  = %d", cus_mon_ptr->gdf_name, cus_mon_ptr->monitor_name, (save_parent_ptr->max_mapping_tbl_vector_entries * sizeof(DvmVectorIdxMappingTbl)) , save_parent_ptr->max_mapping_tbl_vector_entries);

      }
    }
  }

  return 0;
}



//Added By Manish on Date:Wed Nov  9 12:12:10 IST 2011
//This function will stip only one monitor at a time
int stop_one_custom_monitor(CM_info *cus_mon_ptr, int reason)
{
  char buffer[2048]="end_monitor\n";

  NSDL2_MON(NULL, NULL, "Method called");

  if(cus_mon_ptr->flags & OUTBOUND_ENABLED)
  {
    sprintf(buffer, "nd_data_req:action=stop_monitor;server=%s;mon_id=%d;msg=end_monitor\n", cus_mon_ptr->server_display_name, cus_mon_ptr->mon_id);
   
    //Monitors has to be marked deleted even if NDC is not connected. Because if it is called from end_mon() then it will go on and add new monitor even if end_monitor message is not sent to NDC.
    //mark_deleted_in_server_structure(cus_mon_ptr->server_index, cus_mon_ptr->mon_id, reason);
    mark_deleted_in_server_structure(cus_mon_ptr, reason);
    if(ndc_data_mccptr.state & NS_CONNECTED)
    {
      NSDL2_MON(NULL, NULL, "cus_mon_ptr->fd = %d, message = %s", cus_mon_ptr->fd, buffer);
      MLTL3(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "Going to send End test-run message. FD= %d, state = %d, MSG = %s", ndc_data_mccptr.fd, ndc_data_mccptr.state, buffer);
      if(write_msg(&ndc_data_mccptr, (char *)&buffer, strlen(buffer), 0, DATA_MODE) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in sending message to NDC. So adding this message to pending buffer to process later.");
        make_pending_messages_for_ndc_data_conn(buffer, strlen(buffer));
        return -1;
      }
    }
    else
    {
      //we are appending the message to pending_messages_for_ndc_data_conn and will send it later when data connection from NDC is created.
      make_pending_messages_for_ndc_data_conn(buffer, strlen(buffer));
     MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Could not sent Stop message to NDC as it was not in CONNECTED state. cus_mon_ptr->fd = %d, message = %s", cus_mon_ptr->fd, buffer);
      NSDL1_MON(NULL, NULL, "Could not sent Stop message to NDC as it was not in CONNECTED state. cus_mon_ptr->fd = %d, message = %s", cus_mon_ptr->fd, buffer);
      return -1;
    }
  }
  else if(cus_mon_ptr->fd != -1) 
  {
    //We donot need to send end_monitor message to cmon for monitor registered on NSPort.
    if(cus_mon_ptr->is_monitor_registered != MONITOR_REGISTRATION)
    {
      NSDL2_MON(NULL, NULL, "cus_mon_ptr->fd = %d, message = %s", cus_mon_ptr->fd, buffer);
      if (send(cus_mon_ptr->fd, buffer, strlen(buffer), 0) != strlen(buffer))
      {
        MLTL1(EL_D, DM_WARN1, MM_MON, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_CRITICAL,
                                            "Error in sending end_monitor message for custom monitor"
                                            " monitor name '%s' to cav mon server '%s'",
                                            cus_mon_ptr->monitor_name, cus_mon_ptr->cs_ip);
        return -1;
      }
    }
  }
  handle_monitor_stop(cus_mon_ptr, reason);

  NSDL2_MON(NULL, NULL, "Method End.");

  return 0;
}


//This method is to stop connection of all custom monitor at end of test run.
//To stop connection of custom monitor send 'end_monitor' message to create server(cav mon server)
inline void stop_all_custom_monitors()
{
  int mon_idx;

  NSDL1_MON(NULL, NULL, "Method called");
  for (mon_idx = 0; mon_idx < total_monitor_list_entries; mon_idx++)
  {
    //Stop one custome monitor
    stop_one_custom_monitor(monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr, MON_STOP_NORMAL);
  }
}


//Added By Manish: for selecting fd for custom monitor
int add_select_custom_monitor_one_mon(CM_info *cus_mon_ptr)
{ 
  NSDL1_MON(NULL, NULL, "Method called");
  
  NSDL2_MON(NULL, NULL, "cus_mon_ptr->pgm_path = %s fd = %d", cus_mon_ptr->pgm_path, cus_mon_ptr->fd);
  if(cus_mon_ptr->fd > 0 ) 
  {
    //connection already established or monitor is running then add fd for reading only
    if(cus_mon_ptr->conn_state == CM_CONNECTED || cus_mon_ptr->conn_state == CM_RUNNING  || cus_mon_ptr->conn_state == CM_VECTOR_RECEIVED)
    {  
      ADD_SELECT_MSG_COM_CON((char *)cus_mon_ptr, cus_mon_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
    }
    //connection is in progress or connection is established but partial data sent then add fd for OUT event only
    else if(cus_mon_ptr->conn_state == CM_CONNECTING || cus_mon_ptr->conn_state == CM_SENDING)
      ADD_SELECT_MSG_COM_CON((char *)cus_mon_ptr, cus_mon_ptr->fd, EPOLLOUT | EPOLLERR | EPOLLHUP, DATA_MODE);
  }
  return 0;
}

void add_select_custom_monitor()
{
  int mon_idx;

  NSDL2_MON(NULL, NULL, "Method called");

  for (mon_idx = 0; mon_idx < total_monitor_list_entries; mon_idx++) {
    
    if(monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr->is_monitor_registered == MONITOR_REGISTRATION)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not adding monitor(%s) in epoll, This is a component health monitor", monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr->monitor_name);
      continue;
    }

    if(add_select_custom_monitor_one_mon(monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr) < 0)
      NS_EXIT(-1, CAV_ERR_1060026);
  }
}

//Added By Manish
//Date: Tue Nov  8 12:17:50 IST 2011
inline void make_connection_for_one_mon(CM_info *cus_mon_ptr, char *err_msg)
{
  NSDL2_MON(NULL, NULL, "Method called. cus_mon_ptr = %p", cus_mon_ptr);    
  int s_port = 7891;
  char s_ip[512] = "\0";

  /* If monitor is SPECIAL_MONITOR/LOG_MONITOR/AccessLogStats then
   * will make connection with LPS Agent.*/

  CM_SET_AGENT(cus_mon_ptr, s_ip, s_port);
  NSDL3_MON(NULL, NULL, "Connect with server = %s and port = %d", s_ip, s_port);

  cus_mon_ptr->fd = nslb_tcp_client_ex(s_ip, s_port, conn_timeout, err_msg);

  if(cus_mon_ptr->fd < 0)
  {
    NSDL3_MON(NULL, NULL, "Error: in making Connection. cus_mon_ptr->fd = %d.", cus_mon_ptr->fd);
    sprintf(err_msg, "Error: in making Connection for %s", cm_to_str(cus_mon_ptr));
  }
  else
    NSDL3_MON(NULL, NULL, "Connection made. cus_mon_ptr->fd = %d", cus_mon_ptr->fd);
}

static void make_nb_connections(int frequency)
{
  int monitor_list_id, ret;
  CM_info *cm_info_mon_conn_ptr = NULL;
  char err_msg[5*1024]="\0";

  NSDL3_MON(NULL, NULL, "Method called, frequency = %d, total_monitor_list_entries = %d", frequency, total_monitor_list_entries);

  for (monitor_list_id = 0; monitor_list_id < total_monitor_list_entries; monitor_list_id++)
  {
    cm_info_mon_conn_ptr = monitor_list_ptr[monitor_list_id].cm_info_mon_conn_ptr;
    if(cm_info_mon_conn_ptr->cs_port == -1)    //Incase of NetCloud monitors
    {
      NSDL3_MON(NULL, NULL, "For NetCloud monitors connection will not be made.");
      continue;
    }

    //is_product_health_monitor is used for PRODUCT_HEATLH_MONITOR
    if(cm_info_mon_conn_ptr->is_monitor_registered == MONITOR_REGISTRATION)
      continue;
    if(topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].used_row != -1)
    {
    //if(servers_list[cm_info_mon_conn_ptr->server_index].cntrl_conn_state & CTRL_CONN_ERROR)
      if(topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
      {
        MLTL1(EL_F, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,"This monitor return Unknown server error from gethostbyname,therefore retry has been skipped, changing its retry count to 0 and state to stopped");
        cm_info_mon_conn_ptr->cm_retry_attempts = 0;
        cm_info_mon_conn_ptr->conn_state = CM_STOPPED;
        continue;
      }
    }

    //making non blocking connection
    ret = cm_make_nb_conn(cm_info_mon_conn_ptr, 0);
    if (ret < 0)
    {
      if (!(global_settings->continue_on_mon_failure))
      {
        MLTL1(EL_F, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                                              "%s"
                                              " Custom/Standard monitor - %s(%s) failed. Test run Canceled",
                                              err_msg, get_gdf_group_name(cm_info_mon_conn_ptr->gp_info_index), cm_info_mon_conn_ptr->monitor_name);
        NSDL3_MON(NULL, NULL, "Connection making failed for monitor %s(%s). So exitting.", get_gdf_group_name(cm_info_mon_conn_ptr->gp_info_index), cm_info_mon_conn_ptr->monitor_name);
        MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Connection making failed for monitor %s(%s). So exitting.", get_gdf_group_name(cm_info_mon_conn_ptr->gp_info_index), cm_info_mon_conn_ptr->monitor_name);
        
        NS_EXIT(-1, CAV_ERR_1060027, get_gdf_group_name(cm_info_mon_conn_ptr->gp_info_index), cm_info_mon_conn_ptr->monitor_name);
      }
      MLTL1(EL_F, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                                              "%s"
                                              " Data will not be available for this monitor.",
                                              err_msg);
      NSDL3_MON(NULL, NULL, "Connection making failed on Starting of test so Retry later to make connection");
      //TODO: Uncomment this
      //cm_update_dr_table(cm_info_mon_conn_ptr);

       CLOSE_FD(cm_info_mon_conn_ptr->fd);

       continue;
    }
  }
}

// setting up the connection with CS (Create Server).
void custom_monitor_setup(int frequency)
{
  int m=0;
  NSDL1_MON(NULL, NULL, "Method called");

  for(m = 0; m < 128; m++)
    nan_buff[m] = 0.0/0.0;

  make_nb_connections(frequency);

  NSDL1_MON(NULL, NULL, "Method End.");
}

#if 0
// set all fds for custom monitor.
int set_custom_monitor_fd (fd_set * rfd, int max_fd)
{
  int custom_mon_id;
  CM_info *cus_mon_ptr = cm_info_ptr;

  for (custom_mon_id = 0; custom_mon_id < total_cm_entries; custom_mon_id++)
  {
    if(cus_mon_ptr[custom_mon_id].fd > 0)
    {
      //printf("setting fd = %d for Custom mon=%d\n", cus_mon_ptr[custom_mon_id].fd, custom_mon_id);
      FD_SET (cus_mon_ptr[custom_mon_id].fd, rfd);
      if(cus_mon_ptr[custom_mon_id].fd > max_fd)  max_fd = cus_mon_ptr[custom_mon_id].fd;
    }
  }
  return max_fd;
}
#endif

//Check if any of the fd in monTable is set in the rfd set.
//If set, receive the message. This message will be
//the data message as explained task2.  Read comple message
//till newline character.
//convert all data elements into numbers and store in the
//data array in monTable and set is_data_filled flag for the
//corresponding row
//void handle_if_custom_monitor_fd(fd_set *rfd)
int handle_if_custom_monitor_fd(void *ptr)
{
  CM_info *cus_mon_ptr = (CM_info *) ptr;

  //Commented as it fills debug log file.
  NSDL2_MON(NULL, NULL, "Method called, control connection cus_mon_ptr = %p", cus_mon_ptr);
  
  if(receive((cus_mon_ptr)) == FAILURE)
  {
    handle_monitor_stop(cus_mon_ptr, MON_STOP_ON_RECEIVE_ERROR);
  }

  return 1;
}

#if 0
//This method use to print cm mon data for testing 
void fill_cm_data()		
{
  int custom_mon_id;
  CM_info *cus_mon_ptr = cm_info_ptr;
  FILE *fd = fopen("/tmp/cm_data.log", "a+");
  int i;
  int num_graph  = 20;
  static int count = 1;

  fprintf(fd, "%d:---------------------------------------------------------------------------------------------------\n", count++);
  NSDL2_MON(NULL, NULL, "Method called");
  for (custom_mon_id = 0; custom_mon_id < total_cm_entries; custom_mon_id++, cus_mon_ptr++) {
    fprintf(fd, "IDX %d: [", custom_mon_id);
    for (i = 0; i < num_graph; i++) {
      fprintf(fd, "%0.0f ", cus_mon_ptr->data[i]);
    }
    fprintf(fd, "]\n");
  }
  CLOSE_FP(fd);
}
#endif

void fill_cm_data()
{
  int mon_id;
  
  NSDL1_MON(NULL, NULL, "Method called");
  for (mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
  {
    NSDL3_MON(NULL, NULL, "total_monitor_list_entries = %d, mon_id = %d, Address of monitor_list_ptr[mon_id] = %p", 
                           total_monitor_list_entries, mon_id, &(monitor_list_ptr[mon_id]));

    if(!strncmp(monitor_list_ptr[mon_id].gdf_name, "NA", 2))
      continue;
    if(monitor_list_ptr[mon_id].cm_info_mon_conn_ptr->gp_info_index < 0)
      continue;

    if (monitor_list_ptr[mon_id].no_of_monitors) {
      fill_cm_data_ptr(mon_id);
      /* In case of group vector cm, fill_cm_data_ptr() fills data for all cus_mon related to this cus mon, so skip these cust mon. */
      // in case of dyn mon, group_num_vectors is sum of dyn vecotrs of same GDF
      // Check for "is data filled" is in this method
      NSDL3_MON(NULL, NULL, "Number of monitors with same gdf = %d", monitor_list_ptr[mon_id].no_of_monitors);
      mon_id += (monitor_list_ptr[mon_id].no_of_monitors - 1);

    } else { // if(cus_mon_ptr->is_data_filled) // Check is added in fill_cm_data_ptr() so removed this check in 3.8.5
      fill_cm_data_ptr(mon_id);
    }
  }
   
  g_check_nd_overall_to_delete = 0; 
  NSDL1_MON(NULL, NULL, "Method End.");
}

//---------------------------- End Filling Part ---------------------------

//-----------------------------Printing Data ------------------------------

// Note : We are not resetting is_data_filled as we are sending data to RTG GUI
//        with whatever data is filled or not filled...

void print_cm_data(FILE * fp1, FILE* fp2)
{
  int mon_id;
  
  NSDL2_MON(NULL, NULL, "Method called");
  
  for (mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
  {
    //TODO add this log for vectors
   
    if(!strncmp(monitor_list_ptr[mon_id].gdf_name, "NA", 2))
      continue;
   
    if((monitor_list_ptr[mon_id].cm_info_mon_conn_ptr)->gp_info_index < 0)
      continue;

    if (monitor_list_ptr[mon_id].no_of_monitors){
      // Check if all vector data came for dynamic vector monitor
      if (monitor_list_ptr[mon_id].is_dynamic){
        // If num_dynamic_filled is not 0, then we did not get all lines
        // This happens when we  get few lines before and few after the time we are filling data
        // In this case, data for some vectors will be of previus sample
        // It depends on time at which it is send by monitor and very likely to happen
        // So log to debug log only
        if((monitor_list_ptr[mon_id].cm_info_mon_conn_ptr)->num_dynamic_filled != 0)
          NSDL3_MON(NULL, NULL, "All vector data lines not received for dynamic vector '%s'. Received lines = %d", (monitor_list_ptr[mon_id].cm_info_mon_conn_ptr)->monitor_name, (monitor_list_ptr[mon_id].cm_info_mon_conn_ptr)->num_dynamic_filled);
      }
      print_cm_data_ptr(mon_id, fp1, fp2); // is_data_filled check is in this method
      /* Skip the rest. */
      // In case of dynamic mon, all mons with same GDF will be printed together as we keep group_num_vectors as sum of
      // all dyn vectors for all with same GDF
      mon_id += (monitor_list_ptr[mon_id].no_of_monitors - 1);
    }
    else { // if(cus_mon_ptr->is_data_filled) // Check is also in print_cm_data_ptr, so removed from here in 3.8.5
      print_cm_data_ptr(mon_id, fp1, fp2);
    }
  }
}

//write in gui.data
void log_custom_monitor_gp()
{
  int custom_mon_id;
  CM_info *cm_ptr = NULL;

  NSDL2_MON(NULL, NULL, "Method called"); 
  for (custom_mon_id = 0; custom_mon_id < total_monitor_list_entries; custom_mon_id++)
  { 
    cm_ptr = monitor_list_ptr[custom_mon_id].cm_info_mon_conn_ptr;
    if(!strcmp(cm_ptr->gdf_name, "NA"))
      continue;
    if((cm_ptr->vector_list[0].flags & DATA_FILLED) && cm_ptr->gp_info_index >=0)
    {
      if(cm_ptr->access == RUN_LOCAL_ACCESS)
        log_cm_gp_data_ptr(custom_mon_id, cm_ptr->gp_info_index, cm_ptr->num_group, cm_ptr->print_mode, cm_ptr->cs_ip);
      if(cm_ptr->access == RUN_REMOTE_ACCESS)
        log_cm_gp_data_ptr(custom_mon_id, cm_ptr->gp_info_index, cm_ptr->num_group, cm_ptr->print_mode, cm_ptr->rem_ip);
    }
    custom_mon_id +=  monitor_list_ptr[custom_mon_id].no_of_monitors;
  }
}
//----------------------------- End Printing Data ------------------------------

//This method to create table entry
//On success row num contains the newly created row-index of table
int create_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name)
{
  NSDL2_MON(NULL, NULL, "Method called, total = %d, max = %d, size = %d, name = %s", *total, *max, size, name);
  if (*total == *max)
  {
    MY_REALLOC(*ptr, (*max + DELTA_CUSTOM_MONTABLE_ENTRIES) * size, name, -1);
    *max += DELTA_CUSTOM_MONTABLE_ENTRIES;
  }
  *row_num = (*total)++;

  NSDL2_MON(NULL, NULL, "row_num = %d, total = %d, max = %d, ptr = %p, size = %d, name = %s",*row_num, *total, *max, ptr, size, name);
  return 0;
}

int create_table_entry_ex(int *row_num, int *total, int *max, char **ptr, int size, char *name)
{
  NSDL2_MON(NULL, NULL, "Method called");
  if (*total == *max)
  {
    MY_REALLOC_AND_MEMSET_EX(*ptr, (*max + DELTA_CUSTOM_MONTABLE_ENTRIES) * size, (*max) * size, name, -1);
    *max += DELTA_CUSTOM_MONTABLE_ENTRIES;
  }
  //*row_num = (*total)++;
  //if(global_settings->debug) 
    NSDL(NULL, NULL, DM_EXECUTION, MM_MISC, "row_num = %d, total = %d, max = %d, ptr = %p, size = %d, name = %s", (*row_num) + 1, (*total) + 1, *max, &ptr, size, name);
  return 0;
}

//function to get ip for custom montor : called from ns_gdf.c
/*char *get_cm_server(unsigned int index)
{
  CM_info *cus_mon_ptr = cm_info_ptr + index;

  NSDL2_MON(NULL, NULL, "Method called. index=%u", index);
  if(cus_mon_ptr->access == RUN_LOCAL_ACCESS)
     return(cus_mon_ptr->cs_ip); 
  else   
     return(cus_mon_ptr->rem_ip);
}*/

// This method is called before dynamic vector monitors are added in the array
/*int cm_group_is_vector(char *gdf_name)
{
  int i;
  for (i = 0; i < total_cm_entries; i++) {
    if (strcmp(cm_info_ptr[i].gdf_name, gdf_name) == 0) {
      return cm_info_ptr[i].is_group_vector;
    }
  }
  return 0;
}*/

// This method is called before dynamic vector monitors are added in the array
//TODO MSR -> Optimize break when next gdf is different after match
int cm_get_num_vectors(char *gdf_name, int mon_idx)
{
  int cnt = 0;
  int i;

  for (i = mon_idx; i < total_monitor_list_entries; i++) {
    if (strcmp(monitor_list_ptr[i].gdf_name, gdf_name) == 0)
      cnt += monitor_list_ptr[i].cm_info_mon_conn_ptr->total_vectors;
    else
      break;
  }

  return cnt;
}

void  set_no_of_monitors_for_tsdb(int *flag)
{
    //No monitor applied
  if(total_monitor_list_entries == 0)
    return;
   int temp_value;
   int count;
   int mon_idx = 0, local_mon_idx = 0, found_remove_monitor = 0, tmp_monitor_list_entries = total_monitor_list_entries;
   monitor_list_ptr[0].no_of_monitors = 0;    //to reset the no of monitors for first monitor rest will be reset inside the loop

   tmp_monitor_list_entries = total_monitor_list_entries;
   for (mon_idx = 0; mon_idx < total_monitor_list_entries;mon_idx=local_mon_idx)
   {
     NSDL2_MON(NULL, NULL, "mon_idx %d, GDF Name %s, is_dynamic %d", mon_idx, monitor_list_ptr[mon_idx].gdf_name,
	 monitor_list_ptr[mon_idx].is_dynamic);
     temp_value = mon_idx;
     //count = 0;
     while(local_mon_idx < total_monitor_list_entries)
     {
       count = 0;
       if(monitor_list_ptr[local_mon_idx].cm_info_mon_conn_ptr != NULL) //check if cm_info is not made and monitor is added in the monitor list
       {
	 if((monitor_list_ptr[local_mon_idx].cm_info_mon_conn_ptr)->flags & REMOVE_MONITOR)
	 {
	   found_remove_monitor = 1;
	   break;
	 }
	 (monitor_list_ptr[local_mon_idx].cm_info_mon_conn_ptr)->monitor_list_idx = local_mon_idx;

       }
       
       if(!strcmp(monitor_list_ptr[mon_idx].gdf_name, monitor_list_ptr[local_mon_idx].gdf_name))
       {
	 NSDL2_MON(NULL, NULL, "Monitor at index %d found with same GDF Name = %s", local_mon_idx, monitor_list_ptr[local_mon_idx].gdf_name);
	 monitor_list_ptr[mon_idx].no_of_monitors++;
	 CM_info *cm_ptr = monitor_list_ptr[local_mon_idx].cm_info_mon_conn_ptr;
	 if(cm_ptr->gp_info_index > 0) 
	 {
	   count++;
	 }

	 if (mon_idx < total_monitor_list_entries )
	 {
	   CM_info *cm_ptr = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;
	   if(cm_ptr->gp_info_index == -1  && (cm_ptr->total_vectors > 0)) 
	   {
	     *flag = PROCESS_REQUIRED;
	     MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
		 "Inside If part count %d no_of_monitors %d gdf_name %s\n", count , monitor_list_ptr[temp_value].no_of_monitors ,monitor_list_ptr[temp_value].gdf_name);

	   }
	   else
	   {
	     if (count < monitor_list_ptr[temp_value].no_of_monitors)
	     {
	       CM_info *cm_ptr = monitor_list_ptr[temp_value].cm_info_mon_conn_ptr;

	       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, 
		   "Inside else part count %d no_of_monitors %d gdf_name %s\n", count , monitor_list_ptr[temp_value].no_of_monitors ,monitor_list_ptr[temp_value].gdf_name);
	       set_cm_info_values(temp_value, cm_ptr->num_group, cm_ptr->no_of_element, cm_ptr->group_num_vectors, cm_ptr->gp_info_index);

	     }
	     //       *flag = COMPLETED_PROCESSED;

	   }
	 }
       }
       else
       {
	 monitor_list_ptr[local_mon_idx].no_of_monitors = 0;
	 break;
       }
       local_mon_idx++;
     }

     if(found_remove_monitor)
       break;

     /*if (mon_idx < total_monitor_list_entries )
     {
       CM_info *cm_ptr = monitor_list_ptr[temp_value].cm_info_mon_conn_ptr;
       if ((count == 0) && (cm_ptr->total_vectors > 0))
       {
       	 *flag = PROCESS_REQUIRED;
           MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                     "Inside If part count %d no_of_monitors %d gdf_name %s\n", count , monitor_list_ptr[temp_value].no_of_monitors ,monitor_list_ptr[temp_value].gdf_name);
       }
       else
       {
	 if (count < monitor_list_ptr[temp_value].no_of_monitors)
	 {
	   CM_info *cm_ptr = monitor_list_ptr[temp_value].cm_info_mon_conn_ptr;
           MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                     "Inside else part count %d no_of_monitors %d gdf_name %s\n", count , monitor_list_ptr[temp_value].no_of_monitors ,monitor_list_ptr[temp_value].gdf_name);
	   set_cm_info_values(temp_value, cm_ptr->num_group, cm_ptr->no_of_element, cm_ptr->group_num_vectors, cm_ptr->gp_info_index);

	 }
//	 *flag = COMPLETED_PROCESSED;
       }
     }*/

   }

  //ISSUE: NEED TO FIX THIS
  //how we will set no of monitor in case reused monitors found at early index.

   while(local_mon_idx != tmp_monitor_list_entries)
   {
     free_mon_lst_row(local_mon_idx);
     local_mon_idx++;
   }
}


void set_no_of_monitors()
{
  //No monitor applied
  if(total_monitor_list_entries == 0)
    return;

  int mon_idx = 0, local_mon_idx = 0, found_remove_monitor = 0, tmp_monitor_list_entries = total_monitor_list_entries;
  monitor_list_ptr[0].no_of_monitors = 0;    //to reset the no of monitors for first monitor rest will be reset inside the loop
  
  tmp_monitor_list_entries = total_monitor_list_entries;
  for (mon_idx = 0; mon_idx < total_monitor_list_entries;)
  {
    NSDL2_MON(NULL, NULL, "mon_idx %d, GDF Name %s, is_dynamic %d", mon_idx, monitor_list_ptr[mon_idx].gdf_name,
                           monitor_list_ptr[mon_idx].is_dynamic);

    while(local_mon_idx < total_monitor_list_entries)
    {
      if(monitor_list_ptr[local_mon_idx].cm_info_mon_conn_ptr != NULL) //check if cm_info is not made and monitor is added in the monitor list
      {
        if((monitor_list_ptr[local_mon_idx].cm_info_mon_conn_ptr)->flags & REMOVE_MONITOR)
        {
          found_remove_monitor = 1;
          break;
        }
        (monitor_list_ptr[local_mon_idx].cm_info_mon_conn_ptr)->monitor_list_idx = local_mon_idx;
      }

      if(!strcmp(monitor_list_ptr[mon_idx].gdf_name, monitor_list_ptr[local_mon_idx].gdf_name))
      {
         NSDL2_MON(NULL, NULL, "Monitor at index %d found with same GDF Name = %s", local_mon_idx, monitor_list_ptr[local_mon_idx].gdf_name);
         monitor_list_ptr[mon_idx].no_of_monitors++;
      }
      else
      {
        mon_idx = local_mon_idx;
        monitor_list_ptr[local_mon_idx].no_of_monitors = 0;
        break;
      }
       
      local_mon_idx++;
    }
    if(local_mon_idx == total_monitor_list_entries || found_remove_monitor)
      break;
  }

  //ISSUE: NEED TO FIX THIS
  //how we will set no of monitor in case reused monitors found at early index.

  while(local_mon_idx != tmp_monitor_list_entries)
  {
    free_mon_lst_row(local_mon_idx);
    local_mon_idx++; 
  }

}

// This method is called before dynamic vector monitors are added in the array
void initialize_cm_vector_groups(int g_runtime_flag)
{
  int mon_idx, i;
  char *prev_gdf_name = NULL;
  CM_info *cm_info_mon_conn_ptr = NULL;
  CM_vector_info *vector_list = NULL;
  int group_vector_idx = 0;
  int total_entries = 0;

  total_entries = total_monitor_list_entries;

  //set_no_of_monitors();

  for (mon_idx = 0; mon_idx < total_entries; mon_idx++) {
    cm_info_mon_conn_ptr = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;
    cm_info_mon_conn_ptr->monitor_list_idx = mon_idx;

    //setting mon_idx for Netcloud IP data monitor
    if(cm_info_mon_conn_ptr->cs_port == -1)
      nc_ip_data_mon_idx = mon_idx;

    vector_list = cm_info_mon_conn_ptr->vector_list;

    if(monitor_list_ptr[mon_idx].no_of_monitors > 0)
    {
      cm_info_mon_conn_ptr->prev_group_num_vectors = cm_info_mon_conn_ptr->group_num_vectors; //save prev group num vectors
      cm_info_mon_conn_ptr->group_num_vectors = cm_get_num_vectors(cm_info_mon_conn_ptr->gdf_name, mon_idx);
    }
    prev_gdf_name = cm_info_mon_conn_ptr->gdf_name;

    if(cm_info_mon_conn_ptr->prev_group_num_vectors == 0)
      cm_info_mon_conn_ptr->prev_group_num_vectors = cm_info_mon_conn_ptr->group_num_vectors;

    for(i = 0; i < cm_info_mon_conn_ptr->total_vectors; i++){
      if (prev_gdf_name && (strcmp(prev_gdf_name, cm_info_mon_conn_ptr->gdf_name) == 0)) {
        vector_list[i].group_vector_idx = ++group_vector_idx;
      } else {
        group_vector_idx = 0;
        vector_list[i].group_vector_idx = group_vector_idx;
      }
    }
  }
}

//free and making null to deleted pod
void free_node_pod(struct NodePodInfo *node_pod_info_ptr)
{

  FREE_AND_MAKE_NULL(node_pod_info_ptr->NodeName, "Node name", -1);
  FREE_AND_MAKE_NULL(node_pod_info_ptr->NodeIp, "Node ip", -1);
  FREE_AND_MAKE_NULL(node_pod_info_ptr->PodName, "Pode name", -1);
  FREE_AND_MAKE_NULL(node_pod_info_ptr->vector_name, "Pode name", -1);
  FREE_AND_MAKE_NULL(node_pod_info_ptr->appname_pattern, "Pode name", -1);
  FREE_AND_MAKE_NULL(node_pod_info_ptr,"node_pod_info_ptr,", 0)
}

void free_cm_info_node(CM_info *cm_info, int mon_idx)
{
  int app_idx, vec_idx;
  CM_vector_info *vector_list;

  if(cm_info == NULL)
  {
    NSDL3_MON(NULL, NULL, "Called free_cm_info_node to free cm_info of mon_idx = %d but cm_info is null", mon_idx); 
    return;
  }

  vector_list = cm_info->vector_list;
  

  if(cm_info->fd > 0)
  { 
    NSDL3_MON(NULL, NULL, "Closing connection for %s, Sorce ADD = %s",
                         cm_to_str(cm_info), nslb_get_src_addr(cm_info->fd));

    //remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__, cm_info->fd);

    if(close(cm_info->fd) < 0)
    {
      NSDL3_MON(NULL, NULL, "Error: connection close by NS is failed for monitor name '%s' and for fd %d",
                              cm_info->monitor_name, cm_info->fd);
    } 
  }
  
  for(vec_idx=0;vec_idx<cm_info->total_vectors;vec_idx++)
  {
    free_vec_lst_row(&(vector_list[vec_idx]));
  }
  FREE_AND_MAKE_NULL(cm_info->vector_list, "cm_info->vector_list", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->pgm_path, "cm_info->pgm_path", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->monitor_name, "cm_info->monitor_name", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->instance_name, "cm_info->instance_name", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->pgm_path, "cm_info->pgm_path", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->gdf_name, "cm_info->gdf_name", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->gdf_name_only, "cm_info->gdf_name_only", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->config_file, "cm_info->config_file", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->cs_ip, "cm_info->cs_ip", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->cavmon_ip, "cm_info->cavmon_ip", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->rem_ip, "cm_info->rem_ip", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->rem_username, "cm_info->rem_username", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->rem_password, "cm_info->rem_password", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->pgm_args, "cm_info->pgm_args", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->data_buf, "cm_info->data_buf", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->partial_buf, "cm_info->partial_buf", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->dest_file_name, "cm_info->dest_file_name", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->origin_cmon, "cm_info->origin_cmon", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->mbean_monitor_name, "cm_info->mbean_monitor_name", mon_idx);
  for(app_idx=0;app_idx<cm_info->total_appname_pattern;app_idx++)
  FREE_AND_MAKE_NULL(cm_info->appname_pattern[app_idx], "cm_info->appname_pattern[app]", app_idx);
  FREE_AND_MAKE_NULL(cm_info->appname_pattern, "cm_info->appname_pattern", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->tier_name, "cm_info->tier_name", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->server_name, "cm_info->server_name", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->server_display_name, "cm_info->server_display_name", mon_idx)
  FREE_AND_MAKE_NULL(cm_info->pod_name, "cm_info->pod_name", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->vectorReplaceFrom, "cm_info->vectorReplaceFrom", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->vectorReplaceTo, "cm_info->vectorReplaceTo", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->namespace, "cm_info->namespace", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->cmon_pod_pattern, "cm_info->cmon_pod_pattern", mon_idx);
  FREE_AND_MAKE_NULL(cm_info->g_mon_id, "cm_info->g_mon_id", mon_idx);
 
  if(total_mon_config_list_entries > 0)
    free_mon_config_structure(mon_config_list_ptr[cm_info->mon_info_index].mon_config, 1); 

  if(cm_info->csv_stats_ptr)
  {
    FREE_AND_MAKE_NULL(cm_info->csv_stats_ptr->metadata_csv_file, "cm_info->csv_stats_ptr->metadata_csv_file", mon_idx);
    FREE_AND_MAKE_NULL(cm_info->csv_stats_ptr->csv_file, "cm_info->csv_stats_ptr->csv_file", mon_idx);
    FREE_AND_MAKE_NULL(cm_info->csv_stats_ptr, "cm_info->csv_stats_ptr", mon_idx);
  } 
}

void free_mon_lst_row(int mon_idx)
{
  CM_info *cm_info;
  cm_info = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;
  
  NSDL3_MON(NULL, NULL, "Calling free_cm_info_node to free CM index = %d", mon_idx);
 
  free_cm_info_node(cm_info, mon_idx);
  total_monitor_list_entries--;
}

void free_cm_info()
{
  int i;

  NSDL2_MON(NULL, NULL, "Method called. Freeing CM entries");
  for (i = 0; i < total_monitor_list_entries; i++) {
    free_mon_lst_row(i);
  }

  NSDL3_MON(NULL, NULL, "Freeing monitor_list_ptr");
  FREE_AND_MAKE_NULL(monitor_list_ptr, "monitor_list_ptr", -1);
}



/*---------------- | Functions for Custom Monitor Disaster Recovery | --------------------*/

/* Open a socket 
 * Connect that socket  
 */
int cm_make_nb_conn(CM_info *cus_mon_ptr, int init_flag)
{
  //int err_num, err
  int con_state, s_port = 7891;
  char err_msg[1024] = "\0";
  char s_ip[512] = "\0";
  //socklen_t errlen;

  NSDL2_MON(NULL, NULL, "Method Called, cus_mon_ptr = %p", cus_mon_ptr);
  if((cus_mon_ptr->fd = nslb_nb_open_socket((AF_INET), err_msg)) < 0)
  {
    NSDL3_MON(NULL, NULL, "Error: problem in opening socket for %s", cm_to_str(cus_mon_ptr));
    return -1;
  }
  else
  { 
    if(g_enable_tcp_keepalive_opt == '1')
    {
      if(nslb_set_keepalive_params(cus_mon_ptr->fd, g_tcp_conn_idle, g_tcp_conn_keepintvl, g_tcp_conn_keepcnt, err_msg) < 0)
      {
        NSDL1_MON(NULL, NULL, "Error in setting keepalive attributes to the socket fd (%d), GDF: %s, Monitor Name: %s. Error: %s", cus_mon_ptr->fd, cus_mon_ptr->gdf_name, cus_mon_ptr->monitor_name, err_msg);
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                         "Connection failed as it could not set keepalive attributes for %s. %s", cm_event_msg(cus_mon_ptr), err_msg);
        return -1;
      }
    }

    CM_SET_AGENT(cus_mon_ptr, s_ip, s_port);
    NSDL3_MON(NULL, NULL, "Socket opened successfully so making connection for fd %d to server ip =%s, port = %d",
                           cus_mon_ptr->fd, s_ip, s_port);
    //Socket opened successfully so making connection
    int con_ret = nslb_nb_connect(cus_mon_ptr->fd, s_ip, s_port, &con_state, err_msg);
    NSDL3_MON(NULL, NULL, " con_ret = %d", con_ret);
    if(con_ret < 0)
    {
      NSDL3_MON(NULL, NULL, "%s", err_msg);
      MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                                                 "Connection failed for %s. %s", cm_event_msg(cus_mon_ptr), err_msg);
      //if((strstr(err_msg, "gethostbyname")) && (strstr(err_msg, "Unknown server error")))
      //  cus_mon_ptr->conn_state = CM_GETHOSTBYNAME_ERROR; 
      return -1;
    }
    else if(con_ret == 0)
    {
      cus_mon_ptr->conn_state = CM_CONNECTED;
      MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                                                 "Connection established for %s.", cm_event_msg(cus_mon_ptr));
    }
    else if(con_ret > 0)
    {
      if(con_state == NSLB_CON_CONNECTED)
        cus_mon_ptr->conn_state = CM_CONNECTED;

      if(con_state == NSLB_CON_CONNECTING)
        cus_mon_ptr->conn_state = CM_CONNECTING;
    }

    //Add this socket fd in epoll and wait for an event
    //If EPOLLOUT event comes then connect if not connected or/and send messages

    // July 23 2015 - Removing EPOLLET because sometimes we are facing an issue for ND monitor that connection remains in CONNECTING state, we are assuming that we have missed EPOLLOUT event and because its EPOLLET will not get event again.

    if( init_flag == 0 )
    {
      if(cus_mon_ptr->conn_state == CM_CONNECTED)
      {
        cm_send_msg_to_cmon(cus_mon_ptr, init_flag);
      }
    }
    else
      ADD_SELECT_MSG_COM_CON((char *)cus_mon_ptr, cus_mon_ptr->fd, EPOLLOUT | EPOLLERR | EPOLLHUP, DATA_MODE);
  }

  return 0;
}

static void reset_cm_info_on_start(CM_info *cm_ptr)
{
  int vec_idx;
  NSDL2_MON(NULL, NULL, "Method Called, cm_ptr = %p", cm_ptr);
  CM_vector_info *vector_list = cm_ptr->vector_list;

  for(vec_idx = 0; vec_idx < cm_ptr->total_vectors; vec_idx++)
    vector_list[vec_idx].flags &= ~DATA_FILLED;
  cm_ptr->dindex = 0;
  
  return;
}

/* This function will add entry of exited monitor into cm_dr_table
 * And also init all members which are included for disaster recovery   
 */
inline void cm_update_dr_table(CM_info *cus_mon_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called. Updating cm_dr_table: "
                        "g_total_aborted_cm_conn = %d, total_monitor_list_entries = %d,  cm_retry_flag = %d, max_cm_retry_count = %d", 
                         g_total_aborted_cm_conn, total_monitor_list_entries, cm_retry_flag, max_cm_retry_count);

  if(cus_mon_ptr->is_monitor_registered == MONITOR_REGISTRATION)
    return;

  if((cm_retry_flag) && (max_cm_retry_count > 0))
  {
    NSDL3_MON(NULL, NULL, "Adding custom monitor '%s' into cm_dr_table at index %d", 
                           cm_to_str(cus_mon_ptr), g_total_aborted_cm_conn); 
    //Adding custom monitor pointer into cm_dr_table
    //save pointer instead of self index

    if(g_total_aborted_cm_conn >= max_size_for_dr_table)
    {
       MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Reached maximum limit of DR table entry. So cannot allocate any more memory. Hence returning. cm_info->gdf_name = %s, CM_info->monitor_name = %s.",cus_mon_ptr->gdf_name, cus_mon_ptr->monitor_name);
      return ;
    }

    if(g_total_aborted_cm_conn >= max_dr_table_entries)
    {
      MY_REALLOC_AND_MEMSET(cm_dr_table, ((max_dr_table_entries + delta_size_for_dr_table) * sizeof(CM_info *)), (max_dr_table_entries * sizeof(CM_info *)), "Reallocation of DR table", -1);
      max_dr_table_entries += delta_size_for_dr_table;

      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "DR table has been reallocated by DELTA(%d) entries. Now new size will %d", delta_size_for_dr_table, max_dr_table_entries); 
    }

    cm_dr_table[g_total_aborted_cm_conn] = cus_mon_ptr;
    g_total_aborted_cm_conn++;

    NSDL3_MON(NULL, NULL, "Initialize dr members: cm_retry_attempts = %d, partial_buf = %p", 
                           cus_mon_ptr->cm_retry_attempts, cus_mon_ptr->partial_buf); 
    //Init CUSTOM MONITOR DR members 
    //if(cus_mon_ptr->conn_state != CM_GETHOSTBYNAME_ERROR)    
    cus_mon_ptr->conn_state = CM_INIT;
    cus_mon_ptr->bytes_remaining = 0;
    cus_mon_ptr->send_offset = 0;
    reset_cm_info_on_start(cus_mon_ptr);

    if(cus_mon_ptr->cm_retry_attempts == -1)
      cus_mon_ptr->cm_retry_attempts = max_cm_retry_count;
    else
      cus_mon_ptr->cm_retry_attempts--;

    if((max_cm_retry_count - cus_mon_ptr->cm_retry_attempts) > THRESHOLD_RETRY_COUNT)
      update_health_monitor_sample_data(&hm_data->retry_count_exceeding_threshold);

    if(cus_mon_ptr->partial_buf != NULL)
    {
      FREE_AND_MAKE_NULL(cus_mon_ptr->partial_buf, "cus_mon_ptr->partial_buf", cus_mon_ptr->cm_idx);
      cus_mon_ptr->partial_buf_len = 0;
    }

      NSDL3_MON(NULL, NULL, "cm_to_str(cus_mon_ptr) = %s, cus_mon_ptr = %p, cus_mon_ptr->cm_retry_attempts = %d", cm_to_str(cus_mon_ptr), cus_mon_ptr, cus_mon_ptr->cm_retry_attempts);
  }

  NSDL2_MON(NULL, NULL, "g_total_aborted_cm_conn = %d",g_total_aborted_cm_conn);
}

static inline void cus_reset_partial_buf(CM_info *cus_mon_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called, cus_mon_ptr = %p, partial_buf = %p ", 
                         cus_mon_ptr,cus_mon_ptr->partial_buf);
  
 if(cus_mon_ptr->partial_buf)
    (cus_mon_ptr->partial_buf)[0] = '\0';
  //resetting send offset as partition switch msg is to be sent 
  cus_mon_ptr->send_offset = 0;
  cus_mon_ptr->bytes_remaining = 0;
}

inline void cm_handle_err_case(CM_info *cus_mon_ptr, int remove_from_epoll)
{
   NSDL2_MON(NULL, NULL, "Method Called, cus_mon_ptr->fd = %d, cus_mon_ptr->conn_state = %d, remove_from_epoll = %d", 
                          cus_mon_ptr->fd, cus_mon_ptr->conn_state, remove_from_epoll);
   cus_reset_partial_buf(cus_mon_ptr);
   if(cus_mon_ptr->fd < 0)
     return;

   if(remove_from_epoll){
     REMOVE_SELECT_MSG_COM_CON(cus_mon_ptr->fd, DATA_MODE); 
   }
   //if(cus_mon_ptr->conn_state != CM_GETHOSTBYNAME_ERROR)
   cus_mon_ptr->conn_state = CM_INIT;
   CLOSE_FD(cus_mon_ptr->fd);
}

static inline void cm_retry_conn()
{
  int i;
  int tmp_failed_mon_id = 0;
  //int total_aborted_cm_conn = g_total_aborted_cm_conn;
  
  
  NSDL2_MON(NULL, NULL, "Method Called. g_total_aborted_cm_conn = %d", g_total_aborted_cm_conn);

  //For aborted custom monitoirs only
  for(i=0; i < g_total_aborted_cm_conn; i++)
  {
    //We wont be retrying for outbound connection from here, unless the monitor for which we are retrying is LPS based.
    if(cm_dr_table[i]->flags & OUTBOUND_ENABLED)
      continue;
    
    NSDL2_MON(NULL, NULL, "i = %d, cm_dr_table[%d] = %p, cm_retry_attempts = %d, cm_to_str(cm_dr_table[i]) = %s", i, i, cm_dr_table[i], cm_dr_table[i]->cm_retry_attempts, cm_to_str(cm_dr_table[i]));
    //If monitor has been deleted skipped
    if(cm_dr_table[i]->conn_state == CM_DELETED)
    {
      MLTL1(EL_F, 0, 0, _FLN_, cm_dr_table[i], EID_DATAMON_GENERAL, EVENT_INFORMATION,"This monitor has been marked as deleted,therefore retry has been skipped");
      continue;
    }

    //If particular monitor has complete their all attemps then it will not retry for next 
    if(cm_dr_table[i]->cm_retry_attempts == 0)
    {
      //If all retry attemps has complete and still monitor is not starting then remove it from list 
      NSDL3_MON(NULL, NULL, "All retry count has been completed for %s.", cm_to_str(cm_dr_table[i])); 
      MLTL1(EL_F, 0, 0, _FLN_, cm_dr_table[i], EID_DATAMON_GENERAL, EVENT_CRITICAL,
                                 "All retry count has been completed for %s",
                                  cm_event_msg(cm_dr_table[i]));
      continue;
    } 

    
   if(topo_info[topo_idx].topo_tier_info[cm_dr_table[i]->tier_index].topo_server[cm_dr_table[i]->server_index].used_row != -1)
   {
     if(topo_info[topo_idx].topo_tier_info[cm_dr_table[i]->tier_index].topo_server[cm_dr_table[i]->server_index].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
     {
        MLTL1(EL_F, 0, 0, _FLN_, cm_dr_table[i], EID_DATAMON_GENERAL, EVENT_INFORMATION,"This monitor return Unknown server error from gethostbyname,therefore retry has been skipped, changing its retry count to 0 and state to stopped");
        cm_dr_table[i]->cm_retry_attempts = 0;
        cm_dr_table[i]->conn_state = CM_STOPPED;
        continue;
     }
   }
    MLTL1(EL_DF, 0, 0, _FLN_, cm_dr_table[i], EID_DATAMON_GENERAL, EVENT_WARNING,
                          "Starting %s For retry attempt %d.",
                          cm_event_msg(cm_dr_table[i]),
                          ((max_cm_retry_count - cm_dr_table[i]->cm_retry_attempts) == 0)? 
                          1:(max_cm_retry_count - cm_dr_table[i]->cm_retry_attempts) + 1);

    int ret = cm_make_nb_conn(cm_dr_table[i], 1);

    NSDL3_MON(NULL, NULL, "Retry Count left: cm_dr_table[%d]->cm_retry_attempts = %d, ret = %d", 
                           i, cm_dr_table[i]->cm_retry_attempts, ret); 
    if(ret == -1)
    {
      NSDL3_MON(NULL, NULL, "Retry connection failed, so add into cm_dr table");
      MLTL1(EL_F, 0, 0, _FLN_, cm_dr_table[i], EID_DATAMON_GENERAL, EVENT_WARNING,
                               "Retry connection failed for '%s'(%s) on server %s:%d for retry attempt %d.",
                               get_gdf_group_name(cm_dr_table[i]->gp_info_index), 
                               cm_dr_table[i]->monitor_name, 
                               cm_dr_table[i]->cs_ip, cm_dr_table[i]->cs_port,
                               ((max_cm_retry_count - cm_dr_table[i]->cm_retry_attempts) == 0)? 
                               1:(max_cm_retry_count - cm_dr_table[i]->cm_retry_attempts) + 1);


      if(tmp_failed_mon_id > CM_DR_ARRAY_SIZE)
      {
        /*NSTL1_OUT(NULL, NULL, "Warning: Number of failed monitors %d exceeding maximum failed monitors limit %d."
                        "We will not do further retry for this vector %s.\n",
                        tmp_failed_mon_id, CM_DR_ARRAY_SIZE, cm_dr_table[i]->vector_name);*/
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Warning: Number of failed monitors %d exceeding maximum failed monitors limit %d."
                           "We will not do further retry for this monitor %s.",
                           tmp_failed_mon_id, CM_DR_ARRAY_SIZE, cm_dr_table[i]->monitor_name); 
      }
  
      if(tmp_failed_mon_id >= max_dr_table_tmp_entries)
      {
        MY_REALLOC_AND_MEMSET(cm_dr_table_tmp, ((max_dr_table_tmp_entries + delta_size_for_dr_table) * sizeof(CM_info *)), (max_dr_table_tmp_entries * sizeof(CM_info *)), "Reallocation of DR tmp table", -1);
        max_dr_table_tmp_entries += delta_size_for_dr_table;
      }

      cm_dr_table_tmp[tmp_failed_mon_id] = cm_dr_table[i];
      tmp_failed_mon_id++;
      cm_handle_err_case(cm_dr_table[i], CM_NOT_REMOVE_FROM_EPOLL);
      NSDL3_MON(NULL, NULL, "cm_dr_table_tmp[%d] = %d, tmp_failed_mon_id = %d",
                             tmp_failed_mon_id, cm_dr_table_tmp[tmp_failed_mon_id], tmp_failed_mon_id);

      if(cm_dr_table[i]->cm_retry_attempts > 0)
      {
        cm_dr_table[i]->cm_retry_attempts--;

        if(cm_dr_table[i]->cm_retry_attempts == 0)
        {
          num_aborted_cm_non_recoved++;
          cm_dr_table[i]->conn_state = CM_STOPPED;
        }
      } 
      NSDL3_MON(NULL, NULL, "Left retry counts : cm_dr_table[i]->cm_retry_attempts = %d", 
                             cm_dr_table[i]->cm_retry_attempts); 
    }
    if(cm_dr_table[i]->cm_retry_attempts == 0)
    {
      num_aborted_cm_non_recoved++;
      cm_dr_table[i]->conn_state = CM_STOPPED;
    }
  } //End for loop

  //for next sample of progress report
  NSDL3_MON(NULL, NULL, "tmp_failed_mon_id = %d", tmp_failed_mon_id);
  if(tmp_failed_mon_id)
  {
    //reset array of failed monitors
    memset(cm_dr_table, 0, sizeof(CM_info *) * g_total_aborted_cm_conn);
    //copy monitors failed after retry from temporary array to array of failed monitors
    memcpy(cm_dr_table, cm_dr_table_tmp, sizeof(void *) * tmp_failed_mon_id);
    //reset temporary array of failed monitors
    memset(cm_dr_table_tmp, 0, sizeof(CM_info *) * tmp_failed_mon_id);
    g_total_aborted_cm_conn = tmp_failed_mon_id;
  }
  else
  {
    memset(cm_dr_table, 0, sizeof(CM_info *) * g_total_aborted_cm_conn);
    g_total_aborted_cm_conn = 0;
  }

  NSDL3_MON(NULL, NULL, "End retry: g_total_aborted_cm_conn = %d", g_total_aborted_cm_conn);
}

int encoded_custom_mon_json_file_content(char *file, char **buff)
{ 
  FILE *fp = NULL;
  int num_bytes;
  
  fp = fopen(file, "r");
  if(fp == NULL)
  { 
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in opening file %s. Error: %s", file, nslb_strerror(errno));
    return 0;
  }
  
  fseek(fp, 0, SEEK_END);
  num_bytes = ftell(fp);
  MY_MALLOC(*buff, (num_bytes +1), "buffer", -1);

  fseek(fp, 0, SEEK_SET);
  fread((void*)*buff, sizeof(char), num_bytes, fp);
  if(fp)
  { 
    fclose(fp);
  }
  return num_bytes; 
}

char *encode_json_file_content(CURL *curl, char *file, char **buff)
{
  FILE *fp = NULL;
  int num_bytes;
  char *ptr = NULL;

  fp = fopen(file, "r");
  if(fp == NULL)
  { 
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in opening file %s. Error: %s", file, nslb_strerror(errno));
    return NULL;
  }
  
  fseek(fp, 0, SEEK_END);
  num_bytes = ftell(fp);
  MY_MALLOC(*buff, (num_bytes +1), "buffer", -1);
  
  fseek(fp, 0, SEEK_SET);
  fread((void*)*buff, sizeof(char), num_bytes, fp);
 
  ptr = curl_easy_escape(curl, *buff, num_bytes);
  if(fp)
  {
    fclose(fp);
  }
  return ptr;
}

int config_file_for_mbean(CM_info *cm_info_ptr, int buffer_len)  
{
  char *encoded_format = NULL; 
  char *buff = NULL;
  char *tmp;
  int num_bytes = 0;
  int length = 0;
  char tmp_buf[MAX_MONITOR_BUFFER_SIZE];
  char *encode_ptr = NULL;

  length = sprintf(tmp_buf, "MON_NAME=%s;CONFIG=", cm_info_ptr->mbean_monitor_name);
  copy_to_scratch_buffer_for_monitor_request(tmp_buf, length, DONT_RESET_SCRATCH_BUF, 0);

  tmp=strrchr(cm_info_ptr->config_file,'.');
  if((strcmp(tmp, ".enc")) != 0)
  {
    CURL *curl = curl_easy_init();
    if(!curl)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Could not encode config json for monitor %s, as curl_easy_init() returned NULL", cm_info_ptr->monitor_name);  
      return -1;
    }
    // we need to add config file, monitor_name  in cm_info structure for mbean monitor. So that at this time we will encode config and send it to cmon
    encoded_format = encode_json_file_content(curl, cm_info_ptr->config_file, &buff); 
    if(encoded_format != NULL)
      copy_to_scratch_buffer_for_monitor_request(encoded_format, strlen(encoded_format), DONT_RESET_SCRATCH_BUF, 0);
    FREE_AND_MAKE_NULL(buff, "buffer", -1);
    curl_free(encoded_format);
    curl_easy_cleanup(curl); 
  }
  else
  {
    num_bytes = encoded_custom_mon_json_file_content(cm_info_ptr->config_file, &buff); 
    buff[num_bytes]='\0';
    copy_to_scratch_buffer_for_monitor_request(buff, num_bytes, DONT_RESET_SCRATCH_BUF, 0);
    FREE_AND_MAKE_NULL(buff, "buffer", -1);  
  }
 
  if (cm_info_ptr->flags & OUTBOUND_ENABLED)
  {
    encode_ptr = encode_message(monitor_scratch_buf + buffer_len, (monitor_scratch_buf_len - buffer_len));
    if( encode_ptr != NULL)
      copy_to_scratch_buffer_for_monitor_request(encode_ptr, strlen(encode_ptr), RESET_SCRATCH_BUF, buffer_len);
    curl_free(encode_ptr);
  }
 
  return 0;
}

/*
Bug 100630 - cm_init buffer has more extra arguments which is not required

These fields are not used now as discussed with CMON team 
MON_GDF=%s;OWNER=%s;GROUP=%s;MON_NS_SERVER_NAME=%s;MON_NS_FTP_USER=%s;MON_NS_FTP_PASSWORD=%s;

If(MON_ACCESS is ON) these fields arfe required only when mon_access is 1
MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;MON_ACCESS=%d;

These field is server specific So these are stored in cm_init after cm_info ptr structure filling.
VECTOR_NAME=%s;MON_ID=%d;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;ORIGIN_CMON=%s;MON_CAVMON_SERVER_PORT=%d;MON_CAVMON_SERVER_NAME=%s;CUR_PARTITION_IDX=%lld

#These are stored in cm_init_buffer at the time of JSON parsing
MON_FREQUENCY=%d;G_MON_ID=%s;IS_PROCESS=%d;MON_OPTION=%d;NUM_TX=%d;MON_TEST_RUN=%d;MON_NS_WDIR=%s;MON_VECTOR_SEPARATOR=%c;MON_NS_VER=%s;MON_PARTITION_IDX=%lld;
*/
inline void make_cm_init_buffer(CM_info *cm_ptr, int mon_id, char *cm_init_buffer, int *buffer_len)
{
  MonConfig *mon_config;

  char cm_init_remote_buff[1024];
  char owner_name[MAX_STRING_LENGTH];
  char group_name[MAX_STRING_LENGTH];
  int testidx = start_testidx;

  if(total_mon_config_list_entries > 0 )
  { 
    mon_config = mon_config_list_ptr[cm_ptr->mon_info_index].mon_config;

    if(cm_ptr->access == RUN_REMOTE_ACCESS)
      sprintf(cm_init_remote_buff, "MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;", 
                                        cm_ptr->access, cm_ptr->rem_ip, cm_ptr->rem_username, cm_ptr->rem_password);
    else
      sprintf(cm_init_remote_buff, "MON_ACCESS=%d;", cm_ptr->access);
 
    if(cm_ptr->origin_cmon == NULL)
    {
      *buffer_len = sprintf(cm_init_buffer, "%s%sVECTOR_NAME=%s;MON_ID=%d;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;MON_CAVMON_SERVER_NAME=%s;"
                                            "MON_CAVMON_SERVER_PORT=%d;CUR_PARTITION_IDX=%lld;",
                                            mon_config->cm_init_buffer, cm_init_remote_buff, cm_ptr->monitor_name, mon_id, cm_ptr->pgm_path,
                                            cm_ptr->pgm_args, cm_ptr->cavmon_ip, cm_ptr->cavmon_port, g_partition_idx);    
    }  
    else
    {
      *buffer_len = sprintf(cm_init_buffer, "%s%sVECTOR_NAME=%s;MON_ID=%d;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;ORIGIN_CMON=%s;"
                                            "MON_CAVMON_SERVER_NAME=%s;MON_CAVMON_SERVER_PORT=%d;CUR_PARTITION_IDX=%lld;",
                                            mon_config->cm_init_buffer, cm_init_remote_buff, cm_ptr->monitor_name, mon_id, cm_ptr->pgm_path, 
                                            cm_ptr->pgm_args, cm_ptr->origin_cmon, cm_ptr->cavmon_ip, cm_ptr->cavmon_port, g_partition_idx);
    }
  }
  else
  {
    if(cm_ptr->origin_cmon == NULL)
    {
      *buffer_len = sprintf(cm_init_buffer, "cm_init_monitor:OWNER=%s;GROUP=%s;VECTOR_NAME=%s;MON_ID=%d;MON_GDF=%s;MON_PGM_NAME=%s;"
                 "MON_PGM_ARGS=%s;MON_OPTION=%d;MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;"
                 "MON_REMOTE_PASSWD=%s;MON_FREQUENCY=%d;G_MON_ID=%s;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;"
                 "MON_CAVMON_SERVER_NAME=%s;MON_CAVMON_SERVER_PORT=%d;MON_NS_FTP_USER=%s;MON_NS_FTP_PASSWORD=%s;"
                 "MON_NS_WDIR=%s;MON_NS_VER=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;CUR_PARTITION_IDX=%lld;NUM_TX=%d;IS_PROCESS=%d;",
                      nslb_get_owner(owner_name), nslb_get_group(group_name), cm_ptr->monitor_name,
                      mon_id, cm_ptr->gdf_name, cm_ptr->pgm_path, cm_ptr->pgm_args,
                      cm_ptr->option, cm_ptr->access, cm_ptr->rem_ip,
                      cm_ptr->rem_username, cm_ptr->rem_password, cm_ptr->frequency,
                      cm_ptr->g_mon_id, testidx, g_cavinfo.NSAdminIP,
                      cm_ptr->cavmon_ip, cm_ptr->cavmon_port, get_ftp_user_name(), get_ftp_password(), g_ns_wdir,
                      ns_version, global_settings->hierarchical_view_vector_separator, g_start_partition_idx, g_partition_idx, total_tx_entries,cm_ptr->is_process);
    }
    else
    {
      *buffer_len = sprintf(cm_init_buffer, "cm_init_monitor:OWNER=%s;GROUP=%s;VECTOR_NAME=%s;MON_ID=%d;MON_GDF=%s;MON_PGM_NAME=%s;"
                     "MON_PGM_ARGS=%s;MON_OPTION=%d;MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;"
                     "MON_REMOTE_PASSWD=%s;MON_FREQUENCY=%d;G_MON_ID=%s;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;"
                     "MON_CAVMON_SERVER_NAME=%s;MON_CAVMON_SERVER_PORT=%d;MON_NS_FTP_USER=%s;MON_NS_FTP_PASSWORD=%s;"
                     "MON_NS_WDIR=%s;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;"
                     "CUR_PARTITION_IDX=%lld;NUM_TX=%d;IS_PROCESS=%d;",
                      nslb_get_owner(owner_name), nslb_get_group(group_name), cm_ptr->monitor_name,
                      mon_id, cm_ptr->gdf_name, cm_ptr->pgm_path, cm_ptr->pgm_args,
                      cm_ptr->option, cm_ptr->access, cm_ptr->rem_ip,
                      cm_ptr->rem_username, cm_ptr->rem_password, cm_ptr->frequency,
                      cm_ptr->g_mon_id, testidx, g_cavinfo.NSAdminIP,
                      cm_ptr->cavmon_ip, cm_ptr->cavmon_port, get_ftp_user_name(), get_ftp_password(), g_ns_wdir,
                      ns_version, cm_ptr->origin_cmon, global_settings->hierarchical_view_vector_separator,
                      g_start_partition_idx, g_partition_idx, total_tx_entries, cm_ptr->is_process);
    }
  }
}

//TODO PP:Send Num trans also
//here now frequency is not used and we are using cm_info_mon_conn_ptr->frequency i.e given in json file of monitor or we set default by progress// interval
inline void cm_make_send_msg(CM_info *cm_info_mon_conn_ptr, char *msg_buf, int frequency, int *msg_len)
{
  //char owner_name[MAX_STRING_LENGTH];
  //char group_name[MAX_STRING_LENGTH];
  //int testidx = start_testidx;
  int custom_mon_id = cm_info_mon_conn_ptr->cm_idx;
  char *ptr;
  struct stat st;
  size_t file_size = 0;
  char tmp_msg_buf[MAX_BUFFER_SIZE_FOR_MONITOR];
  int msg_length = 0;
  int ptr_len;
  int msg_len_to_reset = 0;

  if(cm_info_mon_conn_ptr->config_file)
  { 
    stat(cm_info_mon_conn_ptr->config_file, &st);
    file_size = st.st_size;
  }

  //MON_ID is added in SendMsg buffer for Service Monitor only.
  //Other monitors will ignore MON_ID.

  //In SendMsg buffer, changes cus_mon_ptr->cs_ip to cus_mon_ptr->cavmon_ip and cus_mon_ptr->cs_port to cus_mon_ptr->cavmon_port in order to pass actual cavmon ip/port to server instead of passing ip/port to which connection is established.

  if(is_outbound_connection_enabled)
  {
    custom_mon_id = cm_info_mon_conn_ptr->mon_id;
  }

  //Here we are making cm_init buffer
  make_cm_init_buffer(cm_info_mon_conn_ptr, custom_mon_id, tmp_msg_buf, &msg_length);
 
  NSTL4(NULL, NULL, "Length of msg_buf received = %d", *msg_len);

  if(*msg_len > 0)
  {
    copy_to_scratch_buffer_for_monitor_request(msg_buf, *msg_len, RESET_SCRATCH_BUF, 0);
  }
  else
  {
    reset_scratch_buffer_length(0);
  }

  if(cm_info_mon_conn_ptr->flags & OUTBOUND_ENABLED)
  {
    ptr = encode_message(tmp_msg_buf, msg_length);
    ptr_len = strlen(ptr);
    copy_to_scratch_buffer_for_monitor_request(ptr, ptr_len, DONT_RESET_SCRATCH_BUF, 0);
    curl_free(ptr);
  }
  else
  {
    copy_to_scratch_buffer_for_monitor_request(tmp_msg_buf, msg_length, DONT_RESET_SCRATCH_BUF, 0);
  }

  msg_len_to_reset = monitor_scratch_buf_len;  

  if (cm_info_mon_conn_ptr->monitor_type & CMON_MBEAN_MONITOR && cm_info_mon_conn_ptr->config_file)
   {
     if (file_size > 0)
     {
       config_file_for_mbean(cm_info_mon_conn_ptr, msg_len_to_reset);
     }
   }

   copy_to_scratch_buffer_for_monitor_request("\n", 1, DONT_RESET_SCRATCH_BUF, 0);
   
   *msg_len = monitor_scratch_buf_len;
   NSTL4(NULL, NULL, "Total bytes written in scartch buffer = %d", monitor_scratch_buf_len);
}


//this function will save the cm_update_monitor buff if we are failed to send the msg to NDC
void store_cm_update_msg_buf(char *mon_buf, int mon_buf_len)
{
  int row;
  //reallocation of monitor config structure
  if(create_table_entry_ex(&row, &total_cm_update_monitor_entries, &max_cm_update_monitor_entries,
                               (char **)&cm_update_monitor_buf, sizeof(char), "Allocating MonConfig Structure Table") == -1)
  {  
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Could not create table entry for Mon Config");
    return;
  }
  MALLOC_AND_COPY(mon_buf, cm_update_monitor_buf[total_cm_update_monitor_entries], (mon_buf_len + 1), "Copying monitor buff", -1);
  total_cm_update_monitor_entries += 1;
}

// this function will make cm_update_monitor and send to ndc
void make_cm_update_msg_and_send_msg_to_ndc(char *mon_args, int mon_info_index)
{
  CM_info *cm_info_mon_conn_ptr = NULL;
  MonConfig *mon_config;

  char *ptr;
  char owner_name[MAX_STRING_LENGTH];
  char group_name[MAX_STRING_LENGTH];
  char tmp_msg_buf[MAX_BUFFER_SIZE_FOR_MONITOR];
  char mon_send_buf[MAX_BUFFER_SIZE_FOR_MONITOR];

  int msg_length;
  int mon_send_buf_len;
  int mon_id, id;
  int index;  //this index is of cm_ptr

  NSDL2_MON(NULL, NULL, "Method called mj_runtime_process_deleted_monitors");

  mon_config = mon_config_list_ptr[mon_info_index].mon_config;

  for(id=0; id<mon_config->total_mon_id_index; id++)
  {
    mon_id = mon_config->mon_id_struct[id].mon_id;
    if(mon_id >= 0 && mon_id <= max_mon_id_entries)
    {
      index = mon_id_map_table[mon_id].mon_index;
      if(index >=0 && index <= total_monitor_list_entries)
      {
        cm_info_mon_conn_ptr = monitor_list_ptr[index].cm_info_mon_conn_ptr;

        if(cm_info_mon_conn_ptr->origin_cmon == NULL)
        {
          msg_length =sprintf(tmp_msg_buf, "cm_update_monitor:OWNER=%s;GROUP=%s;VECTOR_NAME=%s;MON_ID=%d;MON_GDF=%s;MON_PGM_NAME=%s;"
                      "MON_PGM_ARGS=%s;MON_OPTION=%d;MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;"
                      "MON_REMOTE_PASSWD=%s;MON_FREQUENCY=%d;G_MON_ID=%s;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;"
                      "MON_CAVMON_SERVER_NAME=%s;MON_CAVMON_SERVER_PORT=%d;MON_NS_FTP_USER=%s;MON_NS_FTP_PASSWORD=%s;"
                      "MON_NS_WDIR=%s;MON_NS_VER=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;"
                      "CUR_PARTITION_IDX=%lld;NUM_TX=%d;IS_PROCESS=%d;",
                      nslb_get_owner(owner_name), nslb_get_group(group_name), cm_info_mon_conn_ptr->monitor_name,
                      cm_info_mon_conn_ptr->mon_id, cm_info_mon_conn_ptr->gdf_name, cm_info_mon_conn_ptr->pgm_path, mon_args,
                      cm_info_mon_conn_ptr->option, cm_info_mon_conn_ptr->access, cm_info_mon_conn_ptr->rem_ip,
                      cm_info_mon_conn_ptr->rem_username, cm_info_mon_conn_ptr->rem_password, cm_info_mon_conn_ptr->frequency,
                      cm_info_mon_conn_ptr->g_mon_id, testidx, g_cavinfo.NSAdminIP, cm_info_mon_conn_ptr->cavmon_ip,
                      cm_info_mon_conn_ptr->cavmon_port, get_ftp_user_name(), get_ftp_password(), g_ns_wdir,
                      ns_version, global_settings->hierarchical_view_vector_separator, g_start_partition_idx, g_partition_idx, total_tx_entries,
                      cm_info_mon_conn_ptr->is_process);
        }
        else
        {
          msg_length = sprintf(tmp_msg_buf, "cm_update_monitor:OWNER=%s;GROUP=%s;VECTOR_NAME=%s;MON_ID=%d;MON_GDF=%s;MON_PGM_NAME=%s;"
                       "MON_PGM_ARGS=%s;MON_OPTION=%d;MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;"
                       "MON_REMOTE_PASSWD=%s;MON_FREQUENCY=%d;G_MON_ID=%s;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;"
                       "MON_CAVMON_SERVER_NAME=%s;MON_CAVMON_SERVER_PORT=%d;MON_NS_FTP_USER=%s;MON_NS_FTP_PASSWORD=%s;"
                       "MON_NS_WDIR=%s;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;"
                       "CUR_PARTITION_IDX=%lld;NUM_TX=%d;IS_PROCESS=%d;",
                       nslb_get_owner(owner_name), nslb_get_group(group_name), cm_info_mon_conn_ptr->monitor_name,
                       cm_info_mon_conn_ptr->mon_id, cm_info_mon_conn_ptr->gdf_name, cm_info_mon_conn_ptr->pgm_path, mon_args,
                       cm_info_mon_conn_ptr->option, cm_info_mon_conn_ptr->access, cm_info_mon_conn_ptr->rem_ip,
                       cm_info_mon_conn_ptr->rem_username, cm_info_mon_conn_ptr->rem_password, cm_info_mon_conn_ptr->frequency,
                       cm_info_mon_conn_ptr->g_mon_id, testidx, g_cavinfo.NSAdminIP, cm_info_mon_conn_ptr->cavmon_ip,
                       cm_info_mon_conn_ptr->cavmon_port, get_ftp_user_name(), get_ftp_password(), g_ns_wdir,
                       ns_version, cm_info_mon_conn_ptr->origin_cmon, global_settings->hierarchical_view_vector_separator,
                       g_start_partition_idx, g_partition_idx, total_tx_entries,cm_info_mon_conn_ptr->is_process);
        }
        ptr = encode_message(tmp_msg_buf, msg_length);

        if(cm_info_mon_conn_ptr->tier_name)
          mon_send_buf_len = sprintf(mon_send_buf,"nd_data_req:action=mon_config;server=%s%c%s;mon_id=%d;msg=%s\n",
                                     cm_info_mon_conn_ptr->tier_name, global_settings->hierarchical_view_vector_separator,
                                     cm_info_mon_conn_ptr->server_display_name, cm_info_mon_conn_ptr->mon_id, ptr);
        else
          mon_send_buf_len = sprintf(mon_send_buf, "nd_data_req:action=mon_config;server=%s;mon_id=%d;msg=%s\n",
                                      cm_info_mon_conn_ptr->server_display_name, cm_info_mon_conn_ptr->mon_id, ptr);
      }
    }
  }
  if(ndc_data_mccptr.fd > 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                       "Sending message to NDC for MON_CONFIG for monitors added at runtime. Message: %s", mon_send_buf);
    if(write_msg(&ndc_data_mccptr, mon_send_buf, mon_send_buf_len, 0, DATA_MODE) < 0)
    {
       MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
                "Error in sending message to server %s on NDC data connection.", cm_info_mon_conn_ptr->server_display_name);
      store_cm_update_msg_buf(mon_send_buf, mon_send_buf_len);
    }
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
                  "Info: Could not send monitor request for mon_id (%d) to NDC at runtime as connection with NDC is not established. Hence skipping for rest of the monitors of this server.", cm_info_mon_conn_ptr->mon_id);
    store_cm_update_msg_buf(mon_send_buf, mon_send_buf_len);
  }

  curl_free(ptr);
  return;
}


/* Handle partial write*/
int cm_send_msg(CM_info *cus_mon_ptr, char *msg_buf, int init_flag)
{
  char *buf_ptr;
  int bytes_sent;
			
  NSDL2_MON(NULL, NULL, "Method called, cus_mon_ptr = %p, msg_buf = %p, partial_buf = %p ",
                         cus_mon_ptr, msg_buf, cus_mon_ptr->partial_buf);
  //if(send_msg_len) //partial buf is empty 
  if((cus_mon_ptr->partial_buf == NULL || (cus_mon_ptr->partial_buf)[0] == '\0'))
  {
    if(msg_buf == NULL)
    {
      if(cus_mon_ptr->monitor_type & BCI_MBEAN_MONITOR) 
      {
        buf_ptr = mbean_mon_ptr[cus_mon_ptr->mbean_mon_idx].msg_buf;
        cus_mon_ptr->bytes_remaining = strlen(buf_ptr);
      }
      else
      {
        copy_to_scratch_buffer_for_monitor_request('\0', 128, RESET_SCRATCH_BUF, 0);
        cus_mon_ptr->bytes_remaining = 0;
        cm_make_send_msg(cus_mon_ptr, NULL, global_settings->progress_secs, &cus_mon_ptr->bytes_remaining);
        buf_ptr = monitor_scratch_buf;
      }
      
    }
    else
    {
      buf_ptr = msg_buf;
      cus_mon_ptr->bytes_remaining = strlen(buf_ptr);
    }
  }
  else //If there is partial send
  {
    buf_ptr = cus_mon_ptr->partial_buf;
  }
  // Send MSG to CMON
  while(cus_mon_ptr->bytes_remaining)
  {
    NSDL2_MON(NULL, NULL, "Send MSG: cus_mon_ptr->fd = %d, remaining_bytes = %d,  send_offse = %d, buf = [%s]", 
                       cus_mon_ptr->fd, cus_mon_ptr->bytes_remaining , 
                       cus_mon_ptr->send_offset, 
                       buf_ptr + cus_mon_ptr->send_offset);
    if((bytes_sent = send(cus_mon_ptr->fd, buf_ptr + cus_mon_ptr->send_offset, cus_mon_ptr->bytes_remaining, 0)) < 0)
    {
      NSDL2_MON(NULL, NULL, "bytes_sent = %d, errno = %d, error = %s", bytes_sent, errno, nslb_strerror(errno));
      if(errno == EAGAIN) //If message send partially
      {
        if(cus_mon_ptr->partial_buf == NULL ||  (cus_mon_ptr->partial_buf)[0] == '\0')
        {
          if(cus_mon_ptr->partial_buf == NULL)
          {
            MY_MALLOC(cus_mon_ptr->partial_buf, (cus_mon_ptr->bytes_remaining + 1), "buffer for partial send", -1);
          }
          else
          {
           MY_REALLOC(cus_mon_ptr->partial_buf, (cus_mon_ptr->bytes_remaining + 1), "buffer for partial send", -1);
          }
          cus_mon_ptr->partial_buf_len = cus_mon_ptr->bytes_remaining + 1;
          strcpy(cus_mon_ptr->partial_buf, buf_ptr + cus_mon_ptr->send_offset);
          cus_mon_ptr->send_offset = 0;
	}
        cus_mon_ptr->conn_state = CM_SENDING;

        NSDL2_MON(NULL, NULL, "Adding mon fd to EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT");
        //TODO: TEST THIS 
        if(init_flag != 0)
          MOD_SELECT_MSG_COM_CON((char *)cus_mon_ptr, cus_mon_ptr->fd, EPOLLOUT |EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
          MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
	                    	                         "Sending msg '%s' successfull for %s.", 
                                                                               buf_ptr, cm_event_msg(cus_mon_ptr)); 
        
        NSDL3_MON(NULL, NULL, "Partial Send: cus_mon_ptr->fd = %d, remaining_bytes = %d, send_offse = %d, remaning buf = [%s]", 
                               cus_mon_ptr->fd, cus_mon_ptr->bytes_remaining, cus_mon_ptr->send_offset, buf_ptr + cus_mon_ptr->send_offset);
       
        return 0;
      }
      if(errno == EINTR) //If any intrept occurr
      {
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        //cus_mon_ptr->fd = -1;
        continue;
      }
      else 
      {
        //sending msg failed for this monitor, close fd & send for nxt monitor
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                       "Sending msg '%s' failed for %s. Error: send(): errno %d (%s)", buf_ptr, cm_event_msg(cus_mon_ptr),  errno, nslb_strerror(errno));
        cm_handle_err_case(cus_mon_ptr, init_flag);
        cm_update_dr_table(cus_mon_ptr);
        return -1;
      } 
    }

    NSDL2_MON(NULL, NULL, "bytes_sent = %d", bytes_sent);

    if(bytes_sent == 0)
    {
      MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                              "Sending msg '%s' failed for %s. Error: send(): bytes_sent = 0", 
                               buf_ptr, cm_event_msg(cus_mon_ptr));
      cm_handle_err_case(cus_mon_ptr, init_flag);
      cm_update_dr_table(cus_mon_ptr);
      return -1;
    }
    cus_mon_ptr->bytes_remaining -= bytes_sent;
    cus_mon_ptr->send_offset += bytes_sent;
  } //End while Loop 

  //No need to check the remaining bytes as you will reach here only in case bytes remaining is 0
  //if(remaining_bytes == 0)
  //{
    NSDL2_MON(NULL, NULL, "MSG '%s' sent succefully for %s.", buf_ptr, cm_to_str(cus_mon_ptr));
    MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
		                         "Sending msg '%s' successfull for %s.", 
                                          buf_ptr, cm_event_msg(cus_mon_ptr)); 
   
    cus_reset_partial_buf(cus_mon_ptr); 
    cus_mon_ptr->conn_state = CM_RUNNING;

    if( init_flag != 0){
      MOD_SELECT_MSG_COM_CON((char *)cus_mon_ptr, cus_mon_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
    }
  //}

  return 0;
}

int handle_epoll_error_for_cm(void *ptr)
{
  CM_info *cus_mon_ptr = (CM_info *) ptr;
  
  NSDL2_MON(NULL, NULL, "Method Called");
  close_custom_monitor_connection(cus_mon_ptr);
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Control connection caught EpollErr for gdf (%s), monitor name (%s). Monitor connection has been closed and removed from epoll.", cus_mon_ptr->gdf_name, cus_mon_ptr->monitor_name); 
  return 0;
}

int cm_send_msg_to_cmon(void *ptr, int init_flag)
{
  int s_port = 7891, con_state;
  char err_msg[1024] = "\0";
  char s_ip[512] = "\0";

  CM_info *cus_mon_ptr = (CM_info *) ptr;

  NSDL2_MON(NULL, NULL, "Method called, cus_mon_ptr = %p, cus_mon_ptr->fd = %d, cus_mon_ptr->conn_state = %d", 
                        cus_mon_ptr, cus_mon_ptr->fd, cus_mon_ptr->conn_state);
  //Check State first 
  //1. If is in CM_CONNECTED then send message and change state to SENDING
  //2. If is in CM_CONNECTING then again try to connect if connect then change state to CM_CONNECTED other wise into CM_CONNICTING 
  if(cus_mon_ptr->fd < 0) 
  {
    return 0;
  } 

  if(cus_mon_ptr->conn_state == CM_CONNECTING)
  {
    //Again send connect request
    CM_SET_AGENT(cus_mon_ptr, s_ip, s_port);
    NSDL3_MON(NULL, NULL, "Since connection is in CM_CONNECTING so try to Reconnect for fd %d and to server ip = %s, port = %d", 
                           cus_mon_ptr->fd, s_ip, s_port);

    if(nslb_nb_connect(cus_mon_ptr->fd, s_ip, s_port, &con_state, err_msg) != 0 && 
      con_state != NSLB_CON_CONNECTED)
    {
      NSDL3_MON(NULL, NULL, "err_msg = %s", err_msg);
      MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_CRITICAL,
                               "Retry connection failed for %s for retry attempt %d. %s",
                               cm_event_msg(cus_mon_ptr), 
                               ((max_cm_retry_count - cus_mon_ptr->cm_retry_attempts) == 0)? 
                               1:(max_cm_retry_count - cus_mon_ptr->cm_retry_attempts) + 1, err_msg);
      //if((strstr(err_msg, "gethostbyname")) && (strstr(err_msg, "Unknown server error")))
      //cus_mon_ptr->conn_state = CM_GETHOSTBYNAME_ERROR; 
      cm_handle_err_case(cus_mon_ptr, CM_REMOVE_FROM_EPOLL);
      cm_update_dr_table(cus_mon_ptr);
      return -1;
    }

    cus_mon_ptr->cm_retry_attempts = -1;
    cus_mon_ptr->conn_state = CM_CONNECTED;
    MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
			                          "Connection established for %s.", cm_event_msg(cus_mon_ptr)); 
  }

  // Make msg only first time 
  // If connection is in CM_SENDING state then we will make send msg
  //if(cus_mon_ptr->conn_state == CM_CONNECTED || cus_mon_ptr->conn_state == CM_CONNECTING)
  //if(cus_mon_ptr->conn_state == CM_CONNECTED)
  //{ 
    //NSDL3_MON(NULL, NULL, "Making custom monitor init message as state is %d", cus_mon_ptr->conn_state);

    //cm_make_send_msg(cus_mon_ptr, SendMsg, global_settings->progress_secs, cus_mon_ptr->cm_idx, &send_msg_len); 

    //NSDL3_MON(NULL, NULL, "After making send message, SendMsg = [%s], send_msg_len = %d", SendMsg, send_msg_len);
  //}

  if(cm_send_msg(cus_mon_ptr, NULL, init_flag) == -1)
    return -1;

  return 0;
}

//Calling this from deliver_report.c for aborted monitors recovery
inline void handle_cm_disaster_recovery()
{
  //Handling Monitor retry 
  NSDL2_MESSAGES(NULL, NULL, "Method called. g_total_aborted_cm_conn = %d, num_aborted_cm_non_recoved = %d",
                             g_total_aborted_cm_conn, num_aborted_cm_non_recoved);

  if(g_total_aborted_cm_conn > 0 && num_aborted_cm_non_recoved != g_total_aborted_cm_conn)
  {
    NSDL3_MESSAGES(NULL, NULL, "There are monitors to be restarted.");
    cm_retry_conn();  
  }
  else
    NSDL3_MESSAGES(NULL, NULL, "There are no monitors to be restarted.");
  
  NSDL3_MESSAGES(NULL, NULL, "Function end. DR done.");
} 

/* This is to parse keyword:
 * ENABLE_MONITOR_DR <0/1> <retry_count>  
 * */
int kw_set_enable_monitor_dr(char *keyword, char *buf)
{
  char key[1024];
  int retry_flag;
  int retry_count;
  char err_msg[MAX_AUTO_MON_BUF_SIZE];  
      
  NSDL3_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);
 
  if(sscanf(buf, "%s %d %d", key, &retry_flag, &retry_count) != 3)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Too few/more arguments for %s keywords.", key);
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_MONITOR_DR_USAGE, CAV_ERR_1011359,key);
  } 

  cm_retry_flag = retry_flag;   
  if (global_settings->continuous_monitoring_mode)
  {
    max_cm_retry_count = 0xfffffff;
  }
  else
  {
    max_cm_retry_count = retry_count;
  }
  NSDL3_MON(NULL, NULL, "cm_retry_flag = %d, max_cm_retry_count = %d", cm_retry_flag, max_cm_retry_count);
return 0;
}


/************validate data*****************/

//data passed is not yet in custom monitor structure hence passing here
int validate_nd_data(CM_info *cus_mon_ptr, char *vector_name, char *buffer)
{
  int gp_info_index, data_idx = 0, c_graph_numvec;
  Graph_Info *local_graph_data_ptr = NULL;
  char *field[1024], temp_buf[4096];
  int c_graph = 0, ret=0;
  double avg, min, max, count, request_per_sec;

  gp_info_index = cus_mon_ptr->gp_info_index;
  Group_Info *local_group_data_ptr = group_data_ptr + gp_info_index;
  local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;
  if(gp_info_index < 0)
   return 0;

  if(cus_mon_ptr->no_of_element == 0)
  {
    get_no_of_elements(cus_mon_ptr, (int *)&cus_mon_ptr->no_of_element);
  }

  strcpy(temp_buf, buffer);
  ret = get_tokens_with_multi_delimiter(temp_buf, field, " ", cus_mon_ptr->no_of_element); 

  if(ret < cus_mon_ptr->no_of_element)
    return 0;

  for(c_graph = 0; c_graph < local_group_data_ptr->num_graphs; c_graph++)
  {
    for(c_graph_numvec = 0; c_graph_numvec < local_graph_data_ptr->num_vectors; c_graph_numvec++)
    {
      // DATA_TYPE_SAMPLE DATA_TYPE_RATE DATA_TYPE_CUMULATIVE
      if(IS_TIMES_GRAPH(local_graph_data_ptr->data_type)) // avg min max count 
      {
        //   Min, Max, Avg >= 0 and <= resp max
	//   Count >= 0 and <= count max
	//   Request/Sec >= 0 and <= tps max
        avg = atof(field[data_idx]);
        min = atof(field[data_idx + 1]);
        max = atof(field[data_idx + 2]);
        count = atof(field[data_idx + 3]);
        
        if(!((count >= 0) && (count <= g_nd_max_count)))
        {
          MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Received invalid count in dataline (%s) for vector (%s) as count is not in between the 0 and max limit of count set (%d) in ND_DATA_VALIDATION keyword.", buffer, vector_name, g_nd_max_count);
          return -1;
        }
        else if(!((avg >= 0) && (avg <= g_nd_max_resp)))
        {
          MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Received invalid average count in dataline (%s) for vector (%s) as average is not in between the 0 and max limit (%d) of response set in ND_DATA_VALIDATION keyword.", buffer, vector_name, g_nd_max_resp);
          return -1;
        }
        else if(!(min >= 0))
        {
          MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Received invalid min value in dataline (%s) for vector (%s) as min is less than 0.", buffer, vector_name);
          return -1;
        }
        else if(!((max >= 0) && (max <= g_nd_max_resp)))
        {
          MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Received invalid max in dataline (%s) for vector (%s) as max is not in between the 0 and max limit of max value set (%d) in ND_DATA_VALIDATION keyword.", buffer, vector_name, g_nd_max_resp);
          return -1;
        }

        data_idx += 4;
      }
      else 
      {
        if(local_graph_data_ptr->data_type == DATA_TYPE_RATE)
        {
          if((data_idx == 0) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_bt.gdf") != NULL))
          {
            request_per_sec = atof(field[data_idx]);
            // In cm_nd_bt.gdf Request/Sec graph data is at 0 index
            if(!((request_per_sec >= 0) && (request_per_sec <= g_nd_max_tps)))
            {
              MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Received invalid data (%s) for vector (%s) as Request/Sec is crossing the max tps limit set (%d) in ND_DATA_VALIDATION keyword.", buffer, vector_name, g_nd_max_tps);

              return -1;
            }
          }
          if((data_idx == 1) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_http_header_stats.gdf") != NULL))
          {
            request_per_sec = atof(field[data_idx]);
            //In cm_nd_http_header_stats.gdf Request/Sec graph data is at 1 index
            if(!((request_per_sec >= 0) && (request_per_sec <= g_nd_max_tps)))
            {
              MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Received invalid data (%s) for vector (%s) as Request/Sec is crossing the max tps limit set (%d) in ND_DATA_VALIDATION keyword.", buffer, vector_name, g_nd_max_tps);
              return -1;
            }
          }
        }
        data_idx++;
      }
    }
    local_graph_data_ptr++;
  }
  return 0;
}



/******************* for nd backend monitor *************************/

//data passed is not yet in custom monitor structure hence passing here
void fill_agg_data(CM_info *cus_mon_ptr, double *data, int tieridx, int normvecidx, Group_Info *local_group_data_ptr, Graph_Info *local_graph_data_ptr)
{
  int data_idx = 0, c_graph_numvec;
  int c_graph = 0;

  NSDL1_MON(NULL, NULL, "Monitor_name = %s, local_group_data_ptr = %p, local_graph_data_ptr = %p", cus_mon_ptr->monitor_name, local_group_data_ptr, local_graph_data_ptr);
  // aggregate data at index 0 is set to nan. so memset it to 0
  if((cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[0] != (cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[0])
  {
    memset((cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data, 0, cus_mon_ptr->no_of_element * sizeof(double));
  }

  for(c_graph = 0; c_graph < local_group_data_ptr->num_graphs; c_graph++)
  {
    if(local_graph_data_ptr->graph_state != GRAPH_EXCLUDED)
    {
      for(c_graph_numvec = 0; c_graph_numvec < local_graph_data_ptr->num_vectors; c_graph_numvec++)
      {
      // DATA_TYPE_SAMPLE DATA_TYPE_RATE DATA_TYPE_CUMULATIVE  
        if(IS_TIMES_GRAPH(local_graph_data_ptr->data_type)) // avg min max count 
        {
        //avg
          (cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[data_idx] += (data[data_idx] * data[data_idx + 3]);
           data_idx++;
        //min
          if((cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[data_idx] > data[data_idx])
	    (cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[data_idx] = data[data_idx];
          data_idx++;
        //max
          if((cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[data_idx] < data[data_idx])
            (cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[data_idx] = data[data_idx];
          data_idx++;
        //count            
          (cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[data_idx] += data[data_idx];
          data_idx++;
        }
        else
        {
	  (cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->aggrgate_data[data_idx] += data[data_idx];
	  data_idx++;
        }
      }
    }
    else //for rest types we have to add 
    {
      if (IS_TIMES_GRAPH(local_graph_data_ptr->data_type))
	data_idx += 4;
      else
	data_idx++;
    }

    local_graph_data_ptr++;
  }
  (cus_mon_ptr->tierNormVectorIdxTable[normvecidx][tieridx])->got_data_and_delete_flag |= GOT_DATA;
}

//*********************** NV CODE STARTS ********************//

void kw_set_nv_enable_custom_monitor(char *keyword, char *buf, char *nv_mon_gdf_name)
{
  char key[MAX_CM_MSG_SIZE];
  char buff[10 * MAX_CM_MSG_SIZE];
  char server_name[MAX_CM_MSG_SIZE]={0};
  char err_msg[MAX_CM_MSG_SIZE]={0};
  char hpd_root[MAX_CM_MSG_SIZE];
  int value;

  if(sscanf(buf, "%s %d", key, &value) != 2)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Too few/more arguments for %s keywords.", key);
    NS_EXIT(-1,"Error: Too few/more arguments for %s keywords.", key);
  }

  if(value != 1) //keyword disabled
    return;
  
  if (getenv("HPD_ROOT") != NULL)
    strcpy(hpd_root, getenv("HPD_ROOT")); 
 
  if((!strncmp(g_cavinfo.config, "NV", 2)) || (strstr(g_cavinfo.SUB_CONFIG, "NV") != NULL) || (!strncmp(g_cavinfo.SUB_CONFIG, "ALL", 3))) //check if machine is NV
  {
    if(!hpd_port) //check if hpd port is already obtained or not, if not first get hpd port from file
    {
      get_and_set_hpd_port();
    }
    if (hpd_port == -1){
      NSTL1(NULL, NULL, "Machine configuration is set to NV but env var 'HPD_ROOT' is not set.");  
      return;
    }
    sprintf(server_name, "%s:%d", LOOPBACK_IP_PORT, hpd_port);
    
    sprintf(buff, "CUSTOM_MONITOR %s:%d %s Overall 2 %s/bin/cm_rum_stats -h %s -L DATA -t %s",LOOPBACK_IP_PORT, hpd_port, nv_mon_gdf_name, hpd_root, hpd_root, nv_mon_gdf_name);

    NSDL2_MON(NULL, NULL, "Adding %s", buff);
  
    if(custom_monitor_config("CUSTOM_MONITOR", buff, server_name, 0, g_runtime_flag, err_msg, NULL, NULL, 0) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error msg : %s", err_msg);
      NS_EXIT(-1,"Error msg : %s", err_msg);
    }
    else
      g_mon_id = get_next_mon_id(); 
  }
}

int kw_enable_heroku_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]= "";
  char sendbuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int heroku_mon_state ;
  int num = 0;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  num = sscanf(buf, "%s %d", key, &heroku_mon_state);

  if(num != 2) // All fields are mandatory.
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Too few/more arguments for %s keywords\n", key);
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_HEROKU_MONITOR_USAGE, CAV_ERR_1011359, key); 
  }

  NSDL2_MON(NULL, NULL, "heroku_mon_state = %d", heroku_mon_state);

  if(heroku_mon_state != 0 && heroku_mon_state != 1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: ENABLE_HEROKU_MONITORS must have argument as 0 or 1 only.");
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_HEROKU_MONITOR_USAGE, CAV_ERR_1011360);
  }
  if (heroku_mon_state)
  
  {
     sprintf(sendbuffer,"DYNAMIC_VECTOR_MONITOR %s:%d HEROKU_MON cm_heroku_stats.gdf 2 cm_heroku_stats_data EOC cm_heroku_stats_instance",  global_settings->lps_server, global_settings->lps_port);
     NSDL2_MON(NULL,NULL,"Adding %s",sendbuffer);
     kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR",sendbuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
  }
return 0;
}

int kw_set_unknown_breadcrumb(char *keyword, char *buf)
{
  char key[BUF_LENGTH] = "";
  char temp[BUF_LENGTH];
  int value , args;
  char err_msg[MAX_AUTO_MON_BUF_SIZE];

  args = sscanf(buf, "%s %d %s", key, &value, temp);

  if(args != 2)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Too few/more arguments for %s keywords.\n", key);
    NS_KW_PARSING_ERR(keyword, 0, err_msg, SKIP_UNKNOWN_BREADCRUMB_USAGE, CAV_ERR_1011359,key);
  }
 
  if(value > 1 || value < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Incorrect value for keyword %s. Possible values are 0 (disable by default) / 1 (enable).\n", key);
    NS_KW_PARSING_ERR(keyword, 0, err_msg, SKIP_UNKNOWN_BREADCRUMB_USAGE, CAV_ERR_1060030, key);
  }

  skip_unknown_breadcrumb = value;  
  return 0;
}



/********************************************** Start: NetCloud IP Data Monitor Code ***************************************************/

void read_nc_monitor_data(int cmd_fd, User_trace *vuser_trace_msg)
{
  CM_info *cus_mon_ptr = monitor_list_ptr[nc_ip_data_mon_idx].cm_info_mon_conn_ptr;

  //saving here fd in cm table just to access this in fill_data function,because we need to save each generator fd in 2-d table at index '0'
  cus_mon_ptr->fd = cmd_fd; 
  cus_mon_ptr->dindex += vuser_trace_msg->reply_status;   //We will be getting the size of data received in reply_status.
  cus_mon_ptr->data_buf =  vuser_trace_msg->reply_msg;   //We will be getting data in reply_msg.

  NSDL1_MON(NULL, NULL, "Data received in cus_mon_ptr->data_buf is %s for data connection.", cus_mon_ptr->data_buf);  

  if(checkfornewline(cus_mon_ptr) ==  FAILURE)
  {
    NSDL1_MESSAGES(NULL, NULL, "checkfornewline() failed in case for NetCloud monitor for data connection");
  }

  /* NOTE: HANDLING OF PARTIAL DATA IN REPLY MSG BUFFER AT CONTROOLER IS NOT DONE RIGHT NOW. AS DISCUSSED WITH NETCLOUD TEAM reply_msg will never have partial data. generator will never write partial data into this buffer. If generator writes partial data into reply_msg then at controller we need to handle this. 

    And checkfornewline() will process all the new line separated lines of reply_msg but at the end checkfornewline() will leave first line in reply_msg.
   eg: reply_msg buffer input to checkfornewline() -> genid1:vectorid1:data\ngenid2:vectorid2:data\n
       after checkfornewline() , status of buffer reply_msg will be -> genid1:vectorid1:data\0

   hence we are putting null at 0 index of reply_msg just for safety.
  */
  vuser_trace_msg->reply_msg[0] = '\0'; 
}


void gen_conn_failure_monitor_handling(int fd)
{
  NSDL1_MESSAGES(NULL, NULL, "METHOD CALLED");
  int i, j;
 
  if(fd < 0 || !genVectorPtr)     //Connection is already closed | vector ptr exists
    return;

  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    if(&(genVectorPtr[i][0]) != NULL) 
    {
      if(genVectorPtr[i][0].gen_fd == fd)   //If gen is down then condition will match
      {
        for (j = 0; j < (genVectorPtr[i][0]).num_vector; j++)
        {
          if(genVectorPtr[i][j].data != NULL)
          {
            memset(genVectorPtr[i][j].data, 0, ((monitor_list_ptr[nc_ip_data_mon_idx].cm_info_mon_conn_ptr->no_of_element) * sizeof(double)));
            MLTL1(EL_F, 0, 0, _FLN_, monitor_list_ptr[nc_ip_data_mon_idx].cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Generator whose gen_fd = %d is down. Hence setting this generator's data to 0", fd);
          }
        }
        return;
      }
    }
  }
  NSDL1_MESSAGES(NULL, NULL, "METHOD END");
}


void set_netcloud_data_ptrs(int monitor_idx)
{
  int i;
  CM_info *local_cm_info = NULL;
  CM_vector_info *vector_list = NULL;
  int no_of_monitors = (monitor_idx + monitor_list_ptr[monitor_idx].no_of_monitors);
   
  while(monitor_idx < no_of_monitors)
  {
    local_cm_info = monitor_list_ptr[monitor_idx].cm_info_mon_conn_ptr;
    vector_list = local_cm_info->vector_list;
    for(i=0;i<local_cm_info->total_vectors;i++)
    {
      if (&(genVectorPtr[vector_list[i].generator_id][vector_list[i].vectorIdx]) != NULL)
      {
        vector_list[i].data = (genVectorPtr[vector_list[i].generator_id][vector_list[i].vectorIdx]).data;
        vector_list[i].flags |= DATA_FILLED;
      }
    }
    monitor_idx++;
  }
  return;
}

void reset_ip_data_monitor()
{
  int i,j;
  CM_info *cm_ptr = NULL;
  CM_vector_info *vector_list = NULL;
  for(i = nc_ip_data_mon_idx; i < monitor_list_ptr[nc_ip_data_mon_idx].no_of_monitors; i++)
  {
    cm_ptr =  monitor_list_ptr[i].cm_info_mon_conn_ptr;
    vector_list = cm_ptr->vector_list;
    for(j = 0; j < cm_ptr->total_vectors;j++)
      memset(vector_list[j].data, 0, (cm_ptr->no_of_element * sizeof(double)));
  }
}

/************************ End: NetCloud IP Data Monitor Code ****************************/

void apply_monitors_from_json()
{
  char temp_monitor_buf[32*MAX_DATA_LINE_LENGTH]={0};
  char err_msg[32*MAX_DATA_LINE_LENGTH]={0};
  int i;

  for(i=0; i<total_json_monitors; i++)
  {
    
    strcpy(temp_monitor_buf, json_monitors_ptr[i].mon_buff);
    if(json_monitors_ptr[i].mon_type == CHECK_MONITOR)
     {
       if(kw_set_check_monitor("CHECK_MONITOR", temp_monitor_buf, 1, err_msg, json_monitors_ptr[i].json_info_ptr) < 0 )
            ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                  "Failed to apply json auto monitor with buffer: %s . Error: %s", json_monitors_ptr[i].mon_buff, err_msg);
       else
       {
          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Check Monitor applied through json with buffer [%s].\n", json_monitors_ptr[i].mon_buff);
          monitor_runtime_changes_applied = 1;
       }

     }
    
    else if(json_monitors_ptr[i].mon_type == BATCH_JOB)
    {
      if(parse_job_batch(temp_monitor_buf, 1,err_msg, " ", json_monitors_ptr[i].json_info_ptr) < 0 )
            ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                  "Failed to apply json auto monitor with buffer: %s . Error: %s", json_monitors_ptr[i].mon_buff, err_msg);
      else
      {
          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Batch Job Monitor applied through json with buffer [%s].\n", json_monitors_ptr[i].mon_buff);
          monitor_runtime_changes_applied = 1;
      }

    }
    
    else if(json_monitors_ptr[i].mon_type == STANDARD_MONITOR)
    {
      if(kw_set_standard_monitor("STANDARD_MONITOR", temp_monitor_buf, 1, json_monitors_ptr[i].pod_name, err_msg,json_monitors_ptr[i].json_info_ptr) < 0 )
    {   
       ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                  "Failed to apply json auto monitor with buffer: %s . Error: %s", json_monitors_ptr[i].mon_buff, err_msg);
    } 
      else
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Standard Monitor applied through json with buffer [%s].\n", json_monitors_ptr[i].mon_buff);
        monitor_runtime_changes_applied = 1;
      }
    }
    else if(json_monitors_ptr[i].mon_type == SERVER_SIGNATURE)
    {
      if(kw_set_server_signature("SERVER_SIGNATURE", temp_monitor_buf, 1, err_msg, json_monitors_ptr[i].json_info_ptr) < 0 )
            ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                  "Failed to apply json auto monitor with buffer: %s . Error: %s", json_monitors_ptr[i].mon_buff, err_msg);
      else
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "SERVER SIGNATURE applied through json with buffer [%s].\n", json_monitors_ptr[i].mon_buff);
        monitor_runtime_changes_applied = 1;
      }
    }

    else if(json_monitors_ptr[i].mon_type == LOG_MONITOR)
    {
      if(custom_monitor_config("LOG_MONITOR",temp_monitor_buf, NULL, 0, 1, err_msg, NULL, json_monitors_ptr[i].json_info_ptr, 0) >= 0)
        {
          g_mon_id = get_next_mon_id();
          monitor_added_on_runtime = 1;
        }
        else
         MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                      "Failed to apply json auto monitor with buffer: %s . Error: %s", json_monitors_ptr[i].mon_buff, err_msg);
    }
    //This type is custom-gdf-mon type. It will be added here as custom monitor.
    else if(json_monitors_ptr[i].mon_type == CUSTOM_GDF_MON)
    {
      if(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR",temp_monitor_buf,NULL, 0, 1, NULL , err_msg, 0,json_monitors_ptr[i].json_info_ptr, json_monitors_ptr[i].json_info_ptr->skip_breadcrumb_creation) >= 0)
      {
        monitor_added_on_runtime = 1;   
      }
      else
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                      "Failed to apply json auto monitor with buffer: %s . Error: %s", json_monitors_ptr[i].mon_buff, err_msg);
    }

    //free(json_monitors_ptr[i].mon_buff);
    FREE_AND_MAKE_NULL(json_monitors_ptr[i].mon_buff, "json_monitors_ptr[i].mon_buff", i);
    //free(json_monitors_ptr[i].pod_name);
    FREE_AND_MAKE_NULL(json_monitors_ptr[i].pod_name, "json_monitors_ptr[i].pod_name", i);
    if(json_monitors_ptr[i].json_info_ptr)
    {
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->vectorReplaceFrom, "json_monitors_ptr[i].json_info_ptr->vectorReplaceFrom", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->vectorReplaceTo, "json_monitors_ptr[i].json_info_ptr->vectorReplaceTo", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->javaClassPath, "json_monitors_ptr[i].json_info_ptr->javaClassPath", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->javaHome, "json_monitors_ptr[i].json_info_ptr->javaHome", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->init_vector_file, "json_monitors_ptr[i].json_info_ptr->init_vector_file", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->namespace, "json_monitors_ptr[i].json_info_ptr->namespace", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->instance_name, "json_monitors_ptr[i].json_info_ptr->instance_name", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->args, "json_monitors_ptr[i].json_info_ptr->args", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->mon_name, "json_monitors_ptr[i].json_info_ptr->mon_name", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->config_json, "json_monitors_ptr[i].json_info_ptr->config_json", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->use_agent, "json_monitors_ptr[i].json_info_ptr->use_agent", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->os_type, "json_monitors_ptr[i].json_info_ptr->os_type", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->g_mon_id, "json_monitors_ptr[i].json_info_ptr->g_mon_id", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->app_name, "json_monitors_ptr[i].json_info_ptr->app_name", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->pgm_type, "json_monitors_ptr[i].json_info_ptr->pgm_type", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->tier_name, "json_monitors_ptr[i].json_info_ptr->tier_name", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr->server_name, "json_monitors_ptr[i].json_info_ptr->server_name", i);
      FREE_AND_MAKE_NULL(json_monitors_ptr[i].json_info_ptr, "json_monitors_ptr[i].json_info_ptr", i);
      
    }
  }

  total_json_monitors=0;
  max_json_monitors=0;
  FREE_AND_MAKE_NULL(json_monitors_ptr, "json_monitors_ptr", -1);
}

/********************** START : KUBERNETES NODE POD *****************************/

long long calculate_timestamp_from_date_string(char * buffer)
{
  char tmp_buf[1024];
  struct tm tm;
  long long start_time_in_sec;

  memset(&tm, 0, sizeof(struct tm));
  //Format of Date  : 2017-04-08 16:44:13
  //2017-04-08 16:44:13
  tm.tm_isdst = -1;   //auto find daylight saving
  strptime(buffer, "%Y-%m-%d %H:%M:%S", &tm);
  strftime(tmp_buf, 1024, "%s", &tm);
  start_time_in_sec = atoll(tmp_buf);

  return start_time_in_sec;
}


int parse_kubernetes_vector_format(char *tmp_vector_name, char *field[10])
{
  char hv_seperator[2] = {0};
  int num_field;
  char *temp_ptr;
  hv_seperator[0] = global_settings->hierarchical_view_vector_separator;

  num_field = get_tokens_with_multi_delimiter(tmp_vector_name, field, hv_seperator, 10);
  if((num_field > 10) && (num_field < 7))
  {
    return -1;
  }

  temp_ptr = strchr(field[0], 'T');
  if(temp_ptr)
    *temp_ptr = ' ';

  temp_ptr = strchr(field[0], 'Z');
  if(temp_ptr)
    *temp_ptr = '\0';

  temp_ptr = strchr(field[3], 'T');
  if(temp_ptr)
    *temp_ptr = ' ';

  temp_ptr = strchr(field[3], 'Z');
  if(temp_ptr)
    *temp_ptr = '\0';
  
  return num_field;
}


//Loop Node List and check if any changes done in node / pod
//0-Node
//2-Pod
//1-Discovery
//0-Shutdown
void send_node_pod_status_message_to_ndc()
{
  int i, dm;
  int delete_node=0;

  CM_info *cm_ptr = NULL;
  NodePodInfo *temp;
  NodePodInfo *prev;

  //loop node_list
  for(i=0;i<max_node_list_entries;i++)
  {
    //node discovery
    if(node_list[i].flag & ADD_NODE)
    {
      //This will be sent for 1st pod in AddList
      if(send_node_pod_status_to_ndc(node_list[i].AddedList, 0, 1) != -1)
        NSTL2(NULL, NULL, "Msg send to ndc for new node with norm node id = %d and flag is resetted", i);

      node_list[i].flag &= ~ADD_NODE;
    }

    if (node_list[i].flag & ADD_POD)
    {
      if(node_list[i].AddedList != NULL)
      {
        temp = node_list[i].AddedList;
        //Loop through addedlist, send message to NDC for all newly added pod
        // Maintain a prev pointer while traversing
        // Set prev which is last node in added list to head. and point head to addedlist
        while(temp != NULL)
        {
          send_node_pod_status_to_ndc(temp, 2, 1);
          prev = temp;
          temp = temp->next;
        }
        prev->next = node_list[i].head;
        //node_list[norm_node_id]->flag &= RESET;
        node_list[i].head=node_list[i].AddedList;//copy addedlist to head
        node_list[i].AddedList = NULL;

        NSTL2(NULL, NULL, "Msg send to ndc for new pods for norm node id = %d and flag is resetted", i);
      }
      node_list[i].flag &= ~ADD_POD;
    }
    //pod delete
    //LOOP pod list
    if(node_list[i].flag & DELETE_POD)
    { 
      if(node_list[i].DeletedList != NULL)
      {
        //Node Shutdown
        //Node shutdown will be send when head & addedList is NULL deletedList is not null
        if(node_list[i].head == NULL && node_list[i].AddedList == NULL)
        {
          send_node_pod_status_to_ndc(node_list[i].DeletedList, 0, 0);
          delete_node = 1;
          NSTL2(NULL, NULL, "Msg send to ndc for node delete with norm node id = %d", i);
        }
        while(node_list[i].DeletedList != NULL)
        {
          send_node_pod_status_to_ndc(node_list[i].DeletedList, 2, 0);
          NSTL2(NULL, NULL, "Msg send to ndc for pod delete from norm node id = %d", i);

          //deleting all monitors applied on deleted pod
          for(dm = 0; dm < total_monitor_list_entries; dm++)
          {
            cm_ptr = monitor_list_ptr[dm].cm_info_mon_conn_ptr;

            if(cm_ptr->pod_name && !strcmp(cm_ptr->pod_name, node_list[i].DeletedList->PodName))
            {
              ns_cm_monitor_log(EL_F, 0, 0, _FLN_, cm_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
              "Going to delete '%s', as this was applied on pod '%s' and this pod is deleted.", cm_ptr->monitor_name, node_list[i].DeletedList->PodName);
              handle_monitor_stop(cm_ptr, MON_DELETE_ON_REQUEST);
              monitor_runtime_changes_applied=1;
            }
          }
          prev = node_list[i].DeletedList;
          node_list[i].DeletedList = prev->next;
          if(node_list[i].DeletedList == NULL && delete_node)
          {
            delete_node=0;
            node_list_entries--;
            int norm_id;
            node_list[i].CmonPodIp[0] = '\0';
            node_list[i].NodeIp[0] = '\0';
            FREE_AND_MAKE_NULL(node_list[i].CmonPodName, "Node name", -1);
            nslb_delete_norm_id_ex(node_id_key, prev->NodeName, strlen(prev->NodeName), &norm_id);
            NSTL2(NULL, NULL, "Node = %s with norm node id = %d is deleted and node_list_entries = %d",prev->NodeName, i, node_list_entries);
          }
          free_node_pod(prev);
        }
        node_list[i].DeletedList = NULL;
      }
      node_list[i].flag &= ~DELETE_POD;
    }
  }
}


// This function will take argument in node_pod_type as 0 when node, and 2 when pod. In discovery_shutdown, 1 for discovery and 0 for shutdown
int send_node_pod_status_to_ndc(NodePodInfo *node_pod_info_ptr, int node_pod_type, int discovery_shutdown)
{
  int fd;
  char msg_buff[1024];
  char state;
  long long start_time;

  if(node_pod_info_ptr == NULL)
    return -1;

  state = ndc_data_mccptr.state;
  fd = ndc_data_mccptr.fd;

  if(node_pod_type == 0)
    start_time = node_pod_info_ptr->node_start_time;
  else if(node_pod_type == 2)
    start_time = node_pod_info_ptr->pod_start_time;

  if(fd != -1)
  {
    if(state & NS_CONNECTED)
    {
      sprintf(msg_buff, "nd_data_req:action=kubernetes_node_pod_info;type=%d;node_pod_start_time=%lld;node_ip=%s;node_name=%s;pod_name=%s;operation=%d;\n", node_pod_type, start_time, node_pod_info_ptr->NodeIp, node_pod_info_ptr->NodeName, node_pod_info_ptr->PodName, discovery_shutdown);
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                 "Sending MSG to NDC. Message: %s", msg_buff);
      if (send(fd, msg_buff, strlen(msg_buff), 0) != strlen(msg_buff))
      {

        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Net Diagnostics Server IP %s, Port = %d, Error = %s\n ,fd = %d",
                      global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, nslb_strerror(errno),fd);
        CLOSE_FD(ndc_mccptr.fd);
        return -1; //error
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "State is not connected"
                      "Net Diagnostics Server IP %s, Port = %d, state = %c",
                      global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, state); 
      return -1;
    }
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "fd is negative so not able to send the message."
             "Net Diagnostics Server IP %s, Port = %d., state = %c",
               global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, state); 
  }
  return 0;
}


/********************* END : KUBERNETES NODE POD ***********************************/

//This keyword will decide for how much entry should DR table be malloced initially, and upto what max limit will it be realloced. And every reallocation will be with the delta entry mentioned in the keyword.
void kw_set_allocate_size_for_dr_table(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  int num, init_size, delta_size, max_size;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  num = sscanf(buf, "%s %d %d %d", key, &init_size, &delta_size, &max_size);

  if(num != 4)
  {
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Wrong usage of keyword %s. Buffer = %s. USAGE (DR_TABLE_SIZE <init_entry> <delta_entry> <max_entry>. Hence allocating default entry for this keyword.", keyword, buf);
    init_size_for_dr_table = 0;
    delta_size_for_dr_table = 500;
    max_size_for_dr_table = 10000;
  }
  else
  {
    init_size_for_dr_table = init_size;
    delta_size_for_dr_table = delta_size;
    max_size_for_dr_table = max_size;
  }

  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "DR table allocation: init entry = %d, delta entry = %d, max entry = %d", init_size_for_dr_table, delta_size_for_dr_table, max_size_for_dr_table);
}


//There was an issue where cohenerece vector were coming with different server name from server name mentioned in Server.conf. Eg: server mnetioned in topology is like server1, server2. And when data comes its vectors be like server1-1, server2-1 .. This keyword will enable vector correction which will remove the extra character coming at the end.
void kw_coherenece_remove_trailing_space(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  int num;
   
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  num = sscanf(buf," %s %d %s", key, &g_remove_trailing_char,g_trailing_char);
  if(num != 3)
  {
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Wrong usage of keyword %s. Buffer = %s. USAGE (COHERENCE_REMOVE_TRAILING_CHARACTER <1/0> <character>. Disabling this keyword and continuing.", keyword, buf);
    g_remove_trailing_char = 0;
  }

  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "COHERENCE_REMOVE_TRAILING_CHARACTER keyword has been parsed. Values are : g_remove_trailing_char = %d, g_trailing_char = %s", g_remove_trailing_char, g_trailing_char); 
}                                                                                             


//making entry in hash table
int get_norm_id_for_vectors(char *vector_name, char *gdf_name, int *new_flag)
{
  int vec_len;
  int norm_id = -1;

  char buffer[4096 + 1];

  vec_len = snprintf(buffer, 4096, "%s_%s", gdf_name, vector_name);
  buffer[vec_len] = '\0';

  norm_id = nslb_get_or_gen_norm_id(dyn_cm_rt_hash_key, buffer, vec_len, new_flag);

  return norm_id;
}


void initialize_dyn_cm_rt_hash_key()
{
  if(global_settings->dynamic_cm_rt_table_mode && (dyn_cm_rt_hash_key == NULL)) //check with mode
  {
    MY_MALLOC(dyn_cm_rt_hash_key, sizeof(NormObjKey), "Memory allocation to Norm Vector table", -1);
    nslb_init_norm_id_table(dyn_cm_rt_hash_key, global_settings->dynamic_cm_rt_table_size);
    NSDL2_MON(NULL, NULL, "dyn_cm_rt_hash_key has been initialized. dyn_cm_rt_hash_key = %p", dyn_cm_rt_hash_key);
  }
}

void get_group_id_from_gdf_name(char *fname, char group_id[])
{
  FILE *read_gdf_fp;
  int num_fields;
  char *fields[15];
  char line[MAX_LINE_LENGTH + 1];

  read_gdf_fp = open_gdf(fname);
  while (fgets(line, MAX_LINE_LENGTH, read_gdf_fp) !=NULL)
  {
    line[strlen(line) - 1] = '\0';
    NSDL3_GDF(NULL, NULL, "line = %s", line);

    num_fields = get_tokens(line, fields, "|", 15);

    if(num_fields<=0)
      continue;

    if(fields[0][0] == '#' || fields[0][0] == '\0')
      continue;

    else if(!(strncasecmp(fields[0], "info", strlen("info"))))
      continue;

    else if((!(strncasecmp(fields[0], "group", strlen("group")))))
    {
      strcpy(group_id, fields[2]);
      break;
    }
    else if(!(strncasecmp(line, "graph|", strlen("graph|"))))
      continue;
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Invalid line = %s\n", line);
    }
  }
  NSDL3_GDF(NULL, NULL, "Closing gdf");
  close_gdf(read_gdf_fp);

  return;
}

//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX MBEAN Mon Code Starts XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

int search_mon_entry_in_list(char *monitor_name)
{
  int i;
  
  for(i = 0; i < total_mbean_mon; i++)
  {
    if(!strcmp(monitor_name, mbean_mon_ptr[i].mon_name))
      
      return i;
  }
  return -1;
}

CM_info *find_entry_in_cm_info(char *gdf_name)
{
  int i;
  
  //We know that only one parent vector will be present per group in cm_info. So searching would be accurate and easy.
  for(i = 0; i < total_monitor_list_entries;i++)
  {
    if((monitor_list_ptr[i].cm_info_mon_conn_ptr->flags & ND_MONITOR)
        && strstr(monitor_list_ptr[i].cm_info_mon_conn_ptr->gdf_name, gdf_name))
      return (monitor_list_ptr[i].cm_info_mon_conn_ptr);
  }
  return NULL;
}

void free_mbean_json_info(MBeanJsonInfo *json_info) {
  free(json_info->tier_group_info);

  for(int i = 0; i < json_info->total_tier; ++i)
    free(json_info->tier_info[i].tier_name);
  free(json_info->tier_info);

  for(int i = 0; i < json_info->total_exclude_tier; ++i)
    free(json_info->exclude_tier[i]);
  free(json_info->exclude_tier);

  for(int i = 0; i < json_info->total_exclude_server; ++i)
    free(json_info->exclude_server[i]);
  free(json_info->exclude_server);

  for(int i = 0; i < json_info->total_specific_server; ++i)
    free(json_info->specific_server[i]);
  free(json_info->specific_server);
}

MBeanJsonInfo *search_tier_in_mbean_info_tier_list(char *tier_name, int index, char *tier_group, char if_excluded)
{
  int j;
  MBeanJsonInfo *json_info;

  json_info = mbean_mon_ptr[index].mbean_json_info_pool.busy_head;
   
  while (json_info != NULL)
  {
    if(tier_group)
    {
      //Tier group match.
      if(strcmp(tier_group, json_info->tier_group_info->tier_group) == 0)
      {
        if(!(if_excluded & DELETED_TIER)) {  // NOT excluded
          // No need to mark deleted. The slot will be freed
        }
        return json_info;
      }
    }
    else
    { 
      for(j = 0; j < json_info->total_tier; j++)
      {
        //Tier match
        if(strcmp(tier_name, json_info->tier_info[j].tier_name) == 0)
        {
          if(!(if_excluded & DELETED_TIER)) {  // NOT excluded
            // set deleted_tier and decrease number of active tier by 1
            json_info->tier_info[j].flag |= DELETED_TIER;
            json_info->total_active_tier--;
          }

          return json_info;
        }
      }
    } 
    json_info = nslb_next(json_info);
  }
  return NULL;
}

// These arguments are pass through make_and_send_del_msg_to_NDC
void fill_details_for_end_monitor_req(char *msg_buffer, char **exclude_tier, int no_of_exclude_tier, char **specific_server,
  int no_of_specific_server, char **exclude_server, int no_of_exclude_server, int *msg_len)
{
  if(no_of_exclude_tier > 0) {
    *msg_len += sprintf(msg_buffer + *msg_len, "EXCLUDE_TIER=");

    for(int k = 0; k < no_of_exclude_tier; k++) {
      if(k == (no_of_exclude_tier - 1))
        *msg_len += sprintf(msg_buffer + *msg_len, "%s:", exclude_tier[k]);
      else
        *msg_len += sprintf(msg_buffer + *msg_len, "%s,", exclude_tier[k]);
    }
  }
	
  if(no_of_specific_server > 0) {
    *msg_len += sprintf(msg_buffer + *msg_len, "SEPCIFIC_SERVER=");

    for(int k = 0; k < no_of_specific_server; k++) {
      if(k == (no_of_specific_server - 1))
        *msg_len += sprintf(msg_buffer + *msg_len, "%s:", specific_server[k]);
      else
        *msg_len += sprintf(msg_buffer + *msg_len, "%s,", specific_server[k]);
    }
  }

  if(no_of_exclude_server > 0) {
    *msg_len += sprintf(msg_buffer + *msg_len, "EXCLUDE_SERVER=");

    for(int k = 0; k < no_of_exclude_server; k++) {
      if(k == (no_of_specific_server - 1))  
        *msg_len += sprintf(msg_buffer + *msg_len, "%s:", exclude_server[k]); 
      else  
        *msg_len += sprintf(msg_buffer + *msg_len, "%s,", exclude_server[k]); 
    }
  }

}


int make_and_send_del_msg_to_NDC(char **tiername, int no_of_tiers, char **exclude_tier, int no_of_exclude_tier, char **specific_server, int no_of_specific_server, char **exclude_server, int no_of_exclude_server, int tier_group_index, char *instance_name, char *gdf_name)
{
  int i;
  char msg_buffer[MAX_MONITOR_BUFFER_SIZE];
  msg_buffer[0] = '\0';
  int msg_len=0;
  CM_info *cus_mon_ptr;
  MBeanJsonInfo *json_info;
  
  cus_mon_ptr = find_entry_in_cm_info(gdf_name);
  if(cus_mon_ptr == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Could not find entry for gdf (%s) in main CM table", gdf_name);
    return -1;
  }
  //Moving this check on top as we store tier pattern/list in tiername when tiergroup is passed. 
  //Earlier end mon request is sent with tier pattern but now we are sending tier group name.
  if(tier_group_index >= 0)
  {
    json_info = search_tier_in_mbean_info_tier_list(NULL, cus_mon_ptr->mbean_mon_idx, topo_info[topo_idx].topo_tier_group[tier_group_index].GrpName, 0);
    
    if(json_info == NULL)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Invalid tier-group deletion request. Provided tier-group (%s) does not mbean monitor to be deleted.",
              topo_info[topo_idx].topo_tier_group[tier_group_index].GrpName);
      return -1;
    }

    msg_len += sprintf(msg_buffer + msg_len, "end_monitor:MON_NAME=%s;RECORD_ID=%d;TIER_GROUP=%s:", mbean_mon_ptr[cus_mon_ptr->mbean_mon_idx].mon_name,
      mbean_mon_ptr[cus_mon_ptr->mbean_mon_idx].record_id, topo_info[topo_idx].topo_tier_group[tier_group_index].GrpName);

    // fill exclude_server/exclude_tier/specific_server. NDC expects the config in end_monitor msg
    fill_details_for_end_monitor_req(msg_buffer, exclude_tier, no_of_exclude_tier, specific_server, no_of_specific_server, exclude_server,
      no_of_exclude_server, &msg_len);

    // free json_info and return slot to mp pool
    free_mbean_json_info(json_info);
    nslb_mp_free_slot(&(mbean_mon_ptr[cus_mon_ptr->mbean_mon_idx].mbean_json_info_pool), json_info);
  }
  else if(no_of_tiers > 0)
  {
    int flag = 0;

    msg_len += sprintf(msg_buffer + msg_len, "end_monitor:MON_NAME=%s;RECORD_ID=%d;", mbean_mon_ptr[cus_mon_ptr->mbean_mon_idx].mon_name,
         mbean_mon_ptr[cus_mon_ptr->mbean_mon_idx].record_id);

    for(i = 0; i < no_of_tiers; i++)
    {
      json_info = search_tier_in_mbean_info_tier_list(tiername[i], cus_mon_ptr->mbean_mon_idx, NULL, 0);

      if(json_info == NULL)
        continue;

      flag = 1;
      msg_len += sprintf(msg_buffer + msg_len, "TIER=%s:", tiername[i]);
      
      fill_details_for_end_monitor_req(msg_buffer, exclude_tier, no_of_exclude_tier, specific_server, no_of_specific_server, exclude_server,
        no_of_exclude_server, &msg_len);
    }
    
    // free if all tiers/tier_groups are deleted
    if(json_info && json_info->total_active_tier == 0) {
      free_mbean_json_info(json_info);
      nslb_mp_free_slot(&(mbean_mon_ptr[cus_mon_ptr->mbean_mon_idx].mbean_json_info_pool), json_info);
    }
    
    // Did not find any tier/tier_group in MBeanJsonInfo. Deleting an already deleted tier/tier_group
    if (!flag)
      return 0;
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Invalid delete request. Please provide either tier");
    return -1;
  }

  int len = strlen(msg_buffer);
  msg_buffer[len - 1] = '\0';  /* this is to remove last ':' in this message buffer.*/ \
  strcat(msg_buffer, ";\n");

  MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Going to send msg to NDC for delete request for monitor group (%s): %s", cus_mon_ptr->gdf_name, msg_buffer); 
  cm_send_msg(cus_mon_ptr, msg_buffer, 1);
  return 0;
}

void add_entry_for_mbean_mon(char *monitor_name, char **tiername, int no_of_tiers, char **exclude_tier, int no_of_exclude_tier, char **specific_server, int no_of_specific_server, char **exclude_server, int no_of_exclude_server, int tier_group_index, char *gdf_name, int runtime_flag)
{
  int mon_idx = -1;
  int i;
  MBeanJsonInfo *json_info;

  // Serach if monitor already exist in table
  mon_idx = search_mon_entry_in_list(monitor_name);
  //Didnot found entry in mbean structure. Need to add a new row to this column.
  if(mon_idx == -1)
    mon_idx = total_mbean_mon;
  else
    if (runtime_flag)
      mbean_mon_ptr[mon_idx].flag |= UPDATED_NDC_MON; // entry found, mark updated ndc mon if in runtime

  // Realloc if needed. 
  if(total_mbean_mon >= max_mbean_mon)
  {
    MY_REALLOC_AND_MEMSET(mbean_mon_ptr, ((max_mbean_mon + DELTA_MON_ID_ENTRIES) * sizeof(MBeanMonInfo)), (max_mbean_mon * sizeof(MBeanMonInfo)), "Reallocating mbean_mon_ptr", -1);

    i = max_mbean_mon;
    max_mbean_mon += DELTA_MON_ID_ENTRIES;

    // init memory pool for newly added entries
    for ( ; i < max_mbean_mon; ++i) {
      nslb_mp_init(&(mbean_mon_ptr[i].mbean_json_info_pool), sizeof(MBeanJsonInfo), DELTA_MON_ID_ENTRIES, DELTA_MON_ID_ENTRIES, NON_MT_ENV);
      nslb_mp_create(&(mbean_mon_ptr[i].mbean_json_info_pool));
    }
  }
 
  if (mbean_mon_ptr[mon_idx].gdf_name == NULL)
    MALLOC_AND_COPY(gdf_name, mbean_mon_ptr[mon_idx].gdf_name, strlen(gdf_name) + 1, "Copying gdf_name", 0);

  json_info = (MBeanJsonInfo*)nslb_mp_get_slot(&(mbean_mon_ptr[mon_idx].mbean_json_info_pool));

  // tier group cannot have list of tier group, always one
  if(tier_group_index >= 0)
  {
    
    MY_MALLOC_AND_MEMSET(json_info->tier_group_info, (sizeof(TierGroupInfo)), "TierGroupInfo structure", -1);
    MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_group[tier_group_index].GrpName, json_info->tier_group_info->tier_group,
        (strlen(topo_info[topo_idx].topo_tier_group[tier_group_index].GrpName) + 1), "Saving Tier group name", -1);
   
    if (runtime_flag)
      json_info->tier_group_info->flag |= ADDED_TIER;
  }
  //Saving tier list
  else if(no_of_tiers > 0)
  {
    MY_MALLOC_AND_MEMSET(json_info->tier_info, (no_of_tiers * sizeof(TierInfo)), "tier list", -1);

    for(i = 0; i < no_of_tiers; i++)
    { 
      MALLOC_AND_COPY(tiername[i], json_info->tier_info[i].tier_name, (strlen(tiername[i]) + 1), "Saving Tier name", i);

      // mark ADDED_TIER if in runtime so that we can send cm_init only for the newly added tiers
      if (runtime_flag)
        json_info->tier_info[i].flag |= ADDED_TIER;
    }

    json_info->total_active_tier = no_of_tiers;     
    json_info->total_tier = no_of_tiers;
  }

  //Saving exclude tier list
  if(no_of_exclude_tier > 0)
  {
    MY_MALLOC(json_info->exclude_tier, (no_of_exclude_tier * sizeof(char*)), "exclude tier list", -1);

    for(i = 0; i < no_of_exclude_tier; i++)
    {
      MALLOC_AND_COPY(exclude_tier[i], json_info->exclude_tier[i], (strlen(exclude_tier[i]) + 1), "Saving Exclude Tier name", i);
    }
    json_info->total_exclude_tier = no_of_exclude_tier;
  }

  //Saving specific server list
  if(no_of_specific_server > 0)
  {
    MY_MALLOC(json_info->specific_server, (no_of_specific_server * sizeof(char*)), "specific server list", -1);

    for(i = 0; i < no_of_specific_server; i++) 
    {
      MALLOC_AND_COPY(specific_server[i], json_info->specific_server[i], (strlen(specific_server[i]) + 1), "Saving Specific server name", -1);
    }
    json_info->total_specific_server = no_of_specific_server;
  }
   
  //Saving exclude server list 
  if(no_of_exclude_server > 0)
  {
    MY_MALLOC(json_info->exclude_server, (no_of_exclude_server * sizeof(char*)), "exclude server list", -1);

    for(i = 0; i < no_of_exclude_server; i++)
    {
      MALLOC_AND_COPY(exclude_server[i], json_info->exclude_server[i], (strlen(exclude_server[i]) + 1), "Saving Exclude server name", -1);
    }

    json_info->total_exclude_server = no_of_exclude_server;
  }

   
  if(mon_idx == total_mbean_mon)   //Entry not found
  {
    MALLOC_AND_COPY(monitor_name, mbean_mon_ptr[total_mbean_mon].mon_name, strlen(monitor_name) + 1, "Monitor name in mbean_mon_ptr", total_mbean_mon);
    total_mbean_mon++;
  }
}

void make_message_for_mbean(int i, char op, int length) 
{
  int k, len =0;
  int flag;
  char tier_buf[2048];
  int tier_buf_len =0;
  char buffer[64 * 1024];
  MBeanJsonInfo *json_info;

  json_info = (MBeanJsonInfo*)mbean_mon_ptr[i].mbean_json_info_pool.busy_head;

  while (json_info != NULL)
  {
    //Tier Group can only be one per json
    if(json_info->tier_group_info) {
      if (op == ADDED_TIER) {
        if (!(json_info->tier_group_info->flag & ADDED_TIER))
          continue;

        json_info->tier_group_info->flag &= ~ADDED_TIER;
      } 
      len += sprintf(buffer + len, "TIER_GROUP=%s:", json_info->tier_group_info->tier_group);
      copy_to_scratch_buffer_for_monitor_request(buffer, len, RESET_SCRATCH_BUF, length);     
    }    
    //Either it can be tier or tier-group in one json block.
    else if(json_info->tier_info)
    {
      flag = 0;
      tier_buf_len = sprintf(tier_buf, "TIER=");

      if (op == ADDED_TIER)
      {
        for(k = 0; k < json_info->total_tier; k++)
        {
          if(!(json_info->tier_info[k].flag & ADDED_TIER))
            continue;

          flag = 1;
          // unset ADDED_TIER
          json_info->tier_info[k].flag &= ~ADDED_TIER;
          
          if(k == (json_info->total_tier-1))
            tier_buf_len += sprintf(tier_buf + tier_buf_len, "%s:", json_info->tier_info[k].tier_name);
          else
            tier_buf_len += sprintf(tier_buf + tier_buf_len, "%s,", json_info->tier_info[k].tier_name);
        }
        
      }
      else
      {
        for(k = 0; k < json_info->total_tier; k++)
        {
          if(json_info->tier_info[k].flag & DELETED_TIER)
            continue;

          flag = 1;

          if(k == (json_info->total_tier-1))
            tier_buf_len += sprintf(tier_buf + tier_buf_len, "%s:", json_info->tier_info[k].tier_name);
          else
            tier_buf_len += sprintf(tier_buf + tier_buf_len, "%s,", json_info->tier_info[k].tier_name);
        }
      }
      if (flag)
        copy_to_scratch_buffer_for_monitor_request(tier_buf, tier_buf_len, RESET_SCRATCH_BUF, length);
      else {
        json_info = nslb_next(json_info);
        continue;
      }
    }

    if(json_info->exclude_tier)
    {
      len = sprintf(buffer, "EXCLUDE_TIER=");
      for(k = 0; k < json_info->total_exclude_tier; k++)
      {
        if(k == (json_info->total_exclude_tier-1))
          len += sprintf(buffer + len, "%s:", json_info->exclude_tier[k]);
        else
          len += sprintf(buffer + len, "%s,", json_info->exclude_tier[k]);
      }
      copy_to_scratch_buffer_for_monitor_request(buffer, len, DONT_RESET_SCRATCH_BUF, 0);
    }

    if(json_info->exclude_server)
    {
      len = sprintf(buffer, "EXCLUDE_SERVER=");
      for(k = 0; k < json_info->total_exclude_server; k++)
      {
        if(k == (json_info->total_exclude_server-1))
          len += sprintf(buffer + len, "%s:", json_info->exclude_server[k]);
        else
          len += sprintf(buffer + len, "%s,", json_info->exclude_server[k]);
      }
      copy_to_scratch_buffer_for_monitor_request(buffer, len, DONT_RESET_SCRATCH_BUF, 0);
    }

    if(json_info->specific_server)
    {
     len = sprintf(buffer, "SPECIFIC_SERVER=");
      for(k = 0; k < json_info->total_specific_server; k++)
      {
        if(k == (json_info->total_specific_server-1))
          len = sprintf(buffer + len, "%s:", json_info->specific_server[k]);
        else
          len = sprintf(buffer + len, "%s,", json_info->specific_server[k]);
      }
      copy_to_scratch_buffer_for_monitor_request(buffer, len, DONT_RESET_SCRATCH_BUF, 0);
    }

    monitor_scratch_buf[monitor_scratch_buf_len - 1] = '\0';       // this is to remove last ':' in this message buffer.
    copy_to_scratch_buffer_for_monitor_request(";", 1, RESET_SCRATCH_BUF, monitor_scratch_buf_len - 1);

    json_info = nslb_next(json_info);
  }
  // mbean_mon_ptr[i].runtime_offset = mbean_mon_ptr[i].total_mon_entries;
}

void add_in_cm_table_and_create_msg(int runtime_flag)
{
  int i;
  char buffer[MAX_MONITOR_BUFFER_SIZE];
  char err_msg[MAX_MONITOR_BUFFER_SIZE];
  char *encoded_format = NULL;
  char msg_buf[MAX_MONITOR_BUFFER_SIZE];     //64 K buff size is enough
  //char tmp_msg_buf[MAX_MONITOR_BUFFER_SIZE];
  char *buff = NULL;  
  char *tmp;
  int num_bytes;
  int length;
  int global_msg_length;

  NSDL4_MON(NULL, NULL, "Method called. runtime_flag = %d", runtime_flag);

  for(i = 0; i < total_mbean_mon; i++) 
  {
    if(mbean_mon_ptr[i].record_id < 0)
      continue;

    //Donot add in cm table if already added. Just need to update tier entries from JSON.
    if(mbean_mon_ptr[i].flag & UPDATED_NDC_MON || i >= mbean_monitor_offset)
    {
      // Only for newly added MBean monitor
      if (i >= mbean_monitor_offset)
      {
        //Adding in CM table.
        sprintf(buffer, "STANDARD_MONITOR %s:%d %s %s", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port,
          mbean_mon_ptr[i].mon_name, mbean_mon_ptr[i].mon_name);
      
        if(kw_set_standard_monitor("STANDARD_MONITOR", buffer, runtime_flag, 0,err_msg, NULL) == -1)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                "Error while applying MBean monitor, Error: %s", err_msg); 
          continue;
        }
      }
      
      length =sprintf(msg_buf, "cm_init_monitor:MON_NAME=%s;RECORD_ID=%d;MON_PGM_NAME=%s;CONFIG=", mbean_mon_ptr[i].mon_name,
                    mbean_mon_ptr[i].record_id, mbean_mon_ptr[i].mon_name);
      copy_to_scratch_buffer_for_monitor_request(msg_buf, length, RESET_SCRATCH_BUF, 0);

      if(mbean_mon_ptr[i].config_file != NULL) 
        tmp=strrchr(mbean_mon_ptr[i].config_file,'.');

      if((strcmp(tmp, ".enc")) != 0) 
      {
        CURL *curl = curl_easy_init();
        if(!curl)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
               "Could not encode config json for monitor %s, as curl_easy_init() returned NULL", mbean_mon_ptr[i].mon_name);
          continue;
        }
        //Creating message to send to NDC
        encoded_format = encode_json_file_content(curl, mbean_mon_ptr[i].config_file, &buff);
        copy_to_scratch_buffer_for_monitor_request(encoded_format, strlen(encoded_format), DONT_RESET_SCRATCH_BUF, 0);
        copy_to_scratch_buffer_for_monitor_request(";", 1, DONT_RESET_SCRATCH_BUF, 0);
        FREE_AND_MAKE_NULL(buff, "buffer", -1);
        curl_free(encoded_format);
        curl_easy_cleanup(curl);
      }
      else
      {
        num_bytes = encoded_custom_mon_json_file_content(mbean_mon_ptr[i].config_file, &buff);
        strcat(buff, ";");
        //added ";" at num_bytes so \0 at num_bytes + 1
        buff[num_bytes+1] = '\0';
        copy_to_scratch_buffer_for_monitor_request(buff, num_bytes+1, DONT_RESET_SCRATCH_BUF, 0); 
        FREE_AND_MAKE_NULL(buff, "buffer", -1);
      }
      //structure will be updated with msg_buf
      //strcpy(tmp_msg_buf, msg_buf);
      global_msg_length = strlen(monitor_scratch_buf);
      //This function can create a blank msg buffer in case no monitors are added and been called.
      make_message_for_mbean(i, 0, global_msg_length);
      copy_to_scratch_buffer_for_monitor_request("\n", 1, DONT_RESET_SCRATCH_BUF, 0);
      REALLOC_AND_COPY(monitor_scratch_buf, mbean_mon_ptr[i].msg_buf, monitor_scratch_buf_len + 1, "Message buffer", i);

      if (mbean_mon_ptr[i].flag & UPDATED_NDC_MON)
      {
        CM_info *cus_mon_ptr;      

        // unset
        mbean_mon_ptr[i].flag &= ~UPDATED_NDC_MON;

        make_message_for_mbean(i, ADDED_TIER, global_msg_length);
        copy_to_scratch_buffer_for_monitor_request("\n", 1, DONT_RESET_SCRATCH_BUF, 0);
 
        //NSDL4_MON(NULL, NULL, "tmp_msg_buf = %s", tmp_msg_buf);

        cus_mon_ptr = find_entry_in_cm_info(mbean_mon_ptr[i].gdf_name);
        if(cus_mon_ptr == NULL)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                    "Could not find entry for gdf (%s) in main CM table", mbean_mon_ptr[i].gdf_name);
          NSDL1_MON(NULL, NULL, "Could not find entry for gdf (%s) in main CM table", mbean_mon_ptr[i].gdf_name);
          return;
        }

        // check the init_flag below
        if (cm_send_msg(cus_mon_ptr, monitor_scratch_buf, 0) == -1)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
            "cm_send_msg failed for new tier update, mon_name = %s", mbean_mon_ptr[i].mon_name);
          NSDL1_MON(NULL, NULL, "cm_send_msg failed for new tier update, mon_name = %s", mbean_mon_ptr[i].mon_name);
          return;
        }
      }
    }
  }
  mbean_monitor_offset = total_mbean_mon;
}

//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX MBEAN MON Code ENDS XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX MSSQL Mon Code STARTS XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
/*
int read_mssql_file(char *err_msg)
{
  FILE *fptr = NULL;
  char filename[256];
  char *ptr[MAX_FIELDS_FOR_MSSQL];
  char line[MAX_NAME_SIZE] = {0};
  char temp_line[MAX_NAME_SIZE] = {0};

    open the file for reading 
  sprintf(filename, "%s/etc/mssql_monitors.conf", g_ns_wdir);

  fptr = fopen(filename, "r");
  if (fptr == NULL)
  {
    sprintf(err_msg, "Could not open file %s, for reading. Error: %s\n", filename, strerror(errno));
    return -1;
  }

  while (fgets(line, MAX_NAME_SIZE, fptr))
  { 
    if(line[0] == '\0' || line[0] == '#')
      continue;
 
    strcpy(temp_line, line);

    //token(line);
    if(total_mssql_row >= max_mssql_row)
    {
      MY_REALLOC_AND_MEMSET(mssql_table_ptr, ((max_mssql_row + DELTA_MON_ENTRIES) * sizeof(MSSQL_table)), (max_mssql_row * sizeof(MSSQL_table)), "Creating MSSQL Table", -1);
      max_mssql_row += DELTA_MON_ENTRIES;
    }
   
    if(get_tokens_with_multi_delimiter(line, ptr, "|", MAX_FIELDS_FOR_MSSQL) != MAX_FIELDS_FOR_MSSQL)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Fields mismatch in file %s, while parsing line %s.", filename, temp_line);
      continue;
    }
   
    MALLOC_AND_COPY(ptr[0], mssql_table_ptr[total_mssql_row].gdf, (strlen(ptr[0]) + 1), "Copying elements from ptr to gdf", 0);
    MALLOC_AND_COPY(ptr[1], mssql_table_ptr[total_mssql_row].csv_file, (strlen(ptr[1]) + 1), "Copying elements from ptr to csv", 0);
    mssql_table_ptr[total_mssql_row].norm_id_col = atoi(ptr[2]);
    MALLOC_AND_COPY(ptr[3], mssql_table_ptr[total_mssql_row].norm_table_col_list, (strlen(ptr[3]) + 1), "Copying elements from ptr to MoreColoumn", 0);
    MALLOC_AND_COPY(ptr[4], mssql_table_ptr[total_mssql_row].main_table_col_list, (strlen(ptr[4]) + 1), "Copying elements from ptr to ColoumnInMainTable", 0);
    MALLOC_AND_COPY(ptr[5], mssql_table_ptr[total_mssql_row].date_time_col_list, (strlen(ptr[5]) + 1), "Copying elements from ptr to DateTimeColoumn", 0);
    //removing '\n' from the last column.
    //mssql_table_ptr[total_mssql_row].date_time_col_list[strlen(ptr[5]) - 1] = '\0';
    mssql_table_ptr[total_mssql_row].norm_tbl_idx = atoi(ptr[6]);

    total_mssql_row ++;
  }
  fclose(fptr);
  return 0;
}


int convert_buf_to_array(char *buffer, int *arr_val)
{
  int max_col,i;
  char *ptr[MAX_CSV_COLUMN];
  char temp_buff[128];

  memcpy(temp_buff, buffer, strlen(buffer));  //to save original buffer after tokenization.
  max_col = get_tokens_with_multi_delimiter(temp_buff, ptr, ",", MAX_CSV_COLUMN);
  memset(arr_val, -1, (MAX_CSV_COLUMN * sizeof(int)));

  //Column value is 0. No need to fill array.
  if((max_col == 1) && (atoi(ptr[0]) == 0))
    return 0;

  for(i = 0; i < max_col; i++)
    arr_val[i] = atoi(ptr[i]);
  return 0;
}


int fill_data_in_csv_for_mssql_new(CM_info *cus_mon_ptr ,char *buffer)
{
  int i = 0, j = 0, k = 0, l = 0, norm_id = -1,newflag = 0;
  char *pointer;
  time_t now = time(NULL);
  long long time_stamp_in_sec;
  char csv_filepath[128];
  char *fields[MAX_CSV_COLUMN];
  int no_of_fields;
  struct tm tm_struct;

  //if no modification required, then dump as it is.
  if(cus_mon_ptr->csv_stats_ptr->no_modification_flag)
  {
    dump_data_in_csv(buffer, cus_mon_ptr, TIMESTAMP_NEEDED);
    return -1;
  }

  //check if fp exist, if not then open a fp
  if((cus_mon_ptr->csv_stats_ptr->csv_file_fp == NULL) || (cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp == NULL))
  { 
    if(check_for_csv_file(cus_mon_ptr) < 0)
      return -1;
  }

  no_of_fields = get_tokens_with_multi_delimiter(buffer, fields, ",", MAX_CSV_COLUMN);
  if(cus_mon_ptr->csv_stats_ptr->norm_id_col != 0)
  {
    i = (cus_mon_ptr->csv_stats_ptr->norm_id_col - 1);
    norm_id = nslb_get_or_gen_norm_id(cus_mon_ptr->csv_stats_ptr->sql_id_key, fields[i], strlen(fields[i]), &newflag);
    if(newflag)
    {
      if(fprintf(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp, "%d,%s", norm_id, fields[i]) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in csv file %s.", cus_mon_ptr->csv_stats_ptr->metadata_csv_file);
        return -1;
      }
    }
  }

  //  loop of no_of_fields
  for(i = 0; i < no_of_fields; i++)
  {
    if((i+1) == cus_mon_ptr->csv_stats_ptr->norm_id_col)
    {
      if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d,", norm_id) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in csv file %s.", cus_mon_ptr->csv_stats_ptr->metadata_csv_file);
        return -1;
      }
      continue;
    }
    else if((i+1) == cus_mon_ptr->csv_stats_ptr->norm_table_array[j])
    {
      //calculated from previous norm entry
      if(newflag)
      {
        if(fprintf(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp, ",%s", fields[i]) < 0)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in csv file %s.", cus_mon_ptr->csv_stats_ptr->metadata_csv_file);
          return -1;
        }
      }
      j++;
      continue;
    }
    else if(((i+1) == cus_mon_ptr->csv_stats_ptr->date_time_idx[k]))
    {
      pointer = strrchr(fields[i], '.');
      if(pointer)
        *pointer = '\0';
      
      time_stamp_in_sec = caluculate_timestamp_from_date_string(fields[i]);
      if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%lld,", time_stamp_in_sec) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in writing data in csv file %s. Data : %lld", cus_mon_ptr->csv_stats_ptr->metadata_csv_file, time_stamp_in_sec);
        return -1;
      }
      k++;
      continue;
    }
    //Dump in main table.
    else if((cus_mon_ptr->csv_stats_ptr->main_table_col_arr[0] == -1) || ((i+1) == cus_mon_ptr->csv_stats_ptr->main_table_col_arr[l]))
      l++;
    else 
      continue;

    fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%s,", fields[i]);
  }
 
  strftime(csv_filepath, 20, "%Y-%m-%d %H:%M:%S", nslb_localtime(&now, &tm_struct, 1));
  time_stamp_in_sec = caluculate_timestamp_from_date_string(csv_filepath);

  if(fprintf(cus_mon_ptr->csv_stats_ptr->csv_file_fp, "%d,%lld\n", serial_no, time_stamp_in_sec) < 0)
  {
     MLTL1(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Not able to dump timestamp into file. File: %s, Data: %lld ERROR: %s", cus_mon_ptr->csv_stats_ptr->csv_file, time_stamp_in_sec, strerror(errno));
    return -1;
  }
  SERIAL_NO_INCREMENT(serial_no);
  
   flushing data of csv_file_fp and metadata_csv_file_fp 
  fflush(cus_mon_ptr->csv_stats_ptr->csv_file_fp);
  
  if(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp != NULL)
  {
    if(newflag)
    {
      fprintf(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp, "\n");
      fflush(cus_mon_ptr->csv_stats_ptr->metadata_csv_file_fp);
    }
  }
  return 0;
}     
*/
//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX MSSQL MON Code ENDS XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void put_free_id(struct id_pool *pool, int id)
{  
  NSDL1_MON(NULL, NULL, "Method called. id: %d", id);
 
  if(pool->next_available_idx >= pool->id_pool_max_entries) {
    MY_REALLOC(pool->id_pool_list, (pool->id_pool_max_entries + DELTA_MON_ID_ENTRIES) * sizeof(int), "id_pool realloc", -1);

    if (pool->id_pool_list == NULL) {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "realloc failed for id_pool_list");
    }
    else {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "realloc id_pool_list, previous size = %d, new_size = %d",
        pool->id_pool_max_entries * sizeof(int), (pool->id_pool_max_entries + DELTA_MON_ID_ENTRIES) * sizeof(int));
      pool->id_pool_max_entries += DELTA_MON_ID_ENTRIES;
    }
  }

  pool->id_pool_list[pool->next_available_idx] = id;
  pool->next_available_idx++;

  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "Put id: %d in id_pool_list", id);
}


int get_free_id(struct id_pool *pool)
{
  if(pool->next_available_idx == 0) {
    NSDL1_MON(NULL, NULL, "No free id available");
    return -1;
  }
  
  pool->next_available_idx--;
  return pool->id_pool_list[pool->next_available_idx];
}

// will return a mon_id from free_mon_id_pool_list if available otherwise from g_mon_id
int get_next_mon_id()
{
  /* Note:
   * Freed id will be use by the second monitor added after partition switch
   * because a get_next_mon_id call was made before partition switch
   * so the first monitor added after partition switch will use g_all_mon_id even if there is free id available. */

  if(mon_id_pool.next_available_idx > 0) {
    int id = get_free_id(&mon_id_pool);
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "id reused from pool. id: %d", id);
    return id;
  } else {
    return g_all_mon_id++;
  }
}
