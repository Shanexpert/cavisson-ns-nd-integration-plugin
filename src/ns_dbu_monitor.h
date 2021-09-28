#ifndef ND_DBU_MONITOR_H
#define ND_DBU_MONITOR_H

extern int fill_monitor_stat_buffer(char *buffer);
extern void send_monitor_data_to_ns(char *msg);
extern void remove_fd_and_close_monitor_connection();
extern void destroy_monitor_structure(char *err_msg);
extern void init_dbu_monitor();


#endif
