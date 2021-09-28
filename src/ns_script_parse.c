#include "ns_script_parse.h"
#include "ns_click_script_parse.h"
#include "tr069/src/ns_tr069_script_parse.h"
#include "ns_ftp_parse.h"
#include "ns_smtp_parse.h"
#include "ns_dns_parse.h"
#include "ns_pop3_parse.h"
#include "wait_forever.h"
#include "ns_schedule_phases_parse.h"
#include "ns_parent.h"
#include "ns_socket_api_int.h"      //for prototype of parse_ns_socket_api
#include "ns_sync_point.h"
#include "url.h"
#include "ns_njvm.h"
#include "ns_ldap.h"
#include "ns_imap.h"
# include "ns_jrmi.h"
#include "ns_websocket.h"
#include "nslb_util.h"
#include "ns_trace_level.h"
#include "ns_sockjs.h"
#include "ns_fc2.h"
#include "ns_static_vars.h"
#include "ns_error_msg.h"
#include "ns_runtime.h"
#include "ns_exit.h"
#include "ns_socket.h"
#include "ns_rdp_api.h" /*bug 79149*/
#include "util.h"
char indent[2 + 1] = "  ";

#ifndef CAV_MAIN
char *script_name = NULL;
char *flow_filename = NULL;
char *script_line = NULL;
static char *g_script_line = NULL;
int  script_ln_no;
char parse_tti_file_done; //This flag will tells whether tti file has to parse or not?
int page_num_relative_to_flow;
#else
__thread char *script_name = NULL;
__thread char *flow_filename = NULL;
__thread char *script_line = NULL;
static __thread char *g_script_line = NULL;
__thread int  script_ln_no;
__thread char parse_tti_file_done; //This flag will tells whether tti file has to parse or not?
__thread int page_num_relative_to_flow;
#endif

char  g_dont_skip_test_metrics;// This flag tells whether to skip test metric or not
//int first_flow_page;

#define NS_INIT_SCRIPT_FILENAME "init_script.c"
#define NS_EXIT_SCRIPT_FILENAME "exit_script.c"

#define NS_INIT_SCRIPT_FILENAME_JAVA "init_script.java"
#define NS_EXIT_SCRIPT_FILENAME_JAVA "exit_script.java"

extern pthread_t script_val_thid;
extern pthread_attr_t script_val_attr;

/************START bug 79149 ***********************************/
#define CHECK_AND_SET_RDP_FLAG() \
{ \
 if(rdp_enabled)\
   SCRIPT_PARSE_ERROR(script_line,"Error!!!ns_rdp_connect() called more than once"); \
 rdp_enabled = 1;\
}
#define CHECK_RDP_ENABLED() \
{ \
  if(!rdp_enabled)\
   SCRIPT_PARSE_ERROR(script_line,"Error!!! call ns_rdp_connect() before any ns_key or ns_mouse api"); \
}
/************END bug 79149 ***********************************/
inline int write_line(FILE *fp, char *script_line, char new_line)
{
  NSDL2_PARSING(NULL, NULL, "Method Called");
  if(fwrite(script_line, 1, strlen(script_line), fp) == 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012247_ID, CAV_ERR_1012247_MSG, errno, nslb_strerror(errno));

  if(new_line)
  {
    if(fwrite("\n", 1, 1, fp) == 0)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012247_ID, CAV_ERR_1012247_MSG, errno, nslb_strerror(errno));
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}


//#define NS_SOCKET_API "ns_socket_api.so"
/*
One line can have one or more arguments.
One Argurment should be in single script_line except body
E.g. "METHOD = xxx",
     "VERSION =
      1.0"   ->>> Not correct
      INLINE_URLS
*/

char *
read_line_and_comment(FILE *flow_fp, FILE *outfp)
{

   script_line = read_line(flow_fp, NS_DO_NOT_TRIM);
   write_line(outfp,"//",0);
   write_line(outfp,script_line,0);
   return script_line;
}

char *
read_line (FILE *flow_fp, short trim_line)
{
  char *ptr;

  NSDL2_PARSING(NULL, NULL, "Method Called. trim_line = %d", trim_line);
  NSDL2_PARSING(NULL, NULL, "script_line_length [%s] ,Max_LINE_LENGTH is [%d] ",script_line ,MAX_LINE_LENGTH);
  
  script_line = g_script_line;  //Point to script_line_buf
  ptr = nslb_fgets (script_line, MAX_LINE_LENGTH, flow_fp, 0);
  if (ptr == NULL)
  { 
    NSDL4_PARSING(NULL, NULL, "End of file reached");
    return NULL;
  }

  script_ln_no++;

  NSDL4_PARSING(NULL, NULL, "Line (line_num = %d) = %s", script_ln_no, script_line);

  // Replace new line at the end of line by NULL termination
  //len = strlen (script_line); // Length include \n if present
  return script_line;
}

/**********************************************************************
* log_script_parse_error function logs script parsing error.
* Arguments:
*   do_exit      - flag to know whether to exit or not.
*   err_msg_code - Error message code that is defined in ns_error_msg.h
                   Each error should have a error message code.
*   file         - Source file name from where this function is called.
*   line         - Line number of source code
*   fname        - Name of function from where it is called.
*   line_buf     - script line
***********************************************************************/
void log_script_parse_error(int do_exit, char *err_msg_code, char *file, int line, char *fname, char *line_buf, char *format, ...)
{
  #define MAX_SCRIPT_PARSE_LOG_BUF_SIZE 64000
  va_list ap;
  int amt_written = 0, amt_written1=0;
  char buffer[MAX_SCRIPT_PARSE_LOG_BUF_SIZE + 1];
  char *tmp_script_name;

  if(strcmp(flow_filename, "NA")) // Parsing error is for a flow file
  {
    amt_written1 = sprintf(buffer, "%sScript Parsing Error (Script = %s, file = %s, line_num = %d), ",
                                    err_msg_code?err_msg_code:"", script_name, flow_filename, script_ln_no);
  } 
  else if(script_name[0] != '\0')// Not for flow. So do not show flow name and line number
  { 
    //print script name with project/sub_proj only (remove ./script from 'script_name')   
    tmp_script_name = strchr((script_name + 2), '/');
    tmp_script_name++; //skipping character /
    amt_written1 = sprintf(buffer, "%sScript Parsing Error (Script = %s), ", err_msg_code?err_msg_code:"", tmp_script_name);
  }
  else
  {
    amt_written1 = sprintf(buffer, "%s", err_msg_code?err_msg_code:"");
  }
  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_SCRIPT_PARSE_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  buffer[MAX_SCRIPT_PARSE_LOG_BUF_SIZE] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  // This is a quick fix to handle this. need to find the root cause
  if(amt_written < 0)
  {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_SCRIPT_PARSE_LOG_BUF_SIZE - amt_written1))
  {
    amt_written = (MAX_SCRIPT_PARSE_LOG_BUF_SIZE - amt_written1);
  }

  if(line_buf != NULL) // Show parsing error with line
  {
    snprintf(buffer + amt_written + amt_written1, MAX_SCRIPT_PARSE_LOG_BUF_SIZE - (amt_written + amt_written1), "\nLine = %s", line_buf);
  }

  if(do_exit)
  {
    NS_EXIT(-1, buffer);
  }
  else
  {
    if(rtcdata != NULL)
      strcpy(rtcdata->err_msg, buffer);
    //end stage here before overwriting buffer
    end_stage(NS_SCENARIO_PARSING, TIS_ERROR, "%s", buffer);
  }

  // Show netstorm source file, function and line number in the format:
  // (Source File = ns_script_parse.c, Function = get_flow_filelist(), Line = 615)
  sprintf(buffer, "(Source File = %s, Function = %s(), Line = %d)", file, fname, line);
  NSTL1(NULL, NULL,  "%s", buffer);
}

int extract_attribute_value(char *value_start_ptr, char **closing_quotes, char *attribute_name, char *attribute_value)
{
  char *ptr = NULL;
  int len;
  char *start_ptr = value_start_ptr;

  NSDL2_PARSING(NULL, NULL, "Method Called. value_start_ptr =%s, attribute_name = %s", value_start_ptr, attribute_name);
  if(value_start_ptr == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012328_ID, CAV_ERR_1012328_MSG, attribute_name);
  ptr = value_start_ptr;
  attribute_value[0] = '\0';

  while(*ptr != '\0')
  {
    if(*ptr == '"')
    {
      //If \" found, add " in value till quotes removing the backslash
      if(*(ptr - 1) == '\\')
      {
        len = ptr - start_ptr - 1;  //1 for not copying backslash 
        strncat(attribute_value, start_ptr, len);  
        //attribute_value[len] = 34;
        strcat(attribute_value, "\"");
        start_ptr = ptr + 1;
      }
      //If only " found, point the closing_quotes to that quotes
      else
      {
        strncat(attribute_value, start_ptr, ptr - start_ptr);
        *closing_quotes = ptr;
        break;
      }
    }
    else if(*ptr == '\n')
    { 
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012329_ID, CAV_ERR_1012329_MSG, attribute_name);
    }
    ptr++;
  }

  //Checking if attribute value not received
  len = strlen(attribute_value);
  if(len <= 0 && (strcmp(attribute_name,"SUBJECT") && strcmp(attribute_name,"BODY")))
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012330_ID, CAV_ERR_1012330_MSG, attribute_name);

  CLEAR_WHITE_SPACE(attribute_value);
  CLEAR_WHITE_SPACE_FROM_END(attribute_value);

  if(strlen(attribute_value) <= 0 && (strcmp(attribute_name,"SUBJECT") && strcmp(attribute_name,"BODY")))
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012330_ID, CAV_ERR_1012330_MSG, attribute_name);

  NSDL2_PARSING(NULL, NULL, "Exiting Method. Extracted argument value = [%s]", attribute_value);
  return NS_PARSE_SCRIPT_SUCCESS;
}


/*************************************************************************************************
Description       : This method fetches the argument names and argument values from quotes

Input Parameters  : 
           flow_fp: pointer to user table
  starting_quotes : Pointer to starting quotes of arguments

Output Parameters : 
   closing_quotes : closing_quotes will return pointer to the ending quotes after argument

Return Value      : NS_PARSE_SCRIPT_SUCCESS in case function executed successfully
                    NS_PARSE_SCRIPT_ERROR in case any failure occurs in the function
*************************************************************************************************/

int get_next_argument(FILE *flow_fp, char *starting_quotes, char *attribute_name,
                               char *attribute_value, char **closing_quotes, char read_body)
{
  char *ptr = NULL;
  char *value_start_ptr = NULL;
  int len;

  NSDL2_PARSING(NULL, NULL, "Method Called. starting_quotes=%s", starting_quotes);
  
  //if the tag is ITEMDATA, then a value will not follow it, as it is just start of the block
  if(!strncasecmp(starting_quotes, "ITEMDATA", 8) || !strncasecmp(starting_quotes, "ATTR_LIST_BEGIN", 8))
  {
    //ITEMDATA must be followed by a comma
    if((ptr = strchr(starting_quotes, ',')))
    {
      len = ptr - starting_quotes;
      strncpy(attribute_name, starting_quotes, len);
      attribute_name[len] = '\0';

      //clear any white space between ITEMDATA and comma
      CLEAR_WHITE_SPACE_FROM_END(attribute_name);

      //as ptr is pointing to comma, decrease it by 1 to actually pointing to last character before comma
      *closing_quotes = ptr - 1;
      return NS_PARSE_SCRIPT_SUCCESS;
    }
    else
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012327_ID, CAV_ERR_1012327_MSG, starting_quotes);
  }
  // Multipart body start
  if (!strncmp(starting_quotes, "MULTIPART_BODY_BEGIN", strlen("MULTIPART_BODY_BEGIN")) ||
      !strncmp(starting_quotes, "MULTIPART_BODY_END", strlen("MULTIPART_BODY_END")) ||
      !strncmp(starting_quotes, "MULTIPART_BOUNDARY", strlen("MULTIPART_BOUNDARY")) ||
      !strncmp(starting_quotes, "BODY_BEGIN", strlen("BODY_BEGIN")) ||
      !strncmp(starting_quotes, "BODY_END", strlen("BODY_END")) ||
      !strncmp(starting_quotes, "INLINE_URLS", strlen("INLINE_URLS"))) {
    if ( (ptr = strchr(starting_quotes, ',')) == NULL) {
      if (!strncmp(starting_quotes, "MULTIPART_BODY_END", strlen("MULTIPART_BODY_END"))) {
        strcpy(attribute_name, "MULTIPART_BODY_END");
        *closing_quotes = starting_quotes + strlen("MULTIPART_BODY_END")-1;
        return NS_PARSE_SCRIPT_SUCCESS;
      }
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012327_ID, CAV_ERR_1012327_MSG, starting_quotes);
    }
    len = ptr - starting_quotes;
    strncpy(attribute_name, starting_quotes, len);
    attribute_name[len] = '\0';
    //set the closing quotes to 1 byte before the comma, so that we can find the comma soon after this
    *closing_quotes = ptr-1;
    NSDL2_PARSING(NULL, NULL, "returning attribute_name %s closing_quotes %p (%s)", attribute_name, *closing_quotes, *closing_quotes);
    return NS_PARSE_SCRIPT_SUCCESS;
  }
  // Multipart body change end

  if(starting_quotes == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012332_ID, CAV_ERR_1012332_MSG);

  if(*starting_quotes != '"')
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012333_ID, CAV_ERR_1012333_MSG);

  //For skipping " to be copied
  starting_quotes++;

  //Checking for = after attribute name
  ptr = strchr(starting_quotes, '=');
  if(!ptr)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012334_ID, CAV_ERR_1012334_MSG);

  len = ptr - starting_quotes;
  if(len <= 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012335_ID, CAV_ERR_1012335_MSG);

  strncpy(attribute_name, starting_quotes, len);
  attribute_name[len] = '\0';
  NSDL2_PARSING(NULL, NULL, "Parsing argument Name %s script_line %d in script [%s]",
                                                       attribute_name, script_ln_no, flow_filename);
  CLEAR_WHITE_SPACE(attribute_name);
  CLEAR_WHITE_SPACE_FROM_END(attribute_name);

  if(strlen(attribute_name) <= 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012335_ID, CAV_ERR_1012335_MSG);

  //As body can be on multiple lines, body would be extracted using a separate method extract_body()
  //if(strncmp(attribute_name, "BODY", 4) == 0)
  if(strcmp(attribute_name, "BODY") == 0)
  {
    NSDL2_PARSING(NULL, NULL, "Argument is body");
    if(!read_body)
      return NS_PARSE_SCRIPT_SUCCESS;
  }

  //As value would be starting after =, ptr is pointing to =
  value_start_ptr = ++ptr;

  if(extract_attribute_value(value_start_ptr, closing_quotes, attribute_name, attribute_value) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL2_PARSING(NULL, NULL, "Exiting Method. argument Value [%s] extracted for argument Name=[%s]",
                                        attribute_value, attribute_name);
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*************************************************************************************************
Description       : This method parse the characters present after closing quotes of one parameter 
                    and parses till the starting of next quotes and
                    checks for comma as a seperator between the parameters
                    Also handles comments between the parameters

Input Parameters  :
           flow_fp: pointer to user table
         flow_file: flow file path & name
   closing_quotes : Pointer to closing quotes of parameter
    embedded_urls : 1 if it is an embedded url 0 otherwise

Output Parameters : 
   starting_quotes: starting_quotes will return NULL if
                    a. quotes not found and some printable character found or
                    b. quotes found but comma not found between the quotes
                    starting quotes will return pointer to start of next character  when
                    a. quotes not found and some printable character found (needed for INLINE_URLS and ");")

Return Value      : NS_PARSE_SCRIPT_SUCCESS in case function executed successfully
                    NS_PARSE_SCRIPT_ERROR in case any failure occurs in the function

Example           : ns_web_url("Page1", "URL=abc", ..)
                    After page1 is fetched, this method will be called with closing_quotes point to " after Page1
*************************************************************************************************/

int read_till_start_of_next_quotes(FILE *flow_fp, char *flow_file, char *closing_quotes, 
                                        char **starting_quotes, char embedded_urls, FILE *outfp)
{
  char *ptr;
  char comma_found = 0;
  char multiline_comment = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called closing_quotes=%p closing_quotes=[%s] , starting_quotes = [%s]", closing_quotes, closing_quotes, *starting_quotes);

  //if end of ITEMDATA block is indicated by ITEMDATA_END = 12 chars
  if(!strncasecmp(closing_quotes, "ITEMDATA_END", 12)) ptr = closing_quotes + 12;

  //if end of ITEMDATA block is indicated by ITEMDATA_END, = 13 chars
  else if(!strncasecmp(closing_quotes, "ITEMDATA_END,", 13)) ptr = closing_quotes + 13;

  //for all other cases, only 1 char forward
  else ptr = closing_quotes + 1;

  NSDL2_PARSING(NULL, NULL, "read_till_start_of_next_quotes: ptr=%p (%s) \n", ptr, ptr);
  
  while (1)
  {
    if(*ptr == '\0') // End of line reached, so read next line
    {
      if(read_line_and_comment(flow_fp, outfp) == NULL)
      {
        *starting_quotes = NULL;
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012148_ID, CAV_ERR_1012148_MSG);
      }
      //Set ptr to the start of the new script_line when new line is read
      ptr = script_line;
      continue;
    }
    
    if(multiline_comment)
    {
      if(strncmp(ptr, "*/", 2) == 0)
      {
        NSDL3_PARSING(NULL, NULL, "End of multi-line comment");
        ptr = ptr + 2;
        multiline_comment = 0;
        continue;
      }
      ptr++;
    }
    else
    {
      //Handling starting of multi-line comments
      if(strncmp(ptr, "/*", 2) == 0)
      {
        NSDL3_PARSING(NULL, NULL, "Start of multi-line comment");
        ptr = ptr + 2;
        multiline_comment = 1;
        continue;
      }
      //Handling of single-line comments
      else if(strncmp(ptr, "//", 2) == 0)
      {
        NSDL3_PARSING(NULL, NULL, "single-line comment found");
        *ptr = '\0';
        continue;
      }
      //Ignore the white spaces and continue reading script_line
      if(isspace(*ptr))
      {
        ptr++;
        continue;
      }
      else if(*ptr == ',')  //Comma
      {
        NSDL2_PARSING(NULL, NULL, "Comma found at line %d in file %s",script_ln_no, flow_file);
        comma_found = 1;
        ptr++;
        continue;
      }
      else if(*ptr == '"') // Opening quotes
        break;

      // Multipart change start
      else if (!strncmp(ptr, "MULTIPART_BODY_BEGIN", strlen("MULTIPART_BODY_BEGIN")) ||
                 !strncmp(ptr, "MULTIPART_BODY_END", strlen("MULTIPART_BODY_END")) ||
                 !strncmp(ptr, "BODY_BEGIN", strlen("BODY_BEGIN")) ||
                 !strncmp(ptr, "MULTIPART_BOUNDARY", strlen("MULTIPART_BOUNDARY"))) {
        *starting_quotes = ptr;
        return NS_PARSE_SCRIPT_SUCCESS;
      }
      // Multipart change end
      else if(!strncasecmp(ptr, "ITEMDATA", 8) || !strncasecmp(ptr, "ITEMDATA_END", 12))
      {
        *starting_quotes = ptr;
        return NS_PARSE_SCRIPT_SUCCESS;
      }else if(!strncasecmp(ptr, "ATTR_LIST_BEGIN", 15) || !strncasecmp(ptr, "ATTR_LIST_END", 13))
      {        
        *starting_quotes = ptr;
        return NS_PARSE_SCRIPT_SUCCESS;
      }
      
      //If anything else received except comma & white space between the quotes,
      //e.g 1. ns_web_url("HomePage", "METHOD=GET", URL=
      //2. ns_web_url("HomePage", "METHOD=GET",
      //INLINE_URLS,
      else
      {
        *starting_quotes = ptr;
        NSDL2_PARSING(NULL, NULL, "Next Argument Not Found at Line [%d] Script File=[%s] starting_quotes=[%p],\
                     starting_quotes=[%s]", script_ln_no, flow_file, *starting_quotes, *starting_quotes);
        return NS_PARSE_SCRIPT_ERROR;
        //SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012243_ID, CAV_ERR_1012243_MSG);
      }
    }
  }

  if(comma_found)
  {
    *starting_quotes = ptr; // Point to starting quotes
    NSDL2_PARSING(NULL, NULL, "Moving to reading next argument of ns_web_url at Line [%d] Script File=[%s] starting_quotes=%p starting_quotes=%s",
                               script_ln_no, flow_file, *starting_quotes, *starting_quotes);
    return NS_PARSE_SCRIPT_SUCCESS;
  }
  //Double quotes found, comma not found
  //e.g 1. ns_web_url("HomePage", "METHOD=GET" "URL=
  else
  {
    *starting_quotes = NULL;
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012510_ID, CAV_ERR_1012510_MSG);
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*************************************************************************************************
Description       : extract_pagename() fetches the pagename after from quotes 

Input Parameters  : 
           flow_fp: pointer to flow file
         flow_file: flow file path & name
         line_ptr : pointer to the start of the line in which api name is found

Output Parameters : 
         pagename : first agrument after api name
                    e.g. ns_web_url("HomePage", "URL=fdsffa" ....) will return HomePage in pagename
     page_end_ptr : pointer to the ending quotes of pagenae

Return Value      : NS_PARSE_SCRIPT_SUCCESS in case function executed successfully
                    NS_PARSE_SCRIPT_ERROR in case any failure occurs in the function
*************************************************************************************************/
int extract_pagename(FILE *flow_fp, char *flow_file, char *line_ptr, char *pagename, char **page_end_ptr)
{
  char *start_idx = NULL;
  char *end_idx = NULL;
  NSDL2_PARSING(NULL, NULL, "Method Called");

    //Checking for Starting quotes before pagename
    /* Removing do-while loop, because we are assuming that page name is found at the 1st line of API (.e.g. ns_web_url("pagename")). 
       In case page name is not given with doublequotes it will search for doublequotes in whole flow file hence skiping processing of                lines till double quotes */

    if((start_idx = strchr(line_ptr, '"')) == NULL)
       SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, "Pagename");

    ++start_idx;  //Adding 1 for "

    //Checking for Ending quotes after pagename
    end_idx = strchr(start_idx, '"');
    if(end_idx == NULL)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012190_ID, CAV_ERR_1012190_MSG);

    //Checking if Pagename not received, e.g ""
    int len = end_idx - start_idx;
    if(len <= 0)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, "Pagename");

    strncpy(pagename, start_idx, len);
    pagename[len] = '\0';

    //Shibani: Resolve bug 17826 - Tranaction stats not open in dashbord if page name used as tranaction and no suffix added to it.
    //CLEAR_WHITE_SPACE(pagename);
    //CLEAR_WHITE_SPACE_FROM_END(pagename);
    nslb_clear_white_space_from_string(pagename, ALL);

    //Checking if Pagename not received e.g. "  "
    if(strlen(pagename) <= 0)
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, "Pagename");

#if 0

    // Check for comma
    end_idx = strchr(end_idx, ',');
    // TODO - check if only white spaces are between quote and ,
    *page_end_ptr = end_idx + 1; // Point to the closing quotes
#endif
 
    *page_end_ptr = end_idx; // Point to the closing quotes
     NSDL2_PARSING(NULL, NULL, "Pagename '%s' received at script_line %d in file %s page_end_ptr = %p page_end_ptr=[%s]",
                                  pagename, script_ln_no, flow_file, *page_end_ptr, *page_end_ptr);
     return NS_PARSE_SCRIPT_SUCCESS;

    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, "Pagename");
  return NS_PARSE_SCRIPT_ERROR;
}

/*************************************************************************************************
Description       : check_and_write_comments() checks the existance of comments in flow file

Input Parameters  :
           flow_fp: pointer to flow file
             outfp: flow file path & name

Output Parameters : None

Return Value      : NS_PARSE_SCRIPT_SUCCESS in case function executed successfully
                    NS_PARSE_SCRIPT_ERROR in case any failure occurs in the function
*************************************************************************************************/
//Check for comments in flow files except in api's
int check_and_write_comments(FILE *flow_fp, FILE *outfp)
{
  char multiline_comment = 0;
  char commented_code = 0;
  //char *ptr = NULL;
  char *start_line = NULL;  //For sending uncommented line untrimmed

  while(1)
  {
    if(commented_code)
      if(read_line(flow_fp, NS_DO_NOT_TRIM) == NULL)
        SCRIPT_PARSE_ERROR(script_line, "Unexpected end of file");

    start_line = script_line;
    CLEAR_WHITE_SPACE(script_line);
    if(multiline_comment) 
    {
       CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(script_line);
       if (*script_line == '\0')
       {
         if (fwrite("\n", 1, 1, outfp) == 0)
           SCRIPT_PARSE_ERROR(script_line, "Error: Data Write Failed writing, Error = %s ", nslb_strerror(errno))
         commented_code = 1;
         continue;
       } 
      //Check for end of multi-line comment 
      if (strncmp(script_line + (strlen(script_line) - 2), "*/", 2) == 0)
      { 
        NSDL2_PARSING(NULL, NULL, "End of multi-line comment");
        multiline_comment = 0;
      }
      //sprintf(script_line, "%s", script_line); 
      NSDL2_PARSING(NULL, NULL, "multi-line commented line received, script line = %s", script_line);
    }
    else
    {
      if(strncmp(script_line, "//", 2) == 0)
      {
        NSDL2_PARSING(NULL, NULL, "Start of single-line comment");
        CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(script_line);
        //sprintf(script_line, "%s", script_line);
      }
      else if(strncmp(script_line, "/*", 2) == 0)
      {
        CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(script_line);
        if(strncmp(script_line + (strlen(script_line) - 2), "*/", 2) == 0)
        {
          //sprintf(script_line, "%s", script_line); 
          NSDL2_PARSING(NULL, NULL, "End of multi-line comment, multi-line commented line received as a single line comment");
        }
        else
        {
          NSDL2_PARSING(NULL, NULL, "Start of multi-line comment");
          multiline_comment = 1;
        }
      }
      else
      {
        script_line = start_line;
        return NS_PARSE_SCRIPT_SUCCESS;
      }
    }
    script_line = start_line; //Change start line to script line  
    write_line(outfp, script_line, 1);
    commented_code = 1;
  }
}
//This method parses init_script.c for trnasactions 
//This method handles the case if we put transaction in init_script
int parse_tx_from_file(char *file_name , int sess_idx)
{
  FILE *file_fp = NULL;
  char *ptr = NULL;
  
  NSDL2_PARSING(NULL, NULL, "Method called. File = %s", file_name);
  
  if((file_fp = fopen(file_name, "r")) == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, file_name, errno, nslb_strerror(errno));

  while(1)
  {
    ptr = read_line(file_fp, NS_DO_NOT_TRIM);
    if(ptr == NULL)
    {
      NSDL2_PARSING(NULL, NULL, "End of File Found");
      break;
    }
 
    if((strstr(script_line, "ns_start_transaction") != NULL) || (strstr(script_line, "ns_define_transaction") != NULL) ||
      (strstr(script_line, "ns_define_transaction") != NULL) || (strstr(script_line, "ns_end_transaction_as") != NULL))
    {
      NSDL2_PARSING(NULL, NULL, "Transaction found at script_line %d in file %s", script_ln_no, file_name);
      // This must be called after write_line is done as it tokenizes the script_line
      // which causes tokens to get NULL terminated.
      if(enable_test_metrics_mode == 0)
        g_dont_skip_test_metrics = 1;

      parse_transaction(script_line, file_name, script_ln_no, sess_idx);
    }
    if((strstr(script_line, "ns_sync_point") != NULL) || (strstr(script_line, "ns_define_syncpoint") != NULL))
    {
      NSDL2_PARSING(NULL, NULL, "sync point found at script_line %d in file %s", script_ln_no, file_name);
      parse_sp_api(script_line, file_name, script_ln_no);
    }
    // Parsing of timer api in init and exit. Only ns_start_timer will be parsed in init and exit script.
    if ((strstr(script_line, "ns_start_timer") != NULL))
    {
      NSDL2_PARSING(NULL, NULL, "Timer found at script_line %d in file %s", script_ln_no, file_name);
      parse_ns_timer(script_line, file_name, sess_idx);
    }
  } 
  if(fclose(file_fp) != 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000021]: ", CAV_ERR_1000021 + CAV_ERR_HDR_LEN, file_name, errno, nslb_strerror(errno));

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*************************************************************************
Parsing for ns_end_timer api
*************************************************************************/
int validate_ns_end_timer(char *buffer, char *fname, int line_num , int sess_idx)
{
  char *buf_ptr = NULL;
  char *fields[5];
  int nargs;
  int i ;
  char timer_name[1024] ;
  int amt_written = 0;

  NSDL3_TRANS(NULL, NULL, "Method called buffer is [%s]" , buffer);
  if ((buf_ptr = strstr (buffer, "ns_end_timer")) != NULL)
  {
    if (get_tokens(buf_ptr, fields, "\"",5) < 3)
    {
      nargs = get_args(buf_ptr, fields);
      if (nargs == -1 && nargs < 1){ 
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012245_ID, CAV_ERR_1012245_MSG);
      }
    }
    amt_written = snprintf(timer_name, 1024, "timer_%s", fields[1]);
    timer_name[amt_written] = '\0';

    // Checking if variable is already added in vartable or not 
    for (i= gSessionTable[sess_idx].nslvar_start_idx; i < gSessionTable[sess_idx].nslvar_start_idx + gSessionTable[sess_idx].num_nslvar_entries; i++)
    {
      NSDL2_PARSING(NULL, NULL, "nsl_var_entry (%d) , total nsl_var_entries(%d), buffer data(%s)",i , gSessionTable[sess_idx].num_nslvar_entries, RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[i].name));
      if (!strcmp(RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[i].name), timer_name)) {
        NSDL2_PARSING(NULL, NULL, "timer added for (%s) variable ",timer_name);
        break;
      }
    }
    if (i == gSessionTable[sess_idx].nslvar_start_idx + gSessionTable[sess_idx].num_nslvar_entries) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012246_ID, CAV_ERR_1012246_MSG, fields[1]);
    }
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

/****************************************************************************
This function Parses For timer API used in flow file
Here we are using the the argument as declare var 
and adding a prefix timer_ before it 
*****************************************************************************/
int parse_ns_timer(char *buffer, char *fname, int session_idx)
{
  char *buf_ptr;
  char *fields[5]; 
  char timer_name[1024];
  int nargs;
  int rnum;
  char msg[MAX_LINE_LENGTH] = "";
  char* sess_name;
  int i = 0;
  int amt_written = 0;

  NSDL3_TRANS(NULL, NULL, "Method called buffer is [%s]" , buffer);
 
  sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
  sprintf(msg, "script - %s/%s, api - ns_start_timer()", get_sess_name_with_proj_subproj_int(sess_name, session_idx, "/"), fname);
  //Previously taking with only script name
  //sprintf(msg, "script - %s/%s, api - ns_start_timer()", get_sess_name_with_proj_subproj(sess_name), fname);

  // Parse ns_start_timer, ex: ns_start_timer("checkout_timer");
  if ((buf_ptr = strstr (buffer, "ns_start_timer")) != NULL)
  {
    if (get_tokens(buf_ptr, fields, "\"", 5) < 3)
    {
      nargs = get_args(buf_ptr, fields);
      if (nargs != 1) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012008_ID, CAV_ERR_1012008_MSG);
      }
    }
    else 
    {
      NSDL2_PARSING(NULL, NULL, "ns_start_timer found in %s with field[0] = [%s] timer_name = %s", fname, fields[0], fields[1]);

      // Making timer_var timer_ type for eg : fields[1] = "checkout", now it will timer_checkout to avoid conflict for declare var 
      amt_written = snprintf(timer_name, 1024, "timer_%s", fields[1]);
      timer_name[amt_written] = '\0';

      // Checking if variable is already added in vartable to avoid duplicate table entries. 
      for (i= gSessionTable[session_idx].nslvar_start_idx; i < gSessionTable[session_idx].nslvar_start_idx + gSessionTable[session_idx].num_nslvar_entries; i++)
      {
        NSDL2_PARSING(NULL, NULL, "nsl_var_entry (%d) , total nsl_var_entries(%d), buffer data(%s)",i , gSessionTable[session_idx].num_nslvar_entries, RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[i].name));
        if (!strcmp(RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[i].name), timer_name))
        {
          NSDL2_PARSING(NULL, NULL, "variable (%s) already added as declare variable therefore skipping",RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[i].name));
          return 0;
        }
      }
      
      // Create variable table entry
      create_nsvar_table_entry(&rnum);

      if (gSessionTable[session_idx].nslvar_start_idx == -1) {
        gSessionTable[session_idx].nslvar_start_idx = rnum;
        gSessionTable[session_idx].num_nslvar_entries = 0;
      }
      
      gSessionTable[session_idx].num_nslvar_entries++;
      if (validate_var(timer_name)) 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, timer_name);

      if ((nsVarTable[rnum].name = copy_into_temp_buf(timer_name, 0)) == -1)
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN);

      nsVarTable[rnum].type = NS_VAR_SCALAR;
      nsVarTable[rnum].length = 0; /*For scalar type this will be 0*/

      if ((nsVarTable[rnum].default_value = copy_into_big_buf("-1", 0)) == -1) 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN);
    }
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*******************************************************************
This function will parse ns_start_timer api from flow file
(1) This function will parse flow file 
(2) Exclusively for ns_start_timer api other flow file api are not parsed here
(3) ns_end_timer is not parsed here it will be parse in parse_flow_file. 

We require this function to parse flow file  before create_variable_hash 
function whenever there is need to add new ns_var internally and 
not in registration.spec
**************************************************************************/
int parse_flow_file_for_api(char *flow_filename, char *flowout_filename, int sess_idx)
{
  FILE *flow_fp; //File pointer for flow file 
  char *in_flow_file; 
  int in_flow_file_size = 0;
  int total_bytes_read = 0;
  char multiline_comment = 0; //Flag for multiline comment
  char *start_ptr;  //Points to start ptr of substring  
  char *start_buf;  //Points starting address of buffer 
  struct stat fstat;
   
  
  NSDL2_PARSING(NULL, NULL, "Method Called flow_filename=[%s] sess_idx=%d flowout_filename=[%s]",
                flow_filename, sess_idx, flowout_filename);

  if ((flow_fp = fopen(flow_filename, "r")) == NULL){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, flow_filename, errno, nslb_strerror(errno));
  }
  // Calculate size of flow file and allocate memory to buffer   
  stat(flow_filename, &fstat); 
  in_flow_file_size = fstat.st_size;     //calculate size of flow_file 
  MY_MALLOC(in_flow_file, in_flow_file_size + 1, "in_flow_file", -1);   //allocate Memory
  NSDL2_PARSING(NULL, NULL, "total bytes read  [%d] and in_flow_file_size is [%d]", total_bytes_read, in_flow_file_size); 

  // Read file in buffer
  while (in_flow_file_size)
  { 
    total_bytes_read = fread(in_flow_file + total_bytes_read, 1, in_flow_file_size - total_bytes_read, flow_fp);
    NSDL2_PARSING(NULL, NULL, "total bytes read  [%d]", total_bytes_read);
    in_flow_file_size = in_flow_file_size - total_bytes_read;
  }

  in_flow_file[total_bytes_read] = '\0' ;
  NSDL2_PARSING(NULL, NULL, "total bytes read  [%d]", total_bytes_read);
  
  start_buf = in_flow_file;
  NSDL2_PARSING(NULL, NULL, "Starting of Parsing of flow file [%s]", flow_filename);
  
  while (*start_buf)
  { 
    // Read buffer line by line
    if ((start_ptr = strchr(start_buf, '\n')) == NULL) { 
      return 0; //This Will the last line of the script 
    }

    *start_ptr = '\0';
    
    CLEAR_WHITE_SPACE(start_buf);
    NSDL2_PARSING(NULL, NULL, "Now start buffer is [%s]",  start_buf);

    if (multiline_comment) {
      CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(start_buf);

      if (strncmp(start_buf + (strlen(start_buf) - 2), "*/", 2) == 0) {   // Check for end of multi-line comment 
        NSDL2_PARSING(NULL, NULL, "End of multi-line comment");
        multiline_comment = 0;
      }
      NSDL2_PARSING(NULL, NULL, "multi-line commented line received, start_buf = %s", start_buf);
    }
    else { 
      if (strncmp(start_buf, "//", 2) == 0) { // Parsing of single line comments  
        // In case of single line comment we will parse till 1 \n
        CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(start_buf); 
        NSDL2_PARSING(NULL, NULL, "Start of single-line comment");
      }  
      else if (strncmp(start_buf, "/*", 2) == 0) { // Parsing of multiline comments 
        CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(start_buf);
        if (strncmp(start_buf + (strlen(start_buf) - 2), "*/", 2) == 0) {
          NSDL2_PARSING(NULL, NULL, "multi-line commented line received as a single line comment");
        } 
        else { 
          NSDL2_PARSING(NULL, NULL, "Start of multi-line comment");
          multiline_comment = 1;
        }
      }  
      else { // NO comments found 
        NSDL2_PARSING(NULL, NULL, "No comments found in (%s)",start_buf);
        // No comments found in script line therefore we will search for api 
        if ((strstr(start_buf, "ns_start_timer") != NULL)) {
          NSDL2_PARSING(NULL, NULL, "Timer found at script_line in file %s", flow_filename);
          parse_ns_timer(start_buf, flow_filename, sess_idx);
        }
      }
    }
   start_buf = start_ptr + 1;
  }
 
  FREE_AND_MAKE_NULL(in_flow_file , "in_flow_file", -1);
 
  if (fclose(flow_fp) != 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000021]: ", CAV_ERR_1000021 + CAV_ERR_HDR_LEN, flow_filename, errno, nslb_strerror(errno));

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

//Parsing flow file and creating output file
//return NS_PARSE_SCRIPT_ERROR/NS_PARSE_SCRIPT_SUCCESS
int parse_flow_file(char *flow_filename, char *flowout_filename, int sess_idx, char inline_enabled)
{
  FILE *flow_fp = NULL, *outfp = NULL;
  char *ptr;
  int rdp_enabled = 0; /*bug 79149*/
  NSDL2_PARSING(NULL, NULL, "Method Called flow_filename=[%s] sess_idx=%d flowout_filename=[%s]", 
                flow_filename, sess_idx, flowout_filename);

  if ((flow_fp = fopen(flow_filename, "r")) == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, flow_filename, errno, nslb_strerror(errno));
  
  if ((outfp = fopen(flowout_filename, "w+")) == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, flowout_filename, errno, nslb_strerror(errno));

  //if(runProfTable[sess_idx].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    //fprintf(outfp, "#define  NS_SCRIPT_MODE_THREAD 1\n");
  NSDL2_PARSING(NULL, NULL, "Starting of Parsing of flow file [%s]", flow_filename);
  while(1)  //Parse flow fill till end of file or "}" not received.
  {
    ptr = read_line(flow_fp, NS_DO_NOT_TRIM); // It reads in script_line
    if(ptr == NULL)
    {
      NSDL2_PARSING(NULL, NULL, "End of File Found");
      break;
    }

    if(check_and_write_comments(flow_fp, outfp) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

     NSDL2_PARSING(NULL, NULL, "script_line [%s]", script_line);
    if((ptr = strstr(script_line, "ns_web_url")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_web_url() found at script_line %d \n", script_ln_no);
      if(parse_web_url(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, inline_enabled) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_grpc_client")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_grpc_client() found at script_line %d \n", script_ln_no);
      if(parse_web_url(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, GRPC_CLIENT) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
/***************************
 * click script apis BEGIN *
 ***************************/
    else if((ptr = strstr(script_line, "ns_browser")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_browser() found at script_line %d \n", script_ln_no);
      if(parse_ns_browser(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_link")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_link() found at script_line %d \n", script_ln_no);
      if(parse_ns_link(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_button")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_button() found at script_line %d \n", script_ln_no);
      if(parse_ns_button(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_edit_field")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_edit_field() found at script_line %d \n", script_ln_no);
      if(parse_ns_edit_field(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_check_box")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_check_box() found at script_line %d \n", script_ln_no);
      if(parse_ns_check_box(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_radio_group")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_radio_group() found at script_line %d \n", script_ln_no);
      if(parse_ns_radio_group(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_list")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_list() found at script_line %d \n", script_ln_no);
      if(parse_ns_list(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_form")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_form() found at script_line %d \n", script_ln_no);
      if(parse_ns_form(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_map_area")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_map_area() found at script_line %d \n", script_ln_no);
      if(parse_ns_map_area(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_submit_image")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_submit_image() found at script_line %d \n", script_ln_no);
      if(parse_ns_submit_image(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_js_dialog")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_js_dialog() found at script_line %d \n", script_ln_no);
      if(parse_ns_js_dialog(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_text_area")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_text_area() found at script_line %d \n", script_ln_no);
      if(parse_ns_text_area(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_span")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_span() found at script_line %d \n", script_ln_no);
      if(parse_ns_span(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_scroll")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_scroll() found at script_line %d \n", script_ln_no);
      if(parse_ns_scroll(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_element")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_element() found at script_line %d \n", script_ln_no);
      if(parse_ns_element(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_mouse_hover")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_mouse_hover() found at script_line %d \n", script_ln_no);
      if(parse_ns_mouse_hover(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_mouse_out")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_mouse_out() found at script_line %d \n", script_ln_no);
      if(parse_ns_mouse_out(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if(!rdp_enabled && ((ptr = strstr(script_line, "ns_mouse_move")) != NULL))
    {
      NSDL2_PARSING(NULL, NULL, "ns_mouse_move() found at script_line %d \n", script_ln_no);

        if(parse_ns_mouse_move(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
          return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_key_event")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_key_event() found at script_line %d \n", script_ln_no);
      if(parse_ns_key_event(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_get_num_domelement")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_get_num_domelement() found at script_line %d \n", script_ln_no);
      if(parse_ns_get_num_domelement(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_browse_file")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_browse_file() found at script_line %d \n", script_ln_no);
      if(parse_ns_browse_file(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_js_checkpoint")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_js_checkpoint() found at script_line %d \n", script_ln_no);
      if(parse_ns_js_checkpoint(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_execute_js")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_execute_js() found at script_line %d \n", script_ln_no);
      if(parse_ns_execute_js(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }

/*************************
 * click script apis END *
 *************************/

    else if((ptr = strstr(script_line, "ns_ftp_get")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ftp_get() found at script_line %d \n", script_ln_no);
      if(parse_ns_ftp_get(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ftp_put")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ftp_put() found at script_line %d \n", script_ln_no);
      if(parse_ns_ftp_put(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_dns_query")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_dns_query() found at script_line %d \n", script_ln_no);
      if(parse_ns_dns_query(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_pop_list")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_pop_list() found at script_line %d \n", script_ln_no);
      if(parse_ns_pop3_list(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_pop_stat")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_pop_stat() found at script_line %d \n", script_ln_no);
      if(parse_ns_pop3_stat(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_pop_get")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_pop_get() found at script_line %d \n", script_ln_no);
      if(parse_ns_pop3_get(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_smtp_send")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_smtp_send() found at script_line %d \n", script_ln_no);
      if(parse_ns_smtp_send(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }

    else if((ptr = strstr(script_line, "ns_tr069_")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "tr069 api() found at script_line %d \n", script_ln_no);
      if(parse_tr069(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((strstr(script_line, "ns_start_transaction") != NULL) || (strstr(script_line, "ns_end_transaction") != NULL) ||
            (strstr(script_line, "ns_define_transaction") != NULL) || (strstr(script_line, "ns_end_transaction_as") != NULL))
    {
      g_dont_skip_test_metrics = 1;
      NSDL2_PARSING(NULL, NULL, "Transaction found at script_line %d in file %s", script_ln_no, flow_filename);
      char line[1024];
      /* In case of JTS, nsApi object get missed while converting the API's
         line_ptr will have the reference till the API name is found */
      char *line_ptr;
      snprintf(line, 1024, "%s", script_line);
      line_ptr = line;
      int ret = parse_transaction(script_line, flow_filename, script_ln_no, sess_idx);
      if(strstr(line, "ns_define_transaction") == NULL)
      {
         if(ret != 2)
            write_line(outfp, line, 0);
         else{
            char loc_line[1024]={'\0'};
            if((ptr = strstr(line, "ns_start_transaction")) != NULL){ 
               // Take all the data till this APis is found 
               strncpy(loc_line,line_ptr,(ptr-line_ptr));
               strcat(loc_line, "ns_start_transaction_ex");
               strcat(loc_line, ptr + 20);
               //snprintf(loc_line, 1024, "ns_start_transaction_ex%s", line + 20);              
            }
            else if((ptr = strstr(line, "ns_end_transaction_as"))){//ns_end_transaction_as
               // Take all the data till this APis is found 
               strncpy(loc_line,line_ptr,(ptr-line_ptr));
               strcat(loc_line, "ns_end_transaction_as_ex");
               strcat(loc_line, ptr + 21);
               //snprintf(loc_line, 1024, "ns_end_transaction_as_ex%s", line + 21);
            }
            else if((ptr = strstr(line, "ns_end_transaction"))){
               // Take all the data till this APis is found 
               strncpy(loc_line,line_ptr,(ptr-line_ptr));
               strcat(loc_line, "ns_end_transaction_ex");
               strcat(loc_line, ptr + 18);
               //snprintf(loc_line, 1024, "ns_end_transaction_ex%s", line + 21);
            }            
            write_line(outfp, loc_line, 0);
            }
      }
      // This must be called after write_line is done as it tokenizes the script_line
      // which causes tokens to get NULL terminated.
    }
    else if ((strstr(script_line, "ns_end_timer") != NULL))
    {
      NSDL2_PARSING(NULL, NULL, " End Timer found at script_line %d in file %s", script_ln_no, flow_filename);
      write_line(outfp, script_line, 0);
      if (validate_ns_end_timer(script_line, flow_filename, script_ln_no ,sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((strstr(script_line, "ns_sync_point") != NULL) || (strstr(script_line, "ns_define_syncpoint") != NULL))
    { 
      NSDL2_PARSING(NULL, NULL, "SyncPoint API found at script_line %d in file %s", script_ln_no, flow_filename);
      //Define syncpoint is not be executed
      if(strstr(script_line, "ns_define_syncpoint") == NULL)
      { 
        write_line(outfp, script_line, 0);
      }
      parse_sp_api(script_line, flow_filename, script_ln_no);
    }
    else if((ptr = strstr(script_line, "ns_sock_")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "socket api() found at script_line %d \n", script_ln_no);
      if(parse_ns_socket_api(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if ((ptr = strstr(script_line, "ns_jrmi_call")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, " found at script_line %d \n", script_ln_no);
      if(ns_parse_jrmi("ns_jrmi_call", "ns_web_url" ,flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if ((ptr = strstr(script_line, "ns_jrmi_page")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, " found at script_line %d \n", script_ln_no);
      if(ns_parse_jrmi_ex("ns_web_url", "ns_jrmi_page" ,flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if ((ptr = strstr(script_line, "ns_jrmi_sub_step_start")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, " found at script_line %d \n", script_ln_no);
      if(ns_parse_jrmi_ex("ns_web_url", "ns_jrmi_sub_step_start" ,flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }



   /*************Ldap parsing code start ******************/
    else if((ptr = strstr(script_line, "ns_ldap_login")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_login() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_LOGIN) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ldap_logout")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_logout() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_LOGOUT) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ldap_search")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_search() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_SEARCH) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ldap_add")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_add() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_ADD) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ldap_rename")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_rename() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_RENAME) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ldap_delete")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_delete() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_DELETE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ldap_logout")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_logout() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_LOGOUT) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_ldap_modify")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_ldap_modify() found at script_line %d \n", script_ln_no);
      if(ns_parse_ldap(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, LDAP_MODIFY) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    /************* Ldap parsing code end ******************/
    /************* IMAP parsing code start****************/
    else if((ptr = strstr(script_line, "ns_imap_select")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_imap_select() found at script_line %d \n", script_ln_no);
      if(ns_parse_imap("ns_imap_select", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, IMAP_SELECT) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    } else if((ptr = strstr(script_line, "ns_imap_fetch")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_imap_select() found at script_line %d \n", script_ln_no);
      if(ns_parse_imap("ns_imap_fetch", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, IMAP_FETCH) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    } else if((ptr = strstr(script_line, "ns_imap_store")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_imap_store() found at script_line %d \n", script_ln_no);
      if(ns_parse_imap("ns_imap_store", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, IMAP_STORE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_imap_list")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_imap_list() found at script_line %d \n", script_ln_no);
      if(ns_parse_imap("ns_imap_list", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, IMAP_LIST) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_imap_search")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_imap_search() found at script_line %d \n", script_ln_no);
      if(ns_parse_imap("ns_imap_search", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, IMAP_SEARCH) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    /************* IMAP parsing code end****************/

    /*************websocket parsing code start ****/
    else if((ptr = strstr(script_line, "ns_web_websocket_connect")) != NULL)
    {  
      NSDL2_PARSING(NULL, NULL, "ns_web_websocket_connect() found at script_line %d \n", script_ln_no);
      if(ns_parse_websocket_connect(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_web_websocket_send")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_web_websocket_send() found at script_line %d \n", script_ln_no);
      if(ns_parse_websocket_send(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_web_websocket_close")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_web_websocket_close() found at script_line %d \n", script_ln_no);
      if(ns_parse_websocket_close(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
  
    /**************** FC2 parsing code *****************/
    else if((ptr = strstr(script_line, "ns_fc2_send")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_fc2_send() found at script_line %d \n", script_ln_no);
      if(ns_parse_fc2_send(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
 
    /************* XMPP parsing code start *******/
    else if((ptr = strstr(script_line, "ns_xmpp_login")) != NULL)
    {  
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_login() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_login(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_create_group")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_create_group() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_create_group(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_delete_group")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_delete_group() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_delete_group(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_send")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_send() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_send(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_add_contact")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_add_contact() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_add_contact(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_delete_contact")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_delete_contact() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_delete_contact(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_add_member")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_add_member() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_add_member(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_delete_member")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_delete_member() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_delete_member(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_join_group")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_join_group() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_join_group(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_xmpp_leave_group")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_xmpp_leave_group() found at script_line %d \n", script_ln_no);
      if(ns_parse_xmpp_leave_group(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    /************* XMPP parsing code end ********/

    /*************** SSL Setting *********/
    else if((ptr = strstr(script_line, "ns_set_ssl_settings")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_parse_set_ssl_setting() found at script_line %d \n", script_ln_no);
      write_line(outfp, script_line, 0);
      if(ns_parse_set_ssl_setting(ptr) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    /************** SockJs API's *********/
    else if((ptr = strstr(script_line, "ns_sockjs_connect")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_sockjs_connect() found at script_line %d \n", script_ln_no);
      if(parse_ns_sockjs_connect(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_sockjs_send")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_sockjs_send() found at script_line %d \n", script_ln_no);
      if(parse_ns_sockjs_send(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_sockjs_close")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_sockjs_close() found at script_line %d \n", script_ln_no);
      if(parse_ns_sockjs_close(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_url_get_resp_msg")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_url_get_resp_msg() found at script_line %d \n", script_ln_no);
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_HEADER;
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_BODY;
      write_line(outfp, script_line, 0);
    }
    else if((ptr = strstr(script_line, "ns_url_get_hdr_msg")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_url_get_hdr_msg() found at script_line %d \n", script_ln_no);
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_HEADER;
      write_line(outfp, script_line, 0);
    }
    else if((ptr = strstr(script_line, "url_get_body_msg")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "url_get_body_msg found at script_line %d \n", script_ln_no);
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_BODY;
      write_line(outfp, script_line, 0);
    }
    else if((ptr = strstr(script_line, "ns_url_get_resp_size")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_url_get_resp_size() found at script_line %d \n", script_ln_no);
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_HEADER;
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_BODY;
      write_line(outfp, script_line, 0);
    }
    else if((ptr = strstr(script_line, "ns_url_get_body_size")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_url_get_body_size() found at script_line %d \n", script_ln_no);
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_BODY;
      write_line(outfp, script_line, 0);
    }
    else if((ptr = strstr(script_line, "ns_url_get_hdr_size")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_url_get_hdr_size found at script_line %d \n", script_ln_no);
      gSessionTable[sess_idx].save_url_body_head_resp |= SAVE_URL_HEADER;
      write_line(outfp, script_line, 0);
    }
    else if((ptr = strstr(script_line, "ns_socket_open")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_open() found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_open(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_send")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_send() found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_send(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_recv")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_recv() found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_recv(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_close")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_close() found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_close(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_exclude")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_exclude() found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_exclude(flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_set_options")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_set_options () found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_c_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, "ns_socket_set_options")
                      == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_get_options")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_get_options () found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_c_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, "ns_socket_get_options")
                      == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_get_attribute")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_get_attribute () found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_c_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, "ns_socket_get_attribute")
                      == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_enable")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_enable () found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_c_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, "ns_socket_enable")
                      == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_disable")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_disable () found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_c_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, "ns_socket_disable")
                      == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_get_fd")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_get_fd () found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_c_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, "ns_socket_get_fd")
                      == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, "ns_socket_start_ssl")) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "ns_socket_start_ssl () found at script_line %d \n", script_ln_no);
      if(ns_parse_socket_c_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, "ns_socket_start_ssl")
                      == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }/*******************************************  START Bug 79149 ***************************************/
    else if((ptr = strstr(script_line, NS_RDP_CONNECT_STR)))
    {
      CHECK_AND_SET_RDP_FLAG();
      NSDL2_PARSING(NULL, NULL, "ns_rdp_connect found at script_line %d \n", script_ln_no);
      NSDL2_PARSING(NULL, NULL, "calling ns_parse_rdp_connectio_apis for api_type = %d", NS_RDP_CONNECT);
      if(ns_parse_rdp_connection_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, NS_RDP_CONNECT) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, NS_RDP_DISCONNECT_STR)))
    {
      CHECK_RDP_ENABLED();
      NSDL2_PARSING(NULL, NULL, "ns_rdp_disconnect found at script_line %d \n", script_ln_no);
      NSDL2_PARSING(NULL, NULL, "calling ns_parse_rdp_connectio_apis for api_type = %d", NS_RDP_DISCONNECT); 
      if(ns_parse_rdp_disconnect_api(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, NS_RDP_DISCONNECT) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, NS_KEY_STR)))
    {
      CHECK_RDP_ENABLED();
      NSDL2_PARSING(NULL, NULL, "ns_key found at script_line %d \n", script_ln_no);
      NSDL2_PARSING(NULL, NULL, "calling ns_parse_key_apis_ex");
      if(ns_parse_key_apis_ex(script_line, flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, NS_TYPE_STR)))
    {
      CHECK_RDP_ENABLED();
      NSDL2_PARSING(NULL, NULL, "ns_type found at script_line %d \n", script_ln_no);
      NSDL2_PARSING(NULL, NULL, "calling ns_parse_key");
      if(ns_parse_key_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, NS_TYPE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, NS_MOUSE)))
    {
      CHECK_RDP_ENABLED();
      NSDL2_PARSING(NULL, NULL, "ns_mouse found at script_line %d \n", script_ln_no);
      NSDL2_PARSING(NULL, NULL, "calling ns_parse_mouse_apis_ex");
      if(ns_parse_mouse_apis_ex(script_line, flow_fp, outfp, flow_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((ptr = strstr(script_line, NS_SYNC_STR)))
    {
      CHECK_RDP_ENABLED();
      NSDL2_PARSING(NULL, NULL, "ns_sync found at script_line %d \n", script_ln_no);
      NSDL2_PARSING(NULL, NULL, "calling ns_parse_key");
      if(ns_parse_sync_api(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, NS_SYNC) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    } /*******************************************  END Bug 79149 ***************************************/
    else //For function name, {, } etc.
    {
      NSDL2_PARSING(NULL, NULL, "Writing [%s] to [%s] file\n", script_line, flowout_filename);
      write_line(outfp, script_line, 0);
    }
  }
  
  skip_test_metrices();

  if(fclose(flow_fp) != 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000021]: ", CAV_ERR_1000021 + CAV_ERR_HDR_LEN, flow_filename, errno, nslb_strerror(errno));

  if(fclose(outfp) != 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000021]: ", CAV_ERR_1000021 + CAV_ERR_HDR_LEN, flowout_filename, errno, nslb_strerror(errno));

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

void get_used_runlogic_unique_list(int sess_idx, char **list)
{
  char *runlogic_list = NULL;
  int len = 0;
  int size = 0;
  int i;

  NSDL2_PARSING(NULL, NULL, "Method called sess_idx = %d",sess_idx);
  for(i = 0; i <total_runprof_entries; i++)
  {
    if(runProfTable[i].sessprof_idx == sess_idx)
    {
      if (size)
      {
        char *ptr;
        if ((ptr = strstr(runlogic_list, runProfTable[i].runlogic)) != NULL)
        {
          ptr += strlen(runProfTable[i].runlogic);
          if ((ptr != NULL) && *ptr == ',')
          {
            //TODO: Ayush add debug for duplicate entry [Ex:]
            /*
               scnenario
               G_SCRIPT_RUNLOGIC ALL runlogic1
               SGRP G1  script1
               SGRP G2  script1
            */
            continue;
          }
        }
      }
      size += strlen(runProfTable[i].runlogic) + 1;
      MY_REALLOC(runlogic_list, size, "Runlogic List", 0);
      len += sprintf (runlogic_list + len, "%s,", runProfTable[i].runlogic);
    }
  }
  runlogic_list[len-1] = '\0';  //removing last ',' from runlogic_list
  *list = runlogic_list;
   NSDL2_PARSING(NULL, NULL, "list =%s",list);
}

int get_flow_filelist(char *path, FlowFileList_st **filelist_arr, int sess_idx)
{
  int i= 0, ret = 0;
  //Initialized with -1 because we dont want to count the 1 line in flowfile names
  //Output would be like -
  //  UsedFlowList -- First line is a Heading only,
  //  Info
  //  SiginIn
  int file_count = 0;
  char file_path[MAX_LINE_LENGTH + 1];
  FILE *fp;
  char cmd[MAX_LINE_LENGTH + 1];
  char out_filename[MAX_FILE_NAME_LEN + 1];
  char err_msg[1024] = "\0";
  char *runlogic_list;

  NSDL2_PARSING(NULL, NULL, "Method Called. path = %s", path);
  /*bug id: 101320: path would be relative to $NS_WDIR:
    workspace/<user_name>/<profile_name>/cavisson/<proj>/<sub_proj>/scripts/<script_dir>*/
  //Extracting the flow filenames from runlogic using
  //egrep "^extern FlowReturn" runlogic.c | cut third feild without ()

  // get list of run logic used for given session id
  get_used_runlogic_unique_list(sess_idx, &runlogic_list);

  sprintf(out_filename, "%s/nsu_script_tool.out", g_ns_tmpdir);
  if ((run_mode_option & RUN_MODE_OPTION_COMPILE) || (gSessionTable[sess_idx].flags & ST_FLAGS_SCRIPT_OLD_FORMAT))
    sprintf(cmd, "nsu_script_tool -o %s -s %s -w %s/%s> %s 2>&1", (gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_C)?"UsedFlowList":"AllFlowList", get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), GET_NS_WORKSPACE(), GET_NS_PROFILE(), out_filename);
  else
    sprintf(cmd, "nsu_script_tool -o %s -s %s -w %s/%s -r %s> %s 2>&1", (gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_C)?"UsedFlowList":"AllFlowList", get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), GET_NS_WORKSPACE(), GET_NS_PROFILE(), runlogic_list, out_filename);

  NSDL2_PARSING(NULL, NULL, "Runnig cmd = %s", cmd);
  ret = nslb_system(cmd,1,err_msg);
  NSDL2_PARSING(NULL, NULL, "Returning ret = %d", ret);
  if(ret != 0)
  {
    //sprintf(cmd, "cat %s", out_filename);
    //ret = nslb_system(cmd,1,err_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000019]: ", CAV_ERR_1000019 + CAV_ERR_HDR_LEN, cmd, errno, nslb_strerror(errno));
  }
  if((fp = fopen(out_filename, "r")) == NULL)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, out_filename, errno, nslb_strerror(errno));
  }

  NSDL2_PARSING(NULL, NULL, "reding out_filename = %s", out_filename);
  // Read output to get the cout
  while ((read_line(fp, 1)) != NULL)
    file_count++;

  fclose(fp);

  //If file_count is 0, then something wrong as at least header line should come
  if(file_count == 0)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012004_ID, CAV_ERR_1012004_MSG);
  }

  if(gSessionTable[sess_idx].script_type != NS_SCRIPT_TYPE_JAVA)
    file_count--; //Reduce for header line

  if(file_count == 0)
  {
    NSDL2_PARSING(NULL, NULL, "There are no used flows in the script");
    return 0;
  }

/* if(gSessionTable[sess_idx].script_type != NS_SCRIPT_TYPE_JAVA) */
   file_count += 2; // Done for init_script and exit_script. Bug - 111105

  NSDL2_PARSING(NULL, NULL, "Number of Flow files found from runlogic = %d", file_count);
  //Allocate structure array for the number of files returned above
  MY_MALLOC(*filelist_arr, sizeof(FlowFileList_st) * file_count, "Flow File Structure", 0); 

  // 111105 - From 4.6.2, we are giving support of init script to be act like as flow.c. 
  // It means we can have a NS API present in these two files.
  if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA)
    strcpy(((*filelist_arr)[i].orig_filename), "init_script.java");
  else
    strcpy(((*filelist_arr)[i].orig_filename), "init_script.c");


   sprintf(file_path, "%s/%s", path, (*filelist_arr)[i].orig_filename);
   if(access(file_path, R_OK))
   {
     NSDL2_PARSING(NULL, NULL, "Flow file %s/%s doesn't exist", path, ((*filelist_arr)[i].orig_filename));
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000016]: ", CAV_ERR_1000016 + CAV_ERR_HDR_LEN, ((*filelist_arr)[i].orig_filename));
   }

   i++;

  //Open the file again for fetching the filenames
  fp = fopen(out_filename, "r");

  //Ignoring the first line as First line is a Heading only in case of UsedFlowFile
  if(gSessionTable[sess_idx].script_type != NS_SCRIPT_TYPE_JAVA){
    if (read_line(fp, 1) == NULL)
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000020]: ", CAV_ERR_1000020 + CAV_ERR_HDR_LEN, out_filename);
    }
    NSDL2_PARSING(NULL, NULL, "Skipping header line = %s", script_line);
  }

  while ((read_line(fp, 1)) != NULL)
  {
    int len = strlen (script_line); // Length include \n if present
    if(script_line[len - 1] == '\n')
    {
      script_line[len - 1] = '\0';
      len--;
    }
    NSDL2_PARSING(NULL, NULL, "%d Flow File Found %s", i + 1, script_line);

    if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA) {
      char *ptr = strstr(script_line, ".java");
      if(ptr == NULL)
        sprintf(((*filelist_arr)[i].orig_filename), "%s.java", script_line);
      else
        sprintf(((*filelist_arr)[i].orig_filename), "%s", script_line);
    }
    else {
    // FLow name is without .c extension
    // Allow with .c also just in case
    char *ptr = strstr(script_line, ".c");
    if(ptr == NULL)
      sprintf(((*filelist_arr)[i].orig_filename), "%s.c", script_line);
    else
      sprintf(((*filelist_arr)[i].orig_filename), "%s", script_line);
    }

    sprintf(((*filelist_arr)[i].file_withoutcomments), "%s_nocomments.c", script_line);

    sprintf(file_path, "%s/%s", path, (*filelist_arr)[i].orig_filename);
    NSDL2_PARSING(NULL, NULL, "path = %s file_name = %s", path, (*filelist_arr)[i].orig_filename);
    NSDL2_PARSING(NULL, NULL, "file_path = %s", file_path);
    if(!access(file_path, R_OK)) 
    {
/*
      sprintf(cmd, "./bin/nsi_remove_comments.sed < %s/%s > %s/%s", path, ((*filelist_arr)[i].orig_filename), g_ns_tmpdir, ((*filelist_arr)[i].file_withoutcomments));
      NSDL2_PARSING(NULL, NULL, "Running cmd = %s", cmd);
      ret = nslb_system(cmd,1,err_msg); 
      if(ret != 0)
      {
        SCRIPT_PARSE_ERROR_EXIT(NULL, "Error in parsing file %s/%s - Error in preprocessing of the flow file", path, ((*filelist_arr)[i].orig_filename));
        return -1;
      }

      NSDL2_PARSING(NULL, NULL, "%s cpp filename created", ((*filelist_arr)[i].file_withoutcomments));
*/
    }
    else
    {
      NSDL2_PARSING(NULL, NULL, "Flow file %s/%s doesn't exist", path, ((*filelist_arr)[i].orig_filename));
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000016]: ", CAV_ERR_1000016 + CAV_ERR_HDR_LEN, ((*filelist_arr)[i].orig_filename));
    }
    i++;
  }

  fclose(fp);

  // 111105 - From 4.6.2, we are giving support of exit script to be act like as flow.c. 
  // It means we can have a NS API present in these two files.
  if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA)
    strcpy(((*filelist_arr)[i].orig_filename), "exit_script.java");
  else
    strcpy(((*filelist_arr)[i].orig_filename), "exit_script.c");

  sprintf(file_path, "%s/%s", path, (*filelist_arr)[i].orig_filename);
  if(access(file_path, R_OK))
  {
    NSDL2_PARSING(NULL, NULL, "Flow file %s/%s doesn't exist", path, ((*filelist_arr)[i].orig_filename));
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000016]: ", CAV_ERR_1000016 + CAV_ERR_HDR_LEN, ((*filelist_arr)[i].orig_filename));
  }

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return file_count;
}

static int chk_append_file(char *path, char *filename, char *out_buf, char required) 
{
  char file_path[MAX_SCRIPT_FILE_NAME_LEN + 1];

  NSDL3_PARSING(NULL, NULL, "Method Called, Adding filename[%s] to compilation list, required = %d", 
                          filename, required);

  //Check for existance of filename in the specified path
  //If doesn't exist and required in 1, return error

  sprintf(file_path, "%s/%s", path, filename);
  NSDL3_PARSING(NULL, NULL, "path = %s file_path = %s", path, file_path);
  if(access(file_path, R_OK)) {
    if(required == 0) 
      return NS_PARSE_SCRIPT_SUCCESS;
     else{ 
       NSDL3_PARSING(NULL, NULL, "calling SCRIPT_PARSE_ERROR_EXIT_EX");
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000016]: ", CAV_ERR_1000016 + CAV_ERR_HDR_LEN, filename);
     }
  }

  sprintf(out_buf, "%s %s", out_buf, file_path);

  NSDL3_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

void is_script_run_in_thread_mode(int sess_idx, char *thread_cmd_buffer)
{
  int i, j;
  char exit_buf[512]={0};
  for (i = 0; i < total_runprof_entries; i++) {
    if(runProfTable[i].sessprof_idx == sess_idx && runProfTable[i].gset.script_mode != NS_SCRIPT_MODE_LEGACY)
    {
      if(runProfTable[i].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
        sprintf(thread_cmd_buffer, "%s", "-DNS_SCRIPT_MODE_THREAD"); 

      for(j = i+1; j < total_runprof_entries; j++)
      {
        if(runProfTable[j].sessprof_idx == sess_idx && runProfTable[j].gset.script_mode != NS_SCRIPT_MODE_LEGACY)
        {
          if(runProfTable[i].gset.script_mode != runProfTable[j].gset.script_mode)
          {
            if(runProfTable[i].gset.script_mode ==  NS_SCRIPT_MODE_SEPARATE_THREAD)
            {
              sprintf(exit_buf, "Error: Group %s is use script %s in thread mode, and group %s is use in User Context\n",
                               RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name), 
                               RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name),
                               RETRIEVE_BUFFER_DATA(runProfTable[j].scen_group_name));
            }
            else
            {
              sprintf(exit_buf, "Error: Group %s is use script %s in User Context, and group %s is use in thread\n",
                               RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name),
                               RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name),
                               RETRIEVE_BUFFER_DATA(runProfTable[j].scen_group_name));

            }
            NS_EXIT (-1, "%s", exit_buf);
          }
        }
      }
    }
  }
}

static void check_flow_exist(char *flow, void *handle, int sess_idx)
{
  void *flow_exist;

  flow_exist = dlsym(handle, flow);
  if(flow_exist == NULL) 
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012016_ID, CAV_ERR_1012016_MSG, flow, flow);
}

//For printing Script error and warning 
void output_script_error(char *err_warn_file){
  char *ptr;
  char *ptr1;
  char script_err_line[MAX_LINE_LENGTH];
  int tmp_flow_path_len;
  FILE *script_err_log_fp;  //file pointer for .script.log file 
  
  if ((script_err_log_fp = fopen(err_warn_file, "r")) == NULL)
  {
    NS_EXIT(-1, "Error in openning file %s", err_warn_file);
  }

  tmp_flow_path_len = strlen(g_ns_tmpdir);

  while((ptr = nslb_fgets(script_err_line, MAX_LINE_LENGTH, script_err_log_fp, 1)) != NULL)
  {
    if ((ptr1 = strstr(script_err_line, g_ns_tmpdir)) == NULL) { 
      fprintf(stderr, "%s", script_err_line);
      continue;
    }
    ptr1 += (tmp_flow_path_len + 1);
    fprintf(stderr, "%s", ptr1);
    write_log_file(NS_SCENARIO_PARSING, "%s", ptr1);
  }

  fclose(script_err_log_fp);
}

void runprof_save_runlogic(int sess_idx, void *handle)
{
  int i;
  //TODO: Ayush add debug log
  NSDL2_PARSING(NULL, NULL, "Method called sess_idx = %d ",sess_idx);
  for(i = 0; i <total_runprof_entries; i++)
  {
    if( runProfTable[i].sessprof_idx == sess_idx)
    {
      if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_C)
      {
        runProfTable[i].gset.runlogic_func_ptr = dlsym(handle, runProfTable[i].runlogic);
        if(runProfTable[i].gset.runlogic_func_ptr == NULL)
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012015_ID, CAV_ERR_1012015_MSG);
        }
      }
      else
      {
        runProfTable[i].gset.runlogic_func = strdup(runProfTable[i].runlogic);
        NSDL2_PARSING(NULL, NULL, "runProfTable[i].gset.runlogic_func =%s ",runProfTable[i].gset.runlogic_func);
      }
    }
  }
}

static void append_runlogic_files(int sess_idx, char *path, char* cmd_buffer, char required)
{
  int i;
  char *runlogic_list;
  char runlogic[512+1];

  
  //TODO:Ayush add debug log
   NSDL2_PARSING(NULL, NULL, "Method called path = %s ",path);
  // check if script is old or new format by checking runlogic directory
  if (gSessionTable[sess_idx].flags & ST_FLAGS_SCRIPT_NEW_FORMAT)
  {
    get_used_runlogic_unique_list(sess_idx, &runlogic_list);
    char *field[total_runprof_entries];
    int num_toks = get_tokens(runlogic_list, field, ",", total_runprof_entries);
    for(i = 0; i < num_toks; i++)
    {
      snprintf(runlogic, 512, "runlogic/%s.c", field[i]);
      NSDL2_PARSING(NULL, NULL, "runlogic =%s",runlogic);
      chk_append_file(path, runlogic, cmd_buffer, 1);
    }
  }
  else
  {
    chk_append_file(path, "runlogic.c", cmd_buffer, 1);
  }
}

//For creating the shared library from various parsed script files
int
create_page_script_ptr_for_script_type_c(int sess_idx, char *path, FlowFileList_st *lst_ptr, int file_cnt)
{ 
  // Increase buffer size, as we are getting issue with 1920 flows in case of replay
  char cmd_buffer[MAX_LINE_LENGTH * 4]; // keep it big to accomdate large numnber of flow files 
  char script_libs_flag[MAX_LINE_LENGTH];
  char ns_debug_flag[16] = "\0";  // It can have empty or -DNS_DEBUG_ON
  void* handle;
  char* error;
  char* err_in_api = NULL;
  int index, i;
  int return_value;
  char err_warn_file[512] = {0}; //keep error and warning of c type script
  char thread_cmd_buffer[50] = "\0"; // keep it big to accomdate large numnber of flow files
  char *script_name_without_full_path;
  char err_msg[MAX_LINE_LENGTH + 1] = {0};

  script_name_without_full_path = strchr((path + 2), '/');
  script_name_without_full_path++;
  NSDL3_PARSING(NULL, NULL, "Method called, to create script function pointers sess_idx = %d",
                         get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));
                         //Previously taking with only script name
                         //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)));

  read_script_libs(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), script_libs_flag, sess_idx); 
  //complie the script (c file)
  //use ns_string_api.so/ns_string_api_debug.so on the basis of netstorm/netstorm.debug
 
#ifdef NS_DEBUG_ON
  strcpy(ns_debug_flag, "-DNS_DEBUG_ON");
#endif
  is_script_run_in_thread_mode(sess_idx, thread_cmd_buffer);
  sprintf(cmd_buffer, "gcc -g -fgnu89-inline %s %s -m%d -fpic -DENABLE_RUNLOGIC_PROGRESS -shared -Wall -I%s/include/ -I%s/thirdparty/include -I/home/cavisson/thirdparty/include -I%s ./bin/%s -o %s/%s.so -L%s/thirdparty/lib ", thread_cmd_buffer,
                       ns_debug_flag, NS_BUILD_BITS,
                       g_ns_wdir, g_ns_wdir, path,
                       NS_STRING_API,
                       g_ns_tmpdir, get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), 
                       sess_idx, "-"), g_ns_wdir);
  NSDL3_PARSING(NULL, NULL, "cmd before appending script files = %s", cmd_buffer);

  // We need to make .so using following files:
  //   user_test_init.c (if present)
  //   user_test_exit.c (if present)
  //   init_script.c (must)
  //   exit_script.c (must)
  //   websocket_callback.c (if present)
  //   runlogic.c (Must)
  //   All used flow files generated during parsing (May not be any files)

  // Optional files
  chk_append_file(path, "user_test_init.c", cmd_buffer, 0); // Append <space> <file>
  chk_append_file(path, "user_test_exit.c", cmd_buffer, 0);
  //chk_append_file(path, "init_script.c", cmd_buffer, 1);
  //chk_append_file(path, "exit_script.c", cmd_buffer, 1);
  chk_append_file(path, "websocket_callback.c", cmd_buffer, 0); //Added for callbacks in websocket.
  // Required files
  append_runlogic_files(sess_idx, path, cmd_buffer, 1);

  // In a loop append all flow C files
  for(i = 0; i < file_cnt; i++) {
    chk_append_file(g_ns_tmpdir, lst_ptr[i].flow_execution_file, cmd_buffer, 1);
  }
  
  NSDL3_PARSING(NULL, NULL, "cmd after appending script file = [%s]", cmd_buffer);

  strcat(cmd_buffer, " ");
  strcat(cmd_buffer, script_libs_flag);
  sprintf(err_warn_file, "%s/logs/TR%d/.script.log", g_ns_wdir, testidx);
  sprintf(cmd_buffer, "%s 2>%s", cmd_buffer, err_warn_file);
  NSDL3_PARSING(NULL, NULL, "Final cmd for compile script file = %s", cmd_buffer);

  //return_value = system(cmd_buffer);
    return_value = nslb_system(cmd_buffer,1,err_msg); 
  //Checking compilation status, in case of error, all error and warning will be printed on console
  //if(WEXITSTATUS(return_value) == 1)
  if(return_value != 0)
  { 
    SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012014_ID, CAV_ERR_1012014_MSG);
    output_script_error(err_warn_file);
    NS_EXIT(-1, "Failed to compile script.");
  } 
  
  //else if (WEXITSTATUS(return_value) == 0)
  else if(return_value == 0)
  {
    sprintf(cmd_buffer, "%s/%s/%s.so", g_ns_wdir, g_ns_tmpdir, get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "-"));
    NSDL3_PARSING(NULL, NULL, "Shared library file = %s", cmd_buffer);
    
    //loads the dynamic shared object , in case of any error like undefined symbol, will be print on console.  
    handle = dlopen (cmd_buffer, RTLD_NOW);
    if ((error = dlerror()))
    {
      //  If so, print the error message and exit.
      // Sourabh:: BUG 93356:- give proper compilation error of script compilaation fail through shell nsi_compile_script
      if ((err_in_api = strstr(error, "undefined symbol")) != NULL)
      {
        SCRIPT_PARSE_NO_RETURN(NULL, "Error in compiling the script.\nError = %s\n", err_in_api);
        output_script_error(err_warn_file);
        NS_EXIT(-1, "Failed to compile script.\nERROR: %s\n", err_in_api);
      }
      else {
        SCRIPT_PARSE_NO_RETURN(NULL, "Error in compiling the script.\nError = %s\n", error);
        output_script_error(err_warn_file);
        NS_EXIT(-1, "Failed to compile script.\nERROR: %s \n", error);
      }  
    }
    else if (run_mode_option & RUN_MODE_OPTION_COMPILE)
    {
      char script_path[512];
      char cmd[2048];
      //Terminate to thread in case and print error from file if has 
      if (!global_settings->disable_script_validation)
      {
        int *script_val_ret;
        pthread_attr_destroy(&script_val_attr);
        if(pthread_join(script_val_thid, (void *)&script_val_ret) != 0)
        {
          //Error
          NSTL1(NULL, NULL, "Error in joining script validation thread");
        }
        if(*script_val_ret != 0)
        { 
          sprintf(script_path, "%s/webapps/netstorm/temp/script_validation.log", g_ns_wdir);
          FILE *efp = fopen(script_path, "r");
          if(efp)
          {
            while(!feof(efp))
            {
              fread(cmd, 2048, 1, efp);
              fprintf(stderr, "%s", cmd);
            }
            fclose(efp);
            unlink(script_path);
          } 
          SCRIPT_PARSE_ERROR_EXIT(NULL, "Script Parsing Error: Invalid Syntax\n");
        }
      }
      NS_EXIT(0, "Successfully compiled script (%s)", script_name_without_full_path);
    }
  }
  
  gSessionTable[sess_idx].handle = handle;
  // user_test_init and user_test_exit are optional, so do not check for error
  gSessionTable[sess_idx].user_test_init = dlsym(handle, "user_test_init");
  gSessionTable[sess_idx].user_test_exit = dlsym(handle, "user_test_exit");

  int send_tb_idx = 0;
  //For Websocket
  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)
  {
    NSDL3_WS(NULL, NULL, "Websocket protocol_enabled = %x, total_ws_callback_entries = %d", 
                               global_settings->protocol_enabled & WS_PROTOCOL_ENABLED, total_ws_callback_entries);
    for(send_tb_idx = 0; send_tb_idx < total_ws_callback_entries; send_tb_idx++)
    {
      g_ws_callback[send_tb_idx].opencb_ptr = dlsym(handle, "opencb_ptr");
      g_ws_callback[send_tb_idx].msgcb_ptr = dlsym(handle, "msgcb_ptr");
      g_ws_callback[send_tb_idx].errorcb_ptr = dlsym(handle, "errorcb_ptr");
      g_ws_callback[send_tb_idx].closecb_ptr = dlsym(handle, "closecb_ptr");
    }
  } 

  //save runlogic on gset
  runprof_save_runlogic(sess_idx, handle);

  //Bug 51216: flow function definition exists or not
  char flow_func[256 + 1], *ptr;
  for(i = 0; i < file_cnt; i++) {
    ptr = strchr(lst_ptr[i].orig_filename, '.'); //we are adding .c on filelist array
    snprintf(flow_func, (ptr - lst_ptr[i].orig_filename) + 1, "%s", lst_ptr[i].orig_filename);
    check_flow_exist(flow_func, handle, sess_idx);
  }
  // In C script,
  // init_script is not called from netstorm. So no need to keep the function pointer
  // But this is called from runlogic. So check if not present. 
  // We can keep the pointer but not use  it
  gSessionTable[sess_idx].init_func_ptr = dlsym(handle, "init_script");
  if ((error = dlerror())) 
    /* If so, print the error message and exit. */
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012016_ID, CAV_ERR_1012016_MSG, "init_script", "init_script");
  

  gSessionTable[sess_idx].exit_func_ptr = dlsym(handle, "exit_script");
  if ((error = dlerror())) 
    /* If so, print the error message and exit. */
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012016_ID, CAV_ERR_1012016_MSG, "exit_script", "exit_script");

  for (index = gSessionTable[sess_idx].first_page; index < gSessionTable[sess_idx].first_page + gSessionTable[sess_idx].num_pages; index++) {
    init_pre_post_url_callback(index, handle);
  }

  return NS_PARSE_SCRIPT_SUCCESS;
}

/*parsing the string "ns_tr069_" ,if found TR069_PROTOCOL_ENABLED protocol enabled */
int get_protocols_from_flow_files(char *script_filepath, FlowFileList_st *script_filelist, int num_files)
{
  FILE *fp;
  char filename[1024];
  char *ptr;
  int i;
  NSDL2_PARSING(NULL, NULL, "Method Called, script_filepath = %s, num_files = %d", script_filepath, num_files);

  for(i=0;i<num_files;i++)
  {
    sprintf(filename,"%s/%s",script_filepath,script_filelist[i].orig_filename);
    NSDL2_PARSING(NULL, NULL, "Open/Read file [%s]", filename);

    if ((fp = fopen(filename, "r")) == NULL)
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, filename, errno, nslb_strerror(errno));
   
    while(1)  //Parse flow fill till end of file or "}" not received.
    {
      ptr = read_line(fp, NS_DO_NOT_TRIM); // It reads in script_line
      if(ptr == NULL)
      {
        NSDL2_PARSING(NULL, NULL, "read_line returned NULL, continueing ...");
        break;
      }

      NSDL2_PARSING(NULL, NULL, "filename = [%s], script_line = [%s]",
                                 filename, script_line); 
      if((ptr = strstr(script_line, "ns_tr069_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "tr069 api() found in [%s] at script_line %d.\n",
                                   filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of TR069_PROTOCOL_ENABLED
        if(!(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= TR069_PROTOCOL_ENABLED;
        }
        return TR069_PROTOCOL_ENABLED;
      }

      if((ptr = strstr(script_line, "ns_sock_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "ns_sock api() found in [%s] at script_line %d.\n",
                                   filename, script_ln_no);
        fclose(fp);
        if(!(global_settings->protocol_enabled & USER_SOCKET_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= USER_SOCKET_PROTOCOL_ENABLED;
        }
        return USER_SOCKET_PROTOCOL_ENABLED;
      }

      //Setting Bitmask of DOS SYN ATTACK 
      if((ptr = strstr(script_line, "ns_dos_syn_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "ns_dos_syn_attack api() found in [%s] at script_line %d.\n",
                                   filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of DOS_ATTACK_ENABLED
        if(!(global_settings->protocol_enabled & DOS_ATTACK_ENABLED)) {
          global_settings->protocol_enabled |= DOS_ATTACK_ENABLED;
        }
        return DOS_ATTACK_ENABLED;
      }

      //Setting Bitmask of MONGODB Protocol 
      if((ptr = strstr(script_line, "ns_mongodb_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "API start with ns_mongodb_ found in [%s] at script_line %d.\n", filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of DOS_ATTACK_ENABLED
        if(!(global_settings->protocol_enabled & MONGODB_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= MONGODB_PROTOCOL_ENABLED;
        }
        return MONGODB_PROTOCOL_ENABLED;
      }

      //Setting Bitmask of CASSDB Protocol 
      if((ptr = strstr(script_line, "ns_cassdb_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "API start with ns_cassdb_ found in [%s] at script_line %d.\n", filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of CASSDB_PROTOCOL_ENABLED
        if(!(global_settings->protocol_enabled & CASSDB_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= CASSDB_PROTOCOL_ENABLED;
        }
        return CASSDB_PROTOCOL_ENABLED;
      }
      //Setting Bitmask of RTE Protocol 
      if((ptr = strstr(script_line, "NS_RTE_")) || (ptr = strstr(script_line, "ns_rdp_"))) /*bug 79149: ns_rdp_ added*/
      {
        NSDL2_PARSING(NULL, NULL, "API start with ns_rte_ found in [%s] at script_line %d.\n", filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of RTE_PROTOCOL_ENABLED
        if(!(global_settings->protocol_enabled & RTE_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= RTE_PROTOCOL_ENABLED;
        }
        return RTE_PROTOCOL_ENABLED;
      }
      //Setting Bitmask of SOCKJS Protocol 
      if((ptr = strstr(script_line, "ns_sockjs_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "API start with ns_sockjs_ found in [%s] at script_line %d.\n", filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of SOCKJS_PROTOCOL_ENABLED
        if(!(global_settings->protocol_enabled & SOCKJS_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= SOCKJS_PROTOCOL_ENABLED;
        }
        return SOCKJS_PROTOCOL_ENABLED;
      }
      // Setting Bitmask of XMPP_PROTOCOL_ENABLED
      if((ptr = strstr(script_line, "ns_xmpp_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "API start with ns_xmpp_ found in [%s] at script_line %d.\n", filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of XMPP_PROTOCOL_ENABLED
        if(!(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= XMPP_PROTOCOL_ENABLED;
        }
        return XMPP_PROTOCOL_ENABLED;
      }
      // Setting Bitmask of JMS_PROTOCOL_ENABLED
      if((strstr(script_line, "ns_ibmmq_") != NULL) || (strstr(script_line, "ns_kafka_") != NULL) || (strstr(script_line, "ns_tibco_") != NULL) || (strstr(script_line, "ns_amazonsqs_") != NULL))
      {
        NSDL2_PARSING(NULL, NULL, "API start with ns_ibmmq_ or ns_kafka_ or ns_tibco_ found in [%s] at script_line %d.\n", filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of JMS_PROTOCOL_ENABLED
        if(!(global_settings->protocol_enabled & JMS_PROTOCOL_ENABLED)) {
          global_settings->protocol_enabled |= JMS_PROTOCOL_ENABLED;
        }
        return JMS_PROTOCOL_ENABLED;
      }
      
#if 0      
// For future
      //Setting Bitmask of Yahoo protocol 
      if((ptr = strstr(script_line, "ns_ymsg_")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "ns_ymsg_login api() found in [%s] at script_line %d.\n",
                                   filename, script_ln_no);
        fclose(fp);
        // Setting Bitmask of Yahoo protocol
        if(!(global_settings->protocol_enabled & YAHOO_PROTOCAL_ENABLED)) {
          global_settings->protocol_enabled |= YAHOO_PROTOCAL_ENABLED;
        }
        return YAHOO_PROTOCAL_ENABLED;
      }
#endif

    }
    fclose(fp);
  }
  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse registration.spces and create 
 *             corresponding tables.
 *
 * Input     : registration_file   - to provide file in which different parameter exist 
 *                                   Eg - registration.spec and .registration.spec
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int ns_parse_registration_file(char *registration_file, int sess_idx, int *reg_ln_no)
{
  FILE *reg_fp = NULL;

  if ((reg_fp = fopen(registration_file, "r")) == NULL)
  {
    if(errno != ENOENT) // registration file is optinal. So check if any other error
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, registration_file, errno, nslb_strerror(errno));
  }

  // Open registrations.spec and set line_number to 1 
  if(reg_fp != NULL)
  {
    if(read_urlvar_file(reg_fp, sess_idx, reg_ln_no, registration_file) == -1)
    {
      SCRIPT_PARSE_ERROR(NULL, "%s(): Error in read_urlvar_file(%s)\n", (char*)__FUNCTION__, registration_file);
      return NS_PARSE_SCRIPT_ERROR;
    }


    //Create NS var hash function and insert the variables in shm
    if(fclose(reg_fp) != 0)
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000021]: ", CAV_ERR_1000021 + CAV_ERR_HDR_LEN, registration_file, errno, nslb_strerror(errno));
  }

  return NS_PARSE_SCRIPT_SUCCESS; 
}

//Atul 14-Aug-2015
//This function will parse rbu_tti.json file and make json tree
//And save the tree into session table for further use. 
int ns_rbu_parse_tti_json_file(int sess_idx)
{
  char rbu_tti_json_file[1024 +1] = "";
  nslb_json_t *json = NULL;
  nslb_json_error err;

  NSDL2_PARSING(NULL, NULL, "Method called, script_file_path = %d", sess_idx);
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  sprintf(rbu_tti_json_file, "%s/%s/rbu_tti.json", GET_NS_TA_DIR(),
                     get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));
                     //Previously taking with only script name
                     //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)));

  NSDL2_PARSING(NULL, NULL, "rbu_tti_json_file = %s", rbu_tti_json_file);

  //If file rbu_tti.json exist then initialize and make json tree of profiles 
  //and if not then just set rbu_tti_prof_tree to NULL
  if(!access(rbu_tti_json_file, F_OK))
  {
    NSDL2_PARSING(NULL, NULL, "File '%s' exist", rbu_tti_json_file);
    json = nslb_json_init(rbu_tti_json_file, 0, 0, &err);
    if(!json) 
    {
      fprintf(stderr, "Error:  - failed to init json, error = %s\n", err.str);
      NS_DUMP_WARNING("Failed to initilize json due to %s", err.str);
      gSessionTable[sess_idx].rbu_tti_prof_tree = NULL;
      return NS_PARSE_SCRIPT_SUCCESS;
    }
  }
  else
  { 
    fprintf(stderr, "Warning: - failed to open rbu_tti_json_file =  %s, error = %s\n", rbu_tti_json_file, nslb_strerror(errno));
    NS_DUMP_WARNING("Failed to open rbu_tti_json_file '%s' due to %s", rbu_tti_json_file, nslb_strerror(errno));
    gSessionTable[sess_idx].rbu_tti_prof_tree = NULL;
    return NS_PARSE_SCRIPT_SUCCESS; 
  }

  //save json tree into SessionTableEnrty and use later to make request.
  gSessionTable[sess_idx].rbu_tti_prof_tree = nslb_json_to_jsont(json);
  //free json
  nslb_json_free(json);
  if(gSessionTable[sess_idx].rbu_tti_prof_tree == NULL) 
  {
    fprintf(stderr, "Error: ns_rbu_parse_tti_json_file() - Failed to make json tree from file %s", rbu_tti_json_file);
    return NS_PARSE_SCRIPT_ERROR;
  }

  return NS_PARSE_SCRIPT_SUCCESS;
}

/*   Atul: This function will do the following task
 *1) Make tti_prof_root path 
 *2) Then search that path in json tree and return whole node
 *3) finally get json string from json node
 */
long get_tti_profile(char *prof, int url_idx, int sess_idx, int script_ln_no, char *flow_file)
{

  NSDL2_PARSING(NULL, NULL, "Method called, primary_content_profile = %s, url_idx = %d, sess_idx = %d", prof, url_idx, sess_idx);

  nslb_jsont *profiles_tree; //profiles tree it is more specific tree of json tree
  char tti_prof_path[512];
  char profile_value_str[RBU_MAX_TTI_FILE_LENGTH + 1];
  char tti_prof[64] = "";    //tti profile name 

  //For TTI profile name
  strcpy(tti_prof, prof);

  //Storing offset of big_buf
  long prof_big_buf_idx = copy_into_big_buf(tti_prof, strlen(tti_prof));

  if(prof_big_buf_idx == -1)
  {
    NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", tti_prof);
    return NS_PARSE_SCRIPT_ERROR;
  }

  //Storing big_buf_idx to the RBU_Param structure 
  requests[url_idx].proto.http.rbu_param.tti_prof = prof_big_buf_idx;

  //create profile_path
  sprintf(tti_prof_path, "root.%s", prof);
  if(gSessionTable[sess_idx].rbu_tti_prof_tree == NULL)
  {
    fprintf(stderr, "Error: get_tti_profile() - Failed to make json tree.(flow = %s, line = '%d')\n", flow_file, script_ln_no);
    return -1;
  }
  NSDL2_PARSING(NULL, NULL, "tti_prof_path = %s", tti_prof_path);
  profiles_tree = nslb_jsont_eval_expr(gSessionTable[sess_idx].rbu_tti_prof_tree, tti_prof_path);

  NSDL2_PARSING(NULL, NULL, "profiles_tree = %s", profiles_tree);
  
  if(profiles_tree == NULL)
  {
    fprintf(stderr, "Error: get_tti_profile() - Failed to make profile tree from json tree.(flow = '%s', line = '%d')\n", 
                                                                                                      flow_file, script_ln_no);
    return -1;
  }
  //convert profile tree into string.
  if(jsont_to_string(profiles_tree, profile_value_str, RBU_MAX_TTI_FILE_LENGTH, 0, 0, 0) == NULL)
  {
    fprintf(stderr, "Error: get_tti_profile() - Failed to make final string from profile treei.(flow =  '%s', line = '%d')\n", 
                                                                                                      flow_file, script_ln_no);
    return -1;
  }

  //convert profile_value_str into big buf.
  long big_buf_idx = copy_into_big_buf(profile_value_str, strlen(profile_value_str));
  if(big_buf_idx == -1)
  {
    fprintf(stderr, "Error: get_tti_profile() - Failed to make final sting from profile tree.(flow = '%s', line = '%d')\n", 
                                                                                                      flow_file, script_ln_no);
    NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", profile_value_str);
    return NS_PARSE_SCRIPT_ERROR;
  }
  NSDL2_RBU(NULL, NULL, "profile_value_str = '%s', big_buf_idx = '%ld'", profile_value_str, big_buf_idx);
  return big_buf_idx; 
}

static int ns_rte_setup_display(int sess_idx, char *script_filepath)
{
  int gp_idx;
  int reg_ln_no = 0;
  int quantity=0;
  char cmd_buff[1024 + 1];
  char filename[1024 + 1];
  char registration_file[1024 + 1];
  FILE *fp;
  NSDL2_RTE(NULL, NULL, "Method Called");


  sprintf(filename, "%s/%s/.rte_params.csv", g_ns_wdir, script_filepath);
  if((fp = fopen(filename,"w")) == NULL)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, filename, errno, nslb_strerror(errno));
  }
  fclose(fp);

  //Allocate display id and store in .rte_params.csv files
  for(gp_idx = 0; gp_idx < total_runprof_entries; gp_idx++)
  {
     //Skip non rte groups or not rte display group for this script
     if(!runProfTable[gp_idx].gset.rte_settings.enable_rte ||
        !runProfTable[gp_idx].gset.rte_settings.rte.terminal ||
        runProfTable[gp_idx].sessprof_idx != sess_idx)
       continue;

     quantity += runProfTable[gp_idx].quantity;
  }
  if(quantity)
  {
    #ifdef NS_DEBUG_ON
      sprintf(cmd_buff, "nsu_auto_gen_prof_and_vnc -o start -n %d -f %s -t %d -e -D >/dev/null 2>&1" , quantity, filename, testidx);
    #else
      sprintf(cmd_buff, "nsu_auto_gen_prof_and_vnc -o start -n %d -f %s -t %d -e >/dev/null 2>&1" ,quantity, filename, testidx);
    #endif

    NSDL2_RTE(NULL,NULL, "RTE start vnc cmd_buff = %s", cmd_buff);
    system(cmd_buff);
    //Read Registration file for rte display
    sprintf(registration_file,"%s/etc/.rte.spec", g_ns_wdir);
    NSDL2_RTE(NULL,NULL, "parse registration file = %s for session %d", registration_file, sess_idx);
    flow_filename = NS_REGISTRATION_FILENAME;
    if (ns_parse_registration_file(registration_file, sess_idx, &reg_ln_no) == NS_PARSE_SCRIPT_ERROR)
    {
      return NS_PARSE_SCRIPT_ERROR;
    }
  }
  return 0;
}


int parse_script(int sess_idx, char *script_filepath)
{
  char script_filename[MAX_SCRIPT_FILE_NAME_LEN + 1];
  char registration_file[MAX_SCRIPT_FILE_NAME_LEN + 1];
  char file_name[MAX_SCRIPT_FILE_NAME_LEN + 1];
  char script_line_buf[MAX_LINE_LENGTH + 1];
  int file_count, cnt;
  static __thread FlowFileList_st *script_filelist = NULL;
  //FILE *reg_fp;
  int reg_ln_no = 0;
  char flowout_filename[MAX_SCRIPT_FILE_NAME_LEN + 1];
  int proto_enabled;
  int flag_found = 0, i;
  char inline_enabled_for_script = 0; 
  int grp_start_idx = total_group_entries; 
  parse_tti_file_done = 0;
  char err_msg[1024 + 1];  

  // This code is for restrciting host entry gServerTable in case get_no_inline is enabled for the script in all the groups
  // This change is done to fix Bug 17490. Initially we were restricting inline host resolution in insert_default_svr_location. But there is  
  // case when inline host is added in table and it is not resolved and this host comes as a location in a redirected url, unresolved host 
  // will be selected and NS will try to make a connection with that and as host is not resolved, connection will fail.
   
   for(i = 0; i <total_runprof_entries; i++){
   // This script is used in grop i   
   NSDL3_PARSING(NULL, NULL, "runProfTable[i].sessprof_idx = %d, sess_idx = %d", runProfTable[i].sessprof_idx, sess_idx);
   if( runProfTable[i].sessprof_idx == sess_idx){
     NSDL3_PARSING(NULL, NULL, "runProfTable[i].gset.get_no_inlined_obj = %d", runProfTable[i].gset.get_no_inlined_obj);
     if(!runProfTable[i].gset.get_no_inlined_obj){
        inline_enabled_for_script = 1;
       break;
     }
   }
  }

  script_line = script_line_buf; // Set so that we can use global variable in all functions
  g_script_line = script_line_buf; // To reset script_line to initial position while reading line of flow file

  NSDL2_PARSING(NULL, NULL, "Method Called script_filepath = %s", script_filepath);

  flow_filename = "NA"; // Set to "NA" so that we know it is not set yet
  script_name = script_filepath + 10; // Set it so that we can use in script parse error macro

  //Get the list of flow files in specified path
  flow_filename = NS_SCRIPT_TYPE_C? "runlogic.c": "runlogic.java";
  file_count = get_flow_filelist(script_filepath, &script_filelist, sess_idx);
  NSDL2_PARSING(NULL, NULL, "file_count = %d", file_count);
  //if(file_count < 0) {// Error
   // return NS_PARSE_SCRIPT_ERROR;
  //}

  if(file_count == 0) // This is for dummy script  used for server only kind of scenario
  {
    NSTL1(NULL,NULL, "Info: Script %s does not have any flow files", script_filepath);
    NS_DUMP_WARNING("Script %s does not have any flow files", script_filepath);
  }
  /* Netomni Changes:
   * In order to have compatible progress report size at both controller and generator
   * machines. We need to send unused scripts at generators with qty/pct zero in SGRP 
   * keyword. 
   * 
   * Generator side:
   * Therefore if we send a script having any varibles (eg. file parameter) with 
   * number of users 0 then in such case we will ignore parsing its registration.spec 
   * file.
   * Above changes were required bec in netomni we allow SGRP groups with qty 0 at 
   * generator side.
   * 
   * Stand Alone:
   * In netstorm we support zero qty/pct in SGRP keyword, but no changes required  
   * becoz such groups are already ignore at scenario parsing.
   * */ 
  NSDL2_PARSING(NULL, NULL, "loader_opcode = %d", loader_opcode);

  if (loader_opcode == CLIENT_LOADER) 
  {
    for (i = 0; i < total_runprof_entries; i++) 
    {
      if (!strcmp(RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[i].sessprof_idx].sess_name), RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name))) 
      {
        NSDL4_PARSING(NULL, NULL, "For SGRP group index %d quantity is equal to %d", i, runProfTable[i].quantity);
        /* NOTE: In CLIENT_LOADER only NUM mode is supported therefore verfying only quantity field of runProfTable*/
        if (runProfTable[i].quantity) 
        {
          NSDL4_PARSING(NULL, NULL, "Scenario group(%s)with quantity greater than zero", RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
          flag_found = 1;
          break;
        }
      }
    }
  } 
  else //(STAND ALONE)
    flag_found = 1;

  if (flag_found) {
    NSDL2_PARSING(NULL, NULL, "flag_found = %d is set then parse registration_file for the group", flag_found);
    sprintf(registration_file, "%s/%s", script_filepath, NS_REGISTRATION_FILENAME);
    write_log_file(NS_SCENARIO_PARSING, "Validating script parameters of %s", RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
    u_ns_ts_t start_ts = time(NULL);
    flow_filename = NS_REGISTRATION_FILENAME;
    if (ns_parse_registration_file(registration_file, sess_idx, &reg_ln_no) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    write_log_file(NS_SCENARIO_PARSING, "Parameters parsing took %llu secs time", time(NULL) - start_ts);
    /* RBU - parse RBU registration file only if and onlu if there is one group and that group have RBU script */
    NSDL2_PARSING(NULL, NULL, "total_runprof_entries = %d, global_settings->protocol_enabled = %d", 
                                total_runprof_entries, global_settings->protocol_enabled & RBU_API_USED);

    //RBU Automation 
    if((global_settings->protocol_enabled & RBU_API_USED) && (loader_opcode != MASTER_LOADER) && (global_settings->rbu_enable_auto_param))
    {
      flow_filename = RBU_REGISTRATION_FILENAME;
      reg_ln_no = 0;
      write_log_file(NS_SCENARIO_PARSING, "Validating RBU script parameters");
      if (ns_rbu_parse_registrations_file(script_filepath, sess_idx, &reg_ln_no) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }

  }

/*  
  flow_filename = NS_SCRIPT_TYPE_C? NS_INIT_SCRIPT_FILENAME: NS_INIT_SCRIPT_FILENAME_JAVA;
  if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA)
    sprintf(file_name, "%s/%s", script_filepath, NS_INIT_SCRIPT_FILENAME_JAVA);
  else
    sprintf(file_name, "%s/%s", script_filepath, NS_INIT_SCRIPT_FILENAME);
  write_log_file(NS_SCENARIO_PARSING, "Parsing transaction APIs");
  parse_tx_from_file(file_name , sess_idx);
  
  flow_filename = NS_SCRIPT_TYPE_C? NS_EXIT_SCRIPT_FILENAME: NS_EXIT_SCRIPT_FILENAME_JAVA;
  if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA){
    sprintf(file_name, "%s/%s", script_filepath, NS_EXIT_SCRIPT_FILENAME_JAVA);
  }else{
    sprintf(file_name, "%s/%s", script_filepath, NS_EXIT_SCRIPT_FILENAME);
  }
 
  parse_tx_from_file(file_name, sess_idx);
*/

  write_log_file(NS_SCENARIO_PARSING, "Reading protocols from flow files");
  proto_enabled = get_protocols_from_flow_files(script_filepath, script_filelist,file_count);

  NSDL2_PARSING(NULL, NULL, "proto_enabled = 0x%x", proto_enabled);

  /*We need to parse tr069_registrations.spec only if TR069_PROTOCOL_ENABLED is enabled */
  if(proto_enabled & TR069_PROTOCOL_ENABLED) {
    NSDL2_PARSING(NULL, NULL, "Parsing tr069_registration vars");
    read_tr069_registration_vars(sess_idx);
  }

   //RTE Automation
  if(proto_enabled & RTE_PROTOCOL_ENABLED)
  {
    write_log_file(NS_SCENARIO_PARSING, "Setting Remote Terminal Emulator display");
    if (ns_rte_setup_display(sess_idx, script_filepath) != 0)
    {
      NSDL2_RTE(NULL,NULL,"Returned from Method ns_rte_setup_display with error");
      NS_EXIT(-1,"ns_rte_setup_display failed");
    }
  }

  // Here we are parsing all the flow files for ns_start_timer API, as this api need to be parsed before creating the hash of variables
  // We are adding a declare variable for saving timer in ns_start_timer API  
  for (cnt = 0; cnt < file_count; cnt++)
  {
    sprintf(script_filename, "%s/%s", script_filepath, script_filelist[cnt].orig_filename);
    script_ln_no = 0;
    flow_filename = script_filelist[cnt].orig_filename; // Point to flow file name (e.g flow1.c)

    // Create file name this will be used in making of so or class file based on script type
    if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA){
      sprintf(script_filelist[cnt].flow_execution_file, "%s", script_filelist[cnt].orig_filename); 
    }else{
      sprintf(script_filelist[cnt].flow_execution_file, ".%s", script_filelist[cnt].orig_filename);
    }
    sprintf(flowout_filename, "%s/%s", g_ns_tmpdir, script_filelist[cnt].flow_execution_file); 
 
    NSDL2_PARSING(NULL, NULL, "Starting parsing of script = %s. Flow file (%s), flow .....",  
                                                          script_filepath, script_filelist[cnt].orig_filename);

    #ifdef NS_DEBUG_ON
      //fprintf(stdout, "Parsing Flow File <%s>\n", script_filelist[cnt].orig_filename);
    #endif
    write_log_file(NS_SCENARIO_PARSING, "Validating flow file %s (%d out of %d)", script_filelist[cnt].orig_filename, cnt + 1, file_count);
    if (parse_flow_file_for_api(script_filename, flowout_filename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }
  write_log_file(NS_SCENARIO_PARSING, "Creating script parameter table and checking redundancy of parameter");
  create_variable_functions(sess_idx);

  //TODO: #Ayush: 
  int g,v,pointer_idx;
  NSDL4_SCHEDULE(NULL, NULL, "grp_start_idx = %d total_group_entries = %d", grp_start_idx, total_group_entries);
  for(g = grp_start_idx ; g < total_group_entries; g++)
  {
    if (groupTable[g].index_var != -1)
      continue;
    for(v = 0; v < groupTable[g].num_vars; v++)
    {
      NSDL4_SCHEDULE(NULL, NULL, "is_file = %d", varTable[groupTable[g].start_var_idx + v].is_file);
      if(varTable[groupTable[g].start_var_idx + v].is_file == IS_FILE_PARAM)
      { 
        //StrEnt seg_value = {0};
        for(i = 0; i < groupTable[g].num_values; i++)
        {
          StrEnt seg_value = {0};
          NSDL4_SCHEDULE(NULL, NULL, "pointer-> i = %d, v = [%d], start_var_idx = %d, value_start_ptr = %d", 
                                i, v, groupTable[g].start_var_idx, varTable[groupTable[g].start_var_idx + v].value_start_ptr);

          pointer_idx = varTable[groupTable[g].start_var_idx + v].value_start_ptr  + i; 

          NSDL4_SCHEDULE(NULL, NULL, "Fparam Segmentation: pointer_idx = %d, data = %s", 
                                      pointer_idx, fparamValueTable[pointer_idx].big_buf_pointer);

          segment_line(&seg_value, (char *)fparamValueTable[pointer_idx].big_buf_pointer, 0, -1, sess_idx, NULL);

          FREE_AND_MAKE_NOT_NULL_EX((char *)fparamValueTable[pointer_idx].big_buf_pointer, fparamValueTable[pointer_idx].size, "Fparam buffer for type 2", -1);
         
          fparamValueTable[pointer_idx].num_entries = seg_value.num_entries; 
          fparamValueTable[pointer_idx].seg_start = seg_value.seg_start;
        
          NSDL4_SCHEDULE(NULL, NULL, "pointer_idx = %d, .num_entries = %d, seg_start = %p", 
                                      pointer_idx, fparamValueTable[pointer_idx].num_entries, fparamValueTable[pointer_idx].seg_start);
        }
      }
    }
  }
  
  //Page Ids are reset after session
  init_web_url_page_id();

  //For each flow files
  for(cnt = 0; cnt < file_count; cnt++)
  {
    page_num_relative_to_flow = -1;
    sprintf(script_filename, "%s/%s", script_filepath, script_filelist[cnt].orig_filename);
    //sprintf(script_filename, "%s/%s", g_ns_tmpdir, script_filelist[cnt].file_withoutcomments);
   
    script_ln_no = 0;
    flow_filename = script_filelist[cnt].orig_filename; // Point to flow file name (e.g flow1.c)

    //Changes in 4.1.13: flow file name will be same as flow file name not .flow.c on path $NS_WDIR/.tmp/cavisson/inst0/   
#if 0 
    // Create file name this will be used in making of so or class file based on script type
    if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA){
      sprintf(script_filelist[cnt].flow_execution_file, "%s", script_filelist[cnt].orig_filename); 
    }else{
      sprintf(script_filelist[cnt].flow_execution_file, ".%s", script_filelist[cnt].orig_filename);
    }
#endif
    sprintf(script_filelist[cnt].flow_execution_file, "%s", script_filelist[cnt].orig_filename); 
    sprintf(flowout_filename, "%s/%s", g_ns_tmpdir, script_filelist[cnt].flow_execution_file); 
 
    NSDL2_PARSING(NULL, NULL, "Starting parsing of script = %s. Flow file (%s), flow .....",  
                                                          script_filepath, script_filelist[cnt].orig_filename);

    #ifdef NS_DEBUG_ON
      //fprintf(stdout, "Parsing Flow File <%s>\n", script_filelist[cnt].orig_filename);
    #endif
    //first_flow_page = 1;
    if (parse_flow_file(script_filename, flowout_filename, sess_idx, inline_enabled_for_script) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  flow_filename = "NA"; // Set to "NA" so that we know it is not set yet

  arrange_pages(sess_idx);  
  
  if (process_checkpoint_table_per_session(sess_idx, err_msg) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR(NULL, "Error in processing the checkpoint table, error: %s", err_msg);

  /* process the check reply size for per session */
  if (process_check_replysize_table_per_session(sess_idx, err_msg) == -1)
    SCRIPT_PARSE_ERROR(NULL, "Error in processing the checkreplysize for per session table, error: %s", err_msg);

  // Now all flow files are done with the parsing so create script pointer
  if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA){
    if(create_classes_for_java_type_script(sess_idx, script_filepath, flowout_filename, file_count, script_filelist) == NS_PARSE_SCRIPT_ERROR)
    {
      SCRIPT_PARSE_ERROR(NULL, "Unable to create class files for script [%s]\n", script_filepath);
      return NS_PARSE_SCRIPT_ERROR;
    }
  }else if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_C){
    if(create_page_script_ptr_for_script_type_c(sess_idx, script_filepath, script_filelist, file_count) == NS_PARSE_SCRIPT_ERROR) 
    {
      // SCRIPT_PARSE_ERROR(NULL, "Unable to create shared object for script [%s]", script_filepath);
      return NS_PARSE_SCRIPT_ERROR;
    }
  }

  // TODO: Try to reuse for all scripts and then free it.
  if(file_count > 0)
    FREE_AND_MAKE_NULL_EX(script_filelist, sizeof(FlowFileList_st), "Freeing Flow File Structure", -1);

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

int get_group_idx(int sess_idx)
{
  int grp_idx = -1, i;
  NSDL2_PARSING(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);
  for (i = 0; i < total_runprof_entries; i++) {
    if (runProfTable[i].sessprof_idx == sess_idx) {
      grp_idx = i;
      break;
    }
  }
  return grp_idx;
}

void update_default_runlogic(int sess_idx, char* runlogic)
{
  int i;
  NSDL2_PARSING(NULL, NULL, "Method Called, runlogic = [%s]", runlogic);
  for (i = 0; i < total_runprof_entries; i++)
  {
    if (runProfTable[i].sessprof_idx == sess_idx && (!strcmp(runProfTable[i].runlogic, "default") || !runProfTable[i].runlogic[0]))
    {
      strcpy(runProfTable[i].runlogic, runlogic);
      NSDL2_PARSING(NULL, NULL,"runProfTable[%d].runlogic = %s ", i, runProfTable[i].runlogic);
    }
  }
}

int get_runlogic_format(char *script_filepath)
{
  char path[1024+1];
  struct stat sdir;
  NSDL2_PARSING(NULL, NULL, "Method Called, script_filepath = [%s]", script_filepath);
  //TODO:Ayush need to add debug log 
  snprintf(path, 1024, "%s/runlogic", script_filepath);
  if(stat(path, &sdir))
  {
    //TODO:Ayush need to add debug log
    NSDL2_PARSING(NULL, NULL,"path =[%s]" ,path);
    return 0;
  }
  return S_ISDIR(sdir.st_mode);
}

int get_script_type(char *path, int *script_type, char *version, int sess_idx)
{
  FILE *fp;
  char *field[8];
  char scripttype_fname[MAX_LINE_LENGTH + 1];
  char cap_fname[MAX_LINE_LENGTH + 1];
  char read_line[MAX_LINE_LENGTH + 1];
  int num_toks;
  int grp_idx;
  int set_default_runlogic = 1;

  NSDL2_PARSING(NULL, NULL, "Method Called, path = [%s] sess_idx = [%d]", path, sess_idx);
 
  *script_type = NS_SCRIPT_TYPE_C; // Default will now be c type

  //get format and set format flag
  if(get_runlogic_format(path))
    gSessionTable[sess_idx].flags |= ST_FLAGS_SCRIPT_NEW_FORMAT;
  else
    gSessionTable[sess_idx].flags |= ST_FLAGS_SCRIPT_OLD_FORMAT;

  sprintf(scripttype_fname , "%s/.script.type", path);
  if((fp = fopen(scripttype_fname, "r")) != NULL)
  {
    while(nslb_fgets(read_line, sizeof(read_line), fp, 1) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "line = [%s]", read_line);
 
      if (read_line[strlen(read_line) -2] == '\r' && read_line[strlen(read_line) -1] == '\n')
        read_line[strlen(read_line) - 2] = '\0';
      else if (read_line[strlen(read_line) -1] == '\n')
        read_line[strlen(read_line) - 1] = '\0';
 
      NSDL2_PARSING(NULL, NULL, "line = [%s]", read_line);

      // Ignore Empty & commented lines
      if(read_line[0] == '#' || read_line[0] == '\0') {
        NSDL2_PARSING(NULL, NULL, "Commented/Empty line continuing..");
        continue;
      }

      num_toks = get_tokens(read_line, field, "=", 8);
      NSDL2_PARSING(NULL, NULL, "num_toks = %d", num_toks);
 
      if(num_toks < 2) {
       NSDL2_PARSING(NULL, NULL, "num_toks < 2, continuing...");
       // No keyword value -- Give warning
       continue;
      }

      NSDL2_PARSING(NULL, NULL, "field[0] = [%s], field[1] = [%s]", field[0], field[1]);
      // field[0] is keyword & field[1] has its value
      if(strcasecmp(field[0], "SCRIPT_TYPE") == 0)
      {
        if(strcasecmp(field[1], "C") == 0)
         *script_type = NS_SCRIPT_TYPE_C;
        else if(strcasecmp(field[1], "JAVA") == 0)
         *script_type = NS_SCRIPT_TYPE_JAVA;
      }

      if(strcasecmp(field[0], "SCRIPT_VERSION") == 0) {
        strcpy(version, field[1]);
      }

      if(strcasecmp(field[0], "RUNLOGIC") == 0)
      {
        set_default_runlogic = 0;
        update_default_runlogic(sess_idx, field[1]);
      }
    }
    if(set_default_runlogic)
      update_default_runlogic(sess_idx, "runlogic"); 
    fclose(fp);
  }
  else
  {
    //Auto detect script type if .script.type file is not present If script.capture is found set it to legacy mode
    char *tmp_ptr = cap_fname;

    update_default_runlogic(sess_idx, "runlogic");
    if (gSessionTable[sess_idx].flags & ST_FLAGS_SCRIPT_NEW_FORMAT)
    {
      grp_idx = get_group_idx(sess_idx);
      sprintf(tmp_ptr, "%s/runlogic/%s.java", path, runProfTable[grp_idx].runlogic);
    }
    else
    {
      // Removing support for legacy type script here.
      sprintf(tmp_ptr, "%s/runlogic.java", path);
    }
    if(!access(tmp_ptr, R_OK))
      *script_type = NS_SCRIPT_TYPE_JAVA;
    else
      *script_type = NS_SCRIPT_TYPE_C;
  }

  return NS_PARSE_SCRIPT_SUCCESS;
}

void skip_test_metrices()
{
  /*********************************************************************************************************
    Code to set flag skip_test_metics if total_page_entries & total_tx_entries & total_request entries is 0
  ***********************************************************************************************************/
  if(enable_test_metrics_mode == 0) // AUTO Mode for Test Metrics
  {
    if((total_page_entries || total_tx_entries || total_request_entries || g_dont_skip_test_metrics || global_settings->protocol_enabled) == 0)
      g_dont_skip_test_metrics = 0;
    else
      g_dont_skip_test_metrics = 1;
  }
  else
    g_dont_skip_test_metrics = 1;

}
