/********************************************************************
* Name: ns_trans_parse.c
* Purpose: Function related to parsing of transaction and PAGE_AS_TRANSACTION
* Author: Anuj
* Intial version date: 27/10/07
* Last modification date
********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/types.h>    // for match_pattern()
#include <regex.h>        // for match_pattern()

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
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

//#include "nslb_hash_code.c"

#include "ns_trans_parse.h"

#include "ns_parse_scen_conf.h"

#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "ns_trans.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_trans_normalization.h" 
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_script_parse.h"

//extern void end_test_run( void );
static char tx_pg_name[200]; // used in gen_page_name_tx (), to store the page as tx
static int tx_page_as_tx_dup = 0; // This is set to 1 if duplicity was found

#ifndef CAV_MAIN
TxTableEntry *txTable = NULL;
int max_tx_entries;   //moved from util.c
// Following are defined as extern in util.h
int total_tx_entries;   //moved from util.c
TxTableEntry_Shr* tx_table_shr_mem = NULL;
int *tx_hash_to_index_table = NULL;
int *tx_hash_to_index_table_shr_mem = NULL; 
unsigned int (*tx_hash_func)(const char*, unsigned int);
extern NormObjKey normRuntimeTXTable;
#else
__thread TxTableEntry *txTable = NULL;
__thread int total_tx_entries;   //moved from util.c
__thread int max_tx_entries;   //moved from utili.c
__thread TxTableEntry_Shr* tx_table_shr_mem = NULL;
__thread int *tx_hash_to_index_table = NULL;
__thread int *tx_hash_to_index_table_shr_mem = NULL; 
__thread unsigned int (*tx_hash_func)(const char*, unsigned int);
extern __thread NormObjKey normRuntimeTXTable;
#endif
//char *(*get_tx)(unsigned int);

/* Keyword parsing code */

#define max_pages_per_tx_usage(err, runtime_flag, err_buff) \
{ \
  sprintf(err_buff, "Error: Invalid value of G_MAX_PAGES_PER_TX keyword: %s\n", err); \
  strcat(err_buff, "  Usage: G_MAX_PAGES_PER_TX <group_name> <count>\n"); \
  strcat(err_buff, "  Where:\n"); \
  strcat(err_buff, "    <group_name> It can be ALL or any valid group name\n"); \
  strcat(err_buff, "    <count> - Maximum number of page instances allowed in one transaction. It should be >= 1 and <= 64000\n"); \
  if(runtime_flag != 1) \
    {NS_EXIT(-1, "%s", err_buff);} \
  else { \
    NSTL1_OUT(NULL, NULL,"%s", err_buff); \
    return -1; \
  } \
}

#ifndef CAV_MAIN
TxDataSample *txData = NULL;
int g_trans_avgtime_idx = -1;
#else
__thread TxDataSample *txData = NULL;
__thread int g_trans_avgtime_idx = -1;
#endif
TxDataCum *txCData = NULL;
int g_trans_cavgtime_idx = -1;

//called by parent
inline void update_trans_cavgtime_size(){

  NSDL3_TRANS(NULL, NULL, "Method Called, g_cavgtime_size = %d, g_trans_cavgtime_idx = %d",
                                          g_cavgtime_size, g_trans_cavgtime_idx);

    g_trans_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += ((total_tx_entries)*sizeof(TxDataCum));

  NSDL3_TRANS(NULL, NULL, "After g_cavgtime_size = %d, g_trans_cavgtime_idx = %d",
                                          g_cavgtime_size, g_trans_cavgtime_idx);
}

//Called by parent
inline void update_trans_avgtime_size() {

  NSDL3_TRANS(NULL, NULL, "Method Called, g_avgtime_size = %d, g_ftp_avgtime_idx = %d",
                                          g_avgtime_size, g_trans_avgtime_idx);

  nslb_init_norm_id_table_ex(&normRuntimeTXTable, global_settings->dyn_tx_norm_table_size);
  tx_hash_func = ns_tx_hash_func;
  g_trans_avgtime_idx = g_avgtime_size;
  g_avgtime_size += ((total_tx_entries)*sizeof(TxDataSample));

  ns_trans_init_loc2norm_table(total_tx_entries);

  NSDL3_TRANS(NULL, NULL, "After g_avgtime_size = %d, g_trans_avgtime_idx = %d ",
                                          g_avgtime_size, g_trans_avgtime_idx);
}

// Called by child
inline void set_trans_avgtime_ptr() {
  NSDL3_TRANS(NULL, NULL, "Method Called");

  //Here we must set txData in case of static or dyanmic as well
  //if(total_tx_entries) {
    NSDL3_TRANS(NULL, NULL, "total_tx_entries = %d", total_tx_entries);
    txData = (TxDataSample*)((char *)average_time + g_trans_avgtime_idx);
  /*} else {
    NSDL2_TRANS(NULL, NULL, "TX not found.");
    txData = NULL;
  }*/

  NSDL3_TRANS(NULL, NULL, "txData = %p", txData);
}

// G_MAX_PAGES_PER_TX <grp_name> <count>
// Note - This is not run time changeable. But it handling it for future
int kw_set_g_max_pages_per_tx(char *buf, unsigned short *max_pages_per_tx, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH], chk_count[100];
  char tmp[100];
  int num, num_count;
  sg_name[0] = chk_count[0] = '\0';

  NSDL2_PARSING(NULL, NULL, "Method called. buf = %s, runtime_flag = %d", buf, runtime_flag);

  // tmp as last field is to validate if any extra field is given in the keyword
  num = sscanf(buf, "%s %s %s %s", keyword, sg_name, chk_count, tmp);
  if (num != 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_PAGES_PER_TX_USAGE, CAV_ERR_1011218, CAV_ERR_MSG_1);
  }

  //check for numeric value of  count
  if(ns_is_numeric(chk_count) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_PAGES_PER_TX_USAGE, CAV_ERR_1011218, CAV_ERR_MSG_2);
  }

  num_count = atoi(chk_count);
  // Check range of count
  if((num_count < 1) || (num_count > 64000))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_PAGES_PER_TX_USAGE, CAV_ERR_1011219, "");
  }
  
  *max_pages_per_tx = (unsigned short)num_count;

  NSDL2_PARSING(NULL, NULL, "Maximum page instances allowed in one transaction for scenario group %s is %hd", sg_name, num_count);
  return 0;
}

/*****************************************************************/

// START - Standard fns, used in this file, need to be moved to some common library later

// This fn will be called from tx_val_name()
// Pattern is "^[a-zA-Z][a-zA-Z0-9_]*$" for the tranasction name
int match_pattern(const char *name, char *pattern)
{
  int status;
  regex_t re;

  NSDL3_TRANS(NULL, NULL, "Method called, name = %s, pattern = %s", name, pattern);

  if(regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0)
  {
    return 0;
  }
  status = regexec(&re, name, (size_t)0, NULL, 0);
  regfree(&re);
  if(status != 0)
  {
   return 0;
  }
  return 1;
}

// This fn is used for getting the output of command for checking the duplicity
int get_cmd_output (char *cmd, char *purpose)
{
  FILE* app;
  char buffer[MAX_LINE_LENGTH];

  NSDL3_TRANS(NULL, NULL, "Method called, command = %s, purpose = %s", cmd, purpose);

  app = popen (cmd, "r");
  if (app == NULL)
  {
    NS_EXIT(-1, "Error: Error in running the command (%s) for %s", cmd, purpose);
  }
  if (!nslb_fgets(buffer, MAX_LINE_LENGTH, app, 1))
  {
    NS_EXIT(-1, "Error: Error in reading the command (%s) output for %s", cmd, purpose);
  }
  pclose(app);

  NSDL3_TRANS(NULL, NULL, "The output of command is = %d", atoi(buffer));
  return (atoi(buffer));
}

// END - Standard fns, used in this file, need to be moved to some common library later

/*****************************************************************/

// START - Utililty Functions used withing this file

void create_and_fill_norm_ids_in_session_table(int norm_id, int sess_idx)
{
  NSDL3_TRANS(NULL, NULL, "Method called norm_id = %d sess_idx = %d", norm_id, sess_idx);
  if(gSessionTable[sess_idx].num_dyn_entries == gSessionTable[sess_idx].max_dyn_entries)
  {
    MY_REALLOC(gSessionTable[sess_idx].dyn_norm_ids, (gSessionTable[sess_idx].max_dyn_entries + DELTA_TX_ENTRIES) * sizeof(int), "dyn_norm_ids", -1); 
    gSessionTable[sess_idx].max_dyn_entries += DELTA_TX_ENTRIES;
  } 
  gSessionTable[sess_idx].dyn_norm_ids[gSessionTable[sess_idx].num_dyn_entries++] = norm_id;
}

// This fn moved from util.c
int create_tx_table_entry(int *row_num)
{
  NSDL3_TRANS(NULL, NULL, "Method called");

  if (total_tx_entries == max_tx_entries)
  {
    MY_REALLOC(txTable ,(max_tx_entries + DELTA_TX_ENTRIES) *sizeof(TxTableEntry), "txTable", -1);
    max_tx_entries += DELTA_TX_ENTRIES;
  }
  *row_num = total_tx_entries++;
  txTable[*row_num].sp_grp_tbl_idx = -1;   //default value for Sync Point
  
  NSDL3_TRANS(NULL, NULL, "Updated total_tx_entries = %d", total_tx_entries);
  return (SUCCESS);
}

// Moved from util.c
// This method is called from two places - From Add trans and from generate hash code
static inline int find_tx_idx(char* name)
{
  int i;

  NSDL3_TRANS(NULL, NULL, "name = %s, Method called", name);
  for (i = 0; i < total_tx_entries; i++)
  {
    if (!strcmp(RETRIEVE_BUFFER_DATA(txTable[i].tx_name), name))
      return i;
  }
  //NSDL3_TRANS(NULL, NULL, "Transaction with name = %s does not found in the txTable\n", name);
  return -1;
}
/*
inline char* find_tx_name(int idx)
{
  int i;

  NSDL3_TRANS(NULL, NULL, "idx = %d, Method called", idx);
  for (i = 0; i < total_tx_entries; i++)
  {
     if(i == idx)
     {
       return(RETRIEVE_BUFFER_DATA(txTable[i].tx_name));
     }
  }
  return NULL;
}*/

static inline int create_tbl_and_add_trans_name (char *tx_name)
{
  int tx_idx;

  NSDL3_TRANS(NULL, NULL, "transaction name = %s, Method called", tx_name);

  if (create_tx_table_entry(&tx_idx) == -1)
  {
    NS_EXIT(-1, "Error: Error allocating memory for Tranasction. Tranasction name is %s", tx_name);
  }
  if ((txTable[tx_idx].tx_name = copy_into_big_buf(tx_name, 0)) == -1)
  {
    NS_EXIT(-1, "Error: Error allocating shared memory for Tranasction, Tranasction name is %s", tx_name);
  }
  return (tx_idx);
}


// Add the tx_name in TxTableEntry, return 0 on success or -1 on failure
// This fn will be called after the genrate_page_name () in the tx_add_pages_as_trans ()//////chaeck it not sure
// Issue: how to handle if the page based tx name and user specified tx name are same (API)????

//Splitting this function becoz doing tx table creation two times: {1} only tx name (2) tx name with suffix
inline int add_trans_name (char *tx_name, int sess_idx)
{
  int tx_idx;

  NSDL3_TRANS(NULL, NULL, "name = %s, Method called sess_idx = %d", tx_name, sess_idx);

  if ((tx_idx = find_tx_idx(tx_name)) == -1)
  {
    tx_idx = create_tbl_and_add_trans_name(tx_name);
  }
  create_and_fill_norm_ids_in_session_table(tx_idx, sess_idx);
  return (tx_idx);
}

inline void add_trans_name_with_netcache()
{
  int i, grp_idx, use_flag = 0;
  char tx_name_with_suffix[1024 + 1];

  NSDL3_TRANS(NULL, NULL, "Method called");

  for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
  {
    NSDL3_TRANS(NULL, NULL, "runProfTable[%d].gset.ns_tx_http_header_s.end_tx_mode = %d, grp name = %s", 
                             grp_idx, runProfTable[grp_idx].gset.ns_tx_http_header_s.end_tx_mode, 
                             RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name));
    if(runProfTable[grp_idx].gset.ns_tx_http_header_s.end_tx_mode == END_TX_BASED_ON_NETCACHE_HIT)
    {

      global_settings->protocol_enabled |= TX_END_NETCACHE_ENABLED;
      use_flag++;
      break;
    }
  }

   NSDL3_TRANS(NULL, NULL, "use_flag = %d", use_flag);
  if(use_flag == 0)
  {
    NSDL3_TRANS(NULL, NULL, "NetCache stats are not enabled for this scenario group. Hence returning...");
    return;
  }

  // We need to save this as total_tx_entries gets increamented when we add tx
  int total_tx = total_tx_entries;
  for (i = 0; i < total_tx; i++)
  {
    NSDL3_TRANS(NULL, NULL, "in for loop i = %d, total_tx = %d", i, total_tx);

    sprintf(tx_name_with_suffix, "%s%s", RETRIEVE_BUFFER_DATA(txTable[i].tx_name), runProfTable[grp_idx].gset.ns_tx_http_header_s.end_tx_suffix);

    NSDL3_TRANS(NULL, NULL, "Adding tx_name_with_suffix = %s", tx_name_with_suffix);
    create_tbl_and_add_trans_name(tx_name_with_suffix);
  }
}

// It will genrate the trans name depending up on the value of PAGE_AS_TRANSACTION

static inline char *gen_page_name_tx(char *sess_name, char *page_name, char *suffix)
{
  NSDL3_TRANS(NULL, NULL, "sess_name = %s, page_name = %s, suffix = %s, Method called", sess_name, page_name, suffix);

  if(global_settings->pg_as_tx_name == 0) //default add tx_ in transaction name
  {  
    strcpy(tx_pg_name, "tx_");
  } else { //only page name
    strcpy(tx_pg_name, "");
  }
  
  if(tx_page_as_tx_dup == 1)  // If duplicity
  {
    strcat(tx_pg_name, sess_name);
    strcat(tx_pg_name, "_");
  }

  strcat(tx_pg_name, page_name);

  if (suffix[0] != '\0')
  {
    strcat(tx_pg_name, "_");
    strcat(tx_pg_name, suffix);
  }

  return (tx_pg_name);
}

// This fn validates the name of tx given by user
static inline void tx_val_name (char *tx_name)
{
  int tx_len = strlen(tx_name);

  NSDL3_TRANS(NULL, NULL, "tx_name = %s, tx_len = %d, Method called", tx_name, tx_len);

  if (tx_len > TX_NAME_MAX_LEN)
  {
    NS_EXIT(-1, "Error: Length of transaction name (%s) is (%d) larger than %d characters", tx_name, tx_len, TX_NAME_MAX_LEN);
  }

  if (match_pattern(tx_name, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
  {
    NS_EXIT(-1, "Error: 1. Name of Transaction should contain only alphanumeric character with including <91>_<92> but first character should be alpha, Name of Transaction given by User is = %s", tx_name);
  }
}

// This fn for checking the duplicity in transaction names generated from page name,
// The duplicity can be due to two resons:
//   1. Two or more scripts used in the scenario, and they have same page names.
//   2. Transaction name used in the API is same as main transaction of any page in any script used.
// Return - Sets tx_page_as_tx_dup = 1 if duplicate. Return value is not used
static inline int tx_check_dup ()
{
  int num_entry = 0;
  int i, j;
  char *page_name;
  PageTableEntry *pg_table_ptr;

  FILE* tx_file;
  char fname[MAX_LINE_LENGTH];

  char cmd[200];
  int cmd_out;

  NSDL3_TRANS(NULL, NULL, "Method called");

  sprintf(fname, "%s/tx_names_dup.txt", g_ns_tmpdir);// contains the tx_name for duplicity check
  if ((tx_file = fopen(fname, "w+")) == NULL)
  {
    perror("fopen");
    NS_EXIT(-1, "Error: Error in opening file %s", fname);
  }

   //printf("The total_tx_entries = %d\n", total_tx_entries);
   // First add all transaction from API int file
  for(i = 0; i < total_tx_entries; i++)
  {
    fprintf(tx_file, "%s\n", RETRIEVE_BUFFER_DATA(txTable[i].tx_name));
    num_entry++;
  }

  for (i = 0; i < total_sess_entries; i++)
  {
    for (j = 0; j < gSessionTable[i].num_pages; j++)
    {
      pg_table_ptr = &gPageTable[gSessionTable[i].first_page + j];
      page_name = RETRIEVE_BUFFER_DATA(pg_table_ptr->page_name);
      //fprintf(stderr,"tx_add_pages_as_trans() - Name of the session is %s and name of page is %s\n",sess_name, page_name);

      fprintf(tx_file, "%s\n", gen_page_name_tx("", page_name, ""));
      num_entry++;
    }
  }
  fclose(tx_file); // end of writing to the file
  //printf("tx_check_dup() : The num of total Transaction in the tx_names_dup.txt file is = %d\n", num_entry);

  // Run command to check if there are any duplicate entries in the file
  sprintf(cmd, "sort -u %s | wc -l", fname);
  cmd_out = get_cmd_output (cmd, "checking duplicity in Tranasction");
  if (num_entry != cmd_out)
  {
    tx_page_as_tx_dup = 1;
    NSDL3_TRANS(NULL, NULL, "There is duplicity in the tx_names");
    return 0;
  }
  NSDL3_TRANS(NULL, NULL, "There is no duplicity in the tx_names");
  return 0;
}

// End - Utililty Functions used withing this file
/******************************************************************/

/******************************************************************/

// Start: Functions which are called from other source files of netstorm.

// This fn sets the value of keyword PAGE_AS_TRANSACTION, it will be called from util.c,
// While parsing the keyword PAGE_AS_TRANSACTION
inline int set_page_as_trans (char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  char tx_value[MAX_DATA_LINE_LENGTH];
  char tx_name[MAX_DATA_LINE_LENGTH];
  char jm_parent_sm[MAX_DATA_LINE_LENGTH];// This field is only used for jmeter test - Jmeter Parent Sampler mode
  int num, value, tx_name_value, jm_parent_sample_mode;
  //Default value PAGE_AS_TRANSACTION 0 0
  tx_value[0] = '0';
  tx_value[1] = '\0';
  tx_name[0] = '0';
  tx_name[1] = '\0';
  jm_parent_sm[0] = '0';
  jm_parent_sm[1] = '\0';
 
  NSDL4_TRANS(NULL, NULL, "Method called, buf = %s", buf); 

  // Set default value of trans prefix in case of replay
  if(global_settings->replay_mode != 0){
    NSDL4_TRANS(NULL, NULL, "Setting default value of transaction prefix"); 
    tx_name[0] = '1';
  }

  //fprintf(stderr, "from fprintf() - set_page_as_trans()- Value of the keyword = %d, Method called", value);
  //Number of arguments
  num = sscanf(buf, "%s %s %s %s %s", keyword, tx_value, tx_name, jm_parent_sm, tmp);

  if ((num < 2) || (num > 4)) //Number of arguments can be 2 or 3(with transaction name format field && jmeter parent sampler mode)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PAGE_AS_TRANSACTION_USAGE, CAV_ERR_1011090, CAV_ERR_MSG_1);
  }
 
  //Validation for transaction value 
  if((ns_is_numeric(tx_value) == 0))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PAGE_AS_TRANSACTION_USAGE, CAV_ERR_1011090, CAV_ERR_MSG_2);
  }
  value = atoi(tx_value);

  if ((value < 0) || (value > 3)) // has user suppiled valid value  < 0 and  > 3
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PAGE_AS_TRANSACTION_USAGE, CAV_ERR_1011090, CAV_ERR_MSG_3);
  }
  
  if(num == 3){
    //Validation for transaction name format 
    if((ns_is_numeric(tx_name) == 0))
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PAGE_AS_TRANSACTION_USAGE, CAV_ERR_1011090, CAV_ERR_MSG_2);
    }
  }
  tx_name_value = atoi(tx_name);

  if ((tx_name_value < 0) || (tx_name_value > 1)) // has user suppiled valid value < 0 and > 1
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PAGE_AS_TRANSACTION_USAGE, CAV_ERR_1011090, CAV_ERR_MSG_3);
  }

  // Parsing of below field is only required for jmeter
  if(num == 4){
    //Validation for transaction name format for jmeter
    if((ns_is_numeric(jm_parent_sm) == 0))
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PAGE_AS_TRANSACTION_USAGE, CAV_ERR_1011090, CAV_ERR_MSG_2);
    }
  }
  jm_parent_sample_mode = atoi(jm_parent_sm);

  if ((jm_parent_sample_mode < 0) || (jm_parent_sample_mode > 1)) // has user suppiled valid value < 0 and > 1
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PAGE_AS_TRANSACTION_USAGE, CAV_ERR_1011376, CAV_ERR_MSG_3);
  }
  
  //Update global settings vars
  global_settings->pg_as_tx = value;
  global_settings->pg_as_tx_name = tx_name_value;
  global_settings->page_as_tx_jm_parent_sample_mode = jm_parent_sample_mode;
  return 0;
}

// Code for checking duplicate page name
  //{put here}

// For adding the page as tx in the create_page_table_entry
// This fn will be called from url.c
// The hash code for create_tx_table_entry will be made after this fn
inline void tx_add_pages_as_trans()
{
  char *page_name;
  char *sess_name;
  int i,j,k;
  char *err_name;
  PageTableEntry *pg_table_ptr;

  if (global_settings->pg_as_tx == 0)
    return;

  NSDL3_TRANS(NULL, NULL, "Starting : tx_add_pages_as_trans ()");

  tx_check_dup();  // for cheking the duplicity in tx_names

  for (i = 0; i < total_sess_entries; i++) // util.h:extern int total_sess_entries;
  {
    // sess_name = gSessionTable[i].sess_name;
    sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[i].sess_name);
    NSDL3_TRANS(NULL, NULL, "Name of the session is %s", sess_name);

    for (j = 0; j < gSessionTable[i].num_pages; j++)
    {
      pg_table_ptr = &gPageTable[gSessionTable[i].first_page + j];
      page_name = RETRIEVE_BUFFER_DATA(pg_table_ptr->page_name);
      //printf("tx_add_pages_as_trans() - Name of the session is %s and name of page is %s\n",sess_name, page_name);
      // case 1, will be used in all the cases

      // used for setting the idx of the main page tx in the page table
      pg_table_ptr->tx_table_idx = add_trans_name(gen_page_name_tx(sess_name, page_name, ""), gSessionTable[i].sess_id);

      switch(global_settings->pg_as_tx)
      {
        case 2:
          add_trans_name(gen_page_name_tx(sess_name, page_name, "Success"), gSessionTable[i].sess_id); //return value not required
          add_trans_name(gen_page_name_tx(sess_name, page_name, "Fail"), gSessionTable[i].sess_id);  //return value not required
          break;

        case 3:
          for (k = 0; k < TOTAL_USED_PAGE_ERR; k++)
          {
            err_name = RETRIEVE_BUFFER_DATA(errorCodeTable[pg_error_code_start_idx + k].error_msg);
            add_trans_name(gen_page_name_tx(sess_name, page_name, err_name), gSessionTable[i].sess_id);
          }
          break;
        } // end of switch
    } //end of for: page_num
  } //end of for: total_sess_entries
}  //end of fn

int
get_args(char *read_buf, char *args[])
{
  char *ptr = read_buf;
  int i = -1;
  
  i = strcspn(ptr, ")");
  if (i != -1) {
    ptr[i] = '\0';
  } else {
    return -1;
  }

  ptr = strtok(ptr, "(");
  if (ptr == NULL) return -1;

  i = 0;
  while (1) {
    args[i] = ptr;
    ptr = strtok(NULL, ",");
    if (ptr == NULL) break;
    i++;
  }
  return i;
}

int check_transaction_for_param(char *tx_name){
  
  int i = 0;
  int len;
  if(!tx_name)
    return 0;
  len = strlen(tx_name);
  if(tx_name[0] == '{' && tx_name[len-1] == '}')
  {
    i = 1;
    len = len -1;
  } 
  while(i < len){
    if(tx_name[i] == '{')
      return 1;
    i++;
  }
  return 0;
}

// Assumption is that only one API is in one line
int parse_transaction(char *buffer, char *fname, int line_num, int sess_idx)
{
char* buf_ptr;
char* fields[100]; // Keep for more as file may not be correct
char*  tx_name;

  // Parse ns_start_transaction("<Tx Name"); E.g ns_start_transaction("login_trans");
  // Parse ns_define_transaction("<Tx Name"); E.g ns_define_transaction("login_trans");
  // ns_define_transaction is used to define a transaction (not starting it). This
  // is used when we want to start a transaction using a variable. So we need to 
  // define these transactions so that these names can be added in the  hash table
  // For example:

  // ns_define_transaction("Login");
  // ns_define_transaction("SendMail");
  // ns_define_transaction("Logout");

  // char tx_name[64] = "SendMail";
  // ns_start_transaction(tx_name);

  if ((buf_ptr = strstr (buffer, "ns_start_transaction")) || (buf_ptr = strstr (buffer, "ns_define_transaction")))
  {
    if(get_tokens(buf_ptr, fields, "\"", 100) < 3)
    {
      int nargs = get_args(buf_ptr, fields);
      if (nargs == -1 || nargs != 1) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012003_ID, CAV_ERR_1012003_MSG);
      }
    }
    else {
      NSDL3_TRANS(NULL, NULL, "ns_start_transaction found in %s with tx_name = %s - added in the TxTable", fname, fields[1]);
      tx_name    = fields[1]; // tx_name to be started
      
      // If Tranasaction name is NS variable then skip to add transaction name as it should be added by ns_define_tranaction.
      // Here we are assuming that if first character of transaction name is { then it is NS variable
      if (strstr(fields[0], "ns_start_transaction") && check_transaction_for_param(fields[1]))
      {
        NSDL3_TRANS(NULL, NULL, "ns_start_transaction with mix parameter found therefore returning fields[0] = %s, fields[1][0] = %c ", fields[0], fields[1][0]);
        return 2;
      }

      if (fields[1][0] == '{')
      {
        NSDL3_TRANS(NULL, NULL, "ns_start_transaction with parameter found therefore returning fields[0] = %s, fields[1][0] = %c ", fields[0], fields[1][0]);
        return 0;
      }
     
      tx_val_name (tx_name);
      add_trans_name(tx_name, sess_idx); 
    }
  }
  //Parse ns_end_transaction_as("<Tx Name", <status>, "<End Tx Name"); E.g 
  //ns_end_transaction_as("login_trans", 0, "login_trans_suc");
  //ns_end_transaction_as("login_trans", 0, dyn_tx_name);
  //ns_end_transaction_as(dyn_tx_name, 0, dyn_tx_name);
  //ns_end_transaction_as(dyn_tx_name, 0, "login_trans");

  else if ((buf_ptr = strstr (buffer, "ns_end_transaction_as")))
  {
    if(get_tokens(buf_ptr, fields, "\"", 100) < 5)
    {
      //Incase of static to dynamic and dynamic to static we have to rebuild the buf_ptr as it gets tokenized due to '"' in input;
      if(fields[0] && fields[1] && fields[2])
      {
        sprintf(buf_ptr,"%s%s%s",fields[0],fields[1],fields[2]); //
      }

      int nargs = get_args(buf_ptr, fields);
      if (nargs == -1 || nargs != 3) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012005_ID, CAV_ERR_1012005_MSG);
      }
    } else {
      NSDL3_TRANS(NULL, NULL, "ns_end_transaction_as found in %s with start_tx_name = %s and end_tx_name = %s - added in the TxTable", fname, fields[1], fields[3]);
      tx_name = fields[3]; // tx_name to be end as

      // If Tranasaction name is NS variable then skip to add transaction name as it should be added by ns_define_tranaction.
      // Here we are assuming that if first character of transaction name is { then it is NS variable
      if (check_transaction_for_param(fields[1]))
      {
        NSDL3_TRANS(NULL, NULL, "ns_end_transaction_as with mixed parameter found therefore returning fields[0] = %s, fields[1][0] = %c ", fields[0], fields[1][0]);
       return 2;
      }

      if (check_transaction_for_param(fields[3]))
      {
        
        NSDL3_TRANS(NULL, NULL, "ns_end_transaction_as with mixed parameter found therefore returning fields[0] = %s, fields[3][0] = %c ", fields[0], fields[3][0]);
       return 2;
      }

      if (fields[1][0] == '{')
      {
        NSDL3_TRANS(NULL, NULL, "ns_end_transaction_as with parameter found therefore returning fields[0] = %s, fields[1][0] = %c ", fields[0], fields[1][0]);
        return 0;
      }

      // If Tranasaction name is NS variable then skip to add transaction name as it should be added by ns_define_tranaction.
      // Here we are assuming that if first character of transaction name is { then it is NS variable. Skipping validation for new name
      if (fields[3][0] == '{')
      {
        NSDL3_TRANS(NULL, NULL, "ns_end_transaction_as with parameter found therefore returning fields[0] = %s, fields[3][0] = %c ", fields[0], fields[3][0]);
        return 0;
      }

      if (tx_name) tx_val_name (tx_name);
      if (tx_name) add_trans_name(tx_name, sess_idx);
    }

  }
  // Parse ns_end_transaction("<Tx Name", <status>); E.g ns_start_transaction("login_trans", 0);
  // This must be done after ns_end_transaction_as
  else if ((buf_ptr = strstr (buffer, "ns_end_transaction")))
  {
    if(get_tokens(buf_ptr, fields, "\"", 100) < 3)
    {
      int nargs = get_args(buf_ptr, fields);
      if (nargs == -1 || nargs != 2) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012006_ID, CAV_ERR_1012006_MSG);
      }
    } else {
      NSDL3_TRANS(NULL, NULL, "ns_end_transaction found (here we are not ending the tx, this is parsing only) in %s with tx_name = %s\n", fname, fields[1]);
      tx_name = fields[1];
      if (check_transaction_for_param(fields[1]))
      {
        NSDL3_TRANS(NULL, NULL, "ns_end_transaction with mix parameter found therefore returning fields[0] = %s, fields[1][0] = %c ", fields[0], fields[1][0]);
        return 2;
      }
      if (fields[1][0] == '{')
      {
        NSDL3_TRANS(NULL, NULL, "ns_end_transaction with parameter found therefore returning fields[0] = %s, fields[1][0] = %c ", fields[0], fields[1][0]);
        return 0;
      }

      // No need to add this transaction as this should have added from ns_start_transaction()
    }
  }
  return 0;
}

// Anuj for getting the tx_names from the script.c
inline int get_tx_names(FILE* c_file)
{
  int line_num = 0;
  char buffer[MAX_LINE_LENGTH];
  int dumm_sess_id = 0;

  NSDL3_TRANS(NULL, NULL, "Method called");

  while (nslb_fgets(buffer, MAX_LINE_LENGTH, c_file, 1))
  {
    line_num++;
    parse_transaction(buffer, "script.c", line_num, dumm_sess_id);
  }
  return 0;
}
  

// used for genrating the hash_code for Transactions
// moved from url.c
inline int get_tx_hash()
{
  FILE* tx_file;
  int i;
  int in_names = 0;
  FILE* read_file;
  FILE* write_file;
  char cmd_buffer[MAX_LINE_LENGTH];
  char file_buffer[MAX_FILE_NAME];
  char fname[MAX_LINE_LENGTH];
  void* handle;
  char* error;
  char buffer[MAX_LINE_LENGTH];
  char wbuffer[MAX_LINE_LENGTH];
  char* buf_ptr;
  char* ptr;
  char tmp_buf[1024];
  char tmp_buf1[1024];
  int do_replace = 0;
  char err_msg[MAX_LINE_LENGTH] = {0};

  //Tx_Hash_code_to_str get_tx;
  NSDL3_TRANS(NULL, NULL, "Method Called");


  // If total_tx_entries is more than MAX_ALLOWED_TX (300), than the msg size will be more than the buffer size used for socket communication between child and parent.
  // Now we are using TCP communication instead of UDP so there is no limit of size hence no limit required for TX as well: 26/05/08: Anuj

#if 0
  if (total_tx_entries > MAX_ALLOWED_TX)
  {
    fprintf(stderr, "Error: get_tx_hash() - Total number of Transactions (%d) is more than the maximum (%d) allowed Transactions.\n", total_tx_entries, MAX_ALLOWED_TX);
    exit(-1);
  }
#endif

  //printf("The expected size of periodic Data before including TxData is = %d\n", sizeof(avgtime));
  //KJ remove this as we don't need periodic
  g_avgtime_size = sizeof(avgtime); //+ (total_tx_entries)*sizeof(TxDataSample);
 
  //printf("The expected size of periodic Data including TxData %d Transactions is = %d\n", total_tx_entries, g_avgtime_size);

  //printf("The expected size of cummulative Data before including TxData is = %d\n", sizeof(cavgtime));
  g_cavgtime_size = sizeof(cavgtime); //+ (total_tx_entries)*sizeof(TxDataCum);
  //printf("The expected size of cummulative Data including TxData for %d Transactions is = %d\n", total_tx_entries, g_cavgtime_size);

  if (total_tx_entries)
  {
    sprintf(fname, "%s/tx_names.txt", g_ns_tmpdir);
    if ((tx_file = fopen(fname, "w+")) == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, fname, errno, nslb_strerror(errno));
    }

    for (i = 0; i < total_tx_entries; i++)
    {
      fprintf(tx_file, "%s\n", RETRIEVE_BUFFER_DATA(txTable[i].tx_name));
    }

    fclose(tx_file);
 
    sprintf(fname, "gperf -c -C -I -G %s %s/tx_names.txt -N hash_tx > %s/hash_tx.c", 
                    global_settings->gperf_cmd_options, g_ns_tmpdir, g_ns_tmpdir);

    NSDL3_TRANS(NULL, NULL, "Gperf command = %s", fname);
    if(nslb_system(fname, 1, err_msg) != 0) 
    //if (system(fname) == -1)
    {
      NS_EXIT(-1, CAV_ERR_1000019, fname, errno, nslb_strerror(errno));
    }

    sprintf(tmp_buf, "%s (str, len)", "hash_tx");
    sprintf(tmp_buf1, "%s (register const char *str, register size_t len)", "hash_tx");
    NSDL3_TRANS(NULL, NULL, "KKKK = [%s]", tmp_buf);

    sprintf(fname, "%s/hash_tx.c", g_ns_tmpdir);
    if ((read_file = fopen(fname, "r")) == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, fname, errno, nslb_strerror(errno));
    }

    sprintf(fname, "%s/hash_tx_write.c", g_ns_tmpdir);
    if ((write_file = fopen(fname, "w+")) == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, fname, errno, nslb_strerror(errno));
    }

    while (nslb_fgets(buffer, MAX_LINE_LENGTH, read_file, 1))
    {
      if (in_names)
      {
        if (strstr(buffer, "{"))
        {
        }
        else if (strstr(buffer, "}"))
        {
          in_names = 0;
        }
        else
        {
          buf_ptr = buffer;
          CLEAR_WHITE_SPACE(buf_ptr);
          buf_ptr[strlen(buf_ptr) - 1] = '\0'; //remove new line char
          strcpy (wbuffer, buf_ptr);
          buf_ptr = strtok (wbuffer, ",");
          buffer[0] = '\0';
          while (buf_ptr)
          {
            if (i)
                    strcat (buffer, ",");
            ptr = buffer + strlen(buffer);
            sprintf (ptr, "{%s, ", buf_ptr);
            ptr = buffer + strlen(buffer);
            buf_ptr[strlen(buf_ptr) -1 ] = '\0'; //Remove trailing ""
            //printf("calling find_tx_idx() from get_tx_hash()\n");
            sprintf (ptr, "%d}", find_tx_idx (buf_ptr+1));   //strating after leading ""
            buf_ptr = strtok( NULL, ",");
            i++;
          }
          strcat (buffer, "\n");
        }
      }
      else if ((strstr(buffer, "const wordlist")))
      {
        in_names = 1;
        i = 0;
        strcpy (buffer, "static struct { char *name; short num; } wordlist[] =\n");
      }
      else if ((buf_ptr = strstr(buffer, "wordlist[key]")))
      {
        strcpy(buf_ptr, "wordlist[key].name;\n");
      }
      else if ((buf_ptr = strstr(buffer, "duplicates = ")))
      {
        buf_ptr += strlen("duplicates = ");
        if (atoi(buf_ptr) != 0)
        {
          NS_EXIT(-1, "CavErr[1031029]: duplicates in hash_tx.c file");
        }
      }
      else if ((buf_ptr = strstr(buffer, "return s")))
      {
        //strcpy(buf_ptr, "return key;\n");
        strcpy(buf_ptr, "return (int)wordlist[key].num;\n");
      }
      else if ((buf_ptr = strstr(buffer, "return 0")) && do_replace)
      {
        strcpy(buf_ptr, "return -1;\n");
        do_replace = 0;
        NSDL3_TRANS(NULL, NULL, "KKKKK replacing...");
      }
      else if (!strncasecmp(buffer, "const char *", strlen("const char *"))){
        strcpy(buffer, "int\n");
      } else if ((!strncmp(buffer, tmp_buf, strlen(tmp_buf))) || (!strncmp(buffer, tmp_buf1, strlen(tmp_buf1)))) {
        NSDL3_TRANS(NULL, NULL, "KKKKKK setting replace flag");
        do_replace = 1;
      }


      if (fputs(buffer, write_file) < 0)
      {
        NS_EXIT(-1, CAV_ERR_1000032, fname, errno, nslb_strerror(errno));
      }
    }

    fclose(read_file);
    fclose(write_file);

    //compile and link hash_tx_write.c
    sprintf(cmd_buffer, "gcc -g -m%d -fpic -shared -o %s/hash_tx_write.so %s/hash_tx_write.c",
                         NS_BUILD_BITS, g_ns_tmpdir, g_ns_tmpdir);
    //sprintf(cmd_buffer, "gcc -g -fpic -shared -o tmp/hash_tx_write.so tmp/hash_tx_write.c");

    if(nslb_system(cmd_buffer, 1, err_msg) != 0)
   // if (system(cmd_buffer) == -1)
    {
      NS_EXIT(-1, CAV_ERR_1000019, cmd_buffer, errno, nslb_strerror(errno));
    }

    if (!getcwd(file_buffer, MAX_FILE_NAME))
    {
      NS_EXIT(-1, CAV_ERR_1000035, errno, nslb_strerror(errno));
    }

    sprintf(file_buffer, "%s/%s/hash_tx_write.so", file_buffer, g_ns_tmpdir);
    //sprintf(file_buffer, "%s/tmp/hash_tx_write.so", file_buffer);

    handle = dlopen (file_buffer, RTLD_LAZY);

    if ((error = dlerror()))
    {
      // If so, print the error message and exit.
      fprintf (stderr, "%s\n", error);
      return -1;
    }

    tx_hash_func = dlsym(handle, "hash_tx");

    if ((error = dlerror()))
    {
      //If so, print the error message and exit.
      fprintf (stderr, "%s\n", error);
      return -1;
    }
    
    for (i = 0; i < total_tx_entries; i++)
    {
      char* tx_name = RETRIEVE_BUFFER_DATA(txTable[i].tx_name);
      txTable[i].tx_hash_idx = tx_hash_func(tx_name, strlen(tx_name));
      //txTable[i].tx_table_idx = total_tx_entries - 1;     // Added by Anuj
      assert(txTable[i].tx_hash_idx != -1);
    }
  
   //for testing only
   /* for (i = 0; i < total_tx_entries; i++)
    {
      fprintf(stdout, "\n tx name from hash code is : %s \n", get_tx(txTable[i].tx_hash_idx));
    }*/
  }
  return 0;
}

// used for allocating the memory in the bigbuf for TxTableEntry, moved from util.c
inline void alloc_mem_for_txtable ()
{
  total_tx_entries = 0;
  NSDL3_TRANS(NULL, NULL, "Method Called");
  //printf("alloc_mem_for_txtable(), Memory allocated for total %d entries\n", total_tx_entries);

  MY_MALLOC(txTable, INIT_TX_ENTRIES * sizeof(TxTableEntry), "txTable", -1);
  max_tx_entries = INIT_TX_ENTRIES;
}

// used for copying the txTable to tx_table_shr_mem, called from util.c
inline void copy_tx_entry_to_shr (void *tx_page_table_shr_mem, int total_page_entries)
{
  int i;
  NSDL3_TRANS(NULL, NULL, "Method Called, Total transaction entries = %d", total_tx_entries);

  tx_table_shr_mem = (TxTableEntry_Shr*) (tx_page_table_shr_mem + WORD_ALIGNED(sizeof(PageTableEntry_Shr) * total_page_entries));

  //printf("copy_tx_entry_to_shr(), The total tx entries going in shr Memory are = %d \n", total_tx_entries);

  for (i = 0; i < total_tx_entries; i++)
  {
    tx_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(txTable[i].tx_name);
    tx_table_shr_mem[i].tx_hash_idx = txTable[i].tx_hash_idx; //both ptr used by other fn also in util.c
    tx_table_shr_mem[i].sp_grp_tbl_idx = txTable[i].sp_grp_tbl_idx; 
  }
}

// End: Functions which are called from other source files of netstorm.

/******************************************************************/

/* NS netcache Tx keywords */

/*Purpose: Parse G_SEND_NS_TX_HTTP_HEADER to send transaction name in http header.
 * G_SEND_NS_TX_HTTP_HEADER  <grp/ALL>  <mode> [Tx Variable] [<HTTP Header name>]
 * */
int kw_set_g_send_ns_tx_http_header(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char dummy[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int enable_ns_tx_header;
  char header_name[MAX_DATA_LINE_LENGTH] = {0};
  char tx_name[MAX_DATA_LINE_LENGTH];
  int sg_fields = 3;
  int num;

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %d %s %s", dummy, sg_name, &enable_ns_tx_header, header_name, tx_name)) < sg_fields)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SEND_NS_TX_HTTP_HEADER_USAGE, CAV_ERR_1011039, CAV_ERR_MSG_1);
  }

  NSDL2_PARSING(NULL, NULL, " num = %d ", num);

  val_sgrp_name(buf, sg_name, 0);//validate group name

  if(enable_ns_tx_header != NS_TX_HTTP_HEADER_DO_NOT_SEND && enable_ns_tx_header != NS_TX_HTTP_HEADER_SEND_FOR_MAIN_URL && enable_ns_tx_header != NS_TX_HTTP_HEADER_SEND_FOR_BOTH_MAIN_AND_INLINE)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SEND_NS_TX_HTTP_HEADER_USAGE, CAV_ERR_1011039, CAV_ERR_MSG_3);
  }

  if(strlen(header_name) > MAX_NS_TX_HTTP_HEADER) {
    sprintf(err_msg, "Warning: Header_Name provide with keyword G_SEND_NS_TX_HTTP_HEADER is too large. Will get truncated to size %d",  MAX_NS_TX_HTTP_HEADER);
    NS_DUMP_WARNING("Header name provide with keyword G_SEND_NS_TX_HTTP_HEADER is too large. Header name will be truncated to size %d",  MAX_NS_TX_HTTP_HEADER);
  }

  gset->ns_tx_http_header_s.mode = enable_ns_tx_header;

  if(num < 4)
    sprintf(gset->ns_tx_http_header_s.header_name, "%s: ", "CavTxName");
  else
    sprintf(gset->ns_tx_http_header_s.header_name, "%s: ",  header_name);

    gset->ns_tx_http_header_s.header_len = strlen(gset->ns_tx_http_header_s.header_name);

  memset(gset->ns_tx_http_header_s.tx_variable, 0, sizeof(gset->ns_tx_http_header_s.tx_variable));
  if(num > 4)
    strcpy(gset->ns_tx_http_header_s.tx_variable, tx_name);

  NSDL2_PARSING(NULL, NULL, "After Parsing G_SEND_NS_TX_HTTP_HEADER GroupName = %s, enable_ns_tx_header mode = %d," 
                      "header_name = %s, len = %d",
                       sg_name, gset->ns_tx_http_header_s.mode,
                       gset->ns_tx_http_header_s.header_name, gset->ns_tx_http_header_s.header_len);

  return 0;
}

/*Purpose: Parse G_END_TX_NETCACHE to decide whether to end transaction based on NetCache hit or not
 * G_END_TX_NETCACHE <group-name> <mode>
 * */
int kw_set_g_end_tx_netcache(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char dummy[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char end_tx_suffix[MAX_DATA_LINE_LENGTH];
  int enable_end_tx_netcache;
  int sg_fields = 3;
  int num;

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %d %s", dummy, sg_name, &enable_end_tx_netcache, end_tx_suffix)) < sg_fields)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_END_TX_NETCACHE_USAGE, CAV_ERR_1011038, CAV_ERR_MSG_1);
  }

  NSDL2_PARSING(NULL, NULL, " num = %d ", num);

  val_sgrp_name(buf, sg_name, 0);//validate group name

  if(enable_end_tx_netcache != END_TX_BASED_ON_NETCACHE_HIT && enable_end_tx_netcache != NS_TX_HTTP_HEADER_DO_NOT_SEND)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_END_TX_NETCACHE_USAGE, CAV_ERR_1011038, CAV_ERR_MSG_3);
  }

  //This keyword can be used only if G_ENABLE_NETWORK_CACHE_STATS for the corresponding group is used.
  if((gset->enable_network_cache_stats == 0) && (enable_end_tx_netcache == END_TX_BASED_ON_NETCACHE_HIT))
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING, __FILE__, (char*)__FUNCTION__, 
                "Transaction names for scenario group '%s' cannot be ended with different"
                " name based on NetCache hit as NetCache stats are not enabled for this scenario group.", sg_name);

     gset->ns_tx_http_header_s.end_tx_mode = 0;
     return 0;
  }

  gset->ns_tx_http_header_s.end_tx_mode = enable_end_tx_netcache;

  memset(gset->ns_tx_http_header_s.end_tx_suffix, 0, sizeof(gset->ns_tx_http_header_s.end_tx_suffix));

  if(num >= 4)
    strcpy(gset->ns_tx_http_header_s.end_tx_suffix, end_tx_suffix);
  else
    strcpy(gset->ns_tx_http_header_s.end_tx_suffix, "_NetCache");

  NSDL2_PARSING(NULL, NULL, "After Parsing G_END_TX_NETCACHE GroupName = %s, enable_end_tx_netcache = %d," 
                            "end_tx_suffix = %s", sg_name, gset->ns_tx_http_header_s.end_tx_mode, 
                             gset->ns_tx_http_header_s.end_tx_suffix);
  return 0;
}

/*****************************************Dynamic Tranasaction Parsing Code Begin*************************************/
// keyword parsing usages 
static void ns_show_runtime_tx_data_usage(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of DYNAMIC_TX_SETTINGS keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: DYNAMIC_TX_SETTINGS <TX name length> <TX limit> <norm table size> <Table init size> <Table delta entry size>.\n");
  NSTL1_OUT(NULL, NULL, "  This keyword is used to enable or disable the dynamic Transaction graph data.\n");
  NSTL1_OUT(NULL, NULL, "    TX name length: Length of TX arrived in runtime.\n");
  NSTL1_OUT(NULL, NULL, "    TX limit : limit of total transaction.\n");
  NSTL1_OUT(NULL, NULL, "    TX norm_table size: Size of norm table.\n");
  NSTL1_OUT(NULL, NULL, "    TX table init size: Initial Size of norm table.\n");
  NSTL1_OUT(NULL, NULL, "    TX table delta entry size: Incrementing size of TX table.\n");
  NS_EXIT(-1, "Error: Invalid value of DYNAMIC_TX_SETTINGS keyword: %s\nUsage: DYNAMIC_TX_SETTINGS <TX name length> <TX limit> <norm table size> <Table init size> <Table delta entry size>.", err);
}

//keyword parsing
// DYNAMIC_TX_SETTINGS <enable dyn tx (1)/0> <max dyn name tx len> <max limit of tx> <dyn tx norm_table size> <initial size> <delta><threshold_for_using_gperf>

int kw_set_dynamic_tx_settings(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char max_dyn_tx_name_len_str[32 + 1] = {0};
  char total_tx_limit_str[32 + 1] = {0};
  char dyn_tx_norm_table_size_str[32 + 1] = {0};
  char dyn_tx_initial_row_size_str[32 + 1] = {0};
  char dyn_tx_delta_row_size_str[32 + 1] = {0};
  char threshold_for_using_gperf_str[32 + 1] = {0};
  
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;

  short max_dyn_tx_name_len = 1024;  // Maximum lenght of dynamics transactions
  int total_tx_limit = 10000;  // Max limit of total transactions
  short dyn_tx_norm_table_size = 4096;  // Table size of dynamic normalization table
  short dyn_tx_initial_row_size = 100;  // Initial number of rows to allocate for dyn tx 
  short dyn_tx_delta_row_size = 10;  // Delta number of rows to allocate for dyn tx 
  int threshold_for_using_gperf = 1000; //above this gperf is slow

  num = sscanf(buf, "%s %s %s %s %s %s %s %s ", keyword, max_dyn_tx_name_len_str, total_tx_limit_str, dyn_tx_norm_table_size_str, dyn_tx_initial_row_size_str, dyn_tx_delta_row_size_str, threshold_for_using_gperf_str, tmp);

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, keyword = %d, max_dyn_tx_name_len = [%s], total_tx_limit=[%s]  dyn_tx_norm_table_size = [%s] dyn_tx_initial_row_size = [%s] dyn_tx_delta_row_size = [%s] threshold_for_using_gperf_str = [%s]", buf, keyword, max_dyn_tx_name_len_str, total_tx_limit_str, dyn_tx_norm_table_size_str, dyn_tx_initial_row_size_str, dyn_tx_delta_row_size_str,threshold_for_using_gperf_str);

  //check for total number of arguments parsed

  if(num < 2 || num > 7)
  {
    ns_show_runtime_tx_data_usage("Invalid number of arguments");
  }
  
  // Maximum lenght of dynamics transactions
  if(max_dyn_tx_name_len_str[0] != '\0')
  {
    if(ns_is_numeric(max_dyn_tx_name_len_str) == 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS transaction name length is not numeric");
    }

    max_dyn_tx_name_len = atoi(max_dyn_tx_name_len_str);

    if(max_dyn_tx_name_len < 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS transaction name length should not be negative!!");
    }

    if(max_dyn_tx_name_len < 48)
    {
         NSDL2_PARSING(NULL, NULL, "Setting Max_Transaction_Len to default 48");
         max_dyn_tx_name_len = 48;
    }

    if(max_dyn_tx_name_len > 1024)
    {
        NSDL2_PARSING(NULL, NULL, "Setting Max_Transaction_Len to default 1024");
        max_dyn_tx_name_len = 1024;
    }

  }
  
  // Max limit of total transactions
   if(total_tx_limit_str[0] != '\0')
  {
    if(ns_is_numeric(total_tx_limit_str) == 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS maximum umber of transactions is not numeric");
    }

    total_tx_limit = atoi(total_tx_limit_str);

    if(total_tx_limit < 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS number of tranasction should not be negative!!");
    }
  }
  
  // Table size of dynamic normalization table
  if(dyn_tx_norm_table_size_str[0] != '\0')
  {
    if(ns_is_numeric(dyn_tx_norm_table_size_str) == 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS Size of normalization table is not numeric");
    }

    dyn_tx_norm_table_size = atoi(dyn_tx_norm_table_size_str);

    if(dyn_tx_norm_table_size < 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS Normalizalization table size should not be negative!!");
    }
  }
  
  // Initial number of rows to allocate for dyn tx
  if(dyn_tx_initial_row_size_str[0] !='\0')
  {
    if(ns_is_numeric(dyn_tx_initial_row_size_str) == 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS Initially no of rows in transaction table is not numeric");
    }

    dyn_tx_initial_row_size = atoi(dyn_tx_initial_row_size_str);
 
    if(dyn_tx_initial_row_size < 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS Transaction table initial no of row size should not be negative !!");
    }
  }
  
  //check for Incrementing size of transaction table
  if(dyn_tx_delta_row_size_str[0] != '\0')
  {
    if(ns_is_numeric(dyn_tx_delta_row_size_str) == 0)
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS Incrementing size of transaction table is not numeric !!");
    }

    dyn_tx_delta_row_size = atoi(dyn_tx_delta_row_size_str);

    if(dyn_tx_delta_row_size < 0 )
    {
      ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS Incrementing size of transaction table should not be negative!!");
    }
  }

  //check for threshold for using gperf
  if(threshold_for_using_gperf_str[0] !='\0')
  {
    if(ns_is_numeric(threshold_for_using_gperf_str) == 0)
    {
     ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS threshold for using gperf table is not numeric !!");
    }
    
    threshold_for_using_gperf = atoi(threshold_for_using_gperf_str);
    
    if(threshold_for_using_gperf < 0)
    {
     ns_show_runtime_tx_data_usage("DYNAMIC_TX_SETTINGS threshold for using gperf table should not be negative!!");
    }
  }
  global_settings->max_dyn_tx_name_len = max_dyn_tx_name_len;
  global_settings->total_tx_limit = total_tx_limit;
  global_settings->dyn_tx_norm_table_size = dyn_tx_norm_table_size;
  global_settings->dyn_tx_initial_row_size = dyn_tx_initial_row_size;
  global_settings->dyn_tx_delta_row_size = dyn_tx_delta_row_size;
  global_settings->threshold_for_using_gperf = threshold_for_using_gperf;

  NSDL2_PARSING(NULL, NULL, "Transaction Name Length = [%d], Tranasaction Maximum Limit = [%d], Transaction Norm Table Size = [%d], Initial Size of Transaction Table= [%d], Incrementing Size of Transaction Table = [%d],threshold_for_using_gperf_str = [%d]", global_settings->max_dyn_tx_name_len, global_settings->total_tx_limit, global_settings->dyn_tx_norm_table_size, global_settings->dyn_tx_initial_row_size, global_settings->dyn_tx_delta_row_size,global_settings->threshold_for_using_gperf);

  return 0;
}

// End of file
