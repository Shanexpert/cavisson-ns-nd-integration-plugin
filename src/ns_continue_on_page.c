/*********************************************************************************************
* Name                   : ns_parse_scen_conf.c 
* Purpose                : This C file holds the function to parse scenario file keywords. 
* Author                 : Arun Nishad
* Intial version date    : Tuesday, January 27 2009 
* Last modification date : - Monday, July 13 2009 
* SM, 11-02-2014: Parsing for RBU_SCREEN_SIZE_SIM
*********************************************************************************************/

#include <assert.h>
#include "url.h"
#include "util.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_parse_scen_conf.h"
//#include "init_cav.h"
#include "ns_runtime_changes.h"
#include "ns_common.h"
#include "nslb_util.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#if 0
static int create_runprof_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_runprof_entries == max_runprof_entries) {
    MY_REALLOC_EX (runProfTable, (max_runprof_entries + DELTA_RUNPROF_ENTRIES) * sizeof(RunProfTableEntry), max_runprof_entries * sizeof(RunProfTableEntry),"runProfTable", -1);
    if (!runProfTable) {
      fprintf(stderr,"create_runprof_table_entry(): Error allocating more memory for runprof entries\n");
      return(FAILURE);
    } else max_runprof_entries += DELTA_RUNPROF_ENTRIES;
  }
  *row_num = total_runprof_entries++;
  //runProfTable[*row_num].pacing_idx = -1;
  return (SUCCESS);
}
#endif

#define continue_on_page_err_usage(err, runtime_flag, err_buff) \
{ \
  sprintf(err_buff, "Error: Invalid value of G_CONTINUE_ON_PAGE_ERROR keyword: %s\n", err); \
  strcat(err_buff, " Usage: G_CONTINUE_ON_PAGE_ERROR <group_name> <page_name> <value>\n"); \
  strcat(err_buff, " Group name can be ALL or any group name used in scenario group\n"); \
  strcat(err_buff, " Page name can be ALL or name of page for which continue on page error to be enabled.\n"); \
  strcat(err_buff, " ALL and particular page combination is not a valid combination (e.g. ALL page1)\n"); \
  strcat(err_buff, " Continue on page error value is 0 (default) for disabling and 1 for enabling continue on page error\n"); \
  if(runtime_flag != 1){ \
    NS_EXIT(-1, "%s",err_buff); \
  } \
  else { \
    NSTL1_OUT(NULL, NULL, "%s", err_buff); \
    return -1; \
  } \
} 

void initialize_runprof_cont_on_page_err()
{
  int i, num_pages;

  for (i = 0; i < total_runprof_entries; i++) {
    num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;

    MY_MALLOC(runProfTable[i].continue_onpage_error_table, sizeof(int) * num_pages, "runProfTable[i].continue_onpage_error_table;", i);

    memset(runProfTable[i].continue_onpage_error_table, -1, sizeof(int) * num_pages);
    runProfTable[i].num_pages = num_pages;
  }
}

void free_runprof_cont_on_page_err_idx()
{
  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    //num_pages = runProfTable[i].num_pages;
    FREE_AND_MAKE_NULL_EX(runProfTable[i].continue_onpage_error_table, sizeof(int) * runProfTable[i].num_pages, "runProfTable[i].continue_onpage_error_table", i); //added size of number of pages
  }
}


void copy_continue_on_page_err_to_shr() 
{
  int i;

  NSDL2_PARSING(NULL, NULL, "Method called. total continue on page error entries = %d", total_cont_on_err_entries);

  if (total_cont_on_err_entries)
  {
    int total_num_pages = 0; // Total number of pages across all scenario groups
    for (i = 0; i < total_runprof_entries; i++)
    {
      total_num_pages += runProfTable[i].num_pages;
      NSDL4_PARSING(NULL, NULL, "total_num_pages = %d", total_num_pages);
    }
    continueOnPageErrorTable_shr_mem  = (ContinueOnPageErrorTableEntry_Shr*) do_shmget(sizeof( ContinueOnPageErrorTableEntry_Shr) * total_num_pages, "continue on page err");
    /* Per session, per page assign continue on page error. */
    int j, k = 0;
    for (i = 0; i < total_runprof_entries; i++)
    {
      for (j = 0; j < runProfTable[i].num_pages; j++)
      {
        int idx = runProfTable[i].continue_onpage_error_table[j] == -1 ? 0 : runProfTable[i].continue_onpage_error_table[j];
        continueOnPageErrorTable_shr_mem[k].continue_error_value = continueOnPageErrorTable[idx].continue_error_value;
        k++;
      }
    }
  }
}   

static int create_continue_on_page_err_table_entry(int *row_num)
{
  NSDL2_PARSING(NULL, NULL, "Method called");
  if (total_cont_on_err_entries == max_cont_on_err_entries)
  {       
    MY_REALLOC_EX(continueOnPageErrorTable, (max_cont_on_err_entries + DELTA_CONT_ON_ERR_ENTRIES) * sizeof(ContinueOnPageErrorTableEntry), (max_cont_on_err_entries * sizeof(ContinueOnPageErrorTableEntry)), "continueOnPageErrorTable", -1);    
    if (!continueOnPageErrorTable)
    { 
      fprintf(stderr, "create_continue_on_page_err_table_entry(): Error allcating more memory for continue on page err entries\n");
      return FAILURE;
    } else max_cont_on_err_entries += DELTA_CONT_ON_ERR_ENTRIES;
  } 
  *row_num = total_cont_on_err_entries++; 
  return (SUCCESS); 
}

void create_default_cont_onpage_err(void)
{
  int rnum;

  NSDL2_PARSING(NULL, NULL, "Method called");
  if (create_continue_on_page_err_table_entry(&rnum) == FAILURE) {
    NS_EXIT(-1, "Error in creating new continue on page err table entry");
  }

  assert(rnum == GLOBAL_CONTINUE_PAGE_ERROR_IDX);

  continueOnPageErrorTable[rnum].continue_error_value = 0;
}

   
static int continue_on_page_error_not_runtime(char *group, char *page_name, int value, char *err_msg, int runtime_flag, char *buf)
{
  char *session_name;
  int session_idx, page_id, rnum;
  int grp_idx; 
  // Case where both group and page are ALL.
  // These need to be set at index 0 
  
  if (( strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") == 0))
  {
    continueOnPageErrorTable[GLOBAL_CONTINUE_PAGE_ERROR_IDX].continue_error_value = value;
    NSDL3_PARSING(NULL, NULL, "For ALL groups and ALL pages - continue on page error value = %d", continueOnPageErrorTable[0].continue_error_value);
    return 0;
  }
  
  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") != 0)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CONTINUE_ON_PAGE_ERROR_USAGE, CAV_ERR_1011105, page_name, "");
  }
  
  if ((grp_idx = find_sg_idx(group)) == -1) //invalid group name
  {
    NSTL1(NULL, NULL, CAV_ERR_MSG_7, group);
    return 0;
  }
  
  session_idx = runProfTable[grp_idx].sessprof_idx;
  
  // For all pages
  if (strcasecmp(page_name, "ALL") == 0)
  {
    int i;
    for(i = 0; i < gSessionTable[session_idx].num_pages; i++)
    {
      if(runProfTable[grp_idx].continue_onpage_error_table[i] == -1)
      {
        if(create_continue_on_page_err_table_entry(&rnum) == FAILURE)
        {
          NS_EXIT(-1, CAV_ERR_1000002);
        }
        continueOnPageErrorTable[rnum].continue_error_value = value;
        runProfTable[grp_idx].continue_onpage_error_table[i] = rnum;
        NSDL3_PARSING(NULL, NULL, "ALL runProfTable[%d].continue_onpage_error_table[%d] = %d\n",
        grp_idx, i, runProfTable[grp_idx].continue_onpage_error_table[i]);
        NSDL3_PARSING(NULL, NULL, "for ALL pages continueOnPageErrorTable[rnum].continue_error_value%d\n",
        continueOnPageErrorTable[rnum].continue_error_value);
 
      }else
          NSDL3_PARSING(NULL, NULL, "ELSE runProfTable[%d].continue_onpage_error_table[%d] = %d\n",
                                 grp_idx, i, runProfTable[grp_idx].continue_onpage_error_table[i]);      
   }
   return 0;
  }
  if ((page_id = find_page_idx(page_name, session_idx)) == -1) // for particular page
  {
    session_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CONTINUE_ON_PAGE_ERROR_USAGE, CAV_ERR_1011106, page_name, group, session_name, "");
  }
  if (create_continue_on_page_err_table_entry(&rnum) == FAILURE) // allocate memory to continue on page error table
  {
    NS_EXIT(-1, CAV_ERR_1000002);
  }
  continueOnPageErrorTable[rnum].continue_error_value = value;
  runProfTable[grp_idx].continue_onpage_error_table[page_id - gSessionTable[session_idx].first_page] = rnum;
  NSDL3_PARSING(NULL, NULL, "runProfTable[%d].continue_onpage_error_table[page_id(%d) - gSessionTable[%d].first_page(%d)] = %d\n",
                           grp_idx, page_id, session_idx, gSessionTable[session_idx].first_page,
                           runProfTable[grp_idx].continue_onpage_error_table[page_id - gSessionTable[session_idx].first_page]);
  return 0;
}

int continue_on_page_error_runtime(char *group, char *page_name, int value, char *err_msg, int runtime_flag, char *buf)
{
  int page_id;
  int i,j;
  ContinueOnPageErrorTableEntry_Shr *ptr;
  
  // called for Runtime change
  // for shared memory table
  // Case 1 - ALL and ALL
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") == 0)) { // for all groups and pages
    for (i = 0; i < total_runprof_entries; i++) {
     for (j = 0; j < runprof_table_shr_mem[i].num_pages; j++) {
        ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[i].continue_onpage_error_table[j];
        ptr->continue_error_value = value; 
        NSDL3_PARSING(NULL, NULL, "Setting continue on page error for run prof %d and page index %d with continue on error value = %d", i, j, ptr->continue_error_value);
      }
    }
    return 0;
  }
   // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") != 0)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CONTINUE_ON_PAGE_ERROR_USAGE, CAV_ERR_1011105, page_name, "");
  }
  
   // Case 3 - Invalid group name
  int grp_idx; // for particular group
  if ((grp_idx = find_sg_idx_shr(group)) == -1) { // for invalid group name
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CONTINUE_ON_PAGE_ERROR_USAGE, CAV_ERR_MSG_7, group);
  }
  
   // Case 4 - Group and ALL pages 
  if (strcasecmp(page_name, "ALL") == 0) { // for all pages
    for (i = 0; i < runprof_table_shr_mem[grp_idx].num_pages; i++)
    {
      ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[grp_idx].continue_onpage_error_table[i];
      ptr->continue_error_value = value;
      NSDL3_PARSING(NULL, NULL,"For ALL pages continue on error value = %d", ptr->continue_error_value);
    }
    return 0;
  }
  
   // Case 4 - Group and particular page (e.g. G1 Page1)

  SessTableEntry_Shr* sess_ptr = runprof_table_shr_mem[grp_idx].sess_ptr;
  // Case 4a - Group and particular page (e.g. G1 Page1) - Invalid page
  if ((page_id = find_page_idx_shr(page_name, sess_ptr)) == -1) { // for particular pages
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CONTINUE_ON_PAGE_ERROR_USAGE, CAV_ERR_1011106, page_name, group, sess_ptr->sess_name, "");
  }
  
  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[grp_idx].continue_onpage_error_table[page_id];
  ptr->continue_error_value = value; 
  return 0;
 
}


int kw_set_continue_on_page_error (char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int value;
  char page_name[MAX_DATA_LINE_LENGTH];
  int num;
  char page_err_value[MAX_DATA_LINE_LENGTH];

  num = sscanf(buf, "%s %s %s %s", keyword, grp, page_name, page_err_value);
  if(num < 4 )
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CONTINUE_ON_PAGE_ERROR_USAGE, CAV_ERR_1011104, CAV_ERR_MSG_1);
  }
  if(ns_is_numeric(page_err_value) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CONTINUE_ON_PAGE_ERROR_USAGE, CAV_ERR_1011104, CAV_ERR_MSG_2);
  }
  value = atoi(page_err_value);
  if ((value != 0) && (value != 1)) {
    value = 0;
  }
  NSDL2_MISC(NULL, NULL, "value of continue on page error[%d]", value);

  if (!runtime_flag)
  {
    continue_on_page_error_not_runtime(grp, page_name, value, err_msg, runtime_flag, buf);
  }
  else  
  {
    int ret = continue_on_page_error_runtime(grp, page_name, value, err_msg, runtime_flag, buf);
    return(ret);
  }
  return 0;
}
