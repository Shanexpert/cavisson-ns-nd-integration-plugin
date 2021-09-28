/******************************************************************
 * Name    : ns_auto_cookie.c
 * Author  : Archana
 * Purpose : This file contains methods related to implement auto cookie feature
             - Parse AUTO_COOKIE Keyword
             - Set Cookie in the HTTP response
             - Set-Cookie Linked List management
             - Sending Cookie in the HTTP request
             - Cookie Related APIs
 * Note:
 * Modification History:
 * 04/10/08 - Initial Version

  Set-Cookie format: 
  Begin BNF:
    token         = 1*<any allowed-chars except separators>
    value         = token-value | quoted-string
    token-value   = 1*<any allowed-chars except value-sep>
    quoted-string = ( <"> *( qdtext | quoted-pair ) <"> )
    qdtext        = <any allowed-chars except <">>             ; CR | LF removed by necko
    quoted-pair   = "\" <any OCTET except NUL or cookie-sep>   ; CR | LF removed by necko
    separators    = ";" | "="
    value-sep     = ";"
    cookie-sep    = CR | LF
    allowed-chars = <any OCTET except NUL or cookie-sep>
    OCTET         = <any 8-bit sequence of data>
    LWS           = SP | HT
    NUL           = <US-ASCII NUL, null control character (0)>
    CR            = <US-ASCII CR, carriage return (13)>
    LF            = <US-ASCII LF, linefeed (10)>
    SP            = <US-ASCII SP, space (32)>
    HT            = <US-ASCII HT, horizontal-tab (9)>

    set-cookie    = "Set-Cookie:" cookies
    cookies       = cookie *( cookie-sep cookie )
    cookie        = [NAME "="] VALUE *(";" cookie-av)    ; cookie NAME/VALUE must come first
    NAME          = token                                ; cookie name
    VALUE         = value                                ; cookie value
    cookie-av     = token ["=" value]

    valid values for cookie-av (checked post-parsing) are:
    cookie-av     = "Path"    "=" value
                  | "Domain"  "=" value
                  | "Expires" "=" value
                  | "Max-Age" "=" value
                  | "Comment" "=" value
                  | "Version" "=" value
                  | "Secure"
                  | "HttpOnly"

  Set-Cookie: <name>=<value>[; <name>=<value>]... [; expires=<date>][; domain=<domain_name>] [; path=<some_path>][; secure][; httponly] (line will end using \r\n)
**Examples: 
    Set-Cookie: emp_name=newvalue; expires=date; path=/; domain=.example.org.(line will end using \r\n)
    Set-Cookie: emp_name=newvalue; emp_address=jss; expires=date; path=/; domain=.example.org.(line will end using \r\n)
    Set-Cookie: emp_name=newvalue; (line will end using \r\n)
    Set-Cookie: emp_name=newvalue (line will end using \r\n)

*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_http_hdr_states.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_cookie.h"
#include "ns_alloc.h"
#include "ns_auto_cookie.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_http_cache_hdr.h"
#include "nslb_date.h"
#include "nslb_time_stamp.h"
#include "nslb_util.h"
#include "ns_parent.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

static char s_cookie_value[MAX_COOKIE_LENGTH];
//int g_auto_cookie_mode = AUTO_COOKIE_DISABLE; //Default is disable
 
/* --- START: Method used during Parsing Time  ---*/

/*
 * Description        : kw_set_auto_cookie() method used to parse AUTO_COOKIE keyword. And set values of mode and expires_mode. 
 *                      Format:
 *                         AUTO_COOKIE <mode> <expires_mode>
 *                            Where mode is:
 *                               0 Disable auto cookie, i.e. do not use Auto cookie (Default).
 *                               1 Use auto cookie with name, path and domain.
 *                               2 Use auto cookie with name only.
 *                               3 Use auto cookie with name and path only.
 *                               4 Use auto cookie with name and domain only.
 *                             Where expire_mode is:
 *                               0 Do not use expires attribute.
 *                               1 Use expires attribute.
 *                               2 Check expiry for past and for future time (Not supported).   
 * Input Parameter     
 *           buf      : Pointer in the received buffer for the keyword.
 *           err_msg  : Print error message.
 * Output Parameter   : Set global_settings->g_auto_cookie_mode and global_settings->g_auto_cookie_expires_mode.             
 * Return             : None                         
*/

int kw_set_auto_cookie(char *buf, char *err_msg, int runtime_flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;
  char auto_mode[MAX_DATA_LINE_LENGTH];
  auto_mode[0] = 0;
  char expires_mode[MAX_DATA_LINE_LENGTH];
  expires_mode[0] = 0; // Set to empty so that we can check if it was given in keyword or not
  int expires = AUTO_COOKIE_IGNORE_EXPIRES; // Set default value

  NSDL2_COOKIES(NULL, NULL, "Method called. scenario line = %s", buf); 

  num = sscanf(buf, "%s %s %s %s", keyword, auto_mode, expires_mode, tmp); // This is used to check number of fields for AUTO_COOKIE keyword  
  NSDL3_COOKIES(NULL, NULL, "Number of arguments for AUTO_COOKIE keyword = %d", num);
  
  if((num < 2) || (num > 3)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_COOKIE_USAGE, CAV_ERR_1011024, CAV_ERR_MSG_1);
  }
  
  if(ns_is_numeric(auto_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_COOKIE_USAGE, CAV_ERR_1011024, CAV_ERR_MSG_2);
  }
  
  mode = atoi(auto_mode);
  
  NSDL3_COOKIES(NULL, NULL, "AUTO_COOKIE mode = %d", mode);
  
  if((mode < 0) || (mode > 4)) //Validation for mode option. 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_COOKIE_USAGE, CAV_ERR_1011024, CAV_ERR_MSG_3);
  }

  if(strcmp(expires_mode, "")) // expires mode given in the keyword
  {
    if(ns_is_numeric(expires_mode) == 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_COOKIE_USAGE, CAV_ERR_1011024, CAV_ERR_MSG_2);
    }
    expires = atoi(expires_mode);
    if((expires < 0) || (expires > 1)) //Validation for expire_mode option.
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_COOKIE_USAGE, CAV_ERR_1011024, CAV_ERR_MSG_3);
    }
  }

  NSDL3_COOKIES(NULL, NULL, "AUTO_COOKIE expires_mode =%d", expires);

  global_settings->g_auto_cookie_mode = mode; // Set g_auto_cookie_mode value. 
  global_settings->g_auto_cookie_expires_mode = expires; // Set g_auto_cookie_expires_mode value.
  NSDL2_COOKIES(NULL, NULL, "Exiting method.");
  return 0;
}


/* --- END: Method used during Parsing Time  ---*/

/* --- START: Method used to set Cookie in the HTTP response  ---*/

/*
**Purpose: Get cookie field value based on attr_name
**Arguments:
  Arg1 – cl is complete cookie buffer line 
  Arg2 – length is cookie buffer line length
  Arg3 – attr_name is to compare for "domain=" and "path=" with cookie buffer line
  Arg4   attr_len added to get attribute length in cookie buffer line
**Return: 
  return value of compared attribute if found else return NULL. This value is malloced buffer
*/
static char *get_cookie_attribute_value(char *cl, int length, char *attr_name, int *attr_len)
{
  int i;
  int start_n = 0;
  int dlim = -1;
  int end_v = -1;
  int len;
  char *value = NULL; // Set value to NULL in case attribute is not found

  NSDL2_COOKIES(NULL, NULL, "Method called. cookie_line = %*.*s, length = %d, attr_name = %s", length, length, cl, length, attr_name);

  for (i = start_n; i < length; i++) 
  {
    if (cl[i] == ' ') continue; // Skip space
    if (cl[i] == ';') { i++; while ((cl[i] == ' ') && (i < length)) i++; start_n = i; continue; } // Skip space after ;

    if (cl[i] == '=')  // Found '='
    {
      dlim = i; // dlim is index for =
      if (!strncasecmp(cl + start_n, attr_name, strlen(attr_name)))
      {
        NSDL3_COOKIES(NULL, NULL, "Attribute %s found", attr_name);
        for (i++; i < length; i++)  // Loop till end of line
        {
          if (cl[i] == ';') { end_v = i; break; } // Found ;
        }
        if (end_v == -1) end_v = i; // This is to handle no ; after attribute

        len = end_v - dlim - 1; // Reduce by one for '=' sign
 
        if(len) 
        {
          MY_MALLOC(value , len + 1, "value ", -1); // Allocate one exta for NULL termination
          strncpy(value, cl + dlim + 1, len);
          value[len] = '\0';
          *attr_len = len;  // Length of attribute without NULL bytes
        }
        return value;
      } 
      else 
      {
        /* skip the rest */
        /* we find the end_v and set new start_n */
        for (i++; i < length; i++) 
        {
          if (cl[i] == ';') { start_n = i + 1; break; }
        }
        i--;
        continue; /* break off main for */
      }
    }
  }
  return value;
}

/**
 * Returns NULL if name value pair is not found, cookie_name and cookie_val are set to NULL.
 * Return cookie node if new name value pair is found.
 */
//Get cookie name and value till found and skip "domain=" or "path=" or "expires=" 
//This method will handle multiple cookies in one Set-Cookie header line
static CookieNode *get_next_name_value_pair(char *cl, int length, int *start_from)
{
  int i;
  int start_n = *start_from;
  int dlim = -1;
  int end_v = -1;
  int len;
  CookieNode *cn = NULL;

  NSDL2_COOKIES(NULL, NULL, "Method called. cookie line = %*.*s, length = %d, starting point = %d", length, length, cl, length, start_n);

  for (i = start_n; i < length; i++) 
  {
    if (cl[i] == ' ') continue;
    if (cl[i] == ';') { i++; while (cl[i] == ' ' && i < length) i++; start_n = i; continue; }

    if (cl[i] == '=') 
    {
      dlim = i;

      // To find cookie name and value, skip all attributes
      if (!strncasecmp(cl + start_n, "domain=", strlen("domain=")) ||
          !strncasecmp(cl + start_n, "path=", strlen("path=")) ||
          !strncasecmp(cl + start_n, "max-age=", strlen("max-age=")) ||
          !strncasecmp(cl + start_n, "comment=", strlen("comment=")) ||
          !strncasecmp(cl + start_n, "version=", strlen("version=")) ||
          !strncasecmp(cl + start_n, "expires=", strlen("expires="))) {

        /* we find the end_v and set new start_n */
        for (i++; i < length; i++) 
        {
          if (cl[i] == ';') {
            start_n = i + 1;
            break;
          }
        }
        i--;
        continue; /* break off main for */
      }
      else 
      {
        for (i++; i < length; i++) {
         if (cl[i] == ';') {
            break;
          }
        }
        end_v = i;

        //Once get expires=<date>; domain=<domain_name>; path=<some_path> 
        //allocate memory to CookieNode
        MY_MALLOC(cn , sizeof(CookieNode), "cn ", -1);
        memset(cn, 0, sizeof(CookieNode)); // To make all fields 0 or NULL

        //calculate length using dlim (=) and start location of cookie name 
        len = dlim - start_n;

        //allocate memory for cookie name and cookie value, and copy name and value if length is not zero.
        if (len) {
          MY_MALLOC(cn->cookie_name , len + 1, "cn->cookie_name ", -1);
          strncpy(cn->cookie_name, cl + start_n, len);
          cn->cookie_name[len] = '\0';
          cn->cookie_name_len = len;
        }
        len = end_v - dlim;
        if(len - 1) {
          MY_MALLOC(cn->cookie_val , len + 1, "cn->cookie_val ", -1);
          strncpy(cn->cookie_val, cl + dlim + 1, len - 1);
          cn->cookie_val[len - 1] = '\0';
          cn->cookie_val_len = len - 1;
        } 
        
        *start_from = end_v;
        return cn;
      }
    }
  }
  return NULL;
}

/* --- START: Method used to set-Cookie Linked List management  ---*/

//To convert CookieNode to string for printing cookie info
//If domain and path not specified then it will print null (NULL handled by code)
#ifdef NS_DEBUG_ON
char s_cookie_buf[MAX_COOKIE_LENGTH]; // This is used for multiple purpose - used to store cookie name, cookie value and printing CookieNode
char *cookie_node_to_string(CookieNode *cn, char *buf)
{
  // Note - Some fields can be NULL. DL will log as null
  sprintf(buf, "Cookie Name = %s, "
               "Cookie Value = %s, "
               "Cookie name length = %d, "
               "Cookie Value length = %d, "
               "Path = %s, " 
               "Domain = %s ",
               cn->cookie_name, 
               cn->cookie_val, 
               cn->cookie_name_len, 
               cn->cookie_val_len, 
               cn->path?cn->path:"NULL",
               cn->domain?cn->domain:"NULL"); 

  return(buf);
}
#endif

//To delete cookie node from link list
static void do_cn_free(CookieNode *cn) 
{
  if (cn)
  {
    NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Node => %s", cookie_node_to_string(cn, s_cookie_buf));

    FREE_AND_MAKE_NOT_NULL_EX(cn->cookie_name, (cn->cookie_name_len + 1) , "cn->cookie_name", -1); //Added cookie name length
    FREE_AND_MAKE_NOT_NULL_EX(cn->domain,(cn->domain_len + 1), "cn->domain", -1);// Added cookie domain length
    FREE_AND_MAKE_NOT_NULL_EX(cn->path, (cn->path_len + 1), "cn->path", -1);// Added cookie path length
    FREE_AND_MAKE_NOT_NULL_EX(cn->cookie_val, (cn->cookie_val_len + 1), "cn->cookie_val", -1); // Added cookie value length
    
    FREE_AND_MAKE_NOT_NULL_EX(cn, sizeof(CookieNode), "cn", -1); // added node size
  } 
}

//To delete all cookies node from list
void delete_all_cookies_nodes(VUser *vptr)
{
  CookieNode *ptr, *old;
  NSDL2_COOKIES(vptr, NULL, "Method called.");

  ptr = (CookieNode *)vptr->uctable;
  while (ptr) 
  {
    old = ptr;
    ptr = ptr->next;
    do_cn_free(old);
  }
  vptr->uctable = NULL;
}
//This method is used to compare old cookie node with new cookie node,cookie nodes match then return 1 else return 0.
static int compare_cookie(CookieNode *c_new, CookieNode *c_old)
{
  NSDL2_COOKIES(NULL, NULL, "Method called.");

  NSDL3_COOKIES(NULL, NULL, "Old Cookie Node => %s", cookie_node_to_string(c_old, s_cookie_buf));
  NSDL3_COOKIES(NULL, NULL, "New Cookie Node => %s", cookie_node_to_string(c_new, s_cookie_buf));

  if (!strcmp(c_old->cookie_name, c_new->cookie_name)) 
  {
    NSDL2_COOKIES(NULL, NULL, "Found Cookie Node with same name. Checking other key values");
    if (c_new->domain)
    {
      /* Issue - If we got Set-Cookie not for any domain, then we got same cookie with some domain, then
                 we will have two nodes and we will set two times same cookie in Req 
                 Same issue is for path */
      if (c_old->domain)
      {
        if(strcmp(c_old->domain, c_new->domain)) 
        {
          NSDL2_COOKIES(NULL, NULL, "New Cookie Node is not matching with old cookie node due to domain");
          return 0;
        }
      }
      else
        NSDL2_COOKIES(NULL, NULL, "New Cookie Node came with domain but old cookie node does not have domain");
    }
    if (c_new->path)
    {
      if (c_old->path)
      {
        if(strcmp(c_old->path, c_new->path)) 
        {
          NSDL2_COOKIES(NULL, NULL, "New Cookie Node is not matching with old cookie node due to path");
          return 0;
        }
      }
      else
        NSDL2_COOKIES(NULL, NULL, "New Cookie Node came with path but old cookie node does not have path");
    }
    NSDL2_COOKIES(NULL, NULL, "New Cookie Node is matching with old cookie node");
    return 1;
  }
  NSDL2_COOKIES(NULL, NULL, "New Cookie Node is not matching with old cookie node, cookie name does not matched");
  return 0;
}

//This method to add a node at the front end of the link list CookieNode (head is vptr->uctable).
static void add_cookie_to_list(CookieNode *cn, VUser *vptr)
{
  CookieNode *ptr;
  int cookie_count = 0; // Used for debug only

  NSDL2_COOKIES(vptr, NULL, "Method called. Cookie Node => %s", cookie_node_to_string(cn, s_cookie_buf));

  if (!vptr->uctable) //Check if no node exist in list
  {
    //Add node at the tail of linked list
    NSDL3_COOKIES(vptr, NULL, "No node exist in list. So adding node at the head/tail of list.");
    vptr->uctable = (void *)cn;
  } 
  else 
  {
    ptr = (CookieNode *)vptr->uctable;
    while (ptr) 
    {
      if (!(cn->cookie_val)) // Got cookie value as empty string from server
      {
        if (compare_cookie(cn, ptr)) 
        {
          NSDL3_COOKIES(vptr, NULL, "Add Cookie Node with empty value - Found cookie node in the list which same key. Making this Node invalid");
          FREE_AND_MAKE_NULL_EX(ptr->cookie_val, (ptr->cookie_val_len + 1), "ptr->cookie_val", -1); // Previous node value.
          ptr->cookie_val_len = 0;
          do_cn_free(cn); // Free new node as it is not needed
          return;
        }
      }
      else if (compare_cookie(cn, ptr)) 
      {
        /* replace */
        NSDL3_COOKIES(vptr, NULL, "Add Cookie Node with valid value - Found cookie node in the list which same key. Replacing this Node with new value");
        FREE_AND_MAKE_NOT_NULL_EX(ptr->cookie_val, (ptr->cookie_val_len + 1), "ptr->cookie_val", -1); //Free old cookie value buffer
        ptr->cookie_val = cn->cookie_val;   //Reuse same buffer to avoid extra malloc
        cn->cookie_val = NULL; //Make NULL so that do_cn_free() do not free it
        ptr->cookie_val_len = cn->cookie_val_len;
        do_cn_free(cn); // Free new node as it is not needed
        return;
      } 
      else if(ptr->next == NULL) 
      {
        NSDL3_COOKIES(vptr, NULL, "Add Cookie Node with valid value - Not Found cookie node in the list which same key. Adding this Node at the end of the list. Total cookie node count = %d", cookie_count);
        ptr->next = cn;
        return;
      }
      cookie_count++;
      ptr = ptr->next;
    }
  }
}

/*
 * Description      :delete_cookie_from_list() method use to delete a node from the list CookieNode (head is vptr->uctable).
 *                   Search cookie node in the list and then delete that node.
 * Input Parameter  
 *       cn         :Pointer refers to cookie node which need to be deleted. 
 *       vptr       :Pointer refers to vitrual user structure.
 * Output Parameter :None      
 * Return           :None        
 *
*/
static void delete_cookie_from_list(CookieNode *cn, VUser *vptr)
{
  CookieNode *prev_node, *cur_node, *head;

  NSDL2_COOKIES(vptr, NULL, "Method called. Cookie Node => %s", cookie_node_to_string(cn, s_cookie_buf));
  head = (CookieNode *)vptr->uctable; 
  if(!vptr->uctable) // Check if no node exist in list
  {
    NSDL3_COOKIES(vptr, NULL, "No node exist in list.");
    return;
  }
  
  if(cn->cookie_name == NULL) // Check if cookie name is a null string. 
  {
    NSDL3_COOKIES(vptr, NULL, "Cookie name is NULL");
    return;
  }
  
  prev_node = cur_node = head;
  while(cur_node) // Iterate loop till no cookie node left in list.
  {
    NSDL4_COOKIES(vptr, NULL, "Checking Cookie Node => %s", cookie_node_to_string(cur_node, s_cookie_buf));
    
    if(compare_cookie(cn,cur_node)) 
    {
      if(cur_node == head) // Cookie node found is the head node of list.
      {
        NSDL3_COOKIES(vptr, NULL, "Found cookie at head with expired date. Deleting cookie node => %s", cookie_node_to_string(cur_node, s_cookie_buf));
        vptr->uctable = (void *)cur_node->next; 
      }
      else
      {
        NSDL3_COOKIES(vptr, NULL, "Found cookie with expired date but not at head. Deleting cookie node => %s", cookie_node_to_string(cur_node, s_cookie_buf));
        prev_node->next = cur_node->next;
      }
      do_cn_free(cur_node);// Deleting cookie node.
      return;
    }

    prev_node = cur_node;
    cur_node = cur_node->next;//Traversing cookie list.
  }
  NSDL3_COOKIES(vptr, NULL, "Cookie node not found.");
} 
  
/* --- END: Method used to set-Cookie Linked List management  ---*/

static inline char *alloc_and_copy(char *src, int len, char *msg, int index)
{
  char *dest;
  NSDL2_COOKIES(NULL, NULL, "Method called. length = %d, message = %s", len, msg);
  MY_MALLOC(dest, len + 1, msg, index);
  strcpy(dest, src);
  return dest;
}

// req_line is like GET /mysite/project1/xyz.html HTTP/1.1
// In this case, path will be populate with "/mysite/project1/"
// In case, URL is paramterized, URL may not be complete and HTTP/1.1 is not there
// GET /catalog/index.ognc?CategoryID=
static int extract_path_of_url_from_req_line(char *req_line, char *path)
{
  char /* url_start,*/ *url_path_end;
  int len;

  NSDL2_COOKIES(NULL, NULL, "Method called. req_line = %s", req_line);
  strcpy(path, req_line);
#if 0
  url_start = strstr(req_line, " ");
  if(url_start == NULL)
  {
    fprintf(stderr, "extract_path_of_url_from_req_line() - Invalid req_line = %s\n", req_line);
    strcpy(path, "");
    return 0;
  }
  url_start++; //skip space

  strcpy(path, url_start);
  url_path_end = strstr(path, " ");
  if(url_path_end == NULL)
    NSDL2_COOKIES(NULL, NULL, "req_line is not complete. req_line = %s", req_line);
  else
    *url_path_end = '\0'; // Null terminate till before space
#endif
  url_path_end = strrchr(path, '/');
  if(url_path_end != NULL)
  {
    *(url_path_end + 1) = '\0';
    len = url_path_end - path + 1; // Do not do strlen for optimizatio.
  }
  else // Should not come here but to make sure we check it. This is the case when the URL 
       // is truncated.
    len = strlen(path);

  return(len);
}

/*This method is used to parse set cookie header. And calculate expiry date for the cookie node.
**Arguments:
    Arg1 – cookie_line: This is Cookie line to parse 
    Arg2 – cookie_length:  
    Arg3 - vptr:
*/
static void parse_set_cookie_header(char *cookie_line, int cookie_length, VUser *vptr, connection *cptr)
{
  CookieNode *cn;
  char *domain = NULL;
  char *path = NULL;
  char *cookie_expire_date = NULL;
  int start_from = 0;
  char req_url_path[10000];
  int current_time, expire_value;
  int expires = -1; // Set to -1 if case expires attribute is not found
  int domain_len = 0; // domain length
  int path_len = 0;  // path length
  int attr_len = 0;  // attribute length

  NSDL2_COOKIES(NULL, cptr, "Method called. cookie line = %*.*s, length = %d", cookie_length, cookie_length, cookie_line, cookie_length);
  
  if (global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH_DOMAIN || global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_DOMAIN)
  {
    domain = get_cookie_attribute_value(cookie_line, cookie_length, "domain=", &domain_len);// added domain length
    NSDL3_COOKIES(NULL, cptr, "Domain length= %d", domain_len);
    if(domain) // Domain is present
    {
      NSDL3_COOKIES(NULL, cptr, "Domain found in Set-Cookie. Domain = %s", domain);
    }
    else // domain == NULL means Domain is not present, then use domain of the requested URL
    {
      domain_len = strlen(cptr->old_svr_entry->server_name); //domain length for default domain of cookie
      domain = alloc_and_copy(cptr->old_svr_entry->server_name, domain_len, "Default domain of cookie", -1);
      NSDL3_COOKIES(NULL, cptr, "Domain not present in Set-Cookie. Defaulting to domain of the HTTP request. Domain = %s", domain);
    }
  }
  if (global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH_DOMAIN || global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH)
  {
    path = get_cookie_attribute_value(cookie_line, cookie_length, "path=", &path_len);// added path length
    NSDL3_COOKIES(NULL, cptr, "Path length = %d", path_len);
    if(path) // Path is present
    {
      NSDL3_COOKIES(NULL, cptr, "Path found in Set-Cookie. Path = %s", path);
    }
    else // path == NULL means Path is not present, then use path of the requested URL
    {
      char *req_url_line;
      // In case of Auto Redirect, request line has the URL requested
      req_url_line = get_url_req_url(cptr);
      path_len = extract_path_of_url_from_req_line(req_url_line, req_url_path);//path length for default path of cookie
      path = alloc_and_copy(req_url_path, path_len, "Default path of cookie", -1);
      NSDL3_COOKIES(NULL, cptr, "Path not present in Set-Cookie. Defaulting to path of the HTTP request. Path = %s", path);
    }
  }

  if(global_settings->g_auto_cookie_expires_mode != AUTO_COOKIE_IGNORE_EXPIRES)//Check g_auto_cookie_expires_mode is set to 1
  {
    cookie_expire_date = get_cookie_attribute_value(cookie_line, cookie_length, "expires=", &attr_len);// attribute length

    if(cookie_expire_date)
    { 
      NSDL3_COOKIES(NULL, cptr, "Expires attribute found in Set_cookie. Expires = %s", cookie_expire_date);    
      expire_value = cache_convert_http_hdr_date_in_secs(cptr, cookie_expire_date, "Set-Cookie: expired attribute", COOKIE_EXPIRES);// To get cookie expiry date in seconds.
      NSDL3_COOKIES(NULL, cptr, "Calculated expire_value in secs = %d", expire_value);
      if(expire_value != -1) { // For valid expires attribute value. 
        current_time = (get_ns_start_time_in_secs() + get_ms_stamp()/1000 + get_timezone_diff_with_gmt());//To find current time in secs
        NSDL3_COOKIES(NULL, cptr, "Current time in secs = %d", current_time);
        expires = expire_value - current_time; // To get cookie expires attribute for relative time. 
        NSDL3_COOKIES(NULL, cptr, "Expires relative time = %d", expires);      
        NSDL3_COOKIES(NULL, cptr, "Calculated expire_value in secs = %d", expire_value);
      }else { 
        NSDL3_COOKIES(NULL, cptr, "Expires attribute found in Set_cookie,date with incorrect format. Expires = %s", cookie_expire_date);      }  
    }else{
      NSDL3_COOKIES(NULL, cptr, "Expires not present in Set-Cookie. Set expires to -1.");       
    }
  }
  else
  {
    NSDL3_COOKIES(NULL, cptr, "Keyword g_auto_cookie_expires_mode was not set.");
  } 
 
  while ((cn = get_next_name_value_pair(cookie_line, cookie_length, &start_from))) 
  {
    if(global_settings->g_auto_cookie_expires_mode == AUTO_COOKIE_EXPIRES_PAST_ONLY) // Handle expires attibutes
    {
      NSDL3_COOKIES(NULL, cptr, "Keyword g_auto_cookie_expires_mode was set.");
      if((expires != -1) && (expires <= 0))
      {
        NSDL3_COOKIES(NULL, cptr, "Found cookie with expired date. Deleting cookie node.");
        delete_cookie_from_list(cn, vptr); // Deleting cookie node with past expiry date.
        continue;
      }
    }   
    //Expires attribute value is either -1 or greater than 0 .Or g_auto_cookie_expires_mode is not set.
    //For multiple Set-Cookie header.  
    cn->expires = expires; // Not used. This will be used when we enable AUTO_COOKIE_EXPIRES_ALL
    if (domain) 
    {
      cn->domain = alloc_and_copy(domain, domain_len, "Domain of cookie", -1); 
      cn->domain_len = domain_len;
    }
    if (path) 
    {
      cn->path = alloc_and_copy(path, path_len, "Path of cookie", -1);
      cn->path_len = path_len;
    }
    add_cookie_to_list(cn, vptr);// Add cookie node to the list 
  }
  // Since domain and path are for all cookies coming in set cookie, we need to free it after while loop
  FREE_AND_MAKE_NOT_NULL_EX(domain, (domain_len + 1) , "domain", -1); // Here while passing size we include NULL
  FREE_AND_MAKE_NOT_NULL_EX(path, (path_len + 1), "path", -1); // Here while passing size we include NULL
}

//This method call if Cookie was not received in one read, then allocate buffer and keep in cptr. This buffer need to be free using this method
void free_auto_cookie_line(connection *cptr)
{
  void *ptr = ((char *)cptr->cookie_hash_code);

  NSDL2_COOKIES(NULL, cptr, "Method called. cptr->cookie_hash_code = %p, ptr->cookie_idx = %d", ptr, cptr->cookie_idx);

  FREE_AND_MAKE_NOT_NULL_EX(ptr, cptr->cookie_idx, "cptr->cookie_hash_code", -1); // previous node size??
  cptr->cookie_hash_code = (ns_ptr_t)NULL;  // Make it NULL to indicate cookie line is not there
  cptr->cookie_idx = 0; // cookie_idx is misued to keep lenght of cookie line
}

//To save cookie line if partial received
static inline void save_cookie_line(char* cookie_buffer, connection *cptr, int length)
{
  void *ptr = ((char *)cptr->cookie_hash_code);

  NSDL2_COOKIES(NULL, cptr, "Method called. Recieved cookie line = %*.*s, length = %d. Previous Cookie length = %d, Prev Cookie Line = %s", length, length, cookie_buffer, length, cptr->cookie_idx, (char *)(cptr->cookie_hash_code));

  short prev_length = cptr->cookie_idx; // This keeps the prev length
  cptr->cookie_idx += length;

  MY_REALLOC_EX(ptr, cptr->cookie_idx, prev_length, "Set-Cookie Line", -1); 
  cptr->cookie_hash_code = (ns_ptr_t)ptr;

  strncpy((char *)cptr->cookie_hash_code + prev_length, cookie_buffer, length);
}

/*
**Purpose:
    This method will extract cookie from the response and store in the linked list.  
    It uses strstr() for \r\n to see if end of cookie is received or not.
    It will handle all cases:
      1.If end of header (\r\n) received
          Case 1 [No earlier and complete now (Complete in one go)] 
             - Parse cookie and add cookie in the linked list depend on mode. 
          Case 2 [Partial earlier, complete now]
             – Realloc buffer
             - Append with existing cookie. 
             - Parse cookie and add cookie in the linked list depend on mode. 
             - Free buffer.
          Set state HANDLE_READ_NL_CR
      2.If end of header (\r\n) not received
          Case 3 [No earlier and partial now (Not complete in one read. It will go to next read)]
             – Realloc buffer
             - Save cookie. 
          Case 4 [Partial earlier, partial now]
             - Realloc buffer
             - Append with previous 
             - Save cookie. 
          Set state HDST_SET_COOKIE_MORE_COOKIE

**Arguments:
    Arg1 – cookie_buffer: Pointer in the received buffer for the cookie. 
    This auto cookie_value buffer contains – cookie name, domain and path for cookie(with \r\n) and last \r\n.
    Arg2 – cookie_buffer_len: Number of bytes available
    Arg3 – cptr: Connection pointer which is used for received response

**Return:
    Set cptr->header_state:
      HANDLE_READ_NL_CR (set if get whole cookie)
      HDST_SET_COOKIE_MORE_COOKIE (set if get partial cookie)
    Number of consumed bytes.
*/
inline int save_auto_cookie(char* cookie_buffer, int cookie_buffer_len, connection* cptr, int http2_flag)
{
  int consumed_bytes;
  char *cookie_end;
  VUser *vptr = cptr->vptr;
  int length;

  NSDL2_COOKIES(NULL, cptr, "Method called, set-cookie line = %*.*s, length = %d, cookie_idx = %d, cookie hash code = %d",
			    cookie_buffer_len, cookie_buffer_len, cookie_buffer, cookie_buffer_len,
			    cptr->cookie_idx, cptr->cookie_hash_code);

  /* Cookie is DISABLED*/
  if(global_settings->cookies == COOKIES_DISABLED) {
     NSDL2_COOKIES(NULL, cptr, "Cookie is DISABLED");
     cptr->header_state = HDST_TEXT;
     return 0;
  }

  // Check if end of set-cookie line is recived 
  //if((cookie_end = memchr(cookie_buffer, "\r\n", cookie_buffer_len)))
  // Issue - memchr only take ont char. So we are only checking \r.
  if(http2_flag || (cookie_end = memchr(cookie_buffer, '\r', cookie_buffer_len)))
  {

    if (http2_flag) {
      length = cookie_buffer_len;
    } else 
      length = cookie_end - cookie_buffer;
    // Case 1 - Complete set-cookie line recieved in one read
    //if(cptr->header_state == HDST_SET_COOKIE_COLON_WHITESPACE)
    if(cptr->cookie_idx == 0)
    { 
      NSDL3_COOKIES(NULL, cptr, "Complete set-cookie line received in one read");
      //if(global_settings->cookies != COOKIES_DISABLED) parse_set_cookie_header(cookie_buffer, length, vptr, cptr);
      parse_set_cookie_header(cookie_buffer, length, vptr, cptr);
    }
    // Case 2 - Complete set-cookie line recieved now and was partially recieved earlier
    else
    {
      NSDL3_COOKIES(NULL, cptr, "Complete cookie line received now and it was partial earlier");
      save_cookie_line(cookie_buffer, cptr, length);
      parse_set_cookie_header((char*)cptr->cookie_hash_code, cptr->cookie_idx, vptr, cptr);
      free_auto_cookie_line(cptr);
    }
    consumed_bytes = length;   // add 2 for \r\n
    cptr->header_state = HDST_CR; // Complete set-cookie header line processed. Set state to CR to parse next header line
  }
  else 
  {
    /*
    // Case 3 - Parital set-cookie line recieved first time
    if(cptr->header_state == HDST_SET_COOKIE_COLON_WHITESPACE)
    {
      NSDL3_COOKIES(NULL, cptr, "Parital set-cookie line recieved first time");
      cptr->cookie_idx = 0;
      cptr->cookie_hash_code = 0;
    }
    // Case 4 - Parital set-cookie line recieved and was partially recieved earlier
    else
    {
      NSDL3_COOKIES(NULL, cptr, "Partial set-cookie line recieved and was partially recieved earlier"); 
    }
    //if(global_settings->cookies != COOKIES_DISABLED) 
    */
    save_cookie_line(cookie_buffer, cptr, cookie_buffer_len);
    consumed_bytes = cookie_buffer_len; 
    //cptr->header_state = HDST_SET_COOKIE_MORE_COOKIE;
  }

  NSDL3_COOKIES(NULL, cptr, "consumed_bytes = %d", consumed_bytes);
  return(consumed_bytes);
}

/* --- END: Method used to set Cookie in the HTTP response  ---*/

/* --- START: Method used during Run Time to send Cookie in the HTTP request  ---*/


// Return 1 if match else 0
static inline int is_domain_match(char *cookie_domain, char *url_domain, int cookie_domain_len, int url_domain_len)
{
  NSDL2_COOKIES(NULL, NULL, "Method called, cookie domain name = %s, url domain name = %s," 
                  " cookie domain len =%d, url domain len = %d", 
                   cookie_domain, url_domain, cookie_domain_len, url_domain_len);

  int ulen = 0, clen = 0, len = 0;
  char domain[1024], *cookie_end_ptr = NULL, *url_end_ptr = NULL;
  char domain_without_port[1024], cdomain_without_port[1024];
  //Initialize
  clen = cookie_domain_len;
  ulen = url_domain_len;
  strcpy(cdomain_without_port, cookie_domain);
  strcpy(domain_without_port, url_domain);

  //Find PORT  
  cookie_end_ptr = index (cookie_domain, ':'); 
  url_end_ptr = index (url_domain, ':');

  //Find cookie domain length without port
  if (cookie_end_ptr)
  {
    clen = (cookie_end_ptr - cookie_domain);
    cdomain_without_port[clen] = '\0';
    NSDL2_COOKIES(NULL, NULL, "Cookie domain found with port," 
                " clen= %d, cookie_domain_len = %d", clen, cookie_domain_len);
  }
  //Find url domain length without port
  if (url_end_ptr)
  {
    ulen = (url_end_ptr - url_domain);
    domain_without_port[ulen] = '\0';
    NSDL2_COOKIES(NULL, NULL, "URL domain found with port," 
              " ulen= %d, url_domain_len = %d", ulen, url_domain_len);
  }

  if (url_domain_len == -1 && cookie_domain_len == -1)
    len = strlen(url_domain) - strlen(cookie_domain);
  else
    len = ulen - clen;

  strcpy(domain, domain_without_port + len);

  NSDL2_COOKIES(NULL, NULL, "Extracted domain name from url domain name is %s, len = %d", domain, strlen(domain));

  if(!strcmp(domain, cdomain_without_port)) {
    NSDL2_COOKIES(NULL, NULL, "match");
    return 1;
  }
  return 0;
}

/*

Purpose: To search cookie name and value from list depend on mode for requested domain and path
Arguments
  head - Cookie node head 
  domain - domain of the URL. if domain is NULL, then not to be check
  path - path of the URL. if path is NULL, then not to be check.
*/
CookieNode *search_auto_cookie(CookieNode *head, char *domain, char *path, int url_domain_len)
{
  CookieNode *s_ptr = head;
  CookieNode *ptr;
  int  chk_domain = 0, chk_path = 0;
  
  NSDL2_COOKIES(NULL, NULL, "Method called, domain = %s, path = %s, head = %p", domain, path, head);

  while (s_ptr) 
  {
    ptr = s_ptr;
    s_ptr = s_ptr->next;

    NSDL3_COOKIES(NULL, NULL, "Checking Cookie Node. Cookie Node => %s", cookie_node_to_string(ptr, s_cookie_buf));

    //if cookie is empty, then ignore it 
    if (ptr->cookie_val == NULL || ptr->flag)
    { 
      NSDL2_COOKIES(NULL, NULL, "Cookie is empty or flag set to skip cookie from request, so ignoring it.");
      continue; /* Skip NULL'ed nodes */
    }

    chk_domain = 0; chk_path = 0;

    if (global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH) 
      if ((ptr->path) && (path) && (path[0] != 0)) chk_path = 1;

    if (global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_DOMAIN) 
      if ((ptr->domain) && (domain) && (domain[0] != 0)) chk_domain = 1;

    if (global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH_DOMAIN) 
    {
      if ((ptr->path) && (path) && (path[0] != 0)) chk_path = 1;
      if ((ptr->domain) && (domain) && (domain[0] != 0)) chk_domain = 1;
    }

    if (!chk_path && !chk_domain)
    {
      NSDL3_COOKIES(NULL, NULL, "Node matched with key. Path and Domain are not part of key");
      return ptr;
    }

    if (chk_path && !chk_domain)
    {
      if(!strncmp(ptr->path, path, strlen(ptr->path))) 
      {
        NSDL3_COOKIES(NULL, NULL, "Node matched with key. Only Path is part of key");
        return ptr;
      }
      else
      {
        NSDL3_COOKIES(NULL, NULL, "Node not matched with key. Only Path is part of key");
      }
    }
    
    if (!chk_path && chk_domain)
    {
      if(is_domain_match(ptr->domain, domain, ptr->domain_len, url_domain_len)) 
      {
        NSDL3_COOKIES(NULL, NULL, "Node matched with key. Only Domain is part of key");
        return ptr;
      }
      else
      {
        NSDL3_COOKIES(NULL, NULL, "Node not matched with key. Only Domain is part of key");
      }
    }

    if (chk_path && chk_domain)
    {  
      if(!strncmp(ptr->path, path, strlen(ptr->path)) && (is_domain_match(ptr->domain, domain, ptr->domain_len, url_domain_len))) 
      {
        NSDL3_COOKIES(NULL, NULL, "Node matched with key. Both Path and Domain is part of key");
        return ptr;
      }
      else
      {
        NSDL3_COOKIES(NULL, NULL, "Node not matched with key. Either Path or Domain or both is not part of key");
      }
    }
  }
  NSDL3_COOKIES(NULL, NULL, "No more cookie nodes");
  return NULL;
}

static CookieNode *get_auto_cookie_node(VUser *cur_vptr, char *name, char *domain, char *path)
{
  CookieNode *cnode;
  cnode = (CookieNode *)cur_vptr->uctable;
  
  NSDL1_COOKIES(cur_vptr, NULL, "Method called. cur_vptr = %p, name = %s, domain = %s, path = %s", 
                 cur_vptr, name, domain, path);
  //In this case we are comparing cookie node within cookie table hence check complete length
  while((cnode = search_auto_cookie(cnode, domain, path, -1))) {
    NSDL3_COOKIES(cur_vptr, NULL, "Searching : name = %s/%s, domain = %s/%s, path = %s|%s\n", cnode->cookie_name, 
           name, cnode->domain, domain, cnode->path, path);
    if (!strcmp(cnode->cookie_name, name)) {
      return cnode;
    }
    cnode = cnode->next;
  }

  /* Not found */
  return NULL;
}

static inline int insert_cookie_in_vector(VUser* vptr, CookieNode *cnode)
{
  //Insert "Cookie name" in vector for request
  NSDL3_COOKIES(vptr, NULL, "Insert cookie name = %s in vector for request", cnode->cookie_name);
  NS_FILL_IOVEC(g_req_rep_io_vector, cnode->cookie_name, cnode->cookie_name_len);

  //Insert "=" in vector for request
  NSDL3_COOKIES(vptr, NULL, "Insert '=' in vector for request");
  NS_FILL_IOVEC(g_req_rep_io_vector, EQString, EQString_Length);

  //Insert "Cookie value" in vector for request
  NSDL3_COOKIES(vptr, NULL, "Insert Cookie value = %s in vector for request", cnode->cookie_val);
  NS_FILL_IOVEC(g_req_rep_io_vector, cnode->cookie_val, cnode->cookie_val_len);

  return NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
}

inline void fill_cookie(NSIOVector *ns_iovec, CookieNode *cnode)
{
  NSDL2_COOKIES(NULL, NULL, "Method called");

  //Insert "Cookie name" in vector for request
  NSDL3_COOKIES(NULL, NULL, "Insert cookie name = %s in vector for request", cnode->cookie_name);
  NS_FILL_IOVEC(*ns_iovec, cnode->cookie_name, cnode->cookie_name_len);
  
  //Insert "=" in vector for request
  NSDL3_COOKIES(NULL, NULL, "Insert '=' in vector for request");
  NS_FILL_IOVEC(*ns_iovec, EQString, EQString_Length);
  
  //Insert "Cookie value" in vector for request
  NSDL3_COOKIES(NULL, NULL, "Insert Cookie value = %s in vector for request", cnode->cookie_val);
  NS_FILL_IOVEC(*ns_iovec, cnode->cookie_val, cnode->cookie_val_len);
}

#define FILL_FIRST_COOKIE(ns_iovec, cnode) fill_cookie(ns_iovec, cnode)

#define FILL_NEXT_COOKIE(ns_iovec, cnode) \
{\
  NSDL3_COOKIES(vptr, cptr, "Insert '; ' in vector for request");\
  NS_FILL_IOVEC(*ns_iovec, SCString, SCString_Length);\
  fill_cookie(ns_iovec, cnode);\
}

//To insert searched cookie node depend on mode into vector to send response cookie for next request
//Request will be in following format
//Cookie: <name>=<value> [;<name>=<value>]...
inline int insert_auto_cookie(connection* cptr, VUser* vptr, NSIOVector *ns_iovec)
{
  CookieNode *cnode;
  char *req_url_domain, *req_url_path;

  NSDL2_COOKIES(vptr, cptr, "Method called");

  //req_url_domain = cptr->url_num->proto.http.index.svr_ptr->server_hostname; //Domain for requested url
  req_url_domain = cptr->old_svr_entry->server_name; // Domain for requested url (Must use Mapped host)
  //Path for request

  req_url_path = get_url_req_url(cptr);
  NSDL3_COOKIES(vptr, cptr, "HTTP Request URL Path = %s, URL Domain = %s", req_url_path, req_url_domain);

  cnode = (CookieNode *)vptr->uctable;

  // Cookie: <name>=<value>; <name2>=<value2>\r\n
  //Search cookie name and value from list depend on auto cookie mode
  cnode = search_auto_cookie(cnode, req_url_domain, req_url_path, cptr->old_svr_entry->server_name_len);
  if(cnode)
  {
    NS_CHK_AND_GROW_IOVEC(vptr, *ns_iovec, io_vector_delta_size);
    NSDL3_COOKIES(vptr, cptr, "Cookie Node => %s", cookie_node_to_string(cnode, s_cookie_buf));
    NSDL3_COOKIES(vptr, cptr, "Insert 'Cookie: ' in vector for request");
    //Insert "Cookie: " in vector for request, this will insert as cookie header only for first time
    NS_FILL_IOVEC(*ns_iovec, CookieString, CookieString_Length);
    FILL_FIRST_COOKIE(ns_iovec,cnode);
    while((cnode = search_auto_cookie(cnode->next, req_url_domain, req_url_path, cptr->old_svr_entry->server_name_len)))
    {
      FILL_NEXT_COOKIE(ns_iovec,cnode);
    }
    // Add end of header marker ("\r\n")
    NSDL3_COOKIES(vptr, cptr, "Add end of header marker");
    NS_FILL_IOVEC(*ns_iovec, CRLFString, CRLFString_Length);
  } 
  NSDL2_COOKIES(vptr, cptr, "Returning from insert auto cookie");
  return 0;
}

/* --- END: Method used during Run Time to send Cookie in the HTTP request  ---*/


/* --- START: Method used for Cookie API ---*/

//This method returns Pointer to cookie value populated in a static buffer or NULL if cookie name is not found.
// if domain or path NULL, then not to be checked
char *ns_get_cookie_val_auto_mode(char *cookie_name, char *domain, char *path, VUser *my_vptr)
{
  CookieNode *cnode;
  char *ptr;
  int len;

  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s", cookie_name, domain, path);

  if((cnode = get_auto_cookie_node(my_vptr, cookie_name, domain, path)) == NULL)
  {
    NSDL2_COOKIES(NULL, NULL, "ns_get_cookie_val_auto_mode(): Cookie Node not found for Cookie %s, domain %s, path %s", cookie_name, domain, path);
    return NULL;
  }

  NSDL3_COOKIES(NULL, NULL, "Cookie Node => %s", cookie_node_to_string(cnode, s_cookie_buf));
 
  if(cnode->cookie_name == NULL)
  {
    NSDL2_COOKIES(NULL, NULL, "ns_get_cookie_val_auto_mode(): Cookie value is NULL for Cookie %s, domain %s, path %s", cookie_name, domain, path);
    return NULL;
  }
  ptr = cnode->cookie_val;
  len = cnode->cookie_val_len;

  if (len <= 0)
  {
    NSDL2_COOKIES(NULL, NULL, "ns_get_cookie_val_auto_mode(): Cookie len is <= 0 for Cookie %s, domain %s, path %s", cookie_name, domain, path);
    return NULL;
  }

  if (len >= MAX_COOKIE_LENGTH)
  {
    fprintf(stderr, "ns_get_cookie_val_auto_mode(): Cookie value len %d more than 8K. Truncating to 8K\n", len);
    len = MAX_COOKIE_LENGTH - 1;
  }
 
  bcopy (ptr, s_cookie_value, len);
  s_cookie_value[len]= '\0';
 
  return (s_cookie_value);
}

char *get_all_cookies(VUser *vptr, char *cookie_buf, int max_cookie_buf_len) {
  char *tmp_cookies_ptr;
  //char *cur_cookies_ptr;
  int tmp_cookies_len = 0;
  int cookie_path_len, cookie_domain_len, cookie_buf_left;
  int cookie_complete_len; 

  CookieNode *head = (CookieNode *)vptr->uctable;

  NSDL2_COOKIES(NULL, NULL, "Method called");

  if(vptr == NULL) {
     NSDL2_COOKIES(NULL, NULL, "vptr is null.");
     return NULL;
  }  

  if(cookie_buf && cookie_buf[0]) {
    tmp_cookies_ptr = cookie_buf;
    cookie_buf_left = max_cookie_buf_len;
  } else {
    tmp_cookies_ptr = s_cookie_value;
    cookie_buf_left = MAX_COOKIE_LENGTH;
  }
    
  //cur_cookies_ptr = tmp_cookies_ptr;

  CookieNode *s_ptr = head;
  CookieNode *ptr;
  tmp_cookies_ptr[0] = '\0';
  
  while (s_ptr) {
    ptr = s_ptr;
    s_ptr = s_ptr->next;

    NSDL3_COOKIES(NULL, NULL, "Checking Cookie Node. Cookie Node => %s",
                               cookie_node_to_string(ptr, s_cookie_buf));

    if (ptr->cookie_val == NULL) { 
      NSDL2_COOKIES(NULL, NULL, "Cookie is empty, so ignoring it.");
      continue; /* Skip NULL'ed nodes */
    }

    if(ptr->path) {
       cookie_path_len = strlen(ptr->path);
    } else {
       cookie_path_len = 0; 
    }

    if(ptr->domain) {
       cookie_domain_len = strlen(ptr->domain);
    } else {
       cookie_domain_len = 0;
    }

    cookie_complete_len = ptr->cookie_name_len + 1 /*=*/ + ptr->cookie_val_len + 1 /*;*/ + 
           (cookie_path_len > 0 ? (5 /*strlen("path=")*/ + cookie_path_len + 1 /*;*/):0) +
           (cookie_domain_len > 0 ? (7 /*strlen("domain=")*/ + cookie_domain_len + 1 /*;*/):0) + 1/*,*/;

    NSDL2_COOKIES(NULL, NULL, "cookie_complete_len = %d", cookie_complete_len);

    if((tmp_cookies_len + cookie_complete_len) < cookie_buf_left) {
      NSDL2_COOKIES(NULL, NULL, "tmp_cookies_ptr = [%s], tmp_cookies_len = %d", tmp_cookies_ptr, tmp_cookies_len);
      tmp_cookies_len += cookie_complete_len; 
      sprintf(tmp_cookies_ptr, "%s%s=%s;",
                               tmp_cookies_ptr,
                               ptr->cookie_name,
                               ptr->cookie_val);

      NSDL2_COOKIES(NULL, NULL, "tmp_cookies_ptr = [%s], tmp_cookies_len = %d", tmp_cookies_ptr, tmp_cookies_len);
      if(cookie_path_len) {
        sprintf(tmp_cookies_ptr, "%spath=%s;",
                                 tmp_cookies_ptr,
                                 ptr->path);

      }
      NSDL2_COOKIES(NULL, NULL, "tmp_cookies_ptr = [%s], tmp_cookies_len = %d", tmp_cookies_ptr, tmp_cookies_len);

      if(cookie_domain_len) {
        sprintf(tmp_cookies_ptr, "%sdomain=%s;",
                                 tmp_cookies_ptr,
                                 ptr->domain);

      }
      if(tmp_cookies_ptr[tmp_cookies_len - 2] == ';'){
  
        tmp_cookies_ptr[tmp_cookies_len - 2] = '\0';
        tmp_cookies_len--;
      }
     
      NSDL2_COOKIES(NULL, NULL, "tmp_cookies_ptr = [%s], tmp_cookies_len = %d", tmp_cookies_ptr, tmp_cookies_len);
      strcat(tmp_cookies_ptr, ",");
      NSDL2_COOKIES(NULL, NULL, "tmp_cookies_ptr = [%s], tmp_cookies_len = %d", tmp_cookies_ptr, tmp_cookies_len);
    } else {
      break;
    }

  }

  tmp_cookies_ptr[tmp_cookies_len - 1] = '\0';
 NSDL2_COOKIES(NULL, NULL, "tmp_cookies_ptr = [%s], tmp_cookies_len = %d", tmp_cookies_ptr, tmp_cookies_len);
  
  return (tmp_cookies_ptr);
}
// This method returns 0 is successful otherwise returns -1 if cookie name is not found in list node
// if domain or path NULL, then not to be checked
int ns_set_cookie_val_auto_mode(char *cookie_name, char *domain, char *path, char *cookie_val, VUser *my_vptr)
{
  char *ptr;
  CookieNode *cnode;
  int len = 0;
  
  if(cookie_val != NULL) 
    len = strlen(cookie_val);

  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s, Cookie Value = %s", 
                             cookie_name, domain, path, (cookie_val && *cookie_val) ? cookie_val : "NULL");
  
  cnode = (CookieNode *)my_vptr->uctable;
  if((cnode = get_auto_cookie_node(my_vptr, cookie_name, domain, path)) == NULL)
  {
    NSDL2_COOKIES(NULL, NULL, "ns_set_cookie_val_auto_mode(): Cookie Node not found for Cookie %s, domain %s, path %s", cookie_name, domain, path);
    return -1;
  }

  NSDL3_COOKIES(NULL, NULL, "Cookie Node => %s", cookie_node_to_string(cnode, s_cookie_buf));

  if(cnode->cookie_name == NULL)
  {
    NSDL2_COOKIES(NULL, NULL, "ns_set_cookie_val_auto_mode(): Cookie value is NULL for Cookie %s, domain %s, path %s", cookie_name, domain, path);
    return -1;
  }

  if(cookie_val == NULL)
  {
    FREE_AND_MAKE_NOT_NULL_EX(cnode->cookie_val, (cnode->cookie_val_len + 1), "Overwrite old Cookie Value ", -1);
    NSDL2_COOKIES(NULL, NULL, "setting cookie = %s to NULL", cnode->cookie_name);
    cnode->cookie_val = NULL;
    cnode->cookie_val_len = 0;
    return 0;
  }
 
  ptr = cnode->cookie_val;

  FREE_AND_MAKE_NOT_NULL_EX(ptr, (cnode->cookie_val_len + 1), "Overwrite old Cookie Value ", -1);

  ptr = alloc_and_copy(cookie_val, len, "Cookie Value ", -1);

  cnode->cookie_val = ptr;
  cnode->cookie_val_len = len;

  return 0;
}

int ns_get_cookie_mode_auto()
{
  NSDL2_COOKIES(NULL, NULL, "Method called."); 
  return (global_settings->g_auto_cookie_mode);
}

/**
 * Function assumes atleast cookie_name to be not null.
 */
static CookieNode *new_cookie_node(char *cookie_name, char *domain, char *path, char *cookie_val)
{
  CookieNode *cn;
  int len;

  MY_MALLOC_AND_MEMSET(cn , sizeof(CookieNode), "cn ", -1);

  len = strlen(cookie_name);
  cn->cookie_name = alloc_and_copy(cookie_name, len, "Cookie name", -1);// allocate and copy new cookie name.
  cn->cookie_name_len = len;

  if (path) {
    len = strlen(path); 
    cn->path = alloc_and_copy(path, len, "Path of cookie", -1);// allocate and copy cookie path.
    cn->path_len = len;
  }
  if (cookie_val) {
    len = strlen(cookie_val); 
    cn->cookie_val = alloc_and_copy(cookie_val, len, "Cookie value", -1);// allocate and copy new cookie value.
    cn->cookie_val_len = len;
  }
  if (domain) {
    len = strlen(domain);
    cn->domain = alloc_and_copy(domain, len, "Domain of cookie", -1);// allocate and copy cookie domain.
    cn->domain_len = len;
  }
  
  return cn;
}

/**
 * This function is called from ns_string_api.c:ns_add_cookie_val_ex()
 */
int ns_add_cookie_val_auto_mode(char *cookie_name, char *domain, char *path, char *cookie_val, VUser *my_vptr)
{
  CookieNode *cnode;

  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s, Cookie Value = %s", cookie_name, domain, path, cookie_val);
  

  if (!cookie_name || !cookie_name[0]) {
    NSDL2_COOKIES(NULL, NULL, "ns_add_cookie_val_auto_mode(): Cookie Name value is NULL for Cookie %s, domain %s, path %s", cookie_name, domain, path);
    fprintf(stderr, "Error - cookie_name supplied in API can not be null\n");
    return -1;
  }

  cnode = new_cookie_node(cookie_name, domain, path, cookie_val);

  NSDL3_COOKIES(NULL, NULL, "Cookie Node => %s", cookie_node_to_string(cnode, s_cookie_buf));

  add_cookie_to_list(cnode, my_vptr);
  return 0;
}

#define CONTINUE_TO_NXT_NODE \
    NSDL4_COOKIES(vptr, NULL, "Continue to next node");\
    prev_node = cnode;\
    cnode = cnode->next;\
    continue;

/********************************************************************************************
Purpose: This function read an arguments from ns_remove_cookie() API and search cookie in the 
	 list. If cookie found then we remove specific cookie from list otherwise we continue
	 We can remove cookie with name, domain and path. 
Input:	 VUser *vptr->uctable  -> which is pointing the head node of CookieNode for every user 
	 cookie table.
	 cookie_name, domain, path -> All arguments read which is passed by user in 
	 ns_remove_cookie API.  
********************************************************************************************/
void find_and_remove_specific_cookie(VUser *vptr, char *cookie_name, char *path, char *domain, int free_for_next_req)
{
  CookieNode *prev_node, *cnode, *tmp;
  NSDL2_COOKIES(vptr, NULL, "Method called");
  //Check if no node exist in list
  if(!vptr->uctable)
  {
    NSDL3_COOKIES(vptr, NULL, "No node exist in list.");
    return;
  }
  prev_node = cnode = (CookieNode *)vptr->uctable;
  
  //run until cnode not equal to NULL
  while(cnode)
  { 
    //if cookie name not NULL
    if(cookie_name != NULL) 
    {
      NSDL4_COOKIES(vptr, NULL, "Given cookie name = %s, Cnode->name = %s", cookie_name, cnode->cookie_name);
      if((cnode->cookie_name == NULL) || strcmp(cnode->cookie_name, cookie_name)) {
        CONTINUE_TO_NXT_NODE 
      }
    }

    if(domain != NULL)
    {
      NSDL4_COOKIES(vptr, NULL, "Given domain name = %s, cnode->domain = %s", domain, cnode->domain);
      if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH_DOMAIN || global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_DOMAIN)
      {  
        if (strcmp(cnode->domain, domain)) {
          CONTINUE_TO_NXT_NODE   
        }
      }
    }

    if(path != NULL)
    { 
      NSDL4_COOKIES(vptr, NULL, "Given path = %s, Cnode->path = %s", path, cnode->path);
      if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH_DOMAIN || global_settings->g_auto_cookie_mode == AUTO_COOKIE_NAME_PATH) 
      {
        if(strcmp(cnode->path, path)) {
          CONTINUE_TO_NXT_NODE
        }
      } 
    } 
    //Only mark it. No need for else part as it will get break when cnode become NULL
    if(free_for_next_req) 
    {
       cnode->flag = 1;
       CONTINUE_TO_NXT_NODE
    }

    if(cnode == (CookieNode *)vptr->uctable) //if node is head
    {
      NSDL4_COOKIES(vptr, NULL, "Found cookie specification at head, Deleting cookie node => %s", cookie_node_to_string(cnode, s_cookie_buf));
      tmp = cnode; 
      vptr->uctable = (void *)cnode->next;
      prev_node = cnode = cnode->next; 
      do_cn_free(tmp);  //delete the node
    }
    else 
    { 
      NSDL4_COOKIES(vptr, NULL, "Found cookie specification not at head, Deleting cookie node => %s", cookie_node_to_string(cnode, s_cookie_buf));
      tmp = cnode;
      prev_node->next = cnode = cnode->next;
      do_cn_free(tmp);
    }
  } 
}

void reset_cookie_flag(VUser *vptr)
{
  NSDL1_COOKIES(vptr, NULL, "Method called");
  CookieNode *cnode = (CookieNode *)vptr->uctable;
  while(cnode)
  {
    cnode->flag = 0;
    cnode = cnode->next;
  }
}
/* --- END: Method used for Cookie API ---*/
