/******************************************************************
 * #include "logging.h"
 * #include "ns_ssl.h"
 * #include "ns_fdset.h"
 * #include "ns_goal_based_sla.h"
 * #include "ns_schedule_phases.h"
 *
 * Name    : ns_cookie.c
 * Author  : Archana
 * Purpose : This file contains methods related to 
             parsing keyword, shared memory, run time for cookies
 * Note:
 * Modification History:
 * 10/09/08 - Initial Version
 * 18/09/08 - Last modification
*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <regex.h>
#include <nghttp2/nghttp2.h>

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
#include "url.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_http_hdr_states.h"
#include "ns_cookie.h"
#include "ns_alloc.h"
#include "ns_auto_cookie.h"
#include "ns_url_req.h"
#include "ns_parent.h"
#include "nslb_hash_code.h"
#include "ns_h2_req.h"
#include "ns_exit.h"
#include "ns_script_parse.h"

//#define MAX_COOKIE_FILE_SIZE 20
#define DELTA_COOKIE_ENTRIES 8
#define DELTA_REQCOOK_ENTRIES 4*1024
#define INIT_COOKIE_ENTRIES 24

char* CookieString = "Cookie: ";
int CookieString_Length = 8;

char EQString[2] = "=";
int EQString_Length = 1;

char SCString[2] = "; ";
int SCString_Length = 2;

static char ms_cookie_buf[MAX_COOKIE_LENGTH]; // This is used for multiple purpose - used to store cookie name, cookie value and printing CookieNode

int (*cookie_hash)(const char*, unsigned int);
//in_cookie_hash() used to get cookie index of whole cookie received
int (*in_cookie_hash)(const char*, unsigned int);
int (*cookie_hash_parse)(const char*, unsigned int);
//in_cookie_hash_parse() used to get hash code of each partial cookie received 
int (*in_cookie_hash_parse)(const char*, unsigned int);
const char* (*cookie_get_key)(unsigned int);


#ifndef CAV_MAIN
CookieTableEntry* cookieTable;
int total_cookie_entries;
int max_cookie_entries = 0;
int max_reqcook_entries;
int total_reqcook_entries;
int max_cookie_hash_code;
#else
__thread CookieTableEntry* cookieTable;
__thread int total_cookie_entries;
__thread int max_cookie_entries = 0;
__thread int max_reqcook_entries;
__thread int total_reqcook_entries;
__thread int max_cookie_hash_code;
#endif

/* --- START: Method used during Parsing Time  ---*/

//This method used to parse DISABLE_COOKIES keyword
// Format:
//   DISABLE_COOKIES <value>
//   Where value is 0 (default) to enable cookies and 1 to disable cookies
int kw_set_cookies(char *keyword, char *text, char *err_msg)
{
  NSDL2_COOKIES(NULL, NULL, "Method called. Keyword = %s, value = %s ", keyword, text);
  global_settings->cookies = atoi(text);
  NSDL3_COOKIES(NULL, NULL, "global_settings->cookies = %d", global_settings->cookies);
  return 0;
}

void free_cookie_value(VUser* vptr)
{
  int i;
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    for (i = 0; i < max_cookie_hash_code; i++)
    {
      FREE_AND_MAKE_NULL(vptr->uctable[i].cookie_value, "vptr->uctable[i].cookie_value", i);
    }
  }
  else
    delete_all_cookies_nodes(vptr);  //to remove all auto cookie nodes from list
}

/* This method used to parse COOKIE keyword for main url in script.detail for legacy scripts
   For C Type, it used for both main and inline (embedded) URL cookies
   Format:
     COOKIE=<cookie-name>; <cookie-name2>;
   Notes: Space after ; is optional.
          ; after last cookie is optional
          There should NOT be any space before and after =
          \r\n is not at end of line. line is NULL terminated
  Method return
    0 - Line is for cookie
    1 - Line is not for cookie

   Example:
   Valid Cookie Header
     COOKIE=s_cc;s_sq;COOKIE_SID;SMSESSION;wfacookie;MethodTracker;BILLPAY_COOKIE;ISD_BP_COOKIE;ISD_DAS_COOKIE;HttpOnly;  
*/
int add_main_url_cookie(char *line, int line_number, char *det_fname, int cur_request_idx, int sess_idx)
{
  char* cookie_ptr;
  int cookie_length;
  char cookie[MAX_LINE_LENGTH];
  int rnum;

  NSDL2_COOKIES(NULL, NULL, "Method called, Line = %s, det_fname = %s, cur_request_idx = %d, sess_idx = %d", line, det_fname, cur_request_idx, sess_idx);

  // If Cookies are enabled and AUTO_COOKIE mode is disabled 
  if ((global_settings->cookies == COOKIES_ENABLED) && (global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)) 
  {
    cookie_ptr = line;
    while (cookie_ptr) 
    {
      char *cookie_end_ptr = strchr(cookie_ptr, ';');
      if (cookie_end_ptr != NULL) // ; found
        cookie_length = strchr(cookie_ptr, ';') - cookie_ptr;
      else
        cookie_length = strlen(cookie_ptr);

/*
      {
        fprintf(stderr, "add_main_url_cookie(): Wrong format. Expected COOKIE=<cookie-name>; in file %s at line %d\n", det_fname, line_number);
        exit(-1);
      }
*/
      if (cookie_length + 1 >= MAX_LINE_LENGTH) 
      {
        bcopy (cookie_ptr, cookie, MAX_LINE_LENGTH);
        cookie[MAX_LINE_LENGTH-1] = '\0';
        NS_EXIT(-1, "add_main_url_cookie(): too big cookie (%s ...) in file %s at line %d\n", cookie, det_fname, line_number);
      }

      bcopy (cookie_ptr, cookie, cookie_length);
      cookie[cookie_length] = '\0';
      NSDL2_COOKIES(NULL, NULL, "Cookie at line = %s, cur_request_idx = %d, sess_idx = %d", line, cur_request_idx, sess_idx);
      rnum = Create_cookie_entry(cookie, sess_idx);
      Create_reqcook_entry(cookieTable[rnum].name, cookie_length, cur_request_idx);

      cookie_ptr = cookie_end_ptr; // point to next cookie if any
      if (cookie_ptr)  // ; was there
      {
        cookie_ptr++; // point next to ;
        CLEAR_WHITE_SPACE(cookie_ptr); // Remove all white spaces if any
        if ((cookie_ptr[0] == '\0') || (cookie_ptr[0] == '\r') || (cookie_ptr[0] == '\n'))
          cookie_ptr = NULL;
      }
    }
    return 0;
  }
  return 1;  //return 1 when global_settings->cookies is 1 means cookies is disabled
}

/*This method used to parse Cookie keyword for embedded url in script.detail
  Format:
    Cookie: <cookie-name1>=<cookie-value1>; <cookie-name1>=<cookie-value1>;
    Notes: Space after ; is optional.
          ; after last cookie is optional
          There should NOT be any space before and after =
    line end with /r/n. 
  Method return
    0 - Line is for cookie
    1 - Line is not for cookie
  Example:
  Format of Cookie Header
  Cookie: s_cc=true; s_sq=%5B%5BB%5D%5D; COOKIE_SID=khYfTnhTBLyY148XxvqWTZLQWQ0FXvsnTvDpyz2bGLBdPhYyLGT1!-1600299628; SMSESSION=FGG
  
  Invalid Cookie Header 
  Cookie: s_cc=true; s_sq

*/
inline int add_embedded_url_cookie(char *line, int line_num, int req_idx, int sess_idx)
{
  char* cookie_ptr;
  char* cookie_end_ptr;
  int cookie_length;
  char temp_line[MAX_LINE_LENGTH];
  int rnum;

  NSDL2_COOKIES(NULL, NULL, "Method called, Line = %s, req_idx = %d, sess_idx = %d", line, req_idx, sess_idx);

  if (strncasecmp(line, "Cookie:", strlen("Cookie:")) == 0) 
  {
    // If Cookies are enabled and AUTO_COOKIE mode is disabled 
    if ((global_settings->cookies == COOKIES_ENABLED) && (global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)) 
    {
      // cookie_ptr = line;
      cookie_ptr = line + strlen("Cookie:");
      if (cookie_ptr[0] == ' ')
        cookie_ptr++;
      
      NSDL2_COOKIES(NULL, NULL, "Cookie Line = %s", cookie_ptr);
      while (cookie_ptr) 
      {
        cookie_end_ptr = strstr(cookie_ptr, "="); 
        if(cookie_end_ptr) // = found
        {
          cookie_length = cookie_end_ptr - cookie_ptr;
        }
        else 
        {
          NS_EXIT(-1, "Invalid cookie header syntax(%s) at line=%d missing = after cookie name(%s)", line, line_num,cookie_ptr);
        }
        NSDL2_COOKIES(NULL, NULL, "Cookie Length = %d", cookie_length);
        
        if (cookie_length + 1 >= MAX_LINE_LENGTH) 
        {
          bcopy (cookie_ptr, temp_line, MAX_LINE_LENGTH);
          temp_line[MAX_LINE_LENGTH-1] = '\0';
          NS_EXIT(-1, "add_embedded_url_cookie(): too big cookie (%s ...) at line=%d\n", temp_line, line_num);
        }
        bcopy (cookie_ptr, temp_line, cookie_length);
        temp_line[cookie_length] = '\0';
        NSDL2_COOKIES(NULL, NULL, "Cookie name = %s", temp_line);
        rnum = Create_cookie_entry(temp_line, sess_idx);
           
        Create_reqcook_entry(cookieTable[rnum].name, cookie_length, req_idx);
        
        cookie_ptr = strstr(cookie_ptr, ";");
        if (cookie_ptr) 
        {
          cookie_ptr++;
          CLEAR_WHITE_SPACE(cookie_ptr); // Remove all white spaces if any
          if ((cookie_ptr[0] == '\0') || (cookie_ptr[0] == '\r') || (cookie_ptr[0] == '\n'))
            cookie_ptr = NULL;
        }
      }
      return 0;
    } 
    else // if (global_settings->cookies == COOKIES_DISABLED)
      return 0;
  }
  return 1;
}
/* --- END: Method used during Parsing Time  ---*/


/* --- START: Method used during Shared Memory  ---*/
void init_cookie_info(void)
{
  NSDL2_COOKIES(NULL, NULL, "Method called.");
  
  total_cookie_entries = 0;

  MY_MALLOC(cookieTable , INIT_COOKIE_ENTRIES * sizeof(CookieTableEntry), "cookieTable ", -1);

  if(cookieTable)
  {
    max_cookie_entries = INIT_COOKIE_ENTRIES;
  }
  else
  {
    max_cookie_entries = 0;
    NS_EXIT(-1, CAV_ERR_1031013, "CookieTableEntry");
  }
}

static int create_cookie_table_entry(int *row_num) 
{
  NSDL2_COOKIES(NULL, NULL, "Method called");
  if (total_cookie_entries == max_cookie_entries) {
    MY_REALLOC (cookieTable,
		(max_cookie_entries + DELTA_COOKIE_ENTRIES) *
		sizeof(CookieTableEntry), "cookieTable", -1);
    if (!cookieTable) {
      fprintf(stderr,"create_cookie_table_entry(): Error allocating more memory for cookie entries\n");
      return(FAILURE);
    } else max_cookie_entries += DELTA_COOKIE_ENTRIES;
  }
  *row_num = total_cookie_entries++;
  return (SUCCESS);
}

static int create_reqcook_table_entry(int *row_num) 
{
  NSDL2_COOKIES(NULL, NULL, "Method called");
  if (total_reqcook_entries == max_reqcook_entries) {
    MY_REALLOC (reqCookTable, (max_reqcook_entries + DELTA_REQCOOK_ENTRIES) *
		sizeof(ReqCookTableEntry), "reqCookTable ", -1);
    if (!reqCookTable) {
      fprintf(stderr,"create_reqcook_table_entry(): Error allocating more memory for reqcook entries\n");
      return(FAILURE);
    } else max_reqcook_entries += DELTA_REQCOOK_ENTRIES;
  }
  *row_num = total_reqcook_entries++;
  return (SUCCESS);
}

void Create_reqcook_entry (int cname_offset, int cookie_length, int req_idx)
{
  int rnum;

   NSDL2_COOKIES(NULL, NULL, "Method called, cname_offset = %d, cookie_length = %d, req_idx = %d", cname_offset, cookie_length, req_idx);
   if (create_reqcook_table_entry(&rnum) != SUCCESS) {
      fprintf(stderr, "Create_reqcook_entry(): Error, could not allocate memory for reqcook table\n");
      NS_EXIT(-1, "Create_reqcook_entry(): Error, could not allocate memory for reqcook table\n");
    }

    reqCookTable[rnum].name = cname_offset;
    reqCookTable[rnum].length = cookie_length;

    if (requests[req_idx].proto.http.cookies.cookie_start == -1) {
      requests[req_idx].proto.http.cookies.cookie_start = rnum;
      requests[req_idx].proto.http.cookies.num_cookies = 0;
    }

    requests[req_idx].proto.http.cookies.num_cookies++;
}

static int find_cookie_idx(char* name, int sess_idx) 
{
  int i;

  NSDL2_COOKIES(NULL, NULL, "Method called, name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].cookievar_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].cookievar_start_idx; i < gSessionTable[sess_idx].cookievar_start_idx + gSessionTable[sess_idx].num_cookievar_entries; i++)
    if (!strcmp(RETRIEVE_BUFFER_DATA(cookieTable[i].name), name))
      return i;

  return -1;
}

int Create_cookie_entry (char *cookie, int sess_idx) 
{
  int rnum;

    NSDL2_COOKIES(NULL, NULL, "Method called, cookie = %s, sess_idx = %d", cookie, sess_idx);
    rnum = find_cookie_idx(cookie, sess_idx);
    if (rnum == -1) {
              if (create_cookie_table_entry(&rnum) != SUCCESS) {
                NS_EXIT(-1, "Create_cookie_entry(): Error, could not allocate memory for cookie table\n");
              }

              if (gSessionTable[sess_idx].cookievar_start_idx == -1) {
                gSessionTable[sess_idx].cookievar_start_idx = rnum;
                gSessionTable[sess_idx].num_cookievar_entries = 0;
              }

              gSessionTable[sess_idx].num_cookievar_entries++;

              if ((cookieTable[rnum].name = copy_into_big_buf(cookie, strlen(cookie))) == -1) {
                NS_EXIT(-1, "Create_cookie_entry(): Error in copying data into big buf\n");
              }

              cookieTable[rnum].sess_idx = sess_idx;
    }
    return rnum;
}

// -- Add Achint- For global cookie - 10/04/2007

static void Create_reqcook_entry_for_session (int cname_offset, int cookie_length, int sess_idx)
{
int rnum;

   NSDL2_COOKIES(NULL, NULL, "Method called, cname_offset = %d, cookie_length = %d, sess_idx = %d", cname_offset, cookie_length, sess_idx);
   if (create_reqcook_table_entry(&rnum) != SUCCESS) {
      NS_EXIT(-1, "Create_reqcook_entry_for_session(): Error, could not allocate memory for reqcook table\n");
    }

    reqCookTable[rnum].name = cname_offset;
    reqCookTable[rnum].length = cookie_length;

    if (gSessionTable[sess_idx].cookies.cookie_start == -1) {
      gSessionTable[sess_idx].cookies.cookie_start = rnum;
      gSessionTable[sess_idx].cookies.num_cookies = 0;
    }
    gSessionTable[sess_idx].cookies.num_cookies++;
    NSDL3_COOKIES(NULL, NULL, "Cookie for session, total cookies = %d", gSessionTable[sess_idx].cookies.num_cookies);
}

int input_global_cookie (char* line, int line_number, int sess_idx, char *fname, char *script_filename)
{
  char *cookie_ptr;
  short cookie_length;
  char cookie[MAX_LINE_LENGTH];
  int rnum;

  NSDL2_COOKIES(NULL, NULL, "Method called, fname = %s, line = %s, line_number = %d, sess_idx = %d",fname, line, line_number, sess_idx);

  cookie_ptr = line;
  cookie_length = strlen(cookie_ptr);
  NSDL3_COOKIES(NULL, NULL, "cookie_ptr = %s, cookie_length = %d", cookie_ptr, cookie_length);
  if (cookie_length + 1 >= MAX_LINE_LENGTH)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012056_ID, CAV_ERR_1012056_MSG, cookie_ptr, cookie_length, MAX_LINE_LENGTH);
  }
  bcopy (cookie_ptr, cookie, cookie_length);
  cookie[cookie_length] = '\0';
  rnum = Create_cookie_entry(cookie, sess_idx);
  NSDL3_COOKIES(NULL, NULL, "Rnum = %d", rnum);
  Create_reqcook_entry_for_session(cookieTable[rnum].name, cookie_length, sess_idx);
  return 0;
}

// -- End Global cookie

// This function creates hash table for cookie.
// Fri Apr  8 04:13:48 PDT 2011 TODO: generate_hash_table can be used it is duplicated lots of places
int create_cookie_hash(void) 
{
  FILE* cookie_file, *cookie_parse_file;
  int i, j;
  char* cookie_name;
  int cookie_size;
  CookieTableEntry* cook_ptr;
  char fname[MAX_LINE_LENGTH];
  char absol_cook[MAX_LINE_LENGTH];
  int absol_cook_len;
  int sess_idx;
  char err_msg[MAX_ERR_MSG_LINE_LENGTH + 1];

  NSDL2_COOKIES(NULL, NULL, "Method called to create hash table for cookie");

  if (total_cookie_entries == 0) 
  {
    max_cookie_hash_code = 0;
    cookie_hash = NULL;
    in_cookie_hash = NULL;
    cookie_hash_parse = NULL;
    in_cookie_hash_parse = NULL;
    return 0;
  }

  /* first create cookie.txt */
  sprintf(fname, "%s/cookie.txt", g_ns_tmpdir);
  NSDL3_COOKIES(NULL, NULL, "Creating %s", fname);
  if ((cookie_file = fopen(fname, "w+")) == NULL) {
    write_log_file(NS_SCENARIO_PARSING, "Failed to create cookie file %s, error:%s", fname, nslb_strerror(errno));
    NS_EXIT(-1, CAV_ERR_1000006, fname, errno, nslb_strerror(errno));
  }

  for (sess_idx = 0; sess_idx < total_sess_entries; sess_idx++) {
    for (i = 0, cook_ptr = &cookieTable[gSessionTable[sess_idx].cookievar_start_idx]; i < gSessionTable[sess_idx].num_cookievar_entries; i++, cook_ptr++) {
      cookie_name = RETRIEVE_BUFFER_DATA(cook_ptr->name);
      sprintf(absol_cook, "%s!%s\n", cookie_name, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

      NSDL4_COOKIES(NULL, NULL, "cookie_name = %s, absolute cookie name = %s", cookie_name, absol_cook);

      NSDL3_COOKIES(NULL, NULL, "Adding absolute cookie in %s", fname);
      cookie_size = strlen(absol_cook);
      if (fwrite(absol_cook, sizeof(char), cookie_size, cookie_file) != cookie_size) {
        write_log_file(NS_SCENARIO_PARSING, "Failed to write cookie into cookie.txt file");
        NS_EXIT(-1, CAV_ERR_1000032, fname, errno, nslb_strerror(errno));
      }
    }
  }

  fclose(cookie_file);

  /* next, create cookie_parse.txt */
  sprintf(fname, "%s/cookie_parse.txt", g_ns_tmpdir);
  NSDL3_COOKIES(NULL, NULL, "Creating %s", fname);
  if ((cookie_parse_file = fopen(fname, "w+")) == NULL) {
    write_log_file(NS_SCENARIO_PARSING, "Failed to create cookie parse file %s, error:%s", fname, nslb_strerror(errno));
    NS_EXIT(-1, CAV_ERR_1000006, fname, errno, nslb_strerror(errno));
  }

  for (sess_idx = 0; sess_idx < total_sess_entries; sess_idx++) {
    for (i = 0, cook_ptr = &cookieTable[gSessionTable[sess_idx].cookievar_start_idx]; i < gSessionTable[sess_idx].num_cookievar_entries; i++, cook_ptr++) {
      cookie_name = RETRIEVE_BUFFER_DATA(cook_ptr->name);
      cookie_size = strlen(cookie_name);
      for (j = 1; j <= cookie_size; j++) {
        strncpy(absol_cook, cookie_name, j);
        absol_cook[j] = '\0';
        sprintf(absol_cook, "%s!%s\n", absol_cook, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
        absol_cook_len = strlen(absol_cook);
        if (fwrite(absol_cook, sizeof(char), absol_cook_len, cookie_parse_file) != absol_cook_len) {
          write_log_file(NS_SCENARIO_PARSING, "Failed to write cookie combo into cookie_parse.txt file");
          NS_EXIT(-1, CAV_ERR_1000032, fname, errno, nslb_strerror(errno));
        }
      }
    }
  }

  fclose(cookie_parse_file);

  NSDL3_COOKIES(NULL, NULL, "Sort line of %s/cookie_parse.txt file and make unique file %s/cookie_parse_u.txt", g_ns_tmpdir, g_ns_tmpdir);
  sprintf(fname, "sort %s/cookie_parse.txt -u > %s/cookie_parse_u.txt", g_ns_tmpdir, g_ns_tmpdir);
  if (system(fname) == -1) {
    NS_EXIT(-1, CAV_ERR_1000019, fname, errno, nslb_strerror(errno));
  }


  //replaced generating hash code to common library method 
  max_cookie_hash_code = generate_hash_table_ex_ex("cookie.txt", "in_cookie_set", &in_cookie_hash, &cookie_get_key, "hash_cookie",
                                                                                  &cookie_hash, NULL, 0, g_ns_tmpdir, err_msg);

  if(max_cookie_hash_code < 0)
  {
    write_log_file(NS_SCENARIO_PARSING, "Failed to create hash table for cookie.txt, error:%s", err_msg);
    NS_EXIT(-1, "%s", err_msg);
  }
 
  //TODO::what to do with max code ??
  int max_code = generate_hash_table_ex_ex("cookie_parse_u.txt", "in_cookie_set_parse", &in_cookie_hash_parse, &get_key_parse,
                                                              "hash_cookie_parse", &cookie_hash_parse, NULL, 0, g_ns_tmpdir, err_msg);

  if( max_code < 0)
  {
    write_log_file(NS_SCENARIO_PARSING, "Failed to create hash table for cookie_parse_u.txt, error:%s", err_msg);
    NS_EXIT(-1, "%s", err_msg);
  }

  NSDL3_COOKIES(NULL, NULL, "Successfully created hash table for cookies");
  return 0;
}

/* --- END: Method used during Shared Memory  ---*/


/* --- START: Method used during Run Time  ---*/
inline int insert_cookies(const ReqCookTab_Shr* cookies, VUser* vptr, NSIOVector *ns_iovec) 
{
  ReqCookTableEntry_Shr* cookie_entry = cookies->cookie_start;
  int cookie_entered;
  int uctable_idx;
  int num_added = 0;
  int i;
  int sess_name_length = strlen(vptr->sess_ptr->sess_name);

  NSDL2_COOKIES(vptr, NULL, "Method called");

  NS_FILL_IOVEC(*ns_iovec, CookieString, CookieString_Length);

  for (cookie_entered = 0; (ns_iovec->cur_idx < ns_iovec->tot_size) && (cookie_entered < cookies->num_cookies); cookie_entered++, cookie_entry++) {
    int cookie_buf_length = cookie_entry->length + sess_name_length + 2;
    char cookie_buf[cookie_buf_length];
    sprintf(cookie_buf, "%s!%s", cookie_entry->name, vptr->sess_ptr->sess_name);
    uctable_idx = in_cookie_hash(cookie_buf, cookie_buf_length - 1);

    NSDL4_COOKIES(vptr, NULL, "cookie_buf = %s, Uctable idx = %d, length = %d", cookie_buf, uctable_idx, vptr->uctable[uctable_idx].length);

    if ((uctable_idx == -1) || (vptr->uctable[uctable_idx].length == 0)) {
      if (uctable_idx == -1)
          fprintf(stderr, "insert_cookies(): cookie not in hash table\n");
      for (i = 0; i < 4; i++) { //fill cookiname=val; all 4 as NULL
        NS_FILL_IOVEC(*ns_iovec, NULL, 0);  //TODO:
      }
      continue;
    }
    num_added++;
    NS_FILL_IOVEC(*ns_iovec, cookie_entry->name, cookie_entry->length);

    NS_FILL_IOVEC(*ns_iovec, EQString, EQString_Length);

    NS_FILL_IOVEC(*ns_iovec, vptr->uctable[uctable_idx].cookie_value, vptr->uctable[uctable_idx].length);

    NS_FILL_IOVEC(*ns_iovec, SCString, SCString_Length);
  }

  if (num_added) { //Replace separator with end
    ns_iovec->cur_idx--;
    NS_FILL_IOVEC(*ns_iovec, CRLFString, CRLFString_Length);
  } 
  NSDL2_COOKIES(vptr, NULL, "Returning from insert cookie");
  return 0;
}

int inline is_cookies_disallowed (VUser *vptr)
{
  NSDL2_COOKIES(vptr, NULL, "Method called");
  return ((vptr->flags)& NS_COOKIES_DISALLOWED);
}

inline int save_cookie_name(char *cookie_start_name, char *cookie_end_name,
			    int cookie_len, connection* cptr) {
  char* cookie_end;
  VUser* vptr = cptr->vptr;
  int consumed_bytes;
  UserCookieEntry *vuctable = ((VUser*)cptr->vptr)->uctable;
  
  NSDL2_COOKIES(NULL, cptr, "Method called, cookie_start_name = %*.*s,"
			    " cookie_length = %d",
			    cookie_len, cookie_len,
			    cookie_start_name, cookie_len);
  
  if ((cookie_end = memchr(cookie_start_name, '=', cookie_len))) {  /* we got the whole cookie name */
    int cookie_length = cookie_end - cookie_start_name;
    int cookie_buf_length = cookie_length + strlen(vptr->sess_ptr->sess_name) + 2;
    char cookie_buf[cookie_buf_length];
    *cookie_end = '\0';
    NSDL2_COOKIES(NULL, cptr, "cookie_buf_length=%d, size of cookie_buf=%d",
			      cookie_buf_length, sizeof(cookie_buf)); 
    consumed_bytes = cookie_length;
    memcpy(cookie_buf, cookie_start_name, cookie_length);
    cookie_buf[cookie_length] = '\0';
    NSDL3_COOKIES(NULL, cptr, "Got whole cookie Name. cookie name = %s,"
			      " cookie_length = %d, cookie_buf_length = %d",
			      cookie_buf, cookie_length, cookie_buf_length);
    
    sprintf(cookie_buf, "%s!%s", cookie_buf, vptr->sess_ptr->sess_name);
    NSDL3_COOKIES(NULL, cptr, "Cookie Name with session name = %s", cookie_buf);

    if (!in_cookie_hash ||
       ((cptr->cookie_idx = in_cookie_hash(cookie_buf, cookie_buf_length-1)) == -1)) {
      NSEL_WAR(NULL, cptr, ERROR_ID, ERROR_ATTR,
			"Recieved cookie %s on URL %s."
			" Ignoring it",
			cookie_start_name, get_url_req_url(cptr));
      cptr->header_state = HDST_TEXT;
      return consumed_bytes;
    }
    vuctable[cptr->cookie_idx].cookie_value = NULL; // Should we free it if already something 
    //cptr->header_state = HDST_SET_COOKIE_VALUE;
  } else { /* did not get whole cookie name */
    cookie_end = cookie_end_name + 1;
    int cookie_length = cookie_end - cookie_start_name;
    int cookie_buf_length = cookie_length + strlen(vptr->sess_ptr->sess_name) + 2;
    char cookie_buf[cookie_buf_length];
    NSDL2_COOKIES(NULL, cptr, "cookie_buf_length = %d, size of cookie_buf = %d",
			      cookie_buf_length, sizeof(cookie_buf)); 

    consumed_bytes = cookie_length;
    memcpy(cookie_buf, cookie_start_name, cookie_length);
    cookie_buf[cookie_length] = '\0';
    NSDL3_COOKIES(NULL, cptr, "Did not get whole cookie Name. cookie name = %s,"
			      " cookie_length = %d, cookie_buf_length = %d",
			      cookie_buf, cookie_length, cookie_buf_length);

    sprintf(cookie_buf, "%s!%s", cookie_buf, vptr->sess_ptr->sess_name);
    NSDL3_COOKIES(NULL, cptr, "Cookie Name with session name = %s", cookie_buf);

    //Still we have only partial cookie name so just get hash code for the that was inputted so far
    //Save hash code of partial cookie name received so far
    NSDL3_COOKIES(NULL, cptr, "Since did not get whole cookie Name."
			      " So getting hash code for that");
    if (!in_cookie_hash_parse ||
        (cptr->cookie_hash_code = in_cookie_hash_parse(cookie_buf, cookie_buf_length - 1)) == -1) {
      NSEL_WAR(NULL, cptr, ERROR_ID, ERROR_ATTR,
			   "Recieved cookie %s on URL %s. Ignoring it",
			   cookie_start_name, get_url_req_url(cptr));
      cptr->header_state = HDST_TEXT;
      return consumed_bytes;
    }
    //cptr->header_state = HDST_SET_COOKIE_MORE_COOKIE;
  }
  return(consumed_bytes);
}

/* This method is used to extract remainder of cookie name if partial cookie name was already received from the HTTP response
   Inputs
     Arg1 - Cookie Start Name
     Arg2 - Cookie End Name
     Arg3 - Cookie length
     Arg4 - connection* 
   Return - Number of bytes
*/
inline int save_cookie_name_more(char *cookie_start_name, char *cookie_end_name, int cookie_length, connection* cptr)
{
  char* cookie_buffer;
  char* cookie_end;
  const char* existing_cookie;
  int existing_cookie_length;
  int new_cookie_length;
  char* cookie_div;
  int div_length;
  int consumed_bytes;
  char got_complete_cookie_val = 0; // No
  VUser* vptr = cptr->vptr;
  UserCookieEntry *vuctable = ((VUser*)cptr->vptr)->uctable;

  NSDL2_COOKIES(NULL, cptr, "Method called, cookie_start_name = %*.*s,"
			    " cookie_length = %d",
			    cookie_length, cookie_length, cookie_start_name,
			    cookie_length);

  if ((existing_cookie = get_key_parse(cptr->cookie_hash_code)) == NULL) 
  {
    NSEL_WAR(NULL, cptr, ERROR_ID, ERROR_ATTR,
    			 "Previous cookie hash code unknown: %d",
			 cptr->cookie_hash_code);
    cptr->header_state = HDST_TEXT;
    return 0; // No bytes consumed
  }

  NSDL3_COOKIES(NULL, cptr, "Existing cookie name = %s", existing_cookie);

  /* NOTE: If we do not find the "=" character, then we will continue reading the buffer as if it was
     a cookie name.  We do not look for a '\n'.  Theorictically, we could read the whole page and think
     it is a cookie name */

  if ((cookie_end = memchr(cookie_start_name, '=', cookie_length))) 
  {  /* we got the rest of the cookie name*/
    *cookie_end = '\0';
    NSDL3_COOKIES(NULL, cptr, "Got remaining cookie Name = %s", cookie_end);
    //cptr->header_state = HDST_SET_COOKIE_VALUE;
    new_cookie_length = cookie_end - cookie_start_name;
    got_complete_cookie_val = 1; // Yes
    vuctable[cptr->cookie_idx].cookie_value = NULL; // Should we free it if already something 
  } 
  else 
  { /* still only got partial cookie name*/
    NSDL3_COOKIES(NULL, cptr, "Still got partial cookie Name = %s", cookie_end_name);
    cookie_end = cookie_end_name; 
    //cptr->header_state = HDST_SET_COOKIE_MORE_COOKIE;
    new_cookie_length = cookie_end - cookie_start_name + 1;
  }

  consumed_bytes = (cookie_end - cookie_start_name);

  existing_cookie_length = strlen(existing_cookie);
  MY_MALLOC(cookie_buffer , existing_cookie_length + new_cookie_length + 1, "cookie_buffer ", -1);

  cookie_div = strchr(existing_cookie, '!');
  assert(cookie_div);

  div_length = cookie_div - existing_cookie;
  strncpy(cookie_buffer, existing_cookie, div_length);
  NSDL3_COOKIES(NULL, cptr, "cookie_buffer = %s, new_cookie_length =%d,"
			    " existing_cookie = %s div_length = %d,"
			    " cookie_start_name = %s",
			    cookie_buffer, new_cookie_length,
			    existing_cookie, div_length, cookie_start_name);
  strncpy(cookie_buffer + div_length , cookie_start_name, new_cookie_length);

  cookie_buffer[div_length + new_cookie_length] = '\0';
  NSDL3_COOKIES(NULL, cptr, "Cookie Name = %s", cookie_buffer);
  
  sprintf(cookie_buffer, "%s!%s", cookie_buffer, vptr->sess_ptr->sess_name);
  NSDL3_COOKIES(NULL, cptr, "Cookie Name with session name = %s", cookie_buffer);
 
  //if (cptr->header_state == HDST_SET_COOKIE_VALUE) 
  if (got_complete_cookie_val) { /* we have the whole cookie name, so get the cookie idx */
    NSDL3_COOKIES(NULL, cptr, "Got whole cookie Name, now getting cookie index");

    NSDL3_COOKIES(NULL, cptr, "cookie_buffer = %s, existing_cookie_length = %d,"
			      " new_cookie_length = %d",
			      cookie_buffer, existing_cookie_length,
			      new_cookie_length);

    if (!in_cookie_hash || 
        ((cptr->cookie_idx = in_cookie_hash(cookie_buffer, existing_cookie_length + new_cookie_length)) == -1)) {
      NSEL_WAR(NULL, cptr, ERROR_ID, ERROR_ATTR,
		     "Recieved cookie %s and ignoring it", cookie_buffer);
      cptr->header_state = HDST_TEXT;
      cptr->cookie_hash_code = -1;
      FREE_AND_MAKE_NULL(cookie_buffer, "cookie_buffer", -1);
      return consumed_bytes;
    }
    cptr->cookie_hash_code = -1;
    FREE_AND_MAKE_NULL(cookie_buffer, "cookie_buffer", -1);
  } 
  else 
  { /* we don't have whole cookie name so just get hash code for the that was inputted so far */
    NSDL3_COOKIES(NULL, cptr, "Did not get whole cookie Name. So getting hash code for that");

    NSDL3_COOKIES(NULL, cptr, "cookie_buffer = %s, existing_cookie_length = %d,"
			      " new_cookie_length = %d",
			      cookie_buffer, existing_cookie_length,
			      new_cookie_length);

    if (!in_cookie_hash_parse ||
        (cptr->cookie_hash_code = in_cookie_hash_parse(cookie_buffer, existing_cookie_length + new_cookie_length)) == -1) 
    {
      NSEL_WAR(NULL, cptr, ERROR_ID, ERROR_ATTR,
		     "Recieved cookie %s and ignoring it",
		     cookie_buffer);
      cptr->header_state = HDST_TEXT;
      FREE_AND_MAKE_NULL(cookie_buffer, "cookie_buffer", -1);
      return consumed_bytes;
    }
    FREE_AND_MAKE_NULL(cookie_buffer, "cookie_buffer", -1);
  }
  return consumed_bytes;
}

/* This method is used to extract the cookie value from the HTTP response
   Inputs
     Arg1 - Cookie Start Value
     Arg2 - Cookie End Value
     Arg3 - Cookie length
     Arg4 - connection* 
   Return - Number of bytes
*/
inline int save_cookie_value(char *cookie_start_value, char *cookie_end_value,
			     int cookie_length, connection* cptr) {
  char* value_end;
  int value_length;
  UserCookieEntry *vuctable = ((VUser*)cptr->vptr)->uctable;
  int cookie_idx = cptr->cookie_idx;
  char** cookie_value;
  char* end_of_line;
  char* semicolon_end;
  int consumed_bytes;

  NSDL2_COOKIES(NULL, cptr, "Method called, cookie_start_value = %*.*s, cookie_length = %d", cookie_length, cookie_length, cookie_start_value, cookie_length);

  if (cookie_idx == -1) 
  {  /* This really should never happen */
    NSEL_WAR(NULL, cptr, ERROR_ID, ERROR_ATTR,
	           "Error, got a cookie_idx of -1 in state HDST_SET_COOKIE_VALUE");
    cptr->header_state = HDST_TEXT;
    return 0;  // No bytes consumed
  }

  cookie_value = &vuctable[cookie_idx].cookie_value;

  if (vuctable[cookie_idx].cookie_value)  { /* overwrite old value */
    NSDL3_COOKIES(NULL, cptr, "Overwrite old value");
    FREE_AND_MAKE_NULL(vuctable[cookie_idx].cookie_value, "vuctable[cookie_idx].cookie_value", -1);
    vuctable[cookie_idx].cookie_value = NULL;
  }

  end_of_line = memchr(cookie_start_value, '\r', cookie_length);
  semicolon_end = memchr(cookie_start_value, ';', cookie_length);

  if (end_of_line && semicolon_end) 
  {
    if (end_of_line < semicolon_end)
      value_end = end_of_line;
    else
      value_end = semicolon_end;
  } 
  else 
  {
    if (end_of_line && !semicolon_end)
      value_end = end_of_line;
    else 
    {
      if (!end_of_line && semicolon_end)
        value_end = semicolon_end;
      else   /* if both are null */
        value_end = NULL;
    }
  }

  if (value_end) 
  {  /* we got the whole cookie value */
    NSDL3_COOKIES(NULL, cptr, "Got whole cookie value. Buffer after cookie value = %s", value_end);
    value_length = value_end - cookie_start_value;
    if (!value_length)
      NSDL3_COOKIES(NULL, cptr, "Got a NULL value for cookie, cookie_idx = %d, cookie value = %s", cptr->cookie_idx, value_end);

    MY_MALLOC(*cookie_value , value_length + 1, "*cookie_value ", -1);
    strncpy(*cookie_value, cookie_start_value, value_length);
    (*cookie_value)[value_length] = '\0';
    vuctable[cookie_idx].length = value_length;
    NSDL3_COOKIES(NULL, cptr, "Cookie set in vuser table, cookie_value = %s, length = %d", *cookie_value, value_length);

    consumed_bytes = value_length;
    cptr->header_state = HDST_TEXT;
  }
  else 
  {
    value_end = cookie_end_value; 
    NSDL3_COOKIES(NULL, cptr, "Did not get whole cookie value. Partial cookie value = %s", value_end);

    value_length = value_end - cookie_start_value + 1;
    MY_MALLOC(*cookie_value , value_length + 1, "*cookie_value ", -1);
    strncpy(*cookie_value, cookie_start_value, value_length);
    (*cookie_value)[value_length] = '\0';
    NSDL3_COOKIES(NULL, cptr, "Cookie set in vuser table, cookie_value = %s, length = %d", *cookie_value, value_length);

    consumed_bytes = (value_length-1);
    //cptr->header_state = HDST_SET_COOKIE_MORE_VALUE;
  }
  return consumed_bytes;
}

/* This method is used to extract remainder of cookie value if partial cookie value was already received from the HTTP response
   Inputs
     Arg1 - Cookie Start Value
     Arg2 - Cookie End Value
     Arg3 - Cookie length
     Arg4 - connection* 
   Return - Number of bytes
*/
inline int save_cookie_value_more(char *cookie_start_value, char *cookie_end_value,
				  int cookie_length, connection* cptr) {
  char* value_end;
  int new_value_length;
  int existing_value_length;
  UserCookieEntry *vuctable = ((VUser*)cptr->vptr)->uctable;
  int cookie_idx = cptr->cookie_idx;
  char** cookie_value;
  char* end_of_line = NULL;
  char* semicolon_end = NULL;
  int consumed_bytes;

  NSDL2_COOKIES(NULL, cptr, "Method called, cookie_start_value = %*.*s,"
			    " cookie_length = %d",
			    cookie_length, cookie_length,
			    cookie_start_value, cookie_length);

  if (cookie_idx == -1) 
  {  /* This really should never happen */
    NSEL_WAR(NULL, cptr, ERROR_ID, ERROR_ATTR,
	           "Error, got a cookie_idx of -1 in state HDST_SET_COOKIE_VALUE");
    cptr->header_state = HDST_TEXT;
    return 0;  // No bytes consumed
  }

  cookie_value = &vuctable[cookie_idx].cookie_value;
  existing_value_length = strlen(*cookie_value);

  NSDL3_COOKIES(NULL, cptr, "Partial cookie value = %s, length = %d", *cookie_value, existing_value_length);

  if (end_of_line && semicolon_end) 
  {
    if (end_of_line < semicolon_end)
      value_end = end_of_line;
    else
      value_end = semicolon_end;
  } 
  else 
  {
    if (end_of_line && !semicolon_end)
      value_end = end_of_line;
    else 
    {
      if (!end_of_line && semicolon_end)
        value_end = semicolon_end;
      else   /* if both are null */
        value_end = NULL;
    }
  }

  if ((value_end = memchr(cookie_start_value, ';', cookie_length))) 
  {  /* we got the rest of the value */
    NSDL3_COOKIES(NULL, cptr, "Got remaining cookie value. cookie value = %s", value_end);

    value_end[0] = '\0';
    new_value_length = value_end - cookie_start_value;
    MY_REALLOC(*cookie_value, new_value_length + existing_value_length + 1, 
	    "*cookie_value ", -1);

    strncpy(*cookie_value + existing_value_length, cookie_start_value, new_value_length);
    (*cookie_value)[new_value_length + existing_value_length] = '\0';
    vuctable[cookie_idx].length = new_value_length + existing_value_length;

    NSDL3_COOKIES(NULL, cptr, "Cookie value = %s, length = %d", *cookie_value, vuctable[cookie_idx].length);

    consumed_bytes = new_value_length;
    cptr->header_state = HDST_TEXT;
  } 
  else 
  { /* still only got partial value */
    NSDL3_COOKIES(NULL, cptr, "Still got partial value. cookie value = %s", value_end);

    value_end = cookie_end_value;
    new_value_length = value_end - cookie_start_value + 1;
    MY_REALLOC(*cookie_value, new_value_length + existing_value_length + 1, 
	       "*cookie_value", -1);

    strncpy(*cookie_value + existing_value_length, cookie_start_value, new_value_length);
    (*cookie_value)[new_value_length + existing_value_length] = '\0';
    consumed_bytes = value_end - cookie_start_value;
    /* cptr->header_start is already at HDST_SET_COOKIE_MORE_VALUE */
    NSDL3_COOKIES(NULL, cptr, "Cookie value = %s, length = %d", *cookie_value, new_value_length);
  }
  return consumed_bytes;
}

/*
o  Following variables in cptr are used for reading of Set-Cookie header value
   - cookie_hash_code is the hash code of partial cookie name
   - cookie_idx is the hash code of complete Cookie name
   - vuctable[cookie_idx].cookie_value is used to keep the cookie value
     (Partial and complete)

o  Cookie name as read and if paritial, we get the hash code of partial cookie
   name using in_cookie_hash_parse() and store    it in cptr->cookie_hash_code.

o  Once we get complte cookie name, we calculate hash code using 
   in_cookie_hash() and store in cptr->cookie_idx.

o  Following table show variables values in reading of cookie name and value:

cookie_hash_code cookie_idx  vuctable[].cookie_value
-1               -1          NA  
                             o Called first time. Set cookie_hash_code if
                               paritial cookie name else set cookie_idx.
			       vuctable[].cookie_value = NULL;

>= 0             -1          NA
		             o Called second time to read cookie name. Set
			       cookie_idx if complete name received and
			       free cookie_value and make NULL

NA               >= 0        NULL
			     o Called first time to read cookie value.
			       Save in cookie_value

NA               >= 0        != NULL
			     o Called Second time to read cookie value

Header format:
  Set-Cookie: <name>=<value>[; <name>=<value>]... [; expires=<date>][;
	      domain=<domain_name>] [; path=<some_path>][; secure][; httponly]
	      (line will end using \r\n)

**Examples:
    Set-Cookie: emp_name=newvalue; expires=date; path=/; domain=.example.org.
		(line will end using \r\n)

    Set-Cookie: emp_name=newvalue; emp_address=jss; expires=date; path=/;
		domain=.example.org.(line will end using \r\n)

    Set-Cookie: emp_name=newvalue; (line will end using \r\n)

    Set-Cookie: emp_name=newvalue (line will end using \r\n)

Limitations:

1. Is current code handling multiple cookies set in one Set-Cookie header
2. Expired/Domain/Path etc are not handled in case of manual cookie and are ignored

This method is used to extract the cookie name from the HTTP response
   Inputs
     Arg1 - Cookie Start Name
     Arg2 - Cookie End Name
     Arg3 - Cookie length
     Arg4 - connection* 
   Return - Number of bytes
*/
inline int save_manual_cookie(char *cookie_start_name, char *cookie_end_name,
			      int cookie_len, connection* cptr) {
  //char* cookie_end;
  int consumed_bytes;
  UserCookieEntry *vuctable = ((VUser*)cptr->vptr)->uctable;
  
  NSDL2_COOKIES(NULL, cptr, "Method called, cookie_start_name = %*.*s,"
			    " cookie_length = %d",
			    cookie_len, cookie_len,
			    cookie_start_name, cookie_len);

  /* Cookie is DISABLED*/
  if(global_settings->cookies == COOKIES_DISABLED) {
     NSDL2_COOKIES(NULL, cptr, "Cookie is DISABLED");
     cptr->header_state = HDST_TEXT;
     return 0;
  }

  if(cptr->cookie_idx < 0)  {  // We are coming first time or we are in partial cookie name state
     if(cptr->cookie_hash_code < 0) {  // We are in coming here first time 
       consumed_bytes = save_cookie_name(cookie_start_name, cookie_end_name, cookie_len, cptr);
     } else {  // We are in partial cookie name state 
       consumed_bytes = save_cookie_name_more(cookie_start_name, cookie_end_name, cookie_len, cptr);
     }
  }  else {  // We have savad the cookie name has index in cookie_idx; ie we have parsed cookie name 
     if(vuctable[cptr->cookie_idx].cookie_value == NULL) {  // We are coming to save cookie value first time 
       consumed_bytes = save_cookie_value(cookie_start_name, cookie_end_name, cookie_len, cptr);
     } else {  // We are in partial cookie name state 
       consumed_bytes = save_cookie_value_more(cookie_start_name, cookie_end_name, cookie_len, cptr);
     }
  }
 
  return consumed_bytes;
}

/* --- END: Method used during Run Time  ---*/

/* --- START: Method used for Cookie API ---*/

/*
**Purpose: To get cookie table index
**Arguments:
  Arg1 - cname - Cookie name
  Arg2 - sname - Session name
**Return:
  This method returns uctable index
*/
int ns_get_cookie_idx_non_auto_mode(char *cname, char *sname)
{
  char cookie_buf[MAX_COOKIE_LENGTH];
  int uctable_idx;
  int cookie_buf_length;

  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie name = %s, Session Name = %s", cname, sname);
  cookie_buf_length = strlen(cname) + strlen(sname) + 2;
  if (cookie_buf_length > MAX_COOKIE_LENGTH) 
  {
    fprintf(stderr, "ns_get_cookie_idx_non_auto_mode(): Error - %s!%s is more than 8K\n", cname, sname);
    return -2;
  }
  sprintf(cookie_buf, "%s!%s", cname, sname);

  if (!in_cookie_hash || ((uctable_idx = in_cookie_hash(cookie_buf, cookie_buf_length - 1)) == -1))
  {
    NSDL2_COOKIES(NULL, NULL, "ns_get_cookie_idx_non_auto_mode(): Warning - Cookie not in the hash table. Cookie  = %s", cookie_buf);
    return -1;
  }

  NSDL3_COOKIES(NULL, NULL, "cookie_buf = %s, uctable_idx = %d", cookie_buf, uctable_idx);

  return uctable_idx;
}

//This method returns Pointer to cookie value populated in a static buffer or NULL if cookie index is less than 0 or greater than max cookie hash code.
char *ns_get_cookie_val_non_auto_mode(int cookie_idx, VUser *my_vptr)
{
  UserCookieEntry* cookie_val_ptr;
  char *ptr;
  int len;

  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Index = %d", cookie_idx);
  if ((cookie_idx < 0) || (cookie_idx >= max_cookie_hash_code)) 
  {
    fprintf(stderr, "ns_get_cookie_val_non_auto_mode(): Invalid cookie index %d\n", cookie_idx);
    return NULL;
  }

  cookie_val_ptr = &my_vptr->uctable[cookie_idx];
  ptr = cookie_val_ptr->cookie_value;
  len = cookie_val_ptr->length;
  if (len >= MAX_COOKIE_LENGTH) 
  {
    fprintf(stderr, "ns_get_cookie_val_non_auto_mode(): Cookie value len %d more than 8K. Truncating to 8K\n", len);
    len = MAX_COOKIE_LENGTH - 1;
  }

  if ((ptr == NULL) || (len == 0))
    len = 0;
  else
    bcopy (ptr, ms_cookie_buf, len);

  ms_cookie_buf[len]= '\0';

  NSDL3_COOKIES(NULL, NULL, "Cookie value = %s", ms_cookie_buf);

  return (ms_cookie_buf);
}

//This method returns Pointer to 0 is successful else -1 if cookie index is less than 0 or greater than max cookie hash code. 
int ns_set_cookie_val_non_auto_mode(int cookie_idx, char *cookie_val, VUser *my_vptr)
{
  UserCookieEntry* cookie_val_ptr;
  char *ptr;

  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Index = %d, Cookie Value = %s", cookie_idx, cookie_val);

  if ((cookie_idx < 0) || (cookie_idx >= max_cookie_hash_code)) 
  {
    fprintf (stderr, "ns_set_cookie_val_non_auto_mode(): Invalid cookie index %d\n", cookie_idx);
    return -1;
  }
  if(cookie_val == NULL)
  {
    fprintf (stderr, "ns_set_cookie_val_non_auto_mode(): Invalid cookie name %s\n", cookie_val);
    return -1;
  }

  int len = strlen(cookie_val);
  
  cookie_val_ptr = &my_vptr->uctable[cookie_idx];
  ptr = cookie_val_ptr->cookie_value;

  FREE_AND_MAKE_NOT_NULL(ptr, "Overwrite old Cookie Value ", -1);

  MY_MALLOC(ptr , len + 1, "Cookie Value ", -1);

  strcpy (ptr, cookie_val);
  cookie_val_ptr->cookie_value = ptr;
  cookie_val_ptr->length = len;

  return (0);
}
/* --- END: Method used for Cookie API ---*/

