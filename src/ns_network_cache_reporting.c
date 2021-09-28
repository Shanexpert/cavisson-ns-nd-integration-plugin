/********************************************************************** 
 * File Name            : ns_network_cache_stats_reporting.c
 * Author(s)            : Naveen Raina, Abhishek Mittal
 * Date                 : 2 Apr 2013
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Parsing & Reporting Network Cache Stats
 *                        
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location> 
**********************************************************************/
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

#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <libgen.h>
#include <sys/stat.h>
#include <strings.h>

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
#include "ns_alloc.h"
#include "ns_http_hdr_states.h"
#include "ns_cache_include.h"
#include "ns_network_cache_reporting.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_group_data.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

#define NW_CACHE_ERROR                     -1
#define NW_CACHE_SUCCESS                    0


#define NW_CACHE_HDR_BUF_SIZE  512

int network_cache_stats_header_buf_len = 0;
char *network_cache_stats_header_buf_ptr = NULL;
int max_buf_len = 0;
NetworkCacheStatsAvgTime *local_nw_cache_avg = NULL;

//All methods to start with nw_cache_stats

//---------------- KEYWORD PARSING SECTION BEGINS------------------------------------------
int kw_set_g_enable_network_cache_stats(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char vendor[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  char network_cache_stats_mode[MAX_DATA_LINE_LENGTH];
  char consider_refresh_hit_as_hit_mode[MAX_DATA_LINE_LENGTH];
  network_cache_stats_mode[0] = 0;  //Default Mode is Disabled
  int mode = 0;

  NSDL2_REPORTING(NULL, NULL, "Method called, buf = %s", buf);

  strcpy(network_cache_stats_mode,"0");
  strcpy(consider_refresh_hit_as_hit_mode,"0");

  num = sscanf(buf, "%s %s %s %s %s %s", keyword, sgrp_name, network_cache_stats_mode, consider_refresh_hit_as_hit_mode, vendor, tmp);
  NSDL2_REPORTING(NULL, NULL, "Method called, num= %d , key=[%s], groupname=[%s], network_cache_stats_mode=[%s], consider_refresh_hit_as_hit_mode = [%s]", num, sgrp_name, network_cache_stats_mode, network_cache_stats_mode, consider_refresh_hit_as_hit_mode);

  if(num > 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_NETWORK_CACHE_STATS_USAGE, CAV_ERR_1011037, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(network_cache_stats_mode) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_NETWORK_CACHE_STATS_USAGE, CAV_ERR_1011037, CAV_ERR_MSG_2);

  mode = atoi(network_cache_stats_mode);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_NETWORK_CACHE_STATS_USAGE, CAV_ERR_1011037, CAV_ERR_MSG_3);
  }

  gset->enable_network_cache_stats = mode;

  mode = atoi(consider_refresh_hit_as_hit_mode);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_NETWORK_CACHE_STATS_USAGE, CAV_ERR_1011037, CAV_ERR_MSG_3);
  }

  gset->consider_refresh_hit_as_hit = mode;
  NSDL2_REPORTING(NULL, NULL, "gset->enable_network_cache_stats = %d, gset->consider_refresh_hit_as_hit = %d", gset->enable_network_cache_stats, gset->consider_refresh_hit_as_hit);
  return 0;
}
//---------------- KEYWORD PARSING SECTION ENDS ------------------------------------------




//---------------- FETCHING NETWORK CACHE HEADERS SECTION BEGINS ------------------------------------------
static void nw_cache_stats_read_header_file()
{
  FILE *fp = NULL; 
  char line[MAX_FILE_DATA_LENGTH];
  char file_name[MAX_FILE_DATA_LENGTH];
  char *ptr;

  NSDL2_REPORTING(NULL, NULL, "Method Called ");
  sprintf(file_name, "%s/sys/network_cache_stats_debug_headers.dat", g_ns_wdir);
  
  MY_MALLOC(network_cache_stats_header_buf_ptr, max_buf_len + NW_CACHE_HDR_BUF_SIZE + 1, "nw_cache_stats_hdr_buf allocated", -1);   // 1 for NULL
  max_buf_len += NW_CACHE_HDR_BUF_SIZE + 1;

  if((fp = fopen(file_name,"r")) != NULL)
  {
    while ((nslb_fgets(line, MAX_FILE_DATA_LENGTH, fp, 0) != NULL))
    {
      ptr = line;
      CLEAR_WHITE_SPACE(ptr); 
      CLEAR_WHITE_SPACE_FROM_END(ptr);
      NSDL2_REPORTING(NULL, NULL,"ptr=[%s]", ptr);      

      if(*ptr == '#' || *ptr == '\n')
      {
        NSDL2_REPORTING(NULL, NULL, "Line is commented so skipping it"); 
        continue;
      }

      char *tmp_ptr = NULL;

      if((tmp_ptr = strchr(ptr, '\r')) || (tmp_ptr = strchr(ptr, '\n')))
        *tmp_ptr = '\0';

      NSDL2_REPORTING(NULL, NULL, "ptr=[%s]", ptr);
      int line_len = strlen(ptr) + 1 + 1;//1 for \r, 1 for \n
      if(max_buf_len < (network_cache_stats_header_buf_len + line_len))
      {
        MY_REALLOC(network_cache_stats_header_buf_ptr, network_cache_stats_header_buf_len + line_len,"nw_cache_stats_hdr_buf realloced", -1);         
        max_buf_len += line_len;
      }
      network_cache_stats_header_buf_len += sprintf(network_cache_stats_header_buf_ptr + network_cache_stats_header_buf_len, "%s\r\n", ptr);
    }
  }
   
  if(!network_cache_stats_header_buf_len)
  {
    network_cache_stats_header_buf_len = sprintf(network_cache_stats_header_buf_ptr, "Pragma: akamai-x-cache-on, akamai-x-cache-remote-on, akamai-x-check-cacheable\r\n");
    NSDL2_REPORTING(NULL, NULL,"network_cache_stats_header_buf_ptr=[%s], pragma buf len =[%d]", network_cache_stats_header_buf_ptr, network_cache_stats_header_buf_len);
  }
}

//Either rename it to check & set
//Call it at init time
static inline void check_and_set_nw_cache_stats_enabled()
{
  NSDL2_REPORTING(NULL, NULL,"Method called.");
  int i;

  //Checks computation is made for proxy already set or not. If set, no further computation done, and network_cache_stats_flag value returned 
  NSDL2_REPORTING(NULL, NULL,"total_runprof_entries=%d", total_runprof_entries);
  for (i=0; i < total_runprof_entries; i++)
  {
    if(runProfTable[i].gset.enable_network_cache_stats == 1)
    {
      global_settings->protocol_enabled |= NETWORK_CACHE_STATS_ENABLED;
      NSDL2_REPORTING(NULL, NULL,"Setting the flag as network cache is enabled for a group=%d", runProfTable->group_num);
      break;
    }
  }
}

void network_cache_stats_init()
{
  check_and_set_nw_cache_stats_enabled();
  if(IS_NETWORK_CACHE_STATS_ENABLED)
    nw_cache_stats_read_header_file();
}

//---------------- FETCHING NETWORK CACHE HEADERS SECTION BEGINS ------------------------------------------


//---------------- GDF PARSING SECTION BEGINS ------------------------------------------
extern void fprint2f(FILE *fp1, FILE* fp2,char* format, ...);

/* Http Network Caching Graph info */
HttpNetworkCacheStats_gp *http_network_cache_stats_gp_ptr = NULL;
unsigned int http_network_cache_stats_gp_idx;

#ifndef CAV_MAIN
int g_network_cache_stats_avgtime_idx = -1;
NetworkCacheStatsAvgTime *network_cache_stats_avgtime = NULL; //used to fill network cache pointers 
#else
__thread int g_network_cache_stats_avgtime_idx = -1;
__thread NetworkCacheStatsAvgTime *network_cache_stats_avgtime = NULL; //used to fill network cache pointers 
#endif


inline void set_nw_cache_stats_avgtime_ptr() 
{
  NSDL2_GDF(NULL, NULL, "Method Called");

  if(IS_NETWORK_CACHE_STATS_ENABLED) {
    NSDL2_GDF(NULL, NULL, "HTTP Network Cache is enabled. g_network_cache_stats_avgtime_idx=%d", g_network_cache_stats_avgtime_idx);
   /* We have allocated average_time with the size of NetworkCacheStatsAvgTime
    * also now we can point that using g_network_cache_stats_avgtime_idx*/ 
    network_cache_stats_avgtime = (NetworkCacheStatsAvgTime*)((char *)average_time + g_network_cache_stats_avgtime_idx);
  } else {
    NSDL2_GDF(NULL, NULL, "HTTP Network Cache is disabled for all the groups.");
    network_cache_stats_avgtime = NULL;
  }

  NSDL2_GDF(NULL, NULL, "network_cache_stats_avgtime = %p", network_cache_stats_avgtime);
}


// Add size of Netwok Cache if it is enabled 
inline void update_nw_cache_stats_avgtime_size() 
{
 
  NSDL2_GDF(NULL, NULL, "Method Called, g_avgtime_size = %d, g_network_cache_stats_avgtime_idx = %d",
					  g_avgtime_size, g_network_cache_stats_avgtime_idx);
  
  if(IS_NETWORK_CACHE_STATS_ENABLED){
    NSDL2_GDF(NULL, NULL, "HTTP Network Cache is enabled.");
    g_network_cache_stats_avgtime_idx = g_avgtime_size;
    g_avgtime_size +=  sizeof(NetworkCacheStatsAvgTime);
  } else {
    NSDL2_GDF(NULL, NULL, "HTTP Network Cache is disabled.");
  }

  NSDL2_GDF(NULL, NULL, "After g_avgtime_size = %d, g_network_cache_stats_avgtime_idx = %d",
					  g_avgtime_size, g_network_cache_stats_avgtime_idx);
}



//---------------- GDF PARSING SECTION ENDS ------------------------------------------


//---------------- NW CACHE RESPONSE HEADERS PROCESSING SECTION BEGINS ------------------------------------------

void nw_cache_stats_headers_parse_set_value(char *nw_cache_stats_hdr, int nw_cache_stats_hdr_len, connection *cptr, unsigned int *nw_cache_stats_hdr_pkt)
{
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  int grp_idx = ((VUser *)(cptr->vptr))->group_num;
  NSDL2_REPORTING(vptr, cptr, "Method Called. nw_cache_stats_hdr=[%s], nw_cache_stats_hdr_len=%d", nw_cache_stats_hdr, nw_cache_stats_hdr_len);

  CLEAR_WHITE_SPACE(nw_cache_stats_hdr);

  if(!strncasecmp(nw_cache_stats_hdr, "TCP_HIT", 7))
    *nw_cache_stats_hdr_pkt |= TCP_HIT;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_MISS", 8))
    *nw_cache_stats_hdr_pkt |= TCP_MISS;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_REFRESH_HIT", 15))
  {
    if(!(runprof_table_shr_mem[grp_idx].gset.consider_refresh_hit_as_hit))
      *nw_cache_stats_hdr_pkt |= TCP_REFRESH_HIT_AS_HIT;
    else
      *nw_cache_stats_hdr_pkt |= TCP_REFRESH_HIT_AS_MISS; 
  }
  // Nishi:1/04/2016 6:22 PM 
  // Changes for the bug 15872, adding TCP_CLIENT_REFRESH_MISS, TCP_SWAPFAIL_MISS( Both are treated as TCP_REFRESH_MISS ) and TCP_OFFLINE_HIT(     This is treated as TCP_REFRESH_FAIL_HIT)
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_REFRESH_MISS", 16) || !strncasecmp(nw_cache_stats_hdr, "TCP_CLIENT_REFRESH_MISS", 23) || !strncasecmp(nw_cache_stats_hdr, "TCP_SWAPFAIL_MISS", 17))
    *nw_cache_stats_hdr_pkt |= TCP_REFRESH_MISS;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_REFRESH_FAIL_HIT", 20) || !strncasecmp(nw_cache_stats_hdr, "TCP_OFFLINE_HIT", 15))
    *nw_cache_stats_hdr_pkt |= TCP_REFRESH_FAIL_HIT;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_IMS_HIT", 11))
    *nw_cache_stats_hdr_pkt |= TCP_IMS_HIT;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_NEGATIVE_HIT", 16))
    *nw_cache_stats_hdr_pkt |= TCP_NEGATIVE_HIT;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_MEM_HIT", 11))
    *nw_cache_stats_hdr_pkt |= TCP_MEM_HIT;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_DENIED", 10))
    *nw_cache_stats_hdr_pkt |= TCP_DENIED;
  else if(!strncasecmp(nw_cache_stats_hdr, "TCP_COOKIE_DENY", 15))
    *nw_cache_stats_hdr_pkt |= TCP_COOKIE_DENY;
  else if(!strncasecmp(nw_cache_stats_hdr, "HIT", 3))
    *nw_cache_stats_hdr_pkt |= HIT_FROM_CLOUD_FRONT;
  else if(!strncasecmp(nw_cache_stats_hdr, "MISS", 4) || !strncasecmp(nw_cache_stats_hdr, "EXPIRED", 7) ||
          !strncasecmp(nw_cache_stats_hdr, "STALE", 5) || !strncasecmp(nw_cache_stats_hdr, "BYPASS", 6) ||
          !strncasecmp(nw_cache_stats_hdr, "REVALIDATED", 11) || !strncasecmp(nw_cache_stats_hdr, "UPDATING", 8) ||
          !strncasecmp(nw_cache_stats_hdr, "DYNAMIC", 7))
    *nw_cache_stats_hdr_pkt |= MISS_FROM_CLOUD_FRONT;
  else
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFORMATION, __FILE__, (char *)__FUNCTION__,
                                               "Received X-cache header with invalid value '%s'.", nw_cache_stats_hdr);
  }
}

inline void nw_cache_stats_save_partial_headers(char *header_buffer, connection *cptr, int length)
{
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  cptr_data_t *cptr_data = (cptr->cptr_data);

  NSDL3_REPORTING(vptr, cptr, "Method Called");

  int prev_length = cptr_data->use_len;
  cptr_data->use_len += length; 
  if(cptr_data->use_len > cptr_data->len)
  {
    cptr_data->len = cptr_data->use_len;
    MY_REALLOC_EX(cptr_data->buffer, cptr_data->len, prev_length, "Set Partial Header Line", -1);
  }

  strncpy((char *)cptr_data->buffer + prev_length, header_buffer, length);
  NSDL2_REPORTING(vptr, cptr, "Exiting Method = %*.*s", cptr_data->use_len, cptr_data->use_len, cptr_data->buffer);  
}

inline void nw_cache_stats_free_partial_hdr(connection *cptr)
{
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  cptr_data_t *cptr_data = (cptr->cptr_data);

  NSDL2_REPORTING(vptr, cptr, "Method Called");

  cptr_data->use_len = 0; // is used to keep length of header
  // We are not freeing it here a we an reuse in this session
  NSDL2_REPORTING(vptr, cptr, "Exiting Method");
}

inline void  proc_nw_cache_stats_hdr(connection *cptr, char *header_buffer, int header_buffer_len,
                                             int *consumed_bytes, u_ns_ts_t now, unsigned int *nw_cache_stats_state)
{
  char *header_end;
  int length;
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  
  NSDL2_REPORTING(vptr, cptr, "Method Called");

  cptr_data_t *cptr_data = (cptr->cptr_data);

  //Check for end of the line is received
  if((header_end = memchr(header_buffer, '\r', header_buffer_len)))
  {  
    length = header_end - header_buffer; // Length is without \r

    //A. Replacing \r with \0 to identify the end-of-the-header-line
    //for facilitating the string search and string copy functions,
    //as they operate on \0
    header_buffer[length] = 0;

    // Case 1 - Complete header line received in one read
    if(cptr_data->use_len == 0)
    {
      NSDL3_REPORTING(vptr, cptr, "Complete header line received in one read");
      // +1 in lenght is so that we send NULL termination also
      nw_cache_stats_headers_parse_set_value(header_buffer, length, cptr, nw_cache_stats_state);
    }

    // Case 2 - Complete line received now and was partially received earlier
    else
    {
      NSDL3_REPORTING(vptr, cptr, "Complete header line received now and it was partial earlier");
      nw_cache_stats_save_partial_headers(header_buffer, cptr, length);
      nw_cache_stats_headers_parse_set_value(cptr_data->buffer, cptr_data->use_len, cptr, nw_cache_stats_state);
      nw_cache_stats_free_partial_hdr(cptr);
    }
   
    *consumed_bytes = length;   // add 1 as we have checked till /r
    cptr->header_state = HDST_CR; // Complete header line processed. Set state to CR to parse next header line
    //Reverting \0 back with \r for reasons mentioned in (A) above
    header_buffer[length] = '\r';
  } 
  else
  {
    // Case 3 - Parital header line received add_comma time
    if(cptr_data->use_len == 0)
    {
      NSDL3_REPORTING(vptr, cptr, "Partial header line received add_comma time");
    }
    // Case 4 - Parital header line received and was partially received earlier  
    else
    {
      NSDL3_REPORTING(vptr, cptr, "Partial header line received and was partially received earlier");
    }

    nw_cache_stats_save_partial_headers(header_buffer, cptr, header_buffer_len);
    *consumed_bytes = header_buffer_len; // All bytes are consumed
  }
  NSDL2_REPORTING(vptr, cptr, "Exiting Method, consumed_bytes = %d", *consumed_bytes);
}


int proc_http_hdr_x_cache(void *cur_cptr, char *header_buffer, int header_buffer_len,
                                             int *consumed_bytes, u_ns_ts_t now)
{
  connection *cptr = (connection *)cur_cptr;
  NSDL2_REPORTING(NULL, cptr, "Method Called");

  *consumed_bytes = 0; // Set to 0 in case of header is to be ignored
  if(!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.enable_network_cache_stats)
  {
    cptr->header_state = HDST_TEXT;
    NSDL2_REPORTING(NULL, cptr, "Ignoring Network cache Header as newtwork cache is not enable");
    return NW_CACHE_SUCCESS; // Must return 0
  }  

  if(!cptr->cptr_data)
  {
    MY_MALLOC(cptr->cptr_data, sizeof(cptr_data_t), "cptr data", -1); 
    memset(cptr->cptr_data, 0, sizeof(cptr_data_t));
  }

  //Network_cache *nw_cache = &(cptr->cptr_data->nw_cache);
  proc_nw_cache_stats_hdr(cptr, header_buffer, header_buffer_len, consumed_bytes, now, &(cptr->cptr_data->nw_cache_state));
  return NW_CACHE_SUCCESS;
}

int proc_http_hdr_x_cache_remote(void *cur_cptr, char *header_buffer, int header_buffer_len,
                                             int *consumed_bytes, u_ns_ts_t now)
{
  connection *cptr = (connection *)cur_cptr;
  NSDL2_REPORTING(NULL, cptr, "Method Called");

  *consumed_bytes = 0; // Set to 0 in case of header is to be ignored
  if(!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.enable_network_cache_stats)
  {
    cptr->header_state = HDST_TEXT;
    NSDL2_REPORTING(NULL, cptr, "Ignoring Network cache Header as newtwork cache is not enable");
    return NW_CACHE_SUCCESS; // Must return 0
  }  

  if(!cptr->cptr_data)
  {
    MY_MALLOC(cptr->cptr_data, sizeof(cptr_data_t), "cptr data", -1); 
    memset(cptr->cptr_data, 0, sizeof(cptr_data_t));
  }

  //Network_cache *nw_cache = &(cptr->cptr_data->nw_cache);
  proc_nw_cache_stats_hdr(cptr, header_buffer, header_buffer_len, consumed_bytes, now, &(cptr->cptr_data->nw_cache_state));
  return NW_CACHE_SUCCESS;
}


void nw_cache_stats_chk_cacheable_header_parse_set_val(char *nw_cache_stats_hdr, int nw_cache_stats_hdr_len, connection *cptr)
{
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  //Network_cache *nw_cache = &(cptr->cptr_data->nw_cache);
  unsigned int *nw_cache_stats_state = &(cptr->cptr_data->nw_cache_state);

  NSDL2_REPORTING(vptr, cptr, "Method Called. nw_cache_stats_hdr=[%s], nw_cache_stats_hdr_len=%d", nw_cache_stats_hdr, nw_cache_stats_hdr_len);

  CLEAR_WHITE_SPACE(nw_cache_stats_hdr);

  if(!strcasecmp(nw_cache_stats_hdr, "YES"))
    *nw_cache_stats_state |= NW_CACHE_STATS_CACHEABLE_YES;
  else if(!strcasecmp(nw_cache_stats_hdr, "NO"))
    *nw_cache_stats_state |= NW_CACHE_STATS_CACHEABLE_NO;
  else
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFORMATION, __FILE__, (char *)__FUNCTION__,
                                               "Received X-Check-Cacheable header with invalid value '%s'.", nw_cache_stats_hdr);
}

int proc_http_hdr_x_check_cacheable(void *cur_cptr, char *header_buffer, int header_buffer_len,
                                             int *consumed_bytes, u_ns_ts_t now)
{
  char *header_end;
  int length;
  connection *cptr = (connection *)cur_cptr;
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  
  NSDL2_REPORTING(vptr, cptr, "Method Called");

  *consumed_bytes = 0; // Set to 0 in case of header is to be ignored
  if(!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.enable_network_cache_stats)
  {
    cptr->header_state = HDST_TEXT;
    NSDL2_REPORTING(vptr, cptr, "Ignoring Network cache Header as newtwork cache is not enable");
    return NW_CACHE_SUCCESS; // Must return 0
  }  

  if(!cptr->cptr_data)
  {
    MY_MALLOC(cptr->cptr_data, sizeof(cptr_data_t), "cptr data", -1); 
    memset(cptr->cptr_data, 0, sizeof(cptr_data_t));
  }
  cptr_data_t *cptr_data = (cptr->cptr_data);

  //Check for end of the line is received
  if((header_end = memchr(header_buffer, '\r', header_buffer_len)))
  {  
    length = header_end - header_buffer; // Length is without \r

    //A. Replacing \r with \0 to identify the end-of-the-header-line
    //for facilitating the string search and string copy functions,
    //as they operate on \0
    header_buffer[length] = 0;

    // Case 1 - Complete header line received in one read
    if(cptr_data->use_len == 0)
    {
      NSDL3_REPORTING(vptr, cptr, "Complete header line received in one read");
      // +1 in lenght is so that we send NULL termination also
      nw_cache_stats_chk_cacheable_header_parse_set_val(header_buffer, length, cptr);
    }

    // Case 2 - Complete line received now and was partially received earlier
    else
    {
      NSDL3_REPORTING(vptr, cptr, "Complete header line received now and it was partial earlier");
      nw_cache_stats_save_partial_headers(header_buffer, cptr, length);
      nw_cache_stats_chk_cacheable_header_parse_set_val(cptr_data->buffer, cptr_data->use_len, cptr);
      nw_cache_stats_free_partial_hdr(cptr);
    }
   
    *consumed_bytes = length;   // add 1 as we have checked till /r
    cptr->header_state = HDST_CR; // Complete header line processed. Set state to CR to parse next header line
    //Reverting \0 back with \r for reasons mentioned in (A) above
    header_buffer[length] = '\r';
  } 
  else
  {
    // Case 3 - Parital header line received add_comma time
    if(cptr_data->use_len == 0)
    {
      NSDL3_REPORTING(vptr, cptr, "Partial header line received add_comma time");
    }
    // Case 4 - Parital header line received and was partially received earlier  
    else
    {
      NSDL3_REPORTING(vptr, cptr, "Partial header line received and was partially received earlier");
    }

    nw_cache_stats_save_partial_headers(header_buffer, cptr, header_buffer_len);
    *consumed_bytes = header_buffer_len; // All bytes are consumed
  }
  NSDL2_REPORTING(vptr, cptr, "Exiting Method, consumed_bytes = %d", *consumed_bytes);
  return NW_CACHE_SUCCESS;
}

/* -                                  -- */
#ifdef NS_DEBUG_ON
static char *cache_stats_to_str(NetworkCacheStatsAvgTime *nw_cache_stats_avgtime_ptr, char *debug_buf)
{
  sprintf(debug_buf, "Req = %d, No NetCache Req = %d, Failed req = %d, "
                              "Others req = %d, Non Cacheable req = %d, Cacheable Req = %d, "
                              "Hits = %d, Misses = %d, Resp Time Miss( Min=%'.3f, Max=%'.3f, Tot=%'.3f ), "
                              "Resp Time Hit(Min=%'.3f, Max=%'.3f, Tot=%'.3f), Cacheable Throughput=%'.3f,"
                              "Non Cacheable Throughput=%'.3f," 
                              "TCP_REFRESH_HITS=%d", 
                     nw_cache_stats_avgtime_ptr->network_cache_stats_probe_req, nw_cache_stats_avgtime_ptr->non_network_cache_used_req, 
                     nw_cache_stats_avgtime_ptr->network_cache_stats_num_fail, nw_cache_stats_avgtime_ptr->network_cache_stats_state_others, 
                     nw_cache_stats_avgtime_ptr->num_non_cacheable_requests, nw_cache_stats_avgtime_ptr->num_cacheable_requests,
                     nw_cache_stats_avgtime_ptr->network_cache_stats_num_hits, nw_cache_stats_avgtime_ptr->network_cache_stats_num_misses, (double) (nw_cache_stats_avgtime_ptr->network_cache_stats_miss_response_time_min)/1000.0, (double) (nw_cache_stats_avgtime_ptr->network_cache_stats_miss_response_time_max)/1000.0, (double) (nw_cache_stats_avgtime_ptr->network_cache_stats_miss_response_time_total)/1000.0, (double) (nw_cache_stats_avgtime_ptr->network_cache_stats_hits_response_time_min)/1000.0, (double) (nw_cache_stats_avgtime_ptr->network_cache_stats_hits_response_time_max)/1000.0, (double) (nw_cache_stats_avgtime_ptr->network_cache_stats_hits_response_time_total)/1000.0, convert_8B_bytes_Kbps(nw_cache_stats_avgtime_ptr->content_size_recv_from_cache) ,convert_8B_bytes_Kbps(nw_cache_stats_avgtime_ptr->content_size_not_recv_from_cache), nw_cache_stats_avgtime_ptr->network_cache_refresh_hits);

  return (debug_buf);
}
#endif

void print_nw_cache_stats_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg) 
{

  double network_cache_stats_req_rate = 0;
  double non_network_cache_used_req = 0;
  double network_cache_stats_num_hits = 0;
  double network_cache_stats_num_misses = 0;

  double network_cache_stats_num_fail = 0;
  double network_cache_stats_state_others = 0;
  double num_cacheable_requests = 0;
  double num_non_cacheable_requests = 0;

  double network_cache_stats_hits_response_time_max = 0;
  double network_cache_stats_hits_response_time_min = 0;
  double network_cache_stats_hits_response_time_avg = 0;

  double network_cache_stats_miss_response_time_max = 0;
  double network_cache_stats_miss_response_time_min = 0;
  double network_cache_stats_miss_response_time_avg = 0;

  double content_size_recv_from_cache = 0;
  double content_size_not_recv_from_cache = 0; 

  double network_cache_refresh_hits= 0; 

  NetworkCacheStatsAvgTime* network_cache_stats_avgtime_local = NULL;

  NSDL2_GDF(NULL, NULL, "Method Called,is_periodic = %d", is_periodic);
 
  network_cache_stats_avgtime_local = (NetworkCacheStatsAvgTime*)((char*)avg + g_network_cache_stats_avgtime_idx);

  // For debug logging only
#ifdef NS_DEBUG_ON
  char debug_buf[4096];
  NSDL2_GDF(NULL, NULL, "Parent: NetworkCacheStats = %s", cache_stats_to_str(network_cache_stats_avgtime_local, debug_buf)); 
#endif

  network_cache_stats_req_rate = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->network_cache_stats_probe_req);
   // network_cache_stats_req_rate = (network_cache_stats_avgtime_local->network_cache_stats_probe_req * 1000)/(double)global_settings->progress_secs;
  non_network_cache_used_req = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->non_network_cache_used_req);

  network_cache_stats_num_hits = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->network_cache_stats_num_hits);
  network_cache_stats_num_misses = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->network_cache_stats_num_misses);
  network_cache_stats_num_fail = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->network_cache_stats_num_fail);
  network_cache_stats_state_others = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->network_cache_stats_state_others);

  num_cacheable_requests = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->num_cacheable_requests);
  num_non_cacheable_requests = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->num_non_cacheable_requests);


   // time_total has total time in milli-seconds. So we need to divide by num completed * 1000 to convert to avg in seconds
  NSDL4_GDF(NULL, NULL, "network_cache_stats_hits_response_time_total=%ld, network_cache_stats_hits_response_time_min=%d", network_cache_stats_avgtime_local->network_cache_stats_hits_response_time_total, network_cache_stats_avgtime_local->network_cache_stats_hits_response_time_min);

  if(network_cache_stats_avgtime_local->network_cache_stats_num_hits)
  {
     // Min and max are in milli-seconds. So we need to divide by 1000 to convert to seconds
    network_cache_stats_hits_response_time_min = (network_cache_stats_avgtime_local->network_cache_stats_hits_response_time_min == MAX_VALUE_4B_U)?0:convert_sec((double)network_cache_stats_avgtime_local->network_cache_stats_hits_response_time_min);

    network_cache_stats_hits_response_time_max = convert_sec((double)network_cache_stats_avgtime_local->network_cache_stats_hits_response_time_max);
     // We are not casting to double  in division as we can live with sub-ms time
    network_cache_stats_hits_response_time_avg = convert_sec((double )(network_cache_stats_avgtime_local->network_cache_stats_hits_response_time_total)/(network_cache_stats_avgtime_local->network_cache_stats_num_hits));

    NSDL4_GDF(NULL,NULL, "network_cache_stats_hits_response_time_avg = %f", network_cache_stats_hits_response_time_avg);
  }


   // time_total has total time in milli-seconds. So we need to divide by 1000 to convert to avg in seconds
  if(network_cache_stats_avgtime_local->network_cache_stats_num_misses)
  {
    // Min and max are in milli-seconds. So we need to divide by 1000 to convert to seconds
    network_cache_stats_miss_response_time_min = (network_cache_stats_avgtime_local->network_cache_stats_miss_response_time_min == MAX_VALUE_4B_U)?0:convert_sec((double)network_cache_stats_avgtime_local->network_cache_stats_miss_response_time_min);

    network_cache_stats_miss_response_time_max = convert_sec((double)network_cache_stats_avgtime_local->network_cache_stats_miss_response_time_max);
    network_cache_stats_miss_response_time_avg = convert_sec((double)(network_cache_stats_avgtime_local->network_cache_stats_miss_response_time_total)/(network_cache_stats_avgtime_local->network_cache_stats_num_misses));

    NSDL2_GDF(NULL,NULL, "network_cache_stats_miss_response_time_avg = %f", network_cache_stats_miss_response_time_avg);
  }

  content_size_recv_from_cache = convert_8B_bytes_Kbps(network_cache_stats_avgtime_local->content_size_recv_from_cache);
  content_size_not_recv_from_cache = convert_8B_bytes_Kbps(network_cache_stats_avgtime_local->content_size_not_recv_from_cache);

  network_cache_refresh_hits = convert_long_data_to_ps_long_long(network_cache_stats_avgtime_local->network_cache_refresh_hits);

  fprint2f(fp1, fp2, "    Network Cache rate (per sec): Total=%'.3f (No NetCache Used=%'.3f, Cacheable=%'.3f, Non-Cacheable=%'.3f, Failures=%'.3f, Others=%'.3f)\n"
                     "    Network Cache rate (per sec): (Hits=%'.3f Misses=%'.3f )\n"
                     "    Network Cache Time (Sec): Cache-Hit: min %'.3f  avg %'.3f max %'.3f  Cache-Miss: min %'.3f avg %'.3f max %'.3f\n"
                     "    Network Cache Throughput (Kbps): From Cache=%'.3f, Not from cache=%'.3f\n"
                     "    Network Cache rate (per sec): (TCP_REFRESH_HITS =%'.3f )\n",
	                 network_cache_stats_req_rate, non_network_cache_used_req, num_cacheable_requests, num_non_cacheable_requests,  
	                 network_cache_stats_num_fail,  network_cache_stats_state_others, network_cache_stats_num_hits, network_cache_stats_num_misses,
                         network_cache_stats_hits_response_time_min, network_cache_stats_hits_response_time_avg, network_cache_stats_hits_response_time_max ,
                         network_cache_stats_miss_response_time_min, network_cache_stats_miss_response_time_avg, network_cache_stats_miss_response_time_max,
                         content_size_recv_from_cache, content_size_not_recv_from_cache, network_cache_refresh_hits);

}

inline void fill_nw_cache_stats_gp (avgtime **g_avg) 
{
  int g_idx = 0, v_idx, grp_idx;
  NetworkCacheStatsAvgTime *network_cache_stats_avg = NULL;
  Long_data network_cache_stats_hits_response_time_avg=0;
  Long_data network_cache_stats_miss_response_time_avg=0;
  avgtime *avg = NULL;

  if(http_network_cache_stats_gp_ptr == NULL) 
    return;

  HttpNetworkCacheStats_gp *http_network_cache_stats_local_gp_ptr = http_network_cache_stats_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      network_cache_stats_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx);
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->network_cache_stats_probe_req)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_probe_req_ps); g_idx++;

      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->non_network_cache_used_req)),
                           http_network_cache_stats_local_gp_ptr->non_network_cache_used_req_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->num_cacheable_requests)),
                           http_network_cache_stats_local_gp_ptr->num_cacheable_requests_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->num_non_cacheable_requests)),
                           http_network_cache_stats_local_gp_ptr->num_non_cacheable_requests_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->network_cache_stats_num_hits)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_num_hits_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->network_cache_stats_num_misses)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_num_misses_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->network_cache_stats_num_fail)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_num_fail_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->network_cache_stats_state_others)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_state_others_ps); g_idx++;


      if(network_cache_stats_avg->network_cache_stats_num_hits)
      {
        network_cache_stats_hits_response_time_avg = (Long_data )network_cache_stats_avg->network_cache_stats_hits_response_time_total/((Long_data )network_cache_stats_avg->network_cache_stats_num_hits * 1000.0);
      }
      network_cache_stats_avg->network_cache_stats_hits_response_time_min = (network_cache_stats_avg->network_cache_stats_hits_response_time_min);

      GDF_COPY_TIMES_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                                 network_cache_stats_hits_response_time_avg, network_cache_stats_avg->network_cache_stats_hits_response_time_min/1000.0, network_cache_stats_avg->network_cache_stats_hits_response_time_max/1000.0, network_cache_stats_avg->network_cache_stats_num_hits,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_hits_response_time.avg_time,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_hits_response_time.min_time,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_hits_response_time.max_time,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_hits_response_time.succ); g_idx++;

      if(network_cache_stats_avg->network_cache_stats_num_misses)
        network_cache_stats_miss_response_time_avg = (Long_data )network_cache_stats_avg->network_cache_stats_miss_response_time_total/((Long_data )network_cache_stats_avg->network_cache_stats_num_misses * 1000.0);
     network_cache_stats_avg->network_cache_stats_miss_response_time_min = (network_cache_stats_avg->network_cache_stats_miss_response_time_min);


      GDF_COPY_TIMES_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                                 network_cache_stats_miss_response_time_avg, network_cache_stats_avg->network_cache_stats_miss_response_time_min/1000.0, network_cache_stats_avg->network_cache_stats_miss_response_time_max/1000.0, network_cache_stats_avg->network_cache_stats_num_misses,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_miss_response_time.avg_time,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_miss_response_time.min_time,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_miss_response_time.max_time,
                                 http_network_cache_stats_local_gp_ptr->network_cache_stats_miss_response_time.succ); g_idx++;


      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_8B_bytes_Kbps((network_cache_stats_avg->content_size_recv_from_cache)),
                           http_network_cache_stats_local_gp_ptr->content_size_recv_from_cache_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_8B_bytes_Kbps((network_cache_stats_avg->content_size_not_recv_from_cache)),
                           http_network_cache_stats_local_gp_ptr->content_size_not_recv_from_cache_ps); g_idx++;


      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           ((double)(network_cache_stats_avg->network_cache_stats_num_hits * 100)/(network_cache_stats_avg->network_cache_stats_probe_req?network_cache_stats_avg->network_cache_stats_probe_req:1)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_hits_percentage); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           ((double)(network_cache_stats_avg->network_cache_stats_num_misses * 100)/(network_cache_stats_avg->network_cache_stats_probe_req?network_cache_stats_avg->network_cache_stats_probe_req:1)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_miss_percentage); g_idx++;

      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           ((double)(network_cache_stats_avg->network_cache_stats_num_fail * 100)/(network_cache_stats_avg->network_cache_stats_probe_req?network_cache_stats_avg->network_cache_stats_probe_req:1)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_failure_percentage); g_idx++;

/*
      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           ((network_cache_stats_avg->num_cacheable_requests * 100)/(network_cache_stats_avg->network_cache_stats_probe_req?network_cache_stats_avg->network_cache_stats_probe_req:1)),
                           http_network_cache_stats_local_gp_ptr->cacheable_requests_percentage); g_idx++;
*/

      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           ((double)(network_cache_stats_avg->num_non_cacheable_requests * 100)/(network_cache_stats_avg->network_cache_stats_probe_req?network_cache_stats_avg->network_cache_stats_probe_req:1)),
                           http_network_cache_stats_local_gp_ptr->non_cacheable_requests_percentage); g_idx++;
//originreq and others

      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           ((double)(network_cache_stats_avg->non_network_cache_used_req * 100)/(network_cache_stats_avg->network_cache_stats_probe_req?network_cache_stats_avg->network_cache_stats_probe_req:1)),
                           http_network_cache_stats_local_gp_ptr->non_network_cache_used_req_percentage); g_idx++;

      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           ((double)(network_cache_stats_avg->network_cache_stats_state_others * 100)/(network_cache_stats_avg->network_cache_stats_probe_req?network_cache_stats_avg->network_cache_stats_probe_req:1)),
                           http_network_cache_stats_local_gp_ptr->network_cache_stats_state_others_percentage); g_idx++;

      GDF_COPY_VECTOR_DATA(http_network_cache_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((network_cache_stats_avg->network_cache_refresh_hits)),
                           http_network_cache_stats_local_gp_ptr->network_cache_refresh_hits_ps); g_idx++;
      g_idx = 0;
      http_network_cache_stats_local_gp_ptr++;
    }
  }
}
//---------------- NW CACHE RESPONSE HEADERS PROCESSING SECTION BEGINS ------------------------------------------

// This method is called only is network cache stats is enabled for the URL part of the scena grp
void nw_cache_stats_update_counter(void *cur_cptr, unsigned int download_time, int url_ok)
{
  connection *cptr = (connection *)cur_cptr;
  VUser *vptr = cptr->vptr;

  NSDL2_REPORTING(NULL, cptr, "Method Called, cptr->cptr_data = %p", cptr->cptr_data);

  network_cache_stats_avgtime->network_cache_stats_probe_req++;
  
  INC_GRP_BASED_NW_CACHE_STATS_PROBE_REQ(vptr);

  if(!cptr->cptr_data)  // Can this be NULL - Yes if not headers for cache stats came in response
  {
    //Origin requests counter should be incremented as in this case we will recieve response from direct server
    network_cache_stats_avgtime->non_network_cache_used_req++;

    INC_GRP_BASED_NON_NW_CACHE_USED_REQ(vptr);
    return;
  }

  unsigned int nw_cache = cptr->cptr_data->nw_cache_state;

  NSDL2_REPORTING(NULL, cptr, "Bits Status: Failure= 0x%x, Cacheable =0x%x, No header recv=0x%x, Non cacheable=0x%x, Hits=0x%x, Miss=0x%x , Refresh Hits = 0x%x", nw_cache & TCP_FAILURE_MASK, nw_cache & NW_CACHE_STATS_CACHEABLE_YES, nw_cache & NO_HEADER_RECIEVED, nw_cache & NW_CACHE_STATS_CACHEABLE_NO, nw_cache & TCP_HIT_MASK, nw_cache & TCP_MISS_MASK , nw_cache & TCP_REFRESH_HIT_AS_HIT_OR_MISS);

  // Case 1: get fail state or URL faileed
  if ((url_ok == 0) || (nw_cache & TCP_FAILURE_MASK)) 
  {
    network_cache_stats_avgtime->network_cache_stats_num_fail++;
    
    INC_GRP_BASED_NW_CACHE_STATS_NUM_FAIL(vptr);
  }
  //Case 2: If no headers are received, then we assume request has gone to origin server
  else if(!(nw_cache & NO_HEADER_RECIEVED))
  {
    network_cache_stats_avgtime->non_network_cache_used_req++;
    
    INC_GRP_BASED_NON_NW_CACHE_USED_REQ(vptr);
  }
  //Case 3: value of cachable header in NO
  else if(nw_cache & NW_CACHE_STATS_CACHEABLE_NO)
  {
    network_cache_stats_avgtime->num_non_cacheable_requests++;
    INC_GRP_BASED_NUM_NON_CACHEABLE_REQUESTS(vptr);

    //Treating the non cacheable requests also a miss so incrementing miss (response time and throughput) counters
    UPDATE_MISS_RESP_TIME_AND_THROUGHPUT_COUNTERS(download_time, vptr);
  }
  else
  {
    // Either Cacheable or not known

    //Case 4: value of cachable header in YES
    if(nw_cache & NW_CACHE_STATS_CACHEABLE_YES)
    {
      network_cache_stats_avgtime->num_cacheable_requests++;

      INC_GRP_BASED_NUM_CACHEABLE_REQUESTS(vptr);
    }
  
    // Here we can come even if nw_cache_state value is not returned in repsonse or unknown value
    if(nw_cache & TCP_HIT_MASK) 
    {
      network_cache_stats_avgtime->network_cache_stats_num_hits++;
     
      INC_GRP_BASED_NW_CACHE_STATS_NUM_HITS(vptr);

      UPDATE_HIT_RESP_TIME_AND_THROUGHPUT_COUNTERS(download_time, vptr);
    }
    else if(nw_cache & TCP_MISS_MASK) 
    {
      network_cache_stats_avgtime->network_cache_stats_num_misses++;

      INC_GRP_BASED_NW_CACHE_STATS_NUM_MISSES(vptr);

      UPDATE_MISS_RESP_TIME_AND_THROUGHPUT_COUNTERS(download_time, vptr);
    }
    else 
    {
      //Case 5: We are treating all other cases here
      network_cache_stats_avgtime->network_cache_stats_state_others++;
      
      INC_GRP_BASED_NW_CACHE_STATS_STATE_OTHERS(vptr);

      UPDATE_MISS_RESP_TIME_AND_THROUGHPUT_COUNTERS(download_time, vptr);
    }

    if(nw_cache & TCP_REFRESH_HIT_AS_HIT_OR_MISS)
    {
      network_cache_stats_avgtime->network_cache_refresh_hits++;
      
      INC_GRP_BASED_NW_CACHE_REFRESH_HITS(vptr);
    }
  }

  // For debug logging only
#ifdef NS_DEBUG_ON
  char debug_buf[4096];
  NSDL4_GDF(NULL, NULL, "NVM: NetworkCacheStats (After URL Complete) = %s", cache_stats_to_str(network_cache_stats_avgtime, debug_buf)); 
#endif

  // No need to reset state because we are doing free and make null cptr data in handle url complete 
  //cptr->cptr_data->nw_cache_state = 0; // Must reset for next request on same connection
}

