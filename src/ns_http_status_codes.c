/************************************************************************************
* Name      : ns_http_status_codes.c 
* Purpose   : This file contains functions related to HTTP response code, file driven feature 
*             eg: parsing function, page execution function
* Author(s) : Neeraj/Shikha
* Date      : Dec 1st, 2018
* Document: Refer to Req/HLD doc in CVSDocs in cavisson/docs/Products/NetStorm/TechDocs/ProtoHTTP/Req/HTTPProtoRespCodeMatricesReq.doc
* Modification History :
***********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>

#include "url.h"

#include "ns_msg_def.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_fdset.h"
#include "ns_http_version.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_group_data.h"
#include "amf.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_http_status_codes.h"
#include "nslb_util.h"
#include "ns_test_init_stat.h"
#include "ns_dynamic_avg_time.h"
#include "ns_script_parse.h"

#define FULL_DYN_STATUS_CODE            0
#define PARTIAL_DYN_STATUS_CODE         1
#define DYN_STATUS_CODE_LOC2_NORM_DELTA_SIZE     5

#ifndef CAV_MAIN
int http_resp_code_avgtime_idx = -1;                   // Index of HTTPRespCodeAvgTime structure
HTTP_Status_Code_loc2norm_table *g_http_status_code_loc2norm_table = NULL;
HTTPRespCodeAvgTime *http_resp_code_avgtime = NULL;
#else
__thread int http_resp_code_avgtime_idx = -1;                   // Index of HTTPRespCodeAvgTime structure
__thread HTTP_Status_Code_loc2norm_table *g_http_status_code_loc2norm_table = NULL;
__thread HTTPRespCodeAvgTime *http_resp_code_avgtime = NULL;
#endif
static HTTPRespCodeInfo *http_resp_code_arr;                   //HTTPRespCodeInfo Structure pointer
static int max_http_resp_codes_for_info = INIT_HTTP_STATUS_CODE_ARR_SIZE;

static signed char *resp_code_mapping_tbl = NULL;
int total_http_resp_codes = 0; // Total number of HTTP status codes configured e.g. 41

int max_http_resp_code = 0; // Maximum http status code configured e.g. 505


NormObjKey status_code_normtbl;

static int validate_status_code_display_name(char *str, int length)
{
  int i;

  for(i = 0; i < length; i++)
  {
    if((isalnum(str[i])) || (str[i] == '.') || (str[i] == '_') || (str[i] == '-') || (str[i] == '\n'))
    {
      if(str[i] == '\n')
      {
        str[i] = '\0';
        return 1; //success case
      }
      continue;
    }
    else
      return 0; // error case
  }
  return 0; // error case
}

/********************************************************************************
 Function    : ns_init_http_status_code_loc2norm_table()	                *
 Purpose     : initialise loc2norm table of HTTP Response Code   		*
             : initialise loc2norm table of HTTP Response Code                  *
 Called from : init_all_avgtime()                                               *
********************************************************************************/
void ns_init_http_status_code_loc2norm_table(int entries)
{
  int nvmindex = 0;
  int num_process;
  int i;
  NSDL2_HTTP(NULL, NULL, "Method called , entries = %d global_settings->num_process = %d, "
                         "sgrp_used_genrator_entries = %d, g_http_status_code_loc2norm_table = %p",
                          entries, global_settings->num_process, sgrp_used_genrator_entries, g_http_status_code_loc2norm_table);
  

  //Bug 69239: On init, num_process is input from NUM_NVM keyword, for NC use sgrp_used_genrator_entries
  num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);

  MY_MALLOC_AND_MEMSET(g_http_status_code_loc2norm_table, num_process * sizeof(HTTP_Status_Code_loc2norm_table), "HTTP_Status_Code_loc2norm_table", (int)-1);

  for(nvmindex = 0; nvmindex < num_process; nvmindex++)
  {
    MY_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_http_status_code_loc2norm_table[nvmindex].nvm_http_status_code_loc2norm_table, entries * sizeof(int), "nvm_http_status_code_loc2norm_table", (int)-1); 
    g_http_status_code_loc2norm_table[nvmindex].loc2norm_http_status_code_alloc_size = entries;
    g_http_status_code_loc2norm_table[nvmindex].loc_http_status_code_avg_idx = http_resp_code_avgtime_idx;
    g_http_status_code_loc2norm_table[nvmindex].num_entries = entries; //initializing with 9 entries
    g_http_status_code_loc2norm_table[nvmindex].last_gen_local_norm_id = entries;
    for(i = 0; i < entries; i++)
      g_http_status_code_loc2norm_table[nvmindex].nvm_http_status_code_loc2norm_table[i] = i;
  }
  NSDL2_HTTP(NULL, NULL, "Method End");
}

/********************************************************************************
 Function    : update_http_resp_code_avgtime_size() 		                *
 Purpose     : increments avg_size with size of HTTPRespCodeAvgTime   		*
             : initialise loc2norm table of HTTP Response Code                  *
 Called from : init_all_avgtime()                                               *
*********************************************************************************/
inline void update_http_resp_code_avgtime_size() 
{
  NSDL2_HTTP(NULL, NULL, "Method Called, g_avgtime_size = %d, http_resp_code_avgtime_idx = %d, total_http_resp_code_entries = %d",
                         g_avgtime_size, http_resp_code_avgtime_idx, total_http_resp_code_entries);
  
  http_resp_code_avgtime_idx = g_avgtime_size;
  g_avgtime_size += (sizeof(HTTPRespCodeAvgTime) * max_http_resp_code_entries);

  //Allocating normalization map table for status codes and Initialising g_http_status_code_loc2norm_table table
  ns_init_http_status_code_loc2norm_table(total_http_resp_code_entries);
  
  NSDL2_HTTP(NULL, NULL, "After g_avgtime_size = %d, http_resp_code_avgtime_idx = %d",
                  g_avgtime_size, http_resp_code_avgtime_idx);
}

/************************************************************************
 Function    : ns_add_dyn_status_code() 		                *
 Purpose     : fills g_http_status_code_loc2norm_table         		*
 Called from : process_new_object_discovery_record()                    *
************************************************************************/
int ns_add_dyn_status_code(short nvmindex, int local_dyn_status_code_id, char *status_name, short status_name_len, int *flag_new)
{ 
   NSDL1_HTTP(NULL, NULL, "Method called, nvmindex = %d, local_dyn_status_code_id = %d, status_name = %s status_name_len = %d, "
                          "loc2norm_http_status_code_alloc_size= %d",
                          nvmindex, local_dyn_status_code_id, status_name, 
                          status_name_len, g_http_status_code_loc2norm_table[nvmindex].loc2norm_http_status_code_alloc_size);

   int norm_resp_code_id, num_process, i;
  
   num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);
 
   if(!status_name || nvmindex < 0 || local_dyn_status_code_id < 0)
   {
     NSTL1(NULL, NULL, "Status Code addition failed (Error: Invalid argument) status_name = %s "
                       "or nvmindex = %d or local_dyn_status_code_id = %d", 
                        status_name, nvmindex, local_dyn_status_code_id);
     return 0;    //for any error case, setting '0' i.e., Others
   }

   if(total_http_resp_code_entries >= g_http_status_code_loc2norm_table[nvmindex].loc2norm_http_status_code_alloc_size)
   {
     int old_size = g_http_status_code_loc2norm_table[nvmindex].loc2norm_http_status_code_alloc_size * sizeof(int);
     int new_size = (total_http_resp_code_entries + DYN_STATUS_CODE_LOC2_NORM_DELTA_SIZE) * sizeof(int);
     
     NSTL1(NULL, NULL, "local_dyn_status_code_id is Greater than the size of g_http_status_code_loc2norm_table ,"
                       "So reallocating this table with New_size = %d, Old_size = %d for NVM = %d", new_size, old_size, nvmindex);
    
     for(i = 0; i < num_process; i++){ 
       MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(g_http_status_code_loc2norm_table[i].nvm_http_status_code_loc2norm_table, new_size, old_size, "g_http_status_code_loc2norm_table", i);   
       g_http_status_code_loc2norm_table[i].loc2norm_http_status_code_alloc_size = total_http_resp_code_entries + DYN_STATUS_CODE_LOC2_NORM_DELTA_SIZE;
     }
   }
   
   norm_resp_code_id = nslb_get_or_set_norm_id(&status_code_normtbl, status_name, status_name_len, flag_new);

   if(g_http_status_code_loc2norm_table[nvmindex].nvm_http_status_code_loc2norm_table[local_dyn_status_code_id] == -1){
     g_http_status_code_loc2norm_table[nvmindex].num_entries++;
     g_http_status_code_loc2norm_table[nvmindex].dyn_total_entries++;
   }

   g_http_status_code_loc2norm_table[nvmindex].nvm_http_status_code_loc2norm_table[local_dyn_status_code_id] = norm_resp_code_id;
   
   if(*flag_new)
   {
     dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].total++;  //It will store total discovered status codes btwn every progress interval
     check_if_realloc_needed_for_dyn_obj(NEW_OBJECT_DISCOVERY_STATUS_CODE);
   } 
   
   NSTL1(NULL, NULL, "HTTPResponseCode: on new discovery - "
                      "nvmindex = %d, status_name = %s, "
                      "g_http_status_code_loc2norm_table[%d].nvm_http_status_code_loc2norm_table[%d] = %d, "
                      "num_entries = %d, dyn_total_entries = %d, total = %d", 
                       nvmindex, status_name, nvmindex, local_dyn_status_code_id, 
                       g_http_status_code_loc2norm_table[nvmindex].nvm_http_status_code_loc2norm_table[local_dyn_status_code_id],
                       g_http_status_code_loc2norm_table[nvmindex].num_entries,
                       g_http_status_code_loc2norm_table[nvmindex].dyn_total_entries, 
                       dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].total);

   return norm_resp_code_id;
}

/************************************************************************
 Function    : static void init_resp_code_mapping_tbl()                 *
 Purpose     : Fills the response code mapping table         		*
 Called from : init_http_response_codes()                               *
************************************************************************/
static void init_resp_code_mapping_tbl()
{
  NSDL1_HTTP(NULL, NULL, "Method Called. max_http_resp_code = %d, total_http_resp_codes = %d",
                          max_http_resp_code, total_http_resp_codes);

  MY_MALLOC_AND_MEMSET_WITH_MINUS_ONE(resp_code_mapping_tbl, (MAX_HTTP_RESP_CODE + 1), "resp_code_mapping_tbl", -1);
}

void create_status_code_info_table()
{
  NSDL3_HTTP(NULL, NULL, "Method called, total_http_resp_codes = %d, max_http_resp_codes_for_info = %d",
                         total_http_resp_codes, max_http_resp_codes_for_info);

  //It should be incremented before checking and status code will start filling from index 1 as 0 is reserved for 'others'
  total_http_resp_codes++;

  if(total_http_resp_codes >= max_http_resp_codes_for_info)
  {
    max_http_resp_codes_for_info += DELTA_STATUS_CODE_ENTRIES;

    NSDL3_HTTP(NULL, NULL, "max_http_resp_codes_for_info = %d", max_http_resp_codes_for_info);

    MY_REALLOC(http_resp_code_arr, (sizeof(HTTPRespCodeInfo) * max_http_resp_codes_for_info), "http_resp_code_arr", -1);
  }
}

/* function will do normalisation of status code and text*/
int ns_handle_dyn_status_code(VUser *vptr,char *status_txt, int len)
{
  NSDL3_HTTP(NULL, NULL, "Method called");

  int is_new_status_code;
  int norm_id; 
  static int row_num = -1;

  //Fetch norm_id acording to the vector
  norm_id = nslb_get_or_set_norm_id(&status_code_normtbl, status_txt, len, &is_new_status_code);
 
  if(is_new_status_code)
  {

    NSDL4_HTTP(vptr, NULL, "New HTTP Response Code discovered, len = %d, vector name = %s, norm_id = %d",
                           len, status_txt, norm_id);
 
    int old_avg_size = g_avgtime_size;
    create_dynamic_data_avg(&g_child_msg_com_con, &row_num, my_port_index, NEW_OBJECT_DISCOVERY_STATUS_CODE);
  
    //send local_norm_id to parent
    send_new_object_discovery_record_to_parent(vptr, len, status_txt, NEW_OBJECT_DISCOVERY_STATUS_CODE, norm_id);

    check_if_need_to_realloc_connection_read_buf(&g_dh_child_msg_com_con, my_port_index, old_avg_size, NEW_OBJECT_DISCOVERY_STATUS_CODE);
  }

  NSDL3_HTTP(NULL, NULL, "Method Exit");
  return (norm_id<0?0:norm_id);
}

/*************************************************************************/
/* Function    : ns_parse_usr_dfnd_file_for_status_code()                */
/* Purpose     : This will read                                          */
/*                 $NS_WDIR/scenario/<proj>/<sub-proj>/<scenario>/       */
/*                 http_response_codes.dat file                          */
/*             : After reading, parse and make data table for            */
/*             : response code.                                          */
/* Called from : ns_parse_system_file_for_status_code()                  */
/*************************************************************************/
int ns_parse_usr_dfnd_file_for_status_code()
{
  char resp_code_scen_file_buf[MAX_FILE_NAME + 1];
  char line[MAX_FILE_NAME + 1];
  char *ptr; 
  int line_num = 0;
  int dyn_status_code;
  int dyn_status_code_bit = 0;
  int str_len;
  FILE *dyn_status_code_file_fp;

  NSDL2_HTTP(NULL, NULL, "Method called");
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scenarios dir*/
  //Need to check if file present at $NS_WDIR/scenarios/<proj>/<sub-proj>/<scenario>/http_response_codes.dat
  snprintf(resp_code_scen_file_buf, MAX_FILE_NAME, "%s/%s/%s/%s/%s/http_response_codes.dat", GET_NS_TA_DIR(), g_project_name, g_subproject_name, "scenarios", g_scenario_name);
  NSDL2_HTTP(NULL, NULL, "resp_code_scen_file_buf = %s", resp_code_scen_file_buf);
  dyn_status_code_file_fp = fopen(resp_code_scen_file_buf, "r");

  if(dyn_status_code_file_fp == NULL)
  {
    NSDL2_HTTP(NULL, NULL, "file '%s' have no entry or have permission issues", resp_code_scen_file_buf);
    NSDL2_HTTP(NULL, NULL, "So, HTTP Response code will be fully dynamic");
    return FULL_DYN_STATUS_CODE;
  }

  //Comes only when status code is defined for particular scenario
  while(nslb_fgets(line, MAX_LINE_LENGTH, dyn_status_code_file_fp, 0))
  {
    line_num++;
    if(line[0] == '#')
    {
      NSDL2_HTTP(NULL, NULL, "Line started with '#', So, considering it to be comment, line = [%s]", line);
      continue;
    }

    if(!(ptr = index(line, '|')))
    {
      NSDL2_HTTP(NULL, NULL, "line do not contains '|', hence ignoring line number = %d", line_num);
      continue;
    }

    *ptr = '\0';

    if(!ns_is_numeric(line))
    {
      NS_EXIT(-1, "Visibility bit can only be numeric");
    }

    dyn_status_code_bit = atoi(line);
    if(dyn_status_code_bit > 1)
    {
      NS_EXIT(-1, "Visibility Bit cannot be greater than one");
    }

    ptr = ptr + 1; // Point after | to extract status code
    strcpy(line, ptr);
    NSDL2_HTTP(NULL, NULL, "status code and text is = %s", line);

    if(!(ptr = index(line, '|')))
    {
      NSDL2_HTTP(NULL, NULL, "status code and status text is not in valid format need pipeline(|) at line : %d", line_num);
      continue;
    }

    *ptr = '\0';

    if(!ns_is_numeric(line))
    {
      NS_EXIT(-1, "HTTP Status code (%s) is not numeric  or negative at line number = %d", line, line_num);
    }

    dyn_status_code = atoi(line);
    if(dyn_status_code < 0 || dyn_status_code > MAX_HTTP_RESP_CODE)
    {
      NS_EXIT(-1, "HTTP Status Code at line num [%d] of file [%s] is [%d], which is -ve or greater than max http code [%d]", 
                           line_num, resp_code_scen_file_buf, dyn_status_code, MAX_HTTP_RESP_CODE);
    }
    
    ptr = ptr + 1; // Point after | to display name
    str_len = strlen(ptr);

    if(!validate_status_code_display_name(ptr, str_len))
    {
      NS_EXIT(-1, "only '-', '_', '.' special character allowed at line number = %d", line_num);
    }

    if(str_len <= 0 || str_len > MAX_HTTP_CODE_DISPLAY_NAME)
    {
      NS_EXIT(-1, "HTTP Response Code display name at line num [%d] of file [%s] is [%s], which is 0 or greater than max length [%d]", 
                           line_num, dyn_status_code_file_fp, ptr, MAX_HTTP_CODE_DISPLAY_NAME);
    }


    NSDL2_HTTP(NULL, NULL, "For status code [%d] we have mapping table entry as [%d]", 
                            dyn_status_code, resp_code_mapping_tbl[dyn_status_code]);

    if(resp_code_mapping_tbl[dyn_status_code] == -1)
    {
      //when everything goes well, realloc info table if needed.
      create_status_code_info_table();

      resp_code_mapping_tbl[dyn_status_code] = total_http_resp_codes; 

      if(dyn_status_code > max_http_resp_code)
        max_http_resp_code = dyn_status_code;  // assigning the max codes encountered

      NSDL2_HTTP(NULL, NULL, "for status code [%d] max_http_resp_code = [%d]", dyn_status_code, max_http_resp_code);
    }

    // Use memcpy for faster copy
    snprintf(http_resp_code_arr[resp_code_mapping_tbl[dyn_status_code]].status_text, str_len + 1, "%s", ptr);
    http_resp_code_arr[resp_code_mapping_tbl[dyn_status_code]].status_text[str_len] = '\0';
    http_resp_code_arr[resp_code_mapping_tbl[dyn_status_code]].display = dyn_status_code_bit;
    http_resp_code_arr[resp_code_mapping_tbl[dyn_status_code]].status_code_norm_idx = -1;

    NSDL2_HTTP(NULL, NULL, "Processed line_num = %d, dyn_status_code = %d, display name = %s for norm_idx = %d, "
                            "dyn_status_code_bit = %d, max_http_resp_code = %d", 
                           line_num, dyn_status_code, http_resp_code_arr[resp_code_mapping_tbl[dyn_status_code]].status_text,
                           http_resp_code_arr[resp_code_mapping_tbl[dyn_status_code]].status_code_norm_idx, 
                           dyn_status_code_bit, max_http_resp_code);

  }  

  if(fclose(dyn_status_code_file_fp) < 0)
    NS_EXIT(-1, "Not able to close File [%s], error = %s", resp_code_scen_file_buf, nslb_strerror(errno));

  NSDL2_HTTP(NULL, NULL, "Method End");

  return PARTIAL_DYN_STATUS_CODE;    
}


/*************************************************************************/
/* Function    : ns_parse_system_file_for_status_code()                  */
/* Purpose     : This will read sys/http_response_codes.dat file         */
/*             : After reading, parse and make data table for            */
/*             : response code.                                          */
/* Called from : init_http_response_codes()                              */
/*************************************************************************/
static void ns_parse_system_file_for_status_code()
{
  char resp_code_file_buf[MAX_FILE_NAME];
  char line[MAX_LINE_LENGTH];
  char *ptr;
  int line_num = 0;
  int http_status_code = 0;
  FILE *resp_code_fp = NULL;
  int dyn_status_code_bit = 0;
  int str_len;

  NSDL2_HTTP(NULL, NULL, "Method Called");

  sprintf(resp_code_file_buf, "%s/sys/http_response_codes.dat", g_ns_wdir);

  resp_code_fp = fopen(resp_code_file_buf, "r");
  if(resp_code_fp == NULL)
  {
    NS_EXIT(-1, "Failed to open HTTP Status Code File [%s], error:%s", resp_code_file_buf, nslb_strerror(errno));
  }
 
  //Need to malloc with 50 for first time
  MY_MALLOC(http_resp_code_arr, (sizeof(HTTPRespCodeInfo) * max_http_resp_codes_for_info), "http_resp_code_arr", -2);

  // Handling for others, here total_http_resp_codes is '0'
  strcpy(http_resp_code_arr[total_http_resp_codes].status_text, HTTP_STATUS_CODE_OTHERS);
  http_resp_code_arr[total_http_resp_codes].status_code_norm_idx = -1;
  http_resp_code_arr[total_http_resp_codes].display = 1;

  //No normtable entry for system file
  while(nslb_fgets(line, MAX_LINE_LENGTH, resp_code_fp, 0))
  {
    line_num++;
    if(line[0] == '#')
    {
      NSDL2_HTTP(NULL, NULL, "Line started with '#', So, considering it to be comment, line = [%s]", line);
      continue;
    }

    if(!(ptr = index(line, '|')))
    {
      NSDL2_HTTP(NULL, NULL, "line do not contains '|', hence ignoring line number = %d", line_num);
      continue;
    }

    *ptr = '\0';

    if(!ns_is_numeric(line))
    {
      NS_EXIT(-1, "HTTP Status code (%s) is not numeric  or negative at line number = %d", line, line_num);
    }

    dyn_status_code_bit = atoi(line);
   
    if(dyn_status_code_bit > 1)
    {
      NS_EXIT(-1, "Visibility Bit cannot be greater than one");
    }

    ptr = ptr + 1; // Point after | to extract status code
    strcpy(line, ptr);
    NSDL2_HTTP(NULL, NULL, "status code and text is = %s", line);

    if(!(ptr = index(line, '|')))
    {
      NSDL2_HTTP(NULL, NULL, "line do not contains '|', hence ignoring line number = %d", line_num);
      continue;
    }

    *ptr = '\0';

    if(!ns_is_numeric(line))
    {
      NS_EXIT(-1, "HTTP Status code (%s) is not numeric  or negative at line number = %d", line, line_num);
    }

    http_status_code = atoi(line);
    if(http_status_code < 0 || http_status_code > MAX_HTTP_RESP_CODE)
    {
      NS_EXIT(-1, "HTTP Status Code at line num [%d] of file [%s] is [%d], which is -ve or greater than max http code [%d]", 
                           line_num, resp_code_file_buf, http_status_code, MAX_HTTP_RESP_CODE);
    }

    //to Keep track on maximum status code till encountered
    if(http_status_code > max_http_resp_code)
      max_http_resp_code = http_status_code;  // assigning the max codes encountered

    ptr = ptr + 1; // Point after | to display name
    str_len = strlen(ptr);

    if(!validate_status_code_display_name(ptr, str_len))
    {
      NS_EXIT(-1, "only '-', '_', '.' special character allowed at line number = %d", line_num);
    }

    if(str_len <= 0 || str_len > MAX_HTTP_CODE_DISPLAY_NAME)
    {
      NS_EXIT(-1, "HTTP Response Code display name at line num [%d] of file [%s] is [%s], which is 0 or greater than max length [%d]", 
                           line_num, resp_code_file_buf, ptr, MAX_HTTP_CODE_DISPLAY_NAME);
    }

    //if needed realloc size for 5 more size
    create_status_code_info_table();

    // Use memcpy for faster copy
    //memcpy(http_resp_code_arr[total_http_resp_codes].status_text, ptr, str_len + 1);
    snprintf(http_resp_code_arr[total_http_resp_codes].status_text, str_len + 1, "%s", ptr);
    http_resp_code_arr[total_http_resp_codes].status_text[str_len] = '\0';
    http_resp_code_arr[total_http_resp_codes].status_code_norm_idx = -1;
    http_resp_code_arr[total_http_resp_codes].display = dyn_status_code_bit;
    resp_code_mapping_tbl[http_status_code] = total_http_resp_codes; 
	
    NSDL2_HTTP(NULL, NULL, "Processed line_num = %d, http_status_code = %d, display name = %s for norm_idx = %d", 
                            line_num, http_status_code, http_resp_code_arr[total_http_resp_codes].status_text,
                            http_resp_code_arr[total_http_resp_codes].status_code_norm_idx);
  }
  
  if(fclose(resp_code_fp) < 0)
    NS_EXIT(-1, "Not able to close File [%s], error = %s", resp_code_file_buf, nslb_strerror(errno));

  if(total_http_resp_codes == 0)
    NS_EXIT(-1, "No valid entry found for Response code in files [%s]", resp_code_file_buf);

  if(total_http_resp_codes > MAX_HTTP_RESP_CODES_ALLOWED)
    NS_EXIT(-1, "HTTP Response Code in file %s cannot be more than [%d]", resp_code_file_buf, MAX_HTTP_RESP_CODES_ALLOWED);

  NSDL2_HTTP(NULL, NULL, "total_http_resp_codes = %d, max_http_resp_code = %d", total_http_resp_codes, max_http_resp_code);

  NSDL2_HTTP(NULL, NULL, "Method End");
}

/*************************************************************************/
/* Function    : init_http_response_codes()                              */
/* Purpose     : This will read system file and user defined file        */
/* Called from : parse_file()                                            */
/*************************************************************************/
void init_http_response_codes()
{
  NSDL2_HTTP(NULL, NULL, "Method called");
  int i;
  int is_new;
  total_http_resp_code_entries = 0;
  max_http_resp_code_entries = 0;

  nslb_init_norm_id_table_ex(&status_code_normtbl, INIT_HTTP_STATUS_CODE_ARR_SIZE);

  //create mapping table for storing infoTable against status-code
  init_resp_code_mapping_tbl();

  ns_parse_system_file_for_status_code();

  //TODO 4.1.15 : Make a function for static entry i.e., coming from <scenario_name>/http_response_code.dat 
  //add norm_id for all which have discovery bit
  //make object of &resp_code_normtbl to use norm_id_table for HTTP Reponse Code
  //if fully dynamic : norm_id will be on runtime 
  ns_parse_usr_dfnd_file_for_status_code();

  for(i = 0; i <= total_http_resp_codes; i++)
  {
    if(http_resp_code_arr[i].display)
    {
      //If discovery is bit , set the norm_id
      http_resp_code_arr[i].status_code_norm_idx = nslb_get_or_set_norm_id(&status_code_normtbl, http_resp_code_arr[i].status_text, strlen( http_resp_code_arr[i].status_text), &is_new);
      total_http_resp_code_entries++;
    }
  }
  //here total_http_resp_code_entries will have count of static status codes.
  //Keeping max as total, so that mem move can be done for static status code.
  max_http_resp_code_entries = total_http_resp_code_entries;

  //Re-setting resp_code_mapping_tbl from -1 to 0 for 'Others' so that uncondigured status code directly map to others 
  for(i = 0; i < MAX_HTTP_RESP_CODE + 1; i++)
  {
    if(resp_code_mapping_tbl[i] == -1)
      resp_code_mapping_tbl[i] = 0;
  }
  NSDL2_HTTP(NULL, NULL, "Method exits, max_http_resp_code_entries = %d", max_http_resp_code_entries);
}

/****************************************************************************************               
 Purpose: writing dynamic discoverd response code in gdf file                           *
          Called from print_resp_status_code_gdf_grp_vectors()                          *
 Arguments :                                                                            *
   TwoD    : Store data                                                                 *
   Idx2d   : Index of TwoD     		                                                *
   prefix  : NC Case Handling for hierarchy                                             *
   groupID : Group ID of Graph in GDF                                                   *
   genId   : generator ID is case of NC, else -1                                        * 
****************************************************************************************/   
static void print_status_code_graph(char **TwoD , int *Idx2d, char *prefix, int groupId, int genId)
{
  char buff[MAX_VAR_SIZE + 1];
  char vector_name[MAX_VAR_SIZE + 1]; 
  char *name;
  int i = 0;
  int dyn_obj_idx, count;
  dyn_obj_idx = 0;
  count = 0;

  NSDL2_GDF(NULL, NULL, " Method called. Idx2d = %d, prefix = %s, genId = %d, groupId = %d", *Idx2d, prefix, genId, groupId);
  
  if(groupId == HTTP_STATUS_CODE_RPT_GRP_ID)
  { 
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_STATUS_CODE;
    count = dynObjForGdf[dyn_obj_idx].total + dynObjForGdf[dyn_obj_idx].startId;
    NSDL2_GDF(NULL, NULL, "HTTP_STATUS_CODE_RPT_GRP_ID : total = %d, strtId = %d, count = %d", 
                          dynObjForGdf[dyn_obj_idx].total, dynObjForGdf[dyn_obj_idx].startId, count);
  } 
  
  for(i = 0; i < count; i++)
  { 
    name = nslb_get_norm_table_data(dynObjForGdf[dyn_obj_idx].normTable, i);
    
    if(g_runtime_flag == 0)
    { 
      dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i] = msg_data_size + ((dynObjForGdf[dyn_obj_idx].rtg_group_size) * (*Idx2d));     
      NSDL2_GDF(NULL, NULL, "RTG index set for NS/NC Controller/GeneratorId = %d, and Name = %s is %d. Index of DynObjForGdf = %d", genId, name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i], dyn_obj_idx);
    }

    snprintf(vector_name, MAX_VAR_SIZE, "%s%s", prefix, name);
    snprintf(buff, MAX_VAR_SIZE, "%s %d", vector_name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i]);
    fprintf(write_gdf_fp, "%s\n", buff);
    
    fill_2d(TwoD, *Idx2d, vector_name);
    *Idx2d = *Idx2d  + 1; 
    NSDL2_GDF(NULL, NULL, "Idx2d = %d", *Idx2d);
  }
}

/****************************************************************************************               
 Purpose: writing response code in gdf file                                             *
          Called from printGroupVector()		                                *
 Arguments:                                                                             *
   file : groupId     		                                                        *
****************************************************************************************/   
//This method will be called at partition switch
char **print_resp_status_code_gdf_grp_vectors(int groupId)
{
  int i = 0;
  char **TwoD;
  char prefix[1024];
  int Idx2d = 0, dyn_obj_idx = 0;

  NSDL2_GDF(NULL, NULL, "Method called, total_discovered_status_code = %d", total_http_resp_code_entries);

  int total_discovered_status_code_entry = total_http_resp_code_entries * (sgrp_used_genrator_entries + 1);

  TwoD = init_2d(total_discovered_status_code_entry);

  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS as grp_data_flag is disabled and grp index fixed
    NSDL2_GDF(NULL, NULL, "in trans prefix is = %s", prefix);
    print_status_code_graph(TwoD, &Idx2d, prefix, groupId, i);
  }
  if(groupId == HTTP_STATUS_CODE_RPT_GRP_ID)
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_STATUS_CODE;

  msg_data_size = msg_data_size + ((dynObjForGdf[dyn_obj_idx].rtg_group_size ) * (sgrp_used_genrator_entries));

  return TwoD;
}

// Start - Methods called by NVM
/****************************************************************************************               
 Called from set_avgtime_ptr()                    				        *
          set all http reponse code avg ptr in case of static or dynamic call		*                                 *
****************************************************************************************/
inline void set_http_resp_code_avgtime_ptr() {
  NSDL2_HTTP(NULL, NULL, "Method Called");
  http_resp_code_avgtime = (HTTPRespCodeAvgTime *)((char *)average_time + http_resp_code_avgtime_idx);
}

/****************************************************************************************		
 Purpose: Update status code based count						*
          Called after every HTTP response is processed					*
 Arguments:										*
   cptr: connection pointer								*
   avg: avgtime (overall or for the group if group based stats enabled)			*
****************************************************************************************/  						
void update_http_status_codes(connection *cptr, avgtime *avg)				
{
  VUser *vptr = cptr->vptr;
 
  NSDL4_HTTP(vptr, cptr, "Method called, cptr->req_code = [%d], cptr->req_code_filled = [%d], "
                         "max_http_resp_code = %d, http_resp_code_avgtime_idx = %d", 
                          cptr->req_code, cptr->req_code_filled, max_http_resp_code, http_resp_code_avgtime_idx);


  // if cptr->req_code_filled is not zero , it means response code is not read than we will return without incrementing counter
  // this happen in case of connection fail, dns lookup fail, ssl fail, requests fail, response partially read before status code
  if(cptr->req_code_filled != 0)
    return;
 
  set_http_status_codes_count(vptr, cptr->req_code, 1, avg);
}

/****************************************************************************************               
 Purpose: set status code based count							*
          Called if JMeter script is used.						*
 Arguments:                                                                             *
   code: status code									*
   count: count of the status code							*
****************************************************************************************/ 
void set_http_status_codes_count(VUser *vptr, int code, int count, avgtime *avg)
{
  int avg_norm_idx = 0;
  int idx;

  NSDL4_HTTP(NULL, NULL, "Method called, code = [%d], count = [%d]", code, count);

  // if status code is more than max, then map to Others index
  if((code >= 0) && (code <= max_http_resp_code))
    idx = resp_code_mapping_tbl[code]; 
  else
    idx = 0; //Mapping to 'others'when out of configured range

  NSDL4_HTTP(NULL, NULL, "idx = %d, status_code_norm_idx = %d", idx, http_resp_code_arr[idx].status_code_norm_idx);
  if(http_resp_code_arr[idx].status_code_norm_idx == -1)
  {
    http_resp_code_arr[idx].status_code_norm_idx = ns_handle_dyn_status_code(vptr, http_resp_code_arr[idx].status_text, 
                                                             strlen(http_resp_code_arr[idx].status_text));
  }

  avg_norm_idx = http_resp_code_arr[idx].status_code_norm_idx;

  http_resp_code_avgtime[avg_norm_idx].http_resp_code_count += count;
  NSDL4_HTTP(NULL, NULL, "After update, for http_resp_code_avgtime = [%p], status code [%d], count is [%d], avg_norm_idx = [%d]", 
                          http_resp_code_avgtime, code, http_resp_code_avgtime[avg_norm_idx].http_resp_code_count, 
                          avg_norm_idx);
}

char* get_http_status_text(int code)
{
  int idx;

  NSDL4_HTTP(NULL, NULL, "Method called, code = [%d]", code);

  // if status code is more than max, then map to Others index
  if((code >= 0) && (code <= max_http_resp_code))
    idx = resp_code_mapping_tbl[code]; 
  else
    idx = 0; //Mapping to 'others'when out of configured range

  NSDL4_HTTP(NULL, NULL, "status_code = %d, idx = %d, status_text = %s", code, idx, http_resp_code_arr[idx].status_text);

  return http_resp_code_arr[idx].status_text;
}
/****************************************************************************************               
 Purpose: updates/fill gdf data								*
          Called from deliver_report                                              	*
 Arguments:                                                                             *
   avg : HTTPRespCodeAvgTime								*
   gp  : Http_Status_Codes_gp								*
****************************************************************************************/
void fill_http_status_codes_gp(avgtime **g_avg)
{
  int i, k = 0, gv_idx;
  avgtime *tmp_avg = NULL;

  NSDL4_HTTP(NULL, NULL, "Method called");

  //When no request is involved in test, ignore filling data, init is already ignored with other dyn objects
  if(!g_dont_skip_test_metrics)
    return;

  HTTPRespCodeAvgTime *http_resp_code_avgtime_loc = NULL;
  Http_Status_Codes_gp *http_status_codes_gp_ptr_loc;
  
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx] ;
    http_resp_code_avgtime_loc = (HTTPRespCodeAvgTime *)((char *)tmp_avg + http_resp_code_avgtime_idx);

    for (i = 0; i < total_http_resp_code_entries; i++, k++) 
    {
      NSDL4_HTTP(NULL, NULL, "HTTP_STATUS_CODES : http_resp_code_avgtime_loc = [%p],k = %d, gv_idx = [%d], "
                             "http_status_codes_gp_idx = [%d], i = %d, http_resp_code_count = [%d]", 
                              http_resp_code_avgtime_loc, k, gv_idx, http_status_codes_gp_idx, 
                              i, http_resp_code_avgtime_loc[i].http_resp_code_count);

      if(dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].rtg_index_tbl[gv_idx][i] < 0)
        continue;

      http_status_codes_gp_ptr_loc = (Http_Status_Codes_gp *)((char *)msg_data_ptr + dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].rtg_index_tbl[gv_idx][i]);

      //k = (gv_idx * TOTAL_GRP_ENTERIES_WITH_GRP_KW * total_http_resp_code_entries) + (grp_idx * total_http_resp_code_entries) + i;

      GDF_COPY_VECTOR_DATA(http_status_codes_gp_idx, 0, k, 0,
                           (Long_data) http_resp_code_avgtime_loc[i].http_resp_code_count, 
                           http_status_codes_gp_ptr_loc->http_resp_code_count);
      http_status_codes_gp_ptr_loc++;
    }
  }
}
