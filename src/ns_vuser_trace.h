/*<?xml version="1.0"?>
<UserTrace>
  <StartSession>
    <StartSessionTime>HH:MM:SS</StartSessionTime>
    <SessionID>NVM_ID:Session_ID</SessionID>
    <ScriptName>script1</ScriptName>
  </StartSession>

  <Pages>
    <Page>
      <PageName>Page1</PageName>
      <StartTime>HH:MM:SS</StartTime>
      <Protocol>HTTP</Protocol>
      <ResponseTime>0.05</ResponseTime>
      <HTTPStatusCode>200</HTTPStatusCode >
      <PageStatus>Success</PageStatus >
      <PageThinkTime>1.5</PageThinkTime>
      <RequestURL>http://192.168.1.68:8000/index</RequestURL>

      <SearchParameters>
        <SearchParameter>
          <Name>SessionID</Name>
	  <Value>123456</Value>
	  <ORD>1</ORD>
	</SearchParameters>
	<SearchParameter>
	  <Name>Color</Name>
	  <Value>Blue</Value>
	  <ORD>1</ORD>
        </SearchParameter>
      </SearchParameters>
        
      <Parameterization>
        <Parameter>
          <ParamterType>File</ParamterType>
          <Name>Login</Name>
          <Value>abc@a.com</Value>        
        </Parameter>
        <Parameter>
          <ParamterType>File</ParamterType>
          <Name>Password</Name>
          <Value>pass1234</Value>        
        </Parameter>
      </Parameterization>
                
      <Validations>
        <Checkpoint>
          <Detail>Text="welcome"</Detail>
          <ID>Login page is not found</Name>
          <Status>Pass</Status>        
        </Checkpoint>
      </Validations>

      <Request>req1.log</Request >
      <Response>rep1.log</Response >
      <ResponseBody>rep_body1.log</ResponseBody>
    </Page>
  </Pages>
    
  <EndSession>
    <SessionTime>35.245</SessionTime>
  </EndSession>
    
</UserTrace> */

#ifndef NS_USER_TRACE_H
#define NS_USER_TRACE_H

#define NS_UT_TYPE_START_SESSION 0
#define NS_UT_TYPE_PAGE          1
#define NS_UT_TYPE_END_SESSION   2
#define NS_UT_TYPE_PAGE_REQ      3

#define  VUSER_TRACE_SUCCESS       0
#define  VUSER_TRACE_SYS_ERR       1
#define  VUSER_TRACE_INVALID_GRP   2
#define  VUSER_TRACE_NVM_OVER   3

#define NS_IF_TRACING_ENABLE_FOR_USER (vptr->flags & NS_VUSER_TRACE_ENABLE)

#define NS_VUSER_TRACE_XML_WRITTEN  0x0001 

#define GET_UTD_NODE \
  NSDL1_USER_TRACE(vptr, NULL, "vptr->flags = %x", vptr->flags); \
  if (NS_IF_TRACING_ENABLE_FOR_USER){ \
    utd_node = user_trace_grp_ptr[vptr->group_num].utd_head; \
    NSDL1_USER_TRACE(vptr, NULL, "Utd node(Traceing is enabled) = %p", utd_node); \
  } else if(NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL){ \
    utd_node = (userTraceData*)vptr->pd_head; \
    NSDL1_USER_TRACE(vptr, NULL, "Utd node(Page dump is enabled) = %p", utd_node); \
  } else \
    NSDL1_USER_TRACE(vptr, NULL, "Invalid case, neither user trace enable nor page dump");

#define GET_UT_TAIL_PAGE_NODE \
  /*userTraceData* utd_node = NULL;*/\
  GET_UTD_NODE \
  UserTraceNode *page_node = (UserTraceNode *)utd_node->ut_tail; \
  if(page_node->page_info == NULL) \
  { \
    NSDL1_USER_TRACE(vptr, NULL, "Page node is NULL for the user, returning..");\
    return;\
  }

#define GET_UT_TAIL_NODE \
  userTraceData* utd_node = NULL;\
  GET_UTD_NODE \
  UserTraceNode *page_node = (UserTraceNode *)utd_node->ut_tail;

typedef struct user_trace_param
{
  char type;       //For type of parameter(Ex: Search, Index, File..)
  char *name;       //Name of the variable
  char *value;      //value of the variable
  short ord;
  struct user_trace_param *next;
} UserTraceParam;

typedef struct user_trace_param_used
{
  short type;       //For type of parameter(Ex: Search, Index, File..)
  char *name;       //Name of the variable
  char *value;      //value of the variable
  struct user_trace_param_used *next;
} UserTraceParamUsed;

typedef struct user_trace_validation
{
  char *detail;
  char *id;
  char *status;
  struct user_trace_validation *next;
} UserTraceValidation;

typedef struct 
{
  char *page_name;
  char *req_url;
  char *protocol;
  char taken_from_cache;
  int page_think_time; // Milli-secs
  int page_rep_time; // Milli-secs
  short page_status;
  short http_status_code;

  struct user_trace_param *param_head;
  struct user_trace_param_used *param_used_head;
  struct user_trace_validation *validation_head;

  char *req; // Malloced buffer with complete main URL request
  int req_size;
  char *rep; // Malloced buffer with complete main URL response
  int rep_size;
  char *rep_body; // Malloced buffer with main URL response (uncompresses if compressed)
  int rep_body_size;
  //unsigned int page_id;
  char *flow_name;
  u_ns_ts_t page_end_time;
  u_ns_ts_t page_start_time;
} UserTracePageInfo;

typedef struct user_trace_node
{
  char type;             // Type of node
  char dump_on_tx_fail; //This flag should be set in case of transaction failure
  char  *start_time; // Start time stamp
  // u_ns_ts_t  end_time;   // Start time stamp
  // char *script_name;
  // int nvm_id;
  // int sess_inst; // Take from vptr
  union
  {
   UserTracePageInfo *page_info;       // Additional node info if any or NULL
  };
  unsigned char sess_status;
  //This is to hold page id or session id depending upon type of node
  unsigned int sess_or_page_id; 
  //This is to hold page inst or session inst depending upon type of node
  unsigned int sess_or_page_inst;
  
  struct user_trace_node *next;
} UserTraceNode;

/*This is for per vuser
 * There can be many users from one group*/
typedef struct
{
  VUser *vptr;
  void *ut_head; // We are making it void ptr so that other files need not include header file
  void *ut_tail; // We are making it void ptr so that other files need not include header file
  unsigned short flags;
  unsigned short curr_node_id;
  // struct userTraceData *next; // For future
} userTraceData;

/*This is for group.
 * It will be malloced total number of groups*/
typedef struct
{
  userTraceData *utd_head;
  short num_users;
} userTraceGroup;

#ifndef CAV_MAIN
extern userTraceGroup *user_trace_grp_ptr;
#else
extern __thread userTraceGroup *user_trace_grp_ptr;
#endif

/*Added for line-break*/
extern char line_break[];
extern int line_break_length;
/*Enable/Diable vuser tracing*/
extern void ut_vuser_check_and_enable_tracing(VUser *vptr);
extern void init_vuser_grp_data();
extern void ut_vuser_check_and_disable_tracing(VUser *vptr);
extern void process_user_tracing (int fd, User_trace *user_trace_msg);

/*Functions for adding nodes*/
extern void ut_add_start_session_node(VUser *vptr);
extern void ut_add_internal_page_node(VUser *vptr, char *page_name, char *url, char *protocol, int page_think_time);
extern void ut_add_page_node(VUser *vptr); 
extern void ut_add_param_node(UserTraceNode *page_node);
extern void ut_add_param_used_node (VUser *vptr);
extern void ut_add_validation_node(UserTraceNode *page_node); 
extern void ut_add_end_session_node(VUser *vptr);

extern void ut_update_param(UserTraceNode *page_node, char *name, char *value, int type, short ord, int name_len, int value_len);
extern void ut_update_validation(UserTraceNode *page_node, char *detail, char *id, char *status);
extern void ut_update_url_values (connection *cptr, int cache_check); 
extern void ut_update_page_values (VUser *vptr, int page_resp_time, u_ns_ts_t now);
extern void ut_update_page_think_time(VUser *vptr);

/*Resp related code*/
extern void ut_update_req_file(VUser *vptr, int http_size, int num_vectors, struct iovec *vector_ptr);
extern void ut_update_rep_file(VUser *vptr, int resp_size, char *resp);
extern void ut_update_rep_body_file(VUser *vptr, int resp_size, char *resp);
extern void ut_update_req_body(UserTraceNode *page_node, char *value, int value_len); 
//extern char *make_req_file(int http_size, int num_vectors, struct iovec *vector_pt);
extern void start_vuser_trace (User_trace *msg, u_ns_ts_t now);
extern void process_vuser_tracing_req (int cmd_fd, User_trace *vuser_trace_msg);
extern void process_vuser_tracing_rep (int child_fd, User_trace *vuser_trace_msg);
extern void ut_vuser_check_and_enable_tracing(VUser *vptr);
extern int kw_set_g_user_trace(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern void ut_update_http_status_code (connection *cptr); 
extern void ut_free_all_nodes(userTraceData *utd_node);
extern void ut_update_rep_file_for_page_dump(VUser *vptr, int resp_size, char *resp);
extern void ut_update_rep_file_line_break_for_page_dump(connection *cptr);
#endif
