#ifndef NS_RUNTIME_CHANGES_H
#define NS_RUNTIME_CHANGES_H

#define GROUP_IS_NOT_IN_GEN 9

//extern int for_quantity_rtc;
extern FILE *rtc_log_fp;
extern void parse_runtime_changes();
extern void parse_sm_monitor_rtc_data(char *msg, char *send_msg);
extern int delete_runtime_changes_conf_file();
extern int read_runtime_keyword(char *line, char *err_msg, int *mon_status, int *first_time, int id, int is_rtc_for_qty, int rtc_send_counter, int *rtc_rec_counter, int *rtc_msg_len);
extern void apply_runtime_changes(char only_monitor_runtime_change);
extern int parse_runtime_schedule_phase_ramp_up(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int *flag, int phase_id, char *quantity, int runtime_id);
extern int parse_runtime_schedule_phase_ramp_down(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int *flag, int rtc_idx, char *quantity, int runtime_id);
extern int parse_rtc_qty_phase_ramp_up(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int rtc_idx, int ph_idx);
extern int is_runtime_changes_for_monitor();
extern int parse_rtc_quantity_keyword1(char *tool_msg);
extern int parse_qty_buff_and_distribute_on_gen();
extern int find_grp_idx_from_kwd(char *buf, char *err_msg);
extern void parse_monitor_rtc_data(char *msg);
extern int is_rtc_applied_for_dyn_objs();
extern void reset_dynamic_obj_structure();
extern void apply_resume_rtc(int only_monitor);
extern int update_runtime_sessions(char mode, int num_fetches, int grp_idx, char *err_msg);
extern void process_alert_rtc(Msg_com_con *mccptr, char *msg);
/* This macro will check
     Types of RTC:            OPCODE
     APPLY_FPARAM_RTC         149
     APPLY_QUANTITY_RTC       150
     APPLY_MONITOR_RTC        151
     QUANTITY_PAUSE_RTC       154 Not supported
     QUANTITY_RESUME_RTC      155 Not supported
     APPLY_PHASE_RTC       156
     TIER_GROUP_RTC           163
   If one of the above RTC is already in progress, then we can not able to applied other RTC.
*/
#define CHECK_RTC_APPLIED() \
{ \
  char runtime_msg_buf[128 + 1]; \
  short runtime_msg_len = 0; \
  char ret; \
  NSDL2_PARENT(NULL, NULL, "opcode = %d", msg->top.internal.opcode); \
  if (loader_opcode == CLIENT_LOADER) {\
    NSTL1_OUT(NULL, NULL, "RTC can not be applied on generator"); \
  }\
  switch(msg->top.internal.opcode) \
  { \
    case APPLY_FPARAM_RTC: \
    case APPLY_MONITOR_RTC: \
    case APPLY_QUANTITY_RTC: \
    case APPLY_PHASE_RTC: \
    case TIER_GROUP_RTC: \
      NSTL1_OUT(NULL, NULL, "RTC can not be applied as one RTC is already in progress"); \
      runtime_msg_len = snprintf(runtime_msg_buf + 4, 128, "REJECT"); \
      memcpy(runtime_msg_buf, &rtcdata->type, 4); \
      runtime_msg_len += 4; \
      ret = write_msg(mccptr, runtime_msg_buf, runtime_msg_len, 0, CONTROL_MODE); \
      if(ret) \
        fprintf(stderr, "Error: CHECK_RTC_APPLIED() - write message failed\n"); \
      \
      if((msg->top.internal.opcode != APPLY_FPARAM_RTC) && (msg->top.internal.opcode != TIER_GROUP_RTC)) \
        delete_runtime_changes_conf_file(); \
      return 1; \
  } \
}

#endif  /* NS_RUNTIME_CHANGES_H */
