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
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_script_parse.h"

#ifndef CAV_MAIN
RandomVarTableEntry* randomVarTable;
int max_randomvar_entries = 0;
static int max_randompage_entries;
static int total_randomvar_entries;
static int total_randompage_entries;
#else
//__thread RandomVarTableEntry* randomVarTable;
__thread int max_randomvar_entries = 0;
static __thread int max_randompage_entries;
__thread int total_randomvar_entries;
__thread int total_randompage_entries;
#endif

static int create_randomvar_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_randomvar_entries = %d, max_randomvar_entries = %d", total_randomvar_entries, max_randomvar_entries);

  if (total_randomvar_entries == max_randomvar_entries) {
    MY_REALLOC (randomVarTable, (max_randomvar_entries + DELTA_RANDOMVAR_ENTRIES) * sizeof(RandomVarTableEntry), "randomvar entries", -1);
    max_randomvar_entries += DELTA_RANDOMVAR_ENTRIES;
  }
  *row_num = total_randomvar_entries++;
  return (SUCCESS);
}

void init_randomvar_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_randomvar_entries = 0;
  total_randompage_entries = 0;

  MY_MALLOC (randomVarTable, INIT_RANDOMVAR_ENTRIES * sizeof(RandomVarTableEntry), "randomVarTable", -1);

  if(randomVarTable)
  {
    max_randomvar_entries = INIT_RANDOMVAR_ENTRIES;
    max_randompage_entries = INIT_RANDOMPAGE_ENTRIES;
  }
  else
  {
    max_randomvar_entries = 0;
    max_randompage_entries = 0;
    NS_EXIT(-1, CAV_ERR_1031013, "RandomVarTableEntry");
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

int input_randomvar_data(char* line, int line_number, int sess_idx, char *script_filename)
{
#define NS_ST_SE_NAME 1
#define NS_ST_SE_OPTIONS 2
#define NS_ST_STAT_VAR_NAME 1
#define NS_ST_STAT_VAR_OPTIONS 2
#define NS_ST_STAT_VAR_NAME_OR_OPTIONS 3
#define INIT_PERPAGESERVAR_ENTRIES 64
#define DELTA_PERPAGESERVAR_ENTRIES 32

  int state = NS_ST_SE_NAME;
  char randomvar_buf[MAX_LINE_LENGTH];
  char* line_ptr = line;
  int done = 0;
  int rnum;

  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char msg[MAX_LINE_LENGTH];
  int i;
  char script_file_msg[MAX_FIELD_LENGTH];
  int format_begins = 0;
  int format_complete = 0;

  NSDL1_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d", line, line_number, sess_idx);
  //fprintf(stderr, "input_randomvar_data: line = %s, line_number = %d, sess_idx = %d\n", line, line_number, sess_idx);
  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_random_number_var() declaration on line %d of scripts/%s/%s: ", line_number,
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
          randomvar_buf[i] = *line_ptr;
        }

        randomvar_buf[i] = '\0';

        if(validate_var(randomvar_buf)) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, randomvar_buf);
        }
      
        CLEAR_WHITE_SPACE(line_ptr);

        CHECK_CHAR(line_ptr, ',', msg);
        create_randomvar_table_entry(&rnum);

        /* First fill the default values */
        randomVarTable[rnum].name = -1;
        randomVarTable[rnum].min = 0;
        randomVarTable[rnum].max = -1;
        randomVarTable[rnum].format =-1;
        randomVarTable[rnum].refresh = SESSION;
        randomVarTable[rnum].sess_idx = sess_idx;

        if (gSessionTable[sess_idx].randomvar_start_idx == -1) {
          gSessionTable[sess_idx].randomvar_start_idx = rnum;
          gSessionTable[sess_idx].num_randomvar_entries = 0;
        }

        gSessionTable[sess_idx].num_randomvar_entries++;

        if ((randomVarTable[rnum].name = copy_into_big_buf(randomvar_buf, 0)) == -1) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, randomvar_buf);
        }

        state = NS_ST_SE_OPTIONS;
        break;

      case NS_ST_SE_OPTIONS:
        CLEAR_WHITE_SPACE(line_ptr);
        if(randomVarTable[rnum].name == -1)
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Random Number" );
        }
        if (!strncasecmp(line_ptr, "format=", strlen("format="))) {
          line_ptr += strlen("format=");
          CLEAR_WHITE_SPACE(line_ptr);
          for (i = 0; (*line_ptr != ',') && ((*line_ptr) != ' ') && ((*line_ptr) != '\t') && (*line_ptr != '\0'); line_ptr++, i++) {
            PARSE_FORMAT(randomvar_buf);
          }
          if(!format_complete)
          {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012039_ID, CAV_ERR_1012039_MSG, "Random Number");
          }

          if(i == 0) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Format", "Random Number");
          }

          randomvar_buf[i] = '\0';
          NSDL1_VARS(NULL, NULL, "The full format buffer is =%s", randomvar_buf);
          randomVarTable[rnum].format_len = strlen(randomvar_buf);
          CLEAR_WHITE_SPACE(line_ptr);

          CHECK_CHAR(line_ptr, ',', msg);

          if ((randomVarTable[rnum].format = copy_into_big_buf(randomvar_buf, 0)) == -1) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, randomvar_buf);
          }
          CLEAR_WHITE_SPACE(line_ptr);
        }else if (!strncasecmp(line_ptr, "max=", strlen("max="))) {
          line_ptr += strlen("max=");
          CLEAR_WHITE_SPACE(line_ptr);
          for (i = 0; (*line_ptr != ',') && (*line_ptr != '\0') && (*line_ptr != ' '); line_ptr++, i++) 
          {
            if (isdigit(*line_ptr))
              randomvar_buf[i] = *line_ptr;
            else {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "Maximum");
            }
          }
          if(i == 0) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Maximum", "Random Number");
          }

          CLEAR_WHITE_SPACE(line_ptr);
          if (*line_ptr == ',')
            line_ptr++; 
          randomvar_buf[i]='\0';
          randomVarTable[rnum].max = atoi(randomvar_buf);
          CLEAR_WHITE_SPACE(line_ptr);
        }else if (!strncasecmp(line_ptr, "min=", strlen("min="))) {
          line_ptr += strlen("min=");
          CLEAR_WHITE_SPACE(line_ptr);
          for (i = 0; (*line_ptr != ',') && (*line_ptr != '\0') && (*line_ptr != ' '); line_ptr++, i++)
          {
            if (isdigit(*line_ptr))
              randomvar_buf[i] = *line_ptr;
            else {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "Minimum");
            }
          }
          if(i == 0) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Minimum", "Random Number");
          }

          CLEAR_WHITE_SPACE(line_ptr);
          if (*line_ptr == ',')
            line_ptr++;
          randomvar_buf[i]= '\0';
          randomVarTable[rnum].min = atoi(randomvar_buf);
          CLEAR_WHITE_SPACE(line_ptr);
        } else if (!strncasecmp(line_ptr, "REFRESH", strlen("REFRESH"))) {
          if (state == NS_ST_STAT_VAR_NAME) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Random Number");
          }

          line_ptr += strlen("REFRESH");
          CLEAR_WHITE_SPACE(line_ptr);
          CHECK_CHAR(line_ptr, '=', script_file_msg);
          CLEAR_WHITE_SPACE(line_ptr);
          NSDL3_VARS(NULL, NULL, "Refresh Name = %s", line_ptr);
          for (i = 0; *line_ptr != '\0' && (*line_ptr != ' '); line_ptr++, i++) {
            randomvar_buf[i] = *line_ptr;
            NSDL3_VARS(NULL, NULL, "i = %d, Refresh line for token = %s, *line_ptr = %c, randomvar_buf[%d] = %c", 
                i, line_ptr, *line_ptr, i, randomvar_buf[i]);
          }
          randomvar_buf[i] = '\0';

          if (!strcmp(randomvar_buf, "SESSION")) {
            randomVarTable[rnum].refresh = SESSION;
          } else if (!strcmp(randomvar_buf, "USE")) {
            randomVarTable[rnum].refresh = USE;
          } else {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, randomvar_buf, "Update Value On", "Random Number");
          }
          CLEAR_WHITE_SPACE(line_ptr);
        } else if (*line_ptr == '\0') {
          done = 1;
        } else {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, line_ptr, "Random Number");
        }
    }
  } //while
  if (randomVarTable[rnum].max == -1)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Maximum", "Random Number");
  }

  if (randomVarTable[rnum].format == -1)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Format", "Random Number");
  }

  if(randomVarTable[rnum].max <= randomVarTable[rnum].min)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012044_ID, CAV_ERR_1012044_MSG, "Random Number");
  }

        
#undef NS_ST_SE_NAME
#undef NS_ST_SE_OPTIONS
  return(0);
}

/*
 * copies values in the randomvar table into shared memory
 */

RandomVarTableEntry_Shr *copy_randomvar_into_shared_mem(void)
{
  int i;
  RandomVarTableEntry_Shr *randomvar_table_shr_mem_local = NULL;

  NSDL1_VARS(NULL, NULL, "Method called. total_randomvar_entries = %d", total_randomvar_entries);
  /* insert the SearchVarTableEntry_Shr and the PerPageSerVarTableEntry_shr */

  if (total_randomvar_entries ) {
    randomvar_table_shr_mem_local = do_shmget(total_randomvar_entries * sizeof(RandomVarTableEntry_Shr), "search var tables");

    for (i = 0; i < total_randomvar_entries; i++) {
      randomvar_table_shr_mem_local[i].var_name = BIG_BUF_MEMORY_CONVERSION(randomVarTable[i].name);
      randomvar_table_shr_mem_local[i].max = randomVarTable[i].max;
      randomvar_table_shr_mem_local[i].min = randomVarTable[i].min;
      randomvar_table_shr_mem_local[i].format_len = randomVarTable[i].format_len;
      randomvar_table_shr_mem_local[i].refresh = randomVarTable[i].refresh;
      randomvar_table_shr_mem_local[i].sess_idx = randomVarTable[i].sess_idx;

      if (randomVarTable[i].format == -1)
        randomvar_table_shr_mem_local[i].format = NULL;
      else
        randomvar_table_shr_mem_local[i].format = BIG_BUF_MEMORY_CONVERSION(randomVarTable[i].format);

      randomvar_table_shr_mem_local[i].uv_table_idx =
        gSessionTable[randomVarTable[i].sess_idx].vars_trans_table_shr_mem[gSessionTable[randomVarTable[i].sess_idx].var_hash_func(randomvar_table_shr_mem_local[i].var_name, strlen(randomvar_table_shr_mem_local[i].var_name))].user_var_table_idx;
      assert (randomvar_table_shr_mem_local[i].uv_table_idx != -1);
    }
  }
  return (randomvar_table_shr_mem_local);
}

/*
 * routine to get a random variable using a random number (between min and max
 * value) and arbitrary input format 
 * eg.,
 * min = 1, max = 10, format = "xx%dyy" --> xx5yy
 * min = 2 , max = 50, format = "x%dyyyy" --> x25yyyy 
 */
/*If malloc_or_static is 1 then send random number in malloced buffer
 * otherwise send in static buffer*/
char* get_random_number( double min , double  max, char *format, int format_len, int *total_len, int malloc_or_static)
{
  int out;
  static __thread char outstr_static[64+1];
#define MAX_STATIC_BUF_LEN 64 + 1
  char *outstr_malloc;
  char *outstr;
  char *specifier_pos = NULL;
  char* tmp_format = format;
  int len = 12 + format_len + 1;
  int additional_len = 0 ;

//fixes bug 874
//Getting the total length to be malloc in case some specifiers i.e %23lu.
  if((specifier_pos = strchr(tmp_format, '%')) != NULL){
    specifier_pos++;
  }

  NSDL1_VARS(NULL, NULL, "specifier_pos = %s", specifier_pos);
  if(specifier_pos != '\0')
    additional_len = atoi(specifier_pos);

  if(malloc_or_static)
    len = len + additional_len;
  else
    len = (len+additional_len) > MAX_STATIC_BUF_LEN ?MAX_STATIC_BUF_LEN:(len+additional_len);
    
  NSDL1_VARS(NULL, NULL, "Method called min %lf max %lf format %s format length %d len = %d additionla_len = %d",min, max, format, format_len, len, additional_len);
  //srand(getpid());
 /*
  * using 12 for max no of digits in a 32 bit signed int (2^31 -1)- this gives 10
  *  digits. we're using 12 instead  + 1 for NULL.
  */
  if(malloc_or_static == 1)//Malloced buffer to send
  {
    MY_MALLOC (outstr_malloc, len, "outstr", -1);
    memset(outstr_malloc, 0, len);
    outstr = outstr_malloc;
  }
  else
    outstr = outstr_static;  
  
  out = min + (double)((max - (min - 1)) * (rand()/(RAND_MAX + max)));
  if((*total_len = snprintf(outstr, len, format, out)) > len)
  {
    *total_len = len;
     outstr[len] = '\0';
  }
  NSDL2_VARS(NULL, NULL, "out = %d, outstr  = %s", out, outstr?outstr:NULL);
  return outstr;
}

char* get_random_var_value(RandomVarTableEntry_Shr* var_ptr, VUser* vptr, int
    var_val_flag, int* total_len)
{
  char * random_val;
  NSDL1_VARS(NULL, NULL, "Method called var_ptr %p vptr %p var_val_flag %d",var_ptr, vptr,
      var_val_flag);

  if (var_val_flag) { //fill new value 
    if (var_ptr->refresh == SESSION)  { //SESSION
       NSDL1_VARS(NULL, NULL, "Entering method %s, with value  = %s",
                 __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value);
       NSDL1_VARS(NULL, NULL, "Entering method %s, with filled_flag = %d",
                 __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].filled_flag);
       
      if (vptr->uvtable[var_ptr->uv_table_idx].filled_flag == 0) { //first call. assign value 
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Calling get_random_number first time");
        random_val = get_random_number( var_ptr->min , var_ptr->max, var_ptr->format,
            var_ptr->format_len, total_len, 1);
        NSDL1_VARS(NULL, NULL, "Got Random var  = %s, Length = %d", random_val,
            *total_len);
        vptr->uvtable[var_ptr->uv_table_idx].value.value  = random_val;
        vptr->uvtable[var_ptr->uv_table_idx].length = *total_len;
        vptr->uvtable[var_ptr->uv_table_idx].filled_flag = 1;
      }else{ // need to return length as we're using this on return
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Value already set");
        *total_len = vptr->uvtable[var_ptr->uv_table_idx].length;
      }
    }else{ //USE
      NSDL1_VARS(NULL, NULL, "Refresh is USE, Calling get_random_number");
      random_val = get_random_number( var_ptr->min , var_ptr->max, var_ptr->format,
          var_ptr->format_len, total_len, 1);
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
  
  NSDL1_VARS(NULL, NULL, "Returning from method %s, with value  = %s",
      __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value);
  return (vptr->uvtable[var_ptr->uv_table_idx].value.value);
}


int find_randomvar_idx(char* name, int sess_idx) {
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].randomvar_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].randomvar_start_idx; i <
      gSessionTable[sess_idx].randomvar_start_idx +
      gSessionTable[sess_idx].num_randomvar_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(randomVarTable[i].name), name))
      return i;
  }

  return -1;
}


/* Function clear(s) value of random variable only in case user wants to retain value of user varible(s) for next sesssions . */
void clear_uvtable_for_random_var(VUser *vptr)
{
  int sess_idx = vptr->sess_ptr->sess_id;
  int i, uv_idx;
  UserVarEntry *cur_uvtable = vptr->uvtable; 

  NSDL2_VARS(vptr, NULL, "Method called, session_idx = %d, sess_name = %s", sess_idx, vptr->sess_ptr->sess_name);  

  if (sess_idx == -1)
    return;

 for (i=0; i <= total_randomvar_entries; i++){

    NSDL3_VARS(vptr, NULL, "randomvar_table_shr_mem idx = %d, sess_idx = %d, uv_idx = %d",
                      i, randomvar_table_shr_mem[i].sess_idx, randomvar_table_shr_mem[i].uv_table_idx);

    /*Check for those entry which have same sess_idx as of current one*/
    if (randomvar_table_shr_mem[i].sess_idx != sess_idx){
      continue;
    }
 
    /* Calculate uv_idx */
    if (randomvar_table_shr_mem[i].var_name)
    {
      uv_idx = randomvar_table_shr_mem[i].uv_table_idx;
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
