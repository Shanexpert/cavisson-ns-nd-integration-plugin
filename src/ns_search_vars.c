/******************************************************************
 * Name    :    ns_search_vars.c
 * Purpose :    SEARCH_VAR - parsing, shared memory, run time 
 * Note    :    Search Variables are initialized as a result of 
                searching a response document for user specified criteria.
* Syntax  :    nsl_search_var (svar, PAGE=page1, PAGE=page2, LB=left-boundary, RB=right-boundary, ORD=ALL, SaveOffset=5,SaveLen=20, Convert=HTLMToURL, RedirectionDepth=1, ActionOnNotFound=Warning);
* Author  :    
* Intial version date:    05/06/08
* Last modification date: 19/11/09
*****************************************************************/

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
#include "ns_parent.h"
#include "child_init.h" //for ns_handle
#include "ns_debug_trace.h"
#include "poi.h" //for ns_get_random
#include "ns_string.h" //ns_encode_url etc
#include "ns_event_log.h" 
#include "ns_event_id.h"
#include "nslb_sock.h"
#include "ns_vuser_trace.h"
#include "ns_page_dump.h"

#include "nslb_util.h" //for get_tokens()
#include "nslb_search_vars.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_script_parse.h"
#include "ns_socket.h"

#ifndef CAV_MAIN
SearchVarTableEntry* searchVarTable;
static int total_searchvar_entries;
static int total_perpageservar_entries;
static int total_searchpage_entries;
SearchVarTableEntry_Shr *searchvar_table_shr_mem = NULL;
int max_searchvar_entries = 0;
int max_searchpage_entries = 0;
#else
//__thread SearchVarTableEntry* searchVarTable;
__thread int total_searchvar_entries;
__thread int total_perpageservar_entries;
__thread int total_searchpage_entries;
__thread SearchVarTableEntry_Shr *searchvar_table_shr_mem = NULL;
int __thread max_searchvar_entries = 0;
int __thread max_searchpage_entries = 0;
#endif

/* Required for support of Regex in search param.
   We are only supporting til group 10 */
#define MIN_REGEX_GROUP 0
#define MAX_REGEX_GROUP 10

extern int loader_opcode;

static int create_searchvar_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_searchvar_entries = %d, max_searchvar_entries = %d", total_searchvar_entries, max_searchvar_entries);

  if (total_searchvar_entries == max_searchvar_entries) {
    //MY_REALLOC (searchVarTable, (max_searchvar_entries + DELTA_SEARCHVAR_ENTRIES) * sizeof(SearchVarTableEntry), "searchvar entries", -1);
    MY_REALLOC_EX(searchVarTable, (max_searchvar_entries + DELTA_SEARCHVAR_ENTRIES) * sizeof(SearchVarTableEntry), max_searchvar_entries * sizeof(SearchVarTableEntry), "searchvar entries", -1);
    max_searchvar_entries += DELTA_SEARCHVAR_ENTRIES;
  }
  *row_num = total_searchvar_entries++;
  return (SUCCESS);
}

static int create_searchpage_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_searchpage_entries = %d, max_searchpage_entries = %d", total_searchpage_entries, max_searchpage_entries);
  if (total_searchpage_entries == max_searchpage_entries) {
    //MY_REALLOC (searchPageTable, (max_searchpage_entries + DELTA_SEARCHPAGE_ENTRIES) * sizeof(SearchPageTableEntry), "searchpage entries", -1);
    MY_REALLOC_EX(searchPageTable, (max_searchpage_entries + DELTA_SEARCHPAGE_ENTRIES) * sizeof(SearchPageTableEntry), max_searchpage_entries * sizeof(SearchPageTableEntry), "searchpage entries", -1);
    max_searchpage_entries += DELTA_SEARCHPAGE_ENTRIES;
  }
  *row_num = total_searchpage_entries++;
  return (SUCCESS);
}

static int create_perpageservar_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_perpageservar_entries = %d, max_perpageservar_entries = %d", total_perpageservar_entries, max_perpageservar_entries);
  if (total_perpageservar_entries == max_perpageservar_entries) {
    //MY_REALLOC (perPageSerVarTable, (max_perpageservar_entries + DELTA_PERPAGESERVAR_ENTRIES) * sizeof(PerPageSerVarTableEntry), "perpageservar entries", -1);
    MY_REALLOC_EX(perPageSerVarTable, (max_perpageservar_entries + DELTA_PERPAGESERVAR_ENTRIES) * sizeof(PerPageSerVarTableEntry), max_perpageservar_entries * sizeof(PerPageSerVarTableEntry), "perpageservar entries", -1);
    max_perpageservar_entries += DELTA_PERPAGESERVAR_ENTRIES;
  }
  *row_num = total_perpageservar_entries++;
  return (SUCCESS);
}

/***********************************/

#define NS_ST_SE_NAME 1
#define NS_ST_SE_OPTIONS 2

static int set_search_encodeSpaceBy(char* value, char* EncodeSpaceBy, char *key, char *msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], key = [%s]", value, key);

  strcpy(EncodeSpaceBy, value);
  NSDL2_VARS(NULL, NULL, "After tokenized EncodeSpaceBy = [%s]", EncodeSpaceBy);

  if(!strcmp(EncodeSpaceBy, "+") || !strcmp(EncodeSpaceBy, "%20"))
    return 0;
  else
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012139_ID, CAV_ERR_1012139_MSG, "Search");

  return 0;
}

static int set_char_to_encode_buf(char *value, char *char_to_encode_buf, int rnum, char* msg,int encode_flag_specified, char *encode_chars_done){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  int i;
  if (encode_flag_specified == 0){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012137_ID, CAV_ERR_1012137_MSG , "Search");
  }

  strcpy(char_to_encode_buf, value);
  NSDL3_VARS(NULL, NULL, "After tokenized CharatoEncode = %s", char_to_encode_buf);
  
  /*Encode chars can have any special characters including space, single quiote, double quotes. Few examples:
 *   EncodeChars=", \[]"
 *     ncodeChars="~`!@#$%^&*-_+=[]{}\|;:'\" (),<>./?"
 *       */
      
  for (i = 0; char_to_encode_buf[i] != '\0'; i++) {
    if(isalnum(char_to_encode_buf[i])){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012138_ID, CAV_ERR_1012138_MSG, char_to_encode_buf, "Search");
    }

    NSDL3_VARS(NULL, NULL, "i = %d, char_to_encode_buf[i] = [%c]", i, char_to_encode_buf[i]);

    searchVarTable[rnum].encode_chars[(int)char_to_encode_buf[i]] = 1;
  }

  *encode_chars_done = 1;

  return 0;
}

static int set_buffer_for_encodeMode(char *value, int rnum, char* msg, int *encode_flag_specified){
   NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  NSDL3_VARS(NULL, NULL, "After tokenized EncodeMode Name = %s", value);
  if (!strcasecmp(value, "All")) {
        searchVarTable[rnum].encode_type = ENCODE_ALL;
  } else if (!strcasecmp(value, "None")) {
      searchVarTable[rnum].encode_type = ENCODE_NONE;
      memset(searchVarTable[rnum].encode_chars, 0, TOTAL_CHARS);
  } else if (!strcasecmp(value, "Specified")) {
      *encode_flag_specified = 1;
      searchVarTable[rnum].encode_type = ENCODE_SPECIFIED;
      memset(searchVarTable[rnum].encode_chars, 0, TOTAL_CHARS);
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "EncodeMode", "Search");
  }
  return 0;
}



/**********************************/
void init_searchvar_info(void) 
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_searchvar_entries = 0;
  total_perpageservar_entries = 0;
  total_searchpage_entries = 0;

  MY_MALLOC (searchVarTable, INIT_SEARCHVAR_ENTRIES * sizeof(SearchVarTableEntry), "searchVarTable", -1);
  MY_MALLOC (searchPageTable, INIT_SEARCHPAGE_ENTRIES * sizeof(SearchPageTableEntry), "searchPageTable", -1);

  if(searchVarTable && searchPageTable)
    {
      max_searchvar_entries = INIT_SEARCHVAR_ENTRIES;
      max_searchpage_entries = INIT_SEARCHPAGE_ENTRIES;
    } 
  else
    {
      max_searchvar_entries = 0;
      max_searchpage_entries = 0;
      NS_EXIT(-1, CAV_ERR_1031012, "SearchVarTableEntry", "SearchPageTableEntry");
    }
}

/********************************************************
Added By:    Manish Kr. Mishra
Date:        Fri Jan 20 18:20:33 EST 2012

Discription: We know that API's in script.capture or registration.specs are parsed sequentialy so in case of search in 
             search if some one use API which contain search in serach parameter before the API from which this parameter
             will take the value, he will not found the value so we have to make sure that the dependent parameters should be in their right sequence.

Return:      0--> If sequence of API's is wrong
             1--> If sequence of API's is right
********************************************************/
int check_var_for_search_in_var(int var_idx, char *var_name)
{
  int srchVarTbl_idx = 0;
  
  for(srchVarTbl_idx = 0; srchVarTbl_idx < var_idx; srchVarTbl_idx++)
    if(!strcmp(var_name, RETRIEVE_BUFFER_DATA(searchVarTable[srchVarTbl_idx].name)))
      return 1;
  return 0;
}

int input_searchvar_data(char* line, int line_number, int sess_idx, char *script_filename) 
{
  int state = NS_ST_SE_NAME;
  char searchvar_buf[MAX_LINE_LENGTH];
  int i;
  int return_value = 0;
  int rnum;
  int lb_done = 0;
  int rb_done = 0;
  int re_done = 0;
  int do_page = 0;
  int lb_processing = 0;
  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char page_name[MAX_LINE_LENGTH];
  char msg[MAX_LINE_LENGTH];
  int varpage_idx;
  int num_tok =0;
  char *fields[34];
  /*Added By Manish: parse through new parse api */
  NSApi api_ptr;  //Manish: for parsing from parse_api
  int j, ret;
  char key[MAX_ARG_NAME_SIZE + 1];
  char value[MAX_ARG_VALUE_SIZE + 1];
  //char temp[MAX_ARG_VALUE_SIZE + 1];       //temporary buff used to store converted .* value of regex type LB & RB 
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];
  int encode_flag_specified = 0;
  char convert_buf[MAX_CONVERT_BUF_LENGTH] = "";

  char EncodeSpaceBy[16] = "+";
  char encode_chars_done = 0;
  char char_to_encode_buf[1024];
 
  NSDL1_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d", line, line_number, sess_idx);
  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_search_var() declaration on line %d of scripts/%s/%s: ", line_number, 
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
  //Parsing api nsl_search_var() api
  //ret = parse_api(&api_ptr, line, file_name, err_msg, line_number);
  //Since we need to remove spaces from value so pass set flag trim_spaces = 1
  //parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  int loc_reggroup = 0;
  
  for(j = 0; j < api_ptr.num_tokens; j++) {
    strcpy(key, api_ptr.api_fields[j].keyword);
    strcpy(value, api_ptr.api_fields[j].value);
 
    NSDL2_VARS(NULL, NULL, "j = %d, api_ptr.num_tokens = %d, key = [%s], value = [%s], state = %d", 
                            j, api_ptr.num_tokens, key, value, state);
    switch (state) {
    case NS_ST_SE_NAME:
      //Manish: serach parameter should not have any value
      if(strcmp(value, ""))
        break;

      strcpy(searchvar_buf, key);
      NSDL2_VARS(NULL, NULL, "parameter name = [%s]", searchvar_buf);
    
      /* For validating the variable we are calling the validate_var funcction */
      if(validate_var(searchvar_buf)) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, searchvar_buf);
      }

      create_searchvar_table_entry(&rnum);

      searchVarTable[rnum].sess_idx = sess_idx;

      if (gSessionTable[sess_idx].searchvar_start_idx == -1) {
        gSessionTable[sess_idx].searchvar_start_idx = rnum;
        gSessionTable[sess_idx].num_searchvar_entries = 0;
      }

      gSessionTable[sess_idx].num_searchvar_entries++;

      if ((searchVarTable[rnum].name = copy_into_big_buf(searchvar_buf, 0)) == -1) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, searchvar_buf);
      }
     
      /* Fills the default values */
      searchVarTable[rnum].action_on_notfound = VAR_NOTFOUND_IGNORE;
      searchVarTable[rnum].searchvar_rdepth_bitmask = VAR_IGNORE_REDIRECTION_DEPTH;//take the last bydefault
      searchVarTable[rnum].convert = VAR_CONVERT_NONE;
      searchVarTable[rnum].ord = 1;
      searchVarTable[rnum].lbmatch = LBMATCH_CLOSEST;
      searchVarTable[rnum].search_flag =0;        //set only for regular expression for LB and RB
      /* Not Yet Implemented */
      searchVarTable[rnum].search = SEARCH_BODY;
      searchVarTable[rnum].search_in_var_name = NULL; // Variable 
      searchVarTable[rnum].saveoffset = 0;
      searchVarTable[rnum].savelen = -1;
      searchVarTable[rnum].pgall = 0;
      searchVarTable[rnum].lb = -1;
      searchVarTable[rnum].rb = -1;
      //searchVarTable[rnum].regex = -1;
      searchVarTable[rnum].encode_type = ENCODE_NONE;
      searchVarTable[rnum].lb_rb_type = 0;
      //searchVarTable[rnum].group = 0;

      memset(searchVarTable[rnum].encode_chars, 49, TOTAL_CHARS);
      
      for(i = 'a'; i<='z';i++)
        searchVarTable[rnum].encode_chars[i] = 0;
      for(i = 'A'; i<='Z';i++)
        searchVarTable[rnum].encode_chars[i] = 0;
      for(i = '0'; i<='9';i++)
        searchVarTable[rnum].encode_chars[i] = 0;

      searchVarTable[rnum].encode_chars['+'] = 0;
      searchVarTable[rnum].encode_chars['.'] = 0;
      searchVarTable[rnum].encode_chars['_'] = 0;
      searchVarTable[rnum].encode_chars['-'] = 0;
 
      
      state = NS_ST_SE_OPTIONS;
      break;

    case NS_ST_SE_OPTIONS:
      if (!strcasecmp(key, "ActionOnNotFound")) {
        NSDL2_VARS(NULL, NULL, "action not found  = [%s]", value);
        if (!strcasecmp(value, "Error")) {
          searchVarTable[rnum].action_on_notfound = VAR_NOTFOUND_ERROR;
        } else if (!strcasecmp(value, "Warning")) {
          searchVarTable[rnum].action_on_notfound = VAR_NOTFOUND_WARNING;
        } else {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "ActionOnNotFound", "Search");
        }
      } else if (!strcasecmp(key, "Convert")) {
          NSDL2_VARS(NULL, NULL, "Convert  = [%d]", value);
          if (!strcasecmp(value, VAR_CONVERT_TEXT_TO_URL_STR)) {
            searchVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_URL;
          } else if (!strcasecmp(value, VAR_CONVERT_URL_TO_TEXT_STR)) {
              searchVarTable[rnum].convert = VAR_CONVERT_URL_TO_TEXT;
          } else if (!strcasecmp(value, VAR_CONVERT_TEXT_TO_HTML_STR)) {
              searchVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_HTML;
          } else if (!strcasecmp(value, VAR_CONVERT_HTML_TO_TEXT_STR)) {
              searchVarTable[rnum].convert = VAR_CONVERT_HTML_TO_TEXT;
          } else if (!strcasecmp(value, VAR_CONVERT_HTML_TO_URL_STR)) {
              /*Note:- currently we are assuming (html <--> url) <==> (text <--> url)*/
              //searchVarTable[rnum].convert = VAR_CONVERT_HTML_TO_URL;
              searchVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_URL;
          } else if (!strcasecmp(value, VAR_CONVERT_URL_TO_HTML_STR)) {
              //searchVarTable[rnum].convert = VAR_CONVERT_URL_TO_HTML;
              searchVarTable[rnum].convert = VAR_CONVERT_URL_TO_TEXT;
          } else if (!strcasecmp(value, VAR_CONVERT_TEXT_TO_BASE64_STR)) {
              searchVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_BASE64;
          } else if (!strcasecmp(value, VAR_CONVERT_BASE64_TO_TEXT_STR)) {
              searchVarTable[rnum].convert = VAR_CONVERT_BASE64_TO_TEXT;
          } else if(!strcasecmp(value, VAR_CONVERT_TEXT_TO_HEX_STR)){
              searchVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_HEX;
          } else {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "Convert", "Search" );
          } 
      } else if (!strcasecmp(key, "LB_RB_TYPE")) 
        {
          NSDL3_VARS(NULL, NULL, "Parsing of SEARCH argument value = %s", value);
          if(!strcasecmp(value, "NS_SEARCH_VAR_LB")) {
            NSDL3_VARS(NULL, NULL, "Search type is NS_SEARCH_VAR_LB");
            searchVarTable[rnum].lb_rb_type = NS_SEARCH_VAR_LB;
           }
          else if(!strcasecmp(value, "NS_SEARCH_VAR_RB")) {
            NSDL3_VARS(NULL, NULL, "Search type is NS_SEARCH_VAR_RB");
            searchVarTable[rnum].lb_rb_type = NS_SEARCH_VAR_RB;
          }
          else if(!strcasecmp(value, "NS_SEARCH_VAR_LB_RB")) {
            NSDL3_VARS(NULL, NULL, "Search type is NS_SEARCH_VAR_LB_RB");
            searchVarTable[rnum].lb_rb_type = NS_SEARCH_VAR_LB_RB;
          }
          else {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "Search" );
          }
        } 
        else if (!strcasecmp(key, "RedirectionDepth")) {
          if(!global_settings->g_follow_redirects && !(run_mode_option & RUN_MODE_OPTION_COMPILE))
          {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012030_ID ,CAV_ERR_1012030_MSG);
          }

          if (!strcasecmp(value, "Last")) {
            searchVarTable[rnum].searchvar_rdepth_bitmask = VAR_IGNORE_REDIRECTION_DEPTH;
          } else if(!strcasecmp(value, "ALL")) {
              searchVarTable[rnum].searchvar_rdepth_bitmask = VAR_ALL_REDIRECTION_DEPTH;
          } else {
              searchVarTable[rnum].searchvar_rdepth_bitmask = 0; /*must reset this since default is set -1
                                                      * we are going to set the bit on it. 
                                                      */ 
              strcpy(searchvar_buf, value); //save the buffer
              NSDL2_VARS(NULL, NULL, "redirection = [%s]", searchvar_buf);
              num_tok = get_tokens(searchvar_buf, fields, ";" , MAX_REDIRECTION_DEPTH_LIMIT +1);
              if(num_tok == 0)
              {  
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID ,CAV_ERR_1012132_MSG, key, "Search");
              }
              char *ptr1 = NULL, *ptr2 = NULL, *value_ptr = NULL;
              int local_rdepth = 0;
              value_ptr = value;
              for(i = 0; i < num_tok; i++)
              {
              /*chain last value handle here[if RedirectionDepth=1;2;3, next any field then handle 3 here]             without chain eg RedirectionDepth=1,next any field also handle here
              Rest of the chain element5s handled in below
             */
                if(strstr(fields[i], ",")) {
                  ptr1 = fields[i];
                  ptr2 = strstr(ptr1, ","); 
                  strncpy(searchvar_buf, fields[i], ptr2 - ptr1);
                  searchvar_buf[ptr2 - ptr1] = 0;
                  local_rdepth = atoi(searchvar_buf);
                  searchVarTable[rnum].searchvar_rdepth_bitmask |= (1 << (local_rdepth - 1 ));
                  check_redirection_limit(local_rdepth);
                  //line_ptr = ptr2 +1; //update the line_ptr
                  value_ptr += (ptr2 - ptr1);
                } else {  
                    local_rdepth = atoi(fields[i]);    
                    check_redirection_limit(local_rdepth);
                    searchVarTable[rnum].searchvar_rdepth_bitmask |= (1 << (local_rdepth - 1 ));
                }
                //chain handle
                if(i < (num_tok - 1)) 
                  value_ptr += strlen(fields[i]) + 1;  // 1 is incr for ;
                else if(!(ptr2 - ptr1))
                  value_ptr += strlen(fields[i]);
              } /* end of num_tok for loop*/
          } 
      }else if((!strcasecmp(key, "LBMATCH"))){
           if (atoi(value) < 0)
           {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012033_ID, CAV_ERR_1012033_MSG, key, "Search");
           }
           else if(isalpha(value[0]) && (strcasecmp(value, "CLOSEST")!=0) && (strcasecmp(value, "FIRST")!=0))
           {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012034_ID, CAV_ERR_1012034_MSG);
           }
           else if(strlen(value)== 0 )
           {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Search");
           }
           if(!strcasecmp(value, "CLOSEST"))
           {
             searchVarTable[rnum].lbmatch = LBMATCH_CLOSEST; 
           }
           else if(!strcasecmp(value, "FIRST"))
             searchVarTable[rnum].lbmatch = LBMATCH_FIRST; 
          else
            searchVarTable[rnum].lbmatch = atoi(value); 
      }else if ((!strncasecmp(key, "LB", 2)) || (!strncasecmp(key, "RB", 2))) { 
          if ((!strcasecmp(key, "LB")) || (!strcasecmp(key, "LB/RE")) || (!strcasecmp(key, "LB/BINARY")) || (!strcasecmp(key, "LB/BINARY/RE"))
             || (!strcasecmp(key, "LB/IC")))
          {  
            if (lb_done) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, key, "Search");
            }
            if (!strcasecmp(key, "LB/RE"))
              searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_LB_REGX);   //set only for regexp for LB 
          
            if (!strcasecmp(key, "LB/BINARY"))
              searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_LB_BINARY);  //set lb binary flag for binary data 
         
            if (!strcasecmp(key, "LB/BINARY/RE")) {
              searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_LB_REGX);   //set only for regexp for LB 
              searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_LB_BINARY);  //set lb binary flag for binary data 
            }
 
             if (!strcasecmp(key, "LB/IC"))
               searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_LB_IC);   //set lb ignore case flag

	     lb_processing = 1;
           } 
           else 
           {
              if (rb_done) {
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, key, "Search");
              }

             if (!strcasecmp(key, "RB/RE"))
               searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_RB_REGX);   //set only for regexp for RB

             if (!strcasecmp(key, "RB/BINARY"))
               searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_RB_BINARY);   //set rb binary flag for binary data 

             if (!strcasecmp(key, "RB/BINARY/RE")) {
               searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_RB_REGX);   //set only for regexp for RB 
               searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_RB_BINARY);  //set rb binary flag for binary data 
             }
             if (!strcasecmp(key, "RB/IC"))
               searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_RB_IC);   //set rb ignore case flag

             lb_processing = 0;
           }

           if (strlen(value) == 0) {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Search");
           }

           if (lb_processing) {
             if(((searchVarTable[rnum].search_flag & VAR_LB_BINARY) == VAR_LB_BINARY) || (!strcasecmp(key, "LB/BINARY")) || 
               (!strcasecmp(key, "LB/BINARY/RE"))) {
               convert_buf[0] = '\0';
               convert_hex_to_char(value, convert_buf);
               searchVarTable[rnum].lb = copy_into_big_buf(convert_buf, 0);
               return_value = regcomp( &(searchVarTable[rnum].preg_lb), convert_buf, REG_EXTENDED);
             }
             else{ 
              searchVarTable[rnum].lb = copy_into_big_buf(value, 0);
              if(!strcasecmp(key, "LB/RE"))
                return_value = regcomp( &(searchVarTable[rnum].preg_lb), value, REG_EXTENDED);
             }
 
           if(searchVarTable[rnum].lb == -1)
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
 
           if (return_value != 0)
           {
             regerror(return_value, &(searchVarTable[rnum].preg_lb), err_msg, 1000);
             print_core_events((char*)__FUNCTION__, __FILE__,"%s regcomp failed:%s", msg, err_msg);

             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000022]: ", CAV_ERR_1000022 + CAV_ERR_HDR_LEN, err_msg);
           }
 
            lb_done = 1;
          }

         else {  
           if(((searchVarTable[rnum].search_flag & VAR_RB_BINARY) == VAR_RB_BINARY) || (!strcasecmp(key, "RB/BINARY")) || 
              (!strcasecmp(key, "RB/BINARY/RE")))
           {
             convert_buf[0] = '\0';
             convert_hex_to_char(value, convert_buf); 
             searchVarTable[rnum].rb = copy_into_big_buf(convert_buf, 0);
             return_value = regcomp( &(searchVarTable[rnum].preg_rb), convert_buf, REG_EXTENDED);
           }
           else { 
             searchVarTable[rnum].rb = copy_into_big_buf(value, 0);
             if(!strcasecmp(key, "RB/RE"))
               return_value=regcomp(&(searchVarTable[rnum].preg_rb), value, REG_EXTENDED);
           }

           if (searchVarTable[rnum].rb == -1)
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);

           if (return_value != 0)
           {
             regerror(return_value, &(searchVarTable[rnum].preg_rb), err_msg, 1000);
             print_core_events((char*)__FUNCTION__, __FILE__,"%s regcomp failed:%s", msg, err_msg);

             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000022]: ", CAV_ERR_1000022 + CAV_ERR_HDR_LEN, err_msg);
           }
           rb_done = 1;
         }
 
       }
       else if(!strncasecmp(key, "GROUP", 5)) {
         if(nslb_atoi(value, &loc_reggroup) < 0)
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, key, "Search");
         if((loc_reggroup < MIN_REGEX_GROUP) || (loc_reggroup > MAX_REGEX_GROUP))
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012020_ID, CAV_ERR_1012020_MSG, key, "Search");
       }
       else if (!strcasecmp(key, "RE")) {
        if(re_done) 
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, key, "Search");
        if((!lb_done) && (!rb_done))
        {
          searchVarTable[rnum].search_flag = (searchVarTable[rnum].search_flag | VAR_REGEX); 
          re_done++;
         
          if (strlen(value) == 0)
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Search");

          if((re_done) && (searchVarTable[rnum].search_flag == VAR_REGEX))
          {
            searchVarTable[rnum].lb = copy_into_big_buf(value, 0);
            if (searchVarTable[rnum].lb == -1)
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
            return_value=regcomp(&(searchVarTable[rnum].preg_lb), value, REG_EXTENDED);
            if (return_value != 0)
            {
              regerror(return_value, &(searchVarTable[rnum].preg_lb), err_msg, 1000);
              print_core_events((char*)__FUNCTION__, __FILE__,"%s regcomp failed:%s", msg, err_msg);
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000022]: ", CAV_ERR_1000022 + CAV_ERR_HDR_LEN, err_msg);
            }
          }          
        }           
      }

#if 0 //not yet implemented
       else if (!strncasecmp(line_ptr, "relFrameId=", strlen("relFrameId="))) {
        line_ptr += strlen("relFrameId=");
        for (i = 0; *line_ptr != '"'; line_ptr++, i++) {
          searchvar_buf[i] = *line_ptr;
        }
        if (!strncasecmp(searchvar_buf, "ALL", strlen("ALL")))
          searchVarTable[rnum].relframeid = -1;
        else
          searchVarTable[rnum].relframeid = atoi(searchvar_buf);
       }
#endif
      else if(!strcasecmp(key, "EncodeMode")){
          set_buffer_for_encodeMode(value, rnum, msg, &encode_flag_specified);
      }else if (!strncasecmp(key, "CharstoEncode", strlen("CharstoEncode"))) {
         set_char_to_encode_buf(value, char_to_encode_buf, rnum, msg, encode_flag_specified, &encode_chars_done);
      }else if (!strncasecmp(key, "EncodeSpaceBy", strlen("EncodeSpaceBy"))) {
         set_search_encodeSpaceBy(value, EncodeSpaceBy, key, msg);
      }else if (!strcasecmp(key, "ORD")) {
          strcpy(searchvar_buf, value);
          if(!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "ORD" ,"Search");
          }

          /*Manish: Fixed Bug3495 */
          if(!strcmp(value, "0"))
          {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012041_ID, CAV_ERR_1012041_MSG, "Search");
          }

          if (!strcasecmp(searchvar_buf, "ALL"))
            searchVarTable[rnum].ord = ORD_ALL;
          else if (!strcasecmp(searchvar_buf, "ANY")){
            searchVarTable[rnum].ord = ORD_ANY;
          }
          else if (!strcasecmp(searchvar_buf, "ANY_NONEMPTY")){
            searchVarTable[rnum].ord = ORD_ANY_NON_EMPTY;
          }
          else if (!strcasecmp(searchvar_buf, "LAST")){
            searchVarTable[rnum].ord = ORD_LAST;
          }
           else {
            for (i = 0; searchvar_buf[i]; i++) {
              if (!isdigit(searchvar_buf[i])) {
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012136_ID, CAV_ERR_1012136_MSG ,"ALL, ANY, ANY_NONEMPTY, LAST", "Search");
              }
            }
            searchVarTable[rnum].ord = atoi(searchvar_buf);
          }
      } else if (!strcasecmp(key, "Search")) {
          if (!strcasecmp(value, "BODY")) {
            searchVarTable[rnum].search = SEARCH_BODY;
          }
          else if (!strcasecmp(value, "VARIABLE")) {
            searchVarTable[rnum].search = SEARCH_VARIABLE;
             } else if(! strcasecmp(value, "HEADER")) {
            searchVarTable[rnum].search = SEARCH_HEADER;
          } else if(!strcasecmp(value, "ALL")) {
             searchVarTable[rnum].search = SEARCH_ALL;
          }
/* Not implemented
        else if (!strncasecmp(line_ptr, "HEADERS", strlen("HEADERS"))) {
          searchVarTable[rnum].search = SEARCH_HEADERS;
          line_ptr += strlen("HEADERS");
        } else if (!strncasecmp(line_ptr, "ALL", strlen("ALL"))) {
          searchVarTable[rnum].search = SEARCH_ALL;
          line_ptr += strlen("ALL");
        }
*/
      } else if (!strcasecmp(key, "SaveOffset")) {
          strcpy(searchvar_buf, value);
          for (i = 0; searchvar_buf[i]; i++) {
            if (!isdigit(searchvar_buf[i])) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, key);
            }
          }

          if(!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "SaveOffset", "Search");
          }

          searchVarTable[rnum].saveoffset = atoi(searchvar_buf);
      } else if (!strcasecmp(key, "SaveLen")) {
          strcpy(searchvar_buf, value);
          for (i = 0; searchvar_buf[i]; i++) {
            if (!isdigit(searchvar_buf[i])) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, key);       
            }
          }

          if(!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Search");
          }

          searchVarTable[rnum].savelen = atoi(searchvar_buf);
      } else if (!strcasecmp(key, "VAR")) {
          strcpy(searchvar_buf, value);
          if(!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Search");
          }

          MY_MALLOC(searchVarTable[rnum].search_in_var_name, strlen(searchvar_buf) + 1, "search_in_var_name", -1 );
          strcpy(searchVarTable[rnum].search_in_var_name, searchvar_buf);
          if(!check_var_for_search_in_var(rnum, searchvar_buf)){
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012046_ID, CAV_ERR_1012046_MSG, searchvar_buf, RETRIEVE_BUFFER_DATA(searchVarTable[rnum].name));
          }
      } else if (!strcasecmp(key, "PAGE")) {
          strcpy(page_name, value);
          if(!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Search");
          }
          //Manish: When PAGE = ALL
          //Note here we are * sign insted of word ALL because it may be possible that one can put their page name ALL  
          if (strcmp(page_name, "*")) {
	    create_searchpage_table_entry(&varpage_idx);

	    searchPageTable[varpage_idx].searchvar_idx = rnum;
	    if ((searchPageTable[varpage_idx].page_name = copy_into_temp_buf(page_name, 0)) == -1) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL,  "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, page_name);
	    }
	    NSDL2_VARS(NULL, NULL, "varpage_idx = %d", varpage_idx);
	    if (gSessionTable[sess_idx].searchpagevar_start_idx == -1) {
	      gSessionTable[sess_idx].searchpagevar_start_idx = varpage_idx;
	      gSessionTable[sess_idx].num_searchpagevar_entries = 0;
	    }

	    gSessionTable[sess_idx].num_searchpagevar_entries++;
	    searchPageTable[varpage_idx].page_idx = -1;
            searchPageTable[varpage_idx].sess_idx = sess_idx; //Manish
	    do_page=1;
	    NSDL3_VARS(NULL, NULL, "page_name = %d, searchvar_idx = %d, searchpagevar_start_idx = %d", searchPageTable[varpage_idx].page_name, rnum, gSessionTable[sess_idx].searchpagevar_start_idx);
          } else { //PAGE= * case
              searchVarTable[rnum].pgall = 1;
          }
          if ((do_page == 1) && (searchVarTable[rnum].pgall == 1)) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012050_ID, CAV_ERR_1012050_MSG, key);
          }
      } else if (!strcasecmp(key, "RetainPreValue")) {
          NSDL2_VARS(NULL, NULL, "RetainPreValue  = [%s]", value);
          if (!strcasecmp(value, "Yes")) {
            searchVarTable[rnum].retain_pre_value = RETAIN_PRE_VALUE;
          } else if (!strcasecmp(value, "No")) {
            searchVarTable[rnum].retain_pre_value = NOT_RETAIN_PRE_VALUE;
          } else {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "Search");
        }
     } else {
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, key, "Search");
      }
    }
    //Manish: search variable is mandatory
    if(state == NS_ST_SE_NAME)
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Search");
    }
  }

  if(searchVarTable[rnum].search_flag == VAR_REGEX)
    searchVarTable[rnum].lbmatch = loc_reggroup;
 
  if ((searchVarTable[rnum].encode_space_by = copy_into_big_buf(EncodeSpaceBy, 0)) == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL,  "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, EncodeSpaceBy);
  } 

  // In case we have given convert method TextToHtml, TextToURL, URLToHTML, HtmlToURL we are setting EncodeMode as None to prevent encoding of special     characters found in encoded value

  if(searchVarTable[rnum].convert == VAR_CONVERT_TEXT_TO_URL || searchVarTable[rnum].convert == VAR_CONVERT_TEXT_TO_HTML){
   searchVarTable[rnum].encode_type = ENCODE_NONE; 
  }

  //NSDL3_VARS(NULL, NULL, "Search Var Data dump: parameter name = [%s], lb = [%s], rb = [%s], saveoffset = [%d], savelen = [%d], ActionOnNotFound = [%d], ord = [%d]", RETRIEVE_BUFFER_DATA(searchVarTable[rnum].name), RETRIEVE_BUFFER_DATA(searchVarTable[rnum].lb), RETRIEVE_BUFFER_DATA(searchVarTable[rnum].rb), searchVarTable[rnum].saveoffset, searchVarTable[rnum].savelen, searchVarTable[rnum].action_on_notfound, searchVarTable[rnum].ord, searchVarTable[rnum].retain_pre_value);

  if ((!do_page) && (!searchVarTable[rnum].pgall)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "PAGE", "Search");
  }

  if ((((!lb_done) || (!rb_done)) && (searchVarTable[rnum].ord != 1)) && (!re_done)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012054_ID, CAV_ERR_1012054_MSG);
  }

  if ((searchVarTable[rnum].search == SEARCH_VARIABLE) && (searchVarTable[rnum].search_in_var_name == NULL)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "VAR", "Search");
  }

  if ((searchVarTable[rnum].search != SEARCH_VARIABLE) && (searchVarTable[rnum].search_in_var_name != NULL)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012052_ID, CAV_ERR_1012052_ID);
  }

  if((encode_chars_done == 0) && (encode_flag_specified == 1))
  {
    searchVarTable[rnum].encode_chars[' '] = 1;
    searchVarTable[rnum].encode_chars[39] = 1; //Setting for (') as it was givin error on compilation 
    searchVarTable[rnum].encode_chars[34] = 1; //Setting for (") as it was givin error on compilation 
    searchVarTable[rnum].encode_chars['<'] = 1;
    searchVarTable[rnum].encode_chars['>'] = 1;
    searchVarTable[rnum].encode_chars['#'] = 1;
    searchVarTable[rnum].encode_chars['%'] = 1;
    searchVarTable[rnum].encode_chars['{'] = 1;
    searchVarTable[rnum].encode_chars['}'] = 1;
    searchVarTable[rnum].encode_chars['|'] = 1;
    searchVarTable[rnum].encode_chars['\\'] = 1;
    searchVarTable[rnum].encode_chars['^'] = 1;
    searchVarTable[rnum].encode_chars['~'] = 1;
    searchVarTable[rnum].encode_chars['['] = 1;
    searchVarTable[rnum].encode_chars[']'] = 1;
    searchVarTable[rnum].encode_chars['`'] = 1;
  }


  return 0;
#undef NS_ST_SE_NAME
#undef NS_ST_SE_OPTIONS
}

PerPageSerVarTableEntry_Shr *copy_searchvar_into_shared_mem(void)
{
  void *searchvar_perpage_table_shr_mem;
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. total_searchvar_entries = %d, total_perpageservar_entries = %d", total_searchvar_entries, total_perpageservar_entries);
  /* insert the SearchVarTableEntry_Shr and the PerPageSerVarTableEntry_shr */
  
  if (total_searchvar_entries + total_perpageservar_entries) {
    searchvar_perpage_table_shr_mem = do_shmget(total_searchvar_entries * sizeof(SearchVarTableEntry_Shr) +
						total_perpageservar_entries * sizeof(PerPageSerVarTableEntry_Shr), "search var tables");
    searchvar_table_shr_mem = searchvar_perpage_table_shr_mem;

    for (i = 0; i < total_searchvar_entries; i++) {
      searchvar_table_shr_mem[i].var_name = BIG_BUF_MEMORY_CONVERSION(searchVarTable[i].name);
      searchvar_table_shr_mem[i].action_on_notfound = searchVarTable[i].action_on_notfound;
      searchvar_table_shr_mem[i].convert = searchVarTable[i].convert;

      memcpy(searchvar_table_shr_mem[i].encode_chars, searchVarTable[i].encode_chars, TOTAL_CHARS);
      searchvar_table_shr_mem[i].encode_type = searchVarTable[i].encode_type;
      searchvar_table_shr_mem[i].encode_space_by = BIG_BUF_MEMORY_CONVERSION(searchVarTable[i].encode_space_by);

      searchvar_table_shr_mem[i].searchvar_rdepth_bitmask = searchVarTable[i].searchvar_rdepth_bitmask;
      if (searchVarTable[i].lb == -1)
	searchvar_table_shr_mem[i].lb = NULL;
      else
        searchvar_table_shr_mem[i].lb = BIG_BUF_MEMORY_CONVERSION(searchVarTable[i].lb);
      if (searchVarTable[i].rb == -1 )
	searchvar_table_shr_mem[i].rb = NULL;
      else
          searchvar_table_shr_mem[i].rb = BIG_BUF_MEMORY_CONVERSION(searchVarTable[i].rb);
 
      searchvar_table_shr_mem[i].ord = searchVarTable[i].ord;
      searchvar_table_shr_mem[i].lbmatch = searchVarTable[i].lbmatch;
     /* Not yet implemented */
     // searchvar_table_shr_mem[i].relframeid = searchVarTable[i].relframeid;
      searchvar_table_shr_mem[i].search = searchVarTable[i].search;
      if (searchVarTable[i].search_in_var_name) {
	searchvar_table_shr_mem[i].search_in_var_hash_code = gSessionTable[searchVarTable[i].sess_idx].var_hash_func(searchVarTable[i].search_in_var_name, strlen(searchVarTable[i].search_in_var_name));
	if (searchvar_table_shr_mem[i].search_in_var_hash_code == -1) {
	  NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "Var name %s used for search in search variable name %s is not a valid varable. Exiting..\n", searchVarTable[i].search_in_var_name, BIG_BUF_MEMORY_CONVERSION(searchVarTable[i].name));
	  //fprintf(stderr, "Var name %s used for search in search variable name %s is not a valid varable. Exiting..\n", searchVarTable[i].search_in_var_name, BIG_BUF_MEMORY_CONVERSION(searchVarTable[i].name));
	  NS_EXIT(-1, CAV_ERR_1031039, searchVarTable[i].search_in_var_name, BIG_BUF_MEMORY_CONVERSION(searchVarTable[i].name));
	}
	//FREE_AND_MAKE_NOT_NULL(searchVarTable[i].search_in_var_name, "searchVarTable[i].search_in_var_name", -1);
	FREE_AND_MAKE_NOT_NULL_EX(searchVarTable[i].search_in_var_name, strlen(searchVarTable[i].search_in_var_name), "searchVarTable[i].search_in_var_name", -1);
      }
      searchvar_table_shr_mem[i].saveoffset = searchVarTable[i].saveoffset;
      searchvar_table_shr_mem[i].savelen = searchVarTable[i].savelen;
      searchvar_table_shr_mem[i].retain_pre_value = searchVarTable[i].retain_pre_value;
      searchvar_table_shr_mem[i].hash_idx = gSessionTable[searchVarTable[i].sess_idx].var_hash_func(searchvar_table_shr_mem[i].var_name, strlen(searchvar_table_shr_mem[i].var_name));
      searchvar_table_shr_mem[i].search_flag = searchVarTable[i].search_flag;
      searchvar_table_shr_mem[i].preg_lb = searchVarTable[i].preg_lb;
      searchvar_table_shr_mem[i].preg_rb = searchVarTable[i].preg_rb;
      searchvar_table_shr_mem[i].lb_rb_type = searchVarTable[i].lb_rb_type;
      assert (searchvar_table_shr_mem[i].hash_idx != -1);
    }

    perpageservar_table_shr_mem = searchvar_perpage_table_shr_mem + (total_searchvar_entries * sizeof(SearchVarTableEntry_Shr));
    for (i = 0; i < total_perpageservar_entries; i++) {
      perpageservar_table_shr_mem[i].searchvar_ptr = searchvar_table_shr_mem + perPageSerVarTable[i].searchvar_idx;
    }
  }
  return perpageservar_table_shr_mem; 
}

static int searchpage_cmp(const void* ent1, const void* ent2) {
  NSDL1_VARS(NULL, NULL, "Method called.");
  if (((SearchPageTableEntry *)ent1)->page_idx < ((SearchPageTableEntry *)ent2)->page_idx)
    return -1;
  else if (((SearchPageTableEntry *)ent1)->page_idx > ((SearchPageTableEntry *)ent2)->page_idx)
    return 1;
  else if (((SearchPageTableEntry *)ent1)->searchvar_idx < ((SearchPageTableEntry *)ent2)->searchvar_idx)
    return -1;
  else if (((SearchPageTableEntry *)ent1)->searchvar_idx > ((SearchPageTableEntry *)ent2)->searchvar_idx)
    return 1;
  else return 0;
}

int process_searchvar_table(void) {

  int i,j,k;
  int page_idx;
  int chkpage_idx;
  int last_searchvar_idx;
  int rnum; 
 
  NSDL1_VARS(NULL, NULL, "Method called. total_sess_entries = %d", total_sess_entries);

  //Create page searchvar table  based on PAGEALL searchvar entried. could not have been done earlier as the pages were not parsed before
  for (i = 0; i < total_sess_entries; i++) {
    NSDL2_VARS(NULL, NULL, "Case: PAGE = *: i(sess_idx) = %d, sess_name = %s, searchvar_start_idx = %d, num_searchvar_entries = %d ",
                     i, RETRIEVE_BUFFER_DATA(gSessionTable[i].sess_name),
                     gSessionTable[i].searchvar_start_idx, gSessionTable[i].num_searchvar_entries);
    for (j = gSessionTable[i].searchvar_start_idx; j < (gSessionTable[i].searchvar_start_idx + gSessionTable[i].num_searchvar_entries); j++) {
      if (searchVarTable[j].pgall) {
        NSDL2_VARS(NULL, NULL, "j(searchvar_idx) = %d, first_page = %d, num_pages = %d", j, gSessionTable[i].first_page, gSessionTable[i].num_pages);
	for (k = gSessionTable[i].first_page; k < (gSessionTable[i].first_page + gSessionTable[i].num_pages); k++) {
          NSDL2_VARS(NULL, NULL, "k(page_idx) = %d", k);
	  if (create_searchpage_table_entry(&chkpage_idx) == -1) {
	    fprintf(stderr, "Could not create create_serachpage_table_entry. Short on memory\n");
            write_log_file(NS_SCENARIO_PARSING, "Failed to create search page table entry");
	    return -1;
	  }
	  NSDL2_VARS(NULL, NULL, "chkpage_idx = %d", chkpage_idx);
	  searchPageTable[chkpage_idx].searchvar_idx = j;

	  if (gSessionTable[i].searchpagevar_start_idx == -1) {
	    gSessionTable[i].searchpagevar_start_idx = chkpage_idx;
	    gSessionTable[i].num_searchpagevar_entries = 0;
	  }

	  gSessionTable[i].num_searchpagevar_entries++;
	  searchPageTable[chkpage_idx].page_idx = k;
          searchPageTable[chkpage_idx].sess_idx = i; 

          /* We also need to propogate redirection_depth_bitmask to page lvl for PAGE=* */
          if(searchVarTable[j].searchvar_rdepth_bitmask != VAR_IGNORE_REDIRECTION_DEPTH) {
            gPageTable[k].redirection_depth_bitmask = 
              set_depth_bitmask(gPageTable[k].redirection_depth_bitmask, 
                                searchVarTable[j].searchvar_rdepth_bitmask);
          }
 
	  NSDL2_VARS(NULL, NULL, "searchpagevar_start_idx = %d, num_searchpagevar_entries = %d, searchvar_idx = %d, page_idx = %d", gSessionTable[i].searchpagevar_start_idx, gSessionTable[i].num_searchpagevar_entries, j, k);
	}
      }
    }
  }

  /*Added By Manish Mishra:
    Till now we have made a complete searchPageTable but not assigned page_idx to all the pages.
    case1: 
      page_idx = -1: mean this page belongs to those searchVar which has to be searched on this particular page 
      so we have to assign page_idx here
    case2:
      page_idx != -1: mean this page belongs to those searchVar which has to be searched on all page
      this case has been handle in above function
  */
  NSDL3_VARS(NULL, NULL, "total_searchpage_entries = %d", total_searchpage_entries);
  int srchPageTbl_idx = 0;
  for (srchPageTbl_idx = 0; srchPageTbl_idx < total_searchpage_entries; srchPageTbl_idx++) {
    NSDL3_VARS(NULL, NULL, "Case: PAGE = particular: srchPageTbl_idx = %d", srchPageTbl_idx);
    if (searchPageTable[srchPageTbl_idx].page_idx == -1) {
      if ((searchPageTable[srchPageTbl_idx].page_idx = find_page_idx(RETRIEVE_TEMP_BUFFER_DATA(searchPageTable[srchPageTbl_idx].page_name), searchPageTable[srchPageTbl_idx].sess_idx)) == -1) {
        NSDL3_VARS(NULL, NULL, "srchPageTbl_idx = %d, unknown page name = %s", srchPageTbl_idx, RETRIEVE_TEMP_BUFFER_DATA(searchPageTable[srchPageTbl_idx].page_name));
                      //Previously taking with only script name
                      //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[searchPageTable[srchPageTbl_idx].sess_idx].sess_name)));
        //Manish: Fix Bug3448
        NS_EXIT(-1, CAV_ERR_1031030, RETRIEVE_TEMP_BUFFER_DATA(searchPageTable[srchPageTbl_idx].page_name), "Search", RETRIEVE_BUFFER_DATA(searchVarTable[searchPageTable[srchPageTbl_idx].searchvar_idx].name), get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[searchPageTable[srchPageTbl_idx].sess_idx].sess_name), searchPageTable[srchPageTbl_idx].sess_idx, "/")); 
      }
    }
  }

  assert (srchPageTbl_idx == total_searchpage_entries);

  //sort  search Page table in the order of page index & search var index
  qsort(searchPageTable, total_searchpage_entries, sizeof(SearchPageTableEntry), searchpage_cmp);

  //Create per page search var table
  //This table has the list of search vars that need to be initialized on specific pages
  //Each page has the start & num entries for this per page search var table entries.
  for (i = 0; i < total_searchpage_entries; i++) {
    if ((page_idx = searchPageTable[i].page_idx) != -1) {
      if (gPageTable[page_idx].first_searchvar_idx == -1) {
	if (create_perpageservar_table_entry(&rnum) != SUCCESS) {
	  fprintf(stderr, "Unable to create perpage table entry\n");
	  return -1;
        }
	perPageSerVarTable[rnum].searchvar_idx = searchPageTable[i].searchvar_idx;
	gPageTable[page_idx].first_searchvar_idx = rnum;
	last_searchvar_idx = searchPageTable[i].searchvar_idx;
      } else {
	//Remove duplicate entries for same var for same page
 	//Note that searchPage Table is ordered in page number order
	if (last_searchvar_idx == searchPageTable[i].searchvar_idx)
	  continue;
	if (create_perpageservar_table_entry(&rnum) != SUCCESS) {
	  fprintf(stderr, "Unable to create perpage table entry\n");
	  return -1;
        }
	perPageSerVarTable[rnum].searchvar_idx = searchPageTable[i].searchvar_idx;
	last_searchvar_idx = searchPageTable[i].searchvar_idx;
      }
      /* Set the per page depth bit if RedirectionDepth=1,2,3,..n and
      *  in case of RedirectionDepth=ALL set all the bits so that
      *  copy_retrieve_data() will be called.
      *  Do not set any bits for the RedirectionDepth=Last
      */
      if(searchVarTable[searchPageTable[i].searchvar_idx].searchvar_rdepth_bitmask != VAR_IGNORE_REDIRECTION_DEPTH)
      {
        NSDL1_VARS(NULL, NULL, "Before setting the redirection_depth_bitmask value is = 0x%x, searchVarTable idx = %d", gPageTable[page_idx].redirection_depth_bitmask, searchPageTable[i].searchvar_idx);
        gPageTable[page_idx].redirection_depth_bitmask = set_depth_bitmask(gPageTable[page_idx].redirection_depth_bitmask, searchVarTable[searchPageTable[i].searchvar_idx].searchvar_rdepth_bitmask);
      
        NSDL1_VARS(NULL, NULL, "After setting the redirection_depth_bitmask value is = 0x%x searchVarTable idx = %d", gPageTable[page_idx].redirection_depth_bitmask, searchPageTable[i].searchvar_idx);
      }

      if((requests[gPageTable[page_idx].first_eurl].request_type == SOCKET_REQUEST) ||
         (requests[gPageTable[page_idx].first_eurl].request_type == SSL_SOCKET_REQUEST))
      {
        NSTL1(NULL, NULL, "Since there is no header concept in SOCKET API, hence setting to search in Body for page[%s]",
              RETRIEVE_TEMP_BUFFER_DATA(searchPageTable[i].page_name));

        gPageTable[page_idx].save_headers |= SEARCH_IN_RESP;
        if(searchVarTable[searchPageTable[i].searchvar_idx].search != SEARCH_VARIABLE)
          searchVarTable[searchPageTable[i].searchvar_idx].search = SEARCH_BODY;
      }
      else
      {
        if(searchVarTable[searchPageTable[i].searchvar_idx].search == SEARCH_ALL){
          gPageTable[page_idx].save_headers |= SEARCH_IN_RESP;
          gPageTable[page_idx].save_headers |= SEARCH_IN_HEADER;
        }else if(searchVarTable[searchPageTable[i].searchvar_idx].search == SEARCH_HEADER){
          gPageTable[page_idx].save_headers |= SEARCH_IN_HEADER;
        }else{
          gPageTable[page_idx].save_headers |= SEARCH_IN_RESP;
        }
      }
      gPageTable[page_idx].num_searchvar++;
    }
  }

  return 0;
}

int find_searchvar_idx(char* name, int len, int sess_idx) {
  int i;
  if(!len)
    len = strlen(name);
  char save_chr = name[len];
  name[len] = 0;

  NSDL1_VARS(NULL, NULL, "Method called. name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].searchvar_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].searchvar_start_idx; i < gSessionTable[sess_idx].searchvar_start_idx + gSessionTable[sess_idx].num_searchvar_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(searchVarTable[i].name), name)){
       name[len] = save_chr;
      return i;
    }
  }
  name[len] = save_chr;
  return -1;
}

//TODO add for RedirectionDepth
/* Start: Code for run time
*  Common function to print the correct debugging information
*/

static inline
char *search_var_to_str(PerPageSerVarTableEntry_Shr* perpageservar_ptr, char *buf)
{
  char ord_buf[10];
  char action_on_notfound_buf[20];
  char convert_buf[20];
  int buf_len;

  if(perpageservar_ptr->searchvar_ptr->ord == ORD_ANY) 
    strcpy(ord_buf, "ANY");
  else if(perpageservar_ptr->searchvar_ptr->ord == ORD_ALL)
    strcpy(ord_buf, "ALL");
  else if(perpageservar_ptr->searchvar_ptr->ord == ORD_LAST)
    strcpy(ord_buf, "LAST");
   else 
    sprintf(ord_buf, "%d", perpageservar_ptr->searchvar_ptr->ord);

  if(perpageservar_ptr->searchvar_ptr->action_on_notfound == VAR_NOTFOUND_ERROR)
    strcpy(action_on_notfound_buf, "Error");
  else if(perpageservar_ptr->searchvar_ptr->action_on_notfound == VAR_NOTFOUND_WARNING)
    strcpy(action_on_notfound_buf, "Warning");
  else
    strcpy(action_on_notfound_buf, "Ignore");
 
  if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_TEXT_TO_URL)
    strcpy(convert_buf, "TextToUrl");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_URL_TO_TEXT)
    strcpy(convert_buf, "UrlToText");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_TEXT_TO_HTML)
    strcpy(convert_buf, "TextToHtml");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_HTML_TO_TEXT)
    strcpy(convert_buf, "HtmlToText");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_HTML_TO_URL)
    strcpy(convert_buf, "TextToUrl");
    //strcpy(convert_buf, "HtmlToUrl");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_URL_TO_HTML)
    strcpy(convert_buf, "UrlToText");
    //strcpy(convert_buf, "UrlToHtml");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64)
    strcpy(convert_buf, "TextToBase64");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT)
    strcpy(convert_buf, "TextToBase64");
  else if(perpageservar_ptr->searchvar_ptr->convert == VAR_CONVERT_TEXT_TO_HEX)
    strcpy(convert_buf, "TextToHex");
  else
    strcpy(convert_buf, "NoConversion");

  buf_len = snprintf(buf, 1024, "Search Parameter (Parameter Name = %s; LB = %s; RB = %s; ORD = %s; "
	       "ActionOnNotFound = %s; Convert = %s; SaveLen = %d; "
	       "SaveOffset = %d; Depth = %d)",
	       perpageservar_ptr->searchvar_ptr->var_name, perpageservar_ptr->searchvar_ptr->lb,
	       perpageservar_ptr->searchvar_ptr->rb, ord_buf,
	       action_on_notfound_buf, convert_buf,
	       perpageservar_ptr->searchvar_ptr->savelen,
	       perpageservar_ptr->searchvar_ptr->saveoffset,
	       perpageservar_ptr->searchvar_ptr->searchvar_rdepth_bitmask);

  buf[buf_len] = '\0';
  return buf;
}

/* This function is used to set the to_stop flag */
static inline void
search_var_check_status(connection *cptr, VUser *vptr, PerPageSerVarTableEntry_Shr* perpageservar_ptr, int string_found_flag, int *page_fail, int *to_stop)
{
  char tmp_buf[1024];
  /* If this search var is failing and action_on_notfound is equal to Error, then set to_stop to 1
  *  Note - page_fail, serach_var_fail and to_stop should be set one time
  *  Once set, this should remain set
  */
  if(!string_found_flag) 
  {
    search_var_to_str(perpageservar_ptr, tmp_buf);

    // Show in Debug trace and debug log 
    // before 3.8.1, if LB and RB given but value is NULL then it was showing 
    // search variable not found. Added condition for that
    if(perpageservar_ptr->searchvar_ptr->search_flag == VAR_REGEX)
    {
      if(perpageservar_ptr->searchvar_ptr->lb) {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Search variable found but has NULL value %s", tmp_buf);
      } else {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Search variable not found for %s", tmp_buf);
      } 
    }
    else
    {
      if(perpageservar_ptr->searchvar_ptr->lb && perpageservar_ptr->searchvar_ptr->rb) {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Search variable found but has NULL value %s", tmp_buf);
      } else {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Search variable not found for %s", tmp_buf);
      }
    }

    if(perpageservar_ptr->searchvar_ptr->action_on_notfound == VAR_NOTFOUND_ERROR) 
    {
      /*In Error case we have to fail the page.
      * In Warning case we just write to the event.log file only.
      */ 
      NS_EL_4_ATTR(EID_HTTP_PAGE_ERR_START + NS_REQUEST_CV_FAILURE, vptr->user_index,
                                 vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
				 vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
				 tmp_buf, 
                                 "Page failed with status %s due to search parameter (%s) not found in response.",
				 get_error_code_name(NS_REQUEST_CV_FAILURE),
				 perpageservar_ptr->searchvar_ptr->var_name);
      *page_fail = 1;
      *to_stop =  1;
    } else if(perpageservar_ptr->searchvar_ptr->action_on_notfound == VAR_NOTFOUND_WARNING) {
      NS_EL_4_ATTR(EID_HTTP_PAGE_ERR_START + NS_REQUEST_CV_FAILURE, vptr->user_index,
                                 vptr->sess_inst, EVENT_CORE, EVENT_WARNING,
				 vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
				 tmp_buf, 
                                 "Search parameter (%s) not found in response.",
				 perpageservar_ptr->searchvar_ptr->var_name);
    } 
    // Nothing to do for ignore
  }
  return;
}


void
process_search_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int blen, int present_depth)
{
  PerPageSerVarTableEntry_Shr* perpageservar_ptr;
  //int blen = vptr->bytes;(13/sep/2013) removed now it will be passed from do_data_processing
  char* buf_ptr;
  int count, i;
  int actual_ord, count_ord;
  char *end_ptr; //points to end of buffer 
  int to_stop, page_fail; // Once set, this should remain set
  to_stop = page_fail = 0;
  int string_found_flag = 0;
  int buf_len = 0;  // This will be used for memmem in case of binary data search

  /* parameterization of lb/rb variables*/
  char *rb_param;
  char *lb_param;
  long len = 0;

  /* added for common code (NS & NO) */
  UserVarEntry *uservartable_entry; 
  int user_var_idx;
  static char *error_var = "";
  userTraceData* utd_node = NULL;
#ifdef NS_DEBUG_ON
  char tmp_buf[1024 + 1]; //for search_var_to_str()
#endif

  NSDL2_VARS(vptr, NULL, "first_searchvar_ptr = %p, page_name = %s", vptr->cur_page->first_searchvar_ptr, vptr->cur_page->page_name);

  if (!(vptr->cur_page->first_searchvar_ptr)) /* No SearchVar is defined */
     return;

  NSDL2_VARS(vptr, NULL, "Method called. full_buffer = %s, present_depth = %d", full_buffer, present_depth);
  
  for (perpageservar_ptr = vptr->cur_page->first_searchvar_ptr, count = 0; count < vptr->cur_page->num_searchvar; count++, perpageservar_ptr++) 
  {
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Checking search parameter for %s", search_var_to_str(perpageservar_ptr, tmp_buf));
    rb_param = NULL;
    lb_param = NULL;
    /*Apply search if  
    *    depth is set 1,2,..n  
    *    OR 
    *    depth = ALL[-2] and is not LAST[-1]
    * special case: if depth=1 or 2 or 3 ... AND this is last depth then apply search var on it.
    * depth = -1 for ReDirectionDepth=LAST will be passed from do_data_processing()
    *  [called from handle_page_complete()].
    * for RedirectionDepth=LAST, searchvar is only applied if present_depth = -1
    */ 
    if(((present_depth > 0) && ((perpageservar_ptr->searchvar_ptr->searchvar_rdepth_bitmask) & (1 << (present_depth -1)))) ||
         ((perpageservar_ptr->searchvar_ptr->searchvar_rdepth_bitmask == VAR_ALL_REDIRECTION_DEPTH) && (present_depth != VAR_IGNORE_REDIRECTION_DEPTH)) || 
         ((present_depth == VAR_IGNORE_REDIRECTION_DEPTH) && (perpageservar_ptr->searchvar_ptr->searchvar_rdepth_bitmask == present_depth))
     ) 
    {
      NSDL2_VARS(vptr, NULL, "present depth = %d for %s", present_depth, search_var_to_str(perpageservar_ptr, tmp_buf));
      if (perpageservar_ptr->searchvar_ptr->search == SEARCH_BODY) {
        /* If search = ALL and SEARCH=BODY is applied on same page then pointing buf_ptr to response body, as full_buffer 
          contains header as well as body */
        if (vptr->cur_page->save_headers & SEARCH_IN_HEADER){
          buf_ptr = full_buffer + vptr->response_hdr->used_hdr_buf_len;
          buf_len = blen - vptr->response_hdr->used_hdr_buf_len;
        }
        else {
          buf_ptr = full_buffer;
          buf_len = blen;
        }
        end_ptr = full_buffer + blen;
      } else if(perpageservar_ptr->searchvar_ptr->search == SEARCH_HEADER) {
        buf_ptr = vptr->response_hdr->hdr_buffer;
        end_ptr = vptr->response_hdr->hdr_buffer + vptr->response_hdr->used_hdr_buf_len;
        buf_len = vptr->response_hdr->used_hdr_buf_len;
      } else if(perpageservar_ptr->searchvar_ptr->search == SEARCH_ALL) {
         buf_ptr = full_buffer;
         end_ptr = buf_ptr + blen;
         buf_len = blen;
      } else {
        // For search in a variable, set buf_ptr and end_ptr to var value
        // var is assumed to be NULL terminated as it may be logged in debug log
	int uservar_idx = vptr->sess_ptr->vars_trans_table_shr_mem[perpageservar_ptr->searchvar_ptr->search_in_var_hash_code].user_var_table_idx;
	/* should we memcpy ?? */
	buf_ptr = vptr->uvtable[uservar_idx].value.value;
	end_ptr = buf_ptr + vptr->uvtable[uservar_idx].length;
        buf_len = vptr->uvtable[uservar_idx].length;
      }

      /* For ORD ANY, we need to find all occurence and then randomly pick one
      *  This code will find the count of all occurence and then set cur_ord with random number
      */
 
      /* current user variable index */
      user_var_idx = vptr->sess_ptr->vars_trans_table_shr_mem[perpageservar_ptr->searchvar_ptr->hash_idx].user_var_table_idx;
      uservartable_entry = &vptr->uvtable[user_var_idx];

      /* parametization support for LB and RB in search parameter*/
      if(perpageservar_ptr->searchvar_ptr->lb_rb_type == NS_SEARCH_VAR_LB_RB)
      {
        NSDL2_VARS(vptr, NULL, "evaluating parameter type = %d", perpageservar_ptr->searchvar_ptr->lb_rb_type);
        char *tmp_ptr = NULL;
        tmp_ptr = ns_eval_string_flag_internal(perpageservar_ptr->searchvar_ptr->lb, 0, &len, vptr);
        if(!len)
        {
          NSDL3_VARS(vptr, NULL, "left bound doesn't contain any value hence returing");
          return;
        }
        if( len > ns_nvm_scratch_buf_size)
        {
          MY_REALLOC(ns_nvm_scratch_buf, len + 1, "reallocating for lower bound search parameter", -1);
          ns_nvm_scratch_buf_size = len;
        }
        strcpy(ns_nvm_scratch_buf, tmp_ptr);
        lb_param = ns_nvm_scratch_buf;
        NSDL2_VARS(vptr, NULL, "left bound parameter value = %s", ns_nvm_scratch_buf);
 
        rb_param = ns_eval_string_flag_internal(perpageservar_ptr->searchvar_ptr->rb, 0, &len, vptr);
        if(!len)
        {
          NSDL3_VARS(vptr, NULL, "right bound doesn't contain any value hence returing");
          return;
        }
        NSDL2_VARS(vptr, NULL, "right bound parameter value = %s", rb_param);

      }
      else if (perpageservar_ptr->searchvar_ptr->lb_rb_type == NS_SEARCH_VAR_LB) 
      {
        lb_param = ns_eval_string_flag_internal(perpageservar_ptr->searchvar_ptr->lb, 0, &len, vptr);
        rb_param = perpageservar_ptr->searchvar_ptr->rb;
        if(!len)
        {
          NSDL3_VARS(vptr, NULL, "left bound doesn't contain any value hence returing");
          return;
        }
        
        NSDL2_VARS(vptr, NULL, "left bound parameter value = %s", lb_param);
      }
      else if (perpageservar_ptr->searchvar_ptr->lb_rb_type == NS_SEARCH_VAR_RB) 
      {
        rb_param = ns_eval_string_flag_internal(perpageservar_ptr->searchvar_ptr->rb, 0, &len, vptr);
        lb_param = perpageservar_ptr->searchvar_ptr->lb;
        if(!len)
        {
          NSDL3_VARS(vptr, NULL, "right bound doesn't contain any value hence returing");
          return;
        }
        NSDL2_VARS(vptr, NULL, "right bound parameter value = %s", rb_param);
      }
      else
      {
        lb_param = perpageservar_ptr->searchvar_ptr->lb;
        rb_param = perpageservar_ptr->searchvar_ptr->rb;
      }

     // We can have either LB/RB or RE so to avoid passing new arguments and using the existing one
//     regex_t *regexptr = &perpageservar_ptr->searchvar_ptr->preg_lb;
//     if(perpageservar_ptr->searchvar_ptr->search_flag == VAR_REGEX)
//       regexptr = &perpageservar_ptr->searchvar_ptr->pregex;
     
     /* In case of ORD_ANY_NON_EMPTY we call nslb_get_search_var_value first time to save non empty string to tmpArray and then if 
      * actual ord is valid then we will save tmpArray value at that index to uservartable_entry and clear tmpArray
      * Now nslb_get_search_var_value will only be called if ord is not ORD_ANY_NON_EMPTY*/ 
     if (perpageservar_ptr->searchvar_ptr->ord == ORD_ANY || perpageservar_ptr->searchvar_ptr->ord == ORD_ANY_NON_EMPTY || perpageservar_ptr->searchvar_ptr->ord == ORD_LAST) 
     {
       // passing three more argument for regexp type lb, rb and regex_flag
       actual_ord = nslb_get_search_var_value(perpageservar_ptr->searchvar_ptr->var_name, lb_param, rb_param, perpageservar_ptr->searchvar_ptr->saveoffset, perpageservar_ptr->searchvar_ptr->savelen, buf_ptr, end_ptr, &string_found_flag, perpageservar_ptr->searchvar_ptr->ord, perpageservar_ptr->searchvar_ptr->convert, 0, uservartable_entry, perpageservar_ptr->searchvar_ptr->retain_pre_value, NULL, 0, 0, &(perpageservar_ptr->searchvar_ptr->preg_lb), &(perpageservar_ptr->searchvar_ptr->preg_rb), perpageservar_ptr->searchvar_ptr->search_flag, perpageservar_ptr->searchvar_ptr->lbmatch, buf_len);// 0 for reverse method. In 3.8.1 we are not supporting reverse method in NS 

       //Issue - If no search var found, then actual_ord will be -ve TODO
       if(actual_ord == -1) // No found. (ord starts with 1)
       {
         // Fill empty value
         NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Search variable not found, actual_ord = %d for %s", actual_ord, search_var_to_str(perpageservar_ptr, tmp_buf));
         nslb_save_search_var_value(perpageservar_ptr->searchvar_ptr->var_name, perpageservar_ptr->searchvar_ptr->convert, 0, error_var, strlen(error_var), 0, 1, actual_ord, uservartable_entry, ORD_ANY, NULL,0, 0, perpageservar_ptr->searchvar_ptr->retain_pre_value);
       }
       else if(perpageservar_ptr->searchvar_ptr->ord == ORD_ANY_NON_EMPTY) {//we are going to save value only in case of ORD_ANY_NON_EMPTY
         NSDL2_VARS(vptr, NULL, "Saving value for ORD_ANY_NON_EMPTY, value = %s, and length = %d",tempArrayVal[actual_ord - 1].value, tempArrayVal[actual_ord - 1].length);
         nslb_save_search_var_value(perpageservar_ptr->searchvar_ptr->var_name, perpageservar_ptr->searchvar_ptr->convert, 0, tempArrayVal[actual_ord - 1].value, tempArrayVal[actual_ord - 1].length, 0, 0, actual_ord, uservartable_entry, actual_ord, NULL, 0, 0, perpageservar_ptr->searchvar_ptr->retain_pre_value);
         free_tmp_arr_table();
       }
     }             
     else // ORD_ALL or particular given ord
       actual_ord = perpageservar_ptr->searchvar_ptr->ord;
 
      // Now search to get the requested ORD value or ALL values
      /* Pass the random choosen actual_ord so that ORD_ANY can be processed inside this function. 
      *  Get the updated count_ord and pass it to copy_all_search_vars() function
      */
//passing three more argument for regexp type lb, rb and regex_flag
     
     if(perpageservar_ptr->searchvar_ptr->ord != ORD_ANY_NON_EMPTY)
       count_ord = nslb_get_search_var_value(perpageservar_ptr->searchvar_ptr->var_name, lb_param, rb_param, perpageservar_ptr->searchvar_ptr->saveoffset, perpageservar_ptr->searchvar_ptr->savelen, buf_ptr, end_ptr, &string_found_flag, actual_ord, perpageservar_ptr->searchvar_ptr->convert, 0, uservartable_entry, perpageservar_ptr->searchvar_ptr->retain_pre_value, NULL, 0, 0, &(perpageservar_ptr->searchvar_ptr->preg_lb), &(perpageservar_ptr->searchvar_ptr->preg_rb), perpageservar_ptr->searchvar_ptr->search_flag, perpageservar_ptr->searchvar_ptr->lbmatch, buf_len);
     
     if ((perpageservar_ptr->searchvar_ptr->ord == ORD_ALL) && (count_ord)) // If at least one found
     { 
       nslb_copy_all_search_vars(perpageservar_ptr->searchvar_ptr->var_name, uservartable_entry, perpageservar_ptr->searchvar_ptr->convert, 0, count_ord);        
       
       for(i = 0; i < count_ord; i++)
       {
         //Cant move before for loop bcoz need to check for Debug Trace also
         if(NS_IF_TRACING_ENABLE_FOR_USER ||
           NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL) 
         {
           GET_UT_TAIL_PAGE_NODE 
           if(uservartable_entry->value.array[i].value){
             ut_add_param_node(page_node);
             ut_update_param(page_node, perpageservar_ptr->searchvar_ptr->var_name, uservartable_entry->value.array[i].value, SEARCH_VAR, i + 1, strlen(perpageservar_ptr->searchvar_ptr->var_name), uservartable_entry->value.array[i].length);           
           }
         } 
         NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Saved array value for ord = %d in user table is = %s for %s", i, uservartable_entry->value.array[i].value, search_var_to_str(perpageservar_ptr, tmp_buf));
       }
     }
     else  //Paricular or ANY case
     { 
       if(uservartable_entry->value.value) 
       {
         if(NS_IF_TRACING_ENABLE_FOR_USER ||
           NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL)
         {
           GET_UT_TAIL_PAGE_NODE 
           /*Always send ut_tail for adding node*/
           ut_add_param_node(page_node);
           ut_update_param(page_node, perpageservar_ptr->searchvar_ptr->var_name, uservartable_entry->value.value, SEARCH_VAR, count_ord, strlen(perpageservar_ptr->searchvar_ptr->var_name), uservartable_entry->length);
         }
         NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Search variable found for %s. Value Length is %d, Value = %*.*s", search_var_to_str(perpageservar_ptr, tmp_buf), uservartable_entry->length, uservartable_entry->length, uservartable_entry->length, uservartable_entry->value.value);
       }
     }
      
     //check the status and set the flags.
     search_var_check_status(cptr, vptr, perpageservar_ptr, string_found_flag, &page_fail, &to_stop); 
   } 
   else
     NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Search parameter not applicable for the current redirection depth (%d)", present_depth);
   
  } /* for all search var end */
  // If at least one search var fails, then we need to fail the page and session.
  if(page_fail) {
    set_page_status_for_registration_api(vptr, NS_REQUEST_CV_FAILURE, to_stop, "SearchVar");
  }
}
