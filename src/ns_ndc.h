#ifndef NS_NDC_H
#define NS_NDC_H

#define START_REPLY_FROM_NDC "nd_data_rep:action=start_monitor_data_conn;result=Ok;"
#define STOP_REPLY_FROM_NDC "nd_data_rep:action=stop_monitor_data_conn;result=ok;"

#define NODE_DISCOVERY 1
#define NODE_MODIFY 2
#define END_MONITOR 3
#define NODE_INACTIVE 4
#define INSTANCE_INACTIVE 5

#define ACTIVE 1
#define INACTIVE 0

#define ND_BT_TIER_LIST 1
#define ND_IP_TIER_LIST 2

#define NDC_START_CONNECTION_INIT 0
#define NDC_WAIT_FOR_RESP 1 
#define NDC_TIMEOUT 900000              //Timeout in case of no response from NDC 
extern double g_ndc_start_time;
extern char ndc_connection_state;
//NDC Percentile related funtions
extern void check_and_delete_nd_percentile_vectors();
extern void check_and_delete_nd_percentile_vectors_using_tiers(int tier_type);
extern void delete_entry_for_nd_monitors(CM_info *cus_mon_ptr, int vectorIdx, char flag );
extern CM_info *set_or_get_nd_percentile_ptrs(CM_info *cus_mon_ptr);
extern int fetch_hash_index_and_delete_nd_vector(char *vector_name, CM_info *cus_mon_ptr);
void end_mon(int index, int operation , int cm_idx, int reason,int tier_idx);
extern int stop_nd_ndc();
extern int stop_nd_ndc_data_conn();
extern int ndc_recovery_connect();
extern int chk_connect_to_ndc(); 
extern int read_ndc_stop_msg();
extern void start_nd_ndc();
extern inline void handle_ndc(struct epoll_event *pfds, int i);
extern void send_partition_switching_msg_to_ndc();
extern  void delete_vector_matched_inst_id(int inst_id, char mark_delete_flag);
extern void make_ndlogs_dir_and_link();
extern int parse_and_extract_server_info(char **data, int max_field, int operation);
extern void do_remove_add_control_fd();

//For OutboundConnection
extern Msg_com_con ndc_data_mccptr;
extern void nde_skip_bad_partition();

extern int send_msg_to_ndc(char *msg_buff);

//For node discovery msgs
extern int total_ndc_node_msg;
extern int max_ndc_node_msg;
extern char **ndc_node_msg;
extern int g_check_nd_overall_to_delete;
extern int mark_nd_overall_delete(CM_info *cus_mon_ptr, int parent_idx);
extern void parse_ndc_node_msgs();
#endif //NS_NDC_H

//Structure For PercentileList
typedef struct PercentileList
{
  char state;   //Flag for Active or Inactive Flag  eg. 0 or 1
  short len;    //length of the percentile msg      eg. >80thPercentile
  int  value;   //value of Percentile               eg. 1 to 100
  char *name;   //name of Percentile                eg. >80thPercentile
  struct PercentileList *next; //will store address of next node
}PercentileList;

//Structure For TierList
typedef struct TierList
{
  char state;
  char tier_type; // value for tier type 1 = BT , 2 = IP
  short tier_len;
  int  tier_id;   //value of tier id
  char *tier_name;   //name of tier
  struct TierList *next; //will store address of next node
}TierList;
void do_add_control_fd_wrapper(TopoServerInfo *server_ptr);
extern PercentileList *percentile_list_head_ptr;
extern TierList *tier_list_head_ptr;
extern void process_nd_overall_data();
extern void do_add_control_fd(int tier_idx,int server_idx);
extern void do_remove_control_fd(int tier_idx,int server_idx);
