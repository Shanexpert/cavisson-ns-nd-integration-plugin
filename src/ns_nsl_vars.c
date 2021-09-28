/*  ns_nsl_vars.c  ************/
/*  Date - 21 Aug 2013 ********/
/******************************/


/********************************************************/ 
/* This file handle both declare array and declare var  */
/********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nslb_parse_api.h"
#include "nslb_util.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "util.h"
#include "ns_nsl_vars.h"
#include "ns_static_vars.h"
#include "tmr.h"
#include "timing.h"
#include "ns_schedule_phases.h"
#include "logging.h"
#include "netstorm.h"
#include "ns_error_msg.h"
#include "ns_script_parse.h"


int input_nsl_var(char *line, int line_number, int session_idx, char *s_fname/*script file name */)
{
  NSApi api_ptr; 
  char err_msg[MAX_ERR_MSG_SIZE + 1] = "";
  char msg[MAX_LINE_LENGTH] = "";
  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
  int rnum, ret;

  /*Prepare error message*/ 
  sprintf(msg, "script - %s/%s, line number - %d, api - nsl_decl_param()", get_sess_name_with_proj_subproj_int(sess_name, session_idx, "/"),
		s_fname, line_number);
  //sprintf(msg, "script - %s/%s, line number - %d, api - nsl_decl_array()", get_sess_name_with_proj_subproj(sess_name), s_fname, line_number);
 
  NSDL1_VARS(NULL, NULL, "Method called. script_filename = %s, line number = %d", s_fname, line_number);
  //parse_api_ex(&api_ptr, line, s_fname, err_msg, line_number, 1, 1);
  ret = parse_api_ex(&api_ptr, line, s_fname, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  if(api_ptr.num_tokens <= 0 || api_ptr.num_tokens > 3){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012126_ID, CAV_ERR_1012126_MSG);
  }

  if(!api_ptr.api_fields[0].keyword[0]){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012127_ID, CAV_ERR_1012127_MSG);
  }

  /*create ns var table entry */ 
  create_nsvar_table_entry(&rnum);

  if (gSessionTable[session_idx].nslvar_start_idx == -1) {
    gSessionTable[session_idx].nslvar_start_idx = rnum;
    gSessionTable[session_idx].num_nslvar_entries = 0;
  }
  gSessionTable[session_idx].num_nslvar_entries++;
  
    
  //save name.
  if(validate_var(api_ptr.api_fields[0].keyword)){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, api_ptr.api_fields[0].keyword);
  }
  
  if ((nsVarTable[rnum].name = copy_into_temp_buf(api_ptr.api_fields[0].keyword, 0)) == -1) {
    NS_EXIT(-1, CAV_ERR_1000018, api_ptr.api_fields[0].keyword);
  }
 
  /* set default values */
  nsVarTable[rnum].type = NS_VAR_SCALAR;
  nsVarTable[rnum].length = 0;
  nsVarTable[rnum].default_value = -1;
  nsVarTable[rnum].retain_pre_value = RETAIN_PRE_VALUE;
 
  /*Start parsing */
  int j = 0; 
  char *key;
  char *value;
   
  for (j = 1; j < api_ptr.num_tokens; j++)
  {
    key = api_ptr.api_fields[j].keyword;
    value = api_ptr.api_fields[j].value;
    NSDL2_VARS(NULL, NULL, "Key - %s, value = %s", key, value);
    if(!strcasecmp(key, "DefaultValue"))
    {
      if((nsVarTable[rnum].default_value = copy_into_big_buf(value, 0)) == -1) {
        NS_EXIT(-1, CAV_ERR_1000018, value);
      }
    }
    else if (!strcasecmp(key, "RetainPreValue")) 
    {
      NSDL2_VARS(NULL, NULL, "RetainPreValue  = [%s]", value);
      if (!strcasecmp(value, "Yes"))
        nsVarTable[rnum].retain_pre_value = RETAIN_PRE_VALUE;
      else if (!strcasecmp(value, "No"))
        nsVarTable[rnum].retain_pre_value = NOT_RETAIN_PRE_VALUE;
      else
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "declare");
      }
    }
    else 
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, key, "declare");
    }
  }
  return 0;
}

int input_nsl_array_var(char *line, int line_number, int session_idx, char *s_fname/*script file name */)
{
  NSApi api_ptr; 
  char err_msg[MAX_ERR_MSG_SIZE + 1] = "";
  char msg[MAX_LINE_LENGTH] = "";
  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
  int rnum, ret;

  /*Prepare error message*/ 
  sprintf(msg, "script - %s/%s, line number - %d, api - nsl_decl_array()", get_sess_name_with_proj_subproj_int(sess_name, session_idx, "/"),
		s_fname, line_number);
  //sprintf(msg, "script - %s/%s, line number - %d, api - nsl_decl_array()", get_sess_name_with_proj_subproj(sess_name), s_fname, line_number);
 
  NSDL1_VARS(NULL, NULL, "Method called. script_filename = %s, line number = %d", s_fname, line_number);
  //parse_api_ex(&api_ptr, line, s_fname, err_msg, line_number, 1, 1);
  ret = parse_api_ex(&api_ptr, line, s_fname, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  if(api_ptr.num_tokens <= 0){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012126_ID, CAV_ERR_1012126_MSG);
  }

  if(!api_ptr.api_fields[0].keyword[0]){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012127_ID, CAV_ERR_1012127_MSG);
  }

  /*create ns var table entry */ 
  create_nsvar_table_entry(&rnum);

  if (gSessionTable[session_idx].nslvar_start_idx == -1) {
    gSessionTable[session_idx].nslvar_start_idx = rnum;
    gSessionTable[session_idx].num_nslvar_entries = 0;
  }
  gSessionTable[session_idx].num_nslvar_entries++;
  
    
  //save name.
  if(validate_var(api_ptr.api_fields[0].keyword)){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, api_ptr.api_fields[0].keyword);
  }
  
  if ((nsVarTable[rnum].name = copy_into_temp_buf(api_ptr.api_fields[0].keyword, 0)) == -1) {
    NS_EXIT(-1, CAV_ERR_1000018, api_ptr.api_fields[0].keyword);
  }
 
  /* set default values */
  nsVarTable[rnum].type = NS_VAR_ARRAY; 
  nsVarTable[rnum].length = 1;
  nsVarTable[rnum].default_value = -1;
  nsVarTable[rnum].retain_pre_value = RETAIN_PRE_VALUE; 
  /*Start parsing */
  int j = 0; 
  char *key;
  char *value;
  char size_flag = 0;
  for (j = 1; j < api_ptr.num_tokens; j++)
  {
    key = api_ptr.api_fields[j].keyword;
    value = api_ptr.api_fields[j].value; 
    NSDL2_VARS(NULL, NULL, "Key - %s, value = %s", key, value);
     
    if(!strcasecmp(key, "SIZE")){
      size_flag = 1;
      if(!ns_is_numeric(value)) {
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012128_ID, CAV_ERR_1012128_MSG);
      }
      if(atoi(value) <= 0 || atoi(value) > 10000)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012129_ID, CAV_ERR_1012129_MSG);
      } 
      nsVarTable[rnum].length = atoi(value);
    }
    else if(!strcasecmp(key, "DefaultValue")){
      if((nsVarTable[rnum].default_value = copy_into_big_buf(value, 0)) == -1) {
        NS_EXIT(-1, CAV_ERR_1000018, value); 
      }
    }
    else if (!strcasecmp(key, "RetainPreValue")) 
    {
      NSDL2_VARS(NULL, NULL, "RetainPreValue  = [%s]", value);
      if (!strcasecmp(value, "Yes")) 
        nsVarTable[rnum].retain_pre_value = RETAIN_PRE_VALUE;
      else if (!strcasecmp(value, "No")) 
        nsVarTable[rnum].retain_pre_value = NOT_RETAIN_PRE_VALUE;
      else 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "declare array");
      }
    }
    else 
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, key, "declare array");
    }
  }
  if(!size_flag) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012131_ID, CAV_ERR_1012131_MSG);
  }
  return 0;
}

inline void nsl_set_sess_idx(int var_idx, int *sess_idx)
{
  int j;
  for(j = 0; j < total_sess_entries; j++){
    if(var_idx >= gSessionTable[j].nslvar_start_idx && var_idx < (gSessionTable[j].nslvar_start_idx + gSessionTable[j].num_nslvar_entries)) {
      *sess_idx = j;
      break;
    }
  }
  if(j == total_sess_entries)
    *sess_idx = -1;
}

/*This will copy nsl var table's attribute to shared memory */
void copy_nsl_var_into_shared_mem(){

  int shr_mem_size = 0;
  NSDL1_VARS(NULL, NULL, "Method called, total nsl var entries = %d, size of NslVarTableEntry_Shr = %d",
                 total_nsvar_entries, sizeof(NslVarTableEntry_Shr));
  if(!total_nsvar_entries)
    return;

  /*One extra entry will be for setting null entry*/ 
  shr_mem_size = sizeof(NslVarTableEntry_Shr) * (total_nsvar_entries + g_max_script_decl_param + 1);
  NSDL2_VARS(NULL, NULL, "Shared memory size = %d", shr_mem_size);
  
  nsl_var_table_shr_mem = do_shmget(shr_mem_size, "nsl_var_table_shr_mem");
  memset(nsl_var_table_shr_mem, 0, shr_mem_size);
  
  int i;
  int sess_idx;
  char var_name[512] = "";
  int var_idx;
  for(i = 0; i < total_nsvar_entries; i++)  {
    nsl_set_sess_idx(i, &(nsl_var_table_shr_mem[i].sess_idx));
    nsl_var_table_shr_mem[i].length = nsVarTable[i].length;
    if(nsVarTable[i].default_value != -1)
      nsl_var_table_shr_mem[i].default_value = BIG_BUF_MEMORY_CONVERSION(nsVarTable[i].default_value);
       
    //save uvtable idx.
    sess_idx = nsl_var_table_shr_mem[i].sess_idx;
    if(sess_idx != -1){
      sprintf(var_name, "%s", RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[i].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(var_name, strlen(var_name));
      nsl_var_table_shr_mem[i].uv_table_idx = gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx; 
    }
    else
      nsl_var_table_shr_mem[i].uv_table_idx = -1;

    NSDL3_VARS(NULL, NULL, "nsl_var_idx = %d, length = %d, default value = %s, sess_idx = %d, uv_table_idx = %d",
                             i, nsl_var_table_shr_mem[i].length, nsl_var_table_shr_mem[i].default_value,
                             nsl_var_table_shr_mem[i].sess_idx, nsl_var_table_shr_mem[i].uv_table_idx);
  }
  //set null entry.
  nsl_var_table_shr_mem[i].sess_idx = -1;  /**  these two values is to indicate terminating value **/
  nsl_var_table_shr_mem[i].length = -1;    /********************************************************/
  nsl_var_table_shr_mem[i].uv_table_idx = -1;
  nsl_var_table_shr_mem[i].default_value = NULL;

  NslVarTableEntry_Shr *grp_nsl_var_table_shr_mem;
  grp_nsl_var_table_shr_mem = &nsl_var_table_shr_mem[i];
  for(i = 0; i < g_max_script_decl_param; i++)
  {
    if (grpNsVarTable[i].default_value != -1)
    {
      grp_nsl_var_table_shr_mem[i].default_value = BIG_BUF_MEMORY_CONVERSION(grpNsVarTable[i].default_value);
      grp_nsl_var_table_shr_mem[i].uv_table_idx = grpNsVarTable[i].length; 
      grp_nsl_var_table_shr_mem[i].sess_idx = grpNsVarTable[i].type; 
    }
  }
  for (i = 0; i < total_runprof_entries; i++)
  {
    if (runProfTable[i].grp_ns_var_start_idx != -1)
    {
      runprof_table_shr_mem[i].nsl_var_table_shr_mem = &grp_nsl_var_table_shr_mem[runProfTable[i].grp_ns_var_start_idx];  
    } 
  } 
}

/*This method will insert default values of nsl array var to uvtable
 * Note: this method will not be called if nsl_var_table_shr_mem is null so first check it*/
void init_nsl_array_var_uvtable(VUser *vptr)
{
  int sess_idx = vptr->sess_ptr->sess_id;
  UserVarEntry *cur_uvtable = vptr->uvtable;  

  NSDL2_VARS(vptr, NULL, "Method called, session_idx = %d, sess_name = %s", sess_idx, vptr->sess_ptr->sess_name);  

  if(sess_idx == -1)
    return;
  int i = 0, uv_idx;
  while(nsl_var_table_shr_mem[i].length != -1){

    NSDL3_VARS(vptr, NULL, "nsl_var_table_idx = %d, sess_idx = %d, default_value = %s, length = %d, uv_idx = %d",
                         i, nsl_var_table_shr_mem[i].sess_idx, nsl_var_table_shr_mem[i].default_value, 
                            nsl_var_table_shr_mem[i].length, nsl_var_table_shr_mem[i].uv_table_idx);

    /*Check for those entry which have same sess_idx as of current one*/
    if(nsl_var_table_shr_mem[i].sess_idx != sess_idx){
      i++;
      continue;
    } 
    /*Only for NS_VAR_ARRAY length will be greater than 0*/
    if(nsl_var_table_shr_mem[i].length > 0)
    {
      uv_idx = nsl_var_table_shr_mem[i].uv_table_idx;
      if(uv_idx < 0){
        i++;
        continue;
      }
      cur_uvtable[uv_idx].length = nsl_var_table_shr_mem[i].length;
      MY_MALLOC(cur_uvtable[uv_idx].value.array, sizeof(ArrayValEntry)*cur_uvtable[uv_idx].length, "cur_uvtable[uv_idx].value.array", -1);
      memset(cur_uvtable[uv_idx].value.array, 0, sizeof(ArrayValEntry)*cur_uvtable[uv_idx].length);
#if 0
      //set default values
      if(nsl_var_table_shr_mem[i].default_value){
        NSDL3_VARS(vptr, NULL, "Copying default value = %s to uvtable at index = %d", nsl_var_table_shr_mem[i].default_value, uv_idx);
        int def_val_len = strlen(nsl_var_table_shr_mem[i].default_value);
        for(j = 0; j < nsl_var_table_shr_mem[i].length; j++){
          MY_MALLOC(cur_uvtable[uv_idx].value.array[j].value, def_val_len + 1, "cur_uvtable[uv_idx].value.array[j].value", -1);
          memcpy(cur_uvtable[uv_idx].value.array[j].value, nsl_var_table_shr_mem[i].default_value, def_val_len);
          cur_uvtable[uv_idx].value.array[j].value[def_val_len] = 0;
          cur_uvtable[uv_idx].value.array[j].length = def_val_len;
        }
      }  
#endif 
    }
    i++;  
  } 
  NSDL2_VARS(vptr, NULL, "Method completed");
}

/*vptr to get sess_idx*/
char *get_nsl_var_default_value(int sess_idx, int uv_idx, int *value_len, int grp_idx){
  int i = 0;
  int ns_var_table_idx = -1;
  NslVarTableEntry_Shr *loc_nsl_var_table_shr_mem;
  NSDL2_VARS(NULL, NULL, "Method called, sess_idx = %d, uv_index = %d", sess_idx, uv_idx);
  *value_len = 0;

  if(!nsl_var_table_shr_mem) 
    return NULL;
 
  if (runprof_table_shr_mem[grp_idx].nsl_var_table_shr_mem)
  {
    loc_nsl_var_table_shr_mem = runprof_table_shr_mem[grp_idx].nsl_var_table_shr_mem;
    int total_nsl_var_entries = runprof_table_shr_mem[grp_idx].total_nsl_var_entries; 
    for (i = 0; i <total_nsl_var_entries; i++)
    {
      if(loc_nsl_var_table_shr_mem[i].sess_idx == sess_idx && loc_nsl_var_table_shr_mem[i].uv_table_idx == uv_idx){
        if(loc_nsl_var_table_shr_mem[i].default_value){
          *value_len = strlen(loc_nsl_var_table_shr_mem[i].default_value);
          return loc_nsl_var_table_shr_mem[i].default_value;
        }
      }
    }
    return NULL;
  }
  /* till null entry */ 
  while(nsl_var_table_shr_mem[i].length != -1){
    if(nsl_var_table_shr_mem[i].sess_idx == sess_idx && nsl_var_table_shr_mem[i].uv_table_idx == uv_idx){
      ns_var_table_idx = i;
      break;       
    }
    i++;
  }
  /*If request values doesn't match with any entry of nsl_var_table_shr_mem then either it is not NSL_VAR or something is wrong */
  if(ns_var_table_idx == -1)
    return NULL;
  
  /* return default value and set length */
  if(nsl_var_table_shr_mem[ns_var_table_idx].default_value){
    *value_len = strlen(nsl_var_table_shr_mem[ns_var_table_idx].default_value);
    return nsl_var_table_shr_mem[ns_var_table_idx].default_value;
  }
  else
    return NULL;  
}


