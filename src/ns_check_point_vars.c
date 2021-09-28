/******************************************************************
 * Name    :    ns_check_point_vars.c
 * Purpose :    nsl_check_point - parsing, shared memory, run time
 * Note    :    nsl_web_find is API to register a text string or pattern
                with one or more pages for content verification. 
                This API is also called checkpoint registration API.
* Syntax  :    nsl_web_find(TEXT=”text-string”, PAGE=page-name, FAIL=condition, ID=”identity-string”); 
* Author  :    Archana
* Intial version date:    05/06/08
* Last modification date: 05/06/08
*****************************************************************/

#define _GNU_SOURCE
#include <stdlib.h>
#include <ctype.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"

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
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_vars.h"

#include "ns_check_point_vars.h"

#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_debug_trace.h"
#include "nslb_sock.h"
#include "ns_vuser_trace.h"
#include "nslb_util.h"        //For parse_api
#include "nslb_http_auth.h"   //For md5 sum
//#include "ns_chekc_point_vars.h"  //included in util.h
#include "ns_debug_trace.h" 
#include "ns_vuser_tasks.h"
#include "nslb_encode.h"
#include "ns_exit.h"
#include "wait_forever.h"
#include "ns_error_msg.h"
#include "ns_script_parse.h"
#include "ns_socket.h"

#ifndef CAV_MAIN
CheckPointTableEntry* checkPointTable;
static int total_checkpoint_entries;
static int max_checkpoint_entries;
static int total_perpagechkpt_entries;
int max_perpagechkpt_entries = 0;
CheckPointTableEntry_Shr *checkpoint_table_shr_mem = NULL;
#else
__thread CheckPointTableEntry* checkPointTable;
__thread int total_checkpoint_entries;
static __thread int max_checkpoint_entries;
__thread int total_perpagechkpt_entries;
__thread int max_perpagechkpt_entries = 0;
__thread CheckPointTableEntry_Shr *checkpoint_table_shr_mem = NULL;
#endif


static int create_checkpoint_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_checkpoint_entries = %d, max_checkpoint_entries = %d", total_checkpoint_entries, max_checkpoint_entries);
  if (total_checkpoint_entries == max_checkpoint_entries) 
    {
      MY_REALLOC (checkPointTable, (max_checkpoint_entries + DELTA_CHECKPOINT_ENTRIES) * sizeof(CheckPointTableEntry), "checkpoint entries", total_checkpoint_entries);
      max_checkpoint_entries += DELTA_CHECKPOINT_ENTRIES;
    }
  *row_num = total_checkpoint_entries++;
  return (SUCCESS);
}

static int create_checkpage_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_checkpage_entries = %d, max_checkpage_entries = %d", total_checkpage_entries, max_checkpage_entries);

  if (total_checkpage_entries == max_checkpage_entries) 
    {
      MY_REALLOC (checkPageTable, (max_checkpage_entries + DELTA_CHECKPAGE_ENTRIES) * sizeof(CheckPageTableEntry), "checkpage entries", total_checkpage_entries);
      max_checkpage_entries += DELTA_CHECKPAGE_ENTRIES;
    }
  *row_num = total_checkpage_entries++;
  return (SUCCESS);
}

void init_checkpoint_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_checkpoint_entries = 0;
  total_perpagechkpt_entries = 0;

  MY_MALLOC (checkPointTable, INIT_CHECKPOINT_ENTRIES * sizeof(CheckPointTableEntry), "checkPointTable", -1);
  MY_MALLOC (checkPageTable, INIT_CHECKPAGE_ENTRIES * sizeof(CheckPageTableEntry), "checkPageTable", -1);

  if(searchVarTable && searchPageTable)
    {
      max_checkpoint_entries = INIT_CHECKPOINT_ENTRIES;
      max_perpagechkpt_entries = INIT_PERPAGECHKPT_ENTRIES;
    }
  else
    {
      max_checkpoint_entries = 0;
      max_perpagechkpt_entries = 0;
      NS_EXIT(-1, CAV_ERR_1031012, "CheckPointTableEntry", "CheckPageTableEntry");
    }
}

//static char *get_quoted_string(char *line_ptr, char *msg, char *data)

#if 0
static void get_quoted_string(char *line_ptr, char *msg, char *data)
{
  int i;
  NSDL1_VARS(NULL, NULL, "Method called. line_ptr = %s, msg = %s", line_ptr, msg);
  for (i = 0 ; *line_ptr != '\0' ; line_ptr++, i++) 
    {
      NSDL1_VARS(NULL, NULL, "*line_ptr = [%c]", *line_ptr);
      if (*line_ptr == '\\') 
	{
	  line_ptr++;
	  if (line_ptr) 
	    {
	      switch (*line_ptr) 
		{
		case 'n': data[i] = '\n'; break;
		//case '\\': data[i] = '\\'; break;
		//case '"': data[i] = '"'; break;
		case 't': data[i] = '\t'; break;
		case 'b': data[i] = '\b'; break;
		case 'v': data[i] = '\v'; break;
		case 'f': data[i] = '\f'; break;
		case 'r': data[i] = '\r'; break;
		default:
		  fprintf(stderr, "%s Bad format. unrecognised '%s' \n", msg, line_ptr); exit (-1);
		}
	    }
	} 
      /*else if (*line_ptr == '\"') 
	{
	  data[i] = '\0'; line_ptr++; break;
	}*/ 

      else 
	data[i] = *line_ptr;
      NSDL3_VARS(NULL, NULL, "data[%d] = [%c]", i, data[i]);
    }
    data[i] = '\0';

  //return line_ptr;
}
#endif


int create_perpagechkpt_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_perpagechkpt_entries = %d, max_perpagechkpt_entries = %d", total_perpagechkpt_entries, max_perpagechkpt_entries); 

  if (total_perpagechkpt_entries == max_perpagechkpt_entries) 
    {
      MY_REALLOC (perPageChkPtTable, (max_perpagechkpt_entries + DELTA_PERPAGECHKPT_ENTRIES) * sizeof(PerPageChkPtTableEntry), "perpagechkpt entries", total_perpagechkpt_entries);
      max_perpagechkpt_entries += DELTA_PERPAGECHKPT_ENTRIES;
    }
  *row_num = total_perpagechkpt_entries++;
  return (SUCCESS);
}

int checkpage_cmp(const void* ent1, const void* ent2) 
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  if (((CheckPageTableEntry *)ent1)->page_idx < ((CheckPageTableEntry *)ent2)->page_idx)
    return -1;
  else if (((CheckPageTableEntry *)ent1)->page_idx > ((CheckPageTableEntry *)ent2)->page_idx)
    return 1;
  else if (((CheckPageTableEntry *)ent1)->checkpoint_idx < ((CheckPageTableEntry *)ent2)->checkpoint_idx)
    return -1;
  else if (((CheckPageTableEntry *)ent1)->checkpoint_idx > ((CheckPageTableEntry *)ent2)->checkpoint_idx)
    return 1;
  else return 0;
}

int process_checkpoint_table_per_session(int session_id, char *err_msg) 
{
  int i,j,k;
  int chkpage_idx;

  NSDL1_VARS(NULL, NULL, "Method called. total_sess_entries = %d total_checkpage_entries = %d, session_id = %d", total_sess_entries, total_checkpage_entries, session_id);

  //Create page checkpoint table  based on PAGEALL checkpoint entried
  //for (i = 0; i < total_sess_entries; i++) 
  i = session_id;
    {
      for (j = gSessionTable[i].checkpoint_start_idx; j < (gSessionTable[i].checkpoint_start_idx + gSessionTable[i].num_checkpoint_entries); j++) 
	{
	  if (checkPointTable[j].pgall) 
	    {
	      for (k = gSessionTable[i].first_page; k < (gSessionTable[i].first_page + gSessionTable[i].num_pages); k++) 
		{
		  if (create_checkpage_table_entry(&chkpage_idx) == -1) 
		    {
		      snprintf(err_msg, 1024, "Could not create create_checkpage_table_entry. Short on memory\n");
		      return -1;
		    }

                  NSDL2_VARS(NULL, NULL, "i = %d, checkpage_idx = %d\n", i, chkpage_idx);
		  checkPageTable[chkpage_idx].checkpoint_idx = j;
		  if (gSessionTable[i].checkpage_start_idx == -1) 
		    {
		      gSessionTable[i].checkpage_start_idx = chkpage_idx;
		      gSessionTable[i].num_checkpage_entries = 0;
		    }
		  gSessionTable[i].num_checkpage_entries++;
		  checkPageTable[chkpage_idx].page_idx = k;
                  checkPageTable[chkpage_idx].sess_idx = i;

		  NSDL2_VARS(NULL, NULL, "checkpage_start_idx = %d, checkpoint_idx = %d, page_idx = %d", gSessionTable[i].checkpage_start_idx, j, k);
		}
	    }
	}
    }



  /*Added By Manish Mishra:
    Till now we have made a complete checkPageTable but not assigned page_idx to all the pages.
    case1: 
      page_idx = -1: mean this page belongs to those checkpoint which has to be searched on this particular page 
      so we have to assign page_idx here
    case2:
      page_idx != -1: mean this page belongs to those checkpoint which has to be searched on all page
      this case has been handle in above function
  */
  /*Nikita: Bug fix 401
  */
  NSDL3_VARS(NULL, NULL, "total_checkpage_entries = %d", total_checkpage_entries);
  int chkPageTbl_idx = 0;
  for (chkPageTbl_idx = 0; chkPageTbl_idx < total_checkpage_entries; chkPageTbl_idx++) {
    NSDL3_VARS(NULL, NULL, "Case: PAGE = particular: chkPageTbl_idx = %d", chkPageTbl_idx);
    if (checkPageTable[chkPageTbl_idx].page_idx == -1) {
      if ((checkPageTable[chkPageTbl_idx].page_idx = find_page_idx(RETRIEVE_TEMP_BUFFER_DATA(checkPageTable[chkPageTbl_idx].page_name), checkPageTable[chkPageTbl_idx].sess_idx)) == -1) {
        NSDL3_VARS(NULL, NULL, "chkPageTbl_idx = %d, unknown page name = %s", chkPageTbl_idx, RETRIEVE_TEMP_BUFFER_DATA(checkPageTable[chkPageTbl_idx].page_name));
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012018_ID, CAV_ERR_1012018_MSG, RETRIEVE_TEMP_BUFFER_DATA(checkPageTable[chkPageTbl_idx].page_name), "checkpoint");
        //Manish: Fix Bug3448
      }
      NSDL3_VARS(NULL, NULL, "page_idx = %d, page_name = %s",  
                              checkPageTable[chkPageTbl_idx].page_idx, 
                              RETRIEVE_TEMP_BUFFER_DATA(checkPageTable[chkPageTbl_idx].page_name));
    }
  }

  #if 0
  k = 0;
  //Convert page name to page idx for all checkpoint, if needed (For PAGE=ALL, page already has idx)
  //for (i = 0; i < total_sess_entries; i++) 
  i = session_id;
    {
      for (j = gSessionTable[i].checkpage_start_idx; 
           j < (gSessionTable[i].checkpage_start_idx + gSessionTable[i].num_checkpage_entries); j++) 
	{
          NSDL2_VARS(NULL, NULL, "i = %d, j = %d, checkpage_idx = %d, page_idx = %d\n", 
                 i, j, chkpage_idx, checkPageTable[j].page_idx);
 	  if (checkPageTable[j].page_idx == -1 ) 
	    {
	      if ((checkPageTable[j].page_idx = find_page_idx(RETRIEVE_TEMP_BUFFER_DATA(checkPageTable[j].page_name), i)) == -1)
		fprintf(stderr, "unknown page %s for the checkpoint \n", 
                        RETRIEVE_TEMP_BUFFER_DATA(checkPageTable[j].page_name));
	    }
	  k++;
	}
    }
   #endif

  //assert (k == total_checkpage_entries);


#if 0
  //Create per page checkpoint table  based on PAGEALL checkpoint entried
  for (i = 0; i < total_sess_entries; i++) 
    {
      for (j = gSessionTable[i].checkpoint_start_idx; j < (gSessionTable[i].checkpoint_start_idx + gSessionTable[i].num_checkpoint_entries); j++) 
	{
	  if (checkPointTable[j].pgall) 
	    {
	      for (k = gSessionTable[i].first_page; k < (gSessionTable[i].first_page + gSessionTable[i].num_pages); k++) 
		{
		  if (create_perpagechkpt_table_entry(&rnum) != SUCCESS) 
		    {
		      fprintf(stderr, "Unable to create perpage checkpoint table entry\n");
		      return -1;
		    }
		  if (gPageTable[k].first_checkpoint_idx == -1)
		    gPageTable[k].first_checkpoint_idx = rnum;
		  perPageChkPtTable[rnum].checkpoint_idx = j;
		  gPageTable[k].num_checkpoint++;
		}
	    }
	}
    }
#endif

  return 0;
}

int process_checkpoint_table() 
{
  int last_checkpoint_idx;
  int rnum;
  int page_idx;
  int i;

  NSDL1_VARS(NULL, NULL, "Method called");
  //sort  serach Page table in the irde of page index & serach var index
  qsort(checkPageTable, total_checkpage_entries, sizeof(CheckPageTableEntry),checkpage_cmp);

  //Create per page checkpoint table
  //This table has the list of checkpoint that need to be initialized on specific pages
  //Each page has the start & num entries for this per page serach var table entries.
  for (i = 0; i < total_checkpage_entries; i++) {
    if ((page_idx = checkPageTable[i].page_idx) != -1) 
      {
        NSDL2_VARS(NULL, NULL, "page_idx = %d\n", page_idx);
        if (gPageTable[page_idx].first_checkpoint_idx == -1) 
          {
            NSDL2_VARS(NULL, NULL, "First Checkpoint found for page_idx = %d, page_name = %s check_page_idx = %d\n", page_idx, RETRIEVE_BUFFER_DATA(gPageTable[page_idx].page_name), i);
            if (create_perpagechkpt_table_entry(&rnum) != SUCCESS) 
              {
                fprintf(stderr, "Unable to create perpage checkpoint table entry\n");
                return -1;
              }
            perPageChkPtTable[rnum].checkpoint_idx = checkPageTable[i].checkpoint_idx;
            gPageTable[page_idx].first_checkpoint_idx = rnum;
            last_checkpoint_idx = checkPageTable[i].checkpoint_idx;
          } 
        else 
          {
            //Remove duplicate entries for same var for same page
            //Note that checkPage Table is ordered in page number order
            if (last_checkpoint_idx == checkPageTable[i].checkpoint_idx){
            
              NSDL2_VARS(NULL, NULL, "Duplicate entry found for page idx %d, page_name = %s", page_idx, RETRIEVE_BUFFER_DATA(gPageTable[page_idx].page_name));
              // return -1;
              continue;

            } 
            NSDL2_VARS(NULL, NULL, "Checkpoint found for page_idx = %d,  page_name = %s check_page_idx = %d\n", page_idx, RETRIEVE_BUFFER_DATA(gPageTable[page_idx].page_name), i);
            if (create_perpagechkpt_table_entry(&rnum) != SUCCESS) 
              {
                fprintf(stderr, "Unable to create perpage checkpoint table entry\n");
                return -1;
              }
            perPageChkPtTable[rnum].checkpoint_idx = checkPageTable[i].checkpoint_idx;
            last_checkpoint_idx = checkPageTable[i].checkpoint_idx;
          }
        gPageTable[page_idx].num_checkpoint++;

        //Add this to save header when Search_IN field is used in checkpoint API
        if((requests[gPageTable[page_idx].first_eurl].request_type == SOCKET_REQUEST) ||
           (requests[gPageTable[page_idx].first_eurl].request_type == SSL_SOCKET_REQUEST))
        {
          NSTL1(NULL, NULL, "Since there is no header concept in SOCKET API, hence setting to search in Body");
          gPageTable[page_idx].save_headers |= SEARCH_IN_RESP;
          checkPointTable[checkPageTable[i].checkpoint_idx].search_in = NS_CP_SEARCH_BODY;
        }
        else
        {
          if(checkPointTable[checkPageTable[i].checkpoint_idx].search_in == NS_CP_SEARCH_ALL) {
            gPageTable[page_idx].save_headers |= SEARCH_IN_RESP;
	    gPageTable[page_idx].save_headers |= SEARCH_IN_HEADER;
          }
          else if(checkPointTable[checkPageTable[i].checkpoint_idx].search_in == NS_CP_SEARCH_HEADER) {
	    gPageTable[page_idx].save_headers |= SEARCH_IN_HEADER;
          }
          else//Case of NS_CP_SEARCH_BODY
	    gPageTable[page_idx].save_headers |= SEARCH_IN_RESP;
        }
      }
  }
  return 0;
}

// Purpose:
// Returns:
//char *replace_escaped_char(char *line_ptr, char *dest_buf, char *msg)
#if 0
static void replace_escaped_char(char *line_ptr, char *dest_buf, char *msg)
{
  int i;

  for (i = 0 ; *line_ptr != '\0'; line_ptr++, i++) {
    NSDL3_VARS(NULL, NULL, "*line_ptr = [%c]", *line_ptr);
    if (*line_ptr == '\\') {
      line_ptr++;
      if (line_ptr) {
        switch (*line_ptr) {
        case 'n':
          dest_buf[i] = '\n';
          break;
        #if 0
        case '\\':
          dest_buf[i] = '\\';
          break;
        case '"':
          dest_buf[i] = '"';
          break;
        #endif
        case 't':
          dest_buf[i] = '\t';
          break;
        case 'b':
          dest_buf[i] = '\b';
          break;
        case 'v':
          dest_buf[i] = '\v';
          break;
        case 'f':
          dest_buf[i] = '\f';
          break;
        case 'r':
          dest_buf[i] = '\r';
          break;
        default:
          fprintf(stderr, "%s Bad checkpoint declaraction format. unrecognised '%s' \n", msg, line_ptr);
          //return NULL;
        }

      }
    /*} else if (*line_ptr == '\"') {
      dest_buf[i] = '\0';
      line_ptr++;
      break;
    } */ 
    } else {
      dest_buf[i] = *line_ptr;
    }
  }
    dest_buf[i] = '\0';
  //return line_ptr;
}
#endif

/*This function is used to make precompiled pattern and save into the preg
* that is used by regexec().
* Argument:
*    preg: pattern buffer storage area.
     ignorecase: flag to ignore case(lower/upper case) or not
*    data: pointer to null terminated character string that is compiled.
* Return Value: return 0 on success.
* else exit.
*/

void my_regcomp(regex_t *preg, short ignorecase, char *data, char *msg)
{
  int return_value;
  char err_msg[1000 + 1];

  NSDL2_VARS(NULL, NULL, "Method called. ignorecase = %d, data = [%s]", ignorecase, data);
  
  if (ignorecase)
    return_value = regcomp(preg, data, REG_EXTENDED|REG_ICASE);
  // return_value = regcomp(preg, data, REG_EXTENDED|REG_NOSUB|REG_ICASE);
  else
    return_value = regcomp(preg, data, REG_EXTENDED);
  //return_value = regcomp(preg, data, REG_EXTENDED|REG_NOSUB);

  NSDL2_VARS(NULL, NULL, "return_value = %d", return_value);
  if (return_value != 0) {
    regerror(return_value, preg, err_msg, 1000);
    print_core_events((char*)__FUNCTION__, __FILE__, 
                      "%s regcomp failed:%s", msg, err_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000022]: ", CAV_ERR_1000022 + CAV_ERR_HDR_LEN, err_msg);
  }
}
/*This function will parse the RE/IC from the string.
* Argument:
*   rnum: table entry of the CheckPointTableEntry structure array.
*   pfx_sfx_flag: Flag to check for TEXT/TextPfx and TextSfx.
*   line_ptr: Pointer to the char pointer.
* Return Value: none 
*/

static void
parse_re_ic(int rnum, char **line_ptr, int pfx_sfx_flag)
{
  NSDL1_VARS(NULL, NULL, "Method called. line = %s, pfx_sfx_flag = %d", *line_ptr, pfx_sfx_flag);

  if (!strncmp(*line_ptr, "/RE/IC", 6)) {
    NSDL1_VARS(NULL, NULL, "When RE-IC line_ptr = %s", *line_ptr);
    //*line_ptr += 6;
    if(pfx_sfx_flag == TEXT_PFX_FLAG) {
      checkPointTable[rnum].ignorecase_textpfx = 1;
      checkPointTable[rnum].regexp_textpfx = 1;
    } else if(pfx_sfx_flag == TEXT_SFX_FLAG) {
      checkPointTable[rnum].ignorecase_textsfx = 1;
      checkPointTable[rnum].regexp_textsfx = 1;
    }
  } else if (!strncmp(*line_ptr, "/RE", 3)) {
    NSDL1_VARS(NULL, NULL, "When RE line_ptr = %s", *line_ptr);
    //*line_ptr += 3;
    if(pfx_sfx_flag == TEXT_PFX_FLAG)
      checkPointTable[rnum].regexp_textpfx = 1;
    else if(pfx_sfx_flag == TEXT_SFX_FLAG)
      checkPointTable[rnum].regexp_textsfx = 1;
  } 
  NSDL3_VARS(NULL, NULL, "checkPointTable[rnum].ignorecase_textpfx = %d, checkPointTable[rnum].regexp_textpfx = %d"
                         " checkPointTable[rnum].ignorecase_textsfx = %d, checkPointTable[rnum].regexp_textsfx = %d", 
                          checkPointTable[rnum].ignorecase_textpfx, checkPointTable[rnum].regexp_textpfx,
                          checkPointTable[rnum].ignorecase_textsfx, checkPointTable[rnum].regexp_textsfx);
  return;
} 

static int set_data_for_text(char *key, char *value, char *msg, int text_pfx_done, int text_sfx_done, int *text_done, int rnum){
  NSDL2_VARS(NULL, NULL, "Method Called, key = [%s], value = [%s], msg = [%s], text_pfx_done = %d, text_sfx_done = %d, rnum = %d", key, value, msg, text_pfx_done, text_sfx_done, rnum);

  if(text_pfx_done || text_sfx_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012122_ID, CAV_ERR_1012122_MSG);
  }

  if (*text_done) 
  { 
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_MSG, CAV_ERR_1012036_MSG, "TEXT", "Checkpoint");
  }
  /* pass last argument 0 for TEXT */
  //parse_re_ic(rnum, &line_ptr, 0);
  key += strlen("TEXT");
  
  if(!strncmp(key, "/IC", 3))
    checkPointTable[rnum].ignorecase_textpfx = 1;
  else
    parse_re_ic(rnum, &key, 0);

  //line_ptr = get_quoted_string(value, msg, data);
  //get_quoted_string(value, msg, data);
  NSDL3_VARS(NULL, NULL, "value = [%s]", value);

  if (strlen(value) == 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "TEXT", "Checkpoint");
  }
  /* put the text index into text_pfx
   *  make sure text_sfx set by -1 
   */
  if ((checkPointTable[rnum].text_pfx = copy_into_big_buf(value, 0)) == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
  }
  /*set text_sfx to -1 for TEXT. This will be used while copying the
   * data into shared memory. 
   */
  checkPointTable[rnum].text_sfx = -1;
  /* Make the compiled pattern buffer for regexec() */ 
  if (checkPointTable[rnum].regexp_textpfx)
    my_regcomp(&checkPointTable[rnum].preg_textpfx, checkPointTable[rnum].ignorecase_textpfx, value, msg);
    *text_done = 1;
  return 0;
}

static int set_data_for_textPfx_and_TextSfx(char *key, char *value, char *msg, int *text_pfx_done, int *text_sfx_done, int text_done, int rnum){
  NSDL2_VARS(NULL, NULL, "Method Called, key = [%s], value = [%s], msg = [%s], *text_pfx_done = %d, *text_sfx_done = %d", 
                          key, value, msg, *text_pfx_done, *text_sfx_done);

  int text_pfx_processing = 0;
  IW_UNUSED(int text_sfx_processing = 0);

  if(text_done == 1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012122_ID, CAV_ERR_1012122_MSG);
  }
 
  if (!strncasecmp(key, "TextPfx", 7)) {
    if (*text_pfx_done) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_MSG, CAV_ERR_1012036_MSG, "TextPfx", "Checkpoint");
    }
    key += strlen("TextPfx"); 
    /* pass last argument 0 for TextSfx */
    parse_re_ic(rnum, &key, 0);
    text_pfx_processing = 1;
    IW_UNUSED(text_sfx_processing = 0);
  } else if (!strncasecmp(key, "TextSfx", 7)) {
      if (*text_sfx_done) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_MSG, CAV_ERR_1012036_MSG, "TextSfx", "Checkpoint");
      }
      key += strlen("TextSfx");
      /* pass last argument 1 for TextSfx */
      parse_re_ic(rnum, &key, 1);
      IW_UNUSED(text_sfx_processing = 1);
      text_pfx_processing = 0;
  }

  NSDL2_VARS(NULL, NULL, "text_pfx_processing = %d, text_sfx_processing = %d", text_pfx_processing, text_sfx_processing);
  // TODO - why this is not done in TEXT and why it is required for Pfx/Sfx
  //if((line_ptr = replace_escaped_char(line_ptr, data, msg)) == NULL)
  //replace_escaped_char(value, data, msg);

  if (strlen(value) == 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "TextPfx/TextSfx", "Checkpoint");
  }

  if (text_pfx_processing) {
    if ((checkPointTable[rnum].text_pfx = copy_into_big_buf(value, 0)) == -1) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
    }
    *text_pfx_done = 1;
    /* Make the compiled pattern buffer for regexec() */
    if (checkPointTable[rnum].regexp_textpfx)
      my_regcomp(&checkPointTable[rnum].preg_textpfx, checkPointTable[rnum].ignorecase_textpfx, value, msg);
  } else {
    if ((checkPointTable[rnum].text_sfx = copy_into_big_buf(value, 0)) == -1) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
    }
    *text_sfx_done = 1;
    /* Make the compiled pattern buffer for regexec() */
    if (checkPointTable[rnum].regexp_textsfx)
      my_regcomp(&checkPointTable[rnum].preg_textsfx, checkPointTable[rnum].ignorecase_textsfx, value, msg);
  }

  return 0;
}

static int set_data_for_SaveCount(char *value, char *data, char *msg, int rnum){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d", value, msg, rnum);
  
  strcpy(data, value);
  NSDL2_VARS(NULL, NULL, "data = [%s]", data);
  if (!strcmp(data, "")) {
    NSTL1(NULL, NULL, "Empty SaveCount value provided for checkpoint.");
    checkPointTable[rnum].save_count_var = -1;
  } else {
    if ((checkPointTable[rnum].save_count_var = copy_into_big_buf(data, 0)) == -1) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, data);
    }
  }
  return 0;
}

static int set_ActionOnFail(char *value, char *msg, int rnum){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d", value, msg, rnum);

  if (!strcasecmp (value, "Stop")) {
    checkPointTable[rnum].action_on_fail = NS_CP_ACTION_STOP; //0
  } else if (!strcasecmp (value, "Continue")) {
     checkPointTable[rnum].action_on_fail= NS_CP_ACTION_CONTINUE; //1
  } else {
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "Action on failure", "Checkpoint");
  }
  
  return 0;
}

static int set_page_name_for_PAGE(char *value, char *page_name, char *msg, int rnum, int *do_page, int sess_idx){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = [%s], *do_page = %d, sess_idx = %d", value, msg, *do_page, sess_idx);
  int chkpage_idx;
   
  strcpy(page_name, value);
  NSDL2_VARS(NULL, NULL, "page_name = [%s]", page_name);

  if (strcmp(page_name, "*")) {
    create_checkpage_table_entry(&chkpage_idx);
    NSDL2_VARS(NULL, NULL, "chkpage_idx = %d", chkpage_idx);
    checkPageTable[chkpage_idx].checkpoint_idx = rnum;
    if ((checkPageTable[chkpage_idx].page_name = copy_into_temp_buf(page_name, 0)) == -1) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, page_name);
    }

    if (gSessionTable[sess_idx].checkpage_start_idx == -1) {
      gSessionTable[sess_idx].checkpage_start_idx = chkpage_idx;
      gSessionTable[sess_idx].num_checkpage_entries = 0;
    }
    gSessionTable[sess_idx].num_checkpage_entries++;

    checkPageTable[chkpage_idx].page_idx = -1;
    checkPageTable[chkpage_idx].sess_idx = sess_idx; //Added by Manish 
    *do_page=1;
    NSDL2_VARS(NULL, NULL, "page_name = %s, checkpoint_idx = %d, checkpage_start_idx = %d", RETRIEVE_BUFFER_DATA(checkPageTable[chkpage_idx].page_name), rnum, gSessionTable[sess_idx].checkpage_start_idx);

  } else { //PAGE= * case
     checkPointTable[rnum].pgall = 1;
  }
  
  return 0; 
}

static int set_FAIL(char *value, char *msg, int rnum){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d", value, msg, rnum);

  if (!strcasecmp (value, "FOUND")) {
    checkPointTable[rnum].fail = NS_CP_FAIL_ON_FOUND;
  } else if (!strcasecmp (value, "NOTFOUND")) {
      checkPointTable[rnum].fail = NS_CP_FAIL_ON_NOT_FOUND;
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "Failure condition", "Checkpoint");
  }
  
  return 0;
}

static int set_REPORT(char *value, char *msg, int rnum){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d", value, msg, rnum);

  if (!strcasecmp (value, "SUCCESS")) {
    checkPointTable[rnum].fail = NS_CP_REPORT_SUCCESS;
  } else if (!strcasecmp (value, "FAILURE")) {
      checkPointTable[rnum].fail = NS_CP_REPORT_FAILURE;
  } else if (!strcasecmp (value, "ALWAYS")) {
      checkPointTable[rnum].fail = NS_CP_REPORT_ALWAYS;
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, "REPORT", "Checkpoint");
  }

  return 0;
}

static int set_data_for_ID(char *value, char *msg, int rnum){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d", value, msg, rnum);

  if (checkPointTable[rnum].id != -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_MSG, CAV_ERR_1012036_MSG, "Message on Failure", "Checkpoint");
  }

  //get_quoted_string (value, msg, data);
  if (strlen(value) == 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Message on Failure", "Checkpoint");
  }

  if ((checkPointTable[rnum].id = copy_into_big_buf(value, 0)) == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
  }
 
  return 0;
}

static int set_data_for_CompareFile(char *value, char *data, char *msg, int rnum, int *file_done, int sess_idx){
  char compareFile[MAX_LINE_LENGTH + 1]; 
 
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d, *file_done = %d", value, msg, rnum, *file_done);

  if (*file_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_MSG, CAV_ERR_1012036_MSG, "Compare File", "Checkpoint");
  }

  if (!strcmp(value, "")) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Compare File", "Checkpoint");
  }

  if(*value == '/') {  // absolute path given
    strcpy(compareFile, value);
  }
  else {
   // Create ABSOLUTE PATH
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    sprintf(compareFile, "%s/%s/%s",
                         GET_NS_TA_DIR(),
                         get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"),
                         value);
    //Previously taking with only script name
    /*sprintf(compareFile, "%s/scripts/%s/%s",
                         g_ns_wdir,
                         get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)),
                         value); */
  }

  NSDL3_VARS(NULL, NULL, "compareFile = [%s]", compareFile); 
 
  *file_done = 1;

  /*Check the size of file and dump the content into memory*/
  struct stat stat_st;
  long data_file_size = 0;
  int read_data_fd = 0;
  //int read_bytes = 0;
  //char *data_file_buf = NULL;
  //int malloced_sized = 0;  
 
  /*Finding the size of data file */
  if(stat(compareFile, &stat_st) == -1)
  {
    NSDL3_VARS(NULL,NULL, "File %s does not exists\n", compareFile);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000016]: ", CAV_ERR_1000016 + CAV_ERR_HDR_LEN, compareFile);
  }
  else
  {
    if(stat_st.st_size == 0){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000017]: ", CAV_ERR_1000017 + CAV_ERR_HDR_LEN, compareFile);
    }
  }
  data_file_size = stat_st.st_size;
  checkPointTable[rnum].compare_file_size = stat_st.st_size;

  /*Now dump the above data of file into memory*/
  NSDL3_VARS(NULL, NULL, "Before allocating memory  malloced_sized = %ld", malloced_sized);
  if (malloced_sized < data_file_size)
  {
    NSDL3_VARS(NULL, NULL, "malloced_sized = %ld, data_file_size = %ld", malloced_sized, data_file_size);
    MY_REALLOC_EX(data_file_buf, data_file_size + 1, malloced_sized, "data file buf", -1);
    malloced_sized = data_file_size;
  }
#if 0
  if(malloced_sized == 0) /*first time*/
  {
    NSDL3_VARS(NULL, NULL, "malloced_sized = %d, data_file_size = %d", malloced_sized, data_file_size);
    data_file_buf = (char *)malloc(data_file_size + 1);
    if (data_file_buf == NULL) {
      fprintf(stderr, "Error: Out of memory.\n");
      return -1;
    }
    malloced_sized = data_file_size;
  }
  else if (malloced_sized < data_file_size)
  {
    NSDL3_VARS(NULL, NULL, "malloced_sized = %d, data_file_size = %d", malloced_sized, data_file_size);
    data_file_buf = (char *)realloc(data_file_buf, (data_file_size + 1));
    if (data_file_buf == NULL) {
      fprintf(stderr, "Error: Out of memory.\n");
      return -1;
    }
    malloced_sized = data_file_size;
  }
#endif
  NSDL3_VARS(NULL, NULL, "After allocating memory malloced_sized = %ld", malloced_sized);

  if ((read_data_fd = open(compareFile, O_RDONLY | O_CLOEXEC | O_LARGEFILE)) < 0){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006, compareFile, errno, nslb_strerror(errno));
  }

  data_file_buf[0] = '\0';
  nslb_read_file_and_fill_buf (read_data_fd, data_file_buf, data_file_size);
  close(read_data_fd);

  if ((checkPointTable[rnum].compare_file = copy_into_big_buf(data_file_buf, stat_st.st_size)) == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, data_file_buf);
  }
 
  return 0;
}

static int set_data_for_checksum(char *value, char *data, char *msg, int rnum, int *checksum_done){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d, checksum_done = %d", 
                          value, msg, rnum, *checksum_done);

  int i = 0;

  if (*checksum_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_MSG, CAV_ERR_1012036_MSG, "Checksum Name", "Checkpoint");
  }

  strcpy(data, value);
  NSDL2_VARS(NULL, NULL, "data = [%s]", data);
  if (!strcmp(data, "")) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Checksum", "Checkpoint");
  } else {
       if (strlen(data) > 32)
       {
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012305_ID, CAV_ERR_1012305_MSG, data, (int)strlen(data));
       }

       for (i = 0; data[i]; i++)
       {
         NSDL2_VARS(NULL, NULL, "validate the data: data = [%c]", data[i]);
         if (!isxdigit(data[i]))
         { 
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012306_ID, CAV_ERR_1012306_MSG, value);
         }
       }

      if ((checkPointTable[rnum].checksum = copy_into_big_buf(data, 0)) == -1) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, data);
      }
  }

  *checksum_done = 1;

  return 0;
}

static int set_data_for_checksum_cookie(char *value, char *msg, int rnum, int *checksumCookie_done){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], msg = %s, rnum = %d, *checksumCookie_done = %d", 
                          value, msg, rnum, *checksumCookie_done);

  if (*checksumCookie_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_MSG, CAV_ERR_1012036_MSG, "Cookie Name", "Checkpoint");
  }

  //get_quoted_string (value, msg, data);
  if (strlen(value) == 0) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Cookie Name", "Checkpoint");
  }

  if ((checkPointTable[rnum].checksum_cookie = copy_into_big_buf(value, 0)) == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, value);
  }
 
  *checksumCookie_done = 1; 
  return 0;
}

/* This function is used to parse the check point api values
*Format of API :
*  nsl_web_find(TEXT=text-string Or TextPfx=text-prefix and TextSfx=text-suffix, PAGE=page-name, [FAIL=condition, ID=identity-string, SaveCount=<var-name>, ActionOnFail=<Stop or Continue>]);
* values closed by [] are optional. rest value are mandatory.
* TEXT, TestPfx and TextSfx can have optional RE or RE/IC. Only IC is not allowed
* Arguments: 
*  line: a pointer to the null terminated string.
*  line_number: script.capture file line number.
* sess_idx: session index.
* Return value: return 0 on suceess and -1 on fail.
*/
int input_checkpoint_data(char* line, int line_number, int sess_idx, char *script_filename) 
{
  int rnum;
  char data[MAX_LINE_LENGTH + 1]; // Used to store value of argument
  char msg[MAX_LINE_LENGTH + 1];
  char page_name[MAX_LINE_LENGTH + 1];
  //int done = 0;
  char *sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  int text_done = 0;
  int do_page = 0;
  int file_done = 0;
  int checksum_done = 0;
  int checksumCookie_done = 0;

  //int chkpage_idx;

  //New update 
  int text_pfx_done = 0;
  int text_sfx_done = 0;
  //int text_pfx_processing = 0;
  //int text_sfx_processing = 0;

  /*Manish: For new parsing api*/
  NSApi api_ptr;
  int j, ret;
  char key[MAX_ARG_NAME_SIZE + 1];
  char value[MAX_ARG_VALUE_SIZE + 1];
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];
  
  NSDL2_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d, script name = %s", line, line_number, sess_idx, sess_name);
  /*bug id: 101320: trace updated to show NS_TA_DIR*/
  snprintf(msg, MAX_LINE_LENGTH, "Error in parsing nsl_web_find() declaration at line %d of %s/%s/%s: ", line_number, 
	   GET_NS_RTA_DIR(),get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
           //Previously taking with only script name
	   //get_sess_name_with_proj_subproj(sess_name), script_filename);
  msg[MAX_LINE_LENGTH-1] = '\0';

  sprintf(file_name, "%s/%s/%s", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), script_filename);
  //Previously taking with only script name
  //sprintf(file_name, "scripts/%s/%s", get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), script_filename);
  file_name[strlen(file_name)] = '\0';

  NSDL3_VARS(NULL, NULL, "api_ptr = %p, file_name = %s", &api_ptr, file_name);
  
  //parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  #if 0
 /*  For testing purpose */
  int i;
  fprintf(stderr, "API Line = %s\n.api_ptr.api_name = [%s]\napi_ptr.num_tokens = %d\n",line, api_ptr.api_name, api_ptr.num_tokens);
  for(i = 0; i < api_ptr.num_tokens; i++) {
    fprintf(stderr, "\t\t%d. Keyword = [%s] : Value = [%s]\n", i, api_ptr.api_fields[i].keyword, api_ptr.api_fields[i].value);
  }
  #endif

  /*First of all create check point table*/ 
  create_checkpoint_table_entry(&rnum);

  /*Initialize members of check point tables*/
  checkPointTable[rnum].sess_idx = sess_idx; /* For hashcode */
  checkPointTable[rnum].text_pfx = -1;
  checkPointTable[rnum].text_sfx = -1;
  checkPointTable[rnum].compare_file = -1;
  checkPointTable[rnum].checksum_cookie = -1;
  //checkPointTable[rnum].fail = NS_CP_FAIL_ON_NOT_FOUND;
  /* Set Default value of FAIL, this will be overwritten if FAIL is given and
  *  will be set if SaveCount is not given
  */
  checkPointTable[rnum].fail = NS_CP_IGNORE;
  checkPointTable[rnum].report = NS_CP_REPORT_FAILURE;
  checkPointTable[rnum].ignorecase_textpfx = 0;
  checkPointTable[rnum].regexp_textpfx = 0;
  checkPointTable[rnum].ignorecase_textsfx = 0;
  checkPointTable[rnum].regexp_textsfx = 0;
  checkPointTable[rnum].id = -1;
  checkPointTable[rnum].pgall = 0;
  checkPointTable[rnum].save_count_var = -1; 
  checkPointTable[rnum].action_on_fail = NS_CP_ACTION_STOP; 
  checkPointTable[rnum].checksum = -1;
  checkPointTable[rnum].search = NS_CP_SEARCH_STR;
  checkPointTable[rnum].search_in = NS_CP_SEARCH_BODY;
 
  if (gSessionTable[sess_idx].checkpoint_start_idx == -1) 
  {
    gSessionTable[sess_idx].checkpoint_start_idx = rnum;
    gSessionTable[sess_idx].num_checkpoint_entries = 0;
  }
  gSessionTable[sess_idx].num_checkpoint_entries++;

  for(j = 0; j < api_ptr.num_tokens; j++) {
    strcpy(key, api_ptr.api_fields[j].keyword);
    strcpy(value, api_ptr.api_fields[j].value);

    NSDL3_VARS(NULL, NULL, "j = %d, key = [%s], value = [%s]", j, key, value);
    NSDL3_VARS(NULL, NULL, "api_ptr.num_tokens = %d", api_ptr.num_tokens);
    if (!strcasecmp(key, "TEXT") || !strcasecmp(key, "TEXT/IC")  || !strcasecmp(key, "TEXT/RE") || !strcasecmp(key, "TEXT/RE/IC")) {  
      set_data_for_text(key, value, msg, text_pfx_done, text_sfx_done, &text_done, rnum);
    } else if (!strcasecmp(key, "TextPfx") || !strcasecmp(key, "TextPfx/RE") || !strcasecmp(key, "TextPfx/RE/IC")|| 
               !strcasecmp(key, "TextSfx") || !strcasecmp(key, "TextSfx/RE") || !strcasecmp(key, "TextSfx/RE/IC")) {
        set_data_for_textPfx_and_TextSfx(key, value, msg, &text_pfx_done, &text_sfx_done, text_done, rnum);
    } else if (!strcasecmp(key, "SaveCount")) {
        set_data_for_SaveCount(value, data, msg, rnum);
    } else if (!strcasecmp(key, "ActionOnFail")) {
        set_ActionOnFail(value, msg, rnum);
    } else if (!strcasecmp(key, "PAGE")) {
        set_page_name_for_PAGE(value, page_name, msg, rnum, &do_page, sess_idx);
    } else if (!strcasecmp(key, "FAIL")) {
        set_FAIL(value, msg, rnum);
    } else if (!strcasecmp(key, "REPORT")) {
        set_REPORT(value, msg, rnum);
    } else if (!strcasecmp(key, "ID")) {
        set_data_for_ID(value, msg, rnum);
    } else if (!strcasecmp(key, "CompareFile")) {
        set_data_for_CompareFile(value, data, msg, rnum, &file_done, sess_idx);
    } else if (!strcasecmp(key, "Checksum")) {
        set_data_for_checksum(value, data, msg, rnum, &checksum_done);
    } else if (!strcasecmp(key, "ChecksumCookie")) {
        set_data_for_checksum_cookie(value, msg, rnum, &checksumCookie_done);
    } else if (!strcasecmp(key, "Search")) {
      NSDL3_VARS(NULL, NULL, "Parsing of SEARCH argument value = %s", value);
      if(!strcasecmp(value, "VAR_TXT")) {
        NSDL3_VARS(NULL, NULL, "Search type is VAR_TXT");
        checkPointTable[rnum].search = NS_CP_SEARCH_VAR_TXT;
      }
      else if(!strcasecmp(value, "VAR_PFX")) {
        NSDL3_VARS(NULL, NULL, "Search type is VAR_PFX");
        checkPointTable[rnum].search = NS_CP_SEARCH_VAR_PFX;
      }
      else if(!strcasecmp(value, "VAR_SFX")) {
        NSDL3_VARS(NULL, NULL, "Search type is VAR_SFX");
        checkPointTable[rnum].search = NS_CP_SEARCH_VAR_SFX;
      }
      else if(!strcasecmp(value, "VAR_PFX_SFX")) {
        NSDL3_VARS(NULL, NULL, "Search type is VAR_PFX_SFX");
        checkPointTable[rnum].search = NS_CP_SEARCH_VAR_PFX_SFX;
      }
      else {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "Checkpoint");
      } 
    } else if(!strcasecmp(key, "Search_IN")) {
      NSDL3_VARS(NULL, NULL, "Parsing of Search_IN argument value = %s", value);
      if(!strcasecmp(value, "ALL")) {
        NSDL3_VARS(NULL, NULL, "Search type is All");
        checkPointTable[rnum].search_in = NS_CP_SEARCH_ALL;
      }
      else if(!strcasecmp(value, "HEADER")) {
        NSDL3_VARS(NULL, NULL, "Search type is Header");
        checkPointTable[rnum].search_in = NS_CP_SEARCH_HEADER;
      }
      else if(!strcasecmp(value, "BODY")) {
        NSDL3_VARS(NULL, NULL, "Search type is Body");
        checkPointTable[rnum].search_in = NS_CP_SEARCH_BODY;
      }
      else {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, value, key, "Checkpoint");
      }
    } else {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, key, "Checkpoint");
   }
  }

  /*NSDL3_VARS(NULL, NULL, "Data Dump: Check Point Table:%d"
                    "sess_idx = %d, text_pfx = [%s], text_sfx = [%s], id = [%s], save_count_var = [%s]," 
                    "compare_file = [%s], checksum_cookie = [%s], fail = [%d], report = [%d], pgall = [%d]"
                    "",
                     rnum, checkPointTable[rnum].sess_idx, RETRIEVE_BUFFER_DATA(checkPointTable[rnum].text_pfx), 
                     RETRIEVE_BUFFER_DATA(checkPointTable[rnum].text_sfx), 
                     RETRIEVE_BUFFER_DATA(checkPointTable[rnum].id),
                     RETRIEVE_BUFFER_DATA(checkPointTable[rnum].save_count_var),
                     RETRIEVE_BUFFER_DATA(checkPointTable[rnum].compare_file),
                     RETRIEVE_BUFFER_DATA(checkPointTable[rnum].checksum_cookie),
                     checkPointTable[rnum].fail, checkPointTable[rnum].report, checkPointTable[rnum].pgall 
           );*/

  /*  Somebody just need the SaveCount to save the count but he dont want ot fail a page 
  *   so if fail is not given & variable is used then we will not set any condition like
  *   NS_CP_FAIL_ON_NOT_FOUND or NS_CP_FAIL_ON_FOUND. 
  */
  if( checkPointTable[rnum].fail == NS_CP_IGNORE &&
      checkPointTable[rnum].save_count_var == -1 ) {

      checkPointTable[rnum].fail = NS_CP_FAIL_ON_NOT_FOUND; 
  }

  if ((!do_page) && (!checkPointTable[rnum].pgall)) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "PAGE", "Checkpoint");
  }

  if ((!text_done) && (!(text_pfx_done && text_sfx_done)) && (!checksum_done) && (!checksumCookie_done) && (!file_done)) {
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012310_ID, CAV_ERR_1012310_MSG);
  }

  if (checkPointTable[rnum].id == -1) {
    if ((checkPointTable[rnum].id = copy_into_big_buf("-", 0)) == -1) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, "-");
    }
  }
   
  return 0;
}

/*This function is used to copy the check 
* point api values into the shared memory.
* Return value: returns the pointer to 
* PerPageChkPtTableEntry_Shr structure.
*/
PerPageChkPtTableEntry_Shr *copy_checkpoint_into_shared_mem(void)
{
  void *checkpoint_perpage_table_shr_mem;
  int i;
  int var_hashcode;

  NSDL1_VARS(NULL, NULL, "Method called. total_checkpoint_entries = %d, total_perpagechkpt_entries = %d", total_checkpoint_entries, total_perpagechkpt_entries);

  /* insert the CheckPointTableEntry_Shr and the PerPageChkPtTableEntry_shr */
  if (total_checkpoint_entries + total_perpagechkpt_entries) {
  
    checkpoint_perpage_table_shr_mem = do_shmget(total_checkpoint_entries * sizeof(CheckPointTableEntry_Shr) +
						 total_perpagechkpt_entries * sizeof(PerPageChkPtTableEntry_Shr), "checkpoint tables");
    checkpoint_table_shr_mem = checkpoint_perpage_table_shr_mem;
    for (i = 0; i < total_checkpoint_entries; i++) {
      /*Manish
      if(checkPointTable[i].text_pfx == -1)  // This should never happen as parsing is checking it
      {
        fprintf(stderr, "Error: Text or TestPfx is not given for checkpoint\n");
        exit(-1);
      }*/ 
        // checkpoint_table_shr_mem[i].text_pfx = NULL;
      // else
      
      if (checkPointTable[i].text_pfx == -1)
        checkpoint_table_shr_mem[i].text_pfx = NULL;
      else
        checkpoint_table_shr_mem[i].text_pfx = BIG_BUF_MEMORY_CONVERSION(checkPointTable[i].text_pfx);

      if(checkPointTable[i].text_sfx == -1) //ie it is TEXT
        checkpoint_table_shr_mem[i].text_sfx = NULL; //make it NULL in case of TEXT
      else 
        checkpoint_table_shr_mem[i].text_sfx = BIG_BUF_MEMORY_CONVERSION(checkPointTable[i].text_sfx);

      checkpoint_table_shr_mem[i].id = BIG_BUF_MEMORY_CONVERSION(checkPointTable[i].id);
      if(checkPointTable[i].save_count_var == -1) {
        checkpoint_table_shr_mem[i].save_count_var = NULL;
      } else {
         checkpoint_table_shr_mem[i].save_count_var = BIG_BUF_MEMORY_CONVERSION(checkPointTable[i].save_count_var);
         var_hashcode = gSessionTable[checkPointTable[i].sess_idx].var_hash_func(checkpoint_table_shr_mem[i].save_count_var, strlen(checkpoint_table_shr_mem[i].save_count_var));
         if(var_hashcode == -1) {
           print_core_events((char*)__FUNCTION__, __FILE__, 
                             "Error: Undeclared SaveCount variable"
                             " name used in checkpoint. Variable name:%s",
                             checkpoint_table_shr_mem[i].save_count_var);
           //checkpoint_table_shr_mem[i].save_count_var = NULL;  
           NS_EXIT(-1, CAV_ERR_1012019_MSG, checkpoint_table_shr_mem[i].save_count_var, 
                   BIG_BUF_MEMORY_CONVERSION(gSessionTable[checkPointTable[i].sess_idx].sess_name));
         }
       }

       if(checkPointTable[i].compare_file == -1) //compare file exist
       {
         checkpoint_table_shr_mem[i].compare_file = NULL; //make it NULL in case of CompareFile
         checkpoint_table_shr_mem[i].compare_file_size = -1;
       }
       else 
       {
         checkpoint_table_shr_mem[i].compare_file = BIG_BUF_MEMORY_CONVERSION(checkPointTable[i].compare_file);
         checkpoint_table_shr_mem[i].compare_file_size = checkPointTable[i].compare_file_size;
       }

       if(checkPointTable[i].checksum_cookie == -1)
         checkpoint_table_shr_mem[i].checksum_cookie = NULL;       
       else
         checkpoint_table_shr_mem[i].checksum_cookie = BIG_BUF_MEMORY_CONVERSION(checkPointTable[i].checksum_cookie);

       if (checkPointTable[i].checksum == -1)
         checkpoint_table_shr_mem[i].checksum = NULL;
       else
         checkpoint_table_shr_mem[i].checksum = BIG_BUF_MEMORY_CONVERSION(checkPointTable[i].checksum);
       
       checkpoint_table_shr_mem[i].action_on_fail = checkPointTable[i].action_on_fail;
       checkpoint_table_shr_mem[i].fail = checkPointTable[i].fail;
       checkpoint_table_shr_mem[i].report = checkPointTable[i].report;
       checkpoint_table_shr_mem[i].ignorecase_textpfx = checkPointTable[i].ignorecase_textpfx;
       checkpoint_table_shr_mem[i].ignorecase_textsfx = checkPointTable[i].ignorecase_textsfx;
       checkpoint_table_shr_mem[i].regexp_textpfx = checkPointTable[i].regexp_textpfx;
       checkpoint_table_shr_mem[i].regexp_textsfx = checkPointTable[i].regexp_textsfx;
       checkpoint_table_shr_mem[i].preg_textpfx = checkPointTable[i].preg_textpfx;
       checkpoint_table_shr_mem[i].preg_textsfx = checkPointTable[i].preg_textsfx;
       checkpoint_table_shr_mem[i].search = checkPointTable[i].search;
       checkpoint_table_shr_mem[i].search_in = checkPointTable[i].search_in;
    }

    perpagechkpt_table_shr_mem = checkpoint_perpage_table_shr_mem + (total_checkpoint_entries * sizeof(CheckPointTableEntry_Shr));
    for (i = 0; i < total_perpagechkpt_entries; i++) {
      perpagechkpt_table_shr_mem[i].checkpoint_ptr = checkpoint_table_shr_mem + perPageChkPtTable[i].checkpoint_idx;
    }
  }
  return perpagechkpt_table_shr_mem;
}



/*This function will search the precompiled pattern in the 
* string.
* Argument:
*    preg: Precompiled pattern. 
*    buf_ptr_tmp: Pointer to the char pointer.
* Return Value: return 0 on match found and advance the pointer for next search.
* else return 1.
*/
int
my_regexec(regex_t preg, char **buf_ptr_tmp)
{
  regmatch_t pmatch[1];
  int status;
  int eflag = 0;

  status = regexec(&preg, *buf_ptr_tmp, 1, pmatch, eflag);
  if(status == 0) { // Found
    NSDL3_VARS(NULL, NULL, "match found at: %d, string=%s",pmatch[0].rm_so, *buf_ptr_tmp + pmatch[0].rm_so);
    *buf_ptr_tmp += pmatch[0].rm_eo;//update the buf_ptr_temp for next search
  }
  return status;
}

/*This function will search the precompiled pattern in the * string.
* Argument:
*    buf_ptr_tmp: Pointer to the char pointer.
*    str: string to be searched. 
* Return Value: return 0 on match found and advance the pointer for next search.
* else return 1.
*/

/* return 0 if string found else return 1 */
int
my_strstr(char **buf_ptr_tmp, char *str)
{
  char *strstr_ptr;
  strstr_ptr =  strstr(*buf_ptr_tmp, str);
  if(strstr_ptr != NULL) {
    *buf_ptr_tmp = strstr_ptr + strlen(str); /* advance the pointer */ 
    return 0;//found
  }
  return 1; // not found

}

//Same function as above + ignore case in strstr
int
my_strcasestr(char **buf_ptr_tmp, char *str)
{
  char *strstr_ptr;
  strstr_ptr =  nslb_strcasestr(*buf_ptr_tmp, str);
  if(strstr_ptr != NULL) {
    *buf_ptr_tmp = strstr_ptr + strlen(str); /* advance the pointer */ 
    return 0;//found
  }
  return 1; // not found

}

/* TODO - Keep this is the data structure so that we do not make this string every time */
char *checkpoint_to_str(CheckPointTableEntry_Shr* checkpoint_ptr, char *buf) {
  NSDL2_VARS(NULL, NULL, "Method Called.");
  char param_name[256 + 1] = "";
  char *search_ptr = NULL;
  char *search_suffix_ptr = NULL;

  if(checkpoint_ptr->text_pfx) {
    //BugId: 52155 Add this to evaluate value when text_pfx or text_sfx are of VAR_TXT, VAR_PFX, or VAR_PFX_SFX type
    sprintf(param_name, "{%s}", checkpoint_ptr->text_pfx);
    search_ptr = ns_eval_string(param_name);
    sprintf(buf, "(TEXT = %s;", search_ptr);
  }

  if(checkpoint_ptr->text_pfx && checkpoint_ptr->text_sfx) {
    //BugId: 52155 Add this to evaluate value when text_pfx or text_sfx are of VAR_TXT, VAR_PFX, or VAR_PFX_SFX type
    sprintf(param_name, "{%s}", checkpoint_ptr->text_pfx);
    search_ptr = ns_eval_string(param_name);
    sprintf(param_name, "{%s}", checkpoint_ptr->text_sfx);
    search_suffix_ptr =  ns_eval_string(param_name);
    sprintf(buf, "(TextPfx = %s; TextSfx = %s;", search_ptr, search_suffix_ptr);
  }

  sprintf(buf, "%s Msg = '%s';", buf, ns_eval_string(checkpoint_ptr->id));

  if(checkpoint_ptr->action_on_fail == NS_CP_ACTION_STOP)
    sprintf(buf, "%s  %s;", buf, "ActionOnFail = Stop");
  else
    sprintf(buf, "%s  %s;", buf, "ActionOnFail = Continue");

  if(checkpoint_ptr->fail == NS_CP_FAIL_ON_FOUND)
    sprintf(buf, "%s %s", buf, "Fail = Found");
  else if(checkpoint_ptr->fail == NS_CP_FAIL_ON_NOT_FOUND)
    sprintf(buf, "%s %s", buf, "Fail = NotFound");
  else
    sprintf(buf, "%s %s", buf, "Fail = Ignore");

  sprintf(buf, "%s)", buf);

  NSDL2_VARS(NULL, NULL, "buf = %s", buf);
  return buf;
}

/*Manish*/
static void process_text(CheckPointTableEntry_Shr* checkpoint_ptr, int *count, VUser *vptr, char *full_buffer, int blen)
{
  int text_pfx_length;
  int text_sfx_length;
  //int lb_found, rb_found;
  char *buf_ptr = NULL, *end_ptr = NULL;
  //int blen = vptr->bytes;
  int ret;
  char param_name[256] = "";
  char *search_ptr = NULL;
  char *search_suffix_ptr = NULL;
  int used_hdr_buf_len;
  char *hdr_buffer;

  //If Response Header saved in Vptr then only apply check point in header/all response
  if(vptr->response_hdr)
  {
     hdr_buffer = vptr->response_hdr->hdr_buffer;
     used_hdr_buf_len = vptr->response_hdr->used_hdr_buf_len;
  }
  else
  { 
     used_hdr_buf_len=0;
     hdr_buffer = NULL;
  }
  // Init variable for each check point
  //lb_found = rb_found = 0;

  //BugId: 51978- Search text in header, body, or full response.
  //For Inline url: Search only in response body as we do not save header for inline url
  if(checkpoint_ptr->search_in == NS_CP_SEARCH_ALL) {  //Header + Body
    buf_ptr = full_buffer;
    end_ptr = buf_ptr + blen;
  }
  else if(checkpoint_ptr->search_in == NS_CP_SEARCH_HEADER) { //Header only
    buf_ptr = hdr_buffer;
    end_ptr = buf_ptr + used_hdr_buf_len;
  }
  else { // checkpoint_ptr->search_in == NS_CP_SEARCH_BODY   //Body Only (Default) 
    if(vptr->cur_page->save_headers & SEARCH_IN_HEADER)
      buf_ptr = full_buffer + used_hdr_buf_len;
    else
      buf_ptr = full_buffer;
    end_ptr = full_buffer + blen;
  }

  // No need to apply check point if there in no buffer is available
  if(!buf_ptr) {
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Not able to apply checkpoint as we do not have any response");
    return;
  }

  long len = 0;

  NSDL2_VARS(vptr, NULL, "checkpoint_ptr = %p, *count = %d, full_buffer = [%s] blen = [%d]", checkpoint_ptr, *count, full_buffer, blen);

  if (checkpoint_ptr->text_pfx) { // Safety check. This is always there
    /* Bug 43100
       Searched string can be a parameter, in case of parameter use ns_eval_string for getting the value
       the value of parameter */
    if (checkpoint_ptr->search == NS_CP_SEARCH_VAR_TXT || checkpoint_ptr->search == NS_CP_SEARCH_VAR_PFX || 
        checkpoint_ptr->search == NS_CP_SEARCH_VAR_PFX_SFX)
    {
      sprintf(param_name, "{%s}", checkpoint_ptr->text_pfx);
      char *tmp_ptr = NULL;
      tmp_ptr = ns_eval_string_flag_internal(param_name, 0, &len, vptr);
      text_pfx_length = len;
      if(!text_pfx_length)
      { 
        NSDL3_VARS(vptr, NULL, "pfx doesn't contain any value hence returing");
        return;
      }
      if( text_pfx_length > ns_nvm_scratch_buf_size)
      { 
        MY_REALLOC(ns_nvm_scratch_buf, len + 1, "reallocating for pfx checkpoint parameter", -1);
        ns_nvm_scratch_buf_size = text_pfx_length;
      }
      strcpy(ns_nvm_scratch_buf, tmp_ptr);
      search_ptr = ns_nvm_scratch_buf; 
    }
    else {
      search_ptr = checkpoint_ptr->text_pfx; 
      text_pfx_length = strlen(search_ptr);
   }
    NSDL3_VARS(vptr, NULL, "string length of TextPfx/TEXT, text_pfx_length = %d checkpoint_ptr->text_pfx = %s", text_pfx_length, 
    search_ptr);
  } else  {
      text_pfx_length = 0;
      NSDL3_VARS(vptr, NULL, "string length of TextPfx/TEXT, text_pfx_length = %d", text_pfx_length);
  }

  if (checkpoint_ptr->text_sfx) { // This is optional (For TEXT case)
    /* Bug 43100
       TextSfx can be a parameter, in case of parameter use ns_eval_string for getting the value
       the value of parameter */
    if (checkpoint_ptr->search == NS_CP_SEARCH_VAR_SFX || checkpoint_ptr->search == NS_CP_SEARCH_VAR_PFX_SFX)
    {
       sprintf(param_name, "{%s}", checkpoint_ptr->text_sfx);
       search_suffix_ptr =  ns_eval_string_flag_internal(param_name, 0, &len, vptr);;  
       text_sfx_length = len;
       if(!text_sfx_length)
       {
         NSDL3_VARS(vptr, NULL, "sfx doesn't contain any value hence returing");
         return;
       }
    }
    else { 
      search_suffix_ptr =  checkpoint_ptr->text_sfx;
      text_sfx_length = strlen(search_suffix_ptr);
    }
    NSDL3_VARS(vptr, NULL, "string length of Textsfx, text_sfx_length = %d", text_sfx_length);
  } else {
      text_sfx_length = 0;
      NSDL3_VARS(vptr, NULL, "string length of Textsfx, text_sfx_length = %d", text_sfx_length);
  }
    
  
  while (buf_ptr < end_ptr) {
    NSDL4_VARS(vptr, NULL, "Starting search for checkpoint. Iteration = %d", *count + 1);
    /* TEXT/TextPfx Processing - First find Text/TextPfx */
    if (text_pfx_length) {
      if (checkpoint_ptr->regexp_textpfx)
        ret = my_regexec(checkpoint_ptr->preg_textpfx, &buf_ptr); 
      else if(checkpoint_ptr->ignorecase_textpfx)
        ret =  my_strcasestr(&buf_ptr, search_ptr);
      else /* strstr */
        ret = my_strstr(&buf_ptr, search_ptr);

      NSDL3_VARS(vptr, NULL, "After TEXT/TextPfx - buf_ptr = %p, end_ptr = %p\n", buf_ptr, end_ptr);
      if(ret == 0) { /* found */ 
        NSDL3_VARS(vptr, NULL, "Got the TextPfx/TEXT, text_pfx_length = %d", text_pfx_length);
      } else {
          NSDL3_VARS(vptr, NULL, "Did not got the TextPfx/TEXT, text_pfx_length = %d", text_pfx_length);
          break; 
      }
    } 
  
    NSDL3_VARS(vptr, NULL, "text_sfx_length = %d, regexp_textsfx = %d", text_sfx_length, checkpoint_ptr->regexp_textsfx);
    /* TextSfx Processing */
    if (text_sfx_length) {
      if (checkpoint_ptr->regexp_textsfx)
        ret = my_regexec(checkpoint_ptr->preg_textsfx, &buf_ptr);  
      else 
        ret = my_strstr(&buf_ptr, search_suffix_ptr);

      NSDL3_VARS(vptr, NULL, "buf_ptr = %p, end_ptr = %p\n", buf_ptr, end_ptr);
      if (ret == 0) {     /*found */
        NSDL3_VARS(vptr, NULL, "Got the TextSfx, text_sfx_length = %d\n", text_sfx_length);
      } else {
          NSDL3_VARS(vptr, NULL, "Did not got the TextSfx, text_sfx_length = %d", text_sfx_length);
          break; 
      }
    }


    // At this point - one match found
    (*count)++;
    NSDL3_VARS(vptr, NULL, "Count = %d, text_pfx_length = %d, text_sfx_length = %d", *count, text_pfx_length, text_sfx_length);

    // This is to optimize as it save_count_var is not defined, then no need to search again
    if(checkpoint_ptr->save_count_var == NULL) break;
  } /* End of while for search for one check point */
}

static void process_compare_file(CheckPointTableEntry_Shr* checkpoint_ptr, int *count, VUser *vptr, char *full_buffer, int blen)
{
  char* buf_ptr;
  //int blen = vptr->bytes;
  int used_hdr_buf_len;
  char *hdr_buffer;
  
  NSDL2_VARS(vptr, NULL, "checkpoint_ptr = %p, *count = %d, blen = %d, full_buffer = [%s]", 
                          checkpoint_ptr, *count, blen, full_buffer);
  // Init variable for each check point
  //buf_ptr = full_buffer;

  //BugId: 51978- Search text in header, body, or full response.
  //If Response Header saved in Vptr then only apply check point in header/all response
  if(vptr->response_hdr)
  {
    hdr_buffer = vptr->response_hdr->hdr_buffer;
    used_hdr_buf_len = vptr->response_hdr->used_hdr_buf_len;
  }
  else
  { 
    used_hdr_buf_len=0;
    hdr_buffer = NULL;
  }
  if(checkpoint_ptr->search_in == NS_CP_SEARCH_ALL) // Header + Body
    buf_ptr = full_buffer;
  
  else if(checkpoint_ptr->search_in == NS_CP_SEARCH_HEADER) // Header Only
    buf_ptr = hdr_buffer;
  
  else // checkpoint_ptr->search_in == NS_CP_SEARCH_BODY 
  {
    if(vptr->cur_page->save_headers & SEARCH_IN_HEADER)
      buf_ptr = full_buffer + used_hdr_buf_len;  //Body (Default)
    else
      buf_ptr = full_buffer;
  }

  // No need to apply check point if there in no buffer is available
  if(!buf_ptr) {
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Not able to apply checkpoint as we do not have any response");
    return;
  }

  NSDL2_VARS(vptr, NULL, "compare_file_size = %d, compare_file = [%s]", 
                     checkpoint_ptr->compare_file_size, checkpoint_ptr->compare_file);
  if(checkpoint_ptr->compare_file_size != blen) 
    *count = 0;
  else if(!memcmp(checkpoint_ptr->compare_file, buf_ptr, checkpoint_ptr->compare_file_size))
  {
    *count = 1;
  } 
}

#define NS_MD5_CHECKSUM_BYTES 16

static void process_checksum(CheckPointTableEntry_Shr* checkpoint_ptr, int *count, VUser *vptr, char *full_buffer, int blen)
{
  char* buf_ptr;
  //int blen = vptr->bytes;
  char checksum_buf[2 * NS_MD5_CHECKSUM_BYTES + 1];
  int used_hdr_buf_len;
  char *hdr_buffer;

  NSDL2_VARS(vptr, NULL, "checkpoint_ptr = %p, *count = %d, blen = %d, full_buffer = [%s]", 
                          checkpoint_ptr, *count, blen, full_buffer);
  // Init variable for each check point
  //buf_ptr = full_buffer;

  //BugId: 51978- Search text in header, body, or full response.
  //If Response Header saved in Vptr then only apply check point in header/all response
  if(vptr->response_hdr)
  {
    hdr_buffer = vptr->response_hdr->hdr_buffer;
    used_hdr_buf_len = vptr->response_hdr->used_hdr_buf_len;
  }
  else
  { 
    used_hdr_buf_len=0;
    hdr_buffer = NULL;
  }
  if(checkpoint_ptr->search_in == NS_CP_SEARCH_ALL) // Header + Body
    buf_ptr = full_buffer;
  
  else if(checkpoint_ptr->search_in == NS_CP_SEARCH_HEADER) // Header Only
    buf_ptr = hdr_buffer;
  
  else // checkpoint_ptr->search_in == NS_CP_SEARCH_BODY   // Body (Default)
  {
    if(vptr->cur_page->save_headers & SEARCH_IN_HEADER)
      buf_ptr = full_buffer + used_hdr_buf_len;
    else
      buf_ptr = full_buffer;
  }

  // No need to apply check point if there in no buffer is available
  if(!buf_ptr) {
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Not able to apply checkpoint as we do not have any response");
    return;
  }
 
  ns_gen_md5_checksum((unsigned char *)buf_ptr, blen, (unsigned char *)checksum_buf);

  NSDL2_VARS(vptr, NULL, "checksum_buf = [%s], checkpoint_ptr->checksum = [%s]", 
                      checksum_buf, checkpoint_ptr->checksum);
  
  if (!strcmp(checksum_buf, checkpoint_ptr->checksum)) 
    *count = 1;
  else
    *count = 0;
}

static void process_checksum_cookie(CheckPointTableEntry_Shr* checkpoint_ptr, int *count, VUser *vptr, char *full_buffer, int blen)
{
  char* buf_ptr;
  //int blen = vptr->bytes;
  char checksum_buf[2 * NS_MD5_CHECKSUM_BYTES + 1]; 
  int used_hdr_buf_len;
  char *hdr_buffer;
 
  NSDL2_VARS(vptr, NULL, "checkpoint_ptr = %p, *count = %d, blen = %d, full_buffer = [%s]", 
                          checkpoint_ptr, *count, blen, full_buffer);
  // Init variable for each check point
  //buf_ptr = full_buffer;
 
  //BugId: 51978- Search text in header, body, or full response.
  //If Response Header saved in Vptr then only apply check point in header/all response
  if(vptr->response_hdr)
  {
    hdr_buffer = vptr->response_hdr->hdr_buffer;
    used_hdr_buf_len = vptr->response_hdr->used_hdr_buf_len;
  }
  else
  { 
    used_hdr_buf_len=0;
    hdr_buffer = NULL;
  }
  if(checkpoint_ptr->search_in == NS_CP_SEARCH_ALL) // Header + Body
    buf_ptr = full_buffer;
  
  else if(checkpoint_ptr->search_in == NS_CP_SEARCH_HEADER) // Header Only
    buf_ptr = hdr_buffer;
  
  else // checkpoint_ptr->search_in == NS_CP_SEARCH_BODY // Body (Default) 
  {
    if(vptr->cur_page->save_headers & SEARCH_IN_HEADER)
      buf_ptr = full_buffer + used_hdr_buf_len;
    else
      buf_ptr = full_buffer;
  }

  //No need to apply check point if there in no buffer is available
  if(!buf_ptr) {
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Not able to apply checkpoint as we do not have any response");
    return;
  }

  ns_gen_md5_checksum((unsigned char *)buf_ptr, blen, (unsigned char *)checksum_buf); 
  char *cav_body_check_sum = ns_get_cookie_val_ex(checkpoint_ptr->checksum_cookie, NULL, NULL);

  if(cav_body_check_sum == NULL)
  {
    NSDL2_VARS(vptr, NULL, "Error - Cookie %s is not present in response", checkpoint_ptr->checksum_cookie);
    *count = 0;
    return;
  }

  NSDL2_VARS(vptr, NULL, "Checksum calculated from resp body = %s, Checksum from cookie = %s", 
                                checksum_buf, cav_body_check_sum);

  if (strcmp(cav_body_check_sum, checksum_buf) !=0 )
  {
    NSDL2_VARS(vptr, NULL, "CheckSumCookieStatus: Fail. Checksum from cookie = %s, checksum of body = %s", cav_body_check_sum, checksum_buf);
    *count = 0;
  }
  else
  {
    NSDL2_VARS(vptr, NULL, "CheckSumStatus: passed. Checksum from cookie = %s, checksum of body = %s", cav_body_check_sum, checksum_buf);
    *count = 1;
  }
}

// Also called from check point API
// to_stop and page_fail must NOT be set to 0 here as these need retain non 0 value across multiple calls of this method
inline void apply_checkpoint(CheckPointTableEntry_Shr* checkpoint_ptr, int *count, int *page_fail, int *to_stop, char *full_buffer, int full_buffer_len, VUser *vptr, connection *cptr)
{
  int check_point_fail;
  char save_count_str[10];
  char param_name[256 + 1] = "";
  char param_suffix_name[256 + 1] = "";

  NSDL2_VARS(vptr, NULL, "Method Called");
  if (checkpoint_ptr->text_pfx && checkpoint_ptr->text_sfx) { 
    sprintf(param_name, "{%s}", checkpoint_ptr->text_pfx);
    sprintf(param_suffix_name, "{%s}", checkpoint_ptr->text_sfx);
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Applying Checkpoint TextPfx = %s, TextSfx = %s, ID = %s", ns_eval_string(param_name), ns_eval_string(param_suffix_name), checkpoint_ptr->id);
  }
  else if (checkpoint_ptr->text_pfx && !checkpoint_ptr->text_sfx) {
    sprintf(param_name, "{%s}", checkpoint_ptr->text_pfx);
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Applying Checkpoint Text = %s, ID = %s", ns_eval_string(param_name), checkpoint_ptr->id);
  }

  // Must set to 0 first
  *count = check_point_fail = 0;
  if(checkpoint_ptr->save_count_var) {
    NSDL4_VARS(vptr, NULL, "Initializing value of save count var = %d", count);
    sprintf(save_count_str, "%d", *count);
    ns_save_string(save_count_str, checkpoint_ptr->save_count_var); 
  }
 
  if(checkpoint_ptr->text_pfx != NULL)
    process_text(checkpoint_ptr, count, vptr, full_buffer, full_buffer_len);
  else if (checkpoint_ptr->compare_file != NULL)
    process_compare_file(checkpoint_ptr, count, vptr, full_buffer, full_buffer_len);
  else if (checkpoint_ptr->checksum != NULL)
    process_checksum(checkpoint_ptr, count, vptr, full_buffer, full_buffer_len);
  else if (checkpoint_ptr->checksum_cookie != NULL)
    process_checksum_cookie(checkpoint_ptr, count, vptr, full_buffer, full_buffer_len);

  // At this point, this check point search is over.
  /* save the save count value to the user defined variable */
  if(checkpoint_ptr->save_count_var) {
    sprintf(save_count_str, "%d", *count);
    NSDL2_VARS(vptr, NULL, "Saving count in SaveCount variable. SaveCount vaiable name = %s, count = %s", checkpoint_ptr->save_count_var, save_count_str);
    ns_save_string(save_count_str, checkpoint_ptr->save_count_var); 
  } 

  /* If this checkpoint is failing and action is to stop, then set to_stop to 1
    *  Note - page_fail, check_point_fail and to_stop should be set one time
    *  Once set, this should remain set
  */
  NSDL4_VARS(vptr, NULL,  "Before checking check point the conditions: Count = %d, to_stop = %d, checkpoint_ptr->fail=%d,checkpoint_ptr->action_on_fail=%d", *count, *to_stop,  checkpoint_ptr->fail, checkpoint_ptr->action_on_fail);

  if(*count && (checkpoint_ptr->fail == NS_CP_FAIL_ON_FOUND)) {
    *page_fail = 1;
    check_point_fail = 1;
    if(checkpoint_ptr->action_on_fail == NS_CP_ACTION_STOP) {
      NSDL3_VARS(vptr, NULL, "Checkpoint failed. Checkpoint found the string and it is fail on found and action is Stop. Count = %d", *count);
      *to_stop =  1;
    } else
      NSDL3_VARS(vptr, NULL, "Checkpoint failed. Checkpoint found the string and it is fail on found and action is Continue. Count = %d", count);
  } else if(!*count && (checkpoint_ptr->fail == NS_CP_FAIL_ON_NOT_FOUND)) {
    *page_fail = 1;
    check_point_fail = 1;
    if (checkpoint_ptr->action_on_fail == NS_CP_ACTION_STOP) {
      NSDL3_VARS(vptr, NULL, "Checkpoint failed. Checkpoint did not find the string and it is fail on not found and action is Stop. Count = %d", count);
      *to_stop =  1; 
    } else
      NSDL3_VARS(vptr, NULL, "Checkpoint failed. Checkpoint did not find the string and it is fail on not found and action is Continue. Count = %d", *count);
  }

  NSDL3_VARS(vptr, NULL, "check_point_fail = %d", check_point_fail);
  if(check_point_fail) {
    //If fails then output should go on screen. 
    checkpoint_to_str(checkpoint_ptr, ns_nvm_scratch_buf);

    char msg[1024 + 1];
    int write_idx;
    if(checkpoint_ptr->fail == NS_CP_FAIL_ON_NOT_FOUND)
    {
      write_idx = snprintf(msg, 1024, "ContentValidationError: Response did not contain the expected Text for Page '%s' of Script '%s'",
                           vptr->cur_page->page_name, vptr->sess_ptr->sess_name);
    }
    else
    {
      write_idx = snprintf(msg, 1024, "ContentValidationError: Response contains unexpected Text for Page '%s' of Script '%s'",
                           vptr->cur_page->page_name, vptr->sess_ptr->sess_name);

    }
    if (send_events_to_master == 1)
      snprintf(msg + write_idx, 1024 - write_idx, " from Generator '%s'", global_settings->event_generating_host);

    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "%s %s\n", msg, ns_nvm_scratch_buf);

    NS_EL_4_ATTR(EID_HTTP_PAGE_ERR_START + NS_REQUEST_CV_FAILURE, vptr->user_index,
                 vptr->sess_inst,
                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                 vptr->cur_page->page_name,
                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                 ns_nvm_scratch_buf,
                 msg);

    //Handle RBU case
    //Resolved Bug 28130 - RBU-Core | Alert is required whenever there is a failure in login credentials
    NSDL2_VARS(vptr, NULL, "check_point_fail = %d, page_fail = %d, rbu_enable = %d",
               check_point_fail, page_fail, (global_settings->protocol_enabled & RBU_API_USED));

    if((global_settings->protocol_enabled & RBU_API_USED) && page_fail)
    {
      char buf[2048 + 1];

      snprintf(buf, 2048, "%s %s", msg, ns_nvm_scratch_buf);

      NSDL2_RBU(vptr, NULL, "err_msg = %s", buf);
      make_msg_and_send_alert(vptr, buf);
      //For NetTest
      NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Warning; flowName=%s; Page=%s; Session=%s; "
                 "MSG=%s %s",
                 get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"), vptr->cur_page->flow_name,
                 vptr->cur_page->page_name, vptr->sess_ptr->sess_name, msg, ns_nvm_scratch_buf);
    }
  } else {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Content check (%s) passed\n", ns_eval_string (checkpoint_ptr->id));
  }

}

/*Purpose:This function processes all checkpoint defined on the page.
*         This will print the fail output on the screen if the checkpoint is failed.
* Arguments:
*     vptr : pointer to the VUser structure.
*     full_buffer : Pointer to the character string that contains the response data.
*     outlen: length
* Return value: None. 
* 
*  Note - This method is NOT called if page has already failed either due to serach parameter or InLine check point using C API
*/
void process_checkpoint_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int full_buffer_len, int *outlen)
{
  int i;
  PerPageChkPtTableEntry_Shr* perpagechkpt_ptr;
  int to_stop = 0, page_fail = 0; // Once set, this should remain set
  int count, check_point_fail;
  count = check_point_fail = 0;
  NSDL2_VARS(vptr, NULL, "Method Called.Page name = %s, Script name = %s", 
             vptr->cur_page->page_name, vptr->sess_ptr->sess_name); 

  if (!(vptr->cur_page->first_checkpoint_ptr)) return; // No checkpoints defined

  vptr->httpData->check_pt_fail_start = -1; // Init to -1, it will be used as index of checkpoint for failure 
  for (perpagechkpt_ptr = vptr->cur_page->first_checkpoint_ptr, i = 0; i < vptr->cur_page->num_checkpoint; i++, perpagechkpt_ptr++) 
  {

    apply_checkpoint(perpagechkpt_ptr->checkpoint_ptr, &count, &page_fail, &to_stop, full_buffer, full_buffer_len, vptr, cptr);

    // If first time checkpoint is failed then stores it's ID in to check_pt_fail_start, will be used to save failure msg in pagedump
    if(page_fail == 1 && vptr->httpData->check_pt_fail_start == -1)
      vptr->httpData->check_pt_fail_start = i;

    if(NS_IF_TRACING_ENABLE_FOR_USER){
      checkpoint_to_str(perpagechkpt_ptr->checkpoint_ptr, ns_nvm_scratch_buf);
      ut_add_validation_node(user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail);
      ut_update_validation(user_trace_grp_ptr[vptr->group_num].utd_head->ut_tail, ns_nvm_scratch_buf, ns_eval_string(perpagechkpt_ptr->checkpoint_ptr->id), check_point_fail?"Fail":"Pass");
    }
  } // End of all checkpoints

  // If at least one checkpoint fails, the we need to fail the page and session
  NSDL3_VARS(vptr, NULL, "page_fail = %d, vptr->httpData->check_pt_fail_start = %hi", page_fail, vptr->httpData->check_pt_fail_start);
  if(page_fail) {
    set_page_status_for_registration_api(vptr, NS_REQUEST_CV_FAILURE, to_stop, "CheckPoint");
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Page is failed due to Checkpoint failure."); 

    //In Case of RBU with 'C' type script, Setting CV fail status here.
    if(vptr->httpData && vptr->httpData->rbu_resp_attr)
      vptr->httpData->rbu_resp_attr->cv_status = NS_REQUEST_CV_FAILURE; 
  }
}

