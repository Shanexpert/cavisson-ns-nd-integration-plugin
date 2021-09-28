#ifndef _ns_get_log_file_monitor_h
#define _ns_get_log_file_monitor_h

#define USER_FILE_NAME          1
#define USER_FILE_NAME_EXIST    2
#define USER_FILE_PATH          3
#define USER_FILE_PATH_EXIST    4
#define TEST_RUN_DIR            5
#define LPS_LOG_DIR             6
#define SEND_ON_IP_PORT         7


/*  This function takes argument string of cm_get_log_file monitor and parses it.
 *  Then it checks arguments and validates destination file path and creates destination file.
 */
extern int create_getFileMonitor_dest_file(char *pgm_path, char *pgm_args, char *vector_name, char *server_name, char *dest_file, char *err_buf);
extern int decode_file_name(char *input, char *output);
#endif
