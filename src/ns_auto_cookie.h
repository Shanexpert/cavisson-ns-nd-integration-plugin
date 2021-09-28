/******************************************************************
 * Name    : ns_auto_cookie.h
 * Author  : Archana
 * Purpose : This file contains declaration of methods related to
             implement auto cookie feature
 * Note:
 * Modification History:
 * 04/10/08 - Initial Version
*****************************************************************/

#ifndef NS_AUTO_COOKIE_H_
#define NS_AUTO_COOKIE_H_

typedef struct cookieNode
{
  char *cookie_name; // Cookie name (allocated buffer) (Null Terminated).
  short cookie_name_len; //Cookie name length
  char flag;  // default value is 0, which is used for ns_remove_cookie API 
  short domain_len; // cookie domain length (used for passing in free)
  char *domain;  // domain means domain name that specifies the domain for which the cookie is valid.
  char *path; // If a path is specified, then cookie is considered valid for any requests that match that path, otherwise If no path is specified, then the path is assumed to be the path of the resource associated with the Set-Cookie header.  If path is '/', we keep it NULL.
  short path_len; // cookie path length (used for passing in free)
  char *cookie_val; //Cookie value (allocated buffer) (NULL Teminated). NULL means node is not valid
  short cookie_val_len; //Cookie value length
  struct cookieNode *next; /* makes list */
  int expires; // Set from expires attributes or -1 if expires is not present
} CookieNode;

// Auto Cookie Modes
#define AUTO_COOKIE_DISABLE 0                      //default is disable
#define AUTO_COOKIE_NAME_PATH_DOMAIN 1
#define AUTO_COOKIE_NAME_ONLY 2
#define AUTO_COOKIE_NAME_PATH 3
#define AUTO_COOKIE_NAME_DOMAIN 4

// Auto Cookie Expires Modes
#define AUTO_COOKIE_IGNORE_EXPIRES    0  // Do not process expires attribute of cookie
#define AUTO_COOKIE_EXPIRES_PAST_ONLY 1  // Check only if expiry is in the past and then delete cookie
#define AUTO_COOKIE_EXPIRES_ALL       2  // Check expiry for past and for future time (Not supported)

//extern int g_auto_cookie_mode; 
//#ifdef NS_DEBUG_ON
extern char s_cookie_buf[8192 + 1]; // This is used for multiple purpose - used to store cookie name, cookie value and printing Cookie
//#endif
extern int kw_set_auto_cookie(char *buf, char *err_msg, int runtime_flag);
extern void delete_all_cookies_nodes(VUser* vptr);
extern void free_cookie_value(VUser* vptr);
extern inline int save_auto_cookie(char* cookie_buffer, int cookie_buffer_len, connection* cptr, int http2_flag);
extern inline int insert_auto_cookie(connection* cptr, VUser* vptr, NSIOVector *ns_iovec);

extern inline char *ns_get_cookie_val_auto_mode(char *cookie_name, char *domain, char *path, VUser *my_vptr);
extern inline int ns_set_cookie_val_auto_mode(char *cookie_name, char *domain, char *path, char *cookie_val, VUser *my_vptr);
extern int ns_get_cookie_mode_auto();
extern int ns_add_cookie_val_auto_mode(char *cookie_name, char *domain, char *path, char *cookie_val, VUser *my_vptr);
extern void free_auto_cookie_line(connection *cptr);
extern char *get_all_cookies(VUser *vptr, char *cookie_buf, int max_cookie_buf_len);
extern void reset_cookie_flag(VUser *vptr);
extern void find_and_remove_specific_cookie(VUser *vptr, char *cookie_name, char *path, char *domain, int free_for_next_req);
extern CookieNode *search_auto_cookie(CookieNode *head, char *domain, char *path, int url_domain_len);
extern char *cookie_node_to_string(CookieNode *cn, char *buf);
#endif /* NS_AUTO_COOKIE_H_ */

