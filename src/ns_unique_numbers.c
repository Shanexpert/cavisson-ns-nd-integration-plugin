/************************************************************************************
* Name      : ns_unique_numbers.c 
* Purpose   : This file contains functions related to unique number 
* Author(s) : Abhay Singh
* Document: Refer to Req/HLD doc in CVSDocs in cavisson/docs/Products/NetStorm/TechDocs/Parameterization/Req/ReqDocUniqueNumberParameter.doc
* Modification History : 5th Feb, 2019
***********************************************************************************/

#include <stdlib.h>
#include <ctype.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>
#include <regex.h>

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
#include "ns_log.h"
#include "logging.h"
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
#define DELTA_UNIQUEVAR_ENTRIES 32
#include "ns_exit.h"
#include "wait_forever.h"
#include "ns_error_msg.h"
#include "ns_script_parse.h"

#ifndef CAV_MAIN
int max_uniquevar_entries = 0;
static int total_uniquevar_entries = 0;
#else
__thread int max_uniquevar_entries = 0;
__thread int total_uniquevar_entries = 0;
#endif
//var_unique_number *uniqueTableEntry 

static int create_uniquevar_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_uniquevar_entries = %d, max_uniquevar_entries = %d", total_uniquevar_entries, max_uniquevar_entries);

  if (total_uniquevar_entries == max_uniquevar_entries) {
    MY_REALLOC (uniqueVarTable, (max_uniquevar_entries + DELTA_UNIQUEVAR_ENTRIES) * sizeof(UniqueVarTableEntry), "uniquevar entries", -1);
    max_uniquevar_entries += DELTA_UNIQUEVAR_ENTRIES;
  }
  *row_num = total_uniquevar_entries++;
  return (SUCCESS);
}
 
void init_uniquevar_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_uniquevar_entries = 0;
   MY_MALLOC (uniqueVarTable, INIT_UNIQUEVAR_ENTRIES * sizeof(UniqueVarTableEntry), "uniqueVarTable", -1);

  if(uniqueVarTable)
  {
    max_uniquevar_entries = INIT_UNIQUEVAR_ENTRIES;
     }
  else
  {
    max_uniquevar_entries = 0;
    NS_EXIT(-1, CAV_ERR_1031013, "UniqueVarTableEntry");
  }
}


int input_uniquevar_data(char* line, int line_number, int sess_idx, char *script_filename)
{
#define NS_ST_SE_NAME 1
#define NS_ST_SE_OPTIONS 2
#define NS_ST_STAT_VAR_NAME 1
#define MIN_LIMIT 8
#define MAX_LIMIT 32
//#define INIT_PERPAGESERVAR_ENTRIES 64
//#define DELTA_PERPAGESERVAR_ENTRIES 32

  int state = NS_ST_SE_NAME;
  char uniquevar_buf[MAX_LINE_LENGTH];
  char* line_ptr = line;
  int done = 0;
  int rnum;

  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char msg[MAX_LINE_LENGTH];
  int i;
  char script_file_msg[MAX_FIELD_LENGTH];
  int format_begins = 0;
  int format_complete = 0;
  char buf[16] = "";
  int num_digit = 1;
  int len = 0;
  char *format_st = NULL;

  NSDL1_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d", line, line_number, sess_idx);
  /*bug id: 101320: trace updated to show NS_TA_DIR*/ 
   //fprintf(stderr, "input_randomvar_data: line = %s, line_number = %d, sess_idx = %d\n", line, line_number, sess_idx);
  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_unique_number_var() declaration on line %d of %s/%s/%s: ", line_number, GET_NS_RTA_DIR(),
      get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
      //Previously taking with only script name
      //get_sess_name_with_proj_subproj(sess_name), script_filename);
  msg[MAX_LINE_LENGTH-1] = '\0';

  while (!done) {
    NSDL2_VARS(NULL, NULL, "state = %d", state);
    //fprintf(stderr, "input_randomvar_data: line = %s\n", line_ptr);
    switch (state) {

      case NS_ST_SE_NAME:
        CLEAR_WHITE_SPACE(line_ptr);
        for (i = 0; (*line_ptr != ',') && ((*line_ptr) != ' ') && ((*line_ptr) != '\t') && (*line_ptr != '\0'); line_ptr++, i++) {
          uniquevar_buf[i] = *line_ptr;
        }
        uniquevar_buf[i] = '\0';
        CLEAR_WHITE_SPACE(line_ptr);

        CHECK_CHAR(line_ptr, ',', msg);
        create_uniquevar_table_entry(&rnum);

        /* First fill the default values */
        uniqueVarTable[rnum].name = -1;
        uniqueVarTable[rnum].num_digits = 8;
        uniqueVarTable[rnum].format =-1;
        uniqueVarTable[rnum].refresh = SESSION;
        uniqueVarTable[rnum].sess_idx = sess_idx;

        if (gSessionTable[sess_idx].uniquevar_start_idx == -1) {
          gSessionTable[sess_idx].uniquevar_start_idx = rnum;
          gSessionTable[sess_idx].num_uniquevar_entries = 0;
        }
   
         /* For validating the variable we are calling the validate_var funcction */
         if(validate_var(uniquevar_buf)) {
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, uniquevar_buf);
         }

        gSessionTable[sess_idx].num_uniquevar_entries++;

        if ((uniqueVarTable[rnum].name = copy_into_big_buf(uniquevar_buf, 0)) == -1) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, uniquevar_buf);
        }

        state = NS_ST_SE_OPTIONS;
        break;

      case NS_ST_SE_OPTIONS:
        CLEAR_WHITE_SPACE(line_ptr);
        if(uniqueVarTable[rnum].name == -1)
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Unique Number");
        }
        if (!strncasecmp(line_ptr, "format=", strlen("format="))) {
          line_ptr += strlen("format=");
          CLEAR_WHITE_SPACE(line_ptr);
          //for (i = 0; (*line_ptr != ',') && ((*line_ptr) != ' ') && ((*line_ptr) != '\t') && (*line_ptr != '\0'); line_ptr++, i++) {
          for (i = 0; (*line_ptr != ',') && (*line_ptr != '\0'); line_ptr++, i++) { // Fixed bug 1126
            PARSE_FORMAT(uniquevar_buf);
          }

          if(!format_complete)
          {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012039_ID, CAV_ERR_1012039_MSG, "Unique Number");
          }

          if(i == 0) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Format", "Unique Number");
          }

          uniquevar_buf[i] = '\0';
          CLEAR_WHITE_SPACE(line_ptr);
          CHECK_CHAR(line_ptr, ',', msg);

          if ((uniqueVarTable[rnum].format = copy_into_big_buf(uniquevar_buf, 0)) == -1) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, uniquevar_buf);
          }
          format_st = strchr(uniquevar_buf, '%');

          if(format_st[1] != '0')
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012045_ID, CAV_ERR_1012045_MSG);

          while(isdigit(format_st[num_digit]))
          {
            buf[num_digit-1] = format_st[num_digit];
            num_digit++;
          }
          len = strlen(buf);
          if(len > 3)
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012047_ID, CAV_ERR_1012047_MSG);
       
          num_digit = atoi(buf);
          if(num_digit < MIN_LIMIT)
          {
            NSTL1(NULL, NULL, "WARNING :- Setting length of digit to 8(default) as specifier value is less than 8\n");
            uniqueVarTable[rnum].num_digits = 8;
          }
          else 
          {
            if(num_digit > MAX_LIMIT)
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012048_ID, CAV_ERR_1012048_MSG);
            uniqueVarTable[rnum].num_digits = num_digit;
          }
          CLEAR_WHITE_SPACE(line_ptr);
        }      
          else if (!strncasecmp(line_ptr, "REFRESH", strlen("REFRESH"))) {
          if (state == NS_ST_STAT_VAR_NAME) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Unique Number");
          }

          line_ptr += strlen("REFRESH");
          CLEAR_WHITE_SPACE(line_ptr);
          CHECK_CHAR(line_ptr, '=', script_file_msg);
          CLEAR_WHITE_SPACE(line_ptr);
          NSDL3_VARS(NULL, NULL, "Refresh Name = %s", line_ptr);
          for (i = 0; *line_ptr != '\0' && (*line_ptr != ' '); line_ptr++, i++) {
            uniquevar_buf[i] = *line_ptr;
            NSDL3_VARS(NULL, NULL, "i = %d, Refresh line for token = %s, *line_ptr = %c, uniquevar_buf[%d] = %c",
                i, line_ptr, *line_ptr, i, uniquevar_buf[i]);
          }
          uniquevar_buf[i] = '\0';

          if (!strcmp(uniquevar_buf, "SESSION")) {
            uniqueVarTable[rnum].refresh = SESSION;
          } else if (!strcmp(uniquevar_buf, "USE")) {
            uniqueVarTable[rnum].refresh = USE;
          } else {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, uniquevar_buf, "Update Value On", "Unique Number");
          }
          CLEAR_WHITE_SPACE(line_ptr);
        } else if (*line_ptr == '\0') {
          done = 1;
        } else {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, line_ptr, "Unique Number");
        }
    }
  } //while
  
     if (uniqueVarTable[rnum].format == -1)
     {
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Format", "Unique Number");
     }
   
 #undef NS_ST_SE_NAME
#undef NS_ST_SE_OPTIONS
  return(0);
}
    
      
UniqueVarTableEntry_Shr *copy_uniquevar_into_shared_mem(void)
{
  int i;
  UniqueVarTableEntry_Shr *uniquevar_table_shr_mem_local = NULL;

  NSDL1_VARS(NULL, NULL, "Method called. total_uniquevar_entries = %d", total_uniquevar_entries);
  /* insert the SearchVarTableEntry_Shr and the PerPageSerVarTableEntry_shr */

  if (total_uniquevar_entries ) {
    uniquevar_table_shr_mem_local = do_shmget(total_uniquevar_entries * sizeof(UniqueVarTableEntry_Shr), "unique var tables");

    for (i = 0; i < total_uniquevar_entries; i++) {
      uniquevar_table_shr_mem_local[i].var_name = BIG_BUF_MEMORY_CONVERSION(uniqueVarTable[i].name);
      uniquevar_table_shr_mem_local[i].num_digits = uniqueVarTable[i].num_digits;
      uniquevar_table_shr_mem_local[i].refresh = uniqueVarTable[i].refresh;
      uniquevar_table_shr_mem_local[i].sess_idx = uniqueVarTable[i].sess_idx;

      if (uniqueVarTable[i].format == -1)
        uniquevar_table_shr_mem_local[i].format = NULL;
      else
        uniquevar_table_shr_mem_local[i].format = BIG_BUF_MEMORY_CONVERSION(uniqueVarTable[i].format);

      uniquevar_table_shr_mem_local[i].uv_table_idx =
        gSessionTable[uniqueVarTable[i].sess_idx].vars_trans_table_shr_mem[gSessionTable[uniqueVarTable[i].sess_idx].var_hash_func(uniquevar_table_shr_mem_local[i].var_name, strlen(uniquevar_table_shr_mem_local[i].var_name))].user_var_table_idx;
      assert (uniquevar_table_shr_mem_local[i].uv_table_idx != -1);
    }
  }
  return (uniquevar_table_shr_mem_local);
}

long long uni_mask[18] = {
  100000000, //0
  100000000, //1
  100000000, //2
  100000000, //3
  100000000, //4
  100000000, //5
  100000000, //6
  100000000, //7
  100000000,//8
  1000000000, //9
  10000000000, //10
  100000000000, //11 
  1000000000000,//12
  10000000000000,//13
  100000000000000,//14
  1000000000000000,//15
  10000000000000000,//16
  100000000000000000,//17
  };


char* get_unique_number(char *format, int len, int *total_len)
{
  long long out = 0;
  char *outstr;
  static int count = 1;
  int nvm_id ;
  int gen_id ;
  long long time_since_epoch;
  int time_since_1_jan_2012;

  NSDL1_API(NULL, NULL, "Method called, g_parent_idx = %d", g_parent_idx);
 
  /* Initially unique number is generated by left shifting the nvm_id by 24 because of this when test run multiple times, 
     value of unique number parameter was same for all test runs(Bug 27895), to fix the same time_since_1_jan_2012 is added with nvm_id*/

  time_since_epoch = time(NULL);
  time_since_1_jan_2012 = (int) (time_since_epoch - TIMESTAMP_2012);
  nvm_id = my_child_index + 1;
  gen_id = g_parent_idx + 1;
  if(g_parent_idx < 0)
  { 
    if(len < 18)
      out = (((((long long) time_since_1_jan_2012) << 32) + (count << 24)  + nvm_id)) % uni_mask[len] ;  // 4 bytes times since Jan 1 2012, 			                                                                                         // 1 Byte - NVM Id, 3 Bytes - Counter
    else
      out = ((((long long) time_since_1_jan_2012) << 32) + (count << 24)  + nvm_id);  // 4 bytes times since Jan 1 2012, 			                                                                                         // 1 Byte - NVM Id, 3 Bytes - Counter
    count++;
    if (count >= 0x1000000) /* 0x1000000 - max hexa value for 3 byte number, re-assinging 0 to count when we reach max of the 3 byte*/
    count = 0;
  }
  else
  {
    //In case of NC
    if(len < 18)
      out = ((((long long) time_since_1_jan_2012) << 32) + (count << 24)  + (gen_id << 16) + nvm_id) % uni_mask[len]; //1 Byte - Gen Id,
														     //1 Byte - nvm_id 													       			     //2 Bytes - counter
    else  
      out = (((long long) time_since_1_jan_2012) << 32) + (count << 24)  +  (gen_id << 16) + nvm_id; //4 bytes times since Jan 1 2012
													       //1 Byte- Gen Id, 1 Byte -nvm_id,
 													       //2 bytes - Counter
  count++;
  if (count >= 0x10000)
    count = 0;
  }
  
  if(len == 8)
    format="%08lu";

  *total_len = len + 1; 
  MY_MALLOC(outstr, *total_len, "Malloc buffer for unique number",-1);
  memset(outstr, 0, *total_len);
  *total_len = sprintf(outstr, format, out);
  //*total_len = *total_len + 1; //Doing +1 because we are doing strcpy in caller  
  NSDL2_VARS(NULL, NULL, "Unique Number = %lld, outstr = %s", out, outstr);
  return outstr;
}

 char* get_unique_var_value(UniqueVarTableEntry_Shr* var_ptr, VUser* vptr, int
    var_val_flag, int* total_len)
{
  char * unique_val;
  NSDL1_VARS(NULL, NULL, "Method called var_ptr %p vptr %p var_val_flag %d",var_ptr, vptr,
      var_val_flag);

  if (var_val_flag) { //fill new value
    if (var_ptr->refresh == SESSION)  { //SESSION
       NSDL1_VARS(NULL, NULL, "Entering method %s, with value  = %s",
                 __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value);
       NSDL1_VARS(NULL, NULL, "Entering method %s, with filled_flag = %d",
                 __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].filled_flag);

      if (vptr->uvtable[var_ptr->uv_table_idx].filled_flag == 0) { //first call. assign value
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Calling get_unique_number first time");
        unique_val = get_unique_number( var_ptr->format, var_ptr->num_digits, total_len);
        NSDL1_VARS(NULL, NULL, "Got Unique var  = %s, Length = %d", unique_val,
            *total_len);
        vptr->uvtable[var_ptr->uv_table_idx].value.value  = unique_val;
        vptr->uvtable[var_ptr->uv_table_idx].length = *total_len;
        vptr->uvtable[var_ptr->uv_table_idx].filled_flag = 1;
      }else{ // need to return length as we're using this on return
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Value already set");
        *total_len = vptr->uvtable[var_ptr->uv_table_idx].length;
      }
    }else{ //USE
      NSDL1_VARS(NULL, NULL, "Refresh is USE, Calling get_unique_number");
      unique_val = get_unique_number( var_ptr->format,var_ptr->num_digits,total_len);
      /*Checking here for filled flag, if its set to 1 it means we have already
      * malloced the buffer.Its possible if we are using same variable multiple
      * times with USE in same session. So, we need to first free old memory
      * then assign new memory*/
      if (vptr->uvtable[var_ptr->uv_table_idx].filled_flag == 1)
      {   

        FREE_AND_MAKE_NOT_NULL(vptr->uvtable[var_ptr->uv_table_idx].value.value,
            "vptr->uvtable[var_ptr->uv_table_idx].value.value", var_ptr->uv_table_idx);
      }
      vptr->uvtable[var_ptr->uv_table_idx].value.value  = unique_val;
      vptr->uvtable[var_ptr->uv_table_idx].length = *total_len;
      vptr->uvtable[var_ptr->uv_table_idx].filled_flag = 1;
    }
   }else{
     *total_len = vptr->uvtable[var_ptr->uv_table_idx].length;
   }

   NSDL1_VARS(NULL, NULL, "Returning from method %s, with value  = %s", __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value);
  return (vptr->uvtable[var_ptr->uv_table_idx].value.value);
}

int find_uniquevar_idx(char* name, int sess_idx) {
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].uniquevar_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].uniquevar_start_idx; i < gSessionTable[sess_idx].uniquevar_start_idx + gSessionTable[sess_idx].num_uniquevar_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(uniqueVarTable[i].name), name))
      return i;
  }

  return -1;
}

/* Function clear(s) value of unique variable only in case user wants to retain value of user varible(s) for next sesssions . */
void clear_uvtable_for_unique_var(VUser *vptr)
{
  int sess_idx = vptr->sess_ptr->sess_id;
  int i, uv_idx;
  UserVarEntry *cur_uvtable = vptr->uvtable; 

  NSDL2_VARS(vptr, NULL, "Method called, session_idx = %d, sess_name = %s", sess_idx, vptr->sess_ptr->sess_name);  

  if (sess_idx == -1)
    return;

  for( i = 0; i <= total_uniquevar_entries; i++){

    NSDL3_VARS(vptr, NULL, "uniquevar_table_shr_mem idx = %d, sess_idx = %d, uv_idx = %d",
                      i, uniquevar_table_shr_mem[i].sess_idx, uniquevar_table_shr_mem[i].uv_table_idx);

    /*Check for those entry which have same sess_idx as of current one*/
    if (uniquevar_table_shr_mem[i].sess_idx != sess_idx){
      continue;
    }
 
    /* Calculate uv_idx */
    if (uniquevar_table_shr_mem[i].var_name)
    {
      uv_idx = uniquevar_table_shr_mem[i].uv_table_idx;
      if (uv_idx < 0){
        continue;
      }
      if (cur_uvtable[uv_idx].value.value) 
        FREE_AND_MAKE_NULL(cur_uvtable[uv_idx].value.value, "cur_uvtable[uv_idx].value.value", uv_idx);
      cur_uvtable[uv_idx].length = 0;
      cur_uvtable[uv_idx].filled_flag = 0;
    }
  }
}

