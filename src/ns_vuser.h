/********************************************************************************************
 * Name            : ns_vuser.h 
 * Purpose         : - 
 * Initial Version : Monday, July 13 2009
 * Modification History : 12/12/2018
 * Author(s)            : Devendar/Anubhav/Nisha
 * Purpose              : Added changes for VUser Management
 ********************************************************************************************/

#ifndef NS_VUSER_H
#define NS_VUSER_H

#include "ns_vuser_runtime_control.h"

#define DEBUG_LOG_USER_COUNT_BY_STATE(vptr, msg) \
  if(vptr != NULL) \
  { \
    NSDL4_SCHEDULE(vptr, NULL, "%s. User = %p, UserState = %s. User count by state: gNumVuserActive = %d, " \
                               "gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, " \
                               "gNumVuserSPWaiting = %d, gNumVuserBlocked = %d, gRunPhase = %d", \
                                msg, vptr, vuser_states[vptr->vuser_state], gNumVuserActive, gNumVuserThinking, \
                                gNumVuserWaiting, gNumVuserCleanup, gNumVuserSPWaiting, gNumVuserBlocked, gRunPhase); \
    \
    NSDL4_SCHEDULE(vptr, NULL, "Dump gVUserSummaryTable: group_num = %d, num_down_vuser = %d, num_running_vuser = %d, " \
                               "num_spwaiting_vuser = %d, num_paused_vuser = %d, num_exiting_vuser = %d, " \
                               "num_gradual_exiting_vuser = %d, num_stopped_vuser = %d, total_vusers = %d", \
                                vptr->group_num, gVUserSummaryTable[vptr->group_num].num_down_vuser, \
                                gVUserSummaryTable[vptr->group_num].num_running_vuser, \
                                gVUserSummaryTable[vptr->group_num].num_spwaiting_vuser, \
                                gVUserSummaryTable[vptr->group_num].num_paused_vuser, \
                                gVUserSummaryTable[vptr->group_num].num_exiting_vuser, \
                                gVUserSummaryTable[vptr->group_num].num_gradual_exiting_vuser, \
                                gVUserSummaryTable[vptr->group_num].num_stopped_vuser, \
                                gVUserSummaryTable[vptr->group_num].total_vusers); \
  } \
  else \
    NSDL4_SCHEDULE(NULL, NULL, "%s. User count by state: gNumVuserActive = %d, gNumVuserThinking = %d, " \
                               "gNumVuserWaiting = %d, gNumVuserCleanup = %d, gNumVuserSPWaiting = %d, " \
                               "gNumVuserBlocked = %d, gRunPhase = %d", \
                                msg, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, \
                                gNumVuserSPWaiting, gNumVuserBlocked, gRunPhase) \


#define DEBUG_LOG_USER_BY_GRP(vptr, msg) \
  if(vptr != NULL) \
    NSDL4_SCHEDULE(vptr, NULL, "msg = %s, grp_vuser = %p, group_num = %d, cur_vusers_active = %d, cur_vusers_waiting = %d, cur_vusers_thinking = %d, cur_vusers_cleanup = %d, cur_vusers_in_sp = %d, cur_vusers_blocked = %d", msg, average_time, (vptr->group_num), grp_vuser[vptr->group_num].cur_vusers_active, grp_vuser[vptr->group_num].cur_vusers_waiting, grp_vuser[vptr->group_num].cur_vusers_thinking, grp_vuser[vptr->group_num].cur_vusers_idling, grp_vuser[vptr->group_num].cur_sp_users, grp_vuser[vptr->group_num].cur_vusers_blocked); \
  else \
    NSDL4_SCHEDULE(vptr, NULL, "vptr is null, msg = %s, grp_vuser = %p, group_num = %d, cur_vusers_active = %d, cur_vusers_waiting = %d, cur_vusers_thinking = %d, cur_vusers_cleanup = %d, cur_vusers_in_sp = %d, cur_vusers_blocked = %d", msg, average_time, (vptr->group_num), grp_vuser[vptr->group_num].cur_vusers_active, grp_vuser[vptr->group_num].cur_vusers_waiting, grp_vuser[vptr->group_num].cur_vusers_thinking, grp_vuser[vptr->group_num].cur_vusers_idling, grp_vuser[vptr->group_num].cur_sp_users, grp_vuser[vptr->group_num].cur_vusers_blocked); 

#define CHK_AND_CLOSE_ACCOUTING(vptr) \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "CheckAndCloseAccouting"); \
  if(gNumVuserActive < 0 || gNumVuserThinking < 0 || gNumVuserWaiting < 0 || gNumVuserSPWaiting < 0 || gNumVuserBlocked < 0)\
  { \
    NSDL1_SCHEDULE(NULL, NULL, "Users are -ve. gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, gNumVuserSPWaiting = %d, gNumVuserBlocked = %d", gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, gNumVuserSPWaiting, gNumVuserBlocked); \
    print_core_events((char*)__FUNCTION__, __FILE__, "User are -ve. gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserSPWaiting = %d, gNumVuserBlocked = %d\n", gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserSPWaiting, gNumVuserBlocked);\
    /*In some cases some type of users become -ve and +ve instead of zero but overall sum remain zero and test stuck. So setting all type of users to zero in case overall sum is zero*/\
    if(!(gNumVuserActive + gNumVuserThinking + gNumVuserWaiting + gNumVuserSPWaiting + gNumVuserBlocked))\
    {\
      gNumVuserActive = gNumVuserThinking = gNumVuserWaiting = gNumVuserSPWaiting = gNumVuserBlocked = 0;\
    }\
  } \
  if ((gRunPhase == NS_ALL_PHASE_OVER) && (gNumVuserActive == 0) && (gNumVuserThinking == 0) && (gNumVuserWaiting == 0) && (gNumVuserSPWaiting == 0) && (gNumVuserBlocked == 0) && (gNumVuserPaused == 0)) \
  { \
    close_accounting(now); \
    send_finish_report(); \
  }

#define VUSER_ACTIVE_TO_WAITING(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state active to waiting (session pacing)"); \
  if(vptr->vuser_state != NS_VUSER_ACTIVE) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not active for changing from active to waiting", __LINE__, vptr->vuser_state); \
  } \
  vptr->vuser_state = NS_VUSER_SESSION_THINK; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active--; \
    grp_vuser[vptr->group_num].cur_vusers_waiting++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_ACTIVE_TO_WAITING"); \
  } \
  gNumVuserActive--; \
  gNumVuserWaiting++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state active to waiting (session pacing)"); \
}

#define VUSER_WAITING_TO_ACTIVE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state waiting to active"); \
  if(vptr->vuser_state != NS_VUSER_SESSION_THINK) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not waiting for changing from waiting to active", __LINE__, vptr->vuser_state); \
  } \
  vptr->vuser_state = NS_VUSER_ACTIVE; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_waiting--; \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_WAITING_TO_ACTIVE"); \
  }\
  gNumVuserWaiting--; \
  gNumVuserActive++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state waiting to active"); \
}

#define VUSER_THINKING_TO_ACTIVE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state thinking to active"); \
  if(vptr->vuser_state != NS_VUSER_THINKING) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not thinking for changing from thinking to active", __LINE__, vptr->vuser_state); \
  } \
  vptr->vuser_state = NS_VUSER_ACTIVE; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_thinking--; \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_THINKING_TO_ACTIVE"); \
  }\
  gNumVuserThinking--; \
  gNumVuserActive++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state thinking to active"); \
}

#define VUSER_THINKING_TO_ACTIVE_WITHOUT_STATE_CHANGE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state thinking to active"); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_thinking--; \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_THINKING_TO_ACTIVE_WITHOUT_STATE_CHANGE"); \
  } \
  gNumVuserThinking--; \
  gNumVuserActive++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state thinking to active"); \
}

#define VUSER_ACTIVE_TO_THINKING(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state active to thinking"); \
  if(vptr->vuser_state != NS_VUSER_ACTIVE) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not active for changing from active to thinking", __LINE__, vptr->vuser_state); \
  } \
  vptr->vuser_state = NS_VUSER_THINKING; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active--; \
    grp_vuser[vptr->group_num].cur_vusers_thinking++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_ACTIVE_TO_THINKING"); \
  } \
  gNumVuserActive--; \
  gNumVuserThinking++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state active to thinking"); \
}


#define VUSER_TO_CLEANUP(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state to cleanup"); \
  if((vptr->vuser_state != NS_VUSER_ACTIVE) && (vptr->vuser_state != NS_VUSER_THINKING)) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not active or thinking for changing to cleanup", __LINE__, vptr->vuser_state); \
  } \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_idling++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_TO_CLEANUP"); \
  } \
  vptr->vuser_state = NS_VUSER_CLEANUP; \
  gNumVuserCleanup++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state to cleanup"); \
}

#define VUSER_TO_IDLE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state to idle"); \
  if(vptr->vuser_state == NS_VUSER_PAUSED)\
    gNumVuserPaused--;\
  if(vptr->flags & NS_VUSER_GRADUAL_EXITING){\
    gVUserSummaryTable[vptr->group_num].num_gradual_exiting_vuser--;\
  }else if (vptr->flags & NS_VUSER_RAMPING_DOWN){\
    gVUserSummaryTable[vptr->group_num].num_exiting_vuser--;\
  }else{\
    gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  }\
  gVUserSummaryTable[vptr->group_num].num_stopped_vuser++;\
  vptr->vuser_state = NS_VUSER_IDLE; \
  gFreeVuserCnt++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state to idle"); \
}

#define VUSER_TO_ACTIVE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Befor changing state to active"); \
  vptr->vuser_state = NS_VUSER_ACTIVE; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_TO_ACTIVE"); \
  } \
  gNumVuserActive++; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser++;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state to active"); \
  NSDL2_MESSAGES(vptr,NULL, "gVUserSummaryTable[%d].num_running_vuser %d",vptr->group_num, gVUserSummaryTable[vptr->group_num].num_running_vuser);\
}

// Cleanup of user done
#define VUSER_FROM_CLEANUP(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state to cleanup"); \
  if(vptr->vuser_state != NS_VUSER_CLEANUP) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not cleanup for removing user from cleanup state", __LINE__, vptr->vuser_state); \
  } \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_idling--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_FROM_CLEANUP"); \
  } \
  gNumVuserCleanup--; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing to state cleanup"); \
}

#define VUSER_INC_EXIT(vptr) \
{\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before incrementing exiting count"); \
  if(vptr->vuser_state == NS_VUSER_ACTIVE || vptr->vuser_state == NS_VUSER_THINKING || vptr->flags & NS_VPTR_FLAGS_SP_WAITING) \
    gVUserSummaryTable[vptr->group_num].num_running_vuser--; \
  else if(vptr->vuser_state == NS_VUSER_SYNCPOINT_WAITING) \
    gVUserSummaryTable[vptr->group_num].num_spwaiting_vuser--; \
  if(vptr->flags & NS_VUSER_GRADUAL_EXITING)\
    gVUserSummaryTable[vptr->group_num].num_gradual_exiting_vuser++;\
  else\
    gVUserSummaryTable[vptr->group_num].num_exiting_vuser++;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After incrementing exiting count"); \
}

#define VUSER_DEC_WAITING(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before decrementing waiting count"); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_waiting--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_DEC_WAITING"); \
  } \
  gNumVuserWaiting--; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After decrementing waiting count"); \
}

#define VUSER_DEC_ACTIVE(vptr) \
{ \
  if(vptr->vuser_state != NS_VUSER_PAUSED)\
  {\
    DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before decrementing active count"); \
    if(SHOW_GRP_DATA) \
    { \
      grp_vuser[vptr->group_num].cur_vusers_active--; \
      DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_DEC_ACTIVE"); \
    } \
    gNumVuserActive--; \
    DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After decrementing active count"); \
  }\
}
#define VUSER_INC_ACTIVE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before increamenting state to active"); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_INC_ACTIVE"); \
  } \
  gNumVuserActive++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After increamenting state to active"); \
}

#define VUSER_ACTIVE_TO_PAUSED(vptr)\
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state from active to paused"); \
  if(vptr->vuser_state != NS_VUSER_ACTIVE) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not active for changing from active to paused. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_PAUSED;\
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active--; \
    grp_vuser[vptr->group_num].cur_vusers_paused++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_ACTIVE_TO_PAUSED"); \
  } \
  gNumVuserActive--; \
  gNumVuserPaused++; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  gVUserSummaryTable[vptr->group_num].num_paused_vuser++;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state from active to paused"); \
}

#define VUSER_BLOCKED_TO_PAUSED(vptr)\
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state from blocked to paused "); \
  if(vptr->vuser_state != NS_VUSER_BLOCKED) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not blocked for changing from blocked to paused. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_PAUSED;\
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_blocked--; \
    grp_vuser[vptr->group_num].cur_vusers_paused++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_BLOCKED_TO_PAUSED"); \
  } \
  gNumVuserBlocked--; \
  gNumVuserPaused++; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  gVUserSummaryTable[vptr->group_num].num_paused_vuser++;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state from blocked to paused"); \
}

#define VUSER_THINKING_TO_PAUSED(vptr)\
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state from thinking to paused"); \
  if(vptr->vuser_state != NS_VUSER_THINKING) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not thinking for changing from thinking to paused. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_PAUSED;\
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_thinking--; \
    grp_vuser[vptr->group_num].cur_vusers_paused++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_THINKING_TO_PAUSED"); \
  } \
  gNumVuserThinking--; \
  gNumVuserPaused++; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  gVUserSummaryTable[vptr->group_num].num_paused_vuser++;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state thinking to paused"); \
}

#define VUSER_WAITING_TO_PAUSED(vptr)\
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state from waiting to paused"); \
  if(vptr->vuser_state != NS_VUSER_SESSION_THINK) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not waiting for changing from waiting to paused. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_PAUSED;\
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_waiting--; \
    grp_vuser[vptr->group_num].cur_vusers_paused++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_THINKING_TO_PAUSED"); \
  } \
  gNumVuserWaiting--; \
  gNumVuserPaused++; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  gVUserSummaryTable[vptr->group_num].num_paused_vuser++;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state from waiting to paused"); \
}

#define VUSER_PAUSED_TO_ACTIVE(vptr)\
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state from paused to active"); \
  if(vptr->vuser_state != NS_VUSER_PAUSED) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not paused for changing from paused to active. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_ACTIVE; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    grp_vuser[vptr->group_num].cur_vusers_paused--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_PAUSED_TO_ACTIVE"); \
  } \
  gNumVuserActive++; \
  gNumVuserPaused--; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser++;\
  gVUserSummaryTable[vptr->group_num].num_paused_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state from paused to active"); \
}

#define VUSER_PAUSED_TO_THINKING(vptr)\
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state from paused to thinking"); \
  if(vptr->vuser_state != NS_VUSER_PAUSED) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not paused for changing from paused to thinking. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_THINKING; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_thinking++; \
    grp_vuser[vptr->group_num].cur_vusers_paused--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_PAUSED_TO_THINKING"); \
  } \
  gNumVuserThinking++; \
  gNumVuserPaused--; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser++;\
  gVUserSummaryTable[vptr->group_num].num_paused_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state from paused to thinking"); \
}

#define VUSER_PAUSED_TO_WAITING(vptr)\
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state from paused to waiting"); \
  if(vptr->vuser_state != NS_VUSER_PAUSED) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not paused for changing from paused to waiting. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_SESSION_THINK; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_waiting++; \
    grp_vuser[vptr->group_num].cur_vusers_paused--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_PAUSED_TO_WAITING"); \
  } \
  gNumVuserWaiting++; \
  gNumVuserPaused--; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser++;\
  gVUserSummaryTable[vptr->group_num].num_paused_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state from paused to waiting"); \
}

#define VUSER_ACTIVE_TO_SYNCPOINT_WAITING(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state active to syncpoint waiting"); \
  if(vptr->vuser_state != NS_VUSER_ACTIVE) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not active for changing from active to syncpoint waiting. vptr = %p", __LINE__, vptr->vuser_state, vptr); \
  } \
  vptr->vuser_state = NS_VUSER_SYNCPOINT_WAITING; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active--; \
    grp_vuser[vptr->group_num].cur_sp_users++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_ACTIVE_TO_SYNCPOINT_WAITING"); \
  } \
  gNumVuserActive--; \
  gNumVuserSPWaiting++; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  gVUserSummaryTable[vptr->group_num].num_spwaiting_vuser++;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state active to syncpoint waiting"); \
}

#define VUSER_SYNCPOINT_WAITING_TO_ACTIVE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state syncpoint waiting to active"); \
  if(vptr->vuser_state != NS_VUSER_SYNCPOINT_WAITING) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not syncpoint waiting for changing from syncpoint waiting to active. vptr = %p",__LINE__, vptr->vuser_state, vptr);\
  } \
  vptr->vuser_state = NS_VUSER_ACTIVE; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    grp_vuser[vptr->group_num].cur_sp_users--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_SYNCPOINT_WAITING_TO_ACTIVE"); \
  } \
  gNumVuserActive++; \
  gNumVuserSPWaiting--; \
  if (!(vptr->flags & NS_VUSER_RAMPING_DOWN)){\
    gVUserSummaryTable[vptr->group_num].num_running_vuser++;\
    gVUserSummaryTable[vptr->group_num].num_spwaiting_vuser--;\
  }\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state syncpoint waiting to active"); \
}

#define VUSER_SYNCPOINT_DEC_WAITING(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before decrementing waiting count of syncpoint users"); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_sp_users--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_SYNCPOINT_DEC_WAITING"); \
  } \
  gNumVuserSPWaiting--; \
  gVUserSummaryTable[vptr->group_num].num_spwaiting_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After decrementing waiting count of syncpoint users"); \
}

/* NVM changes the state of user from ACTIVE to SP WAITING & it will keep sending msg to parent: "whether Sync Point will has to release or not"
 * But parent is taking time in response.
 * which results continous increment in gNumVuserSPWaiting counter( it may go abouve release policy defined).
 * To control this issue we are changing state but not incrementing counters immediately. will increment later while releasing or while adding in syncpoint linked list.
 * */
#define VUSER_SYNCPOINT_WAITING_TO_ACTIVE_WITHOUT_INCREMENTING_COUNTERS(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state syncpoint waiting to active without incrementing counter."); \
  if(vptr->vuser_state != NS_VUSER_SYNCPOINT_WAITING) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not syncpoint waiting for changing from syncpoint waiting to active without incrementing counter. vptr = %p",__LINE__, vptr->vuser_state, vptr);\
  } \
  vptr->vuser_state = NS_VUSER_ACTIVE; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state syncpoint waiting to active without incrementing counter."); \
}

#define VUSER_ACTIVE_TO_SYNCPOINT_WAITING_WITHOUT_INCREMENTING_COUNTERS(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state syncpoint active to waiting without incrementing counter."); \
  if(vptr->vuser_state != NS_VUSER_ACTIVE) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not syncpoint active for changing from active to syncpoint waiting without incrementing counter. vptr = %p",__LINE__, vptr->vuser_state, vptr);\
  } \
  vptr->flags |= NS_VPTR_FLAGS_SP_WAITING;\
  vptr->vuser_state = NS_VUSER_SYNCPOINT_WAITING; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state syncpoint active to waiting without incrementing counter."); \
}

#define INCREMENTING_VUSER_SYNCPOINT_WAITING_COUNTERS_WITHOUT_STATE_CHANGE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before incrementing counter without state change."); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_sp_users++; \
    grp_vuser[vptr->group_num].cur_vusers_active--; \
  } \
  gNumVuserSPWaiting++; \
  gNumVuserActive--; \
  if (!(vptr->flags & NS_VUSER_RAMPING_DOWN)){\
    gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
    gVUserSummaryTable[vptr->group_num].num_spwaiting_vuser++;\
  }\
  vptr->flags &= ~NS_VPTR_FLAGS_SP_WAITING;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After incrementing counter without state change."); \
}

//Blocked state in FCS when vusers are waiting in the pool to get their chance of execution
#define VUSER_ACTIVE_TO_BLOCKED(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state active to blocked"); \
  if(vptr->vuser_state != NS_VUSER_ACTIVE) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not active for changing from active to blocked", __LINE__, vptr->vuser_state); \
  } \
  vptr->vuser_state = NS_VUSER_BLOCKED; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_active--; \
    grp_vuser[vptr->group_num].cur_vusers_blocked++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_ACTIVE_TO_BLOCKED"); \
  } \
  gNumVuserActive--; \
  gNumVuserBlocked++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state active to blocked"); \
}

#define VUSER_BLOCKED_TO_ACTIVE(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before changing state blocked to active"); \
  if(vptr->vuser_state != NS_VUSER_BLOCKED) \
  { \
    print_core_events((char*)__FUNCTION__, __FILE__, "At line %d User state (%d) is not blocked for changing from blocked to active", __LINE__, vptr->vuser_state); \
  } \
  vptr->vuser_state = NS_VUSER_ACTIVE; \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_blocked--; \
    grp_vuser[vptr->group_num].cur_vusers_active++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_BLOCKED_TO_ACTIVE"); \
  } \
  gNumVuserBlocked--; \
  gNumVuserActive++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After changing state blocked to active"); \
}

#define VUSER_DEC_PAUSED(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before decrementing paused count"); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_paused--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_DEC_PAUSED"); \
  } \
  gVUserSummaryTable[vptr->group_num].num_paused_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After decrementing paused count"); \
}

#define VUSER_DEC_DOWN(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before decrementing down count"); \
  if((get_group_mode(vptr->group_num)) == TC_FIX_CONCURRENT_USERS) \
    gVUserSummaryTable[vptr->group_num].num_down_vuser--;\
  else \
    gVUserSummaryTable[vptr->group_num].num_down_vuser = -1;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After decrementing down count"); \
}

#define VUSER_DEC_STOP(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before decrementing stop count"); \
  gVUserSummaryTable[vptr->group_num].num_stopped_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After decrementing stop count"); \
}

#define VUSER_DEC_BLOCKED(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before decrementing blocked count"); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_blocked--; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_DEC_BLOCKED"); \
  } \
  gNumVuserBlocked--; \
  gVUserSummaryTable[vptr->group_num].num_running_vuser--;\
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After decrementing blocked count"); \
}

#define VUSER_INC_BLOCKED(vptr) \
{ \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "Before increamenting state to blocked"); \
  if(SHOW_GRP_DATA) \
  { \
    grp_vuser[vptr->group_num].cur_vusers_blocked++; \
    DEBUG_LOG_USER_BY_GRP(vptr, "VUSER_INC_BLOCKED"); \
  } \
  gNumVuserBlocked++; \
  DEBUG_LOG_USER_COUNT_BY_STATE(vptr, "After increamenting state to blocked"); \
}


extern void redirect_connection( VUser *vptr, connection* cptr, u_ns_ts_t now , action_request_Shr* last_url);
extern void next_connection( VUser *vptr, connection* cptr, u_ns_ts_t now );
extern void user_cleanup_timer( ClientData client_data, u_ns_ts_t now );
extern void reuse_user( VUser *vptr, connection* cptr, u_ns_ts_t now );
extern char *vuser_states[];
extern void start_new_user_callback( ClientData cd, u_ns_ts_t now );
extern void start_reuse_user_callback( ClientData cd, u_ns_ts_t now );
extern int generate_scen_group_num();
extern void init_nsl_array_var_uvtable(VUser *vptr);

//RBU
extern inline void ns_rbu_user_cleanup(VUser *vptr);
extern int get_debug_level(VUser *vptr); 

//MONGODB
extern void ns_mongodb_client_cleanup(VUser *vptr);
extern void ns_jmeter_user_cleanup(VUser *vptr);
extern void ns_cassdb_free_cluster_and_session(VUser *vptr);
extern void cav_main_set_global_vars(SMMonSessionInfo *sm_mon_ptr);
#endif
