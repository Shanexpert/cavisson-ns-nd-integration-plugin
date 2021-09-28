//SM, 12-05-2014: Added handling for PAGELOADWAITTIME

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <libgen.h>

#include "util.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_script_parse.h"
#include "ns_click_script_parse.h"

static int last_argument = 0;
static char *ln_ptr;
#ifndef CAV_MAIN
extern char *script_line;
extern int script_ln_no;
#else
extern __thread char *script_line;
extern __thread int script_ln_no;
#endif
static char last_url[64*1024] = "\0"; /* If a click api does not have a URL attribute, last */
                                   /* main page URL is saved in this variablei */

char *attribute_name[NUM_ATTRIBUTE_TYPES]={
/* Any change should be done to enum attribute_type also defined in url.h*/
"APINAME",
"ALT",
"TAG",
"VALUE",
"TYPE",
"ID",
"ORDINAL",
"ACTION",
"ONCLICK",
"NAME",
"CONTENT",
"SHAPE",
"COORDS",
"TITLE",
"SRC",
"TEXT",
"HREF",
"URL",
"XPATH",
"XPATH1",
"XPATH2",
"XPATH3",
"XPATH4",
"XPATH5",
"HARFLAG",
"IFRAMEID",
"IFRAMEXPATH",
"IFRAMEXPATH1",
"IFRAMEXPATH2",
"IFRAMEXPATH3",
"IFRAMEXPATH4",
"IFRAMEXPATH5",
"IFRAMECLASS",
"IFRAMEDOMSTRING",
"IFRAMECSSPATH",
"PAGELOADWAITTIME",
"DOMSTRING",
"FOCUSONLY",  //to simulate mouseover event
"MERGEHARFILES",
"SCROLLPAGEX",
"SCROLLPAGEY",
"CSSPATH",
"CSSPATH1",
"HarLogDir",
"BrowserUserProfile",
"VncDisplayId",
"HarRenameFlag",
"ScrollPageX",
"ScrollPageY",
"PrimaryContentProfile",
"SPAFRAMEWORK",
"COOKIES",
"CLIPINTERVAL",
"CLASS",
"WaitForNextReq",
"WaitForActionDone",
"WaitUntil",
"PhaseInterval",
"PerformanceTraceLog",
"AuthCredential",
"COORDINATES",
"propertyName",
"propertyName1",
"valueType",
"abortTest",
"OPERATOR",
"AUTOSELECTOR"
};


struct click_api_argument 
{
  char name[MAX_LINE_LENGTH];
  char value[MAX_LINE_LENGTH];
};


static int create_click_action_table_entry(int *row_num)
{

  int i;

  NSDL2_PARSING(NULL, NULL, "Method called");
  if (total_clickaction_entries == max_clickaction_entries)
  {
    MY_REALLOC_EX(clickActionTable, (max_clickaction_entries + DELTA_CLICKACTION_ENTRIES) * sizeof(ClickActionTableEntry), (max_clickaction_entries * sizeof(ClickActionTableEntry)), "clickActionTable", -1); 
    if (!clickActionTable)
    {
      fprintf(stderr, "create_click_action_table_entry(): Error allcating more memory for click action entries\n");
      return FAILURE;
    } else max_clickaction_entries += DELTA_CLICKACTION_ENTRIES;
  }
  *row_num = total_clickaction_entries++;

  for(i=0; i<NUM_ATTRIBUTE_TYPES;i++)
  {
    clickActionTable[*row_num].att[i].seg_start = -1;
    clickActionTable[*row_num].att[i].num_entries = 0;
  }

  return (SUCCESS);
}


#ifdef NS_DEBUG_ON
char *attributes2str(char **att, char *msg)
{
  int i;

  msg[0] = '\0';

  for(i=0; i<NUM_ATTRIBUTE_TYPES; i++)
    if(att[i] !=NULL)
      sprintf(msg, "%s \n    %s: %p '%s'", msg, attribute_name[i], att[i], att[i]);
    else
      sprintf(msg, "%s \n    %s: NULL", msg, attribute_name[i]);

  return msg;
}
#endif

int free_attributes_array (char **att)
{
  int i;

#ifdef NS_DEBUG_ON
  char debug_msg[64*1024];
#endif

  NSDL2_PARSING(NULL, NULL, "Method Called, ATTRIBUTES: %s", attributes2str(att, debug_msg));

  for (i=0; i<NUM_ATTRIBUTE_TYPES; i++)
    FREE_AND_MAKE_NULL_EX(att[i], strlen(att[i]), "attribute", 0);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int set_rbu_param_for_ns_browser(char **att, char *flow_file, int url_idx, int sess_idx, int script_ln_no)
{
  #ifdef NS_DEBUG_ON
  char debug_msg[4096 + 1];
  #endif

  NSDL2_PARSING(NULL, NULL, "Method called, att = %p, flow_file = %s, url_idx = %d, sess_idx = %d, script_ln_no = %d", 
                                            att, flow_file, url_idx, sess_idx, script_ln_no);

  NSDL2_PARSING(NULL, NULL, "%s", attributes2str(att, debug_msg));

  if(att[BrowserUserProfile]) /*BrowserUserProfile */
  {
    if(set_rbu_param(att[BrowserUserProfile], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_BROWSER_USER_PROFILE) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[PAGELOADWAITTIME]) /*PageLoadWaitTile */
  {
    if(set_rbu_param(att[PAGELOADWAITTIME], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_PAGE_LOAD_WAIT_TIME) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[VncDisplayId]) /* VncDisplayId */
  {
    if(set_rbu_param(att[VncDisplayId], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_VNC_DISPLAY_ID) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[MERGEHARFILES]) /* MergeHarFile */
  {
    if(set_rbu_param(att[MERGEHARFILES], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_MERGE_HAR_FILES) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[HarRenameFlag]) /* HarRenameFlag */
  {
    if(set_rbu_param(att[HarRenameFlag], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_HAR_RENAME_FLAG) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[HarLogDir]) /* HarLohDir */
  {
    if(set_rbu_param(att[HarLogDir], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_HAR_LOG_DIR) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[ScrollPageX]) /* ScrollPageX */
  {
     if(set_rbu_param(att[ScrollPageX], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_SCROLL_PAGE_X) == NS_PARSE_SCRIPT_ERROR)
       return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[ScrollPageY]) /* ScrollPageY */
  {
    if(set_rbu_param(att[ScrollPageY], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_SCROLL_PAGE_Y) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[PrimaryContentProfile]) /* PrimaryContentProfile */
  {
    if(set_rbu_param(att[PrimaryContentProfile], flow_file, url_idx, sess_idx, script_ln_no, PrimaryContentProfile) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }                                                                                                                             

  if(att[WaitForNextReq]) /* WaitForNextReq */
  {
    if(set_rbu_param(att[WaitForNextReq], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_WAIT_FOR_NEXT_REQ) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }
 
  if(att[PerformanceTraceLog]) /* PerformanceTraceLog */
  {
    if(set_rbu_param(att[PerformanceTraceLog], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_PERFORMANCE_TRACE) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[AuthCredential]) /* AuthCredential */
  {
    if(set_rbu_param(att[AuthCredential], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_AUTH_CREDENTIAL) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  if(att[PhaseInterval]) /* PhaseInterval */
  {
    if(set_rbu_param(att[PhaseInterval], flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_PHASE_INTERVAL) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }
  return 0;
}

static int set_ca_headers(char* http_hdr, char *all_http_hdrs) 
{
   char temp_buf[2*1024] = {0};

   NSDL2_PARSING(NULL, NULL, "Method called. http_hdr = %s, all_http_hdrs = %s", http_hdr, all_http_hdrs);

  // User Agent is filled by netsorm based on the profile used. So ignore it
  if ((strncasecmp(http_hdr, "User-Agent:", strlen("User-Agent:")) == 0) && (!(group_default_settings->disable_headers & NS_UA_HEADER)))
    return 0;

  // Conetent-Length header, if allowed can only be put by NetStorm
  if (strncasecmp(http_hdr, "Content-Length:", strlen("Content-Length:")) == 0)
    return 0;

  // Connection header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Connection:", strlen("Connection:")) == 0) && (!(group_default_settings->disable_headers & NS_CONNECTION_HEADER)))
    return 0;

  // Keep-Alive header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Keep-Alive:", strlen("Keep-Alive:")) == 0) && (!(group_default_settings->disable_headers & NS_KA_HEADER)))
    return 0;

  // Accept header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Accept:", strlen("Accept:")) == 0) && (!(group_default_settings->disable_headers & NS_ACCEPT_HEADER)))
    return 0;

  // Accept-Encoding header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Accept-Encoding:", strlen("Accept-Encoding:")) == 0) && (!(group_default_settings->disable_headers & NS_ACCEPT_ENC_HEADER)))
    return 0;

  // If-Modified-Since header, if allowed can only be put by NetStorm (For caching)
  if ((strncasecmp(http_hdr, "If-Modified-Since:", strlen("If-Modified-Since:")) == 0))
    return 0;
  
  // If-None-Match header, if allowed can only be put by NetStorm (For caching)
  if ((strncasecmp(http_hdr, "If-None-Match:", strlen("If-None-Match:")) == 0))
    return 0; 
  
  //Host header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Host:", 5) == 0) && (!(group_default_settings->disable_headers & NS_HOST_HEADER))) 
    return 0;
  
  strcpy(temp_buf, all_http_hdrs);
  // Concat http header with \r\n
  snprintf(all_http_hdrs, 2*1024, "%s%s\r\n", temp_buf, http_hdr);

  NSDL2_PARSING(NULL, NULL, "Exiting Method. http_hdr = %s, all_http_hdrs = %s", http_hdr, all_http_hdrs);
  return 1;
}

/* add_click_action_entry() will delete the att[] array. So don't access it after callng this method. */
static int add_click_action_entry(char **att, int sess_idx, char *flow_file_name, int *p_ca_idx)
{
  int i;

#ifdef NS_DEBUG_ON
  char debug_msg[64*1024];
#endif

  NSDL2_PARSING(NULL, NULL, "Method Called, ATTRIBUTES: %s", attributes2str(att, debug_msg));

  create_click_action_table_entry(p_ca_idx);
  
  for(i=0; i<NUM_ATTRIBUTE_TYPES; i++)
    segment_line(&(clickActionTable[*p_ca_idx].att[i]), att[i], 0, script_ln_no, sess_idx, flow_file_name);

  //Shibani: we need att for function set_rbu_param_for_ns_browser() so freeing att after calling this function 
  //free_attributes_array(att);

  return NS_PARSE_SCRIPT_SUCCESS;

}

/****************************************************
 * Function populate_attributes_array
 * Popultaes one element of attributes array att[]
 * **************************************************/
static void populate_attributes_array(struct click_api_argument argument, char **att)
{
  NSDL4_PARSING(NULL, NULL, "Method Called. argument.name = %s, argument.value = %s", argument.name, argument.value);
  char *tmp_ptr = NULL;
  int i;

  if(!strncasecmp(argument.name,"linkurl", 7) || !strncasecmp(argument.name,"browserurl", 10))
    strcpy(argument.name, "href");

  if(!strncasecmp(argument.name,"reloadHar", 9))
    strcpy(argument.name, "HARFLAG");
  
  if(!strncasecmp(argument.name,"tagName", 7))
    strcpy(argument.name, "TAG");

  for (i = 0; i<NUM_ATTRIBUTE_TYPES; i++){

    if (!strncasecmp(argument.name, attribute_name[i], strlen(argument.name))){
      MY_MALLOC(tmp_ptr, strlen(argument.value)+1, "click api attribute",0);
      strcpy(tmp_ptr, argument.value);
      tmp_ptr[strlen(argument.value)] = '\0';

      NSDL4_PARSING(NULL, NULL, "setting attributes array element: argument.name = %s, attribute_name[i] = %s,  tmp_ptr = %p, '%s'", argument.name, attribute_name[i], tmp_ptr, tmp_ptr);
      
      att[i] = tmp_ptr;
      break;
    }
  }

  if (i == NUM_ATTRIBUTE_TYPES) /* Unknown attribute */
    NSDL4_PARSING(NULL, NULL, "Unknown attribute name '%s', Ignoring", argument.name);

}

/**********************************************************************************
 * Function Name : read_stepname()
 *     Arguments : stepname       - Output argument. Contains the parsed first argument 
 *                                  of click api which is the stepname (main page url name).
 *                 clickscript_fp - Input argument. File pointer of click script file.
 *       Returns : type int, 
 *                 NS_PARSE_SCRIPT_ERROR - in case there was some error parsing the script.
 *                 NS_PRSE_SCRIPT_SUCCESS - in case function executed successfully.
 *      Synopsis : Reads the first argument of the click and script API.
 *                 Ex:
 *                 ns_browser (
 *                      "MyHomePage",
 *                      "browserurl=http://cavisson.com"
 *                 );
 *                 In this case, stepname will be filled with MyHomePage text string.
 *                 It is assumed that the within quotes, there will be no line breaks.
 *                 However, quotes may appear if escaped with backslash
 *                 ex. My\"New\"HomePage
 ***********************************************************************************/

static int read_stepname(char *stepname, FILE* clickscript_fp, FILE* outfp)
{
  char *start_idx, *end_idx;

  NSDL2_PARSING(NULL, NULL, "Method Called");

  while (1)
  {
 
   /* Checking for Starting quotes before pagename */
    start_idx = strchr(ln_ptr, '"');
    if (start_idx == NULL)
    {
        if(NULL == read_line_and_comment(clickscript_fp, outfp)){ /* It reads in script_line */
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, "pagename");  
        }
        else 
        {
          ln_ptr = script_line;
          continue;
        }
    } else {
      ++start_idx;  /* Adding 1 for " */
      break;
    }
  }
  
  end_idx = start_idx;

  while(1) /* Search for ending quotes */
  {
    end_idx = strchr(end_idx, '"');
    if(end_idx == NULL)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012190_ID, CAV_ERR_1012190_MSG);

    if(*(end_idx-1) == '\\') /* quote may be part of the string we are reading. In that case it is assumed to be escaped with backslash */
    {
      end_idx++;
      continue;
	}
    break;
  }

  /* Checking if Pagename not received, e.g "" */
  /* Assuming that the pagename will be on single line */
  CLEAR_WHITE_SPACE(start_idx);
  int len = end_idx - start_idx;
  if(len <= 0){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, "MainPagename");
  }
  strncpy(stepname, start_idx, len);
  stepname[len] = '\0';

  CLEAR_WHITE_SPACE_FROM_END(stepname);

  //Checking if Pagename not received e.g. "  "
  if(strlen(stepname) <= 0){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, "MainPagename");
  }
   NSDL2_PARSING(NULL, NULL, "MainPagename '%s' received at script_line %d", stepname, script_ln_no);

  ln_ptr = end_idx + 1;
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int check_is_url(char *ln_ptr)
{
  char *tt;
  int url = 0;
  if((tt = strstr(ln_ptr, "url")) != NULL) 
  {
    tt +=3;
    CLEAR_WHITE_SPACE(tt);
    if(*tt == '=')
      url = 1; 
  }
  return url;
}

static int read_argument(struct click_api_argument *argument, FILE *clickscript_fp, FILE *outfp)
{
  char argstr[MAX_LINE_LENGTH +1];
  char *start_idx, *end_idx;
  int multiline_comment = 0, comma_found = 0, len=0;
  int do_not_enter_open_bracket = 0;
  static int nested_attributes = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called, ln_ptr = %p, '%s' script_line = '%s'", ln_ptr, ln_ptr, script_line);

  while(1)
  {

    NSDL4_PARSING(NULL, NULL, "Inside while(1), ln_ptr = %p, '%s' script_line = '%s'", ln_ptr, ln_ptr, script_line);

    if (*ln_ptr == '\0' || *ln_ptr == '\n') /* End of line reached */
    {
      if(NULL == read_line_and_comment(clickscript_fp, outfp)){ /* It reads in script_line */
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012192_ID, CAV_ERR_1012192_MSG, "argument", "click");
      }
      else 
      {
        ln_ptr = script_line;
        continue;
      }
      
    } /* if *ln_ptr == '\0' */

    else if(multiline_comment)
    {
      if(strncmp(ln_ptr, "*/", 2) == 0)
      {
        NSDL3_PARSING(NULL, NULL, "End of multi-line comment");
        ln_ptr += 2;
        multiline_comment = 0;
        continue;
      }
      ln_ptr++; /* current character is a part of comment, ignore  */
      continue;
    }
    /* Handling starting of multi-line comments */
    else if(strncmp(ln_ptr, "/*", 2) == 0)
    {
      NSDL3_PARSING(NULL, NULL, "Start of multi-line comment");
      ln_ptr = ln_ptr + 2;
      multiline_comment = 1;
      continue;
    } 

    else if (isspace(*ln_ptr)) /* Ignore any white space here */
    {
      ln_ptr++; 
      continue;
    }
    

    else if(*ln_ptr == ',')
    { 
      NSDL3_PARSING(NULL, NULL, "Comma found");
      comma_found = 1;
      ln_ptr++;
      continue;
    }

    /* Handle single line comments */
    else if(strncmp(ln_ptr, "//", 2) == 0)
    {
      NSDL3_PARSING(NULL, NULL, "single-line comment found");
      *ln_ptr = '\0';
      continue;
    }
    else if (!check_is_url(ln_ptr) && strchr(ln_ptr, '[') != NULL && !do_not_enter_open_bracket)
    /* Assuming that 'attributes=[' will be on single line  */
    {
      
      char *tt;
      tt = strchr(ln_ptr, '[') +1;
      CLEAR_WHITE_SPACE(tt);
      if(*tt != '\"' && *tt != ']')
      {
        /* If this is the case, [ is part of an attribute withing quotes 
         * This will be the case when we are already pass the [ with attributes=[ */
 
        do_not_enter_open_bracket = 1;
        /* This will be reset only all the args in attributes= are parsed */
      } else {
        if ((start_idx = strstr(ln_ptr, "attributes")) == NULL)
        {
          if(!nested_attributes)
            SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012194_ID, CAV_ERR_1012194_MSG, "[");
        }
        nested_attributes++; // To maintain number of open-closed brackets in attribute
        ln_ptr = strchr(ln_ptr, '[');
        ln_ptr++;
      } 
      continue;
    }
    else if(*ln_ptr == '"')
    {
      NSDL3_PARSING(NULL, NULL, "Starting quote found");
      start_idx = ln_ptr;
      break; /* Exit from while(1) loop as we found the starting quote 
                we were looking for. Move on to reading the argument    */
    
    }
    else if (strchr(ln_ptr, ']') != NULL)
    { 
      if (!nested_attributes)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012194_ID, CAV_ERR_1012194_MSG, "]");
      }
      nested_attributes--;
      do_not_enter_open_bracket = 0;

      ln_ptr = strchr(ln_ptr, ']');
      ln_ptr++;
      CLEAR_WHITE_SPACE(ln_ptr);
      if (*ln_ptr == '\"')
        ln_ptr++;

      continue;
    }

    else{
      CLEAR_WHITE_SPACE(ln_ptr);
      if (*ln_ptr == ')')
      {
        ln_ptr++;
        CLEAR_WHITE_SPACE(ln_ptr);
        if (*ln_ptr == ';'){
          last_argument = 1;
          return NS_PARSE_SCRIPT_SUCCESS;
        }
      } else  
        {
          NSDL3_PARSING(NULL, NULL, "Script line = '%s', ln_ptr = %p '%s'", script_line, ln_ptr, ln_ptr);
          /* In case any thing except comment, space or comma character found */
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012511_ID, CAV_ERR_1012511_MSG);
        }
    }
  } /* while(1) */

  /* We are here means start_idx points to the opening quotes of next argument */
  if(!comma_found && !nested_attributes){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012195_ID, CAV_ERR_1012195_MSG);
  }

  if(*start_idx != '"'){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012196_ID, CAV_ERR_1012196_MSG);
  }

  start_idx++;

  argstr[0] = '\0';

  while(1)
  {
    end_idx = strstr(start_idx, "\"");

    if(end_idx != NULL)
    {
      if(end_idx <= start_idx && strlen(argstr) == 0)
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012335_ID, CAV_ERR_1012335_MSG);

      if(*(end_idx-1) == '\\'){ /* If it was escaped */
        *(end_idx-1) = '\0';
        strcat(argstr, start_idx);
        strcat(argstr, "\"");
        len += strlen(start_idx)+1;
        start_idx = end_idx+1;        
        continue;

      } else { /* found the ending quotes */

        *end_idx = '\0';
        strcat(argstr, start_idx);
        len += strlen(start_idx);
        ln_ptr = end_idx +1;
        NSDL3_PARSING(NULL,NULL,"After ending quotes, ln_ptr = '%p', '%s'", ln_ptr, ln_ptr);

        break; 
      }

    }

    /* We are here means argument string is multiline */
    strcat(argstr, start_idx);
    len += strlen(start_idx);

    if(NULL == read_line_and_comment(clickscript_fp, outfp)){ /* It reads in script_line */
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012511_ID, CAV_ERR_1012511_MSG);
    }
    else
    {
      ln_ptr = script_line;
    }

    start_idx = ln_ptr;
    continue;
  }

  if(len <= 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012198_ID, CAV_ERR_1012198_MSG);
  }

  argstr[len] = '\0';

  CLEAR_WHITE_SPACE_FROM_END(argstr);

  //Checking if Pagename not received e.g. "  "
  if(strlen(argstr) <= 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012198_ID, CAV_ERR_1012198_MSG);
  }

  NSDL4_PARSING(NULL, NULL, "argument string read at script_ln_no %d in script file. String = \"%s\"", script_ln_no, argstr);

  start_idx = strchr(argstr, '=');

  if (!start_idx) {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012199_ID, CAV_ERR_1012199_MSG);
  }
  len = start_idx - argstr;

  strncpy(argument->name, argstr, len);
  argument->name[len] = '\0';

  int blnk_chars = 0;
  while(argument->name[blnk_chars] == ' ' || argument->name[blnk_chars] == '\t')
    blnk_chars++;

  if(blnk_chars)
    snprintf(argument->name, len - blnk_chars + 1, "%s", &argument->name[blnk_chars]);

 
  CLEAR_WHITE_SPACE_FROM_END(argument->name);

  start_idx++;

  CLEAR_WHITE_SPACE(start_idx);

  strcpy(argument->value, start_idx);

  /* Now chck if this was last argument */

  while(1)
  {
    CLEAR_WHITE_SPACE(ln_ptr);    
    if (*ln_ptr == '\0' || *ln_ptr == '\n')
    {
      if(NULL == read_line_and_comment(clickscript_fp, outfp)){ /* It reads in script_line */
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012511_ID, CAV_ERR_1012511_MSG);
      }
      else 
      {
        ln_ptr = script_line;
        continue;
      }
    }

    if (*ln_ptr == ')')
    {
      ln_ptr++;
      CLEAR_WHITE_SPACE(ln_ptr);
      if (*ln_ptr == ';'){
        last_argument = 1;
        break;
      }
      else
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012200_ID, CAV_ERR_1012200_MSG);
      }

    } else break; 
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_headers_flags(char *fields[], int url_idx, int sess_idx, int num_headers)
{
  char *content_type_val = NULL;
  int i;

  for(i=0; i< num_headers; i++)
  {
    if(!strncasecmp(fields[i], "Content-Type:", 13))
    {
      content_type_val = fields[i] + 13;
      if(*content_type_val)
      {
        NSDL2_PARSING(NULL, NULL, "content_type_val = %s", content_type_val);
        CLEAR_WHITE_SPACE(content_type_val);
        //Setting Encoding Type
        if (strstr(content_type_val, "x-www-form-urlencoded"))
        {
          requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_URL_ENCODED;
        }

        else if (strstr(content_type_val, "application/x-amf"))
        {
          requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_AMF;
        }

        else if (strstr(content_type_val, "application/x-hessian") || strstr(content_type_val, "x-application/hessian"))
        {
          requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_HESSIAN;
        }
        else if (strstr(content_type_val, "application/octet-stream") && (global_settings->use_java_obj_mgr))
        {
          requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_JAVA_OBJ;
        }

        //Setting text/Javascript
        if (strstr(content_type_val, "text/javascript") || (runProfTable[sess_idx].gset.js_all == 1))
        {
          requests[url_idx].proto.http.header_flags |= NS_URL_KEEP_IN_CACHE;
        }
      }
    }
    
    if(!strncasecmp(fields[i], "Expect: 100-continue", strlen("Expect: 100-continue")))
    {
      requests[url_idx].proto.http.header_flags |= NS_HTTP_100_CONTINUE_HDR;

      /* Expect continue is only for POST body*/
      if(requests[url_idx].proto.http.http_method != HTTP_METHOD_POST)
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012076_ID, CAV_ERR_1012076_MSG);
      //pipelining issue TODO
      if(get_any_pipeline_enabled() == 1)
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012077_ID, CAV_ERR_1012077_MSG);
    }
  }

  return NS_PARSE_SCRIPT_SUCCESS;
}

static int create_pg_and_url (char *pagename, char *url, int sess_idx, char *flow_file_name, int *p_url_idx, char *apiname)
{

  char tmpflow_filename[1024];
  char tmpPagename[1024];
  strcpy(tmpflow_filename, flow_file_name);
  strcpy(tmpPagename, pagename);
  char *flow_name = NULL;
  char *ptr;
  int page_norm_id;


  NSDL2_PARSING(NULL, NULL, "Method Called, pagename = '%s', url = '%s'", pagename, url);

  //Check for format of the page. If : is given then make it NULL for page name
  ptr = strchr(pagename, ':');
  if(ptr != NULL)
    *ptr = '\0';


  if(NS_PARSE_SCRIPT_ERROR == check_duplicate_pagenames(pagename, sess_idx)){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012068_ID, CAV_ERR_1012068_MSG, pagename, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
  }

  create_page_table_entry(&g_cur_page); 

  if ((gPageTable[g_cur_page].page_name = copy_into_big_buf(pagename, 0)) == -1)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, pagename);

  /*Extract flow name from flow_file which includes path n name of the flow file */
  flow_name = basename(tmpflow_filename);
  NSDL3_PARSING(NULL, NULL, "flow_name = %s", flow_name);

  /*Copy flow file into big buffer */
  if ((gPageTable[g_cur_page].flow_name = copy_into_big_buf(flow_name, 0)) == -1)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, flow_name);

   page_norm_id = get_norm_id_for_page(tmpPagename, 
                                       get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), 
                                                                           sess_idx, "/"), 
                                       gSessionTable[sess_idx].sess_norm_id);

  if (gSessionTable[sess_idx].num_pages == 0)
  {
    gSessionTable[sess_idx].first_page = g_cur_page;
    NSDL2_PARSING(NULL, NULL, "Current Page Number = %d", g_cur_page);
  }

  gSessionTable[sess_idx].num_pages++;

  gPageTable[g_cur_page].num_eurls = 0;
  gPageTable[g_cur_page].head_hlist = -1;
  gPageTable[g_cur_page].tail_hlist = -1;
  gPageTable[g_cur_page].page_norm_id = page_norm_id;
  NSDL2_PARSING(NULL, NULL, "Number of Pages = %d", gSessionTable[sess_idx].num_pages);

  if (NS_PARSE_SCRIPT_ERROR == init_url(p_url_idx, 0, flow_file_name, HTTP_NO_INLINE)){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012511_ID, CAV_ERR_1012511_MSG);
  }

  // We are passing last argument 1 as it is used to resolve inline host, but in click ans scirpt there is no concept of main url so for 
  // safty we are parsing it one. This will not create any impact  
  if (NS_PARSE_SCRIPT_ERROR == set_url_internal(url, flow_file_name, sess_idx, *p_url_idx, 0, 1, apiname)){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012511_ID, CAV_ERR_1012511_MSG);
  }
  
  return NS_PARSE_SCRIPT_SUCCESS;
}

int parse_ns_click_element(char *apiname, FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  char pagename[MAX_LINE_LENGTH + 1];
  struct click_api_argument argument;
  int  len=0, i, ca_idx=-1, url_idx;
  char tmpstr[MAX_LINE_LENGTH + 1];
  char *att[NUM_ATTRIBUTE_TYPES];
  char url[64*1024] = "\0";
  char headers_buf[2*1024]= "\0";

#ifdef NS_DEBUG_ON
  char debug_msg[64*1024];
#endif

  NSDL2_PARSING(NULL, NULL, "Method Called");

  for(i=0; i<NUM_ATTRIBUTE_TYPES; i++)
   att[i] = NULL;

  global_settings->protocol_enabled |= CLICKSCRIPT_PROTOCOL_ENABLED;

  ln_ptr = strstr(script_line, apiname);

  if(ln_ptr == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012189_ID, CAV_ERR_1012189_MSG, apiname);
  
  tmpstr[0] = '\0';
  len = ln_ptr - script_line;
  if(len>0) 
    strncpy(tmpstr, script_line, len);
  
  tmpstr[len] = '\0';

  if (NS_PARSE_SCRIPT_ERROR == read_stepname(pagename,  clickscript_fp, outfp))
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012192_ID, CAV_ERR_1012192_MSG, "pagename", apiname);

  strcpy(argument.name, "APINAME");
  strcpy(argument.value, apiname);
  populate_attributes_array(argument, att);

  while(!last_argument)
  {
    if (NS_PARSE_SCRIPT_ERROR == read_argument(&argument, clickscript_fp, outfp)){
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012206_ID, CAV_ERR_1012206_MSG, apiname);
    }
 
    //Bug 28526 - RBU- Getting core when we are running click & Script. 
    if((strncasecmp(att[APINAME], "ns_browser", strlen("ns_browser"))) && (!strcasecmp(argument.name, "BrowserUserProfile") || 
        !strcasecmp(argument.name, "HarLogDir") || !strcasecmp(argument.name, "VncDisplayId")))
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012207_ID, CAV_ERR_1012207_MSG, apiname);
    }
 
    if(att[APINAME] && (!(strncasecmp(att[APINAME], "ns_browser", strlen("ns_browser"))) || 
                        !(strncasecmp(att[APINAME], "ns_link", strlen("ns_link"))) ||
                        !(strncasecmp(att[APINAME], "ns_button", strlen("ns_button"))) ||
                        !(strncasecmp(att[APINAME], "ns_key_event", strlen("ns_key_event")))))
    {
      if(!strncasecmp(argument.name, "HEADER", strlen("HEADER")))
      {
        set_ca_headers(argument.value, headers_buf);
      }
    }
 
    populate_attributes_array(argument, att);

  }

  last_argument = 0;
   //Atul: RBU Click and Script check for CLIPINTERVAL should not be less then 5 millisecond.
   if(att[CLIPINTERVAL] != NULL && atoi(att[CLIPINTERVAL]) < 5)
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012208_ID, CAV_ERR_1012208_MSG);

  /* Now add this click action attributes in click action table */
  NSDL4_PARSING(NULL, NULL, "adding click action table entry, apiname = %s, ATTRIBUTES: %s, sess_idx = %d", 
     apiname, attributes2str(att, debug_msg), sess_idx);

  if(att[HREF] && !strncasecmp(att[HREF], "http", 4)) // if href is set take this one
    strcpy(url, att[HREF]);
  else if(att[URL] && att[URL][0] != '\0') // else fallback on url
    strcpy(url, att[URL]);
  else if(last_url[0] != '\0') //else fallback on last valid url
    strcpy(url, last_url);
  
  if (strlen(url) == 0) // Case when url could not be read, fail 
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012212_ID, CAV_ERR_1012212_MSG, apiname);

  if (strncasecmp(url, "http", 4) != 0) // Case when url is not fully qualified, fail 
   SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012213_ID, CAV_ERR_1012213_MSG, url, apiname);

  if(att[PerformanceTraceLog])
    g_rbu_create_performance_trace_dir = PERFORMANCE_TRACE_DIR_FLAG;

  /* add_click_action_entry() will delete the att[] array. So don't access it after callng this method. */
  if (NS_PARSE_SCRIPT_ERROR == add_click_action_entry(att, sess_idx, flow_file, &ca_idx))
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012214_ID, CAV_ERR_1012214_MSG, apiname);

  /* Now write this in out flow file */
  /* ret = ns_click_api(0, 4) // api_name = ns_button, pagename=clickscript_2 */
  sprintf(tmpstr, "%s %s(%d, %d); // api name = '%s', pagename = '%s'",
     tmpstr, "ns_click_api",
     web_url_page_id, ca_idx,
     apiname, pagename);

  NSDL4_PARSING(NULL, NULL, "writing tmpstr to out flow file string='%s', len = %d", tmpstr, len);

  if(write_line(outfp, tmpstr, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012071_ID, CAV_ERR_1012071_MSG);

  web_url_page_id++;

  if (NS_PARSE_SCRIPT_ERROR == create_pg_and_url (pagename, url, sess_idx, flow_file, &url_idx, apiname))
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012216_ID, CAV_ERR_1012216_MSG);

  if(!strncasecmp(apiname, "ns_browser", 10))
  {
    //Shibani: currently script_ln_no has wrong data because all attribute of RBU has parsed and script_ln_no increased 
    if(set_rbu_param_for_ns_browser(att, flow_file, url_idx, sess_idx, script_ln_no) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    
    /* Reset the NS_URL_CLICK_TYPE bit of http request table entry */ 
    requests[url_idx].proto.http.header_flags = requests[url_idx].proto.http.header_flags &  ~(NS_URL_CLICK_TYPE);
   
    NSDL4_PARSING(NULL, NULL, "NS_URL_CLICK_TYPE bit 'RESET' on &requests[url_idx].proto.http = %p, click apiname = '%s'", &requests[url_idx].proto.http, apiname);
  } else {
    /* Set the NS_URL_CLICK_TYPE bit of http request table entry. This will cause the 
     * url to be searched in DOM. Checked in http_make_url_and_check_cache() in "ns_url_req.c" */ 
    requests[url_idx].proto.http.header_flags = requests[url_idx].proto.http.header_flags | NS_URL_CLICK_TYPE;

    NSDL4_PARSING(NULL, NULL, "NS_URL_CLICK_TYPE bit 'SET' on &requests[url_idx].proto.http = %p, click apiname = '%s'", &requests[url_idx].proto.http, apiname);
  }


  /* Now save URL, if present, to static str variable last_url. Can be used for creating 
   * page entry for next click api */
  if(strlen(url) > 0)
  {
    NSDL4_PARSING(NULL, NULL, "%s: Saving URL '%s' to static string variable last_url", apiname, url);
    strcpy(last_url, url);
  }

  /* set the headers buffer */
  strcat(headers_buf, "\r\n"); /* Extra \r\n before body */
  char tmp_headers_buf[2*1024] = {0};
  char *headers_fields[128];
  int num_headers = 0;

  strcpy(tmp_headers_buf, headers_buf); 
  num_headers = get_tokens_ex2(tmp_headers_buf, headers_fields, "\r\n", 128);
  NSDL4_PARSING(NULL, NULL, "num_headers = %d, headers_buf = %s", num_headers, headers_buf);

  if((set_headers_flags(headers_fields, url_idx, sess_idx, num_headers)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  segment_line(&(requests[url_idx].proto.http.hdrs), headers_buf, 0, script_ln_no, sess_idx, flow_file); 

  free_attributes_array(att);

  return NS_PARSE_SCRIPT_SUCCESS;
}


int parse_ns_browser(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_browser", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}


int parse_ns_link (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_link", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}


int parse_ns_button(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_button", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_edit_field(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_edit_field", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}


int parse_ns_check_box(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_check_box", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_radio_group(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_radio_group", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_list(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_list", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_form(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_form", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_map_area(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_map_area", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_submit_image(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_submit_image", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_js_dialog(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_js_dialog", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_text_area(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_text_area", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_span(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_span", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_scroll(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_scroll", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_element(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_element", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}
 
int parse_ns_mouse_hover(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_mouse_hover", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_mouse_out(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{     
  return parse_ns_click_element("ns_mouse_out", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

int parse_ns_mouse_move(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{     
  return parse_ns_click_element("ns_mouse_move", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}
  
int parse_ns_key_event(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_key_event", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}
int parse_ns_get_num_domelement(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_get_num_domelement", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}
int parse_ns_browse_file(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{   
  return parse_ns_click_element("ns_browse_file", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}
int parse_ns_js_checkpoint(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_js_checkpoint", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}
int parse_ns_execute_js(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx)
{
  return parse_ns_click_element("ns_execute_js", clickscript_fp, outfp, flow_file, flow_outfile, sess_idx);
}

/****** END OF FILE ******/
