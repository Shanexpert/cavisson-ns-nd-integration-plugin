
/********************************************************************************
 * File Name            : ns_page_think_time_parse.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 04 November 2011 
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains parsing function for G_PAGE_THINK_TIME keyword.
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include "ns_page_think_time_parse.h"
#include "util.h"
#include "ns_trace_level.h"
#include "nslb_alloc.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
/******************************************************************************************** 
 * Description       : Method used to allocate memory to page think table.
 * Input Parameters
 *      row_num      : Pointer to row number.
 * Output Parameters : Set total entries of page think table(total_thinkprof_entries).
 * Return            : Return -1 on error in allocating memory to thinkProfTable else return 1.
 ********************************************************************************************/

static int create_thinkprof_table_entry(int *row_num) 
{
  NSDL2_PARSING (NULL, NULL, "Method called");

  if (total_thinkprof_entries == max_thinkprof_entries) 
  {
    MY_REALLOC_EX (thinkProfTable, (max_thinkprof_entries + DELTA_THINKPROF_ENTRIES) * sizeof(ThinkProfTableEntry), (max_thinkprof_entries * sizeof(ThinkProfTableEntry)), "thinkProfTable", -1); // Added old size of table
    if (!thinkProfTable) {
      fprintf(stderr, "create_thinkprof_table_entry(): Error allcating more memory for thinkprof entries\n");
      return FAILURE;
    } else max_thinkprof_entries += DELTA_THINKPROF_ENTRIES;
  }
  *row_num = total_thinkprof_entries++;
  return (SUCCESS);
}

/**************************************************************************************** 
 * Description       : Method used to create page think time for default value of keyword.
 *                     This method is called from parse_files()in url.c. 
 * Input Parameters  : None
 * Output Parameters : Set page think time entries, mode, avg_time, median_time, var_time.
 * Return            : None
 ****************************************************************************************/

void create_default_global_think_profile(void)
{
  int rnum;

  NSDL2_PARSING (NULL, NULL, "Method called");
  if (create_thinkprof_table_entry(&rnum) == FAILURE) {
    NS_EXIT(-1, "Error in creating new think time entry");
  }

  assert(rnum == GLOBAL_THINK_IDX);
  // Setting default values
  thinkProfTable[rnum].mode = PAGE_THINK_TIME_MODE_NO_THINK_TIME;
  thinkProfTable[rnum].avg_time = 0;
  thinkProfTable[rnum].median_time = 0;
  thinkProfTable[rnum].var_time = 0;
  thinkProfTable[rnum].custom_page_think_time_callback = -1;
  thinkProfTable[rnum].custom_page_think_func_ptr = NULL;
}

/**********************************************************************************************************
 * Description       : think_mode_to_str() method used to get think mode string in debug logs.
 * Input Parameter
 *       think_mode  : think mode value
 * Output Parameter  : none
 * Return            : Return string value corresonding think mode.
 **********************************************************************************************************/
#ifdef NS_DEBUG_ON
static char *think_mode_to_str(int think_mode)
{
  NSDL2_PARSING(NULL, NULL, "Method called. think_mode = %d", think_mode);

  switch(think_mode)
  {
    case PAGE_THINK_TIME_MODE_NO_THINK_TIME:
      return("NoThinkTime");
      break;

    case PAGE_THINK_TIME_MODE_INTERNET_RANDOM:
      return("InternetRandomThinkTime");
      break;

    case PAGE_THINK_TIME_MODE_CONSTANT:
      return("ConstantThinkTime");
      break;

    case PAGE_THINK_TIME_MODE_UNIFORM_RANDOM:
      return("UniformRandomThinkTime");
      break;

    case PAGE_THINK_TIME_MODE_CUSTOM:
      return("CustomPageThinkTime");
      break;
    default:
      return("InvalidThinkTime");
  }
}
#endif

/**********************************************************************************************
 * Description       : think_prof_data() method used to print data for particular think
 *                     mode in shared memory.
 * Input Parameter
 *      grp_id       : group index
 *      pg_idx       : page index
 *   think_prof_ptr  : pointer to shared memory
 *      buf          : to fill information
 * Output Parameter  : None
 * Return            : returns information buffer
 *************************************************************************************************/
#ifdef NS_DEBUG_ON
static char *think_prof_data(int grp_id, int pg_idx, ThinkProfTableEntry_Shr *think_prof_ptr, char *buf)
{
  char grp_name[128] = "ALL";
  char page_name[2048] = "ALL";

  NSDL2_PARSING(NULL, NULL, "Method called. grp_id = %d pg_idx = %d", grp_id, pg_idx);

  if (grp_id != -1)
  {
    if(runProfTable)
      strcpy(grp_name, BIG_BUF_MEMORY_CONVERSION(runProfTable[grp_id].scen_group_name));
    else
      strcpy(grp_name,runprof_table_shr_mem[grp_id].scen_group_name); // for shared memory
  }
  if (pg_idx != -1)
  {
    if(runProfTable) { // For non shared memory
      PageTableEntry *page;
      page  = gPageTable + gSessionTable[runProfTable[grp_id].sessprof_idx].first_page + pg_idx;
      strcpy(page_name, BIG_BUF_MEMORY_CONVERSION(page->page_name));
    }
    else { // For shared memory
     PageTableEntry_Shr *page;
     page = runprof_table_shr_mem[grp_id].sess_ptr->first_page + pg_idx;
     strcpy(page_name, page->page_name);
    }
  }
  if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_NO_THINK_TIME)
    sprintf(buf, "Group = %s, Page = %s, PageThinkMode = %s", grp_name, page_name, think_mode_to_str(think_prof_ptr->mode));
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_INTERNET_RANDOM)
    sprintf(buf, "Group = %s, Page = %s, PageThinkMode = %s, Median = %.3f sec", grp_name, page_name, think_mode_to_str(think_prof_ptr->mode), (double)think_prof_ptr->median_time/1000.0);
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_CONSTANT)
 sprintf(buf, "Group = %s, Page = %s, PageThinkMode = %s, Constant = %.3f sec", grp_name, page_name, think_mode_to_str(think_prof_ptr->mode), (double)think_prof_ptr->avg_time/1000.0);
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_CUSTOM)
    sprintf(buf, "Group = %s, Page = %s, PageThinkTimeMode = %s", grp_name, page_name, think_mode_to_str(think_prof_ptr->mode));
  else /* UNIFORM_RANDOM */
    sprintf(buf, "Group = %s, Page = %s, PageThinkMode = %s, Min = %.3f sec, Max = %.3f sec", grp_name, page_name, think_mode_to_str(think_prof_ptr->mode), (double)think_prof_ptr->avg_time/1000.0, (double)think_prof_ptr->median_time/1000.0);

  return(buf);
}
#endif

/**********************************************************************************************
 * Description       : think_prof_to_str_shm() method used to print data for particular think mode
 *                     in shared memory.                                 
 * Input Parameter
 *      grp_id       : group index
 *      pg_idx       : page index
 *   think_prof_ptr  : pointer to shared memory
 * Output Parameter  : None
 * Return            : returns information buffer
 * Notes             : For grp_id = -1 and pg_idx = -1 here group and page value is ALL
 ************************************************************************************************/

static char *think_prof_to_str_shm(int grp_id, int pg_idx, ThinkProfTableEntry_Shr *think_prof_ptr, char *buf) 
{
  char grp_name[128] = "ALL";
  char page_name[128] = "ALL";

  NSDL2_PARSING(NULL, NULL, "Method called. grp_id = %d pg_idx = %d", grp_id, pg_idx);
  
  if (grp_id != -1) 
  { 
    if(runProfTable) // for non shared memory
      strcpy(grp_name, BIG_BUF_MEMORY_CONVERSION(runProfTable[grp_id].scen_group_name));
    else
      strcpy(grp_name,runprof_table_shr_mem[grp_id].scen_group_name); // for shared memory
  }
  if (pg_idx != -1)
  {
    if(runProfTable) { // For non shared memory
      PageTableEntry *page;
      page  = gPageTable + gSessionTable[runProfTable[grp_id].sessprof_idx].first_page + pg_idx;
      strcpy(page_name, BIG_BUF_MEMORY_CONVERSION(page->page_name));
    }
    else { // For shared memory
      PageTableEntry_Shr *page;
      page = runprof_table_shr_mem[grp_id].sess_ptr->first_page + pg_idx;
      strcpy(page_name, page->page_name);
    }
  }
  //Run time changes applied to Page Think Time for scenario group 'G1' and page 'page1'. New setting is 'no think time'
  //New setting is 'Internet Random think time with median of 1.5 seconds'
  if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_NO_THINK_TIME)
    sprintf(buf, "Run time changes applied to Page Think Time for scenario group '%s' and page '%s'.New setting is No think time.", grp_name, page_name);
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_INTERNET_RANDOM)
    sprintf(buf, "Run time changes applied to Page Think Time for scenario group '%s'and page '%s'. New setting is Random (Internet type distribution) think time with median of %.3f seconds.", grp_name, page_name, (double)think_prof_ptr->median_time/1000.0);
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_CONSTANT)
    sprintf(buf, "Run time changes applied to Page Think Time for scenario group '%s'and page '%s'. New setting is Constant think time of %.3f seconds.", grp_name, page_name, (double)think_prof_ptr->avg_time/1000.0);
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_CUSTOM)
    sprintf(buf, "Run time changes applied to Page Think Time for scenario group '%s'and page '%s'. New setting is Custom page think time. Callback Method is %s", grp_name, page_name, think_prof_ptr->custom_page_think_time_callback);
  else /* UNIFORM_RANDOM */
    sprintf(buf, "Run time changes applied to Page Think Time for scenario group '%s'and page '%s'. New setting is Random (Uniform distribution) think time from %.3f to %.3f seconds.", grp_name, page_name, (double)think_prof_ptr->avg_time/1000.0, (double)think_prof_ptr->median_time/1000.0);
 
  return(buf);
}

/**********************************************************************************************
 * Description       : think_prof_to_str_non_shm() method used to print data for particular 
 *                     think mode in non-shared memory.
 * Input Parameter
 *      grp_name     : current group name
 *      pg_name      : current page name
 *   think_prof_ptr  : pointer to non-shared memory
 * Output Parameter  : None
 * Return            : returns information buffer 
 ************************************************************************************************/

#ifdef NS_DEBUG_ON
static char *think_prof_to_str_non_shm(char *grp_name, char *pg_name,  ThinkProfTableEntry *think_prof_ptr, char *buf)
{
  NSDL2_PARSING(NULL, NULL, "Method called. grp_name = %s pg_name = %s ", grp_name, pg_name);
    
  if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_NO_THINK_TIME)
    sprintf(buf, "PageThinkTime for Group = %s, Page = %s, PageThinkMode = %s", grp_name, pg_name, think_mode_to_str(think_prof_ptr->mode));
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_INTERNET_RANDOM)
    sprintf(buf, "PageThinkTime for Group = %s, Page = %s, PageThinkMode = %s, MedianTime = %.3f secs", grp_name, pg_name, think_mode_to_str(think_prof_ptr->mode), (double )think_prof_ptr->median_time/1000.0);
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_CONSTANT)
    sprintf(buf, "PageThinkTime for Group = %s, Page = %s, PageThinkMode = %s, ConstantTime = %.3f secs", grp_name, pg_name, think_mode_to_str(think_prof_ptr->mode), (double )think_prof_ptr->avg_time/1000.0);
  else if(think_prof_ptr->mode == PAGE_THINK_TIME_MODE_CUSTOM)
    sprintf(buf, "PageThinkTime for Group = %s, Page = %s, PageThinkMode = %s, Callback Method = %s", grp_name, pg_name,
            think_mode_to_str(think_prof_ptr->mode), RETRIEVE_BUFFER_DATA(think_prof_ptr->custom_page_think_time_callback));
  else  /* UNIFORM_RANDOM */
    sprintf(buf, "PageThinkTime for Group = %s, Page = %s, PageThinkMode = %s, MinTime = %.3f secs, MaxTime = %.3f secs", grp_name, pg_name, think_mode_to_str(think_prof_ptr->mode), (double )think_prof_ptr->avg_time/1000.0, (double )think_prof_ptr->median_time/1000.0);
  
 return(buf);
}
#endif

/****************************************************************************************** 
 * Description      : In InternetRandom page think mode, gsl calculation are done to find 
 *                    mean,mode,median etc to obtain page think time.Now gsl warnings were 
 *                    getting printed on console, as per Kohls requirement warings/error 
 *                    message need to redirected. Hence creating file in TR directory 
 *                    named octave.out
 * Input Parameter  : None
 * Output Parameter : Fill octave_file_name buffer with octave.out address 
 * Return           : None
 ******************************************************************************************/
static void initialize_octave()
{
  NSDL1_PARSING (NULL, NULL, "Method called");
  
  sprintf(octave_file_name,"%s/logs/TR%d/octave.out", g_ns_wdir, testidx);

  NSDL2_PARSING (NULL, NULL, "Octave file name is %s", octave_file_name);
}

/******************************************************************************************************
 * Description        : Method compare_ptt_per_pg_per_grp() used to compare median time in InternetRandom
 *                      mode within or among groups.Here we need to determine thinkProfTable index 
 *                      to compare median time for combination of group and page.  
 * Input Parameter    :
 * cur_idx            : current index in thinprof table  
 * Return             : If median matches then return 0 else for new entry return 1
 ******************************************************************************************************/
static int compare_ptt_per_pg_per_grp(int cur_idx)
{
  int idx;
  NSDL2_PARSING (NULL, NULL, "Method called, current index = %d", cur_idx);

  /* Find thinkProfTable index to determine median time and mode matches for current index
   * Here we find index in reverse order, for cur_idx = 5, we will be finding ptt for (4-0) index*/ 
  for (idx = (cur_idx - 1); idx >= 0; idx--) 
  {
    if ((thinkprof_table_shr_mem[cur_idx].median_time == thinkprof_table_shr_mem[idx].median_time) && (thinkprof_table_shr_mem[cur_idx].mode == thinkprof_table_shr_mem[idx].mode))  
    { 
      NSDL2_PARSING (NULL, NULL, "PTT matches in thinkprof_table_shr_mem, index = %d", idx);
      return idx;//Match entry
    }
  }
  return -1;//New entry 
}


/******************************************************************************************************
 * Description        : Method copy_think_prof_to_shr() used to copy page think table in shared memory.
 *                      Add page think mode values for total number of pages across all scenario groups.
 *                      This method is called from copy_structs_into_shared_mem()in util.c.
 * Input Parameter    : None
 * Output Parameter   : Set page think mode values for total number of pages.
 * Return             : None
 ******************************************************************************************************/

void copy_think_prof_to_shr(void)
{
  NSDL2_PARSING (NULL, NULL, "Method called");
  int i;
#ifdef NS_DEBUG_ON
  char buf[4096];
#endif
  //printf("Initializing page think time settings. It may take time...\n");
  NSTL1(NULL, NULL, "Initializing page think time settings. It may take time...");
  if (total_thinkprof_entries == 0) 
    return;

  int actual_num_pages = 0;

  /*Redirect ns_weibthink_calc warning/error message to file octave.out*/
  initialize_octave();

  for (i = 0; i < total_runprof_entries; i++) {
    actual_num_pages += runProfTable[i].num_pages;
    NSDL4_PARSING(NULL, NULL, "Total number of pages of all scenario groups = %d", actual_num_pages);
  }
  
  thinkprof_table_shr_mem = (ThinkProfTableEntry_Shr*) do_shmget(sizeof(ThinkProfTableEntry_Shr) * actual_num_pages, "think profile table");

  /* Per session, per page assign think prof. */
  int j, k = 0;
  for (i = 0; i < total_runprof_entries; i++) {
    NSDL4_PARSING(NULL, NULL, "runProfTable[%d].num_pages = %d", i, runProfTable[i].num_pages);
    for (j = 0; j < runProfTable[i].num_pages; j++) {
      //NSDL4_PARSING(NULL, NULL, "overrideRecordedThinktimeTable[k =%d].mode = %d", k, overrideRecordedThinktimeTable[k].mode);
      int override_idx = runProfTable[i].override_recorded_think_time_table[j] == -1 ? 0 : runProfTable[i].override_recorded_think_time_table[j];
      int idx = runProfTable[i].page_think_table[j] == -1 ? 0 : runProfTable[i].page_think_table[j];
      NSDL4_PARSING(NULL, NULL, "k = %d, runProfTable[%d].page_think_table[%d] = %d, idx = %d, avg_time = %d override recorded mode = %d min = %f max = %f", k, i, j, runProfTable[i].page_think_table[j], idx, thinkProfTable[idx].avg_time, overrideRecordedThinktimeTable[override_idx].mode, overrideRecordedThinktimeTable[override_idx].min, overrideRecordedThinktimeTable[override_idx].max);

      thinkprof_table_shr_mem[k].mode = thinkProfTable[idx].mode;
      thinkprof_table_shr_mem[k].avg_time = thinkProfTable[idx].avg_time;
      thinkprof_table_shr_mem[k].median_time = thinkProfTable[idx].median_time;
      thinkprof_table_shr_mem[k].var_time = thinkProfTable[idx].var_time;
      thinkprof_table_shr_mem[k].custom_page_think_func_ptr = thinkProfTable[idx].custom_page_think_func_ptr;

      // Earlier override think time was kept in gset, now it is page base so keeping it into thinkprof_table_shr_mem   
      thinkprof_table_shr_mem[k].override_think_time.mode = overrideRecordedThinktimeTable[override_idx].mode;
      thinkprof_table_shr_mem[k].override_think_time.min = overrideRecordedThinktimeTable[override_idx].min;
      thinkprof_table_shr_mem[k].override_think_time.max = overrideRecordedThinktimeTable[override_idx].max;
      thinkprof_table_shr_mem[k].override_think_time.multiplier = overrideRecordedThinktimeTable[override_idx].multiplier;

      NSDL3_PARSING(NULL, NULL, "custom page think time callback offset in local table = [%d]", 
                                 thinkprof_table_shr_mem[idx].custom_page_think_func_ptr);
      if(thinkProfTable[idx].custom_page_think_time_callback != -1) 
         thinkprof_table_shr_mem[k].custom_page_think_time_callback = BIG_BUF_MEMORY_CONVERSION(thinkProfTable[idx].custom_page_think_time_callback);
      else
        thinkprof_table_shr_mem[k].custom_page_think_time_callback = NULL; 
      if (thinkProfTable[idx].mode == PAGE_THINK_TIME_MODE_INTERNET_RANDOM) {
        int matching_idx = compare_ptt_per_pg_per_grp(k);
        /* If median time is unique for a group and page combination then we need to calculate
         * a and b value. Otherwise update thinkprof_table_shr_mem with old think table entry */
        if (matching_idx == -1) {
          if (ns_weibthink_calc(thinkProfTable[idx].avg_time, thinkProfTable[idx].median_time, thinkProfTable[idx].var_time, &thinkprof_table_shr_mem[k].a, &thinkprof_table_shr_mem[k].b) == -1) {
            NS_EXIT(-1, "error in calculating a and b values for the weib ran num gen");
          }
        } else {
          /* Changes done to fill a and b value, page index should be pg_idx */
          NSDL3_PARSING(NULL, NULL, "PTT entry found in thinkprof table matching_idx= %d", matching_idx);
          thinkprof_table_shr_mem[k].a = thinkprof_table_shr_mem[matching_idx].a;
          thinkprof_table_shr_mem[k].b = thinkprof_table_shr_mem[matching_idx].b;
        }
        NSDL3_PARSING(NULL, NULL, "Average_time = %d, Median_Time = %d, Var_Time = %d", thinkProfTable[idx].avg_time, thinkProfTable[idx].median_time, thinkProfTable[idx].var_time);
        NSDL3_PARSING(NULL, NULL, "The calulated a and b values for G_Page_Think_Time Mode = 1 are: a = %f, b = %f", thinkprof_table_shr_mem[k].a, thinkprof_table_shr_mem[k].b);
      }
      else if (thinkProfTable[idx].mode == PAGE_THINK_TIME_MODE_UNIFORM_RANDOM) {
        thinkprof_table_shr_mem[k].a = thinkProfTable[idx].avg_time;
        thinkprof_table_shr_mem[k].b = thinkProfTable[idx].median_time;
      }
      NSDL3_PARSING(NULL, NULL, "PageThinkTime: Shared Memory - %s", think_prof_data(i, j, &(thinkprof_table_shr_mem[k]), buf));
      k++;
    }
  }
  //printf("Initializations of page think time settings completed.\n");
  NSTL1(NULL, NULL, "Initialization of page think time settings completed.");
}

/**********************************************************************************************************************
 * Description       : page_think_time_usage() macro used to print usage for G_PAGE_THINK_TIME keyword and exit.
 *                     Called from kw_set_g_page_think_time().
 * Input Parameters
 *       err         : Print error message.
 *       runtime_flag: Check for runtime changes
 *       err_buf     : Pointer to error buffer
 * Output Parameters : None
 * Return            : None
 ************************************************************************************************************************/
// Make sure size of err_msg buffer passed has enough space which is filled by usage message.

#define page_think_time_usage(err, runtime_flag, err_buff) \
{ \
  sprintf(err_buff, "Error: Invalid value of G_PAGE_THINK_TIME keyword: %s\n", err); \
  strcat(err_buff, "  Usage: G_PAGE_THINK_TIME <group_name> <page_name> <think_mode> <value1> <value2>\n"); \
  strcat(err_buff, "    Group name can be ALL or any group name used in scenario group\n"); \
  strcat(err_buff, "    Page name can be ALL or name of page for which page think time is to be enabled.\n"); \
  strcat(err_buff, "    ALL and particular page combination is not a valid combination (e.g. ALL page1)\n"); \
  strcat(err_buff, "    Think mode:\n");\
  strcat(err_buff, "    0: No think time, value1 and value2 not applicable (Default)\n");\
  strcat(err_buff, "    1: Internet type random, value1 is median time and value2 not applicable.\n"); \
  strcat(err_buff, "    2: Constant time, value1 is constant time and value2 not applicable.\n");\
  strcat(err_buff, "    3: Uniform random, value1 is the minimum time and value2 is maximum time.\n"); \
  strcat(err_buff, "    4: Custom Delay, value1 is the name of Callback Method.\n"); \
  strcat(err_buff, "Note: Time values (value1 and value2) should be specified in milliseconds.\n"); \
  if(runtime_flag != 1){ \
    NS_EXIT(-1, "%s", err_buff); \
  } \
  else{ \
    NSTL1_OUT(NULL, NULL, "%s", err_buff); \
    return -1; \
  } \
}

/***************************************************************************************************
 * Description       : RTC_PAGE_THINK_TIME_LOG() macro used to print runtime changes and log message
 *                     in debug log.
 * Input Parameter   
 *      ptt_ptr      : pointer to think table in shared memory
 *      grp_idx      : group index
 *      pg_idx       : page index
 *      rtc_msg_buf  : message buffer
 * Output Parameter  : None
 * Return            : None                        
 **************************************************************************************************/

/*#define RTC_PAGE_THINK_TIME_LOG(ptt_ptr, grp_idx, pg_idx, rtc_msg_buf) \
{ \
char *rtc_msg_ptr; \
 \
  if(rtc_msg_buf[0] == '\0') \
    sprintf(rtc_msg_buf, "%s", "Run time changes applied to Page Think Time"); \
  int  len = strlen(rtc_msg_buf); \
  rtc_msg_ptr = rtc_msg_buf + len; \
  sprintf(rtc_msg_ptr, "%s\n",think_prof_to_str_shm(grp_idx, pg_idx, ptt_ptr, rtc_msg_ptr)); \
  NSDL3_PARSING(NULL, NULL, "%s", rtc_msg_ptr);\
}*/

/***********************************************************************************************
 * Description       : calc_a_and_b_value() macro used to calculate a and b value at runtime 
 *                     change and update shared memory.
 * Input Parameter
 *      ptt_ptr      : pointer to think table in shared memory
 * Output Parameter  : None
 * Return            : None     
 ***********************************************************************************************/

#define calc_a_and_b_value(ptt_ptr) \
{                            \
  if (ptt_ptr->mode == PAGE_THINK_TIME_MODE_INTERNET_RANDOM) { \
    NSDL3_PARSING(NULL, NULL, "ptt_ptr->avg_time = %d, ptt_ptr->median_time = %d, ptt_ptr->var_time = %d, ptt_ptr->a = %f, ptt_ptr->b = %f", ptt_ptr->avg_time, ptt_ptr->median_time, ptt_ptr->var_time, ptt_ptr->a, ptt_ptr->b); \
    if (ns_weibthink_calc(ptt_ptr->avg_time, ptt_ptr->median_time, ptt_ptr->var_time, &ptt_ptr->a, &ptt_ptr->b) == -1) {  \
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,__FILE__, (char*)__FUNCTION__,"Error:Calculating a and b values for the weib ran num gen");  \
    return -1; \
    } \
    NSDL3_PARSING(NULL, NULL, "The calulated a and b values for G_Page_Think_Time PageThinkMode = %s a = %f, b = %f", think_mode_to_str(ptt_ptr->mode), ptt_ptr->a, ptt_ptr->b); \
  } \
  else if (ptt_ptr->mode == PAGE_THINK_TIME_MODE_UNIFORM_RANDOM) { \
    ptt_ptr->a = ptt_ptr->avg_time; \
    ptt_ptr->b = ptt_ptr->median_time; \
  } \
} 

/***********************************************************************************************
 * Description       : ADD_THINK_PROF_AT_SHR() macro used to add value to think prof table at
 *                     runtime changes and call calc_a_and_b_value and RTC_PAGE_THINK_TIME_LOG.
 * Input Parameter
 *      ptt_ptr      : pointer to think table in shared memory
 *      grp_idx      : group index
 *      pg_idx       : page index
 *      change_msg   : message buffer
 * Output Parameter  : Set think prof value in shared memory
 * Return            : None
 ************************************************************************************************/

#define ADD_THINK_PROF_AT_SHR(ptt_ptr, grp_idx, pg_idx, change_msg)  \
{  \
  ptt_ptr->mode = think_mode; \
  ptt_ptr->avg_time = avg_time; \
  ptt_ptr->median_time = median_time;\
  ptt_ptr->var_time = var_time; \
  calc_a_and_b_value(ptt_ptr); \
}
 
/********************************************************************************************************
 * Description       : page_think_time_runtime() method used when runtime flag is enabled, here we
 *                     check for all possible cases. For all groups and pages, for particular group and
 *                     all pages or particular pages. Method called from kw_set_g_page_think_time().
 * Input Parameters
 *         group     : pointer to group name.
 *         page name : pointer to page name.
 *         avg_time  : average page think time.
 *       median_time : median page think time.
 *        think_mode : page think mode.
 *          var_time : variable time.
 *         change_msg   : error message.
 * Output Parameters : Update shared memory.
 * Return            : Return 0 for success case.
 **********************************************************************************************************/

static int page_think_time_runtime(char *kw_buf, char *sg_name, char *name, int avg_time, int median_time, int think_mode, int var_time, int runtime_flag, char *change_msg, char *custom_page_think_time_callback )
{
  NSDL2_PARSING(NULL, NULL, "Method called.");
  int page_id;
  int i, j;
  #ifdef NS_DEBUG_ON 
  char buf[4096];
  #endif
   
  ThinkProfTableEntry_Shr *ptt_ptr;
  //printf("Initializing page think time settings. It may take time...\n");  
  NSTL1(NULL, NULL, "Initializing page think time settings. It may take time...");
  // called for Runtime change
  // for shared memory table
  // Case 1 - for ALL group and page
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") == 0)) {
    for (i = 0; i < total_runprof_entries; i++) {
      for (j = 0; j < runprof_table_shr_mem[i].num_pages; j++) {
        ptt_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[i].page_think_table[j];
        ADD_THINK_PROF_AT_SHR(ptt_ptr, i, j, change_msg); 
        NSDL3_PARSING(NULL, NULL, "%s", think_prof_data(i, j, ptt_ptr, buf));
      }
    }
    think_prof_to_str_shm(-1, -1, ptt_ptr, change_msg);
    //printf("Initializations of page think time settings completed.\n");
    NSTL1(NULL, NULL, "Initialization of page think time settings completed.");
    return 0;
  } 
  
  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") != 0)) {
    NS_KW_PARSING_ERR(kw_buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011006, name, "");
  }

  
  // Case 3 - Invalid group name
  // for particular group
  int grp_idx;
  if ((grp_idx = find_sg_idx_shr(sg_name)) == -1) {
    NS_KW_PARSING_ERR(kw_buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011008, sg_name, "");
  }  
  
  // Case 4 - Group and ALL pages
  if (strcasecmp(name, "ALL") == 0) {
    for (i = 0; i < runprof_table_shr_mem[grp_idx].num_pages; i++) {
      ptt_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[grp_idx].page_think_table[i];
      ADD_THINK_PROF_AT_SHR(ptt_ptr, grp_idx, i, change_msg);
      NSDL3_PARSING(NULL, NULL, "%s", think_prof_data(grp_idx, i, ptt_ptr, buf));
    }
    think_prof_to_str_shm(grp_idx, -1, ptt_ptr, change_msg);
    //printf("Initializations of page think time settings completed.\n");
    NSTL1(NULL, NULL, "Initialization of page think time settings completed.");
    return 0;
  } 
  
  // Case 5 - Group and particular page (e.g. G1 Page1)
  SessTableEntry_Shr* sess_ptr = runprof_table_shr_mem[grp_idx].sess_ptr;
  // Case 5a - Group and particular page (e.g. G1 Page1) - For invalid page, print warning and return without applying changes
  if ((page_id = find_page_idx_shr(name, sess_ptr)) == -1)
  {
    NS_KW_PARSING_ERR(kw_buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011007, name, sg_name, sess_ptr->sess_name, "");
  }
  ptt_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[grp_idx].page_think_table[page_id];
  ADD_THINK_PROF_AT_SHR(ptt_ptr, grp_idx, page_id, change_msg);
  think_prof_to_str_shm(grp_idx, page_id, ptt_ptr, change_msg);
  NSDL3_PARSING(NULL, NULL, "%s", think_prof_data(grp_idx, page_id, ptt_ptr, buf));
  //printf("Initializations of page think time settings completed.\n");
  NSTL1(NULL, NULL, "Initialization of page think time settings completed.");
  return 0;
}

/********************************************************************************************************
 * Description       : page_think_time_not_runtime() method used when runtime flag is disable, here we 
 *                     check for all possible cases. For all groups and pages, for particular group and 
 *                     all pages or particular pages. Method called from kw_set_g_page_think_time().
 * Input Parameters
 *         group     : pointer to group name.
 *         page name : pointer to page name.
 *         avg_time  : average page think time.
 *       median_time : median page think time.
 *        think_mode : page think mode
 *        var_time   : variable time 
 *      change_msg   : error message
 * Output Parameters : Set fetch option for auto fetch embedded keyword.
 * Return            : Return 0 for success case
 *********************************************************************************************************/

static int page_think_time_not_runtime(char *buf, char *sg_name, char *name, int avg_time, int median_time, int think_mode, int var_time, int runtime_flag, char *change_msg, char *custom_page_think_time_callback)
{
  //char session_name[MAX_DATA_LINE_LENGTH];
  int session_idx, page_id, rnum;
  IW_UNUSED(char think_prof_buf[1024]);
  IW_UNUSED(think_prof_buf[0] = '\0');
  NSDL2_PARSING(NULL, NULL, "Method called.");
  // Case 1 - where both group and page are ALL.
  // These need to be set at index 0
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") == 0))
  {
    thinkProfTable[GLOBAL_THINK_IDX].mode = think_mode;
    thinkProfTable[GLOBAL_THINK_IDX].avg_time = avg_time;
    thinkProfTable[GLOBAL_THINK_IDX].median_time = median_time;
    thinkProfTable[GLOBAL_THINK_IDX].var_time = var_time;
    if (custom_page_think_time_callback[0])
      thinkProfTable[GLOBAL_THINK_IDX].custom_page_think_time_callback = copy_into_big_buf(custom_page_think_time_callback, 0);
    else
      thinkProfTable[GLOBAL_THINK_IDX].custom_page_think_time_callback = -1;
     
    NSDL3_PARSING(NULL, NULL, "%s", think_prof_to_str_non_shm(sg_name, name, &(thinkProfTable[GLOBAL_THINK_IDX]),think_prof_buf));
    return 0;
  }

  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") != 0)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011006, name, "");
  }
  
  // Case 3 - Invalid group name
  // for particular group
  int grp_idx;
  if ((grp_idx = find_sg_idx(sg_name)) == -1) //invalid group
  {
    NSTL1(NULL,NULL, "Warning: For Keyword G_PAGE_THINK_TIME, page think time can not be applied for unknown group '%s'. Group (%s) ignored.", sg_name, sg_name);
    return 0;  
  }
      
  session_idx = runProfTable[grp_idx].sessprof_idx;
  NSDL2_PARSING(NULL, NULL, "Session index = %d", session_idx);
  
  // Case 4 - Group and ALL pages
  if (strcasecmp(name, "ALL") == 0)
  {
    int i;
    for (i = 0; i < gSessionTable[session_idx].num_pages; i++)
    {
      if (runProfTable[grp_idx].page_think_table[i] == -1)
      {
        if (create_thinkprof_table_entry(&rnum) == FAILURE)
        {
          NS_EXIT(-1, CAV_ERR_1000002);
        }
        thinkProfTable[rnum].mode = think_mode;
        thinkProfTable[rnum].avg_time = avg_time;
        thinkProfTable[rnum].median_time = median_time;
        thinkProfTable[rnum].var_time = var_time;
        if (custom_page_think_time_callback[0])
          thinkProfTable[rnum].custom_page_think_time_callback = copy_into_big_buf(custom_page_think_time_callback, 0);
        else
          thinkProfTable[rnum].custom_page_think_time_callback = -1;
        NSDL3_PARSING(NULL, NULL, "%s", think_prof_to_str_non_shm(sg_name, name, &(thinkProfTable[rnum]),think_prof_buf));
        /*Converting session name and page name to page id */
  
        runProfTable[grp_idx].page_think_table[i] = rnum;
        NSDL3_PARSING(NULL, NULL, "ALL runProfTable[%d].page_think_table[%d] = %d\n", grp_idx, i, runProfTable[grp_idx].page_think_table[i]);
      } else
        NSDL3_PARSING(NULL, NULL, "ELSE runProfTable[%d].page_think_table[%d] = %d\n", grp_idx, i, runProfTable[grp_idx].page_think_table[i]);
    }
    return 0;
  }
 
  // Case 5 - Group and particular page (e.g. G1 Page1)
  // Case 5a - Group and particular page (e.g. G1 Page1) - Invalid page
  if ((page_id = find_page_idx(name, session_idx)) == -1) // invalid page print error message and exit
  {
    char *session_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011007, name, sg_name, session_name, "");
  }
  if (create_thinkprof_table_entry(&rnum) == FAILURE)
  {
    NS_EXIT(-1, CAV_ERR_1000002);
  }
  thinkProfTable[rnum].mode = think_mode;
  thinkProfTable[rnum].avg_time = avg_time;
  thinkProfTable[rnum].median_time = median_time;
  thinkProfTable[rnum].var_time = var_time;
  if (custom_page_think_time_callback[0]){
    thinkProfTable[rnum].custom_page_think_time_callback = copy_into_big_buf(custom_page_think_time_callback, 0);
  }
  else
    thinkProfTable[rnum].custom_page_think_time_callback = -1;
  NSDL3_PARSING(NULL, NULL, "%s", think_prof_to_str_non_shm(sg_name, name, &(thinkProfTable[rnum]),think_prof_buf));
  runProfTable[grp_idx].page_think_table[page_id - gSessionTable[session_idx].first_page] = rnum;
  NSDL3_PARSING(NULL, NULL, "runProfTable[%d].page_think_table[page_id(%d) - gSessionTable[%d].first_page(%d)] = %d\n Page_thinkTable[rnum].custom_page_think_time_callback[%s]", grp_idx, page_id, session_idx, gSessionTable[session_idx].first_page,runProfTable[grp_idx].page_think_table[page_id - gSessionTable[session_idx].first_page], RETRIEVE_BUFFER_DATA(thinkProfTable[rnum].custom_page_think_time_callback));
 return 0;                   
}

/*********************************************************************************************************
 * Description        : kw_set_g_page_think_time() method used to parse G_PAGE_THINK_TIME keyword,
 *                      parsing keyword for non runtime and runtime changes. This method is called from 
 *                      parse_group_keywords() in ns_parse_scen_conf.c.
 * Format             : G_PAGE_THINK_TIME <group_name> <page_name> <think_mode value1 value2>
 * Input Parameter
 *           buf      : Pointer in the received buffer for the keyword.
 *           pass     : Receive pass value.
 *        change_msg  : change_msg buffer prints error message if receiving error and at runtime buffer 
 *                      receives runtime change message.
 *      runtime_flag  : Receives runtime flag.
 * Output Parameter   : Set avg_time, median_time, think_mode, var_time.
 * Return             : Return 0 for sucess and non zero value for failure
 *************************************************************************************************************/
// Make sure size of err_msg buffer passed has enough space which is filled by usage message

char ptt_cll_back_msg[1024];

int  kw_set_g_page_think_time(char *buf, char *change_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH], value1[100], value2[100], chk_think_mode[100];
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  int num, think_mode, min_time, avg_time, median_time, max_time, var_time, ret;
  avg_time = median_time = var_time = 0;

  char *constant_value_str = value1;
  char *min_value_str = value1;
  char *avg_value_str = value1;
  char *max_value_str = value2;
  char *custom_page_think_time_callback = value1; 
  // Must make empty
  chk_think_mode[0] = 0; // default value is 0
  value1[0] = 0;
  value2[0] = 0;

  num = sscanf(buf, "%s %s %s %s %s %s %s", keyword, sg_name, name, chk_think_mode, value1, value2, tmp);
  
  if(num < 4 || num > 6) { //Check for extra arguments.
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_1);
  }    
  
  think_mode = atoi(chk_think_mode);

  if(think_mode < 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_8);

  if(ns_is_numeric(chk_think_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_2);
  }

  //validation for different think modes
  //PAGE_THINK_TIME_MODE_UNIFORM_RANDOM
  if (think_mode == PAGE_THINK_TIME_MODE_UNIFORM_RANDOM) {   
    if (num != 6)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_1);
    }

    min_time = atoi(min_value_str);

    if(min_time < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_8);

    if(ns_is_numeric(min_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_2);
    }
  
    max_time = atoi(max_value_str);

    if(max_time < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_8);

    if(ns_is_numeric(max_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_2); 
    }

    if (max_time <= min_time)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_5);
    }
    
    avg_time = min_time;
    median_time = max_time;

  } else if (think_mode == PAGE_THINK_TIME_MODE_INTERNET_RANDOM) {  //PAGE_THINK_TIME_MODE_INTERNET_RANDOM
    if (num != 5) 
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_1);
    
    median_time = atoi(avg_value_str); 

    if(median_time < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_8);

    if(ns_is_numeric(avg_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_2); 
    }
    
    if(median_time == 0 ){
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_4); 
    }

    //median_time = avg_time;/* The user is always inputting the first time arguement as the median time for mode 1, others are not provided by user */
    avg_time = -1;
    var_time = -1;

  } else if (think_mode == PAGE_THINK_TIME_MODE_CONSTANT) {  // PAGE_THINK_TIME_MODE_CONSTANT
    if (num != 5) 
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_1);

    avg_time = atoi(constant_value_str);

    if(avg_time < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_8);

    if (ns_is_numeric(constant_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_2); 
    }

  } else if (think_mode == PAGE_THINK_TIME_MODE_CUSTOM) {  // PAGE_THINK_TIME_MODE_CONSTANT
    if (num != 5) 
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_1);
    if(val_fname(custom_page_think_time_callback, 64))
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, ptt_cll_back_msg);
    }
  } else if (think_mode != PAGE_THINK_TIME_MODE_NO_THINK_TIME) { //PAGE_THINK_TIME_MODE_NO_THINK_TIME
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_PAGE_THINK_TIME_USAGE, CAV_ERR_1011005, CAV_ERR_MSG_3);
  }
    
  if (!runtime_flag)
  {
    ret = page_think_time_not_runtime(buf, sg_name, name, avg_time, median_time, think_mode, var_time, runtime_flag, change_msg, custom_page_think_time_callback);
  }
  else
  {
    ret = page_think_time_runtime(buf, sg_name, name, avg_time, median_time, think_mode, var_time, runtime_flag, change_msg, custom_page_think_time_callback);
    return(ret);
  }

  return 0;
}
 
/************************************************************************************************************
 * Description       : initialize_runprof_page_think_idx() method used to allocate memory to page_think_table 
 *                     in runproftable and memset -1 as default value, method is called from url.c.
 * Input Parameters  : None
 * Output Parameters : Set -1 values in page think time table for number of pages.
 * Return            : None
 ************************************************************************************************************/

void initialize_runprof_page_think_idx()
{
  int i, num_pages;
  g_actual_num_pages = 0;
  g_rbu_num_pages = 0;
  NSDL4_PARSING(NULL, NULL, "initialize_runprof_page_think_idx() : total_runprof_entries = %d", total_runprof_entries);
  for (i = 0; i < total_runprof_entries; i++) {
    num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;

    MY_MALLOC(runProfTable[i].page_think_table, sizeof(int) * num_pages, "runProfTable[i].page_think_table", i);
    memset(runProfTable[i].page_think_table, -1, sizeof(int) * num_pages);
    runProfTable[i].num_pages = num_pages;
    runProfTable[i].grp_ns_var_start_idx = -1;
   
    g_actual_num_pages += runProfTable[i].num_pages;

    //We are calculating number of pages which are using RBU.
    if(runProfTable[i].gset.rbu_gset.enable_rbu) {
      g_rbu_num_pages += runProfTable[i].num_pages;
    }
  }

  NSDL4_PARSING(NULL, NULL, "initialize_runprof_page_think_idx() : g_actual_num_pages = %d, "
                            "g_rbu_num_pages = %d, total_rbu_domain_entries = %d", 
                             g_actual_num_pages, g_rbu_num_pages, max_rbu_domain_entries);
}

/************************************************************************************************
 * Description       :free_runprof_page_think_idx() method used to free memory of page_think_table 
 *                    defined in runproftable, method is called from util.c.
 * Input Parameters  : None
 * Output Parameters : None
 * Return            : None
 ************************************************************************************************/

void free_runprof_page_think_idx()
{
  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    //num_pages = runProfTable[i].num_pages;
    FREE_AND_MAKE_NULL_EX(runProfTable[i].page_think_table, sizeof(int) * runProfTable[i].num_pages, "runProfTable[i].page_think_table", i); //added size of number of pages
  }
}


/**********************************************************************************************************
 * Description       : overrid_mode_to_str() method used to get think mode string in debug logs.
 * Input Parameter
 *             mode  : think mode value
 * Output Parameter  : none
 * Return            : Return string value corresonding think mode.
 **********************************************************************************************************/
#ifdef NS_DEBUG_ON
static char *overrid_mode_to_str(int mode)
{
  NSDL2_PARSING(NULL, NULL, "Method called. mode = %d", mode);

  switch(mode)
  {
    case OVERRIDE_REC_THINK_MODE_USE_SCEN_SETTING:
      return("UseScenSettings");
      break;

    case OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME:
      return("MultiplyRecordedThinkTime");
      break;

    case OVERRIDE_REC_THINK_MODE_RANDOM_PCT_REC_THINK_TIME:
      return("RandomPercentageOfRecordedThinkTime. ");
      break;
   
    default:
      return("InvalidThinkTime");
  }
}
#endif

/**********************************************************************************************
 * Description       : override_rec_to_str_shm() method used to print data for particular 
 *                     think mode in shared memory.
 * Input Parameter
 *      grp_id       : group index
 *   think_time_ptr  : pointer to shared memory
 * Output Parameter  : None
 * Return            : returns information buffer
 *************************************************************************************************/
#ifdef NS_DEBUG_ON
static char *override_rec_to_str_shm(int grp_id, OverrideRecordedThinkTime *think_time_ptr, char *buf)
{
  char grp_name[128] = "ALL";
  
  NSDL2_PARSING(NULL, NULL, "Method called. grp_id = %d", grp_id);

  if (grp_id != -1)
  {
    if(runProfTable) 
      strcpy(grp_name, BIG_BUF_MEMORY_CONVERSION(runProfTable[grp_id].scen_group_name));
    else
      strcpy(grp_name,runprof_table_shr_mem[grp_id].scen_group_name); // for shared memory
  }
  if(think_time_ptr->mode == OVERRIDE_REC_THINK_MODE_USE_SCEN_SETTING)
    sprintf(buf, "Group = %s, OverrideRecThinkMode = %s", grp_name,  overrid_mode_to_str(think_time_ptr->mode));
  else if(think_time_ptr->mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME)
    sprintf(buf, "Group = %s, OverrideRecThinkMode = %s, Multiplier = %f", grp_name, overrid_mode_to_str(think_time_ptr->mode), think_time_ptr->multiplier);
  else /*RANDOM_PCT_REC_THINK_TIME */
    sprintf(buf, "Group = %s, OverrideRecThinkMode = %s, Minimum = %f, Maximum = %f", grp_name, overrid_mode_to_str(think_time_ptr->mode), think_time_ptr->min, think_time_ptr->max );

  return(buf);
}
#endif

/***************************************************************************************************
 * Description       : log_page_time_for_debug()this method will log page think time in a data file
 *                     Called from ns_page_think_time.c. 
 * Input Parameter
 *             vptr  : pointer to VUser struct
 *   think_prof_ptr  : pointer to ThinkProfTableEntry_Shr 
 *   override_think  : pointer to OverrideRecordedThinkTime
 *       think_time  : Think time value
 *              now  : current time
 * Output Parameter  : none
 * Return            : none
 ***************************************************************************************************/

#ifdef NS_DEBUG_ON
void log_page_time_for_debug(VUser *vptr, ThinkProfTableEntry_Shr *think_prof_ptr, OverrideRecordedThinkTime* override_think, int think_time, u_ns_ts_t now)
{
  char file_name[4096];
  char page_think_info_buf[4096]; // use to fill with page think detail for debug log
  char override_think_info_buf[4096];

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC3) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_SCHEDULE)))
  return;
  sprintf(file_name, "%s/logs/TR%d/page_think_time.dat", g_ns_wdir, testidx);
  // Format of file
  // TimeStamp|Group|Page|PageThinkMode|OverrideOption|ThinkTime(ms)
  // 00:00:10|G1|Page1|Mode=Constat Constant=2000|0|0|0|1123|1.123
  ns_save_data_ex(file_name, NS_APPEND_FILE, "%s|%s|%s|%s|%s|%d", get_relative_time(), runprof_table_shr_mem[vptr->group_num].scen_group_name, vptr->cur_page->page_name, think_prof_data(vptr->group_num, vptr->cur_page->page_number, think_prof_ptr, page_think_info_buf), override_rec_to_str_shm(vptr->group_num, override_think, override_think_info_buf), think_time);

}
#endif

/**********************************************************************************************************************
 * Description       : override_rec_think_time_usage() macro used to print usage for G_OVERRIDE_RECORDED_THINK_TIME 
 *                     keyword and exit. Called from kw_set_g_page_think_time().
 * Input Parameters
 *        err         : Print error message.
 *        runtime_flag: Check for runtime changes
 *        err_buf     : Pointer to error buffer
 *  Output Parameters : None
 *  Return            : None
 *************************************************************************************************************************/
// Make sure size of err_msg buffer passed has enough space which is filled by usage message.

#define override_rec_think_time_usage(err, runtime_flag, err_buff) \
{ \
  sprintf(err_buff, "Error: Invalid value of G_OVERRIDE_RECORDED_THINK_TIME keyword: %s\n", err); \
  strcat(err_buff, "  Usage: G_OVERRIDE_RECORDED_THINK_TIME <group-name> <page_name> <mode> <value1> <value2>\n"); \
  strcat(err_buff, "    Group name can be ALL or any group name used in scenario group\n"); \
  strcat(err_buff, "    Mode:\n");\
  strcat(err_buff, "    1: Using scenario settings, value1 and value2 not applicable (Default)\n");\
  strcat(err_buff, "    2: Multiply recorded think time, value1 is multiplier and value2 not applicable.\n"); \
  strcat(err_buff, "    3: Random percentage of recorded think time, value1 and value2 are percentages.\n");\
  if(runtime_flag != 1){ \
    NS_EXIT(-1, "%s", err_buff); \
  } \
  else { \
    NSTL1_OUT(NULL, NULL, "%s", err_buff); \
    return -1; \
  } \
}

/*****************************************************************************************************
  Purpose: This function is used for initialising the tunprof table for override recorded think time
*****************************************************************************************************/
void initialize_runprof_override_rec_think_time()
{
  int i, num_pages;
  NSDL2_PARSING(NULL, NULL, "Method called");

  for (i = 0; i < total_runprof_entries; i++) {
    num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;

    MY_MALLOC(runProfTable[i].override_recorded_think_time_table, sizeof(int) * num_pages, "runProfTable[i].override_recorded_think_time_table;", i);
    memset(runProfTable[i].override_recorded_think_time_table, -1, sizeof(int) * num_pages);
    runProfTable[i].num_pages = num_pages;
  }
}

/*****************************************************************************************************
  Purpose: This function is used for freeing the runproftable for Override recorded think time.
*****************************************************************************************************/
void free_runprof_overrided_rec_think_time()
{
  int i;
  NSDL2_PARSING(NULL, NULL, "Method called");
  for (i = 0; i < total_runprof_entries; i++) {
    //num_pages = runProfTable[i].num_pages;
    FREE_AND_MAKE_NULL_EX(runProfTable[i].override_recorded_think_time_table, sizeof(int) * runProfTable[i].num_pages, "runProfTable[i].override_recorded_think_time_table", i); //added size of number of pages
  }
}

/*****************************************************************************************************************
   Purpose: This function is used for allocating the memory of overrideRecordedThinktimeTable and incrementing the 
            rnum to the total_recorded_think_time_entries.
   Input:  It takes group row num as input.
********************************************************************************************************************/
int create_recorded_think_time_table_entry(int *rnum)
{
  NSDL2_PARSING(NULL, NULL, "Method called");
  if (total_recorded_think_time_entries == max_recorded_think_time_entries)
  {
    MY_REALLOC_EX(overrideRecordedThinktimeTable, (max_recorded_think_time_entries + DELTA_RECORDED_THINK_TIME_ENTRIES) * sizeof(OverrideRecordedThinkTime), (max_recorded_think_time_entries * sizeof(OverrideRecordedThinkTime)), "override recorded think time", -1);
    if (!overrideRecordedThinktimeTable)
    {
      fprintf(stderr, "create_recorded_think_time_table_entry(): Error allcating more memory for override recorded think time\n");
      return FAILURE;
    } else max_recorded_think_time_entries += DELTA_RECORDED_THINK_TIME_ENTRIES;
  }
  *rnum = total_recorded_think_time_entries++;
  return (SUCCESS);
}


/*******************************************************************************************************
   Purpose: This function is used for G_OVERRIDE_PAGE_THINK_TIME keyword for initialising override think
            tables. It initialises all the members of override think time table to 0.
********************************************************************************************************/
void create_default_recorded_think_time(void)
{ 
  int rnum;
  
  NSDL2_PARSING(NULL, NULL, "Method called");
  if (create_recorded_think_time_table_entry(&rnum) == FAILURE) {
    NS_EXIT(-1, "Error in creating new recorded think time table entry");
  }

  assert(rnum == GLOBAL_RECORDED_THINK_TIME_IDX);

  overrideRecordedThinktimeTable[rnum].min = 0;
  overrideRecordedThinktimeTable[rnum].max = 0;
  overrideRecordedThinktimeTable[rnum].mode = 0;
  overrideRecordedThinktimeTable[rnum].multiplier = 0;
}


/*******************************************************************************************************
   Purpose: This function is used for G_OVERRIDE_PAGE_THINK_TIME keyword for runtime changes.
   Input:  It takes group name, page name, min time and think mode as arguments.
********************************************************************************************************/
int override_page_think_runtime(char *group, char *page_name, int min_time, int max_time, int think_mode, char *err_msg, int runtime_flag)
{
  int page_id;
  int i,j;
  //OverrideRecordedThinkTime_Shr *ptr;
  ThinkProfTableEntry_Shr *think_ptr;

  // called for Runtime change
  // for shared memory table
  // Case: where both group and page are ALL. These need to be set at index 0
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") == 0)) { // for all groups and pages
    for (i = 0; i < total_runprof_entries; i++) {
     for (j = 0; j < runprof_table_shr_mem[i].num_pages; j++) {
       think_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[i].page_think_table[j];
       if(think_mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME)
       {
         think_ptr->override_think_time.mode = think_mode;
         think_ptr->override_think_time.multiplier = min_time;
       }
       else{
         think_ptr->override_think_time.mode = think_mode;
         think_ptr->override_think_time.min = min_time;
         think_ptr->override_think_time.max = max_time;
       }
       NSDL3_PARSING(NULL, NULL, "Setting override recorded think time for run prof %d and page index %d with mode = %d", i, j, think_ptr->override_think_time.mode);
      }
    }
    return 0;
  }
  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") != 0)) {
    override_rec_think_time_usage("With group ALL, page name can't be specified", runtime_flag, err_msg);
  }

   // Case 3 - Invalid group name
  int grp_idx; // for particular group
  if ((grp_idx = find_sg_idx_shr(group)) == -1) { // for invalid group name
    sprintf(err_msg, "Warning: Unknown group %s for key G_OVERRIDE_RECORDED_THINK_TIME.\n.", group);
    return -1;
  }

  // Case 4 - Group and ALL pages 
  if (strcasecmp(page_name, "ALL") == 0) { // for all pages
    for (i = 0; i < runprof_table_shr_mem[grp_idx].num_pages; i++)
    {
      think_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[grp_idx].page_think_table[i];
      if(think_mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME)
      {
        think_ptr->override_think_time.mode = think_mode;
        think_ptr->override_think_time.multiplier = min_time;
      }
      else{
        think_ptr->override_think_time.mode = think_mode;
        think_ptr->override_think_time.min = min_time;
        think_ptr->override_think_time.max = max_time;
      }
      NSDL3_PARSING(NULL, NULL,"For ALL pages override recorded think mode = %d", think_ptr->override_think_time.mode);
    }
    return 0;
  }

   // Case 4 - Group and particular page (e.g. G1 Page1)

  SessTableEntry_Shr* sess_ptr = runprof_table_shr_mem[grp_idx].sess_ptr;
  if ((page_id = find_page_idx_shr(page_name, sess_ptr)) == -1) { // for particular pages
    //fprintf(stderr, "Warning:Unknown page %s for group %s for keyword G_OVERRIDE_RECORDED_THINK_TIME\n", page_name, group);
    NSTL1(NULL,NULL, "Warning:Unknown page %s for group %s for keyword G_OVERRIDE_RECORDED_THINK_TIME", page_name, group);
    sprintf(err_msg, "Warning:Unknown page %s for group %s for keyword G_OVERRIDE_RECORDED_THINK_TIME\n", page_name, group);
    return -1;
  }
  think_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[grp_idx].page_think_table[page_id];
  if (think_mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME)
  {
    think_ptr->override_think_time.mode = think_mode;
    think_ptr->override_think_time.multiplier = min_time;
  }
  else{
    think_ptr->override_think_time.mode = think_mode;
    think_ptr->override_think_time.min = min_time;
    think_ptr->override_think_time.max = max_time;
  }
  return 0;
}

/*******************************************************************************************************
   Purpose: This function is used for G_OVERRIDE_PAGE_THINK_TIME keyword for not runtime.
   Input:  It takes group name, page name, min time and think mode as arguments.
********************************************************************************************************/
int override_page_think_not_runtime(char *group, char *page_name, int min_time, int max_time, int think_mode, char *change_msg, int runtime_flag)
{
  char session_name[MAX_DATA_LINE_LENGTH];
  int session_idx, page_id, rnum;
  int grp_idx;

  NSDL2_PARSING(NULL, NULL, "Method called");

  // Case: where both group and page are ALL. These need to be set at index 0 
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") == 0))
  {
    if(think_mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME)
    {
      overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].multiplier = min_time;
      overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].mode = think_mode;
    }
    else {
      overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].min = min_time;
      overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].max = max_time;
      overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].mode = think_mode;
    }
    NSDL3_PARSING(NULL, NULL, "For ALL groups and ALL pages - override recorded think time mode = %d min value = %d max value = %d", overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].mode, overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].min, overrideRecordedThinktimeTable[GLOBAL_RECORDED_THINK_TIME_IDX].max);
    return 0;
  }
 
  // Case: - group ALL and page specified (This case is not allowed, as pagename may not present in all group)
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") != 0)) {
    override_rec_think_time_usage("With group ALL, page name can't be specified", runtime_flag, change_msg);
  }

  // Check for valid group
  if ((grp_idx = find_sg_idx(group)) == -1) //invalid group name
  {
    /*fprintf(stderr, "Warning: Scenario group (%s) used for G_OVERRIDE_RECORDED_THINK_TIME is not a valid group name. Group (%s) ignored.\n",
                                  group, group);*/
    NSTL1(NULL,NULL, "Warning: Scenario group (%s) used for G_OVERRIDE_RECORDED_THINK_TIME is not a valid group name. Group (%s) ignored.", group, group);
    return 0;
  }

  session_idx = runProfTable[grp_idx].sessprof_idx;
 
  // Case: group is specified and page is ALL
  if (strcasecmp(page_name, "ALL") == 0)
  {
    int i;
    for(i = 0; i < gSessionTable[session_idx].num_pages; i++)
    {
      if (runProfTable[grp_idx].override_recorded_think_time_table[i] == -1)
      {
        if (create_recorded_think_time_table_entry(&rnum) == FAILURE)
        {
          NS_EXIT(-1, "Error in creating new override recorded think time table entry");
        }
        if (think_mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME)
        {
          overrideRecordedThinktimeTable[rnum].multiplier = min_time;
          overrideRecordedThinktimeTable[rnum].mode = think_mode;
        }
        else {
          overrideRecordedThinktimeTable[rnum].min = min_time;
          overrideRecordedThinktimeTable[rnum].max = max_time;
          overrideRecordedThinktimeTable[rnum].mode = think_mode;
        }
        runProfTable[grp_idx].override_recorded_think_time_table[i] = rnum;
        NSDL3_PARSING(NULL, NULL, "ALL runProfTable[%d].override_recorded_think_time_table[%d] = %d\n",
					        grp_idx, i, runProfTable[grp_idx].override_recorded_think_time_table[i]);
        NSDL3_PARSING(NULL, NULL, "for ALL pages override recorded think time min value %d max value = %d mode = %d\n",
        	overrideRecordedThinktimeTable[rnum].min, overrideRecordedThinktimeTable[rnum].max, overrideRecordedThinktimeTable[rnum].mode);
      } else
        NSDL3_PARSING(NULL, NULL, "ELSE runProfTable[%d].override_recorded_think_time_table[%d] = %d\n",
                                 grp_idx, i, runProfTable[grp_idx].override_recorded_think_time_table[i]);
   }
   return 0;
  }

  // Case: Group and page both are specific
  if ((page_id = find_page_idx(page_name, session_idx)) == -1) // for particular page
  {
    NS_EXIT(-1, "Unknown page %s for session %s for keyword G_OVERRIDE_RECORDED_THINK_TIME", page_name, session_name);
  }
  if (create_recorded_think_time_table_entry(&rnum) == FAILURE) // allocate memory to override recorded think time table
  {
    NS_EXIT(-1, "Error in creating new override recorded think time table entry");
  }
  if(think_mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME)
  {
    overrideRecordedThinktimeTable[rnum].multiplier = min_time;
    overrideRecordedThinktimeTable[rnum].mode = think_mode;
  }
  else{
    overrideRecordedThinktimeTable[rnum].min = min_time;
    overrideRecordedThinktimeTable[rnum].max = max_time;
    overrideRecordedThinktimeTable[rnum].mode = think_mode;
  }

  NSDL3_PARSING(NULL, NULL, "recorded think time mode = %d min value = %d max value = %d", overrideRecordedThinktimeTable[rnum].mode, 
                overrideRecordedThinktimeTable[rnum].min, overrideRecordedThinktimeTable[rnum].max);
  runProfTable[grp_idx].override_recorded_think_time_table[page_id - gSessionTable[session_idx].first_page] = rnum;
  NSDL3_PARSING(NULL, NULL, "runProfTable[%d].override_recorded_think_time_table[page_id(%d) - gSessionTable[%d].first_page(%d)] = %d\n",
                           grp_idx, page_id, session_idx, gSessionTable[session_idx].first_page,
                           runProfTable[grp_idx].override_recorded_think_time_table[page_id - gSessionTable[session_idx].first_page]);
  return 0;
}

/****************************************************************************************************************
 * Description        : kw_set_override_recorded_think_time() method used to parse G_OVERRIDE_RECORDED_THINK_TIME
 *                      keyword,parsing keyword for non runtime and runtime changes. This method is called from
 *                      parse_group_keywords() in ns_parse_scen_conf.c and ns_runtime_changes.c.
 * Format             : G_OVERRIDE_RECORDED_THINK_TIME <group-name> <mode> <min_value> <max_value>
 * Input Parameter
 *           buf      : Pointer in the received buffer for the keyword.
 *           to_change: Pointer to OverrideRecordedThinkTime
 *        change_msg  : change_msg buffer prints error message if receiving error and at runtime buffer
 *                      receives runtime change message.
 *      runtime_flag  : Receives runtime flag.
 * Output Parameter   : Set think_mode, min_value, max_value.
 * Return             : Return 0 for sucess and non zero value for failure
 ****************************************************************************************************************/
// Make sure size of err_msg buffer passed has enough space which is filled by usage message

int kw_set_override_recorded_think_time(char *buf, char *change_msg, int runtime_flag)
{
  int num;
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; // for extra argument
  char mode[100], min_value[100], max_value[100];
  char page_name[MAX_DATA_LINE_LENGTH]; 
  mode[0] = 0; // setting default value(first value of array is 0)
  min_value[0] = 0;
  max_value[0] = 0;
    
  float min_time = 0.0;
  float max_time = 0.0;
  int think_mode = 0;

  NSDL2_PARSING(NULL, NULL, "Method called");
  num = sscanf(buf, "%s %s %s %s %s %s %s", keyword, sg_name, page_name, mode, min_value, max_value, tmp);
  
  if(num < 3 || num > 6) { //Check for extra arguments.
   override_rec_think_time_usage("Invalid number of arguments", runtime_flag, change_msg);
  }

  if(ns_is_numeric(mode) == 0) {
    override_rec_think_time_usage("Override recorded think mode option can have only integer value.", runtime_flag, change_msg);
  }
  
  think_mode = atoi(mode);
  
  if(think_mode < 0 || think_mode > 3) {
    override_rec_think_time_usage("Unknown think mode for keyword G_OVERRIDE_RECORDED_THINK_TIME", runtime_flag, change_msg);
  }
  else if(think_mode == OVERRIDE_REC_THINK_MODE_RANDOM_PCT_REC_THINK_TIME) 
  {
    if (num != 6)
      override_rec_think_time_usage("Need at least FIVE fields after keyword G_OVERRIDE_RECORDED_THINK_TIME for think_mode of 3", runtime_flag, change_msg);

    if(ns_is_numeric(min_value) == 0) 
      override_rec_think_time_usage("Minimum value can have only integer value", runtime_flag, change_msg);
    
    min_time = atof(min_value);

    if(ns_is_numeric(max_value) == 0) 
      override_rec_think_time_usage("Maximum value can have only integer value", runtime_flag, change_msg);
    
    max_time = atof(max_value);
 
    if((min_time < 0.0) || (max_time < 0.0)) 
      override_rec_think_time_usage("Override recorded think time value can not be less than 0", runtime_flag, change_msg);
    
    if (max_time <= min_time)
      page_think_time_usage("For keyword G_OVERRIDE_RECORDED_THINK_TIME the max value must be greater than the min value", runtime_flag, change_msg);
    
   if(runtime_flag)
      override_page_think_runtime(sg_name, page_name, min_time, max_time, think_mode, change_msg, runtime_flag);
    else
      override_page_think_not_runtime(sg_name, page_name, min_time, max_time, think_mode, change_msg, runtime_flag);
    return 0;

  } else if (think_mode == OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME) {
    if (num != 5) 
      override_rec_think_time_usage("Need at least Four fields after keyword G_OVERRIDE_RECORDED_THINK_TIME for think_mode of 2", runtime_flag, change_msg);
    
    if(ns_is_float(min_value) == 0) 
      override_rec_think_time_usage("Multiplier can have only decimal value", runtime_flag, change_msg);//decimal allowed
    
    min_time = atof(min_value);
    
    if(min_time < 0.0) 
      override_rec_think_time_usage("Override recorded think time value can not be less than 0", runtime_flag, change_msg);
    
   if(runtime_flag)
      override_page_think_runtime(sg_name, page_name, min_time, max_time, think_mode, change_msg, runtime_flag);
    else
      override_page_think_not_runtime(sg_name, page_name, min_time, max_time, think_mode, change_msg, runtime_flag);
    return 0; 
  }
  if(runtime_flag)
    override_page_think_runtime(sg_name, page_name, min_time, max_time, think_mode, change_msg, runtime_flag);
  else
    override_page_think_not_runtime(sg_name, page_name, min_time, max_time, think_mode, change_msg, runtime_flag);
  return 0;
}

