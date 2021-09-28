#include <dlfcn.h>
#include "ns_inline_delay.h"
#include "ns_page_based_stats.h"
#include "ns_exit.h"
#include "ns_trace_level.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_page_think_time_parse.h"

#define INLINE_HEADER "RelativeTime|PageId|UrlId|LogTime"

static int create_inline_delay_table_entry(int *row_num); 
static int block_time_debug_fd = -1 ;

/*----------------------------------------------------------------------------
This function will set member custom_delay_func_ptr of table inlineDelayTable. 
This function will be called from ns_parent.c when shared memory is created.
------------------------------------------------------------------------------*/

inline void fill_custom_delay_fun_ptr()
{
  int i, j;
  int num_pages;
  void *handle;
  InlineDelayTableEntry_Shr *ptr;

  NSDL2_PARSING(NULL, NULL, "total_runprof_entries = [%d]", total_runprof_entries);
  // As Inline Delay is page based, we need to apply handle (which we have saved in session table in ns_script_parse.c) on each page.
  for (i = 0; i < total_runprof_entries; i++) {
    // Fetch handle for current session.
    handle = gSessionTable[runprof_table_shr_mem[i].sess_ptr->sess_id].handle;
    // Fetch number of pages for current session.
    num_pages = gSessionTable[runprof_table_shr_mem[i].sess_ptr->sess_id].num_pages;
    NSDL2_PARSING(NULL, NULL, "handle = [%p], num_pages = [%d]", handle, num_pages);
   
    // Fill custom_delay_func_ptr for each page if custom_delay_callback method is given.
    for (j = 0; j < num_pages; j++) {
      ptr = (InlineDelayTableEntry_Shr *)(runprof_table_shr_mem[i].inline_delay_table[j]);
      if( ptr && ptr->custom_delay_callback) {
        ptr->custom_delay_func_ptr = dlsym(handle, ptr->custom_delay_callback);
        NSDL2_PARSING(NULL, NULL, "custom_delay_callback = [%s], custom_delay_func_ptr = [%p]", 
                                   ptr->custom_delay_callback, ptr->custom_delay_func_ptr);
      }
    }
  }
}


/*----------------------------------------------------------------------------
This function is used  to Dump inline block time in  file 'inline_block_time'.
This file is created per NVM and stored in TR directory.
-----------------------------------------------------------------------------*/

void dump_inline_block_time(VUser *vptr, connection* cptr, u_ns_ts_t time_diff)
{
  char log_file[1024], buffer[1024];

  NSDL2_PARSING (NULL, NULL, "Method called, time_diff = [%llu]", time_diff);
  sprintf(log_file, "%s/logs/TR%d/inline_block_time_%d", g_ns_wdir, testidx, child_idx);
  if (block_time_debug_fd <= 0 )
  {
    block_time_debug_fd = open (log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
    if (block_time_debug_fd <= 0)
    {
      NS_EXIT(-1, "Error: Error in opening file '%s', Error = '%s'", log_file, nslb_strerror(errno));
    }
    if(INLINE_HEADER) {
      write(block_time_debug_fd, INLINE_HEADER, strlen(INLINE_HEADER));
    }
  }
  sprintf(buffer,"\n%s|%u|%u|%llu", get_relative_time(), vptr->cur_page->page_id, cptr->url_num->proto.http.url_index, time_diff);
  NSDL2_PARSING (NULL, NULL, "buffer = %s", buffer);
  write(block_time_debug_fd, buffer, strlen(buffer));
  
}

/**************************************************************************************** 
 * Description       : Method used to create inline delay time for default value of keyword.
 *                     This method is called from parse_files()in url.c. 
 * Input Parameters  : None
 * Output Parameters : Set page delay time entries, mode, avg_time, median_time, var_time.
 * Return            : None
 ****************************************************************************************/

void create_default_global_inline_delay_profile(void)
{
  int rnum;

  NSDL2_PARSING (NULL, NULL, "Method called");
  if (create_inline_delay_table_entry(&rnum) == FAILURE) {
    NS_EXIT(-1, "Error in creating new think time entry");
  }

  assert(rnum == GLOBAL_INLINEDELAY_IDX);
  // Setting default values
  inlineDelayTable[rnum].mode = INLINE_DELAY_MODE_NO_DELAY;
  inlineDelayTable[rnum].avg_time = 0;
  inlineDelayTable[rnum].median_time = 0;
  inlineDelayTable[rnum].var_time = 0;
  inlineDelayTable[rnum].additional_delay_mode1 = 0;
  inlineDelayTable[rnum].min_limit_time = 0;
  inlineDelayTable[rnum].max_limit_time = 0x7FFFFFFF;
  inlineDelayTable[rnum].custom_delay_callback = -1;
  inlineDelayTable[rnum].custom_delay_func_ptr = NULL;
}

/******************************************************************************************** 
 * Description       : Method used to allocate memory to inline delay table.
 * Input Parameters
 *      row_num      : Pointer to row number.
 * Output Parameters : Set total entries of inline delay table(total_inline_delayprof_entries).
 * Return            : Return -1 on error in allocating memory to inlineDelayTable else return 1.
 ********************************************************************************************/

static int create_inline_delay_table_entry(int *row_num) 
{
  NSDL2_PARSING (NULL, NULL, "Method called");

  if (total_inline_delay_entries == max_inline_delay_entries) 
  {
    MY_REALLOC_EX (inlineDelayTable, (max_inline_delay_entries + DELTA_INLINE_DELAY_ENTRIES) * sizeof(InlineDelayTableEntry), (max_inline_delay_entries * sizeof(InlineDelayTableEntry)), "inlineDelayTable", -1); // Added old size of table
    if (!inlineDelayTable) {
      fprintf(stderr, "create_inline_delay_table_entry(): Error allcating more memory for inline delay entries\n");
      return FAILURE;
    } else max_inline_delay_entries += DELTA_INLINE_DELAY_ENTRIES;
  }
  *row_num = total_inline_delay_entries++;
  return (SUCCESS);
}


/************************************************************************************************************
 * Description       : initialize_runprof_inline_delay_idx() method used to allocate memory to inline_delay_table 
 *                     in runproftable and memset -1 as default value, method is called from url.c.
 * Input Parameters  : None
 * Output Parameters : Set -1 values in inline delay time table for number of pages.
 * Return            : None
 ************************************************************************************************************/

void initialize_runprof_inline_delay_idx()
{
  int i, num_pages;
  for (i = 0; i < total_runprof_entries; i++) {
    num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;

    MY_MALLOC(runProfTable[i].inline_delay_table, sizeof(int) * num_pages, "runProfTable[i].inline_delay_table", i);
    memset(runProfTable[i].inline_delay_table, -1, sizeof(int) * num_pages);
    runProfTable[i].num_pages = num_pages;
  }
}


/************************************************************************************************
 * Description       :free_runprof_inline_delay_idx() method used to free memory of inline_delay_table 
 *                    defined in runproftable, method is called from util.c.
 * Input Parameters  : None
 * Output Parameters : None
 * Return            : None
 ************************************************************************************************/

void free_runprof_inline_delay_idx()
{
  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    //num_pages = runProfTable[i].num_pages;
    FREE_AND_MAKE_NULL_EX(runProfTable[i].inline_delay_table, sizeof(int) * runProfTable[i].num_pages, "runProfTable[i].inline_delay_table", i); 
    //added size of number of pages
  }
}


/**********************************************************************************************************
 * Description       : delay_mode_to_str() method used to get delay mode string in debug logs.
 * Input Parameter
 *       delay_mode  : delay mode value
 * Output Parameter  : none
 * Return            : Return string value corresonding delay mode.
 **********************************************************************************************************/
#ifdef NS_DEBUG_ON
static char *delay_mode_to_str(int delay_mode)
{
  NSDL2_PARSING(NULL, NULL, "Method called. delay_mode = %d", delay_mode);

  switch(delay_mode)
  {
    case INLINE_DELAY_MODE_NO_DELAY:
      return("NoDelayTime");
      break;

    case INLINE_DELAY_MODE_INTERNET_RANDOM:
      return("InternetRandomDelayTime");
      break;

    case INLINE_DELAY_MODE_CONSTANT:
      return("ConstantDelayTime");
      break;

    case INLINE_DELAY_MODE_UNIFORM_RANDOM:
      return("UniformRandomDelayTime");
      break;

    case INLINE_DELAY_MODE_CUSTOM_DELAY:
      return("Custom Delay");
      break;

    default:
      return("InvalidDelayTime");
  }
}
#endif


/****************************************************************************************** 
 * Description      : In InternetRandom inline delay mode, gsl calculation are done to find 
 *                    mean,mode,median etc to obtain inline delay time.Now gsl warnings were 
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
 *                      mode within or among groups.Here we need to determine inlineDelayTable index 
 *                      to compare median time for combination of group and page.  
 * Input Parameter    :
 * cur_idx            : current index in inline delay prof table  
 * Return             : If median matches then return 0 else for new entry return 1
 ******************************************************************************************************/
static int compare_ptt_per_pg_per_grp(int cur_idx)
{
  int idx;
  NSDL2_PARSING (NULL, NULL, "Method called, current index = %d", cur_idx);

  /* Find inlineDelayTable index to determine median time and mode matches for current index
   * Here we find index in reverse order, for cur_idx = 5, we will be finding ptt for (4-0) index*/
  for (idx = (cur_idx - 1); idx >= 0; idx--)
  {
    if ((inline_delay_table_shr_mem[cur_idx].median_time == inline_delay_table_shr_mem[idx].median_time) && (inline_delay_table_shr_mem[cur_idx].mode == inline_delay_table_shr_mem[idx].mode))
    {
      NSDL2_PARSING (NULL, NULL, "PTT matches in inline_delay_table_shr_mem, index = %d", idx);
      return idx;//Match entry
    }
  }
  return -1;//New entry 
}


/**********************************************************************************************
 * Description       : inline_delay_prof_data() method used to print data for particular delay
 *                     mode in shared memory.
 * Input Parameter
 *      grp_id       : group index
 *      pg_idx       : page index
 *   inline_delay_ptr  : pointer to shared memory
 *      buf          : to fill information
 * Output Parameter  : None
 * Return            : returns information buffer
 *************************************************************************************************/
#ifdef NS_DEBUG_ON
static char *inline_delay_prof_data(int grp_id, int pg_idx, InlineDelayTableEntry_Shr *inline_delay_ptr, char *buf)
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
  if(inline_delay_ptr->mode == INLINE_DELAY_MODE_NO_DELAY)
    sprintf(buf, "Group = %s, Page = %s, InlineDelayMode = %s", grp_name, page_name, delay_mode_to_str(inline_delay_ptr->mode));

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_INTERNET_RANDOM)
    sprintf(buf, "Group = %s, Page = %s, InlineDelayMode = %s, Median = %.3f sec, ConstantTimeMode1 = %.3f secs",
            grp_name, page_name, delay_mode_to_str(inline_delay_ptr->mode), (double)inline_delay_ptr->median_time/1000.0,
            (double)inline_delay_ptr->additional_delay_mode1/1000.0);

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_CONSTANT)
    sprintf(buf, "Group = %s, Page = %s, InlineDelayMode = %s, Constant = %.3f sec",
            grp_name, page_name, delay_mode_to_str(inline_delay_ptr->mode), (double)inline_delay_ptr->avg_time/1000.0);

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_CUSTOM_DELAY)
    sprintf(buf, "Group = %s, Page = %s, InlineDelayMode = %s", grp_name, page_name, delay_mode_to_str(inline_delay_ptr->mode)); 

  else /* UNIFORM_RANDOM */
    sprintf(buf, "Group = %s, Page = %s, InlineDelayMode = %s, Min = %.3f sec, Max = %.3f sec",
            grp_name, page_name, delay_mode_to_str(inline_delay_ptr->mode), (double)inline_delay_ptr->avg_time/1000.0,
            (double)inline_delay_ptr->median_time/1000.0);

  return(buf);
}
#endif

/**********************************************************************************************
 * Description       : inline_delay_prof_to_str_non_shm() method used to print data for particular 
 *                     inline delay mode in non-shared memory.
 * Input Parameter
 *      grp_name     : current group name
 *      pg_name      : current page name
 *   inline_delay_ptr  : pointer to non-shared memory
 * Output Parameter  : None
 * Return            : returns information buffer 
 ************************************************************************************************/

#ifdef NS_DEBUG_ON
static char *inline_delay_prof_to_str_non_shm(char *grp_name, char *pg_name,  InlineDelayTableEntry *inline_delay_ptr, char *buf)
{
  NSDL2_PARSING(NULL, NULL, "Method called. grp_name = %s pg_name = %s ", grp_name, pg_name);

  if(inline_delay_ptr->mode == INLINE_DELAY_MODE_NO_DELAY)
    sprintf(buf, "Inline delay for Group = %s, Page = %s, InlinDelayMode = %s",
            grp_name, pg_name, delay_mode_to_str(inline_delay_ptr->mode));

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_INTERNET_RANDOM)
    sprintf(buf, "Inline delay for Group = %s, Page = %s, InlineDelayMode = %s, MedianTime = %.3f secs,\
            ConstantTimeMode1 = %.3f secs min_limit_time = %.3f max_limit_time = %.3f",
            grp_name, pg_name, delay_mode_to_str(inline_delay_ptr->mode), (double)inline_delay_ptr->median_time/1000.0,
            (double)inline_delay_ptr->additional_delay_mode1/1000.0, (double)inline_delay_ptr->min_limit_time/1000.0,
            (double)inline_delay_ptr->max_limit_time/1000.0);

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_CONSTANT)
    sprintf(buf, "Inline delay for Group = %s, Page = %s, InlineDelayMode = %s, ConstantTime = %.3f secs", grp_name, pg_name,
            delay_mode_to_str(inline_delay_ptr->mode), (double )inline_delay_ptr->avg_time/1000.0);

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_CUSTOM_DELAY)
    sprintf(buf, "Inline delay for Group = %s, Page = %s, InlineDelayMode = %s, Callback Method = %s", grp_name, pg_name,
            delay_mode_to_str(inline_delay_ptr->mode), RETRIEVE_BUFFER_DATA(inline_delay_ptr->custom_delay_callback));

  else  /* UNIFORM_RANDOM */
    sprintf(buf, "Inline delay for Group = %s, Page = %s, InlineDelayMode = %s, MinTime = %.3f secs, MaxTime = %.3f secs", 
            grp_name, pg_name, delay_mode_to_str(inline_delay_ptr->mode), (double )inline_delay_ptr->avg_time/1000.0,
            (double )inline_delay_ptr->median_time/1000.0);

  return(buf);
}
#endif



/******************************************************************************************************
 * Description        : Method copy_inline_delay_prof_to_shr() used to copy inline delay table in shared memory.
 *                      Add page inline delay mode values for total number of pages across all scenario groups.
 *                      This method is called from copy_structs_into_shared_mem()in util.c.
 * Input Parameter    : None
 * Output Parameter   : Set page inline delay mode values for total number of pages.
 * Return             : None
 ******************************************************************************************************/

void copy_inline_delay_to_shr(void)
{
  NSDL2_PARSING (NULL, NULL, "Method called");
  int i;
  char *ptr = NULL;
  
#ifdef NS_DEBUG_ON
  char buf[4096];
#endif
  //printf("Initializing page inline delay time settings. It may take time...\n");
  NSTL1(NULL, NULL, "Initializing page inline delay time settings. It may take time...");
  if (total_inline_delay_entries == 0) 
    return;

  int actual_num_pages = 0;

  /*Redirect ns_weibthink_calc warning/error message to file octave.out*/
  initialize_octave();

  for (i = 0; i < total_runprof_entries; i++) {
    actual_num_pages += runProfTable[i].num_pages;
    NSDL4_PARSING(NULL, NULL, "Total number of pages of all scenario groups = %d", actual_num_pages);
  }
  
  inline_delay_table_shr_mem = (InlineDelayTableEntry_Shr*) do_shmget(sizeof(InlineDelayTableEntry_Shr) * actual_num_pages, "inline delay profile table");
  /* Per session, per page assign inline delay prof. */
  int j, k = 0;
  for (i = 0; i < total_runprof_entries; i++) {
    NSDL4_PARSING(NULL, NULL, "runProfTable[%d].num_pages = %d", i, runProfTable[i].num_pages);
    for (j = 0; j < runProfTable[i].num_pages; j++) {
      int idx = (runProfTable[i].inline_delay_table[j] == -1) ? 0 : runProfTable[i].inline_delay_table[j];
      NSDL4_PARSING(NULL, NULL, "k = %d, runProfTable[%d].inline_delay_table[%d] = %d, idx = %d, avg_time = %d, "
                                "inlineDelayTable[idx].custom_delay_callback = %lu", 
                                 k, i, j, runProfTable[i].inline_delay_table[j], idx, inlineDelayTable[idx].avg_time,
                                 inlineDelayTable[idx].custom_delay_callback);

      /*Bug 53236: Incase of SockJS, one URL is always send as Inline*/
      if(!(ptr = strstr(RETRIEVE_BUFFER_DATA(gPageTable[j].page_name), "sockJsConnect_")))
      {
        inline_delay_table_shr_mem[k].mode = inlineDelayTable[idx].mode;
        inline_delay_table_shr_mem[k].avg_time = inlineDelayTable[idx].avg_time;
        inline_delay_table_shr_mem[k].median_time = inlineDelayTable[idx].median_time;
        inline_delay_table_shr_mem[k].var_time = inlineDelayTable[idx].var_time;
        inline_delay_table_shr_mem[k].additional_delay_mode1 = inlineDelayTable[idx].additional_delay_mode1;
        inline_delay_table_shr_mem[k].min_limit_time = inlineDelayTable[idx].min_limit_time;
        inline_delay_table_shr_mem[k].max_limit_time = inlineDelayTable[idx].max_limit_time;
        inline_delay_table_shr_mem[k].custom_delay_func_ptr = inlineDelayTable[idx].custom_delay_func_ptr;
         
        NSDL3_PARSING(NULL, NULL, "custom delay callback offset in local table = [%d]", inlineDelayTable[idx].custom_delay_callback); 
        if(inlineDelayTable[idx].custom_delay_callback != -1)
          inline_delay_table_shr_mem[k].custom_delay_callback = BIG_BUF_MEMORY_CONVERSION(inlineDelayTable[idx].custom_delay_callback);
        else
          inline_delay_table_shr_mem[k].custom_delay_callback = NULL;
      }
 
      if (inlineDelayTable[idx].mode == INLINE_DELAY_MODE_INTERNET_RANDOM) {
        int matching_idx = compare_ptt_per_pg_per_grp(k);
        /* If median time is unique for a group and page combination then we need to calculate
         * a and b value. Otherwise update inline_delay_table_shr_mem with old delay table entry */
        if (matching_idx == -1) {
          if (ns_weibthink_calc(inlineDelayTable[idx].avg_time, inlineDelayTable[idx].median_time, inlineDelayTable[idx].var_time, &inline_delay_table_shr_mem[k].a, &inline_delay_table_shr_mem[k].b) == -1) {
            NS_EXIT(-1, "error in calculating a and b values for the weib ran num gen");
          }
        } else {
          /* Changes done to fill a and b value, page index should be pg_idx */
          NSDL3_PARSING(NULL, NULL, "PTT entry found in inline delay table matching_idx= %d", matching_idx);
          inline_delay_table_shr_mem[k].a = inline_delay_table_shr_mem[matching_idx].a;
          inline_delay_table_shr_mem[k].b = inline_delay_table_shr_mem[matching_idx].b;
        }
        NSDL3_PARSING(NULL, NULL, "Average_time = %d, Median_Time = %d, Var_Time = %d ConstantTimeMode1 = %d min_limit_time = %d max_limit_time = %d", inlineDelayTable[idx].avg_time, inlineDelayTable[idx].median_time, inlineDelayTable[idx].var_time, inlineDelayTable[idx].additional_delay_mode1, inlineDelayTable[idx].min_limit_time, inlineDelayTable[idx].max_limit_time);
        NSDL3_PARSING(NULL, NULL, "The calulated a and b values for G_INLINE_DELAY Mode = 1 are: a = %f, b = %f", inline_delay_table_shr_mem[k].a, inline_delay_table_shr_mem[k].b);
      }
      else if (inlineDelayTable[idx].mode == INLINE_DELAY_MODE_UNIFORM_RANDOM) {
        inline_delay_table_shr_mem[k].a = inlineDelayTable[idx].avg_time;
        inline_delay_table_shr_mem[k].b = inlineDelayTable[idx].median_time;
      }
      NSDL3_PARSING(NULL, NULL, "InlineDelay: Shared Memory - %s", inline_delay_prof_data(i, j, &(inline_delay_table_shr_mem[k]), buf));
      k++;
    }
  }
  //printf("Initializations of inline delay time settings completed.\n");
  NSTL1(NULL, NULL, "Initialization of inline delay time settings completed.");
}


/**********************************************************************************************************************
 * Description       : inline_delay_usage() macro used to print usage for G_PAGE_THINK_TIME keyword and exit.
 *                     Called from kw_set_g_inline_delay_time().
 * Input Parameters
 *       err         : Print error message.
 *       runtime_flag: Check for runtime changes
 *       err_buf     : Pointer to error buffer
 * Output Parameters : None
 * Return            : None
 ************************************************************************************************************************/
// Make sure size of err_msg buffer passed has enough space which is filled by usage message.

#define inline_delay_usage(err, runtime_flag, err_buff) \
{ \
  sprintf(err_buff, "Error: Invalid value of G_INLINE_DELAY keyword: %s\n", err); \
  strcat(err_buff, "  Usage: G_INLINE_DELAY <group_name> <page_name> <delay_mode> <value1> <value2>\n"); \
  strcat(err_buff, "    Group name can be ALL or any group name used in scenario group\n"); \
  strcat(err_buff, "    Page name can be ALL or name of page for which inline delay time is to be enabled.\n"); \
  strcat(err_buff, "    ALL and particular page combination is not a valid combination (e.g. ALL page1)\n"); \
  strcat(err_buff, "    DELAY mode:\n");\
  strcat(err_buff, "    0: No delay time, value1 and value2 not applicable (Default)\n");\
  strcat(err_buff, "    1: Internet type random, value1 is median time and value2 is constant delay and value3 for min_limit_time and value4 for max_limit_time.\n"); \
  strcat(err_buff, "    2: Constant time, value1 is constant time and value2 not applicable.\n");\
  strcat(err_buff, "    3: Uniform random, value1 is the minimum time and value2 is maximum time.\n"); \
  strcat(err_buff, "    4: Custom Delay, value1 is the name of Callback Method.\n"); \
  strcat(err_buff, "Note: Time values (value1 and value2) should be specified in milliseconds.\n"); \
  if(runtime_flag != 1) { \
    NS_EXIT(-1, "%s", err_buff); \
  } \
  else { \
    NSTL1_OUT(NULL, NULL, "%s", err_buff); \
    return -1; \
  } \
}

#define inline_min_con_reuse_delay_usage(err, err_buff) \
{ \
  sprintf(err_buff, "Error: Invalid value of G_INLINE_MIN_CON_REUSE_DELAY keyword: %s\n", err); \
  strcat(err_buff, " Usage: G_INLINE_MIN_CON_REUSE_DELAY <group_name> <min_value> <max_value>\n"); \
  strcat(err_buff, " Group name can be ALL or any group name used in scenario group\n"); \
  strcat(err_buff, "Note: Time values (MIN and MAX) should be specified in milliseconds.\n"); \
  NS_EXIT(-1, "%s", err_buff); \
}


/***********************************************************************************************
 * Description       : calc_a_and_b_value() macro used to calculate a and b value at runtime 
 *                     change and update shared memory.
 * Input Parameter
 *      ptt_ptr      : pointer to inline delay table in shared memory
 * Output Parameter  : None
 * Return            : None     
 ***********************************************************************************************/

#define calc_a_and_b_value(ptt_ptr) \
{                            \
  if (ptt_ptr->mode == INLINE_DELAY_MODE_INTERNET_RANDOM) { \
    NSDL3_PARSING(NULL, NULL, "ptt_ptr->avg_time = %d, ptt_ptr->median_time = %d, ptt_ptr->var_time = %d, ptt_ptr->a = %f,ptt_ptr->b = %f ptt_ptr->additional_delay_mode1 = %d", ptt_ptr->avg_time, ptt_ptr->median_time, ptt_ptr->var_time, ptt_ptr->a, ptt_ptr->b, ptt_ptr->additional_delay_mode1); \
    if (ns_weibthink_calc(ptt_ptr->avg_time, ptt_ptr->median_time, ptt_ptr->var_time, &ptt_ptr->a, &ptt_ptr->b) == -1) {  \
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,__FILE__, (char*)__FUNCTION__,"Error:Calculating a and b values for the weib ran num gen");  \
    return -1; \
    } \
    NSDL3_PARSING(NULL, NULL, "The calulated a and b values for G_INLINE_DELAY InlineDelayMode = %s a = %f, b = %f", delay_mode_to_str(ptt_ptr->mode), ptt_ptr->a, ptt_ptr->b); \
  } \
  else if (ptt_ptr->mode == INLINE_DELAY_MODE_UNIFORM_RANDOM) { \
    ptt_ptr->a = ptt_ptr->avg_time; \
    ptt_ptr->b = ptt_ptr->median_time; \
  } \
} 

/***********************************************************************************************
 * Description       : ADD_THINK_PROF_AT_SHR() macro used to add value to inline delay prof table at
 *                     runtime changes and call calc_a_and_b_value and RTC_PAGE_THINK_TIME_LOG.
 * Input Parameter
 *      ptt_ptr      : pointer to inline delay table in shared memory
 *      grp_idx      : group index
 *      pg_idx       : page index
 *      change_msg   : message buffer
 * Output Parameter  : Set delay prof value in shared memory
 * Return            : None
 ************************************************************************************************/

#define ADD_INLINE_DELAY_PROF_AT_SHR(ptt_ptr, grp_idx, pg_idx, change_msg)  \
{  \
  ptt_ptr->mode = delay_mode; \
  ptt_ptr->avg_time = avg_time; \
  ptt_ptr->median_time = median_time;\
  ptt_ptr->var_time = var_time; \
  ptt_ptr->additional_delay_mode1 = additional_delay_mode1; \
  ptt_ptr->min_limit_time = min_limit_time; \
  ptt_ptr->max_limit_time = max_limit_time; \
  calc_a_and_b_value(ptt_ptr); \
}
 

/***********************************************************************************************
 * Description       : ADD_THINK_PROF_AT_SHR() macro used to add value to inline delay prof table at
 *                     runtime changes and call calc_a_and_b_value and RTC_PAGE_THINK_TIME_LOG.
 * Input Parameter
 *      ptt_ptr      : pointer to inline delay table in shared memory
 *      grp_idx      : group index
 *      pg_idx       : page index
 *      change_msg   : message buffer
 * Output Parameter  : Set inline delay prof value in shared memory
 * Return            : None
 ************************************************************************************************/
    
#define ADD_THINK_PROF_AT_SHR(ptt_ptr, grp_idx, pg_idx, change_msg)\
{  \
  ptt_ptr->mode = delay_mode; \
  ptt_ptr->avg_time = avg_time; \
  ptt_ptr->median_time = median_time;\
  ptt_ptr->var_time = var_time; \
  ptt_ptr->additional_delay_mode1 = additional_delay_mode1; \
  ptt_ptr->min_limit_time = min_limit_time; \
  ptt_ptr->max_limit_time = max_limit_time; \
  calc_a_and_b_value(ptt_ptr); \
}
    


/**********************************************************************************************
 * Description       : inline_delay_prof_to_str_shm() method used to print data for particular delay mode
 *                     in shared memory.                                 
 * Input Parameter
 *      grp_id       : group index
 *      pg_idx       : page index
 *   inline_delay_ptr  : pointer to shared memory
 * Output Parameter  : None
 * Return            : returns information buffer
 * Notes             : For grp_id = -1 and pg_idx = -1 here group and page value is ALL
 ************************************************************************************************/

static char *inline_delay_prof_to_str_shm(int grp_id, int pg_idx, InlineDelayTableEntry_Shr *inline_delay_ptr, char *buf)
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
  //Run time changes applied to Page Inline Delay Time for scenario group 'G1' and page 'page1'. New setting is 'no delay time'
  //New setting is 'Internet Random inline delay time with median of 1.5 seconds'
  if(inline_delay_ptr->mode == INLINE_DELAY_MODE_NO_DELAY)
    sprintf(buf, "Run time changes applied to Page Inline Delay Time for scenario group '%s' and page '%s'.New setting is No inline delay time.", grp_name, page_name);

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_INTERNET_RANDOM)
    sprintf(buf, "Run time changes applied to Page Inline Delay Time for scenario group '%s'and page '%s'. New setting is Random (Internet type distribution) inline delay time with median of %.3f seconds and constant delay of %.3f seconds. min_limit_time = %.3f max_limit_time = %.3f", grp_name, page_name, (double)inline_delay_ptr->median_time/1000.0, (double)inline_delay_ptr->additional_delay_mode1/1000.0, (double)inline_delay_ptr->min_limit_time/1000.0, (double)inline_delay_ptr->max_limit_time/1000.0);

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_CONSTANT)
    sprintf(buf, "Run time changes applied to Page Inline Delay Time for scenario group '%s'and page '%s'. New setting is Constant inline delay time of %.3f seconds.", grp_name, page_name, (double)inline_delay_ptr->avg_time/1000.0);

  else if(inline_delay_ptr->mode == INLINE_DELAY_MODE_CUSTOM_DELAY)
    sprintf(buf, "Run time changes applied to Page Inline Delay Time for scenario group '%s'and page '%s'. New setting is Custom inline delay time. Callback Method is %s", grp_name, page_name, inline_delay_ptr->custom_delay_callback);

  else /* UNIFORM_RANDOM */
    sprintf(buf, "Run time changes applied to Page Inline Delay Time for scenario group '%s'and page '%s'. New setting is Random (Uniform distribution) inline delay time from %.3f to %.3f seconds.", grp_name, page_name, (double)inline_delay_ptr->avg_time/1000.0, (double)inline_delay_ptr->median_time/1000.0);

  return(buf);
}



/********************************************************************************************************
 * Description       : inline_delay_not_runtime() method used when runtime flag is disable, here we 
 *                     check for all possible cases. For all groups and pages, for particular group and 
 *                     all pages or particular pages. Method called from kw_set_g_page_inline_delay_time().
 * Input Parameters
 *         group     : pointer to group name.
 *         page name : pointer to page name.
 *         avg_time  : average inline delay time.
 *       median_time : median inline delay time.
 *        delay_mode : page inline delay mode
 *        var_time   : variable time 
 *      change_msg   : error message
 * Output Parameters : Set fetch option for auto fetch embedded keyword.
 * Return            : Return 0 for success case
 *********************************************************************************************************/

static int inline_delay_not_runtime(char *buf, char *sg_name, char *name, int avg_time, int median_time, int delay_mode, int var_time, char *change_msg, int additional_delay_mode1, int min_limit_time, int max_limit_time, char *custom_delay_callback)
{
  //char session_name[MAX_DATA_LINE_LENGTH];
  int session_idx, page_id, rnum;
  IW_UNUSED(char inline_delay_buf[1024]);
  IW_UNUSED(inline_delay_buf[0] = '\0');

  NSDL2_PARSING(NULL, NULL, "Method called.");
  // Case 1 - where both group and page are ALL.
  // These need to be set at index 0
  //TODO: make a macro function to fill inlineDelayTable entry
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") == 0))
  {
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].mode = delay_mode;
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].avg_time = avg_time;
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].median_time = median_time;
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].var_time = var_time;
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].additional_delay_mode1 = additional_delay_mode1;
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].min_limit_time = min_limit_time;
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].max_limit_time = max_limit_time;
    inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].custom_delay_callback = -1;
    if(custom_delay_callback[0])
      inlineDelayTable[GLOBAL_INLINE_DELAY_IDX].custom_delay_callback = copy_into_big_buf(custom_delay_callback, 0);
    NSDL3_PARSING(NULL, NULL, "%s", inline_delay_prof_to_str_non_shm(sg_name, name, &(inlineDelayTable[GLOBAL_INLINE_DELAY_IDX]), inline_delay_buf));
    return 0;
  }

  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") != 0)) {
    NS_KW_PARSING_ERR(buf, 0, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011012, name, "");
  }
  
  // Case 3 - Invalid group name
  // for particular group
  int grp_idx;
  if ((grp_idx = find_sg_idx(sg_name)) == -1) //invalid group
  {
    NSTL1(NULL, NULL, "Warning: For Keyword G_INLINE_DELAY, inline delay time can not be applied for unknown group '%s'. Group (%s) ignored.\n", sg_name, sg_name);
    NS_DUMP_WARNING("Inline delay time can not applied on page '%s' bacause page name is unknown for group '%s'.", name, sg_name);
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
      if (runProfTable[grp_idx].inline_delay_table[i] == -1)
      {
        create_inline_delay_table_entry(&rnum);
        inlineDelayTable[rnum].mode = delay_mode;
        inlineDelayTable[rnum].avg_time = avg_time;
        inlineDelayTable[rnum].median_time = median_time;
        inlineDelayTable[rnum].var_time = var_time;
        inlineDelayTable[rnum].additional_delay_mode1 = additional_delay_mode1;
        inlineDelayTable[rnum].min_limit_time = min_limit_time;
        inlineDelayTable[rnum].max_limit_time = max_limit_time;
        inlineDelayTable[rnum].custom_delay_callback = -1;
        if(custom_delay_callback[0])
          inlineDelayTable[rnum].custom_delay_callback = copy_into_big_buf(custom_delay_callback, 0);
        NSDL3_PARSING(NULL, NULL, "%s", inline_delay_prof_to_str_non_shm(sg_name, name, &(inlineDelayTable[rnum]), inline_delay_buf));
        /*Converting session name and page name to page id */
  
        runProfTable[grp_idx].inline_delay_table[i] = rnum;
        NSDL3_PARSING(NULL, NULL, "ALL runProfTable[%d].inline_delay_table[%d] = %d\n", 
                      grp_idx, i, runProfTable[grp_idx].inline_delay_table[i]);
      } else
        NSDL3_PARSING(NULL, NULL, "ELSE runProfTable[%d].inline_delay_table[%d] = %d\n", 
                      grp_idx, i, runProfTable[grp_idx].inline_delay_table[i]);
    }
    return 0;
  }
 
  // Case 5 - Group and particular page (e.g. G1 Page1)
  // Case 5a - Group and particular page (e.g. G1 Page1) - Invalid page
  if ((page_id = find_page_idx(name, session_idx)) == -1) // invalid page print error message and exit
  {
    char *sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
    NS_KW_PARSING_ERR(buf, 0, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011013, name, sg_name, sess_name, "");
  }
  create_inline_delay_table_entry(&rnum);

  inlineDelayTable[rnum].mode = delay_mode;
  inlineDelayTable[rnum].avg_time = avg_time;
  inlineDelayTable[rnum].median_time = median_time;
  inlineDelayTable[rnum].var_time = var_time;
  inlineDelayTable[rnum].additional_delay_mode1 = additional_delay_mode1;
  inlineDelayTable[rnum].min_limit_time = min_limit_time;
  inlineDelayTable[rnum].max_limit_time = max_limit_time;
  inlineDelayTable[rnum].custom_delay_callback = -1;
  if(custom_delay_callback[0])
    inlineDelayTable[rnum].custom_delay_callback = copy_into_big_buf(custom_delay_callback, 0);
  NSDL3_PARSING(NULL, NULL, "%s", inline_delay_prof_to_str_non_shm(sg_name, name, &(inlineDelayTable[rnum]), inline_delay_buf));
  runProfTable[grp_idx].inline_delay_table[page_id - gSessionTable[session_idx].first_page] = rnum;

  NSDL3_PARSING(NULL, NULL, "runProfTable[%d].inline_delay_table[page_id(%d) - gSessionTable[%d].first_page(%d)] = %d\n",
                grp_idx, page_id, session_idx, gSessionTable[session_idx].first_page,
                runProfTable[grp_idx].inline_delay_table[page_id - gSessionTable[session_idx].first_page]);

  NSDL3_PARSING(NULL, NULL, "inlineDelayTable[rnum].mode = [%d], inlineDelayTable[rnum].avg_time = [%d], "
                            "inlineDelayTable[rnum].median_time = [%d], inlineDelayTable[rnum].var_time = [%d], "
                            "inlineDelayTable[rnum].additional_delay_mode1 = [%d], inlineDelayTable[rnum].min_limit_time = [%d], "
                            "inlineDelayTable[rnum].max_limit_time = [%d], inlineDelayTable[rnum].custom_delay_callback = [%s]", 
                            inlineDelayTable[rnum].mode, inlineDelayTable[rnum].avg_time,  inlineDelayTable[rnum].median_time, 
                            inlineDelayTable[rnum].var_time, inlineDelayTable[rnum].additional_delay_mode1, 
                            inlineDelayTable[rnum].min_limit_time, inlineDelayTable[rnum].max_limit_time, 
                            RETRIEVE_BUFFER_DATA(inlineDelayTable[rnum].custom_delay_callback));

 return 0;                   
}



/********************************************************************************************************
 * Description       : inline_delay_runtime() method used when runtime flag is enabled, here we
 *                     check for all possible cases. For all groups and pages, for particular group and
 *                     all pages or particular pages. Method called from kw_set_g_inline_delay_time().
 * Input Parameters
 *         group     : pointer to group name.
 *         page name : pointer to page name.
 *         avg_time  : average inline delay time.
 *       median_time : median inline delay time.
 *        delay_mode : inline delay mode.
 *          var_time : variable time.
 *         change_msg   : error message.
 * Output Parameters : Update shared memory.
 * Return            : Return 0 for success case.
 **********************************************************************************************************/

static int inline_delay_runtime(char *kw_buf, char *sg_name, char *name, int avg_time, int median_time, int delay_mode, int var_time, char *change_msg, int additional_delay_mode1, int min_limit_time, int max_limit_time, char *custom_delay_callback)
{
  NSDL2_PARSING(NULL, NULL, "Method called.");
  int page_id;
  int i, j;
  #ifdef NS_DEBUG_ON 
  char buf[4096];
  #endif
   
  InlineDelayTableEntry_Shr *ptt_ptr;
  //printf("Initializing inline delay settings. It may take time...\n");  
  NSTL1(NULL, NULL, "Initializing inline delay settings. It may take time...");
  // called for Runtime change
  // for shared memory table
  // Case 1 - for ALL group and page
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") == 0)) {
    for (i = 0; i < total_runprof_entries; i++) {
      for (j = 0; j < runprof_table_shr_mem[i].num_pages; j++) {
        ptt_ptr = (InlineDelayTableEntry_Shr *)runprof_table_shr_mem[i].inline_delay_table[j];
        //TODO: how custom_delay_callback will be copied in runtime ?
        ADD_THINK_PROF_AT_SHR(ptt_ptr, i, j, change_msg); 
        NSDL3_PARSING(NULL, NULL, "%s", inline_delay_prof_data(i, j, ptt_ptr, buf));
      }
    }
    inline_delay_prof_to_str_shm(-1, -1, ptt_ptr, change_msg);
    //printf("Initializations of inline delay settings completed.\n");
    NSTL1(NULL, NULL, "Initialization of inline delay settings completed.");
    return 0;
  } 
  
  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(name, "ALL") != 0)) {
    NS_KW_PARSING_ERR(kw_buf, 1, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011012, name, "");
  }
  
  // Case 3 - Invalid group name
  // for particular group
  int grp_idx;
  if ((grp_idx = find_sg_idx_shr(sg_name)) == -1) {
    NS_KW_PARSING_ERR(kw_buf, 1, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011014, sg_name, "");
  }  
  
  // Case 4 - Group and ALL pages
  if (strcasecmp(name, "ALL") == 0) {
    for (i = 0; i < runprof_table_shr_mem[grp_idx].num_pages; i++) {
      ptt_ptr = (InlineDelayTableEntry_Shr *)runprof_table_shr_mem[grp_idx].inline_delay_table[i];
      ADD_THINK_PROF_AT_SHR(ptt_ptr, grp_idx, i, change_msg);
      NSDL3_PARSING(NULL, NULL, "%s", inline_delay_prof_data(grp_idx, i, ptt_ptr, buf));
    }
    inline_delay_prof_to_str_shm(grp_idx, -1, ptt_ptr, change_msg);
    //printf("Initializations of inline delay time settings completed.\n");
    NSTL1(NULL, NULL, "Initializing of inline delay time settings completed.");
    return 0;
  } 
  
  // Case 5 - Group and particular page (e.g. G1 Page1)
  SessTableEntry_Shr* sess_ptr = runprof_table_shr_mem[grp_idx].sess_ptr;
  // Case 5a - Group and particular page (e.g. G1 Page1) - For invalid page, print warning and return without applying changes
  if ((page_id = find_page_idx_shr(name, sess_ptr)) == -1)
  {
    NS_KW_PARSING_ERR(kw_buf, 1, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011013, name, sg_name, sess_ptr->sess_name, ""); 
  }
  ptt_ptr = (InlineDelayTableEntry_Shr *)runprof_table_shr_mem[grp_idx].inline_delay_table[page_id];
  ADD_INLINE_DELAY_PROF_AT_SHR(ptt_ptr, grp_idx, page_id, change_msg);
  inline_delay_prof_to_str_shm(grp_idx, page_id, ptt_ptr, change_msg);
  NSDL3_PARSING(NULL, NULL, "%s", inline_delay_prof_data(grp_idx, page_id, ptt_ptr, buf));
  //printf("Initializations of inline delay time settings completed.\n");
  NSTL1(NULL, NULL, "Initializing of inline delay time settings completed.");
  return 0;
}

/**********************************************************************************************
 * Description       : delay_data() method used to print data for particular delay
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
static char *delay_data(int grp_id, int pg_idx, InlineDelayTableEntry_Shr *delay_ptr, char *buf)
{
  char grp_name[128] = "ALL";
  char page_name[128] = "ALL";

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
  if(delay_ptr->mode == INLINE_DELAY_MODE_NO_DELAY)
    sprintf(buf, "Group = %s, Page = %s, DelayMode = %s", grp_name, page_name, delay_mode_to_str(delay_ptr->mode));

  else if(delay_ptr->mode == INLINE_DELAY_MODE_INTERNET_RANDOM)
    sprintf(buf, "Group = %s, Page = %s, DelayMode = %s, Median = %.3f sec, ConstantTimeMode1 = %.3f, min_limit_time = %.3f,"
            "max_limit_time = %.3f", grp_name, page_name, delay_mode_to_str(delay_ptr->mode), (double)delay_ptr->median_time/1000.0,
            (double)delay_ptr->additional_delay_mode1/1000.0, (double)delay_ptr->min_limit_time/1000.0,
            (double)delay_ptr->max_limit_time/1000.0);

  else if(delay_ptr->mode == INLINE_DELAY_MODE_CONSTANT)
    sprintf(buf, "Group = %s, Page = %s, DelayMode = %s, Constant = %.3f sec", grp_name, page_name, delay_mode_to_str(delay_ptr->mode),
            (double)delay_ptr->avg_time/1000.0);
  
  else if(delay_ptr->mode == INLINE_DELAY_MODE_CUSTOM_DELAY)
    sprintf(buf, "Group = %s, Page = %s, DelayMode = %s, Callback Method = %s", grp_name, page_name, delay_mode_to_str(delay_ptr->mode),
            delay_ptr->custom_delay_callback);

  else /* UNIFORM_RANDOM */
    sprintf(buf, "Group = %s, Page = %s, PageThinkMode = %s, Min = %.3f sec, Max = %.3f sec", grp_name, page_name, delay_mode_to_str(delay_ptr->mode), (double)delay_ptr->avg_time/1000.0, (double)delay_ptr->median_time/1000.0);

  return(buf);
}
#endif



/***************************************************************************************************
 * Description       : log_delay_time_for_debug()this method will log delay time in a data file
 *                     Called from ns_inline_delay.c. 
 * Input Parameter
 *             vptr  : pointer to VUser struct
 *        delay_ptr  : pointer to InlineDelayTableEntry_Shr 
 *      return_time  : return time value
 *              now  : current time
 * Output Parameter  : none
 * Return            : none
 ***************************************************************************************************/

#ifdef NS_DEBUG_ON
void log_delay_time_for_debug(VUser *vptr, InlineDelayTableEntry_Shr *delay_ptr, int return_time, u_ns_ts_t now)
{
  char file_name[4096];
  char delay_info_buf[4096]; // use to fill with delay time detail for debug log

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC3) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_SCHEDULE)))
  return;
  sprintf(file_name, "%s/logs/TR%d/delay_time.dat", g_ns_wdir, testidx);
  // Format of file
  // TimeStamp|Group|Page|DelayMode|ReturnTime(ms)
  // 00:00:10|G1|Page1|Mode=Constant Constant=2000|0|0|0|1123|1.123

  ns_save_data_ex(file_name, NS_APPEND_FILE, "%s|%s|%s|%s|%d", get_relative_time(), runprof_table_shr_mem[vptr->group_num].scen_group_name, 
                  vptr->cur_page->page_name, delay_data(vptr->group_num, vptr->cur_page->page_number, delay_ptr, delay_info_buf), return_time);
}
#endif

inline void calculate_inline_delay(VUser *vptr, u_ns_ts_t *return_time, u_ns_ts_t now){

  InlineDelayTableEntry_Shr *delay_ptr;

  NSDL2_SCHEDULE(vptr, NULL, "Method called");
  // If there is no page in the script, then cur_page will be NULL
  if(vptr->cur_page == NULL)
  {
    // In this case, page delay time will be what is passed in the API in case of C Type
    NSDL2_SCHEDULE(vptr, NULL, "cur_page is NULL. Script may be without any page");
    // return; // Do not to use return as it will not be inline
  }
  else
  {
    delay_ptr = (InlineDelayTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].inline_delay_table[vptr->cur_page->page_number];
    /*NSDL4_SCHEDULE(vptr, NULL, "delay ptr for grp = %d, page number = %d, mode = %d, avg_time = %d, var_time = %d, "
                               "rand_gen_idx = %d, a = %u, b = %u, ConstantTimeMode1 = %d, min_limit_time = %d, max_limit_time = %d, "
                               "custom_delay_callback method = %s ",
                                vptr->group_num, vptr->cur_page->page_number, delay_ptr->mode, delay_ptr->avg_time, delay_ptr->median_time,
                                delay_ptr->var_time, delay_ptr->a, delay_ptr->b, delay_ptr->additional_delay_mode1, 
                                delay_ptr->min_limit_time, delay_ptr->max_limit_time,
                                delay_ptr->custom_delay_callback);
    */                            //RETRIEVE_SHARED_BUFFER_DATA(delay_ptr->custom_delay_callback));
    if (delay_ptr)
    {
      if (delay_ptr->mode)
      {
        switch (delay_ptr->mode)
        {
          case INLINE_DELAY_MODE_INTERNET_RANDOM:
          {
            NSDL2_SCHEDULE(vptr, NULL, "Passing a and b values for Delay_Time Mode = 1, a = %f, b = %f, additional_delay_mode1 = [%d] min_limit_time = %d max_limit_time = %d",  delay_ptr->a, delay_ptr->b, delay_ptr->additional_delay_mode1, delay_ptr->min_limit_time ,delay_ptr->max_limit_time);
            *return_time = gsl_ran_weibull(weib_rangen, delay_ptr->a, delay_ptr->b) + delay_ptr->additional_delay_mode1;
            NSDL3_TESTCASE(vptr, NULL, "delay for Delay_Time Mode = 1, return_time = %llu" , *return_time);
            if(*return_time < delay_ptr->min_limit_time) 
              *return_time = delay_ptr->min_limit_time;

            if(*return_time > delay_ptr->max_limit_time)
              *return_time = delay_ptr->max_limit_time;
            NSDL2_SCHEDULE(vptr, NULL, "delay for Delay_Time Mode = 1, return_time = %llu" , *return_time);
            break;
          }
          case INLINE_DELAY_MODE_CONSTANT:
          {
            *return_time = delay_ptr->avg_time;
            break;
          }
          case INLINE_DELAY_MODE_UNIFORM_RANDOM:
          {
            int range = delay_ptr->b - delay_ptr->a;
            *return_time = delay_ptr->a + ns_get_random_max(gen_handle, range);
            break;
          }
          case INLINE_DELAY_MODE_CUSTOM_DELAY:
          {
            //custom delay function returns delay in seconds, so we need to convert it to milliseconds
            if(delay_ptr->custom_delay_func_ptr) {
              double val = delay_ptr->custom_delay_func_ptr();
              NSDL3_SCHEDULE(vptr, NULL, "In case INLINE_DELAY_MODE_CUSTOM_DELAY: val = [%f],",val);
              if(val < 0) {
                 NSEL_CRI(vptr, NULL, ERROR_ID, ERROR_ATTR, "Got negative value of 'custom_delay_func_ptr', setting delay = 0");
                 NSDL2_SCHEDULE(vptr, NULL, "Got negative value of 'custom_delay_func_ptr', setting delay = 0");
                 val = 0;
              }
              *return_time = val * 1000;
              NSDL3_SCHEDULE(vptr, NULL, "In case INLINE_DELAY_MODE_CUSTOM_DELAY: return_time = [%llu],"
                                         "delay_ptr->custom_delay_func_ptr = [%p]", *return_time, delay_ptr->custom_delay_func_ptr);
              NSDL3_SCHEDULE(vptr, NULL, "In case INLINE_DELAY_MODE_CUSTOM_DELAY: return_time = [%llu],"
                                         "delay_ptr->custom_delay_func_ptr = [%p]", *return_time, delay_ptr->custom_delay_func_ptr);
            } else {
                NSDL2_SCHEDULE(vptr, NULL, "Error: Unable to find Custom Delay Callback method [%s], thus exiting.\n", 
                                            delay_ptr->custom_delay_callback);
                NS_EXIT(-1, "Error: Unable to find Custom Delay Callback method [%s], thus exiting.", delay_ptr->custom_delay_callback);
            } 
            break;
          }
          default:
          {
            NSDL2_TESTCASE(vptr, NULL, "Invalid delay mode for inline urls hence assigning default value");
            //fprintf(stderr, "Warning: Invalid delay mode for inline urls hence assigning default value");
            NSTL1(NULL, NULL, "Warning: Invalid delay mode for inline urls hence assigning default value");
            *return_time = 0;
          }
        } // End switch
      }
#ifdef NS_DEBUG_ON
      log_delay_time_for_debug(vptr, delay_ptr, *return_time, now);
#endif
    } // End of if delay_ptr
  }
}


/*********************************************************************************************************
 * Description        : kw_set_g_inline_delay() method used to parse G_INLINE_DELAY keyword,
 *                      parsing keyword for non runtime and runtime changes. This method is called from 
 *                      parse_group_keywords() in ns_parse_scen_conf.c.
 * Format             : G_INLINE_DELAY <group_name> <page_name> <delay_mode value1 value2>
 * Input Parameter
 *           buf      : Pointer in the received buffer for the keyword.
 *           pass     : Receive pass value.
 *        change_msg  : change_msg buffer prints error message if receiving error and at runtime buffer 
 *                      receives runtime change message.
 *      runtime_flag  : Receives runtime flag.
 * Output Parameter   : Set avg_time, median_time, delay_mode, var_time.
 * Return             : Return 0 for sucess and non zero value for failure
 *************************************************************************************************************/
// Make sure size of err_msg buffer passed has enough space which is filled by usage message

int  kw_set_g_inline_delay(char *buf, char *change_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH], value1[100], value2[100], chk_delay_mode[100], value3[100], value4[100];
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  int num, delay_mode, min_time, avg_time, median_time, max_time, var_time, ret, additional_delay_mode1, min_limit_time, max_limit_time;

  NSDL1_PARSING(NULL, NULL, "Method called.");

  avg_time = median_time = var_time = additional_delay_mode1 = min_limit_time =  0;
  max_limit_time = 0x7FFFFFFF;

  char *constant_value_str = value1;
  char *min_value_str = value1;
  char *avg_value_str = value1;
  char *max_value_str = value2;
  char *min_value_str_mode1 = value3;
  char *max_value_str_mode1 = value4;
  char *custom_delay_callback = value1;

  // Must make empty
  chk_delay_mode[0] = 0; // default value is 0
  value1[0] = 0;
  value2[0] = 0;
  value3[0] = 0;
  value4[0] = 0;
  num = sscanf(buf, "%s %s %s %s %s %s %s %s %s", keyword, sg_name, name, chk_delay_mode, value1, value2, value3, value4, tmp);

  if(num < 4 || num > 9) { //Check for extra arguments.
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(chk_delay_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
  }
  delay_mode = atoi(chk_delay_mode);

  //validation for different delay modes
  //INLINE_DELAY_MODE_UNIFORM_RANDOM
  if (delay_mode == INLINE_DELAY_MODE_UNIFORM_RANDOM) {
    if (num != 6)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_1);

    min_time = atoi(min_value_str);
    if(min_time < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_8);

    if(ns_is_numeric(min_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
    }

    max_time = atoi(max_value_str);
    if(max_time < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_8);

    if(ns_is_numeric(max_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
    }

    if (max_time <= min_time)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_5);

    avg_time = min_time;
    median_time = max_time;

  } else if (delay_mode == INLINE_DELAY_MODE_INTERNET_RANDOM) {  // INLINE_DELAY_MODE_INTERNET_RANDOM
    if (num < 5 || num > 9) // check for extra arguments
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_1);

    if(ns_is_numeric(avg_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
    }
    median_time = atoi(avg_value_str);
    
    if (num > 5) { 
      if(ns_is_numeric(max_value_str) == 0) {
        NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
      }
       additional_delay_mode1 = atoi(max_value_str);
     
      if(num > 6 ) { 
      if(ns_is_numeric(min_value_str_mode1) == 0) {
        NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
      }
      min_limit_time = atoi(min_value_str_mode1);
    
      if(ns_is_numeric(max_value_str_mode1) == 0) {
        NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
      }
      max_limit_time = atoi(max_value_str_mode1);
    
      if (min_limit_time > median_time || max_limit_time < 4*median_time)
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011015, "");
      }
     }
    }

    NSDL2_SCHEDULE(NULL, NULL, "median_time = %d , additional_delay_mode1 = %d , min_limit_time = %d, max_limit_time = %d", median_time, additional_delay_mode1, min_limit_time, max_limit_time);
    
    if(median_time <= 0 ){
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_9);
    }

    //median_time = avg_time;/* The user is always inputting the first time arguement as the median time for mode 1, others are not provided by user */
    avg_time = -1;
    var_time = -1;
  } else if (delay_mode == INLINE_DELAY_MODE_CONSTANT) {  // INLINE_DELAY_MODE_CONSTANT
    if (num != 5)
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_1);

    avg_time = atoi(constant_value_str);
    if (avg_time < 0 ){
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_8);
    }

    if (ns_is_numeric(constant_value_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_2);
    }

  } else if (delay_mode == INLINE_DELAY_MODE_CUSTOM_DELAY) {  // INLINE_DELAY_MODE_CUSTOM_DELAY
      if (custom_delay_callback[0] == 0) {
        NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_1);
      } else if(val_fname(custom_delay_callback, 64)) { 
        NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, ptt_cll_back_msg);
      }
    NSDL2_SCHEDULE(NULL, NULL, "Delay Mode = [%d] , Callback method = [%s]", INLINE_DELAY_MODE_CUSTOM_DELAY, custom_delay_callback);
  } else if (delay_mode != INLINE_DELAY_MODE_NO_DELAY) { //INLINE_DELAY_MODE_NO_DELAY
      NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, INLINE_DELAY_USAGE, CAV_ERR_1011011, CAV_ERR_MSG_3);
    }

  if (!runtime_flag)
  {
    ret = inline_delay_not_runtime(buf, sg_name, name, avg_time, median_time, delay_mode, var_time, change_msg, additional_delay_mode1, min_limit_time, max_limit_time, custom_delay_callback);
  }
  else
  {
    ret = inline_delay_runtime(buf, sg_name, name, avg_time, median_time, delay_mode, var_time, change_msg, additional_delay_mode1, min_limit_time, max_limit_time, custom_delay_callback);
    return(ret);
  }

  return 0;
}

#ifdef NS_DEBUG_ON
static char * get_format_time(char *format, time_t time)
{
  char *ptr = NULL;
  static char output[256];
  struct tm *tm, tm_struct;
  time = time/1000;
  tm = nslb_localtime(&time, &tm_struct, 1);
  strftime(output, 256, format, tm);
  ptr = output;
  return ptr;
}
#endif

/* This Method set schedule time for inline urls, called form http_close_connection
* schedule_time is used to apply delay on inline urls. In this method we loop all the hosts and urls to that hos for a page and set its 
* schedule time by adding delay in current time stamp. We use this time stamp in renew_connection to set the timer for delay
*/
inline void set_inline_schedule_time(VUser *vptr, u_ns_ts_t now){
  HostSvrEntry* host_head = vptr->head_hlist;
  NSDL2_CONN(vptr, NULL, "Method Called. now = %lu", now);
  u_ns_ts_t delay = 0; 
  u_ns_ts_t cum_delay = 0;
  char first = 0;
  int i;
  //char time_buf[512];

  while(host_head){
    NSDL2_SCHEDULE(vptr, NULL, "Setting schedule time for host_head = %p, host_head->hurl_left = %d", host_head, host_head->hurl_left);
    for(i = 0; i < host_head->hurl_left; i++)  {
      if(!first)  {
        first = 1;
      } else  
        calculate_inline_delay(vptr, &delay, now);
      cum_delay += delay;
      host_head->cur_url[i].schedule_time = now + cum_delay;

      NSDL2_SCHEDULE(vptr, NULL, "Schedule timestamp for [%dth] url is set to [%lu]. schedule time = [%s], Delay added = [%llu]", 
                     i, host_head->cur_url[i].schedule_time, get_format_time("%H:%M:%S", host_head->cur_url[i].schedule_time), delay);
      if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) 
        set_page_based_counter_for_inline_delay_time(vptr, delay);
      NSDL2_CONN(vptr, NULL, "Inline delay = [%d], cum_delay = [%d]", delay, cum_delay);
    }
    host_head = (HostSvrEntry*)host_head->next_hlist;
  }
}

inline void free_repeat_urls(VUser *vptr){
  int i;
  HostSvrEntry *hptr;

  NSDL2_SCHEDULE(vptr, NULL, "Method Called");
  for (i = 0, hptr = vptr->hptr; i <= g_cur_server; i++, hptr++) {
    if(hptr->cur_url_head)
      FREE_AND_MAKE_NULL(hptr->cur_url_head, "hptr->cur_url_head", i);
  }
}

/*Function to calculate value of min_con_reuse_delay
 *as per the Keyword G_INLINE_MIN_CON_REUSE_DELAY
*/ 
inline void calculate_con_reuse_delay(VUser *vptr, int *min_con_reuse_delay)
{
  int max, min = 0;

  NSDL2_SCHEDULE(vptr, NULL, "Method Called");
  max = runprof_table_shr_mem[vptr->group_num].gset.max_con_reuse_delay;

  if(max == 0) // No delay
    *min_con_reuse_delay = 0;
  else 
  {
    min = runprof_table_shr_mem[vptr->group_num].gset.min_con_reuse_delay;
    if(min == max) // Constant delay
      *min_con_reuse_delay = min;
    else // Random delay
      *min_con_reuse_delay = ns_get_random_number_int(min, max);
  }
  NSDL2_SCHEDULE(vptr, NULL, "min = [%d], max = [%d], min_con_reuse_delay = [%d]", min, max, *min_con_reuse_delay);
}


int kw_set_g_inline_min_con_reuse_delay(char *buf, char *change_msg, int *min_con_delay, int *max_con_delay)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char value1[100], value2[100];
  //char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  int num;

  NSDL1_PARSING(NULL, NULL, "Method called.");

  // Must make empty
  value1[0] = 0;
  value2[0] = 0;
  num = sscanf(buf, "%s %s %s %s", keyword, sg_name, value1, value2);

  if(num < 4 || num > 4) { //Check for extra arguments.
    inline_min_con_reuse_delay_usage("Invalid number of arguments", change_msg);
  }
  if(ns_is_numeric(value1) == 0) {
    inline_min_con_reuse_delay_usage("Minimum value can have only integer value", change_msg);
  }

  *min_con_delay = atoi(value1);

  if(ns_is_numeric(value2) == 0) {
    inline_min_con_reuse_delay_usage("Maximum value can have only integer value", change_msg);
  }

  *max_con_delay = atoi(value2);

  if(*max_con_delay < *min_con_delay){
    inline_min_con_reuse_delay_usage("Maximum value should be greater than minimum", change_msg);
  }

  NSDL1_PARSING(NULL, NULL, "min_con_delay = [%d], max_con_delay = [%d]", *min_con_delay, *max_con_delay);
  return(0);
}
                                                                                                                            
