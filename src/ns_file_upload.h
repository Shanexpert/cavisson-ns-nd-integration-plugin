#ifndef NS_FILE_UPLOAD_H 
#define NS_FILE_UPLOAD_H

#include "nslb_alert.h"
#include "nslb_util.h"

#include "ns_string.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_trace_level.h"

#define FILE_MAX_UPLOAD_SIZE        10240

extern NetstormAlert *g_ns_file_upload;

void ns_init_file_upload(int thread_pool_init_size, int thread_pool_max_size, int alert_queue_init_size, int alert_queue_max_size);
int ns_config_file_upload(char *server_ip, unsigned short server_port, char protocol, char *url,
                           unsigned short max_conn_retry, unsigned short retry_timer, int trace_fd);
int ns_file_upload(char *file_name, char* content,  unsigned int size);
int ns_upload_clips(char *source_path, char *dest_path, char *filter);

#endif
