#ifndef NS_USER_DEFINE_HEADERS_H
#define NS_USER_DEFINE_HEADERS_H 

//set custom header macros
#define NS_MAIN_URL_HDR_IDX 0
#define NS_EMBD_URL_HDR_IDX 1
#define NS_ALL_URL_HDR_IDX 2
#define NS_AUTO_HDR_IDX 3

//set bits for auto header
#define MAIN_URL_HDR 0x00000001
#define EMBD_URL_HDR 0x00000002

//These function should be used for user define header API
extern void ns_web_add_hdr_data(VUser *vptr, char *header, char *content, int flag);
extern void reset_header_flag(VUser* vptr);
extern void ns_web_add_auto_header_data(VUser *vptr, char *header, char *content, int flag);
extern void add_header_to_list(char *header, char *content, VUser *vptr, int flag);
extern inline int insert_auto_headers(connection* cptr, VUser* vptr);
extern void ns_web_remove_auto_header_data(VUser *vptr, char *header, int flag, connection* cptr);
extern int get_header_data(User_header *cnode, char *header_buff, char *header, int flag, int *status_flg);
extern User_header* create_node(char *header, char *content, int total_len);
extern void delete_all_auto_header(VUser *vptr);
extern void delete_all_api_headers(VUser *vptr);
extern void reset_all_api_headers(VUser *vptr);
extern void reset_bit(int flag, User_header *cnode);
#endif
