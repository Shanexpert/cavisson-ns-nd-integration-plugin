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
#include "nslb_comman_api.h"

#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_exit.h"
#include "ns_script_parse.h"

#ifndef CAV_MAIN
RandomStringTableEntry* randomStringTable;
int max_randomstring_entries = 0;
static int total_randomstring_entries;
#else
//__thread RandomStringTableEntry* randomStringTable;
__thread int max_randomstring_entries = 0;
__thread int total_randomstring_entries;
#endif


#define NS_ST_STAT_VAR_NAME 1
#define NS_ST_STAT_VAR_OPTIONS 2

static int create_randomstring_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_randomstring_entries = %d, max_randomstring_entries = %d", total_randomstring_entries, max_randomstring_entries);

  if (total_randomstring_entries == max_randomstring_entries) {
    MY_REALLOC (randomStringTable, (max_randomstring_entries + DELTA_RANDOMSTRING_ENTRIES) * sizeof(RandomStringTableEntry), "randomstring entries", -1);
    max_randomstring_entries += DELTA_RANDOMSTRING_ENTRIES;
  }
  *row_num = total_randomstring_entries++;
  return (SUCCESS);
}

void init_randomstring_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_randomstring_entries = 0;

  MY_MALLOC (randomStringTable, INIT_RANDOMSTRING_ENTRIES * sizeof(RandomStringTableEntry), "randomStringTable", -1);

  if(randomStringTable)
  {
    max_randomstring_entries = INIT_RANDOMSTRING_ENTRIES;
  }
  else
  {
    NS_EXIT(-1, CAV_ERR_1031013, "RandomStringTableEntry");
    max_randomstring_entries = 0;
  }
}

/*
 * routine to read and parse the line -- nsl_random_number_var() in script.capture
 * format -- nsl_random_number_var(var_name, min, max, format)
 * var_name - variable name to store the random number + format
 * min - min value of random no
 * max - max value of random no
 * format - the random no is printed in the
 * format specified into the variable and stored as such.
 */
static int set_refresh(char *value, int rnum, int *state, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Random String");
  }

  if (rnum == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012177_ID, CAV_ERR_1012177_MSG);
  }

  if (!strncasecmp(value, "SESSION", strlen("SESSION"))) {
    randomStringTable[rnum].refresh = SESSION;
  } else if (!strncasecmp(value, "USE", strlen("USE"))) {
    randomStringTable[rnum].refresh = USE;
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "Update Value On", "Random String");
  }

  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}

static int set_name_option(char *value, char *line_tok, char *randomstring_buf, 
                int *rnum, int sess_idx, char *script_file_msg, int *state){

  NSDL2_VARS(NULL, NULL, "Method Called, line_tok = [%s], value = [%s], group_idx = %d"
                         "sess_idx = %d",
                         line_tok, value, rnum, sess_idx);

  if (*state == NS_ST_STAT_VAR_OPTIONS) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, line_tok, "Random String");
  }

  /*For variable, keyword itself is variable name value*/
  strcpy(randomstring_buf, line_tok);
  NSDL2_VARS(NULL, NULL, "After tokenized randomstring_buf = %s", randomstring_buf);

  /* For validating the variable we are calling the validate_var funcction */
  if(validate_var(randomstring_buf)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, randomstring_buf); 
  }

  create_randomstring_table_entry(rnum);

   /* First fill the default values */
   randomStringTable[*rnum].name = -1;
   randomStringTable[*rnum].min = 0;
   randomStringTable[*rnum].max = -1;
   randomStringTable[*rnum].char_set = -1;
   randomStringTable[*rnum].refresh = SESSION;
   randomStringTable[*rnum].sess_idx = sess_idx;
    
    if(gSessionTable[sess_idx].randomstring_start_idx == -1) {
     gSessionTable[sess_idx].randomstring_start_idx = *rnum;
     gSessionTable[sess_idx].num_randomstring_entries = 0;
   }
   
  gSessionTable[sess_idx].num_randomstring_entries++;

 if ((randomStringTable[*rnum].name = copy_into_big_buf(randomstring_buf, 0)) == -1) {
   SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, randomstring_buf);
 }

  if(randomStringTable[*rnum].name == -1)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Random String");
  }

  *state = NS_ST_STAT_VAR_OPTIONS;
   return 0;
}

static int set_min_value(char *value, char *line_tok, int rnum, int *state, char *script_file_msg)
{
  int i; 
  char min[MAX_LINE_LENGTH];
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value); 
  NSDL2_VARS(NULL, NULL, "line_tok = [%s]", line_tok);  
 
  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Random String");
  }

  strcpy(min, value);
  NSDL2_VARS(NULL, NULL, "After tokenized min = %s", min);

  for (i = 0; min[i]; i++){
    if (!isdigit(min[i])){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "Minimum");
    }
  }

  if(i == 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Minimum", "Random String");
  }

  randomStringTable[rnum].min = atoi(min);

  NSDL2_VARS(NULL, NULL, "randomStringTable[%d].min = %d", rnum, randomStringTable[rnum].min);

  *state = NS_ST_STAT_VAR_OPTIONS;
   return 0;
}

static int set_max_value(char *value, char *line_tok, int rnum, int *state, char *script_file_msg)
{
  int i;
  char max[MAX_LINE_LENGTH];
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value); 
  NSDL2_VARS(NULL, NULL, "line_tok = [%s]", line_tok);  
 
  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Random String");
  }

  strcpy(max, value);
  for (i = 0; max[i]; i++){
    if (!isdigit(max[i])){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "Maximum");
    }
  }

  if(i == 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Maximum", "Random String");
  }

  randomStringTable[rnum].max = atoi(max);
  NSDL2_VARS(NULL, NULL, "randomStringTable[%d].max = %d", rnum, randomStringTable[rnum].max);
  
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}

static int set_charset(char *value, int *state, int rnum, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Random String");
  }
  
  if(strlen(value) == 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Character Set", "Random String");	
  }

  if ((randomStringTable[rnum].char_set = copy_into_big_buf(value, 0)) == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
  }

  NSDL2_VARS(NULL, NULL, "After tokenized charset = %s", value);
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}


int input_randomstring_data(char* line, int line_number, int sess_idx, char *script_filename)
{
  int state = NS_ST_STAT_VAR_NAME;
  char randomstring_buf[MAX_LINE_LENGTH];
  char* line_tok;
  int rnum;
  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char script_file_msg[MAX_LINE_LENGTH];
  NSApi api_ptr;
  int j, ret;
  char *value;
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];
 
  NSDL1_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d", line, line_number, sess_idx);

  /*bug id: 101320: using GET_NS_RTA_DIR() to get relative TA dir*/
  snprintf(script_file_msg, MAX_LINE_LENGTH, "Parsing nsl_random_string_var() declaration on line %d of %s/%s/%s: ", 
                                              line_number, GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
  
  //snprintf(script_file_msg, MAX_LINE_LENGTH, "Parsing nsl_random_string_var() declaration on line %d of scripts/%s/%s: ", line_number, get_sess_name_with_proj_subproj(sess_name), script_filename);
  script_file_msg[MAX_LINE_LENGTH-1] = '\0';

   sprintf(file_name, "%s/%s/%s", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), 
			               sess_idx, "/"), script_filename);
   //sprintf(file_name, "scripts/%s/%s", get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), script_filename);
  file_name[strlen(file_name)] = '\0';
 
  //nsl_random_string_var(rand_str, Min=1, Max=10,Charset="\\\"A-Z", refresh=USE);

  //parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 0);
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 0);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  for(j = 0; j < api_ptr.num_tokens; j++) {
    line_tok = api_ptr.api_fields[j].keyword;
    value = api_ptr.api_fields[j].value;

    NSDL2_VARS(NULL, NULL, "line_tok = [%s], value = [%s]", line_tok, value);
    NSDL2_VARS(NULL, NULL, "api_ptr.num_tokens = %d", api_ptr.num_tokens);
  
    if (!strncasecmp(line_tok, "Min", strlen("Min"))) {
      set_min_value(value, line_tok, rnum, &state, script_file_msg);
    }else if (!strncasecmp(line_tok, "Max", strlen("Max"))) {
       set_max_value(value, line_tok, rnum, &state, script_file_msg);
    }else if (!strncasecmp(line_tok, "REFRESH", strlen("REFRESH"))) {
       set_refresh(value, rnum, &state, script_file_msg);
    }else if (!strncasecmp(line_tok, "Charset", strlen("Charset"))) {
       set_charset(value, &state, rnum, script_file_msg);
    }else { /* this is for variables inputted */
       set_name_option(value, line_tok, randomstring_buf, &rnum, sess_idx, script_file_msg, &state);
    }
  }
  
  if(randomStringTable[rnum].max == -1)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Maximum", "Random String");
  }

  if(randomStringTable[rnum].char_set == -1)
  { 
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Character Set", "Random String");
  }


  if(randomStringTable[rnum].max < randomStringTable[rnum].min)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012044_ID, CAV_ERR_1012044_MSG, "Random String");
  }
  NSDL2_VARS(NULL, NULL, "value of max after parsing   = %f , value of min after parsing = %f ",randomStringTable[rnum].min,randomStringTable[rnum].max);
  return 0;
}
/*
 * copies values in the randomvar table into shared memory
 */ 
RandomStringTableEntry_Shr *copy_randomstring_into_shared_mem(void)
{
  int i;
  RandomStringTableEntry_Shr *randomstring_table_shr_mem_local = NULL;

  NSDL1_VARS(NULL, NULL, "Method called. total_randomstring_entries = %d", total_randomstring_entries);
  /* insert the SearchVarTableEntry_Shr and the PerPageSerVarTableEntry_shr */

  if (total_randomstring_entries ) {
    randomstring_table_shr_mem_local = do_shmget(total_randomstring_entries * sizeof(RandomStringTableEntry_Shr), "random var tables");

    for (i = 0; i < total_randomstring_entries; i++) {
      randomstring_table_shr_mem_local[i].var_name = BIG_BUF_MEMORY_CONVERSION(randomStringTable[i].name);
      randomstring_table_shr_mem_local[i].max = randomStringTable[i].max;
       //fprintf(stderr, " value of max  %d in SHARE MEMORY ", randomStringTable[i].max );

      randomstring_table_shr_mem_local[i].min = randomStringTable[i].min;
      // fprintf(stderr, " value of min   %d in SHARE MEMORY ", randomStringTable[i].min );
     // randomvar_table_shr_mem_local[i].format_len = randomStringTable[i].format_len;
      randomstring_table_shr_mem_local[i].refresh = randomStringTable[i].refresh;
      randomstring_table_shr_mem_local[i].sess_idx = randomStringTable[i].sess_idx;
       //fprintf(stderr, " value of refresh   %d in SHARE MEMORY " , randomStringTable[i].refresh );
   // fprintf(stderr, "value of i in share memory is %d ",i);

      if (randomStringTable[i].char_set == -1)
        randomstring_table_shr_mem_local[i].char_set = NULL;
      else
        randomstring_table_shr_mem_local[i].char_set = BIG_BUF_MEMORY_CONVERSION(randomStringTable[i].char_set);

      randomstring_table_shr_mem_local[i].uv_table_idx =
        gSessionTable[randomStringTable[i].sess_idx].vars_trans_table_shr_mem[gSessionTable[randomStringTable[i].sess_idx].var_hash_func(randomstring_table_shr_mem_local[i].var_name, strlen(randomstring_table_shr_mem_local[i].var_name))].user_var_table_idx;
      assert (randomstring_table_shr_mem_local[i].uv_table_idx != -1);
    }
  }
  return (randomstring_table_shr_mem_local);
}


#if 0
void expand_char_list(char start, char end, char *outstr, int *outstr_idx)
{
  int i;
  for (i = start ; i <= end; i++,(*outstr_idx)++){
    outstr[*outstr_idx] = i;
  }

 // printf("list = %s\n", outstr);
}


void get_expanded_list(char *outstr, char *char_set)
{
  int len;
  char last;
  int i;
  int outstr_idx = 0;
  char *data = NULL;
  len = strlen(char_set);

  MY_MALLOC(data, len+1, "Allocatring memory for char-Set",-1);
  memset(data, 0, len+1);
  strcpy(data, char_set);
 // printf("###########Length = %d\n", len);

  for(i = 0; i < len; i++)
  {
    if( i == 0){
      last = data[i];
    }
    else
      last = data[i - 1];
   // printf("I = %d\n", i);
    if(data[i] == '-')
    {
      i++;
     // printf("Before callin expand outstr_idx = %d, i = %d\n", outstr_idx, i);
      expand_char_list(last, data[i], outstr, &outstr_idx);
    //  printf("After callin expand outstr_idx = %d, i = %d\n", outstr_idx, i);
    }
    else if((data[i + 1] != '-') && (data[i - 1] != '-'))
    {
    //  printf("outstr_idx = %d, i = %d, data = %c\n", outstr_idx, i, data[i]);
      outstr[outstr_idx] = data[i];
      outstr_idx++;
    }
  }
  NSDL1_VARS(NULL, NULL, "Expanded list  = %s", outstr);
  FREE_AND_MAKE_NULL(data, "Free Data pointer", -1);
}

char* get_random_string (double min, double max, char *char_set, int *total_len)
{
  char ex_str[4048];
  int ex_len;
  int rlen;
  char *outstr;
  int i = 0, idx;

  NSDL1_VARS(NULL, NULL, "Min  %f, Max = %f", min, max);
  memset(ex_str, 0, 4048);
  get_expanded_list(ex_str, char_set);
  ex_len = strlen(ex_str);

  rlen = (int)(min + (double)((max - (min - 1)) * (rand()/(RAND_MAX + max))));
  MY_MALLOC(outstr, rlen + 1, "Malloc buffer for random string", -1);
  memset(outstr, 0, (rlen+1));
  
  *total_len = rlen;
 
  while(rlen && ex_len)
  {
    idx = (int)(1.0 + (double)((ex_len) * (rand()/(RAND_MAX + (double)ex_len))));
    NSDL1_VARS(NULL, NULL, "Idx in  expanded buffer= %d ,rlen= %d ,ex_len= %d ",idx, rlen, ex_len);
    //Doing -1 because we will never get 0 and we may get the number which is equal to length of the extended array.a
    outstr[i] = ex_str[idx - 1];
    rlen--;
    i++;
  }
  NSDL1_VARS(NULL, NULL, "Random Str  = %s", outstr);
  return outstr;
}
#endif

char* get_random_string_value(RandomStringTableEntry_Shr* var_ptr, VUser* vptr, int var_val_flag, int* total_len)
{
  char* random_val;
  NSDL1_VARS(NULL, NULL, "Method called var_ptr %p vptr %p var_val_flag %d",var_ptr, vptr,
      var_val_flag);

  if (var_val_flag) { //fill new value 
    if (var_ptr->refresh == SESSION)  { //SESSION
       NSDL1_VARS(NULL, NULL, "Entering method %s, with value  = %s", __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value);
       NSDL1_VARS(NULL, NULL, "Entering method %s, with filled_flag = %d",__FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].filled_flag);
       
      if (vptr->uvtable[var_ptr->uv_table_idx].filled_flag == 0) { //first call. assign value 
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Calling nslb_get_random_string first time");
         random_val = nslb_get_random_string( var_ptr->min , var_ptr->max, var_ptr->char_set, total_len);
        NSDL1_VARS(NULL, NULL, "Got Random var  = %s, Length = %d", random_val,*total_len);
        vptr->uvtable[var_ptr->uv_table_idx].value.value  = random_val;
        vptr->uvtable[var_ptr->uv_table_idx].length = *total_len;
        vptr->uvtable[var_ptr->uv_table_idx].filled_flag = 1;
      }else{ // need to return length as we're using this on return
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Value already set");
        *total_len = vptr->uvtable[var_ptr->uv_table_idx].length;
      }
    }else{ //USE
      NSDL1_VARS(NULL, NULL, "Refresh is USE, Calling nslb_get_random_string");
       random_val = nslb_get_random_string( var_ptr->min , var_ptr->max, var_ptr->char_set, total_len);
      /*Checking here for filled flag, if its set to 1 it means we have already
      * malloced the buffer.Its possible if we are using same variable multiple
      * times with USE in same session. So, we need to first free old memory
      * then assign new memory*/
      if (vptr->uvtable[var_ptr->uv_table_idx].filled_flag == 1)
      {
        FREE_AND_MAKE_NOT_NULL(vptr->uvtable[var_ptr->uv_table_idx].value.value,
            "vptr->uvtable[var_ptr->uv_table_idx].value.value", var_ptr->uv_table_idx);
      }
      vptr->uvtable[var_ptr->uv_table_idx].value.value  = random_val;
      vptr->uvtable[var_ptr->uv_table_idx].length = *total_len;
      vptr->uvtable[var_ptr->uv_table_idx].filled_flag = 1;
    }
  }else{
    *total_len = vptr->uvtable[var_ptr->uv_table_idx].length;
  }
  
  NSDL1_VARS(NULL, NULL, "Returning from method %s, with value  = %s",__FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value);
  return (vptr->uvtable[var_ptr->uv_table_idx].value.value);
}


int find_randomstring_idx(char* name, int sess_idx)
{
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].randomstring_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].randomstring_start_idx; i <
      gSessionTable[sess_idx].randomstring_start_idx +
      gSessionTable[sess_idx].num_randomstring_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(randomStringTable[i].name), name))
      return i;
  }

  return -1;
}

/* Function clear(s) value of random string var only in case user wants to retain value of user varible(s) for next sesssions . 
   Random string will be changed for every session  */
void clear_uvtable_for_random_string_var(VUser *vptr)
{
  int sess_idx = vptr->sess_ptr->sess_id;
  int i, uv_idx;
  UserVarEntry *cur_uvtable = vptr->uvtable; 

  NSDL2_VARS(vptr, NULL, "Method called, session_idx = %d, sess_name = %s", sess_idx, vptr->sess_ptr->sess_name);  

  if (sess_idx == -1)
    return;

  for(i = 0; i <= total_randomstring_entries; i++){

    NSDL3_VARS(vptr, NULL, "randomvar_table_shr_mem idx = %d, sess_idx = %d, uv_idx = %d",
                      i, randomstring_table_shr_mem[i].sess_idx, randomstring_table_shr_mem[i].uv_table_idx);

    /*Check for those entry which have same sess_idx as of current one*/
    if (randomstring_table_shr_mem[i].sess_idx != sess_idx){
      continue;
    } 
    /* Calculate uv_idx */
    if (randomstring_table_shr_mem[i].var_name)
    {
      uv_idx = randomstring_table_shr_mem[i].uv_table_idx;
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

// End of File

