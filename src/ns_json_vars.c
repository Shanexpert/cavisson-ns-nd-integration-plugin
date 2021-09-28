/******************************************************************
 * Name    :    ns_json_vars.c
 * Purpose :    JSON_VAR - parsing, shared memory, run time
 * Note    :    JSON Variables are initialized as a result of
                searching a response document for user specified Object_Path 
 * Syntax  :    nsl_json_var ( <JsonVarName>,  PAGE=<page1>, PAGE=<page2>, OBJECT_PATH= <Object path>,
		WHERE=<Condition to validate>, ORD=ALL, SaveOffset=<offset>, SaveLen=<length>,
		RedirectionDepth=Last, ActionOnNotFound=Error, Search=BODY, Convert=TextToUrl>,
		EncodeMode=None);
 * Intial version date:    
 * Last modification date: 
*****************************************************************/

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
#include "ns_debug_trace.h"
#include "poi.h" //for ns_get_random
#include "ns_string.h" //ns_encode_url etc
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "nslb_sock.h"
#include "ns_vuser_trace.h"
#include "ns_page_dump.h"

#include "ns_json_vars.h"
#include "nslb_util.h" //for get_tokens()
#include "nslb_log.h"
#include "nslb_uservar_table.h"
#include "nslb_search_vars.h"
#include "nslb_json_parser.h" 
#include "ns_exit.h" 
#include "ns_error_msg.h"
#include "ns_script_parse.h"

#define NS_ST_JSON_NAME 1
#define NS_ST_JSON_OPTIONS 2

#ifndef CAV_MAIN
JSONVarTableEntry *jsonVarTable;
int max_jsonvar_entries = 0;
int max_jsonpage_entries = 0;
static int total_jsonvar_entries;
static int total_perpagejsonvar_entries;
static int total_jsonpage_entries;
JSONVarTableEntry_Shr *jsonvar_table_shr_mem = NULL;
int total_json_temparrayval_entries; 
int max_json_temparrayval_entries;
ArrayValEntry* json_tempArrayVal;
#else
//__thread JSONVarTableEntry *jsonVarTable;
__thread int max_jsonvar_entries = 0;
__thread int max_jsonpage_entries = 0;
__thread int total_jsonvar_entries;
__thread int total_perpagejsonvar_entries;
__thread int total_jsonpage_entries;
__thread JSONVarTableEntry_Shr *jsonvar_table_shr_mem = NULL;
__thread int total_json_temparrayval_entries; 
__thread int max_json_temparrayval_entries;
__thread ArrayValEntry* json_tempArrayVal;
#endif


//static char json_res[1024*1024];

// These variables will be used for json variables ord any and all case

extern int loader_opcode;
/*-----------------------------------------------------------------------------------------------------------------
Function to allocate memory for jsonvar Table

------------------------------------------------------------------------------------------------------------------*/

static int create_jsonvar_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_jsonvar_entries = %d, max_jsonvar_entries = %d", total_jsonvar_entries,                    max_jsonvar_entries);

  if (total_jsonvar_entries == max_jsonvar_entries) {
    MY_REALLOC_EX(jsonVarTable, (max_jsonvar_entries + DELTA_JSONVAR_ENTRIES) * sizeof(JSONVarTableEntry),
                  max_jsonvar_entries * sizeof(JSONVarTableEntry), "jsonvar entries", -1);
    max_jsonvar_entries += DELTA_JSONVAR_ENTRIES;
  }

  *row_num = total_jsonvar_entries++;
  return (SUCCESS);
}

/*---------------------------------------------------------------------------------------------
Function to allocate memory for jsonpage table

----------------------------------------------------------------------------------------------*/

static int create_jsonpage_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_jsonpage_entries = %d, max_jsonpage_entries = %d", total_jsonpage_entries,
             max_jsonpage_entries);
  if (total_jsonpage_entries == max_jsonpage_entries) {
    MY_REALLOC_EX(jsonPageTable, (max_jsonpage_entries + DELTA_JSONPAGE_ENTRIES) * sizeof(JSONPageTableEntry),
                  max_jsonpage_entries * sizeof(JSONPageTableEntry), "jsonpage entries", -1);

    max_jsonpage_entries += DELTA_JSONPAGE_ENTRIES;
  }

  *row_num = total_jsonpage_entries++;
  return (SUCCESS);
}

/*-------------------------------------------------------------------------------------------
Function to allocate memory for parpage jsonvar Table

-------------------------------------------------------------------------------------------*/
static int create_perpagejsonvar_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_perpagejsonvar_entries = %d, max_perpagejsonvar_entries = %d",
             total_perpagejsonvar_entries, max_perpagejsonvar_entries);
  if (total_perpagejsonvar_entries == max_perpagejsonvar_entries) {
    MY_REALLOC_EX(perPageJSONVarTable, (max_perpagejsonvar_entries + DELTA_PERPAGEJSONVAR_ENTRIES) * sizeof(PerPageJSONVarTableEntry), max_perpagejsonvar_entries * sizeof(PerPageJSONVarTableEntry), "perpagejsonvar entries", -1);
    max_perpagejsonvar_entries += DELTA_PERPAGEJSONVAR_ENTRIES;
  }
  *row_num = total_perpagejsonvar_entries++;
  return (SUCCESS);
}

/*----------------------------------------------------------------------------------------
Function to support Encode mode

-----------------------------------------------------------------------------------------*/
static int set_json_encodeSpaceBy(char* value, char* EncodeSpaceBy, char *key, char *msg)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], key = [%s]", value, key);

  strcpy(EncodeSpaceBy, value);
  NSDL2_VARS(NULL, NULL, "After tokenized EncodeSpaceBy = [%s]", EncodeSpaceBy);

  if (!strcmp(EncodeSpaceBy, "+") || !strcmp(EncodeSpaceBy, "%20"))
    return 0;
  else
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012139_ID, CAV_ERR_1012139_MSG, "Json");

  return 0;
}


/*-------------------------------------------------------------------------------------
Function to Support Encode mode

--------------------------------------------------------------------------------------*/
static int set_char_to_encode_buf(char *value, char *char_to_encode_buf, int rnum, char* msg,
                                  int encode_flag_specified, char *encode_chars_done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  int i;
  if (encode_flag_specified == 0){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012137_ID, CAV_ERR_1012137_MSG, "Json");
  }

  strcpy(char_to_encode_buf, value);
  NSDL3_VARS(NULL, NULL, "After tokenized CharatoEncode = %s", char_to_encode_buf);

  /* Encode chars can have any special characters including space, single quiote, double quotes. Few examples:
   * EncodeChars=", \[]"
   * ncodeChars="~`!@#$%^&*-_+=[]{}\|;:'\" (),<>./?"
   */

  for (i = 0; char_to_encode_buf[i] != '\0'; i++) {
    if (isalnum(char_to_encode_buf[i])){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012138_ID, CAV_ERR_1012138_MSG, char_to_encode_buf, "Json");
    }

    NSDL3_VARS(NULL, NULL, "i = %d, char_to_encode_buf[i] = [%c]", i, char_to_encode_buf[i]);

    jsonVarTable[rnum].encode_chars[(int)char_to_encode_buf[i]] = 1;
  }

  *encode_chars_done = 1;

  return 0;
} 

/*----------------------------------------------------------------------------------------------
Function to Support Encode mode

----------------------------------------------------------------------------------------------*/

static int set_buffer_for_encodeMode(char *value, int rnum, char* msg, int *encode_flag_specified)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  NSDL3_VARS(NULL, NULL, "After tokenized EncodeMode Name = %s", value);
  if (!strcasecmp(value, "All")) {
    jsonVarTable[rnum].encode_type = ENCODE_ALL;
  } else if (!strcasecmp(value, "None")) {
      jsonVarTable[rnum].encode_type = ENCODE_NONE;
      memset(jsonVarTable[rnum].encode_chars, 0, TOTAL_CHARS);
  } else if (!strcasecmp(value, "Specified")) {
      *encode_flag_specified = 1;
      jsonVarTable[rnum].encode_type = ENCODE_SPECIFIED;
      memset(jsonVarTable[rnum].encode_chars, 0, TOTAL_CHARS);
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "EncodeMode", "Json");
  }
  return 0;
}  
  
/*----------------------------------------------------------------------------
Function to initialize all the variables before parsing the json var

-----------------------------------------------------------------------------*/

void init_jsonvar_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_jsonvar_entries = 0;
  total_perpagejsonvar_entries = 0;
  total_jsonpage_entries = 0;

  MY_MALLOC (jsonVarTable, INIT_JSONVAR_ENTRIES * sizeof(JSONVarTableEntry), "jsonVarTable", -1);
  MY_MALLOC (jsonPageTable, INIT_JSONPAGE_ENTRIES * sizeof(JSONPageTableEntry), "jsonPageTable", -1);

  if (jsonVarTable && jsonPageTable)
    {
      max_jsonvar_entries = INIT_JSONVAR_ENTRIES;
      max_jsonpage_entries = INIT_JSONPAGE_ENTRIES;
    }
  else
    {
      max_jsonvar_entries = 0;
      max_jsonpage_entries = 0;
      NS_EXIT(-1, CAV_ERR_1031012, "JSONVarTableEntry", "JSONPageTableEntry");
    }
}


/*
To be removed

*******************************************************

Discription: We know that API's in script.capture or registration.specs are parsed sequentialy so in case of search in 
             search if some one use API which contain search in serach parameter before the API from which this parameter
             will take the value, he will not find the value, so we have to make sure that the dependent parameters should                be in their right sequence.

Return:      0--> If sequence of API's is wrong
             1--> If sequence of API's is right
********************************************************/
int check_jsonvar_for_search_in_var(int var_idx, char *var_name)
{
  int jsonVarTbl_idx = 0;

  for (jsonVarTbl_idx = 0; jsonVarTbl_idx < var_idx; jsonVarTbl_idx++)
    if (!strcmp(var_name, RETRIEVE_BUFFER_DATA(jsonVarTable[jsonVarTbl_idx].name)))
      return 1;
  return 0;
}

/*--------------------------------------------------------------------------------------
Function to Fetch and parse all the information regarding jsonvar from Registration.spec
and fillup all the required data structures accordingly.

----------------------------------------------------------------------------------------*/

int input_jsonvar_data(char* line, int line_number, int sess_idx, char *script_filename)
{
  NSApi api_ptr; //for parsing from parse_api
  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char msg[MAX_LINE_LENGTH];
  char file_name[MAX_ARG_VALUE_SIZE +1];
  int state = NS_ST_JSON_NAME;
  char jsonvar_buf[MAX_LINE_LENGTH];
  int rnum, i, j, ret;
  int num_tok =0;
  int encode_flag_specified = 0;
  char EncodeSpaceBy[16] = "+";
  char encode_chars_done = 0;
  char char_to_encode_buf[1024];
  char page_name[MAX_LINE_LENGTH];
  char key[MAX_ARG_NAME_SIZE + 1];
  char value[MAX_ARG_VALUE_SIZE + 1];
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  int varpage_idx;
  int do_page = 0;
  char *fields[34];

  NSDL1_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d", line, line_number, sess_idx);
  /*bug id: 101320: trace updated to show NS_TA_DIR ToDo: TBD with DJA*/
  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_json_var() declaration on line %d of %s/%s/%s: ", line_number, GET_NS_RTA_DIR(),
           get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
           //Previously taking with only script name
           //get_sess_name_with_proj_subproj(sess_name), script_filename);
  msg[MAX_LINE_LENGTH-1] = '\0';
 
  sprintf(file_name, "%s/%s/%s", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
  //Previously taking with only script name
  //sprintf(file_name, "scripts/%s/%s", get_sess_name_with_proj_subproj(sess_name), script_filename);
  file_name[strlen(file_name)] = '\0';

  NSDL1_VARS(NULL, NULL, "api_ptr = %p, file_name = %s", &api_ptr, file_name);

  //Parsing api nsl_json_var() api
  //Since we need to remove spaces from value so pass set flag trim_spaces = 1
  //parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  for (j = 0; j < api_ptr.num_tokens; j++) {
    strcpy(key, api_ptr.api_fields[j].keyword);
    strcpy(value, api_ptr.api_fields[j].value);

    NSDL2_VARS(NULL, NULL, "j = %d, api_ptr.num_tokens = %d, key = [%s], value = [%s], state = %d",
               j, api_ptr.num_tokens, key, value, state);
    switch (state) {
    case NS_ST_JSON_NAME:
      //JSON parameter should not have any value
      if ( strcmp(value, "" ))
        break;

      strcpy(jsonvar_buf, key);
      NSDL2_VARS(NULL, NULL, "parameter name = [%s]", jsonvar_buf);

      // For validating the variable we are calling the validate_var function 
      if (validate_var(jsonvar_buf)) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, jsonvar_buf);
      }

      create_jsonvar_table_entry(&rnum);

      jsonVarTable[rnum].sess_idx = sess_idx;

      if (gSessionTable[sess_idx].jsonvar_start_idx == -1) {
        gSessionTable[sess_idx].jsonvar_start_idx = rnum;
        gSessionTable[sess_idx].num_jsonvar_entries = 0;
      }

      gSessionTable[sess_idx].num_jsonvar_entries++;
      
      if ((jsonVarTable[rnum].name = copy_into_big_buf(jsonvar_buf, 0)) == -1) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, jsonvar_buf);
      }    
  
      // Fills the default values 
      jsonVarTable[rnum].action_on_notfound = VAR_NOTFOUND_IGNORE;
      jsonVarTable[rnum].jsonvar_rdepth_bitmask = VAR_IGNORE_REDIRECTION_DEPTH;//take the last bydefault
      jsonVarTable[rnum].convert = VAR_CONVERT_NONE;
      jsonVarTable[rnum].ord = 1;
      // Not Yet Implemented 
      jsonVarTable[rnum].search = SEARCH_BODY;
      jsonVarTable[rnum].search_in_var_name = NULL; // Variable 
      jsonVarTable[rnum].saveoffset = 0;
      jsonVarTable[rnum].savelen = -1;
      jsonVarTable[rnum].pgall = 0;
      jsonVarTable[rnum].encode_type = ENCODE_NONE;

      memset(jsonVarTable[rnum].encode_chars, 49, TOTAL_CHARS);

      for (i = 'a'; i<='z';i++)
        jsonVarTable[rnum].encode_chars[i] = 0;

      for (i = 'A'; i<='Z';i++)
        jsonVarTable[rnum].encode_chars[i] = 0;

      for (i = '0'; i<='9';i++)
        jsonVarTable[rnum].encode_chars[i] = 0;

      jsonVarTable[rnum].encode_chars['+'] = 0;
      jsonVarTable[rnum].encode_chars['.'] = 0;
      jsonVarTable[rnum].encode_chars['_'] = 0;
      jsonVarTable[rnum].encode_chars['-'] = 0;


      state = NS_ST_JSON_OPTIONS;
      break;

    case NS_ST_JSON_OPTIONS:
      if (!strcasecmp(key, "ActionOnNotFound")) {
      NSDL2_VARS(NULL, NULL, "action not found  = [%s]", value);
        if (!strcasecmp(value, "Error")) {
          jsonVarTable[rnum].action_on_notfound = VAR_NOTFOUND_ERROR;
        } else if (!strcasecmp(value, "Warning")) {
          jsonVarTable[rnum].action_on_notfound = VAR_NOTFOUND_WARNING;
        } else {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "Json");
        }
      } else if (!strcasecmp(key, "Convert")) {
          NSDL2_VARS(NULL, NULL, "Convert  = [%d]", value);
          if (!strcasecmp(value, VAR_CONVERT_TEXT_TO_URL_STR)) {
            jsonVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_URL;
          } else if (!strcasecmp(value, VAR_CONVERT_URL_TO_TEXT_STR)) {
              jsonVarTable[rnum].convert = VAR_CONVERT_URL_TO_TEXT;
          } else if (!strcasecmp(value, VAR_CONVERT_TEXT_TO_HTML_STR)) {
              jsonVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_HTML;
          } else if (!strcasecmp(value, VAR_CONVERT_HTML_TO_TEXT_STR)) {
              jsonVarTable[rnum].convert = VAR_CONVERT_HTML_TO_TEXT;
          } else if (!strcasecmp(value, VAR_CONVERT_HTML_TO_URL_STR)) {
              //Note:- currently we are assuming (html <--> url) <==> (text <--> url)
              jsonVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_URL;
          } else if (!strcasecmp(value, VAR_CONVERT_URL_TO_HTML_STR)) {
              jsonVarTable[rnum].convert = VAR_CONVERT_URL_TO_TEXT;
          } else if (!strcasecmp(value, VAR_CONVERT_TEXT_TO_BASE64_STR)) {
              jsonVarTable[rnum].convert = VAR_CONVERT_TEXT_TO_BASE64;
          } else if (!strcasecmp(value, VAR_CONVERT_BASE64_TO_TEXT_STR)) {
              jsonVarTable[rnum].convert = VAR_CONVERT_BASE64_TO_TEXT;
          } else {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "Json");
          }
      } else if (!strcasecmp(key, "RedirectionDepth")) {
          if (!global_settings->g_follow_redirects)
          {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012030_ID, CAV_ERR_1012030_MSG);
          }

          if (!strcasecmp(value, "Last")) {
            jsonVarTable[rnum].jsonvar_rdepth_bitmask = VAR_IGNORE_REDIRECTION_DEPTH;
          } else if(!strcasecmp(value, "ALL")) {
              jsonVarTable[rnum].jsonvar_rdepth_bitmask = VAR_ALL_REDIRECTION_DEPTH;
          } else {
              jsonVarTable[rnum].jsonvar_rdepth_bitmask = 0; /* must reset this since default is set -1
                                                                we are going to set the bit on it. */
                                                              
              strcpy(jsonvar_buf, value); //save the buffer
              NSDL2_VARS(NULL, NULL, "redirection = [%s]", jsonvar_buf);
              num_tok = get_tokens(jsonvar_buf, fields, ";" , MAX_REDIRECTION_DEPTH_LIMIT +1);
              if(num_tok == 0)
              {
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Json");
              }
              char *ptr1 = NULL, *ptr2 = NULL, *value_ptr = NULL;
              int local_rdepth = 0;
              value_ptr = value;
              for(i = 0; i < num_tok; i++)
              {
              /*chain last value handle here[if RedirectionDepth=1;2;3, next any field then handle 3 here] without chain eg R                edirectionDepth=1,next any field also handle here. Rest of the chain element5s handled in below
              */                            
                if (strstr(fields[i], ",")) {
                  ptr1 = fields[i];
                  ptr2 = strstr(ptr1, ",");
                  strncpy(jsonvar_buf, fields[i], ptr2 - ptr1);
                  jsonvar_buf[ptr2 - ptr1] = 0;
                  local_rdepth = atoi(jsonvar_buf);
                  jsonVarTable[rnum].jsonvar_rdepth_bitmask |= (1 << (local_rdepth - 1 ));
                  check_redirection_limit(local_rdepth);
                  //line_ptr = ptr2 +1; //update the line_ptr
                  value_ptr += (ptr2 - ptr1);
                } else {
                    local_rdepth = atoi(fields[i]);
                    check_redirection_limit(local_rdepth);
                    jsonVarTable[rnum].jsonvar_rdepth_bitmask |= (1 << (local_rdepth - 1 ));
                }
                //chain handle 
                 if (i < (num_tok - 1))
                  value_ptr += strlen(fields[i]) + 1;  // 1 is incr for ;
                else if (!(ptr2 - ptr1))
                  value_ptr += strlen(fields[i]);
              } /* end of num_tok for loop*/
          }
      } else if ((!strcasecmp(key, "OBJECT_PATH"))) {
        if (strlen(value) == 0) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Json");
        }
	
	if (strncmp("root.", value, 5) == 0) {
       	  jsonVarTable[rnum].json_path_entries =  nslb_str_to_expr_cond(value, &(jsonVarTable[rnum].json_path));
        }
        else {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012184_ID, CAV_ERR_1012184_MSG);
	}	
      } else if (!strcasecmp(key, "EncodeMode")){
          set_buffer_for_encodeMode(value, rnum, msg, &encode_flag_specified);
      } else if (!strncasecmp(key, "CharstoEncode", strlen("CharstoEncode"))) {
          set_char_to_encode_buf(value, char_to_encode_buf, rnum, msg, encode_flag_specified,
                                              &encode_chars_done);
      } else if (!strncasecmp(key, "EncodeSpaceBy", strlen("EncodeSpaceBy"))) {
          set_json_encodeSpaceBy(value, EncodeSpaceBy, key, msg);
      } else if (!strcasecmp(key, "ORD")) {
          strcpy(jsonvar_buf, value);
          if (!strcmp(value, "")) {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Json");
          }
          if (!strcmp(value, "0")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012041_ID, CAV_ERR_1012041_MSG, "Json");
          }

          if (!strcasecmp(jsonvar_buf, "ALL"))
            jsonVarTable[rnum].ord = ORD_ALL;
          else if (!strcasecmp(jsonvar_buf, "ANY")){
            jsonVarTable[rnum].ord = ORD_ANY;
          }
          else if (!strcasecmp(jsonvar_buf, "ANY_NONEMPTY")){
            jsonVarTable[rnum].ord = ORD_ANY_NON_EMPTY;
          }
          else if (!strcasecmp(jsonvar_buf, "LAST")){
            jsonVarTable[rnum].ord = ORD_LAST;
          }
           else {
            for (i = 0; jsonvar_buf[i]; i++) {
              if (!isdigit(jsonvar_buf[i])) {
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012136_ID, CAV_ERR_1012136_MSG, "ALL, ANY, ANY_NONEMPTY, LAST", "Json");
              }
            }
            jsonVarTable[rnum].ord = atoi(jsonvar_buf);
          }
      } else if (!strcasecmp(key, "Search")) {
          if (!strcasecmp(value, "BODY")) {
            jsonVarTable[rnum].search = SEARCH_BODY;
          }
          else if (!strcasecmp(value, "VARIABLE")) {
            jsonVarTable[rnum].search = SEARCH_VARIABLE;
          } else if (! strcasecmp(value, "HEADER")) {
              jsonVarTable[rnum].search = SEARCH_HEADER;
          }                  
      } else if (!strcasecmp(key, "SaveOffset")) {
          strcpy(jsonvar_buf, value);
          for (i = 0; jsonvar_buf[i]; i++) {
            if (!isdigit(jsonvar_buf[i])) {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, key);
            }
          }

          if(!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Json");
          }
          jsonVarTable[rnum].saveoffset = atoi(jsonvar_buf);
      } else if (!strcasecmp(key, "SaveLen")) {
          strcpy(jsonvar_buf, value);
          for (i = 0; jsonvar_buf[i]; i++) {
            if (!isdigit(jsonvar_buf[i])) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, key);
            }
          }

          if (!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Json");
          }

          jsonVarTable[rnum].savelen = atoi(jsonvar_buf);
      } else if (!strcasecmp(key, "VAR")) {
          strcpy(jsonvar_buf, value);
          if (!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Json");
          }

          MY_MALLOC(jsonVarTable[rnum].search_in_var_name, strlen(jsonvar_buf) + 1, "search_in_var_name", -1 );
          strcpy(jsonVarTable[rnum].search_in_var_name, jsonvar_buf);
          if (!check_jsonvar_for_search_in_var(rnum, jsonvar_buf)){
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012046_ID, CAV_ERR_1012046_MSG, jsonvar_buf, RETRIEVE_BUFFER_DATA(jsonVarTable[rnum].name));
          }
      } else if (!strcasecmp(key, "PAGE")) {      
          strcpy(page_name, value);
          if(!strcmp(value, "")) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, key, "Json");
          }
          //When PAGE = ALL
          //Note here we are * sign insted of word ALL because it may be possible that one can put their page name ALL  
          if (strcmp(page_name, "*")) {  
            create_jsonpage_table_entry(&varpage_idx);

            jsonPageTable[varpage_idx].jsonvar_idx = rnum;
            if ((jsonPageTable[varpage_idx].page_name = copy_into_temp_buf(page_name, 0)) == -1) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, page_name);
            }
            NSDL2_VARS(NULL, NULL, "varpage_idx = %d", varpage_idx);

            if (gSessionTable[sess_idx].jsonpagevar_start_idx == -1) {
              gSessionTable[sess_idx].jsonpagevar_start_idx = varpage_idx;
              gSessionTable[sess_idx].num_jsonpagevar_entries = 0;
            }
     
            gSessionTable[sess_idx].num_jsonpagevar_entries++;
            jsonPageTable[varpage_idx].page_idx = -1;
            jsonPageTable[varpage_idx].sess_idx = sess_idx; 
            do_page=1;
            NSDL3_VARS(NULL, NULL, "page_name = %d, jsonvar_idx = %d, jsonpagevar_start_idx = %d", 
                       jsonPageTable[varpage_idx].page_name, rnum, gSessionTable[sess_idx].jsonpagevar_start_idx);
          } else { //PAGE= * case
              jsonVarTable[rnum].pgall = 1;
          }
          if ((do_page == 1) && (jsonVarTable[rnum].pgall == 1)) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012050_ID, CAV_ERR_1012050_MSG, key);
          }
      } else if (!strcasecmp(key, "RetainPreValue")) {    
          NSDL2_VARS(NULL, NULL, "RetainPreValue  = [%s]", value);
          if (!strcasecmp(value, "Yes")) {
            jsonVarTable[rnum].retain_pre_value = RETAIN_PRE_VALUE;
          } else if (!strcasecmp(value, "No")) {
              jsonVarTable[rnum].retain_pre_value = NOT_RETAIN_PRE_VALUE;
            } else {
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "Json");
              }
      } else {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, key, "Json");
        } 
    }//End of Switch-case

    // JSON variable is mandatory
    if (state == NS_ST_JSON_NAME)
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "JSON");
    }
  }//End for for loop for parsing tokens

  if ((jsonVarTable[rnum].encode_space_by = copy_into_big_buf(EncodeSpaceBy, 0)) == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, EncodeSpaceBy);
  }  
                         
  /* In case we have given convert method TextToHtml, TextToURL, URLToHTML, HtmlToURL we are setting 
     EncodeMode as None to prevent encoding of special characters found in encoded value. */

  if (jsonVarTable[rnum].convert == VAR_CONVERT_TEXT_TO_URL || jsonVarTable[rnum].convert == VAR_CONVERT_TEXT_TO_HTML){
    jsonVarTable[rnum].encode_type = ENCODE_NONE;
  }  

  NSDL3_VARS(NULL, NULL, "JSON Var Data dump: parameter name = [%s], saveoffset = [%d], savelen = [%d],"
  "ActionOnNotFound = [%d], ord = [%d]", RETRIEVE_BUFFER_DATA(jsonVarTable[rnum].name),
  jsonVarTable[rnum].saveoffset, jsonVarTable[rnum].savelen, 
  jsonVarTable[rnum].action_on_notfound, jsonVarTable[rnum].ord, jsonVarTable[rnum].retain_pre_value); 

  if ((!do_page) && (!jsonVarTable[rnum].pgall)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "PAGE", "Json");
  }

/*  if (((!lb_done) || (!rb_done)) && (searchVarTable[rnum].ord != 1)) {
    fprintf(stderr, "%s ORD must be 1 when either of LB and/Or RB arguments not specified\n", msg);
    return -1;
  }
*/
  if ((jsonVarTable[rnum].search == SEARCH_VARIABLE) && (jsonVarTable[rnum].search_in_var_name == NULL)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "VAR", "Json");
  }

  if ((jsonVarTable[rnum].search != SEARCH_VARIABLE) && (jsonVarTable[rnum].search_in_var_name != NULL)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012035_ID, CAV_ERR_1012035_MSG);
  }

  if ((encode_chars_done == 0) && (encode_flag_specified == 1))
  {
    jsonVarTable[rnum].encode_chars[' '] = 1;
    jsonVarTable[rnum].encode_chars[39] = 1; //Setting for (') as it was givin error on compilation 
    jsonVarTable[rnum].encode_chars[34] = 1; //Setting for (") as it was givin error on compilation 
    jsonVarTable[rnum].encode_chars['<'] = 1;
    jsonVarTable[rnum].encode_chars['>'] = 1;
    jsonVarTable[rnum].encode_chars['#'] = 1;
    jsonVarTable[rnum].encode_chars['%'] = 1;
    jsonVarTable[rnum].encode_chars['{'] = 1;
    jsonVarTable[rnum].encode_chars['}'] = 1;
    jsonVarTable[rnum].encode_chars['|'] = 1;
    jsonVarTable[rnum].encode_chars['\\'] = 1;
    jsonVarTable[rnum].encode_chars['^'] = 1;
    jsonVarTable[rnum].encode_chars['~'] = 1;
    jsonVarTable[rnum].encode_chars['['] = 1;
    jsonVarTable[rnum].encode_chars[']'] = 1;
    jsonVarTable[rnum].encode_chars['`'] = 1;
  }


  return 0;
#undef NS_ST_JSON_NAME
#undef NS_ST_JSON_OPTIONS

}

/*------------------------------------------------------------------
Function to make perpage JSON var Table in shared memory


-------------------------------------------------------------------*/

PerPageJSONVarTableEntry_Shr *copy_jsonvar_into_shared_mem(void)
{
  void *jsonvar_perpage_table_shr_mem;
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. total_jsonvar_entries = %d, total_perpagejsonvar_entries = %d",
  total_jsonvar_entries, total_perpagejsonvar_entries);
  /* insert the JSONVarTableEntry_Shr and the PerPageJSONVarTableEntry_shr */

  if (total_jsonvar_entries + total_perpagejsonvar_entries) {
    jsonvar_perpage_table_shr_mem = do_shmget(total_jsonvar_entries * sizeof(JSONVarTableEntry_Shr) +
                                              total_perpagejsonvar_entries * sizeof(PerPageJSONVarTableEntry_Shr),
                                              "JSON var tables");
    jsonvar_table_shr_mem = jsonvar_perpage_table_shr_mem;

    for (i = 0; i < total_jsonvar_entries; i++) {
      jsonvar_table_shr_mem[i].var_name = BIG_BUF_MEMORY_CONVERSION(jsonVarTable[i].name);
      jsonvar_table_shr_mem[i].action_on_notfound = jsonVarTable[i].action_on_notfound;
      jsonvar_table_shr_mem[i].convert = jsonVarTable[i].convert;

      memcpy(jsonvar_table_shr_mem[i].encode_chars, jsonVarTable[i].encode_chars, TOTAL_CHARS);
      jsonvar_table_shr_mem[i].encode_type = jsonVarTable[i].encode_type;
      jsonvar_table_shr_mem[i].encode_space_by = BIG_BUF_MEMORY_CONVERSION(jsonVarTable[i].encode_space_by);

      jsonvar_table_shr_mem[i].jsonvar_rdepth_bitmask = jsonVarTable[i].jsonvar_rdepth_bitmask;

      jsonvar_table_shr_mem[i].json_path = jsonVarTable[i].json_path;
      jsonvar_table_shr_mem[i].json_path_entries = jsonVarTable[i].json_path_entries;     
 
      jsonvar_table_shr_mem[i].ord = jsonVarTable[i].ord;
      /* Not yet implemented */
      // jsonvar_table_shr_mem[i].relframeid = jsonVarTable[i].relframeid;
      jsonvar_table_shr_mem[i].search = jsonVarTable[i].search;
      if (jsonVarTable[i].search_in_var_name) {
        jsonvar_table_shr_mem[i].search_in_var_hash_code = gSessionTable[jsonVarTable[i].sess_idx].var_hash_func(jsonVarTable[i].search_in_var_name, strlen(jsonVarTable[i].search_in_var_name));
        if (jsonvar_table_shr_mem[i].search_in_var_hash_code == -1) {
          NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "Var name %s used for search in search variable name %s is not a valid varable. Exiting..\n", jsonVarTable[i].search_in_var_name, BIG_BUF_MEMORY_CONVERSION(jsonVarTable[i].name));
          NS_EXIT(-1, CAV_ERR_1031040, jsonVarTable[i].search_in_var_name, BIG_BUF_MEMORY_CONVERSION(jsonVarTable[i].name));
        }
        //FREE_AND_MAKE_NOT_NULL(jsonVarTable[i].search_in_var_name, "jsonVarTable[i].search_in_var_name", -1);
        FREE_AND_MAKE_NOT_NULL_EX(jsonVarTable[i].search_in_var_name, strlen(jsonVarTable[i].search_in_var_name), "jsonVarTable[i].search_in_var_name", -1);
      } 
      jsonvar_table_shr_mem[i].saveoffset = jsonVarTable[i].saveoffset;
      jsonvar_table_shr_mem[i].savelen = jsonVarTable[i].savelen;
      jsonvar_table_shr_mem[i].retain_pre_value = jsonVarTable[i].retain_pre_value;
      jsonvar_table_shr_mem[i].hash_idx = gSessionTable[jsonVarTable[i].sess_idx].var_hash_func(jsonvar_table_shr_mem[i].var_name, strlen(jsonvar_table_shr_mem[i].var_name));
      assert (jsonvar_table_shr_mem[i].hash_idx != -1);
    }

    perpagejsonvar_table_shr_mem = jsonvar_perpage_table_shr_mem + (total_jsonvar_entries * sizeof(JSONVarTableEntry_Shr));
    for (i = 0; i < total_perpagejsonvar_entries; i++) {
      perpagejsonvar_table_shr_mem[i].jsonvar_ptr = jsonvar_table_shr_mem + perPageJSONVarTable[i].jsonvar_idx;
    }
  }
  return perpagejsonvar_table_shr_mem;
}


int jsonpage_cmp(const void* ent1, const void* ent2) {
  NSDL1_VARS(NULL, NULL, "Method called.");
  if (((JSONPageTableEntry *)ent1)->page_idx < ((JSONPageTableEntry *)ent2)->page_idx)
    return -1;
  else if (((JSONPageTableEntry *)ent1)->page_idx > ((JSONPageTableEntry *)ent2)->page_idx)
    return 1;
  else if (((JSONPageTableEntry *)ent1)->jsonvar_idx < ((JSONPageTableEntry *)ent2)->jsonvar_idx)
    return -1;
  else if (((JSONPageTableEntry *)ent1)->jsonvar_idx > ((JSONPageTableEntry *)ent2)->jsonvar_idx)
    return 1;
  else return 0;
}



int process_jsonvar_table(void) {

  int i,j,k;
  int page_idx;
  int chkpage_idx;
  int last_jsonvar_idx;
  int rnum;

  NSDL1_VARS(NULL, NULL, "Method called. total_sess_entries = %d", total_sess_entries);

  //Create page jsonvar table  based on PAGEALL jsonvar entried. could not have been done earlier as the pages were not parsed before
  for (i = 0; i < total_sess_entries; i++) {
    NSDL2_VARS(NULL, NULL, "Case: PAGE = *: i(sess_idx) = %d, sess_name = %s, jsonvar_start_idx = %d, num_jsonvar_entries = %d ", i, RETRIEVE_BUFFER_DATA(gSessionTable[i].sess_name), gSessionTable[i].jsonvar_start_idx, gSessionTable[i].num_jsonvar_entries);
    for (j = gSessionTable[i].jsonvar_start_idx; j < (gSessionTable[i].jsonvar_start_idx + gSessionTable[i].num_jsonvar_entries); j++) {
      if (jsonVarTable[j].pgall) {
        NSDL2_VARS(NULL, NULL, "j(jsonvar_idx) = %d, first_page = %d, num_pages = %d", j, gSessionTable[i].first_page, gSessionTable[i].num_pages);
        for (k = gSessionTable[i].first_page; k < (gSessionTable[i].first_page + gSessionTable[i].num_pages); k++) {
          NSDL2_VARS(NULL, NULL, "k(page_idx) = %d", k);
          if (create_jsonpage_table_entry(&chkpage_idx) == -1) {
            fprintf(stderr, "Could not create create_jsonpage_table_entry. Short on memory\n");
            return -1;
          }
          NSDL2_VARS(NULL, NULL, "chkpage_idx = %d", chkpage_idx);
          jsonPageTable[chkpage_idx].jsonvar_idx = j;


          if (gSessionTable[i].jsonpagevar_start_idx == -1) {
            gSessionTable[i].jsonpagevar_start_idx = chkpage_idx;
            gSessionTable[i].num_jsonpagevar_entries = 0;
          }

          gSessionTable[i].num_jsonpagevar_entries++;
          jsonPageTable[chkpage_idx].page_idx = k;
          jsonPageTable[chkpage_idx].sess_idx = i;

          /* We also need to propogate redirection_depth_bitmask to page lvl for PAGE=* */
          if(jsonVarTable[j].jsonvar_rdepth_bitmask != VAR_IGNORE_REDIRECTION_DEPTH) {
            gPageTable[k].redirection_depth_bitmask = set_depth_bitmask(gPageTable[k].redirection_depth_bitmask, jsonVarTable[j].jsonvar_rdepth_bitmask);
          }
          if(jsonVarTable[j].search == SEARCH_HEADER){
            gPageTable[i].save_headers = 1;
          }
          NSDL2_VARS(NULL, NULL, "jsonpagevar_start_idx = %d, num_jsonpagevar_entries = %d, jsonvar_idx = %d, page_idx = %d", gSessionTable[i].jsonpagevar_start_idx, gSessionTable[i].num_jsonpagevar_entries, j, k);
        }
      }
    }
  }   
  
  /*
    Till now we have made a complete jsonPageTable but not assigned page_idx to all the pages.
    case1: 
      page_idx = -1: mean this page belongs to those jsonVar which has to be searched on this particular page 
      so we have to assign page_idx here
    case2:
      page_idx != -1: mean this page belongs to those jsonVar which has to be searched on all page
      this case has been handle in above function
  */
  NSDL3_VARS(NULL, NULL, "total_jsonpage_entries = %d", total_jsonpage_entries);
  int jsonPageTbl_idx = 0;
  for (jsonPageTbl_idx = 0; jsonPageTbl_idx < total_jsonpage_entries; jsonPageTbl_idx++) {
    NSDL3_VARS(NULL, NULL, "Case: PAGE = particular: jsonPageTbl_idx = %d", jsonPageTbl_idx);
    if (jsonPageTable[jsonPageTbl_idx].page_idx == -1) {
      if ((jsonPageTable[jsonPageTbl_idx].page_idx = find_page_idx(RETRIEVE_TEMP_BUFFER_DATA(jsonPageTable[jsonPageTbl_idx].page_name), jsonPageTable[jsonPageTbl_idx].sess_idx)) == -1) {
        NSDL3_VARS(NULL, NULL, "jsonPageTbl_idx = %d, unknown page name = %s", jsonPageTbl_idx, RETRIEVE_TEMP_BUFFER_DATA(jsonPageTable[jsonPageTbl_idx].page_name));
                      //Previously taking with only script name
                      //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[jsonPageTable[jsonPageTbl_idx].sess_idx].sess_name)));
        // Fix Bug3448
        NS_EXIT(-1, CAV_ERR_1031030, RETRIEVE_TEMP_BUFFER_DATA(jsonPageTable[jsonPageTbl_idx].page_name), "Json", RETRIEVE_BUFFER_DATA(jsonVarTable[jsonPageTable[jsonPageTbl_idx].jsonvar_idx].name), get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[jsonPageTable[jsonPageTbl_idx].sess_idx].sess_name), jsonPageTable[jsonPageTbl_idx].sess_idx, "/")); 
      }
    }
  }

  assert (jsonPageTbl_idx == total_jsonpage_entries);

  //sort  search Page table in the order of page index & search var index
  qsort(jsonPageTable, total_jsonpage_entries, sizeof(JSONPageTableEntry), jsonpage_cmp);

  //Create per page json var table
  //This table has the list of json vars that need to be initialized on specific pages
  //Each page has the start & num entries for this per page json var table entries.
  for (i = 0; i < total_jsonpage_entries; i++) {
    if ((page_idx = jsonPageTable[i].page_idx) != -1) {
      if (gPageTable[page_idx].first_jsonvar_idx == -1) {
        if (create_perpagejsonvar_table_entry(&rnum) != SUCCESS) {
          fprintf(stderr, "Unable to create perpage table entry\n");
          return -1;
        }
        perPageJSONVarTable[rnum].jsonvar_idx = jsonPageTable[i].jsonvar_idx;
        gPageTable[page_idx].first_jsonvar_idx = rnum;
        last_jsonvar_idx = jsonPageTable[i].jsonvar_idx;
      } else {
        //Remove duplicate entries for same var for same page
        //Note that jsonPage Table is ordered in page number order
        if (last_jsonvar_idx == jsonPageTable[i].jsonvar_idx)
          continue;
        if (create_perpagejsonvar_table_entry(&rnum) != SUCCESS) {
          fprintf(stderr, "Unable to create perpage table entry\n");
          return -1;
        }
        perPageJSONVarTable[rnum].jsonvar_idx = jsonPageTable[i].jsonvar_idx;
        last_jsonvar_idx = jsonPageTable[i].jsonvar_idx;
      }
      /* Set the per page depth bit if RedirectionDepth=1,2,3,..n and
      *  in case of RedirectionDepth=ALL set all the bits so that
      *  copy_retrieve_data() will be called.
      *  Do not set any bits for the RedirectionDepth=Last
      */
      if (jsonVarTable[jsonPageTable[i].jsonvar_idx].jsonvar_rdepth_bitmask != VAR_IGNORE_REDIRECTION_DEPTH)
      {
        NSDL1_VARS(NULL, NULL, "Before setting the redirection_depth_bitmask value is = 0x%x, jsonVarTable idx = %d", gPageTable[page_idx].redirection_depth_bitmask, jsonPageTable[i].jsonvar_idx);
        gPageTable[page_idx].redirection_depth_bitmask = set_depth_bitmask(gPageTable[page_idx].redirection_depth_bitmask, jsonVarTable[jsonPageTable[i].jsonvar_idx].jsonvar_rdepth_bitmask);

        NSDL1_VARS(NULL, NULL, "After setting the redirection_depth_bitmask value is = 0x%x jsonVarTable idx = %d", gPageTable[page_idx].redirection_depth_bitmask, jsonPageTable[i].jsonvar_idx);
      }
      if (jsonVarTable[jsonPageTable[i].jsonvar_idx].search == SEARCH_HEADER){
        gPageTable[page_idx].save_headers = 1;
      }
      gPageTable[page_idx].num_jsonvar++;
      NSDL1_VARS(NULL, NULL, "gPageTable[page_idx].num_jsonvar = %d for page = %d", gPageTable[page_idx].num_jsonvar, page_idx);
    }
  }

  return 0;
}  
       
/*-----------------------------------------------------------------
Function to find JSON var index

------------------------------------------------------------------*/

int find_jsonvar_idx (char* name, int len, int sess_idx) {
  int i;
  if (!len)
    len = strlen(name);
  char save_chr = name[len];
  name[len] = 0;

  NSDL1_VARS(NULL, NULL, "Method called. name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].jsonvar_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].jsonvar_start_idx; i < gSessionTable[sess_idx].jsonvar_start_idx + gSessionTable[sess_idx].num_jsonvar_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(jsonVarTable[i].name), name)){
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
char *json_var_to_str(PerPageJSONVarTableEntry_Shr* perpagejsonvar_ptr, char *buf)
{
  char ord_buf[10];
  char action_on_notfound_buf[20];
  char convert_buf[20];

  if (perpagejsonvar_ptr->jsonvar_ptr->ord == ORD_ANY)
    strcpy(ord_buf, "ANY");
  else if (perpagejsonvar_ptr->jsonvar_ptr->ord == ORD_ALL)
    strcpy(ord_buf, "ALL");
  else
    sprintf(ord_buf, "%d", perpagejsonvar_ptr->jsonvar_ptr->ord);

  if (perpagejsonvar_ptr->jsonvar_ptr->action_on_notfound == VAR_NOTFOUND_ERROR)
    strcpy(action_on_notfound_buf, "Error");
  else if (perpagejsonvar_ptr->jsonvar_ptr->action_on_notfound == VAR_NOTFOUND_WARNING)
    strcpy(action_on_notfound_buf, "Warning");
  else
    strcpy(action_on_notfound_buf, "Ignore");

  if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_TEXT_TO_URL)
    strcpy(convert_buf, "TextToUrl");
  else if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_URL_TO_TEXT)
    strcpy(convert_buf, "UrlToText");
  else if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_TEXT_TO_HTML)
    strcpy(convert_buf, "TextToHtml");
  else if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_HTML_TO_TEXT)
    strcpy(convert_buf, "HtmlToText");
  else if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_HTML_TO_URL)
    strcpy(convert_buf, "TextToUrl");
    //strcpy(convert_buf, "HtmlToUrl");
  else if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_URL_TO_HTML)
    strcpy(convert_buf, "UrlToText");
    //strcpy(convert_buf, "UrlToHtml");
  else if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64)
    strcpy(convert_buf, "TextToBase64");
  else if (perpagejsonvar_ptr->jsonvar_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT)
    strcpy(convert_buf, "TextToBase64");
  else
    strcpy(convert_buf, "NoConversion");

  sprintf(buf, "JSON Parameter (Parameter Name = %s; ORD = %s; "
               "ActionOnNotFound = %s; Convert = %s; SaveLen = %d; "
               "SaveOffset = %d; Depth = %d)",
               perpagejsonvar_ptr->jsonvar_ptr->var_name,
               ord_buf,
               action_on_notfound_buf, convert_buf,
               perpagejsonvar_ptr->jsonvar_ptr->savelen,
               perpagejsonvar_ptr->jsonvar_ptr->saveoffset,
               perpagejsonvar_ptr->jsonvar_ptr->jsonvar_rdepth_bitmask);
 
  return buf;
}


/*------------------------------------------------------------------
 This function is used to set the to_stop flag 

---------------------------------------------------------------------*/

static inline void
json_var_check_status(connection *cptr, VUser *vptr, PerPageJSONVarTableEntry_Shr* perpagejsonvar_ptr, int string_found_flag, int *page_fail, int *to_stop)
{
  NSDL2_VARS(NULL, NULL, "Method called. string_found_flag = %d page_fail = %d to_stop = %d", string_found_flag, *page_fail, *to_stop);
  char tmp_buf[1024];
  /* If this search var is failing and action_on_notfound is equal to Error, then set to_stop to 1
  *  Note - page_fail, json_var_fail and to_stop should be set one time
  *  Once set, this should remain set
  */
  if (!string_found_flag)
  {
    json_var_to_str(perpagejsonvar_ptr, tmp_buf);

    // Show in Debug trace and debug log 
    // json variable not found. Added condition for that
      NS_DT1(vptr, NULL, DM_L1, MM_VARS, "json variable not found for %s", tmp_buf);

    if (perpagejsonvar_ptr->jsonvar_ptr->action_on_notfound == VAR_NOTFOUND_ERROR)
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
                                 "Page failed with status %s due to json parameter (%s) not found in response.",
                                 get_error_code_name(NS_REQUEST_CV_FAILURE),
                                 perpagejsonvar_ptr->jsonvar_ptr->var_name);
      *page_fail = 1;
      *to_stop =  1;
    } else if (perpagejsonvar_ptr->jsonvar_ptr->action_on_notfound == VAR_NOTFOUND_WARNING) {
      NS_EL_4_ATTR(EID_HTTP_PAGE_ERR_START + NS_REQUEST_CV_FAILURE, vptr->user_index,
                                 vptr->sess_inst, EVENT_CORE, EVENT_WARNING,
                                 vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 tmp_buf,
                                 "json parameter (%s) not found in response.",
                                 perpagejsonvar_ptr->jsonvar_ptr->var_name);
    }
  }
  return;
}


/*added free_jsonvar_value here we reset uservar_table_ptr->flags
 * */
void free_jsonvar_value(UserVarEntry *uservartable_entry)
{
  int j;

  NSLBDL2_SEARCH("Method called");

  // Free old value(s) of variable if any
  if ((uservartable_entry->flags & VAR_IS_VECTOR) == VAR_IS_VECTOR)
  {
    NSLBDL2_SEARCH("Previous variable is Vector.");
    if (uservartable_entry->value.array) {
      for (j = 0; j < uservartable_entry->length; j++) {
        if (uservartable_entry->value.array[j].value){
          free(uservartable_entry->value.array[j].value);
          uservartable_entry->value.array[j].value = NULL;
          uservartable_entry->value.array[j].length = 0;
        }
      }
      free(uservartable_entry->value.array);
      uservartable_entry->value.array = NULL;
      uservartable_entry->length = 0;
    }
    /*Reset the flag value*/
      uservartable_entry->flags = uservartable_entry->flags & ~VAR_IS_VECTOR;
  }
  else
  {
    NSLBDL2_SEARCH("Previous variable is Scalar.");
    if (uservartable_entry->value.value){
      free(uservartable_entry->value.value);
      uservartable_entry->value.value = NULL;
      uservartable_entry->length = 0;
    }
  }
}


static int create_json_temparrayval_table_entry(int* row_num)
{
  NSDL2_VARS(NULL, NULL, "Method called.");
  if (total_json_temparrayval_entries == max_json_temparrayval_entries) {
    json_tempArrayVal = (ArrayValEntry *)realloc(json_tempArrayVal, (max_json_temparrayval_entries + DELTA_TEMPARRAYVAL_ENTRIES) * sizeof(ArrayValEntry));
    if (!json_tempArrayVal) {
      fprintf(stderr, "create_json_temparrayval_table_entry(): Error allocating more memory for tempArrayValues entries\n");
      return (-1);
    } else max_json_temparrayval_entries += DELTA_TEMPARRAYVAL_ENTRIES;
  }
  *row_num = total_json_temparrayval_entries++;
  return (1);
}

void save_json_var_value (char *var_name, char convert, int method, char *var_value,
                 int var_len, int savelen, int value_null, int ord_count, 
                       UserVarEntry *uservartable_entry, int req_ord, char *method_type, int total_method)
{
  NSDL2_VARS(NULL, NULL, "Method called. var_name = %s, var_value = %s, var_len = %d," 
                "savelen = %d, value_null = %d, ord_count = %d, req_ord =%d," 
               " convert = %d, method = %d,  total_method = %d", var_name, var_value, var_len, savelen, value_null, ord_count, req_ord, method, convert, total_method);
  
  int rnum;
  unsigned int amount_to_save; 
  
  //amount_to_save = ((savelen == -1) || (savelen >= var_len))?var_len:savelen;
  //In case of NO default savelen value is 0 whereas in NS default savelen is -1 
  //here we r supporting both cases
  amount_to_save = ((savelen <= 0) || (savelen >= var_len))?var_len:savelen;

  if(req_ord != ORD_ALL) 
  {
    NSDL3_VARS(NULL, NULL, "Save single json var value. var_name = %s", var_name);    
    /* If var value is already there, then free old value
     * bug fixed free and make it NULL since we are going to print this value*/
    free_var_value(uservartable_entry);
    NSDL3_VARS(NULL, NULL, "amount_to_save. var_name = %d", amount_to_save);
    uservartable_entry->value.value = get_search_final_val(var_name, convert, 
                           method, var_value, var_len, savelen, value_null,
                            &uservartable_entry->length, amount_to_save, method_type, total_method, 0);
  } 
  else
  { 
    //ORD ALL case. First fill temp array
    NSDL3_VARS(NULL, NULL, "Save json var value for %s. var_name = %s", var_name, (req_ord == ORD_ALL)?"ORD_ALL":"ORD_ANY_NON_EMPTY");
    create_json_temparrayval_table_entry(&rnum);   
    json_tempArrayVal[rnum].value = get_search_final_val(var_name, convert, method, var_value, var_len, savelen, value_null, &json_tempArrayVal[rnum].length, amount_to_save, method_type, total_method, 0);
  } 
}

#define APPLY_OFFSET \
  if ((value_len <= 0) || (offset >= value_len)) { \
    NSDL2_VARS(NULL, NULL, "Either value length is <= 0 or saveoffset is >= value_length. Treating it as null value. value_length = %d for %s", value_len, var_name); \
    value_null = 1; \
  }

/* Name: process_json_var
* Args: This method will take following arguments
* char *json_buff: This is the buffer from where we will search json variable 
* int json_len: This is the length of json resonse
* char *object_path: This includes path to the object and member name of the object whose value will be saved in json variable
* char *var_name: This will be used just for debug purpose  
* purpose: This method will parse object path, look in the json response for the path and save the value in json var, if path does not
* exist in json variable then fill erro  
*/
int get_json_var_value(nslb_jsont *json_tree, char *var_name, json_expr_cond *json_path, int num_entries, int offset, int var_len, char *json_buff, char *json_buff_end, int *string_found_flag, int req_ord, char convert, int method, UserVarEntry *uservartable_entry, char *method_type, int total_method)
{
  int value_len;
  int value_null = 0;
  int actual_ord = req_ord; // Used only for ORD=ANY and ORD=number not for ORD=ALL
  int i;
  int local_count_ord;
  char *values[10000]; 
  NSDL2_VARS(NULL, NULL, "Method called.");

  local_count_ord = nslb_jsont_eval_json_expr(json_tree, json_path, num_entries, values, 10000);

  NSDL2_VARS(NULL, NULL, "local_count_ord = %d", local_count_ord);

  if ((req_ord != ORD_ANY) && local_count_ord)
  {
    *string_found_flag = 1; //we got the matched string, set the flag
    
    if (req_ord == ORD_ALL){
      for(i = 0; i < local_count_ord; i++){ // Save all values for ORD ALL
        value_len = strlen(values[i]);
        APPLY_OFFSET;
        NSDL3_VARS(NULL, NULL, "Going to save json Var for ord ALL. value_length = %d, for %s", value_len, var_name);
        save_json_var_value(var_name, convert, method, values[i] + offset , value_len - offset, var_len, value_null, local_count_ord, uservartable_entry, req_ord, method_type, total_method); 
      }
    }
    else if (local_count_ord >= actual_ord) { // only specific value for specific value 
      value_len = strlen(values[actual_ord -1]);
      NSDL3_VARS(NULL, NULL, "json Var value_length = %d, for %s, value = %s", value_len, var_name, values[actual_ord -1]);
      NSDL3_VARS(NULL, NULL, "Going to save json Var for all or specific ord. value_length = %d, for %s", value_len, var_name);
      APPLY_OFFSET;
      save_json_var_value(var_name, convert, method, values[actual_ord -1] + offset, value_len - offset, var_len, value_null, local_count_ord, uservartable_entry, req_ord, method_type, total_method); 
    }  
  } 
  return local_count_ord;
}

void copy_all_json_vars(char *var_name, UserVarEntry* uservartable_entry, char convert, int method, int count_ord) 
{
  int j;
  NSLBDL2_SEARCH("Method called. count_ord = %d, method = %d", count_ord, method);
  
  free_var_value(uservartable_entry);
  uservartable_entry->value.array = (ArrayValEntry *)malloc(count_ord * sizeof(ArrayValEntry));
  uservartable_entry->length = count_ord;
  uservartable_entry->flags = uservartable_entry->flags | VAR_IS_VECTOR;// set uservartable_entry->flags as vector

  for (j = 0; j < count_ord; j++)
  {
    uservartable_entry->value.array[j].value = json_tempArrayVal[j].value;
    uservartable_entry->value.array[j].length = json_tempArrayVal[j].length;
    tempArrayVal[j].value = NULL;
    tempArrayVal[j].length = 0;
    NSLBDL3_SEARCH("Saved array value for count ord = %d in user table is = %s for %s", count_ord, uservartable_entry->value.array[j].value, var_name);
  }

  total_json_temparrayval_entries = 0;
}

void process_json_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int blen, int present_depth){

  PerPageJSONVarTableEntry_Shr* perpagejsonvar_ptr;
  char* buf_ptr;
  int count, i;
  int actual_ord, count_ord;
  char *end_ptr; //points to end of buffer 
  int to_stop, page_fail; // Once set, this should remain set
  to_stop = page_fail = 0;
  int string_found_flag = 0;
  UserVarEntry *uservartable_entry;
  int user_var_idx;
  static char *error_var = "";
  userTraceData* utd_node = NULL;

#ifdef NS_DEBUG_ON
  char tmp_buf[1024]; //for json_var_to_str()
#endif
 
  NSDL2_VARS(vptr, NULL, "Method called. vptr->cur_page->first_jsonvar_ptr = %p, present_depth = %d",vptr->cur_page->first_jsonvar_ptr, present_depth);
  
  if(!(vptr->cur_page->first_jsonvar_ptr)) // No JSONVar is defined 
    return;

  NSDL2_VARS(vptr, NULL, "Method called. full_buffer = %s, present_depth = %d", full_buffer, present_depth);

  for(perpagejsonvar_ptr = vptr->cur_page->first_jsonvar_ptr, count = 0; count < vptr->cur_page->num_jsonvar; 
      count++, perpagejsonvar_ptr++)
  {
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Checking json parameter for %s", json_var_to_str(perpagejsonvar_ptr, tmp_buf));
    /*Apply search if  
    *    depth is set 1,2,..n  
    *    OR 
    *    depth = ALL[-2] and is not LAST[-1]
    * special case: if depth=1 or 2 or 3 ... AND this is last depth then apply json var on it.
    * depth = -1 for ReDirectionDepth=LAST will be passed from do_data_processing()
    *  [called from handle_page_complete()].
    * for RedirectionDepth=LAST, jsonvar is only applied if present_depth = -1
    */
    if(((present_depth > 0) && ((perpagejsonvar_ptr->jsonvar_ptr->jsonvar_rdepth_bitmask) & (1 << (present_depth -1)))) ||
       ((perpagejsonvar_ptr->jsonvar_ptr->jsonvar_rdepth_bitmask == VAR_ALL_REDIRECTION_DEPTH) && 
        (present_depth != VAR_IGNORE_REDIRECTION_DEPTH)) || 
       ((present_depth == VAR_IGNORE_REDIRECTION_DEPTH) && 
        (perpagejsonvar_ptr->jsonvar_ptr->jsonvar_rdepth_bitmask == present_depth)))
    {
      NSDL2_VARS(vptr, NULL, "present depth = %d for %s", present_depth, json_var_to_str(perpagejsonvar_ptr, tmp_buf));
      if(perpagejsonvar_ptr->jsonvar_ptr->search == SEARCH_BODY) {
        buf_ptr = full_buffer;
        end_ptr = buf_ptr + blen;
      } else if(perpagejsonvar_ptr->jsonvar_ptr->search == SEARCH_HEADER) {
        NSDL2_VARS(vptr, NULL, "Search is set headers");
        buf_ptr = vptr->response_hdr->hdr_buffer;
        end_ptr = vptr->response_hdr->hdr_buffer + vptr->response_hdr->used_hdr_buf_len;
      } else {
        // For search in a variable, set buf_ptr and end_ptr to var value
        // var is assumed to be NULL terminated as it may be logged in debug log   
        int uservar_idx = vptr->sess_ptr->vars_trans_table_shr_mem[perpagejsonvar_ptr->jsonvar_ptr->search_in_var_hash_code].user_var_table_idx;
        buf_ptr = vptr->uvtable[uservar_idx].value.value;
        end_ptr = buf_ptr + vptr->uvtable[uservar_idx].length;
      }

      // current user variable index 
      user_var_idx = vptr->sess_ptr->vars_trans_table_shr_mem[perpagejsonvar_ptr->jsonvar_ptr->hash_idx].user_var_table_idx;
      uservartable_entry = &vptr->uvtable[user_var_idx];

      // Init json var 
      nslb_json_error error;
      nslb_json_t *json = nslb_json_init_buffer(buf_ptr, 1024, end_ptr - buf_ptr, &error);
      if(json == NULL) {
        NSDL2_VARS(vptr, NULL, "Failed to initialize json, hence we will not extract json here. Hence continue ");
        continue; 
      }
      // Create json tree from json, this tree will be used to traverse and find the value of json vars  
      nslb_jsont *json_tree = nslb_json_to_jsont(json);
      if(json_tree == NULL){
        NSDL2_VARS(vptr, NULL, "Failed to initialize json, hence we will not extract json here. Hence continue ");
        continue; 
      }
       
      // For ORD ANY, we need to find all occurence and then randomly pick one
      // This code will find the count of all occurence and then set cur_ord with random number
      if(perpagejsonvar_ptr->jsonvar_ptr->ord == ORD_ANY || perpagejsonvar_ptr->jsonvar_ptr->ord == ORD_LAST) 
      {
        actual_ord = get_json_var_value(json_tree, perpagejsonvar_ptr->jsonvar_ptr->var_name,
                                             perpagejsonvar_ptr->jsonvar_ptr->json_path,
                                             perpagejsonvar_ptr->jsonvar_ptr->json_path_entries,
                                             perpagejsonvar_ptr->jsonvar_ptr->saveoffset,
                                             perpagejsonvar_ptr->jsonvar_ptr->savelen,
                                             buf_ptr, end_ptr, &string_found_flag, 
                                             ORD_ANY, 
                                             perpagejsonvar_ptr->jsonvar_ptr->convert, 0,
                                             uservartable_entry,
                                             NULL, 0);
        // We have got count for occurence, now we will calculate a random no select the occurence in case of count 
        if (actual_ord) { // If some occurence found  
          NSLBDL2_SEARCH(NULL, NULL,"ORD=ANY, got total %d vars.", actual_ord);
          if (perpagejsonvar_ptr->jsonvar_ptr->ord == ORD_ANY)
            actual_ord = nslb_get_random(actual_ord);
          NSLBDL2_SEARCH(NULL, NULL,"ORD=ANY, Randomly generated actual_ord = %d", actual_ord);
        } else { // If occurence not found then set null value
          // actual_ord will remain with ORD_ANY which is -2
          NSLBDL2_SEARCH(NULL, NULL, "Did not find any occurence of json var for ORD=ANY case");
          actual_ord = -1;
        }
        if(actual_ord == -1) // Not found. (ord starts with 1)
        {
          // Fill empty value
          NS_DT1(vptr, NULL, DM_L1, MM_VARS, "JSON variable not found, actual_ord = %d for %s",
                 actual_ord, json_var_to_str(perpagejsonvar_ptr, tmp_buf));
          save_json_var_value(perpagejsonvar_ptr->jsonvar_ptr->var_name, perpagejsonvar_ptr->jsonvar_ptr->convert,
                                   0, error_var, strlen(error_var), 0, 1, actual_ord, uservartable_entry, ORD_ANY, NULL,0);
        }
      } else // ORD_ALL or particular given ord
         actual_ord = perpagejsonvar_ptr->jsonvar_ptr->ord;

      // Now search to get the requested ORD value or ALL values
      // Pass the random choosen actual_ord so that ORD_ANY can be processed inside this function. 
      // Get the updated count_ord and pass it to copy_all_json_vars() function
      if(perpagejsonvar_ptr->jsonvar_ptr->ord != ORD_ANY_NON_EMPTY)
        count_ord = get_json_var_value(json_tree, perpagejsonvar_ptr->jsonvar_ptr->var_name,
                                            perpagejsonvar_ptr->jsonvar_ptr->json_path,
                                            perpagejsonvar_ptr->jsonvar_ptr->json_path_entries,
                                            perpagejsonvar_ptr->jsonvar_ptr->saveoffset,
                                            perpagejsonvar_ptr->jsonvar_ptr->savelen, buf_ptr, end_ptr,
                                            &string_found_flag, actual_ord, perpagejsonvar_ptr->jsonvar_ptr->convert,
                                            0, uservartable_entry, 
                                            NULL, 0);    

      NSDL2_VARS(vptr, NULL, "count_ord = %d", count_ord);


    // extracted values, so free json here
    nslb_json_free(json);
    // extracted values, so free json tree here
    nslb_jsont_free(json_tree);

      if((perpagejsonvar_ptr->jsonvar_ptr->ord == ORD_ALL) && (count_ord)) // If at least one found
      {
         copy_all_json_vars(perpagejsonvar_ptr->jsonvar_ptr->var_name, uservartable_entry, 
                                perpagejsonvar_ptr->jsonvar_ptr->convert, 0, count_ord);

        for(i = 0; i < count_ord; i++)
        {
          //Cant move before for loop bcoz need to check for Debug Trace also
          if(NS_IF_TRACING_ENABLE_FOR_USER || NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL)
          {
            GET_UT_TAIL_PAGE_NODE
            if(uservartable_entry->value.array[i].value)
            {
              ut_add_param_node(page_node);
              ut_update_param(page_node, perpagejsonvar_ptr->jsonvar_ptr->var_name,
                              uservartable_entry->value.array[i].value, JSON_VAR, i + 1,
                              strlen(perpagejsonvar_ptr->jsonvar_ptr->var_name), 
                              uservartable_entry->value.array[i].length);           
            }
          }
          NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Saved array value for ord = %d in user table is = %s for %s", i,
                 uservartable_entry->value.array[i].value, json_var_to_str(perpagejsonvar_ptr, tmp_buf));
        }
      } else  //Particular or ANY case
      {
          if(uservartable_entry->value.value)
          {
            if(NS_IF_TRACING_ENABLE_FOR_USER || NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL)
            {
              GET_UT_TAIL_PAGE_NODE
              //Always send ut_tail for adding node
              ut_add_param_node(page_node);
              ut_update_param(page_node, perpagejsonvar_ptr->jsonvar_ptr->var_name, 
                              uservartable_entry->value.value, JSON_VAR, count_ord,
                              strlen(perpagejsonvar_ptr->jsonvar_ptr->var_name),
                              uservartable_entry->length);
            }
            NSDL2_VARS(vptr, NULL, "JSON variable found for %s. Value Length is %d, Value = %s",
                   json_var_to_str(perpagejsonvar_ptr, tmp_buf), uservartable_entry->length,            
                   uservartable_entry->value.value);
         }
      }
     //check the status and set the flags.
     json_var_check_status(cptr, vptr, perpagejsonvar_ptr, string_found_flag, &page_fail, &to_stop);
    } else
      NS_DT1(vptr, NULL, DM_L1, MM_VARS, "JSON parameter not applicable for the current redirection depth (%d)",
               present_depth);
  } // for all JSON var end 

  // If at least one JSON var fails, then we need to fail the page and session.
  if(page_fail) {
    set_page_status_for_registration_api(vptr, NS_REQUEST_CV_FAILURE, to_stop, "JSONVar");
  }

}



