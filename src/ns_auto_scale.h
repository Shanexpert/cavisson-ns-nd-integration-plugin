extern int g_time_to_get_inactive_inst;
extern char g_enable_new_gdf_on_partition_switch;

extern void kw_set_enable_auto_scale_cleanup_setting(char *buffer);
extern void merge_monitor_list_into_rtgRewrt_monitor_list(Monitor_list *source_cm_ptr, Monitor_list *destination_cm_ptr, int num_source_cm_to_copy);
extern void memset_group_data_ptr_of_mon();
extern void send_msg_to_get_inactive_instance_to_ndc(int th_flag);
extern void do_remove_add_select_and_reset_mon_rtgRewrt_tables();
extern void make_new_parent(CM_info * cm_info, int i);
extern void reset_mon_rtgRewrt_tables();
extern void check_mon_cleanup();
