/************************************************************************************
 * Name            : ns_ftp_parse.c 
 * Purpose         : This file contains all the ftp parsing related function of netstorm
 * Initial Version : Wednesday, January 06 2010 
 * Modification    : -
 ***********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// #include <sys/socket.h>
// #include <netinet/in.h>  //not needed on IRIX 
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
// #include <sys/select.h>
// #include <netdb.h>
#include <ctype.h>
// #include <dlfcn.h>
// #include <sys/ipc.h>
// #include <sys/shm.h>
// #include <sys/wait.h>
// #include <assert.h>
#include <errno.h>

#include <regex.h>
#include <libgen.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_page.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_ftp_parse.h"
#include "ns_ftp.h"
#include "ns_smtp_parse.h"
#include "ns_script_parse.h"
#include "ns_exit.h"

static short int ftp_save_file_name(int req_index , char *email_id,  int *line_number, int sess_idx)
{
  char *fields[4096]; 
  int num_files, i;//, len;
  int rnum;
  int ret = -1;
  int count;

  if(email_id[0] == '\0')
     return 0;
   
  num_files = get_tokens(email_id, fields, ",", 4096);
  count = num_files;
  for(i = 0; i< count; i++) 
  {
    NSDL3_FTP(NULL, NULL, "i = %d, fields[i] = %s", i, fields[i]);
    if(requests[req_index].proto.ftp.ftp_action_type == FTP_ACTION_RETR)
    {  
      init_post_buf();
      if (copy_to_post_buf(fields[i], strlen(fields[i]))) {
        printf("parse_ftp_send(): Attachment is too big at line=%d\n", *line_number);
        return -1;
      } 

      if(create_post_table_entry(&rnum) != SUCCESS) {
        NS_EXIT(-1, "Error in allocating memory for post table");
      }

      if(requests[req_index].proto.ftp.get_files_idx == -1)
         requests[req_index].proto.ftp.get_files_idx = rnum;
      
      segment_line(&postTable[rnum], g_post_buf, 0, *line_number, sess_idx, "");
    }
    else if(requests[req_index].proto.ftp.ftp_action_type == FTP_ACTION_STOR)
    {
      NSDL3_FTP(NULL, NULL, "i = %d, fields[i] = %s", i, fields[i]);
      ret = access(fields[i], F_OK );
      if(( ret == -1 ) && count <= 1)
      {
        NSDL3_FTP(NULL, NULL, "num_files = %d,inside if fields[i] = %s", num_files, fields[i]);
        NS_EXIT (0, "file [%s]  not found and num_files is [%d] so exiting \n ",fields[i] , num_files);
      }
      else if( ret != -1 )
      {
        NSDL3_FTP(NULL, NULL, "num_files = %d, inside else if fields[i] = %s", num_files, fields[i]);
        init_post_buf();
        if (copy_to_post_buf(fields[i], strlen(fields[i]))) {
          printf("parse_ftp_send(): Attachment is too big at line=%d\n", *line_number);
          return -1;
        }

        if (create_post_table_entry(&rnum) != SUCCESS) {
          NS_EXIT(-1, "Error in allocating memory for post table");
        }
        
        if(requests[req_index].proto.ftp.get_files_idx == -1)
           requests[req_index].proto.ftp.get_files_idx = rnum;
        
        segment_line(&postTable[rnum], g_post_buf, 0, *line_number, sess_idx, "");
     }
     else
     {
      fprintf(stderr, "Unable to locate file [%s]", fields[i]);
      num_files--;
      NSDL3_FTP(NULL, NULL, "num_files = %d,inside else fields[i] = %s", num_files, fields[i]);
     }
    }
  }

 // (num_files != 0)?(return num_files):(exit 0);
 if(num_files)
  {
    return num_files;
  }
  else
  {
    NS_EXIT(0, "Unable to locate file [%s]", fields[i]);
  }
  return 0; 
}

int parse_ftp_get(FILE *cap_fp, int sess_idx, int *line_num) 
{
  int ii, len;
  int function_ends = 0;
  char *line_ptr;
  char line[MAX_LINE_LENGTH];
  time_t tloc;
  char files_buf[MAX_LINE_LENGTH];
  int ftp_server_flag = 0, passwd_flag = 0, user_flag = 0;
  files_buf[0] = '\0';
  int passive_flag = 0;

  (void)time(&tloc);

  if (create_requests_table_entry(&ii) != SUCCESS) {   // Fill request type inside create table entry
      NS_EXIT (-1, "get_url_requets(): Could not create ftp request entry while parsing line %d\n", *line_num);
  }

  proto_based_init(ii, FTP_REQUEST);

  NSDL2_FTP(NULL, NULL, "ii = %d, total_request_entries = %d, total_ftp_request_entries = %d",
                          ii, total_request_entries, total_ftp_request_entries);

  requests[ii].proto.ftp.passive_or_active = FTP_TRANSFER_TYPE_PASSIVE;

  while (nslb_fgets(line, MAX_LINE_LENGTH, cap_fp, 1)) {
    NSDL3_FTP(NULL, NULL, "line = %s", line);

    (*line_num)++;
    line_ptr = line;

    CLEAR_WHITE_SPACE(line_ptr);
    IGNORE_COMMENTS(line_ptr);
   
    if (*line_ptr == '\n')
     continue;

    /* remove the newline character from end of line. */
    if (strchr(line_ptr, '\n')) {
      if (strlen(line_ptr) > 0)
        line_ptr[strlen(line_ptr) - 1] = '\0';
    }

    if(search_comma_as_last_char(line_ptr, &function_ends))
      exit (-1);
    
    if(function_ends && line_ptr[0] == ',')
      break;

    if (!strncmp(line_ptr, "FTP_SERVER", strlen("FTP_SERVER"))) {  // Parametrization = No
      if(ftp_server_flag) {
        fprintf(stderr, "FTP_SERVER can be given once.\n");
        exit(-1);
      }
      ftp_server_flag = 1;
        
      line_ptr += strlen("FTP_SERVER");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         fprintf(stderr, "= expected after FTP_SERVER at line %d.\n", *line_num);
         exit(-1);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      requests[ii].index.svr_idx = get_server_idx(line_ptr, requests[ii].request_type, *line_num);
      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(ii, "Method called from parse_ftp_get");

    } else if (!strncmp(line_ptr, "USER_ID", strlen("USER_ID"))) {  // Parametrization = Yes
      if(user_flag) {
        fprintf(stderr, "USER_ID can be given once.\n");
        exit(-1);
      }
      user_flag = 1;
      line_ptr += strlen("USER_ID");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         fprintf(stderr, "= expected after USER_ID at line %d.\n", *line_num);
         exit(-1);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.ftp.user_id), line_ptr, 0, *line_num, sess_idx, "");

    } else if (!strncmp(line_ptr, "PASSWORD", strlen("PASSWORD"))) {  // Parametrization = Yes
      if(passwd_flag) {
        fprintf(stderr, "PASSWORD can be given once.\n");
        exit(-1);
      }
      passwd_flag = 1;
      line_ptr += strlen("PASSWORD");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         fprintf(stderr, "= expected after PASSWORD at line %d.\n", *line_num);
         exit(-1);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.ftp.passwd), line_ptr, 0, *line_num, sess_idx, "");
    } else if (!strncmp(line_ptr, "PASSIVE", strlen("PASSIVE"))) {  // Parametrization = Yes
      if(passive_flag) {
        fprintf(stderr, "PASSIVE can be given once.\n");
        exit(-1);
      }
      passive_flag = 1;
      line_ptr += strlen("PASSIVE");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         fprintf(stderr, "= expected after PASSIVE at line %d.\n", *line_num);
         exit(-1);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';

      if(!strcasecmp(line_ptr, "N"))
        requests[ii].proto.ftp.passive_or_active = FTP_TRANSFER_TYPE_ACTIVE;
      else if(!strcasecmp(line_ptr, "Y"))
        requests[ii].proto.ftp.passive_or_active = FTP_TRANSFER_TYPE_PASSIVE;
      else{
        fprintf(stderr, "Unexpected option for passive %s, option can be either Y or N", line_ptr);
        exit(-1);
      }
    } else if (!strncmp(line_ptr, "FILE", strlen("FILE"))) {  // Parametrization = Yes
      len = strlen("FILE");
      line_ptr += len;

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         fprintf(stderr, "= expected after TO_EMAILS at line %d.\n", *line_num);
         exit(-1);
      }
      CLEAR_WHITE_SPACE(line_ptr);
      strcat(files_buf, line_ptr); /* Limitation of MAX_LINE_LENGTH */
      
      NSDL4_FTP(NULL, NULL, "files_buf = %s", files_buf);
    } else {
      fprintf(stderr, "Line %d not expected\n", *line_num);
      return -1;
    }

    if(function_ends)
     break;
  }

  requests[ii].proto.ftp.num_get_files = ftp_save_file_name(ii, files_buf, 
                                                            line_num, sess_idx);

  if(!ftp_server_flag) {
     fprintf(stderr, "FTP_SERVER must be given for FTP SESSION for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
     exit (-1);
  }

  if(!user_flag) {
     fprintf(stderr, "USER_ID must be given for FTP SESSION for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
     exit (-1);
  }

  if(!passwd_flag) {
     fprintf(stderr, "PASSWORD must be given for FTP SESSION for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
     exit (-1);
  }

  if(!function_ends) {
    fprintf(stderr, "End of function ftp_send not found\n");
    return -1;
  }

  
  
  gPageTable[g_cur_page].first_eurl = ii;

  return 0;
}

int kw_set_ftp_timeout(char *buf, int *to_change, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;
  int num_args;

  num_args = sscanf(buf, "%s %s %d", keyword, grp, &num_value);

  if(num_args != 3) {
    fprintf(stderr, "Two arguments expected for %s\n", keyword);
    return 1;
  }

  if (num_value <= 0) {
    fprintf(stderr, "Keyword (%s) value must be greater than 0.\n", keyword); 
    return 1;
  }

  *to_change = num_value;
  return 0;
}



/*  Sample */

// script_ln_no is a global varable

int parse_ns_ftp(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx, int ftp_action_type)
{

  int url_idx;
  char files_buf[MAX_LINE_LENGTH];
  int ftp_server_flag = 0, passwd_flag = 0, user_flag = 0, f_flag = 0, type_flag = 0, command_flag = 0;
  files_buf[0] = '\0';
  int passive_flag = 0;
  char *start_quotes;
  char *close_quotes;
  char pagename[MAX_LINE_LENGTH + 1];
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  char *page_end_ptr;
  int ret;
  char * attribute_ptr;
  attribute_ptr = attribute_value;

  create_requests_table_entry(&url_idx);   // Fill request type inside create table entry

  proto_based_init(url_idx, FTP_REQUEST);
  
  NSDL2_FTP(NULL, NULL, "url_idx = %d, total_request_entries = %d, total_ftp_request_entries = %d",
                          url_idx, total_request_entries, total_ftp_request_entries);

  requests[url_idx].proto.ftp.passive_or_active = FTP_TRANSFER_TYPE_PASSIVE; // default is Passive

  ret = extract_pagename(flow_fp, flow_filename, script_line, pagename, &page_end_ptr);
  if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

  // For FTP, we are internally using ns_web_url API
    if((parse_and_set_pagename((ftp_action_type==FTP_ACTION_RETR)?"ns_ftp_get":"ns_ftp_put", "ns_web_url", flow_fp, flow_filename, script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  gPageTable[g_cur_page].first_eurl = url_idx;
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  close_quotes = page_end_ptr;
  start_quotes = NULL;
  // Point to next argument
  ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
  
  //This will return if start quotes of next argument is not found or some other printable
  //is found including );
  if(ret == NS_PARSE_SCRIPT_ERROR)
  {
    SCRIPT_PARSE_ERROR(script_line, "Syntax error");
    return NS_PARSE_SCRIPT_ERROR;
  }
  
  while (1) 
  {
    NSDL3_FTP(NULL, NULL, "line = %s", script_line);

    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes,0);
   
    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

    if (!strcmp(attribute_name, "FTP_SERVER")) // Parametrization is not allowed for this argument
    { 
      if(ftp_server_flag) { SCRIPT_PARSE_ERROR(NULL, "FTP_SERVER can be given once."); }

      ftp_server_flag = 1;
      requests[url_idx].index.svr_idx = get_server_idx(attribute_value, requests[url_idx].request_type, script_ln_no);
      NSDL2_FTP(NULL, NULL, "FTP: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].index.svr_idx);
      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from parse_ns_ftp");
    } 
    else if (!strcmp(attribute_name, "USER_ID"))  // Parametrization is allowed for this argument
    { 
      if(user_flag) { SCRIPT_PARSE_ERROR(NULL, "USER_ID can be given once.\n"); }
      user_flag = 1;
      segment_line(&(requests[url_idx].proto.ftp.user_id), attribute_value, 0, script_ln_no, sess_idx, "");
      NSDL2_FTP(NULL, NULL, "FTP: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.ftp.user_id);
    } 
    else if (!strcmp(attribute_name, "PASSWORD")) // Parametrization is allowed for this argument
    {
      if(passwd_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "PASSWORD");
      }
     passwd_flag = 1;
     segment_line(&(requests[url_idx].proto.ftp.passwd), attribute_value, 0, script_ln_no, sess_idx, "");
     NSDL2_FTP(NULL, NULL, "FTP: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.ftp.passwd);
    }else if (!strcmp(attribute_name, "CMD")) // Parametrization is allowed for this argument
    {
      if(command_flag)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "COMMAND");
      }

      command_flag = 1;
      CLEAR_WHITE_SPACE(attribute_ptr);
      NSDL2_FTP(NULL, NULL, "FTP: Value of command is = [%s]", attribute_ptr);
      
      segment_line(&(requests[url_idx].proto.ftp.ftp_cmd), attribute_ptr, 0, script_ln_no, sess_idx, "");
    }else if (!strcmp(attribute_name, "PASSIVE"))
    {  
      if(passive_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "PASSIVE");
      }
      passive_flag = 1;
      if(!strcasecmp(attribute_value, "N"))
      requests[url_idx].proto.ftp.passive_or_active = FTP_TRANSFER_TYPE_ACTIVE;
      else if(!strcasecmp(attribute_value, "Y"))
      requests[url_idx].proto.ftp.passive_or_active = FTP_TRANSFER_TYPE_PASSIVE;
      else
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012220_ID, CAV_ERR_1012220_MSG, attribute_value, "Passive", "Y", "N");
      }
      NSDL2_FTP(NULL, NULL, "FTP: Value of %s = %s, passive_or_active = %d", attribute_name, attribute_value, requests[url_idx].proto.ftp.passive_or_active);
    } 
     else if (!strcmp(attribute_name, "FILE"))  // Parametrization is allowed for this argument
    {
      f_flag = 1; 
      strcat(files_buf, attribute_value); /* Limitation of MAX_LINE_LENGTH */
      strcat(files_buf, ","); /* Limitation of MAX_LINE_LENGTH */
      NSDL4_FTP(NULL, NULL, "files_buf = %s", files_buf);
    }else if (!strcmp(attribute_name, "TYPE"))
    {  
      if(type_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "TYPE");
      }
      type_flag = 1;
      if(!strcasecmp(attribute_value, "A"))
      requests[url_idx].proto.ftp.file_type = 0;
      else if(!strcasecmp(attribute_value, "I"))
      requests[url_idx].proto.ftp.file_type = 1;
      else
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012220_ID, CAV_ERR_1012220_MSG, attribute_value, "TYPE", "A", "I");
      }

      NSDL2_FTP(NULL, NULL, "FTP: Value of %s = %s, TYPE = %d", attribute_name, attribute_value, requests[url_idx].proto.ftp.file_type);
    } 
    else
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012219_ID, CAV_ERR_1012219_MSG, attribute_name);
    }

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    //In case next comma not found between quotes or end_of_file found 
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_FTP(NULL, NULL, "Next attribute is not found");
      break;
    }
  }

  if(start_quotes == NULL)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012209_ID, CAV_ERR_1012209_MSG, "FTP");
  }
  else
  {
      //Checking Function Ending
    if(!strncmp(start_quotes, ");", 2))
    {
      NSDL2_FTP(NULL, NULL, "End of function ns_ftp_get found %s", start_quotes);
    }
    else 
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012210_ID, CAV_ERR_1012210_MSG, start_quotes);
    }
  }

  requests[url_idx].proto.ftp.ftp_action_type = ftp_action_type;
  requests[url_idx].proto.ftp.num_get_files = ftp_save_file_name(url_idx, files_buf, 
                                                            &script_ln_no, sess_idx);

  // Validate all mandatory arguments are given
  if(!ftp_server_flag) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "FTP_SERVER", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if(!user_flag) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "USER_ID", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if(!passwd_flag) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "PASSWORD", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
  if(!f_flag)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "FILE", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
  return  NS_PARSE_SCRIPT_SUCCESS;
}

int parse_ns_ftp_get(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  return  parse_ns_ftp( flow_fp, outfp, flow_filename, flowout_filename, sess_idx, FTP_ACTION_RETR);
}

int parse_ns_ftp_put(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  return  parse_ns_ftp( flow_fp, outfp, flow_filename, flowout_filename, sess_idx, FTP_ACTION_STOR);
}

