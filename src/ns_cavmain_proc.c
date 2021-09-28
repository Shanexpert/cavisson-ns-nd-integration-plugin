/******************************************************************************************************
 * Name                :  ns_cavmain_proc.c
 * Purpose             :  Process SM Request of CMON
 * Author              :  Devendar Jain/Anup Singh
 * Intial version date :  29/07/2020
 * Last modification date:
*******************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "nslb_util.h"
#include "nslb_alloc.h"
#include "nslb_thread_queue.h"

#include "ns_tls_utils.h"

#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_trace_level.h"
#include "ns_cavmain.h"
#include "nslb_map.h"
#include "ns_log.h"
#include "nslb_sock.h"
#include "signal.h"
#include <fcntl.h>

/*****************************************************************************/
#define CM_TEST_STOPPED		1
#define NS_INVALID_FD		-1


#define CM_COMPLETE_REQ_RECV	0
#define CM_CONT_READING         1
//#define NORM_ID_NOT_FOUND       -2
#define CM_INVALID_REQ          -2
#define NS_CMD_SIZE		1024
#define NS_MSG_BUF_SIZE		5*1024
#define NS_RTG_DATA_LEN		15
#define NS_TEST_START_LEN	18
#define SM_MIN_FIELD_COUNT	2

#define NEW_LINE_CHAR           '\n'
#define SPACE_CHAR              ' '
#define SM_REQ_DELIM            ";"


#define SM_INIT_REQ_TXT		"cm_init_monitor"
#define SM_STOP_REQ_TXT		"cm_stop_monitor"
#define SM_UPDATE_REQ_TXT	"cm_update_monitor"
#define SM_PAUSE_TXT		"cg_pause"
#define SM_RESUME_TXT		"cg_resume"
#define SM_CONFIG_REQ_TXT	"cm_config"
#define CM_ACK			":status="
#define CM_STATUS		"CM_STATUS"
#define CM_RESULT		"CM_RESULT"

#define SM_UPDATE_TXT		"cg_update"

#define NS_OK_TXT		"OK"
#define NS_ERR_TXT		"ERROR"
#define NS_STOPPED_TXT		"STOPPED"
#define NS_SUCCESS_TXT		"SUCCESS"
#define NS_COMPLETED_TXT        "COMPLETED"
#define CM_EXIT_STATUS_OK	"_CM_STATUS_SUCCESS_"
#define CM_EXIT_STATUS_NOK      "_CM_STATUS_ERROR_"
#define NS_POPEN_READ		"re"
#define NS_ZERO_TXT		"0"
#define NS_RTG_DATA_TXT		"RTG_DATA_START|"
#define NS_START_CMD		"nsu_start_test"
#define NS_STOP_CMD		"nsu_stop_test"
#define NS_TEST_START		"Starting Test run "

#define CM_MSG_KEY			"msg="
#define TEST_MONITOR_CONFIG_KWD		"TEST_MONITOR_CONFIG"
#define NS_DEFAULT_SCENARIO_PATH       "/default/default/scenarios/"
#define NS_FAILED_TXT			"Failed to process request"
#define NS_DEFAULT_SCRIPT_PATH         "/default/default/scripts/"

/*check space for 1st and last char and return error if found*/
#define RETURN_IF_SPACE_CHAR_FOUND(vlaue_ptr, len)      \
{                                               \
  if( (SPACE_CHAR == vlaue_ptr[NS_ZERO]) || (SPACE_CHAR == vlaue_ptr[len - 1]) ) \
    return CM_INVALID_REQ; \
}
#define NS_HANDLE_EPOLL_FAILURE(errno)\
{\
  switch(errno)\
  {\
    case EBADF:\
    perror("Bad epoll fd");\
    NSDL1_MESSAGES(NULL, NULL,"Bad epoll fd");\
    break;\
    case EFAULT:\
    perror("The memory area");\
    NSDL1_MESSAGES(NULL, NULL,"The memory area");\
    break;\
    case EINVAL:\
    perror("epoll fd is not valid");\
    NSDL1_MESSAGES(NULL, NULL,"epoll fd is not valid");\
    break;\
    case EINTR:\
    break;\
    default:\
    perror("epoll_wait() failed");\
    NSDL1_MESSAGES(NULL, NULL,"epoll_wait() failed");\
    break;\
  }\
}

#define MAX_MON_NAME_LENGTH     128

static __thread int tls_is_init_thread;
static __thread pthread_t tls_id;
extern NSLBMap *sm_map;

#ifndef CAV_MAIN
extern int g_avgtime_size;
#else
extern __thread int g_avgtime_size;
#endif
static int cur_child_idx;


typedef enum {

  SM_INIT_REQ,
  SM_STOP_REQ,
  SM_UPDATE_REQ,
  SM_CONFIG_REQ,
  SM_PAUSE_RESUME_REQ,
  SM_CLOSE_REQ, 
  MAX_NUM_SMREQ
} SmReqIdx;

/*
* SM_INIT -->Fields Count => 8
* SM_STOP -->Fields Count => 1
* UPDATE_UPDATE_REQ -->Fields Count  =>4
* SM_CONFIG-->Fields Count => 2
* SM_PAUSE_RESUME_REQ,-->Fields Count => 3
*/
int max_fields_count[MAX_NUM_SMREQ] = {8, 1, 4, 2, 3};

#define CM_GET_SM_MAP() sm_map

typedef int (*smreq_func_ptr)(Msg_com_con*);

smreq_func_ptr smreq_func_arr[MAX_NUM_SMREQ];


NSError g_ns_err;

char g_scratch_buff[NS_MSG_BUF_SIZE];
int exit_status_arr[2] = {CM_ERROR, CM_TEST_STOPPED};

#define NS_GET_SCRATCH_BUF()	g_scratch_buff
#define NS_RESET_SCRATCH_BUF()    memset(g_scratch_buff, 0, NS_MSG_BUF_SIZE)


#define NS_THROW(ns_err, err_code, err_msg) \
{ \
  ns_err.code = err_code; \
  strcpy(ns_err.msg, err_msg);\
}


static char g_read_buf[READ_BUF_SIZE];

#define NS_SET_READ_BUF(read_buf)\
{ \
  NSDL2_MESSAGES(NULL, NULL,  "recieved data=(%s)", read_buf); \
  memset(g_read_buf, 0, READ_BUF_SIZE); \
  strcpy(g_read_buf, read_buf); \
  NSDL2_MESSAGES(NULL, NULL,  "data set=(%s)", g_read_buf); \
}

#define NS_GET_READ_BUF() g_read_buf
#define NS_DELETE_SMREQ_FROM_MAP(sm_req) \
{\
  NSDL2_MESSAGES(NULL, NULL,"sm_req to delete from MAP=%p", sm_req);\
  if((nslb_map_delete(CM_GET_SM_MAP(), sm_req->req_field_arr[SM_MONITOR_ID].value)) != CM_SUCCESS )\
  { \
    NSDL2_MESSAGES(NULL, NULL,"Unable to delete entry(%s) from MAP", sm_req->req_field_arr[SM_MONITOR_ID].value);\
  }\
  NSLB_FREE_AND_MAKE_NULL(sm_req, "Freeing SM Req ptr", -1, NULL);\
}
static int rcv_amt;

/* find key in MAP*/
#define CM_GET_SM_REQ(sm_req, mon_id)\
{\
   void *out_ptr;\
   NSLBMap *nslb_map_ptr = CM_GET_SM_MAP();\
   if(nslb_map_find(nslb_map_ptr, mon_id, &out_ptr) < 0) {\
      NSDL2_MESSAGES(NULL, NULL, "Unable to find mon_id in MAP");\
      return CM_ERROR; \
   }\
   sm_req = (SmRequest*)out_ptr;\
}

/*****************************************************************************/
static void *g_cm_tm_obj;
static void *g_cm_tm_obj_cleanup;

static inline void ns_fill_buff(char* buff, size_t size, char* monitor_index, char* status, char* msg)
{ 
   NSDL2_MESSAGES(NULL, NULL,  "Method called. monitor_index=%s, status=%s msg=%p", monitor_index, status, msg);
   memset(buff, 0, size);
   sprintf(buff, "%s%s%s", monitor_index, CM_ACK, status);
   if(!msg)
    sprintf(buff, "%s\n", buff);
   else
    sprintf(buff, "%s%s%s%s\n", buff, SM_REQ_DELIM, CM_MSG_KEY, msg);
   NSDL2_MESSAGES(NULL, NULL, "reply=%s", buff);
}


void cm_send_reply(Msg_com_con *mccptr, char* reply)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr=%p reply=%p", mccptr, reply);
  if(!reply || !mccptr)
    return;
  
  SmRequest *sm_req = (SmRequest*)mccptr->vptr; 
  NSDL2_MESSAGES(NULL, NULL,"msg to send[%s] len=%d sm_req=%p", reply, strlen(reply), sm_req);
  
  if(!sm_req){
    NSDL2_MESSAGES(NULL, NULL,"Start writing with NO lock");
    write_msg(mccptr, reply, strlen(reply), 0, 0);
    return;
  }
  pthread_mutex_lock(&sm_req->lock);
  NSDL2_MESSAGES(NULL, NULL,"lock accquired");
  write_msg(mccptr, reply, strlen(reply), 0, 0);
  NSDL2_MESSAGES(NULL, NULL,"write_msg done");
  if(!(mccptr->write_bytes_remaining))
  {
     NSDL2_MESSAGES(NULL, NULL,"mccptr->write_bytes_remaining=%d", mccptr->write_bytes_remaining);
     NSDL2_MESSAGES(NULL, NULL,"msg sent[%s] successfully. sm_req->mccptr=%p", reply, sm_req->mccptr);
     if(!sm_req->mccptr)
     {
       CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);    
     }
  }
  pthread_mutex_unlock(&sm_req->lock);
}

void cm_process_cmd_output(SmRequest *sm_req, char *output, int *amt_written)
{
   NSDL2_MESSAGES(NULL, NULL,"Method called, sm_req=%p output=%s amt_written=%d", sm_req, output, *amt_written);
   //Trace for output 
   //ToDo:check for TR NUMBER
   char* str;
   if((str = strstr(output, NS_TEST_START)))   
   {
     if(NEW_LINE_CHAR !=  output[*amt_written - 1])
        return;
     int len = strlen(str);
     str[len - 1] = '\0';
     NSDL2_MESSAGES(NULL, NULL,"now str=%s", str);
     *amt_written = 0;
     int tr_num;
     NSDL2_MESSAGES(NULL, NULL,"now amt_written=%d", *amt_written); 
     NSDL2_MESSAGES(NULL, NULL, "tr num =%s", str + 18);
     if(nslb_atoi(str + 18, &tr_num) == CM_SUCCESS)
     {
        sm_req->child_id = tr_num;
        NSDL2_MESSAGES(NULL, NULL, "sm_req->child_id=%d", sm_req->child_id);
        if(!sm_req->mccptr) {
          cm_run_stop_cmd(sm_req);
          return; 
        }
        char msg[NS_CMD_SIZE];
        ns_fill_buff(msg, NS_CMD_SIZE, sm_req->req_field_arr[SM_MONITOR_INDEX].value, NS_SUCCESS_TXT, "Monitor started successfully");
        cm_send_reply(sm_req->mccptr, msg);     
     }
     return;
   }

   if((str = strstr(output, NS_RTG_DATA_TXT))) /*!(strncmp(output, NS_RTG_DATA_TXT, NS_RTG_DATA_LEN)))*/
   {
     if(NEW_LINE_CHAR !=  output[*amt_written - 1])
        return;
     NSDL2_MESSAGES(NULL, NULL, "output %s", output);
     cm_send_reply(sm_req->mccptr, /*output*/str + NS_RTG_DATA_LEN);     
   }
   *amt_written = 0; 
   NSDL2_MESSAGES(NULL, NULL,"now amt_written=%d", *amt_written); 
}

void cm_web_page_audit_cleanup(void *args)
{
  
  NS_INIT_GROUP_N_GLOBAL_SETTINGS();
  global_settings->ns_trace_level = 1;
  group_default_settings->debug = 0xFFFFFFFF;
  group_default_settings->module_mask = MM_ALL;
  SM_SET_CAVGEN_VERSION(CAVGEN_VERSION_2);

  ns_tls_init(VUSER_THREAD_LOCAL_BUFFER_SIZE);
  tls_id = pthread_self();

  char *sm_mon_id = (char*)args;
  char cmd_buf [2048 + 1];
  char rbu_param_path [512 + 1];
  char controller_name[1024  + 1] = "";
  char *controller_name_ptr = NULL;

  cmd_buf [0] = 0;
  rbu_param_path [0] = 0;

  NSDL2_MESSAGES(NULL, NULL,"Method called, thread_id = %d,sm_mon_id = %s", tls_id, sm_mon_id);
  //STOP VNC and CLEAN Profile
  if((controller_name_ptr = strrchr(g_ns_wdir, '/')) != NULL)
    strcpy(controller_name, controller_name_ptr + 1);
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  sprintf(rbu_param_path, "%s/default/default/%s/.rbu_parameter.csv", GET_NS_TA_DIR(), sm_mon_id);

  NSDL3_RBU(NULL, NULL, "controller_name = %s, rbu_param_path with NS_TA_DIR(%s) = %s", controller_name, GET_NS_TA_DIR(), rbu_param_path);

  //For Chrome
  sprintf(cmd_buf, "vid=`cat %s 2>/dev/null |awk -F',' '{printf $3\",\"}'` "
                   " && nsu_auto_gen_prof_and_vnc -o stop -N $vid -P CavTest-%s-cavisson-TR%d-1- -w -B 1 >/dev/null 2>&1",
                    rbu_param_path, controller_name, testidx);
  NSDL2_RBU(NULL, NULL, "cmd_buf = %s", cmd_buf);

  nslb_system2(cmd_buf);

  //For Firefox
  sprintf(cmd_buf, "vid=`cat %s |awk -F',' '{printf $3\",\"}'` "
                   " && nsu_auto_gen_prof_and_vnc -o stop -N $vid -P CavTest-%s-cavisson-TR%d-0- -w -B 0 >/dev/null 2>&1",
                    rbu_param_path, controller_name, testidx);

  NSDL2_RBU(NULL, NULL, "cmd_buf = %s", cmd_buf);

  nslb_system2(cmd_buf);

  NS_FREE_GROUP_N_GLOBAL_SETTINGS();
}

/*thread function*/
void cm_run_command(void *args)
{
   
  NS_INIT_GROUP_N_GLOBAL_SETTINGS();
  global_settings->ns_trace_level = 1;
  group_default_settings->debug = 0xFFFFFFFF;
  group_default_settings->module_mask = MM_ALL;
  SM_SET_CAVGEN_VERSION(CAVGEN_VERSION_1);

  ns_tls_init(VUSER_THREAD_LOCAL_BUFFER_SIZE);
  tls_id = pthread_self();
  NSDL2_MESSAGES(NULL, NULL,"Method called. thread_id=%d", tls_id);
  CMThreadArgs *targs = (CMThreadArgs*)args;
  int amt_written = 0;
  char tmp[MAX_CMD_LEN + 1] = "";
  NSDL2_MESSAGES(NULL, NULL,"thread_id=%d cmd=%s", tls_id, targs->cmd_buf);
  if(strstr(targs->cmd_buf, NS_START_CMD))
    tls_is_init_thread = 1;
  else
    tls_is_init_thread = 0;

 
  FILE *app = popen(targs->cmd_buf, NS_POPEN_READ);
  int fd; 
  if ((app == NULL) || ((fd = fileno(app)) == NS_INVALID_FD) || (fcntl(fd, F_SETFL, O_NONBLOCK) < NS_ZERO))
  {
    sprintf(targs->cmd_buf, "ERROR: file=%p => fd =%d  Error in executing command [%s]. Error = %s", app, fd, targs->cmd_buf , nslb_strerror(errno));
    NSDL2_MESSAGES(NULL, NULL,"thread_id=%d calling cm_send_reply. app=%p", tls_id, app);
    cm_send_reply(targs->smReq->mccptr, targs->cmd_buf);
    NS_FREE_GROUP_N_GLOBAL_SETTINGS();
    return;
  }
 
  //FILE* fread = fdopen(fd, "r"); 
  NSDL2_MESSAGES(NULL, NULL,"thread_id=%d tls_is_init_thread=%d fd=%d app=%p", tls_id, tls_is_init_thread, fd, app);
  int exit_status = CM_ERROR; 
  //avoid loop in case of nsu_stop_test command
  while(NS_WAIT_FOREVER && tls_is_init_thread)
  { 

    if(NULL == fgets(tmp, MAX_CMD_LEN, /*fread*/app))
    {
       continue;
    }
    int read_bytes = strlen(tmp); 

    //int read_bytes = read(fd, tmp, MAX_CMD_LEN);
    tmp[read_bytes] = '\0';
    int exit_while_loop = NS_ZERO;
    if(strstr(tmp, CM_EXIT_STATUS_OK) || (0 == read_bytes))
    {
        exit_while_loop = 1;
        exit_status = CM_SUCCESS;
        NSDL2_MESSAGES(NULL, NULL,"thread_id=%d exit_status=%d read data=%s len=%d opcode=%d exit_while_loop=%d", tls_id,  exit_status, tmp, read_bytes, targs->smReq->opcode, exit_while_loop);
    }
    else if(strstr(tmp, CM_EXIT_STATUS_NOK))
    {
        exit_while_loop = 1;
        exit_status = exit_status_arr[targs->smReq->opcode];
        NSDL2_MESSAGES(NULL, NULL,"thread_id=%d exit_status=%d read data=%s len=%d opcode=%d exit_while_loop=%d", tls_id,  exit_status, tmp, read_bytes, targs->smReq->opcode, exit_while_loop);
    }
    else
      exit_while_loop = 0;

    if(exit_while_loop){
      break;// from while loop
    }
         
    if(read_bytes > NS_ZERO) 
    {  
 
      NSDL2_MESSAGES(NULL, NULL,"thread_id=%d tmp=%s MAX_CMD_LEN=%d read_bytes=%d", tls_id, tmp, MAX_CMD_LEN, read_bytes);
     // int toDelete = amt_written;
      amt_written += snprintf(targs->cmd_buf + amt_written , MAX_OUTPUT_BUF_SIZE - amt_written, "%s", tmp);
      NSDL2_MESSAGES(NULL, NULL,"thread_id=%d amt_written=%d  targs->cmd_buf[amt_written]=[%c]", tls_id, amt_written, targs->cmd_buf[amt_written - 1]);
      NSDL2_MESSAGES(NULL, NULL,"thread_id=%d calling cm_process_cmd_output.", tls_id);
      /*if(strstr(targs->cmd_buf, NS_TEST_START))  //done for UT
      { 
         amt_written = toDelete;
         amt_written += snprintf(targs->cmd_buf + amt_written , MAX_OUTPUT_BUF_SIZE - amt_written, "%s\n", "20/10/14 03:05:21.0852: [29369]:  WARNING:       mongoc: Falling back to malloc for counters" );
         amt_written += snprintf(targs->cmd_buf + amt_written , MAX_OUTPUT_BUF_SIZE - amt_written, "%s\n", "Starting Test run 4928" );
      } */
     /*if(strstr(targs->cmd_buf, NS_RTG_DATA_TXT))  //done for UT
      { 
         amt_written = toDelete;
         amt_written += snprintf(targs->cmd_buf + amt_written , MAX_OUTPUT_BUF_SIZE - amt_written, "%s\n", "20/10/14 03:05:21.0852: [29369]:  WARNING:       mongoc: Falling back to malloc for counters" );
         amt_written += snprintf(targs->cmd_buf + amt_written , MAX_OUTPUT_BUF_SIZE - amt_written, "%s\n", "RTG_DATA_START|119:0:mon_name>page1|0.000000 -1.000000 0.000000 0.000000 0.000000 -1.000000 0.000000 0.000000 5.000000 5.000000 5.000000 1.000000 87.000000 87.000000 87.000000 1.000000 0.000000 -1.000000 0.000000 0.000000 195.000000 195.000000 195.000000 1.000000 12.000000 12.000000 12.000000 1.000000 302.000000 302.000000 302.000000 1.000000 1.000000 4294967295.000000 18446744073709551616.000000 18446744073709551616.000000 404.000000 18446744073709551616.000000 403.000000 1836.000000 59162.000000 60998.000000 200.000000 1.000000 0.000000" );
      }*/
      cm_process_cmd_output(targs->smReq, targs->cmd_buf, &amt_written);
    }

    usleep(100*1000);
  }
  NSDL2_MESSAGES(NULL, NULL,"thread_id=%d exit_status=%d out from while loop, tls_is_init_thread =%d targs->smReq->opcode=%d cmd=%s", tls_id, exit_status, tls_is_init_thread, targs->smReq->opcode,  targs->cmd_buf);
  pclose(app);
  close(fd);
  if(!tls_is_init_thread)
  { 
    NS_FREE_GROUP_N_GLOBAL_SETTINGS();
    return;
  }   
  Msg_com_con *mccptr = targs->smReq->mccptr;
  NSDL2_MESSAGES(NULL, NULL,"thread_id=%d mccptr=%p", tls_id, mccptr);
  targs->smReq->mccptr = NULL;
  char msg[NS_CMD_SIZE];
  switch(exit_status)
  {
    case CM_SUCCESS:
    case CM_TEST_STOPPED:
    ns_fill_buff(msg, NS_CMD_SIZE, targs->smReq->req_field_arr[SM_MONITOR_INDEX].value, NS_STOPPED_TXT, CM_MON_STOPPED);
    break;

    case CM_ERROR:
    ns_fill_buff(msg, NS_CMD_SIZE, targs->smReq->req_field_arr[SM_MONITOR_INDEX].value, NS_ERR_TXT, CM_MON_STOPPED_ERR);
    break;
  }
  cm_send_reply(mccptr, msg);     
  int status; 
  if((status = nslb_map_delete(CM_GET_SM_MAP(), targs->smReq->req_field_arr[SM_MONITOR_ID].value)) != CM_SUCCESS )
  {
     NSDL2_MESSAGES(NULL, NULL,"thread_id=%d Unable to delete entry(%s) from MAP. err=%d", tls_id, targs->smReq->req_field_arr[SM_MONITOR_ID].value, status);
  }
  NSDL2_MESSAGES(NULL, NULL,"Freeing SM Req ptr=%p", targs->smReq);
  NSLB_FREE_AND_MAKE_NULL(targs->smReq, "Freeing SM Req ptr", -1, NULL);
  NSDL2_MESSAGES(NULL, NULL,"Freeing SM Req Done");
  if(mccptr)
    mccptr->vptr = NULL;

  NS_FREE_GROUP_N_GLOBAL_SETTINGS();
}

int cm_init_thread_manager(int trace_fd)
{
   /*set max limit at run time*/
   g_cm_tm_obj = nslb_tm_init(1, 0, 1, 0); //TODO: configuration
   return nslb_tm_config(g_cm_tm_obj, (void *)cm_run_command, trace_fd);
}

int cm_init_thread_manager_v2(int trace_fd)
{
   /*set max limit at run time*/
   g_cm_tm_obj_cleanup = nslb_tm_init(1, 0, 1, 0); //TODO: configuration
   return nslb_tm_config(g_cm_tm_obj_cleanup, (void *)cm_web_page_audit_cleanup, trace_fd);
}

int cm_run_command_in_thread(SmRequest *smReq, char *cmd)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. smReq=%p cmd=%s", smReq, cmd);
  CMThreadArgs *args;

  NSLB_MALLOC_AND_MEMSET(args, sizeof(CMThreadArgs), "CMThreadArgs", -1, NULL);

  strcpy(args->cmd_buf, cmd); 
  args->smReq = smReq;
  return nslb_tm_exec(g_cm_tm_obj, (void *)args, sizeof(CMThreadArgs));
}

int cm_wpa_cleanup_in_thread(char *sm_mon_id, int size)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. sm_mon_id = %s, size = %d", sm_mon_id, size);

  return nslb_tm_exec(g_cm_tm_obj_cleanup, (void *)sm_mon_id, size);
}
/*****************************************************************************/
void cm_wait_forever()
{
   NSDL1_PARENT(NULL, NULL,"Method called");
   struct epoll_event *epev;

   NSLB_MALLOC(epev, sizeof(struct epoll_event)*NS_EPOLL_MAXFD, "epev", -1, NULL);

   int event_count;

   int count;
   int timeout = 600000;
   while(NS_WAIT_FOREVER)
   {

    NSDL1_PARENT(NULL, NULL, " wait on epoll_fd(%d) event_count=%d", CM_GET_EPOLL_FD(), event_count);
    errno = 0; 
    event_count = epoll_wait(CM_GET_EPOLL_FD(), epev, NS_EPOLL_MAXFD, timeout);
    NSDL1_PARENT(NULL, NULL, "now event_count=%d", event_count);
    int events;
    if(NS_ZERO == event_count)
    {
      NSDL1_MESSAGES(NULL, NULL, "epoll_wait() timeout for control connection. No event found.. continue");
      continue; 
    }
    else if(event_count > NS_ZERO)
    {
      for(count = 0; count < event_count; ++count)
      {
        Msg_com_con* mccptr = (Msg_com_con*)epev[count].data.ptr;
        events = epev[count].events;
        if(mccptr) {
          NSDL1_PARENT(NULL, NULL, "events=%d  mccptr[%p]->con_type=%d mccptr->fd=%d", events, mccptr, mccptr->con_type, mccptr->fd);
          switch(mccptr->con_type)
          {
              case NS_STRUCT_TYPE_LISTEN:
              /* Connection came from one of the tools like CMON */
              //ToDo: Error Handling required
              accept_connection_from_tools();
              continue;

              case NS_STRUCT_TYPE_TOOL:
              //trace and error handling???
              cm_handle_event_from_tool(mccptr, events);
              continue;
          
              case NS_STRUCT_TYPE_LISTEN_CHILD:
              NSDL1_MESSAGES(NULL, NULL, "calling accept_connection_from_child");
              accept_connection_from_child();
              break;
              
              case NS_STRUCT_TYPE_CHILD:
              //trace and error handling???
              cm_handle_event_from_child(mccptr, events);
              break;
              //ToDo: call read_msg() from waitforever.c file
              //continue;

              default:
              //error handling
              continue;
          }
        }
      }
    }
    else
   {
     NS_HANDLE_EPOLL_FAILURE(errno)
   }
 }
}

int cm_check_for_epollerr_and_partial_write(Msg_com_con *mccptr, int events)
{

  NSDL2_PARENT(NULL, NULL, "mccptr = %p, events = %d", mccptr, events);
  if (NS_INVALID_FD == mccptr->fd)
  {
    NS_PUT_ERR_ON_STDERR("Invalid mccptr->fd=-1")
    return CM_ERROR;
  }
  if (events & EPOLLERR) {
    
    sprintf(NS_GET_ERR_BUF(), "EPOLLERR occured on sock %s. error = %s for control connection", msg_com_con_to_str(mccptr), strerror(errno));
    NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    //CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
    cm_handle_client_close(mccptr); 
    return CM_ERROR;
  }

  if (events & EPOLLHUP) {
    sprintf(NS_GET_ERR_BUF(), "EPOLLHUP occured on sock %s. error = %s for control connection", msg_com_con_to_str(mccptr), strerror(errno));
    NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    //CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
    cm_handle_client_close(mccptr);
    return CM_ERROR;
  }

  NSDL3_MESSAGES(NULL, NULL,"mccptr->state=%d", mccptr->state);
  /* handle partial write */
  if (events & EPOLLOUT){
    if (mccptr->state & NS_STATE_WRITING) {
      write_msg(mccptr, NULL, NS_ZERO, NS_ZERO, CONTROL_MODE);
      NSDL3_MESSAGES(NULL, NULL,"mccptr->vptr=%p", mccptr->vptr);
      if(!mccptr->vptr)
        CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
    }
    else {
      NSDL3_MESSAGES(NULL, NULL, "Write state not `writing', still we got EPOLLOUT event on fd = %d for control connection", mccptr->fd);
    }
  }
  return CM_SUCCESS;
}

int cm_handle_event_from_child(Msg_com_con *mccptr, int events)
{

  NSDL2_PARENT(NULL, NULL, "mccptr = %p, events = %d", mccptr, events);
  if(CM_ERROR ==  cm_check_for_epollerr_and_partial_write(mccptr, events))
    return CM_ERROR;

  char* msg;
  if(events & EPOLLIN)
  {
    if(NULL == (msg = read_msg(mccptr, &rcv_amt, CONTROL_MODE)))
    {
       CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE); 
       return CM_ERROR;
    }
    
  //ToDo: process msg 
  //return cm_process_child_request(mccptr);
    
  }
  NSDL2_PARENT(NULL, NULL, "rcv_amt=%d", rcv_amt);
  return cm_process_child_msg(mccptr,(CM_MON_REQ*)msg); //process_msg_From_Child();
}

int cm_handle_event_from_tool(Msg_com_con *mccptr, int events)
{

  NSDL2_PARENT(NULL, NULL, "mccptr = %p, events = %d", mccptr, events);
  if(CM_ERROR ==  cm_check_for_epollerr_and_partial_write(mccptr, events))
    return CM_ERROR;
  int status;
  if(events & EPOLLIN)
  {
    if(CM_ERROR == (status = cm_handle_read(mccptr)))
    {
       CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE); 
    }
  }
  NSDL3_MESSAGES(NULL, NULL,"status=%d", status);
  return status;
}

int cm_handle_read(Msg_com_con *mccptr)
{
   
   NSDL3_MESSAGES(NULL, NULL,"Method called. mccptr=%p", mccptr);
   if (NS_INVALID_FD == mccptr->fd) {
     sprintf(NS_GET_ERR_BUF(), "fd is -1 for %s.. returning", msg_com_con_to_str(mccptr));
     NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
     NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
     return CM_ERROR;  // Issue - this is misleading as it means read is not complete
   }
 
   NS_RESET_ERR() 
   NSDL3_MESSAGES(NULL, NULL,"mccptr->state=%p", mccptr->state);
   //default satate = 0
   if (!(mccptr->state & NS_STATE_READING)) // Method called for first time to read message
   {
     NSDL1_MESSAGES(NULL, NULL, "Method called to read message for the first time. %s", msg_com_con_to_str(mccptr));
     mccptr->read_offset = 0;
     mccptr->read_bytes_remaining = mccptr->read_buf_size;
   }

   int bytes_read;
   while(1)
   {
      NSDL2_MESSAGES(NULL, NULL, "Reading rest of the message. offset = %d, bytes_remaining = %d, %s", mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));
      //we will take read_buf_size as max size, the default size would be 1k 
      if((bytes_read = read(mccptr->fd, (mccptr->read_buf + mccptr->read_offset), mccptr->read_bytes_remaining)) <= NS_ZERO)
      {
         if (bytes_read == NS_ZERO) {
           NSDL2_MESSAGES(NULL, NULL,"Handle client Close. sm_req=%p bytes_read=%d", (SmRequest*)(mccptr->vptr), bytes_read);
           /*In case of reading 0 bytes print success instead of error string*/
           return cm_handle_client_close(mccptr);
         }

         if(errno == EAGAIN)
         {
           mccptr->state |=  NS_STATE_READING;
           NSDL2_MESSAGES(NULL, NULL,"EAGAIN recieved");
	   break; //brek from while loop
         }
         NSDL2_MESSAGES(NULL, NULL, "Complete message is not available for read. offset = %d, bytes_remaining = %d, %s",
                  mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));
         continue;
      }
      else
      {
        mccptr->read_offset += bytes_read;
        mccptr->read_bytes_remaining -= bytes_read;
      }
      NSDL2_MESSAGES(NULL, NULL, "Complete message is not available for read. offset = %d, bytes_remaining = %d, %s", mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));

  } // end while loop

  NSDL2_MESSAGES(NULL, NULL, "Complete message read or EAGAIN occured. Total message size read = %d, %s", mccptr->read_offset, msg_com_con_to_str(mccptr));
  //call cm_parse_and_process_sm_request()  for CMON related read event 
  int status;
  switch((status = cm_parse_and_process_sm_request(mccptr)))
  {
      case CM_COMPLETE_REQ_RECV:
      NSDL2_MESSAGES(NULL, NULL,"Complete data read");
      status = CM_SUCCESS;
      break;
      case CM_CONT_READING:
      NSDL2_MESSAGES(NULL, NULL,"Partial data read[%s], cont..reading", mccptr->read_buf);
      status = CM_SUCCESS;
      break;
      case CM_INVALID_REQ:
      NSDL2_MESSAGES(NULL, NULL,"Invalid Request. Send Failure STATUS to comon");
      cm_send_reply(mccptr, NS_GET_ERR_MSG()); 
      status = CM_ERROR;

  }
  NSDL2_MESSAGES(NULL, NULL,"returning with status=%d", status);
  return status;
}

int cm_process_child_request(Msg_com_con *mccptr)
{
 //START_MSG_FROM_CLIENT
 //STATUS
 //Started TEST
 //
 //TEST RESULT
 return CM_SUCCESS;
}
inline void ns_register_sm_req()
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  smreq_func_arr[SM_INIT_REQ] = cm_process_sm_start_req;
  smreq_func_arr[SM_STOP_REQ] = cm_process_sm_stop_req;
  smreq_func_arr[SM_UPDATE_REQ] = cm_process_sm_update_req;
  smreq_func_arr[SM_CONFIG_REQ] = cm_process_sm_config_req;
  smreq_func_arr[SM_CLOSE_REQ] = cm_process_sm_close_req;
}

inline void ns_register_sm_req_v2()
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  smreq_func_arr[SM_INIT_REQ] = cm_process_sm_start_req_v2;
  smreq_func_arr[SM_STOP_REQ] = cm_process_sm_stop_req;
  smreq_func_arr[SM_UPDATE_REQ] = cm_process_sm_update_req_v2;
  smreq_func_arr[SM_CONFIG_REQ] = cm_process_sm_config_req_v2;
  smreq_func_arr[SM_CLOSE_REQ] = cm_process_sm_close_req_v2;
}



int cm_parse_and_process_sm_request(Msg_com_con *mccptr)
{

  /*CM_INIT|<Monitor-ID>|<Monitor-Name>|<MONITOR-TYPE>|<TIER-NAME>|<SERVER-NAME>|<PARTITION-ID>|<FILE-UPLOAD-URL>\n
    CM_UPDATE_CONFIG|<PARTITION-ID>\n										*/
  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr=%p", mccptr);
  //check if the recieved data conatins EOL char 
  if((mccptr->read_buf[mccptr->read_offset - 1] !=  NEW_LINE_CHAR))
  {
     NSDL2_MESSAGES(NULL, NULL,"Partial message read[%s]. EOL(%c) not at (%d) ", mccptr->read_buf, NEW_LINE_CHAR, ( mccptr->read_offset - 1)); 
     return CM_CONT_READING;
  }
   
  NSDL2_MESSAGES(NULL, NULL,"Complete data read[%p]=>[%s]. Start parsing. mccptr->read_offset=%d  mccptr->state=%d", mccptr->read_buf,  mccptr->read_buf, mccptr->read_offset, mccptr->state);
  mccptr->read_buf[mccptr->read_offset -1] = '\0';
  NSDL2_MESSAGES(NULL, NULL,"after removing eol,  data read[%s]", mccptr->read_buf);
  //reset mccptr state
  mccptr->state &= ~NS_STATE_READING;
  NSDL2_MESSAGES(NULL, NULL,"now  mccptr->state=%d",  mccptr->state); 
  int opcode;
  if(CM_INVALID_REQ == (opcode = cm_parse_request(mccptr)))
     return CM_INVALID_REQ;
  NSDL2_MESSAGES(NULL, NULL,"SM Req of opcode=%d parsed successfully", opcode);
  NSDL2_MESSAGES(NULL, NULL," start req processing");
  //2nd once  request parsed 
  if(cm_process_sm_request(mccptr) != CM_SUCCESS)
  {
    NSDL2_MESSAGES(NULL, NULL,"cm_process_sm_request failed");
    return CM_ERROR;
  }
  return CM_SUCCESS;
}

 
//return -1 for eror and opcode i.e. >= 0 for success
int cm_parse_request(Msg_com_con *mccptr)
{
   NSDL2_MESSAGES(NULL, NULL, "Method called. mccptr=%p", mccptr);
   //the read_buf should conatin at max 3 fields, if we are getting more than 3, then its error, max_size = 4
   int max_fields = SM_REQ_MAX_FIELDS + 1;
   char* sm_req_fields[max_fields];
   //tokenise | separate
   int field_count;

   NS_RESET_SCRATCH_BUF();

   NS_SET_READ_BUF(mccptr->read_buf)

   //set defult error message without Monitor Index
   sprintf(NS_GET_SCRATCH_BUF(),"%s%s%s%s%s(%s).", CM_ACK, NS_ERR_TXT, SM_REQ_DELIM, CM_MSG_KEY, NS_FAILED_TXT, NS_GET_READ_BUF());
  
   if((field_count = nslb_get_tokens(mccptr->read_buf, sm_req_fields, SM_REQ_DELIM, max_fields)) < SM_MIN_FIELD_COUNT)
   {
      NSDL2_MESSAGES(NULL, NULL, "field_count=%d", field_count);
      NSDL2_MESSAGES(NULL, NULL, "err=%s",  NS_GET_SCRATCH_BUF());
      sprintf(NS_GET_ERR_BUF(),"%sThe SM request contains %d < %d minimal required fields\n", NS_GET_SCRATCH_BUF(), field_count, SM_MIN_FIELD_COUNT);
      NSDL2_MESSAGES(NULL, NULL,"Now %s", NS_GET_ERR_MSG()); 
      return CM_INVALID_REQ;
   }

   char sm_req_val[SM_REQ_MAX_FIELDS][256] ;
 
   for(int count =0 ; count < SM_REQ_MAX_FIELDS; ++count)
    memset(sm_req_val[count], 0, 256);
  
   /*in it to avoid space*/
   //memset(sm_req_val, '\0', SM_REQ_MAX_FIELDS*256);
   //memset to avoid unnecessary space in rows which are not being populated during sm_get_req_val()
   if(CM_INVALID_REQ ==(field_count = sm_get_req_val(sm_req_fields, field_count, sm_req_val)))
   { //ToDO
     sprintf(NS_GET_ERR_BUF(),"%s\n", NS_GET_ERR_MSG());
     return CM_INVALID_REQ;
   }

   //set default err message including Monitor Index
   sprintf(NS_GET_ERR_BUF(),"%s%s", sm_req_val[SM_MONITOR_INDEX], NS_GET_SCRATCH_BUF());

   //check for scenario file
   struct stat stat_buf;
   NS_SET_SCENARIO_PATH(sm_req_val[SM_MONITOR_ID])
   int status;
   NSDL2_MESSAGES(NULL, NULL,"scenario=%s", NS_GET_SCENARIO_PATH());
   if((status = stat(NS_GET_SCENARIO_PATH(), &stat_buf)) || (NS_ZERO == stat_buf.st_size))
   {    
     sprintf(NS_GET_ERR_BUF(),"%sScenario file not accessible or file is empty=%s\n", NS_GET_ERR_MSG(), NS_GET_SCENARIO_PATH());
     NSDL2_MESSAGES(NULL, NULL,"err msg=%s stat_buf.st_size=%d", NS_GET_ERR_MSG(), stat_buf.st_size);
     return CM_INVALID_REQ;
   } 

   //check for scripts
   stat_buf.st_size = 0;
   NS_SET_SCRIPT_PATH(sm_req_val[SM_MONITOR_ID])
   if((status = stat(NS_GET_SCRIPT_PATH(), &stat_buf)) || (NS_ZERO == stat_buf.st_size))
   {  
     sprintf(NS_GET_ERR_BUF(),"%sScript directory not accessible=%s \n", NS_GET_ERR_MSG(), NS_GET_SCRIPT_PATH());
     NSDL2_MESSAGES(NULL, NULL,"err msg=%s stat_buf.st_size=%d", NS_GET_ERR_MSG(), stat_buf.st_size);
     return CM_INVALID_REQ;
   } 


   int opcode;
   //1st: check for opcode
   if(CM_INVALID_REQ == (opcode = cm_get_req_opcode(sm_req_val[SM_OPCODE]))) {
     sprintf(NS_GET_ERR_BUF(),"%sInvalid opcode\n", NS_GET_ERR_MSG() ); 
     return CM_INVALID_REQ; 
   }
   //reduce by one for Req Type 
   --field_count;
   int max_count;
   NSDL2_MESSAGES(NULL, NULL,"opcode=%d", opcode);
   switch(opcode)
   {
     case SM_UPDATE_REQ:
     if(!strcmp(sm_req_val[SM_OPERATION], SM_UPDATE_TXT))
       max_count =  max_fields_count[opcode];
     else if(!(strcmp(sm_req_val[SM_OPERATION], SM_PAUSE_TXT)) || !(strcmp(sm_req_val[SM_OPERATION], SM_RESUME_TXT))) 
       max_count = max_fields_count[SM_PAUSE_RESUME_REQ];
     else {
       //invalid request as it include invalid operation 
       sprintf(NS_GET_ERR_BUF(),"%sInvalid operation=%s\n", NS_GET_ERR_MSG(), sm_req_val[SM_OPERATION]); 
       return CM_INVALID_REQ;
     }
     break;

     default:
     max_count =  max_fields_count[opcode];
   } 

   NSDL2_MESSAGES(NULL, NULL,"The SM request contains field_count =%d max_count=%d", field_count, max_count);
  //check if the request conatins all  required files or dones't include more than required fields
   if(field_count !=  max_count/*max_fields_count[opcode]*/)
   {
      sprintf(NS_GET_ERR_BUF(),"%sThe SM request contains fileds=%d != %d required fields\n", NS_GET_ERR_MSG(), field_count, max_count);
      NSDL2_MESSAGES(NULL, NULL,"%s", NS_GET_ERR_MSG());
      return CM_INVALID_REQ;
   }
   return cm_validate_sm_req(mccptr, opcode, sm_req_val, SM_REQ_MAX_FIELDS); /*-1 for opcode*/
}

int cm_get_req_opcode(char* str)
{
  
  NSDL2_MESSAGES(NULL, NULL, "Method called, Recived SM req=%s", str);
  int opcode;
  if(NS_ZERO == strcmp(str, SM_INIT_REQ_TXT))
    opcode = SM_INIT_REQ;
  else if(NS_ZERO == strcmp(str, SM_STOP_REQ_TXT))
    opcode = SM_STOP_REQ;
  else if(NS_ZERO == strcmp(str, SM_UPDATE_REQ_TXT))
    opcode = SM_UPDATE_REQ;
  else if(NS_ZERO == strcmp(str, SM_CONFIG_REQ_TXT))
    opcode = SM_CONFIG_REQ;
  else {
    NSDL2_MESSAGES(NULL, NULL,"Invalid Request");
    opcode = CM_INVALID_REQ;
  }
  NSDL2_MESSAGES(NULL, NULL, "return with opcode=%d", opcode);
  return opcode;
}

/*opcode >=0: in case of success
   <0   : error  */
int cm_validate_sm_req(Msg_com_con *mccptr, int opcode, char sm_req_val[][256], int arr_size)
{
  
  NSDL2_MESSAGES(NULL, NULL, "Method called, mccptr=%p opcode=%d arr_size=%d", mccptr, opcode, arr_size);
  if(arr_size <= NS_ZERO)
  {
    sprintf(NS_GET_ERR_BUF(), "Internal Error. Wrong size %d <= 0", arr_size);
    NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    return CM_INVALID_REQ;
  }
  NSDL2_MESSAGES(NULL, NULL,"check whether any fields in request conatins spcae at 0th or last index");
  if(CM_INVALID_REQ ==  cm_check_for_space_char(sm_req_val, arr_size)) {
    sprintf(NS_GET_ERR_BUF(),"%sSM Request fields conatins space, either at the begining or end including field\n", NS_GET_ERR_MSG());
    NSDL2_MESSAGES(NULL, NULL,"err msg=%s", NS_GET_ERR_MSG());
    return CM_INVALID_REQ;
  }
  
  /*return if Req is CM_UPDATE_CONFIG */
  if(SM_CONFIG_REQ == opcode)
    return CM_SUCCESS;
   
  char* monitor_id = sm_req_val[SM_MONITOR_ID];
  //validate Monitor ID
  int mon_id_len = strlen(monitor_id);
  NSDL2_MESSAGES(NULL, NULL, "opcode=%d, monitor_id[%s]=>len=%d > %d, monitor_name=%s", opcode, monitor_id, mon_id_len, SM_MON_ID_MAX_LEN, sm_req_val[SM_MONITOR_NAME]);
  if(mon_id_len > SM_MON_ID_MAX_LEN){
     sprintf(NS_GET_ERR_BUF(),"%sMonitor ID length exceeds(%d > %d) \n", NS_GET_ERR_MSG(), mon_id_len, SM_MON_ID_MAX_LEN);
     return CM_INVALID_REQ;
  }
 
  SmRequest *sm_req;
  NSLBMap *nslb_map_ptr = CM_GET_SM_MAP();
  void* out_ptr;
  /* find key in MAP*/
  int norm_id = nslb_map_find(nslb_map_ptr, monitor_id, &out_ptr);
  switch(norm_id)
  {
     case CM_ERROR:
     NSDL2_MESSAGES(NULL, NULL,"Internal error");
     sprintf(NS_GET_ERR_BUF(),"%sSM Request for Monitor ID(%s) can not be processed due to some internal error\n", NS_GET_ERR_MSG(), sm_req_val[SM_MONITOR_ID]);
      return CM_INVALID_REQ;

     case NORM_ID_NOT_FOUND:
     NSDL2_MESSAGES(NULL, NULL,"monitor_id=%s not present in MAP for opcode=%d", monitor_id, opcode);
     if(SM_INIT_REQ != opcode)
     {
       NSDL2_MESSAGES(NULL, NULL,"Invalid req as opcode=%d is other than SM_INIT_REQ", opcode);
       sprintf(NS_GET_ERR_BUF(),"%sInvalid SM Request, as there is No Test case is running for Monitor ID(%s)\n", NS_GET_ERR_MSG(), sm_req_val[SM_MONITOR_ID]);
       return CM_INVALID_REQ; //DISCARD MESSAGE
     }
   
     if(!(sm_req = init_and_get_sm_req(mccptr,  opcode, sm_req_val, SM_REQ_TOT_FIELDS)))
     {
       sprintf(NS_GET_ERR_BUF(),"%sSM Request for Monitor ID(%s) can not be processed due to some internal error\n", NS_GET_ERR_MSG(), sm_req_val[SM_MONITOR_ID]);
       return CM_INVALID_REQ;
     }
           
     if( (norm_id = nslb_map_insert(nslb_map_ptr, sm_req->req_field_arr[SM_MONITOR_ID].value, (void*)sm_req)) < NS_ZERO)
     {
       sprintf(NS_GET_ERR_BUF(), "nslb_map_insert failed. status=%d", norm_id);
       NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
       NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
       NSLB_FREE_AND_MAKE_NULL(sm_req, "Freeing SM Req ptr", -1, NULL);
       return CM_ERROR; // or DISCARD MESSAGE
     }
     break;

     default:
     NSDL2_MESSAGES(NULL, NULL,"monitor_id=%s present in MAP for opcode=%d req=%p", monitor_id, opcode, out_ptr);
     if(SM_INIT_REQ == opcode){
       NSDL2_MESSAGES(NULL, NULL,"Invalid req as monitor_id=%s should not be present in MAP for ININT opcode=%d", monitor_id, opcode);
       sprintf(NS_GET_ERR_BUF(),"%sSM Request for Monitor ID(%s) is already in process\n", NS_GET_ERR_MSG(), sm_req_val[SM_MONITOR_ID]);
       return CM_INVALID_REQ;
     }

     sm_req = (SmRequest*)out_ptr;
     if(mccptr->fd != sm_req->mccptr->fd) {
       NSDL2_MESSAGES(NULL, NULL,"Invalid req as monitor_id=%s  curr mccptr->fd=%d existing sm_req->mccptr->fd=%d", monitor_id, opcode, mccptr->fd, sm_req->mccptr->fd);
       sprintf(NS_GET_ERR_BUF(),"%sSM Request for Monitor ID(%s) can not be processed, as it is initialised with another connection\n", NS_GET_ERR_MSG(), sm_req_val[SM_MONITOR_ID]);
       return CM_INVALID_REQ;
     }
     //copy only opcode and fd
     NSDL2_MESSAGES(NULL, NULL,"mccptr->vptr=%p sm_req=%p sm_req->opcode=%d", mccptr->vptr, sm_req, sm_req->opcode);
     sm_req->opcode = opcode;
     mccptr->vptr = (struct VUser* )sm_req;
     NSDL2_MESSAGES(NULL, NULL," now sm_req=%p sm_req->opcode=%d", sm_req, sm_req->opcode);
   }
   NSDL2_MESSAGES(NULL, NULL," return opcode=%d mccptr[%p]->vptr=%p", opcode, mccptr, mccptr->vptr);
   return opcode;
}


int cm_process_sm_request(Msg_com_con* mccptr)
{
  /*
    smreq_func_arr[SM_INIT_REQ] = cm_process_sm_start_req;
    smreq_func_arr[SM_STOP_REQ] = cm_process_sm_stop_req;
    smreq_func_arr[SM_UPDATE_REQ] = cm_process_sm_update_req;
    smreq_func_arr[SM_PAUSE_REQ] = cm_process_sm_pause_req;
    smreq_func_arr[SM_RESUME_REQ] = cm_process_sm_resume_req; */
  SmRequest *sm_req =  (SmRequest*) mccptr->vptr;
  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr[%p] sm_req[%p]->opcode=%d", mccptr, sm_req, sm_req->opcode );
  return smreq_func_arr[sm_req->opcode](mccptr);
}

int ns_append_str(char* file_path, char* str)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. file_path[%s] str[%s]", file_path, str);
  char cmd[MAX_CMD_LEN];
  sprintf(cmd, "echo %s >> %s", str, file_path);
  NSDL2_MESSAGES(NULL, NULL,"cmd=[%s] to execute", cmd);
  if(CM_ERROR == system(cmd))
  {
    sprintf(NS_GET_ERR_BUF(), "Unable to run cmd=%s", cmd);
    NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    return CM_ERROR;
  }
  NSDL2_MESSAGES(NULL, NULL,"returning");
  return CM_SUCCESS;
}
int cm_process_sm_start_req(Msg_com_con *mccptr)
{
  /*monId=xx;o=cm_init_monitor;gMonId=G-Monitor-ID;name=SM-Monitor-Name;monType=<webapi>;tier=<cmonTierName>;server=<cmonServerName>;tr=<xxx>;partitionId=<id>;\n*/
  SmRequest *sm_req = (SmRequest*)mccptr->vptr;
  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr[%p] sm_req[%p]", mccptr,  sm_req);
  if(!sm_req) {
    sprintf(NS_GET_ERR_BUF(), "Internal Error. sm_req=%p", sm_req);
    NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    return CM_ERROR;
  }
  //send ACK to CMON
  ns_fill_buff(NS_GET_SCRATCH_BUF(), NS_MSG_BUF_SIZE, sm_req->req_field_arr[SM_MONITOR_INDEX].value, NS_SUCCESS_TXT, NULL);
  cm_send_reply(mccptr, NS_GET_SCRATCH_BUF());
  //append TEST_MONITOR_CONFIG <MON_TYPE> <MON_INDEX>  <MON_NAME> <TIER_NAME> <SERVER_NAME> <TR_NUMBER> <PARTITION_ID> <FILE_UPLOAD_URL>
  char config_keyword[NS_CMD_SIZE];
  sprintf(config_keyword, "%s %s %s %s %s %s %s %s %s", TEST_MONITOR_CONFIG_KWD, sm_req->req_field_arr[SM_MONITOR_TYPE].value, sm_req->req_field_arr[SM_MONITOR_INDEX].value, sm_req->req_field_arr[SM_MONITOR_NAME].value, sm_req->req_field_arr[SM_TIER_NAME].value, sm_req->req_field_arr[SM_SRV_NAME].value, sm_req->req_field_arr[SM_TR_NUM].value, sm_req->req_field_arr[SM_PARTITION_ID].value, NS_GET_FILE_UPLOAD_URL());  

  NSDL2_MESSAGES(NULL, NULL,"scenario file path[%s] config_kwd[%s]", NS_GET_SCENARIO_PATH(), config_keyword);
  if(CM_ERROR == ns_append_str(NS_GET_SCENARIO_PATH(), config_keyword)) {
    sprintf(NS_GET_ERR_BUF(), "ns_append_str() failed");
    NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    return CM_ERROR;
  }
 
  char cmd[NS_CMD_SIZE]; 
  //char cmd[NS_CMD_SIZE]; 
  sprintf(cmd, "nsu_start_test -n  %s.conf -S guiFg 2>&1", sm_req->req_field_arr[SM_MONITOR_ID].value/*monitor_id.value*/);
  NSDL2_MESSAGES(NULL, NULL,"cmd=[%s] to run", cmd);
  int status = cm_run_command_in_thread(sm_req, cmd);
  NSDL2_MESSAGES(NULL, NULL, "returing..status=%d", status);  
  return status;
}
 

int cm_process_sm_close_req_v2(Msg_com_con *mccptr)
{

  SmRequest *sm_req = (SmRequest *)mccptr->vptr;
  NSDL2_MESSAGES(NULL, NULL,"Method called. sm_req[%p]->child_id = %d sm_req->opcode = %d mccptr = %p", sm_req, sm_req->child_id, sm_req->opcode, g_msg_com_con_arr[sm_req->child_id]);
  char buff[256];
  sm_req->opcode = SM_STOP_REQ;
  CM_MON_REQ *cm_req = (CM_MON_REQ*)buff;
  cm_req->data = buff + sizeof(CM_MON_REQ);
  cm_req->opcode = sm_req->opcode;
  CM_MON_STOP* cm_req_stop = (CM_MON_STOP*)cm_req->data; 
 
  int addr_offset = sizeof(CM_MON_STOP);    
  ASSIGN_ADDRESS_AND_VALUE_LEN(&(cm_req_stop->mon_id.value), &(cm_req_stop->mon_id.len), (char*)cm_req_stop + addr_offset, sm_req->req_field_arr[SM_MONITOR_ID].len)
  addr_offset += (sm_req->req_field_arr[SM_MONITOR_ID].len + 1); //+1 for null '\0' character
  FILL_SM_REQ_FIELDS(cm_req_stop->mon_id.value, sm_req->req_field_arr[SM_MONITOR_ID].value, sm_req->req_field_arr[SM_MONITOR_ID].len)
  int  data_size = addr_offset + sizeof(CM_MON_REQ);
  cm_req->msg_len =  data_size - sizeof(int);
  NSDL2_MESSAGES(NULL, NULL, "data_size = %d", data_size);
  
  if((write_msg(g_msg_com_con_arr[sm_req->child_id], (char *)cm_req, data_size, 0, CONTROL_MODE)) < 0)
  {
    NSDL2_MESSAGES(NULL, NULL, "Failed to write message");
    return -1;
  }

  return CM_SUCCESS;
}

int cm_process_sm_close_req(Msg_com_con* mcctptr)
{

  char cmd[NS_CMD_SIZE];
  SmRequest *sm_req = (SmRequest *)mcctptr->vptr;
  sprintf(cmd, "%s -f %d", NS_STOP_CMD, sm_req->child_id);
  NSDL2_MESSAGES(NULL, NULL,"cmd=%s to execute", cmd);
  cm_run_command_in_thread(sm_req, cmd);
  NSDL2_MESSAGES(NULL, NULL,"returning..."); 
  return CM_SUCCESS;
}

int cm_run_stop_cmd(SmRequest *sm_req)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. sm_req=%p  sm_req->child_id=%d, cavgen_version = %d, mccptr=%p", sm_req,  sm_req->child_id, SM_GET_CAVGEN_VERSION(), sm_req->mccptr);
 // char cmd[NS_CMD_SIZE];
  if((!sm_req->child_id) && (SM_GET_CAVGEN_VERSION() == CAVGEN_VERSION_1))
    return CM_ERROR;

  return(smreq_func_arr[SM_CLOSE_REQ](sm_req->mccptr));
  
/*  sprintf(cmd, "%s -f %d", NS_STOP_CMD, sm_req->child_id);
  NSDL2_MESSAGES(NULL, NULL,"cmd=%s to execute", cmd);
  cm_run_command_in_thread(sm_req, cmd);
  NSDL2_MESSAGES(NULL, NULL,"returning..."); 
  return CM_SUCCESS; */
}

int cm_handle_client_close(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr=%p sm_req=%p", mccptr, mccptr->vptr);
  NSDL2_MESSAGES(NULL, NULL,"Connection close from client or client not available anymore"); 
  /*if Test is still running*/
  if(mccptr->vptr)
  {
    SmRequest *sm_req = (SmRequest*)mccptr->vptr;
    NSDL2_MESSAGES(NULL, NULL," Test=%d is still running. calling cm_run_stop_cmd", ((SmRequest*)mccptr->vptr)->child_id);
    //return cm_run_stop_cmd((SmRequest*)mccptr->vptr);
    cm_run_stop_cmd(sm_req);

    sm_req->mccptr = NULL;
  }
  mccptr->vptr = NULL;
  if(mccptr->fd >= NS_ZERO){
   CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
  }
  NSLB_FREE_AND_MAKE_NULL(mccptr, "mccptr", -1, NULL);  
  return CM_SUCCESS;
}
//mccptr required as in case of parsing failure we will 
/* The stop would be called in below scenarios only:
* 1st: CM_STOP command: in this case 
*     -> mccptr->vptr NOT NULL
*     -> AND sm_req->opcode = 1 i.e. CM_STOP_REQ
*2nd: When connection is closed from client:
*	2.1:mccptr->vptr i.e sm_req NOT NULL
           :AND sm_req->opcode = 0 i.e. CM_INIT_REQ
             : THEN FREE sm_req and close connection
        2.2: mccptr->vpt i.e. sm_req is NULL
	    THEN close connection
*/
int cm_process_sm_stop_req(Msg_com_con *mccptr)
{
  
  /*o=cm_stop_monitor;gMonId=G-Monitor-ID\n*/
  NSDL2_MESSAGES(NULL, NULL,"Method called");
  SmRequest *sm_req = (SmRequest*)mccptr->vptr;
  NSDL2_MESSAGES(NULL, NULL,"sm_req=%p sm_req->opcode=%d mccptr=%p mccptr->fd=%d sm_req->mccptr->fd=%d", sm_req, sm_req->opcode,  mccptr, mccptr->fd, sm_req->mccptr->fd);
  //send ACK to CMON
  ns_fill_buff(NS_GET_SCRATCH_BUF(), NS_MSG_BUF_SIZE, sm_req->req_field_arr[SM_MONITOR_INDEX].value, NS_SUCCESS_TXT, NULL);
  cm_send_reply(mccptr, NS_GET_SCRATCH_BUF());

  if(CM_ERROR == cm_run_stop_cmd(sm_req))
  {
    CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE); 
  }
  //NS_DELETE_SMREQ_FROM_MAP(sm_req)
  //mccptr->vptr = NULL;
  //CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);

  NSDL2_MESSAGES(NULL, NULL,"returning...");
  return CM_SUCCESS;
}

int cm_process_sm_update_req(Msg_com_con *mccptr)
{
  /*o=cm_update_monitor;operation=cg_update;gMonId=G-Monitor-ID;name=Monitor-Name;keyword=”keyword”\n
    o=cm_update_monitor;operation=cg_resume;gMonId=G-Monitor-ID;name=Monitor-Name\n
    o=cm_update_monitor;operation=cg_pause;gMonId=G-Monitor-ID;name=Monitor-Name\n
  */ 

  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr=%p", mccptr);
  //send ACK to CMON

  /*if(!strcpm(<>, SM_PAUSE_REQ_TXT))
    return cm_process_sm_pause_req(mccptr);
  else if(!strcpm(<>, SM_RESUME_REQ_TXT))
    return cm_process_sm_resume_req(mccptr);
  else if(strcpm(<>, SM_UPDATE_TXT))
    return CM_INVALID_REQ; */

 //proceed further with Update Request
 NSDL2_MESSAGES(NULL, NULL,"returning...");
 return CM_SUCCESS;
}

int cm_process_sm_pause_req(Msg_com_con *mccptr)
{
  return CM_SUCCESS;
}

int cm_process_sm_resume_req(Msg_com_con *mccptr)
{
  return CM_SUCCESS;
}

int cm_process_sm_config_req(Msg_com_con *mccptr)
{
  /*o=cm_config;tr=<xx>;partitionId=<newPartitionID>;\n*/ 
 
  return CM_SUCCESS;
}



SmRequest* init_and_get_sm_req(Msg_com_con *mccptr, int opcode, char sm_req_val[][256], int arr_size)
{
   NSDL2_MESSAGES(NULL, NULL,"Method called. opcode=%d arr_size= %d", opcode, arr_size);
  
   /*BU: The very basic or golden rule/Business rule is that this method would be called during SM_INIT parsing only*/
   if((arr_size < SM_REQ_TOT_FIELDS) || (arr_size > SM_REQ_TOT_FIELDS)) {
     sprintf(NS_GET_ERR_BUF(), "Internal Error. arr_size=%d != %d", arr_size, SM_REQ_TOT_FIELDS);
     NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
     NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
     return NULL; 
   }
   
   size_t size = 0;
   int req_len_arr[SM_REQ_TOT_FIELDS];
   int idx;
   for(idx = 0 ; idx < arr_size; ++idx)
   {
      req_len_arr[idx] = strlen(sm_req_val[idx]);
      size += req_len_arr[idx];
      NSDL2_MESSAGES(NULL, NULL,"size=%d idx=%d field=%s", size, idx, sm_req_val[idx]);
   } 
   
   SmRequest* sm_req;
   NSLB_MALLOC(sm_req, (sizeof(SmRequest) + size + arr_size), "SM Req ptr", -1, NULL );
   NSDL2_MESSAGES(NULL, NULL,"sizeof(int)=%d sizeof(SmRequest)=%d sm_req=%p => %d", sizeof(int), sizeof(SmRequest), sm_req, sm_req);
   sm_req->opcode = opcode;
   sm_req->child_id = NS_ZERO;
   sm_req->mccptr = mccptr;

   int addr_offset = sizeof(SmRequest); 
   /*Assign address and value_len to sm_req data members */
   for(idx = 0; idx < arr_size; ++idx)
   {
     NSDL2_MESSAGES(NULL, NULL,"sm_req=%p idx=%d value=%p ", sm_req, idx, addr_offset);
     
     ASSIGN_ADDRESS_AND_VALUE_LEN(&(sm_req->req_field_arr[idx].value), &(sm_req->req_field_arr[idx].len), (char*)sm_req + addr_offset, req_len_arr[idx])
     addr_offset += (req_len_arr[idx] + 1); //+1 for null '\0' character
     FILL_SM_REQ_FIELDS(sm_req->req_field_arr[idx].value, sm_req_val[idx], req_len_arr[idx])
     NSDL2_MESSAGES(NULL, NULL,"sm_req=%p idx=%d value=%p ", sm_req, idx, sm_req->req_field_arr[idx].value);
   }
   pthread_mutex_init(&sm_req->lock, NULL) ;    
   //assing to mccptr as a VUser
   mccptr->vptr = (struct VUser* )sm_req;
   NSDL2_MESSAGES(NULL, NULL," returning with sm_req=%p sm_req->opcode=%d", sm_req, sm_req->opcode);
   return sm_req;
}



int cm_check_for_space_char(char sm_req_val[][256], int arr_size)
{
   NSDL2_MESSAGES(NULL, NULL,"Method called. arr_szie=%d ", arr_size);
   char* vlaue_ptr;
   int len;

    //for SM_INIT: check for all from Monitor Index to Partition ID
   //For SM_STOP check Monitor ID only
   //For SM_CONFIG: check for TR Number and partition ID
   //For SM_UPDATE: cg_update check for operation, MonId, MonNam and keyword” 
   //For SM_UPDATE: cg_pause/cg_resume check for operation, MonId, MonNam

   for(int idx = 0; idx < arr_size; ++idx) 
   { 
     vlaue_ptr = sm_req_val[idx];
     len = strlen(vlaue_ptr);
     NSDL2_MESSAGES(NULL, NULL,"idx=%d len=%d vlaue_ptr=%s", idx, len, vlaue_ptr);
     
     if(!len)
       continue;
     if((SPACE_CHAR == vlaue_ptr[NS_ZERO]) || (SPACE_CHAR == vlaue_ptr[len - 1]))
     { 
       NSDL2_MESSAGES(NULL, NULL,"space found at field idx=%d", idx);
       return CM_INVALID_REQ;
     }
   }
  return CM_SUCCESS;  
}

/*return number of req fields recieved*/
int sm_get_req_val(char *in[], int in_size, char out[][256])
{

  NSDL2_MESSAGES(NULL, NULL,"Method called. in_size=%d ", in_size);
  if(in_size <= NS_ZERO){
   sprintf(NS_GET_ERR_BUF(), "Internal Error. Invalid in_size=%d", in_size);
   NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
   NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
   return CM_ERROR;
  }
  int num_fields = 0;
  char* tokens[2];
  int field_count;
  int norm_id;
  for(int idx = 0; idx < in_size; ++idx)
  {
     //tokenise = separate
     if((field_count = nslb_get_tokens(in[idx], tokens, "=", 3)) > 2)
     {
        sprintf(NS_GET_ERR_BUF(), "%srecieved more than required filed=%d", NS_GET_SCRATCH_BUF(), field_count);
        NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
        return CM_INVALID_REQ;
     }
     NSDL2_MESSAGES(NULL, NULL," recieved req_filed_name=%s req_field_val=%s", tokens[0], tokens[1]);
     if((norm_id = nslb_get_norm_id(&(req_field_normtbl), tokens[0], strlen(tokens[0]))) < NS_ZERO)
     {
        sprintf(NS_GET_ERR_BUF(), "%sInvalide request filed=%s, as unable to find it in Norm table ", NS_GET_SCRATCH_BUF(), tokens[0]);
        NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
        return CM_INVALID_REQ; 
     }
     if(!strlen(tokens[1]))
     {
        sprintf(NS_GET_ERR_BUF(), "%srequest filed=%s has not corresponding value=%s ", NS_GET_SCRATCH_BUF(), tokens[0], tokens[1]);
        NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
        return CM_INVALID_REQ;
     }
     strcpy(out[norm_id], tokens[1]);
     ++num_fields;
     NSDL2_MESSAGES(NULL, NULL,"out[%d]=%s ", norm_id, out[norm_id]);
  } 
  return num_fields;
}

/******************************************************************
 * Name    :    send_rtc_msg_to_all_clients
 * Purpose :    This function will check whether it is called from
                Controller or NS Parent and then send message to all
                clients/NVMS using send_rtc_msg_to_client() 
******************************************************************/
/*will be use only in case of partition id change*/
int cm_send_msg_to_all_childs(SmRequest* sm_req, char *msg, int size)
{
  int i;
  NSDL2_MESSAGES(NULL, NULL, "Method called. size = %d", size);
  for(i = 0; i <  global_settings->num_process; i++)
  {
    //TODO: if nvm has send finished report and is in pause
    NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, msg = %p", i, msg);
    NSTL1(NULL, NULL, "Sending msg to client '%d'", i);
    cm_send_msg_to_child(&g_msg_com_con[i], sm_req);
  }
  return 0;
}

/********************************************************************
 * Name    :    send_rtc_msg_to_client
 * Purpose :    This function will write msg to all its clients/NVMS.
                Here "vuser_client_mask" is taken to keep track
                of messages sent, to which NVMS/clients
 * Note    :
 * Author  :    
 * Intial version date:    28/11/18
 * Last modification date: 
*********************************************************************/
int cm_send_msg_to_child(Msg_com_con *mccptr, SmRequest *sm_req)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called. mccptr=%p", mccptr);
  if(mccptr->fd == -1)  //Check if nvm/generator is dead or not
  {
    if(mccptr->ip)
    {
      NSDL2_MESSAGES(NULL, NULL, "Connection with the NVM/Generator is already closed so not sending the msg to %s",
                                  msg_com_con_to_str(mccptr));
    }
    return -1;
  }
  
  CM_MON_REQ *cm_req = (CM_MON_REQ*)g_scratch_buff;
  cm_req->data = g_scratch_buff + sizeof(CM_MON_REQ); 
  
  /* MSG_HDR 
   int msg_len; ==>packet length
   int opcode; \   ==>opcode
   int child_id; ==> nvm idx
   int ns_version; ==> mon_type or operation[update, pause, resume]
   int gen_rtc_idx; ==> mon_index 
   int testidx; ==> tr number
   long partition_idx; ==> partition id
   double abs_ts; ==> time when we sent req ro child*/

  cm_req->opcode = sm_req->opcode;
  //cm_req->ns_version = sm_req->mon_type;
  //cm_req->gen_rtc_idx = sm_req->mon_index;
  //cm_req->testidx = g_testidx;
  //cm_req->partition_idx = g_partitionid;
   
  //  Test Started Successfully
  // Error => 
 // COMPLETED
 //
  int addr_offset = 0;
  CM_MON_INIT* cm_req_init;
  NSDL2_MESSAGES(NULL, NULL, "sm_req->opcode=%d", sm_req->opcode);
  switch(sm_req->opcode)
  {
    case SM_INIT_REQ:
    NSDL2_MESSAGES(NULL, NULL, "SM_INIT_REQ mon_type = %s mon_index = %s", sm_req->req_field_arr[SM_MONITOR_TYPE].value, sm_req->req_field_arr[SM_MONITOR_INDEX].value);
    cm_req->ns_version = atoi(sm_req->req_field_arr[SM_MONITOR_TYPE].value);
    cm_req->gen_rtc_idx = atoi(sm_req->req_field_arr[SM_MONITOR_INDEX].value);
    cm_req->testidx = atoi(sm_req->req_field_arr[SM_TR_NUM].value);
    cm_req->partition_idx = atol(sm_req->req_field_arr[SM_PARTITION_ID].value);

    NSDL2_MESSAGES(NULL, NULL, "to child -->  opcode = %d mon_type = %d, mon_idx = %d", cm_req->opcode, cm_req->ns_version, cm_req->gen_rtc_idx);
    cm_req_init = (CM_MON_INIT*)cm_req->data;
    addr_offset = sizeof(CM_MON_INIT);
    /*Assign address and value_len to sm_req data members */
    ASSIGN_ADDRESS_AND_VALUE_LEN(&(cm_req_init->mon_id.value), &(cm_req_init->mon_id.len), (char*)cm_req_init + addr_offset, sm_req->req_field_arr[SM_MONITOR_ID].len)
    addr_offset += (sm_req->req_field_arr[SM_MONITOR_ID].len + 1); //+1 for null '\0' character
    FILL_SM_REQ_FIELDS(cm_req_init->mon_id.value, sm_req->req_field_arr[SM_MONITOR_ID].value, sm_req->req_field_arr[SM_MONITOR_ID].len)

    ASSIGN_ADDRESS_AND_VALUE_LEN(&(cm_req_init->mon_name.value), &(cm_req_init->mon_name.len), (char*)cm_req_init + addr_offset, sm_req->req_field_arr[SM_MONITOR_NAME].len)
    addr_offset += (sm_req->req_field_arr[SM_MONITOR_NAME].len + 1); //+1 for null '\0' character
    FILL_SM_REQ_FIELDS(cm_req_init->mon_name.value, sm_req->req_field_arr[SM_MONITOR_NAME].value, sm_req->req_field_arr[SM_MONITOR_NAME].len)

    ASSIGN_ADDRESS_AND_VALUE_LEN(&(cm_req_init->tier_name.value), &(cm_req_init->tier_name.len), (char*)cm_req_init + addr_offset, sm_req->req_field_arr[SM_TIER_NAME].len)
    addr_offset += (sm_req->req_field_arr[SM_TIER_NAME].len + 1); //+1 for null '\0' character
    FILL_SM_REQ_FIELDS(cm_req_init->tier_name.value, sm_req->req_field_arr[SM_TIER_NAME].value, sm_req->req_field_arr[SM_TIER_NAME].len)

    ASSIGN_ADDRESS_AND_VALUE_LEN(&(cm_req_init->server_name.value), &(cm_req_init->server_name.len), (char*)cm_req_init + addr_offset, sm_req->req_field_arr[SM_SRV_NAME].len)
    addr_offset += (sm_req->req_field_arr[SM_SRV_NAME].len + 1); //+1 for null '\0' character
    FILL_SM_REQ_FIELDS(cm_req_init->server_name.value, sm_req->req_field_arr[SM_SRV_NAME].value, sm_req->req_field_arr[SM_SRV_NAME].len)
    break;
  }

  NSDL2_MESSAGES(NULL, NULL, "mon_id.value=[%s] mon_id.len =%d", cm_req_init->mon_id.value, cm_req_init->mon_id.len);
  NSDL2_MESSAGES(NULL, NULL, "mon_name.value=[%s] mon_name.len =%d", cm_req_init->mon_name.value, cm_req_init->mon_name.len);
  int  data_size = addr_offset + sizeof(CM_MON_REQ);
  cm_req->msg_len =  data_size - sizeof(int);
  NSDL2_MESSAGES(NULL, NULL, "data_size = %d", data_size);
  if((write_msg(mccptr, (char *)cm_req, data_size, 0, CONTROL_MODE)) < 0)
  {
    NSDL2_MESSAGES(NULL, NULL, "Failed to write message");
    return -1;
  }

  return 0;
}

typedef struct
{
  Msg_com_con *mccptr;
  SmRequest  *sm_req_list;
}CmChildInfo;

int cm_process_child_msg(Msg_com_con *mccptr, CM_MON_REQ* rcv_data)
{
 
  NSDL2_MESSAGES(NULL, NULL, "Method called. mccptr=%p rcv_data->opcode=%d rcv_data->child_id=%d", mccptr, rcv_data->opcode, rcv_data->child_id);
  char mon_id[SM_MON_ID_MAX_LEN + 1];
  char msgProc[MAX_CMD_LEN];
  char *status_msg = msgProc;
  SmRequest *sm_req;

  /*common for all, irrespective of opcode*/
  if(rcv_data->opcode != START_MSG_BY_CLIENT)
  {
    cm_unpack_status_msg(rcv_data, mon_id, status_msg);
    NSDL2_MESSAGES(NULL, NULL, "recieved mon_id=[%s] and status_msg=[%s] from child", mon_id, status_msg);
  }
 
  switch(rcv_data->opcode)
  {
    case START_MSG_BY_CLIENT:
    /*check for valid child_id or nvm_index*/
    if((rcv_data->child_id < NS_ZERO) || (rcv_data->child_id >= global_settings->num_process))
    {
      NSDL2_MESSAGES(NULL, NULL, "Error!!! Invalid child_id=%d", rcv_data->child_id);
      //ERROR Handling
      //CONNECTION CLOSE
      //NVM RECOVERY
      return CM_ERROR;
    }
    mccptr->nvm_index = rcv_data->child_id;
    /*save mccptr specific to a child*/
    g_msg_com_con_arr[rcv_data->child_id] = mccptr;
    break;
    
    case NS_TEST_STARTED: /*#define NS_TEST_STARTED	0*/
    NSDL2_MESSAGES(NULL, NULL, "opcode NS_TEST_STARTED ");
    /*get sm_req from MAP using mon_id recieved from child*/
    CM_GET_SM_REQ(sm_req, mon_id) 
    cm_prep_and_send_msg_to_tool(sm_req, NS_SUCCESS_TXT, status_msg);
    //ToDo:testidx will be used 
    break;
    
    case NS_TEST_RESULT: /*#define NS_TEST_RESULT          1*/ 
    //ToDo: get mon_id from msg
    NSDL2_MESSAGES(NULL, NULL, "opcode NS_TEST_RESULT");
    /*get sm_req from MAP using mon_id recieved from child*/
    CM_GET_SM_REQ(sm_req, mon_id) 
    cm_prep_and_send_msg_to_tool(sm_req, NS_SUCCESS_TXT, status_msg);
    break;


    case NS_TEST_COMPLETED: /*#define NS_TEST_COMPLETED       2*/ 
    //ToDo: get mon_id from msg
    NSDL2_MESSAGES(NULL, NULL, "opcode NS_TEST_COMPLETED ");
    /*get sm_req from MAP using mon_id recieved from child*/
    CM_GET_SM_REQ(sm_req, mon_id) 
    if(sm_req->mccptr){
       cm_prep_and_send_msg_to_tool(sm_req, NS_STOPPED_TXT, status_msg);
       cm_close_conn_and_release_req(sm_req);
    }
    break;

    case NS_TEST_ERROR: /*#define NS_TEST_ERROR           3*/ 
    //ToDo: get mon_id from msg
    NSDL2_MESSAGES(NULL, NULL, "opcode NS_TEST_ERROR ");
    /*get sm_req from MAP using mon_id recieved from child*/
    CM_GET_SM_REQ(sm_req, mon_id) 
    if(sm_req->mccptr){
       cm_prep_and_send_msg_to_tool(sm_req, NS_ERR_TXT, status_msg);
       cm_close_conn_and_release_req(sm_req);
    }
    break;
    
    default:
    NSDL2_MESSAGES(NULL, NULL, "Error!!! Invalid opcde=%d from child", rcv_data->opcode);
    return CM_ERROR;
  }
  NSDL2_MESSAGES(NULL, NULL, "returning");
  return CM_SUCCESS; 
}

int cm_process_sm_start_req_v2(Msg_com_con *mccptr)
{
  /*monId=xx;o=cm_init_monitor;gMonId=G-Monitor-ID;name=SM-Monitor-Name;monType=<webapi>;tier=<cmonTierName>;server=<cmonServerName>;tr=<xxx>;partitionId=<id>;\n*/
  SmRequest *sm_req = (SmRequest*)mccptr->vptr;
  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr[%p] sm_req[%p]", mccptr,  sm_req);
  if(!sm_req) {
    sprintf(NS_GET_ERR_BUF(), "Internal Error. sm_req=%p", sm_req);
    NSDL3_MESSAGES(NULL, NULL, "err=%s", NS_GET_ERR_MSG());
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    return CM_ERROR;
  }
  //NSDL2_MESSAGES(NULL, NULL,"cavgen_Version = %d", SM_GET_CAVGEN_VERSION());

  //send ACK to CMON
  NSDL2_MESSAGES(NULL, NULL,"send ACK to tool for monitor index=%d", sm_req->req_field_arr[SM_MONITOR_INDEX].value);
  ns_fill_buff(NS_GET_SCRATCH_BUF(), NS_MSG_BUF_SIZE, sm_req->req_field_arr[SM_MONITOR_INDEX].value, NS_SUCCESS_TXT, NULL);
  cm_send_reply(mccptr, NS_GET_SCRATCH_BUF());

  //ToDo: sm_req->child = cm_select_child();
  //m_req->child
  //get index specific mccptr
  sm_req->child_id = cm_get_available_child_idx();
  NSDL2_MESSAGES(NULL, NULL,"sm_req[%p]->child_id = %d", sm_req, sm_req->child_id);
  return cm_send_msg_to_child(g_msg_com_con_arr[sm_req->child_id], sm_req); 
}

int cm_process_sm_stop_req_v2(Msg_com_con *mccptr)
{
  return CM_SUCCESS;
}

int cm_process_sm_update_req_v2(Msg_com_con *mccptr)
{
  // RTC
  return CM_SUCCESS;
}

int cm_process_sm_config_req_v2(Msg_com_con *mccptr)
{
  // PARTITION OR TR CHANGE USING RTC
  return CM_SUCCESS;
}

int cm_get_available_child_idx()
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. cur_child_idx=%d", cur_child_idx);
  //round robin approach
  int avlbl_child_idx = cur_child_idx;
  ++cur_child_idx; 
  NSDL2_MESSAGES(NULL, NULL,"now cur_child_idx=%d max num_process=%d", cur_child_idx, global_settings->num_process);
  if(cur_child_idx == global_settings->num_process)
    cur_child_idx = 0;
  NSDL2_MESSAGES(NULL, NULL,"avlbl_child_idx=%d cur_child_idx=%d", avlbl_child_idx, cur_child_idx);
  return avlbl_child_idx;
}

void cm_close_conn_and_release_req(SmRequest *sm_req)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. sm_req=%p", sm_req);
  Msg_com_con *mccptr = sm_req->mccptr;
  CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
  if((nslb_map_delete(CM_GET_SM_MAP(), sm_req->req_field_arr[SM_MONITOR_ID].value)) != CM_SUCCESS )
  {
     NSDL2_MESSAGES(NULL, NULL,"Unable to delete entry(%s) from MAP. err=%d", sm_req->req_field_arr[SM_MONITOR_ID].value);
  }

  //WEB_PAGE_AUDIT CLEANUP
  if ((SM_GET_CAVGEN_VERSION() == CAVGEN_VERSION_2) && (atoi(sm_req->req_field_arr[SM_MONITOR_TYPE].value) == 2))
  {
    NSDL2_MESSAGES(NULL, NULL,"Stop VNC and Clean Profile.");

    cm_wpa_cleanup_in_thread(sm_req->req_field_arr[SM_MONITOR_ID].value, sm_req->req_field_arr[SM_MONITOR_ID].len + 1);
  }

  NSDL2_MESSAGES(NULL, NULL,"Freeing SM Req ptr=%p", sm_req);
  NSLB_FREE_AND_MAKE_NULL(sm_req, "Freeing SM Req ptr", -1, NULL);
  NSDL2_MESSAGES(NULL, NULL,"Freeing SM Req Done");
  mccptr->vptr = NULL;
}


void cm_prep_and_send_msg_to_tool(SmRequest *sm_req, char* status_txt, char *status_msg)
{
   NSDL2_MESSAGES(NULL, NULL, "Method called. sm_req=%p status_txt=%s status_msg=%s", sm_req, status_txt, status_msg);
   char buff[NS_CMD_SIZE];


   /*CM_CHILD_STATUS *nvm_msg = (CM_CHILD_STATUS*) ((char*)rcv_data + sizeof(CM_MON_REQ));
   int addr_offset = sizeof(CM_CHILD_STATUS);

   ASSIGN_ADDRESS_AND_VALUE_LEN(&nvm_msg->mon_id.value, &nvm_msg->mon_id.len, (char*)nvm_msg + addr_offset, nvm_msg->mon_id.len);
   NSDL2_MESSAGES(NULL, NULL, "now  nvm_msg->mon_id.value=%s", nvm_msg->mon_id.value);
   addr_offset += nvm_msg->mon_id.len + 1;//+1 for null char
   ASSIGN_ADDRESS_AND_VALUE_LEN(&nvm_msg->msg.value, &nvm_msg->msg.len, (char*)nvm_msg + addr_offset, nvm_msg->msg.len);
   NSDL2_MESSAGES(NULL, NULL, "now  nvm_msg->msg.value=%s", nvm_msg->msg.value);*/

   //get mon id
   //on the basis of MON ID, get corresponding SM_REQ from MAP
   // then from SM_REQ, get MCCPTR and send response accordingly
   /* find key in MAP*/
   /*void *out_ptr;
   NSLBMap *nslb_map_ptr = CM_GET_SM_MAP();
   if(nslb_map_find(nslb_map_ptr, nvm_msg->mon_id.value, &out_ptr) < 0) {
      NSDL2_MESSAGES(NULL, NULL, "Unable to find mon_id in MAP");
      return; //CM_ERROR;
   }
   SmRequest *sm_req = (SmRequest*)out_ptr;*/
   
   ns_fill_buff(buff, NS_CMD_SIZE, sm_req->req_field_arr[SM_MONITOR_INDEX].value, status_txt, status_msg);
   cm_send_reply_v2(sm_req->mccptr, buff);
  
}

void cm_send_reply_v2(Msg_com_con *mccptr, char* reply)
{
  NSDL2_MESSAGES(NULL, NULL,"Method called. mccptr=%p reply=%p", mccptr, reply);
  if(!reply || !mccptr)
    return;
  
  SmRequest *sm_req = (SmRequest*)mccptr->vptr; 
  NSDL2_MESSAGES(NULL, NULL,"msg to send[%s] len=%d sm_req=%p", reply, strlen(reply), sm_req);
  
  write_msg(mccptr, reply, strlen(reply), 0, 0);
  NSDL2_MESSAGES(NULL, NULL,"write_msg done");
  if(!(mccptr->write_bytes_remaining))
  {
     NSDL2_MESSAGES(NULL, NULL,"mccptr->write_bytes_remaining=%d", mccptr->write_bytes_remaining);
     NSDL2_MESSAGES(NULL, NULL,"msg sent[%s] successfully. sm_req->mccptr=%p", reply, sm_req->mccptr);
     if(!sm_req->mccptr)
     {
       CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);    
     }
  }
}

void cm_unpack_status_msg(CM_MON_REQ *rcv_data, char *out_mon_id, char *out_msg)
{

   NSDL2_MESSAGES(NULL, NULL, "Method called. rcv_data=%p out_mon_id=%p out_msg=%p", rcv_data, out_mon_id, out_msg);
   
   CM_CHILD_STATUS *nvm_msg = (CM_CHILD_STATUS*) ((char*)rcv_data + sizeof(CM_MON_REQ));
   int addr_offset = sizeof(CM_CHILD_STATUS);

   ASSIGN_ADDRESS_AND_VALUE_LEN(&nvm_msg->mon_id.value, &nvm_msg->mon_id.len, (char*)nvm_msg + addr_offset, nvm_msg->mon_id.len);
   NSDL2_MESSAGES(NULL, NULL, "now  nvm_msg->mon_id.value=%s", nvm_msg->mon_id.value);
   addr_offset += nvm_msg->mon_id.len + 1;//+1 for null char
   ASSIGN_ADDRESS_AND_VALUE_LEN(&nvm_msg->msg.value, &nvm_msg->msg.len, (char*)nvm_msg + addr_offset, nvm_msg->msg.len);
   NSDL2_MESSAGES(NULL, NULL, "now  nvm_msg->msg.value=%s", nvm_msg->msg.value);
   
   strncpy(out_mon_id, nvm_msg->mon_id.value, nvm_msg->mon_id.len);
   strncpy(out_msg, nvm_msg->msg.value, nvm_msg->msg.len);

   out_mon_id[nvm_msg->mon_id.len] = '\0';
   out_msg[nvm_msg->msg.len] = '\0';

   NSDL2_MESSAGES(NULL, NULL, "now mon_id=%s  msg=%s", out_mon_id, out_msg);
}
