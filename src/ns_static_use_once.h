#ifndef NS_STATIC_USE_ONCE_H
#define NS_STATIC_USE_ONCE_H

#define USE_ONCE_EVERY_USE -1

extern void save_ctrl_file();
extern void write_last_data_file (int used_index, int val_remaining, int total_val, u_ns_ts_t time, int fd, char *data_file, char absolute_flag);
extern void remove_ctrl_and_last_files();
extern void divide_data_files ();
extern void open_last_file_fd();
extern void close_last_file_fd();

#endif
