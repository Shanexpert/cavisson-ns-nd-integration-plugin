/******************************************************************
 * Name    :    ns_vuser_runtime_control.h 
 * Purpose :    
 * Note    :
 * Author  :    
 * Intial version date:    28/11/18
 * Last modification date: 
******************************************************************/
#ifndef NS_VUSER_RUNTIME_CONTROL_H
#define NS_VUSER_RUNTIME_CONTROL_H

// VUser status used for filtering and indexing in data structure(must be from 0 to N)
#define NS_VUSER_RTC_DOWN                0
#define NS_VUSER_RTC_RUNNING  		 1
#define NS_VUSER_RTC_SYNC_POINT		 2
#define NS_VUSER_RTC_PAUSED		 3
#define NS_VUSER_RTC_EXITING		 4
#define NS_VUSER_RTC_GRADUAL_EXITING	 5
#define NS_VUSER_RTC_STOPPED		 6

#define RTC_MSG_BUF_BLOCK_SIZE          10240
#define RTC_MSG_BUF_MAX_LINE_SIZE       2048  
#define MAX_TOKEN                       1024
#ifndef MAX_MSG_LEN
  #define MAX_MSG_LEN                     128
#endif
#define MAX_VUSER_ID_LEN                24

#define SEND_MSG_TO_MASTER(X, Y, Z)  send_rtc_msg_to_parent_or_master(X, Y, Z) 
#define SEND_MSG_TO_PARENT(X, Y, Z)  send_rtc_msg_to_parent_or_master(X, Y, Z) 


#define CHECK_ALL_VUSER_CLIENT_DONE  nslb_check_all_reset_bits(vuser_client_mask)

#define INC_VUSER_MSG_COUNT(client_id)\
{\
  nslb_set_bitflag(vuser_client_mask, client_id);\
}

#define DEC_VUSER_MSG_COUNT(client_id, is_nvm_failed)\
{\
  int is_new;\
  if(is_nvm_failed == 1)\
  {\
    ns_reduce_vuser_summary_counters_on_nvm_failure(client_id);\
  }\
  is_new = nslb_reset_bitflag(vuser_client_mask, client_id);\
  if(!is_new && is_nvm_failed)\
  {\
    NSDL2_MESSAGES(NULL, NULL, "generator/child id = %d failed", client_id);\
    ns_vuser_client_failed(client_id);\
  }\
  else if(is_new && !is_nvm_failed)\
  {\
    NSDL2_MESSAGES(NULL, NULL, "generator/child id = %d active", client_id);\
    return 0;\
  }\
}


// Comments:
typedef struct
{
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR
  char *data;
} RTC_VUser;

typedef struct
{
  int num_down_vuser;
  int num_running_vuser;                       //num_running_vuser = (active_vusers + thinking_vusers + waiting_vusers + blocked_vusers)
  int num_spwaiting_vuser;
  int num_paused_vuser;
  int num_exiting_vuser;
  int num_gradual_exiting_vuser;
  int num_stopped_vuser;
  int total_vusers;
}GroupVUserSummaryTable;

typedef struct 
{
  char vuser_id[MAX_VUSER_ID_LEN + 1];                             // gen_id:nvm_id:vuser_id
  char vuser_location[MAX_MSG_LEN + 1];              // Use existing location size macro
  char message[MAX_MSG_LEN + 1];                     // example : some error condition or to show user is gradual exiting 
  int session_elapsed_time;                          // in ms since beginning of the session
  int vuser_elapsed_time;
  unsigned char vuser_status;
  char script_status;
  char page_name[MAX_MSG_LEN + 1];                  // take page_id instead
  char tx_name[MAX_MSG_LEN + 1];                    // take tx_id instead (Map to normalised id)
  short grp_idx;
  short gen_idx;
  short msg_opcode;				    // Message Index for Standard Message
  long vptr;				    // VUser vptr  
}VUserInfoTable;

typedef struct 
{
  int grp_idx;
  int offset;
  int limit;
  int status;
  int gen_idx; 
}VUserQueryInfo;

typedef struct 
{
  int grp_idx;
  int quantity;
  int gen_idx;
  int nvm_id;
  int vuser_idx;
  long vptr;
}VUserPRSInfo;    //PRS: Pause Resume Stop

// This structure is used by NVMs. Total entries for vulist  structure will be the total number of group entries
typedef struct
{
  //This is linked list for vusers(used by child). NVMs will have own linkedlist for vusers.
  VUser *vptr_head; //No need to have tail. We add and remove from head only.
}VUserPauseList;

extern unsigned long vuser_client_mask[4]; //resetting it at CHECK_AND_SET_INVOKER_MCCPTR
extern int g_vuser_msg_seq_num;
extern GroupVUserSummaryTable *gVUserSummaryTable;
extern void init_vuser_summary_table(int parent_only);
extern void process_get_vuser_summary(Msg_com_con *mccptr);
extern int process_get_vuser_summary_ack(int child_id, GroupVUserSummaryTable *msg);
extern void process_get_vuser_list(Msg_com_con *mccptr, char *msg, int size);
extern int process_get_vuser_list_ack(int child_id, VUserInfoTable *msg, int size);
extern void process_pause_vuser(Msg_com_con *mccptr, char *msg, int size);
extern int process_pause_vuser_ack(int child_id, char *msg);
extern void process_resume_vuser(Msg_com_con *mccptr, char *msg, int size);
extern int process_resume_vuser_ack(int child_id, char *msg); 
extern void process_stop_vuser(Msg_com_con *mccptr, char *msg, int size);
extern void process_stop_vuser_ack(int child_id, char *msg); 
extern void ns_process_vuser_summary();
extern void ns_process_vuser_list(void *msg, int size);
extern void ns_process_pause_vuser(void *msg, int size);
extern void ns_process_resume_vuser(void *msg, int size);
extern void ns_process_stop_vuser(void *msg, int size);
extern void increase_vuser_client_message_count(int child_idx);
extern void decrease_vuser_client_message_count(int child_idx, int *);
extern void ns_vuser_client_failed(int client_idx);
extern int ns_rampdown_vuser_quantity(int in_grp_idx, int quantity, u_ns_ts_t now, int runtime_flag);
extern void remove_from_pause_list(VUser *vptr);
extern inline int remove_select(int fd);
extern int add_select(void* cptr, int fd, int event);
extern void idle_connection( ClientData client_data, u_ns_ts_t now );
extern int on_new_session_start (VUser *vptr, u_ns_ts_t now);
extern void send_rtc_msg_to_invoker(Msg_com_con *mccptr, int opcode, void *data, int size);
extern int stop_user_immediately(VUser *vptr, u_ns_ts_t now);
extern int send_rtc_msg_to_all_clients(int opcode, char *msg, int size);
void ns_reduce_vuser_summary_counters_on_nvm_failure(int nvm_idx);
#endif
