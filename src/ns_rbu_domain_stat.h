#ifndef NS_RBU_DOMAIN_STATS_H
#define NS_RBU_DOMAIN_STATS_H

#include "netstorm.h"
#include "nslb_get_norm_obj_id.h"

#define RBU_DOMAIN_NORM_TABLE_SIZE    4096
#define DELTA_RBU_DOMAIN_ENTRIES      20  

#define RBU_DOMAIN_STAT_AVG_SIZE          (sizeof(Rbu_domain_stat_avgtime) * max_rbu_domain_entries) 

#define RBU_DOMAIN_STAT_ENABLED       1

//Set periodic elements of struct a with b into a
#define SET_MIN_MAX_PERIODICS_RBU_DOMAIN_STATS(count, a, b, child_id)\
{ \
  for (i = 0; i < count; i++) {\
    int parent_index = g_domain_loc2norm_table[child_id].nvm_domain_loc2norm_table[i];\
    NSDL2_MESSAGES(NULL, NULL, "child_id = %d, domainid = %d, parent_index = %d", child_id, i, parent_index); \
    SET_MIN (a[parent_index].dns_min_time, b[i].dns_min_time);\
    SET_MAX (a[parent_index].dns_max_time, b[i].dns_max_time);\
    SET_MIN (a[parent_index].tcp_min_time, b[i].tcp_min_time);\
    SET_MAX (a[parent_index].tcp_max_time, b[i].tcp_max_time);\
    SET_MIN (a[parent_index].ssl_min_time, b[i].ssl_min_time);\
    SET_MAX (a[parent_index].ssl_max_time, b[i].ssl_max_time);\
    SET_MIN (a[parent_index].connect_min_time, b[i].connect_min_time);\
    SET_MAX (a[parent_index].connect_max_time, b[i].connect_max_time);\
    SET_MIN (a[parent_index].wait_min_time, b[i].wait_min_time);\
    SET_MAX (a[parent_index].wait_max_time, b[i].wait_max_time);\
    SET_MIN (a[parent_index].rcv_min_time, b[i].rcv_min_time);\
    SET_MAX (a[parent_index].rcv_max_time, b[i].rcv_max_time);\
    SET_MIN (a[parent_index].blckd_min_time, b[i].blckd_min_time);\
    SET_MAX (a[parent_index].blckd_max_time, b[i].blckd_max_time);\
    SET_MIN (a[parent_index].url_resp_min_time, b[i].url_resp_min_time);\
    SET_MAX (a[parent_index].url_resp_max_time, b[i].url_resp_max_time);\
    SET_MIN (a[parent_index].num_req_min, b[i].num_req_min);\
    SET_MAX (a[parent_index].num_req_max, b[i].num_req_max);\
  } \
}

#define ACC_PERIODICS_RBU_DOMAIN_STATS(count, a, b, child_id)\
{ \
  for (i = 0; i < count; i++) {\
    int parent_index = g_domain_loc2norm_table[child_id].nvm_domain_loc2norm_table[i];\
    a[parent_index].dns_time += b[i].dns_time;\
    a[parent_index].dns_counts += b[i].dns_counts;\
    a[parent_index].tcp_time += b[i].tcp_time;\
    a[parent_index].tcp_counts += b[i].tcp_counts;\
    a[parent_index].ssl_time += b[i].ssl_time;\
    a[parent_index].ssl_counts += b[i].ssl_counts;\
    a[parent_index].connect_time += b[i].connect_time;\
    a[parent_index].connect_counts += b[i].connect_counts;\
    a[parent_index].wait_time += b[i].wait_time;\
    a[parent_index].wait_counts += b[i].wait_counts;\
    a[parent_index].rcv_time += b[i].rcv_time;\
    a[parent_index].rcv_counts += b[i].rcv_counts;\
    a[parent_index].blckd_time += b[i].blckd_time;\
    a[parent_index].blckd_counts += b[i].blckd_counts;\
    a[parent_index].url_resp_time += b[i].url_resp_time;\
    a[parent_index].url_resp_counts += b[i].url_resp_counts;\
    a[parent_index].num_req += b[i].num_req;\
    a[parent_index].num_req_counts += b[i].num_req_counts;\
  } \
}

#define COPY_PERIODICS_RBU_DOMAIN_STATS_FROM_CUR_TO_NEXT_AVG(start_idx, total_idx, a, b) {\
  for (i = start_idx ; i < total_idx; i++) {\
    a[i].dns_time += b[i].dns_time;\
    a[i].dns_counts += b[i].dns_counts;\
    a[i].tcp_time += b[i].tcp_time;\
    a[i].tcp_counts += b[i].tcp_counts;\
    a[i].ssl_time += b[i].ssl_time;\
    a[i].ssl_counts += b[i].ssl_counts;\
    a[i].connect_time += b[i].connect_time;\
    a[i].connect_counts += b[i].connect_counts;\
    a[i].wait_time += b[i].wait_time;\
    a[i].wait_counts += b[i].wait_counts;\
    a[i].rcv_time += b[i].rcv_time;\
    a[i].rcv_counts += b[i].rcv_counts;\
    a[i].blckd_time += b[i].blckd_time;\
    a[i].blckd_counts += b[i].blckd_counts;\
    a[i].url_resp_time += b[i].url_resp_time;\
    a[i].url_resp_counts += b[i].url_resp_counts;\
    a[i].num_req += b[i].num_req;\
    a[i].num_req_counts += b[i].num_req_counts;\
    SET_MIN(a[i].dns_min_time, b[i].dns_min_time);\
    SET_MAX(a[i].dns_max_time, b[i].dns_max_time);\
    SET_MIN(a[i].tcp_min_time, b[i].tcp_min_time);\
    SET_MAX(a[i].tcp_max_time, b[i].tcp_max_time);\
    SET_MIN(a[i].ssl_min_time, b[i].ssl_min_time);\
    SET_MAX(a[i].ssl_max_time, b[i].ssl_max_time);\
    SET_MIN(a[i].connect_min_time, b[i].connect_min_time);\
    SET_MAX(a[i].connect_max_time, b[i].connect_max_time);\
    SET_MIN(a[i].wait_min_time, b[i].wait_min_time);\
    SET_MAX(a[i].wait_max_time, b[i].wait_max_time);\
    SET_MIN(a[i].rcv_min_time, b[i].rcv_min_time);\
    SET_MAX(a[i].rcv_max_time, b[i].rcv_max_time);\
    SET_MIN(a[i].blckd_min_time, b[i].blckd_min_time);\
    SET_MAX(a[i].blckd_max_time, b[i].blckd_max_time);\
    SET_MIN(a[i].url_resp_min_time, b[i].url_resp_min_time);\
    SET_MAX(a[i].url_resp_max_time, b[i].url_resp_max_time);\
    SET_MIN(a[i].num_req_min, b[i].num_req_min);\
    SET_MAX(a[i].num_req_max, b[i].num_req_max);\
  }\
}

#define CHILD_RESET_RBU_DOMAIN_STAT_AVGTIME(a) \
{ \
  NSDL2_MESSAGES(NULL, NULL, "Reset CHILD_RESET_RBU_DOMAIN_STAT_AVGTIME for total_rbu_domain_entries = %d", total_rbu_domain_entries); \
  for (i = 0; i < total_rbu_domain_entries; i++) {\
    a[i].dns_min_time = MAX_VALUE_8B_U;\
    a[i].dns_max_time = 0;\
    a[i].dns_counts = 0;\
    a[i].dns_time = 0;\
    a[i].tcp_min_time = MAX_VALUE_8B_U;\
    a[i].tcp_max_time = 0;\
    a[i].tcp_counts = 0;\
    a[i].tcp_time = 0;\
    a[i].ssl_min_time = MAX_VALUE_8B_U;\
    a[i].ssl_max_time = 0;\
    a[i].ssl_counts = 0;\
    a[i].ssl_time = 0;\
    a[i].connect_min_time = MAX_VALUE_8B_U;\
    a[i].connect_max_time = 0;\
    a[i].connect_counts = 0;\
    a[i].connect_time = 0;\
    a[i].wait_min_time = MAX_VALUE_8B_U;\
    a[i].wait_max_time = 0;\
    a[i].wait_counts = 0;\
    a[i].wait_time = 0;\
    a[i].rcv_min_time = MAX_VALUE_8B_U;\
    a[i].rcv_max_time = 0;\
    a[i].rcv_counts = 0;\
    a[i].rcv_time = 0;\
    a[i].blckd_min_time = MAX_VALUE_8B_U;\
    a[i].blckd_max_time = 0;\
    a[i].blckd_counts = 0;\
    a[i].blckd_time = 0;\
    a[i].url_resp_min_time = MAX_VALUE_8B_U;\
    a[i].url_resp_max_time = 0;\
    a[i].url_resp_counts = 0;\
    a[i].url_resp_time = 0;\
    a[i].num_req_min = MAX_VALUE_8B_U;\
    a[i].num_req_max = 0;\
    a[i].num_req_counts = 0;\
    a[i].num_req = 0;\
  }\
}

//Storing data from RBU_RespAttr to rbu_domain_stat_avgtime_t for Times graph
#define FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(src, dest, dest_min, dest_max, dest_count) \
{ \
  if(rbu_domains[domain_idx].src < 0) \
    rbu_domains[domain_idx].src = 0; \
  SET_MIN(rbu_domain_stat_avg[domain_idx].dest_min, rbu_domains[domain_idx].src); \
  SET_MAX(rbu_domain_stat_avg[domain_idx].dest_max, rbu_domains[domain_idx].src); \
  rbu_domain_stat_avg[domain_idx].dest += rbu_domains[domain_idx].src; \
  rbu_domain_stat_avg[domain_idx].dest_count++; \
}

#define RESET_RBU_DOMAIN_STAT_AVG(a, start, total) \
{ \
  int i = start; \
  for(; i < total; i++) \
  { \
    a[i].dns_min_time = MAX_VALUE_4B_U; \
    a[i].tcp_min_time = MAX_VALUE_4B_U; \
    a[i].ssl_min_time = MAX_VALUE_4B_U; \
    a[i].connect_min_time = MAX_VALUE_4B_U; \
    a[i].wait_min_time = MAX_VALUE_4B_U; \
    a[i].rcv_min_time = MAX_VALUE_4B_U; \
    a[i].blckd_min_time = MAX_VALUE_4B_U; \
    a[i].url_resp_min_time = MAX_VALUE_4B_U; \
    a[i].num_req_min = MAX_VALUE_4B_U; \
  } \
}
     

// RBU Domain Stat Data Structure
typedef struct rbu_domain_time_data_gp_t
{
  Times_data dns_time_gp;                     //Overall DNS time
  Times_data tcp_time_gp;                     //Overall TCP time
  Times_data ssl_time_gp;                     //Overall SSL time 
  Times_data connect_time_gp;                 //Overall Connect time
  Times_data wait_time_gp;                    //Overall Wait time
  Times_data rcv_time_gp;                     //Overall Receive time
  Times_data blckd_time_gp;                   //Overall Blocked time
  Times_data url_resp_time_gp;                //Overall URL response time
  Times_data num_req_gp;                       //Overall Number of request from one domain

} Rbu_domain_time_data_gp;

typedef struct rbu_domain_stat_avgtime_t
{
  int dns_counts;                                             //DNS timing attributes
  float dns_time;
  float dns_min_time;
  float dns_max_time;

  int tcp_counts;                                             //TCP timing attributes
  float tcp_time;
  float tcp_min_time;
  float tcp_max_time;

  int ssl_counts;                                              //SSL timing attributes
  float ssl_time;
  float ssl_min_time;
  float ssl_max_time;

  int connect_counts;                                          //Connect timing attributes
  float connect_time;
  float connect_min_time;
  float connect_max_time;

  int wait_counts;                                            //Wait timing attributes
  float wait_time;
  float wait_min_time;
  float wait_max_time;

  int rcv_counts;                                             //Receive timing attributes
  float rcv_time;
  float rcv_min_time;
  float rcv_max_time;

  int blckd_counts;                                           //Blocked timing attributes
  float blckd_time;
  float blckd_min_time;
  float blckd_max_time;

  int url_resp_counts;                                        //URL timing attributes
  float url_resp_time;
  float url_resp_min_time;
  float url_resp_max_time;

  int num_req_counts;                                     //Number of request from one domain
  float num_req;
  float num_req_min;
  float num_req_max;
} Rbu_domain_stat_avgtime;

typedef struct rbu_domains_t
{
  int is_filled;
  float dns_time;                       //will store DNS time
  float tcp_time;                       //will store TCP time
  float ssl_time;                       //will store SSL time
  float connect_time;                   //will store connect time
  float wait_time;                      //will store wait time
  float rcv_time;                       //will store receive time
  float blckd_time;                     //will store blocked time
  float url_resp_time;                  //stores overall URL elapse time
  int num_request;                      //Stores number of requests served from same domain
} Rbu_domains;
 
typedef struct rbu_domain_loc2norm_table_t
{
  int loc2norm_domain_alloc_size;    //store number of domain entries
  int *nvm_domain_loc2norm_table;
  int loc_domain_avg_idx;            //Point to particular generator's avgtime for RBU Domain Stats
} Rbu_domain_loc2norm_table;

#ifndef CAV_MAIN
extern NormObjKey rbu_domian_normtbl; 
extern int rbu_domain_stat_avg_idx;
extern Rbu_domain_loc2norm_table *g_domain_loc2norm_table; //Global variable for starting address of g_domain_loc2norm table
extern Rbu_domain_stat_avgtime *rbu_domain_stat_avg;
#else
extern __thread NormObjKey rbu_domian_normtbl;
extern __thread int rbu_domain_stat_avg_idx;
extern __thread Rbu_domain_loc2norm_table *g_domain_loc2norm_table; //Global variable for starting address of g_domain_loc2norm table
extern __thread Rbu_domain_stat_avgtime *rbu_domain_stat_avg;
#endif

extern Rbu_domain_time_data_gp *rbu_domain_stat_gp_ptr;
extern Rbu_domains *rbu_domains;

extern int rbu_domain_stat_data_gp_idx;
extern int rbu_per_page_domain_entries;

extern char **print_rbu_domain_stat();
extern char **init_2d(int no_of_host);
extern void fill_2d(char **TwoD, int i, char *fill_data);

extern void ns_rbu_domain_init_loc2norm_table(int domain_entries);
extern void update_rbu_domain_stat_avgtime_size();
extern void initialise_rbu_domain_stat_min();
extern void set_rbu_domain_stat_data_avgtime_ptr();
extern void set_rbu_domain_stat_data_avgtime_data();
extern void parse_and_set_rbu_domain_stat();
extern void fill_rbu_domain_stat_gp(avgtime **rbu_domain_stat_avg);
extern void set_rbu_domain_status_data_avgtime_data();
extern int ns_rbu_add_dynamic_domain(short nvmindex, int local_rbu_domain_id, char *rbu_domain_name, short rbu_domain_name_len, int* flag_new);
extern void printRbuGraph(char **TwoD , int *Idx2d, char *prefix, int groupId, int genId);
extern char **printRbuDomainStat(int groupId);
extern bigbuf_t dname_big_buf;

#endif
