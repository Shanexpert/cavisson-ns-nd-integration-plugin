//#include <sys/types.h>

//#define _XOPEN_SOURCE /* glibc2 needs this */
#include "ns_cache_include.h"
#include "ns_common.h"
#include "nslb_date.h"
#include "nslb_util.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

/*G_HTTP_CACHING <group name> <pct>  <mode>*/
int kw_set_g_http_caching(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  char cpct_user[MAX_DATA_LINE_LENGTH];
  cpct_user[0] = 0;
  float pct_user;
  int mode = 0;
  char cmode[MAX_DATA_LINE_LENGTH];
  cmode[0]=0;
  char cClient_freshness[MAX_DATA_LINE_LENGTH];
  cClient_freshness[0] = 0;
  int client_freshness = 0;

  NSDL2_CACHE(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s %s %s", keyword, sgrp_name, cpct_user, cmode,  cClient_freshness, tmp); 
  NSDL2_CACHE(NULL, NULL, "Method called, num= %d", num);
  
  if((num < 3) || (num > 5)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_1);
  }
  if(num == 3){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(cpct_user))
    pct_user = atof(cpct_user);
  else
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_2);

  if((pct_user < 0.0) || (pct_user > 100.0)) 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_6);

  if(ns_is_numeric(cmode))
    mode = atoi(cmode);
  else
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_2);
 
  if(mode != 0 && mode != 1) 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_3);

  if(num == 5)
  { 
    if(ns_is_numeric(cClient_freshness))
      client_freshness=atoi(cClient_freshness);
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_2);
    }
    if(client_freshness != 0 && client_freshness != 1)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHING_USAGE, CAV_ERR_1011031, CAV_ERR_MSG_3);
    } 
  }
  gset->cache_mode = mode; 
  gset->client_freshness_constraint = client_freshness;
  gset->cache_user_pct = pct_user * 100; 

  if(gset->cache_user_pct) 
    global_settings->protocol_enabled |= HTTP_CACHE_ENABLED;

  NSDL2_CACHE(NULL, NULL, "mode = %d, cache_user_pct (*100) = %d, client freshness = %d ", gset->cache_mode, gset->cache_user_pct, gset->client_freshness_constraint);
  return 0;
}

/*G_HTTP_TEST_CACHING <group name> <pct>  <mode>*/
int kw_set_g_http_caching_test(char *buf, GroupSettings *gset, char *err_msg) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  //int num;
  double freshness_factor = 1;
  int add_delay = 1;

  NSDL2_CACHE(NULL, NULL, "Method called, buf = %s", buf);

  sscanf(buf, "%s %s %lf %d", keyword, sgrp_name,&freshness_factor, &add_delay); 
  //NSDL2_CACHE(NULL, NULL, "Method called, num= %d", num);
 
/*
  if(num != 2)
  { 
    fprintf(stderr, "Error: Invalid value of G_HTTP_TEST_CACHING keyword\n");
    fprintf(stderr, "  Usage: G_HTTP_CACHING_TEST <group name> <freshness factor> \n");
    fprintf(stderr, "    Group name can be ALL or any group name used in scenario group\n");
    fprintf(stderr, "    Freshness Factor cannot be -ve \n");
    exit(-1);
  }

  if(freshness_factor < 0) {
    fprintf(stderr, "Error: Invalid value of mode in G_HTTP_TEST_CACHING keyword\n");
    fprintf(stderr, "    Freshness Factor cannot be -ve \n");
    exit(-1);
  }
*/

  gset->cache_freshness_factor = freshness_factor; 
  gset->cache_delay_resp = add_delay; 

  NSDL2_CACHE(NULL, NULL, "freshness factor= %lf, cache_delay_resp = %d",
                           gset->cache_freshness_factor,
                           gset->cache_delay_resp);
  return 0;
}

int kw_set_g_http_caching_master_table(char *buf, GroupSettings *gset, char *err_msg, int rumtime_flag){

  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;
  char cmode[MAX_DATA_LINE_LENGTH];
  cmode[0] = '0';
  cmode[1] = 0;
  char csize_value[MAX_DATA_LINE_LENGTH];
  csize_value[0] = 0;
  char table_name[MAX_DATA_LINE_LENGTH];
   table_name[0] = 0;

  #define START_CACHE_ENTRIES 512 //this is used to initiate cache table size
  int size_value = START_CACHE_ENTRIES;

  NSDL2_CACHE(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s %s %s", keyword, sgrp_name, cmode, table_name, csize_value, tmp);
  NSDL2_CACHE(NULL, NULL, "Method called, num= %d", num);

  if(ns_is_numeric(cmode) == 0)
    NS_KW_PARSING_ERR(buf, rumtime_flag, err_msg, G_HTTP_CACHE_MASTER_TABLE_USAGE, CAV_ERR_1011033, CAV_ERR_MSG_2);

  mode = atoi(cmode);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, rumtime_flag, err_msg, G_HTTP_CACHE_MASTER_TABLE_USAGE, CAV_ERR_1011033, CAV_ERR_MSG_3);
  }

  if(mode == 0){
    if((num < 3) || (num > 5)) 
    {
      NS_KW_PARSING_ERR(buf, rumtime_flag, err_msg, G_HTTP_CACHE_MASTER_TABLE_USAGE, CAV_ERR_1011033, CAV_ERR_MSG_1);
    }
  }
  else
  {
    if((num < 4) || (num > 5)) 
    {
      NS_KW_PARSING_ERR(buf, rumtime_flag, err_msg, G_HTTP_CACHE_MASTER_TABLE_USAGE, CAV_ERR_1011033, CAV_ERR_MSG_1);
    }

    if(num > 4)
    {
      if(ns_is_numeric(csize_value) == 0)
      { 
       NS_KW_PARSING_ERR(buf, rumtime_flag, err_msg, G_HTTP_CACHE_MASTER_TABLE_USAGE, CAV_ERR_1011033, CAV_ERR_MSG_2);
      }
      size_value = atoi(csize_value);
    }

    gset->master_cache_mode = mode;
    gset->master_cache_table_size = size_value;
    if(validate_var(table_name))
    {
      NS_KW_PARSING_ERR(buf, rumtime_flag, err_msg, G_HTTP_CACHE_MASTER_TABLE_USAGE, CAV_ERR_1011034, "");
    }
    strcpy(gset->master_cache_tbl_name, table_name);
  }
  NSDL2_CACHE(NULL, NULL, "gset->master_cache_mode is = %d. gset->master_cache_table_size = %d, gset->master_cache_tbl_name = %s",gset->master_cache_mode, gset->master_cache_table_size, gset->master_cache_tbl_name);
  return 0;
}

// G_HTTP_CACHE_TABLE_SIZE <grp-name> <mode> <value>
int kw_set_g_http_cache_table_size(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;
  char cmode[MAX_DATA_LINE_LENGTH];
  cmode[0] = 0;
  char csize_value[MAX_DATA_LINE_LENGTH];
  csize_value[0] = 0;

  #define START_CACHE_ENTRIES 512 //this is used to initiate cache table size
  int size_value = START_CACHE_ENTRIES;

  NSDL2_CACHE(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s %s", keyword, sgrp_name, cmode, csize_value, tmp);
  NSDL2_CACHE(NULL, NULL, "Method called, num= %d", num);

  if((num < 3) || (num > 4)) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHE_TABLE_SIZE_USAGE, CAV_ERR_1011032, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(cmode) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHE_TABLE_SIZE_USAGE, CAV_ERR_1011032, CAV_ERR_MSG_2);

  mode = atoi(cmode);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHE_TABLE_SIZE_USAGE, CAV_ERR_1011032, CAV_ERR_MSG_3);
  }

  if(num > 3)
  {
    if(ns_is_numeric(csize_value) == 0)
    { 
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_CACHE_TABLE_SIZE_USAGE, CAV_ERR_1011032, CAV_ERR_MSG_2);
    }
    size_value = atoi(csize_value);
  }
  
  gset->cache_table_size_mode = mode;
  if(size_value > MAX_CACHE_TABLE_SIZE) 
  {  
    NS_DUMP_WARNING("Cache table size is %d, which is greater than max limit. So, setting cache size to '%d'", 
                     size_value, MAX_CACHE_TABLE_SIZE);
  }
  gset->cache_table_size_value = change_to_2_power_n(size_value);
  NSDL2_CACHE(NULL, NULL, "gset->cache_table_size_mode is = %d. gset->cache_table_size_value = %d", gset->cache_table_size_mode, gset->cache_table_size_value);
  return 0;
}

//This method is used to return the next 2^n of the number passed to it. if the no is already a power of to then it will 
//the same no other wise it will return the next power of two.
int change_to_2_power_n(int size_value)
{
int i;
int count = 0;
int pos = 0;

  NSDL2_CACHE(NULL, NULL, "Method called. size_value = %d", size_value);

  if(size_value >= MAX_CACHE_TABLE_SIZE)
  {
    NSDL2_CACHE(NULL, NULL, "limit exceded. size_value = %d", MAX_CACHE_TABLE_SIZE); 
    return MAX_CACHE_TABLE_SIZE;
  }

  //Here we are counting no. of ones in size value to check whether it a power of 2 or not 
  // Optmize
  for(i = 0; i < 32; i++)
  {
    int ii = pow(2,i);
    if(size_value & ii) 
    {
      count++;
      pos = i;
    }
  }
  //count > 1 means it is not a power of 2. so increasing it to next power of 2
  if(count > 1)
  {
    pos = pos +1;
    size_value = pow(2,pos);
    NSDL2_CACHE(NULL, NULL, "Method called. after changing to power of 2 size_value = %d", size_value);
  }
  return size_value;
}

int calc_doc_age_when_received(CacheTable_t *cacheptr)
{  

  int apparent_age, corrected_received_age, response_delay;
  int doc_age_when_received;
  int request_time, response_time;
  int date_value;
  int age_value;

  CacheResponseHeader *crh = cacheptr->cache_resp_hdr;

  date_value = crh->date_value;
  request_time = cacheptr->request_ts;
  response_time = cacheptr->cache_ts;
  age_value = crh->age;
  //current_age = request_time - date_value;

  apparent_age = ((response_time - date_value) > 0)?(response_time - date_value):0;
  NSDL2_CACHE(NULL, NULL, "apparent_age = %d, response_time=%d, date_value=%d", apparent_age,response_time, date_value);

  corrected_received_age = (apparent_age > age_value)?apparent_age:age_value;
  NSDL2_CACHE(NULL, NULL, "corrected_received_age= %d, apparent_age = %d age_value=%d",
                                                corrected_received_age, apparent_age,age_value);

  response_delay = response_time - request_time;
  NSDL2_CACHE(NULL, NULL, "response_delay= %d, response_time= %d request_time=%d",
                                                response_delay, response_time,request_time);

  doc_age_when_received = corrected_received_age + response_delay;
  NSDL2_CACHE(NULL, NULL, "doc_age_when_received= %d, corrected_received_age= %d response_delay=%d",
                                                doc_age_when_received, corrected_received_age, response_delay);
 
  return doc_age_when_received;
}

/*
 *   age_value:
 *         is the value of Age: header received by the cache with
 *         this response.
 *   date_value:
 *            is the value of the origin server's Date: header
 *   request_time:
 *          is the (local) time when the cache made the request
 *               that resulted in this cached response(Will get from cache)
 *   response_time:
 *          is the (local) time when the cache received the
 *                        response(Will get from cache) (cache_ts)
 *   now: is the current (local) time
 */

int cache_get_current_age (connection *cptr, CacheTable_t *cacheptr, u_ns_ts_t now)
{
  int resident_time_in_cache, current_age;
  int response_time, current_time; 

  response_time = cacheptr->cache_ts;

#if 0
  NSDL2_CACHE(NULL, cptr, "Method called");
  //apparent_age = max(0, response_time - date_value);
  NSDL2_CACHE(NULL, cptr, "apparent_age = %d", apparent_age);
  //corrected_received_age = max(apparent_age, age_value);
  apparent_age = ((response_time - date_value) > 0)?(response_time - date_value):0;
  NSDL2_CACHE(NULL, cptr, "apparent_age = %d, response_time=%d, date_value=%d", apparent_age,response_time, date_value);
  corrected_received_age = (apparent_age > age_value)?apparent_age:age_value;
  NSDL2_CACHE(NULL, cptr, "corrected_received_age= %d, apparent_age = %d age_value=%d",
                                                corrected_received_age, apparent_age,age_value);
  response_delay = response_time - request_time;
  NSDL2_CACHE(NULL, cptr, "response_delay= %d, response_time= %d request_time=%d",
                                                response_delay, response_time,request_time);
  corrected_initial_age = corrected_received_age + response_delay;
  NSDL2_CACHE(NULL, cptr, "corrected_initial_age= %d, corrected_received_age= %d response_delay=%d",
                                                corrected_initial_age, corrected_initial_age,response_delay);
#endif
  current_time = get_ns_start_time_in_secs() + get_ms_stamp()/1000 + get_timezone_diff_with_gmt ();
  resident_time_in_cache = current_time - response_time;
  NSDL2_CACHE(NULL, cptr, "resident_time= %d, current_time=%d, response_time=%d",
                                                resident_time_in_cache, current_time, response_time);
  current_age = cacheptr->doc_age_when_received + resident_time_in_cache;
  NSDL2_CACHE(NULL, cptr, "doc_age_when_received= %d, resident_time = %d",
                                                cacheptr->doc_age_when_received, resident_time_in_cache);
  NSDL2_CACHE(NULL, cptr, "Current age = %d", current_age);
  return current_age;

}

//#Shilpa 16Feb2011 Bug#2037
//Implementing Client Freshness Constraint
void cache_copy_req_hdr_shr(int i)
{
  memcpy(&(request_table_shr_mem[i].proto.http.cache_req_hdr), &(requests[i].proto.http.cache_req_hdr), sizeof(requests[i].proto.http.cache_req_hdr));
  NSDL2_CACHE(NULL, NULL, "values in share memory:min-fresh value = %u, max-age value = %u, max-stale = %u", request_table_shr_mem[i].proto.http.cache_req_hdr.min_fresh, request_table_shr_mem[i].proto.http.cache_req_hdr.max_age, request_table_shr_mem[i].proto.http.cache_req_hdr.max_stale);
}




