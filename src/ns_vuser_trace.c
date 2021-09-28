
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <sys/ioctl.h>
#include <assert.h>
//#include <linux/cavmodem.h>
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <libgen.h>
#include <sys/stat.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_tag_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
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
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "init_cav.h"
#include "ns_parse_src_ip.h"
#include "nslb_sock.h"
#include "ns_trans_parse.h"
#include "ns_custom_monitor.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
//#include "ns_handle_read.h"
#include "ns_goal_based_sla.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_monitor_profiles.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_wan_env.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_debug_trace.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_child_msg_com.h"
#include "ns_page.h"
#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_index_vars.h"
#include "ns_unique_numbers.h"
#include "ns_date_vars.h"
#include "ns_error_codes.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_event_log.h"
#include "ns_data_types.h"
#include "ns_parse_scen_conf.h"
#include "ns_event_id.h"
#include "ns_string.h"

#include "ns_vuser_trace.h"
#include "ns_trace_log.h"
#include "ns_url_req.h"
#include "nslb_encode.h"

#include "nslb_util.h"
#include "ns_http_cache.h"
#include "ns_page_dump.h"
#include "wait_forever.h" //For loader_opcode
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_cav_conf.h"
//#define _FL_ __FUNCTION__,__LINE__

/* Start: child code */

char url[] = "www.google.com";
char param_name[] = "Search";
char param_value[] = "XXXXXXX";
char req_log[] = "req_log";
char rep_log[] = "rep_log";
char rep_body_log[] = "rep_body_log";

#ifndef CAV_MAIN
userTraceGroup *user_trace_grp_ptr = NULL;
#else
__thread userTraceGroup *user_trace_grp_ptr = NULL;
#endif

//128 is max chars 
// It is assumed that it will be set to 0 as it is static
static char specified_char[128]; 

#define MAX_LEN_TO_COPY 4096 

static int make_xml_file_path (VUser *vptr, char *xml_file_path);
static int ut_save_xml(VUser *vptr, char *xml_file_path, int called_frm);


static void inti_specified_char()
{
  memset(specified_char, 0, 128);
  specified_char['&'] = 1;
  specified_char['<'] = 1;
  specified_char['>'] = 1;
}

static int inline ns_memncpy(char* dest, char* source, int num)
{
  int i;

  NSDL2_USER_TRACE(NULL, NULL, "Method called");
 
  if (!source)
    return 0;

  for (i = 0; i < num; i++, dest++, source++) 
  { 
    *dest = *source;
  } 
  return i;
}


static void make_req_file(VUser *vptr, int http_size, int num_vectors, struct iovec *vector_ptr)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  int num_left = http_size;
  //char *send_buf;
  struct iovec* v_ptr;
  int amt_writen;
  int i;
  int buf_offset = 0;
  userTraceData* utd_node = NULL;
  //UserTraceNode *page_node = user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  GET_UT_TAIL_PAGE_NODE
    //MY_MALLOC (send_buf, num_left+1, "Allocating memory for vuser trace request file", -1);
    //
  NSDL1_USER_TRACE(NULL, NULL, "Method called page_node->page_info->page_name = %s",  page_node->page_info->page_name);
 
  // For now, page_node->page_info->req_size will be 0.
  // When we append embedded URL also, then it will get append to the same buffer
  MY_REALLOC_EX (page_node->page_info->req, (page_node->page_info->req_size + num_left + line_break_length + 1) , page_node->page_info->req_size, "REALLOC for REDIRECT_URL", -1);

  buf_offset += page_node->page_info->req_size;
  for (i = 0, v_ptr = vector_ptr; i < num_vectors; i++, v_ptr++)
  {
    //This block should not be needed, as size is calculated apriori
    if (num_left < v_ptr->iov_len)
    {
      fprintf(stderr, "Error: make_req_file(): Request is too big\n");
      //free_vectors (num_vectors, free_array, vector_ptr);
      end_test_run();   /* Mind as well end the test run because this will always fail */
    }
    if(v_ptr->iov_len == 0){
      NSDL1_USER_TRACE(NULL, NULL, "v_ptr->iov_base is NULL, so continue");
      continue;
    }
    amt_writen = ns_memncpy(page_node->page_info->req + buf_offset, v_ptr->iov_base, v_ptr->iov_len);
    num_left -= amt_writen;
    buf_offset += amt_writen;
  }
  /* For bug #28512, in case of #G_TRACING ALL 4 1 2 0 0 0 in url_req file, 2 line breaks were dumped, for removing it 
     following 2 lines is commented*/
  //amt_writen = ns_memncpy(page_node->page_info->req + buf_offset, line_break, line_break_length);
  //buf_offset += amt_writen;
  page_node->page_info->req[buf_offset] = '\0';
  page_node->page_info->req_size = buf_offset; // Total legth

  /*else if(cptr->url_num->proto.http.type == REDIRECT_URL)
  {
    MY_REALLOC (send_buf, (page_node->page_info->req_size + num_left + 1) , "REALLOC for REDIRECT_URL", -1);
    buf_offset += page_node->page_info->req_size;
    send_buf[buf_offset] = '\n';
    for (i = 0, v_ptr = vector_ptr; i < num_vectors; i++, v_ptr++)
    {
      //This block should not be needed, as size is calculated apriori
      if (num_left < v_ptr->iov_len)
      {
        fprintf(stderr, "make_req_file(): Request is too big\n");
        //free_vectors (num_vectors, free_array, vector_ptr);
        end_test_run();  
      }
      amt_writen = ns_memncpy(send_buf + buf_offset, v_ptr->iov_base, v_ptr->iov_len);
      num_left -= amt_writen;
      buf_offset += amt_writen;
      }
    send_buf[buf_offset] = '\0';
    page_node->page_info->req_size = buf_offset + 1;
  }*/
  //return send_buf;
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting");
}
void append_req_file(VUser *vptr, int http_size, int num_vectors, struct iovec *vector_ptr)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  char req_file[19+1];
  FILE *req_file_fp = NULL;
  char xml_file_path[4048];
  char *tmp_ptr;
  char tmp_buf[4048];

  int num_left = http_size;
  struct iovec* v_ptr;
  //int amt_writen;
  int i;
  //int buf_offset = 0;
  
  userTraceData* utd_node = user_trace_grp_ptr[vptr->group_num].utd_head;
  //utd_node->curr_node_id;
 
  make_xml_file_path (vptr, xml_file_path);
  sprintf(req_file, "req_%d.log", utd_node->curr_node_id);

  //UserTraceNode *page_node = user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  tmp_ptr = strrchr(xml_file_path, '/');
  *tmp_ptr = '\0';
  sprintf(tmp_buf, "%s/%s", xml_file_path, req_file);
  NSDL1_USER_TRACE(NULL, NULL, "Req file path = %s", tmp_buf);
  req_file_fp = fopen(tmp_buf, "a+");
  if(req_file_fp == NULL){
    NSDL1_USER_TRACE(NULL, NULL, "Error in openning file = %s", tmp_buf);
    fprintf(stderr, "Error in openning file = %s", tmp_buf);
    return;    
  }

  for (i = 0, v_ptr = vector_ptr; i < num_vectors; i++, v_ptr++)
  {
    //This block should not be needed, as size is calculated apriori
    if (num_left < v_ptr->iov_len)
    {
      fprintf(stderr, "Error: make_req_file(): Request is too big\n");
      //free_vectors (num_vectors, free_array, vector_ptr);
      end_test_run();
    }
    if(v_ptr->iov_len == 0){
      NSDL1_USER_TRACE(NULL, NULL, "v_ptr->iov_base is NULL, so continue");
      continue;
    }
    fprintf(req_file_fp, "%*.*s", (int)v_ptr->iov_len, (int)v_ptr->iov_len, (char *)v_ptr->iov_base);
  } 
  fclose(req_file_fp); 
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting");
}

void ut_update_req_file(VUser *vptr, int http_size, int num_vectors, struct iovec *vector_ptr){

  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  //userTraceData* utd_node = user_trace_grp_ptr[vptr->group_num].utd_head;
//#ifdef NS_DEBUG_ON
  //UserTraceNode *page_node = user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  userTraceData* utd_node = NULL;
  GET_UT_TAIL_PAGE_NODE
//#endif
  NSDL1_USER_TRACE(NULL, NULL, "Method called for page no = %d", utd_node->curr_node_id );
  char xml_file_path [4048];
  if(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN)
  {
    //Here make_req_file is called because in Page dump we need full buffer
    //So, now it will fill buffer also if page dump is enable
    if(NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL)
      make_req_file(vptr, http_size, num_vectors, vector_ptr);
    append_req_file(vptr, http_size, num_vectors, vector_ptr);
  }
  else
  {
    //page_node->page_info->req = make_req_file(cptr, vptr, http_size, num_vectors, vector_ptr);
    make_req_file(vptr, http_size, num_vectors, vector_ptr);
  }
  page_node->type = NS_UT_TYPE_PAGE_REQ;
  NSDL1_USER_TRACE(NULL, NULL, "Page node next = %p", page_node->next);
  //if(!(vptr->flags & NS_PAGE_DUMP_ENABLE)) //If this node is not for Page dump
  if(NS_IF_TRACING_ENABLE_FOR_USER) //If this node is not for Page dump
    ut_save_xml(vptr, xml_file_path, 1);
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting");
}

/*This function is used for page dump only*/
void ut_update_rep_file_for_page_dump(VUser *vptr, int resp_size, char *resp)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  userTraceData* utd_node = NULL;
  int buf_offset = 0;
  GET_UT_TAIL_PAGE_NODE

  buf_offset = page_node->page_info->rep_size;
  MY_REALLOC (page_node->page_info->rep, (page_node->page_info->rep_size + resp_size + 1), "Allocating memory for vuser trace rep file", -1);
  ns_memncpy(page_node->page_info->rep + buf_offset, resp, resp_size);
  page_node->page_info->rep_size += resp_size;
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting");
}

void ut_update_rep_file_line_break_for_page_dump(connection *cptr){
  NSDL1_USER_TRACE(NULL, cptr, "Method called");
  ut_update_rep_file_for_page_dump(cptr->vptr, line_break_length, line_break);
}

void ut_update_rep_file(VUser *vptr, int resp_size, char *resp)
{
  NSDL1_USER_TRACE(vptr, NULL, "Method called");
  char xml_file_path[4048];
  userTraceData* utd_node = NULL;
  //UserTraceNode *page_node = user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  GET_UT_TAIL_PAGE_NODE

  MY_MALLOC (page_node->page_info->rep, resp_size+1, "Allocating memory for vuser trace rep file", -1);
  strncpy(page_node->page_info->rep, resp, resp_size);
  page_node->page_info->rep[resp_size] = '\0';
  NSDL1_USER_TRACE(NULL, NULL, "Page node next = %p", page_node->next);
  //if(!(vptr->flags & NS_PAGE_DUMP_ENABLE)) //If this node is not for Page dump
  if(NS_IF_TRACING_ENABLE_FOR_USER) //If this node is not for Page dump
    ut_save_xml(vptr, xml_file_path, 1);
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting");
}

void ut_update_rep_body_file(VUser *vptr, int resp_size, char *resp)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  userTraceData* utd_node = NULL;
  //UserTraceNode *page_node = user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  GET_UT_TAIL_PAGE_NODE
  NSDL1_USER_TRACE(NULL, NULL, "Method called with page_node->page_info->rep_body = %p", page_node->page_info->rep_body);

  //If connection is failing then it is genrating resp body file
  //Putted check for resp length, if length is 0 then dont create
  //buffer for resp body
  if(page_node->page_info->rep_body == NULL && resp_size > 0) 
  {
    NSDL1_USER_TRACE(NULL, NULL, "Method called with page_node->page_info->rep_body = %p", page_node->page_info->rep_body);
    MY_MALLOC (page_node->page_info->rep_body, resp_size + 1, "Allocating memory for vuser trace rep file", -1);
    NSDL1_USER_TRACE(NULL, NULL, "After allocation rep_body = %p", page_node->page_info->rep_body);
    if(page_node->page_info->rep_body)
    {
      strncpy(page_node->page_info->rep_body, resp, resp_size);
      page_node->page_info->rep_body[resp_size] = '\0';
      page_node->page_info->rep_body_size = resp_size;
    }
  }
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting with page_node->page_info->rep_body = %p", page_node->page_info->rep_body);
}

static UserTraceNode *ut_alloc_node()
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  UserTraceNode *tmp_ut_node = NULL;
  MY_MALLOC_AND_MEMSET(tmp_ut_node, sizeof(UserTraceNode), "User Trace", -1);
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
  return tmp_ut_node;
}

/*This function is for adding node into 
*a linkedlist.
*Input : vptr and node to add*/
static void ut_add_node(VUser* vptr, UserTraceNode *node, char *node_info)
{
  //int grp_num;

  //grp_num = vptr->group_num;

  userTraceData* utd_node = NULL;
  GET_UTD_NODE

  //userTraceData* utd_node = user_trace_grp_ptr[grp_num].utd_head;

  NSDL1_USER_TRACE(NULL, NULL, "Method called for %s. group_num = %d", node_info, vptr->group_num);

  // Add at the head if head is NULL else at the tail
  // Create the first node
  if(utd_node->ut_head == NULL)
  {
    // head and tail are same point for first node
    utd_node->ut_head = node;
    utd_node->ut_tail = node;
  }
  else
  {
    // tail is move to next node
    ((UserTraceNode*)(utd_node->ut_tail))->next = node;
    utd_node->ut_tail = node;
  }
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting"); 
}


static UserTracePageInfo *create_page_info_node()
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  UserTracePageInfo *tmp_ut_page_node = NULL;
  MY_MALLOC_AND_MEMSET(tmp_ut_page_node, sizeof(UserTracePageInfo), "User Trace", -1);

  //Set all numeric value to -1
  tmp_ut_page_node->page_rep_time = -1;
  tmp_ut_page_node->http_status_code = -1;
  tmp_ut_page_node->page_status = -1;
  tmp_ut_page_node->page_think_time = -1;

  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
  return tmp_ut_page_node;
}


static UserTraceParam *create_param_node()
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  UserTraceParam *tmp_ut_param_node = NULL;
  MY_MALLOC_AND_MEMSET(tmp_ut_param_node, sizeof(UserTraceParam), "User Trace", -1);
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
  return tmp_ut_param_node;
}

/*This function is for adding parameter node into
*a linkedlist.
*Input : vptr_node and node to add*/
static void add_param_node(UserTracePageInfo *page_info_node, UserTraceParam *param_node)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");

  // Add at the head if head is NULL else at the tail
  // Create the first node
  UserTraceParam *tmp_param_node = page_info_node->param_head;

  if(page_info_node->param_head == NULL)
  {
    //For first parameter node
    page_info_node->param_head = param_node;
    page_info_node->param_head->next = NULL;

  }
  else
  {
    //Add another node from front
    page_info_node->param_head = param_node;
    page_info_node->param_head->next = tmp_param_node;
  }
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}


static UserTraceParamUsed *create_param_used_node()
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  UserTraceParamUsed *tmp_ut_param_used_node = NULL;
  MY_MALLOC_AND_MEMSET(tmp_ut_param_used_node, sizeof(UserTraceParamUsed), "User Trace", -1);
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
  return tmp_ut_param_used_node;
}

/*This function is for adding parameter node into
*a linkedlist.
*Input : vptr_node and node to add*/
static void add_param_used_node(UserTracePageInfo *page_info_node, UserTraceParamUsed *param_used_node)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");

  // Add at the head if head is NULL else at the tail
  // Create the first node
  UserTraceParamUsed *tmp_param_used_node = page_info_node->param_used_head;

  if(page_info_node->param_used_head == NULL)
  {
    //For first parameter node
    page_info_node->param_used_head = param_used_node;
    page_info_node->param_used_head->next = NULL;

  }
  else
  {
    //Add another node from front
    page_info_node->param_used_head = param_used_node;
    page_info_node->param_used_head->next = tmp_param_used_node;
  }
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}


static UserTraceValidation *create_validation_node()
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  UserTraceValidation *tmp_ut_validation_node = NULL;
  MY_MALLOC_AND_MEMSET(tmp_ut_validation_node, sizeof(UserTraceValidation), "User Trace", -1);
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
  return tmp_ut_validation_node;
}

/*This function is for adding parameter node into 
*a linkedlist.
*Input : vptr_node and node to add*/
static void add_validation_node(UserTracePageInfo *page_info_node, UserTraceValidation *validation_node)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");

  // Add at the head if head is NULL else at the tail
  // Create the first node
  UserTraceValidation *tmp_validation_node = page_info_node->validation_head;
  
  if(page_info_node->validation_head == NULL)
  {
    //For first parameter node
    page_info_node->validation_head = validation_node;
    page_info_node->validation_head->next = NULL;
    
  }
  else
  {
    //Add another node from front
    page_info_node->validation_head = validation_node;
    page_info_node->validation_head->next = tmp_validation_node;
  }
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}


void ut_update_page_values (VUser *vptr, int page_resp_time, u_ns_ts_t now)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  //UserTraceNode *ut_node = (UserTraceNode *)user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  char xml_file_path [4048];
  userTraceData* utd_node = NULL;

  GET_UT_TAIL_PAGE_NODE

  page_node->page_info->page_status = vptr->page_status;
  page_node->page_info->page_rep_time = page_resp_time;
  //Page end time
  page_node->page_info->page_end_time = now;
  //if(!(vptr->flags & NS_PAGE_DUMP_ENABLE)) //If this node is not for Page dump
  if(NS_IF_TRACING_ENABLE_FOR_USER) //If this node is not for Page dump
    ut_save_xml(vptr, xml_file_path, 1);
}

void ut_update_url_values (connection *cptr, int cache_check)
{

  NSDL1_USER_TRACE(NULL, NULL, "Method called. ");

  VUser *vptr = (VUser*) cptr->vptr;
  char xml_file_path [4048];
  char *ptr;
  char full_url[MAX_LINE_LENGTH + 1] = "\0";
  int full_url_len = 0;
  int url_offset = 0;
  int out_len;

  userTraceData* utd_node = NULL;
  //UserTraceNode *page_node = (UserTraceNode *)user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  GET_UT_TAIL_PAGE_NODE

  if (page_node->page_info->req_url != NULL) return;

  {
    NSDL1_USER_TRACE(NULL, NULL, "full url = %s", cptr->url);
    get_abs_url_req_line(cptr->url_num, vptr, cptr->url, full_url, &full_url_len, &url_offset, MAX_LINE_LENGTH);

    page_node->page_info->req_url = ns_escape(full_url, full_url_len, &out_len, specified_char, NULL, 0);
    NSDL1_USER_TRACE(NULL, NULL, "After ns_escape full url = %s", page_node->page_info->req_url);
 
    ptr = proto_to_str(cptr->url_num->request_type);
    MY_MALLOC(page_node->page_info->protocol, strlen(ptr) + 1, "Protocol", -1);
    strcpy(page_node->page_info->protocol, ptr);
  }
  
  NSDL1_USER_TRACE(NULL, NULL, "cache_check = %d", cache_check);
  if(cache_check == CACHE_RESP_IS_FRESH)
  {
    // set respone is from cache
    page_node->page_info->taken_from_cache = 1;
  }

  //if(!(vptr->flags & NS_PAGE_DUMP_ENABLE)) //If this node is not for Page dump
  if(NS_IF_TRACING_ENABLE_FOR_USER) //If this node is not for Page dump
    ut_save_xml(vptr, xml_file_path, 1);
  
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting");
}

void ut_update_http_status_code (connection *cptr)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  VUser *vptr = (VUser*) cptr->vptr;

  //UserTraceNode *page_node = (UserTraceNode *)user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail;
  userTraceData* utd_node = NULL;
  GET_UT_TAIL_PAGE_NODE

  page_node->page_info->http_status_code = cptr->req_code;
}

void ut_free_all_nodes(userTraceData *utd_node)
{
  UserTraceNode *utd_node_tmp;
  UserTraceNode *node;
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
 if (utd_node != NULL) {
  NSDL1_USER_TRACE(NULL, NULL, "utd_node->ut_head = %p", utd_node->ut_head);
  while(utd_node->ut_head != NULL)
  { 
    node = (UserTraceNode *)utd_node->ut_head;
    utd_node->flags &=  ~NS_VUSER_TRACE_XML_WRITTEN; 
    utd_node_tmp = node->next;
    if(node->page_info != NULL)
    {
      while(node->page_info->param_head != NULL)
      {
        UserTraceParam *param_tmp = node->page_info->param_head->next;
        FREE_AND_MAKE_NULL_EX(node->page_info->param_head->name, strlen(node->page_info->param_head->name) + 1,"node->page_info->param_head->name", -1);
        FREE_AND_MAKE_NULL_EX(node->page_info->param_head->name, strlen(node->page_info->param_head->name) + 1,"node->page_info->param_head->name", -1);
        FREE_AND_MAKE_NULL_EX(node->page_info->param_head->value, strlen(node->page_info->param_head->value) + 1,"node->page_info->param_head-value", -1);
    
        FREE_AND_MAKE_NULL_EX(node->page_info->param_head, sizeof(UserTraceParam),"node->page_info->param_head", -1);
        node->page_info->param_head = param_tmp;
      }

      while(node->page_info->param_used_head != NULL)
      {
        UserTraceParamUsed *param_used_tmp = node->page_info->param_used_head->next;
        FREE_AND_MAKE_NULL_EX(node->page_info->param_used_head->name, strlen(node->page_info->param_used_head->name) + 1,"node->page_info->param_used_head->name", -1);
        FREE_AND_MAKE_NULL_EX(node->page_info->param_used_head->value, strlen(node->page_info->param_used_head->value) + 1,"node->page_info->param_used_head->value", -1);

        FREE_AND_MAKE_NULL_EX(node->page_info->param_used_head, sizeof(UserTraceParamUsed),"Freeing old_buffer", -1);
        node->page_info->param_used_head = param_used_tmp;
      }
   
      while(node->page_info->validation_head != NULL)
      {
        UserTraceValidation *validation_tmp = node->page_info->validation_head->next;
        FREE_AND_MAKE_NULL_EX(node->page_info->validation_head->detail, strlen(node->page_info->validation_head->detail) + 1,"node->page_info->validation_head->detail", -1);
        FREE_AND_MAKE_NULL_EX(node->page_info->validation_head->id, strlen(node->page_info->validation_head->id) + 1,"node->page_info->validation_head->id", -1);
        FREE_AND_MAKE_NULL_EX(node->page_info->validation_head->status, strlen(node->page_info->validation_head->status) + 1,"node->page_info->validation_head->status", -1);

        FREE_AND_MAKE_NULL_EX(node->page_info->validation_head, sizeof(UserTraceValidation),"node->page_info->validation_head", -1);
        node->page_info->validation_head = validation_tmp;
      } 

      FREE_AND_MAKE_NULL_EX(node->page_info->page_name, strlen(node->page_info->page_name) + 1,"node->page_info->page_name", -1);
      FREE_AND_MAKE_NULL_EX(node->page_info->flow_name, strlen(node->page_info->flow_name) + 1,"node->page_info->flow_name", -1);
      FREE_AND_MAKE_NULL_EX(node->page_info->req_url, strlen(node->page_info->req_url) + 1,"node->page_info->req_url", -1);
      FREE_AND_MAKE_NULL_EX(node->page_info->req, node->page_info->req_size + 1,"node->page_info->req", -1);
      FREE_AND_MAKE_NULL_EX(node->page_info->rep, node->page_info->rep_size + 1,"node->page_info->rep", -1);
      FREE_AND_MAKE_NULL_EX(node->page_info->rep_body, node->page_info->rep_body_size + 1,"node->page_info->rep_body", -1);
      FREE_AND_MAKE_NULL_EX(node->page_info->protocol, strlen(node->page_info->protocol) + 1,"node->page_info->protocol", -1);

      FREE_AND_MAKE_NULL_EX(node->page_info, sizeof(UserTracePageInfo),"node->page_info", -1);
      //UserTraceNode *node1 = ((UserTraceNode *) vptr->ut_head);
    }
    FREE_AND_MAKE_NULL_EX(node->start_time, strlen(node->start_time) + 1,"utd_node->ut_head->start_time", -1);
    FREE_AND_MAKE_NULL_EX(utd_node->ut_head, sizeof(UserTraceNode),"utd_node->ut_head", -1);

    utd_node->ut_head = utd_node_tmp;
  }
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting with utd_node->ut_head = %p", utd_node->ut_head);
 }
}

//Add a start session node
void ut_add_start_session_node(VUser *vptr)
{
  NSDL1_USER_TRACE(vptr, NULL, "Method called");
  UserTraceNode *node; 
  userTraceData* utd_node = NULL;
  char *start_time;
  char xml_file_path [4048];


  node = ut_alloc_node();
  /*TODO: Doing this for demo.
   * we will remove later*/ 

  GET_UTD_NODE

  NSDL1_USER_TRACE(vptr, NULL, "Method called curr_node_id = %d", utd_node->curr_node_id);
  if(utd_node->ut_head  != NULL)
  {
    NSDL4_USER_TRACE(vptr, NULL, "Already allocated user trace data");
    ut_free_all_nodes(utd_node);
  } 

  if(runprof_table_shr_mem[vptr->group_num].gset.vuser_trace_enabled == 2)
    utd_node->flags |= NS_VUSER_TRACE_XML_WRITTEN;
  
  utd_node->curr_node_id = 0;

  node->type = NS_UT_TYPE_START_SESSION;

  start_time = get_relative_time();
  MY_MALLOC(node->start_time, strlen(start_time) + 1, "User Trace start_time", -1);
  strcpy(node->start_time, start_time);
  //node->start_time = vptr->started_at; // Session start time taken from vptr
  
  node->sess_or_page_id = vptr->sess_ptr->sess_id;
  node->sess_or_page_inst = vptr->sess_inst;
  ut_add_node(vptr, node, "StartSession");

  //if(!(vptr->flags & NS_PAGE_DUMP_ENABLE)) //If this node is not for Page dump
  if(NS_IF_TRACING_ENABLE_FOR_USER) //If this node is not for Page dump
    ut_save_xml(vptr, xml_file_path, 1);
  NSDL1_USER_TRACE(vptr, NULL, "Method Exiting with curr_node_id = %d", utd_node->curr_node_id);
}


//Add a end session node
void ut_add_end_session_node(VUser *vptr)
{
  #ifdef NS_DEBUG_ON
  userTraceData* utd_node = NULL;
  GET_UTD_NODE
  //userTraceData *utd_node = user_trace_grp_ptr[vptr->group_num].utd_head;
  #endif

  NSDL1_USER_TRACE(vptr, NULL, "Method called vptr->sess_status = %d", vptr->sess_status);
  //utd_node->curr_node_id++;
  char xml_file_path [4048];
  char *start_time1;
  //ctrate a node
  UserTraceNode *node = ut_alloc_node();
  // Fill the values
  node->type = NS_UT_TYPE_END_SESSION;
   
  start_time1 = get_relative_time();
  MY_MALLOC(node->start_time, strlen(start_time1) + 1, "User Trace start_time", -1);
  strcpy(node->start_time, start_time1);
  node->sess_status = vptr->sess_status;
  
  //node->start_time = vptr->end_sess;
  //node->start_time = start_time;
  // Add at the appropriated location
  ut_add_node(vptr, node, "End session");
  //if(!(vptr->flags & NS_PAGE_DUMP_ENABLE)) //If this node is not for Page dump
  if(NS_IF_TRACING_ENABLE_FOR_USER) //If this node is not for Page dump
    ut_save_xml(vptr, xml_file_path, 1); 
  //utd_node->flags &=  ~NS_VUSER_TRACE_XML_WRITTEN;
  //ut_free_all_nodes(user_trace_grp_ptr[vptr->group_num].utd_head);
  NSDL1_USER_TRACE(vptr, NULL, "Method Exiting with curr_node_id = %d", utd_node->curr_node_id);
}
static char *make_data_file_name (char *data_file, int *data_file_id, int type)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called, data_file_id = %d", *data_file_id);
  if(type == 1)
  {
    (*data_file_id)++;
    NSDL1_USER_TRACE(NULL, NULL, "Method Colled with data_file_id = %d", *data_file_id);
    sprintf(data_file, "req_%d.log", *data_file_id);
  }
  else if (type == 0)
    sprintf(data_file, "rep_%d.log", *data_file_id);
  else
    sprintf(data_file, "rep_body_%d.log", *data_file_id);
  NSDL1_USER_TRACE(NULL, NULL, "Method exiting, data_file = %s", data_file);
  return data_file;
}
static void data_write_file (UserTraceNode *page_node, char *page_name, char *data_file, int data_file_id, char *xml_path, char *req_buff, int grp_num, int type)
{  
  //char *req_file; 
  char *tmp_ptr = NULL;
  char tmp_buf[8048];
  FILE *data_file_fp = NULL;
  char xml_path_lol [8048];
 
  userTraceData *utd_node = user_trace_grp_ptr[grp_num].utd_head;
  //UserTraceNode *ut_node = (UserTraceNode *)(utd_node)->ut_tail;
  //UserTraceNode *ut_node = page_node; 
  
  NSDL1_USER_TRACE(NULL, NULL, "Method called, Page name = %s", page_name);

  /*if(type == 1)
    sprintf(data_file, "req_%d.log", data_file_id);
  else if (type == 0)
    sprintf(data_file, "rep_%d.log", data_file_id);
  else
    sprintf(data_file, "rep_body_%d.log", data_file_id);*/

  //Condition1: we have already written and this node is last node then write req file
  //Condition2: Writting first time in req file

  if(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN)
  {
    NSDL1_USER_TRACE(NULL, NULL, "XML file written bit is set");
  }
  else
    NSDL1_USER_TRACE(NULL, NULL, "XML file written bit is not set");
  
  if (page_node->next == NULL)
  {
    NSDL1_USER_TRACE(NULL, NULL, "page_node->next is NULL");
  }
  else
   NSDL1_USER_TRACE(NULL, NULL, "page_node->next is not NULL");

  if(!(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN) || page_node->next == NULL)
  {
    //xml_path contain file anme also.
    //First remove file name from xml_path then save file
    strcpy(xml_path_lol, xml_path);
    tmp_ptr = strrchr(xml_path_lol, '/');
    *tmp_ptr = '\0';
    sprintf(tmp_buf, "%s/%s", xml_path_lol, data_file);
    NSDL1_USER_TRACE(NULL, NULL, "Req file path = %s", tmp_buf);
    data_file_fp = fopen(tmp_buf, "w");
    if(data_file_fp == NULL){
      //TODO: 
    }
    fprintf(data_file_fp, "%s", req_buff);
    fclose(data_file_fp);
  }
  //return data_file;
}

static int make_xml_file_path (VUser *vptr, char *xml_file_path){
  char grp_name[1024];
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
    //Create XML file path
  strcpy(grp_name, runprof_table_shr_mem[vptr->group_num].scen_group_name);

  sprintf(xml_file_path, "%s/logs/TR%d/vuser_trace/%s_%d_%u_%u/vuser_trace.xml", g_ns_wdir, testidx, grp_name, my_port_index, vptr->user_index, vptr->sess_inst);

    // TODO: Optmize so that mkdir is done on first start of trace for this session
  if(mkdir_ex(xml_file_path) == 0) // Error
  {
    fprintf(stderr, "Fail: Error in creating virutal user trace directory");
    return 1;
  }

  NSDL1_USER_TRACE(NULL, NULL, "Method exiting with path = %s", xml_file_path);
  return 0;
}

static int ut_save_xml(VUser *vptr, char *xml_file_path, int called_from)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");

  // First make directory for the user trace if dir is not existing 
  // Use system call (not command)
  // Dir = work_dir/logs/TR111/user_trace/
  // In a loop, save in XML
  // Open XML file
  // Loop
  
  //char xml_file_path[4048 + 1];
  FILE *xml_fp;
  int grp_num;
  int data_file_id = 0;

  grp_num = vptr->group_num;

  userTraceData *utd_node = NULL;
  GET_UTD_NODE   
  UserTraceNode *node = ((UserTraceNode *)(user_trace_grp_ptr[grp_num].utd_head)->ut_head);

  if(!(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN) && called_from == 1){
    NSDL1_USER_TRACE(NULL, NULL, "XML file never written. Once it is write by tool then it will get updated.");
    return 0;
  }

  if(make_xml_file_path (vptr, xml_file_path) == 1)
  {
    fprintf(stderr, "Fail: Error in creating virutal user trace directory");
    return -1;
  }
 //to handle multiple running <> cmd
 //If user hitting cmd multiple times then no need to 
  if(called_from == 0 && (utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN))
  {
    NSDL1_USER_TRACE(NULL, NULL, "called_from = 0 && NS_VUSER_TRACE_XML_WRITTEN is set");
    return 0;
  }
  
  if(node == NULL)
  {
    NSDL1_USER_TRACE(NULL, NULL, "UTD node is NULL");
    return -1;
  }

  {
    if(!(xml_fp = fopen(xml_file_path, "w+")))    
    {
      NS_EL_3_ATTR(EID_JS_EVALUATE_SCRIPT, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                 "Error in opeaning %s file\n", xml_file_path);

      fprintf(stderr,"Fail: Error in opeaning %s file\n", xml_file_path);
      return -1;
    }
    fprintf(xml_fp,"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    fprintf(xml_fp,"<UserTrace>\n");
    fprintf(xml_fp,"  <StartSession>\n");
    fprintf(xml_fp,"    <StartSessionTime>%s</StartSessionTime>\n", node->start_time);
    fprintf(xml_fp,"    <SessionID>%d:%d</SessionID>\n", my_port_index, vptr->sess_inst); 
    fprintf(xml_fp,"    <ScriptName>%s</ScriptName>\n", vptr->sess_ptr->sess_name); 
    fprintf(xml_fp,"  </StartSession>\n");
    node = node->next;
    if(node != NULL)//It can be last session or page node
    {
      fprintf(xml_fp,"\n  <Pages>\n");
      while(node->page_info != NULL )
      {
        UserTracePageInfo *node_page_info = node->page_info; 
        fprintf(xml_fp,"    <Page>\n");
        fprintf(xml_fp,"      <PageName>%s</PageName>\n", node_page_info->page_name);
        fprintf(xml_fp,"      <StartTime>%s</StartTime>\n", node->start_time);

        if(node_page_info->protocol)
          fprintf(xml_fp,"      <Protocol>%s</Protocol>\n", node_page_info->protocol?node_page_info->protocol:"NA");
        if (node_page_info->req_url)
          {
            fprintf(xml_fp,"      <RequestURL>%s</RequestURL>\n", node_page_info->req_url);
            fprintf(xml_fp,"      <InCache>%s</InCache>\n", node_page_info->taken_from_cache?"True":"False");
          }
        if((node_page_info->page_rep_time >= 0) && !node_page_info->taken_from_cache)
          fprintf(xml_fp,"      <ResponseTime>%'.3f sec</ResponseTime>\n", (double )(node_page_info->page_rep_time/1000.0));
        if((node_page_info->http_status_code >= 0) && !node_page_info->taken_from_cache)
          fprintf(xml_fp,"      <HTTPStatusCode>%d</HTTPStatusCode>\n", node_page_info->http_status_code);
        if(node_page_info->page_status >= 0)
          fprintf(xml_fp,"      <PageStatus>%s</PageStatus>\n", get_error_code_name(node_page_info->page_status));
        if(node_page_info->page_think_time >= 0)
          fprintf(xml_fp,"      <PageThinkTime>%'.3f sec</PageThinkTime>\n", (double)(node_page_info->page_think_time/1000.0));

          UserTraceParam *page_info_param = node_page_info->param_head;
          fprintf(xml_fp,"\n      <SearchParameters>\n");
          while(page_info_param != NULL)
          {
            fprintf(xml_fp,"        <SearchParameter>\n");
            fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param->name);
            fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param->value);
            fprintf(xml_fp,"          <ORD>%d</ORD>\n", page_info_param->ord);
            fprintf(xml_fp,"        </SearchParameter>\n");
            page_info_param = page_info_param->next;
          }
          fprintf(xml_fp,"      </SearchParameters>\n");
        
          UserTraceParamUsed *page_info_param_used = node_page_info->param_used_head;
          fprintf(xml_fp,"\n      <Parameterization>\n");
          while(page_info_param_used != NULL)
          {  
            fprintf(xml_fp,"        <Parameter>\n");
            if(page_info_param_used->type == VAR)
            {
              fprintf(xml_fp,"          <ParameterType>File</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == DATE_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Date Time</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == SEARCH_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Search</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == TAG_VAR)
            { 
              fprintf(xml_fp,"          <ParameterType>XML</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == INDEX_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Index</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == RANDOM_STRING)
            {
              fprintf(xml_fp,"          <ParameterType>Randam String</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == RANDOM_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Randam Number</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == UNIQUE_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Unique Number</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == NSL_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Declare Var</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == COOKIE_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Cookie Var</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == CLUST_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Clust Var</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == GROUP_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Group Var</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == GROUP_NAME_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Group Name Var</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == CLUST_NAME_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Clust Name Var</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == USERPROF_NAME_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>User Profile Name Var</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            else if(page_info_param_used->type == HTTP_VERSION_VAR)
            {
              fprintf(xml_fp,"          <ParameterType>Http Version</ParameterType>\n");
              fprintf(xml_fp,"          <Name>%s</Name>\n", page_info_param_used->name);
              fprintf(xml_fp,"          <Value>%s</Value>\n", page_info_param_used->value);
            }
            fprintf(xml_fp,"        </Parameter>\n");
            page_info_param_used = page_info_param_used->next; 
          }
          fprintf(xml_fp,"      </Parameterization>\n");
  
          UserTraceValidation *page_info_validation = node_page_info->validation_head;
          fprintf(xml_fp,"\n      <Validations>\n"); 
          while(page_info_validation != NULL)
          {
            fprintf(xml_fp,"        <Checkpoint>\n"); 
            fprintf(xml_fp,"          <Detail>Text=\"%s\"</Detail>\n", page_info_validation->detail);
            fprintf(xml_fp,"          <ID>%s</ID>\n", page_info_validation->id);
            fprintf(xml_fp,"          <Status>%s</Status>\n", page_info_validation->status);
            fprintf(xml_fp,"        </Checkpoint>\n");
            page_info_validation = page_info_validation->next;
          }
          fprintf(xml_fp,"      </Validations>\n");
      
        //data_file_id++;
        //rep_body_999999.log
        char data_file[19 + 1];//Will use in both req and respi
        if(called_from == 0)
          NSDL4_USER_TRACE(NULL, NULL, "called_from == 0");
        else
          NSDL4_USER_TRACE(NULL, NULL, "called_from == 1");
        if(!(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN))
          NSDL4_USER_TRACE(NULL, NULL, "(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN)is not set");
        else
          NSDL4_USER_TRACE(NULL, NULL, "(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN)is set");
  
        /*First time coming for writting the XML file
         * If first time is coming then req will be in buffer, so dump it into file and free all the buffer*/
        if(called_from == 0 && !(utd_node->flags & NS_VUSER_TRACE_XML_WRITTEN) && !node_page_info->taken_from_cache)
        {	
          if(node_page_info->req != NULL)
          {
            make_data_file_name(data_file, &data_file_id, 1);
            data_write_file(node, node_page_info->page_name, data_file, data_file_id, xml_file_path, node_page_info->req, grp_num, 1);
            fprintf(xml_fp,"      <Request>%s</Request>\n", data_file);
            FREE_AND_MAKE_NULL_EX(node->page_info->req, strlen(node->page_info->req) + 1,"(node->page_info->req", -1);
          }
          else 
            fprintf(xml_fp,"      <Request></Request>\n");
          /*
          if(node_page_info->rep != NULL)
          {
             make_data_file_name(data_file, data_file_id, 0);
            //data_write_file(node, node_page_info->page_name, data_file, data_file_id, xml_file_path, node_page_info->rep, grp_num, 0);
            //fprintf(xml_fp,"      <Response>%s</Response>\n", data_file);
            FREE_AND_MAKE_NULL_EX(node->page_info->req, strlen(node->page_info->req) + 1,"(node->page_info->rep", -1);
          }*/
        }
        else if ((node->type == NS_UT_TYPE_PAGE_REQ) && !node_page_info->taken_from_cache)
        {
          /*This is case when request is not from cache, and command is already ran. 
           *Now, we will just make the file name to write into XM file.
           * Data will be written into req file from the request update function*/
          NSDL4_USER_TRACE(NULL, NULL, "Else part");
          make_data_file_name(data_file, &data_file_id, 1);
          fprintf(xml_fp,"      <Request>%s</Request>\n", data_file);
        }
        else if (!node_page_info->taken_from_cache)
        { 
          /*This is case when we dont have reuest, Ex: TR069*/
          fprintf(xml_fp,"      <Request></Request>\n");
        }
        else 
          data_file_id++;

        if(node_page_info->rep_body != NULL)
        {
          make_data_file_name(data_file, &data_file_id, 2);
          data_write_file(node, node_page_info->page_name, data_file, data_file_id, xml_file_path, node_page_info->rep_body, grp_num, 2);
          fprintf(xml_fp,"      <ResponseBody>%s</ResponseBody>\n", data_file);
          FREE_AND_MAKE_NULL_EX(node->page_info->req, strlen(node->page_info->req) + 1, "node->page_info->req_body", -1);
        }
        else
          fprintf(xml_fp,"      <ResponseBody></ResponseBody>\n");


        node = node->next;
        fprintf(xml_fp,"    </Page>\n");
        if (!node)
        {
          NSDL4_USER_TRACE(NULL, NULL, "BREAK STATEMENT");
          break;
        }
      } //While
    fprintf(xml_fp,"  </Pages>\n");
    }
    
    fprintf(xml_fp,"\n  <EndSession>\n");
    if(node != NULL)
    { 
      fprintf(xml_fp,"    <SessionTime>%s</SessionTime>\n", node->start_time);
      fprintf(xml_fp,"    <SessionStatus>%s</SessionStatus>\n", node->sess_status == 0 ? "Success" : "Fail");
    }
    fprintf(xml_fp,"  </EndSession>\n");
    fprintf(xml_fp,"</UserTrace>\n");
    fclose(xml_fp);
  }
  
  utd_node->flags |= NS_VUSER_TRACE_XML_WRITTEN; 

  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
  return 0;
}

/*To make search parameter node*/
void ut_add_param_node (UserTraceNode *page_node)
{
  UserTracePageInfo *page_info_node = page_node->page_info;
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  
  UserTraceParam *param_node = create_param_node();
  add_param_node(page_info_node, param_node);
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}

static void ut_fill_used_param_node(UserTraceParamUsed *param_used_node, char *name, int name_len, int vector_var_idx, char *value, int value_len, int max_value_len, int type)
{
  NSDL1_USER_TRACE(NULL, NULL, "Method called");
  int len_to_copy; 
  int out_len = 0;
  char vector_var_idx_buf[32] = "";

  /*  In case of vector parameter we need to add index after param name 
   *  index can we greater than 0 or count */
 
  if(vector_var_idx > 0)
    sprintf(vector_var_idx_buf, "_%d", vector_var_idx);
  else if(vector_var_idx == -1)
    strcpy(vector_var_idx_buf, "_count");

  MY_MALLOC(param_used_node->name, name_len + strlen(vector_var_idx_buf) + 1, "User Trace param_used_name", -1);
  sprintf(param_used_node->name, "%s%s", name, vector_var_idx_buf);

  if(value_len > max_value_len)
    len_to_copy = max_value_len;
  else
    len_to_copy = value_len;
  len_to_copy = len_to_copy + 100 + 1; 
  //param_used_node->value = ns_escape(value, len_to_copy, &out_len, specified_char, NULL, 0);
  //In page dump redesign, special characters are encode using format $Cav_%XX parameter value 
  //is used in both page dump and vuser trace. Hence need to call copy_and_escape instead of ns_escape 
  MY_MALLOC(param_used_node->value, len_to_copy, "User Trace param_used_value", -1);
  out_len = copy_and_escape(value, value_len, param_used_node->value, len_to_copy);
  NSDL1_USER_TRACE(NULL, NULL, "out_len = %d", out_len);
  if(out_len > max_value_len)
    out_len = max_value_len;
  NSDL1_USER_TRACE(NULL, NULL, "After truncating out_len = %d", out_len);
  param_used_node->value[out_len] = '\0';
  
  param_used_node->type = type;

  NSDL1_USER_TRACE(NULL, NULL, "Method exiting");
}

/*To make other parameter node*/
void ut_add_param_used_node (VUser *vptr)
{
  //int grp_num = vptr->group_num;
  userTraceData* utd_node;
  
  GET_UTD_NODE   

  UserTracePageInfo *page_info_node = ((UserTraceNode *)utd_node->ut_tail)->page_info;
  
  char *name = NULL;;
  int name_len = 0;
  char *value = NULL;;
  int value_len = 0;

  int used_param_id;
  int vector_var_idx = 0;  //index is used for scalar variable example - search_var_1 then index = 1
  int num_entries;
  int used_param_type;
  /* max length of parameter value */
  int max_value_len = runprof_table_shr_mem[vptr->group_num].gset.max_trace_param_value_size;

  NSDL1_USER_TRACE(NULL, NULL, "Method called");

  if(vptr->url_num == NULL)
  {
    NSDL1_USER_TRACE(NULL, NULL, "vptr->url_num is NULL, so return from ut_add_param_used_node");
    return; 
  }

  num_entries = vptr->httpData->up_t.total_entries;

  for (used_param_id = 0; used_param_id < num_entries; used_param_id++) {
    used_param_type = get_parameter_name_value(vptr, used_param_id, &name, &name_len, &vector_var_idx,  &value, &value_len);
    if(used_param_type == -1){
      NSDL1_USER_TRACE(NULL, NULL, "used_param_type = %d", used_param_type);
      continue;   
    }
    if((name != NULL && name_len != 0) && (value != NULL && value_len != 0)){
      UserTraceParamUsed *param_used_node = create_param_used_node();
      ut_fill_used_param_node(param_used_node, name, name_len, vector_var_idx, value, value_len, max_value_len, used_param_type);
      NSDL4_USER_TRACE(vptr, NULL, "name = %*.*s and value = %*.*s", name_len, name_len, param_used_node->name, value_len, value_len, param_used_node->value);
      add_param_used_node(page_info_node, param_used_node);
    }
  }

  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}

/* To make validation node*/
void ut_add_validation_node (UserTraceNode *page_node)
{
  UserTracePageInfo *page_info_node = page_node->page_info;
  NSDL1_USER_TRACE(NULL, NULL, "Method called");

  UserTraceValidation *validation_node = create_validation_node();
  add_validation_node(page_info_node, validation_node);
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}

//update parameter values
void ut_update_param(UserTraceNode *page_node, char *name, char *value, int type, short ord, int name_len, int value_len)
{
  int out_len;
  int len_to_copy = 0;

  NSDL1_USER_TRACE(NULL, NULL, "Method called, name = %s value = %s type = %d, ord = %d page_node = %p page_name = %s", name, value, type, ord, page_node, page_node->page_info->page_name);

  MY_MALLOC(page_node->page_info->param_head->name, name_len + 1, "User Trace param_name", -1);
  strcpy(page_node->page_info->param_head->name, name);

  /*Dont need to malloc value, because ns_escape returns malloced buffer*/
  if(value_len > MAX_LEN_TO_COPY)
    len_to_copy = MAX_LEN_TO_COPY;
  else
    len_to_copy = value_len;
  page_node->page_info->param_head->value = ns_escape(value, len_to_copy, &out_len, specified_char, NULL, 0); 
  if(out_len > MAX_LEN_TO_COPY)
    out_len = MAX_LEN_TO_COPY;
  NSDL1_USER_TRACE(NULL, NULL, "After truncating out_len = %d", out_len);
  page_node->page_info->param_head->value[out_len] = '\0';
  page_node->page_info->param_head->type = type;
  page_node->page_info->param_head->ord = ord;
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}


void ut_update_validation(UserTraceNode *page_node, char *detail, char *id, char *status)
{
  int detail_len = strlen(detail); 
  int id_len = strlen(id);
  int status_len = strlen(status);
  int out_len;
  int len_to_copy; 
  NSDL1_USER_TRACE(NULL, NULL, "Method called, detail = %s id = %s status = %s, ord = %d", detail, id, status);

  if(detail_len > MAX_LEN_TO_COPY)
    len_to_copy = MAX_LEN_TO_COPY;
  else
    len_to_copy = detail_len;

  page_node->page_info->validation_head->detail = ns_escape(detail, len_to_copy, &out_len, specified_char, NULL, 0);
  if(out_len > MAX_LEN_TO_COPY)
    out_len = MAX_LEN_TO_COPY;
  NSDL1_USER_TRACE(NULL, NULL, "After truncating out_len = %d", out_len);
  page_node->page_info->validation_head->detail[out_len] = '\0';

  MY_MALLOC(page_node->page_info->validation_head->id, id_len + 1, "User Trace valid_id", -1);
  strcpy( page_node->page_info->validation_head->id, id);
  MY_MALLOC(page_node->page_info->validation_head->status, status_len + 1, "User Trace status_id", -1);
  strcpy( page_node->page_info->validation_head->status, status);
  NSDL4_USER_TRACE(NULL, NULL, "Method Exiting");
}

#if 0

int main()
{
 //while(1)
  {
    VUser vptr_buf, *vptr;

    vptr = &vptr_buf;
    memset(vptr, 0, sizeof(VUser));
  
    ut_add_start_session_node(vptr);
    ut_update_nvm_id_sess_id(vptr->ut_head, 2, 1234);
    ut_add_page_node(vptr);

    //Tail will always point to page node
    UserTraceNode *user_trace = ((UserTraceNode *) vptr->ut_tail);
    ut_update_page_name(user_trace, "page1");
    ut_update_script_name(user_trace, "tours");
    ut_save_xml(vptr);
    //getchar();
    ut_update_rep_time(user_trace, 123);
    ut_update_page_think_time(user_trace,456);
    ut_update_page_status(user_trace, 202);
    ut_update_http_status_code(user_trace, 404);
    //ut_update_req_url(vptr->ut_tail, "www.google.com"); 
    ut_update_req_url(user_trace, url);

    add_param_node (user_trace->page_info);
    //ut_update_param_type(vptr ,1);
    //ut_update_param_name(user_trace, "Search");
    ut_update_param_name(user_trace, param_name, SEARCH); 
    ut_update_param_value(user_trace, "0987");  

    add_param_node (user_trace->page_info);
    ut_update_param_name(user_trace, "File", STATIC);

    //ut_update_param_value(user_trace, "0987");
    ut_update_param_value(user_trace, param_value); 
    //ut_update_param_ord(vptr->ut_head,vptr->);

    ut_update_req(user_trace, req_log);
    ut_update_rep(user_trace, rep_log);
    ut_update_req_body(user_trace, rep_body_log);
    ut_update_protocol(user_trace, "FTP");

    ut_add_page_node(vptr);
    user_trace = ((UserTraceNode *) vptr->ut_tail);

    ut_update_page_name(user_trace, "page2");
    ut_save_xml(vptr);
    //getchar();

    ut_update_rep_time(user_trace, 123);
    ut_update_page_think_time(user_trace,456);
    ut_update_page_status(user_trace, 202);
    ut_update_http_status_code(user_trace, 404);
    //ut_update_req_url(vptr->ut_tail, "www.google.com");
    ut_update_req_url(user_trace, url);

    add_param_node (user_trace->page_info);
    //ut_update_param_type(vptr ,1);
    //ut_update_param_name(user_trace, "Search");
    ut_update_param_name(user_trace, param_name, SEARCH);
    //ut_update_param_value(user_trace, "0987");
    ut_update_param_value(user_trace, param_value);
    //ut_update_param_ord(vptr->ut_head,vptr->);

    add_param_node (user_trace->page_info);
    //ut_update_param_type(vptr ,1);
    ut_update_param_name(user_trace, "File", SEARCH);
    ut_update_param_value(user_trace, "0987");
    //ut_update_param_ord(vptr->ut_head,vptr->);

    ut_update_req(user_trace, req_log);
    ut_update_rep(user_trace, rep_log);
    ut_update_req_body(vptr->ut_tail, rep_body_log);

    ut_add_end_node(vptr);
  
    ut_save_xml(vptr);
    ut_free_all_nodes(vptr);
  }
  return 0;
}
#endif

// This method is used to add page type node which is not a page
// For example is TR069, it is used to add node for listen for RFC
void ut_add_internal_page_node(VUser *vptr, char *page_name, char *url, char *protocol, int page_think_time)
{
  char *start_time;

  NSDL2_USER_TRACE(vptr, NULL, "Method called, user = %p, page_name = %s, url = %s, protocol = %s, page_think_time = %d ms", vptr, page_name, url, protocol, page_think_time);

  //userTraceData* utd_node = user_trace_grp_ptr[grp_num].utd_head;
  //utd_node->curr_node_id++;
  //NSDL2_USER_TRACE(NULL, NULL, "Method called curr_node_id = %d", utd_node->curr_node_id);

  UserTraceNode *page_node = ut_alloc_node();
  page_node->type = NS_UT_TYPE_PAGE;

  //Creating a page information node
  page_node->page_info = create_page_info_node();

  MY_MALLOC(page_node->page_info->page_name, strlen(page_name) + 1, "User Trace page_name", -1);
  strcpy(page_node->page_info->page_name, page_name);
  
  MY_MALLOC(page_node->page_info->req_url, strlen(url) + 1, "User Trace URL", -1);
  strcpy(page_node->page_info->req_url, url);
  
  MY_MALLOC(page_node->page_info->protocol, strlen(protocol) + 1, "Protocol", -1);
  strcpy(page_node->page_info->protocol, protocol);
  
  start_time = get_relative_time();
  MY_MALLOC(page_node->start_time, strlen(start_time) + 1, "User Trace page start_time", -1);
  strcpy(page_node->start_time, start_time); 

  page_node->page_info->page_think_time = page_think_time; 

  ut_add_node(vptr, page_node, "Page");
  NSDL2_USER_TRACE(vptr, NULL, "Method Exiting"); 
}

void ut_update_page_think_time(VUser *vptr){

  NSDL2_USER_TRACE(vptr, NULL, "Method called, user = %p,  vptr->pg_think_time = %d ", vptr,  vptr->pg_think_time);

  userTraceData* utd_node = NULL;
  GET_UTD_NODE
  UserTraceNode *ut_tail = utd_node->ut_tail;
  //If we have page node then current page will be always on tail.
  NSDL2_USER_TRACE(vptr, NULL, "Method called, user = %p,  ut_tail->type = %d", vptr,  ut_tail->type);
  if(ut_tail->type == NS_UT_TYPE_PAGE || ut_tail->type == NS_UT_TYPE_PAGE_REQ)
  {
    ut_tail->page_info->page_think_time = vptr->pg_think_time; 
    NSDL2_USER_TRACE(vptr, NULL, "User = %p,  ut_tail->page_info->page_think_time = %d", vptr,  ut_tail->page_info->page_think_time);
  }
  else{
    NSDL2_USER_TRACE(vptr, NULL, "Current node %p is not page node. Skipping this page think time", ut_tail);
  }
  NSDL2_USER_TRACE(vptr, NULL, "Method Exiting");
}

//void ut_add_page_node(VUser *vptr, connection *cptr) {
void ut_add_page_node(VUser *vptr){
   char *start_time;
  //This vptr should be enabled for user trace
  NSDL2_USER_TRACE(vptr, NULL, "Method called, user = %p", vptr);
  //UserTraceNode *ut_tail = NULL;

  //char *flow_name = NULL;

  userTraceData* utd_node;

  //userTraceData* utd_node = user_trace_grp_ptr[grp_num].utd_head;
  GET_UTD_NODE
  utd_node->curr_node_id++;
  NSDL2_USER_TRACE(NULL, NULL, "Method called curr_node_id = %d", utd_node->curr_node_id);

  UserTraceNode *page_node = ut_alloc_node();
  page_node->type = NS_UT_TYPE_PAGE;
  page_node->sess_or_page_id = vptr->cur_page->page_id;
  page_node->sess_or_page_inst = vptr->page_instance;

  //Creating a page information node
  page_node->page_info = create_page_info_node();

  MY_MALLOC(page_node->page_info->page_name, strlen(vptr->cur_page->page_name) + 1, "User Trace page_name", -1);
  strcpy(page_node->page_info->page_name, vptr->cur_page->page_name);
  
  MY_MALLOC(page_node->page_info->flow_name, strlen(vptr->cur_page->flow_name?vptr->cur_page->flow_name:"NA") + 1, "User Trace flow name", -1);
  strcpy(page_node->page_info->flow_name, vptr->cur_page->flow_name?vptr->cur_page->flow_name:"NA");

  start_time = get_relative_time();
  MY_MALLOC(page_node->start_time, strlen(start_time) + 1, "User Trace page start_time", -1);
  strcpy(page_node->start_time, start_time); 
  //page_node->start_time = vptr->pg_begin_at;
  //Start time for page
  page_node->page_info->page_start_time = vptr->pg_begin_at;


  ut_add_node(vptr, page_node, "Page");
  NSDL2_USER_TRACE(vptr, NULL, "Method Exiting"); 
}


/*Function to initialize group user trace*/
void init_vuser_grp_data()
{
  NSDL2_USER_TRACE(NULL, NULL, "Method called, total group = %d, user trace size = %d", total_runprof_entries, sizeof(userTraceGroup));
  MY_MALLOC_AND_MEMSET(user_trace_grp_ptr, sizeof(userTraceGroup) * total_runprof_entries, "User trace data group allocated", 0);
  NSDL2_USER_TRACE(NULL, NULL, "Method exiting");
}

/**/
void init_user_tracing_data_and_add_in_list(VUser *vptr)
{
  userTraceData *tmp_user_trace_data_ptr = NULL;
  //userTraceData *tmp_user_trace_data_ptr_2 = NULL;
  int grp_num = vptr->group_num;

  NSDL2_USER_TRACE(vptr, NULL, "Method called");

  MY_MALLOC_AND_MEMSET(tmp_user_trace_data_ptr, sizeof(userTraceData) , "User trace data allocated", 0);

  tmp_user_trace_data_ptr->vptr = vptr;

  if(user_trace_grp_ptr[grp_num].utd_head == NULL)
  {
    user_trace_grp_ptr[grp_num].utd_head = tmp_user_trace_data_ptr;
    //user_trace_grp_ptr[grp_num].utd_head->next = NULL; For future
  }
  else
  {
    fprintf(stderr, "Error: User trace header node is not NULL. There is some problem. It should always NULl while caaling this method.\n");
    return;
  }
/*
  else // For future as we are doing only one user trace
  {
    //Add another node from front
    tmp_user_trace_data_ptr_2 = user_trace_grp_ptr[grp_num].utd_head;
    user_trace_grp_ptr[grp_num].utd_head = tmp_user_trace_data_ptr;
    user_trace_grp_ptr[grp_num].utd_head->next = tmp_user_trace_data_ptr_2;
  }
*/
  NSDL2_USER_TRACE(vptr, NULL, "Method Exiting");
}

void ut_vuser_check_and_enable_tracing(VUser *vptr)
{

   NSDL2_USER_TRACE(vptr, NULL, "Method called, user = %p, tracing is enabled for %d number of users and my_port_index = %d", vptr, user_trace_grp_ptr[vptr->group_num].num_users, my_port_index);

   //if(runprof_table_shr_mem[vptr->group_num].gset.vuser_trace_enabled)
   //{
     if((user_trace_grp_ptr[vptr->group_num].num_users == 0) && (my_port_index == 0))
     {
       vptr->flags |= NS_VUSER_TRACE_ENABLE;
       user_trace_grp_ptr[vptr->group_num].num_users++;
       init_user_tracing_data_and_add_in_list(vptr);
       inti_specified_char();
     }
     else
     {
       NSDL2_USER_TRACE(vptr, NULL, "Tracing already enabled for this group. Total enable users = %d", user_trace_grp_ptr[vptr->group_num].num_users);
       return;
     }
     NSDL2_USER_TRACE(vptr, NULL, "Method exiting with number of tracing enabled users = %d", user_trace_grp_ptr[vptr->group_num].num_users);
   //}
}

void ut_vuser_check_and_disable_tracing(VUser *vptr)
{

  NSDL2_USER_TRACE(vptr, NULL, "Method called");
  if(!(vptr->flags & NS_VUSER_TRACE_ENABLE))
  {
    NSDL2_USER_TRACE(vptr, NULL, "Tracing is not enabled for user");
    return;
  }
  NSDL2_USER_TRACE(vptr, NULL, "Tracing is enabled for user");
  vptr->flags = vptr->flags & ~NS_VUSER_TRACE_ENABLE;
  user_trace_grp_ptr[vptr->group_num].num_users--;
  //Currently we are passing utd_head coz assuming only one vuser.
  //In future we need to change it
  ut_free_all_nodes(user_trace_grp_ptr[vptr->group_num].utd_head);
  FREE_AND_MAKE_NULL_EX(user_trace_grp_ptr[vptr->group_num].utd_head, sizeof(userTraceData),"Freeing user_trace_grp_ptr[vptr->group_num].utd_head", -1);
  NSDL2_USER_TRACE(vptr, NULL, "Method Exiting");
}

void ut_send_reply_to_parent(User_trace *msg, int status, char *nvm_msg){
 User_trace msg_lol;

  NSDL2_USER_TRACE(NULL, NULL, "Method called");
 
  memcpy(&msg_lol, msg, sizeof(User_trace)); 
  msg_lol.opcode = VUSER_TRACE_REP;
  msg_lol.reply_status = status;
  sprintf(msg_lol.reply_msg, "%s", nvm_msg);
  send_child_to_parent_msg("NVM sending message to parent", (char *)&msg_lol, sizeof(User_trace), CONTROL_MODE);
  NSDL2_USER_TRACE(NULL, NULL, "Method Exiting");
}


/*This function is to start the user tracing for given group*/
void start_vuser_trace (User_trace *msg, u_ns_ts_t now)
{
  //char grp_name[256];
  int grp_idx;
  char msg_buf[4048];
  VUser *vptr = NULL;
  char xml_file_path[4048];
  //int len;

  NSDL2_USER_TRACE(NULL, NULL, "Method Called");

  grp_idx = msg->grp_idx;

  //Tracing is enabled for given grp
  //XML file path
  NSDL2_USER_TRACE(NULL, NULL, "Tracing is enabled for group = %d", grp_idx);
  
  if( user_trace_grp_ptr[grp_idx].utd_head == NULL)
  {
    sprintf(msg_buf, "FAIL: There is no active user in the netstorm for group_idx %d", grp_idx);
    ut_send_reply_to_parent(msg, VUSER_TRACE_SYS_ERR, msg_buf);
    //fprintf(stderr, "FAIL:  There is no active user in the netstorm for group_idx %d", grp_idx);
    return;
  }
  vptr = user_trace_grp_ptr[grp_idx].utd_head->vptr;
  if(ut_save_xml(vptr, xml_file_path, 0) < 0)
  {
    ut_send_reply_to_parent(msg, VUSER_TRACE_SYS_ERR, "FAIL: Error in creating virutal user trace xml file");
    //fprintf(stderr, "FAIL: Error in creating virutal user trace xml file");
    return;
  }

  sprintf(msg_buf, "PASS:XML_FILE=%s\n", xml_file_path);
  ut_send_reply_to_parent(msg, VUSER_TRACE_SUCCESS, msg_buf);
}

/* End: child code */

/* Start: Parent code                */

//G_VUSER_TRACE <grp> <mode>
int kw_set_g_user_trace(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  char cmode[MAX_DATA_LINE_LENGTH];
  cmode[0] = 0;
  int mode = 1;

  NSDL2_USER_TRACE(NULL, NULL, "Method called, buf = %s", buf);
  

  num = sscanf(buf, "%s %s %s %s", keyword, sgrp_name, cmode, tmp);
  NSDL2_USER_TRACE(NULL, NULL, "Method called, num= %d", num);

  if(num != 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_VUSER_TRACE_USAGE, CAV_ERR_1011016, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(cmode) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_VUSER_TRACE_USAGE, CAV_ERR_1011016, CAV_ERR_MSG_2);

  mode = atoi(cmode);
  if(mode < 0 || mode > 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_VUSER_TRACE_USAGE, CAV_ERR_1011016, CAV_ERR_MSG_3);
  }
  if(mode && (loader_opcode != STAND_ALONE))
  {
    NS_DUMP_WARNING("Virtual User Trace is not supported in Netcloud");
    mode = 0;
  }
  gset->vuser_trace_enabled = mode;
  
  NSDL2_USER_TRACE(NULL, NULL, "gset->user_trace_enabled = %d", gset->vuser_trace_enabled);
  return 0;
}

/*Function to send data to cmd.
This will be called by Parent*/
void send_msg_to_cmd(User_trace *vuser_trace_msg){
  User_trace vuser_trace_msg_lol;

  NSDL2_USER_TRACE(NULL, NULL, "Method called");
  
  memset(&vuser_trace_msg_lol, 0, sizeof(User_trace));
  memcpy(&vuser_trace_msg_lol, vuser_trace_msg, sizeof(User_trace));
  vuser_trace_msg_lol.opcode = VUSER_TRACE_REP;
  if (send(vuser_trace_msg->cmd_fd, &vuser_trace_msg_lol, sizeof(User_trace), 0) != sizeof(User_trace)) {
     fprintf(stderr, "Error: Unable to send data to command\n");
  }
  NSDL2_USER_TRACE(NULL, NULL, "Method exiting");
}
// This will send message to NVM 0 only for now
// as user trace will be enabled only in NVM 0
// This is called from parent on getting req from command
void process_vuser_tracing_req (int cmd_fd, User_trace *vuser_trace_msg)
{
  //int i, len;
  //char msg_buf[2048];
  int grp_idx;
  char grp_name[SGRP_NAME_MAX_LENGTH + 1];
  char *ip;
  int i;

  NSDL2_USER_TRACE(NULL, NULL, "Method called, group name = %s for control connection", vuser_trace_msg->grp_name);

  // This will be send back by NVM so that parent can send reply to cmd
  vuser_trace_msg->cmd_fd = cmd_fd; 
  strcpy(grp_name, vuser_trace_msg->grp_name);

  /*In case of netcloud, vuser trace is not supported in controller mode hence returning*/
  if(loader_opcode == MASTER_LOADER)
  {
    NSDL2_USER_TRACE(NULL, NULL, "loader_opcode = %d for control connection", loader_opcode);
    NSDL2_USER_TRACE(NULL, NULL, "Virtual User trace feature is not supported in controller mode for control connection. Hence returning.");
    vuser_trace_msg->reply_status = VUSER_TRACE_SYS_ERR;
    sprintf(vuser_trace_msg->reply_msg, "FAIL: Virtual User trace feature is not supported in controller mode for control connection.\n"); 
    // Send reply to cmd using cmd_fd
    send_msg_to_cmd(vuser_trace_msg);
    return;
  }

  // Validate that grp name is existing grp. If not send error
  grp_idx = find_sg_idx_shr(vuser_trace_msg->grp_name);
  if(grp_idx < 0)
  {
    NSDL2_USER_TRACE(NULL, NULL, "Scenario group is not valid for control connection. Group name = %s\n", grp_name);
    vuser_trace_msg->reply_status = VUSER_TRACE_INVALID_GRP;
    sprintf(vuser_trace_msg->reply_msg, "FAIL: Scenario group is not valid for control connection. Group name = %s\n", grp_name); 
    // Send reply to cmd using cmd_fd
    send_msg_to_cmd(vuser_trace_msg);
    return;
  }

  if(runprof_table_shr_mem[grp_idx].gset.vuser_trace_enabled == 0)
  {
    NSDL2_USER_TRACE(NULL, NULL, "User tracing is not enabled on %s group for control connection", grp_name);
    vuser_trace_msg->reply_status = VUSER_TRACE_INVALID_GRP;
    sprintf(vuser_trace_msg->reply_msg, "FAIL: Vuser tracing is not enabled for group = %s for control connection\n", grp_name); 
    // Send reply to cmd using cmd_fd
    send_msg_to_cmd(vuser_trace_msg);
    return;
  }

  vuser_trace_msg->grp_idx = grp_idx;
  
  // TODO: How to handle in master mode
  //if(loader_opcode == MASTER_LOADER) 

  //We are sending message only to NVM 0
  ip = nslb_sock_ntop((const struct sockaddr *)&v_port_table[0].sockname);
  NSDL2_USER_TRACE(NULL, NULL, "ip = %s and num of nvms = %d", ip, global_settings->num_process);
  for(i = 0; i < global_settings->num_process; i++ )
  {
    NSDL2_USER_TRACE(NULL, NULL, "g_msg_com_con[%d].ip = %s for control connection", i, g_msg_com_con[i].ip);
    if(strcmp(ip, g_msg_com_con[i].ip) == 0)
    {
      NSDL2_USER_TRACE(NULL, NULL, "writting msg at g_msg_com_con[%d].ip = %s, fd = %d for control connection", i, g_msg_com_con[i].ip, g_msg_com_con[i].fd);

      /* After sending the finish report NVM fd is close by parent */ 
      if(g_msg_com_con[i].fd == -1)
      {
        vuser_trace_msg->reply_status = VUSER_TRACE_NVM_OVER;
        sprintf(vuser_trace_msg->reply_msg, "FAIL: NVM0 is already finished for control connection, so we can not use Vuser Trace\n");
        send_msg_to_cmd(vuser_trace_msg);
        return;
      }
      vuser_trace_msg->msg_len = sizeof(User_trace) - sizeof(int);
      write_msg(&g_msg_com_con[i], (char *)vuser_trace_msg, sizeof(User_trace), 0, CONTROL_MODE);
      break;
    }
  }
  if(i == global_settings->num_process)
  {
    NSDL2_USER_TRACE(NULL, NULL, "Error in getting NVM0 in msg com structure for control connection");
    vuser_trace_msg->reply_status = VUSER_TRACE_SYS_ERR;
    sprintf(vuser_trace_msg->reply_msg, "System erorr: Error in getting NVM0 in msg com structure for control connection. group = %s\n", grp_name); 
    send_msg_to_cmd(vuser_trace_msg);
    return;
  }

}

// This is called from parent on getting rep from NVM
void process_vuser_tracing_rep (int child_fd, User_trace *vuser_trace_msg)
{
  NSDL2_USER_TRACE(NULL, NULL, "Method called, group name = %s for control connection", vuser_trace_msg->grp_name);

  send_msg_to_cmd(vuser_trace_msg);
  //write_msg(vuser_trace_msg, (char *)vuser_trace_msg, sizeof(User_trace), 0);
}

/* End: Parent code                */
