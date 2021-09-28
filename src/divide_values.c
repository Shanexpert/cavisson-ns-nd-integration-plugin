/*****************************************************************************************
Divide static var Values over nvm's
	sort per_script_totals in oder (sgroupid)
	per_proc_vgroup_table is shm has per_proc_num_script_users start_val and num_vals
	initialized only for sequntail and Uniq vars
	per_proc_num_script_users is the number users/pct for associated script for var-group 
	
	For Seq:
		if (total vals < total_script_num_users)
			all users share all values
		else
			for each val-group divide in proportin of users with alteast 1 per proc

	For Uniq
		if (num_users > num_values) give error
		for each val_group divide in proportin of users with alteast num_user per proc
		if less give error

Anil 09/08/05
**************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/stat.h>

#include "nslb_big_buf.h"

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h" 
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include <libgen.h>

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "divide_users.h"
#include "divide_values.h"
#include "nslb_util.h"
#include "ns_static_use_once.h"
#include "ns_parse_scen_conf.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_static_vars_rtc.h"
#include "ns_trace_level.h"
#include "ns_exit.h"
#include "ns_script_parse.h"
#ifndef  CAV_MAIN
 PerProcVgroupTable *per_proc_vgroup_table;
#else
__thread PerProcVgroupTable *per_proc_vgroup_table;
#endif
PerProcVgroupTable *per_proc_vgroup_table_rtc;
static void dump_group_table_values ();

int
get_per_proc_num_script_users(int proc_id,  char* sess_name)
{
int i;
int num = 0;
  
    NSDL2_SCHEDULE(NULL, NULL, "Method called, proc_id = %d, sess_name = %s", proc_id, sess_name);
    RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;
    for (i=0; i < total_runprof_entries; i++, rstart++)
	if (rstart->sess_ptr->sess_name == sess_name)
	   num += per_proc_runprof_table[(proc_id*total_runprof_entries) + i];
    return num;
}

static int
get_num_script_users (char* sess_name, PerScriptTotal *psTable)
{
int i, gnum;
    NSDL2_SCHEDULE(NULL, NULL, "Method called, sess_name = %s", sess_name);
    RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;
    for (i=0; i < total_runprof_entries; i++, rstart++) {
	if (rstart->sess_ptr->sess_name == sess_name) {
	    gnum =  rstart->group_num;
	    break;
	}
    }
    assert(i != total_runprof_entries);
    return (psTable[gnum].script_total);
}

static int 
comp_by_sgroupid_script (const void *e1, const void *e2)
{
        NSDL2_SCHEDULE(NULL, NULL, "Method called");
	if (((PerScriptTotal *)e1)->sgroup_num > ((PerScriptTotal *)e2)->sgroup_num)
	    return 1;
	else if (((PerScriptTotal *)e1)->sgroup_num < ((PerScriptTotal *)e2)->sgroup_num)
	    return -1;
	else 
	    return 0;
}

#if 0
void assign_values_frm_last_data_file(char *last_data_file, int vgroup_id, int total_values)
{
  int num_nvm_fields, len, i, num_nvm_last;
  char *nvm_index[255];
  char *nvm_index_fields[100];
  char line[MAX_LINE_LENGTH + 1] = "\0";
  FILE *fp_last = NULL;
  int num_nvm = global_settings->num_process; 
  int err_no;
  int  start_val;
  int  num_val;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, last_data_file = %s, vgroup_id = %d", last_data_file, vgroup_id);

  fp_last = fopen(last_data_file, "r");
  err_no = errno;

  if(fp_last == NULL)
  {
    fprintf(stderr, "Cannot open %s File. Error = %s\n", last_data_file, strerror(err_no) );
    exit(-1);
  }

  // last file is present so extract start idx, total number of records from last file
  // AN-TODO ? -- it there is any error in reading last file then we need to return or read data file & distribute.
  if(fp_last) {
     while(nslb_fgets(line, MAX_LINE_LENGTH, fp_last, 0) != NULL) {
       len = strlen(line);
       if(line[0] == '#' || line[0] == '\n')
        continue;
       //num_nvm_last = get_tokens(line, nvm_index, "|", num_nvm);
       num_nvm_last = get_tokens(line, nvm_index, "|", 255);
       // make sure num_nvm == MAX_NVM
       if(num_nvm_last != num_nvm) {
         fprintf(stderr, "Error: Total number of NVMs (%d) in last data file saved in previous test is not same as number of NVMs (%d) in the current test. To resolve this issue, either remove  %s file or give %d NVM in scenario file.\n", num_nvm_last, num_nvm, last_data_file, num_nvm_last);
         exit(-1); 
       }
       
       // Fill for each NVM
       for(i = 0; i < num_nvm_last; i++) 
       {
         if(nvm_index[i] == NULL) { // Should not happen
           fprintf(stderr, "Error: Invalid last file. Number of fields is 0 for nvm (%d) is last file '%s'.\n", i, last_data_file);
           exit(-1); 
         }

         //FORMAT: start_idx1,num_recs1|start_idx2,num_recs2|start_idx3,num_recs3|start_idx3,num_recs3|start_idx3,num_recs3
         num_nvm_fields = get_tokens(nvm_index[i], nvm_index_fields, ",",  2);
         if(num_nvm_fields != 2) {
           fprintf(stderr, "Error: Invalid last file. Number of fields for nvm (%d) is not 2. Last file name = %s\n", i, last_data_file);
           exit(-1); 
         }

         if(!(nvm_index_fields[0] && nvm_index_fields[1])) { // NULL Check
           fprintf(stderr, "Error: Invalid last file. One or both fields for nvm (%d) is empty. Last file name = %s\n", i, last_data_file);
           break; 
         }
/*
	 if (!(per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].num_script_users)) 
         {
           per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].num_val = 0;
           per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].start_val = 0;
           continue;
         }
*/       
         start_val = atoi(nvm_index_fields[0]);
         num_val = atoi(nvm_index_fields[1]);

         /*If last file start_val and/or (start_val + num_val) value for any NVM is >= number of records in the data file,
          *then error is given and test is aborted.*/
         if((per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].num_val + per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].start_val)!= (start_val + num_val))
         {
           fprintf(stderr, "Error: Invalid last file. Start record (%d) or end record number (%d) for nvm (%d) is more than total number of records (%d) in the data file. Last file name = %s\n", start_val, num_val, i, per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].num_val, last_data_file);
           //exit(-1);
         } 
         
	 per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].start_val = start_val;
	 per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].num_val = num_val;

         NSDL2_SCHEDULE(NULL, NULL, "From Last file - NVM_ID = %d, Start Record Number = %d, Number of records = %d\n",
                 i, per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].start_val, per_proc_vgroup_table[(i * total_group_entries) + vgroup_id].num_val);
       }
       break; // we have to read only one line other than comment & new line
     }
  }
  dump_group_table_values();
  if(fp_last) {
    fclose(fp_last);
  }
}

static int chk_if_last_file_read_needed_for_use_once (int vgroup_id, int total_values)
{
  char last_data_file[5 *1024];
  time_t data_last_file_modification_time;
  time_t data_file_modification_time;
  char locl_data_fname[5 * 1024]; 
  char locl_data_fname2[5 * 1024]; 
    
  if(group_table_shr_mem[vgroup_id].sequence != USEONCE) 
    return 0;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. vgroup_id = %d", vgroup_id);

  // TODO: Handle abs and relative  
  //Get last modification time of data.last file if there. 
  strcpy(locl_data_fname, group_table_shr_mem[vgroup_id].data_fname);
  strcpy(locl_data_fname2, group_table_shr_mem[vgroup_id].data_fname);
   
  sprintf(last_data_file, "%s/.%s.last", dirname(locl_data_fname), basename(locl_data_fname2));

//  sprintf(last_data_file, "%s.last", group_table_shr_mem[vgroup_id].data_fname);
  NSDL2_SCHEDULE(NULL, NULL, "Last Data File = %s", last_data_file);
 // fprintf(stderr, "Last Data File = %s\n", last_data_file);
  data_last_file_modification_time = get_last_access_time(last_data_file);
  NSDL2_SCHEDULE(NULL, NULL, "Last data file modification time = %u\n", (unsigned int)data_last_file_modification_time);

  if(data_last_file_modification_time == -1) //Last data file is not there
    return 0;

  //Get last modification time of data file 
  data_file_modification_time = get_last_access_time(group_table_shr_mem[vgroup_id].data_fname);

  NSDL2_SCHEDULE(NULL, NULL, "Data file modification time = %u\n", (unsigned int)data_file_modification_time);
  
  //Check if data.last file is updated most recently than the data file
   //If yes than copy the data.last file  
  if(data_file_modification_time < data_last_file_modification_time)
  {
    assign_values_frm_last_data_file(last_data_file, vgroup_id, total_values);
    return 1;
  }

  NSDL2_SCHEDULE(NULL, NULL, "Last data file (%s) is older than data file (%s) for use once parameter. Ignoring last data file\n", last_data_file, group_table_shr_mem[vgroup_id].data_fname);

  return 0;
}
#endif

static int is_nvm_running(int nvm_id)
{
  if((g_msg_com_con[nvm_id].fd == -1))
     return 0;

  return 1;
}

static int distribute_values (PerProcVgroupTable *lol_per_proc_vgroup_table, int total_users, int vgroup_id, int runtime, int fparamrtc_idx, int total_vals)
{
  int i, min=1, num_val, num_users, used_val = 0, leftover;
  int seq = group_table_shr_mem[vgroup_id].sequence;
  int ppidx; 
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called, total_users = %d, total_vals = %d, vgroup_id = %d, runtime = %d, fparamrtc_idx = %d", 
                              total_users, total_vals, vgroup_id, runtime, fparamrtc_idx);

  for (i = 0; i < global_settings->num_process; i++) {
    /* In case of RTC, nvm index is retrived from 'g_msg_com_con' structure.*/
    //nvm_idx = runtime?g_msg_com_con[i].nvm_index:i;
    ppidx = (i * total_group_entries) + vgroup_id;
    num_users = lol_per_proc_vgroup_table[ppidx].num_script_users;
    NSDL2_SCHEDULE(NULL, NULL, "num_users = %d", num_users);
    if (!num_users) continue;

    if(!runtime || is_nvm_running(i)){ 
      if ((global_settings->load_key) && (seq == UNIQUE)) min = num_users;
      if(total_users)
        num_val = (total_vals * num_users)/total_users;
      if (num_val < min) num_val = min;
      lol_per_proc_vgroup_table[ppidx].num_val = num_val;
    }
    //For killed NVMs, consider their previous total value if mode is APPEND.
    else if((fparam_rtc_tbl[fparamrtc_idx].mode == APPEND_MODE))
      lol_per_proc_vgroup_table[ppidx].num_val = per_proc_vgroup_table[ppidx].total_val;

    NSDL2_SCHEDULE(NULL, NULL, "num_val = %d", lol_per_proc_vgroup_table[ppidx].num_val);
    used_val += lol_per_proc_vgroup_table[ppidx].num_val;
  }

  leftover = total_vals - used_val;
  NSDL2_SCHEDULE(NULL, NULL, "total_vals = %d, used_val = %d, leftover = %d", total_vals, used_val, leftover);

  if (leftover < 0 ) {
            /* Insufficient number of variable values specified in data file <filename> used in file parameter with mode .Unique. in script Visa01_SignInSignOut_d1. Total users for the scenario group using this script are xxx and total number of variable values in this file are yyy. */
	    /*printf("Error: Insufficient number of variable values specified in data file '%s' used in file parameter with mode Unique in script '%s'. Total users for the scenario group using this script are '%d' and total number of variable values in this file are '%d'.\n", 
			group_table_shr_mem[vgroup_id].data_fname, group_table_shr_mem[vgroup_id].sess_name, 
                        total_users, total_vals);*/
    NSTL1(NULL, NULL, "Error: Insufficient number of variable values specified in data file '%s' used in file parameter with mode Unique in script '%s'. Total users for the scenario group using this script are '%d' and total number of variable values in this file are '%d'.",
                        group_table_shr_mem[vgroup_id].data_fname, group_table_shr_mem[vgroup_id].sess_name,
                        total_users, total_vals);
    if(!runtime)
      NS_EXIT(1, CAV_ERR_1012099_MSG, group_table_shr_mem[vgroup_id].data_fname,
                                     group_table_shr_mem[vgroup_id].sess_name, total_users, total_vals);
      return -1;
  }

  i = 0;
  while (leftover) {
     //nvm_idx = runtime?g_msg_com_con[i].nvm_index:i;
     ppidx = (i * total_group_entries) + vgroup_id;    
     if (((!runtime || is_nvm_running(i)) && (lol_per_proc_vgroup_table[ppidx].num_script_users))) {
	lol_per_proc_vgroup_table[ppidx].num_val++;
        leftover--;
     }
     i++;
     if (i == global_settings->num_process) i = 0;
  }

  used_val = 0;
  for (i = 0; i < global_settings->num_process; i++) {
    ppidx = (i * total_group_entries) + vgroup_id;

    if (!(lol_per_proc_vgroup_table[ppidx].num_script_users)) {
       lol_per_proc_vgroup_table[ppidx].num_val = 0;
       lol_per_proc_vgroup_table[ppidx].start_val = 0;
       lol_per_proc_vgroup_table[ppidx].cur_val = 0;
       continue;
    }

    lol_per_proc_vgroup_table[ppidx].start_val = used_val;
    num_val = lol_per_proc_vgroup_table[ppidx].num_val;
    lol_per_proc_vgroup_table[ppidx].total_val = num_val;
    used_val += num_val;
  }
        
  //chk_if_last_file_read_needed_for_use_once (vgroup_id, total_vals);
  return 0;
}

/* Name      : chk_rbu_group
   Puropose  : This function will check whether input NS file api (i.e. group) belongs to RBU or not?
   Return    : On Success - 1
             : On Failure - 0
   Build_ver : 4.1.5 # 37
   Modification :
               Bug 16309 - NC|RBU|AUTO PARAM:Not able to run the test if we have data files less than number of users
*/
int chk_rbu_group(int file_api_idx)
{
  int sess_idx = 0;
  char param[256 + 1] = "";

  NSDL2_RBU(NULL, NULL, "Method called, file_api_idx = %d", file_api_idx);

  //Find respective script
  for(sess_idx = 0; sess_idx < total_sess_entries; sess_idx++) 
    if(strcmp(group_table_shr_mem[file_api_idx].sess_name, session_table_shr_mem[sess_idx].sess_name) == 0)
      break;
  
  //check variable is of rbu script or not
  sprintf(param, "{%s}", variable_table_shr_mem[group_table_shr_mem[file_api_idx].start_var_idx].name_pointer);

  //Bug 70228 || Check flow have a page or not
  if(session_table_shr_mem[sess_idx].first_page == NULL)
    return 0;

  if((session_table_shr_mem[sess_idx].first_page)->first_eurl->proto.http.rbu_param.browser_user_profile == NULL)
    return 0;

  NSDL2_RBU(NULL, NULL, "param = %s, browser_user_profile = %s ", param,
                         (session_table_shr_mem[sess_idx].first_page)->first_eurl->proto.http.rbu_param.browser_user_profile);

  //If any page of script (i.e. sess_idx) contain RBU attribute (i.e. browser_user_profile, har_log_dir or vnc_display_id) then
  // that parameter should be ignored 
  if(!strcmp(param, (session_table_shr_mem[sess_idx].first_page)->first_eurl->proto.http.rbu_param.browser_user_profile))
    return 1;
 
  return 0;
}


/***********************************************************************************************
* Function_name :-  dump_distributed_value()
* Purpose       :-  This will save distributed data over the nvm at path 
*                   /home/<user>/<controller>/logs/TRxx/partition_idx/scripts/
*                   <script_name>/<Data File>.<first_paramter>.<nvm_id>.<mode>
*       
* Input         :-  -
* Output        :-  - 
* Build_ver     :-  4.1.6 #
* Resolved_Enh  :   Bug 19916 - Data file distribution records should be according to the 
*                               NVM on Controller for both Sequential and Unique mode
************************************************************************************************/
static void dump_distributed_value(int runtime)
{
  int i,j;
  int grp_idx, ppvgrp_idx, proc_idx, widx;
  char fname[1024 + 1] = {"\0"};
  char script_name[1024 + 1] = "";
  char *data_fname;
  char *column_delimiter = NULL;
  char col_del[128 + 1];
  struct stat stat_buf;  
  FILE *wfp = NULL;
  PointerTableEntry_Shr *value_st = NULL;
  PerProcVgroupTable *loc_per_proc_vgroup_table = (!runtime)?per_proc_vgroup_table:per_proc_vgroup_table_rtc;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method called, total_group_entries = %d, runtime = %d", total_group_entries, runtime);
  for(grp_idx = 0; grp_idx < total_group_entries; grp_idx++) // nsl_static_var
  {
    //To resolve bug 24849 - Core is comming in file parameter test, when two API(nsl_static_var & nsl_index_file_var) are used at a time and keyword SAVE_NVM_FILE_PARAM_VAL is present in scenario  
    if(group_table_shr_mem[grp_idx].index_key_var_idx != -1)
     continue;
   
    data_fname = strrchr(group_table_shr_mem[grp_idx].data_fname, '/');
    if(!data_fname) continue; //Never go in this flag as data_fname always with absolute path

    for(proc_idx = 0; proc_idx < global_settings->num_process; proc_idx++)  //NVM
    {
      ppvgrp_idx = (proc_idx * total_group_entries) + grp_idx;
      if(!loc_per_proc_vgroup_table[ppvgrp_idx].total_val) continue;

      column_delimiter = group_table_shr_mem[grp_idx].column_delimiter;
      //script name
      strcpy(script_name, (get_sess_name_with_proj_subproj_int(group_table_shr_mem[grp_idx].sess_name, group_table_shr_mem[grp_idx].sess_idx, "/")));

      //Make dump file name like <original_file_name>.<nvm_id>.
      //In case of RTC make different file name
      char tmp_file_name[1024 + 1] = "\0";
      sprintf(fname, "%s/logs/TR%d/%lld/scripts/%s",g_ns_wdir, testidx, g_partition_idx, script_name); 
 
      if(!proc_idx && !runtime)
      {
        NSDL2_SCHEDULE(NULL, NULL, "path =%s", fname);
        if(stat(fname, &stat_buf) == -1)
        {
	  NSDL2_SCHEDULE(NULL, NULL, "path = %s DOES NOT Exists, so creating", fname);
          sprintf(tmp_file_name, "mkdir -p %s", fname);
          system(tmp_file_name);
        }
      }

      if(runtime)
        sprintf(fname, "%s%s.%d.rtc", fname,data_fname, proc_idx);
      else 
        sprintf(fname, "%s%s.%d", fname,data_fname, proc_idx);
      
      NSDL2_SCHEDULE(NULL, NULL, "Dump distributed value for NVM = %d, data value into file = %s, total_val = %d",
                                  proc_idx, fname, per_proc_vgroup_table[ppvgrp_idx].total_val);

      if((wfp = fopen(fname, "a")) == NULL)
      {
        fprintf(stderr, "Error: dump_distributed_value() - unable to open file '%s'. errno(%d)=%s.\n",
                         fname, errno, nslb_strerror(errno));
        continue; //Try for next nvm
      }

      for(i = 0; i < loc_per_proc_vgroup_table[ppvgrp_idx].total_val; i++)
      {
        widx = 0;
        for(j = 0; j < loc_per_proc_vgroup_table[ppvgrp_idx].group_table_shr_mem->num_vars; j++)
        {

          //TODO: #Ayush - need to handle segmented value here
          value_st = loc_per_proc_vgroup_table[ppvgrp_idx].variable_table_shr_mem[j].value_start_ptr;
          if (loc_per_proc_vgroup_table[ppvgrp_idx].variable_table_shr_mem[j].is_file != IS_FILE_PARAM)
          { 
            NSDL2_SCHEDULE(NULL, NULL, "value = %s, ppvgrp_idx = %d", value_st[i].big_buf_pointer, ppvgrp_idx);
            /******************************************************************************************************************
            Abhay: BUG 62125  - Test is not started and core is formed when we use WebSocket script with 
                                SAVE_NVM_FILE_PARAM_VAL 1 keyword in scenario. 
                   RCA        - Previously we were using buffer(lbuf) of size 2048 bytes then dumping it 
                                into data distribution file, while doing sprintf of size more than 2048 
                                bytes stack smashing was detected.
                   Resolution - We will directly dump the data into data distribution file.
            *******************************************************************************************************************/
            widx = snprintf(col_del, 128, "%s", (j < (loc_per_proc_vgroup_table[ppvgrp_idx].group_table_shr_mem->num_vars -1)?column_delimiter:"\n"));
            NSDL2_SCHEDULE(NULL, NULL, "wdix = %d", widx);
            fwrite(value_st[i].big_buf_pointer, 1, value_st[i].size, wfp);
            fwrite(col_del, 1, widx, wfp);
          }
        }

        //fwrite(value_st[i].big_buf_pointer, 1, value_st[i].size, wfp);
      }
      /*Resolve bug 34349 - SQL_Parameter| Incorrect data lines got distributed on NVMs while applying sql_Parameter */
      fwrite("\n\n", 1, 2, wfp);
      fclose(wfp);
    }
  }
}


int divide_values(PerScriptTotal *psTable, int runtime, int create_per_proc_staticvar_shr_mem)
{
  int i,j;
  int total_vals;
  int total_script_users;
  int seq;
  int is_rbu_test = 0; //Default is non-rbu
  int fparamrtc_idx;
  int shm_size = 0;
  int ppidx = 0;
  PerProcVgroupTable *lol_per_proc_vgroup_table = NULL;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, psTable = %p, runtime = %d, create_per_proc_staticvar_shr_mem = %d", 
                              psTable, runtime, create_per_proc_staticvar_shr_mem);

  qsort(psTable, total_runprof_entries, sizeof(PerScriptTotal), comp_by_sgroupid_script);

  shm_size = global_settings->num_process * total_group_entries * sizeof(PerProcVgroupTable);
  NSDL2_SCHEDULE(NULL, NULL, "shm_size = %d", shm_size);

  if(!runtime)
  {
    per_proc_vgroup_table = (PerProcVgroupTable *) do_shmget(shm_size, "PerProcVgroupTable");
    lol_per_proc_vgroup_table = per_proc_vgroup_table;
 
    if((global_settings->protocol_enabled & RBU_API_USED) && global_settings->rbu_enable_auto_param)
      is_rbu_test = 1;
  }
  else
  {
    //if(!create_per_proc_staticvar_shr_mem)
      RTC_MALLOC_AND_MEMSET(per_proc_vgroup_table_rtc, shm_size, "per_proc_vgroup_table_rtc", -1);

    lol_per_proc_vgroup_table = (PerProcVgroupTable *)per_proc_vgroup_table_rtc;
  }

  NSDL2_SCHEDULE(NULL, NULL, "lol_per_proc_vgroup_table = %p, global_settings->num_process = %d, total_group_entries = %d", 
                              lol_per_proc_vgroup_table, global_settings->num_process, total_group_entries);

  for (i = 0; i < global_settings->num_process; i++) {
    for (j = 0; j < total_group_entries; j++) 
    {
      //Unset as, if AUTO PARAM feature of RBU with C type script is enabled by-default and also global.
      //So, in JTS, we need to unset,  s we dont have 'action_request_Shr' structure
      //If future, if JTS and C type script both with RBU will invoked then there may be some issues
      if(session_table_shr_mem[group_table_shr_mem[j].sess_idx].script_type == NS_SCRIPT_TYPE_JAVA)
        is_rbu_test = 0;
   
      ppidx = (i * total_group_entries) + j;
    
      lol_per_proc_vgroup_table[ppidx].num_script_users = get_per_proc_num_script_users(i, group_table_shr_mem[j].sess_name);
      lol_per_proc_vgroup_table[ppidx].start_val = 0;
      lol_per_proc_vgroup_table[ppidx].cur_val = 0;
      lol_per_proc_vgroup_table[ppidx].num_val = 0;
      lol_per_proc_vgroup_table[ppidx].total_val = 0;
      lol_per_proc_vgroup_table[ppidx].rtc_flag = -1;
      NSDL2_SCHEDULE(NULL, NULL, "is_rbu_test - %d", is_rbu_test);
    }
  }

  // Allocate memory for Static var Shared memory
  if(!runtime)
    alloc_static_vars_shr_mem();

  for (i = 0; i < global_settings->num_process; i++) 
  {
    for (j = 0; j < total_group_entries; j++) 
    {
      ppidx = (i * total_group_entries) + j;
      /* Resolving Bug 86980 - NC|script users is 0 then do not divide values*/
      if(!lol_per_proc_vgroup_table[ppidx].num_script_users) continue;

      /* Resolving Bug 16309 - NC|RBU|AUTO PARAM:Not able to run the test if we have data files less than number of users */
      if(is_rbu_test && (chk_rbu_group(j) == 1)) continue;
 
      total_script_users = get_num_script_users(group_table_shr_mem[j].sess_name, psTable);
      total_vals = group_table_shr_mem[j].num_values;
      seq = group_table_shr_mem[j].sequence;

      if(runtime)
      {
        if(!(fparamrtc_idx = is_rtc_applied_on_group(lol_per_proc_vgroup_table[ppidx].num_script_users, j))) 
          continue;

        fparamrtc_idx--; //Reduced as is_rtc_applied_on_group() added +1 

        lol_per_proc_vgroup_table[ppidx].rtc_flag = fparam_rtc_tbl[fparamrtc_idx].mode;

        if((fparam_rtc_tbl[fparamrtc_idx].mode == APPEND_MODE))
          total_vals = group_table_shr_mem[j].num_values + fparam_rtc_tbl[fparamrtc_idx].num_values;
        else
          total_vals = fparam_rtc_tbl[fparamrtc_idx].num_values;

        NSDL2_SCHEDULE(NULL, NULL, "Set total_vals: nvm = %d, fparam group = %d, fparamrtc_idx = %d, mode = %d, load_key = %d, "
                                   "total_vals = %d, total_script_users = %d, num_process = %d, seq = %d, new values = %d, old values = %d",
                                    i, j, fparamrtc_idx, fparam_rtc_tbl[fparamrtc_idx].mode, global_settings->load_key, total_vals, 
                                    total_script_users, global_settings->num_process, seq, 
                                    fparam_rtc_tbl[fparamrtc_idx].num_values, group_table_shr_mem[j].num_values);
      }

      NSDL2_SCHEDULE(NULL, NULL, "seq = %d, total_script_users = %d, total_vals = %d", seq, total_script_users, total_vals);
      if (seq == SEQUENTIAL)
      {
        //TODO: Manish: this feture will not work form 4.1.6
        if (((global_settings->load_key) && (total_vals < total_script_users)) || 
      			((!(global_settings->load_key)) && (total_vals < global_settings->num_process))) 
        {
          lol_per_proc_vgroup_table[ppidx].start_val = 0;
          lol_per_proc_vgroup_table[ppidx].cur_val = 0;
      	  lol_per_proc_vgroup_table[ppidx].num_val = total_vals;
      	  lol_per_proc_vgroup_table[ppidx].total_val = total_vals;
        }
        else
        {
          if(distribute_values(lol_per_proc_vgroup_table, total_script_users, j, runtime, fparamrtc_idx, total_vals) == -1)
            return -1;
        }
      } 
      else if (seq == UNIQUE) {
        if(distribute_values(lol_per_proc_vgroup_table, total_script_users, j, runtime, fparamrtc_idx, total_vals) == -1)
          return -1;
      }
      else if (seq == USEONCE)
      {
  
        NSDL2_SCHEDULE(NULL, NULL, "total_vals = %d, global_settings->num_process = %d", total_vals, global_settings->num_process);
        if(total_vals < global_settings->num_process)
        {
          if(!runtime)
          {
            fprintf(stderr, "Total numbers of values (%d) in data file (%s) of script (%s) is less "
                            "than the total number of NVMs (%d). Aborting the test run ...\n", 
                             total_vals, group_table_shr_mem[j].data_fname, group_table_shr_mem[j].sess_name, global_settings->num_process);

            NSTL1(NULL, NULL, "Total numbers of values (%d) in data file (%s) of script (%s) is less"
                              "than the total number of NVMs (%d). Aborting the test run ...",
                              total_vals, group_table_shr_mem[j].data_fname, group_table_shr_mem[j].sess_name, global_settings->num_process);
           NS_EXIT(-1, CAV_ERR_1031055, total_vals, group_table_shr_mem[j].data_fname, group_table_shr_mem[j].sess_name, global_settings->num_process);
          }

          fprintf(stderr, "Total numbers of values (%d) in data file (%s) of script (%s) is less "
                          "than the total number of NVMs (%d). Aborting the RTC ...\n", 
                           total_vals, group_table_shr_mem[j].data_fname, group_table_shr_mem[j].sess_name, global_settings->num_process);
       
          NSTL1(NULL, NULL, "Total numbers of values (%d) in data file (%s) of script (%s) is less" 
                            "than the total number of NVMs (%d). Aborting the test run ...",
                            total_vals, group_table_shr_mem[j].data_fname, group_table_shr_mem[j].sess_name, global_settings->num_process); 
          return -1;
        }
        else
        {
          NSDL2_SCHEDULE(NULL, NULL, "total_script_users = %d, j = %d", total_script_users, j);
          if(distribute_values(lol_per_proc_vgroup_table, total_script_users, j, runtime, fparamrtc_idx, total_vals) == -1)
            return -1;
        }
      }
      else if((seq == WEIGHTED) || (seq == RANDOM))
      {
        lol_per_proc_vgroup_table[ppidx].start_val = 0;
        lol_per_proc_vgroup_table[ppidx].cur_val = 0;
        lol_per_proc_vgroup_table[ppidx].num_val = total_vals;
        lol_per_proc_vgroup_table[ppidx].total_val = total_vals;
      }

      NSDL2_SCHEDULE(NULL, NULL, "j = %d, index_key_var_idx = %d", j, group_table_shr_mem[j].index_key_var_idx);
      if(create_per_proc_staticvar_shr_mem && (group_table_shr_mem[j].index_key_var_idx == -1))
      {
        /*Tanmay: Allocating shared memory for all File param groups of particular NVM as per new design (i.e. File Parameter RTC) 
               if and only if data is distributed to that NVM */
        NSDL2_SCHEDULE(NULL, NULL, "Allocating shared memory for NVM = %d, File param group = %d, num_val = %d", 
                                    i, j, lol_per_proc_vgroup_table[ppidx].num_val);
        if(lol_per_proc_vgroup_table[ppidx].num_val)
          per_proc_create_staticvar_shr_mem(i, j, runtime);
      }
    }
  }

  if(total_group_entries != 0)
  {
    if(!runtime)
      dump_group_table_values();
    else
      dump_rtc_group_table_values();
  }

  #ifdef NS_DEBUG_ON
  /*Manish: just show data structures */
  dump_per_proc_vgroup_table(lol_per_proc_vgroup_table);
  #endif

  if(!runtime)
  {
    /*This is to open and save the last file FD.*/
    open_last_file_fd();
  }
  else
  {
    for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
    {
      if(fparam_rtc_tbl[i].mode == APPEND_MODE)
        fparam_rtc_tbl[i].num_values += group_table_shr_mem[fparam_rtc_tbl[i].fparam_grp_idx].num_values;
    }
  }

  /* Enh (bugId-19916):Dump distributed data into TRxx dir */
  if(global_settings->save_nvm_file_param_val)
    dump_distributed_value(runtime);

  return 0;
}


//Add by Manish: Date: Wed Nov  2 13:05:23 IST 2011
/*
For GroupTableEntry.sequence 
SEQUENTIAL     1
RANDOM         2
WEIGHTED       3
UNIQUE         4
USEONCE        5
*/ 
char *find_mode_from_seq_number(short seq)
{
  static char mode[1024];

  switch(seq)
  {
    case USEONCE:
      strcpy(mode,"USEONCE");
      break;
    case UNIQUE:
      strcpy(mode,"UNIQUE");
      break;
    case WEIGHTED:
      strcpy(mode,"WEIGHTED");
      break;
    case RANDOM:
      strcpy(mode,"RANDOM");
      break;
    case SEQUENTIAL:
      strcpy(mode,"SEQUENTIAL");
      break;
  }
 
  return mode;
}

/*
SESSION 1
USE 2
*/
char *find_type_from_type_number(short g_type)
{
  static char type[1024];

  switch(g_type)
  {
    case SESSION:
      strcpy(type,"SESSION");
      break;
    case USE:
      strcpy(type,"USE");
      break;
  }

  return type;
}


static void dump_group_table_values()
{
  int i, j;
  char *mode = NULL;
  char *type = NULL;

  //printf("File Parameter Distribution among NVMs:\n");
  NSTL1(NULL, NULL, "File Parameter Distribution among NVMs:");
  for (i = 0; i < total_group_entries; i++) 
  {
    NSDL2_SCHEDULE(NULL, NULL, "\nGroup=%d sess=%s total_vals=%d type=%d num_vars=%d\n", 
                           i, group_table_shr_mem[i].sess_name, group_table_shr_mem[i].num_values, 
                           group_table_shr_mem[i].sequence, group_table_shr_mem[i].num_vars);

    mode = find_mode_from_seq_number(group_table_shr_mem[i].sequence);
    type = find_type_from_type_number(group_table_shr_mem[i].type);
 
    /*printf("Script=%s, DataFile=%s, TotalValues=%d, Mode=%s, Refresh=%s\n", 
                group_table_shr_mem[i].sess_name, group_table_shr_mem[i].data_fname, group_table_shr_mem[i].num_values, mode, type);*/
    NSTL1(NULL, NULL, "Script=%s, DataFile=%s, TotalValues=%d, Mode=%s, Refresh=%s",
                group_table_shr_mem[i].sess_name, group_table_shr_mem[i].data_fname, group_table_shr_mem[i].num_values, mode, type);
    NSDL2_SCHEDULE(NULL, NULL, "Script=%s, DataFile=%s, TotalValues=%d, Mode=%s, Refresh=%s\n", 
                group_table_shr_mem[i].sess_name, group_table_shr_mem[i].data_fname, group_table_shr_mem[i].num_values, mode, type);
    if(get_group_mode(-1) == TC_FIX_CONCURRENT_USERS)
    {
      for (j = 0; j < global_settings->num_process; j++) 
      {
        /*printf("    NVM%d: Users=%d, StartVal=%d, NumVal=%d\n", j, 
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_script_users,
                    per_proc_vgroup_table[(j * total_group_entries) + i].start_val,
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_val);*/
        NSTL1(NULL, NULL, "    NVM%d: Users=%d, StartVal=%d, NumVal=%d", j,
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_script_users,
                    per_proc_vgroup_table[(j * total_group_entries) + i].start_val,
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_val);
        NSDL2_SCHEDULE(NULL, NULL, "   NVM%d: Users=%d, StartVal=%d, NumVal=%d\n", j, 
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_script_users,
                    per_proc_vgroup_table[(j * total_group_entries) + i].start_val,
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_val);
      }
    }
    else
    {
      for (j = 0; j < global_settings->num_process; j++)
      {
        /*printf("    NVM%d: StartVal=%d, NumVal=%d\n", j,
                    per_proc_vgroup_table[(j * total_group_entries) + i].start_val,
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_val);*/
        NSTL1(NULL, NULL, "    NVM%d: StartVal=%d, NumVal=%d", j,
                    per_proc_vgroup_table[(j * total_group_entries) + i].start_val,
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_val);
        NSDL2_SCHEDULE(NULL, NULL, "    NVM%d: StartVal=%d, NumVal=%d\n", j,
                    per_proc_vgroup_table[(j * total_group_entries) + i].start_val,
                    per_proc_vgroup_table[(j * total_group_entries) + i].num_val);
      }
    }
  }
}

