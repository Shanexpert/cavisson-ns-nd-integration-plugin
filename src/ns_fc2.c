/*********************************************************************************************
* Name                   : ns_fc2.c  

* Author                 : ANUBHAV

* Purpose                : This C file holds the function(s) required for reading /writing of FC2.
                           Also this procress frame  

* Date                   : September 2018

*********************************************************************************************/
#include "url.h"
#include "util.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "netstorm.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
#include "ns_string.h"
#include "ns_url_req.h"
#include "ns_vuser_tasks.h"
#include "ns_http_script_parse.h"
#include "ns_page_dump.h"
#include "ns_vuser_thread.h"
#include "nslb_encode_decode.h"
#include "nslb_util.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_trace_level.h"
#include "ns_group_data.h"
#include "ns_static_files.h"
#include "ns_fc2.h"

#define BUF_SIZE        4096
#define FC2_MAX_ATTR_LEN 1024

// Macro(s) used in Partial read 
// This Macro will retain previous data in vptr
#define COPY_PARTIAL_TO_CPTR(src_ptr) \
  NSDL2_FC2(NULL, NULL, "vptr->fc2->partial_buff_max_size = %d, vptr->fc2->partial_buff_size = %d", \
                                    vptr->fc2->partial_buff_max_size, vptr->fc2->partial_buff_size); \
  if (bytes_to_process > vptr->fc2->partial_buff_max_size - vptr->fc2->partial_buff_size){ \
    MY_REALLOC(vptr->fc2->partial_read_buff, (vptr->fc2->partial_buff_max_size + bytes_to_process), "vptr->fc2->partial_read_buff", -1); \
    vptr->fc2->partial_buff_max_size += bytes_to_process; \
  } \
  memcpy(vptr->fc2->partial_read_buff + vptr->fc2->partial_buff_size, src_ptr, bytes_to_process); \
  vptr->fc2->partial_buff_size += bytes_to_process; \
  NSDL2_FC2(NULL, NULL, "bytes copied in partial buffer = %d, vptr->fc2->partial_buff_size = %d", \
                                                                  bytes_to_process, vptr->fc2->partial_buff_size); 

// This Macro will not retain previous data in vptr.This is the case when we have read extra bytes and atmost 1 frame is cpmplete 
#define COPY_PARTIAL_TO_CPTR_AND_RESET(src_ptr) \
  NSDL2_FC2(NULL, NULL, "vptr->fc2->partial_buff_max_size = %d", vptr->fc2->partial_buff_max_size); \
  if (bytes_to_process > vptr->fc2->partial_buff_max_size){ \
    MY_REALLOC(vptr->fc2->partial_read_buff, bytes_to_process, "vptr->fc2->partial_read_buff", -1); \
    vptr->fc2->partial_buff_max_size = bytes_to_process; \
  } \
  memmove(vptr->fc2->partial_read_buff, src_ptr, bytes_to_process); \
  vptr->fc2->partial_buff_size = bytes_to_process; \
  NSDL2_FC2(NULL, NULL, "bytes copied in partial buffer = %d, vptr->fc2->partial_buff_size = %d", \
                                                                  bytes_to_process, vptr->fc2->partial_buff_size); 
FC2AvgTime *fc2_avgtime = NULL;
FC2CAvgTime *fc2_cavgtime = NULL;
#ifndef CAV_MAIN
int cur_post_buf_len ;
int g_fc2_avgtime_idx = -1;
int g_fc2_cavgtime_idx = -1;
#else
__thread int g_fc2_cavgtime_idx = -1;
__thread int cur_post_buf_len;
__thread int g_fc2_avgtime_idx = -1;
#endif

unsigned long next_refid(void)
{
        static unsigned long refid = 0;

        if (refid == ULONG_MAX)
        {
                refid = 0;
        }
        else
        {
                refid++;
        }

        return(refid);
}

/*
static int fc2_set_api(char *api_name, char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                  FILE *outfp,  char *flow_outfile, int send_tb_idx)
{
  char *start_idx;
  char *end_idx;
  char str[MAX_LINE_LENGTH + 1];
  int len ;

  NSDL1_FC2(NULL, NULL ,"Method Called. send_tb_idx = %d", send_tb_idx);
  start_idx = line_ptr;
  NSDL2_PARSING(NULL, NULL, "start_idx = [%s]", start_idx);
  end_idx = strstr(line_ptr, api_name);
  if(end_idx == NULL)
    SCRIPT_PARSE_ERROR(script_line, "Invalid Format or API Name not found in Line");

  len = end_idx - start_idx;
  strncpy(str, start_idx, len); //Copying the return value first
  str[len] = '\0';

  // Write like this. ret = may not be there
  //         int ret = ns_web_url(0) // HomePage

  NSDL2_FC2(NULL, NULL,"Before sprintf str is = %s ", str);
  NSDL2_FC2(NULL, NULL, "Add api  ns_web_websocket_send hidden file, send_tb_idx = %d", send_tb_idx);
  sprintf(str, "%s %s(%d); ", str, api_to_run, send_tb_idx);
  NSDL2_FC2(NULL, NULL," final str is = %s ", str);
  if(write_line(outfp, str, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR(script_line, "Error Writing in File ");

  return NS_PARSE_SCRIPT_SUCCESS;
}
*/

static int fc2_set_post_body(int send_tbl_idx, int sess_idx, int *script_ln_no, char *cap_fname)
{
  char *fname, fbuf[8192];
  int ffd, rlen, noparam_flag = 0;
    
  NSDL2_PARSING(NULL, NULL, "Method called, send_tbl_idx = %d, sess_idx = %d", send_tbl_idx, sess_idx);
    
  if (cur_post_buf_len <= 0) return NS_PARSE_SCRIPT_SUCCESS; //No BODY, exit
    
  //Removing traing ,\n from post buf.
    
  if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_LEGACY)
  {
    validate_body_and_ignore_last_spaces();
  }
  else
  {
    if(validate_body_and_ignore_last_spaces_c_type(sess_idx) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }
    
  //Check if BODY is provided using $CAVINCLUDE$= directive
  if((strncasecmp (g_post_buf, "$CAVINCLUDE$=", 13) == 0) || (strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)) {

   if(strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)
   {
      fname = g_post_buf + 21;
      noparam_flag = 1;
   }
   else
     fname = g_post_buf + 13;
      /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
      if (fname[0] != '/') {
          sprintf (fbuf, "%s/%s/%s", GET_NS_TA_DIR(),
                   get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), fname);
                   //Previously taking with only script name
                   //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), fname);
          fname = fbuf;
      }
      ffd = open (fname, O_RDONLY);
      if (!ffd) {
          NSDL2_FC2("%s() : Unable to open $CAVINCLUDE$ file %s\n", (char*)__FUNCTION__, fname);
          return NS_PARSE_SCRIPT_ERROR;
      }
      cur_post_buf_len = 0;
      while (1) {
          rlen = read (ffd, fbuf, 8192);
          if (rlen > 0) {
            if (copy_to_post_buf(fbuf, rlen)) {
              NSDL2_FC2(NULL, NULL,"%s(): Request BODY could not alloccate mem for %s\n", (char*)__FUNCTION__, fname);
              return NS_PARSE_SCRIPT_ERROR;
            }
            continue;
          } else if (rlen == 0) {
              break;
          } else {
              perror("reading CAVINCLUDE BODY");
              NSDL2_FC2(NULL, NULL, "%s(): Request BODY could not read %s\n", (char*)__FUNCTION__, fname);
              return NS_PARSE_SCRIPT_ERROR;
          }
      }
      close (ffd);
  }
  if (cur_post_buf_len)
  {
    if (noparam_flag) {
      segment_line_noparam(&(requests[send_tbl_idx].proto.fc2_req.message), g_post_buf, sess_idx);
    } else {
      segment_line(&(requests[send_tbl_idx].proto.fc2_req.message), g_post_buf, 0, *script_ln_no, sess_idx, cap_fname);
    }
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}
  
static int init_fc2_uri(int *url_idx, char *flow_file)
{
  NSDL2_FC2(NULL, NULL, "Method Called url_idx = %d, flow_file = %s", *url_idx, flow_file);
  
  //creating request table
  create_requests_table_entry(url_idx); // Fill request type inside create table entry

  gPageTable[g_cur_page].first_eurl = *url_idx;

  return NS_PARSE_SCRIPT_SUCCESS;
}

static int parse_fc2_uri(char *in_uri, char *host_end_markers, int *request_type, char *hostname, char *path)
{
  char *host_end = NULL;
  char *fc2_str = "http://"; // Assuming FC2 uri to start with this 
  char *uri = in_uri;

  NSDL1_WS(NULL, NULL, "Method called. Parse URI = %s, host_end_markers = %s", uri, host_end_markers);

  if (in_uri[0] == '\0')
  {
    NSTL1_OUT(NULL, NULL, "Error: Url is empty. url = %s\n", in_uri);
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error: Uri is empty. uri = %s",
                                                in_uri);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012330_ID, CAV_ERR_1012330_MSG, "URI");
  }

  if (!strncasecmp(in_uri, fc2_str, strlen(fc2_str))) {
    *request_type = FC2_REQUEST;
    uri += strlen(fc2_str);
    NSDL2_FC2(NULL, NULL, "request_type = %d", *request_type);
  }
  
  // Check if it is empty after schema (e.g. http://)
  if (uri[0] == '\0')
  {
    NSDL2_FC2(NULL, NULL, "Error: Url host is empty. url = %s\n", in_uri);
    NSTL1_OUT(NULL, NULL, "Error: Uri host is empty. uri = %s\n", in_uri);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012249_ID, CAV_ERR_1012249_MSG, "URL host");
  }

  if(host_end_markers)
    host_end = strpbrk(uri, host_end_markers);

  if (!host_end) {
    /* 
      Case 2: when Request type is found and path is not there
      hostname will be url 
      path will be /
      eg -
      http://www.test.com
    */
    strcpy(hostname, uri);
    strcpy(path, "/");
    return RET_PARSE_OK;
  }

  if (host_end == uri) // E.g. ws://?
  {
    NSDL2_WS(NULL, NULL, "Error: Url host is empty with query paramters. uri = %s\n", in_uri);
    NSTL1_OUT(NULL, NULL, "Error: Url host is empty with query paramters. uri = %s\n", in_uri);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012250_ID, CAV_ERR_1012250_MSG);
  }

  /* 
    Case 3: when Request type is found and path is there
    hostname will be extracted 
    path will be extracted
    eg -
    http://www.test.com/abc.html    (path - /abc.html )
    http://www.test.com?x=2         (path - /?x=2     )
    http://www.test.com#hello       (path - /#hello   )
  */
  strncpy(hostname, uri, host_end - uri);
  hostname[host_end - uri] = '\0';

  if(*host_end == '?' || *host_end == '#')
  {
    path[0] = '/';
    strcpy(path+1, host_end);
  }
  else
  {
    strcpy(path, host_end);
  }
  return RET_PARSE_OK;
}

static int fc2_set_uri(char *uri, char *flow_file, int sess_idx, int url_idx)
{
  //TODO reset this function 
  char hostname[MAX_LINE_LENGTH + 1];
  int  request_type;
  char request_line[MAX_LINE_LENGTH + 1];
  //int get_no_inlined_obj_set_for_all = 1;

  NSDL2_PARSING(NULL, NULL, "Method Called Uri=%s", uri);
  //Parses Absolute/Relative URLs
  if(parse_fc2_uri(uri, "{/?#", &request_type, hostname, request_line) != RET_PARSE_OK)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012226_ID, CAV_ERR_1012226_MSG, uri);

  //TODO: we have consider proto type is FC2_REQUEST and set it already
  requests[url_idx].request_type = request_type;

  //Setting url type to Main/Embedded
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  if (g_max_num_embed < gPageTable[g_cur_page].num_eurls) g_max_num_embed = gPageTable[g_cur_page].num_eurls; //Get high water mark

  // check if the hostname exists in the server table, if not add it
  requests[url_idx].index.svr_idx = get_server_idx(hostname, requests[url_idx].request_type, script_ln_no);

  if(requests[url_idx].index.svr_idx != -1)
  {
    if(gServerTable[requests[url_idx].index.svr_idx].main_url_host == -1)
    {
      gServerTable[requests[url_idx].index.svr_idx].main_url_host = 1; // For main url
    }
  }
  else
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012073_ID, CAV_ERR_1012073_MSG);
  }

  segment_line(&(requests[url_idx].proto.fc2_req.uri), request_line, 0, script_ln_no, sess_idx, flow_filename);

  /*Added for filling all server in gSessionTable*/
  CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from fc2_set_uri");

  NSDL3_FC2(NULL, NULL, "Exitting Method ");
  return NS_PARSE_SCRIPT_SUCCESS;
}


// Called by parent
inline void update_fc2_avgtime_size() {
  NSDL2_FC2(NULL, NULL, "Method Called, g_avgtime_size = %d, g_fc2_avgtime_idx = %d",
                                          g_avgtime_size, g_fc2_avgtime_idx);
  
  if((global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED)) {
    NSDL2_FC2(NULL, NULL, "FC2 is enabled.");
    g_fc2_avgtime_idx = g_avgtime_size;
    g_avgtime_size += sizeof(FC2AvgTime);

  } else {
    NSDL2_FC2(NULL, NULL, "FC2 is disabled.");
  }

  NSDL2_FC2(NULL, NULL, "After g_avgtime_size = %d, g_fc2_avgtime_idx = %d",
                                          g_avgtime_size, g_fc2_avgtime_idx);
}

// called by parent
inline void update_fc2_cavgtime_size(){

  NSDL2_FC2(NULL, NULL, "Method Called, g_cavgtime_size = %d, g_fc2_cavgtime_idx = %d",
                                          g_cavgtime_size, g_fc2_cavgtime_idx);

  if(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED) {
    NSDL2_FC2(NULL, NULL, "FC2 is enabled.");
    g_fc2_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(FC2CAvgTime);
  } else {
    NSDL2_FC2(NULL, NULL, "FC2 is disabled.");
  }

  NSDL2_FC2(NULL, NULL, "After g_cavgtime_size = %d, g_fc2_cavgtime_idx = %d",
                                          g_cavgtime_size, g_fc2_cavgtime_idx);
}

// Function for filling the data in the structure of fc2_gp
void fill_fc2_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  //Long_data succ;
  int g_idx = 0, gv_idx, grp_idx;
  double result;
  if(fc2_gp_ptr == NULL) return;

  NSDL2_GDF(NULL, NULL, "Method called");
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  FC2AvgTime *fc2_avg = NULL;
  FC2CAvgTime *fc2_cavg = NULL;

  fc2_gp *fc2_local_gp_ptr = fc2_gp_ptr;
  
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    avg = (avgtime*)g_avg[gv_idx];
    cavg = (cavgtime*)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      fc2_avg = (FC2AvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_fc2_avgtime_idx);
      fc2_cavg = (FC2CAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_fc2_cavgtime_idx);


      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           fc2_avg->fc2_fetches_started, fc2_local_gp_ptr->fc2_req); g_idx++;

      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           fc2_avg->fc2_fetches_sent, fc2_local_gp_ptr->fc2_sent); g_idx++;

      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                          fc2_avg->fc2_num_tries, fc2_local_gp_ptr->fc2_tries); g_idx++;

      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           fc2_avg->fc2_num_hits, fc2_local_gp_ptr->fc2_succ); g_idx++;

      // Here the "response" variable of `Times_data` data type is getting filled
      GDF_COPY_TIMES_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                               fc2_avg->fc2_overall_avg_time, fc2_avg->fc2_overall_min_time,
                               fc2_avg->fc2_overall_max_time, fc2_avg->fc2_num_tries,
                               fc2_local_gp_ptr->fc2_response.avg_time,
                               fc2_local_gp_ptr->fc2_response.min_time,
                               fc2_local_gp_ptr->fc2_response.max_time,
                               fc2_local_gp_ptr->fc2_response.succ); g_idx++;

      if(fc2_avg->fc2_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                                 fc2_avg->fc2_avg_time, fc2_avg->fc2_min_time, fc2_avg->fc2_max_time, fc2_avg->fc2_num_hits,
                                 fc2_local_gp_ptr->fc2_succ_response.avg_time,
                                 fc2_local_gp_ptr->fc2_succ_response.min_time,
                                 fc2_local_gp_ptr->fc2_succ_response.max_time,
                                 fc2_local_gp_ptr->fc2_succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                                 0, -1, 0, 0,
                                 fc2_local_gp_ptr->fc2_succ_response.avg_time,
                                 fc2_local_gp_ptr->fc2_succ_response.min_time,
                                 fc2_local_gp_ptr->fc2_succ_response.max_time,
                                 fc2_local_gp_ptr->fc2_succ_response.succ); g_idx++;
      }

      if(fc2_avg->fc2_num_tries - fc2_avg->fc2_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                                 fc2_avg->fc2_failure_avg_time, fc2_avg->fc2_failure_min_time,
                                 fc2_avg->fc2_failure_max_time, fc2_avg->fc2_num_tries - fc2_avg->fc2_num_hits,
                                 fc2_local_gp_ptr->fc2_fail_response.avg_time,
                                 fc2_local_gp_ptr->fc2_fail_response.min_time,
                                 fc2_local_gp_ptr->fc2_fail_response.max_time,
                                 fc2_local_gp_ptr->fc2_fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                                 0, -1, 0, 0,
                                 fc2_local_gp_ptr->fc2_fail_response.avg_time,
                                 fc2_local_gp_ptr->fc2_fail_response.min_time,
                                 fc2_local_gp_ptr->fc2_fail_response.max_time,
                                 fc2_local_gp_ptr->fc2_fail_response.succ); g_idx++;
      }


      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           fc2_cavg->fc2_fetches_completed, fc2_local_gp_ptr->fc2_cum_tries); g_idx++;

      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           fc2_cavg->fc2_succ_fetches, fc2_local_gp_ptr->fc2_cum_succ); g_idx++;

      /*Here to calculate FC2 failure corresponding to FC2 completed and  Success.
        Formula is :- (((FC2 Completed - FC2 Success) * 100)/FC2 Completed) */
      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           (fc2_avg->fc2_num_tries == 0)?0:((double)((fc2_avg->fc2_num_tries - fc2_avg->fc2_num_hits) * 100)/fc2_avg->fc2_num_tries),
                           fc2_local_gp_ptr->fc2_failure); g_idx++;
      NSDL3_GDF(NULL, NULL, "fc2_num_hits = %llu, fc2_num_tries = %llu, fc2_failure = %f",
                             fc2_avg->fc2_num_hits, fc2_avg->fc2_num_tries, fc2_local_gp_ptr->fc2_failure);

      result = (fc2_avg->fc2_total_bytes)*8/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           result, fc2_local_gp_ptr->fc2_body_throughput); g_idx++;

      GDF_COPY_VECTOR_DATA(fc2_gp_idx, g_idx, gv_idx, 0,
                           fc2_cavg->fc2_c_tot_total_bytes, fc2_local_gp_ptr->fc2_body); g_idx++;

      g_idx = 0;
      fc2_local_gp_ptr++;
    }
  }
}
/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse ns_fc2_send() API and do following things 
 *              
 *             (1) Create and fill following tables -
 *                 (i) Add dummy page name like fc2_<id> into gPageTable
 *                 (ii) Create request table and fill data  
 *                 (iii) create fc2_request Table and fill its members  
 *
 * Input     : flow_fp - pointer to input flow file 
 *             ns_fc2_send("PageName",  //dummy not given by user
 *                         "URI=http://dcrmg:3220",
                           "MESSAGE=$CAVINCLUDE_NOPARAM$=binary.txt",
 *                         );
 *             outfp   - pointer to output flow file (made in $NS_WDIR/.tmp/ns-inst<nvm_id>/)
 *             flow_filename - flow file name 
 *             sess_idx- pointing to session index in gSessionTable 
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/

int ns_parse_fc2_send(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  int  url_idx = 0;
  int ret;
  char attribute_name[FC2_MAX_ATTR_LEN + 1];
  char attribute_value[FC2_MAX_ATTR_LEN + 1];
  char pagename[FC2_MAX_ATTR_LEN + 1];
  char *page_end_ptr = NULL;
  static int cur_page_index = -1; //For keeping track of multiple main urls
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  char uri_exists = 0;
  char message_flag = 0;
  NSDL2_FC2(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  //Adding Dummy page name as in ns_web_websocket_connect() API page name is not given 
  sprintf(pagename, "fc2_%d", web_url_page_id);
  
  init_post_buf();
  page_end_ptr = strchr(script_line, '"');

  NSDL2_FC2(NULL, NULL, "pagename - [%s], page_end_ptr = [%s]", pagename, page_end_ptr);
  //TODO should we set default value of ws_call_back entry

  if((parse_and_set_pagename("ns_fc2_send", "ns_web_url", flow_fp, flow_filename,
              script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
  return NS_PARSE_SCRIPT_ERROR;

  close_quotes = page_end_ptr;
  start_quotes = page_end_ptr;

  if(init_fc2_uri(&url_idx, flow_filename) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(url_idx, FC2_REQUEST);

  while(1)
  {
    NSDL3_FC2(NULL, NULL, "line = %s", script_line);
    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);
    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

    if(!strcasecmp(attribute_name, "URI"))
    {
      NSDL2_FC2(NULL, NULL, "URI [%s] ", attribute_value);
      if(cur_page_index != -1)
      {
        if(cur_page_index == g_cur_page)
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "URI");
      }
      cur_page_index = g_cur_page;
      if(fc2_set_uri(attribute_value, flow_filename, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      uri_exists = 1;
      NSDL2_FC2(NULL, NULL, "FC2: Value of %s = %s , segment offset = %d",
                                                               attribute_name, attribute_value, requests[url_idx].proto.fc2_req.uri);

    }

    if(!strcasecmp(attribute_name, "MESSAGE"))
    {
      //Buffer should be sent one time only
      if(message_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "MESSAGE");
      }
      message_flag++;

      if(extract_buffer(flow_fp, script_line, &close_quotes, flow_filename) == NS_PARSE_SCRIPT_ERROR)
         return NS_PARSE_SCRIPT_ERROR;

      if(fc2_set_post_body(url_idx, sess_idx, &script_ln_no, flow_filename) == NS_PARSE_SCRIPT_ERROR)
      {
         NSDL2_FC2(NULL, NULL, "Send data at message.seg_start = %lu, message.num_ernties = %d ",
                               requests[url_idx].proto.fc2_req.message.seg_start, requests[url_idx].proto.fc2_req.message.num_entries);
         return NS_PARSE_SCRIPT_ERROR;
      }
       NSDL2_FC2(NULL, NULL, "Send data at message.seg_start = %lu, message.num_ernties = %d ",
                               requests[url_idx].proto.fc2_req.message.seg_start, requests[url_idx].proto.fc2_req.message.num_entries);
       NSDL2_FC2(NULL, NULL, "FC2_SEND: BUFFER = %s", g_post_buf);
    }

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_FC2(NULL, NULL, "Next attribute is not found");
      break;
    }
  } //End while loop here

  if(!uri_exists)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012211_ID, CAV_ERR_1012211_MSG, "URI");

  if(!message_flag)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012211_ID, CAV_ERR_1012211_MSG, "MESSAGE");

  NSDL2_FC2(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int get_values_from_segments(connection *cptr, StrEnt_Shr* seg_tab_ptr, char **buffer, int *buf_size)
{
  int i;
  int ret = 0;
  int filled_len=0;
  char *loc_buffer = NULL;
  int loc_buf_size = 0;
  VUser *vptr = cptr->vptr;

  NSDL2_FC2(vptr, cptr, "Method Called");
  // Get all segment values in a vector
  // Note that some segment may be parameterized
  if ((ret = insert_segments(vptr, cptr, seg_tab_ptr, &g_scratch_io_vector,
                                  NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  {
     NSDL2_FC2(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error in insert_segments()");
     return ret;
  }
  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++)
  {
    if((filled_len + NS_GET_IOVEC_LEN(g_scratch_io_vector, i)) > loc_buf_size){
      MY_REALLOC_AND_MEMSET(loc_buffer, (filled_len + NS_GET_IOVEC_LEN(g_scratch_io_vector, i) + 1024), loc_buf_size, "fc2 buffer to send", -1);
      loc_buf_size = filled_len + NS_GET_IOVEC_LEN(g_scratch_io_vector, i) + 1024;
    }
    memcpy(loc_buffer + filled_len, NS_GET_IOVEC_VAL(g_scratch_io_vector, i), NS_GET_IOVEC_LEN(g_scratch_io_vector, i));
    filled_len += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }
  loc_buffer[filled_len] = 0;
  *buffer = loc_buffer;
  *buf_size = loc_buf_size; 
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  NSDL2_FC2(vptr, cptr, "segment value = [%s] len = %d", buffer, filled_len);
  return  filled_len;
}

/*--------------------------------------------------------------------------------
Function Allocates memory for FC2 structure 
----------------------------------------------------------------------------------*/ 
void init_fc2(connection *cptr, VUser *vptr)
{
  NSDL2_FC2(NULL, NULL, "Method called");

  if(!vptr->fc2){
    MY_MALLOC_AND_MEMSET(vptr->fc2, sizeof(FC2), "cptr->fc2", -1);
    vptr->flags |= NS_FC2_ENABLE;
    vptr->fc2_cptr = cptr;
    cptr->fc2_state = FC2_SEND_HANDSHAKE;
  }
}

/*--------------------------------------------------------------------------------
Functions for logging FC2 request/response
----------------------------------------------------------------------------------*/
void debug_log_fc2_rep(connection *cptr, int size, unsigned char *data)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;

  NSDL2_FC2(NULL, cptr, "Method called, size = %d", size);
  
  char buf[3072];  
  char log_req_file[1024];
  int log_data_fd;
  int ret;
  int dump_len = 0;

  if(size > 250){
    dump_len = size - 250;  
    ret = sprintf(buf, "%d, ", dump_len);
    memcpy(buf + ret, data + 250, dump_len);
  }
  else{
    dump_len = size;
    ret = sprintf(buf, "%d, ", dump_len);
    memcpy(buf + ret, data + 250, dump_len);
  }
  
  // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  SAVE_REQ_REP_FILES
  sprintf(log_req_file, "%s/logs/%s/url_rep_fc2_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
      
  if((log_data_fd = open(log_req_file, O_CREAT|O_WRONLY|O_APPEND, 00666)) < 0) 
    fprintf(stderr, "Error: Error in opening file for logging URL request. Error = %s\n", nslb_strerror(errno));
  else
  {
    NSDL2_FC2(NULL, cptr, "Logging FC2 request data");
    ret = write(log_data_fd, buf, dump_len);
    if(ret < 0)
      fprintf(stderr, "Error: Error in dumping data to file for logging URL response. Error = %s\n", nslb_strerror(errno));
    close(log_data_fd);
  } 
}   
 
void debug_log_fc2_rep_ex(connection *cptr, int size, unsigned char *data){

  VUser *vptr;
  vptr = (VUser *)cptr->vptr;

  NSDL1_HTTP2(NULL, NULL, "Method called");

  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED)
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)
                            && ((cptr->url_num->proto.http.type == EMBEDDED_URL
                                 && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url)
                                 || cptr->url_num->proto.http.type != EMBEDDED_URL))
  debug_log_fc2_rep(cptr, size, data);
}

void debug_log_fc2_req(connection *cptr, int size, unsigned char *data){ 
  VUser *vptr;
  vptr = (VUser *)cptr->vptr; 

  NSDL2_FC2(NULL, cptr, "Method called");
  
  char buf[102400];  
  char log_req_file[102400];
  int log_data_fd;
  int ret;
    
  ret = sprintf(buf, "%d, ", size);
  memcpy(buf + ret, data, size);

  // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  SAVE_REQ_REP_FILES
  sprintf(log_req_file, "%s/logs/%s/url_req_fc2_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

  if((log_data_fd = open(log_req_file, O_CREAT|O_WRONLY|O_APPEND, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    NSDL2_FC2(NULL, cptr, "Logging FC2 request data");
    ret = write(log_data_fd, buf, size);
    if(ret == -1)
      fprintf(stderr, "Error: Error in dumping data to file for logging URL request\n");
    close(log_data_fd);
  }
}

void debug_log_fc2_req_ex(connection *cptr, int size, unsigned char *data){
  
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;

  NSDL1_HTTP2(NULL, NULL, "Method called");

  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED)
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)
                            && ((cptr->url_num->proto.http.type == EMBEDDED_URL
                                 && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url)
                                 || cptr->url_num->proto.http.type != EMBEDDED_URL))
  debug_log_fc2_req(cptr, size, data);
}

/*--------------------------------------------------------------------------------
Function to set process frame payload and set states for FC2
----------------------------------------------------------------------------------*/
void process_fc2_frame_payload(connection *cptr, unsigned char *frame_ptr, int payload_length, u_ns_ts_t now){

  FC2_Resp_Hdr2_t *hdr;


  hdr = (FC2_Resp_Hdr2_t *)frame_ptr;

  switch(hdr->ucMessageType)
  {
    case FC2_MSG_TYPE_HANDSHAKE:
      cptr->fc2_state = FC2_SEND_MESSAGE;
      break;

    case FC2_MSG_TYPE_APPLICATION:
      if(cptr->fc2_state == FC2_RECEIVE_HANDSHAKE)
        cptr->fc2_state = FC2_SEND_MESSAGE;
      else if(cptr->fc2_state == FC2_RECEIVE_MESSAGE)
        cptr->fc2_state = FC2_END;
      break;
    
    case FC2_MSG_TYPE_TERMINATE:
      printf("\nFC2_MSG_TYPE_TERMINATE received, this is yet to be handled\n");
      break;
   }
}

/*--------------------------------------------------------------------------------
Function to handle read and process frames for FC2 
----------------------------------------------------------------------------------*/
int fc2_handle_read(connection *cptr, u_ns_ts_t now) {

  int bytes_read = 0;
  unsigned char *frame_ptr = NULL;
  unsigned char buf[READ_BUFF_SIZE + 1] = "";
  unsigned int payload_length;
  int processed_len = 0;
  int bytes_to_process = 0;
  int max_header_frame_len; 
  FC2_Resp_Hdr2_t *hdr;
  NSDL1_FC2(NULL, cptr, "Method called cptr is %p", cptr);

  // Read till no more data available.
  while (1) {
   
    processed_len = 0;
    frame_ptr = NULL;
    
    bytes_read = read(cptr->conn_fd, buf, READ_BUFF_SIZE);
   
    NSDL3_SSL(NULL, cptr, "Read %d bytes HTTP", bytes_read);
   
    if (bytes_read < 0) {
      if (errno == EAGAIN) {
        NSDL2_FC2(NULL, NULL, "Got EAGAIN while reading. cptr->fc2_state = %d", cptr->fc2_state);
          handle_connect(cptr, now, 0);
          return FC2_ERROR;
        }
      if (errno == EINTR) {
        NSDL2_FC2(NULL, NULL, "Got EINTR while reading");
        return FC2_ERROR;
      } else {
        NSDL1_FC2(NULL, cptr, "cptr bytes_read = %d. Hence going to close connection", bytes_read);
        handle_server_close(cptr, now);
        return FC2_ERROR;
        }
    } else if (bytes_read == 0) {
         handle_server_close(cptr, now);
        return FC2_ERROR;
      }
   
    // Update average time 
    cptr->tcp_bytes_recv += bytes_read;
    
    FC2AvgTime *loc_fc2_avgtime; 
    loc_fc2_avgtime = (FC2AvgTime*)((char*)average_time + g_fc2_avgtime_idx); 
    average_time->rx_bytes += bytes_read;
    loc_fc2_avgtime->fc2_total_bytes += bytes_read;
    
    VUser *vptr = cptr->vptr;
   
    if (SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->rx_bytes += bytes_read;
    }
 
    NSDL2_FC2(NULL, NULL, "bytes_read = %d", bytes_read);

    bytes_to_process = bytes_read;
    
    while (bytes_to_process > 0) { // bytes_to_process != 0
    
      NSDL2_FC2(NULL, NULL, "bytes_to_process = %d", bytes_to_process); // Initially process length must be 0 . 
      // Atleast frame header is complete
      if (frame_ptr == NULL){ // This means we are processing first frame 
        if (vptr->fc2->partial_buff_size){
          NSDL2_FC2(NULL, NULL, "copy partial in vptr");
          COPY_PARTIAL_TO_CPTR(buf) 
          bytes_to_process = vptr->fc2->partial_buff_size; 
          frame_ptr = vptr->fc2->partial_read_buff;
        } else {
          frame_ptr = buf; 
        }
      }
      
      hdr = (FC2_Resp_Hdr2_t *)frame_ptr; 
      max_header_frame_len = sizeof(FC2_Resp_Hdr2_t);

      NSDL2_FC2(NULL, NULL, "bytes_to_process = %d max_header_frame_len = %d", bytes_to_process, max_header_frame_len);  
      
      if (bytes_to_process >= max_header_frame_len) {   
       
        payload_length = hdr->ulOutDataSize;

        NSDL2_FC2(NULL, NULL, "bytes_to_process = %d, payload_length=%d , processed_length=%d ", 
                                 bytes_to_process, payload_length, processed_len);
       
        NSDL2_FC2(NULL, NULL, "FC2 Response Headers : hdr->usBOM =%hu hdr->usVersion=%hu hdr->usFC2Mark=%hu hdr->usDataCodePage=%hu"
                              "hdr->ulFlags=%lu hdr->ulResponseCode=%lu hdr->szResponseMsgText=%s hdr->szMsgName=%s"
                              "hdr->ulExHdrSize=%lu hdr->ulOutDataSize=%lu hdr->ucMessageType=%c hdr->ucMessageType=%d", 
                               hdr->usBOM, hdr->usVersion, hdr->usFC2Mark, hdr->usDataCodePage, hdr->ulFlags, hdr->ulResponseCode,
                               hdr->szResponseMsgText,hdr->szMsgName, hdr->ulExHdrSize, hdr->ulOutDataSize, hdr->ucMessageType, 
                               hdr->ucMessageType);
     
        if (bytes_to_process >= (payload_length + max_header_frame_len)) {
        
          NSDL2_FC2(NULL, vptr, "Complete frame received. Going to Process frame");         
          processed_len += max_header_frame_len; 
          bytes_to_process -= max_header_frame_len;
          frame_ptr = frame_ptr + max_header_frame_len;
          
          NSDL2_FC2(NULL, NULL, "processed_len after processing frame_header is [%d]", processed_len);
         
        // Process Frame payload  
          process_fc2_frame_payload(cptr, frame_ptr, payload_length, now); 

          processed_len += payload_length;
          bytes_to_process -= payload_length;
        
          NSDL2_FC2(NULL, NULL, "processed_len is  [%d] \n", processed_len);
          frame_ptr = frame_ptr + payload_length;

          if(vptr->fc2->partial_buff_size) {
            vptr->fc2->partial_buff_size = bytes_to_process;
            NSDL2_FC2(NULL, NULL, "vptr->fc2->partial_buff_size is  [%d]", vptr->fc2->partial_buff_size);
          }
          
          if(cptr->fc2_state == FC2_END){
            //if(hdr->ucMessageType == 0){
              #ifdef NS_DEBUG_ON
                debug_log_fc2_rep(cptr, bytes_read, buf);
              #else
                if(runprof_table_shr_mem[vptr->group_num].gset.trace_level)
                {
                  debug_log_fc2_rep_ex(cptr, bytes_read, buf);
                }
              #endif
            //}
            cptr->fc2_state = FC2_SEND_HANDSHAKE;
            Close_connection(cptr, 1, now, 0, NS_COMPLETION_CONTENT_LENGTH);
            return 0;
          }
          else
            handle_connect(cptr, now, 0);
        } else {
          // This means frame paylaod is partial.  We will neither process frame header nor frame paylaod until complete frame is received . 
          NSDL2_FC2(NULL, NULL, "copy partial in cptr and reset");
          COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
          break ;
        }
      } else{
        NSDL2_FC2(NULL, NULL, "Frame header is partial. Going to copy data to cptr");
        // Frame header is partial. We can not process until we have received complete frame header, hence copying partial data to cptr
        NSDL2_FC2(NULL, NULL, "copy partial in cptr");
        COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
        break; // Break process while loop 
      } 
    } 
  } 
  return 0;
}

/*------------------------------------------------------------
Handles partial write and close connections in case of error  
-------------------------------------------------------------*/
int fc2_handle_write(connection *cptr, u_ns_ts_t now){

  int bytes_written = 0;
  char *write_buff; 

  write_buff = cptr->free_array + cptr->total_pop3_scan_list; 
  VUser *vptr = cptr->vptr;

  NSDL2_FC2(NULL, cptr, "Method Called bytes_left_to_send  are [%d]", cptr->bytes_left_to_send);

  #ifdef NS_DEBUG_ON
    if(cptr->fc2_state == FC2_SEND_MESSAGE)
      debug_log_fc2_req(cptr, cptr->bytes_left_to_send, (unsigned char *)cptr->free_array);
  #else
    if(runprof_table_shr_mem[vptr->group_num].gset.trace_level){
      if(cptr->fc2_state == FC2_SEND_MESSAGE)
        debug_log_fc2_req_ex(cptr, cptr->bytes_left_to_send, (unsigned char *)cptr->free_array);
    }
  #endif

  while (cptr->bytes_left_to_send != 0) { 
    // Check if request type is FC2 
    if (cptr->request_type == FC2_REQUEST) {
      bytes_written = write(cptr->conn_fd, write_buff, cptr->bytes_left_to_send);
      // Check error cases
      if (bytes_written < 0) { 
        if (errno == EAGAIN) // No more data is available at this time, so return
          return FC2_ERROR;
        if (errno == EINTR) // In case of interrupt we will return 
          return FC2_ERROR;
        if ( errno != EAGAIN) {  // In this case we will try to write request again 
          NSDL2_FC2(NULL, NULL, "Retrying connection");
          retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
          return FC2_ERROR;
        }
      }
      if (bytes_written == 0) { // We will continue writing in this case 
        NSDL1_FC2(NULL, cptr, "Total byte written = %d . Hence continue reading", bytes_written);
        continue;
      }
      // Decrement bytes_left_to_send by bytes_written 
      cptr->bytes_left_to_send -= bytes_written;
      cptr->total_pop3_scan_list += bytes_written;  
      cptr->http_payload_sent += bytes_written; // This?
    }  
  } 
  
  if (cptr->request_type == FC2_REQUEST) //  Increment tcp_bytes_sent only here for http only. 
    cptr->tcp_bytes_sent += cptr->bytes_left_to_send;

  NSDL2_FC2(NULL, cptr, "bytes_left_to_send = [%d]", cptr->bytes_left_to_send); 
 
  if (cptr->bytes_left_to_send != 0) {  // This means write is incomplete for FC2 set free array and return. 
    return FC2_SUCCESS;
  }

  // This means that write  is complete . Change proto state and return
  cptr->total_pop3_scan_list = 0; 
  
  if(cptr->fc2_state == FC2_SEND_HANDSHAKE)
    cptr->fc2_state = FC2_RECEIVE_HANDSHAKE;
  if(cptr->fc2_state ==FC2_SEND_MESSAGE){
    cptr->fc2_state = FC2_RECEIVE_MESSAGE;
   
    FC2AvgTime *loc_fc2_avgtime;
    loc_fc2_avgtime = (FC2AvgTime*)((char*)average_time + g_fc2_avgtime_idx);
    loc_fc2_avgtime->fc2_fetches_sent++;

    if(SHOW_GRP_DATA) {
      FC2AvgTime *local_fc2_avg; 
      local_fc2_avg = (FC2AvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_fc2_avgtime_idx);
      local_fc2_avg->fc2_fetches_sent++; 
    }
  } 
  return 0;
}

static void get_curr_date_and_time(unsigned long *date){
  // variables to store date and time components
  //int hours, minutes, seconds, day, month, year;
  NSDL2_FC2(NULL, NULL, "Method Called");
  int day, month, year;
  char buf[16];
  unsigned long loc_date;
  // time_t is arithmetic time type
  time_t now;
  // Obtain current time, time() returns the current time of the system as a time_t value
  time(&now);
  // Convert to local time format and print to stdout

  //printf("Today is : %s", ctime(&now));

  /* localtime converts a time_t value to calendar time and returns a pointer to a tm structure with its members 
     filled with the corresponding values */
  struct tm *local = localtime(&now);
  //hours = local->tm_hour;       // get hours since midnight (0-23)
  //minutes = local->tm_min;      // get minutes passed after the hour (0-59)
  //seconds = local->tm_sec;      // get seconds passed after minute (0-59)

  day = local->tm_mday;         // get day of month (1 to 31)
  month = local->tm_mon + 1;    // get month of year (0 to 11)
  year = local->tm_year + 1900; // get year since 1900

  //printf("Time is : %02d:%02d:%02d\n", hours, minutes, seconds);
  // print current date
  //printf("Date is : %02d/%02d/%d\n", day, month, year);
  sprintf(buf,"%02d%02d%02d", year, month, day);
  loc_date = atol(buf);
  *date = loc_date;
  NSDL2_FC2(NULL, NULL, "date string = %s  date value = %lu", buf, *date); 
}

//short SeqNum = 0;
unsigned long SeqNum = 0;
 
int fc2_make_and_send_frames(VUser *vptr, u_ns_ts_t now) {

 // unsigned char buf[BUF_SIZE];
  //char message[BUF_SIZE];
  unsigned char *buf = NULL;
  //int buf_size;
  char *message = NULL;
  int message_size = 0;
  int len = 0;
  int ret;
  int buf_len;
  int buf_offset;
  FC2_Req_Hdr_t        hdr;
  FC2_Req_Hdr_t        *hdr_ptr;
  //FED_CON_APP_HDR_T    *con_app_ptr;
  //COMMON_HEADER_t      *comm_header;
  COMMON_HEADER_t comm_hdr;
  FC2_Handshake_Req_t  hs_req;
  unsigned long date;
  //unsigned short SeqNum = 0x0100;
  //FC2_NODE_T *req_node;
 
  connection *cptr = vptr->fc2_cptr;

  cptr->url_num = &request_table_shr_mem[vptr->next_pg_id];
  NSDL2_FC2(vptr, cptr, "cptr->url_num = %p vptr->next_pg_id = %d", cptr->url_num, vptr->next_pg_id);
  
  if(cptr->fc2_state == FC2_END)
    cptr->fc2_state = FC2_SEND_HANDSHAKE; 
  
  memset(&hdr,0,sizeof(hdr));
  hdr.usBOM          = FC2_BOM;
  hdr.usVersion      = FC2_VERSION;
  hdr.usFC2Mark      = FC2MARK;
  hdr.usDataCodePage = FC2_CCSID_1252;
  hdr.ulFlags        = FC2_FLG_NONE;
  hdr.ulExHdrSize    = 0;

  if (cptr->fc2_state == FC2_SEND_HANDSHAKE) {

    //
    // Build and send the handshake message.
    //
    hdr.ulInDataSize   = sizeof(FC2_Handshake_Req_t);
    hdr.ulOutDataSize  = sizeof(FC2_Handshake_Resp_t);
    hdr.ulTimeout      = 5;
    hdr.ulRefId        = next_refid();
    hdr.ucMessageType  = FC2_MSG_TYPE_HANDSHAKE;

    memset(&hs_req,0,sizeof(hs_req));
    hs_req.ulIdleTimeout      = 1800;
    hs_req.ucResetIdleTimeout = 0;
  
    MY_MALLOC_AND_MEMSET(buf, BUF_SIZE, "buf to send", -1);
    //buf_size = BUF_SIZE;

    memcpy(buf,(char *)&hdr,sizeof(hdr));
    len = sizeof(hdr);

    memcpy(&buf[len],(char *)&hs_req,sizeof(hs_req));
    len += sizeof(hs_req);
    
    cptr->free_array = (char *)buf;  // We are using cptr->free_array to send handshake
    cptr->bytes_left_to_send = len;
  } 
  
  if (cptr->fc2_state == FC2_SEND_MESSAGE){

    int filled_len = 0;
 
    filled_len = get_values_from_segments(cptr, &cptr->url_num->proto.fc2_req.message, &message, &message_size);
    // Set local pointer aplication header.
    if(filled_len < 0)
      return -1;

    char *loc_message = NULL;
    loc_message = message;
    int char_count = 0;
    int comma_count = 4;

    while(comma_count){
      if(*loc_message == ',')
        comma_count--;
      loc_message++;
      char_count++;
    }
    
    hdr_ptr = (FC2_Req_Hdr_t *)loc_message;
 
    /*
    app_hdr = (FED_CON_APP_HDR_T *)(message + sizeof(FC2_Req_Hdr_t));  //Assuming we will get this msg (from where?) do we have to maintain link list of FC2_NODE_T?

    // Build the outgoing message header.
    
    hdr.ulInDataSize   = app_hdr->ulRequestDataSize;
    hdr.ulOutDataSize  = app_hdr->ulReplyDataSize;
    hdr.ulTimeout      = app_hdr->ulTimeout;
    hdr.ulRefId        = next_refid();
    hdr.ucMessageType  = FC2_MSG_TYPE_APPLICATION;
    memcpy(hdr.szMsgName,app_hdr->szMsgName,sizeof(hdr.szMsgName));
    
    buf_len = sizeof(hdr) + app_hdr->ulRequestDataSize;
    */
    MY_MALLOC_AND_MEMSET(buf, filled_len + 4, "buf to send", -1);
    //buf_size = filled_len + 4;

    //memset(buf,0,sizeof(buf));   
 
    //memcpy(buf, (char *)message, char_count);
    //buf_offset = char_count;

    memcpy(buf, (char *)hdr_ptr, sizeof(hdr));
    buf_offset = sizeof(hdr);

    memcpy(&buf[buf_offset], (char *)&message[buf_offset + char_count],
                                   hdr_ptr->ulInDataSize); 

    char *my_ptr = loc_message + 183;
    
    memcpy(&comm_hdr, my_ptr, sizeof(COMMON_HEADER_t));  
   
    get_curr_date_and_time(&date);
    comm_hdr.MediaDate = date;
    SeqNum++;
    comm_hdr.StoreSerialNum = SeqNum; 
    NSDL2_FC2(NULL, NULL, "date = %lu", comm_hdr.MediaDate);    
    memcpy(&buf[183], (char *)&comm_hdr, sizeof(COMMON_HEADER_t));
    memcpy(&buf[152], (char *)&date, sizeof(unsigned long));
    //memcpy(&buf[150], (char *)&SeqNum, sizeof(unsigned short));
    //memcpy(&buf[165], (char *)&SeqNum, sizeof(unsigned short));
    //memcpy(&buf[188], (char *)&SeqNum, sizeof(unsigned short));
   
    //memcpy(&buf[buf_offset], (char *)hdr_ptr,sizeof(hdr));
    
    //buf_offset = sizeof(hdr) + char_count;
    //msg_offset = sizeof(FED_CON_APP_HDR_T);
   
    //con_app_ptr = (FED_CON_APP_HDR_T *)(hdr_ptr + buf_offset);

    //memcpy(&buf[buf_offset], (char *)con_app_ptr,
     //                        sizeof(FED_CON_APP_HDR_T));

    //buf_offset = sizeof(hdr) + sizeof(FED_CON_APP_HDR_T);

    //char * loc1 = (char *)(hdr_ptr + buf_offset);
    //comm_header = (COMMON_HEADER_t *)(loc1);
    //get_curr_date_and_time(&date);
    //comm_header->MediaDate = date;
    //memcpy(&buf[buf_offset], (char *)con_app_ptr,
    //                       sizeof(COMMON_HEADER_t));

    //buf_offset = sizeof(hdr) + sizeof(FED_CON_APP_HDR_T) + sizeof(COMMON_HEADER_t);

    //if(filled_len - char_count > buf_offset)
    //{    
      //memcpy(&buf[buf_offset], (char *)(hdr_ptr + buf_offset), filled_len - char_count - buf_offset);
    //}
   //memcpy(&buf[buf_offset],(char *)&message[buf_offset],
      //         app_hdr->ulRequestDataSize);
    //memcpy(&buf[buf_offset], (char *)&message[buf_offset],
    //           hdr_ptr->ulInDataSize);
  
     
    buf_len = sizeof(hdr) + hdr_ptr->ulInDataSize;
    //buf_len = sizeof(hdr) + hdr_ptr->ulInDataSize + char_count;
 
    cptr->free_array = (char *)buf;  // We are using cptr->free_array to send message
    cptr->bytes_left_to_send = buf_len; 

  NSDL2_FC2(NULL, NULL, "FC2 Request Headers:hdr_ptr->usBOM =%hu hdr_ptr->usVersion =%hu hdr_ptr->usFC2Mark =%hu hdr_ptr->usDataCodePage =%hu"
                        "hdr_ptr->ulFlags =%lu hdr_ptr->ulExHdrSize =%lu hdr_ptr->ulInDataSize =%lu hdr_ptr->ulOutDataSize = %lu"
                        "hdr_ptr->ulTimeout =%lu hdr_ptr->ulRefId =%lu hdr_ptr->szMsgName =%s hdr_ptr->ucMessageType =%c"
                        "hdr_ptr->ucMessageType = %d",
                         hdr_ptr->usBOM, hdr_ptr->usVersion, hdr_ptr->usFC2Mark, hdr_ptr->usDataCodePage, hdr_ptr->ulFlags,
                         hdr_ptr->ulExHdrSize, hdr_ptr->ulInDataSize, hdr_ptr->ulOutDataSize, hdr_ptr->ulTimeout, hdr_ptr->ulRefId,
                         hdr_ptr->szMsgName, hdr_ptr->ucMessageType, hdr_ptr->ucMessageType);
  }

  cptr->conn_state = CNST_FC2_WRITING; 
  
  NSDL2_FC2(vptr, cptr, "cptr->free_array = %s cptr->bytes_left_to_send = %d", cptr->free_array, cptr->bytes_left_to_send);

  ret = fc2_handle_write(cptr, now);
  
  if (ret == FC2_ERROR)
    return FC2_ERROR; 
  else 
    return FC2_SUCCESS;
}


