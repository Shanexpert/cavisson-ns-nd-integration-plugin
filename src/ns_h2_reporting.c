/*********************************************************************************************
* Name                   : ns_h2_reporting.c  
* Purpose                : This C file holds the function(s) required for logging of Http2 . 
* Author                 :  
* Intial version date    : 14-October-2016 
* Last modification date : 19-October-2016 
*********************************************************************************************/

#include "ns_h2_header_files.h"

/**********Bug 70480: HTTP2 Server Push --> Start *******************/
extern void fprint2f(FILE *fp1, FILE* fp2,char* format, ...);

/* Http2 Server Push Graph info */
Http2SrvPush_gp* http2_srv_push_gp_ptr = NULL;
unsigned int http2_srv_push_gp_idx;

int g_srv_push_cavgtime_idx = -1;
/**********Bug 70480: HTTP2 Server Push --> End *******************/

/*

*/
#define HTTP2_PROTOCOL_ERROR(line_buf, ...)  \
{ \
  log_http2_protocol_error(_FLN_, line_buf, __VA_ARGS__);   \
} 


/*
Function for logging HTTP2 request 
*/

extern int *body_array; /*bug 78106*/

void debug_log_http2_req(connection *cptr, int size, char *data)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;

  NSDL2_HTTP2(NULL, cptr, "Method called");


  //char log_file[1024];
  char log_req_file[1024];
 // int log_fd;
  int log_data_fd;
  int ret;

  // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  SAVE_REQ_REP_FILES
  sprintf(log_req_file, "%s/logs/%s/url_req_http2_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

  if((log_data_fd = open(log_req_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    NSDL2_HTTP2(NULL, cptr, "Logging HTTP2 request data");
    ret = write(log_data_fd, data, size);
    if(ret == -1)
      fprintf(stderr, "Error: Error in dumping data to file for logging URL request\n");
    close(log_data_fd);
  }
}


/*
 Function for logging Http2 resp . This functions logs complete response including settings, Header , data and other frames received . 
*/

void debug_log_http2_rep(connection *cptr, int size, unsigned char *data)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;

  NSDL2_HTTP2(NULL, cptr, "Method called, size = %d", size);
  char log_req_file[1024];
  int log_data_fd;
  int ret;

  // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  SAVE_REQ_REP_FILES
  sprintf(log_req_file, "%s/logs/%s/url_rep_http2_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

  if((log_data_fd = open(log_req_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request. Error = %s\n", nslb_strerror(errno));
  else
  {
    NSDL2_HTTP2(NULL, cptr, "Logging HTTP2 request data");
    ret = write(log_data_fd, data, size);
    if(ret < 0)
      fprintf(stderr, "Error: Error in dumping data to file for logging URL response. Error = %s\n", nslb_strerror(errno));
    close(log_data_fd);
  }
}


void log_http2_res(connection *cptr, VUser *vptr, unsigned char *buf, int size)
{
  char log_file[1024];
  int log_fd;

  NSDL2_HTTP2(NULL, NULL, "Method Called");

  /* Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
   * url_id is not yet implemented (always 0)
   * In release 3.9.7, create directory in TR or partition directory(NDE-continues monitoring) for request and response files
   * path:  logs/TRxx/ns_logs/req_rep/
   * or
   * logs/TRxx/<partition>/ns_logs/req_rep/
   * */
  SAVE_REQ_REP_FILES

  sprintf(log_file, "%s/logs/%s/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

  NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u",
                          GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

  if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    /*bug 68963: print url in response file in case of conn fail */
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    {
      write(log_fd, cptr->url, cptr->url_len);
      write(log_fd, "\n", 1);
    }
    write(log_fd, buf, size); //TODO: check failure for write
    close(log_fd);
  }

  if(g_parent_idx != -1)
    sprintf(log_file, "%s/logs/TR%s/%lld/ns_logs/req_rep/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          getenv("NS_CONTROLLER_WDIR"), getenv("NS_CONTROLLER_TEST_RUN"), vptr->partition_idx,
          child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

  // Do not change the debug trace message as it is parsed by GUI
  if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    NS_DT4(vptr, cptr, DM_L1, MM_HTTP, "Response is in file '%s'", log_file);
}


void debug_log_http2_res(connection *cptr, unsigned char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type; // aded variable request_type to check if it is a HTTP/HTTPS request or not
    
  NSDL1_HTTP2(NULL, NULL, "Method called");
      
  if (((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4)  &&
       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP) && ((request_type == HTTP_REQUEST || request_type == HTTPS_REQUEST || request_type == WS_REQUEST))) || (NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)))
  log_http2_res(cptr, vptr, buf, size);
} 

void log_http2_res_ex(connection *cptr, unsigned char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;

  NSDL1_HTTP2(NULL, NULL, "Method called");

  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED
                                 || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_WHOLE_IF_PG_TX_FAIL)
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)
                            && ((cptr->url_num->proto.http.type == EMBEDDED_URL
                                 && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url)
                                 || cptr->url_num->proto.http.type != EMBEDDED_URL))
  log_http2_res(cptr, vptr, buf, size);
}


void debug_log_http2_dump_req(connection *cptr, struct iovec *vector, int body_start_idx, int total_body_vectors)
{
  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!(((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) &&
       (request_type == HTTP_REQUEST || request_type == HTTPS_REQUEST  || request_type == WS_REQUEST || request_type == WSS_REQUEST)&&
       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP))||
       ((NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)))))
   {
        
     NSDL3_HTTP(vptr, cptr, "Either debug is disable or page dump feature is disable.hence returning");
     return;
   }

  http2_dump_req(cptr, vector, body_start_idx, total_body_vectors);

}

void log_http2_dump_req(connection *cptr, struct iovec *vector, int body_start_idx, int total_body_vectors)
{
  VUser* vptr = cptr->vptr;

  NSDL1_HTTP2(NULL, NULL, "Method called");

  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED 
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_WHOLE_IF_PG_TX_FAIL)
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)
                            && ((cptr->url_num->proto.http.type == EMBEDDED_URL
                                && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url)
                                || cptr->url_num->proto.http.type != EMBEDDED_URL))
  {
    http2_dump_req(cptr, vector, body_start_idx, total_body_vectors);
  }
}
/*bug 78106 - this method dump both HTTP2 request header and body*/
void http2_dump_req(connection *cptr, struct iovec *vector, int body_start_idx, int total_body_vectors)
 {
 
   if((cptr == NULL) || (vector == NULL))
     return ;
   VUser *vptr =  (VUser *)cptr->vptr;
   char log_dump[1024];
   FILE *log_fp = NULL;
   size_t i;
   char line_break[] = "\n---------------------------------------------------------------------------------------------------\n";
   SAVE_REQ_REP_FILES
   sprintf(log_dump, "%s/logs/%s/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
           g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
           vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
           GET_PAGE_ID_BY_NAME(vptr));
 
   log_fp = fopen(log_dump, "a+");
   if(log_fp == NULL)
   {
     fprintf(stderr, "Error: Error in opening file for logging request \n");
     return;
   }
   NSDL2_HTTP2(NULL, NULL, "Logging HTTP2 header");
   for(i = 0; i < nghttp2_nv_tot_entries; i++){
     fprintf(log_fp, "%*.*s: ", (int)http2_hdr_ptr[i].namelen, (int)http2_hdr_ptr[i].namelen, http2_hdr_ptr[i].name);
     fprintf(log_fp, "%*.*s\n", (int)http2_hdr_ptr[i].valuelen, (int)http2_hdr_ptr[i].valuelen, http2_hdr_ptr[i].value);
   }
   //fprintf(log_fp, "\n" ); //add a blank line
   NSDL2_HTTP2(NULL, NULL, "Logging HTTP2 body");
   for(i = body_start_idx; i < total_body_vectors; i++)
   {
       NSDL4_VARS(vptr, NULL, "vector[i].iov_base = %*.*s", (int)vector[i].iov_len, (int)vector[i].iov_len, (char*)vector[i].iov_base);
       fprintf(log_fp, "%*.*s ",  (int)vector[i].iov_len,  (int)vector[i].iov_len, (char*)vector[i].iov_base);
   }
   fprintf(log_fp, "%s", line_break);
   fclose(log_fp);
 }


/**********Bug 70480: HTTP2 Server Push --> Start *******************/
// Add size of http2 server push if it is enabled 
/*update both avg and cavg size*/
inline void h2_server_push_update_avgtime_size() {
 
  NSDL2_CACHE(NULL, NULL, "Method Called, g_avgtime_size = %d, g_srv_push_cavgtime_idx = %d",
                                          g_avgtime_size, g_srv_push_cavgtime_idx);
  if(IS_SERVER_PUSH_ENABLED) {
    NSDL2_CACHE(NULL, NULL, "HTTP2 Server Push is enabled.");
    /*update g_cavtime_size*/
    g_srv_push_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(SrvPushAvgTime);
  } else { 
    NSDL2_CACHE(NULL, NULL, "HTTP2 Server Push is disabled.");
  }

  NSDL2_CACHE(NULL, NULL, "After g_avgtime_size = %d, g_srv_push_cavgtime_idx = %d",
                                          g_avgtime_size, g_srv_push_cavgtime_idx);
}

/*put Server Pushed Rersource information in progress report, if Server Push is enabled*/
inline void h2_server_push_print_progress_report(FILE *fp1, FILE *fp2, cavgtime *cavg) {

  if(!cavg)
    return;
  NSDL2_CACHE(NULL, NULL, "Method Called");
  if(IS_SERVER_PUSH_ENABLED)
    fprint2f(fp1, fp2, "    HTTP2 Server Push Resources : Total=%d\n", cavg->cum_srv_pushed_resources);
  else
    NSDL2_CACHE(NULL, NULL, "Http2 Server Push is  disabled.");
}
/**********Bug 70480: HTTP2 Server Push --> End *******************/
