/********************************************************************************************************************
 * File Name      : ns_cmd_var.c                                                                                    |
 |                                                                                                                  | 
 * Synopsis       : This file contain all the functions related to Dynamic Data Parameter                           | 
 |                                                                                                                  |
 * Author(s)      : Nisha                                                                                           |
 |                                                                                                                  |
 * Date           : Sat June 1 2019                                                                                 |
 |                                                                                                                  |
 * Copyright      : (c) Cavisson Systems                                                                            |
 |                                                                                                                  |
 * Mod. History   :                                                                                                 |
 *******************************************************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <sql.h>
#include <sqlext.h>
#include<sys/socket.h>
#include<errno.h>
#include<netdb.h>
#include<arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#include "util.h"
#include "ns_log.h"
#include "ns_sql_vars.h"
#include "ns_alloc.h"
#include "ns_static_vars.h"
#include "ns_trace_level.h"
#include "nslb_sock.h"
#include "ns_parent.h"
#include "ns_exit.h"
#include "ns_cmd_vars.h"
#include "ns_script_parse.h"

static int set_param_for_cmd_persist_flag(char* value, CMDVarTableEntry* cmdVarTable, char* script_file_msg, int* state, short int* done, int group_idx)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  
  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Command Var");
  }
  
  if ((*done & CMD_PARAM_PERSIST_FLAG_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s  'Persist' can be given only once.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Persist", "Command Var");
  }
  
  char is_save_to_file[16] = {0};
  if(value[0] != '\0')
    strcpy(is_save_to_file, value);
  else
    strcpy(is_save_to_file, "NO");
  
  if((strcasecmp(is_save_to_file, "Yes")) && (strcasecmp(is_save_to_file, "No")))
  { 
    NSTL1_OUT(NULL, NULL, "%s  Value of filed 'IsSaveToFile' is '%s'.It's value can be either Yes or No.\n", script_file_msg, is_save_to_file);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, is_save_to_file, "IsSaveToFile", "Command Var");
  }
  
  if ((cmdVarTable->persist_flag = copy_into_big_buf(is_save_to_file, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, is_save_to_file);
  }
  
  NSDL3_VARS(NULL, NULL, "Persist Flag =  %s", RETRIEVE_BUFFER_DATA(cmdVarTable->persist_flag));
  
  *state = NS_ST_STAT_VAR_OPTIONS;
  if(!strcasecmp(is_save_to_file, "Yes"))
    groupTable[group_idx].persist_flag = 1;

  *done |= CMD_PARAM_PERSIST_FLAG_PARSING_DONE;
  return 0;
}

static int set_cmd_param_for_prog(char* value, CMDVarTableEntry* cmdVarTable, char* script_file_msg, int* state, short int* done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  
  char buffer[512] = {0};
  
  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "Command Var");
  }
  
  if ((*done & CMD_PARAM_PROG_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s 'Command' can be given only once.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Command", "Command Var");
  }
  
  if(value[0] == '\0'){
    NSTL1_OUT(NULL, NULL, "%s Value for field 'Command' must be given.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Command", "Command Var");
  }

  strcpy(buffer, value);
  if ((cmdVarTable->prog_with_args = copy_into_big_buf(buffer, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy prog_with_args into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, buffer);
  }

  NSDL3_VARS(NULL, NULL, "Command =  %s", RETRIEVE_BUFFER_DATA(cmdVarTable->prog_with_args));
   
  *state = NS_ST_STAT_VAR_OPTIONS;
  *done |= CMD_PARAM_PROG_PARSING_DONE;

  return 0;
}

int parse_cmd_var_params(char* keyword, char* value, CMDVarTableEntry *cmdVarTable, int* state, char* script_file_msg, short int *done, char* staticvar_buf, int* group_idx, int* var_start, int sess_idx, int* rw_data, int* create_group, int* var_idx, int* column_flag, unsigned short* reltv_var_idx)
{
  int exitStatus;

  NSDL1_VARS(NULL, NULL, "Method called");

  if(!strcasecmp(keyword, "IsSaveToFile"))
  {
    set_param_for_cmd_persist_flag(value, cmdVarTable, script_file_msg, state, done, *group_idx);
  }
  else if(!strcasecmp(keyword, "Command"))
  {
    set_cmd_param_for_prog(value, cmdVarTable, script_file_msg, state, done);
  }
  else
  {
    NSDL1_VARS(NULL, NULL, "Keyword = %s", keyword);
    exitStatus = set_last_option(value, keyword, staticvar_buf, group_idx, var_start, sess_idx, script_file_msg, state, rw_data, create_group, var_idx, column_flag, *reltv_var_idx);
    *reltv_var_idx = *reltv_var_idx + 1;
    if(exitStatus == -1)
      return exitStatus;
  }
  return 0;
}

int ns_cmd_param_validate_keywords(short int* cmd_args_done_flag, char* script_file_msg)
{
  /*if(!(*dyn_args_done_flag & DYN_PARAM_OUTFILE_NAME_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s Outfile name is not given. It is mandatory field for this API.\n", script_file_msg);
    return -1;
  }*/
  /*if(!(*cmd_args_done_flag & CMD_PARAM_PERSIST_FLAG_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s Persist flag is not given. It is mandatory field for this API.\n", script_file_msg);
    return -1; 
  }*/
  if(!(*cmd_args_done_flag & CMD_PARAM_PROG_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s Command is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Command", "Command Var");
  }
  return 0;
}

int ns_cmd_var_save_prog_output_to_file(char *output, char* file_name, GroupTableEntry* groupTable, int group_idx)
{   
  int fd;
  char tmp_file_name[512] = {0};
  char *path = NULL;
  char cmd[1024] = {0};
  char err_msg[1024] = "\0";

  NSDL2_VARS(NULL, NULL, "Method called file_name = %s, groupTable = %p, group_idx = %d", file_name, groupTable, group_idx);
    
  if(groupTable[group_idx].absolute_path_flag != 1)
  {   
    strcpy(tmp_file_name, file_name);
    path = dirname(tmp_file_name);
    if(!path)
    {   
      NSDL2_VARS(NULL, NULL, "Unable to get dirname of path = %s", file_name);
      return -1;
    }
    NSDL2_VARS(NULL, NULL, "path = %s", path);
    sprintf(cmd, "mkdir -p %s; chmod 0775 %s", path, path);
 
    if(nslb_system(cmd,1,err_msg) != 0) {
       NSTL1_OUT(NULL, NULL, "Unable to create directory '%s'. %s", path, err_msg);
    }

  }

  if((fd = open(file_name, O_WRONLY|O_CREAT|O_CLOEXEC, 0666)) == -1)
  {
     NSTL1_OUT(NULL, NULL, "Unable to open file %s, due to error: %s\n", file_name, nslb_strerror(errno));
     return -1;
  }

  if(write(fd, output, strlen(output)) == -1)
  {
    NSTL1_OUT(NULL, NULL, "Unable to save command result into file %s, due to error: %s\n", file_name, nslb_strerror(errno));
    return -1;
  }
 
  close(fd);
 
  return 0;
}

int run_cmd_and_get_output(char *cmd, char **output, long int *prog_result_buff_size)
{
  char tmp[NS_CMD_MAX_LEN + 1];
  int amt_written =0 ;
  FILE *app = NULL;
  int read_bytes;

  write_log_file(NS_SCENARIO_PARSING, "Going to run command %s", cmd);

  app = popen(cmd, "r");

  if(app == NULL)
  { 
    fprintf(stderr, "ERROR: Error in executing command [%s]. Error = %s\n", cmd , nslb_strerror(errno));
    //write_log_file(NS_SCENARIO_PARSING, "Error in executing command %s", cmd);
    return -1;
  }
  
  while(!feof(app))
  {
    read_bytes = fread(tmp, 1, NS_CMD_MAX_LEN,  app);
    if(read_bytes <= 0)
    {
      pclose(app);
      fprintf(stderr, "ERROR: fread() NO DATA. Error = %s\n", nslb_strerror(errno));
      return -1;
    }
    tmp[read_bytes] = '\0';

    if((amt_written + read_bytes) >= *prog_result_buff_size)
    {
      *prog_result_buff_size += NS_CMD_PARAM_MAX_PROG_OUTPUT_LEN;
      MY_REALLOC(*output, *prog_result_buff_size + 1, "cmd output reallocation of memory", -1);

      write_log_file(NS_SCENARIO_PARSING, "Command output size %d is greater than buffer size. Hence reallocating buffer with %d size", 
                                           *prog_result_buff_size, NS_CMD_PARAM_MAX_PROG_OUTPUT_LEN);
    }

    amt_written += snprintf(*output + amt_written , *prog_result_buff_size - amt_written + 1, "%s", tmp);
  }   

  write_log_file(NS_SCENARIO_PARSING, "Completed execution of command %s", cmd);

  if(pclose(app) == -1)
    fprintf(stderr,"ERROR : pclose() FAILED [%s]\n", nslb_strerror(errno));
  
  return 0;  
}

int ns_cmd_var_get_data_from_prog(CMDVarTableEntry* cmdVarTable, char* column_delimiter, char *err_msg, int grp_idx, char *file_name, GroupTableEntry* groupTable)
{
  int ret;

  NSDL3_VARS(NULL, NULL, "Method Called, cmdVarTable = %p", cmdVarTable);

  MY_MALLOC(cmdVarTable->prog_result, NS_CMD_PARAM_MAX_PROG_OUTPUT_LEN, "Allocate memory for cmdVarTable->prog_result", -1);
  cmdVarTable->prog_result_buff_size = NS_CMD_PARAM_MAX_PROG_OUTPUT_LEN;

  ret = run_cmd_and_get_output(RETRIEVE_BUFFER_DATA(cmdVarTable->prog_with_args), &cmdVarTable->prog_result, &cmdVarTable->prog_result_buff_size); 

  NSDL3_VARS(NULL, NULL, "flag = %s", RETRIEVE_BUFFER_DATA(cmdVarTable->persist_flag));

  if(ret == -1)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012092_ID, CAV_ERR_1012092_MSG, cmdVarTable->prog_with_args);
  }
  else if(!strcasecmp(RETRIEVE_BUFFER_DATA(cmdVarTable->persist_flag), "Yes"))
  {
    NSDL3_VARS(NULL, NULL, "Persist flag is set. Going to write data to file = %s", file_name);

    ns_cmd_var_save_prog_output_to_file(cmdVarTable->prog_result, file_name, groupTable, grp_idx);
  }
  return 0;
}
