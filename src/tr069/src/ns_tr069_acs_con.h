#ifndef _NS_TR069_ACS_CON_H_

extern int tr069_accept_connection(connection *cptr, u_ns_ts_t now);
extern int ns_tr069_register_rfc_ext (VUser *vptr, char* ip, unsigned short port, char *url);
//extern int ns_tr069_get_rfc_ext(VUser *vptr);
extern int nsi_tr069_wait_time(VUser *vptr, u_ns_ts_t now);
extern int ns_tr069_get_rfc_ext(VUser *vptr, int wait_time);
extern int nsi_tr069_wait_time(VUser *vptr, u_ns_ts_t now);
extern void tr069_proc_rfc_from_acs(connection *cptr);

#endif
