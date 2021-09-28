#ifndef _NS_AUTO_FETCH_ENBD_H_
#define _NS_AUTO_FETCH_ENBD_H_

#include "ns_embd_objects.h"

#define RESET_BIT_FLAG_NS_EMBD_OBJS_SET_BY_API(vptr) {   \
          vptr->flags &= ~NS_EMBD_OBJS_SET_BY_API;       \
        }                                               

extern void make_absolute_from_relative_url(char *in_url, char *parent_url_line, char *out_url);
extern void add_to_hlist(VUser *vptr, HostTableEntry_Shr *hel);

extern int url_resp_size;
extern char *url_resp_buff;

extern void populate_auto_fetch_embedded_urls(VUser *vptr, connection *cptr, int pattern_table_index, int hls_flag);
extern void free_all_embedded_urls(VUser *vptr);
extern int extract_hostname_and_request(char *eurl, char *hostname, char *request,
                                  int *port, int *request_type, char *parent_request_line, int parent_url_request_type);

extern void set_embd_objects(connection *cptr, int num_eurls, EmbdUrlsProp *eurls);
extern void populate_replay_embedded_urls(VUser *vptr, connection *cptr);
extern EmbdUrlsProp *eurls_prop;
extern int g_num_eurls_set_by_api;
extern HostTableEntry_Shr *get_hel_from_eurl_list(EmbdUrlsProp *eurls, int *num_eurls, connection *cptr, VUser *vptr);
extern void parse_socket_host_and_port(char *eurl, int *request_type, char *hostname, int *port);

#endif /* _NS_AUTO_FETCH_ENBD_H_ */
