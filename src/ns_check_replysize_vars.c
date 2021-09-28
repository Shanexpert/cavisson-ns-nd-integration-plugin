/******************************************************************
 * Name    :    ns_check_reply_size.c
 * Purpose :    nsl_check_reply_size - parsing, shared memory, run time
 * Note    :    nsl_check_reply_size()  is used to  check page response size.
                This API is also called check reply size registration API.
* Syntax  :    nsl_check_reply_size(MODE=<NotBetweenMinMax>,Value=Value1,Value2=value2, Page=page1, Action=<Stop/Continue>,VAR=<var_name>); 
* Author  :    
* Intial version date: 1 Nov 2009  
* Last modification date:6 Nov 2009 
*****************************************************************/

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
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_vars.h"

#include "amf.h"
#include "ns_string.h"
#include "ns_debug_trace.h"
#include "nslb_sock.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
//#include "ns_check_reply_size.h"  //included in util.h
#include "ns_exit.h" 
#include "ns_error_msg.h" 
#ifndef CAV_MAIN
CheckReplySizeTableEntry *checkReplySizeTable;
CheckReplySizeTableEntry_Shr *checkReplySizeTable_shr_mem = NULL;
static int total_checkreplysize_entries = 0;
static int max_checkreplysize_entries = 0;
static int total_perpagechkrepsize_entries = 0;
int max_perpagechkrepsize_entries = 0;
#else
__thread CheckReplySizeTableEntry *checkReplySizeTable;
__thread CheckReplySizeTableEntry_Shr *checkReplySizeTable_shr_mem = NULL;
__thread int total_checkreplysize_entries = 0;
static __thread int max_checkreplysize_entries = 0;
__thread int total_perpagechkrepsize_entries = 0;
__thread int max_perpagechkrepsize_entries = 0;
#endif
static int create_check_replysize_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_checkreplysize_entries = %d, max_checkreplysize_entries = %d", total_checkreplysize_entries, max_checkreplysize_entries);
  if (total_checkreplysize_entries == max_checkreplysize_entries) 
  {
    MY_REALLOC (checkReplySizeTable, (max_checkreplysize_entries + DELTA_CHECK_REPLYSIZE_ENTRIES) * sizeof(CheckPointTableEntry), "checkreplysize entries", total_checkreplysize_entries);
    max_checkreplysize_entries += DELTA_CHECK_REPLYSIZE_ENTRIES;
  }
  *row_num = total_checkreplysize_entries++;
  return (SUCCESS);
}

static int create_check_replysize_page_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_check_replysize_page_entries = %d, max_check_replysize_page_entries = %d", total_check_replysize_page_entries, max_check_replysize_page_entries);

  if (total_check_replysize_page_entries == max_check_replysize_page_entries) 
    {
      MY_REALLOC (checkReplySizePageTable, (max_check_replysize_page_entries + DELTA_CHECK_REPLYSIZE_PAGE_ENTRIES) * sizeof(CheckReplySizePageTableEntry), "checkreplysize page entries", total_check_replysize_page_entries);
      max_check_replysize_page_entries += DELTA_CHECK_REPLYSIZE_PAGE_ENTRIES;
    }
  *row_num = total_check_replysize_page_entries++;
  return (SUCCESS);
}

void init_check_replysize_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_checkreplysize_entries = 0;
  total_perpagechkrepsize_entries = 0;

  MY_MALLOC (checkReplySizeTable, INIT_CHECK_REPLYSIZE_ENTRIES * sizeof(CheckReplySizeTableEntry), "checkReplySizeTable", -1);

  MY_MALLOC (checkReplySizePageTable, INIT_CHECK_REPLYSIZE_PAGE_ENTRIES * sizeof(CheckReplySizePageTableEntry), "checkReplySizePageTable", -1);

  if(checkReplySizeTable && checkReplySizePageTable)
  {
    max_checkreplysize_entries = INIT_CHECK_REPLYSIZE_ENTRIES;
    max_perpagechkrepsize_entries = INIT_PERPAGECHK_REPSIZE_ENTRIES;
  }
  else
  {
    max_checkreplysize_entries = 0;
    max_perpagechkrepsize_entries = 0;
    NS_EXIT(-1, CAV_ERR_1031012, "CheckReplySizeTableEntry", "CheckReplySizePageTableEntry");
  }
}

int create_perpagechkrepsize_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_perpagechkrepsize_entries = %d, max_perpagechkrepsize_entries = %d", total_perpagechkrepsize_entries, max_perpagechkrepsize_entries); 

  if (total_perpagechkrepsize_entries == max_perpagechkrepsize_entries) 
    {
      MY_REALLOC (perPageChkRepSizeTable, (max_perpagechkrepsize_entries + DELTA_PERPAGECHK_REPSIZE_ENTRIES) * sizeof(PerPageCheckReplySizeTableEntry), "perpagechkrepsize entries", total_perpagechkrepsize_entries);
      max_perpagechkrepsize_entries += DELTA_PERPAGECHK_REPSIZE_ENTRIES;
    }
  *row_num = total_perpagechkrepsize_entries++;
  return (SUCCESS);
}

int check_replysize_page_cmp(const void* ent1, const void* ent2) 
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  if (((CheckReplySizePageTableEntry *)ent1)->page_idx < ((CheckReplySizePageTableEntry *)ent2)->page_idx)
    return -1;
  else if (((CheckReplySizePageTableEntry *)ent1)->page_idx > ((CheckReplySizePageTableEntry *)ent2)->page_idx)
    return 1;
  else if (((CheckReplySizePageTableEntry *)ent1)->checkreplysize_idx < ((CheckReplySizePageTableEntry *)ent2)->checkreplysize_idx)
    return -1;
  else if (((CheckReplySizePageTableEntry *)ent1)->checkreplysize_idx > ((CheckReplySizePageTableEntry *)ent2)->checkreplysize_idx)
    return 1;
  else return 0;
}

int process_check_replysize_table_per_session(int session_id, char* err_msg) 
{
  int i,j,k;
  int chkpage_idx;

  NSDL1_VARS(NULL, NULL, "Method called. total_sess_entries = %d total_check_replysize_page_entries = %d, session_id = %d", total_sess_entries, total_check_replysize_page_entries, session_id);

  //Create page checkreplysize table  based on PAGEALL checkreplysize entried
  //for (i = 0; i < total_sess_entries; i++) 
  i = session_id;
  {
  for (j = gSessionTable[i].checkreplysize_start_idx; j < (gSessionTable[i].checkreplysize_start_idx + gSessionTable[i].num_checkreplysize_entries); j++) 
  {
    if (checkReplySizeTable[j].pgall) 
    {
      for (k = gSessionTable[i].first_page; k < (gSessionTable[i].first_page + gSessionTable[i].num_pages); k++) 
      {
        if (create_check_replysize_page_table_entry(&chkpage_idx) == -1) 
        {
          snprintf(err_msg, 1024, "Could not create create_check_replysize_page_table_entry. Short on memory");
          return -1;
        }

        NSDL2_VARS(NULL, NULL, "i = %d, checkpage_idx = %d\n", i, chkpage_idx);
        checkReplySizePageTable[chkpage_idx].checkreplysize_idx = j;
        if (gSessionTable[i].checkreplysizepage_start_idx == -1) 
        {
          gSessionTable[i].checkreplysizepage_start_idx = chkpage_idx;
          gSessionTable[i].num_checkreplysizepage_entries = 0;
        }
        gSessionTable[i].num_checkreplysizepage_entries++;
        checkReplySizePageTable[chkpage_idx].page_idx = k;
        NSDL2_VARS(NULL, NULL, "checkreplysizepage_start_idx = %d, checkreplysize_idx = %d, page_idx = %d", gSessionTable[i].checkreplysizepage_start_idx, j, k);
    }
   }
  }
  }

  k = 0;
  //Convert page name to page idx for all check reply size, if needed (For PAGE=ALL, page already has idx)
  //for (i = 0; i < total_sess_entries; i++) 
  i = session_id;
    {
      for (j = gSessionTable[i].checkreplysizepage_start_idx; 
           j < (gSessionTable[i].checkreplysizepage_start_idx + gSessionTable[i].num_checkreplysizepage_entries); j++) 
      {
        NSDL2_VARS(NULL, NULL, "i = %d, j = %d, checkpage_idx = %d, page_idx = %d", 
                 i, j, chkpage_idx, checkReplySizePageTable[j].page_idx);
        if (checkReplySizePageTable[j].page_idx == -1 ) 
        {
          if ((checkReplySizePageTable[j].page_idx = find_page_idx(RETRIEVE_TEMP_BUFFER_DATA(checkReplySizePageTable[j].page_name), i)) == -1)
            snprintf(err_msg, 1024, "unknown page %s for the checkreplysize", 
                        RETRIEVE_TEMP_BUFFER_DATA(checkReplySizePageTable[j].page_name));
        }
    k++;
      }
    }
  //assert (k == total_check_replysize_page_entries);


#if 0
  //Create per page checkreplysize table  based on PAGEALL checkreplysize entried
  for (i = 0; i < total_sess_entries; i++) 
    {
      for (j = gSessionTable[i].checkreplysize_start_idx; j < (gSessionTable[i].checkreplysize_start_idx + gSessionTable[i].num_checkreplysize_entries); j++) 
  {
    if (checkReplySizeTable[j].pgall) 
      {
        for (k = gSessionTable[i].first_page; k < (gSessionTable[i].first_page + gSessionTable[i].num_pages); k++) 
    {
      if (create_perpagechkrepsize_table_entry(&rnum) != SUCCESS) 
        {
          fprintf(stderr, "Unable to create perpage checkreplysize table entry\n");
          return -1;
        }
      if (gPageTable[k].first_check_replysize_idx == -1)
        gPageTable[k].first_check_replysize_idx = rnum;
      perPageChkRepSizeTable[rnum].checkreplysize_idx = j;
      gPageTable[k].num_check_replysize++;
    }
      }
  }
    }
#endif

  return 0;
}


int process_check_replysize_table() 
{
  int last_checkreplysize_idx;
  int rnum;
  int page_idx;
  int i;

  NSDL1_VARS(NULL, NULL, "Method called");
  //sort  check replysize Page table in the order of page index & check reply size var index
  qsort(checkReplySizePageTable, total_check_replysize_page_entries, sizeof(CheckReplySizePageTableEntry), check_replysize_page_cmp);

  //Create per page checkreplysize table
  //This table has the list of checkreplysize that need to be initialized on specific pages
  //Each page has the start & num entries for this per page check reply size var table entries.
  for (i = 0; i < total_check_replysize_page_entries; i++) {
    if ((page_idx = checkReplySizePageTable[i].page_idx) != -1) 
    {
      NSDL2_VARS(NULL, NULL, "page_idx = %d\n", page_idx);
      if (gPageTable[page_idx].first_check_replysize_idx== -1) 
      {
        if (create_perpagechkrepsize_table_entry(&rnum) != SUCCESS) 
        {
          fprintf(stderr, "Unable to create perpage check reply size table entry\n");
          return -1;
        }
        perPageChkRepSizeTable[rnum].checkreplysize_idx = checkReplySizePageTable[i].checkreplysize_idx;
        gPageTable[page_idx].first_check_replysize_idx = rnum;
        last_checkreplysize_idx = checkReplySizePageTable[i].checkreplysize_idx;
       } 
       else 
       {
         //Remove duplicate entries for same var for same page
         //Note that checkPage Table is ordered in page number order
         if (last_checkreplysize_idx == checkReplySizePageTable[i].checkreplysize_idx)
           return 0; //continue;
         if (create_perpagechkrepsize_table_entry(&rnum) != SUCCESS) 
         {
           fprintf(stderr, "Unable to create perpage checkreplysize table entry\n");
           return -1;
         }
         perPageChkRepSizeTable[rnum].checkreplysize_idx = checkReplySizePageTable[i].checkreplysize_idx;
         last_checkreplysize_idx = checkReplySizePageTable[i].checkreplysize_idx;
       }
       gPageTable[page_idx].num_check_replysize++;
      }
  }
  return 0;
}


/* This function is used to parse the check reply size api values
*Format of API :
*   nsl_check_reply_size(MODE=<NotBetweenMinMax>,Value=Value1,Value2=value2, Page=page1, Action=<Stop/Continue>,VAR=<var_name>);
* Arguments: 
*  line: a pointer to the null terminated string.
*  line_number: script.capture file line number.
* sess_idx: session index.
* Return value: return 0 on suceess and -1 on fail.
*/
int input_check_reply_size_data(char* line, int line_number, int sess_idx, char *script_filename) 
{
  int rnum;
  char data[MAX_LINE_LENGTH]; // Used to store value of argument
  char* line_ptr = line;
  int done = 0;
  int i;
  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char msg[MAX_LINE_LENGTH];
  char page_name[MAX_LINE_LENGTH];
  int do_page = 0;

  int chkpage_idx;

  unsigned int mode_done = 0;
  int value1_done = 0;
  int value2_done = 0;
  
  NSDL2_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d, script name = %s", line, line_number, sess_idx, sess_name);

  snprintf(msg, MAX_LINE_LENGTH, "Error in parsing nsl_check_reply_size() declaration at line %d of scripts/%s/%s: ", line_number, 
     get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
     //Previously taking with only script name
     //get_sess_name_with_proj_subproj(sess_name), script_filename);
  msg[MAX_LINE_LENGTH-1] = '\0';

  CLEAR_WHITE_SPACE(line_ptr);

  if (create_check_replysize_table_entry(&rnum) != SUCCESS) 
  {
    fprintf(stderr, "%s Not enough memory. Could not create check reply size table entry.\n", msg);
    return -1;
  }

  /* Fills the default values */
  checkReplySizeTable[rnum].sess_idx = sess_idx; /* For hashcode */
  checkReplySizeTable[rnum].mode = NS_CHK_REP_SZ_MODE_NOT_BETWEEN_MIN_MAX;
  checkReplySizeTable[rnum].value1 = 0;
  checkReplySizeTable[rnum].value2 = 0;
  checkReplySizeTable[rnum].action = NS_CHK_REP_SZ_ACTION_STOP; 
  checkReplySizeTable[rnum].pgall = 0;
  checkReplySizeTable[rnum].var = -1;


  if (gSessionTable[sess_idx].checkreplysize_start_idx == -1) 
  {
    gSessionTable[sess_idx].checkreplysize_start_idx = rnum;
    gSessionTable[sess_idx].num_checkreplysize_entries = 0;
  }
  gSessionTable[sess_idx].num_checkreplysize_entries++;

  while (!done) 
  {
    if (*line_ptr == '\0') 
      done = 1;
    else if (!(strncasecmp(line_ptr, "MODE", 4))) 
    {
      if (mode_done) 
      {
        fprintf(stderr, "%s MODE argument can be specified only once\n", msg);
        return -1;
      }
      line_ptr += 4;
      CLEAR_WHITE_SPACE(line_ptr);
      CHECK_CHAR(line_ptr, '=', msg);
      // line_ptr = get_quoted_string (line_ptr, msg, data);
      CLEAR_WHITE_SPACE(line_ptr);
      if (!strncasecmp (line_ptr, "NotBetweenMinMax", strlen("NotBetweenMinMax"))) {
        checkReplySizeTable[rnum].mode = NS_CHK_REP_SZ_MODE_NOT_BETWEEN_MIN_MAX;
        mode_done = NS_CHK_REP_SZ_MODE_NOT_BETWEEN_MIN_MAX; 
        line_ptr += strlen("NotBetweenMinMax");//point to next field
      } else {
        fprintf(stderr, "%s Invalid value (%s) for MODE argument, allowed values is NotBetweenMinMax\n", msg, line_ptr);
        return -1;
      }
     } else if (!strncasecmp(line_ptr, "Value1", strlen("Value1"))) {
       if (value1_done) 
       {
         fprintf(stderr, "%s Value1 argument can be specified only once\n", msg);
         return -1;
       }
       line_ptr += strlen("Value1");
       CLEAR_WHITE_SPACE(line_ptr);
       CHECK_CHAR(line_ptr, '=', msg);
       CLEAR_WHITE_SPACE(line_ptr);
       for (i = 0; (*line_ptr != ',') && (*line_ptr != '\0') && (*line_ptr != ' '); line_ptr++, i++) {
         if (isdigit(*line_ptr))
           data[i] = *line_ptr;
         else {
           fprintf(stderr, "%s Invalid value of Value1. It must be a numeric number\n", msg);
           return -1;    
         }
       }
       data[i] = '\0';
       if (i == 0) {
         fprintf(stderr, "Empty Value1 value.\n");
         checkReplySizeTable[rnum].value1 = -1;
       } else {
         checkReplySizeTable[rnum].value1 = atoi(data);
       }
       value1_done = 1;
     } else if (!strncasecmp(line_ptr, "Value2", strlen("Value2"))) {
       if (value2_done)
       {
         fprintf(stderr, "%s Value2 argument can be specified only once\n", msg);
         return -1;
       }
       line_ptr += strlen("Value2");
       CLEAR_WHITE_SPACE(line_ptr);
       CHECK_CHAR(line_ptr, '=', msg);
       CLEAR_WHITE_SPACE(line_ptr);
       for (i = 0; (*line_ptr != ',') && (*line_ptr != '\0') && (*line_ptr != ' '); line_ptr++, i++) {
         if (isdigit(*line_ptr))
           data[i] = *line_ptr;
         else {
           fprintf(stderr, "%s Invalid value of Value2. It must be a numeric number\n", msg);
           return -1;
         }
       }
       data[i] = '\0';
       if (i == 0) {
         fprintf(stderr, "Empty Value2 value.\n");
         checkReplySizeTable[rnum].value2 = -1;
       } else {
         checkReplySizeTable[rnum].value2 = atoi(data);
       }
       value2_done = 1;
     } else if (!strncasecmp(line_ptr, "Action", strlen("Action"))) {
       line_ptr += strlen("Action");
       CLEAR_WHITE_SPACE(line_ptr);
       CHECK_CHAR(line_ptr, '=', msg);
       CLEAR_WHITE_SPACE(line_ptr);
       if (!strncasecmp (line_ptr, "Stop", 4)) {
         checkReplySizeTable[rnum].action = NS_CHK_REP_SZ_ACTION_STOP; //0
         line_ptr += 4;//point to next
       } else if (!strncasecmp (line_ptr, "Continue", 8)) {
         checkReplySizeTable[rnum].action = NS_CHK_REP_SZ_ACTION_CONTINUE; //1
         line_ptr += 8;
       } else {
         fprintf(stderr, "%s Invalid value (%s) for Action argument, allowed values are Stop and Continue\n", msg, line_ptr);
         return -1;
       }
     } else if (!strncasecmp(line_ptr, "VAR", strlen("VAR"))) {
       line_ptr += 3; //strlen("VAR")
       CLEAR_WHITE_SPACE(line_ptr);
       CHECK_CHAR(line_ptr, '=', msg);
       CLEAR_WHITE_SPACE(line_ptr);
       for (i = 0; (i < MAX_LINE_LENGTH) && (line_ptr && (*line_ptr != ' ') && (*line_ptr != ',') && (*line_ptr != '\0')); i++, line_ptr++)            
         data[i] = *line_ptr;

       data[i] = '\0';
       if (i == 0) {
         fprintf(stderr, "Empty VAR value.\n");
         checkReplySizeTable[rnum].var = -1;
       } else {
         if ((checkReplySizeTable[rnum].var = copy_into_big_buf(data, 0)) == -1) {
           fprintf(stderr, "%s could not copy '%s' as VAR arguments\n", msg, data);
           return -1;
         }
       }
     } else if (!(strncasecmp(line_ptr, "PAGE", 4))) {
       line_ptr += 4;
       CLEAR_WHITE_SPACE(line_ptr);
       CHECK_CHAR(line_ptr, '=', msg);
       CLEAR_WHITE_SPACE(line_ptr);

       for (i = 0; (i < MAX_LINE_LENGTH) && (line_ptr && (*line_ptr != ' ') && (*line_ptr != ',') && (*line_ptr != '\0')); i++, line_ptr++)
         page_name[i] = *line_ptr;
       page_name[i] = '\0';

       if (strcmp(page_name, "*")) {
         if (create_check_replysize_page_table_entry(&chkpage_idx) == -1) {
     fprintf(stderr, "%s Could not create create_check_replysize_page_table_entry. Short on memory\n", msg);
     return -1;
   }
   NSDL2_VARS(NULL, NULL, "chkpage_idx = %d", chkpage_idx);
   checkReplySizePageTable[chkpage_idx].checkreplysize_idx = rnum;
   if ((checkReplySizePageTable[chkpage_idx].page_name = copy_into_temp_buf(page_name, 0)) == -1) {
     fprintf(stderr, "%s Could not copy into temp buf\n", msg);
     return -1;
   }

   if (gSessionTable[sess_idx].checkreplysizepage_start_idx == -1) {
     gSessionTable[sess_idx].checkreplysizepage_start_idx = chkpage_idx;
     gSessionTable[sess_idx].num_checkreplysizepage_entries = 0;
   }
   gSessionTable[sess_idx].num_checkreplysizepage_entries++;

   checkReplySizePageTable[chkpage_idx].page_idx = -1;
   do_page=1;
   NSDL2_VARS(NULL, NULL, "page_name = %d, checkreplysize_idx = %d, checkreplysizepage_start_idx = %d", checkReplySizePageTable[chkpage_idx].page_name, rnum, gSessionTable[sess_idx].checkreplysizepage_start_idx);

       } else { //PAGE= * case
         checkReplySizeTable[rnum].pgall = 1;
       }
       if ((do_page == 1) && (checkReplySizeTable[rnum].pgall == 1)) {
         fprintf(stderr, "%s Specific page and ALL pages(*) cannot be specified togather\n", msg);
         return -1;
       }
     }
     //this is required to remove the white and comma space between the fields
     CLEAR_WHITE_SPACE(line_ptr);
     if (*line_ptr == ',')
       line_ptr++;
     CLEAR_WHITE_SPACE(line_ptr);
  }
  /* Page argument is a mandatory field */
  if ((!do_page) && (!checkReplySizeTable[rnum].pgall)) {
    fprintf(stderr, "%s PAGE argument must be specified\n", msg);
    return -1;
  }
  /* mode argument is a manadatory field */
  if(!mode_done)
  {
    fprintf(stderr, "%s Mode value must be specified\n", msg);  
    return -1;
  }  
  /* check for mode and Value1 and Value2,
   in future we have to support for other mode also */  
  if(mode_done == NS_CHK_REP_SZ_MODE_NOT_BETWEEN_MIN_MAX) {
    if(!(value1_done && value2_done))
    { 
      fprintf(stderr, "%s Both Value1 and Value2 must be specified for 'NotBetweenMinMax' mode\n", msg);
      return -1; 
    }
    if(checkReplySizeTable[rnum].value2 <= checkReplySizeTable[rnum].value1) {  
      fprintf(stderr, "%s Value2[%d] must be greater than Value1[%d]\n", msg, checkReplySizeTable[rnum].value2, checkReplySizeTable[rnum].value1);
      return -1;
    } 
  }

  return 0;
}

/*This function is used to copy the check 
* reply size api values into the shared memory.
* Return value: returns the pointer to 
* PerPageCheckReplySizeTableEntry_Shr structure.
*/
PerPageCheckReplySizeTableEntry_Shr *copy_check_replysize_into_shared_mem(void)
{
  void *check_replysize_perpage_table_shr_mem;
  int i;
  int var_hashcode;

  NSDL1_VARS(NULL, NULL, "Method called. total_checkreplysize_entries = %d, total_perpagechkrepsize_entries = %d", total_checkreplysize_entries, total_perpagechkrepsize_entries);

  /* insert the CheckPointTableEntry_Shr and the PerPageChkPtTableEntry_shr */
  if (total_checkreplysize_entries + total_perpagechkrepsize_entries) {

    check_replysize_perpage_table_shr_mem = do_shmget(total_checkreplysize_entries * sizeof(CheckPointTableEntry_Shr) +
             total_perpagechkrepsize_entries * sizeof(PerPageCheckReplySizeTableEntry_Shr), "checkreplysize tables");
    checkReplySizeTable_shr_mem = check_replysize_perpage_table_shr_mem;
    for (i = 0; i < total_checkreplysize_entries; i++) {

      checkReplySizeTable_shr_mem[i].mode = checkReplySizeTable[i].mode;
      checkReplySizeTable_shr_mem[i].value1 = checkReplySizeTable[i].value1;
      checkReplySizeTable_shr_mem[i].value2 = checkReplySizeTable[i].value2;
      if(checkReplySizeTable[i].var == -1) {
        checkReplySizeTable_shr_mem[i].var = NULL;
      } else {
         checkReplySizeTable_shr_mem[i].var = BIG_BUF_MEMORY_CONVERSION(checkReplySizeTable[i].var);
         var_hashcode = gSessionTable[checkReplySizeTable[i].sess_idx].var_hash_func(checkReplySizeTable_shr_mem[i].var, strlen(checkReplySizeTable_shr_mem[i].var));
         if(var_hashcode == -1) {
           print_core_events((char*)__FUNCTION__, __FILE__, 
                             "Error: Undeclared VAR variable name used"
                             " in nsl_check_reply_size() api. Variable name:%s",
                             checkReplySizeTable_shr_mem[i].var);
           checkReplySizeTable_shr_mem[i].var = NULL;  
           NS_EXIT(-1, CAV_ERR_1031043, checkReplySizeTable_shr_mem[i].var, BIG_BUF_MEMORY_CONVERSION(gSessionTable[checkReplySizeTable[i].sess_idx].sess_name))
         }
       }
       checkReplySizeTable_shr_mem[i].action = checkReplySizeTable[i].action;
    }

    perpagechk_replysize_table_shr_mem = check_replysize_perpage_table_shr_mem + (total_checkreplysize_entries * sizeof(CheckPointTableEntry_Shr));
    for (i = 0; i < total_perpagechkrepsize_entries; i++) {
      perpagechk_replysize_table_shr_mem[i].check_replysize_ptr = checkReplySizeTable_shr_mem + perPageChkRepSizeTable[i].checkreplysize_idx;
    }
  }
  return perpagechk_replysize_table_shr_mem;
}

// End: Parsing code and init code

/*Purpose:This function processes all check reply size defined on the page.
*         This will print the message on the screen if the output is not as per mode.  
* Arguments:
*     vptr : pointer to the VUser structure.
*     full_buffer : Pointer to the character string that contains the response data.
*     outlen: length
* Return value: None. 
* 
*/
void process_check_replysize_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int *outlen)
{
  int i;
  PerPageCheckReplySizeTableEntry_Shr* perpagechk_replysize_ptr;
  int blen = vptr->bytes; //response size

  int to_stop = 0, page_fail = 0; // Once set, this should remain set
  char ret_string[10];
  int pg_status = 0; //must initialize it this is used in NS_DT1 for debug tracing 

  NSDL2_VARS(vptr, NULL, "Method Called.Page name = %s, Script name = %s", 
             vptr->cur_page->page_name, vptr->sess_ptr->sess_name); 

  if (!(vptr->cur_page->first_check_reply_size_ptr)) return; // No check reply size defined

  // This code is desgined to have multiple check reply sizes on one page.
  // But if user gives contradictory checks, then if any one fails, page will fail
  // Page status will be based on the last failed check
  for (perpagechk_replysize_ptr = vptr->cur_page->first_check_reply_size_ptr, i = 0; i < vptr->cur_page->num_check_replysize; i++, perpagechk_replysize_ptr++) 
  {
      NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Checking check reply size %d. Value1 = %d, Value2 = %d, ", i + 1, perpagechk_replysize_ptr->check_replysize_ptr->value1, perpagechk_replysize_ptr->check_replysize_ptr->value2);

      /*Taking precaution-- should be reset by zero for each checkreplysize var */
      int check_replysize_fail, ret_val;
      check_replysize_fail = ret_val = 0;

      NSDL4_VARS(vptr, NULL, "Processing check reply size var, Mode is %d , response size is = %d", perpagechk_replysize_ptr->check_replysize_ptr->mode, blen);

      if(perpagechk_replysize_ptr->check_replysize_ptr->mode == NS_CHK_REP_SZ_MODE_NOT_BETWEEN_MIN_MAX)
      {
        //check the response size with value1 and value2 
        if(blen > perpagechk_replysize_ptr->check_replysize_ptr->value2)
        {
          page_fail = 1;
          check_replysize_fail = 1;
          pg_status = NS_REQUEST_SIZE_TOO_BIG; //set the page status
          ret_val = SIZE_TO_BIG; //size to big
        } else if(blen < perpagechk_replysize_ptr->check_replysize_ptr->value1) {
          page_fail = 1;
          check_replysize_fail = 1;
          pg_status = NS_REQUEST_SIZE_TOO_SMALL; //set the page status
          ret_val = SIZE_TO_SMALL; //size to small
        } else
          ret_val = SIZE_AS_PER_MODE; //ok

        /* save the return value to the var */
        sprintf(ret_string, "%d", ret_val);
        if(perpagechk_replysize_ptr->check_replysize_ptr->var != NULL)
          ns_save_string(ret_string, perpagechk_replysize_ptr->check_replysize_ptr->var);

        NSDL4_VARS(vptr, NULL, "Mode is given as NotBetweenMinMax and value1= %d, value2=%d, response size is = %d, VAR name is = %s", perpagechk_replysize_ptr->check_replysize_ptr->value1, perpagechk_replysize_ptr->check_replysize_ptr->value2, blen, perpagechk_replysize_ptr->check_replysize_ptr->var);
      }

      if(perpagechk_replysize_ptr->check_replysize_ptr->action == NS_CHK_REP_SZ_ACTION_STOP)
      {
        to_stop = 1;
        NSDL3_VARS(vptr, NULL, "Action is Stop. Therefore setting the to_stop variable");
      }


      if(check_replysize_fail) {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Page Response size [for mode NotBetweenMinMax]"
                                           " check failed for page status=%s, response size=%d,"
                                           " min value=%d, max value=%d",
                                           get_error_code_name(pg_status), blen,
                                           perpagechk_replysize_ptr->check_replysize_ptr->value1,
                                           perpagechk_replysize_ptr->check_replysize_ptr->value2);
        //If fails then output should go on screen.
        NS_EL_3_ATTR(EID_HTTP_PAGE_ERR_START + pg_status, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 "Page failed with status %s, Response/Reply size [for mode NotBetweenMinMax] check failed, "
				 "response size=%d, min value=%d, max value=%d",   
				 get_error_code_name(pg_status), blen, 
				 perpagechk_replysize_ptr->check_replysize_ptr->value1,
				 perpagechk_replysize_ptr->check_replysize_ptr->value2);
 /*				 
        printf("Response/Reply size [for mode NotBetweenMinMax] check failed for nvm=%d, userid=%lu, sessidx=%lu, page=%s, script=%s. page status=%s, response size=%ld, min value=%d, max value=%d\n",
               ns_get_nvmid(), ns_get_userid(), ns_get_sessid(),
               vptr->cur_page->page_name, vptr->sess_ptr->sess_name, 
               get_error_code_name(pg_status), blen,
               //errorcode_table_shr_mem[pg_error_code_start_idx + pg_status].error_msg, blen,
               perpagechk_replysize_ptr->check_replysize_ptr->value1,
               perpagechk_replysize_ptr->check_replysize_ptr->value2);
*/
      } else {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Page Response size [for mode NotBetweenMinMax] check passed for page status=%s, "
                                            "response size=%d, min value=%d, max value=%d",
               get_error_code_name(pg_status), blen,
               //errorcode_table_shr_mem[pg_error_code_start_idx + pg_status].error_msg, blen,
               perpagechk_replysize_ptr->check_replysize_ptr->value1,
               perpagechk_replysize_ptr->check_replysize_ptr->value2);

      }

  } //end of all checkreplysize variables processing

  // If at least one checkreplysize fails, then we need to fail the page and session
  if(page_fail) {
    set_page_status_for_registration_api(vptr, pg_status, to_stop, "CheckReplySize");
  }
}
