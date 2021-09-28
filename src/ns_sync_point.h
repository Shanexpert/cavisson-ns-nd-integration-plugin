/*******************************************************************
 *  Name              : ns_sync_point_parse.h
 *  Purpose           : Header file for sync point (ns_sync_point_parse.c and ns_sync_point_api_parse.c)
 *  Initial Version   : Saturday, November 24 2012
 *  MOdification date : -
 ******************************************************************/
#ifndef SYNC_POINT_H
#define SYNC_POINT_H

#define SP_MAX_DATA_LINE_LENGTH 256

#define SP_MAX_RELEASE_TARGET_CLEN 50 //max number of release target

#define SP_DELTA_ENTRIES 20

#define SP_INACTIVE           0
#define SP_ACTIVE             1
#define SP_RELEASING          2
#define SP_DELETE             91
#define SP_RELEASE_RUNTIME    90

#define SP_RELEASE_AUTO           0
#define SP_RELEASE_MANUAL         1

#define SP_RELEASE_TYPE_TARGET    0
#define SP_RELEASE_TYPE_TIME      1
#define SP_RELEASE_TYPE_PERIOD    2

#define SP_RELEASE_SCH_IMMEDIATE 	0
#define SP_RELEASE_SCH_DURATION  	1
#define SP_RELEASE_SCH_RATE      	2
#define SP_RELEASE_SCH_STEP_DURATION    3

#define EXTRA_SYNC_POINT_TABLE_SPACE 50

#define SP_DEFAULT_ACTVE_USR_PCT 100.00

#define SP_TYPE_START_TRANSACTION 0
#define SP_TYPE_START_PAGE        1
#define SP_TYPE_START_SCRIPT      2
#define SP_TYPE_START_SYNCPOINT   3

//#define OATO_DEFAULT_VALUE 600000          // 10 minutes in msecs 
//#define IATO_DEFAULT_VALUE 60000           // 60 secs in msecs

//sp api
#define SP_API_DELTA_ENTRIES 16

#define SP_API_NAME_MAX_LEN 48
#define SP_DELTA_BYTEVAR_ENTRIES 30

//enable sync point keyword
#define SP_ENABLE   1
#define SP_DISABLE  0

//for "sync pt name, sync pt type & group" combination
#define SP_GRP_FOUND_IN_TABLE    0
#define SP_GRP_NOTFOUND_IN_TABLE 1
#define SP_GRP_ERROR_IN_TABLE    2

#define SP_RELEASE_PREV_POLICY -2

#define SP_CONTINUE_WITH_VUSER              1
#define SP_VUSER_GOING_TO_PARTICIPATE_IN_SP 2

#define LAST_RELEASE_REASON_LEN 256


/*NOTE: ANY CHANGES IN ANY STRAUCTURE NEED TO BE DONE IN BOTH SHARED AND NON SHARED STRUCTRE.
 * ALSO NEED TO MAINTAIN THE ELEMENTS POISTION SAME IN BOTH TYPE OF STRAUCTURES
 * BCOZ WE ARE USING MEMCPY WHILE COPYING INTO SHARED STRUCTURES*/

typedef struct SPTableEntry {
  char* sync_pt_name;                //Name of Syncpoint
  char* scripts;                     //Scripts
  float sync_pt_usr_pct;             //Pct users taking part in Sync Point, keeping it as we need to print in event log in decimal format
  int sync_pt_usr_pct_as_int;        //Pct users taking part in Sync Point this is multiply by 100 soo that at run time we dont need to do multiplication

  int sync_group_name_as_int;        //Its is name of group, it will be change to name at run time
  int scen_grp_idx;                  //-1 for ALL //No use but keeping later remve if not used
  int *release_target_usr_policy;    //pointer to array of policy 
  int total_release_policies;        //Total release policies given including *
  int sp_grp_tbl_idx;                //Index into SPGroupTable table

  char sp_type;                      //Type of SyncPoint
  char sp_actv_inactv;               //SyncPoint is active or inactive.

  int total_accu_usrs;
  int cur_policy_idx;
  int release_count;     //Manish: This will track released number of synpoint 
  char last_release_reason[LAST_RELEASE_REASON_LEN + 1];
  char last_release_time[LAST_RELEASE_REASON_LEN + 1];
  int inter_arrival_timeout;
  int overall_timeout;
  timer_type* timer_ptr_iato;
  timer_type* timer_ptr_oato;
  int self_idx;
  // New fields added
  int release_mode;  //AUTO or MANUAL
  int release_type;  //Target , Period, Time
  int release_type_timeout; // Timeout to first target release 
  int release_type_frequency; // Timeout to next ( recurring) target release
  int release_forcefully;
  int release_schedule;       // Immediate, Duration, Rate, StepDuration
  int release_schedule_step; // Total Duration for release 
  int *release_schedule_step_quantity; // Percentage quantity to release in Step 
  int *release_schedule_step_duration;  // Timeout to step release
  timer_type* timer_ptr_reltype;
  // These variables are required to write data in summary report and event log 
  char* s_release_tval;
  char* s_release_sval;
  char next_release;
  char *s_release_policy;
} SPTableEntry; 

typedef SPTableEntry SPTableEntry_shr;

int *all_grp_entries;                // For all group entries
int *all_grp_entries_shr;                // For all group entries

/*Manish: This structure is visible to both Parent and NVMs (i.e. child)*/
typedef struct SPGroupTable {       
  fd_set grpset;
  int pct_usr;
  int sync_group_name_as_int;
  /*No use to have active/inactive in group table. bcoz this is shared for multiple groups*/
  //char active_inactive;
  char sp_type;
} SPGroupTable;

typedef struct SPGroupTable_shr {       
  fd_set grpset;
  int pct_usr;
  int sync_group_name_as_int;
  /*No use to have active/inactive in group table. bcoz this is shared for multiple groups*/
  //char active_inactive;
  char sp_type;
} SPGroupTable_shr;

// This structure is used by NVMs. Total entries for SP_user structure will be the total number of SPTableEntry entries
typedef struct SyncPoint_user {
  //This is linked list for vusers.  (used by child) This will outside this struct. NVMs will have own       linkedlist for vusers.
  VUser *vptr_head; //No need to have tail. We Add and remove from head only.
  timer_type* timer_ptr_sche_timeout; // Timer at NVM side for releasing vuser
  
  int nvm_sync_point_users;// indicate total users handling by NVM
  int num_vusers_to_free;
  int rate;
  int remaining_users;
  int duration_to_add;
  int num_time_frame;
  int extra_users_to_add;
  int sp_id;
  int mod_of_release;
  int sp_type;
  int next_step;   //New field added for next step release
  int *step_duration; 
  int *step_quantity;  
} SP_user;

//Sync point API structure
typedef struct SPApiTableLogEntry {
  char* sp_api_name;                     
  int sp_grp_tbl_idx;                   //Index into SPGroupTable table
  int api_hash_idx;                   
} SPApiTableLogEntry; 

typedef struct SPApiTableLogEntry_shr {
  char* sp_api_name;                     
  int sp_grp_tbl_idx;                   //Index into SPGroupTable table
  int api_hash_idx;                   
} SPApiTableLogEntry_shr; 

//for hash function. need to discuss
typedef struct SyncByteVarTableEntry {
  ns_bigbuf_t name;                      // offset into big buf
} SyncByteVarTableEntry;

extern char g_test_user_name[128 +1];
extern SyncByteVarTableEntry *syncByteVarTblEntry;
extern SPTableEntry *syncPntTable;
extern SPTableEntry_shr *syncPntTable_shr;
extern SPGroupTable *spGroupTable;
extern SPGroupTable_shr *spGroupTable_shr;
extern SPApiTableLogEntry *spApiTable;
extern SPApiTableLogEntry_shr *spApiTable_shr;
extern SP_user *sp_user_tbl;

extern int total_syncpoint_entries;
extern int *all_grp_entries;
extern int *all_grp_entries_shr;
extern int total_sp_grp_entries;
extern int max_syncpoint_entries;
extern int total_malloced_syncpoint_entries;

//sync point keyword
extern int kw_set_sync_point(char *buf, char *err_msg, int run_time_flag);

//sync point time out keyword
extern int kw_set_sync_point_time_out(char *buf, int flag, char *err_msg);

//sync point enable keyword
extern int kw_enable_sync_point(char *buf, char *err_msg, int run_time_flag);

//sync point api
extern int get_sp_api_names(FILE* c_file);
extern int parse_sp_api(char *buffer, char *fname, int line_num);
extern void fill_sp_api_table();
extern int get_sp_api_hash_code(char *sync_pt_name);
extern int total_sp_api_hash_entries;
extern int total_sp_api_found;

extern inline void nsi_send_sync_point_msg(VUser *vptr);
extern int ns_sync_point_ext(char *sync_name, VUser* vptr);

//sync point summary file in ready_reports
extern void create_sync_point_summary_file();

//SPGroup Table
extern void init_sp_group_table(int run_time);
extern void process_sp_wait_msg_frm_parent(SP_msg *rcv_msg);
extern void process_sp_continue_msg_frm_parent(SP_msg *rcv_msg);
extern void process_sp_release_msg_frm_parent(SP_msg *rcv_msg);
extern void init_sp_user_table ();

extern int chk_and_make_test_msg_and_send_to_parent(VUser *vptr, int syncgrptblidx);
extern void process_test_msg_from_nvm (int fd, SP_msg *rcv_msg);

extern void copy_sp_grp_tbl_into_shr_memory();
extern void copy_sp_api_tbl_into_shr_memory();
extern void copy_sp_tbl_into_shr_memory();
extern void copy_all_grp_tbl_into_shr_memory();
extern void release_sync_point(int fd, int sp_tbl_id, SP_msg *rcv_msg, char *syn_name, char *grp_name, char *reason_for_release, int timeout_type);

extern SP_msg* create_sync_point_release_msg(SPTableEntry_shr *syncPntTbl);

//transaction api
extern int ns_trans_chk_for_sp(char *sync_name, VUser* vptr);

//to remove users from linked list
extern void sync_point_remove_users_frm_linked_list(VUser *vptr, u_ns_ts_t now);

//to check whether user is marked for ramp down, if yes then find type of ramp down method 
extern int chk_for_ramp_down(VUser *vptr, u_ns_ts_t now);

extern void sync_point_release_release_type_cb(ClientData client_data, u_ns_ts_t now);
extern int convert_rate_to_duration(int num_users, int release_rate);
extern char *ret_sp_name(int sp_type);

extern SPTableEntry *g_sync_point_table;
/* This is to set fd in spGroupTable_shr.
 * if group index is -1(in case of "ALL") then set fd of total_runprof_entries.
 * else set fd of that particular group. */ 

#define MY_SET_FD(sync_group_name_as_int, hash_code){\
    if(sync_group_name_as_int == -1)  \
       FD_SET (total_runprof_entries, &spGroupTable[hash_code].grpset); \
    else\
      FD_SET (sync_group_name_as_int, &spGroupTable[hash_code].grpset);\
     }

#define MY_CLR_FD(sync_group_name_as_int, hash_code){\
    if(sync_group_name_as_int == -1)  \
       FD_CLR (total_runprof_entries, &spGroupTable[hash_code].grpset);\
    else\
      FD_CLR (sync_group_name_as_int, &spGroupTable[hash_code].grpset);\
     }

#define MY_SET_FD_SHR(sync_group_name_as_int, hash_code){\
    if(sync_group_name_as_int == -1)  \
       FD_SET (total_runprof_entries, &spGroupTable_shr[hash_code].grpset); \
    else\
      FD_SET (sync_group_name_as_int, &spGroupTable_shr[hash_code].grpset);\
     }

#define MY_CLR_FD_SHR(sync_group_name_as_int, hash_code){\
    if(sync_group_name_as_int == -1)  \
       FD_CLR (total_runprof_entries, &spGroupTable_shr[hash_code].grpset);\
    else\
      FD_CLR (sync_group_name_as_int, &spGroupTable_shr[hash_code].grpset);\
     }

/*This macro is to check if syncpoint is enable or not 
 * if enable then check for user if this user is goin to participate in syncpoint or not
 * There is no need to check for Sync Point enable. Will remove later*/

#define CHECK_FOR_SYNC_POINT(curr_page){\
    NSDL1_SP(NULL, NULL, "global_settings->sp_enable = %d, curr_page->sp_grp_tbl_idx = %d",global_settings->sp_enable, curr_page->sp_grp_tbl_idx);\
    if(global_settings->sp_enable && curr_page->sp_grp_tbl_idx > -1) { /*Syncpoint is enable on this page*/ \
       ret = chk_and_make_test_msg_and_send_to_parent(vptr, curr_page->sp_grp_tbl_idx);\
       if(ret == SP_VUSER_GOING_TO_PARTICIPATE_IN_SP)\
         return;\
     }\
 }

/*This macro is to check if syncpoint is enable or not 
 * if enable then check for user if this user is goin to participate in syncpoint or not
 * There is no need to check for Sync Point enable. Will remove later*/

#define CHECK_FOR_SYNC_POINT_FOR_SESSION(curr_sess){\
  NSDL1_SP(NULL, NULL, "global_settings->sp_enable = %d, curr_sess->sp_grp_tbl_idx = %d", global_settings->sp_enable, curr_sess->sp_grp_tbl_idx);\
    int ret;\
    if(global_settings->sp_enable && curr_sess->sp_grp_tbl_idx > -1) { /*Syncpoint is enable on this session*/ \
       ret = chk_and_make_test_msg_and_send_to_parent(vptr, curr_sess->sp_grp_tbl_idx);\
       if(ret == SP_VUSER_GOING_TO_PARTICIPATE_IN_SP)\
         return;\
     }\
 }


#define CHECK_FOR_SYNC_POINT_FOR_API(cur_api){\
  NSDL1_SP(NULL, NULL, "global_settings->sp_enable = %d, cur_api->sp_grp_tbl_idx = %d", global_settings->sp_enable, cur_api->sp_grp_tbl_idx);\
    int ret;\
    if(global_settings->sp_enable && cur_api->sp_grp_tbl_idx > -1) { /*Syncpoint is enable on this session*/ \
      ret = process_sync_point_api(vptr, cur_api->sp_grp_tbl_idx);\
      if(ret == SP_VUSER_GOING_TO_PARTICIPATE_IN_SP)\
        return;\
    }\
 }


#define CHECK_FOR_SUMMARY_FILE_FPRINTF(fprintf_ret){\
  NSDL1_SP(NULL, NULL, "fprintf_ret = %d", fprintf_ret);\
  if(fprintf_ret < 0) { \
    perror("Error: Unable to write in file sync_point_summary.report\n");\
    return;\
  }\
}

#endif 
