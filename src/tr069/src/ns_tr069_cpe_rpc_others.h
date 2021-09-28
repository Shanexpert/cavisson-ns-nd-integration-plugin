#ifndef _NS_TR069_CPE_RPC_OTHERS_H_
#define _NS_TR069_CPE_RPC_OTHERS_H_

extern int tr069_cpe_reboot_ex(VUser *vptr, int page_id);
extern int tr069_cpe_download_ex(VUser *vptr, int page_id);
extern int tr069_cpe_transfer_complete_ex(VUser *vptr, int page_id);
extern int ns_tr069_get_periodic_inform_time_ext(VUser *vptr);

extern inline void tr069_parse_reboot_req(VUser *vptr, char *cur_ptr);
extern inline void tr069_parse_download_req(VUser *vptr, char *cur_ptr);
#endif
