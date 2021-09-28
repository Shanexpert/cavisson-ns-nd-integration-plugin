#include <stdlib.h>
#include <ctype.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "url.h"
#include "ns_nsl_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_log.h"

#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_vars.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"

#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h" //for ns_handle
#include "amf.h" //for amf_asc_ptr
#include "ns_debug_trace.h"
#include "poi.h" //for ns_get_random
#include "ns_string.h" //ns_encode_url etc
#include "ns_event_log.h"
#include "nslb_util.h" //for get_tokens()
#include "ns_random_vars.h"                                                                                    
#include "ns_unique_numbers.h"
#include "ns_unique_range_var.h"
#include "ns_exit.h"
#include "ns_script_parse.h"

#define DELTA_UNIQUE_RANGEVAR_ENTRIES 32
#ifndef CAV_MAIN
int max_unique_range_var_entries = 0;
int total_unique_rangevar_entries = 0;
int user_id = 0;
UniqueRangeVarPerProcessTable *unique_range_var_table;
UniqueRangeVarVuserTable *unique_range_var_vuserTable;
#else
__thread int max_unique_range_var_entries = 0;
__thread int total_unique_rangevar_entries = 0;
__thread int user_id = 0;
__thread UniqueRangeVarPerProcessTable *unique_range_var_table;
__thread UniqueRangeVarVuserTable *unique_range_var_vuserTable;

#endif
#define VALIDATE_RANGE(start,blocksize,sess_name)                                                                           \
{                                                                                                                           \
  NSDL2_VARS(NULL, NULL, "validate range start = %lu", start);                                                              \
  char max_range[] = "18446744073709551615";                                                                                \
  char *ptr;                                                                                                                \
  unsigned long num = strtoul(max_range, &ptr, 10);                                                                         \
  if(start >= num)                                                                                                          \
  {                                                                                                                         \
    fprintf(stderr,"Too large value of StartRange in script %s, value should be less than 18446744073709551615\n",sess_name);    \
    END_TEST_RUN;                                                                                                           \
  }                                                                                                                         \
  if(blocksize >= num)                                                                                                      \
  {                                                                                                                         \
    fprintf(stderr,"Too large value of UserBlockSize in script %s, value should be less than 18446744073709551615\n", sess_name); \
    END_TEST_RUN;                                                                                                           \
  }                                                                                                                         \
}

void free_unique_range_var()
{
  if(uniquerangevarTable)
    FREE_AND_MAKE_NULL_EX (uniquerangevarTable, (max_unique_range_var_entries * sizeof(UniqueRangeVarTableEntry)), "unique range var", -1);

}
/************************************************************************************************
  Purpose: This method is called from na_parent.c, this is called when a new NVM is forked.
           It fills the values in UniqueRangeVarPerProcessTable according to the given StartRange
           and blocksize.

  Input: It takes process_id as input.
  
*************************************************************************************************/
void create_unique_range_var_table_per_proc(int process_id){

  int i,j;
  long num_users = 0;
  int sess_idx;
  unsigned long start_range = 0;

  NSDL2_VARS(NULL, NULL, "Method called. total_unique_rangevar_entries = %d process_id = %d", total_unique_rangevar_entries, process_id);
  for(i = 0; i < total_unique_rangevar_entries; i++){
    start_range = 0;
    unique_range_var_table[i].name = BIG_BUF_MEMORY_CONVERSION(uniquerangevarTable[i].name);
    sess_idx = uniquerangevarTable[i].sess_idx;
    /*This is for finding the number of users of script on previous NVMs, num_users are required to divide the range on NVMs */
    for(j = 0; j < process_id; j++) {
      num_users = get_per_proc_num_script_users(j, session_table_shr_mem[sess_idx].sess_name);
      start_range += num_users * unique_range_var_table[i].block_size;
      NSDL2_VARS(NULL, NULL, "j = %d start_range = %d num_users = %d", j, start_range, num_users);
    }
    unique_range_var_table[i].start = uniquerangevarTable[i].start + start_range;
    unique_range_var_table[i].block_size = uniquerangevarTable[i].block_size;
    VALIDATE_RANGE(unique_range_var_table[i].start, unique_range_var_table[i].block_size, session_table_shr_mem[sess_idx].sess_name);
 
    if(uniquerangevarTable[i].format == -1) {
      unique_range_var_table[i].format = NULL;
    }
    else
      unique_range_var_table[i].format = BIG_BUF_MEMORY_CONVERSION(uniquerangevarTable[i].format);

    unique_range_var_table[i].format_len = uniquerangevarTable[i].format_len;
    unique_range_var_table[i].action = uniquerangevarTable[i].action;
    unique_range_var_table[i].refresh = uniquerangevarTable[i].refresh;
    unique_range_var_table[i].sess_idx = uniquerangevarTable[i].sess_idx;
    unique_range_var_table[i].userid = 0; 
    unique_range_var_table[i].uv_table_idx =
      session_table_shr_mem[uniquerangevarTable[i].sess_idx].vars_trans_table_shr_mem[session_table_shr_mem[uniquerangevarTable[i].sess_idx].var_hash_func(unique_range_var_table[i].name, strlen(unique_range_var_table[i].name))].user_var_table_idx;
    assert (unique_range_var_table[i].uv_table_idx != -1);

    NSDL2_VARS(NULL, NULL, "start = %lu blocksize = %lu format = %s action = %d refresh = %d num_users = %d start_range = %lu uvtable_idx =%d",               unique_range_var_table[i].start, unique_range_var_table[i].block_size, unique_range_var_table[i].format, 
               unique_range_var_table[i].action, unique_range_var_table[i].refresh, num_users, start_range, 
               unique_range_var_table[i].uv_table_idx);

  }
}

void create_uniquerange_var_vuser_table(VUser *vptr)
{
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. total_unique_rangevar_entries = %d vuser id = %d child_idx = %d ", 
                                         total_unique_rangevar_entries, vptr->user_index, child_idx);
  MY_MALLOC (unique_range_var_vuserTable, total_unique_rangevar_entries * sizeof(UniqueRangeVarVuserTable), "unique range var vuser table", -1);

  for(i = 0; i < total_unique_rangevar_entries; i++)
  {
    unique_range_var_vuserTable[i].start = unique_range_var_table[i].start + unique_range_var_table[i].block_size * unique_range_var_table[i].userid; 
    unique_range_var_vuserTable[i].end = unique_range_var_vuserTable[i].start + unique_range_var_table[i].block_size - 1;
    unique_range_var_vuserTable[i].current = unique_range_var_vuserTable[i].start;
    if(vptr->sess_ptr->sess_id == unique_range_var_table[i].sess_idx)
      unique_range_var_table[i].userid++;
    NSDL1_VARS(NULL, NULL, "start = %d  end = %d user id for NVM = %d", unique_range_var_vuserTable[i].start, 
               unique_range_var_vuserTable[i].end, unique_range_var_table[i].userid);
  }
  vptr->uniq_rangevar_ptr = unique_range_var_vuserTable;
  
}

/****************************************************************************************************
  Purpose: This table is used for allocating the memory for UniqueRangeVarTableEntry table
  Inpuut:  It takes row_num as arguments, it increases the row_num to total_unique_rangevar_entries.
  Output: It returns the SUCCESS.
****************************************************************************************************/
static int create_uniquerange_var_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_unique_rangevar_entries = %d, max_unique_range_var_entries = %d", total_unique_rangevar_entries, max_unique_range_var_entries);

  if (total_unique_rangevar_entries == max_unique_range_var_entries) {
    MY_REALLOC(uniquerangevarTable, (max_unique_range_var_entries + DELTA_UNIQUE_RANGE_VAR_ENTRIES) * sizeof(UniqueRangeVarTableEntry), "unique range var entries", -1);
    max_unique_range_var_entries +=  DELTA_UNIQUE_RANGE_VAR_ENTRIES;
  }
  *row_num = total_unique_rangevar_entries++;
  NSDL1_VARS(NULL, NULL, " total_unique_rangevar_entries = %d, row_num = %d", total_unique_rangevar_entries, *row_num);
  return (SUCCESS);
}
 
void init_unique_range_var_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_unique_rangevar_entries = 0;
  MY_MALLOC (uniquerangevarTable, INIT_UNIQUE_RANGE_VAR_ENTRIES * sizeof(UniqueRangeVarTableEntry), "unique range var", -1);

  if(uniquerangevarTable)
  {
    max_unique_range_var_entries = INIT_UNIQUE_RANGE_VAR_ENTRIES;
  }
  else
  {
    max_unique_range_var_entries = 0;
    NS_EXIT(-1, CAV_ERR_1031013, "UniqueRangeVarTableEntry");
  }
}


#define NS_ST_SE_NAME      1
#define NS_ST_SE_OPTIONS   2

/*********************************************************************************************************
  Purpose: This method is used for parsing of nsl_unique_range_var API. It fills UniqueRangeVarTableEntry

  Input: It take line, line_number, sess_idx and script_filename as arguments.
         line: It points to the line in registration.spec where nsl_unique_range_var API is present.
         line_number: It is the line number of the registration.spec file where nsl_unique_range_var 
                      API is present.
         sess_idx: It is the session index.
         
*********************************************************************************************************/
int input_unique_rangevar_data(char* line, int line_number, int sess_idx, char *script_filename)
{
  int state = NS_ST_SE_NAME;
  int rnum = 0;
  int j, i, ret;
  char key[MAX_ARG_NAME_SIZE + 1];
  char value[MAX_ARG_VALUE_SIZE + 1];
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];                   /* Store Registration file name "registration.spec" */
  char unique_rangevar_buf[UNQRANGVAR_MAX_LEN_512B] = "";       /* Store variable name of API nsl_uniquerange_var() */
  char *sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char *ptr = NULL;

  NSApi api_ptr;  //for parsing from parse_api

  NSDL1_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d", line, line_number, sess_idx);
  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_unique_range_var() declaration on line %d of scripts/%s/%s: ", line_number,
  get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
  //Previously taking with only script name
  //get_sess_name_with_proj_subproj(sess_name), script_filename);

  msg[MAX_LINE_LENGTH-1] = '\0';
  /*bug id: 101320: ToDo: TBD with DJA*/
  sprintf(file_name, "%s/%s/%s", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
  //Previously taking with only script name
  //sprintf(file_name, "scripts/%s/%s", get_sess_name_with_proj_subproj(sess_name), script_filename);
  file_name[strlen(file_name)] = '\0';

  NSDL1_VARS(NULL, NULL, "api_ptr = %p, file_name = %s", &api_ptr, file_name);
  //Since we need to remove spaces from value so pass set flag trim_spaces = 1

  if(!strstr(line, "StartRange"))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Start Range", "Unique Range");
  }

  if(!strstr(line, "UserBlockSize"))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "User Block Size", "Unique Range");
  }

  //parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  if(testCase.mode == TC_FIX_USER_RATE) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012038_ID, CAV_ERR_1012038_MSG);
  }
  for(j = 0; j < api_ptr.num_tokens; j++) {
    strcpy(key, api_ptr.api_fields[j].keyword);
    strcpy(value, api_ptr.api_fields[j].value);

    NSDL2_VARS(NULL, NULL, "j = %d, api_ptr.num_tokens = %d, key = [%s], value = [%s], state = %d",
                            j, api_ptr.num_tokens, key, value, state);
    switch (state) {
    case NS_ST_SE_NAME:
      //unique_range_var should not have any value
      if(strcmp(value, ""))
        break;

      strcpy(unique_rangevar_buf, key);
      NSDL2_VARS(NULL, NULL, "parameter name = [%s]", unique_rangevar_buf);
    
      /* For validating the variable we are calling the validate_var funcction */
     if(validate_var(unique_rangevar_buf)) {
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, unique_rangevar_buf);
     }

     create_uniquerange_var_table_entry(&rnum);

     uniquerangevarTable[rnum].sess_idx = sess_idx;
     NSDL1_VARS(NULL, NULL, "gSessionTable[sess_idx].unique_range_var_start_idx = %d", gSessionTable[sess_idx].unique_range_var_start_idx);
     if (gSessionTable[sess_idx].unique_range_var_start_idx == -1) {
        gSessionTable[sess_idx].unique_range_var_start_idx = rnum;
        gSessionTable[sess_idx].num_unique_range_var_entries = 0;
        NSDL1_VARS(NULL, NULL, "gSessionTable[sess_idx].unique_range_var_start_idx %d rnum = %d", gSessionTable[sess_idx].unique_range_var_start_idx, rnum);
     }
     
     gSessionTable[sess_idx].num_unique_range_var_entries++;
     NSDL1_VARS(NULL, NULL, "gSessionTable[sess_idx].num_unique_range_var_entries = %d", gSessionTable[sess_idx].num_unique_range_var_entries);
    

      //Fill the default values to the unique range var table 
      uniquerangevarTable[rnum].start = 0;
      uniquerangevarTable[rnum].block_size = 32;
      uniquerangevarTable[rnum].refresh = SESSION;
      uniquerangevarTable[rnum].format = -1;
      uniquerangevarTable[rnum].format_len = -1;
      uniquerangevarTable[rnum].action = SESSION_FAILURE;
      uniquerangevarTable[rnum].name = -1;
   
      if ((uniquerangevarTable[rnum].name = copy_into_big_buf(unique_rangevar_buf, 0)) == -1) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, unique_rangevar_buf);
      }
      state = NS_ST_SE_OPTIONS;
      break;   

     case NS_ST_SE_OPTIONS:
       if (!strcasecmp(key, "StartRange")) {
         if(!strcmp(value, "")) {
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Unique Range");
         }
 
         for(i = 0; i < strlen(value); i++)
         {
           if (!isdigit(value[i])) {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, key);
            }
         }
         NSDL2_VARS(NULL, NULL, "StartRange = [%s]", value);
         uniquerangevarTable[rnum].start = strtoul(value, &ptr, 10);
       } else if (!strcasecmp(key, "UserBlockSize")) {
           if(!strcmp(value, "")) {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Unique Range");
           }
 
           for(i = 0; i < strlen(value); i++)
           {
             if (!isdigit(value[i])) {
               SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, key);
             }
           }
           NSDL2_VARS(NULL, NULL, "UserBlockSize = [%s]", value);
           uniquerangevarTable[rnum].block_size = strtoul(value, &ptr, 10);
       } else if (!strcasecmp(key, "Refresh")) {
           NSDL2_VARS(NULL, NULL, "Refresh = [%s]", value);
           if(!strcmp(value, "SESSION"))
             uniquerangevarTable[rnum].refresh = SESSION;
           else if(!strcmp(value, "USE"))
             uniquerangevarTable[rnum].refresh = USE;
           else
           {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "Update Value On", "Unique Range");
           }
       } else if (!strcasecmp(key, "Format")) {
           NSDL2_VARS(NULL, NULL, "Format = [%s]", value);
           if ((uniquerangevarTable[rnum].format = copy_into_big_buf(value, 0)) == -1) {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
           }
           uniquerangevarTable[rnum].format_len = strlen(value);
       } else if (!strcasecmp(key, "ActionOnRangeExceed")) {
           NSDL2_VARS(NULL, NULL, "ActionOnRangeExceed = [%s]", value);
           if(!strcmp(value, "SESSION_FAILURE"))
             uniquerangevarTable[rnum].action = SESSION_FAILURE;
           else if(!strcmp(value, "STOP_TEST"))
             uniquerangevarTable[rnum].action = UNIQUE_RANGE_STOP_TEST;
           else if(!strcmp(value, "CONTINUE_WITH_LAST_VALUE"))
             uniquerangevarTable[rnum].action = CONTINUE_WITH_LAST_VALUE;
           else if(!strcmp(value, "REPEAT_THE_RANGE_FROM_START"))
             uniquerangevarTable[rnum].action = REPEAT_THE_RANGE_FROM_START;
           else {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "Action On Range Exceed", "Unique Range");
           }
       } else {
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, key, "Unique Range");
       }
         
     }
     if(state == NS_ST_SE_NAME)
     {
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Unique Range");
     }
   }
  return(0);
}
    

/*******************************************************************************************************
  Purpose: This method is used for filling the data in UniqueRangeVarVuserTable, this table is filled
           for each user.

  Parameters: 
  
  Return: This returns unique number for each VUser.
********************************************************************************************************/
char *get_unique_range_number(int *total_len, VUser *vptr, int unique_range_var_idx){

  UniqueRangeVarVuserTable *ptr;  
  long value;
  char *outstr;
  NSDL1_VARS(NULL, NULL, "Method called unique_range_var_idx = %d format = %s action = %d", unique_range_var_idx, unique_range_var_table[unique_range_var_idx].format, unique_range_var_table[unique_range_var_idx].action);

  if(!vptr->uniq_rangevar_ptr){
    create_uniquerange_var_vuser_table(vptr);
  }

  ptr = vptr->uniq_rangevar_ptr + unique_range_var_idx; 
  NSDL1_VARS(NULL, NULL, "current = %d end = %d unique_range_var_table->action = %d", ptr->current, ptr->end, unique_range_var_table->action);

  if(ptr->current == (ptr->end + 1)){
    if(unique_range_var_table[unique_range_var_idx].action == SESSION_FAILURE)
    {
      vptr->sess_status = NS_UNIQUE_RANGE_ABORT_SESSION;
      vptr->page_status = NS_UNIQUE_RANGE_ABORT_SESSION; 
    }
    else if(unique_range_var_table[unique_range_var_idx].action == UNIQUE_RANGE_STOP_TEST) {
      fprintf(stderr, "Warning: Range of nsl_unique_range_var exceed and ActionOnRangeExceed is STOP_TEST, so test is going to be stopped\n");
      end_test_run();
    }
    else if(unique_range_var_table[unique_range_var_idx].action == CONTINUE_WITH_LAST_VALUE) {
      value = ptr->current - 1;
    }
    else if(unique_range_var_table[unique_range_var_idx].action == REPEAT_THE_RANGE_FROM_START)
    {
      ptr->current = ptr->start;
      value = ptr->current++;
    }
  }
  else
    value = ptr->current++;
 
  *total_len = 32;
  MY_MALLOC(outstr, *total_len, "Malloc buffer for unique range number", -1);
  *total_len = sprintf(outstr, unique_range_var_table[unique_range_var_idx].format, value);
  NSDL2_VARS(NULL, NULL, "Unique range Number = %d, outstr = %s", value, outstr);

  return outstr;
}


char* get_unique_range_var_value(VUser* vptr, int var_val_flag, int* total_len, int unique_var_idx)
{
  char * unique_range_val;
  NSDL1_VARS(NULL, NULL, "Method called var_ptr %p vptr %p var_val_flag %d unique_var_idx = %d",unique_range_var_table, vptr, var_val_flag, unique_var_idx);

  if (var_val_flag) { //fill new value
    if (unique_range_var_table[unique_var_idx].refresh == SESSION)  { //SESSION
       NSDL1_VARS(NULL, NULL, "Entering method %s, with value  = %s",
                 __FUNCTION__, vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].value.value);
       NSDL1_VARS(NULL, NULL, "Entering method %s, with filled_flag = %d",
                 __FUNCTION__, vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].filled_flag);

      if (vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].filled_flag == 0) { //first call. assign value
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Calling get_unique_range_number first time");
        unique_range_val = get_unique_range_number(total_len, vptr, unique_var_idx);
        NSDL1_VARS(NULL, NULL, "Got Unique var  = %s, Length = %d", unique_range_val,
            *total_len);
        vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].value.value  = unique_range_val;
        vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].length = *total_len;
        vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].filled_flag = 1;
      }else{ // need to return length as we're using this on return
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Value already set");
        *total_len = vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].length;
      }
    }else{ //USE
      NSDL1_VARS(NULL, NULL, "Refresh is USE, Calling get_unique_range_number");
      unique_range_val = get_unique_range_number(total_len, vptr, unique_var_idx);
      //Checking here for filled flag, if its set to 1 it means we have already
      /* malloced the buffer.Its possible if we are using same variable multiple
      * times with USE in same session. So, we need to first free old memory
      * then assign new memory*/
      if (vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].filled_flag == 1)
      {   

        FREE_AND_MAKE_NOT_NULL(vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].value.value,
            "vptr->uvtable[var_ptr->uv_table_idx].value.value", unique_range_var_table[unique_var_idx].uv_table_idx);
      }
      vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].value.value  = unique_range_val;
      vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].length = *total_len;
      vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].filled_flag = 1;
    }
   }else{
     *total_len = vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].length;
   }
  
   NSDL1_VARS(NULL, NULL, "Returning from method %s, with value  = %s", __FUNCTION__, vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].value.value);
  return (vptr->uvtable[unique_range_var_table[unique_var_idx].uv_table_idx].value.value);
}


int find_unique_range_var_idx(char* name, int sess_idx) {
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. name = %s, sess_idx = %d unique_range_var_start_idx = %d um_unique_range_var_entries = %d", name, sess_idx, gSessionTable[sess_idx].unique_range_var_start_idx, gSessionTable[sess_idx].num_unique_range_var_entries);
  if (gSessionTable[sess_idx].unique_range_var_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].unique_range_var_start_idx; i < gSessionTable[sess_idx].unique_range_var_start_idx 
      + gSessionTable[sess_idx].num_unique_range_var_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(uniquerangevarTable[i].name), name))
      return i;
  }

  return -1;
}
