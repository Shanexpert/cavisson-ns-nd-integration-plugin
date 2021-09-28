
/********************************************************************************
 * File Name            : ns_auto_fetch_parse.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 2011/09/30
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains parsing function for G_AUTO_FETCH_EMBEDDED keyword.
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 *********************************************************************************/


#include "ns_auto_fetch_parse.h"
#include "ns_embd_objects.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_util.h"

/* Description       : Method used to allocate memory to auto fetch table.
 * Input Parameters
 *      row_num      : Pointer to row number.
 * Output Parameters : Set total entries of auto fetch embedded table(total_autofetch_entries).
 * Return            : Return -1 on error in allocating memory to autofetch table else return 1.
 */

static int create_autofetch_table_entry(int *row_num)
{
  NSDL2_PARSING(NULL, NULL, "Method called");
  if (total_autofetch_entries == max_autofetch_entries)
  {
    MY_REALLOC_EX(autofetchTable, (max_autofetch_entries + DELTA_AUTOFETCH_ENTRIES) * sizeof(AutoFetchTableEntry), (max_autofetch_entries * sizeof(AutoFetchTableEntry)), "autofetchTable", -1); // added prev size(maximum autofetch entries * size of AutoFetchTableEntry table)
    if (!autofetchTable)
    {
      fprintf(stderr, "create_autofetch_table_entry(): Error allcating more memory for autofetch entries\n");
      return FAILURE;
    } else max_autofetch_entries += DELTA_AUTOFETCH_ENTRIES;
  }
  *row_num = total_autofetch_entries++;
  return (SUCCESS);
}

/* Description       : Method used to create auto fetch table for default value of keyword.
 * Input Parameters  : None
 * Output Parameters : Set auto fetch table entries.
 * Return            : None
 */

void create_default_auto_fetch_table(void)
{
  int rnum;

  NSDL2_PARSING(NULL, NULL, "Method called");
  if (create_autofetch_table_entry(&rnum) == FAILURE) {
    NS_EXIT(-1, "read_keywords(): Error in creating new auto fetch table entry");
  }

  assert(rnum == GLOBAL_AUTO_FETCH_IDX);

  autofetchTable[rnum].auto_fetch_embedded = 0;
}

/*
 * Description        : Method copy_auto_fetch_to_shr() used to copy auto fetch table in shared memory.
 *                      Add fetch value for total number of pages across all scenario groups.
 *                      This method is called from copy_structs_into_shared_mem()in util.c. 
 * Input Parameter    : None
 * Output Parameter   : Set fetch value for total number of pages.
 * Return             : None                          
*/
void copy_auto_fetch_to_shr(void) 
{
  int i;
  char *ptr = NULL;

  NSDL2_PARSING(NULL, NULL, "Method called. total_autofetch_entries = %d", total_autofetch_entries);

  if (total_autofetch_entries)
  {
    int total_num_pages = 0; // Total number of pages across all scenario groups
    for (i = 0; i < total_runprof_entries; i++)
    {
      total_num_pages += runProfTable[i].num_pages;
      NSDL4_PARSING(NULL, NULL, "total_num_pages = %d", total_num_pages);
    }
    autofetch_table_shr_mem = (AutoFetchTableEntry_Shr*) do_shmget(sizeof(AutoFetchTableEntry_Shr) * total_num_pages, "auto fetch table");
    /* Per session, per page assign auto fetch. */
    int j, k = 0;
    for (i = 0; i < total_runprof_entries; i++) 
    {
      NSDL4_PARSING(NULL, NULL, "runProfTable[%d].num_pages = %d", i, runProfTable[i].num_pages);
      for (j = 0; j < runProfTable[i].num_pages; j++) 
      {
        NSDL4_PARSING(NULL, NULL, "runProfTable[%d].auto_fetch_table[%d] = %d", i, j,  runProfTable[i].auto_fetch_table[j]);
        int idx = runProfTable[i].auto_fetch_table[j] == -1 ? 0 : runProfTable[i].auto_fetch_table[j];
        NSDL4_PARSING(NULL, NULL, "runProfTable[%d].auto_fetch_table[%d] = %d, idx = %d, k = %d, fetch_option = %d",
                    i, j, runProfTable[i].auto_fetch_table[j], idx, k, autofetchTable[idx].auto_fetch_embedded);

        /*Bug Id: 54295 - In case of SockJS we are sending one inline URL always.*/
        if(!(ptr = strstr(RETRIEVE_BUFFER_DATA(gPageTable[j].page_name), "sockJsConnect_")))
          autofetch_table_shr_mem[k].auto_fetch_embedded = autofetchTable[idx].auto_fetch_embedded;

        k++;
      }
    }
  }
}
/*
 * Description       : check_auto_fetch_enable_for group() method checks auto fetch option enabled for any page in a given group.
 *                     Must be called after shared memory is initialized.
 * Example           : If G_AUTO_FETCH_EMBEDDED was enabled for particular group G1 for all pages or particular page.
 * Input Parameter    
 *       grp_id      : Receive group id for which auto fetch option is enabled.
 * Output Parameter  : None
 * Return            : Return 1 for auto fetch enabled and 0 if auto fetch is disabled.
 */

static int check_auto_fetch_enable_for_group(int grp_id)
{
  NSDL2_PARSING(NULL, NULL, "Method called");
  AutoFetchTableEntry_Shr *ptr = NULL; // pointer to shared memory
  int ret, j;
  
  for (j = 0; j < runprof_table_shr_mem[grp_id].num_pages; j++)
  {
    ptr = (AutoFetchTableEntry_Shr *)runprof_table_shr_mem[grp_id].auto_fetch_table[j]; // for a particular page of a group
    if (ptr->auto_fetch_embedded) // auto fetch enabled
    {
      ret = ptr->auto_fetch_embedded; 
      NSDL3_PARSING(NULL, NULL, "Auto fetch value  = %d", ret);
      return(ret); // returns fetch value
    }
  }
  return 0;
}

/*
 * Description	     : check_auto_fetch_enable() method checks auto fetch option enabled for any page of a group.
 *                     Must be called after shared memory is initialized.     
 * Example           : If G_AUTO_FETCH_EMBEDDED was enabled for particular group G1 for all pages or particular page.
 * Input Parameter   : None
 * Output Parameter  : None
 * Return            : Return 1 for auto fetch enabled and 0 if auto fetch is disabled.   
 */

int check_auto_fetch_enable()
{
  int i,ret;
  NSDL2_PARSING(NULL, NULL, "Method called.");
  for (i = 0; i < total_runprof_entries; i++) 
  {
    ret = check_auto_fetch_enable_for_group(i);
    NSDL3_PARSING(NULL, NULL, "Auto fetch value = %d", ret);
    if (ret) return ret; // check for particular page in group 
  }
  return(0);
}

/* 
 * Description      : validate_g_auto_fetch_embedded_keyword() method used to validate different keywords with G_AUTO_FETCH_EMBEDDED.
 *                    If any group/page has auto fetch embedded enabled, then
 *                    For AUTO_COOKIE
 *                       If auto cookie is disable, then give warning and continue
 *                    For AUTO_REDIRECT
 *                       If auto redirect is disable, then give warning and continue
 *                    For G_NO_VALIDATION
 *                       If no validation is enabled, then give error and exit
 *                       If any page of the group for which has auto fetch on and no validation on, then it is error
 *                       For example: If G1 has no validation and G1 P1 has auto fetch enabled, then it is a error
 *                       For example: If G1 has no validation and G1 ALL or all pages has NO auto fetch enabled, then it is OK
 *                    Must be called after shared memory is initialized for ALL keywords
 *                    Must be called after shared memory is initialized (from ns_parent.c)
 * Input Paramenter : None
 * Output Parameter : None
 * Return           : None                  
*/
void  validate_g_auto_fetch_embedded_keyword()
{
  int check,i,ret;
  NSDL2_PARSING(NULL, NULL, "Method called.");

  check = check_auto_fetch_enable();
  if(!check) 
    return;

  //for auto fetch feature enable check auto redirect and auto cookie.  
  NSDL3_PARSING(NULL, NULL, "Auto fetch embedded is enable = %d checking for auto redirect and auto cookie keyword",check);   
  if (!global_settings->g_follow_redirects) //AUTO_REDIRECT should always be after AUTO_FETCH_EMBEDDED
  {
    NSDL3_PARSING(NULL, NULL,"Auto Redirect = %d", global_settings->g_follow_redirects);
    print_core_events((char*)__FUNCTION__, __FILE__, "Warning: Auto fetch embedded is enabled but AUTO_REDIRECT is disabled. Redirection will not be done.");
    NS_DUMP_WARNING("Auto fetch embedded is enabled but AUTO_REDIRECT is disabled. Redirection will not be done.");
  }

  if (global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    NSDL3_PARSING(NULL, NULL,"Auto_Cookie, cookie mode value = %d", global_settings->g_auto_cookie_mode);  
    print_core_events((char*)__FUNCTION__, __FILE__, "Warning: Auto fetch embedded is enabled but Auto cookie is disabled. Cookies will be send as per recorded cookies in the script");
    NS_DUMP_WARNING("Auto fetch embedded is enabled but auto cookie is disabled. Cookies will be send as per recorded cookies in the script.");
  }  

  // No validation
  for (i = 0; i < total_runprof_entries; i++) {
    if (runProfTable[i].gset.no_validation) {
      ret = check_auto_fetch_enable_for_group(i);
      if(ret) { // check for particular page in group
        NSDL2_PARSING(NULL, NULL, "Auto fetch option= %d", ret);
        print_core_events((char*)__FUNCTION__, __FILE__, "Error: Auto fetch embedded is enabled and no validation is also enabled for scenario group %s. No validation cannot be used with auto fetch embedded.", RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
        NS_EXIT(-1, CAV_ERR_1031045, RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
      }
    }
  }
}

/*
 * Description       : auto_fetch_embedded_not_runtime() method used when runtime flag is disable, here we check for all possible cases. For all groups and pages, for particular group and all pages or particular pages. Method called from kw_set_auto_fetch_embedded().
 * Input Parameters
 *         group     : pointer to group name.
 *         page name : pointer to page name.
 * auto_fetch_option : auto fetch option.
 * Output Parameters : Set fetch option for auto fetch embedded keyword.
 * Return            : None
 */

#ifndef CAV_MAIN
char *g_auto_fetch_info = NULL;
int g_auto_fetch_info_total_size = 0;
#else
__thread char *g_auto_fetch_info = NULL;
__thread int g_auto_fetch_info_total_size = 0;
#endif

static int auto_fetch_embedded_not_runtime(char *buf, char *group, char *page_name, int auto_fetch_option, char *err_msg)
{
  char *session_name;
  int session_idx, page_id, rnum;
  int grp_idx; // for particular group

  if(g_auto_fetch_info_total_size != (total_runprof_entries + 1))  // Specific Group
  {
    MY_REALLOC_AND_MEMSET(g_auto_fetch_info, sizeof(char) * total_runprof_entries + 1, g_auto_fetch_info_total_size, "g_auto_fetch_info", -1);
    g_auto_fetch_info_total_size = total_runprof_entries + 1;

    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
    {
      g_auto_fetch_info[grp_idx + 1] = g_auto_fetch_info[0];
    }
  }

  // Case where both group and page are ALL.
  // These need to be set at index 0 
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") == 0)) // for all groups and pages.
  {
    autofetchTable[GLOBAL_AUTO_FETCH_IDX].auto_fetch_embedded = auto_fetch_option; // set fetch option for auto fetch embedded
    g_auto_fetch_info[0] = auto_fetch_option;
    NSDL3_PARSING(NULL, NULL, "For ALL groups and ALL pages - auto_fetch option = %d", autofetchTable[0].auto_fetch_embedded);
    return 0;
  }

  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") != 0)) {
    NS_KW_PARSING_ERR(buf, 0, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011001, page_name, "");
  }
 
  if ((grp_idx = find_sg_idx(group)) == -1) // Invalid group name.
  {
    NSTL1(NULL, NULL, CAV_ERR_1011003 + CAV_ERR_HDR_LEN, group);
    return 0;
  }

  session_idx = runProfTable[grp_idx].sessprof_idx;
  NSDL2_PARSING(NULL, NULL, "Session index = %d", session_idx);
  g_auto_fetch_info[grp_idx + 1] = auto_fetch_option;

  // For all pages
  if (strcasecmp(page_name, "ALL") == 0)
  {
    int i;
    for (i = 0; i < gSessionTable[session_idx].num_pages; i++)
    {
      if (runProfTable[grp_idx].auto_fetch_table[i] == -1)
      {
        if (create_autofetch_table_entry(&rnum) == FAILURE) //allocate memory to auto fetch table
        {
          NS_EXIT(-1, CAV_ERR_1000002);
        }
        autofetchTable[rnum].auto_fetch_embedded = auto_fetch_option; // set fetch option for auto fetch embedded
        NSDL3_PARSING(NULL, NULL,"Fetch option = %d", autofetchTable[rnum].auto_fetch_embedded);                 
        runProfTable[grp_idx].auto_fetch_table[i] = rnum;
        NSDL3_PARSING(NULL, NULL, "ALL runProfTable[%d].auto_fetch_table[%d] = %d\n",
        grp_idx, i, runProfTable[grp_idx].auto_fetch_table[i]);
      } else
          NSDL3_PARSING(NULL, NULL, "ELSE runProfTable[%d].auto_fetch_table[%d] = %d\n",
                                 grp_idx, i, runProfTable[grp_idx].auto_fetch_table[i]);
   }
   return 0;
  }

  // For particular pages
  if ((page_id = find_page_idx(page_name, session_idx)) == -1) // for particular page
  {
    session_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
    NS_KW_PARSING_ERR(buf, 0, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011002, page_name, group, session_name, "");
  }
  if (create_autofetch_table_entry(&rnum) == FAILURE) // allocate memory to auto fetch table
  {
    NS_EXIT(-1, CAV_ERR_1000002);
  }
  autofetchTable[rnum].auto_fetch_embedded = auto_fetch_option; // set fetch option for auto fetch embedded
  NSDL3_PARSING(NULL, NULL,"Fetch option = %d", autofetchTable[rnum].auto_fetch_embedded);
  runProfTable[grp_idx].auto_fetch_table[page_id - gSessionTable[session_idx].first_page] = rnum;
  NSDL3_PARSING(NULL, NULL, "runProfTable[%d].auto_fetch_table[page_id(%d) - gSessionTable[%d].first_page(%d)] = %d\n",
                           grp_idx, page_id, session_idx, gSessionTable[session_idx].first_page,
                           runProfTable[grp_idx].auto_fetch_table[page_id - gSessionTable[session_idx].first_page]);
  return 0;
}

/*
 * Description       : auto_fetch_embedded_runtime() method used when runtime flag is enabled, here we check for all possible cases. For all groups and pages, for particular group and all pages or particular pages. Method called from kw_set_auto_fetch_embedded(). 
 * Input Parameters
 *         group     : pointer to group name.
 *         page name : pointer to page name.
 * auto_fetch_option : auto fetch option.
 *         err_msg   : pointer to err_msg 
 * Output Parameters : Set fetch option for auto fetch embedded keyword in shared memory.
 * Return            : None
 */

static int auto_fetch_embedded_runtime(char *buf, char *group, char *page_name, int auto_fetch_option, char *err_msg)
{
  int page_id;
  int i,j;
  AutoFetchTableEntry_Shr *ptr;

  // called for Runtime change
  // for shared memory table
  // Case 1 - ALL and ALL
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") == 0)) { // for all groups and pages
    for (i = 0; i < total_runprof_entries; i++) {
     for (j = 0; j < runprof_table_shr_mem[i].num_pages; j++) {
        ptr = (AutoFetchTableEntry_Shr *)runprof_table_shr_mem[i].auto_fetch_table[j];
        ptr->auto_fetch_embedded = auto_fetch_option; // set fetch option for auto fetch embedded
        NSDL3_PARSING(NULL, NULL, "Setting auto fetch for run prof %d and page index %d with value auto_fetch_option = %d", i, j, ptr->auto_fetch_embedded);
      }
    }
    return 0;
  } 

  // Case 2 - ALL and page (Not allowed)
  if ((strcasecmp(group, "ALL") == 0) && (strcasecmp(page_name, "ALL") != 0)) { 
    NS_KW_PARSING_ERR(buf, 1, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011001, page_name, "");
  }

  // Case 3 - Invalid group name
  int grp_idx; // for particular group
  if ((grp_idx = find_sg_idx_shr(group)) == -1) { // for invalid group name
    NS_KW_PARSING_ERR(buf, 1, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011003, group, "");
  } 

  // Case 4 - Group and ALL pages 
  if (strcasecmp(page_name, "ALL") == 0) { // for all pages
    for (i = 0; i < runprof_table_shr_mem[grp_idx].num_pages; i++)
    {
      ptr = (AutoFetchTableEntry_Shr *)runprof_table_shr_mem[grp_idx].auto_fetch_table[i];
      ptr->auto_fetch_embedded = auto_fetch_option;
      NSDL3_PARSING(NULL, NULL,"For ALL pages auto_fetch_option = %d", ptr->auto_fetch_embedded);
    }
    return 0;
  } 

  // Case 4 - Group and particular page (e.g. G1 Page1)
  
  // Case 4a - Group and particular page (e.g. G1 Page1) - Invalid page
  SessTableEntry_Shr* sess_ptr = runprof_table_shr_mem[grp_idx].sess_ptr;
  if ((page_id = find_page_idx_shr(page_name, sess_ptr)) == -1) // for particular pages
  {
    NS_KW_PARSING_ERR(buf, 1, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011002, page_name, group, sess_ptr->sess_name, "");
  }

  ptr = (AutoFetchTableEntry_Shr *)runprof_table_shr_mem[grp_idx].auto_fetch_table[page_id];
  ptr->auto_fetch_embedded = auto_fetch_option; // set fetch option for auto fetch embedded
  NSDL3_PARSING(NULL, NULL,"For particular pages auto_fetch_option = %d", ptr->auto_fetch_embedded);
  return 0;
}

/*
 * Description        : kw_set_auto_fetch_embedded() method used to parse G_AUTO_FETCH_EMEDDED keyword, parse keyword for non runtime and runtime changes. 
 *                      This method is called from parse_group_keywords() in ns_parse_scen_conf.c.
 *
 * Format             : G_AUTO_FETCH_EMBEDDED <group_name> <page_name> <fetch_option>
 * Input Parameter
 *           buf      : Pointer in the received buffer for the keyword.
 *           err_msg  : Print error message.
 *      runtime_flag  : Receives runtime flag.
 * Output Parameter   : Set auto_fetch_embedded.
 * Return             : None
 */

// Make sure size of err_msg buffer passed has enough space which is filled by usage message
int kw_set_auto_fetch_embedded(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char group[MAX_DATA_LINE_LENGTH];
  char page_name[MAX_DATA_LINE_LENGTH];
  int num;
  char fetch_option[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  fetch_option[0] =0;
  int num_value;

  NSDL2_PARSING(NULL, NULL, "Method called");

  num = sscanf(buf, "%s %s %s %s %s", keyword, group, page_name, fetch_option, tmp);//Check for extra arguments.
  if (num != 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011004, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(fetch_option) == 0) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011004, CAV_ERR_MSG_2);
  }

  num_value = atoi(fetch_option);
  if ( num_value < 0 || num_value > 1 ) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_FETCH_EMBEDDED_USAGE, CAV_ERR_1011004, CAV_ERR_MSG_3);
  }

  if (!runtime_flag)
  { 
    auto_fetch_embedded_not_runtime(buf, group, page_name, num_value, err_msg);
  }
  else
  {
    int ret = auto_fetch_embedded_runtime(buf, group, page_name, num_value, err_msg);
    return(ret);
  }
 
  return 0;
}

/*
 * Description       : initialize_runprof_auto_fetch_idx() method used to allocate memory to auto fetch table in runproftable and memset -1 as default value, method is called from url.c.  
 * Input Parameters  : None
 * Output Parameters : Set fetch option for auto fetch embedded keyword in auto fetch table.
 * Return            : None
 */

void initialize_runprof_auto_fetch_idx()
{
  int i, num_pages;

  for (i = 0; i < total_runprof_entries; i++) {
    num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;

    MY_MALLOC(runProfTable[i].auto_fetch_table, sizeof(int) * num_pages, "runProfTable[i].auto_fetch_table", i);
    memset(runProfTable[i].auto_fetch_table, -1, sizeof(int) * num_pages);
    runProfTable[i].num_pages = num_pages; // TODO: We need to do this once. This is also being done for page think time
  }
}

/*
 * Description       :free_runprof_auto_fetch_idx() method used to free memory of auto fetch table defined in runproftable, method is called from util.c.
 * Input Parameters  : None
 * Output Parameters : None.
 * Return            : None
 */

void free_runprof_auto_fetch_idx()
{
  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    //num_pages = runProfTable[i].num_pages;
    FREE_AND_MAKE_NULL_EX(runProfTable[i].auto_fetch_table, sizeof(int) * runProfTable[i].num_pages, "runProfTable[i].auto_fetch_table", i);// size of number of pages.
  }

  FREE_AND_MAKE_NULL_EX(g_auto_fetch_info, sizeof(char) * g_auto_fetch_info_total_size, "g_auto_fetch_info", i)
}

inline void set_regex(char *pattern,  regex_t *comp_regex){
  
  int index = 0;     
  char *tmp = pattern; 
  char reg_pattern[1024] = "\0";
  int ret;
  //regex_t * regex = regex_ptr[i];

  NSDL4_HTTP(NULL, NULL, "pattern  = %s", pattern);
  while(*tmp != '\0')
  {
    if(*tmp == '*'){
      strncpy(&reg_pattern[index], ".*", 2);
      index += 2;
    }
    else{
      reg_pattern[index] = *tmp;
      index += 1;
    }
    tmp++;
  }
  reg_pattern[index] = '\0';
  ret = regcomp(comp_regex, reg_pattern, REG_EXTENDED);
  if(ret != 0)
  {
    NS_EXIT(-1, "Unable to compile reguler expression \'%s\'", pattern);
  }
}

void kw_set_inline_filter_patterns_usage(char *keyword, char *err)
{
  sprintf(err, "Error: Invalid value of %s keyword: %s\n", keyword, err);
  fprintf(stderr, "Usage: %s <group_name> <pattern_list>\n", keyword);
  fprintf(stderr, "Group name can be ALL or any group name used in scenario group\n");
  fprintf(stderr, "Pattern list can be any ragular expression seperated by ,\n");
  NS_EXIT(-1, "%s\nUsage: %s <group_name> <pattern_list>", err, keyword);
}


/* This method will parse four keywords
 * G_INLINE_INCLUDE_DOMAIN_PATTERN
 * G_INLINE_INCLUDE_URL_PATTERN
 * G_INLINE_EXCLUDE_DOMAIN_PATTERN
 * G_INLINE_EXCLUDE_URL_PATTERN
*/

int total_regex_entries = 0;
int max_regex_entries = 0;
void create_regex_table_entry(int *row_num){
  NSDL2_PARSING(NULL, NULL, "Method called");
  if (total_regex_entries <= max_regex_entries)
  {
    MY_REALLOC_EX(reg_array_ptr, (max_regex_entries + DELTA_AUTOFETCH_ENTRIES) * sizeof(RegArray), (max_regex_entries * sizeof(RegArray)), "RegArray", -1); // added prev size(maximum autofetch entries * size of AutoFetchTableEntry table)
    if(!reg_array_ptr)
    {
      fprintf(stderr, "create_regex_table_entry(): Error allcating more memory for regex entries\n");
      return;
    } else max_regex_entries += DELTA_AUTOFETCH_ENTRIES;
  }
  *row_num = total_regex_entries++;
}

void kw_set_inline_filter_patterns(char * buff, GroupSettings *gset, int grp_idx){

NSDL2_PARSING(NULL, NULL, "buffer is [%s]",buff);
  IW_UNUSED(int mode);
  int row_num;
  int start_row = 0;
  char *fields[100];
  char err[1024];
  char buffer[2048];
  int num_tokens;
  int i;
  char *keyword;
  char *list;
  //char *group;
  char *ptr;
  strcpy(buffer,buff);
 
              
  NSDL2_PARSING(NULL, NULL, "Method called, grp_idx = %d", grp_idx);
  NSDL2_PARSING(NULL, NULL, "buffer is [%s]", buffer);

  keyword = buff;

  if((ptr=strchr(buff,' '))!=NULL)
  {
    *ptr='\0';
    ptr++;
  }
  else
  {
    sprintf(err, "Invalid number of arguments  [%s] \n",buffer);
    kw_set_inline_filter_patterns_usage(buffer, err);
  } 
  
  CLEAR_WHITE_SPACE(ptr); // To handle extra spaces after keyword
  
  //group = ptr;

  if((ptr = strchr(ptr,' ')) != NULL)
  {
    *ptr = '\0';
    ptr++;
  }
  else
  {
    sprintf(err, "Invalid number of arguments  [%s] \n", buffer);
    kw_set_inline_filter_patterns_usage(buffer, err);
  }
  list = ptr;
  if((ptr = rindex(list,'\n')) != NULL)
  {
     *ptr='\0';
  }
  /*else
  {
    sprintf(err, "In list %s  Argument given is\n",list);
    kw_set_inline_filter_patterns_usage(list, err);
  }*/
 

  if(grp_idx == -1){
    if(!pattern_table){
       MY_MALLOC(pattern_table, (4 * sizeof(PatternTable)), "PatternTable", -1); // added prev size(maximum autofetch entries * size of AutoFetchTableEntry table)
      memset(pattern_table, 0, 4 * sizeof(PatternTable)); 
      //create_pattern_table_entry(&start_row, all_or_grp);
      gset->pattern_table_start_idx = 0;
      start_row = 0;
    }
  } else {
    start_row = (grp_idx*4);
    gset->pattern_table_start_idx = start_row;
  }

  if(strcmp(keyword, "G_INLINE_INCLUDE_DOMAIN_PATTERN") == 0){
    IW_UNUSED(mode = INCLUDE_DOMAIN);
    row_num = start_row + INCLUDE_DOMAIN;
  } else if(strcmp(keyword, "G_INLINE_INCLUDE_URL_PATTERN") == 0){
    IW_UNUSED(mode = INCLUDE_URL);
    row_num = start_row + INCLUDE_URL;
  } else if(strcmp(keyword, "G_INLINE_EXCLUDE_DOMAIN_PATTERN") == 0){
    IW_UNUSED(mode = EXCLUDE_DOMAIN);
    row_num = start_row + EXCLUDE_DOMAIN;
  } else if(strcmp(keyword, "G_INLINE_EXCLUDE_URL_PATTERN") == 0){
    IW_UNUSED(mode = EXCLUDE_URL);
    row_num = start_row + EXCLUDE_URL;
  }

  NSDL3_PARSING(NULL, NULL, "mode = %d", mode);

  num_tokens = get_tokens(list, fields, ",", 100);
  //MY_MALLOC(pattern_table[row_num].reg_array_ptr, sizeof(RegArray) * num_tokens, "pattern_table[row_num].reg_array_ptr,", row_num);
  int reg_row;
    NSDL2_PARSING(NULL, NULL, "grp_idx  [%d] :",grp_idx); 
  if(grp_idx == -1){
    pattern_table[row_num].reg_start_idx= -1;
    pattern_table[row_num].num_entries = num_tokens;


    int reg_row;
    for( i = 0; i < num_tokens; i++){
      CLEAR_WHITE_SPACE(fields[i]);
      create_regex_table_entry(&reg_row);
      set_regex(fields[i], &(reg_array_ptr[reg_row].comp_regex));
      if(pattern_table[row_num].reg_start_idx == -1)
        pattern_table[row_num].reg_start_idx = reg_row;
     }

     NSDL2_PARSING(NULL, NULL, "row_num = %d, pattern_table[row_num].reg_start_idx = %d, pattern_table[row_num].num_entries = %d", row_num,
                                                           pattern_table[row_num].reg_start_idx, pattern_table[row_num].num_entries);

   } else {
     pattern_table_shr[row_num].reg_start_idx= -1;
     pattern_table_shr[row_num].num_entries = num_tokens;

     for( i = 0; i < num_tokens; i++){
       create_regex_table_entry(&reg_row);
       set_regex(fields[i], &(reg_array_ptr[reg_row].comp_regex));
       if(pattern_table_shr[row_num].reg_start_idx == -1)
         pattern_table_shr[row_num].reg_start_idx = reg_row;
    }
  }
}

void create_pattern_shr_mem(){
  NSDL2_PARSING(NULL, NULL, "Method called");

  int i, j = 0;
  int total_used_pattern_entries;

  total_used_pattern_entries = 4 * total_runprof_entries;

  pattern_table_shr = (PatternTable_Shr*) do_shmget((sizeof(PatternTable_Shr) * total_used_pattern_entries), "PatternTable_Shr");
  memset(pattern_table_shr, 0, sizeof(PatternTable_Shr) * total_used_pattern_entries);
  if(pattern_table){
    for(i = 0; i  < total_runprof_entries; i++){
      memcpy(pattern_table_shr + j, pattern_table, (sizeof(PatternTable_Shr) * 4));
      j = j + 4;
    }
  }
  FREE_AND_MAKE_NOT_NULL(pattern_table, "pattern_table", -1);

  for (i = 0; i < total_used_pattern_entries; i++){
  NSDL2_PARSING(NULL, NULL, "pattern_table_shr[i].reg_start_idx = %d, pattern_table_shr[i].num_entries = %d", pattern_table_shr[i].reg_start_idx, pattern_table_shr[i].num_entries);

  }
}


inline void kw_set_inc_ex_domain_usage(char *buf){

  fprintf(stderr, "Invalid value of G_INC_EX_DOMIAN_SETTINGS %s", buf);
  fprintf(stderr, "Usages: G_INC_EX_DOMIAN_SETTINGS <grp_name> <mode>");
  fprintf(stderr, "  This keyword is used to consider relative url as main url.\n");
  fprintf(stderr, "    in grp_name give any group name or ALL .\n");
  fprintf(stderr, "    Mode: Mode for G_HTTP_MODE 0, 1.\n");
  fprintf(stderr, "      0 - Donot consider relative url host as main url host in auto fetch filters. \n");
  fprintf(stderr, "      1 - Default mode . This is consider relative url host as main url host in auto fetch filters \n");
  NS_EXIT(-1, "%s\nUsages: G_INC_EX_DOMIAN_SETTINGS <grp_name> <mode>", buf);
}

/*----------------------------------------------------------------------------
   This keyword is added to do/dont consider relative url host as its main url host 
   in autofetch filters
   mode 0(do not consider) is not recommended mode.
   mode 1 is supported for khols special case only. This will consider relative url host
   as main url host. This is the default mode. 

------------------------------------------------------------------------------*/
void kw_set_include_exclude_domain_settings(char *buf, GroupSettings *gset) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  int mode;
  int num_fields;
  NSDL2_PARSING(NULL, NULL, "Method called. buf = %s", buf);
        
  num_fields = sscanf(buf, "%s %s %d", keyword, sgrp_name, &mode);
  if (num_fields != 3) {
    kw_set_inc_ex_domain_usage("Error in usage of Keyword must have three argument G_INC_EX_DOMIAN_SETTINGS <group> <mode>\n");
  }  
   
  // Checking for mode here . It should not be les than 0 or greater than 2 . 
  if (mode < 0 || mode > 1) {
    kw_set_inc_ex_domain_usage("G_INC_EX_DOMIAN_SETTINGS can have only two values 0 or 1.\n");
  }
  
  gset->include_exlclude_relative_url = mode;
  NSDL2_PARSING(NULL, NULL, "gset->include_exlclude_relative_url = %d", gset->include_exlclude_relative_url);  

}
