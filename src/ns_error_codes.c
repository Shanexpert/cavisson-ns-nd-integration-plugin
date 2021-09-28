/********************************************************************
 * Name            : ns_error_codes.c 
 * Purpose         : All the error code loaded using nsu_get_error command.
 * Initial Version : Tuesday, November 10 2009
 * Modification    : -
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <regex.h>

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
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_gdf.h"
#include "ns_group_data.h"  //newly added to test group 
#include "ns_exit.h"  //newly added to test group 
#include "ns_error_msg.h" 

extern FILE *write_gdf_fp;
extern int loader_opcode;
// Array of error code have following errors
// 0 to 31 - URL Errors (Also used by page and Tx)
// 32 - 63 - Page specific errors (Also used by Tx)
// 64 - 95 - Tx specific errors
// 96 - 111 - Session errors
//
#ifndef CAV_MAIN
int total_errorcode_entries;
#else
__thread int total_errorcode_entries;
#endif
int pg_error_code_start_idx = 0;  // Since URL errors are also page errors, it is set to 0
int tx_error_code_start_idx = 0;  // Since URL and Page errors are also Tx errors, it is set to 0
int sess_error_code_start_idx = -1; // For session, we have separate memebers in array

void copy_errorCodeTable_to_errorcode_table_shr_mem()
{
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, total_errorcode_entries = %d", total_errorcode_entries);

  if (total_errorcode_entries) {
     errorcode_table_shr_mem = (ErrorCodeTableEntry_Shr*) do_shmget(sizeof(ErrorCodeTableEntry_Shr) * total_errorcode_entries, "errocode Table");
     for (i = 0; i < total_errorcode_entries; i++) {
       errorcode_table_shr_mem[i].error_code = errorCodeTable[i].error_code;
       if (errorCodeTable[i].error_msg != -1)
         errorcode_table_shr_mem[i].error_msg = BIG_BUF_MEMORY_CONVERSION(errorCodeTable[i].error_msg);
       else
         errorcode_table_shr_mem[i].error_msg = NULL;
     }
  }
}

int create_errorcode_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_errorcode_entries == max_errorcode_entries) {
    MY_REALLOC_EX (errorCodeTable, (max_errorcode_entries + DELTA_ERRORCODE_ENTRIES) * sizeof(ErrorCodeTableEntry),(max_errorcode_entries * sizeof(ErrorCodeTableEntry)), "errorCodeTable", -1);// old size(maximun error code entries * size of ErrorCodeTableEntry table)
    if (!errorCodeTable) {
      fprintf(stderr,"create_errorcode_table_entry(): Error allocating more memory for errorcode entries\n");
      return(FAILURE);
    } else max_errorcode_entries += DELTA_ERRORCODE_ENTRIES;
  }
  *row_num = total_errorcode_entries++;
  NSDL4_MISC(NULL, NULL, "row_num = %d", row_num);
  
  return (SUCCESS);
}
typedef void (*sighandler_t)(int);

/*bug 92660: cavisson lite*/
void handler_sigchild_ignore(int data)
{
}

static int run_cmd_and_fill_errorCodeTable(int arg)
{
  char cmd[256];
  char error_name[64];
  int start_index, i = -1;
  FILE *app;

  NSDL2_HTTP(NULL, NULL, "arg = %d", arg) ;

  if(arg == OBJ_PAGE_ID)
    start_index = pg_error_code_start_idx;
  else if (arg == OBJ_TRANS_ID)
    start_index = tx_error_code_start_idx;
  else if (arg == OBJ_SESS_ID)
    start_index = sess_error_code_start_idx;
  else
    NS_EXIT(-1, CAV_ERR_1031015, arg);
  
  sprintf(cmd, "%s/bin/nsu_get_errors %d", g_ns_wdir, arg);

  NSDL3_HTTP(NULL, NULL, "start_index = %d", start_index) ;

 sighandler_t prev_handler; /*bug 92660: cavisson lite*/
  //prev_handler = signal(SIGCHLD, SIG_IGN);
 prev_handler = signal(SIGCHLD, handler_sigchild_ignore);
 
  
  if ((app = popen(cmd, "r")) != NULL) {
    while (nslb_fgets(error_name, 64, app, 0) != NULL) {
      NSDL3_HTTP(NULL, NULL, "error = %s", error_name) ;
      ++i;
      error_name[strlen(error_name) - 1] = '\0';
      if (errorCodeTable[start_index + i].error_msg != -1)
        continue;

      if ((errorCodeTable[start_index + i].error_msg =
                          copy_into_big_buf(error_name, 0)) == -1) {
        NS_EXIT(-1, CAV_ERR_1000018, error_name);
      }
    }
    if (pclose(app) == -1)
    {
      NS_EXIT(-1, CAV_ERR_1000030, cmd, errno, nslb_strerror(errno));
    }
    (void) signal( SIGCHLD, prev_handler); /*bug 92660: cavisson lite*/
  }
  else {
    NS_EXIT(-1, CAV_ERR_1000031, cmd, errno, nslb_strerror(errno));
  }
 
  return 0;
}


int input_error_codes(void) {
  int i;
  int rnum;

  NSDL2_HTTP(NULL, NULL, "Method called");
  
  /* Not Used
  //url_errorcode_table has 0-8 entries same as error_names
  for (i = 0; i < TOTAL_USER_URL_ERR; i++)
      url_errorcode_table[i] = error_names[i];
  */

  // Tx errors -- Start
  // URL + Page + TX Errors --Starts
  for (i = 0; i < TOTAL_TX_ERR; i++) {
    create_errorcode_table_entry(&rnum);
    errorCodeTable[rnum].error_msg = -1;
  }

  // Session Errors
  for (i = 0; i < TOTAL_SESS_ERR; i++) {
    create_errorcode_table_entry(&rnum);
    errorCodeTable[rnum].error_msg = -1;
    if (sess_error_code_start_idx == -1) {
      sess_error_code_start_idx = rnum;
    }

    NSDL4_HTTP(NULL, NULL, "rnum = %d, sess_error_code_start_idx - %d", rnum, sess_error_code_start_idx);
  }

 
  // 2 - means Tx which includes all error codes execpt session
  if(run_cmd_and_fill_errorCodeTable(OBJ_TRANS_ID) == -1) 
    return -1;
  if(run_cmd_and_fill_errorCodeTable(OBJ_SESS_ID) == -1) 
    return -1;

  return 0;
}

void fill_error_code_gdf_grp_kw_enable(char **TwoD, int overall_num_entries, int start_num_entry)
{
  int i, j, k, Idx2d = 0;
  char prefix[1024] = {0};
  char buff[1024] = {0};

  NSDL1_HTTP(NULL, NULL, "Method Called, overall_num_entries = %d, start_num_entry - %d", overall_num_entries, start_num_entry);

  for(k = 0; k < (sgrp_used_genrator_entries + 1); k++)
  {
    for(j = 0; j < TOTAL_GRP_ENTERIES_WITH_GRP_KW; j++)
    {
      strcpy(prefix, "");
      getNCPrefix(prefix, k-1, j-1, ">", SHOW_GRP_DATA); //for controller or NS newly added
      sprintf(buff, "%sAll", prefix);
      fprintf(write_gdf_fp, "%s\n", buff);
      fill_2d(TwoD, Idx2d, buff);
      Idx2d++;
        
      NSDL2_HTTP(NULL, NULL, "prefix = %s, j = %d, k = %d, buff = %s, Idx2d = %d, errorcode_table_shr_mem = %p", prefix, j, k, buff, Idx2d, errorcode_table_shr_mem);	
      if( errorcode_table_shr_mem == NULL ){
        for(i = start_num_entry; i < overall_num_entries; i++){
          if(errorCodeTable[i].error_msg != -1){
            if(strncmp(RETRIEVE_BUFFER_DATA(errorCodeTable[i].error_msg), "Undef", 5)){
              //NSDL2_HTTP(NULL, NULL, "arg = %d", arg);
              sprintf(buff, "%s%s", prefix, RETRIEVE_BUFFER_DATA(errorCodeTable[i].error_msg));
              fprintf(write_gdf_fp, "%s\n", buff);
              fill_2d(TwoD, Idx2d, buff);
              Idx2d++;
            }else{
            //continue;
            }
          } //end for
        }
      } 
      else{ //get from shared
        for(i = start_num_entry; i < overall_num_entries; i++){
          if(strncmp((errorcode_table_shr_mem[i].error_msg), "Undef", 5)){
            sprintf(buff, "%s%s", prefix, errorcode_table_shr_mem[i].error_msg);
            fprintf(write_gdf_fp, "%s\n", buff);
            fill_2d(TwoD, Idx2d, buff);
            Idx2d++;
            NSDL2_HTTP(NULL, NULL, "i = %d, overall_num_entries = %d, buff = %s, Idx2d = %d, prefix = %s", i, overall_num_entries, buff, Idx2d, prefix);
          }else{
              //continue;
          }
        } //end for
      }
    }
  }
}

void fill_error_code_gdf_grp_kw_disabled(char **TwoD, int overall_num_entries, int start_num_entry)
{
  int k, i, Idx2d = 0;
  char prefix[1024] = {0};
  char buff[1024] = {0};

  NSDL1_HTTP(NULL, NULL, "Method Called, overall_num_entries = %d, start_num_entry - %d", overall_num_entries, start_num_entry);

  for(k = 0; k < (sgrp_used_genrator_entries + 1); k++)
  {
    strcpy(prefix, "");
    getNCPrefix(prefix, k-1, -1, ">", 0);

    sprintf(buff, "%sAll", prefix);
    fprintf(write_gdf_fp, "%s\n", buff);
    fill_2d(TwoD, Idx2d, buff);
    Idx2d++;

    if(errorcode_table_shr_mem == NULL ) {
      for(i = start_num_entry; i < overall_num_entries; i++) {
        if(errorCodeTable[i].error_msg != -1) {
          if(strncmp(RETRIEVE_BUFFER_DATA(errorCodeTable[i].error_msg), "Undef", 5)){
            //NSDL2_HTTP(NULL, NULL, "arg = %d", arg);
            sprintf(buff, "%s%s", prefix, RETRIEVE_BUFFER_DATA(errorCodeTable[i].error_msg));
            fprintf(write_gdf_fp, "%s\n", buff);
            fill_2d(TwoD, Idx2d, buff);
            Idx2d++;
          } else {
            //continue;
          }
        } //end for
      }
    } 
    //get from shared memory
    else 
    {  
      for(i = start_num_entry; i < overall_num_entries; i++) {
        if(strncmp((errorcode_table_shr_mem[i].error_msg), "Undef", 5)) {
          sprintf(buff, "%s%s", prefix, errorcode_table_shr_mem[i].error_msg);
          fprintf(write_gdf_fp, "%s\n", buff);
          fill_2d(TwoD, Idx2d, buff);
          Idx2d++;
        } else {
          //continue;
        }
      } //end for
    }
  }
}
 
char **get_error_codes_ex(int arg, int num_err) {

  NSDL2_HTTP(NULL, NULL, "arg = %d", arg);
  //int actual_num_err_entries = 0;
  int overall_num_entries = 0;
  int start_num_entry = 0;
  char **TwoD = NULL;
  //int i = 0;

  NSDL1_HTTP(NULL, NULL, "Method call, TOTAL_ENTERIES = %d, TOTAL_GRP_ENTERIES_WITH_GRP_KW = %d, sgrp_used_genrator_entries = %d", TOTAL_ENTERIES, TOTAL_GRP_ENTERIES_WITH_GRP_KW, sgrp_used_genrator_entries);

  switch(arg){
    case 0:
      start_num_entry = 1;
      overall_num_entries = TOTAL_USED_URL_ERR;
    break;

    case 1:
      start_num_entry = 1;
      overall_num_entries = TOTAL_USED_PAGE_ERR;  
    break;

    case 2:
      start_num_entry = 1;
      overall_num_entries = TOTAL_TX_ERR;  
    break;

    case 3:
      start_num_entry = sess_error_code_start_idx + 1;
      //i = sess_error_code_start_idx + 1; 
      overall_num_entries = start_num_entry + TOTAL_SESS_ERR - 1;
      //overall_num_entries = TOTAL_SESS_ERR;
    break;

    default:
     NS_EXIT(-1, "Error: Unknown argument for getting error codes\n");
      
  }
  NSDL2_HTTP(NULL, NULL, "num_err = %d overall_num_entries = %d", num_err, overall_num_entries);	
  MY_MALLOC (TwoD, (sizeof(char**) * num_err), "mallocd 2D array for error codes", -1);
 
  //if(SHOW_GRP_DATA_KW_FLAG && !(loader_opcode == CLIENT_LOADER))
  if(SHOW_GRP_DATA)
    fill_error_code_gdf_grp_kw_enable(TwoD, overall_num_entries, start_num_entry);
  else 
    fill_error_code_gdf_grp_kw_disabled(TwoD, overall_num_entries, start_num_entry);

  return TwoD;
}


char *get_error_code_name(int error_code)
{

  NSDL2_HTTP(NULL, NULL, "error code  = %d", error_code) ;
  
 if(error_code >= 0)
  return(errorcode_table_shr_mem[tx_error_code_start_idx + error_code].error_msg);
 else
  return("Invalid Error Code");
}

char *get_session_error_code_name(int error_code)
{
  NSDL2_HTTP(NULL, NULL, "error code  = %d", error_code) ;

 if(error_code >= 0)
  return(errorcode_table_shr_mem[sess_error_code_start_idx + error_code].error_msg);
 else
  return("Invalid Error Code");
}
