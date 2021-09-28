#ifndef NS_DYNAMIC_AVG_H
#define NS_DYNAMIC_AVG_H

extern void send_object_discovery_response_to_child(Msg_com_con *mccptr, Norm_Ids *msg);
extern void process_parent_object_discover_response(Msg_com_con *mccptr, Norm_Ids *msg);
extern void process_new_object_discovery_record(Msg_com_con *mccptr, Norm_Ids *msg);
extern void create_dynamic_data_avg(Msg_com_con *mccptr, int *row_num, int nvm_id, int type);
extern void set_cavg_ptr(cavgtime *cavg_ptr, int type);
extern void send_new_object_discovery_record_to_parent(VUser *vptr, int data_len, char *data, int type, int norm_id);
extern void set_and_move_below_status_code_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id);
extern void set_and_move_below_tx_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id);
extern void set_and_move_below_server_ip_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id);
#ifdef CHK_AVG_FOR_JUNK_DATA
extern void check_avgtime_for_junk_data(char *from, int which_pool);
extern void validate_tx_entries(char *from, avgtime *loc_avgtime, int slot_id);
#endif
#endif

