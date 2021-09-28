/*******************************************************
 * File: ns_click_script.c
 *
 *       Contains methods for executing click and script
 *       API's at run time
 ********************************************************/
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

#include "ns_vuser_ctx.h"
#include "ns_vuser_tasks.h"
#include "ns_data_types.h"
#include "ns_js.h"
#include "ns_js_events.h"
#include "ns_http_cache.h"
#include "ns_server.h"
#include "ns_url_hash.h"
#include "ns_click_script_parse.h"
#include "ns_click_script.h"


/* mystrcmp()- Assumptions:
 * 1. s2 has no leading or trailing white spaces, however s1 may have
 * 2. size is strlen of s2
 *
 * Aim is to be as performant as possible */
static inline int mystrcmp(char *s1, char *s2, int size)
{
  int ret, len_of_s1;
  char *tmp;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method called, s1 = '%s', s2 = '%s', size = %d", s1, s2, size);

  len_of_s1 = strlen(s1);
  if(s1 && len_of_s1<size)
  {
    NSDL2_JAVA_SCRIPT(NULL, NULL, "size of s1 is smaller than size of s2, strings don't match");
    return 1;
  }

  tmp = s1;
  while(*tmp == ' ' || *tmp == '\t') {tmp++; len_of_s1--;}
  if(len_of_s1<size)
  {
    NSDL2_JAVA_SCRIPT(NULL, NULL, "size of s1 (after initial spaces) is smaller than size of s2, strings don't match");
    return 1;
  }

  ret = strncmp(tmp, s2, size);

  if(ret != 0)
    return ret;

  if (ret == 0)
  {
    NSDL2_JAVA_SCRIPT(NULL, NULL, "Initial %d characters matched '%s'", size, s2);

    if(tmp[size] == '\0'){

      NSDL2_JAVA_SCRIPT(NULL, NULL, "Full strings matched, returning 0");
      return ret;
    }

    tmp += size;
    while(*tmp != '\0')
    {
      if(*tmp != ' ' && *tmp != '\t'){

        NSDL2_JAVA_SCRIPT(NULL, NULL, "Strings don't match, returning 1");
        return 1;
      }

      tmp++;
    }
  }
  
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Full strings matched, returning 0");
  return ret; 
}

static int extract_request_type_from_url(char *url)
{
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, url = '%s'", url?url:"null");
  int request_type = -1;

  if (!url || url[0] == '\0'){
    NSDL2_JAVA_SCRIPT(NULL, NULL, "blank url, returning request_type=%d", request_type);
    return request_type;
  }

  if (!strncmp(url, "http:", 5)){
    request_type = HTTP_REQUEST;
  }

  else if (!strncmp(url, "https:", 6)){
    request_type =  HTTPS_REQUEST;
  }

  NSDL2_JAVA_SCRIPT(NULL, NULL, "returning request_type=%d", request_type);
  return request_type;
}

/* url must be absolute */
static inline void extract_hostname_from_absolute_url(char *url, char *hostname)
{
  char *hostname_end=NULL;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, url = '%s'", url?url:"null");

  if(!hostname)
  {
    NSDL1_JAVA_SCRIPT(NULL, NULL, "ERROR: hostname is null, no buffer supplied for hostname, returning");
    return;
  }
  
  hostname[0] = '\0';

  if (!url || url[0] == '\0')
  {
    NSDL1_JAVA_SCRIPT(NULL, NULL, "ERROR: url is blank, returning hostname='%s'", hostname?hostname:"null");
    return;
  }

  hostname_end = strchr(url, '/');
  if(hostname_end){
    strncpy(hostname, url, hostname_end - url);
    hostname[hostname_end - url] = '\0';
  }
  else
    strcpy(hostname, url);

  NSDL4_JAVA_SCRIPT(NULL, NULL, "returning hostname='%s'", hostname?hostname:"null");
}

/* hostname must be a valid string */
static int extract_port_number_from_hostname(char *hostname)
{
  char *port_str = NULL; 
  int port = -1;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, hostname = '%s'", hostname?hostname:"null");

  if (!hostname || hostname[0] == '\0'){
    NSDL2_JAVA_SCRIPT(NULL, NULL, "ERROR: hostname is blank, returning port=%d", port);
    return port;
  }

  port_str = strchr(hostname, ':');

  if (port_str)
    port = atoi(port_str + 1);

  NSDL2_JAVA_SCRIPT(NULL, NULL, "returning port=%d", port);
  return port;
}


static inline void create_url_request_line_from_relative(char *url, char *in_url, char *page_main_url)
{
  int parent_request_type = -1;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method called, in_url='%s', page_main_url='%s'", in_url, page_main_url);

  /* First Handle error cases, which are unlikely, but for saftey net */
  if(!page_main_url) /* Error - Should never be the case */
  {
    strcpy(url, "/");
    NSDL4_JAVA_SCRIPT(NULL, NULL, "returning url='%s'", url);
    return;
  }

  parent_request_type =  extract_request_type_from_url(page_main_url);

  if(!in_url || in_url[0] == '\0') /* Error - Should never be the case */
  {
    strcpy(url, (page_main_url + (parent_request_type==HTTP_REQUEST?7:8)));
    NSDL4_JAVA_SCRIPT(NULL, NULL, "returning url='%s'", url);
    return;
  }

  /* make_absolute_from_relative*/
  make_absolute_from_relative_url(in_url, page_main_url + (parent_request_type==HTTP_REQUEST?6:7), url);

  char *request_line_start=url;
  NSDL4_JAVA_SCRIPT(NULL, NULL, "request_line_start='%s'", request_line_start?request_line_start:"NULL");
  if(*request_line_start == '/') request_line_start++;
  NSDL4_JAVA_SCRIPT(NULL, NULL, "request_line_start='%s'", request_line_start?request_line_start:"NULL");
  request_line_start = strchr(request_line_start, '/');
  NSDL4_JAVA_SCRIPT(NULL, NULL, "request_line_start='%s'", request_line_start?request_line_start:"NULL");
  if(request_line_start)
    strcpy(url, request_line_start);

  else
    strcpy(url, "/");

  NSDL4_JAVA_SCRIPT(NULL, NULL, "returning url='%s'", url);
}



/******************************************************************************************
 * make_url_request_line_and_save_server_data_on_vptr()
 *
 * Synopsis            :   This function does the following tasks
 *                         1. takes the in_url which may be relative, converts to absolute 
 *                            url request line (without leading /) and fills in the url and 
 *                            sets its len in url_len
 *                         2. saves the server date (server_hostname, server_port and request_type)
 *                            in vptr->httpData.
 * Arguments:
 *   vptr                  VUser *  - Input argument
 *   in_url                char *   - Input argument. This is the url string that is extracted from the 
 *                                    DOM. this may be relative url. need to be converted
 *                                    to absolute url.
 *   url                   char *   - This is output argument and contains the absolute 
 *                                    url for the next main page to be hit.
 *   url_len               int *    - Output argument.
 ******************************************************************************************/
static inline void make_url_request_line_and_save_server_data_on_vptr(VUser *vptr, char *in_url, char *url, unsigned short *url_len) 
{
  char hostname[MAX_LINE_LENGTH + 1] = "0";
  int hostname_len = 0;
  int port = -1, request_type = -1;
  char *page_main_url = NULL;
  char *url_end_ptr = NULL, *abs_url=NULL;

  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, in_url='%s'", in_url);
  
  if(!in_url || in_url[0] == '\0') /* Error - Should never be the case */
  {
    strcpy(url, "/");
    NSDL4_JAVA_SCRIPT(vptr, NULL, "in_url is blank, setting url='%s'", url);
    return; /*safety net */
  }

  page_main_url = vptr->httpData->page_main_url; /* Current Main page url, parent for next page */
  NSDL4_JAVA_SCRIPT(vptr, NULL, "vptr->httpData->page_main_url='%s'", 
                                page_main_url?page_main_url:"null"); 

  if(!page_main_url || page_main_url[0] == '\0')/* Error - Should never be the case */
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "ERROR: main_page_url_is null");
    return; /*safety net */
  }

  /* CASE 1: Fully Qualified URL */
  if(!strncmp(in_url, "http://", 7) || !strncmp(in_url, "https://", 8) || !strncmp(in_url, "//", 2))
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "Fully qualified in_url='%s'", in_url);

    request_type =  extract_request_type_from_url(in_url);

    if(request_type == HTTP_REQUEST)
      abs_url = in_url + 7;
      //strcpy(url, in_url + 7);
    else if(request_type == HTTPS_REQUEST)
      abs_url = in_url + 8;
      //strcpy(url, in_url + 8);
    else /* // */
      abs_url = in_url + 2;
      //strcpy(url, in_url + 2);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "abs_url='%s'", abs_url);

    extract_hostname_from_absolute_url(abs_url, hostname);
    port = extract_port_number_from_hostname(hostname);

    strcpy(url, abs_url + strlen(hostname));
    NSDL4_JAVA_SCRIPT(vptr, NULL, "url='%s'", url);

    if(url[0] == '\0')
      strcpy(url, "/");

    NSDL4_JAVA_SCRIPT(vptr, NULL, "url='%s'", url);

    if (request_type == -1)
      request_type = extract_request_type_from_url(page_main_url);
  }

  /* CASE 2: Absolute URL */
  else if(in_url[0] == '/')
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "Absolute in_url='%s'", in_url);

    request_type = extract_request_type_from_url(page_main_url);
    extract_hostname_from_absolute_url((page_main_url + (request_type==HTTP_REQUEST?7:8)), hostname);
    port = extract_port_number_from_hostname(hostname);

    strcpy(url, in_url);
    NSDL4_JAVA_SCRIPT(vptr, NULL, "url='%s'", url);
  }

  /* CASE 3: relative url */
  else
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "Relative in_url='%s'", in_url);

    request_type = extract_request_type_from_url(page_main_url);
    extract_hostname_from_absolute_url(page_main_url + (request_type==HTTP_REQUEST?7:8), hostname);
    port = extract_port_number_from_hostname(hostname);

    create_url_request_line_from_relative(url, in_url, page_main_url);
    NSDL4_JAVA_SCRIPT(vptr, NULL, "url='%s'", url);
  }

  NSDL4_JAVA_SCRIPT(vptr, NULL, "truncating url if there is '#' character, url='%s'", url);
  url_end_ptr = strchr(url, '#');
  if(url_end_ptr) *url_end_ptr = '\0';
  *url_len = strlen(url);
  NSDL4_JAVA_SCRIPT(vptr, NULL, "truncated url='%s', url_len=%d", url, *url_len);


  /* At this point we must have valid url request line, request type and hostname; port still may be -1 */
  NSDL4_JAVA_SCRIPT(vptr, NULL, "url request line='%s', hostname='%s', request_type=%d, port=%d", 
                                url, hostname, request_type, port);

  if(port == -1)
  {
    switch (request_type)
    {
      case HTTPS_REQUEST:
        port = 443;
        NSDL4_JAVA_SCRIPT(vptr, NULL, "extracting  port from request_type; port=%d", port);
        break;
      case HTTP_REQUEST:
        port = 80;
        NSDL4_JAVA_SCRIPT(vptr, NULL, "extracting  port from request_type; port=%d", port);
        break;
      default:
        /* Should never come here */
        NSDL4_JAVA_SCRIPT(vptr, NULL, "Could not find port number; port=%d", port);
        break;
    }
  }

  /* Now copy the data on vptr->httpData */    
  hostname_len = strlen(hostname);

  if(hostname_len > vptr->httpData->server_hostname_len)
  {
    MY_REALLOC_EX(vptr->httpData->server_hostname, hostname_len + 1, 
                  vptr->httpData->server_hostname_len + 1, 
                  "reallocate vptr->httpData->server_hostname", 
                  -1);
  }

  vptr->httpData->server_hostname_len = hostname_len;

  strcpy(vptr->httpData->server_hostname, hostname);
  vptr->httpData->server_port = port;
  vptr->httpData->request_type = request_type;

}

inline void extract_and_set_url_params(connection *cptr, char *url, unsigned short *p_url_len)
{

  /*******************************************************************************/
  /* Url may be in following format:                                             */
  /*                                                                             */
  /* http://www.example.com/?username=ram&password=fishwhale&userSession=75893cD */
  /* &JSFormSubmit=off                                                           */
  /*                                                                             */
  /* offset to query param (?) and method type  is saved on vptr->httpData       */
  /*******************************************************************************/

  VUser *vptr = (VUser *)cptr->vptr;
  action_request_Shr* request = cptr->url_num;

  NSDL2_JAVA_SCRIPT(vptr, cptr, "Method called; url = '%s', url_len = %d\n", url, *p_url_len);

  if (vptr->httpData->http_method != -1){
    request->proto.http.http_method = vptr->httpData->http_method;
    request->proto.http.http_method_idx = vptr->httpData->http_method;
  }

  if(request->proto.http.http_method == HTTP_METHOD_POST)
    *p_url_len = strlen(url);
}



inline void set_server(connection *cptr)
{
  VUser *vptr = cptr->vptr;
  unsigned short rec_server_port;
  NSDL2_JAVA_SCRIPT(vptr, cptr, "Method Called");
  
  int hostname_len = find_host_name_length_without_port(vptr->httpData->server_hostname, &rec_server_port);
  int gserver_shr_idx = find_gserver_shr_idx(vptr->httpData->server_hostname, vptr->httpData->server_port, hostname_len);
  unsigned char request_type = vptr->httpData->request_type;

  if (gserver_shr_idx == -1) {

    NSDL4_JAVA_SCRIPT(vptr, cptr, "Error: Recorded host (%s %s) not found."
         " Add \"ADD_RECORDED_HOST %s %s\""
         " in scenario to resolve this issue. Exiting...",
         vptr->httpData->server_hostname,
         request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP",
         vptr->httpData->server_hostname,
         request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP");


    print_core_events((char*)__FUNCTION__, __FILE__,
         "Error: Recorded host (%s %s) not found."
         " Add \"ADD_RECORDED_HOST %s %s\""
         " in scenario to resolve this issue. Exiting...",
         vptr->httpData->server_hostname, 
         request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP",
         vptr->httpData->server_hostname, 
         request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP");

    END_TEST_RUN
  }

/********/
  cptr->url_num->index.svr_ptr = &gserver_table_shr_mem[gserver_shr_idx];
  vptr->ustable[cptr->url_num->index.svr_ptr->idx].svr_ptr = NULL;
/********/


  NSDL3_JAVA_SCRIPT(NULL, cptr, "gserver_shr_idx = %d, "
           "cptr->url_num->index.svr_ptr->server_hostname = '%s',  "
           "cptr->url_num->index.svr_ptr->server_port = %d\n", 
           gserver_shr_idx, 
           cptr->url_num->index.svr_ptr->server_hostname, 
           cptr->url_num->index.svr_ptr->server_port); 

}


/* This function is for filling segmented data.
 * If our element is parameterized then this function will return element
 * with parameterization*/
int get_full_element(VUser *vptr, const StrEnt_Shr* seg_tab_ptr, char *elm_buf, int *elm_size)
{
  int elm_lol_size = 0;
  char val_value_flag = 1;
  int i, ret;

  NS_RESET_IOVEC(g_scratch_io_vector); 

  ret = insert_segments(vptr, NULL, seg_tab_ptr, &g_scratch_io_vector, 
                                NULL, 0, val_value_flag, REQ_PART_REQ_LINE, NULL, SEG_IS_NOT_REPEAT_BLOCK);

  if(ret == MR_USE_ONCE_ABORT)
    return MR_USE_ONCE_ABORT;

  /* Combine all vectors in one big buf and malloc */
  elm_buf[0] = elm_buf[MAX_LINE_LENGTH] = '\0';
  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) 
  {
    if (NS_GET_IOVEC_LEN(g_scratch_io_vector,i) != 0) 
    {
      /* abort filling it since it will go out of bounds. The resultant 
       * URL will be truncated. */
      if ((elm_lol_size + NS_GET_IOVEC_LEN(g_scratch_io_vector,i)) > MAX_LINE_LENGTH) 
      {
        memcpy(elm_buf + elm_lol_size, NS_GET_IOVEC_VAL(g_scratch_io_vector,i), 
               MAX_LINE_LENGTH - elm_lol_size); 
        elm_lol_size = MAX_LINE_LENGTH;
        NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, 
                       "Element is truncated due to bigger size( > %d), element[%s]",
      	      MAX_LINE_LENGTH, elm_buf);
        break;  
      }
      else 
      {
        memcpy(elm_buf + elm_lol_size, NS_GET_IOVEC_VAL(g_scratch_io_vector,i), NS_GET_IOVEC_LEN(g_scratch_io_vector,i));
        elm_lol_size += NS_GET_IOVEC_LEN(g_scratch_io_vector,i);
      }
    }
  }

  elm_buf[elm_lol_size] = '\0';
  *elm_size = elm_lol_size;
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
 
  return 0;
}


void copy_click_actions_table_to_shr(void)
{
  int i,j;

  NSDL2_PARSING(NULL, NULL, "Method called. total_clickaction_entries = %d", total_clickaction_entries);

  if (!total_clickaction_entries)
  {
    return;
  }

  clickaction_table_shr_mem = (ClickActionTableEntry_Shr*) do_shmget(sizeof(ClickActionTableEntry_Shr) * total_clickaction_entries, "click action table");

  for(i=0; i<total_clickaction_entries; i++){

    for(j=0; j<NUM_ATTRIBUTE_TYPES; j++)
    {
      if (clickActionTable[i].att[j].seg_start == -1) {
        clickaction_table_shr_mem[i].att[j].seg_start = NULL;
      } else {
        clickaction_table_shr_mem[i].att[j].seg_start = SEG_TABLE_MEMORY_CONVERSION(clickActionTable[i].att[j].seg_start);
      }

      clickaction_table_shr_mem[i].att[j].num_entries = clickActionTable[i].att[j].num_entries;

    }
  }
}

static inline int attr_flag2index(int flag)
{
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, flag = 0x%x", flag); 
  int index=-1;

  switch (flag)
  {
    case ATTR_TAG:
      index = TAG;
      break;
    case ATTR_ID:
      index = ID;
      break;
    case ATTR_NAME:
      index = NAME;
      break;
    case ATTR_TYPE:
      index = TYPE;
      break;
    case ATTR_VALUE:
      index = VALUE;
      break;
    case ATTR_CONTENT:
      index = CONTENT;
      break;
    case ATTR_ALT:
      index = ALT;
      break;
    case ATTR_SHAPE:
      index = SHAPE;
      break;
    case ATTR_COORDS:
      index = COORDS;
      break;
    case ATTR_TITLE:
      index = TITLE;
      break;
    default:
      index = -1;
      break;
  }

  NSDL2_JAVA_SCRIPT(NULL, NULL, "returning index='%d'", index); 
  return index;
}

static inline char *attr_flag2str(int flag, char *str)
{
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, flag = 0x%x", flag); 

  switch (flag)
  {
    case ATTR_TAG:
      strcpy(str, "tag");
      break;
    case ATTR_ID:
      strcpy(str, "id");
      break;
    case ATTR_NAME:
      strcpy(str, "name");
      break;
    case ATTR_TYPE:
      strcpy(str, "type");
      break;
    case ATTR_VALUE:
      strcpy(str, "value");
      break;
    case ATTR_CONTENT:
      strcpy(str, "content");
      break;
    case ATTR_ALT:
      strcpy(str, "alt");
      break;
    case ATTR_SHAPE:
      strcpy(str, "shape");
      break;
    case ATTR_COORDS:
      strcpy(str, "coords");
      break;
    case ATTR_TITLE:
      strcpy(str, "title");
      break;
    default:
      strcpy(str, "\0");
      break;
  }
  NSDL2_JAVA_SCRIPT(NULL, NULL, "returning str='%s'", str?str:"NULL"); 

  return str;
}


static inline char match_attributes_of_current_node(int attributes_to_be_matched, xmlNode *cur_node, char **att)
{
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called");
 
  char skip_cur_elem = 0;
  int i, flag;
  xmlChar *prop_in_dom = NULL;
  int free_prop = 0;
  char attr_buf[30] = "\0";
  int attr_index;

  char *attr_str = attr_buf;

  for(i=0, flag=0x0001; i<16; i++, flag = flag << 1)
  {
    if (flag & attributes_to_be_matched)
    {
      attr_index = attr_flag2index(flag);
      attr_str = attr_flag2str(flag, attr_buf);
      if(attr_index>=0)
      {
        NSDL4_JAVA_SCRIPT(NULL, NULL, "Going to match attribute '%s', "
                                         "cur_node->name = '%s'",
                                         attr_str?attr_str:"NULL",
                                         cur_node->name?(char *)cur_node->name:"NULL");

        if(!att[attr_index])
        {
          NSDL4_JAVA_SCRIPT(NULL, NULL, "att[%s] is null, skipping '%s' attribute", 
                                        attr_str?attr_str:"NULL",
                                        attr_str?attr_str:"NULL");
          continue;
        }
 
        switch (flag)
        {
          case ATTR_ID:     
          case ATTR_NAME: 
          case ATTR_TYPE:  
          case ATTR_VALUE:  
          case ATTR_ALT:
          case ATTR_SHAPE:
          case ATTR_COORDS:
          case ATTR_TITLE:
            prop_in_dom = xmlGetProp(cur_node, (xmlChar *) attr_str);
            free_prop = 1;
            break;

         case ATTR_CONTENT:
            prop_in_dom = xmlNodeGetContent(cur_node);
            free_prop = 1;
            break;

         case ATTR_TAG:
            prop_in_dom = (xmlChar *) cur_node->name; 
            free_prop = 0;
            break;

          default:
            prop_in_dom=NULL;
            free_prop=0;
            break;
        }

        if(!prop_in_dom)
        {
          NSDL4_JAVA_SCRIPT(NULL, NULL, "'%s' prop in DOM is null, skipping the element", attr_str);
          skip_cur_elem = 1;
          break;
        }

        NSDL4_JAVA_SCRIPT(NULL, NULL, "Trying to match '%s', "
                                         "att[%s] = '%s', "
                                         "prop_in_dom = '%s'",
                                         attr_str?attr_str:"NULL",
                                         attr_str?attr_str:"NULL",
                                         att[attr_index]?att[attr_index]:"NULL",
                                         prop_in_dom?(char *)prop_in_dom:"NULL");

        int cmp;
        
        if (flag == ATTR_TAG || flag == ATTR_TYPE)
          /* In this case the comparison should be case insensitive */
          cmp =  xmlStrcasecmp(prop_in_dom, (xmlChar *) att[attr_index]);
        else
          /* use my strcmp function as any leading or trailing spaces are terminated 
           * in recorder as well as script parser */
          cmp = mystrcmp((char *)prop_in_dom, (char *) att[attr_index], strlen(att[attr_index]));

        if(cmp != 0)
        {
          NSDL4_JAVA_SCRIPT(NULL, NULL, "'%s' not matched, skipping the element", attr_str);
          skip_cur_elem = 1;
          break;
        }
        else
        {
          NSDL4_JAVA_SCRIPT(NULL, NULL, "'%s' matched, cur_node = '%p'", attr_str, cur_node);
        }

        if (free_prop)
          FREE_XML_CHAR(prop_in_dom); 
      }
    }
  }
  return skip_cur_elem;
}

/********************************************************************************
 * search_clicked_element_in_dom()
 *
 *    Recursively searches with api's 'NAME' attribute in all the
 *    select tag elements in DOM tree
 *
 * Arguments:
 *
 *    att  
 *          array containing click api name and attributes of clicked element
 *
 *    first_time: 
 *          input flag set to 1 when called the method for first time. 
 *          All recursive calls make it 0
 *
 *    attributes_to_be_matched: 
 *          logical or'ed bit flags for the attributes to be matched in DOM
 *
 *    nodes_list: 
 *          An array of pointers to the container nodes in DOM for the element
 *          For example an image clicked could be a part of anchor or form.
 * ******************************************************************************/
int search_clicked_element_in_dom(char **att, int first_time, int attributes_to_be_matched, void **nodes_list)
{
  xmlNode *cur_node = NULL;

  int ret = -1;
  static int element_found = 0;
  static int cur_occurance = 1;
  static int ordinal = 0;

  char skip_cur_elem = 0;
  int i;
  void *lol_nodes_list[5];


  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method called, first_time = %d, "
                                "nodes_list[ROOT_NODE_INDEX] = '%p', "
                                "attributes_to_be_matched = '%0#4x', "
                                "container form node = '%p', " 
                                "container select node = '%p', " 
                                "container anchor node = '%p'", 
                                first_time, 
                                nodes_list[ROOT_NODE_INDEX],
                                attributes_to_be_matched,
                                nodes_list[FORM_NODE_INDEX],
                                nodes_list[SELECT_NODE_INDEX],
                                nodes_list[ANCHOR_NODE_INDEX]);
 

  if(first_time)
  {
    element_found=0;

    /* cur_occurance
     * will be incremented every time a match is found; 
     * will be compared with ordinal */
    cur_occurance = 1;

    /* ordinal 
     * will not change through out recursive calls */ 
    ordinal = 0;
    if(att[ORDINAL])
      ordinal = atoi(att[ORDINAL]);

    NSDL4_JAVA_SCRIPT(NULL, NULL, "ordinal = %d", ordinal); 
  }

  /* Create a local copy of all the nodes and pass down this local copy.
   * only when a match is found, update the passed pointers of container
   * form, anchor or select node. This way we shall get the container of
   * the clicked node, and if there is no container, we shall get NULL
   */
  for(i=0; i<5;i++)
   lol_nodes_list[i] = nodes_list[i];

  for (cur_node = (xmlNode *)lol_nodes_list[ROOT_NODE_INDEX]; cur_node && !(element_found); cur_node = cur_node->next) 
  {

    NSDL2_JAVA_SCRIPT(NULL, NULL, "Current node name = %s, address = %p", cur_node->name, cur_node);
    skip_cur_elem = 0;

    if (cur_node->type == XML_ELEMENT_NODE) {

      NSDL4_JAVA_SCRIPT(NULL, NULL, "inside XML_ELEMENT_NODE");

      /* Check if form, a or select node, save the node pointers */
      if (!xmlStrcasecmp((const xmlChar *)cur_node->name, (const xmlChar *) "form"))
      {
        NSDL4_JAVA_SCRIPT(NULL, NULL, "Node containing FORM tag, saving node pointer.");
        lol_nodes_list[FORM_NODE_INDEX] = (void *)cur_node;
      } 
      else if (!xmlStrcasecmp((const xmlChar *)cur_node->name, (const xmlChar *) "a"))
      {
        NSDL4_JAVA_SCRIPT(NULL, NULL, "Node containing ANCHOR tag, saving node pointer.");
        lol_nodes_list[ANCHOR_NODE_INDEX] = (void *)cur_node;
      }
      else if (!xmlStrcasecmp((const xmlChar *)cur_node->name, (const xmlChar *) "select"))
      {
        NSDL4_JAVA_SCRIPT(NULL, NULL, "Node containing SELECT tag, saving node pointer.");
        lol_nodes_list[SELECT_NODE_INDEX] = (void *)cur_node;
      }

      skip_cur_elem = match_attributes_of_current_node(attributes_to_be_matched, cur_node, att);

      if(!skip_cur_elem)
      {
        if(ordinal == 0 || cur_occurance >= ordinal)
        {
          NSDL4_JAVA_SCRIPT(NULL, NULL, "Search stopped as ordinality either not present or is met. "
                                        "ordinal = %d, current occurance = %d",
                                        ordinal, cur_occurance);
          nodes_list[CLICKED_NODE_INDEX] = cur_node;

          /* update the container nodes in the argument nodes_list[] so that 
           * it can be accessed after returned from this function
           */
          nodes_list[FORM_NODE_INDEX]   = lol_nodes_list[FORM_NODE_INDEX];
          nodes_list[ANCHOR_NODE_INDEX] = lol_nodes_list[ANCHOR_NODE_INDEX];
          nodes_list[SELECT_NODE_INDEX] = lol_nodes_list[SELECT_NODE_INDEX];
          element_found = 1;
          ret = 0;
          return ret;
        }
        else
        {
          NSDL4_JAVA_SCRIPT(NULL, NULL, "Continuing search as ordinality not yet met. "
                                        "ordinal = %d, current occurance = %d",
                                        ordinal, cur_occurance);
          cur_occurance++;
        }
      }
    }/* XML_ELEMENT_NODE */ 

    lol_nodes_list[ROOT_NODE_INDEX] = (void *)cur_node->xmlChildrenNode;
    ret = search_clicked_element_in_dom(att, 0, attributes_to_be_matched, lol_nodes_list);
    if(ret == 0){

      /* now copy the lol_nodes_list to nodes_list so that the calling
       * method gets the final container nodes pointers
       */
      for (i=0; i<5; i++)
        nodes_list[i] = lol_nodes_list[i];
      return ret;
    }
  } /* for loop */
  return ret;
}

int read_attributes_array_from_ca_table(VUser *vptr, char **att, ClickActionTableEntry_Shr *ca)
{
  char *tmp_ptr=NULL;
  int i, size;

 // tmp_buf[0]=0;
    g_tls.buffer[0] = 0;

  for (i=0; i<NUM_ATTRIBUTE_TYPES; i++)
  {
    tmp_ptr = NULL;

    if (ca->att[i].num_entries == 1 && ca->att[i].seg_start->type == STR)
    {
      size = ca->att[i].seg_start->seg_ptr.str_ptr->size;
      MY_MALLOC(tmp_ptr, size+1, "click api attribute",0);
      strncpy(tmp_ptr, ca->att[i].seg_start->seg_ptr.str_ptr->big_buf_pointer, size);
      tmp_ptr[size] = '\0';
    } 
    else if (ca->att[i].num_entries >= 1)
    {
      //Bug 24408 - RBU|Core is getting generated when we run the test with RBU_ENABLE_AUTO_PARAM keyword, having rbu parameters in script flow
      if((global_settings->protocol_enabled & RBU_API_USED) && (i == HarLogDir || i == BrowserUserProfile || i == VncDisplayId))
        return -1;
     /* if(get_full_element(vptr, &(ca->att[i]), tmp_buf, &size) < 0)
        return -1;*/
        if(get_full_element(vptr, &(ca->att[i]), g_tls.buffer, &size) < 0)
          return -1;
      /* This condition is introduce to avoid buffer allocated more than g_tls.buffer size */
      if(size >= VUSER_THREAD_BUFFER_SIZE)
         size = VUSER_THREAD_BUFFER_SIZE-1;

      MY_MALLOC(tmp_ptr, size+1, "click api attribute",0);
     // strncpy(tmp_ptr, tmp_buf, size);
      strncpy(tmp_ptr, g_tls.buffer, size);
      tmp_ptr[size]='\0';
    }

    att[i] = tmp_ptr;
  }

  return 0;
}


static inline void save_next_url_on_vptr(VUser *vptr, char *url, int url_len)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, vptr=%p", vptr);

  FREE_AND_MAKE_NULL(vptr->httpData->clicked_url, "vptr->httpData->clicked_url", 0);
  vptr->httpData->clicked_url_len = 0;

  MY_MALLOC(vptr->httpData->clicked_url, url_len+1, "vptr->httpData->clicked_url", 0);
  strncpy(vptr->httpData->clicked_url, url, url_len);
  vptr->httpData->clicked_url_len = url_len;
  vptr->httpData->clicked_url[url_len] = '\0';
}

static int handle_edit_field_checkbox_radio_group(VUser *vptr, char **att, void **nodes_list)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called");
  int create_new_page = 0;

  /* Print the current value */
  xmlChar *lol_value = xmlGetProp((xmlNode *)nodes_list[CLICKED_NODE_INDEX], (xmlChar *)"value");

  NSDL4_JAVA_SCRIPT(NULL, NULL, "Before setting input element node '%p' in dom for att[NAME] = '%s'; "
                    "value = '%s'", 
                    nodes_list[CLICKED_NODE_INDEX], 
                    att[NAME]==NULL?"null":att[NAME],
                    lol_value==NULL?"null":((char *) lol_value)
                    );

  /* Set the value in dom tree */
  if(att[VALUE])
    xmlSetProp((xmlNode *)nodes_list[CLICKED_NODE_INDEX], (xmlChar *) "value", (xmlChar*)att[VALUE]);
  else
    xmlSetProp((xmlNode *)nodes_list[CLICKED_NODE_INDEX], (xmlChar *) "value", (xmlChar*)"\0");

  /* Now set the value in js */
  set_form_input_element_in_js(vptr, (xmlNode *)nodes_list[FORM_NODE_INDEX], att[ID], att[NAME], att[TYPE], att[VALUE], NULL);
  
  /* Print the new set value */
  lol_value = xmlGetProp((xmlNode *)nodes_list[CLICKED_NODE_INDEX], (xmlChar *)"value");

  NSDL4_JAVA_SCRIPT(vptr, NULL, "Set form input element '%p' in JS for att[NAME] = '%s'; "
                                "value = '%s'", 
                                nodes_list[CLICKED_NODE_INDEX], 
                                att[NAME]?att[NAME]:"null",
                                lol_value?(char *) lol_value:"NULL");
            
  FREE_XML_CHAR(lol_value);
  return create_new_page;
}

static int handle_text_area(VUser *vptr, char **att, void **nodes_list)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called");
  int create_new_page = 0;
  
   /* Print the current value */
  xmlChar *lol_content = xmlNodeGetContent(nodes_list[CLICKED_NODE_INDEX]);
  NSDL4_JAVA_SCRIPT(vptr, NULL, "Before setting text area node '%p' in dom for att[NAME] = '%s'; "
                     "Content = '%s'", 
                     nodes_list[CLICKED_NODE_INDEX], 
                     att[NAME]?att[NAME]:"NULL",
                     lol_content?((char *) lol_content):"NULL");

  xmlNodeSetContent(nodes_list[CLICKED_NODE_INDEX], (xmlChar *)"\0");
        /* Set the value in dom tree */
  if(att[CONTENT])
    xmlNodeAddContent(nodes_list[CLICKED_NODE_INDEX], BAD_CAST ((xmlChar*)att[CONTENT]));

  FREE_XML_CHAR(lol_content);

  lol_content = xmlNodeGetContent(nodes_list[CLICKED_NODE_INDEX]);
  NSDL4_JAVA_SCRIPT(vptr, NULL, "After setting in DOM, node = '%p' att[NAME] = '%s'; "
                                "Content = '%s'", 
                                nodes_list[CLICKED_NODE_INDEX], 
                                att[NAME]?att[NAME]:"NULL",
                                lol_content?(char *) lol_content:"NULL");
            
  /* Now set the value in js */
  set_form_input_element_in_js(vptr, (xmlNode *)nodes_list[FORM_NODE_INDEX], att[ID], att[NAME], att[TYPE], NULL, att[CONTENT]);
        
  /* Print the new set value */
  lol_content = xmlNodeGetContent(nodes_list[CLICKED_NODE_INDEX]);

  NSDL4_JAVA_SCRIPT(vptr, NULL, "Set form input element '%p' in JS for att[NAME] = '%s'; "
                                "Content = '%s', "
                                "form node = %p", 
                                nodes_list[CLICKED_NODE_INDEX], 
                                att[NAME]?att[NAME]:"NULL",
                                att[CONTENT]?(char *) att[CONTENT]:"NULL",
                                nodes_list[FORM_NODE_INDEX]);

  FREE_XML_CHAR(lol_content);
  return create_new_page;
}


static int handle_map_area(VUser *vptr, char **att, void **nodes_list, char *url, unsigned short *p_url_len)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called");
  int create_new_page = 0;

   /* find the href */
  xmlChar *lol_href = xmlGetProp(nodes_list[CLICKED_NODE_INDEX], (xmlChar *)"href");

  NSDL4_JAVA_SCRIPT(vptr, NULL, "href = '%s'", lol_href==NULL?"null":(char *)lol_href);

  if(lol_href)
  {
    if(!strncmp((char *)lol_href, "javascript:", 11)){

      NSDL4_JAVA_SCRIPT(NULL, NULL, "searching the onclick event resistered for this node = '%p'", 
                                     nodes_list[CLICKED_NODE_INDEX]);

//      search_and_emit_event(vptr, js_context, global, nodes_list[CLICKED_NODE_INDEX], "onclick");
      create_new_page = 0;
    }else {
      make_url_request_line_and_save_server_data_on_vptr(vptr, (char *)lol_href, url, p_url_len);

      NSDL4_JAVA_SCRIPT(vptr, NULL, "Created url from href found in dom for click action, "
                                    "url= %p, '%s', url_len=%d", 
                                    url, url, *p_url_len);

      /* Now save the url on vptr for next page */
      save_next_url_on_vptr(vptr, url, *p_url_len);
      create_new_page = 1;
    }

    //We got url from href, now free lol_href 
    FREE_XML_CHAR(lol_href);

  } else {

    NS_EL_2_ATTR(EID_JS_XML_ERROR, vptr->user_index,
            vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
            vptr->sess_ptr->sess_name,
            vptr->cur_page->page_name,
            "XML Error: Could not find url in DOM, Alt = %s", att[ALT]?att[ALT]:"null");
    //TODO: What should we do???
  }
  return create_new_page;
}


static int handle_link(VUser *vptr, char **att, void **nodes_list, char *url, unsigned short *p_url_len)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called");
  int create_new_page = 0;

  JSContext *js_context = (JSContext *)vptr->httpData->js_context;
  JSObject *global = (JSObject *)vptr->httpData->global;
  
  if(nodes_list[ANCHOR_NODE_INDEX])
  {
   /* find the href */
    xmlChar *lol_href = xmlGetProp(nodes_list[ANCHOR_NODE_INDEX], (xmlChar *)"href");
 
    NSDL4_JAVA_SCRIPT(vptr, NULL, "href = '%s'", lol_href?(char *)lol_href:"NULL");
 
    if(lol_href)
    {
      if(!strncmp((char *)lol_href, "javascript:", 11)){
 
        NSDL4_JAVA_SCRIPT(NULL, NULL, "searching the onclick event resistered for this node = '%p'", 
                                       nodes_list[ANCHOR_NODE_INDEX]);
 
        search_and_emit_event(vptr, js_context, global, nodes_list[CLICKED_NODE_INDEX], "onclick");
        create_new_page = 0;
      }else {
        make_url_request_line_and_save_server_data_on_vptr(vptr, (char *)lol_href, url, p_url_len);
 
        NSDL4_JAVA_SCRIPT(vptr, NULL, "Created url from href found in dom for click action, "
                                      "url= %p, '%s', url_len=%d", 
                                      url, url, *p_url_len);
 
        create_new_page = 1;
      }
 
      //We got url from href, now free lol_href 
      FREE_XML_CHAR(lol_href);
    }
  }
 
  /*  If it is part of form, and has valid "name" attribute,
   *  we should set in JS, the <name>.x and <name>.y fields as input
   *  attributes of the form. This will be appended in url 
   *  as part of query string in case of method=GET or part
   *  of body in case of method=POST 
   */
  if(att[TAG] && att[NAME] && 
     (!strncasecmp(att[TAG], "img", 3) || !strncasecmp(att[TAG], "input", 5)) && 
     nodes_list[FORM_NODE_INDEX] && nodes_list[CLICKED_NODE_INDEX])
  { 
    set_x_y_coordinates_for_form_image(vptr, att[NAME], nodes_list[FORM_NODE_INDEX], 5,5);
    /* TODO: Temporarily setting the coordinates x and y to 5, eventually
     *       need to be changed to XCOORD and YCOORD  of att once these
     *       attributes are received at the time of recording in script 
     */
  }

  if(!nodes_list[ANCHOR_NODE_INDEX] && att[TAG] && strcasecmp(att[TAG], "input"))
  /* In case tag is 'input' and it is not part of an anchor, we should not 
   * log the event, because that is ok case. Any onclick event will be emitted
   * in handle_click_actions, so do nothing here.
   *
   * However, in case if it is an wnchor node or child of one, and if we could
   * not find the href in dom, then it is failure  case, and deserves an event be logged.
   */
    NS_EL_2_ATTR(EID_JS_XML_ERROR, vptr->user_index,
          vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
          vptr->sess_ptr->sess_name,
          vptr->cur_page->page_name,
          "XML Error: Could not find url in DOM, Alt = %s", att[ALT]?att[ALT]:"null");

  return create_new_page;
}

static int handle_list(VUser *vptr, char **att, void **nodes_list)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called");
  int create_new_page = 0;
 
  /* Now set the value in js */
  /* In case of multiple, take care of the multiple select options 
   * It is assumed that in ns_list() in the CONTENT field, we shall get 
   * the multiple values in single API. Any new ns_list will overwrite
   * the current selected options in JS*/
  set_form_input_element_in_js(vptr, nodes_list[FORM_NODE_INDEX], att[ID], att[NAME], att[TYPE], att[VALUE], att[CONTENT]);
  
  /* Print the new set value */
  NSDL4_JAVA_SCRIPT(vptr, NULL, "ns_list(), Set form input select element '%p' in JS for att[NAME] = '%s'; "
                                "Value = '%s', "
                                "Content = '%s', "
                                "form node = %p", 
                                nodes_list[CLICKED_NODE_INDEX], 
                                att[NAME]?att[NAME]:"NULL",
                                att[VALUE]?att[VALUE]:"NULL",
                                att[CONTENT]?(char *) att[CONTENT]:"NULL",
                                nodes_list[FORM_NODE_INDEX]);
            
  return create_new_page;

}

static inline void set_form_enctype_on_vptr(VUser *vptr, int form_encoding_type)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, form_encoding_type = %d", form_encoding_type);

  switch (form_encoding_type)
  {
    case FORM_POST_ENC_TYPE_TEXT_PLAIN:
      vptr->httpData->formenc_len=10; /* len of "text/plain" */
      MY_MALLOC(vptr->httpData->formencoding, vptr->httpData->formenc_len + 1, "vptr-httpData->formencoding", -1);
      strcpy(vptr->httpData->formencoding, "text/plain");
      break;

#if 0
this case is not yet implemented
    case FORM_POST_ENC_TYPE_MULTIPART_FORM_DATA:
      vptr->httpData->formenc_len=19; /* len of "multipart/form-data" */
      MY_MALLOC(vptr->httpData->formencoding, vptr->httpData->formenc_len + 1, "vptr-httpData->formencoding", -1);
      strcpy(vptr->httpData->formencoding, "multipart/form-data");
      break;
#endif
    case FORM_POST_ENC_TYPE_APPLICATION_X_WWW_FORM_URL_ENCODED:
    default:
      vptr->httpData->formenc_len=33; /* len of "application/x-www-form-urlencoded" */
      MY_MALLOC(vptr->httpData->formencoding, vptr->httpData->formenc_len + 1, "vptr-httpData->formencoding", -1);
      strcpy(vptr->httpData->formencoding, "application/x-www-form-urlencoded");
      break;

 }
}


static int handle_form(VUser *vptr, char **att, void **nodes_list, char *url, unsigned short *p_url_len)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called");
  int create_new_page = 0;
  int form_encoding_type = FORM_POST_ENC_TYPE_APPLICATION_X_WWW_FORM_URL_ENCODED;
 
  xmlChar *lol_url = xmlGetProp(nodes_list[CLICKED_NODE_INDEX], (xmlChar *)"action");
  xmlChar *lol_method = xmlGetProp(nodes_list[CLICKED_NODE_INDEX], (xmlChar *)"method");
  xmlChar *lol_enctype = xmlGetProp(nodes_list[CLICKED_NODE_INDEX], (xmlChar *)"enctype");

  NSDL4_JAVA_SCRIPT(vptr, NULL, "lol_url = '%s' ,"
                                "lol_method = '%s' ,"
                                "lol_enctype = '%s' ,",
                                lol_url?(char *) lol_url:"NULL",
                                lol_method?(char *) lol_method:"NULL",
                                lol_enctype?(char *) lol_enctype:"NULL");

/**************************************************************************/
/* Now create the url in following format:                                */
/*                                                                        */
/* "http://www.example.com/?username=ram&password=fishwhale&              */
/* userSession=ASD4356785"                                                */
/*                                                                        */
/* Also keep the offset to query param,  and http method on               */
/* vptr->httpData that will be retrieved later                            */
/**************************************************************************/
  /* copy url */
  char in_url[MAX_LINE_LENGTH +1] = "\0";

  if(lol_url==NULL){
    NSDL4_JAVA_SCRIPT(vptr, NULL, "'action' field is null in form, will use the current"
                                  " main page url to submit the form data");
    strcpy(in_url, vptr->httpData->page_main_url);
    /* In case the page_main_url has a query string (due to previously submitted form, eg)
     * there will be ? character in url. If present, truncate it.
     */
    char *qstr = NULL;
    qstr = strchr(in_url,'?');
    if(qstr)
    {
      NSDL4_JAVA_SCRIPT(vptr, NULL, "current main page url has a query string, '%s', "
                                     "removing it before appending new query string. ",
                                     in_url);
      *qstr = '\0';
      NSDL4_JAVA_SCRIPT(vptr, NULL, "truncated url = '%s'", in_url);
    }
    
  } else {
    strcpy(in_url, (char *)lol_url);
  }

  NSDL2_JAVA_SCRIPT(vptr, NULL, "ns_form will cause new url to be hit; url = %p, %s", in_url, in_url);
  make_url_request_line_and_save_server_data_on_vptr(vptr, in_url, url, p_url_len);

  /* check and save the method */
  
  vptr->httpData->http_method = HTTP_METHOD_GET;/*Default is GET */

  if (lol_method && lol_method[0] != '\0'){
    if (!strncasecmp((char *)lol_method, "GET", 3))
      vptr->httpData->http_method = HTTP_METHOD_GET;
    else if (!strncasecmp((char *)lol_method, "POST", 4))
      vptr->httpData->http_method = HTTP_METHOD_POST;
  }

  if (vptr->httpData->http_method == HTTP_METHOD_POST)
  {
    form_encoding_type = FORM_POST_ENC_TYPE_APPLICATION_X_WWW_FORM_URL_ENCODED;

    if(lol_enctype)
    {
      if(!xmlStrcmp(lol_enctype, (xmlChar *)"application/x-www-form-urlencoded"))
        form_encoding_type = FORM_POST_ENC_TYPE_APPLICATION_X_WWW_FORM_URL_ENCODED;
      else if (!xmlStrcmp(lol_enctype, (xmlChar *)"text/plain"))
        form_encoding_type = FORM_POST_ENC_TYPE_TEXT_PLAIN;
/*      else if (!xmlStrcmp(lol_enctype, (xmlChar *)"multipart/form-data"))
        form_encoding_type = FORM_POST_ENC_TYPE_MULTIPART_FORM_DATA;

        multipart/form-data not yet supported.
*/
      else
      {
        NS_EL_2_ATTR(EID_JS_DOM, vptr->user_index,
                     vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                     vptr->sess_ptr->sess_name,
                     vptr->cur_page->page_name,
                     "WARNING: Form being submitted has enctype=\"%s\" which is not supported. "
                     "Treating as default enctype=\"application/x-www-form-urlencoded\"", 
                     lol_enctype?(char *)lol_enctype:"null");
      }
    }

    set_form_enctype_on_vptr(vptr, form_encoding_type);
  }

  char *form_data_str = get_form_data_from_js(vptr, nodes_list[CLICKED_NODE_INDEX], form_encoding_type);
  if (!form_data_str)
    form_data_str = "\0";

  int post_body_len=0;

  switch (vptr->httpData->http_method)
  {
    case HTTP_METHOD_GET:
      if (form_data_str && form_data_str[0] != '\0')
      {
        if(strcmp(form_data_str, "undefined") == 0)
          NSDL4_JAVA_SCRIPT(vptr, NULL, " query string read from JS = 'undefined'; not appending to the url");
        else
          sprintf(url, "%s?%s",url, form_data_str);

      *p_url_len += strlen(form_data_str) + 1; /* +1 for the '?' character */
      }
      break;

    case HTTP_METHOD_POST:
      post_body_len = strlen(form_data_str);
      MY_MALLOC(vptr->httpData->post_body, post_body_len + 1, "POST Body", 0);
      strcpy(vptr->httpData->post_body, form_data_str);
//      strcat(vptr->httpData->post_body, "\r\n");
      vptr->httpData->post_body_len = post_body_len;
      break;
  }
  

  /* Now save the url on vptr for next page */
  save_next_url_on_vptr(vptr, url, *p_url_len);

  FREE_XML_CHAR (lol_url);
  FREE_XML_CHAR (lol_method);
  FREE_XML_CHAR (lol_enctype);

  create_new_page = 1;

  return create_new_page;

}

/********* Bug#3570 Begin */
static int confirm_if_new_url_is_different_from_current_main_page_url(VUser *vptr)
{

  char cur_hostname[512] = "\0";
  NSDL4_JAVA_SCRIPT(vptr, NULL, "Method called");

  int cur_page_req_type = extract_request_type_from_url(vptr->httpData->page_main_url);

  if(vptr->httpData->request_type != cur_page_req_type)
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "new request type (%d) does not match with current main page (%d)", 
                                  vptr->httpData->request_type, cur_page_req_type);
    return 1;
  }

  extract_hostname_from_absolute_url(vptr->httpData->page_main_url + (cur_page_req_type==HTTP_REQUEST?7:8), cur_hostname);
  if(strcmp(vptr->httpData->server_hostname, cur_hostname))
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "new hostname (%s) does not match with current main page (%s)", 
                                  vptr->httpData->server_hostname, cur_hostname);
    return 1;
  }

  if(strcmp(vptr->httpData->clicked_url, strstr(vptr->httpData->page_main_url, cur_hostname) + strlen(cur_hostname)))
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "new url request line (%s) does not match with current main page (%s)", 
                                  vptr->httpData->clicked_url, vptr->httpData->page_main_url);
    return 1;  
  }

  if(vptr->httpData->post_body && vptr->httpData->post_body_len > 0 && 
     vptr->httpData->http_method == HTTP_METHOD_POST)
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "new url is same as main page, but the next request is post, with post body");
    return 1;
  }

  NSDL4_JAVA_SCRIPT(vptr, NULL, "new url is same as main page, should not create new page, returning 0");
  return 0;
} 
/********* Bug#3570 End */


/***********************************
 * handle_click_actions()
 * 
 * Engine for executing the clicked action
 * Reads click action attributes from click action table indexed with ca_idx passed
 * This function is called from nsi_click_action() in ns_vuser_tasks.c in NVM ctx.
 * **********************************/
int handle_click_actions(VUser *vptr, int page_id, int ca_idx)
{
  int create_new_page = 0;

  char url[64*1024] = "\0";
  unsigned short url_len;

  ClickActionTableEntry_Shr *ca;
  char *att[NUM_ATTRIBUTE_TYPES];
  char *tmp;

  JSContext *js_context;
  JSObject *global;

  static int caching_warning_displayed = 0;

#ifdef NS_DEBUG_ON
  char debug_msg[64*1024];
#endif
  char warning_msg[1024] = "\0";
  void *nodes_list[4] = {(void *)0,(void *)0,(void *)0,(void *)0};
  int ret=0;


  NSDL4_JAVA_SCRIPT(vptr, NULL, "Method called, page_id = %d, ca_idx = %d", page_id, ca_idx);

  /* Find the click action table entry for ca_idx */
  ca = &(clickaction_table_shr_mem[ca_idx]);

  memset(att, 0, sizeof(att));
  if(read_attributes_array_from_ca_table(vptr, att, ca) < 0) {
    NSDL4_JAVA_SCRIPT(NULL, NULL, "vptr->page_status = %d", vptr->page_status);
    return create_new_page;
  }
  NSDL4_JAVA_SCRIPT(NULL, NULL, "Attributes Array Read, ATTRIBUTES: %s", attributes2str(att, debug_msg));

  /* If JS is disabled, do nothing */
  if(runprof_table_shr_mem[vptr->group_num].gset.js_mode == NS_JS_DISABLE){

    sprintf(warning_msg, "Click Script API '%s()' found for a virtual user in group "
                   "for which Java Script was disabled. Check keyword 'G_JAVA_SCRIPT' in "
                   "scenario file. Ignoring the API in flow file.", 
                   att[APINAME]?att[APINAME]:"NULL");

    fprintf(stderr, "WARNING: %s\n", warning_msg);

    NSDL1_JAVA_SCRIPT(vptr, NULL, "WARNING: %s", warning_msg);

    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__, warning_msg);

    create_new_page = 0;
    free_attributes_array(att);
    return create_new_page;
  }

  if(!NS_IF_CACHING_ENABLE_FOR_USER && !caching_warning_displayed){

    /* Caching disabled warning is displayed only once */
    caching_warning_displayed = 1;

    sprintf(warning_msg, "Click Script API '%s()' found for a virtual user in group "
                   "for which CACHING is disabled. No embedded javascript files will be executed. "
                   "If any embedded javascript files (.js) are expected for this Test Run, it "
                   "is recommended to enable caching using 'G_HTTP_CACHING' key word in "
                   "the scenario file.", att[APINAME]?att[APINAME]:"NULL");

    fprintf(stderr, "WARNING: %s\n", warning_msg);

    NSDL1_JAVA_SCRIPT(vptr, NULL, "WARNING: %s", warning_msg);

    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__, warning_msg);

  }


  js_context = (JSContext *) vptr->httpData->js_context;
  global = (JSObject *) vptr->httpData->global;

  if(att[APINAME] == NULL)
  {
    create_new_page = 0;
    free_attributes_array(att);
    NSDL4_JAVA_SCRIPT(vptr, NULL, "APINAME is null, returning with create_new_page = %d", create_new_page);
    return create_new_page;
  }

  /*************  Handle ns_browser() *************/
  if(!strncasecmp(att[APINAME], "ns_browser", 10))
  {
    create_new_page = 1;
    free_attributes_array(att);
    NSDL4_JAVA_SCRIPT(vptr, NULL, "ns_browser() case, returning with create_new_page = %d", create_new_page);
    return create_new_page;
  }

  /* If DOM was not created due to some reason, we should not process any click api except the ns_browser() */
  if(vptr->httpData->ptr_html_doc == NULL)
  {
    create_new_page = 0;

    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__,
                  "Since DOM is NULL, current api '%s' will not be executed. "
                  "Returning with create_new_page = %d", 
                  att[APINAME]?att[APINAME]:"NULL",
                  create_new_page);

 
    NSDL1_JAVA_SCRIPT(vptr, NULL, "Since DOM is NULL, current api '%s' will not be executed. "
                                  "Returning with create_new_page = %d", 
                                  att[APINAME]?att[APINAME]:"NULL",
                                  create_new_page);

    free_attributes_array(att);
    free_dom_and_js_ctx(vptr);
    return create_new_page;

  }
  

#ifdef NS_DEBUG_ON
  /*********** Read the window.location.href set in JS, should be same as set by bootstrap JS  *********/
  tmp = get_window_location_href(vptr, js_context, global);
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Before emitting click event, window location href = %p, %s", tmp, tmp);
  tmp = NULL;
#endif

  /* Set the root node ptr in nodes_list[] */
  nodes_list[ROOT_NODE_INDEX] = (void *)html_parse_get_root(vptr->httpData->ptr_html_doc);

 
/************** Handle ns_button() and ns_span()***********************************/
/* ns_button() case handles                                                       */
/*     1. tag=input and type=button - In this case "value" is searched in DOM     */
/*     2. tag=button - In this case "content" is searched in DOM                  */
/**********************************************************************************/
  if(!strncasecmp(att[APINAME], "ns_button", 9) ||
     !strncasecmp(att[APINAME], "ns_browse_file", 14) ||
     !strncasecmp(att[APINAME], "ns_span", 7))
  {
    ret = search_clicked_element_in_dom(att, FIRST_TIME, ATTR_MATCH_DEFAULT|ATTR_VALUE|ATTR_CONTENT, nodes_list);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "search_clicked_element_in_dom() returned '%d', "
                                  "FORM_NODE_PTR = '%p', " 
                                  "SELECT_NODE_PTR = '%p', " 
                                  "ANCHOR_NODE_PTR = '%p'",
                                  ret, 
                                  nodes_list[FORM_NODE_INDEX],
                                  nodes_list[SELECT_NODE_INDEX],
                                  nodes_list[ANCHOR_NODE_INDEX]);

    /* In this case only onclick event has to be emitted, which is 
     * done after handling all apis
     */ 


  }
  else

/********************** Handle ns_link() ******************************************/
/* This case handles                                                              */
/*     1. type = link       - in this case DOM is searched for link type          */
/*                            elements with CONTENT field                         */
/*     2. type = image_link - in this case DOM is searched for link type          */
/*                            elements with ALT field                             */
/**********************************************************************************/
 if(!strncasecmp(att[APINAME], "ns_link", 7))
 {
    if(att[TAG] && att[TYPE])
    {
      int flags = 0;
      if(!strncasecmp(att[TAG], "A", 1) && !strncasecmp(att[TYPE], "link", 4))
        flags = ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_TITLE|ATTR_SHAPE|ATTR_COORDS|ATTR_CONTENT;

      else if(!strncasecmp(att[TAG], "IMG", 3) && !strncasecmp(att[TYPE], "image_link", 10))
        flags = ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_TITLE|ATTR_ALT|ATTR_COORDS;

      else if(!strncasecmp(att[TAG], "INPUT", 5) && !strncasecmp(att[TYPE], "IMAGE", 5))
        flags = ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_TYPE|ATTR_TITLE;
      
      ret = search_clicked_element_in_dom(att, FIRST_TIME, flags, nodes_list); 

      NSDL4_JAVA_SCRIPT(vptr, NULL, "search_clicked_element_in_dom() returned '%d', "
                                    "FORM_NODE_PTR = '%p', " 
                                    "SELECT_NODE_PTR = '%p', " 
                                    "ANCHOR_NODE_PTR = '%p'",
                                    ret, 
                                    nodes_list[FORM_NODE_INDEX],
                                    nodes_list[SELECT_NODE_INDEX],
                                    nodes_list[ANCHOR_NODE_INDEX]);
  
      create_new_page = handle_link(vptr, att, nodes_list, url, &url_len);

      if (create_new_page == 1)
      {
        NSDL4_JAVA_SCRIPT(vptr, NULL, "Found url in dom for click action, url= %p, '%s', url_len=%d", url, url, url_len);
        /* Now save the url on vptr for next page */
        save_next_url_on_vptr(vptr, url, url_len);
      }
    } 
  }/* End of ns_link */
  
  else

/**********************************************************************************/
/* Handle                                                                         */
/*   1. ns_edit_field()                                                           */
/*   2. ns_check_box()                                                            */
/*   3. ns_radio_group()                                                          */
/*  Takes care of                                                                 */
/*     1. Text - edit field                                                       */
/*     2. Password - edit field                                                   */
/**********************************************************************************/
  if(!strncasecmp(att[APINAME], "ns_edit_field", 13) ||
     !strncasecmp(att[APINAME], "ns_check_box", 12) ||
     !strncasecmp(att[APINAME], "ns_radio_group", 14))
  {

    ret = search_clicked_element_in_dom(att, FIRST_TIME, ATTR_MATCH_DEFAULT, nodes_list);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "search_clicked_element_in_dom() returned '%d', "
                                  "FORM_NODE_PTR = '%p', " 
                                  "SELECT_NODE_PTR = '%p', " 
                                  "ANCHOR_NODE_PTR = '%p'",
                                  ret,
                                  nodes_list[FORM_NODE_INDEX],
                                  nodes_list[SELECT_NODE_INDEX],
                                  nodes_list[ANCHOR_NODE_INDEX]);

    if(ret == 0) /* Match found */
      create_new_page = handle_edit_field_checkbox_radio_group(vptr, att, nodes_list);
    else
      NSDL4_JAVA_SCRIPT(vptr, NULL, "Clicked element not found in DOM");
  }

  else

/**********************************************************************************/
/* Handle                                                                         */
/*   ns_text_area()                                                            */
/**********************************************************************************/
  if(!strncasecmp(att[APINAME], "ns_text_area", 12))
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "handling ns_text_area()");

    ret = search_clicked_element_in_dom(att, FIRST_TIME, ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_TITLE, nodes_list);
    
    NSDL4_JAVA_SCRIPT(vptr, NULL, "search_clicked_element_in_dom() returned '%d', "
                                  "FORM_NODE_PTR = '%p', " 
                                  "SELECT_NODE_PTR = '%p', " 
                                  "ANCHOR_NODE_PTR = '%p'",
                                  ret,
                                  nodes_list[FORM_NODE_INDEX],
                                  nodes_list[SELECT_NODE_INDEX],
                                  nodes_list[ANCHOR_NODE_INDEX]);
  
    if(ret == 0)   
      create_new_page = handle_text_area(vptr, att, nodes_list);
    else
      NSDL4_JAVA_SCRIPT(vptr, NULL, "Clicked element not found in DOM");
  }

  else

/**********************************************************************************/
/* Handle                                                                         */
/*   ns_map_area()                                                            */
/**********************************************************************************/
  if(!strncasecmp(att[APINAME], "ns_map_area", 11))
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "handling ns_map_area()");
    
    ret = search_clicked_element_in_dom(att, FIRST_TIME, 
                                        ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_ALT|ATTR_TITLE|ATTR_SHAPE|ATTR_COORDS, 
                                        nodes_list);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "search_clicked_element_in_dom() returned '%d', "
                                  "FORM_NODE_PTR = '%p', " 
                                  "SELECT_NODE_PTR = '%p', " 
                                  "ANCHOR_NODE_PTR = '%p'",
                                  ret,
                                  nodes_list[FORM_NODE_INDEX],
                                  nodes_list[SELECT_NODE_INDEX],
                                  nodes_list[ANCHOR_NODE_INDEX]);
    if(ret==0)
      create_new_page = handle_map_area(vptr, att, nodes_list, url, &url_len);
    else
      NSDL4_JAVA_SCRIPT(vptr, NULL, "Clicked element not found in DOM");
  }

  else

/**********************************************************************************/
/* Handle                                                                         */
/*   ns_list()                                                            */
/**********************************************************************************/
  if(!strncasecmp(att[APINAME], "ns_list", 7))
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "handling ns_list()");

    ret = search_clicked_element_in_dom(att, FIRST_TIME, 
                                        ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_TITLE, 
                                        nodes_list);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "search_clicked_element_in_dom() returned '%d', "
                                  "FORM_NODE_PTR = '%p', " 
                                  "SELECT_NODE_PTR = '%p', " 
                                  "ANCHOR_NODE_PTR = '%p'",
                                  ret,
                                  nodes_list[FORM_NODE_INDEX],
                                  nodes_list[SELECT_NODE_INDEX],
                                  nodes_list[ANCHOR_NODE_INDEX]);
    if(ret==0)
      create_new_page = handle_list(vptr, att, nodes_list);
    else
      NSDL4_JAVA_SCRIPT(vptr, NULL, "Clicked element not found in DOM");
  }

  else

/**************************** Handle ns_form() ************************************/
  if(!strncasecmp(att[APINAME], "ns_form", 7))
  {
 
    NSDL4_JAVA_SCRIPT(vptr, NULL, "handling ns_form()");

    ret = search_clicked_element_in_dom(att, FIRST_TIME, 
                                        ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_TITLE, 
                                        nodes_list);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "search_clicked_element_in_dom() returned '%d', "
                                  "FORM_NODE_PTR = '%p', " 
                                  "SELECT_NODE_PTR = '%p', " 
                                  "ANCHOR_NODE_PTR = '%p'",
                                  ret,
                                  nodes_list[FORM_NODE_INDEX],
                                  nodes_list[SELECT_NODE_INDEX],
                                  nodes_list[ANCHOR_NODE_INDEX]);
    if(ret==0)
      create_new_page = handle_form(vptr, att, nodes_list, url, &url_len);
    else
      NSDL4_JAVA_SCRIPT(vptr, NULL, "Clicked element not found in DOM");
  }

    /* Handle other API's */

  if(att[ACTION] && !strcasecmp(att[ACTION], "change"))
    search_and_emit_event(vptr, js_context, global, nodes_list[CLICKED_NODE_INDEX], "onchange");
  else if(att[ACTION] && !strcasecmp(att[ACTION], "click"))
    search_and_emit_event(vptr, js_context, global, nodes_list[CLICKED_NODE_INDEX], "onclick");
    

  if(!create_new_page)
  {

    /* Now check if some JS function executed during above click
     * action processing has updated window.location.href        */
    tmp = get_window_location_href(vptr, js_context, global);

    if(tmp && tmp[0] != '\0' && 
       vptr->httpData->page_main_url && 
       vptr->httpData->page_main_url[0] != '\0' && 
       (strcmp(tmp, vptr->httpData->page_main_url) != 0)) 
    {
      /* Some JS function has updated window.location.href */
      NSDL2_JAVA_SCRIPT(vptr, NULL, "Click action will cause new url to be hit; url = %p, %s", tmp, tmp);
      make_url_request_line_and_save_server_data_on_vptr(vptr, tmp, url, &url_len);
 
      /* Now save the url on vptr for next page */
      save_next_url_on_vptr(vptr, url, url_len);
      create_new_page = 1;
    }
  }

  free_attributes_array(att);

  /********* Bug#3570 Begin */
  if(create_new_page)
  {
    create_new_page = confirm_if_new_url_is_different_from_current_main_page_url(vptr);
  }

  /********* Bug#3570 End */

  NSDL4_JAVA_SCRIPT(vptr, NULL, "Returning create_new_page = %d", create_new_page);
 
  return create_new_page;
}

/* END OF FILE */
