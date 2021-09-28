#ifndef _NS_TR069_CPE_RPC_PARAM_H_
#define _NS_TR069_CPE_RPC_PARAM_H_

extern int tr069_cpe_execute_get_rpc_methods_ex(VUser *vptr, int page_id);
extern int tr069_cpe_execute_set_parameter_values_ex(VUser *vptr, int page_id);
extern int tr069_cpe_execute_get_parameter_values_ex(VUser *vptr, int page_id);
extern int tr069_cpe_execute_get_parameter_names_ex(VUser *vptr, int page_id);
extern int tr069_cpe_execute_set_parameter_attributes_ex(VUser *vptr, int page_id);
extern int tr069_cpe_execute_get_parameter_attributes_ex(VUser *vptr, int page_id);

extern void inline tr069_parse_spv_req(VUser *vptr, char *cur_ptr);
extern void inline tr069_parse_gpv_req(VUser *vptr, char *rep_ptr);
extern void inline tr069_parse_gpn_req (VUser *vptr, char *cur_ptr);
extern void inline tr069_parse_spa_req(VUser *vptr, char *cur_ptr);
extern void inline tr069_parse_gpa_req(VUser *vptr, char *cur_ptr);

extern void inline tr069_parse_get_rpc_method_req(VUser *vptr, char *rpc_method);
#endif
