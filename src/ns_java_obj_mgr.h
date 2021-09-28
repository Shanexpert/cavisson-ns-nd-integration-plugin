#ifndef _ns_java_obj_mgr
#define _ns_java_obj_mgr

extern void init_java_obj_mgr_con(VUser *vptr, int port);
extern int create_java_obj_msg(int src_type, void *src_ptr, char *out, int *len, int *out_len, int opcode);
extern int send_java_obj_mgr_data(char* data, int len, int stop);
extern int read_java_obj_msg(char *read_buffer, int *content_size, int stop);

#endif
