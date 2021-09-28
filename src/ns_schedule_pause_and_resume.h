/************************************************************************************************************
 *  Name            : ns_schedule_pause_and_resume.c 
 *  Purpose         : To control Netstorm Pause/Resume  
 *  Initial Version : Monday, July 06 2009
 *  Modification    : -
 ***********************************************************************************************************/

#ifndef NS_SCHEDULE_PAUSE_AND_RESUME_H 
#define NS_SCHEDULE_PAUSE_AND_RESUME_H 

typedef struct
{ 
  Msg_data_hdr msg_data_hdr;
} TestControlMsg;


extern void process_resume_schedule(int fd, Pause_resume *msg);
extern void process_pause_schedule(int fd, Pause_resume *msg);
extern void pause_resume_netstorm(int type, u_ns_ts_t now);
extern void pause_resume_timers(Schedule * schedule_ptr, int type, u_ns_ts_t now);
extern void process_pause_resume_feature_done_msg(int opcode);
#endif
