#ifndef NS_RUNTIME_H
#define NS_RUNTIME_H

#include "ns_msg_com_util.h"
#include "ns_runtime_changes_quantity.h"
#include "nslb_bitflag.h"

/*_______________________________________________________________________________
  Name:    ns_runtime.h
  Purpose: This consists of data structure used during every runtime changes
           Types of runtime changes
           1. QUANTITY       : VUsers increase or decrease
           2. SCHEDULE       : Schedule pattern changes and keyword values
           3. FILE-PARAMETER : Updation on values of parameter in script
           4. MONITOR        :

  TODO: checks and bounds
        reset flags
        set flags
  Modification history: Monday, 6/1/2020
_________________________________________________________________________________*/

//Macros defined for flags
#define RUNTIME_PROGRESS_FLAG     0x00000001
#define RUNTIME_SUCCESS_FLAG      0x00000002
#define RUNTIME_QUANTITY_FLAG     0x00000004
#define RUNTIME_FPARAM_FLAG       0x00000008
#define RUNTIME_SCHEDULE_FLAG     0x00000010
#define RUNTIME_MONITOR_FLAG      0x00000020
#define RUNTIME_FAIL              0x00000040
#define RUNTIME_ALERT_FLAG        0x00000080
#define RUNTIME_CAVMAIN_FLAG      0x00000100
#define RUNTIME_SET_ALL_FLAG      0xffffffff
#define RUNTIME_PASS     (~RUNTIME_PROGRESS_FLAG & RUNTIME_SUCCESS_FLAG)    /*Progress flag is reset and success is set */
//#define RUNTIME_FAIL     ~(RUNTIME_PROGRESS_FLAG | RUNTIME_SUCCESS_FLAG)     /*Progress flag is reset and success is reset */

#define RESET_RTC_INFO            -1
#define RTC_DELAY_ON_TIMEOUT      15

//Macros for pause state
#define RUNTIME_RESUME_STATE 0
#define RUNTIME_PAUSE_STATE  1

#define CHECK_ALL_RTC_MSG_DONE nslb_check_all_reset_bits(rtcdata->child_bitmask)

//structure layout
typedef struct rtcData {
  int pause_done;                              //rtc pause done
  int opcode;                                  //running opcode
  int type;
  int msg_seq_num;                             //sequence number to track response from child.
  int cur_state;                               //current state of RTC
  int index;                                   //RTC index

  u_ns_4B_t flags;                             //contains quantity, fparam, progress, schedule flags
  u_ns_ts_t epoll_start_time;                  //Save start timestamp of RTC applied
  u_ns_ts_t test_paused_at;                    //Save pause timestamp of RTC applied
  u_ns_ptr_t child_bitmask[4];                 //Child bitmask

  char log_file[MAX_FILE_NAME];                //log file buffer
  char err_msg[MAX_FILE_NAME];              //Error msg buffer
  char msg_buff[RTC_QTY_BUFFER_SIZE + 1];          //Req msg buffer

  FILE *rtclog_fp;                             //log file pointer of current RTC
  FILE *runtime_all_fp;                        //conf file pointer contains all RTC keywords
  Msg_com_con *invoker_mccptr;                 //comm ptr for msging
}RTCData;

//Variables
extern __thread RTCData *rtcdata;

// Bug Id: 81165 
extern int g_start_sch_msg_seq_num;
//Functions
extern void set_rtc_info(Msg_com_con *mccptr, int opcode);

#define RUNTIME_UPDATION_RESET_FLAGS \
  set_rtc_info(NULL, -1);\
  
#define RUNTIME_UPDATION_OPEN_LOG \
  if ((loader_opcode != CLIENT_LOADER) && (rtcdata->rtclog_fp = fopen(rtcdata->log_file, "a+")) == NULL){ \
    fprintf(stderr, "Error in opening file %s. Error = %s", rtcdata->log_file, nslb_strerror(errno)); \
    NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, "NA", "NA", \
                        "Error in opening file %s. Error = %s", rtcdata->log_file, nslb_strerror(errno));\
    SET_RTC_FLAG(RUNTIME_FAIL);\
  }

#define RUNTIME_UPDATION_CLOSE_FILES { \
  if(rtcdata->rtclog_fp) { \
    fclose(rtcdata->rtclog_fp); \
    rtcdata->rtclog_fp = NULL;  \
  } \
  if(rtcdata->runtime_all_fp) { \
    fclose(rtcdata->runtime_all_fp); \
    rtcdata->runtime_all_fp = NULL; \
  } \
}

#define RUNTIME_UPDATION_LOG_ERROR(err_msg, write_on_pr) {\
  SET_RTC_FLAG(RUNTIME_FAIL);\
  print2f_always(write_on_pr?rfp:NULL, "ERROR: %s\n", err_msg);\
  RUNTIME_UPDATE_LOG(err_msg);  \
  NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, \
    "NA", "NA", \
    "Error in applying runtime changes. Error = %s", err_msg); \
}

#define RUNTIME_UPDATE_LOG(msg_buf) {\
  if(rtcdata->rtclog_fp) \
    fprintf(rtcdata->rtclog_fp, "%s\n", msg_buf);  \
  else \
    NSTL1(NULL, NULL, "%s", msg_buf);\
}

#define RUNTIME_UPDATION_RESPONSE {\
  short runtime_msg_len = 0;\
  if(loader_opcode != CLIENT_LOADER) {\
    if((rtcdata->type != APPLY_FPARAM_RTC) && (rtcdata->type != TIER_GROUP_RTC) && (rtcdata->type != APPLY_ALERT_RTC) && (!CHECK_RTC_FLAG(RUNTIME_FAIL)))\
      delete_runtime_changes_conf_file();\
    NSDL1_RUNTIME(NULL, NULL, "RTC flags = %X, fail flag = %X", rtcdata->flags, CHECK_RTC_FLAG(RUNTIME_FAIL));\
    runtime_msg_len = snprintf(rtcdata->err_msg, 1024, "%s", (CHECK_RTC_FLAG(RUNTIME_FAIL)?"FAILURE":"SUCCESS")); \
    if ((write_msg(rtcdata->invoker_mccptr, rtcdata->err_msg, runtime_msg_len, 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE))) \
      fprintf(stderr, "Error: RUNTIME_UPDATION_RESPONSE - write message failed\n");\
  }\
  RUNTIME_UPDATION_RESET_FLAGS\
}

#define RUNTIME_UPDATION_FAILED(err_msg, write_on_pr) {\
  SET_RTC_FLAG(RUNTIME_FAIL);\
  if(loader_opcode != CLIENT_LOADER) { \
    RUNTIME_UPDATION_LOG_ERROR(err_msg, write_on_pr)\
  }\
  else\
    NSTL1(NULL, NULL, "%s", err_msg);\
}

// This must write last line in log with message "Runtime Updation Failed" as this is checked by tool
#define RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES(err_msg) { \
  if((loader_opcode != CLIENT_LOADER) && (rtcdata->type != APPLY_FPARAM_RTC) && (rtcdata->type != TIER_GROUP_RTC))\
    delete_runtime_changes_conf_file();\
  RUNTIME_UPDATION_FAILED(err_msg, 1); \
  RUNTIME_UPDATION_CLOSE_FILES\
}

/*In case of test started or during post processing RTC is not applied
  here we are checking both the phase pre test and post process, and then call parse_runtime_changes method
  which will check and return the value */
#define RUNTIME_UPDATION_VALIDATION \
  if (ns_parent_state == NS_PARENT_ST_INIT ) {\
    RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("Cannot apply runtime changes before start phase");\
  }\
  else if(ns_parent_state == NS_PARENT_ST_TEST_OVER) {\
    RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("Cannot apply during post processing phase");\
  }

#define SET_RTC_FLAG(flag)   rtcdata->flags |= flag
#define RESET_RTC_FLAG(flag) rtcdata->flags &= ~flag
#define CHECK_RTC_FLAG(flag) (rtcdata->flags & flag)

#define INC_RTC_MSG_COUNT(child_idx)\
  nslb_set_bitflag(rtcdata->child_bitmask, child_idx);

#define DEC_RTC_MSG_COUNT(child_idx, child_failed)\
  int is_new;\
  is_new = nslb_reset_bitflag(rtcdata->child_bitmask, child_idx);\
  if(!is_new && child_failed) {\
    NSDL3_MESSAGES(NULL, NULL, "Processing finished/failed child %d, setting bitmask");\
    handle_rtc_child_failed(child_idx);\
  }

#define DEC_CHECK_RTC_RETURN_INT(child_idx, child_failed){\
  DEC_RTC_MSG_COUNT(child_idx, child_failed)\
  if(is_new && !child_failed) {\
    NSDL3_MESSAGES(NULL, NULL, "Ignoring message from child %d as bitmask already set", child_idx);\
    return 0;\
  }\
}

#define DEC_CHECK_RTC_RETURN(child_idx, child_failed) {\
  DEC_RTC_MSG_COUNT(child_idx, child_failed)\
  if(is_new && !child_failed) {\
    NSDL3_MESSAGES(NULL, NULL, "Ignoring message from child %d as bitmask already set", child_idx);\
    return 0;\
  }\
}

#define RESET_RTC_BITMASK \
  memset(rtcdata->child_bitmask, 0, MAX_BITFLAG_SIZE);

#endif
