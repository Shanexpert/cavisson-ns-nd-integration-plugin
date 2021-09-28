/******************************************************************
 * Name                : ns_sync_point_parse.c
 * Author              : Prachi
 * Purpose             :
 * Initial Version     : Saturday, November 24 2012
 * Modification History: 
 *                     : Friday, December 07 2012  Added code for runtime changes - Prachi
 *
 *******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include "ns_log.h"
#include "nslb_hash_code.h"
#include "nslb_util.h"
#include "ns_url_hash.h"
#include "ns_data_types.h"
#include "ns_auto_fetch_parse.h"
#include "ns_global_settings.h"
#include "ns_sync_point.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_script_parse.h"
#include "ns_data_handler_thread.h"

SPTableEntry *syncPntTable = NULL;
SPGroupTable *spGroupTable = NULL;
SyncByteVarTableEntry *syncByteVarTblEntry = NULL;
extern Data_Control_con g_data_control_var;
PageTableEntry *pg_table_ptr;
PageTableEntry_Shr *pg_table_ptr_shr;
//sync types
int total_transaction_type_sync_pt;
int total_page_type_sync_pt;
int total_script_type_sync_pt;
int total_custm_type_sync_pt;

//total hash entries
int total_sp_grp_entries = 0;

//sync table entries
int total_syncpoint_entries = 0;
int max_syncpoint_entries = 0;
int total_malloced_syncpoint_entries = 0; /* total sync_point entries + extra entries (EXTRA_SYNC_POINT_TABLE_SPACE) */


//for hash function.. 
int sync_max_bytevar_entries = 0;
int total_syncbytevar_entries = 0;

//Sync Point time out
int sp_oa_to_in_msecs = 0;
int sp_ia_to_in_msecs = 0;

Str_to_hash_code grp_table_hash_func_ptr;
Hash_code_to_str grp_table_var_get_key;

//script_scriptname, trans_transactionname, page_pagename and syncpt_syncptname
char *dup_file_name = "sync_points_dup.txt";
char *file_name = "sync_points.txt";

SPTableEntry *g_sync_point_table = NULL;

/* Allocating space in sync point table for runtime changes. */
int init_syncpoint_table_entry() 
{
  if (total_syncpoint_entries == max_syncpoint_entries) {
    MY_REALLOC_EX(syncPntTable, (max_syncpoint_entries + SP_DELTA_ENTRIES + EXTRA_SYNC_POINT_TABLE_SPACE) * sizeof(SPTableEntry), (max_syncpoint_entries * sizeof(SPTableEntry)), "syncPntTable", -1);

    max_syncpoint_entries += SP_DELTA_ENTRIES;
    total_malloced_syncpoint_entries = max_syncpoint_entries + EXTRA_SYNC_POINT_TABLE_SPACE;
    g_sync_point_table = syncPntTable;
  }
  return (SUCCESS);
}

/* Creating sync point table and setting default values.
 * EXTRA_SYNC_POINT_TABLE_SPACE -> for runtime changes */
int create_syncpoint_table_entry(int *row_num, int run_time) 
{
  NSDL2_SP(NULL, NULL, "Method Called, row_num = %d, total_syncpoint_entries = %d, max_syncpoint_entries = %d, "
                       "sp_ia_to_in_msecs = %d, sp_oa_to_in_msecs = %d", 
                        *row_num, total_syncpoint_entries, max_syncpoint_entries, sp_ia_to_in_msecs, sp_oa_to_in_msecs);

  if(!run_time){
    init_syncpoint_table_entry();
  }
  else
  {
    if((total_syncpoint_entries + 1) > total_malloced_syncpoint_entries)
    {
      //TODO: Do we need event log????
      fprintf(stderr, "Warning: You can't add more than 50 syncpoints at run time. Not adding syncpoint\n");
      fprintf(stderr, "Warning: Given sync_point entry (%d) is more than total allocated space (%d) in syncpoint_table.\n", total_syncpoint_entries + 1, total_malloced_syncpoint_entries);
      *row_num = -1;
      return(FAILURE);
    }    
  }
  *row_num = total_syncpoint_entries++;
  
  NSDL4_SP( NULL, NULL, "total_syncpoint_entries = %d, max_syncpoint_entries = %d", total_syncpoint_entries, max_syncpoint_entries); 
  return (SUCCESS);
}

// Creating table for hash function
int create_syncbytevar_table_entry(int *row_num) {
  if (total_syncbytevar_entries == sync_max_bytevar_entries) {
    MY_REALLOC_EX (syncByteVarTblEntry, (sync_max_bytevar_entries + SP_DELTA_BYTEVAR_ENTRIES) * sizeof(SyncByteVarTableEntry), (sync_max_bytevar_entries * sizeof(SyncByteVarTableEntry)), "byteVarTable", -1);
    if (!syncByteVarTblEntry) {
      fprintf(stderr,"create_syncbytevar_table_entry(): Error allocating more memory for bytevar entries\n");
      return(FAILURE);
    } else sync_max_bytevar_entries += SP_DELTA_BYTEVAR_ENTRIES;
  }
  *row_num = total_syncbytevar_entries++;
  return (SUCCESS);
}

void syncpoint_usages(char *err, int run_time_flag, int ui_info)
{
  
  NSDL4_SP(NULL, NULL, "syncpoint_usages:: run_time_flag %d, ui_info %d", run_time_flag, ui_info);
  // For every failure while parsing the fields in SYNC-POINT keyword we were showing all the below messages.
  // This message has to be shown only in case if SYNC-POINT keyword contains wrong number of arguments. 
  if((run_time_flag) || (ui_info))
  {
     NSTL1_OUT(NULL, NULL, "Error: Invalid value of SYNC_POINT keyword: %s\n", err);
     NSTL1_OUT(NULL, NULL, "Usage: SYNC_POINT <Group> <Type> <Name> <Active/Inactive> <Pct user> <Release target user> <Scripts> <Release Mode> <Release Type> <Target/Time/Period> <Release Forcefully> <Release Schedule> <Immediate/Duration/Rate>\n");
     NSTL1_OUT(NULL, NULL, " Where:\n");
     NSTL1_OUT(NULL, NULL, " Type :\n");
     NSTL1_OUT(NULL, NULL, "   0: start transaction \n");
     NSTL1_OUT(NULL, NULL, "   1: start page \n");
     NSTL1_OUT(NULL, NULL, "   2: start script \n");
     NSTL1_OUT(NULL, NULL, "   3: custom sync point \n");
     NSTL1_OUT(NULL, NULL, " Active/Inactive :\n");
     NSTL1_OUT(NULL, NULL, "   0: inactive \n");
     NSTL1_OUT(NULL, NULL, "   1: active (Default)\n");
     NSTL1_OUT(NULL, NULL, " pct_user : default value of active user percentage is 100.00 \n");
     NSTL1_OUT(NULL, NULL, " Release Mode :\n");
     NSTL1_OUT(NULL, NULL, "   0: Auto\n");
     NSTL1_OUT(NULL, NULL, "   1: Manual\n");
     NSTL1_OUT(NULL, NULL, " Release Type :\n");
     NSTL1_OUT(NULL, NULL, "   0: Target (if specified next field should be NA)\n");
     NSTL1_OUT(NULL, NULL, "   1: Time (Format - MM/DD/YYYY HH:MIN:SS)\n");
     NSTL1_OUT(NULL, NULL, "   2: Period (Format - HH:MM:SS)\n");
     NSTL1_OUT(NULL, NULL, " Release forcefully on target reached if release type is Time or Period:\n");
     NSTL1_OUT(NULL, NULL, "   0: False\n");
     NSTL1_OUT(NULL, NULL, "   1: True\n");
     NSTL1_OUT(NULL, NULL, " Release Schedule :\n");
     NSTL1_OUT(NULL, NULL, "   0: Immediate (if specified next field should be NA)\n");
     NSTL1_OUT(NULL, NULL, "   1: Duration (Format - HH:MM:SS)\n");
     NSTL1_OUT(NULL, NULL, "   2: Rate (Format - XX)\n");

     if(run_time_flag)
        return;
     // This check is added to show required message if any field is not correct
     // In case if number of arguments is less or more than required show complete usage message  
     if(ui_info)
        NS_EXIT(-1, "Error: Invalid value of SYNC_POINT keyword: %s\nUsage: SYNC_POINT <Group> <Type> <Name> <Active/Inactive> <Pct user> <Release target user> <Scripts> <Release Mode> <Release Type <Target/Time/Period> <Release Forcefully> <Release Schedule> <Immediate/Duration/Rate>", err);
  }
 
  NS_EXIT(-1,"%s",err);
}

//This is to get index of each sync point from syncByteVarTblEntry.
inline int find_sync_idx(char* name) {
  NSDL4_SP(NULL, NULL, "Method Called. name = %s", name);
  int i;
  for (i = 0; i < total_syncbytevar_entries; i++) {
    if (!strncmp(RETRIEVE_BUFFER_DATA(syncByteVarTblEntry[i].name), name, strlen(name)))
      return i;
   }
  return -1;
} 

/****************** SECTION USED IN BOTH THE CASES ( NORMAL AND RUNTIME)  *******************/

/* Putting index of SpGrpTable in all those tables which can be used as SyncPoint(page, session, api and transaction). 
 * So that Parent can access SyncPointTbl using SyncPointTbl index by retrieving SyncPointTbl index from AllGrpEntriesTbl using        * index of SpGrpTable.*/
void put_sp_group_tbl_idx_in_othr_tbl(int sp_type, int indx, int spTbleIdx)
{
  int i, g;

  NSDL1_SP(NULL, NULL, "Method called, sp_type = %d, indx = %d, spTbleIdx = %d", sp_type, indx, spTbleIdx); 

  if(sp_type == SP_TYPE_START_TRANSACTION)
  {
    for(i = 0; i < total_tx_entries; i++)
    { 
      if(strcmp(g_sync_point_table[indx].sync_pt_name, RETRIEVE_BUFFER_DATA(txTable[i].tx_name)) == 0)
        txTable[i].sp_grp_tbl_idx = spTbleIdx; 
    }
  }

  if(sp_type == SP_TYPE_START_PAGE)
  {
    for(i = 0; i < total_sess_entries; i++)
    {
      for (g = 0; g < gSessionTable[i].num_pages; g++)
      {
        pg_table_ptr = &gPageTable[gSessionTable[i].first_page + g];
        if(strcmp(g_sync_point_table[indx].sync_pt_name, RETRIEVE_BUFFER_DATA(pg_table_ptr->page_name)) == 0)
          pg_table_ptr->sp_grp_tbl_idx = spTbleIdx;
      }
    }
  }

  if(sp_type == SP_TYPE_START_SCRIPT)
  {
    for(i = 0; i < total_sess_entries; i++)
    {
      //if(strcmp(syncPntTable[indx].sync_pt_name, RETRIEVE_BUFFER_DATA(gSessionTable[i].sess_name)) == 0)
      if(strcmp(g_sync_point_table[indx].sync_pt_name, get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[i].sess_name),
                                                   i, "/")) == 0)
        gSessionTable[i].sp_grp_tbl_idx = spTbleIdx;
    }
  }

  if(sp_type == SP_TYPE_START_SYNCPOINT)
  {
    for(i = 0; i < total_sp_api_found; i++)
    {
      if (spApiTable[i].sp_api_name == NULL)
       continue;
      else
      {
        if(strcmp(g_sync_point_table[indx].sync_pt_name, spApiTable[i].sp_api_name) == 0)
          spApiTable[i].sp_grp_tbl_idx = spTbleIdx;
      }
    }
  }
  NSDL4_SP(NULL, NULL, "Method Existing.");
}

/*For shared memory, same as above function.*/
void put_sp_group_tbl_idx_in_othr_tbl_shr(int sp_type, int indx, int spTbleIdx)
{
  char *tx_name;
  int i, g;
  
  NSDL1_SP(NULL, NULL, "Method called, sp_type = %d, indx = %d, spTbleIdx = %d", sp_type, indx, spTbleIdx); 

  if(sp_type == SP_TYPE_START_TRANSACTION)
  {
    for(i = 0; i < total_tx_entries; i++)
    {
      tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, i);
      NSDL1_SP(NULL, NULL, "total_tx_entries = %d, g_sync_point_table[indx].sync_pt_name = %s, tx_name = %s \n", total_tx_entries , g_sync_point_table[indx].sync_pt_name, tx_name); 
      if(strcmp(g_sync_point_table[indx].sync_pt_name, tx_name) == 0) 
        tx_table_shr_mem[i].sp_grp_tbl_idx = spTbleIdx; 
    }
  }

  if(sp_type == SP_TYPE_START_PAGE)
  {
    for(i = 0; i < total_sess_entries; i++)
    {
      pg_table_ptr_shr = session_table_shr_mem[i].first_page;
      for (g = 0; g < session_table_shr_mem[i].num_pages; g++)
      {
        if(strcmp(g_sync_point_table[indx].sync_pt_name, pg_table_ptr_shr->page_name) == 0) 
          pg_table_ptr_shr->sp_grp_tbl_idx = spTbleIdx;
        pg_table_ptr_shr++;
      }
    }
  }

  if(sp_type == SP_TYPE_START_SCRIPT)
  {
    for(i = 0; i < total_sess_entries; i++)
    {
      //if(strcmp(syncPntTable_shr[indx].sync_pt_name, session_table_shr_mem[i].sess_name) == 0)
      if(strcmp(g_sync_point_table[indx].sync_pt_name, get_sess_name_with_proj_subproj_int(session_table_shr_mem[i].sess_name,
                                                       i, "/")) == 0)
        session_table_shr_mem[i].sp_grp_tbl_idx = spTbleIdx;
    }
  }

  if(sp_type == SP_TYPE_START_SYNCPOINT)
  {
    for(i = 0; i < total_sp_api_found; i++)
    {
      if (spApiTable_shr[i].sp_api_name == NULL)
        continue;
      else
      {
        if(strcmp(g_sync_point_table[indx].sync_pt_name, spApiTable_shr[i].sp_api_name) == 0)
          spApiTable_shr[i].sp_grp_tbl_idx = spTbleIdx;
      }
    }
  }
  NSDL4_SP(NULL, NULL, "Method Existing.");
}

/* For shared memory - same as above function */
int get_grp_tbl_hash_code(int sp_type, char *sync_pt_name, char *s_script, char *err_msg)
{
  NSDL4_SP(NULL, NULL, "Method Called.");

  char sync_buf[SP_MAX_DATA_LINE_LENGTH];
  char sync_buf_for_err_msg[SP_MAX_DATA_LINE_LENGTH];
  int hash_code;

  if(sp_type == SP_TYPE_START_TRANSACTION) {
    strcpy(sync_buf, "transaction_");
    strcpy(sync_buf_for_err_msg, "SyncPoint transaction"); }
  else if(sp_type == SP_TYPE_START_PAGE) {
    strcpy(sync_buf, "page_");
    strcpy(sync_buf_for_err_msg, "SyncPoint page"); }
  else if(sp_type == SP_TYPE_START_SCRIPT) {
    strcpy(sync_buf, "script_");
    strcpy(sync_buf_for_err_msg, "SyncPoint script"); }
  else {
    strcpy(sync_buf, "syncpoint_");
    strcpy(sync_buf_for_err_msg, "SyncPoint API"); }

  strcat(sync_buf, sync_pt_name);
  hash_code = grp_table_hash_func_ptr(sync_buf, strlen(sync_buf));
  NSDL4_SP(NULL, NULL, "hash_code = %d, sync_pt_name = %s, sp_type = %d ", hash_code, sync_pt_name, sp_type);
  NSTL4(NULL, NULL, "hash_code = %d, sync_pt_name = %s, sp_type = %d ", hash_code, sync_pt_name, sp_type);
 
  if(hash_code < 0) 
  {
    if(sp_type != SP_TYPE_START_SCRIPT)
      sprintf(err_msg, "Given %s %s does not exists in script %s.\n", sync_buf_for_err_msg, sync_pt_name, s_script);
    else
      sprintf(err_msg, "Given script %s does not exists.\n", sync_pt_name);
  }
  NSDL4_SP(NULL, NULL, "Method Exiting...");

  return hash_code;
}

/* Fill SpGrpTbl and set FD of the used group.*/
void fill_sp_group_tbl(int hash_code, int indx)
{
  NSDL4_SP(NULL, NULL, "Method Called.");
  
  // Entry in spGroupTable
  spGroupTable[hash_code].pct_usr = g_sync_point_table[indx].sync_pt_usr_pct_as_int;
  spGroupTable[hash_code].sync_group_name_as_int = g_sync_point_table[indx].sync_group_name_as_int;         //-1 means ALL grp
  spGroupTable[hash_code].sp_type = g_sync_point_table[indx].sp_type;

  NSDL4_SP(NULL, NULL, "MY_SET_FD(%d, %d)", g_sync_point_table[indx].sync_group_name_as_int, hash_code);

  MY_SET_FD(g_sync_point_table[indx].sync_group_name_as_int, hash_code);
  g_sync_point_table[indx].sp_grp_tbl_idx = hash_code;

  NSDL4_SP(NULL, NULL, "spGroupTable[%d].pct_usr = %d, spGroupTable[%d].sync_group_name_as_int = %d, g_sync_point_table[%d].sp_grp_tbl_idx = %d", hash_code, spGroupTable[hash_code].pct_usr, hash_code, spGroupTable[hash_code].sync_group_name_as_int, indx, g_sync_point_table[indx].sp_grp_tbl_idx);

  NSDL4_SP(NULL, NULL, "Method Exiting...");
}

/*For shared memory - same as above */
void fill_sp_group_tbl_shr(int hash_code, int indx)
{
  NSDL4_SP(NULL, NULL, "Method Called.");
  
  // Entry in spGroupTable_shr
  spGroupTable_shr[hash_code].pct_usr = g_sync_point_table[indx].sync_pt_usr_pct_as_int;
  spGroupTable_shr[hash_code].sync_group_name_as_int = g_sync_point_table[indx].sync_group_name_as_int;         //-1 means ALL grp
  spGroupTable_shr[hash_code].sp_type = g_sync_point_table[indx].sp_type;

  NSDL4_SP(NULL, NULL, "g_sync_point_table[indx].sync_group_name_as_int = %d, spGroupTable_shr[hash_code].sync_group_name_as_int = %d", g_sync_point_table[indx].sync_group_name_as_int, spGroupTable_shr[hash_code].sync_group_name_as_int);

  NSDL4_SP(NULL, NULL, "MY_SET_FD(%d, %d)", g_sync_point_table[indx].sync_group_name_as_int, hash_code);

  //FD_ZERO(&(spGroupTable_shr[hash_code].grpset));
  MY_SET_FD_SHR(g_sync_point_table[indx].sync_group_name_as_int, hash_code);
  g_sync_point_table[indx].sp_grp_tbl_idx = hash_code;

  NSDL4_SP(NULL, NULL, "spGroupTable_shr[%d].pct_usr = %d, spGroupTable_shr[%d].sync_group_name_as_int = %d, g_sync_point_table[%d].sp_grp_tbl_idx = %d", hash_code, spGroupTable_shr[hash_code].pct_usr, hash_code, spGroupTable_shr[hash_code].sync_group_name_as_int, hash_code, indx, g_sync_point_table[indx].sp_grp_tbl_idx);

  NSDL4_SP(NULL, NULL, "Method Exiting...");
}

/***************** SECTION USED IN BOTH THE CASES ( NORMAL AND RUNTIME) ENDS ******************/

/* Validating combination of: 'sync_point_name, sync_point_type and sync_point_group'(considering this combination as foreign key)
 * 1. If any particular set of : 'sync_point_name & sync_point_type' exists for any "specific group" then this same set cannot exits f *    or "ALL" and vice versa.
 * 2. If combination found then it means we can update further fields of SyncPointTable for this combination.
 * 3. If this combination does not found then it means group does not exists we can add new entry in SP Table.  */
int validate_type_name_and_grp_combination(int active_inactive, char* s_name, char* s_group, int* sp_idx, int type)
{
  int x;
  char grp_name[1024]; 

  NSDL4_SP(NULL, NULL, "Method Called. s_name = %s, s_group = %s, sp_idx = %d, type = %d", s_name, s_group, sp_idx, type);

  NSDL4_SP(NULL, NULL, "total_syncpoint_entries = %d", total_syncpoint_entries);
  *sp_idx = -1;

  for(x = 0; x < total_syncpoint_entries; x++)
  {
    NSDL4_SP(NULL, NULL, "x = %d", x);

    if(g_sync_point_table[x].sync_group_name_as_int != -1)
    {
      if(!runprof_table_shr_mem)
        strcpy(grp_name, RETRIEVE_BUFFER_DATA(runProfTable[g_sync_point_table[x].sync_group_name_as_int].scen_group_name));
      else
        strcpy(grp_name, runprof_table_shr_mem[g_sync_point_table[x].sync_group_name_as_int].scen_group_name);
    }
    else
      strcpy(grp_name, "ALL");

    NSDL4_SP(NULL, NULL, "grp_name = %s", grp_name);
    NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].sync_pt_name = %s, g_sync_point_table[%d].sp_type = %d", x, g_sync_point_table[x].sync_pt_name, x, g_sync_point_table[x].sp_type);

    NSTL4(NULL, NULL, "grp_name = %s", grp_name);
    NSTL4(NULL, NULL, "g_sync_point_table[%d].sync_pt_name = %s, g_sync_point_table[%d].sp_type = %d", x, g_sync_point_table[x].sync_pt_name, x, g_sync_point_table[x].sp_type);

    //if key(SP name, SP type, SP grp) found then return index to update SPTable.
    if(((strcmp(g_sync_point_table[x].sync_pt_name, s_name)) == 0) && (g_sync_point_table[x].sp_type == type) && (strcmp(grp_name, s_group) == 0))
    {
      *sp_idx = x;
      return SP_GRP_FOUND_IN_TABLE;
    }
    if(((strcmp(g_sync_point_table[x].sync_pt_name, s_name)) == 0) && (g_sync_point_table[x].sp_type == type))
    {
      if(((strcmp(grp_name, "ALL") != 0) && (strcmp(s_group, "ALL") == 0)) ||
                            ((strcmp(grp_name, "ALL") == 0) && (strcmp(s_group, "ALL") != 0)))
      {
        NSTL1(NULL, NULL, "WARNING: same combination of sync_point_type and sync_point_name should not be applied on ALL group and on specific group together.\n");
        return SP_GRP_ERROR_IN_TABLE;
      }
    }
  }
  return(SP_GRP_NOTFOUND_IN_TABLE);
}

//Function to update all schedule related counters.
void update_sync_point_schedule(int rnum, int r_schedule, int r_schedule_step, int *s_schedule_step_quantity, int *s_schedule_step_duration)
{
  NSDL1_SP(NULL, NULL, "Method Called. rnum = %d, r_schedule = %d, r_schedule_step = %d",
                        rnum, r_schedule, r_schedule_step);  
  
  g_sync_point_table[rnum].release_schedule = r_schedule;
  g_sync_point_table[rnum].release_schedule_step = r_schedule_step;

  if(r_schedule != SP_RELEASE_SCH_IMMEDIATE)
  {
    int step_buf_size = r_schedule_step * sizeof(int);
    MY_REALLOC(g_sync_point_table[rnum].release_schedule_step_duration, step_buf_size, "release_schedule_step_duration", -1);
    MY_REALLOC(g_sync_point_table[rnum].release_schedule_step_quantity, step_buf_size, "release_schedule_step_quantity", -1);
    memcpy(g_sync_point_table[rnum].release_schedule_step_duration, s_schedule_step_duration, step_buf_size);
    memcpy(g_sync_point_table[rnum].release_schedule_step_quantity, s_schedule_step_quantity, step_buf_size);
  }
}

/* Called from function add_sync_table() to make entry of new values or to update already existing values of sync point table. */
int update_sync_table(char *s_group, int type, char *s_name, int active_inactive, float s_pct_user, char *s_policy, char *s_script, int rnum,                        int r_mode, int r_type, int r_type_time_period, int r_rel_forcefully, int r_type_frequency, 
                       int r_schedule, int r_schedule_step, int *s_schedule_step_quantity, int *s_schedule_step_duration,                           char *buf, char *err_msg, int run_time_flag)
{
  int r, total_flds = 0;
  char *field[SP_MAX_RELEASE_TARGET_CLEN];
  int total_sp_users = 0;

  NSDL1_SP(NULL, NULL, "Method Called. s_group = %s, type = %d, s_name = %s, active_inactive = %d, s_pct_user = %.2f, s_policy = %s, s_script = %s, rnum = %d r_mode = %d, r_type = %d, r_schedule = %d r_type_time_period = %d", s_group, type, s_name, active_inactive, s_pct_user, s_policy, s_script, rnum, r_mode, r_type, r_schedule, r_type_time_period);

  NSTL4(NULL, NULL, "Method Called. s_group = %s, type = %d, s_name = %s, active_inactive = %d, s_pct_user = %.2f, s_policy = %s, s_script = %s, rnum = %d r_mode = %d, r_type = %d, r_schedule = %d r_type_time_period = %d", s_group, type, s_name, active_inactive, s_pct_user, s_policy, s_script, rnum, r_mode, r_type, r_schedule, r_type_time_period);
  
  memset(field, 0, sizeof(field));

  // Don't fill these data until they are empty as these fields once set can't be modify at runtime.
  if(!g_sync_point_table[rnum].sync_pt_name[0])
     strcpy(g_sync_point_table[rnum].sync_pt_name, s_name);
  if(!g_sync_point_table[rnum].scripts[0])
     strcpy(g_sync_point_table[rnum].scripts, s_script);

  strcpy(g_sync_point_table[rnum].s_release_policy,s_policy);
  
  if(type == SP_TYPE_START_TRANSACTION) 
    total_transaction_type_sync_pt++;
  else if (type == SP_TYPE_START_PAGE) 
    total_page_type_sync_pt++;
  else if (type == SP_TYPE_START_SCRIPT) 
    total_script_type_sync_pt++;
  else 
    total_custm_type_sync_pt++;

  NSDL4_SP(NULL, NULL, "total_transaction_type_sync_pt = %d, total_page_type_sync_pt = %d, total_script_type_sync_pt = %d, total_custm_type_sync_pt = %d", total_transaction_type_sync_pt, total_page_type_sync_pt, total_script_type_sync_pt, total_custm_type_sync_pt);  

  total_flds = get_tokens(s_policy, field, ",", SP_MAX_RELEASE_TARGET_CLEN);
  g_sync_point_table[rnum].total_release_policies = total_flds;

  NSDL4_SP(NULL, NULL, "total_flds = %d, g_sync_point_table[rnum].total_release_policies = %d", total_flds, g_sync_point_table[rnum].total_release_policies);

  //MY_REALLOC(g_sync_point_table[rnum].release_target_usr_policy, (sizeof(int) * total_flds), "policy table creation", -1);
  if(s_pct_user * 100 > 10000)
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011262, "");
  }
  //TODO: if policy is zero or -ve

  if(!run_time_flag)
    g_sync_point_table[rnum].sync_group_name_as_int = find_group_idx(s_group);
  else
    g_sync_point_table[rnum].sync_group_name_as_int = find_sg_idx_shr(s_group);

  // We need to check that syncpoint users or percentage of vuser configured 
  // in syncpoint should be less than total Vusers present in system
  if(g_sync_point_table[rnum].sync_group_name_as_int == -1)
  {
    int i;
    for(i=0; i< total_runprof_entries; i++)
    {
      total_sp_users += (!run_time_flag)?runProfTable[i].quantity:runprof_table_shr_mem[i].quantity;
      NSDL4_SP(NULL, NULL, "Case of ALL, total_sp_users = %d", total_sp_users);
    }     
  }
  else
  {
    int grp_idx = g_sync_point_table[rnum].sync_group_name_as_int;
    total_sp_users = (!run_time_flag)?runProfTable[grp_idx].quantity:runprof_table_shr_mem[grp_idx].quantity;
    NSDL4_SP(NULL, NULL, "Case of group, total_sp_users = %d", total_sp_users);
  }

  total_sp_users = (s_pct_user*total_sp_users)/100;

  //Fill policy into integer array
  //For * fill -1, keep track of total user release policies
  MY_REALLOC(g_sync_point_table[rnum].release_target_usr_policy, (sizeof(int) * total_flds), "policy table creation", -1);
  for(r = 0; r < total_flds; r++) {
    NSDL4_SP(NULL, NULL, "rnum = %d, r = %d, field[%d] = %s", rnum, r, r, field[r]);
    //Valid case: (*) can only be at last
    // Invalid case: (*) cant be alone, in middle and at start of the policy
    if((strcmp(field[r], "*") != 0) && (ns_is_numeric(field[r]) == 0))
    {
      NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011260, "");
    }

    if(((strcmp(field[r], "*") == 0) && (r < (total_flds - 1))) 
         || ((strcmp(field[r], "*") == 0) && (r == 0))) 
    {
      NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011261, "");
    }
   
    if((strcmp(field[r], "*") == 0)) 
    {
      // putting -1 for * in SP table.
      g_sync_point_table[rnum].release_target_usr_policy[r] = SP_RELEASE_PREV_POLICY;   
      NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].release_target_usr_policy[%d] = %d", rnum, r, g_sync_point_table[rnum].release_target_usr_policy[r]);
    }
    
    if((ns_is_numeric(field[r]) != 0)) 
    {
      g_sync_point_table[rnum].release_target_usr_policy[r] = atoi(field[r]);
      if(loader_opcode != CLIENT_LOADER)// We will not perform this check only in case of CLIENT as we never divide syncpoint user on client
      {
        if(g_sync_point_table[rnum].release_target_usr_policy[r] > total_sp_users)
          NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, "Target users or percentage are more then configured Vusers");
      }
      NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].release_target_usr_policy[%d] = %d", rnum, r, g_sync_point_table[rnum].release_target_usr_policy[r]);
    } 
  }

/*  if(s_pct_user * 100 > 10000)
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011262, "");
  }*/

/*  if(!run_time_flag)
    g_sync_point_table[rnum].sync_group_name_as_int = find_group_idx(s_group);
  else
    g_sync_point_table[rnum].sync_group_name_as_int = find_sg_idx_shr(s_group);*/

  g_sync_point_table[rnum].sp_type = type;
  g_sync_point_table[rnum].sp_actv_inactv = active_inactive;
  g_sync_point_table[rnum].sync_pt_usr_pct = s_pct_user;
  g_sync_point_table[rnum].sync_pt_usr_pct_as_int = s_pct_user * 100;
  g_sync_point_table[rnum].release_mode = r_mode;
  g_sync_point_table[rnum].release_type = r_type;
  g_sync_point_table[rnum].release_type_timeout = r_type_time_period;
  g_sync_point_table[rnum].release_type_frequency = r_type_frequency;
  g_sync_point_table[rnum].release_forcefully = r_rel_forcefully;

  update_sync_point_schedule(rnum, r_schedule, r_schedule_step, s_schedule_step_quantity, s_schedule_step_duration);

  NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].sync_group_name_as_int = %d, g_sync_point_table[%d].sp_type = %d, g_sync_point_table[%d].sp_actv_inactv = %d, g_sync_point_table[%d].sync_pt_usr_pct = %.2f, g_sync_point_table[%d].sync_pt_name = %s, g_sync_point_table[%d].scripts = %s", rnum, g_sync_point_table[rnum].sync_group_name_as_int, rnum, g_sync_point_table[rnum].sp_type, rnum, g_sync_point_table[rnum].sp_actv_inactv, rnum, g_sync_point_table[rnum].sync_pt_usr_pct, rnum, g_sync_point_table[rnum].sync_pt_name, rnum, g_sync_point_table[rnum].scripts);

  NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].release_mode = %d, g_sync_point_table[%d].release_type = %d, g_sync_point_table[%d].release_schedule = %d, g_sync_point_table[%d].release_type_timeout = %d, g_sync_point_table[%d].release_schedule_step = %d", rnum, g_sync_point_table[rnum].release_mode, rnum, g_sync_point_table[rnum].release_type, rnum, g_sync_point_table[rnum].release_schedule, rnum, g_sync_point_table[rnum].release_type_timeout, rnum, g_sync_point_table[rnum].release_schedule_step);

  NSTL4(NULL, NULL, "g_sync_point_table[%d].sync_group_name_as_int = %d, g_sync_point_table[%d].sp_type = %d, g_sync_point_table[%d].sp_actv_inactv = %d, g_sync_point_table[%d].sync_pt_usr_pct = %.2f, g_sync_point_table[%d].sync_pt_name = %s, g_sync_point_table[%d].scripts = %s", rnum, g_sync_point_table[rnum].sync_group_name_as_int, rnum, g_sync_point_table[rnum].sp_type, rnum, g_sync_point_table[rnum].sp_actv_inactv, rnum, g_sync_point_table[rnum].sync_pt_usr_pct, rnum, g_sync_point_table[rnum].sync_pt_name, rnum, g_sync_point_table[rnum].scripts);

  NSTL4(NULL, NULL, "g_sync_point_table[%d].release_mode = %d, g_sync_point_table[%d].release_type = %d, g_sync_point_table[%d].release_schedule = %d, g_sync_point_table[%d].release_type_timeout = %d, g_sync_point_table[%d].release_schedule_step = %d", rnum, g_sync_point_table[rnum].release_mode, rnum, g_sync_point_table[rnum].release_type, rnum, g_sync_point_table[rnum].release_schedule, rnum, g_sync_point_table[rnum].release_type_timeout, rnum, g_sync_point_table[rnum].release_schedule_step);

  NSDL4_SP(NULL, NULL, "Method Existing.");
  return 0;
}

inline void init_sp_tbl_default_values(int rnum)
{
  NSDL1_SP(NULL, NULL, "Method clled");
  //fill with default values
   g_sync_point_table[rnum].sp_actv_inactv = SP_INACTIVE;
   g_sync_point_table[rnum].sync_pt_usr_pct = SP_DEFAULT_ACTVE_USR_PCT;
   g_sync_point_table[rnum].cur_policy_idx = 0;
   g_sync_point_table[rnum].release_count = 0;
   g_sync_point_table[rnum].total_accu_usrs = 0;
   g_sync_point_table[rnum].self_idx = rnum;
   g_sync_point_table[rnum].inter_arrival_timeout = sp_ia_to_in_msecs;
   g_sync_point_table[rnum].overall_timeout = sp_oa_to_in_msecs;
   
   /* Initialization of new fields */
   g_sync_point_table[rnum].release_mode = -1;
   g_sync_point_table[rnum].release_type = -1;
   g_sync_point_table[rnum].release_schedule = -1;
   g_sync_point_table[rnum].release_type_timeout = 0;
   g_sync_point_table[rnum].release_schedule_step = 0;
   g_sync_point_table[rnum].release_forcefully = 0;
   g_sync_point_table[rnum].release_target_usr_policy = NULL;
   g_sync_point_table[rnum].release_schedule_step_duration = NULL;
   g_sync_point_table[rnum].release_schedule_step_quantity = NULL;
 
  NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].release_count = %d, g_sync_point_table[%d].sync_pt_usr_pct = %.2f,g_sync_point_table[%d].cur_policy_idx = %d, g_sync_point_table[%d].total_accu_usrs = %d,g_sync_point_table[rnum].overall_timeout = %d, g_sync_point_table[rnum].inter_arrival_timeout = %d", rnum, g_sync_point_table[rnum].release_count, rnum, g_sync_point_table[rnum].sync_pt_usr_pct, rnum, g_sync_point_table[rnum].cur_policy_idx, rnum, g_sync_point_table[rnum].total_accu_usrs,g_sync_point_table[rnum].overall_timeout, g_sync_point_table[rnum].inter_arrival_timeout);

  NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].release_mode = %d, g_sync_point_table[%d].release_type = %d, g_sync_point_table[%d].release_schedule = %d, g_sync_point_table[%d].release_type_timeout = %d, g_sync_point_table[rnum].release_schedule_step = %.2f g_sync_point_table[rnum].release_forcefully = %d", rnum, g_sync_point_table[rnum].release_mode, rnum, g_sync_point_table[rnum].release_type, rnum, g_sync_point_table[rnum].release_schedule, rnum, g_sync_point_table[rnum].release_type_timeout, g_sync_point_table[rnum].release_schedule_step, g_sync_point_table[rnum].release_forcefully);


  NSTL4(NULL, NULL, "g_sync_point_table[%d].release_count = %d, g_sync_point_table[%d].sync_pt_usr_pct = %.2f,g_sync_point_table[%d].cur_policy_idx = %d, g_sync_point_table[%d].total_accu_usrs = %d,g_sync_point_table[rnum].overall_timeout = %d, g_sync_point_table[rnum].inter_arrival_timeout = %d", rnum, g_sync_point_table[rnum].release_count, rnum, g_sync_point_table[rnum].sync_pt_usr_pct, rnum, g_sync_point_table[rnum].cur_policy_idx, rnum, g_sync_point_table[rnum].total_accu_usrs,g_sync_point_table[rnum].overall_timeout, g_sync_point_table[rnum].inter_arrival_timeout);

  NSTL4(NULL, NULL, "g_sync_point_table[%d].release_mode = %d, g_sync_point_table[%d].release_type = %d, g_sync_point_table[%d].release_schedule = %d, g_sync_point_table[%d].release_type_timeout = %d, g_sync_point_table[rnum].release_schedule_step = %.2f g_sync_point_table[rnum].release_forcefully = %d", rnum, g_sync_point_table[rnum].release_mode, rnum, g_sync_point_table[rnum].release_type, rnum, g_sync_point_table[rnum].release_schedule, rnum, g_sync_point_table[rnum].release_type_timeout, g_sync_point_table[rnum].release_schedule_step, g_sync_point_table[rnum].release_forcefully);

  NSDL4_SP(NULL, NULL, "Method Existing.");
}

/* Doing malloc for sync_pt_name & scripts : 
 * 1. either to make entry of new sync point, or
 * 2. to update already existing sync point by deleting older content and storing new content from keyword.*/
void add_sync_table(char* s_group, int type, char* s_name, int active_inactive, float pct_usr, char* s_policy, char *s_script, 
                    int rnum, int r_mode, int r_type, int r_type_time_period, int r_type_frequency, int r_rel_forcefully, int r_schedule, 
                    int r_schedule_step, int *s_schedule_step_quantity, int *s_schedule_step_duration, 
                    char *buf, char *err_msg, int run_time_flag)
{

  NSDL4_SP(NULL, NULL, "Method Called.");

  init_sp_tbl_default_values(rnum);
  //TODO: Need to check if name is not starting with alpha ??? 
  MY_MALLOC_AND_MEMSET(g_sync_point_table[rnum].sync_pt_name, strlen(s_name) + 1, "sync point name", -1); 
  MY_MALLOC_AND_MEMSET(g_sync_point_table[rnum].scripts, strlen(s_script) + 1, "sync point scripts", -1);
  MY_MALLOC_AND_MEMSET(g_sync_point_table[rnum].s_release_tval, 32 + 1, "SP Release type", -1);
  MY_MALLOC_AND_MEMSET(g_sync_point_table[rnum].s_release_sval, SP_MAX_DATA_LINE_LENGTH + 1, "SP Release Val", -1);
  MY_MALLOC_AND_MEMSET(g_sync_point_table[rnum].s_release_policy, strlen(s_policy) + 1, "SP Release Policy", -1);

  //Allocate memory for timer
  NSDL3_SP(NULL, NULL, "Allocating timer memory for syn point index %d", rnum);

  MY_MALLOC(g_sync_point_table[rnum].timer_ptr_iato, sizeof(timer_type), "timer_ptr_iato", -1);
  g_sync_point_table[rnum].timer_ptr_iato->timer_type = -1;

  MY_MALLOC(g_sync_point_table[rnum].timer_ptr_oato, sizeof(timer_type), "timer_ptr_oato", -1);
  g_sync_point_table[rnum].timer_ptr_oato->timer_type = -1;

  MY_MALLOC(g_sync_point_table[rnum].timer_ptr_reltype, sizeof(timer_type), "timer_ptr_reltype", -1);
  g_sync_point_table[rnum].timer_ptr_reltype->timer_type = -1;

  update_sync_table(s_group, type, s_name, active_inactive, pct_usr, s_policy, s_script, rnum, r_mode, r_type, r_type_time_period, 
                    r_rel_forcefully, r_type_frequency, r_schedule, r_schedule_step, s_schedule_step_quantity, 
                    s_schedule_step_duration, buf, err_msg, run_time_flag);
  NSDL4_SP(NULL, NULL, "Method Existing.");
}

/************************************* For creating file in ready_reports ******************************************/

/* This function is called from deliver_report.c in normal case & from this file during runtime.
 * This is to create and update file sync_point_summary.report present inside Test Run dir.*/
void create_sync_point_summary_file()
{
  NSDL1_SP(NULL, NULL, "Method Called.");

  char file_path[1024];
  char grp_name[1024];
  int i, j;
  char rel_target_vusers[1024];
  char string[1024];
  int total_sync_pts_in_file = 0;
  FILE *sync_fp = NULL;
  int fprintf_ret;
  //char sync_rel_schedule[SP_MAX_DATA_LINE_LENGTH + 1]={0};

  sprintf(file_path, "%s/logs/TR%d/ready_reports/sync_point_summary.report", g_ns_wdir, testidx);
  if ((sync_fp = fopen(file_path, "w+")) == NULL) {
    fprintf(stderr, "Error in creating the sync point summary report file\n");
    perror("fopen");
    return;
  }
  
  fprintf_ret = fprintf(sync_fp, "Group|Type|Name|Active|Participating VUsers Pct|Release Target Vusers|Current users|Release count|Last release time|Last release reason|Scripts|Release Mode|Release Type|Release Type Value|Release Forcefully|Release Schedule|Release Schedule Value\n");
  CHECK_FOR_SUMMARY_FILE_FPRINTF(fprintf_ret);

  for(i = 0; i < total_syncpoint_entries; i++)
  {
    strcpy(rel_target_vusers, "");

    NSDL4_SP(NULL, NULL, "i = %d, total_syncpoint_entries = %d, g_sync_point_table[%d].total_release_policies === %d, total_accumulated_users = %d", i, total_syncpoint_entries, i, g_sync_point_table[i].total_release_policies, g_sync_point_table[i].total_accu_usrs );

    for(j = 0; j < g_sync_point_table[i].total_release_policies; j++)
    {
      if(g_sync_point_table[i].release_target_usr_policy[j] == SP_RELEASE_PREV_POLICY)
        sprintf(string, "%s", "*");
      else
        sprintf(string, "%d", g_sync_point_table[i].release_target_usr_policy[j]);

      strcat(rel_target_vusers, string);
  
      if(j != (g_sync_point_table[i].total_release_policies - 1))
        strcat(rel_target_vusers, ",");
    }

    NSDL4_SP(NULL, NULL, "rel_target_vusers === %s", rel_target_vusers);

    //Because we are putting -1 for "ALL" in sync point tbl while parsing. Hence in order to print proper grp name in file.
    if(g_sync_point_table[i].sync_group_name_as_int == -1)
      strcpy(grp_name, "ALL");
    else
      strcpy(grp_name, runprof_table_shr_mem[g_sync_point_table[i].sync_group_name_as_int].scen_group_name);
		     	
    NSDL4_SP(NULL, NULL, " sync_point = %d , g_sync_point_table[i].sp_actv_inactv = %d \n", i , g_sync_point_table[i].sp_actv_inactv);

     // When test gets over we have to make all syncpoint inactive
    /* if(!g_data_control_var.num_active)
         g_sync_point_table[i].sp_actv_inactv = 0;*/
  
    if(g_sync_point_table[i].sp_actv_inactv != SP_DELETE)
    {
      //putting hyphen as release reason when release count is zero.
      if(g_sync_point_table[i].release_mode == SP_RELEASE_MANUAL)
         fprintf_ret = fprintf(sync_fp, "%s|%d|%s|%d|%.2f|%s|%d|%d|%s|%s|%s|%d|%s|%s|%s|%d|%s\n", grp_name, g_sync_point_table[i].sp_type, g_sync_point_table[i].sync_pt_name, g_sync_point_table[i].sp_actv_inactv, g_sync_point_table[i].sync_pt_usr_pct, rel_target_vusers, g_sync_point_table[i].total_accu_usrs, g_sync_point_table[i].release_count, (g_sync_point_table[i].release_count == 0)?"-":g_sync_point_table[i].last_release_time,(g_sync_point_table[i].release_count == 0)?"-":g_sync_point_table[i].last_release_reason, g_sync_point_table[i].scripts, g_sync_point_table[i].release_mode, "NA","NA","0",g_sync_point_table[i].release_schedule, (g_sync_point_table[i].release_schedule == SP_RELEASE_SCH_IMMEDIATE)?"NA":g_sync_point_table[i].s_release_sval);
     else
         fprintf_ret = fprintf(sync_fp, "%s|%d|%s|%d|%.2f|%s|%d|%d|%s|%s|%s|%d|%d|%s|%d|%d|%s\n", grp_name, g_sync_point_table[i].sp_type, g_sync_point_table[i].sync_pt_name, g_sync_point_table[i].sp_actv_inactv, g_sync_point_table[i].sync_pt_usr_pct, rel_target_vusers, g_sync_point_table[i].total_accu_usrs, g_sync_point_table[i].release_count, (g_sync_point_table[i].release_count == 0)?"-":g_sync_point_table[i].last_release_time,(g_sync_point_table[i].release_count == 0)?"-":g_sync_point_table[i].last_release_reason, g_sync_point_table[i].scripts, g_sync_point_table[i].release_mode, g_sync_point_table[i].release_type,(g_sync_point_table[i].release_type == SP_RELEASE_TYPE_TARGET)?"NA":g_sync_point_table[i].s_release_tval,g_sync_point_table[i].release_forcefully,g_sync_point_table[i].release_schedule, (g_sync_point_table[i].release_schedule == SP_RELEASE_SCH_IMMEDIATE)?"NA":g_sync_point_table[i].s_release_sval);

      CHECK_FOR_SUMMARY_FILE_FPRINTF(fprintf_ret);

      total_sync_pts_in_file++;
    }
    else
      continue;

  }

  //GUI consider "Sync_Point=" as last line of file sync_point_summary.report 
  fprintf_ret = fprintf(sync_fp, "Sync_Point=%d\n", total_sync_pts_in_file);
  CHECK_FOR_SUMMARY_FILE_FPRINTF(fprintf_ret);

  fclose(sync_fp);

  NSDL4_SP(NULL, NULL, "Method Existing.");
}


/*This function is to parse SYNC_POINT keyword.
 * buf: Input buffer
 * flag: 
 * run_time_flag: 0 - Normal parsing
 *                1-  Runtime parsing*/

int kw_set_sync_point(char *buf, char *err_msg, int run_time_flag)
{
  char s_keyword[SP_MAX_DATA_LINE_LENGTH];
  char s_group[SP_MAX_DATA_LINE_LENGTH];
  char s_type[SP_MAX_DATA_LINE_LENGTH];
  char s_name[SP_MAX_DATA_LINE_LENGTH];
  char s_pct_user[SP_MAX_DATA_LINE_LENGTH];
  char s_actve_inactve[SP_MAX_DATA_LINE_LENGTH];
  char s_policy[SP_MAX_DATA_LINE_LENGTH];
  char s_script[SP_MAX_DATA_LINE_LENGTH];
  char script_full_name[2048];
  char s_name_full[2048];
  char no_use_buf[SP_MAX_DATA_LINE_LENGTH];       //to solve bug 5208
  int num, rnum, type, active_inactive, ret, sp_idx, hash_code;
  float pct_usr;
  char s_release_mode[SP_MAX_DATA_LINE_LENGTH];
  char s_release_type[SP_MAX_DATA_LINE_LENGTH];
  char s_release_type_val[SP_MAX_DATA_LINE_LENGTH]={0};
  char s_release_schedule[SP_MAX_DATA_LINE_LENGTH];
  char s_release_schedule_val[SP_MAX_DATA_LINE_LENGTH]={0};
  char s_release_forcefully[SP_MAX_DATA_LINE_LENGTH]={0};
  int r_mode, r_type, r_schedule;
  int r_type_time_period = 0, r_schedule_step = 0, r_rel_forcefully = 0, r_type_frequency = 0;
  int total_flds;
  int s_release_schedule_step_quantity[SP_MAX_RELEASE_TARGET_CLEN];
  int s_release_schedule_step_duration[SP_MAX_RELEASE_TARGET_CLEN];
  
  rnum = -1;
  SP_msg *my_msg;

  NSDL4_SP(NULL, NULL, "Method Called. buf = %s, sync point feature enable = %d", 
                         buf, global_settings->sp_enable);

  NSTL1(NULL,NULL, "Method Called. buf = %s, sync point feature enable = %d",
                         buf, global_settings->sp_enable);

  if(!global_settings->sp_enable)
  {
    NSTL1(NULL, NULL, "SyncPoint feature is not enable. Kindly, enable it using [Global Settings] -> [SyncPoints] -> [Enable SyncPoints Settings]");
    return -1;
  } 

  //SYNC_POINT <Group> <type> <name>         <ACTIVE/INACTIVE> <PCT_USER> <POLICY> <SCRIPT> <RELEASE_MODE> <RELEASE_TYPE> <RELEASE_TYPE VAL> <RELEASE_FORCEFULLY> <RELEASE_SCHEDULE> <RELEASE_SCHEDUE VAL>
  //SYNC_POINT   G1      1    page_SyncPoint       1               10           25  hpd 0 2 10:10:10 0 1 00:10:10

  if ((num = sscanf(buf, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s", s_keyword, s_group, s_type, s_name, s_actve_inactve, s_pct_user, s_policy, s_script, s_release_mode, s_release_type, s_release_type_val, s_release_forcefully, s_release_schedule, s_release_schedule_val, no_use_buf)) != 14) 
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_1);
  }
  
  if(nslb_atoi(s_type, &type) < 0)
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_2);
  }

  if((type != SP_TYPE_START_TRANSACTION) && (type != SP_TYPE_START_PAGE) && 
      (type != SP_TYPE_START_SCRIPT) && (type != SP_TYPE_START_SYNCPOINT)) 
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_3);
  }

  if(nslb_atoi(s_actve_inactve, &active_inactive) <0)  
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_2);
  }

  if((active_inactive != SP_INACTIVE) && (active_inactive != SP_ACTIVE) && 
     (active_inactive != SP_DELETE) && (active_inactive != SP_RELEASE_RUNTIME))
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_3);
  }

  if(ns_is_float(s_pct_user) == 0)
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011252, "");
  }

  pct_usr = atof(s_pct_user);

  /* Start - parsing of new fields */

  if(nslb_atoi(s_release_mode, &r_mode) < 0)
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_2);
  }

  if((r_mode != SP_RELEASE_AUTO) && (r_mode != SP_RELEASE_MANUAL))
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_3);
  }

  /* In case of manual mode, RELEASE_TYPE and its value should be NA */
   
  if(r_mode == SP_RELEASE_MANUAL)
  {  
     r_type = -1;
     if((strcmp(s_release_type, "NA") != 0) || (strcmp(s_release_type_val, "NA") != 0))
     {
       NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011253, "");
     }   
  }
  else
  {
     if(nslb_atoi(s_release_type, &r_type) < 0)
     {
       NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_2);
     }

     if((r_type != SP_RELEASE_TYPE_TARGET) && (r_type != SP_RELEASE_TYPE_TIME) &&
       (r_type != SP_RELEASE_TYPE_PERIOD))
     {
       NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_3);
     }

    // Check r_type, if it is SP_RELEASE_TYPE_TARGET then next field should be NA else get the next field
    
    if(r_type == SP_RELEASE_TYPE_TARGET)
    {
      if(strcmp(s_release_type_val, "NA") != 0)
      {
        NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011254, "");
      }
    }
    else
    {
      if(loader_opcode != CLIENT_LOADER)
      {
        if((r_type == SP_RELEASE_TYPE_PERIOD))
        {
          // Get milliseconds from the string
          r_type_time_period = get_time_from_format(s_release_type_val);
          if(r_type_time_period < 0)
          {
            NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011255, s_release_type_val, "");
          }  
        }
        else
        { 
          // This is the case of  Release Type Time
          //Check frequency
          char *ptr;
          char s_release_type_val_temp[SP_MAX_DATA_LINE_LENGTH]={0};//Temp is used to avoid updating original data
          strncpy(s_release_type_val_temp,s_release_type_val,strlen(s_release_type_val));
          if((ptr = strchr(s_release_type_val_temp,',')))
          {
            *ptr = '\0';
            ptr++;
            // Get milliseconds from the string
            r_type_frequency = get_time_from_format(ptr);
            if(r_type_frequency < 0)
            {
              NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011266, ptr, "");
            }  
          }
          
          // Convert time in milliseconds
          r_type_time_period = ns_get_delay_in_secs(s_release_type_val_temp) * 1000;
          // This condition will save the case when user wants to release syncpoint manually or make it deactive:
          // Absolute time is already passed and user is waiting for frequency to complete
          // Don't throw error in this case
          if((active_inactive != SP_RELEASE_RUNTIME) && (active_inactive != SP_INACTIVE))
          {
             if(r_type_time_period < 0)
             {
                NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011256, s_release_type_val_temp, "");
             }
          }       
        }
        // Need to compute Release forcefully value
        if(nslb_atoi(s_release_forcefully, &r_rel_forcefully) < 0)
        {
          NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_2);
        } 
        if((r_rel_forcefully != 0) && (r_rel_forcefully != 1))
        {
          NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_3);
        } 
      }
    }
  }

  if(nslb_atoi(s_release_schedule, &r_schedule) < 0)
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_2);
  }
  if((r_schedule != SP_RELEASE_SCH_IMMEDIATE) && (r_schedule != SP_RELEASE_SCH_DURATION) &&
     (r_schedule != SP_RELEASE_SCH_RATE) && (r_schedule != SP_RELEASE_SCH_STEP_DURATION))
  {
    NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_3);
  }

  // Check r_schedule, if it is SP_RELEASE_SCH_IMMEDIATE then next field should be NA else get the next field
  if(r_schedule == SP_RELEASE_SCH_IMMEDIATE)
  {
    if(strcmp(s_release_schedule_val, "NA") != 0)
    {
      NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011257, "");
    }
  }
  else
  { 
    if(r_schedule == SP_RELEASE_SCH_DURATION)
    {
       /* Get milliseconds from the string */
      r_schedule_step = get_time_from_format(s_release_schedule_val)/1000;
      if(r_schedule_step <= 0)
        NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_9);

      s_release_schedule_step_quantity[0] = 100;
      s_release_schedule_step_duration[0] = r_schedule_step;
      r_schedule_step = 1;
    }
    else if (r_schedule == SP_RELEASE_SCH_RATE)
    {
      ret = nslb_atoi(s_release_schedule_val, &r_schedule_step);
      if(ret < 0)
        NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_2);
 
      if(r_schedule_step <= 0)
        NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_9);

      s_release_schedule_step_quantity[0] = 100;
      s_release_schedule_step_duration[0] = r_schedule_step;
      r_schedule_step = 1; 
    }
    else if (r_schedule == SP_RELEASE_SCH_STEP_DURATION) 
    {
      /* Get milliseconds from the string */
      char *field[SP_MAX_RELEASE_TARGET_CLEN];
      char tmp_str[SP_MAX_DATA_LINE_LENGTH]; 
      int num_step = 0, i, j;
      int total_qty = 0;
      char *ptr;
      strcpy(tmp_str, s_release_schedule_val);
      total_flds = get_tokens(tmp_str, field, ";", SP_MAX_RELEASE_TARGET_CLEN);
      if(total_flds <= 0)
        NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011267, "");

      for(i = 0 ; i < total_flds; i++)
      {
        if((ptr = strchr(field[i], ',')) == NULL)
        {
          NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011267, "");
        }
        *ptr = '\0';
        ptr++;
        /* Get duration in seconds from the string */
        s_release_schedule_step_duration[i] = get_time_from_format(field[i])/1000;
        if ((nslb_atoi(ptr, &s_release_schedule_step_quantity[i]) < 0) || 
            (s_release_schedule_step_duration[i] < 0) || 
            (s_release_schedule_step_quantity[i] < 0))
        {
          NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011251, CAV_ERR_MSG_8);
        }
        total_qty += s_release_schedule_step_quantity[i];
        num_step++; 

        //Release All so these is will be last step 
        //Check & trace in case not
        //Ignoring next schedule step
        if((!s_release_schedule_step_quantity[i]) && (i < (total_flds - 1)))
        {
          for(j = i + 1; j < total_flds; j++)
          {
            NSTL1(NULL, NULL, "Ignoring SyncPoint schedule step %s", field[j]);
          }
          break;
        }
      }
      if( total_qty  > 100)
      {
        NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011268, "");
      }    
      r_schedule_step = num_step;
    }
  } 
  //validate group in case of normal parsing
  if(!run_time_flag) 
  {
    if (strcmp(s_group, "ALL") != 0) 
    {
      if (find_group_idx(s_group) < 0)  
      {           
        NSTL1(NULL, NULL, CAV_ERR_MSG_7, s_group);
        sprintf(err_msg, "Group name (%s) used in %s keyword is not a valid scenario group name.\n",
                          s_group, s_keyword);
        return -1;
      }
    }
  }
  //validate group in case of runtime parsing
  else 
  {
    if (strcmp(s_group, "ALL") != 0) 
    {
      if (find_sg_idx_shr(s_group) < 0) 
      {
        NSTL1(NULL, NULL, CAV_ERR_MSG_7, s_group);
        sprintf(err_msg, "Group name (%s) used in %s keyword is not a valid scenario group name.\n", 
                         s_group, s_keyword);
        return -1;
      }
    }
  }

  //Check for s_name variable with '/' for mode 2, if coming with project and subproject,
  //Then take it as s_name name and process
  //if '/' does not exist, then make full s_name wih proj/sub-proj/script/script_name
  NSDL4_SP(NULL, NULL, "TYPE = %d", type);
  if(type == SP_TYPE_START_SCRIPT)
  {
    if(!strchr(s_name, '/'))
      sprintf(s_name_full, "%s/%s/scripts/%s", g_project_name, g_subproject_name, s_name);
    else
    {
      if(!run_time_flag)// This is done only once at parsing time(during test start)
      {
        char *lptr = strrchr(s_name, '/');
        *lptr = '\0';
        lptr++;
        strcpy(s_name_full, s_name);
        strcat(s_name_full, "/scripts/");
        strcat(s_name_full, lptr);
      }
      else
        strcpy(s_name_full, s_name);
    }
  }
  else
    strcpy(s_name_full, s_name);

  NSDL4_SP(NULL, NULL, "s_name_full = %s", s_name_full);

  //validate combination of "sp_type & sp_name" 
  ret = validate_type_name_and_grp_combination(active_inactive, s_name_full, s_group, &sp_idx, type);
 
  NSDL4_SP(NULL, NULL, "sp_idx = %d, ret = %d", sp_idx, ret);
  NSTL2(NULL, NULL, "sp_idx = %d", sp_idx); 
  
  //Check for script name i.e., s_script variable with '/', if coming with project and subproject,
  //Then take it as s_script name and process
  //if '/' does not exist, then make full scipt name wih proj/sub-proj/script
  if(!strchr(s_script, '/')) {
    sprintf(script_full_name, "%s/%s/scripts/%s", g_project_name, g_subproject_name, s_script);
  }
  else
  {
/*    char *lptr = strrchr(s_script, '/');
    *lptr = '\0';
    lptr++;
    strcpy(script_full_name, s_script);
    strcat(script_full_name, "/scripts/");
    strcat(script_full_name, lptr); */
    strcpy(script_full_name, s_script);
  }
 
  NSDL4_SP(NULL, NULL, "script_full_name = %s and s_script =%s", script_full_name, s_script);
  NSTL2(NULL, NULL, "script_full_name = %s and s_script =%s", script_full_name, s_script);

  // check for runtime changes 
  if(!run_time_flag)                       //not run time 
  {
    if(ret == SP_GRP_FOUND_IN_TABLE)
    {
      NSTL1(NULL, NULL, "\nSyncpoint (%s) is already added in list. Ignoring this sync point.\n", buf);
      return -1;
    }
    
    if(ret == SP_GRP_ERROR_IN_TABLE)
    {
      NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011259, "");
    }

    if (create_syncpoint_table_entry(&rnum, run_time_flag) != SUCCESS)
    {
      NS_EXIT(-1, "Error: Error in getting syncpoint_table entry");
    }

    // Since this function is called when syncpoint is added statically so in anycase we have to allocate timer
    add_sync_table(s_group, type, s_name_full, active_inactive, pct_usr, s_policy, script_full_name, rnum, 
                   r_mode, r_type, r_type_time_period, r_type_frequency, r_rel_forcefully, r_schedule, r_schedule_step, 
                   s_release_schedule_step_quantity, s_release_schedule_step_duration, buf, err_msg, run_time_flag);
    
    // Few arguments are initialized outside of this function
    // s_release_type_val contains value of <PERIOD>, <ABSOLUTE TIME,FREQUENCY
    // s_release_schedule_val contains value <DURATION>, <RATE> <STEP DURATION>
    strncpy(g_sync_point_table[rnum].s_release_tval, s_release_type_val, 32);
    strcpy(g_sync_point_table[rnum].s_release_sval, s_release_schedule_val);
  }
  else                                     //run time 
  {  
    if(ret == SP_GRP_NOTFOUND_IN_TABLE)    //Group not found in table
    {
      /* After getting hash code. */
      if((hash_code = get_grp_tbl_hash_code(type, s_name_full, script_full_name, err_msg)) < 0)
        return -1;

      create_syncpoint_table_entry(&rnum, run_time_flag);
      if(rnum == -1)
        return -1;

      NSDL4_SP(NULL, NULL, "rnum = %d, g_sync_point_table[rnum].sp_actv_inactv = %d, g_sync_point_table[rnum].sync_pt_usr_pct = %d, g_sync_point_table[rnum].cur_policy_idx = %d, g_sync_point_table[rnum].release_count = %d, g_sync_point_table[rnum].total_accu_usrs = %d", rnum , g_sync_point_table[rnum].sp_actv_inactv, g_sync_point_table[rnum].sync_pt_usr_pct, g_sync_point_table[rnum].cur_policy_idx, g_sync_point_table[rnum].release_count, g_sync_point_table[rnum].total_accu_usrs);

      NSDL4_SP(NULL, NULL, "s_group = %s, type = %d, s_name = %s, active_inactive = %d, pct_usr = %.2f, s_policy = %s, s_script = %s, rnum = %d, s_release_mode = %s, s_release_type = %s, s_releaase_type_val = %s, s_release_schedule = %s, s_release_schedule_val = %s", s_group, type, s_name_full, active_inactive, pct_usr, s_policy, script_full_name, rnum, s_release_mode, s_release_type, s_release_type_val, s_release_schedule, s_release_schedule_val);

      NSTL4(NULL, NULL, "rnum = %d, g_sync_point_table[rnum].sp_actv_inactv = %d, g_sync_point_table[rnum].sync_pt_usr_pct = %d, g_sync_point_table[rnum].cur_policy_idx = %d, g_sync_point_table[rnum].release_count = %d, g_sync_point_table[rnum].total_accu_usrs = %d", rnum , g_sync_point_table[rnum].sp_actv_inactv, g_sync_point_table[rnum].sync_pt_usr_pct, g_sync_point_table[rnum].cur_policy_idx, g_sync_point_table[rnum].release_count, g_sync_point_table[rnum].total_accu_usrs);

      NSTL4(NULL, NULL, "s_group = %s, type = %d, s_name = %s, active_inactive = %d, pct_usr = %.2f, s_policy = %s, s_script = %s, rnum = %d, s_release_mode = %s, s_release_type = %s, s_releaase_type_val = %s, s_release_schedule = %s, s_release_schedule_val = %s", s_group, type, s_name_full, active_inactive, pct_usr, s_policy, script_full_name, rnum, s_release_mode, s_release_type, s_release_type_val, s_release_schedule, s_release_schedule_val);

      // Here we are adding sync point for the first time so  need to allocate timers 
      add_sync_table(s_group, type, s_name_full, active_inactive, pct_usr, s_policy, script_full_name, rnum,
                         r_mode, r_type, r_type_time_period, r_type_frequency, r_rel_forcefully,
                         r_schedule, r_schedule_step, s_release_schedule_step_quantity, s_release_schedule_step_duration,
                         buf, err_msg, run_time_flag);

      // Few arguments are initialized outside of this function     
      strncpy(g_sync_point_table[rnum].s_release_tval, s_release_type_val, 32);
      strcpy(g_sync_point_table[rnum].s_release_sval, s_release_schedule_val);

      // As we are making a new syncpoint entry so making reason count = 0
      g_sync_point_table[rnum].release_count = 0;    

      /* After adding at run time need to update summary file*/
      //create_sync_point_summary_file();
      /* Filling sp_group_tbl using above generated hash code as index in this table.*/
      fill_sp_group_tbl_shr(hash_code, rnum);
      /* Now put syncpoint group table index in tx, page, session or syncpoint table depending upon the type of syncpoint*/
      put_sp_group_tbl_idx_in_othr_tbl_shr(g_sync_point_table[rnum].sp_type, rnum, hash_code);

      NSDL4_SP(NULL, NULL, "spGroupTable_shr[%d].pct_usr = %d, spGroupTable_shr[%d].sync_group_name_as_int = %d", hash_code, spGroupTable_shr[hash_code].pct_usr, hash_code, spGroupTable_shr[hash_code].sync_group_name_as_int, hash_code);

     //Update all group table
     all_grp_entries_shr[(hash_code * (total_runprof_entries + 1)) + (spGroupTable_shr[hash_code].sync_group_name_as_int + 1)] = rnum;
    }
    else if(ret == SP_GRP_FOUND_IN_TABLE) 
    {
      // No need to perform any action in case of client loader 
      if(loader_opcode == CLIENT_LOADER)
      {
        NSDL4_SP(NULL, NULL, "Case of generator, not performing any action");
        return 0;
      }

      // To protect the case when release policy is changed and syncpoint is in active state
      if((strcmp(s_policy,g_sync_point_table[sp_idx].s_release_policy)) && (g_sync_point_table[sp_idx].sp_actv_inactv == SP_ACTIVE))
          NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011291, g_sync_point_table[sp_idx].sync_pt_name);      
      
      //found index
      //Here can be multiple case:
      //Case1: update
      //        Change in policies values, change in active/inactive
      //Case2: Delete

      //TODO: we are going to update syncpoint.
      //First check if existing syncpoint is active(still not released) or inactive(released)
      //If active then release it and do what ever is given in active_inactive


      NSDL4_SP(NULL, NULL, "s_group = %s, type = %d, s_name = %s, active_inactive = %d, pct_usr = %.2f, s_policy = %s, s_script = %s, sp_idx = %d", s_group, type, s_name_full, active_inactive, pct_usr, s_policy, script_full_name, sp_idx);
 
      NSDL4_SP(NULL, NULL, "g_sync_point_table[sp_idx].sp_actv_inactv = %d, g_sync_point_table[sp_idx].sync_group_name_as_int = %d", g_sync_point_table[sp_idx].sp_actv_inactv, g_sync_point_table[sp_idx].sync_group_name_as_int);

      NSTL4(NULL, NULL, "s_group = %s, type = %d, s_name = %s, active_inactive = %d, pct_usr = %.2f, s_policy = %s, s_script = %s, sp_idx = %d", s_group, type, s_name_full, active_inactive, pct_usr, s_policy, script_full_name, sp_idx);

      NSTL4(NULL, NULL, "g_sync_point_table[sp_idx].sp_actv_inactv = %d, g_sync_point_table[sp_idx].sync_group_name_as_int = %d, g_sync_point_table[sp_idx].total_accu_usrs = %d", g_sync_point_table[sp_idx].sp_actv_inactv, g_sync_point_table[sp_idx].sync_group_name_as_int, g_sync_point_table[sp_idx].total_accu_usrs);

      if((hash_code = get_grp_tbl_hash_code(g_sync_point_table[sp_idx].sp_type, g_sync_point_table[sp_idx].sync_pt_name, g_sync_point_table[sp_idx].scripts, err_msg)) < 0)
        return -1;

      char buff[64];
      sprintf(buff, "By User '%s'", g_rtc_owner); // copy rtc owner name

      if(active_inactive == SP_RELEASE_RUNTIME)
      {
        
        //User wants to release this syncpoint
        //if(current sp is active)

        if(g_sync_point_table[sp_idx].sp_actv_inactv == SP_ACTIVE)
        { 
          NSDL2_SP(NULL, NULL, " going to call release_sync_point for manual release......");
          NSTL2(NULL, NULL, " going to call release_sync_point for manual release......");
       
          // This is required in each case, as we can update particular schedule to any other value or can change the schedule
          update_sync_point_schedule(sp_idx, r_schedule, r_schedule_step, s_release_schedule_step_quantity, s_release_schedule_step_duration);
          if(r_schedule != SP_RELEASE_SCH_IMMEDIATE)  
               strcpy(g_sync_point_table[sp_idx].s_release_sval,s_release_schedule_val);
          
          NSTL4(NULL, NULL, "sp_idx = %d, g_sync_point_table[sp_idx].release_schedule = %d, g_sync_point_table[sp_idx].release_schedule_step = %.2f", sp_idx, g_sync_point_table[sp_idx].release_schedule, g_sync_point_table[sp_idx].release_schedule_step);

          // In whatever case when we are releasing vsuer, we have to make sure they have to be released
          // either immediate/in duration/with particular rate
          my_msg = create_sync_point_release_msg(&g_sync_point_table[sp_idx]);

          release_sync_point(0, sp_idx, my_msg, s_name_full, s_group, buff, REL_MANUAL);
        }
        else
        {
          NSTL1(NULL, NULL, "Given syncpoint is inactive but user given command to release this syncpoint hence returning...\n");
          sprintf(err_msg, "Given syncpoint is inactive but user given command to release this syncpoint\n");
          return -1;
        }
        return 0;
      }
     

      NSDL4_SP(NULL, NULL, "g_sync_point_table[%d].sp_actv_inactv == %d, active_inactive = %d hash_code = %d", sp_idx, g_sync_point_table[sp_idx].sp_actv_inactv, active_inactive, hash_code);

      // If a request of SP DELETE or INACTIVE is received then only released sync-point,
      // no need to release it else
      if(active_inactive == SP_DELETE || active_inactive == SP_INACTIVE)
      { 
         NSDL4_SP(NULL, NULL, " checking to release syncpoint");
         if(g_sync_point_table[sp_idx].sp_actv_inactv == SP_ACTIVE)
         {
            NSDL4_SP(NULL, NULL, "releasing forcefully..");
            
            my_msg = create_sync_point_release_msg(&g_sync_point_table[sp_idx]);

            //release current syncpoint
            release_sync_point(0, sp_idx, my_msg, s_name_full, s_group, buff, REL_MANUAL);
            NSDL4_SP(NULL, NULL, "released forcefully..");
         }
         NSDL4_SP(NULL, NULL, " go to clr fd ");
         MY_CLR_FD_SHR(g_sync_point_table[sp_idx].sync_group_name_as_int, hash_code);
      }
      //Case when syncpoint has to make ACTIVE from any other state
      else if(active_inactive == SP_ACTIVE)
      {
        MY_SET_FD_SHR(g_sync_point_table[sp_idx].sync_group_name_as_int, hash_code);
      }

      // Get last release_type here
      int last_release_type, last_actv_inactv;
      last_release_type = g_sync_point_table[sp_idx].release_type;
      last_actv_inactv = g_sync_point_table[sp_idx].sp_actv_inactv;
      
      // Remove Release Type Time timer first
      // This timer delete has to be handled when we are making syncpoint inactive + releasing syncpoint because
      // we are using this timer in release_sync_point function and after after addition we are deleting it here. 
      // Due to that message "Releasing sync point successfully" message is not coming in event.log
      if (g_sync_point_table[sp_idx].timer_ptr_reltype->timer_type >= 0)
      {
         NSDL2_SP(NULL, NULL, "Deleting Release Type timer for sync point %d, timer_ptr = %p",
                          sp_idx, g_sync_point_table[sp_idx].timer_ptr_reltype);
         dis_timer_del(g_sync_point_table[sp_idx].timer_ptr_reltype);
      }

      //Using update synctable only, as in runtime when old synpoints are updated then no need to add timers.
      update_sync_table(s_group, type, s_name_full, active_inactive, pct_usr, s_policy, script_full_name, sp_idx,
                        r_mode, r_type, r_type_time_period, r_rel_forcefully, r_type_frequency, r_schedule, r_schedule_step, 
                        s_release_schedule_step_quantity, s_release_schedule_step_duration, buf, err_msg, run_time_flag);

      // We are making syncpoint state from INACTIVE-ACTIVE state here so policy index needs to be
      // initialised again so that new vusers can come into syncpoint again
      if((last_actv_inactv == SP_INACTIVE) && (g_sync_point_table[sp_idx].sp_actv_inactv == SP_ACTIVE))
         g_sync_point_table[sp_idx].cur_policy_idx = 0;

      strncpy(g_sync_point_table[sp_idx].s_release_tval, s_release_type_val, 32);
      strcpy(g_sync_point_table[sp_idx].s_release_sval, s_release_schedule_val);

      // We have received auto mode, add release type timer
      // We are adding timer here because in auto mode we are adding these timer when 1st user comes in syncpoint
      // so when more than 1 user has already reached to the syncpoint and RTC has been applied 
      // we need to add timer again here

      // We are also checking here what is the last release type in case of auto mode, if release type is change
      // by applying RTC, update timer here. If users are reached and new type is TARGET then release them also.
 
     if(r_mode == SP_RELEASE_AUTO)
      {
         if(last_release_type != g_sync_point_table[sp_idx].release_type)
         {
             if((g_sync_point_table[sp_idx].release_type == SP_RELEASE_TYPE_TARGET) && 
                   (g_sync_point_table[sp_idx].total_accu_usrs == g_sync_point_table[sp_idx].release_target_usr_policy[g_sync_point_table[sp_idx].cur_policy_idx]))
             { 
               NSDL4_SP(NULL, NULL, "releasing sync point as TARGET_REACHED");
               my_msg = create_sync_point_release_msg(&g_sync_point_table[sp_idx]);

               //release current syncpoint
               release_sync_point(0, sp_idx, my_msg, s_name_full, s_group, "Target Reached", REL_TARGET_REACHED);
               NSDL4_SP(NULL, NULL, "released due to TARGET REACHED..");
             }

            else if ((g_sync_point_table[sp_idx].release_type == SP_RELEASE_TYPE_TIME) || (g_sync_point_table[sp_idx].release_type == SP_RELEASE_TYPE_PERIOD))
            {
               ClientData client_data;
               g_sync_point_table[sp_idx].timer_ptr_reltype->actual_timeout = g_sync_point_table[sp_idx].release_type_timeout;
               client_data.p = &g_sync_point_table[sp_idx]; 
               NSDL2_SP(NULL, NULL, "Adding Release Type(%d) Timeout, sp_idx = %d, actual_timeout = %d",
                           g_sync_point_table[sp_idx].release_type, sp_idx, g_sync_point_table[sp_idx].timer_ptr_reltype->actual_timeout);

               dis_timer_add_ex(AB_PARENT_SP_RELEASE_TYPE_TIMEOUT, g_sync_point_table[sp_idx].timer_ptr_reltype,
                 get_ms_stamp(), sync_point_release_release_type_cb, client_data, 0, 0);
            }
         }
      }
           
    }
    else
    {
      NS_KW_PARSING_ERR(buf, run_time_flag, err_msg, SYNC_POINT_USAGE, CAV_ERR_1011259, "");
    }
     /*After adding at run time need to update summary file*/
      create_sync_point_summary_file();
  }
  NSDL4_SP(NULL, NULL, "Method Existing.");
  return 0;
}

/************************************* SYNC PONT KEYWORD PARSING ENDS ***************************************/


/************************************* SYNC PONT TIMEOUT KEYWORD PARSING ENDS ***************************************/

int kw_set_sync_point_time_out(char *buf, int flag, char *err_msg)
{
  NSDL4_SP(NULL, NULL, "Method Called.");

  char sp_to_kw[SP_MAX_DATA_LINE_LENGTH];                // sync point timeout keyword
  char sp_overall_to[SP_MAX_DATA_LINE_LENGTH];           // sync point overall timeout
  char sp_intrarrival_to[SP_MAX_DATA_LINE_LENGTH];       // sync point interarrival timeout
  int num;

  if ((num = sscanf(buf, "%s %s %s", sp_to_kw, sp_overall_to, sp_intrarrival_to)) != 3) {
    NS_KW_PARSING_ERR(buf, flag, err_msg, SYNC_POINT_TIME_OUT_USAGE, CAV_ERR_1011065, CAV_ERR_MSG_1);
  }

  sp_oa_to_in_msecs = get_time_from_format(sp_overall_to);
  sp_ia_to_in_msecs = get_time_from_format(sp_intrarrival_to);

  NSDL4_SP(NULL, NULL, "sp_oa_to_in_msecs = %d, sp_ia_to_in_msecs = %d", sp_oa_to_in_msecs, sp_ia_to_in_msecs);
  NSDL4_SP(NULL, NULL, "Method Exiting.");

  return 0;
}

/************************************* SYNC PONT TIMEOUT KEYWORD PARSING ENDS ***************************************/


/************************************* SYNC PONT ENABLE KEYWORD PARSING BEGINS ***************************************/

int kw_enable_sync_point(char *buf, char *err_msg, int runtime_flag)
{
  int num, sp_val;
  char sp_enable_kw[SP_MAX_DATA_LINE_LENGTH];
  char sp_enable_val[SP_MAX_DATA_LINE_LENGTH];

  NSDL1_SP(NULL, NULL, "Method Called. buf = %s", buf);  

  if ((num = sscanf(buf, "%s %s", sp_enable_kw, sp_enable_val)) != 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_SYNC_POINT_USAGE, CAV_ERR_1011023, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(sp_enable_val) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_SYNC_POINT_USAGE, CAV_ERR_1011023, CAV_ERR_MSG_2);
  }

  sp_val = atoi(sp_enable_val);
  if((sp_val != SP_ENABLE) && (sp_val != SP_DISABLE))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_SYNC_POINT_USAGE, CAV_ERR_1011023, CAV_ERR_MSG_3);
  }

  global_settings->sp_enable = sp_val;

  init_syncpoint_table_entry(); 

  NSDL2_SP(NULL, NULL, "Method Exiting. global_settings->sp_enable = %d", global_settings->sp_enable);

  return 0;
}

/************************************* SYNC PONT ENABLE KEYWORD PARSING ENDS ***************************************/


/************************************* SP GROUP TABLE SECTION BEGINS ***********************************************/

/*File 'dup_file_name' may have duplicates syncpoints, hence uniquely sorting this file and creating another file 'file_name' having unique entries of syncpoints.*/
void remove_duplicates_sp()
{
  char cmd[1024]="\0";
  char err_msg[1024]= "\0";

  sprintf(cmd, "LC_ALL=en_US.UTF-8 LC_COLLATE=en_US.UTF-8 sort -u %s/%s >> %s/%s", g_ns_tmpdir, dup_file_name, g_ns_tmpdir, file_name);
  nslb_system(cmd,1,err_msg);
}

/* In this function: 
 * 1. Creating file for hash function. (file will have content: page_pagename, script_scriptname, etc)
 * 2. Filling structure for hash.
 * 3. Generating hash code and hash function.
 * 4. For each entry of sync point table,  getting hash of "SyncPointType_SyncPointName"
 *   4.1. if hash_code found, fill SPGroupTable using generated hash_code as index in this table.
 *   4.2. Also put index of SPGroupTable i.e. hash_code in other tables (txTable, gSessionTable, pg_table_ptr and SPApiTableLogEntry)
     4.3. And put index of sync point table in all_grp_entries table.
 * In this way (4.1., 4.2., 4.3.) we are establish linking in between all the three tables.*/

void init_sp_group_table(int run_time)
{ 
  int indx, hash_code, runprof_indx, gsess_idx, tx_indx, rownum, t;
  FILE* sy_file_ptr;
  char sync_file[1024];
  char sync[SP_MAX_DATA_LINE_LENGTH];
  char sp_error[512 + 1];
  
  NSDL1_SP(NULL, NULL, "Method Called. total_sp_api_found = %d", total_sp_api_found);

  if(total_sp_api_found > 0)
  {
    //read all the sp api from file and fill into sp api table.
    fill_sp_api_table();
  }

  sprintf(sync_file, "%s/%s", g_ns_tmpdir, dup_file_name);
 
 // creating file for hash function
 // Format of SP is: 'SyncPointType_SyncPointName'
  NSDL2_SP(NULL, NULL, " sync_file = %s", sync_file);
  if ((sy_file_ptr = fopen(sync_file, "w+")) == NULL) {
    fprintf(stderr, "Error in opening file %s\n", sync_file);
    perror("fopen");
  }

  NSDL3_SP(NULL, NULL, "total_sess_entries = %d, total_tx_entries = %d, total_sp_api_found = %d", total_sess_entries, total_tx_entries, total_sp_api_found);

  for(runprof_indx = 0; runprof_indx < total_sess_entries; runprof_indx++)     //scripts
  {
    fprintf(sy_file_ptr, "script_%s\n", get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[runprof_indx].sess_name),
                                          runprof_indx, "/"));
    //fprintf(sy_file_ptr, "script_%s\n", RETRIEVE_BUFFER_DATA(gSessionTable[runprof_indx].sess_name));
    
    for (gsess_idx = 0; gsess_idx < gSessionTable[runprof_indx].num_pages; gsess_idx++)            //pages
    {
      pg_table_ptr = &gPageTable[gSessionTable[runprof_indx].first_page + gsess_idx];
      fprintf(sy_file_ptr, "page_%s\n", RETRIEVE_BUFFER_DATA(pg_table_ptr->page_name));
    }
  }

  for(tx_indx = 0; tx_indx < total_tx_entries; tx_indx++)                  //transaction
  {
      fprintf(sy_file_ptr, "transaction_%s\n", RETRIEVE_BUFFER_DATA(txTable[tx_indx].tx_name));
  }

  for(t = 0; t < total_sp_api_found; t++)                               //api
  {
    /*Because we are storing api into spApiTable using its hash as index into spApiTable. That's why its not necessary that all the         api's are stored in sequence, hence need to check whether NULL or not. */
    if (spApiTable[t].sp_api_name == NULL) 
     continue;
    else
     fprintf(sy_file_ptr, "syncpoint_%s\n", spApiTable[t].sp_api_name);
  }
  
  fclose(sy_file_ptr);
 // End of file creation for hash function

  remove_duplicates_sp();

 // Beg of hash_function..() last argument......
 //Here putting 'SyncPointType_SyncPointName' into table so that we can get index of each such combination further.

  sprintf(sync_file, "%s/%s", g_ns_tmpdir, file_name);

  if ((sy_file_ptr = fopen(sync_file, "r")) == NULL) {
    fprintf(stderr, "Error in opening file %s\n", sync_file);
    perror("fopen");
  }

  while (nslb_fgets(sync, SP_MAX_DATA_LINE_LENGTH, sy_file_ptr, 1) != NULL) {
    NSDL4_SP(NULL, NULL, "sync = %s", sync);  

    if (create_syncbytevar_table_entry(&rownum) != SUCCESS)
    {
      NS_EXIT(-1, "Error: Error in getting byte table entry");
    }

    if ((syncByteVarTblEntry[rownum].name = copy_into_big_buf(sync, 0)) == -1)
    {
      NS_EXIT(-1, "Error: Error in copying (%s) into big_buf", sync);
    }
    NSDL4_SP(NULL, NULL, "RETRIEVE_BUFFER_DATA(syncByteVarTblEntry[%d].name) = %s", rownum, RETRIEVE_BUFFER_DATA(syncByteVarTblEntry[rownum].name));
  }
  fclose(sy_file_ptr);
 // End of : hash_function..() last argument.....  

 // generating hash
  total_sp_grp_entries = generate_hash_table_as_index(file_name, "sync_test_hash_variables", &grp_table_hash_func_ptr, &grp_table_var_get_key, 0, g_ns_tmpdir, find_sync_idx);

  NSDL4_SP(NULL, NULL, "total_sp_grp_entries = %d", total_sp_grp_entries);

  if(total_sp_grp_entries == 0){
    NS_EXIT(-1, "Exiting because total_sp_grp_entries = %d", total_sp_grp_entries);
  }

  NSDL4_SP(NULL, NULL, " size of SPGroupTable = %d, size of all_grp_entries = %d", (sizeof(SPGroupTable) * total_sp_grp_entries), (sizeof(int) * (total_sp_grp_entries * (total_runprof_entries + 1))));

  //allocating memory for SPGroupTable and all_grp_entries Table
  MY_MALLOC_AND_MEMSET(spGroupTable, (sizeof(SPGroupTable) * total_sp_grp_entries), "SP Group table creation", -1); 

  MY_MALLOC_AND_MEMSET(all_grp_entries, (sizeof(int) * (total_sp_grp_entries * (total_runprof_entries + 1))), "All group table", -1);

  for(indx = 0; indx < total_syncpoint_entries; indx++)
  { 
    //If syncpoint is inactive then no need to set bit for the given group.
    //we have already memset all bits in the beging
    if(g_sync_point_table[indx].sp_actv_inactv == SP_INACTIVE)
    {
      // Since Inactive syncpoint can be made active during runtime so we can't ignore it.
      NSDL4_SP(NULL, NULL, "Syncpoint %s is inactive. Adding this sycpoint too in the table...", g_sync_point_table[indx].sync_pt_name);
      //continue;
    }
   
    //Get hash code
    if((hash_code = get_grp_tbl_hash_code(g_sync_point_table[indx].sp_type, g_sync_point_table[indx].sync_pt_name, g_sync_point_table[indx].scripts, sp_error)) < 0)
      NS_EXIT(-1, CAV_ERR_1011251, sp_error);
    
    //Take above hash code as index in SpGrpTbl and fill SpGrpTbl
    fill_sp_group_tbl(hash_code, indx);
    //Put above spGroupTable index in other tables: transaction, script, page, api. 
    put_sp_group_tbl_idx_in_othr_tbl(g_sync_point_table[indx].sp_type, indx, hash_code);
 
    NSDL4_SP(NULL, NULL, "spGroupTable[hash_code].sp_type = %d, spGroupTable[%d].pct_usr, = %d, spGroupTable[%d].sync_group_name_as_int = %d ", spGroupTable[hash_code].sp_type, hash_code, spGroupTable[hash_code].pct_usr, hash_code, spGroupTable[hash_code].sync_group_name_as_int, hash_code);
  
    //Put index of SPTableEntry into SPALLGroupEntriesTable to establish linking in between all the three tables: g_sync_point_table, spGroupTable & all_grp_entries
    all_grp_entries[(hash_code * (total_runprof_entries + 1)) + (spGroupTable[hash_code].sync_group_name_as_int + 1)] = indx;

    NSDL4_SP(NULL, NULL, " indx == %d, all_grp_entries index == %d ", indx, ((hash_code * (total_runprof_entries + 1)) + spGroupTable[hash_code].sync_group_name_as_int + 1));
    NSDL4_SP(NULL, NULL, "Method Existing.");
  }
}

/******************************************* SP GROUP TABLE SECTION ENDS ********************************************/

