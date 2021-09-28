#ifndef NS_RUNTIME_CHANGES_MONITOR_H 
#define NS_RUNTIME_CHANGES_MONITOR_H 

#define MON_ALREADY_STOPPED -2

#define RUNTIME_ERROR -1
#define RUNTIME_SUCCESS 0
#define RUNTIME_FAILURE 2

#ifndef MAX_DATA_LINE_LENGTH
  #define MAX_DATA_LINE_LENGTH 2048
#endif

#define RUNTIME_MON_CONDITION \
   if(((!strcasecmp(server_ip, "NA") || !strcasecmp(server_ip, cus_mon_ptr->cs_ip)) && \
      (!strcasecmp(vector_name, "NA") || match_vector_name(vector_name, cus_mon_ptr, server_opt, group_id_pass_on_runtime, grp_id)) && \
        (group_id_pass_on_runtime == -1 || group_id_pass_on_runtime == grp_id)))

#define CLOSE_FP(fp){ \
 if(fp)\
 {\
   if( fclose(fp) != 0)\
     NSTL1(NULL, NULL, "Error in closing fp, Error: %s\n", nslb_strerror(errno));\
 }\
  fp = NULL;\
}

#define CLOSE_FD(fd){ \
 if(fd > 0 )\
 {\
   if ( close(fd) !=0 ) \
    NSTL1(NULL, NULL, "Error in closing fd, Error: %s\n", nslb_strerror(errno));\
 }\
  fd = -1;\
}

extern int kw_set_runtime_monitors(char *buf, char *err_msg);
extern int kw_set_runtime_delete_check_mon(char *check_mon_name);

//monitor runtime add/delete
extern void handle_if_dvm_fd(void *ptr);
extern int dvm_chk_conn_send_msg_to_cmon(void *ptr);
extern void runtime_change_mon_dr();
extern void dump_monitor_table();
extern void make_connection_on_runtime();
extern void kw_set_enable_monitor_delete_frequency(char *keyword, char *buffer);
extern int kw_set_runtime_process_deleted_monitors(char *buf, char *err_msg);
#endif
