#ifndef NS_CAVMAIN_CHILD_H 
#define NS_CAVMAIN_CHILD_H

#include "ns_cavmain.h"

int cm_start_child();
int cm_pack_sm_status_msg(int opcode, char* mon_id, char* msg, char **out_buf);
void cm_process_sm_start_msg_frm_parent(CM_MON_REQ *rcv_data);
void cm_process_sm_stop_msg_frm_parent(CM_MON_REQ *rcv_data);
extern void sm_create_nvm_listener();
extern void accept_connection_from_sm_thread(int fd);
extern void handle_msg_from_sm_thread(Msg_com_con *sm_mon_req);
extern unsigned short sm_nvm_listen_port;
extern void cm_send_message_to_parent(int status, char *mon_id, char *msg);
#endif
