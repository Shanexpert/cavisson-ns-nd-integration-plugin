#ifndef _NS_TR069_CPE_RPC_OBJECT_H_
#define _NS_TR069_CPE_RPC_OBJECT_H_

extern int tr069_cpe_add_object_ex(VUser *vptr, int page_id);
extern int tr069_cpe_delete_object_ex(VUser *vptr, int page_id);

extern inline void tr069_parse_add_obj_req(VUser *vptr, char *cur_ptr);
extern inline void tr069_parse_delete_obj_req(VUser *vptr, char *cur_ptr);
extern inline void tr069_parse_reboot_req(VUser *vptr, char *cur_ptr);
extern inline void tr069_parse_download_req(VUser *vptr, char *cur_ptr);
#endif
