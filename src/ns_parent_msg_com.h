#ifndef __NS_PARENT_MSG_COM_H__
#define __NS_PARENT_MSG_COM_H__

extern void send_schedule_phase_start(Schedule *schedule,  int grp_idx, int phase_idx);
extern void process_http_test_traffic_stats(void *ptr, Msg_com_con *mccptr);

#endif 
