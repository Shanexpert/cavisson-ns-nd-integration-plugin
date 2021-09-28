/********************************************************************************
 * File Name            : ns_rbu_access_log.c
 * Author(s)            : Shikha
 * Date                 : 21 July 2017
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains function related to RBU access log,
 *                        
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "ns_log.h"
#include "nslb_util.h"
#include "nslb_json_parser.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "ns_cache_include.h"
#include "ns_vuser_thread.h"
#include "ns_url_req.h"
#include "ns_page_dump.h"
#include "ns_script_parse.h"
#include "ns_http_script_parse.h"
#include "ns_vuser_tasks.h"
#include "ns_error_codes.h"
#include "ns_http_process_resp.h"
#include "ns_vuser_ctx.h"
#include "ns_alloc.h"
#include "ns_rbu.h"
#include "nslb_json_parser.h"
#include "ns_rbu_api.h"
#include "url.h"
#include "ns_click_script.h"
#include "ns_network_cache_reporting.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_child_thread_util.h"
#include "ns_vuser.h"
#include "ns_session.h"
#include "ns_rbu_page_stat.h"
#include "netstorm.h"
#include "ns_trace_level.h"
#include "ns_data_types.h"
#include "util.h"
#include "math.h"
#include "nslb_cav_conf.h"
#include "netomni/src/core/ni_scenario_distribution.h"        

#define MAX_LOG_BUF_SIZE  32000

#define ACCESS_LOG_HEADER "#ClientIp	ServerIp:port	-	Date	Time	RespTime(ms)	ReqMethod	URL	Parameter	StatusCode	RepSize(kB) 	UserAgent	GrpID:NVMID:UID:SessId:PgId	LocationName	BrowserType	BrowserVersion	Message	TimeStamp	HTTPVersion\n"

/* For usage track*/
static int access_log_fd = -1;
static int dump_header = 0;

#define MAX_AL_FNAME 1024

static char access_log_fname[MAX_AL_FNAME + 1] = "";

//This function will make access log file name and open it
static void gen_and_open_access_log_file(VUser *vptr, int *fd, int write_header, int *dump_header)
{
  char file_name[32+1]  = "rbu_access_log";

  char ns_logs_file_path[256 + 1]; 
  char rbu_logs_file_path[256 + 1]; 
  if(vptr->partition_idx <= 0) { 
    sprintf(ns_logs_file_path, "TR%d/ns_logs", testidx); 
    sprintf(rbu_logs_file_path, "TR%d/rbu_logs", testidx); 
  } 
  else { 
    sprintf(ns_logs_file_path, "TR%d/%lld/ns_logs", testidx, vptr->partition_idx); 
    sprintf(rbu_logs_file_path, "TR%d/%lld/rbu_logs", testidx, vptr->partition_idx); 
  }

  NSDL4_RBU(NULL, NULL, "Method called");


  // Generate file name
  sprintf(access_log_fname, "%s/logs/%s/%s", g_ns_wdir, rbu_logs_file_path, file_name);
  write_header = 1; // Force it to 1 as we need to write header

  NSDL4_RBU(NULL, NULL, "Method called access_log_fname = %s file_name =%s dump_header = %d", 
                        access_log_fname, file_name, *dump_header);

  // Open file
  *fd = open (access_log_fname, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
  NSDL4_RBU(NULL, NULL, "Method called access_log_fname = %s", access_log_fname);
  if (*fd < 0)
  {
    fprintf(stderr, "Error: Error in opening file '%s', Error = '%s'\n", file_name, nslb_strerror(errno));
    return;
  }

  if(write_header && !(*dump_header))
  {
     struct stat stat_buf;
    /* When we restart hpd and access_log file is already exist,
     * Header is not append in this file,
     * If we create acces_log file append header first.
     * For this case we check size of the file 
     * stat return 0 because file is already created, but file is empty yet
     * This is a one time job when restart or rolleover the access_log tife*/
     stat(access_log_fname, &stat_buf);
     if(stat_buf.st_size == 0)
       write(*fd, ACCESS_LOG_HEADER, strlen(ACCESS_LOG_HEADER));
     *dump_header = 1;
  }
}

//In this method we are dumping all acces log data 
//log_hdr_flag : keep track of what to dump, 0: header and 1: access log
void ns_rbu_log_access_log(VUser *vptr, RBU_RespAttr *rbu_resp_attr, int log_hdr_flag)
{
  char buffer[MAX_LOG_BUF_SIZE + 1] = "";
  char resp_size[64] = "-";
  char rbu_req_method[8] = "-";
  char NA[2] = "-";

  char client_ip[32] = "-";
  char server_ip[32] = "-";
  char all_url_resp_time[32] = "-";
  char pg_status[8] = "-";
  char date[32+1] = "-";
  char tme[32+1] = "-";
  char inline_url[128] = "-";                   //Store Inline Url of Page
  char brwsr_vrsn[8] = "-";                     //Store Browser Version 

  char uniq_id[32+1] = "-";
  char UA[RBU_USER_AGENT_LENGTH + 1] = "-"; 
  char brwsr[32 + 1] = "-"; 
  char tm_stmp[128] = "-";                      //Store timestamp for each request
  char param_string[128] = "-";                 //Store Query Parameter of a URL

  char acc_log_msg[512+1] = "-";                //Store Message of Access Logg

  int ua_len = 0;                               //Stores User-Agent len
  char http_vrsn[16];                           //Stores HTTPVersion of response

  NSDL4_RBU(NULL, NULL, "Method called. dump_header = %d and log_hdr_flag = %d", dump_header, log_hdr_flag);

  if(dump_header && log_hdr_flag){
    snprintf(uniq_id, 32, "%d:%d:%d:%d:%d", vptr->group_num, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance);
    uniq_id[strlen(uniq_id)+1] = '\0';

    NSDL4_RBU(NULL, NULL, "uniq_id - [%s]", uniq_id);

    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == 1)
      strcpy(brwsr, "Chromium-Browser");
    else
      strcpy(brwsr, "Firefox");
   
    //In NetCloud test : Access log will be generated at generators, So need to fetch machine IP.
    NSDL4_RBU(NULL, NULL, "RBU Access Log: g_cavinfo.NSAdminIP = %s", g_cavinfo.NSAdminIP);
    strcpy(client_ip, g_cavinfo.NSAdminIP);

    if(rbu_resp_attr == NULL){
      if (vptr->httpData->rbu_resp_attr != NULL) {
        rbu_resp_attr = vptr->httpData->rbu_resp_attr;
        NSDL4_RBU(NULL, NULL, "RBU_RespAttr [%p]", rbu_resp_attr);
      }
      if (vptr->httpData->rbu_resp_attr == NULL){
        NSDL4_RBU(NULL, NULL, "Dont Have RBU Structure, so will not dump access_log");
      }
    }
  
    if(rbu_resp_attr->all_url_resp_time > 0){
      all_url_resp_time[32] = '\0';
      snprintf(all_url_resp_time, 32 - 1, "%f", rbu_resp_attr->all_url_resp_time);
    }
 
    if(rbu_resp_attr->in_url_pg_wgt >= 0)
      snprintf(resp_size, 64, "%f", rbu_resp_attr->in_url_pg_wgt);

    if(vptr->last_cptr->req_code > 0)
      snprintf(pg_status, 8, "%d", vptr->last_cptr->req_code);

    if(!(!strcmp(rbu_resp_attr->req_method, "")))
      strncpy(rbu_req_method, rbu_resp_attr->req_method, 5);

    if(!(!strcmp(rbu_resp_attr->server_ip_add, "")))
      strncpy(server_ip, rbu_resp_attr->server_ip_add, 32);

    if(!(!strcmp(rbu_resp_attr->date, "")))
      strncpy(date, rbu_resp_attr->date, 32);

    if(!(!strcmp(rbu_resp_attr->time, "")))
      strncpy(tme, rbu_resp_attr->time, 32);

    if(!(!strcmp(rbu_resp_attr->inline_url, "")))
      strcpy(inline_url, rbu_resp_attr->inline_url);
    
    if(!(!strcmp(rbu_resp_attr->param_string, "")))
      strcpy(param_string, rbu_resp_attr->param_string);

    if(!(!strcmp(rbu_resp_attr->http_vrsn, "")))
      strcpy(http_vrsn, rbu_resp_attr->http_vrsn);
    
    if(!(!strcmp(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.brwsr_vrsn, "")))
      strncpy(brwsr_vrsn, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.brwsr_vrsn, 8);
   
    if(!(!strcmp(rbu_resp_attr->user_agent_str, "")))
      snprintf(UA, RBU_USER_AGENT_LENGTH, "%s", rbu_resp_attr->user_agent_str);
    else{
      if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 1)
      {
        NSDL2_RBU(vptr, NULL, "Access Log: Setting user agent from user profile");
        if (!(runprof_table_shr_mem[vptr->group_num].gset.disable_headers & NS_UA_HEADER))
        {
          if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL)
            && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
          {
            ua_len = (strlen(vptr->browser->UA) -1);  //Coming with ^M

            if(ua_len < RBU_USER_AGENT_LENGTH)
              ua_len = ua_len;
            else
              ua_len = RBU_USER_AGENT_LENGTH;
        
            NSDL2_RBU(vptr, NULL, "Access Log: User-Agent = %s", vptr->browser->UA);
            snprintf(UA, ua_len, "%s", vptr->browser->UA);
          }
          else {
            ua_len = (strlen(vptr->httpData->ua_handler_ptr->ua_string) -1);

            if(ua_len < RBU_USER_AGENT_LENGTH)
              ua_len = ua_len;
            else
              ua_len = RBU_USER_AGENT_LENGTH;
        
            NSDL2_RBU(vptr, NULL, "Access Log: User-Agent = %s", vptr->httpData->ua_handler_ptr->ua_string);
            snprintf(UA, ua_len, "%s", vptr->httpData->ua_handler_ptr->ua_string);
          }
        }
      }
      else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 2)
      {
        NSDL2_RBU(vptr, NULL, "Access Log: Setting user agent provide by user, User-Agent = %s",
                               runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name);
        snprintf(UA, RBU_USER_AGENT_LENGTH, "%s", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name);
      }
    }

    if(rbu_resp_attr->url_date_time < 0)
      rbu_resp_attr->url_date_time = (time(NULL)  * 1000);

    snprintf(tm_stmp, 32, "%lld", rbu_resp_attr->url_date_time);

    if(vptr->page_status == NS_REQUEST_ERRMISC || vptr->page_status == NS_REQUEST_TIMEOUT)
    {
      strcpy(pg_status, "-");
      strcpy(all_url_resp_time, "-"); 
      strcpy(resp_size, "-"); 
      strcpy(rbu_req_method, "-"); 
      strcpy(server_ip, "-"); 
      strcpy(date, "-"); 
      strcpy(tme, "-"); 
      strcpy(inline_url, "-"); 
      strcpy(param_string, "-"); 
      strcpy(http_vrsn, "-");                             //In case of MiscErr or TimeoOut, HTTPVersion = "-"
    }

    strncpy(acc_log_msg, rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH);
    acc_log_msg[strlen(acc_log_msg)+1] = '\0';

    NSDL4_RBU(NULL, NULL, "Access Log : all_url_resp_time - [%s], rbu_req_method - [%s], resp_size - [%s], server_ip - [%s]"
                          "brwsr_vrsn - [%s], pg_status - [%s], url_time_stamp = %s, param_string = %s, uniq_id = %s, "
                          "http_vrsn = %s", 
                           all_url_resp_time, rbu_req_method, resp_size, server_ip, brwsr_vrsn, pg_status, tm_stmp, param_string, 
                           uniq_id, http_vrsn);
 
    sprintf(buffer, "\n\n%s\t%s\t%s\t%s\t%s\t%s\t%s\t\"%s\"\t\"%s\"\t%s\t%s\t\"%s\"\t%s\t%s\t%s\t%s\t\"%s\"\t%s\t%s", 
                   client_ip, server_ip, NA, date, tme, all_url_resp_time, rbu_req_method, 
                   inline_url, param_string, pg_status, resp_size, UA, uniq_id,
                   global_settings->event_generating_host, brwsr, brwsr_vrsn, acc_log_msg, tm_stmp, http_vrsn);

    NSDL4_RBU(NULL, NULL, "BUFFER after method dump %s", buffer);
  }

  NSDL4_RBU(NULL, NULL, "Access log : dump_header = %d", dump_header);
  gen_and_open_access_log_file(vptr, &access_log_fd, 1, &dump_header);
  write(access_log_fd, buffer, strlen(buffer));
}
