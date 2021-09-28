/******************************************************************
 * Name    : ns_cookie.h
 * Author  : Archana
 * Purpose : This file contains methods related to
             parsing keyword, shared memory, run time for cookies
 * Note:
 * Modification History:
 * 10/09/08 - Initial Version
*****************************************************************/

#ifndef _NS_COOKIE_H_ 
#define _NS_COOKIE_H_

#define COOKIES_ENABLED 0
#define COOKIES_DISABLED 1

extern char* CookieString;
extern int CookieString_Length;

extern char EQString[2];
extern int EQString_Length;

extern char SCString[2];
int SCString_Length;

#ifndef CAV_MAIN
extern int max_reqcook_entries;
extern int total_reqcook_entries;
extern int max_cookie_hash_code;
extern int total_cookie_entries;
extern CookieTableEntry* cookieTable;
#else
extern __thread int max_reqcook_entries;
extern __thread int total_reqcook_entries;
extern __thread int max_cookie_hash_code;
extern __thread int total_cookie_entries;
extern __thread CookieTableEntry* cookieTable;
#endif

extern int (*cookie_hash)(const char*, unsigned int);
extern int (*in_cookie_hash)(const char*, unsigned int);
extern int (*cookie_hash_parse)(const char*, unsigned int);
extern int (*in_cookie_hash_parse)(const char*, unsigned int);
extern const char* (*cookie_get_key)(unsigned int);


#define MAX_COOKIE_LENGTH (8192 + 1)  // Assuming one Cookie Node in string can fit in 8K

/* --- Method related to Shared Memory  ---*/
extern void init_cookie_info(void);
extern void Create_reqcook_entry (int cname_offset, int cookie_length, int req_idx);
extern int Create_cookie_entry (char *cookie, int sess_idx);
extern int input_global_cookie (char* line, int line_number, int sess_idx, char *fname, char *script_filename);
extern int create_cookie_hash(void);

/* --- Method related to Parsing Time  ---*/
extern inline int kw_set_cookies(char *keyword, char *text, char *err_msg);
extern inline int add_main_url_cookie(char *line, int line_number, char *det_fname, int cur_request_idx, int sess_idx);
extern inline int add_embedded_url_cookie(char *line, int line_num, int req_idx, int sess_idx);

/* --- Method related to Run Time  ---*/
extern inline int insert_cookies(const ReqCookTab_Shr* cookies, VUser* vptr, NSIOVector *ns_iovec);
extern int inline is_cookies_disallowed (VUser *vptr);
extern inline int save_cookie_name(char *cookie_start_name, char *cookie_end_name, int cookie_length, connection* cptr);
extern inline int save_manual_cookie(char *cookie_start_name, char *cookie_end_name, int cookie_length, connection* cptr);
extern inline int save_cookie_name_more(char *cookie_start, char *cookie_end_name, int cookie_length, connection* cptr);
extern inline int save_cookie_value(char *cookie_start_value, char *cookie_end_value, int cookie_length, connection* cptr);
extern inline int save_cookie_value_more(char *cookie_start_value, char *cookie_end_value, int cookie_length, connection* cptr);

/* --- Method related to API  ---*/
extern inline int ns_get_cookie_idx_non_auto_mode(char *cname, char *sname);
extern inline char *ns_get_cookie_val_non_auto_mode(int cookie_idx, VUser *my_vptr);
extern inline int ns_set_cookie_val_non_auto_mode(int cookie_idx, char *cookie_val, VUser *my_vptr);
#endif /* _NS_COOKIE_H_ */
