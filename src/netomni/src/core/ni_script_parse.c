/********************************************************************************
 * File Name            : ni_script_parse.c 
 * Author(s)            : Manpreet Kaur
 * Date                 : 17 June 2014
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains script parsing functions 
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/
#define _GNU_SOURCE //Added for O_LARGEFILE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <malloc.h>
#include <string.h>
#include <stdarg.h> 
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <libgen.h>

#include "ni_user_distribution.h"
#include "ni_scenario_distribution.h"
#include "nslb_parse_api.h"
#include "nslb_static_var_use_once.h"
#include "nslb_util.h"
#include "ni_script_parse.h"
#include "ni_sql_vars.h"
#include "../../../ns_exit.h"
#include "../../../ns_error_msg.h"

#define APPEND_MODE              1

//Script table
int total_script_entries = 0;
int max_script_entries = 0;
//API table
int max_api_entries = 0;
int total_api_entries = 0;
//Offset int array per API
int max_entries = 0;
int total_entries = 0;

ScriptTable* script_table = NULL;
APITableEntry* api_table = NULL;
PerGenAPITable* per_gen_api_table = NULL;
PerGenAPITable* per_gen_api_table_unique_var = NULL;
//unique_range_var table
UniqueRangeTableEntry *uniquerangeTable = NULL;
#ifndef CAV_MAIN
extern int  script_ln_no;
extern char *flow_filename;
#else
extern __thread int script_ln_no;
extern __thread char *flow_filename;
#endif
int max_unique_range_entries = 0;
int total_unique_range_entries = 0;

/*Static buffer for data file */
static char *data_file_buf = NULL;
int malloced_size = 0;
//following variable is for SCRIPT COMMENT used for block comment found or not, used in different fn so made static 
int g_cmt_found;
gen_name_quantity_list *num_script_user_per_gen = NULL;
extern int validate_and_copy_datadir(int mode, char *value, char *dest, char *err_msg);

#define OFFSET_VALUE 10000
#define IGNORE_COMMENTS(ptr) {\
                               if (!g_cmt_found && !strncmp(ptr, "//", 2))\
                               {\
                                 NIDL (1, "Ignoring comment = %s", ptr);\
                                 continue;\
                               }\
                               else {\
                                if (!strncmp(ptr, "/*", 2))\
                                  g_cmt_found = 1;\
                                if (g_cmt_found && strstr(ptr, "*/") && strncmp(ptr, "BODY", 4) && strncmp(ptr, "Cookie", 6) && strncmp(ptr, "URL", 3))\
                                {\
                                  g_cmt_found = 0;\
                                  NIDL (1, "Ignoring comment = %s", ptr);\
                                  continue;\
                                }\
                                if (g_cmt_found)\
                                {\
                                  NIDL (1, "Ignoring comment = %s", ptr);\
                                  continue;\
                                }\
                               }\
                             }  


#define CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}

#define IGNORE 1
#define NOT_IGNORE 0
#define NI_SET_IGNORE_LINE_FLAG(line) \
{ \
  char *tmp_ptr = &line[0]; \
 \
  if(*tmp_ptr == '\0' || *tmp_ptr == '\n') {\
    ignore_line_flag = IGNORE; \
    num_line_ignore ++; \
  }\
  else if (*tmp_ptr == '\t' || *tmp_ptr == ' ') \
  { \
    CLEAR_WHITE_SPACE(tmp_ptr); \
    if(*tmp_ptr == '\0' || *tmp_ptr == '\n') {\
      ignore_line_flag = IGNORE; \
      num_line_ignore ++; \
    }\
  } \
 \
  else \
    ignore_line_flag = NOT_IGNORE; \
} 


#define ABORT_USEONCE_PAGE       0
#define ABORT_USEONCE_SESSION    1
#define ABORT_USEONCE_TEST       2
#define ABORT_USEONCE_USER       3

/*Find script index*/
int find_script_idx(char* name) 
{
  int i;
  char script_name_with_proj_subproj[2 * 1024];

  NIDL (1, "Method called, name = %s, total_sess_entries = %d", name, total_script_entries);
  for (i = 0; i < total_script_entries; i++) {
    sprintf(script_name_with_proj_subproj, "%s/%s/%s", script_table[i].proj_name, script_table[i].sub_proj_name, 
                                                     script_table[i].script_name);
    //if (!(strcmp(script_table[i].script_name, name)))
    if (!(strcmp(script_name_with_proj_subproj, name)))
      return i;
  }
  return -1;
}

/****************************************************************************
 * Description          : Create script table entries
 * Input-Parameter      :
 * script_name          : used for storing script name
 * script_full_name     : used for storing script name, project and sub-project name in script structure
 * Output-Parameter     : Set total_generator_list_entries increment as per entries
 * Return               : Return -1 if allocation fails else return 0 on success
 *****************************************************************************/

int create_script_table(char* script_full_name, char *err_msg)
{
  char *fields[10];

  NIDL (1, "Method called, total_used_generator_list_entries = %d", total_script_entries);
  NIDL (1, "create_script_table script_full_name - %s", script_full_name);

  if (total_script_entries == max_script_entries) {
    script_table = (ScriptTable *)realloc(script_table, (max_script_entries + DELTA_SCRIPT_ENTRIES) * sizeof(ScriptTable));
    if (!script_table) {
      sprintf(err_msg, "Error allocating more memory for script_table entries");
      return FAILURE_EXIT;
    } else max_script_entries += DELTA_SCRIPT_ENTRIES;
  }
  if(strchr(script_full_name, '/'))
  {
    get_tokens(script_full_name, fields, "/", 10);

    //script_table[total_script_entries].proj_name = (char *)malloc(strlen(fields[0]) + 1);
    NSLB_MALLOC(script_table[total_script_entries].proj_name, (strlen(fields[0]) + 1), "script proj name", total_script_entries, NULL);
    strcpy(script_table[total_script_entries].proj_name, fields[0]);

    //script_table[total_script_entries].sub_proj_name = (char *)malloc(strlen(fields[1]) + 1);
    NSLB_MALLOC(script_table[total_script_entries].sub_proj_name, (strlen(fields[1]) + 1), "script subproj name", total_script_entries, NULL);
    strcpy(script_table[total_script_entries].sub_proj_name, fields[1]);

    //script_table[total_script_entries].script_name = (char *)malloc(strlen(fields[2]) + 1);
    NSLB_MALLOC(script_table[total_script_entries].script_name, (strlen(fields[2]) + 1), "script_name", total_script_entries, NULL);
    strcpy(script_table[total_script_entries].script_name, fields[2]);
  }
  else
  {
    //script_table[total_script_entries].script_name = (char *)malloc(strlen(script_full_name) + 1);
    NSLB_MALLOC(script_table[total_script_entries].script_name, (strlen(script_full_name)), "script_name", total_script_entries, NULL);
    strcpy(script_table[total_script_entries].script_name, script_full_name);
  }

  //BugId:65495 : Showing wrong script name in case of less data lines present in USE_ONCE file parameter.
  script_table[total_script_entries].sgroup_num = -1;
  total_script_entries++;
  return SUCCESS_EXIT;
}

/****************************************************************************
 * Description          : Function used to find script type 
 *			  (Legacy, C-type, JAVA) and version
 *                        In script folder ".script.type" file which provides 
 *                        script details:
 *                              SCRIPT_TYPE=C
 *                              SCRIPT_VERSION=1.0 
                          If .script.type file is not present then search 
 *                        script.capture and set type to legacy mode
 * Input-Parameter      :
 * path		        : Absolute script file path
 * script_type		: Fill script type  
 * version 	 	: Fill script version
 * Output-Parameter     : 
 * Return               : Return 0 on success
 *****************************************************************************/

int ni_get_script_type(char *path, int *script_type, char *version)
{
  FILE *fp;
  char *field[8];
  char scripttype_fname[MAX_LINE_LENGTH + 1];
  char cap_fname[MAX_LINE_LENGTH + 1];
  char read_line[MAX_LINE_LENGTH + 1];
  int line_count = 0;
  int num_toks;

  NIDL (1, "Method Called, path = [%s]", path);

  *script_type = SCRIPT_TYPE_LEGACY; // default

  sprintf(scripttype_fname , "%s/.script.type", path);
  if((fp = fopen(scripttype_fname, "r")) != NULL)
  {
    while(nslb_fgets(read_line, sizeof(read_line), fp, 1) != NULL)
    {
      NIDL (2, "line = [%s]", read_line);
      read_line[strlen(read_line) - 1] = '\0';
      NIDL (2, "line = [%s]", read_line);
     
      // Ignore Empty & commented lines
      if(read_line[0] == '#' || read_line[0] == '\0') {
        NIDL (2, "Commented/Empty line continuing..");
        continue;
      }

      num_toks = get_tokens_(read_line, field, "=", 8);
      NIDL (2, "num_toks = %d", num_toks);

      if(num_toks < 2) {
       NIDL (2, "num_toks < 2, continuing...");
       // No keyword value -- Give warning
       continue;
      }

      NIDL (2, "field[0] = [%s], field[1] = [%s]", field[0], field[1]);
      // field[0] is keyword & field[1] has its value
      if(strcasecmp(field[0], "SCRIPT_TYPE") == 0)
      {
        if(strcasecmp(field[1], "C") == 0)
         *script_type = SCRIPT_TYPE_C;
        else if(strcasecmp(field[1], "JAVA") == 0)
         *script_type = SCRIPT_TYPE_JAVA;
        else
         *script_type = SCRIPT_TYPE_LEGACY;
        line_count++;
      }

      if(strcasecmp(field[0], "SCRIPT_VERSION") == 0) {
        strcpy(version, field[1]);
      }

    }
    fclose(fp);
  }
  else
  {
    //Auto detect script type if .script.type file is not present If script.capture is found set it to legacy mode
  char *tmp_ptr = cap_fname;

    sprintf(tmp_ptr, "%s/script.capture", path);

    if(!access(tmp_ptr, R_OK))
      *script_type = SCRIPT_TYPE_LEGACY;
    else{
      sprintf(tmp_ptr, "%s/runlogic.java", path);
      if(!access(tmp_ptr, R_OK))
        *script_type = SCRIPT_TYPE_JAVA;
      else
        *script_type = SCRIPT_TYPE_C;
    }
  }
  return SUCCESS_EXIT;
}

/* 
   Success(If generator alive) - return 1
   Failure(If generator not alive) - return 0  
*/
static int is_generator_alive(int gen_id)
{
   NIDL(2, "Method called, gen_id = %d", gen_id);

  if(!(generator_entry[gen_id].flags & IS_GEN_INACTIVE))
    return 1;

  return 0;
}

static char * scan_memory_line_by_line (int gen_id, int api_id, char *line, int line_number, FILE *gen_data_fp, int total_num_records, int need_to_ret_ptr)
{
  char *tmp_ptr = NULL, *cur_ptr = NULL, *start_ptr = NULL;
  int ignore_line_flag, num_line_ignore;

  NIDL(1, "Method called, total_num_records = %d, line_number = %d, need_to_ret_ptr = %d, line = %p", 
           total_num_records, line_number, need_to_ret_ptr, line);

  total_num_records += line_number; 

  /*Save address*/
  start_ptr = line;
  while ((tmp_ptr = strpbrk(line, "\r\n")) != NULL) 
  {
    cur_ptr = line;
    line_number++; //Increment line number 

    /* Here we are dealing with dos and linux file format both.
      Since dos file end with \r\n and linux file end with \n so pointing tmp_ptr as requirment*/
    if (*tmp_ptr == '\r')
    {
      tmp_ptr += 2;   //skip \r \n
    }
    if (*tmp_ptr == '\n')
    {
      tmp_ptr++;
    }
    if (line_number < api_table[api_id].first_data_line) {
      line = tmp_ptr;
      continue;
    } 
        
    /*Ignore blank and new lines*/
    NI_SET_IGNORE_LINE_FLAG(line);
    if (ignore_line_flag == 1)
    {
      NIDL (3, "Ignoring blank line from data file");
      ignore_line_flag = 0;
      line = tmp_ptr;
      line_number--;
      continue;
    }
    /*If line number matches starting index of data file then save address*/
    if (line_number == (api_table[api_id].first_data_line + 
                   per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val)) 
    { 
      NIDL(3, "Set start_ptr.");
      start_ptr = cur_ptr;
    }
              
    NIDL(3, "line_number = %d, total_num_records = %d", line_number, total_num_records);
    /*If line number matches total number of records specified for a file then write complete data from memory to file */ 
    if (line_number == total_num_records) { 
      if (need_to_ret_ptr) {
        return (tmp_ptr);
      } else 
        NIDL(3, "Write data into file.");
        fwrite(start_ptr, sizeof(char), (tmp_ptr - start_ptr), gen_data_fp);  
      break;
    } 
    line = tmp_ptr; //continue reading memory
  }
  return (NULL);
}

//This function will take buckup or make tar
#define NI_OPT_DATA_BCK  0
#define NI_OPT_MAKE_TAR  1
static int ni_rtc_make_tar(int gen_id, int opt)
{
  char cmd[4096 + 1];
  char abs_dir_name[2048 + 1];
  char *dir_name = "rtc"; 
  time_t tloc;
  struct tm *tm = NULL, tm_struct;
  char time_buf[26];
  struct stat st;
  int return_value;

  NIDL(2, "Method called, gen_id = [%d], opt = %d", gen_id, opt);

  sprintf(abs_dir_name, "%s/%s/%s", controller_dir, generator_entry[gen_id].gen_name, dir_name);

  NIDL(2, "dir_name = [%s], abs_dir_name = [%s]", dir_name, abs_dir_name);  

  //Check dir for file parameter data file exist or not?
  if((stat(abs_dir_name, &st) == -1) || !S_ISDIR(st.st_mode))
  {
    NIDL (3, "dir '%s' not exist.", abs_dir_name);
    return -1;
  }

  if(opt == NI_OPT_DATA_BCK)
  {
    time(&tloc);
    if((tm = nslb_localtime(&tloc, &tm_struct, 1)) == NULL)
      sprintf(time_buf, "%s", "bck");
    else
      strftime(time_buf, 26, "%Y%m%d%H%M%S", tm); //YYYYmmddhhmmss
    NIDL (3, "time_buf = [%s]", time_buf);

    //Make backup of old data file
    sprintf(cmd, "mv %s %s_%s >/dev/null", abs_dir_name, abs_dir_name, time_buf); 
    NIDL (3, "take backup of old data dir by running command - [%s]", cmd);
    
    return_value = system(cmd);
    if(WEXITSTATUS(return_value) == 1)
    {
      fprintf(stderr, "Error: ni_rtc_make_tar() - failed to make data file backup '%s'\n", abs_dir_name); 
      return -1;
    }
  }
  else
  {
    sprintf(cmd, "cd %s >/dev/null; tar -I%s/thirdparty/bin/lz4 -cf %s/nc_%s_rtc.tar.lz4 * >/dev/null", 
                  abs_dir_name, work_dir, controller_dir, generator_entry[gen_id].gen_name);
    NIDL (3, "making tar of data file by running command - [%s]", cmd);
    
    return_value = system(cmd);
    if(WEXITSTATUS(return_value) == 1)
    {
      fprintf(stderr, "Error: ni_rtc_make_tar() - faild to make tar file nc_%s_rtc.tar.gz on path %s", 
                       generator_entry[gen_id].gen_name, abs_dir_name);
      return -1;
    }
  }

  return 0;
}

/* Description:
   a) On the basis of distribution 
   a) Creates data files for each generator with their absolute path          
   These data files will be created with absolute path in respective generator directories 
   under TRXX/.controller folder
 */
static int divide_data_file_values(int runtime)
{
  FILE *gen_data_fp = NULL;
  char *line = NULL, *start_ptr = NULL, *start_ptr_save = NULL; 
  char gen_data_file_path[FILE_PATH_SIZE] = {0}, cmd[4096] = {0};
  int api_id, gen_id, line_number = 0;
  int total_records, start_offset_idx, start_line_remainder, end_offset_idx;
  int end_line_remainder;
  char data_dir_path[FILE_PATH_SIZE];
  char data_dir_name[FILE_PATH_SIZE];
  char *tmp;
  char *tmp_file_name = NULL;

  NIDL(1, "Method called. total api entries = %d, total generators = %d, runtime = %d", 
                          total_api_entries, sgrp_used_genrator_entries, runtime);

  int data_dir_path_len = sprintf(data_dir_path, "%s/data/", GET_NS_TA_DIR()); 

  for (gen_id = 0; gen_id < sgrp_used_genrator_entries; gen_id++) 
  {
    NIDL (3, "controller_dir = %s, gen_name = %s", controller_dir, generator_entry[gen_id].gen_name);
    
    if((runtime && (!is_generator_alive(gen_id))))
      continue;
 
    if(runtime)
      ni_rtc_make_tar(gen_id, NI_OPT_DATA_BCK);

    for (api_id = 0; api_id < total_api_entries; api_id++) 
    {
      NIDL (4, "API Details: Gen Index = %d, API Index = %d, Data file = %s, Sequence = %d, controller_dir = [%s], gen_name = [%s], "
               "netomni_proj_subproj_file = %s, data_file_path = [%s], abs_or_relative_data_file_path = %d", 
               gen_id, api_id, api_table[api_id].data_fname, api_table[api_id].sequence, controller_dir, generator_entry[gen_id].gen_name, 
               netomni_proj_subproj_file, api_table[api_id].data_file_path, api_table[api_id].abs_or_relative_data_file_path);

      //Divide file value in case of useonce and unique
      if(runtime || ((api_table[api_id].sequence == UNIQUE) || (api_table[api_id].sequence == USEONCE)))
      {
        if(runtime && (api_table[api_id].rtc_flag == -1))
          continue;

        /* Create data file path in Generator directory and open file in append mode 
           If data file resides in script folder then create new directory data_files 
           in generator directory(<Controller dir>/TRxx/.controller/GenName/)   
        */
        if (api_table[api_id].abs_or_relative_data_file_path == 0) {
          if(!runtime)
          {
            //Bug-101320 
            if(strncmp(api_table[api_id].data_fname, data_dir_path, data_dir_path_len)) 
            {
              sprintf(gen_data_file_path, "%s/%s/rel/%s/%s/scripts/%s", 
                     controller_dir, generator_entry[gen_id].gen_name,
                     script_table[api_table[api_id].script_idx].proj_name, script_table[api_table[api_id].script_idx].sub_proj_name,
                     script_table[api_table[api_id].script_idx].script_name);
            }
            else
            {
              //DATADIR from only script 
              strcpy(data_dir_name, api_table[api_id].data_fname + data_dir_path_len);
              tmp = strrchr(data_dir_name, '/');
              *tmp = '\0';
              if(api_table[api_id].data_dir_idx != -1)
              {
                char file_dir_name[1024+1];
                tmp = strchr(data_dir_name, '/');
                if(tmp)
                  strcpy(file_dir_name, tmp);
                else
                  strcpy(file_dir_name, "");
                strcpy(data_dir_name, g_data_dir_table[api_table[api_id].data_dir_idx]);
                strcat(data_dir_name, file_dir_name);
              }
              //sprintf(data_dir_file_path, "%s/rel/data/%s", controller_dir, data_dir_name);
              sprintf(gen_data_file_path, "%s/%s/rel/data/%s", controller_dir, generator_entry[gen_id].gen_name, data_dir_name);
            }
          }
          else
            sprintf(gen_data_file_path, "%s/%s/rtc/%s/%s/scripts/%s", controller_dir, generator_entry[gen_id].gen_name,
                     script_table[api_table[api_id].script_idx].proj_name, script_table[api_table[api_id].script_idx].sub_proj_name,
                     script_table[api_table[api_id].script_idx].script_name);
        } 
        else 
        { 
          if(!runtime)
            sprintf(gen_data_file_path, "%s/%s/abs/%s", controller_dir, generator_entry[gen_id].gen_name,
                    MAKE_ABS_TO_REL(api_table[api_id].data_file_path));
          else
            sprintf(gen_data_file_path, "%s/%s/rtc/%s", controller_dir, generator_entry[gen_id].gen_name,
                    api_table[api_id].data_file_path);
        }

        sprintf(cmd, "mkdir -p %s", gen_data_file_path);

        int ret = system(cmd);

        if(WEXITSTATUS(ret) == 1) {
          if(!runtime){
            NS_EXIT(-1, "Failed to create generator wise data file in directory %s", gen_data_file_path);
          }
          else{
            fprintf(stderr, "\nError in creating data file directory %s\n", gen_data_file_path);
            return -1;
          }
        }

        tmp_file_name = strrchr(api_table[api_id].data_fname, '/');
        strcat(gen_data_file_path, tmp_file_name);
        if (runtime)
          strcat(gen_data_file_path, "_rtc");

        NIDL (1, "gen_data_file_path = %s", gen_data_file_path);
       
        //Open file in write mode
        if((gen_data_fp = fopen(gen_data_file_path, "w")) == NULL)
        {
          if(!runtime){
            NS_EXIT(-1, "Data file (%s) is not present, error:%s", gen_data_file_path, nslb_strerror(errno));
          }
          else{
            fprintf(stderr, "\nError: divide_data_file_values() -Data file (%s) does not exist. Hence exiting.\n", gen_data_file_path);
            return -1;
          }
        }

        if(!runtime) 
          write_log_file(NS_GEN_VALIDATION, "Dividing script's %s file parameter values among generators",
                        (api_table[api_id].sequence == UNIQUE?"Unique":"Use-Once"));
        NIDL(4, "Write data file from start index = %d to end index = %d", 
                  per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val, 
                  per_gen_api_table[(gen_id * total_api_entries) + api_id].used_val);

        /*Reset line number and start pointer*/
        line_number = 0;
        start_ptr = NULL; 

        /*Starting memory address require to copy header lines into data file*/
        line = api_table[api_id].data_file_buf; 

        /*Number of records need to be dump for each data file*/
        total_records = (per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val +
                       per_gen_api_table[(gen_id * total_api_entries) + api_id].num_val);

        NIDL(4, "first_data_line = %d, total_offset = %d, total_records = %d", 
                 api_table[api_id].first_data_line, api_table[api_id].total_offset, total_records);

        /*Add header lines into each data files*/
        if (api_table[api_id].first_data_line > 1)                                   
          fwrite(line, sizeof(char), (api_table[api_id].end_head_ptr - line), gen_data_fp);

        if (api_table[api_id].total_offset != 0) 
        {
          /* If "start value" is non zero then find index in offset_entry_table and number of records
             need to dump  
                         Index   Records
            For example: [0]     10000
                         [1]     20000
                         [3]     30000  offset_entry_table.
            If start_val = 27546 , 27546/10000 = (2 - 1) then index in offset table will be 1, 
            this helps in finding starting point in memory 
            Find number of records which one need to dump in data file, hence calculate remainder 
          */
          if (per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val != 0) 
          {
            start_offset_idx = (per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val / OFFSET_VALUE) - 1; 
            start_line_remainder = per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val % OFFSET_VALUE; 
            NIDL (4, "Calculate starting offset = %d and remainder = %d", start_offset_idx, start_line_remainder);
          } 
          /* In order to find end pointer in memory, find index in offset_entry_table and number of records
             need to dump*/
          end_offset_idx = (per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val + 
                               per_gen_api_table[(gen_id * total_api_entries) + api_id].num_val) / OFFSET_VALUE - 1;   
          end_line_remainder = (per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val + 
                                 per_gen_api_table[(gen_id * total_api_entries) + api_id].num_val) % OFFSET_VALUE;
          NIDL (4, "Calculate end pointer offset = %d and remainder = %d", end_offset_idx, end_line_remainder);

          /* (START == END) OFFSET INDEX: 
             There could be a condition where both start and end offset index are same, 
             Possibilities: 
               1) Last record to dump is less than 10K, then index will be -1
               2) Both the indexs lie in same offset index in offset_entry_table
           */

          /* Case 1) Dividing data file starting with first_data_line record*/           
          if (per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val == 0) 
          {
            /* a) When last record to dump < 10K
               Consider a eg start_val = 0 but last_record = 9799 
               Here last record is less than 10K therefore we need to scan and dump memory from line 0 to 9799*/
            if (end_offset_idx == -1)
            {
              /*Here if header lines exists then we need to dump from first data line else from starting address of memory*/
              start_ptr = (api_table[api_id].end_head_ptr)?api_table[api_id].end_head_ptr:line;
              line_number = (api_table[api_id].first_data_line > 1)?(api_table[api_id].first_data_line - 1):0;
              scan_memory_line_by_line (gen_id, api_id, start_ptr, line_number, gen_data_fp, total_records, 0);
            } 
            /* b) When Last record to dump is greater than 10K 
               Here we have two situtation:
               1. If last record is a multiple of 10K then calculated index will be used to dump lines in file
                  e.g. start_val = 0, last_record = 50000
               2. If end_line_remainder exists then dump remaining data lines
                  e.g. start_val = 0, last_record = 51000, data till 50K has been dump next dump data from line number 50001 to 51000
            */
            else 
            { 
              start_ptr = (api_table[api_id].end_head_ptr == NULL)?line:api_table[api_id].end_head_ptr;        
              fwrite(start_ptr, sizeof(char), (api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array - start_ptr), gen_data_fp);
              if (end_line_remainder)
              {
                line = api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array;
                line_number = api_table[api_id].offset_table_entry[end_offset_idx].line_num;
                scan_memory_line_by_line (gen_id, api_id, line, line_number, gen_data_fp, end_line_remainder, 0);               
              }                  
            } 
          }
          /*Case 2) Dividing data files among generator*/
          else 
          {
            /* a) start_val = 10000 last_record = 20000, here both remainders are 0 hence dump data with respect to calculated index*/
            if (!start_line_remainder && !end_line_remainder) 
              fwrite(api_table[api_id].offset_table_entry[start_offset_idx].offset_end_ptr_array, sizeof(char), (api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array - api_table[api_id].offset_table_entry[start_offset_idx].offset_end_ptr_array), gen_data_fp); 
            /* Consider an example, need to dump data records from line number start_val = 19799 and last_record = 30200
               In data file records will be written as follow:
                 Steps 1) Scan 9799 records line by line (10000 - 19799) returns pointer
                       2) Write data in file starting record 19799 to 30000  
                       3) Scan 200 records line by line from starting pointer pointing to (30000 lines) and dump data in data file
             */
            else 
            { 
              if (start_line_remainder) 
              {
                /* a) start_value = 9799 last_record = 20000, here start record is less than 10K*/
                if (start_offset_idx == -1) {
                  line = (api_table[api_id].end_head_ptr)?api_table[api_id].end_head_ptr:line;
                  line_number = (api_table[api_id].first_data_line > 1)?(api_table[api_id].first_data_line - 1):0;
                } 
                /* b) start_val = 156784 last_record = 166583, here start record lie within offset table entries*/
                else {
                  line = api_table[api_id].offset_table_entry[start_offset_idx].offset_end_ptr_array;
                  line_number = api_table[api_id].offset_table_entry[start_offset_idx].line_num;
                }

                /*Find the starting pointer of memory and then dump data till last record offset*/ 
                start_ptr = scan_memory_line_by_line (gen_id, api_id, line, line_number, gen_data_fp, start_line_remainder, 1); 

                /* Need to save starting pointer of memory there might be case of (START == END) offset index*/
                start_ptr_save = start_ptr; 

                /*Only in case of mismatch of start and end offset index values, then dump data into file 
                  else we need to calulate end pointer
                  Case: start_value = 10457 and last record = 20457, therefore dump from 10457 to 20000.*/ 
                if (end_offset_idx != start_offset_idx) 
                { 
                  NIDL (4, "Going to write into file: start_ptr = %p, end_ptr = %p", start_ptr, 
                      api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array);
                  fwrite(start_ptr, sizeof(char), (api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array - start_ptr), gen_data_fp); 
                }
              } 
              if (end_line_remainder) 
              { 
                /*Both start and end value is less than 10K hence no offset index, 
                  therefore we need to calculate end pointer in memory
                  Case: start_val = 0    end_val = 1746
                        start_val = 1746 end_val =3492
                 */
                if ((end_offset_idx == -1) && (start_offset_idx == -1))
                {
                  line = start_ptr_save;//Saved start pointer in memory
                  line_number = per_gen_api_table[(gen_id * total_api_entries) + api_id].start_val;//Number of lines to be dump  
                  start_ptr = scan_memory_line_by_line (gen_id, api_id, line, line_number, gen_data_fp, per_gen_api_table[(gen_id * total_api_entries) + api_id].total_val, 1);
                } else {
                  /* start_val = 156784 last_record = 166583*/
                  line = api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array;
                  line_number = api_table[api_id].offset_table_entry[end_offset_idx].line_num;
                  start_ptr = scan_memory_line_by_line (gen_id, api_id, line, line_number, gen_data_fp, end_line_remainder, 1);
                }  
                NIDL (4, "Going to write into file: end ptr = %p, start ptr = %p", start_ptr, 
                        api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array);
                /*If both offset values are same then need to dump data from end pointer to save start pointer in memory
                  Two possible cases: 1) START = END = -1 
                                      2) Both having same offset_end_ptr_array 
                  Issue: Otherwise it will add extra data into file*/
                /* Fix bug#11437, where data file distributed on generator with 0 size and sometimes it will added somes extra data.
		   for e.g: total lines in a data file is 15000
                            gen1 : 5000
			    gen2 : 5000
			    gen3 : 10000 (it added extra or duplicate data) */
                if (end_offset_idx == start_offset_idx) {
                  if(api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array == api_table[api_id].offset_table_entry[start_offset_idx].offset_end_ptr_array && !start_line_remainder) {
                    fwrite(line, sizeof(char), (start_ptr - line), gen_data_fp);
                  }
                  else
                    fwrite(start_ptr_save, sizeof(char), (start_ptr - start_ptr_save), gen_data_fp);
                }/* If start value = 15000 and end value =  45000 */
                else if(!start_line_remainder && (end_offset_idx != start_offset_idx)){ //Resolve bug 13834
                  line = api_table[api_id].offset_table_entry[start_offset_idx].offset_end_ptr_array; 
                  line_number = api_table[api_id].offset_table_entry[end_offset_idx].line_num;
                  start_ptr = scan_memory_line_by_line (gen_id, api_id, api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array, line_number, gen_data_fp, end_line_remainder, 1);
                  fwrite(line, sizeof(char), (start_ptr - line), gen_data_fp); 
                }else{    
                  fwrite(api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array, sizeof(char), (start_ptr - api_table[api_id].offset_table_entry[end_offset_idx].offset_end_ptr_array), gen_data_fp);
                
              }              
            }
          } 
        }
       } 
        else {/*For data files having records less than 10K*/ 
          start_ptr = (api_table[api_id].end_head_ptr)?api_table[api_id].end_head_ptr:line;
          line_number = (api_table[api_id].first_data_line > 1)?(api_table[api_id].first_data_line - 1):0; 
          NIDL (4, "end_head_ptr = %p, line = %p, start_ptr = %p", api_table[api_id].end_head_ptr, line, start_ptr);
          scan_memory_line_by_line (gen_id, api_id, start_ptr, line_number, gen_data_fp, total_records, 0);
        }  
        /*Close data file for that particular generator*/ 
        fclose(gen_data_fp);
        gen_data_fp = NULL;
      }
    }  

    if(runtime)
      ni_rtc_make_tar(gen_id, NI_OPT_MAKE_TAR);
  }
  if(runtime){
    for (api_id = 0; api_id < total_api_entries; api_id++)
    {
     api_table[api_id].rtc_flag = -1;
    }
  }
  return 0;
}

/* Format eg: FILE=/home/cavisson/work3/File/file_name or FILE=file_name */
static int set_staticvar_file(char *value, char *staticvar_datadir, char *staticvar_file, int script_idx, int *state, int api_idx, int *file_done, char *script_file_msg, int *absolute_path_flag, int api_type)
{
  int i;
  int grp_idx = -1;
  int data_grp_idx = -1;
  struct stat s;
  int file_found = 0;

  NIDL (1, "Method Called, value = [%s], script_idx = %d, api_idx = %d, file_done = %d, script_file_msg = %s, api_type = %d", 
             value, script_idx, api_idx, *file_done, script_file_msg, api_type);

  if (*file_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "FILE", "File");
  }

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (api_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if(*value == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "FILE", "File");
  }

  if(*staticvar_datadir != '\0')
  {
    for (i = 0; i < total_sgrp_entries; i++) 
    {
      if(!strcmp(scen_grp_entry[i].sess_name, script_table[script_idx].script_name))
      {
        grp_idx = scen_grp_entry[i].group_num;
        break;
      }
    }
  }

  if(*value == '/') {  // absolute path given
    strcpy(staticvar_file, value);
    *absolute_path_flag = 1;
  }
  else
  {
    //DATADIR is fetched from scenario keyword G_DATADIR
    /* GET_NS_TA_DIR=$NS_WDIR/workspace/$WORKSPACE_NAME/$PROFILE_NAME/cavisson/ */
    if ((grp_idx != -1 && g_data_dir_table) && *(g_data_dir_table[grp_idx])) {
    sprintf(staticvar_file, "%s/data/%s/%s",
                           GET_NS_TA_DIR(),
                           g_data_dir_table[grp_idx],
                           value);
    if(stat(staticvar_file, &s) == 0) {
      NIDL(1, "File '%s' is present.", staticvar_file);
      file_found = 1;
      data_grp_idx = grp_idx;
      //*absolute_path_flag = 1;
    } else
        NIDL(1, "File '%s' is not present.", staticvar_file);
    }

    if ((file_found == 0) && (*staticvar_datadir != '\0'))
    {
      //DATADIR is fetched from file parameter API argument DATADIR
      /* GET_NS_TA_DIR=$NS_WDIR/workspace/$WORKSPACE_NAME/$PROFILE_NAME/cavisson/ */
      sprintf(staticvar_file, "%s/data/%s/%s",
                             GET_NS_TA_DIR(),
                             staticvar_datadir,
                             value);
      if(stat(staticvar_file, &s) == 0) {
        NIDL(1, "File '%s' is present.", staticvar_file);
        file_found = 1;
        //*absolute_path_flag = 1;
      } else
        NIDL(1, "File '%s' is not present.", staticvar_file);
    }

    if(file_found == 0)
      sprintf(staticvar_file, "%s/%s/%s/scripts/%s/%s",
                         GET_NS_TA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                         script_table[script_idx].script_name, value);
  }

 /* }else{
     // Create ABSOLUTE PATH
     //bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir

  
     sprintf(staticvar_file, "%s/%s/%s/scripts/%s/%s",
                         GET_NS_TA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                         script_table[script_idx].script_name, value);
  }*/
  
  *file_done = 1;
  *state = NI_ST_STAT_VAR_OPTIONS;

  NIDL (2, "File Name = %s", staticvar_file);
  NIDL (2, "End Of the function set_static_var_file");
  return data_grp_idx;
}

static int set_buffer_for_mode(char *value, char *buffer, int *state, int api_idx, char *script_file_msg, int *mode_done, int *mode)
{
  NIDL (1, "Method Called, value = [%s]", value);

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (api_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if (*mode_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "MODE", "File");
  }
  strcpy(buffer, value);

  NIDL (3, "After tokenized Mode Name = %s", buffer);

  if (!strcasecmp(buffer, "UNIQUE")) {
    *mode = UNIQUE;
    api_table[api_idx].sequence = UNIQUE;
  } else if (!strcasecmp(buffer, "USE_ONCE")) {
    *mode = USEONCE;
    api_table[api_idx].sequence = USEONCE;
  } else if (!strcasecmp(buffer, "RANDOM")) {
    *mode = RANDOM;
    api_table[api_idx].sequence = RANDOM;
  } else if (!strcasecmp(buffer, "SEQUENTIAL")) {
    *mode = SEQUENTIAL;
    api_table[api_idx].sequence = SEQUENTIAL;
  } else if (!strcasecmp(buffer, "UNIQUEWITHINGEN")) {//RBU: In release 3.9.7 a new mode has been added for Generator, this will be treated as UNIQUE across the NVMs, on controller treat mode as sequential.
    *mode = SEQUENTIAL;
    api_table[api_idx].sequence = SEQUENTIAL;
  } else if (!strcasecmp(buffer, "WEIGHTED_RANDOM")) {
    *mode = WEIGHTED;
    api_table[api_idx].sequence = WEIGHTED;
  }
  *mode_done = 1;
  *state = NI_ST_STAT_VAR_OPTIONS;

  NIDL (1, "End of the method : api_table[api_idx].sequence = [%d]", api_table[api_idx].sequence);
  return 0;
}

static int set_static_firstDataLine(char* value, char* firstdataline, int *state, int api_idx, char *script_file_msg)
{
  int i =0;
  NIDL (1, "Method Called, value = [%s]", value);
  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (api_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(firstdataline, value);
  NIDL (2, "After tokenized FirstDataLine = %s", firstdataline);

  for (i = 0; firstdataline[i]; i++)
  {
    if (!isdigit(firstdataline[i]))
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(value, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "FirstDataLine");
    }
  }

  return 0;
}
 
static int set_static_headerLine(char* value, char *headerLine, int *state, int api_idx, char *script_file_msg)
{
  int i = 0;

  NIDL (1, "Method Called, value = [%s]", value);
  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (api_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(headerLine, value);
  NIDL (2, "After Toknising headerLine = [%s]", headerLine);

  for (i = 0; headerLine[i]; i++)
  {
    if (!isdigit(headerLine[i]))
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(value, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "HeaderLine");
    }
  }

  NIDL (2, "Method End.");
  return 0;
}

static void store_offset_value(int api_id, int line_number, int offset_index, char *ptr)
{
  NIDL (1, "Method called");
  api_table[api_id].offset_table_entry = (OffSetTableEntry*)realloc(api_table[api_id].offset_table_entry, ((offset_index + 1) * sizeof(OffSetTableEntry)));

  api_table[api_id].offset_table_entry[offset_index].offset_end_ptr_array = ptr;
  api_table[api_id].offset_table_entry[offset_index].line_num = line_number;
  NIDL (1, "offset_index = %d, line_number = %d, end ptr = %p", 
         offset_index, api_table[api_id].offset_table_entry[offset_index].line_num, 
          api_table[api_id].offset_table_entry[offset_index].offset_end_ptr_array);
}

int create_api_table_entry(int *row_num) 
{
  NIDL (1, "Method called, row_num = %d, total_api_entries = %d, max_api_entries = %d", 
         *row_num, total_api_entries, max_api_entries);
  if (total_api_entries == max_api_entries) 
  {
    api_table = (APITableEntry*)realloc(api_table, (max_api_entries + DELTA_API_ENTRIES) * sizeof(APITableEntry));
    max_api_entries += DELTA_API_ENTRIES;
  }
  *row_num = total_api_entries++;
  api_table[*row_num].idx = *row_num;
  api_table[*row_num].num_vars = 0; //Added after valgrind reported bug
  api_table[*row_num].sequence = SEQUENTIAL; // This is mode
  api_table[*row_num].start_var_idx = -1;
  api_table[*row_num].rtc_flag = -1;
  api_table[*row_num].is_file = 0;
  return (SUCCESS_EXIT);
}

int ni_set_last_option(int *idx, int *state, int *need_to_create, char* script_file_msg, char* line_tok)
{
  NIDL (1, "Method Called, idx = %d, state =%d, need_to_create = %d", 
                           *idx, *state, *need_to_create);
  if (*state == NI_ST_STAT_VAR_OPTIONS) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, line_tok, "File");
  }
  
  if (*need_to_create) 
  {
    NIDL (2, "Creating API table entry for idx = %d", *idx);
    if (create_api_table_entry(idx) != SUCCESS_EXIT)
      return -1;
    *need_to_create = 0;
  }
  api_table[*idx].num_vars++;
  if (*state == NI_ST_STAT_VAR_NAME)
        *state = NI_ST_STAT_VAR_NAME_OR_OPTIONS;
  NIDL (2, "End of the method ni_set_last_option");
  return 0;
}
			
static int get_data_file_wth_full_path (char *in_file_name, char *out_file_name, int script_idx)
{
  int num;

  NIDL (1, "Method called in_file_name = [%s], out_file_name = [%s]", in_file_name, out_file_name);
  if (in_file_name[0] == '/') { //If absolute path name is given
    if (!strncmp(in_file_name, "/home/cavisson/", 15))
      num = snprintf(out_file_name, MAX_LINE_LENGTH, "%s", in_file_name);
    else{
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012120_ID, CAV_ERR_1012120_MSG, in_file_name);
    }
  } else {\
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    num = snprintf(out_file_name, MAX_LINE_LENGTH, "./%s/%s/%s/scripts/%s/%s",
                   GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                   script_table[script_idx].script_name, in_file_name);
  }
  return num;
}

static void get_str_mode(char *str_mode, char *req_ext, int mode)
{
  NIDL(1, "Method called, mode = %d", mode);

  switch(mode)
  {
    case 1: strcpy(str_mode, "SEQUENTIAL");
            strcpy(req_ext, ".seq");
            break;

    case 2: strcpy(str_mode, "RANDOM");
            strcpy(req_ext, ".ran");
            break;

    case 3: strcpy(str_mode, "WEIGHTED_RANDOM");
            strcpy(req_ext, ".wtr");
            break;

    case 4: strcpy(str_mode, "UNIQUE");
            strcpy(req_ext, ".unq");
            break;

    case 5: strcpy(str_mode, "USE_ONCE");
            strcpy(req_ext, ".use");
            break;

   default: NIDL(1, "Given mode %d is not a valid mode", mode);
  }

  NIDL(1, "str_mode = %s and req_ext = %s", str_mode, req_ext);
}

#define MAX_NUM_FILE 20
#define FILE_EXT_LEN 4

/* Return 0 -> Not Match or Error
          1 -> Match
          2 -> other
*/

static int is_file_ext_match_wth_mode(char *file, char *fext, int seq)
{
  char *sptr = NULL;
  int len;
  int match = 0;
  int num_files = 0;
  char *flist[MAX_NUM_FILE];
  len = strlen(file);
  char *tmp_file = NULL;
  int i;

  len = strlen(file);
  //tmp_file = (char*) malloc(len + 1);
  NSLB_MALLOC(tmp_file, (len + 1), "tokenized file list", -1, NULL);
  strcpy(tmp_file, file);
  len = 0;

  NIDL(1, "Method called, file = %s, seq = %d", file, seq);
  
  num_files = get_tokens(tmp_file, flist, ",", MAX_NUM_FILE);
  NIDL(1, "num_files = %d", num_files);
  
  for(i=0; i< num_files; i++)
  {
    sptr = strrchr(flist[i], '.');

    if(sptr)
     len = strlen(sptr); //get extension length
    else{
     //return 2;
     continue;
    }

    if((len > FILE_EXT_LEN) || (len < FILE_EXT_LEN))
    {
      NIDL(1, "Info: Length of file extension should be %d, but it is %d", FILE_EXT_LEN, len);
      //return 2;
      continue;
    }

    strncpy(fext, sptr, len);

    fext[len] = '\0';

    if(((strcmp(fext, ".seq")) && (strcmp(fext, ".unq")) && (strcmp(fext, ".use")) && (strcmp(fext, ".ran")) && (strcmp(fext, ".wtr"))))
    {
      NIDL(1, "File extension is %s . It is not a valid file extension", fext);
      //return 2;
      continue;
    }

    NIDL(1, "File extension is %s and mode is %d", fext, seq);
    if(((!strcmp(fext, ".seq")) && (seq == SEQUENTIAL)) ||
      ((!strcmp(fext, ".unq")) && (seq == UNIQUE))     ||
      ((!strcmp(fext, ".use")) && (seq == USEONCE))    ||
      ((!strcmp(fext, ".ran")) && (seq == RANDOM))     ||
      ((!strcmp(fext, ".wtr")) && (seq == WEIGHTED)))
       match = 1;

    NIDL(1, "match = %d", match);

    if(match == 0)
     return 0;

    match = 0;
  }
  return 1;
}

int ni_input_static_values(char* file_name, int api_idx, int script_idx, int runtime, char *data_file_buf_rtc, int is_sql_var, NI_SQLVarTableEntry* sqlVarTable, char *err_msg) 
{
  char path_name[MAX_LINE_LENGTH];
  //char line_buf[MAX_LINE_LENGTH];
  char *line_buf = NULL;
  char *line = NULL, *tmp;
  int line_number = 0, data_file_path_len;
  int num_values_per_var = 0;
  char msg_buf[MAX_LINE_LENGTH];
  int num;
  struct stat stat_st;
  long data_file_size = 0;
  int read_data_fd = 0;
  int num_line_ignore = 0;
  //char err_msg[1024];
  time_t now;
  NIDL(1, "Method called, file_name = %s, api_idx = %d, script_idx = %d, runtime = %d, data_file_buf_rtc = %p, is_sql_var = %d, sqlVarTable = %p",
          file_name, api_idx, script_idx, runtime, data_file_buf_rtc, is_sql_var, sqlVarTable);

  if(!runtime)
  {
    num = get_data_file_wth_full_path (file_name, path_name, script_idx);
 
    NIDL (4, "File with path = %s", path_name);
    sprintf(msg_buf, "ni_input_static_values() for file %s", path_name);
 
    if (num >= MAX_LINE_LENGTH) {
      path_name[MAX_LINE_LENGTH-1] = 0;
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012093_ID, CAV_ERR_1012093_MSG, num);
    }

    if(!is_sql_var)
    {
       /*Need to check if there is control file 
       or not.If yes then create new data file otherwise dont do any thing.*/
       if (api_table[api_idx].sequence == USEONCE)
         if (nslb_uo_create_data_file_frm_last_file(path_name, (api_table[api_idx].first_data_line - 1), err_msg, api_idx) == -2)
           fprintf(stderr, "%s", err_msg);
       /*Finding the size of data file */
       if(stat(path_name, &stat_st) == -1)
       {
         NIDL (2, "File %s does not exists\n", path_name);
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000016 + CAV_ERR_HDR_LEN, path_name);
       }
       else
       {
         if(stat_st.st_size == 0){
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, path_name);
         }
       }
       api_table[api_idx].data_file_size = data_file_size = stat_st.st_size;
 
       if ((read_data_fd = open(path_name, O_RDONLY | O_LARGEFILE | O_CLOEXEC)) < 0){
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000006 + CAV_ERR_HDR_LEN, path_name, errno, nslb_strerror(errno));
       }
    }

    //api_table[api_idx].data_fname = (char*)malloc(sizeof(char) * (strlen(path_name) + 1));
    NSLB_MALLOC(api_table[api_idx].data_fname, (sizeof(char) * (strlen(path_name) + 1)), "data file name", api_idx, NULL);
    strcpy(api_table[api_idx].data_fname, path_name);
 
    if ((tmp = strrchr(path_name, '/')) != NULL)
    {
      *tmp = '\0';
      data_file_path_len = strlen(path_name);
      NIDL (1, "After removing data file_name = %s, length = %d", path_name, data_file_path_len);
    }
    //api_table[api_idx].data_file_path = (char*)malloc(sizeof(char) * (data_file_path_len + 1));
    NSLB_MALLOC(api_table[api_idx].data_file_path, (sizeof(char) * (data_file_path_len + 1)), "data file path", api_idx, NULL);
 
    strcpy(api_table[api_idx].data_file_path, path_name);

    if(!is_sql_var)
    { 
     //api_table[api_idx].data_file_buf = (char *)malloc((api_table[api_idx].data_file_size + 1) * sizeof(char));
      NSLB_MALLOC(api_table[api_idx].data_file_buf, ((api_table[api_idx].data_file_size + 1) * sizeof(char)), "data file buf", api_idx, NULL);
      time(&now);
      NIDL (1, "Start time for reading fill into memory =%s\n", ctime(&now));
      nslb_read_file_and_fill_buf_ex (read_data_fd, api_table[api_idx].data_file_buf, data_file_size, '1');
      time(&now);
      NIDL (1, "End time for reading fill into memory =%s\n", ctime(&now));
      close(read_data_fd);
    }
    else
    {
      api_table[api_idx].data_file_buf = sqlVarTable->query_result;
      api_table[api_idx].data_file_size = data_file_size = sqlVarTable->query_result_size; 
    }
  }
  else
  {
    char fext[20] = {0};
    char str_mode[20] = {0};
    char req_ext[20] = {0};

    if(!is_file_ext_match_wth_mode(file_name, fext, api_table[api_idx].sequence))
    {
      get_str_mode(str_mode, req_ext, api_table[api_idx].sequence);
      SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012123_ID, CAV_ERR_1012123_MSG, fext, str_mode, str_mode, req_ext);
      return -1;
    }
    NIDL (1, "Set data_file_buf with data_file_buf %p", data_file_buf);
    api_table[api_idx].data_file_buf = data_file_buf_rtc;
    api_table[api_idx].end_head_ptr = NULL; 
    //TODO: api_table[api_idx].data_file_size = ??
  }
  
  line = api_table[api_idx].data_file_buf;

  int ignore_line_flag = 0;
  char *tmp_ptr = NULL;
  int offset_index = 0;
  int prev_start_offset_val = 0;

  time(&now);

  NIDL (1, "Start time for scanning memory to calculate lines per data file = %s\n", ctime(&now));

  while ((tmp_ptr = strpbrk(line, "\r\n")) != NULL) 
  {
    line_number++;
    //strncpy(line_buf, line, (tmp_ptr - line));
    //line_buf[tmp_ptr - line] = '\0';
    /*Here we are dealing with dos and linux file format both.
      Since dos file end with \r\n and linux file end with \n so pointing tmp_ptr as requirment*/
    if(*tmp_ptr == '\r')
    {
      tmp_ptr ++;   //skip \r
    }
    if(*tmp_ptr == '\n')
    {
      tmp_ptr++;
    }
    line_buf = line;
    //save header line pointer, it is require to be added in each file divided for generator  
    if (line_number == (api_table[api_idx].first_data_line - 1)) 
    {
      if (api_table[api_idx].end_head_ptr == NULL) { 
        api_table[api_idx].end_head_ptr = tmp_ptr; 
        NIDL (2, "Add header lines in data file end_head_ptr = %p", api_table[api_idx].end_head_ptr);
      }
    }

    if(line_number < api_table[api_idx].first_data_line) 
    {
      NIDL (2, "This line is header line, Continuing");
      line = tmp_ptr;
      continue;
    }

    /*Setting ignore_line_flag for ignoring blank line from data file*/
    NI_SET_IGNORE_LINE_FLAG(line_buf);
    //NIDL (4, "ignore_line_flag = %d, num_line_ignore = %d, offset_value = %d", ignore_line_flag, num_line_ignore, offset_value);
    if(ignore_line_flag == 1)
    {
      NIDL (3, "Ignoring blank line from data file");
      ignore_line_flag = 0;
      line = tmp_ptr;
      continue;
    }
    num_values_per_var++;
    /*Save start pointer if line number is equal to first data line*/
    if (num_values_per_var - prev_start_offset_val == OFFSET_VALUE) 
    {
      store_offset_value(api_idx, line_number, offset_index, tmp_ptr);
      offset_index++;
      prev_start_offset_val = num_values_per_var; 
      NIDL (2, "number of lines %d and prev_start_offset_val %d", num_values_per_var, prev_start_offset_val);
    }
    line = tmp_ptr;
  }

  time(&now);
  NIDL (1, "End time for scanning memory to calculate lines per data file=%s", ctime(&now));

  NIDL (1, "api_table[api_idx].rtc_flag = %d, api_idx = %d", api_table[api_idx].rtc_flag, api_idx);

  if (num_values_per_var == 0) {
    if (!runtime){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, file_name);
    }
    SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, file_name);
    return -1;
  }
  //Bug 56941: api table values are updated on wrong runtime changes values
  if(((api_table[api_idx].sequence == UNIQUE) || (api_table[api_idx].sequence == USEONCE)) && (api_table[api_idx].rtc_flag != APPEND_MODE))
  {
    NIDL (1, "api_table[api_idx].rtc_flag = %d, api_table[api_idx].sequence = %d", api_table[api_idx].rtc_flag, api_table[api_idx].sequence);
    if(script_table[api_table[api_idx].script_idx].script_total > num_values_per_var)
    {
      if (!runtime){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012121_ID, CAV_ERR_1012121_MSG, file_name,
                                   (api_table[api_idx].sequence == USEONCE)?"USE-ONCE":"UNIQUE",
                                   script_table[api_table[api_idx].script_idx].script_name,
                                   script_table[api_table[api_idx].script_idx].script_total, num_values_per_var);
      }
      SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012121_ID, CAV_ERR_1012121_MSG, file_name,
                                (api_table[api_idx].sequence == USEONCE)?"USE-ONCE":"UNIQUE",
                                script_table[api_table[api_idx].script_idx].script_name,
                                script_table[api_table[api_idx].script_idx].script_total, num_values_per_var);
      return -1;

    }
  }

  api_table[api_idx].num_values = num_values_per_var;
  api_table[api_idx].total_offset = offset_index;

  NIDL (2, "Method end, total records = %d, total offset = %d", num_values_per_var, api_table[api_idx].total_offset);

  return 0;                                                              
}

static int ni_set_buffer_for_use_once_within_test(char* value, char* buffer, int api_idx, char* script_file_msg, int* state, int* use_once_within_test_done, char* use_once_within_test_val)
{
  if(*state == NI_ST_STAT_VAR_NAME){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (api_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if (*use_once_within_test_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "UseOnceWithinTest", "File");
  }

  strcpy(buffer, value);
  NIDL(2, "After tokenized use_once_within_test = %s", buffer);

  if((!strcasecmp(buffer, "YES")) || (!strcasecmp(buffer, "NO"))){
    strcpy(use_once_within_test_val, buffer);
  }
  else{
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "UseOnceWithinTest", "File");
  }

  *use_once_within_test_done = 1;
  *state = NI_ST_STAT_VAR_OPTIONS;
  return 0;
}

static int set_static_var_column_delim(char* value, char* column_delimiter, int *state, int api_idx, char *script_file_msg)
{
  NIDL (2, "Method Called, value = [%s]", value);

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (api_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if(*value == '\0'){
    column_delimiter[0] = ',';
    column_delimiter[1] = '\0';
  }
  else {
    strcpy(column_delimiter, value);
  }
  NIDL (2, "After tokenized column delimiter = %s", column_delimiter);
  return 0;
}

static int set_ignoreInvalidData(char *value, char *buffer, int api_idx, char *script_file_msg)
{
  NIDL (1, "Method Called, value = [%s]", value);

  strcpy(buffer, value);

  NIDL (2, "buffer = %s", buffer);

  if (!strcasecmp(buffer, "YES")) {
    api_table[api_idx].ignore_invalid_line = 1;
  } else if (!strcasecmp(buffer, "NO")) {
    api_table[api_idx].ignore_invalid_line = 0;
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "IgnoreInvalidData", "File");
  }

  NIDL (1, "End of the method : api_table[api_idx].ignore_invalid_line = [%d], api_idx = %d", api_table[api_idx].ignore_invalid_line, api_idx);
  return 0;
}

static int set_buffer_for_useonce_err(char *value, char *buffer, int *state, int api_idx, char *script_file_msg, int *useonce_err)
{
  NIDL(1, "Method Called, value = [%s]", value);

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (api_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(buffer, value);
  NIDL(2, "After tokenized on_useonce_error = %s", buffer);
  if (!strcasecmp(buffer, "ABORTPAGE")) {
    *useonce_err = ABORT_USEONCE_PAGE;
  } else if (!strcasecmp(buffer, "ABORTSESSION")) {
    *useonce_err = ABORT_USEONCE_SESSION;
  } else if (!strcasecmp(buffer, "ABORTTEST")) {
    *useonce_err = ABORT_USEONCE_TEST;
  } else if (!strcasecmp(buffer, "ABORTUSER")) {
    *useonce_err = ABORT_USEONCE_USER;
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(buffer, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "OnUseOnceError", "File");
  }

  *state = NI_ST_STAT_VAR_OPTIONS;
  return 0;
}

/* Format example: VAR_VALUE=F1=file;F2=value */
static int ni_set_static_var_var_value(char* value, int api_idx, char *script_file_msg){
  NIDL(2, "Method Called, value = [%s], api_idx = [%d]", value, api_idx); //F1=file;F2=value 
  char *ptr, *vv, *start;
  int field;
  char *type;
  ptr = value;
  while(*ptr != '\0') {  /* Extract individual var values */
    start = vv = ptr;

    while (*ptr != '\0') {
      if (*ptr == ';') {
      *ptr = '\0';
       ptr++;
       break;
       }
       ptr++;
    }

    field = -1;

    //CLEAR_WHITE_SPACE(vv);
    while(*vv == ' ') vv++;
    sscanf(vv, "F%d", &field);
    NIDL(2, "vv = %s, field = %d", vv, field);
    //CHECK_CHAR(vv, '=', script_file_msg);
    while(*vv != '\0') {
      if (*vv != '=')
        vv++;
      else
         break;
      }

    if (*vv != '\0') vv++;

    //CLEAR_WHITE_SPACE(vv);
    while(*vv == ' ') vv++;

    type = vv;
    //type[0] = '\0';

    if (*type != '\0') {
      
      //VAR_VALUE is option type is file or file_param.
      if ((strcasecmp(type, "file") == 0) || (strcasecmp(type, "file_param") == 0)) 
      {
        if ((field <= api_table[api_idx].num_vars) && (field >= 0)) 
        {
          NIDL(2, "Setting flag is_file: api_idx = %d, field = %d", api_idx, field);
          if (strcasecmp(type, "file") == 0)
            api_table[api_idx].is_file = IS_FILE;
        } 
        else 
        {
          //NS_EXIT(-1, "Invalid VAR_VALUE %s", start);
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012031_ID, CAV_ERR_1012031_MSG, field, api_table[api_idx].num_vars);
        }
      } 
      else if (strcasecmp(type, "value") != 0) 
      {
        //NS_EXIT(-1, "Invalid type %s specified.", type);
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012037_ID, CAV_ERR_1012037_MSG, type);
      }
    }
    else 
    {
      //NS_EXIT(-1, "Unrecognized VAR_VALUE (%s)", start);
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, start, "Var_Value", "File");
    }
  }
  return 0;
}

/********************************************************************************** 
* Description		: Parse api "nsl_static_var" for option UNQUIE and USE-ONCE 
                          and SQL-PARAMETER
* Inputs Parameter      
  line			: nsl_static_var(name2,address2,image2, FILE=user.dat, 
                          REFRESH=SESSION, MODE=SEQUENTIAL, VAR_VALUE=F2=file,
                          Encode=All|None|Specified, EncodeChars=<chars>, EncodeSpaceBy=+ or %20);
* line_number		: line number on scrip.capture/regristration.spec
* script_idx		: script id of user
* script_file_name	: registration_spec_name
***********************************************************************************/
static int input_staticvar_data(char* line, int line_number, int script_idx, char *reg_spec_name, int api_type) 
{
  int state = NI_ST_STAT_VAR_NAME;
  int create_group = 1;
  int api_idx = -1;
  char* line_tok;
  int mode = -1;
  char staticvar_file[MAX_FIELD_LENGTH] = {0};
  int file_done = 0;
  int mode_done = 0;
  int i;
  char buffer[MAX_FIELD_LENGTH];
  char script_file_msg[MAX_FIELD_LENGTH];
  char column_delimiter[16];
  char firstdataline[12];
  char headerLine[12];
  int absolute_path_flag = 0;
  NSApi api_ptr;
  int j, ret;
  char *value;
  int exitStatus;
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];
  //Set default value
  strcpy(column_delimiter, ",");
  strcpy(firstdataline, "1");
  time_t now;
  int on_use_once_err = ABORT_USEONCE_SESSION; //default value to abort page in useonce case if no values are present
  int is_sql_var = 0;
  int use_once_within_test_done = 0;
  NI_SQLVarTableEntry sqlVarTable = {0};
  NI_SQLConAttr sqlAttr = {0};
  short int sql_args_done_flag = 0x0000;
  char host[512] = {0};
  char port[256] = {0};
  char use_once_within_test_val[16] = "NO"; //Default Value
  char cmd[2 * 1024] = {0};
  char tmp;
  char *ptr = NULL;
  char staticvar_datadir[MAX_UNIX_FILE_NAME + 1] = {0};
  int grp_idx = -1;
  
  NIDL (1, "Method called. line = %s, line_number = %d, script_idx = %d", line, line_number, script_idx);
  
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  if(api_type == STATIC_VAR_NEW)
    sprintf(script_file_msg, "Parsing nsl_static_var() decalaration in file (%s/%s/%s/scripts/%s/%s) at line number %d : ",
                              GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                              script_table[script_idx].script_name, REGISTRATION_FILENAME, line_number);
  else if(api_type == NI_SQL_VAR)
    sprintf(script_file_msg, "Parsing nsl_sql_var() decalaration in file (%s/%s/%s/scripts/%s/%s) at line number %d : ",
                              GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                              script_table[script_idx].script_name, REGISTRATION_FILENAME, line_number);


  sprintf(file_name, "%s/%s/%s/scripts/%s/%s", GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                                               script_table[script_idx].script_name, REGISTRATION_FILENAME);

  file_name[strlen(file_name)] = '\0';
  NIDL (2, "api_ptr = %p, file_name = %s", &api_ptr, file_name);

  if ((ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 0)) != 0)
  {

    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }
  
  for(j = 0; j < api_ptr.num_tokens; j++) 
  {
    line_tok = api_ptr.api_fields[j].keyword;
    value = api_ptr.api_fields[j].value;

    NIDL (2, "line_tok = [%s], value = [%s]", line_tok, value);
    NIDL (2, "api_ptr.num_tokens = %d", api_ptr.num_tokens);

    if(!strcasecmp(line_tok, "DATADIR") ) {
      validate_and_copy_datadir(1, value, staticvar_datadir, err_msg);
      if (staticvar_datadir[0] == '\0')
        strcpy(staticvar_datadir, "0"); //To indicate that data dir is applied in this API
    }
    else if (!strcasecmp(line_tok, "FILE")) {
      grp_idx = set_staticvar_file(value, staticvar_datadir, staticvar_file, script_idx, &state, api_idx, &file_done, script_file_msg, &absolute_path_flag, api_type);
    } else if (!strcasecmp(line_tok, "MODE")) {
      set_buffer_for_mode(value, buffer, &state, api_idx, script_file_msg, &mode_done, &mode);
    } else if (!strcasecmp(line_tok, "FirstDataLine")) {
      set_static_firstDataLine(value, firstdataline, &state, api_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "HeaderLine")) {
      set_static_headerLine( value, headerLine, &state, api_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "ColumnDelimiter")) {
      set_static_var_column_delim(value, column_delimiter, &state, api_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "IgnoreInvalidData")) {
      set_ignoreInvalidData(value, buffer, api_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "OnUseOnceError")) {
      set_buffer_for_useonce_err(value, buffer, &state, api_idx, script_file_msg, &on_use_once_err);
    } else if (!strcasecmp(line_tok, "REFRESH")) {
      NIDL (4, "No need to parse REFRESH option");
    } else if (!strcasecmp(line_tok, "SaveUseOnceOption")) {
      NIDL (4, "No need to parse SaveUseOnceOption option");
    } else if (!strcasecmp(line_tok, "EncodeMode")) {
      NIDL (4, "No need to parse EncodeMode option");
    } else if (!strcasecmp(line_tok, "CharstoEncode")) {
      NIDL (4, "No need to parse CharstoEncode option");
    } else if (!strcasecmp(line_tok, "VAR_VALUE")) { 
      NIDL (4, "No need to parse VAR_VALUE option");
      ni_set_static_var_var_value(value, api_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "EncodeSpaceBy")) {
      NIDL (4, "No need to parse EncodeSpaceBy option");
    } else if (!strcasecmp(line_tok, "CopyFileToTR")) {
      NIDL (4, "No need to parse CopyFileToTR option");
    } else if (!strcasecmp(line_tok, "RemoveDupRecords")) {
      NIDL (4, "No need to parse RemoveDupRecords option");
    } else if (!strcasecmp(line_tok, "UseOnceWithinTest")) {
      ni_set_buffer_for_use_once_within_test(value, buffer, api_idx, script_file_msg, &state, &use_once_within_test_done, use_once_within_test_val);
    } else if (api_type == NI_SQL_VAR){
      if((exitStatus = ni_parse_sql_params(line_tok, value, &sqlVarTable, &state, script_file_msg, host, port, &sql_args_done_flag, &api_idx, &create_group)) == -1)
          return exitStatus;
    } else { /* this is for variables inputted */
      exitStatus = ni_set_last_option(&api_idx, &state, &create_group, script_file_msg, line_tok);
      if (exitStatus == FAILURE_EXIT)
        return FAILURE_EXIT;
    }
  }
  
  //this is being used to determine whether to use scenario datadir or API datadir  
  api_table[api_idx].data_dir_idx = grp_idx;

  if(atoi(firstdataline) <= atoi(headerLine))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012107_ID, CAV_ERR_1012107_MSG, atoi(firstdataline), atoi(headerLine));
  }
  
  if (((api_type == STATIC_VAR_NEW) || (sql_args_done_flag & NI_SQL_PARAM_SAVEINTOFILE_PARSING_DONE)) && ((!file_done) || (!strlen(staticvar_file))))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012089_ID, CAV_ERR_1012089_MSG, "FILE");
  }

  api_table[api_idx].abs_or_relative_data_file_path = absolute_path_flag; 
  api_table[api_idx].script_idx = script_idx;
 
  api_table[api_idx].first_data_line = atoi(firstdataline);

  if(mode == USEONCE){    //if mode is USEONCE then only we will save value for  on_useonce_abort
    api_table[api_idx].UseOnceAbort = on_use_once_err;
  }

  strcpy(api_table[api_idx].UseOnceWithinTest, use_once_within_test_val);
  strcpy(api_table[api_idx].column_delimiter, column_delimiter);

  if(!strcasecmp(api_table[api_idx].UseOnceWithinTest, "YES"))
  {
    // staticvar_file contains absolute path of file.
    if((ptr = strrchr(staticvar_file, '/'))){
    tmp = *ptr;
    *ptr = '\0';  
    }
    
    sprintf(buffer, "%s/logs/TR%d/.controller/.use_once/%s", work_dir, test_run_num, (api_table[api_idx].abs_or_relative_data_file_path == 1)?staticvar_file:"");

   sprintf(cmd, "mkdir -p %s", buffer);
   if(system(cmd) < 0)
     fprintf(stderr, "Failed to execute command %s", cmd);

   //api_table[api_idx].UsedFilePath = (char*) malloc(2 * 1024);
   NSLB_MALLOC(api_table[api_idx].UsedFilePath, (2048 * sizeof(char)), "used file path", api_idx, NULL);
   sprintf(api_table[api_idx].UsedFilePath, "%s/%s", buffer, ptr + 1); 
    NIDL (2, "Used file path = %s", api_table[api_idx].UsedFilePath); 
   *ptr = tmp;
  }

  NIDL (4, "API TABLE: index = %d, script_idx = %d, first_data_line = %d, column_delimiter = %s", api_idx, api_table[api_idx].script_idx, api_table[api_idx].first_data_line, api_table[api_idx].column_delimiter);

  /* In NC mode, for SQL parameter if file name is not given by user then default file will be created at path $NS_WDIR/scripts/project/sub_project/<script_name>/'sql_param_data_file_<api_id>' on generator machine. */
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  if((api_type == NI_SQL_VAR) && (staticvar_file[0] == '\0')){
    sprintf(staticvar_file, "%s/%s/%s/scripts/%s/sql_param_data_file_%d", GET_NS_TA_DIR(), script_table[script_idx].proj_name,
                             script_table[script_idx].sub_proj_name, script_table[script_idx].script_name, (total_api_entries - 1));
    NIDL (2, "staticvar_file = %s", staticvar_file); 
  }

  if(api_type == NI_SQL_VAR)
  {
    char fname[5 * 1024];
    get_data_file_wth_full_path(staticvar_file, fname, script_idx);
    
    for(i = 0; i < total_api_entries - 1; i++)
    { 
      if(!strcmp(fname, api_table[i].data_fname))
      {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012118_ID, CAV_ERR_1012118_MSG, fname);
      }
    }
  } 

  if(api_type == NI_SQL_VAR){
    if((ni_sql_param_validate_keywords(&sql_args_done_flag, script_file_msg)) == -1)
      return -1;
    if((ni_sql_param_validate_host_and_port(host, port, script_file_msg, &sqlVarTable)) == -1)
      return -1;
    if((ni_set_sql_odbc_con_attr(&sqlVarTable, &sqlAttr)) == -1)
      return -1;
    if((ni_sql_get_data_from_db(&sqlVarTable, &sqlAttr, column_delimiter, err_msg)) == -1){
      fprintf(stderr, "%s", err_msg);
      return -1;
    }
    api_table[api_idx].is_sql_var = is_sql_var = 1;
  }

  time(&now);
  NIDL (1, "Start time for parsing data files =%s", ctime(&now));
  
  if (ni_input_static_values(staticvar_file, api_idx, script_idx, 0, NULL, is_sql_var, &sqlVarTable, err_msg) == -1) {
    fprintf(stderr, "%s Error in reading value from data file %s.%s\n", script_file_msg, staticvar_file, err_msg);
    return -1;
  }
  time(&now);
  NIDL (1, "End time for parsing data files =%s", ctime(&now));

  /*In NC mode test, if mode is random or sequential then all generators will get equal numbers of data lines. Hence there is no need of 
    distribution of data. So, here dump query result into given data file. */
  if((api_table[api_idx].is_sql_var) && ((api_table[api_idx].sequence == SEQUENTIAL) || (api_table[api_idx].sequence == RANDOM)))
  {
    FILE *gen_data_fp = NULL;
    if((gen_data_fp = fopen(api_table[api_idx].data_fname, "w")) == NULL)
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000006 + CAV_ERR_HDR_LEN, api_table[api_idx].data_fname, errno, nslb_strerror(errno));
    }
    fwrite(api_table[api_idx].data_file_buf, sizeof(char), api_table[api_idx].data_file_size, gen_data_fp);
    fclose(gen_data_fp);
  }

  //If given parameter have USEONCE mode then check for same data file.
  if(api_table[api_idx].sequence == USEONCE)
  {
    char fname[5 * 1024];
    get_data_file_wth_full_path(staticvar_file, fname, script_idx);

    for(i = 0; i < total_api_entries - 1; i++)
    {
      if(api_table[i].sequence == USEONCE) //Mode is USEONCE then only check the script name
      {
        if(!strcmp(fname, api_table[i].data_fname))
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012118_ID, CAV_ERR_1012118_MSG, fname);
        }
      }
    }
  }
  return 0;
}

/********************************************************************************** 
* Description		: Parse api "nsl_index_file_var" 
* Inputs Parameter      
  line			: nsl_static_var(name2,address2,image2, FILE=user.dat, 
                          REFRESH=SESSION, MODE=SEQUENTIAL, VAR_VALUE=F2=file,
                          Encode=All|None|Specified, EncodeChars=<chars>, EncodeSpaceBy=+ or %20);
* line_number		: line number on scrip.capture/regristration.spec
* script_idx		: script id of user
* script_file_name	: registration_spec_name
***********************************************************************************/
static int input_indexvar_data(char* line, int line_number, int script_idx, char *reg_spec_name, int api_type) 
{
  int state = NI_ST_STAT_VAR_NAME;
  int create_group = 1;
  int api_idx = -1;
  char* line_tok;
  char staticvar_file[MAX_FIELD_LENGTH] = {0};
  int file_done = 0;
  char script_file_msg[MAX_FIELD_LENGTH];
  int absolute_path_flag = 0;
  NSApi api_ptr;
  int j, ret;
  char *value;
  int exitStatus;
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];
  char *tmp;
  char staticvar_datadir[MAX_UNIX_FILE_NAME + 1] = {0};
  int grp_idx = -1;
  int data_file_path_len;
  
  NIDL (1, "Method called. line = %s, line_number = %d, script_idx = %d", line, line_number, script_idx);
  
  if(api_type == NI_INDEX_VAR)
    sprintf(script_file_msg, "Parsing nsl_index_file_var() decalaration in file (%s/%s/%s/scripts/%s/%s) at line number %d : ",
                              GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                              script_table[script_idx].script_name, REGISTRATION_FILENAME, line_number);
  
  sprintf(file_name, "%s/%s/%s/scripts/%s/%s", GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                                               script_table[script_idx].script_name, REGISTRATION_FILENAME);

  file_name[strlen(file_name)] = '\0';
  NIDL (2, "api_ptr = %p, file_name = %s", &api_ptr, file_name);

  if ((ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 0)) != 0)
  {

    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }
  
  NIDL (2, "api_ptr.num_tokens = %d", api_ptr.num_tokens);
  for(j = 0; j < api_ptr.num_tokens; j++) 
  {
    line_tok = api_ptr.api_fields[j].keyword;
    value = api_ptr.api_fields[j].value;

    NIDL (2, "line_tok = [%s], value = [%s]", line_tok, value);

    if(!strcasecmp(line_tok, "DATADIR")) {
      validate_and_copy_datadir(1, value, staticvar_datadir, err_msg);
      if (staticvar_datadir[0] == '\0')
        strcpy(staticvar_datadir, "0"); //To indicate that data dir is applied in this API
    }
    else if (!strcasecmp(line_tok, "FILE")) {
      grp_idx = set_staticvar_file(value, staticvar_datadir, staticvar_file, script_idx, &state, api_idx, &file_done, script_file_msg, &absolute_path_flag, api_type);
    
    }
    else if(api_idx == -1) { /* this is for variables inputted */
     exitStatus = ni_set_last_option(&api_idx, &state, &create_group, script_file_msg, line_tok);
     if (exitStatus == FAILURE_EXIT)
       return FAILURE_EXIT;
    }
  }

  //this is being used to determine whether to use scenario datadir or API datadir  
  api_table[api_idx].data_dir_idx = grp_idx;

  api_table[api_idx].abs_or_relative_data_file_path = absolute_path_flag; 
  api_table[api_idx].script_idx = script_idx;
 
  NSLB_MALLOC(api_table[api_idx].data_fname, (sizeof(char) * (strlen(staticvar_file) + 1)), "data file name", api_idx, NULL);
  strcpy(api_table[api_idx].data_fname, staticvar_file);
 
  if ((tmp = strrchr(staticvar_file, '/')) != NULL)
  {
    *tmp = '\0';
    data_file_path_len = strlen(staticvar_file);
    NIDL (1, "After removing data file_name = %s, length = %d", staticvar_file, data_file_path_len);
  }
  
  NSLB_MALLOC(api_table[api_idx].data_file_path, (sizeof(char) * (data_file_path_len + 1)), "data file path", api_idx, NULL);
  strcpy(api_table[api_idx].data_file_path, staticvar_file);

  return 0;
}

/****************************************************************************************************
  Purpose: This table is used for allocating the memory for UniqueRangeVarTableEntry table
  Inpuut:  It takes row_num as arguments, it increases the row_num to total_unique_range_entries.
  Output: It returns the SUCCESS_EXIT.
****************************************************************************************************/
static int create_uniquerange_var_table_entry(int *row_num) {
  NIDL(1, "Method called. total_unique_range_entries = %d, max_unique_range_entries = %d", total_unique_range_entries, max_unique_range_entries);

  if (total_unique_range_entries == max_unique_range_entries) {
    //MY_REALLOC(uniquerangeTable, (max_unique_range_entries + DELTA_UNIQUE_RANGE_ENTRIES) * sizeof(UniqueRangeVarTableEntry), "unique range var entries", -1);
    uniquerangeTable = (UniqueRangeTableEntry*)realloc(uniquerangeTable, (max_unique_range_entries + DELTA_UNIQUE_RANGE_ENTRIES) * sizeof(UniqueRangeTableEntry));
    max_unique_range_entries +=  DELTA_UNIQUE_RANGE_ENTRIES;
  }
  *row_num = total_unique_range_entries++;
  NIDL(1, "total_unique_range_entries = %d, row_num = %d", total_unique_range_entries, *row_num);
  return (SUCCESS_EXIT);
}

// This function validates the variable name
// 0 valid 
// 1 invalid
int validate_unique_var(char *var) {
        
  int len = strlen(var);
  int i = 0;
      
  if(isdigit(var[i]) || !isalpha(var[i]))
    return 1; 
      
  for(i = 1 ; i < len ; i++) {
    if(!isalnum(var[i]) && (var[i] != '_'))
     return 1;
  }
  return 0;
}

/**********************************************************************************************************
* Purpose     : This method is used for parsing of nsl_unique_range_var API. It fills UniqueRangeTableEntry

* API         : nsl_unique_range_var(unique_num5, StartRange="10", UserBlockSize="10",
                Refresh="USE", Format="%08d", ActionOnRangeExceed="SESSION_FAILURE");
* Input       : It take line, line_number, sess_idx and script_filename as arguments.
* line        : It points to the line in registration.spec where nsl_unique_range_var API is present.
* line_number : It is the line number of the registrations.spec file where nsl_unique_range_var 
                API is present.
* sess_idx    : It is the session index.
***********************************************************************************************************/
static int input_unique_rangevar_data(char *line, int line_number, int script_idx, char *reg_spec_fname)
{
  int state = NS_ST_SE_NAME_NEW;
  int rnum = 0;
  int ret = 0, j, i;
  char key[MAX_ARG_NAME_SIZE + 1];
  char value[MAX_ARG_VALUE_SIZE + 1];
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];                   /* Store Registration file name "registration.spec" */
  char unique_rangevar_buf[UNQRANG_MAX_LEN_512B];       /* Store variable name of API nsl_uniquerange_var() */
  char *ptr = NULL;
  
  NSApi api_ptr;  //for parsing from parse_api
  
  NIDL(1, "Method called. line = %s, line_number = %d, script_idx = %d", line, line_number, script_idx);
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  sprintf(file_name, "%s/%s/%s/scripts/%s", GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                                         script_table[script_idx].script_name);
  file_name[strlen(file_name)] = '\0';
  NIDL(1, "api_ptr = %p, file_name = %s", &api_ptr, file_name);
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_unique_range_var() declaration on line %d of %s/%s/%s/scripts/%s: ", line_number,
                                  GET_NS_RTA_DIR(), script_table[script_idx].proj_name, script_table[script_idx].sub_proj_name, 
                                  script_table[script_idx].script_name);

  msg[MAX_LINE_LENGTH-1] = '\0';
  
  if(!strstr(line, "StartRange"))
  {
    NS_EXIT(FAILURE_EXIT, "StartRange is not provided in nsl_unique_range_var API");
  }

  if(!strstr(line, "UserBlockSize"))
  {
    NS_EXIT(FAILURE_EXIT, "UserBlockSize is not provided in nsl_unique_range_var API");
  }
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return FAILURE_EXIT;
  }
  
  if(scen_grp_entry[script_idx].grp_type == TC_FIX_USER_RATE) {
    NS_EXIT(FAILURE_EXIT, "nsl_unique_range_var is not supported in FIX_SESSION_RATE type scenario");
  }

  for(j = 0; j < api_ptr.num_tokens; j++) {
    strcpy(key, api_ptr.api_fields[j].keyword);
    strcpy(value, api_ptr.api_fields[j].value);

    NIDL(2, "j = %d, api_ptr.num_tokens = %d, key = [%s], value = [%s], state = %d",
                            j, api_ptr.num_tokens, key, value, state);
    switch (state) {
    case NS_ST_SE_NAME_NEW:
      //unique_range_var should not have any value
      if(strcmp(value, ""))
        break;

      strcpy(unique_rangevar_buf, key);
      NIDL(2, "parameter name = [%s]", unique_rangevar_buf);

      /* For validating the variable we are calling the validate_unique_var funcction */
      if(validate_unique_var(unique_rangevar_buf)) {
        printf("%s: Invalid var name '%s'.\nLine = %s\n", msg, unique_rangevar_buf, line);
        return FAILURE_EXIT;
      }
      if(create_uniquerange_var_table_entry(&rnum) != SUCCESS_EXIT) {
        fprintf(stderr, "%s Not enough memory. Could not created unique_range var table entry.\n", msg);
        return FAILURE_EXIT;
      }
      uniquerangeTable[rnum].sess_idx = script_idx;
      NIDL(1, "script_table[script_idx].unique_range_var_start_idx = %d, sessidx = %d, rnum = %d",
               script_table[script_idx].unique_range_var_start_idx, uniquerangeTable[rnum].sess_idx, rnum);
      if (script_table[script_idx].unique_range_var_start_idx == -1) {
        script_table[script_idx].unique_range_var_start_idx = rnum;
        NIDL(1, "script_table[script_idx].unique_range_var_start_idx = %d, rnum = %d", script_table[script_idx].unique_range_var_start_idx,
                 rnum);
      }

      script_table[script_idx].num_unique_range_var_entries = total_unique_range_entries;
      NIDL(1, "script_table[script_idx].num_unique_range_var_entries = %d", script_table[script_idx].num_unique_range_var_entries);

      strcpy(uniquerangeTable[rnum].name, unique_rangevar_buf);

      //Fill the default values to the unique range var table 
      uniquerangeTable[rnum].start = 0;
      uniquerangeTable[rnum].block_size = 32;
      uniquerangeTable[rnum].refresh = SESSION_NEW;
      strcpy(uniquerangeTable[rnum].format, "\0");
      uniquerangeTable[rnum].format_len = -1;
      uniquerangeTable[rnum].action = SESSION_FAILURE_NEW;

      state = NS_ST_SE_OPTIONS_NEW;
      break;

    case NS_ST_SE_OPTIONS_NEW:
      if (!strcasecmp(key, "StartRange")) {
         if(!strcmp(value, "")) {
           fprintf(stderr, "Empty value of StartRange is not allowed\n");
           return FAILURE_EXIT;
         }

         for(i = 0; i < strlen(value); i++)
         {
           if (!isdigit(value[i])) {
              fprintf(stderr, "%s Invalid value of StartRange. It must be a numeric number\n", msg);
              return FAILURE_EXIT;
            }
         }
         NIDL(2, "StartRange = [%s]", value);
         uniquerangeTable[rnum].start = strtoul(value, &ptr, 10);
      } else if(!strcasecmp(key, "UserBlockSize")) {
          if(!strcmp(value, "")) {
             fprintf(stderr, "Empty value of UserBlockSize is not allowed\n");
             return FAILURE_EXIT;
          }

          for(i = 0; i < strlen(value); i++)
          {
            if (!isdigit(value[i])) {
              fprintf(stderr, "%s Invalid value of UserBlockSize. It must be a numeric number\n", msg);
              return FAILURE_EXIT;
            }
          }
          NIDL(2, "UserBlockSize = [%s]", value);
          uniquerangeTable[rnum].block_size = strtoul(value, &ptr, 10);
       } else if (!strcasecmp(key, "Refresh")) {
          NIDL(2, "Refresh = [%s]", value);
          if(!strcmp(value, "SESSION"))
             uniquerangeTable[rnum].refresh = SESSION_NEW;
           else if(!strcmp(value, "USE"))
             uniquerangeTable[rnum].refresh = USE_NEW;
           else
           {
             fprintf(stderr, "Invalid value of Refresh, It can be SESSION or USE\n");
             return FAILURE_EXIT;
           }
       } else if (!strcasecmp(key, "Format")) {
           NIDL(2, "Format = [%s]", value);
           strcpy(uniquerangeTable[rnum].format, value);
           uniquerangeTable[rnum].format_len = strlen(value);
       } else if (!strcasecmp(key, "ActionOnRangeExceed")) {
           NIDL(2, "ActionOnRangeExceed = [%s]", value);
           if(!strcmp(value, "SESSION_FAILURE"))
             uniquerangeTable[rnum].action = SESSION_FAILURE_NEW;
           else if(!strcmp(value, "STOP_TEST"))
             uniquerangeTable[rnum].action = UNIQUE_RANGE_STOP_TEST_NEW;
           else if(!strcmp(value, "CONTINUE_WITH_LAST_VALUE"))
             uniquerangeTable[rnum].action = CONTINUE_WITH_LAST_VALUE_NEW;
           else if(!strcmp(value, "REPEAT_THE_RANGE_FROM_START"))
             uniquerangeTable[rnum].action = REPEAT_THE_RANGE_FROM_START_NEW;
           else {
             fprintf(stderr, "Invalid value of ActionOnRangeExceed, It can be SESSION_FAILURE OR CONTINUE_WITH_LAST_VALUE OR CONTINUE_WITH_LAST_VALUE OR REPEAT_THE_RANGE_FROM_START OR STOP_TEST\n");
             return FAILURE_EXIT;
           }
       } else {
           fprintf(stderr, "%s unknown attribute '%s' in the unique range variable declaration at field '%d'.\nLine = %s\n",
                                                                                             msg, key, (j + 1), line);
           return FAILURE_EXIT;
       }
    }
    if(state == NS_ST_SE_NAME_NEW)
    {
      fprintf(stderr, "%s Unique Range parameter name is missing.It should be at very first position in API\n", msg);
      return FAILURE_EXIT;
    }
  }
  return (SUCCESS_EXIT);        
}

/****************************************************************************
 * Description          : Function used to find parameter type 
 * Input-Parameter      :
 * fp			: Registration.spec file poniter 
 * buf			: Logging purpose
 * line_number          : Used for logging purpose
 * fname		: Registration File name
 * Output-Parameter     : 
 * Return               : Returns parameter type, If file is empty then return
                          FINISH_LINE(9)
 *****************************************************************************/
static int get_var_element(FILE* fp, char* buf, int* line_number, char* fname) 
{
  char line[MAX_LINE_LENGTH];
  int type = 0;
  char* line_ptr;
  int str_len;

  NIDL (1, "Method Called.");
  NIDL (1,"Parsing %s on file %d", fname, *line_number);

  while (1) 
  {
    if (!nslb_fgets(line, MAX_LINE_LENGTH, fp, 1)) {
      return FINISH_LINE; /*Empty file then return FINISH_LINE*/
    } 
    else 
    {
      (*line_number)++;

      line[strlen(line) - 1] = '\0';

      line_ptr = line;
      CLEAR_WHITE_SPACE(line_ptr);
    }
    IGNORE_COMMENTS(line_ptr);
    if (strlen(line_ptr))
      break;
  }
  str_len = strlen(line_ptr);
  memmove(line, line_ptr, str_len+1);
  line_ptr = line;                                           

  if (!strncmp(line_ptr, "nsl_static_var", strlen("nsl_static_var")))
  {
    type = STATIC_VAR_NEW;
  }
  else if (!strncmp(line_ptr, "nsl_index_file_var", strlen("nsl_index_file_var")))
  {
    type = NI_INDEX_VAR;
  }
  else if(!strncmp(line_ptr, "nsl_unique_range_var", strlen("nsl_unique_range_var")))
  {
    type = UNIQUE_RANGE_VAR_NEW;
  }
  else if(!strncmp(line_ptr, "nsl_sql_var", strlen("nsl_sql_var")))
  {
    type = NI_SQL_VAR;
  }

  if (type && (type != FINISH_LINE))
  {
    strcpy(buf, line_ptr);
  } 

  NIDL (1, "Returning %s, type = %d", buf, type);
  return type;
}    
  
/****************************************************************************
 * Description          : Function used to read parameter API used in 
                          registration.spec. And parse file parameter API  
 * Input-Parameter      :
 * fp			: Registration.spec file poniter 
 * line_number          : Used for logging purpose
 * fname		: Registration File name
 * Output-Parameter     : 
 * Return               : Return 0 on success, -1 in case of exit
 *****************************************************************************/
static int read_file(FILE* fp, int* line_number, char* fname, int script_id) 
{
  char line[MAX_LINE_LENGTH];
  int ret;
  int done = 0;

  NIDL (1, "Method Called. fname = %s", fname);

  while (!done) 
  {
    // Find PARAMETER API string
    ret = get_var_element(fp, line, line_number, fname);
    script_ln_no = *line_number;
    NIDL (4, "line=[%s]", line);

    if (ret == FAILURE_EXIT) 
    {
      fprintf(stderr, "Parsing error : File=%s line=%d\n", fname, *line_number);
      return FAILURE_EXIT;
    }

    switch (ret) /*Type*/
    {
      case STATIC_VAR_NEW:
        if (input_staticvar_data(line, *line_number, script_id, fname, ret) == -1)
          return FAILURE_EXIT;
      break;
      case UNIQUE_RANGE_VAR_NEW:
        if (input_unique_rangevar_data(line, *line_number, script_id, fname) == -1)
          return FAILURE_EXIT;
      break;
      case NI_SQL_VAR:
        if (input_staticvar_data(line, *line_number, script_id, fname, ret) == -1)
          return FAILURE_EXIT;
        break;
      case FINISH_LINE:
        done = 1;
      break; 
      case NI_INDEX_VAR:
        if (input_indexvar_data(line, *line_number, script_id, fname, ret) == -1)
          return FAILURE_EXIT;
      break;
    }
  } 
  return SUCCESS_EXIT;
}

/****************************************************************************
 * Description          : Function used to parse registraction.spec file in 
 *                        case of C-type script. 
 * Input-Parameter      :
 * script_path		: Absolute script file path
 * script_id	        : Script index
 * Output-Parameter     : 
 * Return               : Return 0 on success, -1 in case of exit
 *****************************************************************************/
int ni_parse_registration_file(char *script_path, int script_id)
{
  char registration_file[MAXIMUM_SCRIPT_FILE_NAME_LEN + 1];
  FILE *reg_fp = NULL;
  int line_num = 0;

  NIDL (1, "Method called, script_path = %s", script_path);
  sprintf(registration_file, "%s/%s", script_path, REGISTRATION_FILENAME);
  flow_filename = REGISTRATION_FILENAME;
  if ((reg_fp = fopen(registration_file, "r")) == NULL)
  {
    if(errno != ENOENT) {// registration file is optinal. So check if any other error
      NS_EXIT(-1, CAV_ERR_1000006, registration_file, errno, nslb_strerror(errno));
    }
  }
  // Open registrations.spec and set line_number to 1 
  if (reg_fp != NULL)
  {
    if (read_file(reg_fp, &line_num, registration_file, script_id) == -1)
    {
      fprintf(stderr, "\n%s(): Error in read_file(%s)\n", (char*)__FUNCTION__, registration_file);
      return FAILURE_EXIT;
    }

    if (fclose(reg_fp) != 0) {
      NS_EXIT(FAILURE_EXIT, CAV_ERR_1000021, registration_file, errno, nslb_strerror(errno));
    }
  }

  return SUCCESS_EXIT;
}

 /******************************** DISTRIBUTION OF FILE PARAMETER VALUE ******************************/
void fill_group_details_in_script_table()
{
  int i, k = 0, j, m = 0;

  NIDL(1, "Method called total_script_entries = %d, total_sgrp_entries = %d", 
              total_script_entries, total_sgrp_entries);
  /*Total SGRP groups*/
  while (k < total_sgrp_entries)
  {
    for (i = 0; i < total_script_entries; i++)
    { /*Match script name and update scenario group details in corresponding script table entry*/
      if (!strcmp(script_table[i].script_name, scen_grp_entry[k].sess_name))
      { 
        script_table[i].script_total = 0;

        //BugId:65495 : Showing wrong script name in case of less data lines present in USE_ONCE file parameter.
        if(script_table[i].sgroup_num == -1)
          script_table[i].sgroup_num = k;//Unique group id

        for (j = 0; j < scen_grp_entry[k].num_generator; j++)
        {
          if (scen_grp_entry[k].grp_type == TC_FIX_USER_RATE) {
            scen_grp_entry[k].tot_sessions += scen_grp_entry[k + j].percentage/SESSION_RATE_MULTIPLIER;
            NIDL(1, "k = %d, j = %d, scen_grp_entry[k + j].total_sessions = %f", k, j, scen_grp_entry[k + j].tot_sessions);
          } else {
            scen_grp_entry[k].total_quantity_pct += scen_grp_entry[k + j].quantity;
            NIDL(1, "k = %d, j = %d, scen_grp_entry[k + j].quantity = %d", k, j, scen_grp_entry[k + j].quantity);
          }
        }
        if (scen_grp_entry[k].grp_type == TC_FIX_USER_RATE) 
          script_table[i].pct_or_users = (int)scen_grp_entry[k].tot_sessions;
        else
          script_table[i].pct_or_users = scen_grp_entry[k].total_quantity_pct;
        NIDL(1, "Script details Id = %d: Scenario group id = %d, Total quantity = %d", 
                 i, script_table[i].sgroup_num, script_table[i].pct_or_users);
        break;  
      }
    } 
    /* We need to increment for unique group entries
     * Eg. If group idx 0 has 2 generators then next unique entry wud be group idx 2 
     * Go to next group*/
    k = k + scen_grp_entry[k].num_generator;
  }
   
  while (m < total_sgrp_entries)
  {
    for (i = 0; i < total_script_entries; i++)
    {
      if (!strcmp(script_table[i].script_name, scen_grp_entry[m].sess_name))
      {
        if (scen_grp_entry[m].grp_type == TC_FIX_USER_RATE) 
          script_table[i].script_total += (int)scen_grp_entry[m].tot_sessions;
        else
          script_table[i].script_total += scen_grp_entry[m].total_quantity_pct;   
        NIDL(1, "Total number of users = %d for script = %s", 
        script_table[i].script_total, script_table[i].script_name);
      }
    }
    m = m + scen_grp_entry[m].num_generator;   
  }
}

static int comp_by_sgroupid_script (const void *e1, const void *e2)
{
  NIDL (1, "Method called");
  if (((ScriptTable *)e1)->sgroup_num > ((ScriptTable *)e2)->sgroup_num)
    return 1;
  else if (((ScriptTable *)e1)->sgroup_num < ((ScriptTable *)e2)->sgroup_num)
    return -1;
  else
    return 0;
}

static int get_per_gen_num_script_users(int gen_id,  char* script_name)
{
  int i, k = 0;
  int num = 0;

  NIDL (1, "Method called, gen_id = %d, script_name = %s", gen_id, script_name);
  while (k < total_sgrp_entries)
  {
    if(!strcmp(scen_grp_entry[k].sess_name, script_name))
    {
      for (i = 0; i < scen_grp_entry[k].num_generator; i++)
      {
        NIDL (1, " i = %d, k = %d, Index = %d", i, k, num_script_user_per_gen[i + k].gen_id);
        if (num_script_user_per_gen[i + k].gen_id == gen_id) {
          if (scen_grp_entry[k].grp_type == TC_FIX_USER_RATE)
            num += (int)num_script_user_per_gen[i + k].sessions;
          else
            num += num_script_user_per_gen[i + k].qty_per_gen;
          NIDL (1, "Total script quantity per generator = %d", num);
        }  
      }
    }
    k = k + scen_grp_entry[k].num_generator;
  }
  return num;
}

static void create_tbl_per_grp ()
{
  int k = 0, i, prev_tbl_size = 0;
  NIDL (1, "Method called");

  while (k < total_sgrp_entries)
  {
    num_script_user_per_gen = (gen_name_quantity_list*)realloc(num_script_user_per_gen, (scen_grp_entry[k].num_generator + prev_tbl_size) * sizeof(gen_name_quantity_list)); 
    prev_tbl_size += scen_grp_entry[k].num_generator;
    for (i = 0; i < scen_grp_entry[k].num_generator; i++)
    {
      if (scen_grp_entry[k].grp_type == TC_FIX_USER_RATE) {
        num_script_user_per_gen[i + k].sessions = scen_grp_entry[k + i].percentage/SESSION_RATE_MULTIPLIER; 
        NIDL(1, "Group id = %d, Gen index per group = %d, Generator id = %d, Sessions = %f", 
            k, i + k, num_script_user_per_gen[i + k].gen_id, num_script_user_per_gen[i + k].sessions);
      } else {
        num_script_user_per_gen[i + k].qty_per_gen = scen_grp_entry[k + i].quantity; 
        NIDL(1, "Group id = %d, Gen index per group = %d, Generator id = %d, Quantity = %d", 
               k, i + k, num_script_user_per_gen[i + k].gen_id, num_script_user_per_gen[i + k].qty_per_gen);
      }
      num_script_user_per_gen[i + k].gen_id = scen_grp_entry[k].generator_id_list[i];
    }
    k = k + scen_grp_entry[k].num_generator;
  }
}
#if 0
  ROUND-ROBIN DISTRIBUTION LOGIC
  for (i = 0; i < sgrp_used_genrator_entries; i++) 
  {
    num_users = per_gen_api_table[(i * total_api_entries) + api_id].num_script_users;
    NIDL (4, "For Generator index %d, Num script user per generator = %d", i, num_users);
    if (!num_users) 
      continue;
    if (seq == UNIQUE) 
      min = num_users;
    num_val = (total_vals * num_users)/total_users;
    NIDL (4, "Calculate number of value = %d", num_val);
    if (num_val < min) 
      num_val = min;
    per_gen_api_table[(i * total_api_entries) + api_id].num_val = num_val;
    used_val += num_val;
    NIDL (4, "Number of values distributed for entry = %d and Incremented used value = %d", 
             per_gen_api_table[(i * total_api_entries) + api_id].num_val, used_val);
  }

  leftover = total_vals - used_val;
  NIDL (4, "Calculte leftover = %d, total_vals = %d, used_val = %d", leftover, total_vals, used_val);
  if (leftover < 0 ) 
  {
    fprintf(stderr, "Error: Insufficient number of variable values specified in data file '%s' used in file parameter with mode"  
                    " Unique in script '%s'. Total users for the scenario group using this script are '%d' " 
                    " and total number of variable values in this file are '%d'.\n",
                    api_table[api_id].data_fname, script_table[api_table[api_id].script_idx].script_name,
                    total_users, api_table[api_id].num_values);
    exit(1);
  }
  i = 0;
  while (leftover) 
  {
    if (per_gen_api_table[(i * total_api_entries) + api_id].num_script_users) 
    {
      per_gen_api_table[(i * total_api_entries) + api_id].num_val++;
      leftover--;
      NIDL (4, "Distribute leftover = %d, incremented num_val = %d", 
               leftover, per_gen_api_table[(i * total_api_entries) + api_id].num_val);
    }
    i++;
    if (i == sgrp_used_genrator_entries) 
      i = 0;
  }
#endif

static void redistribute_file_data(int leftover, int api_id)
{
  int i = (sgrp_used_genrator_entries - 1), num_val;
  NIDL (1, "Method called, leftover = %d, api_id = %d", leftover, api_id);
  while (leftover && i)
  {
    num_val = per_gen_api_table[(i * total_api_entries) + api_id].num_val;//Just for debugging
    per_gen_api_table[(i * total_api_entries) + api_id].num_val -= 1;  
    leftover++;
    NIDL (4, "Remaining leftover = %d, for gen id = %d number of lines distributed earlier = %d, modified value = %d", leftover, i, num_val, per_gen_api_table[(i * total_api_entries) + api_id].num_val);
    i--;
  }

}

static int distribute_values (int total_users, int api_id, int runtime)
{
  int i, num_val, num_users, used_val = 0, leftover;
  unsigned int total_api_val = api_table[api_id].num_values;
  int seq = api_table[api_id].sequence;
  int total_script_usr = total_users;

  NIDL (2, "Method called, Details: API Index = %d, Sequence = %d, Number of records in file = %d," 
           "Script name = %s, total users per script = %d", 
            api_id, seq, total_api_val, script_table[api_table[api_id].script_idx].script_name,
            total_users);
  if ((total_script_usr > total_api_val) && (api_table[api_id].rtc_flag != APPEND_MODE)) 
  {
    if(!runtime) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012121_ID, CAV_ERR_1012121_MSG, api_table[api_id].data_fname,
                                   (api_table[api_id].sequence == USEONCE)?"USE-ONCE":"UNIQUE",
                                   script_table[api_table[api_id].script_idx].script_name, total_users, api_table[api_id].num_values);
    }
    SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012121_ID, CAV_ERR_1012121_MSG, api_table[api_id].data_fname,
                              (api_table[api_id].sequence == USEONCE)?"USE-ONCE":"UNIQUE",
                              script_table[api_table[api_id].script_idx].script_name, total_users, api_table[api_id].num_values);
    return -1;  
  }
  for (i = 0; i < sgrp_used_genrator_entries; i++)
  {
    num_users = per_gen_api_table[(i * total_api_entries) + api_id].num_script_users;
    if (!num_users)
      continue;
    //Calucate with respect to ratio of users among generator
    //double cal_ratio = (double)((total_usr_double * num_users) / total_vals);
    NIDL (4, "For Generator index %d, num_users = %d, total_vals = %u, total_script_usr = %d, used_val = %d", 
              i, num_users, total_api_val, total_script_usr, used_val);
    /* commenting below code to resolved bug 9509 and 40469 */
    /* double cal_ratio = (double)(num_users * (total_api_val / total_script_usr));
    NIDL (4, "Calculated ratio = %f", cal_ratio);
    cal_ratio = round(cal_ratio); 
    NIDL (4, "Round off ratio = %f", cal_ratio); */
    if((!runtime) || (is_generator_alive(i))){ 
    num_val = (num_users * (total_api_val / total_script_usr));
    per_gen_api_table[(i * total_api_entries) + api_id].num_val = num_val;
    } 
    else
      per_gen_api_table[(i * total_api_entries) + api_id].num_val = 0;
 
    used_val += per_gen_api_table[(i * total_api_entries) + api_id].num_val;
  }
  //leftover = total_vals - used_val;
  leftover = total_api_val - used_val;
  NIDL (4, "Calculate leftover = %d, used_val = %d", leftover, used_val);
  if (leftover < 0 ) 
  {
    if (continue_on_file_param_dis_err == STOP_TEST)
    {
      if(!runtime) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012121_ID, CAV_ERR_1012121_MSG, api_table[api_id].data_fname,
                                     (api_table[api_id].sequence == USEONCE)?"USE-ONCE":"UNIQUE",
                                     script_table[api_table[api_id].script_idx].script_name, total_users, api_table[api_id].num_values);
      }
      SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012121_ID, CAV_ERR_1012121_MSG, api_table[api_id].data_fname,
                                (api_table[api_id].sequence == USEONCE)?"USE-ONCE":"UNIQUE",
                                script_table[api_table[api_id].script_idx].script_name, total_users, api_table[api_id].num_values);
      return -1;  
    } else { //Redistribute file data among generators
      NIDL (4, "Need to redistribute leftover %d", leftover);
      redistribute_file_data(leftover, api_id);
    }
  }

  i = 0;
  while (leftover) 
  {
     if((((!runtime) || (is_generator_alive(i))) && (per_gen_api_table[(i * total_api_entries) + api_id].num_script_users))) {
        per_gen_api_table[(i * total_api_entries) + api_id].num_val++;
        leftover--;
    }
    i++;
    if (i == sgrp_used_genrator_entries) i = 0;
 }

  used_val = 0;
  for (i = 0; i < sgrp_used_genrator_entries; i++) 
  {
    if (!(per_gen_api_table[(i * total_api_entries) + api_id].num_script_users)) 
    {
      per_gen_api_table[(i * total_api_entries) + api_id].num_val = 0;
      per_gen_api_table[(i * total_api_entries) + api_id].start_val = 0;
      continue;
    }
    per_gen_api_table[(i * total_api_entries) + api_id].start_val = used_val;
    num_val = per_gen_api_table[(i * total_api_entries) + api_id].num_val;
    per_gen_api_table[(i * total_api_entries) + api_id].total_val = num_val;
    used_val += num_val;
    per_gen_api_table[(i * total_api_entries) + api_id].used_val = used_val;
    NIDL (4, "index = %d, num_val = %d, start_val = %d, total_val = %d, used_val =%d, data file = %s", 
             i, per_gen_api_table[(i * total_api_entries) + api_id].num_val, 
             per_gen_api_table[(i * total_api_entries) + api_id].start_val, 
             per_gen_api_table[(i * total_api_entries) + api_id].total_val, 
             per_gen_api_table[(i * total_api_entries) + api_id].used_val, api_table[api_id].data_fname);
  }
  return 0;
}

void copy_data_dir_files()
{
  char data_dir_file_path[FILE_PATH_SIZE], cmd[4096];
  int data_dir_path_len;
  char data_dir_path[FILE_PATH_SIZE];
  int api_id, ret;
  char data_dir_name[FILE_PATH_SIZE];
  char *tmp;

  NIDL(1, "Method called. total_api_entries = %d", total_api_entries);

  data_dir_path_len = sprintf(data_dir_path, "%s/data/", GET_NS_TA_DIR());

  for (api_id = 0; api_id < total_api_entries; api_id++) 
  {
    NIDL (4, "API Details: API Index = %d, Data file = %s, Sequence = %d, controller_dir = [%s], "
             "netomni_proj_subproj_file = %s, data_file_path = [%s], abs_or_relative_data_file_path = %d, data_grp_idx = %d, " 
             "data_dir_path_len = %d", api_id, api_table[api_id].data_fname, api_table[api_id].sequence, controller_dir, 
             netomni_proj_subproj_file, api_table[api_id].data_file_path, api_table[api_id].abs_or_relative_data_file_path,
             api_table[api_id].data_dir_idx, data_dir_path_len);

    if(api_table[api_id].abs_or_relative_data_file_path || 
       strncmp(api_table[api_id].data_fname, data_dir_path, data_dir_path_len) ||
       (api_table[api_id].sequence == UNIQUE) || (api_table[api_id].sequence == USEONCE))
        continue;

    //home/cavisson/work/workspace/ayush/system/cavisson/data/new/new1/abc.txt
    strcpy(data_dir_name, api_table[api_id].data_fname + data_dir_path_len);
    tmp = strrchr(data_dir_name, '/');
    *tmp = '\0';
    if(api_table[api_id].data_dir_idx != -1)
    {
      char file_dir_name[1024+1];
      tmp = strchr(data_dir_name, '/');
      if(tmp)
        strcpy(file_dir_name, tmp);
      else
        strcpy(file_dir_name, "");
      strcpy(data_dir_name, g_data_dir_table[api_table[api_id].data_dir_idx]);
      strcat(data_dir_name, file_dir_name);
    }
    sprintf(data_dir_file_path, "%s/rel/data/%s", controller_dir, data_dir_name);
    sprintf(cmd, "mkdir -p %s", data_dir_file_path);
 
    NIDL (1, "data_dir_file_path = %s", data_dir_file_path);
    ret = system(cmd);
 
    if(WEXITSTATUS(ret) == 1) {
        NS_EXIT(-1, "Failed to create generator wise data file in directory %s", data_dir_file_path);
    }

    sprintf(cmd, "cp %s %s", api_table[api_id].data_fname, data_dir_file_path);

    int ret = system(cmd);

    if(WEXITSTATUS(ret) == 1) {
        NS_EXIT(-1, "Failed to copy data file in directory %s", data_dir_file_path);
    }
  }  
}

/* Description: Distribute File parameter API among generators
 		This function performs following tasks:
                1) Update script table with respect to scenario group it belongs to
                2) Allocate memory to table which holds number of script users per generator
                3) Allocate memory to PerGenAPITable which holds Parameter API value data for distribution
		   among generators
 */
int ni_divide_values_per_generator(int runtime)
{
  int i, j;
  int total_vals;
  int total_script_users;
  int seq;
  time_t now;
 
  NIDL (1, "Method called, runtime = %d", runtime);

  if(!runtime)
  {
    /* Update script table*/
    fill_group_details_in_script_table();

    /*Create table to hold script users per generator and generator id*/
    create_tbl_per_grp();

    /*Sort script table with respect to scenario group index*/ 
    qsort(script_table, total_script_entries, sizeof(ScriptTable), comp_by_sgroupid_script);
  }

  /*PerGenAPITable which holds Parameter API value data for distribution among generators*/
  //per_gen_api_table = (PerGenAPITable *)malloc(sizeof(PerGenAPITable) * sgrp_used_genrator_entries * total_api_entries);
  NSLB_MALLOC(per_gen_api_table, (sizeof(PerGenAPITable) * sgrp_used_genrator_entries * total_api_entries), "per gen api table", -1, NULL);

  NIDL (2, "sgrp_used_genrator_entries = %d, total_api_entries = %d", sgrp_used_genrator_entries, total_api_entries);

  for (i = 0; i < sgrp_used_genrator_entries; i++) 
  {
    if((runtime && (!is_generator_alive(i))))
      continue;
    for (j = 0; j < total_api_entries; j++)
    {
      per_gen_api_table[(i * total_api_entries) + j].num_script_users =
                get_per_gen_num_script_users(i, script_table[api_table[j].script_idx].script_name);
      NIDL (2, "num_script_users = %d", per_gen_api_table[(i * total_api_entries) + j].num_script_users);
      per_gen_api_table[(i * total_api_entries) + j].start_val = 0;
      per_gen_api_table[(i * total_api_entries) + j].num_val = 0;
    }
  } 

  for (i = 0; i < sgrp_used_genrator_entries; i++) 
  {
    if((runtime && (!is_generator_alive(i))))
      continue;
    for (j = 0; j < total_api_entries; j++) 
    {
      total_script_users = script_table[api_table[j].script_idx].script_total;
      total_vals = api_table[j].num_values;
      seq = api_table[j].sequence;

      NIDL (2, "seq = %d, total_script_users = %d, total_vals = %d", seq, total_script_users, total_vals);

      if (seq == UNIQUE) 
      {
        if(distribute_values(total_script_users, j, runtime) == -1)
          return -1;
      } 
      else if (seq == USEONCE) 
      {
#if 0
        NIDL (2, "total_vals = %d", total_vals);
        if(total_vals < sgrp_used_genrator_entries)
        {
          fprintf(stderr, "Total numbers of values (%d) in data file (%s) of script (%s) is less than the total number of generators (%d). Aborting the test run ...\n", total_vals, api_table[j].data_fname, script_table[api_table[j].script_idx].script_name, sgrp_used_genrator_entries);
          exit(-1);
        }
        else
#endif
          NIDL (2, "total_script_users = %d, j = %d", total_script_users, j);
          if(distribute_values(total_script_users, j, runtime) == -1)
            return -1;
      }
      else //Handle seq, random and weighted random case
      {
        per_gen_api_table[(i * total_api_entries) + j].start_val = 0;
        per_gen_api_table[(i * total_api_entries) + j].num_val = total_vals;
        per_gen_api_table[(i * total_api_entries) + j].total_val = total_vals;
      }
    }
  }

  time(&now);

  NIDL(1, "Start time for create data files =%s", ctime(&now));

  if(divide_data_file_values(runtime) == -1)
    return -1;

  time(&now);
  NIDL (1, "Exit method, End time for create data files =%s", ctime(&now));
  return 0;
}

/* Description: Create registration files
                a) Creates data files for each generator with their absolute path 
                These data files will be created with absolute path in respective generator directories 
                under TRXX/.controller folder
                b) Make new registration file for each generator using updated start val of unique range var
                inside respective generator directories
 */

int make_registration_file_for_generators(int runtime, char *err_msg)
{
  char reg_spec_file_path[FILE_PATH_SIZE], cmd[4096];
  char reg_spec_new_file[FILE_PATH_SIZE];
  char reg_spec_old_file[FILE_PATH_SIZE];
  char buffer[1024];
  char buf[MAX_DATA_LINE_LENGTH + 1], newbuf[MAX_DATA_LINE_LENGTH] = {0};
  char *fields[20 + 1];
  int gen_id, unq_id = 0;
  int i, total_flds = 0;
  FILE *new_reg_file_fp = NULL;
  FILE *old_reg_file_fp = NULL;
  int return_value;
 
  NIDL(1, "Method called, total_unique_range_entries = %d", total_unique_range_entries);
  for (gen_id = 0; gen_id < sgrp_used_genrator_entries; gen_id++)
  {
    for(unq_id = 0; unq_id < total_unique_range_entries;)
    {
      //Bug-101320
      sprintf(reg_spec_file_path, "%s/%s/rel/%s/%s/scripts/%s",
                                  controller_dir, generator_entry[gen_id].gen_name, script_table[uniquerangeTable[unq_id].sess_idx].proj_name,
                                  script_table[uniquerangeTable[unq_id].sess_idx].sub_proj_name, 
                                  script_table[uniquerangeTable[unq_id].sess_idx].script_name);

      NIDL(1, "Creating Path = %s", reg_spec_file_path);
      sprintf(cmd, "mkdir -p -m 0777 %s", reg_spec_file_path);
      
      return_value = system(cmd);
      if(WEXITSTATUS(return_value) == 1){
        sprintf(err_msg, "\nError in creating unique_var_data directory %s\n", reg_spec_file_path);
        return FAILURE_EXIT;
      }

      //creating new registrations.spec for generator
      sprintf(reg_spec_new_file, "%s/%s", reg_spec_file_path, REGISTRATION_FILENAME);
      if((new_reg_file_fp = fopen(reg_spec_new_file, "w+")) == NULL) {
        sprintf(err_msg, "\nError in creating new file %s, err = %s\n", reg_spec_new_file, nslb_strerror(errno));
        return FAILURE_EXIT;
      }

      //opening registrations.spec
      /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
      sprintf(reg_spec_old_file, "%s/%s/%s/scripts/%s/%s", GET_NS_TA_DIR(), script_table[uniquerangeTable[unq_id].sess_idx].proj_name, 
                                  script_table[uniquerangeTable[unq_id].sess_idx].sub_proj_name,
                                  script_table[uniquerangeTable[unq_id].sess_idx].script_name, REGISTRATION_FILENAME);
      if((old_reg_file_fp = fopen(reg_spec_old_file, "r")) == NULL) {
        sprintf(err_msg, "\nError in opening old file %s in read mode, err = %s\n", reg_spec_old_file, nslb_strerror(errno));
        return FAILURE_EXIT;
      }
       
      NIDL(3, "Reading from file %s and writing on file %s", reg_spec_old_file, reg_spec_new_file);

      while (nslb_fgets(buf, MAX_COMMAND_SIZE, old_reg_file_fp, 1) != NULL)
      {
        sprintf(buffer, "StartRange=\"%d\"", per_gen_api_table_unique_var[(gen_id * total_unique_range_entries) + unq_id].start_val);
        if((strchr(buf, '/') == NULL) && (strstr(buf, "nsl_unique_range_var")) != NULL)
        {
          total_flds = get_tokens_(buf, fields, ",", 20);
          for(i = 0; i < total_flds; i++)
          {
            if(i == 1) {
              strcat(newbuf, buffer);
              strcat(newbuf, ",");
              NIDL(1, "i = %d, buf = %s", i, newbuf);
              continue;
            }
            strcat(newbuf, fields[i]);
            if((strchr(fields[i], ';')) != NULL) break;
            strcat(newbuf, ",");
          }
          NIDL(2, "Api buf = %s", newbuf);
          fputs(newbuf, new_reg_file_fp);
          newbuf[0] = '\0';
          NIDL(2, "gen_id = %d, api_id = %d orig_start = %d, updated_start = %d", gen_id, unq_id, uniquerangeTable[unq_id].start,
                 per_gen_api_table_unique_var[(gen_id * total_unique_range_entries) + unq_id].start_val);
          //incrementing counter when API is found
          unq_id++;
        }
        else
        {
          NIDL(2, "skipping line = %s", buf);
          fputs(buf, new_reg_file_fp);
        }
      }
      //closing files
      fclose(new_reg_file_fp);
      fclose(old_reg_file_fp);
    }
  }
  return SUCCESS_EXIT;
}

/* Description: Distribute Unique range var API among generators
                This function performs following tasks:
                1) Allocate memory to PerGenAPITable which holds Parameter API value data for distribution
                   among generators
                2) Update start_val of unique range parameter of generator in PerGenAPITable
 */

int ni_divide_values_unique_var_per_generator(int runtime)
{
  char err_msg[2048] = {0};
  int i, m;
  int num_users;
  time_t now;

  NIDL(1, "Method called, sgrp_used_genrator_entries = %d, total_unique_range_entries = %d", 
           sgrp_used_genrator_entries, total_unique_range_entries);

  /*Create table to hold script users per generator and generator id*/

  //per_gen_api_table_unique_var = (PerGenAPITable *)malloc(sizeof(PerGenAPITable) * sgrp_used_genrator_entries * total_unique_range_entries);
  NSLB_MALLOC(per_gen_api_table_unique_var, (sizeof(PerGenAPITable) * sgrp_used_genrator_entries * total_unique_range_entries), "per_gen_api_table_unique_var", -1, NULL);

  for (i = 0; i < sgrp_used_genrator_entries; i++)
  {
    for (m = 0; m < total_unique_range_entries; m++)
    {
      per_gen_api_table_unique_var[(i * total_unique_range_entries) + m].num_script_users =
               get_per_gen_num_script_users(i, script_table[uniquerangeTable[m].sess_idx].script_name);

      NIDL(3, "gen id = %d, api id = %d, sess_idx = %d, script_name = %s, script_users = %d", i, m, uniquerangeTable[m].sess_idx,
               script_table[uniquerangeTable[m].sess_idx].script_name,
               per_gen_api_table_unique_var[(i * total_unique_range_entries) + m].num_script_users);

      per_gen_api_table_unique_var[(i * total_unique_range_entries) + m].start_val = 0;
    }
  }
  
  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    for (m = 0; m < total_unique_range_entries; m++)
    {
      num_users = per_gen_api_table_unique_var[(i * total_unique_range_entries) + m].num_script_users; 
      NIDL(4, "users = %d", num_users);
      if(!num_users) 
      {
        per_gen_api_table_unique_var[(i * total_unique_range_entries) + m].start_val = uniquerangeTable[m].start;
        continue;
      }

      per_gen_api_table_unique_var[(i * total_unique_range_entries) + m].start_val = uniquerangeTable[m].start + (uniquerangeTable[m].block_size * i * num_users);
      NIDL(3, "gen_id = %d, api_id = %d, orig_start = %ld, updated_start_val = %d", i, m, uniquerangeTable[m].start,
               per_gen_api_table_unique_var[(i * total_unique_range_entries) + m].start_val);
    }
  }
  
  time(&now);
  NIDL(1, "Start time for making registration files =%s", ctime(&now));
  
  if(make_registration_file_for_generators(runtime, err_msg) != SUCCESS_EXIT) {
    NS_EXIT(FAILURE_EXIT, "%s", err_msg);
  }

  time(&now);
  NIDL (1, "Exit method, End time for making registration files =%s", ctime(&now));

  return SUCCESS_EXIT;
}


static void get_ctrl_file_name(char *data_fname, char *ctrl_file)
{
  char locl_data_fname[5 * 1024];
  char locl_data_fname2[5 * 1024];

  NIDL (1, "Method called. Data file name = %s", data_fname);

  locl_data_fname[0] = '\0';
  locl_data_fname2[0] = '\0';

  strcpy(locl_data_fname, data_fname);
  strcpy(locl_data_fname2, data_fname);

  sprintf(ctrl_file, "%s/.%s.gen.control", dirname(locl_data_fname), basename(locl_data_fname2));

  NIDL (1, "Control file name = %s", ctrl_file);
}

/* Create control files for each data file used in file parameter API for USE-ONCE option
   File format will be similar to NS where parent creates control file for each data file with
   following details 
   testrun, total number of NVM, UseOnceOptType, num values per NVM
   Naming convention: 
   <Directory>/<Filename>.gen.control
   Format: 
   Test index Total number of Generator   0   Total Values per generator 
   **Here UseOnceOptType is not used hence will be writing hardcode value 0 just for sake
   of common parsing mechanism for both NS and NC
*/
void create_ctrl_files()
{
  int api_id, j;
  char ctrl_data_file[5 *1024];
  FILE *fp_ctrl = NULL;
  char buf[30 * 255];
  
  NIDL (1, "Method called, API entries = %d", total_api_entries);
  
  for (api_id = 0; api_id < total_api_entries; api_id++)
  {
    if((api_table[api_id].sequence != USEONCE) || (api_table[api_id].is_sql_var))
    {
      NIDL(4, "Sequence is not USE_ONCE/API is NS_SQL_VAR. Hence continue...");
      continue;
    }
    get_ctrl_file_name(api_table[api_id].data_fname, ctrl_data_file);
    if ((fp_ctrl =  fopen(ctrl_data_file, "w")) == NULL)
    {
      NS_EXIT(FAILURE_EXIT, "Error in opening control file %s, error %s", ctrl_data_file, nslb_strerror(errno));
    }
    chmod(ctrl_data_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    
    NIDL (4, "Control file  = %s, line = [%d|%d|0|%d]", ctrl_data_file, test_run_num, sgrp_used_genrator_entries, api_table[api_id].num_values);
    //Format: Test_run_number Num_Gen 0 Num_values_per_gen
    fprintf(fp_ctrl, "%d|%d|0|%d\n", test_run_num, sgrp_used_genrator_entries, api_table[api_id].num_values);
    for(j = 0; j < sgrp_used_genrator_entries; j++) {
      sprintf(buf, "%s%d,%d|", buf, per_gen_api_table[(j * total_api_entries) + api_id].start_val,
             per_gen_api_table[(j * total_api_entries) + api_id].num_val);
    }

    int to_write = strlen(buf);
    buf[to_write - 1] = '\n';
    buf[to_write] = '\0';

    fprintf(fp_ctrl,"%s", buf);
    fclose(fp_ctrl);
    buf[0] = '\0'; //to clear extra lines 
  }

  NIDL (1, "Exit method.");
}
