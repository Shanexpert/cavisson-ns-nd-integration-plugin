#ifndef _NS_TR069_ACS_RPC_H_ 
#define _NS_TR069_ACS_RPC_H_ 

extern int tr069_cpe_invoke_inform_ex(VUser *vptr, int page_id);
extern int tr069_cpe_invite_rpc_ex(VUser *vptr, int page_id);
extern void tr069_set_download_cmd_key (VUser *vptr, char *cmd_key, int cmd_key_len);
extern void tr069_gen_req_id(char *req_id, char *forWhich);
#endif
