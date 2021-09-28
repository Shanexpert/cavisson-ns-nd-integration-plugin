#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/prctl.h>
#ifdef SLOW_CON
#include <linux/socket.h>
#include <netinet/tcp.h>
#define TCP_BWEMU_REV_DELAY 16
#define TCP_BWEMU_REV_RPD 17
#define TCP_BWEMU_REV_CONSPD 18
#endif
#ifdef NS_USE_MODEM
#include <linux/socket.h>
//#include <linux/cavmodem.h>
#include <netinet/tcp.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_hessian.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "user_tables.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_cavmain_child_thread.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "cavmodem.h"
#include "ns_wan_env.h"

#endif
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef USE_EPOLL
//#include <asm/page.h>
// This code has been commented for FC8 PORTING
//#include <linux/linkage.h>
#include <linux/unistd.h>
#include <sys/epoll.h>
#include <asm/unistd.h>
#endif
#include <math.h>
#include "runlogic.h"
#include "uids.h"
#include "cookies.h"
//#include "logging.h"
#include <gsl/gsl_randist.h>
#include "weib_think.h"
#include "netstorm.h"
#include <pwd.h>
#include <stdarg.h>
#include <sys/file.h>

#include "decomp.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "util.h" 
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "amf.h"
#include "eth.h"
#include "timing.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_master_agent.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "server_stats.h"
#include "ns_trans.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
#include "ns_parent.h"
#include "ns_child_msg_com.h"
#include "ns_http_hdr_states.h"
#include "ns_url_resp.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_auto_fetch_embd.h"
#include "ns_parallel_fetch.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_replay_access_logs.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_page.h"
#include "ns_vuser.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_global_dat.h"
#include "ns_smtp_send.h"
#include "ns_smtp.h"
#include "ns_pop3_send.h"
#include "ns_pop3.h"
#include "ns_ftp_send.h"
#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_http_status_codes.h"

#include "ns_server_mapping.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_log_req_rep.h"
#include "ns_page_dump.h"
#include "ns_vuser_trace.h"
#include "ns_java_obj_mgr.h"
#include "ns_h2_reporting.h"
#include "ns_file_upload.h"
#include "ns_test_monitor.h"

#ifdef NS_DEBUG_ON
static void  gen_debug_log_http_body_xml(VUser *vptr, char *encoding, char *buf_ptr, int bytes_to_log)
{
  // Log file name format is url_req_amf_body_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  char log_file[4096] = "\0";
  /*sprintf(log_file, "%s/logs/TR%d/url_req_%s_body_%hd_%u_%u_%d_0_%d_%d_%d_0.xml",
                    g_ns_wdir, testidx, encoding, child_idx, vptr->user_index, 
                    vptr->sess_inst, vptr->page_instance, vptr->group_num,
                    GET_SESS_ID_BY_NAME(vptr),
                    GET_PAGE_ID_BY_NAME(vptr));*/
  SAVE_REQ_REP_FILES

  sprintf(log_file, "%s/logs/%s/url_req_%s_body_%hd_%u_%u_%d_0_%d_%d_%d_0.xml",
                    g_ns_wdir, req_rep_file_path, encoding, child_idx, vptr->user_index, 
                    vptr->sess_inst, vptr->page_instance, vptr->group_num,
                    GET_SESS_ID_BY_NAME(vptr),
                    GET_PAGE_ID_BY_NAME(vptr));

    NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  FILE *log_fp = fopen(log_file, "a+");
  if (log_fp == NULL)
  {
    fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
    return;
  }
  //write for both ssl and non ssl url
  if(fwrite(buf_ptr, bytes_to_log, 1, log_fp) != 1)
  {
      fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf_ptr);
      return;
  }
  if(fclose(log_fp) != 0)
  {
    fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
    return;
  }
  //SS: 19/4/13 To add amf-xml in url_req. It will be placed above the request.
  log_http_req(NULL, vptr, buf_ptr, bytes_to_log, 1, 0);
}

static void amf_debug_log_http_body_xml(VUser *vptr, char *buf_ptr, int bytes_to_log){
  gen_debug_log_http_body_xml(vptr, "amf", buf_ptr, bytes_to_log);
}

static void hessian_debug_log_http_body_xml(VUser *vptr, char *buf_ptr, int bytes_to_log){
  gen_debug_log_http_body_xml(vptr, "hessian", buf_ptr, bytes_to_log);
}

static void java_obj_debug_log_http_body_xml(VUser *vptr, char *buf_ptr, int bytes_to_log){
  gen_debug_log_http_body_xml(vptr, "java_obj", buf_ptr, bytes_to_log);
}


static void  gen_debug_log_http_body_binary(VUser *vptr, char *encoding, char *buf_ptr, int bytes_to_log)
{
  // Log file name format is url_req_amf_body_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  char log_file[4096] = "\0";
  /*sprintf(log_file, "%s/logs/TR%d/url_req_%s_body_%hd_%u_%u_%d_0_%d_%d_%d_0.%s", 
                     g_ns_wdir, testidx, encoding, child_idx, vptr->user_index,
                     vptr->sess_inst, vptr->page_instance, vptr->group_num, 
                     GET_SESS_ID_BY_NAME(vptr), 
                     GET_PAGE_ID_BY_NAME(vptr), encoding);*/
  SAVE_REQ_REP_FILES

  sprintf(log_file, "%s/logs/%s/url_req_%s_body_%hd_%u_%u_%d_0_%d_%d_%d_0.%s", 
                     g_ns_wdir, req_rep_file_path, encoding, child_idx, vptr->user_index,
                     vptr->sess_inst, vptr->page_instance, vptr->group_num, 
                     GET_SESS_ID_BY_NAME(vptr), 
                     GET_PAGE_ID_BY_NAME(vptr), encoding);
    NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  FILE *log_fp = fopen(log_file, "a+");
  if (log_fp == NULL)
  {
    fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
    return;
  }

  //write for both ssl and non ssl url
  if(fwrite(buf_ptr, bytes_to_log, 1, log_fp) != 1)
  {
      fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf_ptr);
      return;
  }
  if(fclose(log_fp) != 0)
  {
    fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
    return;
  }
}

static void  amf_debug_log_http_body_binary(VUser *vptr, char *amf_buf_ptr, int bytes_to_log){
  gen_debug_log_http_body_binary(vptr, "amf", amf_buf_ptr, bytes_to_log);
}

static void  hessian_debug_log_http_body_binary(VUser *vptr, char *amf_buf_ptr, int bytes_to_log){
  gen_debug_log_http_body_binary(vptr, "hessian", amf_buf_ptr, bytes_to_log);
}
static void  java_obj_debug_log_http_body_binary(VUser *vptr, char *amf_buf_ptr, int bytes_to_log){
  gen_debug_log_http_body_binary(vptr, "java_obj", amf_buf_ptr, bytes_to_log);
}

// These buffers are made static as these were taking 8192 + 2 of stack, and default stack value of system is 8192, so hessian and amf with debug 
// will dump core at this place. These buffers are locally here to dump the response in debug files and have no meaning after that. so anyone can 
// use these buffer for debugging purpose
#define  ENCODING_BUF_SIZE 4*1024*1024
static char xml_buf[ENCODING_BUF_SIZE + 1], mbuf[ENCODING_BUF_SIZE + 1];

void gen_debug_log_http_req(connection *cptr, struct iovec *vector, int num_vectors, int body_encoding_flag)
{

  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        ((request_type == HTTP_REQUEST || request_type == HTTPS_REQUEST )&&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP))))
    return;

  int i, size=0;
  char srcip[128];
  strcpy (srcip, nslb_get_src_addr(cptr->conn_fd));

  NSDL3_HTTP(vptr, cptr, "***** Sending (nvm=%hd sees_inst=%u user_index=%u src_ip=%s dest_ip=%s)", 
       child_idx, vptr->sess_inst, vptr->user_index, srcip, nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1));

  char *mptr = mbuf;
  // char sbyte;

  char *buf_ptr = NULL;
  int bytes_to_log = 0;

  // Combine all vector data so that complete HTTP request is in mbuf (mptr)
  for (i = 0; i < num_vectors; i++)
  {
    if(vector[i].iov_base) {
      memcpy(mptr, vector[i].iov_base, vector[i].iov_len);
      mptr += vector[i].iov_len;
      size += vector[i].iov_len;
    } else {
      NSDL3_HTTP(NULL, cptr, "Got NULL vector at index = %d", i); 
      continue;
    }
  }
  mbuf[size]='\0';
  // At this point size is total size of http request including body


  if ((mptr = strstr(mbuf, "\r\n\r\n"))) // Search end of headers
  {
    mptr += 4; // Point to the start of body
    size -= (mptr-mbuf); // Get body size be reducing size before body

    // sbyte = *mptr;
    // *mptr = '\0';
    // printf(mbuf);
    // *mptr = sbyte;
    // show_buf (mptr, size);

    // Log binary in amf file
    if(size > 0)
    {
      switch (body_encoding_flag){
        case BODY_ENCODING_AMF :
          amf_debug_log_http_body_binary(vptr, mptr, size);

          amf_decode (ENCODING_BUF_SIZE, xml_buf, 0, mptr, &size);
          buf_ptr = xml_buf;
          bytes_to_log = strlen(buf_ptr);

          // Log xml in xml file
          amf_debug_log_http_body_xml(vptr, buf_ptr, bytes_to_log);
         break;
        case BODY_ENCODING_HESSIAN :
          hessian_debug_log_http_body_binary(vptr, mptr, size);
          hessian_set_version(2);
          if((hessian2_decode(ENCODING_BUF_SIZE, xml_buf, 0, mptr, &size))){
            bytes_to_log = strlen(xml_buf);
            hessian_debug_log_http_body_xml(vptr, xml_buf, bytes_to_log);
          }else{
            fprintf(stderr, "Error in decoding hessian buffer, ignored. Original buffer will be used. Error = %s", nslb_strerror(errno));
             bytes_to_log = size;
             hessian_debug_log_http_body_xml(vptr, mptr, bytes_to_log);

          }
          break; 
        case BODY_ENCODING_JAVA_OBJ: {
          int out_len = 0;
          int total_len = 0;
          u_ns_ts_t start_timestamp, end_timestamp, time_taken;

          NSDL3_HTTP(vptr, NULL, "Got Java Object in response");
          java_obj_debug_log_http_body_binary(vptr, mptr, size);
          if((total_len = create_java_obj_msg(1, mptr, xml_buf, &size, &out_len, 2)) > 0){

            start_timestamp = get_ms_stamp();
            if(send_java_obj_mgr_data(xml_buf, out_len, 1) != 0){
	      fprintf(stderr, "Error in sending data to java object manager.\n");
	      end_test_run(); 
            } 
            memset(xml_buf, 0, ENCODING_BUF_SIZE);
            if(read_java_obj_msg(xml_buf, &bytes_to_log, 1) != 0){
	      fprintf(stderr, "Error in receiving data to java object manager.\n");
	      end_test_run(); 
            }
            NSDL3_HTTP(vptr, NULL, "bytes_to_log = %d, xml_buf = %s", bytes_to_log, xml_buf);
            // bytes_to_log = strlen(xml_buf);
            java_obj_debug_log_http_body_xml(vptr, xml_buf, bytes_to_log);
          }else{
            fprintf(stderr, "Error in crating message data for java object manager.\n");
            end_test_run(); 
          }
          end_timestamp = get_ms_stamp();
          time_taken = end_timestamp - start_timestamp;
          NSDL3_HTTP(vptr, NULL, "Time taken = [%d]ms, Threshold = [%lld]ms", time_taken, global_settings->java_object_mgr_threshold);
          if (time_taken > global_settings->java_object_mgr_threshold)
            NS_DT1(vptr, NULL, DM_L1, MM_HTTP, "Time taken by Java object manager is exceeding threshold value. Time taken = [%d]ms, Threshold = [%lld]ms", time_taken, global_settings->java_object_mgr_threshold);

          break;
        }
        default :
          fprintf(stderr, "It should not come here");

      }
    }
  }
  else
  {
    fprintf(stderr, "Error: body encoding  - No body marker found: %s\n", mbuf);
    return;
  }
}

void java_obj_debug_log_http_req(connection *cptr, struct iovec *vector, int num_vectors){
  if (cptr->url_num->proto.http.body_encoding_flag != BODY_ENCODING_JAVA_OBJ)
    return;
  gen_debug_log_http_req(cptr, vector, num_vectors, BODY_ENCODING_JAVA_OBJ);
}
void hessian_debug_log_http_req(connection *cptr, struct iovec *vector, int num_vectors){
  if (cptr->url_num->proto.http.body_encoding_flag != BODY_ENCODING_HESSIAN)
    return;
  gen_debug_log_http_req(cptr, vector, num_vectors, BODY_ENCODING_HESSIAN);
}
void amf_debug_log_http_req(connection *cptr, struct iovec *vector, int num_vectors){
  if (cptr->url_num->proto.http.body_encoding_flag != BODY_ENCODING_AMF)
    return;
  gen_debug_log_http_req(cptr, vector, num_vectors, BODY_ENCODING_AMF);
}
#endif /* NS_DEBUG_ON */

void cache_debug_log_cache_res(connection *cptr)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  CacheTable_t* cache_ptr = (CacheTable_t*)cptr->cptr_data->cache_data;
  CacheResponseHeader* crh = cache_ptr->cache_resp_hdr;
  //int complete_buffers = cache_ptr->resp_len / COPY_BUFFER_LENGTH;
  //int incomplete_buf_size = cache_ptr->resp_len % COPY_BUFFER_LENGTH;
  //struct copy_buffer* buffer = cache_ptr->resp_buf_head;
  //int i;

#ifdef NS_DEBUG_ON
    if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4)  && 
       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_CACHE)))
       return;
#endif

    char log_file[1024];
    FILE *log_fp;

    // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    SAVE_REQ_REP_FILES
  
    sprintf(log_file, "%s/logs/%s/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
            g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));
    
    NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    if((log_fp = fopen(log_file, "a+")) == NULL)
      fprintf(stderr, "Error: Error in opening file for logging URL request \n");
    else
    {
      fprintf(log_fp, "Response from Cache:\n");
      fprintf(log_fp, "--------------------\n");

      if(cache_ptr->url_len > 0)
        fprintf(log_fp, "URL = %s\n", cache_ptr->url);
      
      fprintf(log_fp, "url offset = %hd\n", cache_ptr->url_offset);

      if(cache_ptr->location_url > 0)
        fprintf(log_fp, "Location URL = %s\n", cache_ptr->location_url);

      fprintf(log_fp, "Cache Headers:\n");
      fprintf(log_fp, "Max age = %d\n", crh->max_age);
      fprintf(log_fp, "Expires = %d\n", crh->expires);

      fprintf(log_fp, "Last modified time = "); 
      if(crh->last_modified_len > 0)
         fprintf(log_fp, "%s\n", crh->last_modified);
      else
         fprintf(log_fp, "NULL\n");
           
      fprintf(log_fp, "Etag = "); 
      if(crh->etag_len > 0)
         fprintf(log_fp, "Etag = %s\n", crh->etag);
      else
         fprintf(log_fp, "NULL\n");

      fprintf(log_fp, "Age = %d\n", crh->age);
      fprintf(log_fp, "Server Date= %d\n", crh->date_value);

      fprintf(log_fp, "No-Cache = ");
      if(crh->CacheControlFlags.no_cache)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "No-Store = ");
      if(crh->CacheControlFlags.no_store)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "Must-Revalidate = ");
      if(crh->CacheControlFlags.must_revalidate)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      //fprintf(log_fp, "Other Cache Values:\n");
       
      fprintf(log_fp, "NS_CACHE_ENTRY_IS_CACHABLE = ");
      if(cache_ptr->cache_flags & NS_CACHE_ENTRY_IS_CACHABLE)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "NS_CACHE_ENTRY_IN_CACHE = ");
      if(cache_ptr->cache_flags & NS_CACHE_ENTRY_IN_CACHE)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "NS_CACHEABLE_HEADER_PRESENT = ");
      if(cache_ptr->cache_flags & NS_CACHEABLE_HEADER_PRESENT)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "NS_VALIDATOR_PRESENT = ");
      if(cache_ptr->cache_flags & NS_VALIDATOR_PRESENT)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "NS_304_FOR_ETAG = ");
      if(cache_ptr->cache_flags & NS_304_FOR_ETAG)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "NS_CACHE_ENTRY_NEW = ");
      if(cache_ptr->cache_flags & NS_CACHE_ENTRY_NEW)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "NS_CACHE_ENTRY_VALIDATE = ");
      if(cache_ptr->cache_flags & NS_CACHE_ENTRY_VALIDATE)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");

      fprintf(log_fp, "NS_CACHE_ENTRY_EXPIRED = ");
      if(cache_ptr->cache_flags & NS_CACHE_ENTRY_EXPIRED)
        fprintf(log_fp, "1\n");
      else
        fprintf(log_fp, "0\n");


      fprintf(log_fp, "Response Length = %u\n", cache_ptr->resp_len);
      fprintf(log_fp, "Time in seconds at which the response is received = %llu\n", cache_ptr->cache_ts);

      fprintf(log_fp, "Time in seconds at which the request is send = %llu\n", cache_ptr->request_ts);
      fprintf(log_fp, "Document Age when received = %u\n", cache_ptr->doc_age_when_received);
      fprintf(log_fp, "HashValue = %u\n", cache_ptr->ihashValue);

      fprintf(log_fp, "Body Offset = %d\n", cache_ptr->body_offset);
      fprintf(log_fp, "Compression type = %hd\n", cache_ptr->compression_type);
      fprintf(log_fp, "Cache Hit Count= %hd\n", cache_ptr->cache_hits_cnt);

      if(cache_ptr->next != NULL)
        fprintf(log_fp, "next = %p\n", cache_ptr->next);
      else
        fprintf(log_fp, "next = NULL\n");
      
/*
      for (i = 0; i < complete_buffers; i++) {
        if (buffer && buffer->buffer) {
          fprintf(log_fp, "%*.*s\n", COPY_BUFFER_LENGTH, COPY_BUFFER_LENGTH, buffer->buffer);
          buffer = buffer->next;
        }
      }
      if (incomplete_buf_size)
      {
        if (buffer && buffer->buffer)
          fprintf(log_fp, "%*.*s\n", incomplete_buf_size, incomplete_buf_size, buffer->buffer);
      }
*/
      fclose(log_fp);
    }

    if(g_parent_idx != -1)
      sprintf(log_file, "%s/logs/TR%s/%lld/ns_logs/req_rep/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
            getenv("NS_CONTROLLER_WDIR"), getenv("NS_CONTROLLER_TEST_RUN"), vptr->partition_idx,
            child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));

    NS_DT4(vptr, cptr, DM_L1, MM_CACHE, "Response is in file '%s'", log_file);
    
}

void cache_debug_log_cache_req(connection *cptr, char *parametrized_url, int parametrized_url_len)
{
  VUser* vptr = cptr->vptr;

#ifdef NS_DEBUG_ON
  int request_type = cptr->url_num->request_type;
  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        ((request_type == HTTP_REQUEST || request_type == HTTPS_REQUEST )&&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP))))
    return;
#endif

  if(global_settings->replay_mode == REPLAY_USING_WEB_SERVICE_LOGS) return;

    NSDL3_HTTP(vptr, cptr, "Method called.");
    char log_file[4096] = "\0";
    FILE *log_fp;
    char line_break[] = "\n------------------------------------------------------------\n";

    //Need to check if buf is null since following error is coming when try to write null
    //Error: Can not write to url request file. err = Operation now in progress, bytes_to_log = 0, buf = (null)
    //also check if bytes_to_log is 0, it possible when buf = ""
    if(parametrized_url == NULL)return;  

    // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    /*sprintf(log_file, "%s/logs/TR%d/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                      g_ns_wdir, testidx, child_idx, vptr->user_index,
                      vptr->sess_inst, vptr->page_instance, vptr->group_num,
                      GET_SESS_ID_BY_NAME(vptr),
                      GET_PAGE_ID_BY_NAME(vptr));*/

    SAVE_REQ_REP_FILES
  
    sprintf(log_file, "%s/logs/%s/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                      g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index,
                      vptr->sess_inst, vptr->page_instance, vptr->group_num,
                      GET_SESS_ID_BY_NAME(vptr),
                      GET_PAGE_ID_BY_NAME(vptr));

    NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

    log_fp = fopen(log_file, "a+");
    if (log_fp == NULL)
    {
      fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
      return;
    }

    //write for both ssl and non ssl url
    fprintf(log_fp, "This Request is served from Cache. URL = %*.*s\n", parametrized_url_len, parametrized_url_len, parametrized_url);

    fprintf(log_fp, "%s", line_break);

    if(fclose(log_fp) != 0)
    {
      fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
    }
    if(g_parent_idx != -1)
      sprintf(log_file, "%s/logs/TR%s/%lld/ns_logs/req_rep/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                      getenv("NS_CONTROLLER_WDIR"), getenv("NS_CONTROLLER_TEST_RUN"),
                      vptr->partition_idx, child_idx, vptr->user_index,
                      vptr->sess_inst, vptr->page_instance, vptr->group_num,
                      GET_SESS_ID_BY_NAME(vptr),
                      GET_PAGE_ID_BY_NAME(vptr));
    NS_DT4(vptr, cptr, DM_L1, MM_HTTP, "Request is in file '%s'", log_file);

}

#define CAVTEST_REQ_REP_UPLOAD(in_buf, buffer, buff_size, amt_written, bytes_to_log, complete_data, file_type) { \
  char cm_line_break[] = "\n------------------------------------------------------------\n"; \
  NSDL3_HTTP(NULL, NULL, "buff_size = %d, amt_written = %d, bytes_to_log = %d, complete_data = %d, file_type = %d", buff_size, amt_written, bytes_to_log, complete_data, file_type); \
  if(amt_written + bytes_to_log >= buff_size) \
  { \
    MY_REALLOC(buffer, buff_size + bytes_to_log + MAX_DATA_LENGTH + sizeof(cm_line_break), "allocating cm_nvm_scratch_buf", -1); \
    buff_size = buff_size + bytes_to_log + MAX_DATA_LENGTH; \
  } \
  if(bytes_to_log) \
  { \
    snprintf(buffer + amt_written, bytes_to_log, "%s", in_buf); \
    amt_written += bytes_to_log; \
  } \
  if (complete_data) \
  { \
    if(file_type == 0) \
    { \
      amt_written += snprintf(buffer + amt_written, sizeof(cm_line_break), "%s", cm_line_break); \
    } \
    char cavtest_file_name[512]; \
    char file_type_str[][8] = {"req", "rep"}; \
    snprintf(cavtest_file_name, 512, "TR%s/%lld/ns_logs/req_rep/%s/url_%s_%hd_%u_%u_%d_0_%d_%d_%d_0_%lld.txt", \
             g_controller_testrun, global_settings->cavtest_partition_idx, vptr->sess_ptr->sess_name, \
	     file_type_str[file_type], child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance, \
	     vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr), \
             g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at); \
    ns_file_upload(cavtest_file_name, buffer, amt_written); \
    buffer[0] = 0; \
    amt_written = 0; \
  } \
}

/* Purpose: This method is to write debug url request file
   Arguments:
     cptr         - connection pointer
     buf          - data(ssl/non ssl)
     bytes_to_log - how many bytes to write
*/
void log_http_req(connection *cptr, VUser *vptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag)
{
  NSDL3_HTTP(vptr, cptr, "Method called. bytes_size = %d", bytes_to_log);
  char line_break[] = "\n------------------------------------------------------------\n";
  #ifndef CAV_MAIN
  char log_file[4096] = "\0";
  FILE *log_fp;

  //Need to check if buf is null since following error is coming when try to write null
  //Error: Can not write to url request file. err = Operation now in progress, bytes_to_log = 0, buf = (null)
  //also check if bytes_to_log is 0, it possible when buf = ""
  if((buf == NULL) || ((bytes_to_log == 0) && (!complete_data))) return;

  // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  /*sprintf(log_file, "%s/logs/TR%d/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
                    g_ns_wdir, testidx, child_idx, vptr->user_index,
                    vptr->sess_inst, vptr->page_instance, vptr->group_num, 
                    GET_SESS_ID_BY_NAME(vptr),
                    GET_PAGE_ID_BY_NAME(vptr));*/
  /* In release 3.9.7, create directory in TR or partition directory(NDE-continues monitoring) for request and response files
   * path:  logs/TRxx/ns_logs/req_rep/
   * or
   * logs/TRxx/<partition>/ns_logs/req_rep/
   * */
  SAVE_REQ_REP_FILES

  sprintf(log_file, "%s/logs/%s/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
                    g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index,
                    vptr->sess_inst, vptr->page_instance, vptr->group_num, 
                    GET_SESS_ID_BY_NAME(vptr),
                    GET_PAGE_ID_BY_NAME(vptr));
  NSDL3_HTTP(NULL, NULL, "log_file = %s, first_trace_write_flag = %d, GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", 
                          log_file, first_trace_write_flag, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));


  log_fp = fopen(log_file, "a+");
  if (log_fp == NULL)
  {
    fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
    return;
  }

  //write for both ssl and non ssl url
  if((bytes_to_log > 0) && (fwrite(buf, bytes_to_log, 1, log_fp) != 1))
  {
    fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf);
    //Resetting Complete Data Flag as failed to write the request/response buffer to file
    complete_data = 0;
  }

  if (complete_data)
  {
    NSDL3_HTTP(NULL, NULL, "Printing line break, complete_data = %d, bytes_to_log = %d", complete_data, bytes_to_log);
    fwrite(line_break, strlen(line_break), 1, log_fp);
  }

  if(fclose(log_fp) != 0)
  {
    fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
  }

  if(g_parent_idx != -1) //netcloud
    sprintf(log_file, "%s/logs/TR%s/%lld/ns_logs/req_rep/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
                    getenv("NS_CONTROLLER_WDIR"), getenv("NS_CONTROLLER_TEST_RUN"),
                    vptr->partition_idx, child_idx, vptr->user_index,
                    vptr->sess_inst, vptr->page_instance, vptr->group_num, 
                    GET_SESS_ID_BY_NAME(vptr),
                    GET_PAGE_ID_BY_NAME(vptr));

  // Do not change the debug trace message as it is parsed by GUI
  if(first_trace_write_flag)
    NS_DT4(vptr, cptr, DM_L1, MM_HTTP, "Request is in file '%s'", log_file);

  if(global_settings->monitor_type == HTTP_API)
  {
    static char *cm_nvm_scratch_buf = NULL;
    static int cm_nvm_scratch_buf_size = 0;
    static int amt_written = 0;

    CAVTEST_REQ_REP_UPLOAD(buf, cm_nvm_scratch_buf, cm_nvm_scratch_buf_size, amt_written, bytes_to_log, complete_data, 0);
  }
  #else
  CAVTEST_REQ_REP_UPLOAD(buf, vptr->sm_mon_info->cm_nvm_scratch_buf, vptr->sm_mon_info->cm_nvm_scratch_buf_size, vptr->sm_mon_info->amt_written, bytes_to_log, complete_data, 0);
  #endif
}

void debug_log_http_req(connection *cptr, char *buf, int bytes_to_log, 
      int complete_data, int first_trace_write_flag)
{

  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;
  
  if (!(((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
       (request_type == HTTP_REQUEST || request_type == HTTPS_REQUEST  || request_type == WS_REQUEST || request_type == WSS_REQUEST || request_type == SOCKET_REQUEST || request_type == SSL_SOCKET_REQUEST)&&
       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP))||
       ((NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)))))
   { 
        
     NSDL3_HTTP(vptr, cptr, "Either debug is disable or page dump feature is disable.hence returning");
     return;
   }

  if(global_settings->replay_mode == REPLAY_USING_WEB_SERVICE_LOGS) 
    return;

  if(cptr->http_protocol != HTTP_MODE_HTTP2)
    log_http_req(cptr, vptr, buf, bytes_to_log, complete_data, first_trace_write_flag);
}

void log_http_res(connection *cptr, VUser *vptr, char *buf, int size)
{
  #ifndef CAV_MAIN
  char log_file[1024];
  int log_fd;

  // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  //sprintf(log_file, "%s/logs/TR%d/url_rep_%d_%ld_%ld_%d_0.dat", g_ns_wdir, testidx, my_port_index, vptr->user_index, cur_vptr->sess_inst, vptr->page_instance);
  /* In release 3.9.7, create directory in TR or partition directory(NDE-continues monitoring) for request and response files
   * path:  logs/TRxx/ns_logs/req_rep/
   * or
   * logs/TRxx/<partition>/ns_logs/req_rep/
   * */
  SAVE_REQ_REP_FILES

  sprintf(log_file, "%s/logs/%s/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
   
  NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  
  if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    {
      write(log_fd, cptr->url, cptr->url_len);
      write(log_fd, "\n", 1);
    }
    write(log_fd, buf, size);
    if(runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode == HTTP_MODE_HTTP2)
      write(log_fd, "\n", 1);
    close(log_fd);
  }

  if(g_parent_idx != -1)
    sprintf(log_file, "%s/logs/TR%s/%lld/ns_logs/req_rep/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
           getenv("NS_CONTROLLER_WDIR"), getenv("NS_CONTROLLER_TEST_RUN"),
           vptr->partition_idx, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
           vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
           GET_PAGE_ID_BY_NAME(vptr));
  // Do not change the debug trace message as it is parsed by GUI
  if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    NS_DT4(vptr, cptr, DM_L1, MM_HTTP, "Response is in file '%s'", log_file);

  if(global_settings->monitor_type == HTTP_API)
  {
    cavtest_log_rep(vptr, buf, size, 0);
  } 
  #else
  cavtest_log_rep(vptr, buf, size, 0);
  #endif
}

void cavtest_log_rep(VUser *vptr, char *buf, int bytes_to_log, int complete_data)
{
  #ifndef CAV_MAIN
  static char *cm_nvm_scratch_buf = NULL;
  static int cm_nvm_scratch_buf_size = 0;
  static int amt_written = 0;
  
  CAVTEST_REQ_REP_UPLOAD(buf, cm_nvm_scratch_buf, cm_nvm_scratch_buf_size, amt_written, bytes_to_log, complete_data, 1);

  #else
  CAVTEST_REQ_REP_UPLOAD(buf, vptr->sm_mon_info->cm_nvm_scratch_buf, vptr->sm_mon_info->cm_nvm_scratch_buf_size, vptr->sm_mon_info->amt_written, bytes_to_log, complete_data, 1);
  #endif
}

// This method is called every time some bytes of respsone is read
// This will log headers and body (if any)
inline void debug_log_http_res(connection *cptr, char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type; // aded variable request_type to check if it is a HTTP/HTTPS request or not

  NSDL1_HTTP(NULL, NULL, "Method called, size = %d", size);

   if (((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4)  && 
       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP) && 
       ((request_type == HTTP_REQUEST || request_type == HTTPS_REQUEST || request_type == WS_REQUEST || request_type == WSS_REQUEST || request_type == SOCKET_REQUEST || request_type == SSL_SOCKET_REQUEST))) || 
        (NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL))) 
       /*(runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP)) || (NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS)&& (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)))*/
     log_http_res(cptr, vptr, buf, size);
}

// This will log only BODY as Headers are currently not saved in linked list
void log_http_res_body(VUser* vptr, char *buf, int size)
{
  char log_file[1024];
  int log_fd;

  // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  /*sprintf(log_file, "%s/logs/TR%d/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
            g_ns_wdir, testidx, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));*/
  /* In release 3.9.7, create directory in TR or partition directory(NDE-continues monitoring) for request and response files
   * path:  logs/TRxx/ns_logs/req_rep/
   * or
   * logs/TRxx/<partition>/ns_logs/req_rep/
   * */
  SAVE_REQ_REP_FILES

  sprintf(log_file, "%s/logs/%s/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
            g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));

  NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

  if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    if(g_parent_idx != -1) //netcloud case filename only changes
      sprintf(log_file, "%s/logs/TR%s/%lld/ns_logs/req_rep/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
              getenv("NS_CONTROLLER_WDIR"), getenv("NS_CONTROLLER_TEST_RUN"), vptr->partition_idx,
              child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
              vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
              GET_PAGE_ID_BY_NAME(vptr));
    // Do not change the debug trace message as it is parsed by GUI
    NS_DT4(vptr, NULL, DM_L1, MM_HTTP, "Response body is in file '%s'", log_file);
    write(log_fd, buf, size);
    close(log_fd);
  }

  if(global_settings->monitor_type == HTTP_API)
  {
    char cavtest_res_body_file_name[512];
    snprintf(cavtest_res_body_file_name, 512, "TR%s/%lld/ns_logs/req_rep/%s/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0_%lld.txt",
                                               g_controller_testrun, global_settings->cavtest_partition_idx, vptr->sess_ptr->sess_name,
                                               child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance, vptr->group_num,
                                               GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr),
                                               g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at);
    ns_file_upload(cavtest_res_body_file_name, buf, size);
  }
}

void debug_log_http_res_body(VUser* vptr, char *buf, int size)
{
   if (((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4)  &&
       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP)) || (NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)))
  {   
    log_http_res_body(vptr, buf, size);
  }
}

inline void debug_log_http_res_line_break(connection *cptr)
{
  NSDL1_HTTP(NULL, NULL, "Method called");
  //VUser *vptr = cptr->vptr;
  char line_break[] = "\n------------------------------------------------------------\n";
  debug_log_http_res(cptr, line_break, strlen(line_break));
}

char* create_url_name (connection *cptr, char *url_name)
{
  char *url_name_start_ptr = NULL, *url_name_end_ptr = NULL;
  int url_len = 0;

  NSDL1_HTTP(NULL, NULL, "Method called, cptr->url = [%s]", cptr->url);
  url_name_start_ptr = rindex(cptr->url, '/'); 

  if (url_name_start_ptr != NULL)
  {
    //Skip sign '/'
    url_name_start_ptr++;
    url_name_end_ptr = index(url_name_start_ptr, '?'); 
      
    if (url_name_end_ptr != NULL)
    {
      //Skip ?
      //url_name_end_ptr--;
      url_len = url_name_end_ptr - url_name_start_ptr;
      NSDL2_HTTP(NULL, NULL, "url_len = %d", url_len);
      strncpy(url_name, url_name_start_ptr, url_len);
      url_name[url_len] = 0;
    } else
      strcpy(url_name, url_name_start_ptr);
  }
  else
  strcpy(url_name, cptr->url);
  NSDL2_HTTP(NULL, NULL, "url_name = [%s]", url_name);
  return(url_name);
}
//url_rep_body_<url_base_name>_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_0_<url_inst>_<sess_id>_	<page_id>_<url_id>
//       url                                           --->                url_base_name
//     --------                                                           -----------------
//  1) http://www.google.com/myindex                   --->                 myindex
//  2) http://www.google.com/myData.tar.gz             --->                 myData.tar.gz
//  3) http://www.google.com/index.bz2?name=manish..   --->                 myData.tar.gz
//  4) http://www.google.com/dataDir                   --->                 index
//      
/*-------------------------------------------------------------------
 * Purpose : This function will save the http response body into file according to required condition 
 *         
 * Input   : buf  - contains response body read till. This response have to be dumped into file  
 *           size - size of response body stored in buf
 *           total_size - Total size of the till read body
 * 
 * Output  : 0  - On success
 *           -1 - On failure 
 *-----------------------------------------------------------------*/
int save_http_resp_body(connection *cptr, char *buf, int size, int total_size)
{
  char url_resp_body_file[MAX_RESP_BODY_FILE_LEN + 1];
  char url_name[MAX_RESP_BODY_FILE_LEN + 1];
  //char url_req_body_file[MAX_RESP_BODY_FILE_LEN + 1];
  //int url_resp_body_fd, url_name_len;
  
  //UserTraceNode *pd_node = NULL; 
  //userTraceData* utd_node = NULL;

  VUser *vptr = (VUser *)cptr->vptr; 

  url_resp_body_file[0] = 0, url_name[0] = 0;
  //url_resp_body_fd = -1, url_name_len = 0;

  NSDL1_HTTP(NULL, NULL, "Method Called, cptr = %p, buf = [%*.*s], size = %d, total_size = %d",
                          cptr, size, size, buf, size, total_size);

  //Fetching url name from cptr->url
  //if(total_size == 0 && cptr->url != NULL)
  //{
    /* In release 3.9.7, create directory in TR or partition directory(NDE-continues monitoring) for request and response files
     * path:  logs/TRxx/ns_logs/req_rep/
     * or
     * logs/TRxx/<partition>/ns_logs/req_rep/
     * */
    SAVE_REQ_REP_FILES
    create_url_name(cptr, url_name); 
    if(cptr->flags & NS_CPTR_CONTENT_TYPE_MEDIA)
    { 
      sprintf(url_resp_body_file, "%s/logs/%s/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0_%d_%s", 
                       g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
                       vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
                       GET_PAGE_ID_BY_NAME(vptr), cptr->url_num->proto.http.url_got_bytes, url_name); 

    //cptr->url_num->proto.http.url_got_bytes = 0; //Just Resetting the variable
    }
    else 
    {
      sprintf(url_resp_body_file, "%s/logs/%s/url_rep_body_%s_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
                       g_ns_wdir, req_rep_file_path, url_name, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
                       vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
                       GET_PAGE_ID_BY_NAME(vptr)); 
    }
  //}

  NSDL3_HTTP(NULL, NULL, "Dumping Response Body into file '%s'", url_resp_body_file);
  NSDL3_HTTP(NULL, NULL, "Open file to dump response body...");
 
  OPEN_AND_DUMP(url_resp_body_file, buf, size);
 
  #if 0
  //Opening Response file 
  if((url_resp_body_fd = open(url_resp_body_file, O_CREAT|O_WRONLY|O_APPEND, 00666)) < 0) 
  {
    NSDL3_HTTP(NULL, NULL, "Error: error in opening file %s", url_resp_body_file);
    fprintf(stderr, "Error: error in opening file %s\n", url_resp_body_file); 
    return -1;
  }

  //write buf into file
  NSDL2_HTTP(NULL, NULL, "Writing data in file %s for fd %d, size = %d", url_resp_body_file, url_resp_body_fd, size);
  write(url_resp_body_fd, buf, size);
  close(url_resp_body_fd);
  #endif

  #if 0
  //Make Request file only once for each request and and dump the request
  if(total_size == 0)
  {
    GET_UTD_NODE
    pd_node = (UserTraceNode*)utd_node->ut_head;

    NSDL1_HTTP(NULL, NULL, "utd_node = %p, pd_node = %p, Req len = %d, Req = [%s]", 
                            utd_node, pd_node, pd_node->page_info->req_size, pd_node->page_info->req);
    sprintf(url_req_body_file, "%s/logs/TR%d/ns_logs/http_req_rep_body/url_req_body_%s_%d_%u_%u_%d_0_%d_%d_%d_0.dat", 
                     g_ns_wdir, testidx, url_name, my_port_index, vptr->user_index, vptr->sess_inst, vptr->page_instance,
                     vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
                     GET_PAGE_ID_BY_NAME(vptr)); 

    NSDL3_HTTP(NULL, NULL, "Dumping Request Body into file '%s'", url_req_body_file);
    OPEN_AND_DUMP(url_req_body_file, pd_node->page_info->req, pd_node->page_info->req_size);
  }   
  #endif
 
  return 0;
}
