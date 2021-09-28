#ifndef __NS_SERVER_MAPPING_H__
#define __NS_SERVER_MAPPING_H__

extern inline void save_current_url(connection *cptr);
extern int ns_setup_save_url_ext(int type, int depth, char *var, VUser *vptr);
extern int ns_force_server_mapping_ext(VUser *vptr, char *rec, char* map);
#endif //__NS_SERVER_MAPPING_H__
