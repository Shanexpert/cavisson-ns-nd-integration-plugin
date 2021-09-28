/********************************************************************************************************************
 * File Name      : ns_static_vars_rtc.c                                                                            |
 |                                                                                                                  | 
 * Synopsis       : This file contains all the file which take participation in File Parameter RunTimeChanges       |  
 |                                                                                                                  |
 * Author(s)      : Manish Kumar Mishra                                                                             |
 |                                                                                                                  |
 * Date           : Wed Sep  7 00:12:41 IST 2016                                                                    | 
 |                                                                                                                  |
 * Copyright      : (c) Cavisson Systems                                                                            |
 |                                                                                                                  |
 * Mod. History   :                                                                                                 |
 *******************************************************************************************************************/

#define _GNU_SOURCE 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#include "nslb_util.h"
#include "nslb_big_buf.h"
#include "nslb_static_var_use_once.h"

#include "netomni/src/core/ni_script_parse.h"
#include "netomni/src/core/ni_scenario_distribution.h"

#include "ns_static_use_once.h"
#include "nslb_time_stamp.h"
#include "ns_common.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"

#include "util.h"
#include "ns_test_gdf.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_static_vars.h"
#include "wait_forever.h"
#include "ns_msg_com_util.h"
#include "ns_runtime_changes.h"
#include "ns_static_vars_rtc.h"

#include "netstorm.h"
#include "child_init.h"
#include "unique_vals.h"
#include "ns_child_msg_com.h"
#include "ns_master_agent.h"
#include "ns_trace_level.h"
#include "ns_data_handler_thread.h"
#include "ns_runtime.h"
#include "ns_parent.h"
#include "ns_script_parse.h"

int total_fparam_rtc_tbl_entries = 0;
int all_nvms_done = 0;
int all_nvms_sent[MAX_NVM_NUM]={0};
FileParamRTCTable *fparam_rtc_tbl = NULL;
//int fparam_rtc_done_successfully;
//FILE *rtclog_fp = NULL;
int is_global_vars_reset_done = 0;
/* To handle file parameter runtime changes */
//Msg_com_con *fpram_rtc_mccptr = NULL;
//char fp_rtc_err_msg[1024] = {0};

/* prototype */
int send_attach_shm_msg_to_nvms();
extern int g_rtc_msg_seq_num;

//Bug 39255: This is wrapper of dump_rtc_group_table_values() in case of netcloud
#define WRITE_RTC_DATA_INTO_LOG_FILE() \
{\
  int i, grp_idx;\
  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)\
  {\
    grp_idx = fparam_rtc_tbl[i].fparam_grp_idx;\
    if (grp_idx != -1)\
    {\
      snprintf(rtcdata->msg_buff, RTC_QTY_BUFFER_SIZE, "RTCMode = %s, Script=%s, OrignalDataFile=%s, OldValues = %d, NewDataFile=%s," \
                         " NewValues = %d, TotalValues=%d, Mode=%s, Refresh=%s\n", (fparam_rtc_tbl[i].mode == 1)?"APPEND":"REPLACE",\
                         group_table_shr_mem[grp_idx].sess_name, group_table_shr_mem[grp_idx].data_fname,\
                         group_table_shr_mem[grp_idx].num_values, fparam_rtc_tbl[i].data_file_list, fparam_rtc_tbl[i].num_values,\
                         (fparam_rtc_tbl[i].mode == 1)?(group_table_shr_mem[grp_idx].num_values + fparam_rtc_tbl[i].num_values):\
                         fparam_rtc_tbl[i].num_values,\
                         find_mode_from_seq_number(group_table_shr_mem[grp_idx].sequence),\
                         find_type_from_type_number(group_table_shr_mem[grp_idx].type));\
      RUNTIME_UPDATE_LOG(rtcdata->msg_buff)  \
    }\
  }\
}

/* Warning came for function
static char *get_cur_time()
{
  time_t    tloc;
  struct  tm *lt;
  static  char cur_time[100];

  (void)time(&tloc);
  if((lt = localtime(&tloc)) == (struct tm *)NULL)
    strcpy(cur_time, "Error");
  else
    sprintf(cur_time, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_time);
}*/

/*--------------------------------------------------------------------------------------------------
 * Function name    : is_rtc_applied_on_group() 
 *
 * Synopsis         : 
 *                    
 *
 * Input            :
 *
 *
 * Output           : 
 *                  : 
 *         
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
int is_rtc_applied_on_group(int num_users, int grp_idx)
{
  int i;
  
  NSDL2_RUNTIME(NULL, NULL, "Method called, num_user = %d, grp_idx = %d", num_users, grp_idx);
  if(num_users == 0)
    return 0;

  for(i=0; i < total_fparam_rtc_tbl_entries; i++)
  {
    NSDL2_RUNTIME(NULL, NULL, "i = %d, fparam_grp_idx = %d, grp_idx = %d", i, fparam_rtc_tbl[i].fparam_grp_idx, grp_idx); 
    if(fparam_rtc_tbl[i].fparam_grp_idx == grp_idx)
      return i + 1;
  }

  return 0;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : get_sess_id() 
 *
 * Synopsis         : 
 *                    
 *
 * Input            :
 *
 *
 * Output           : 
 *                  : 
 *         
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
static int get_sess_id(char *sess_name)
{
  int sess_idx = 0;
  for (sess_idx = 0; sess_idx < total_sess_entries; sess_idx++) 
  {
    //if (strcmp(session_table_shr_mem[sess_idx].sess_name, sess_name) == 0)
    if (strcmp(get_sess_name_with_proj_subproj_int(session_table_shr_mem[sess_idx].sess_name, sess_idx, "/"), sess_name) == 0){
      return sess_idx;
    }
  }
  
  return -1;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : get_fparam_id() 
 *
 * Synopsis         : 
 *                    
 *
 * Input            :
 *
 *
 * Output           : 
 *                  : 
 *         
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
static int get_fparam_id(char *param_name, int len, int sess_idx)
{
  int fparam_idx = -1;
  int hash_code = -1;

  hash_code = session_table_shr_mem[sess_idx].var_hash_func(param_name, len);

  if(hash_code < 0)
    return -1;

  fparam_idx = session_table_shr_mem[sess_idx].vars_trans_table_shr_mem[hash_code].fparam_grp_idx;

  return fparam_idx;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : create_and_fill_fparam_rtc_tbl() 
 | 
 * Synopsis         : This function will parse nsi_rtc_invoker message and create table fparam_rtc_tbl  
 |
 * Input            : msg     - message get from tool nsi_rtc_invoker 
 |                              message format -
 |                                mode|script_name:fparam_group_name|file_list;..
 |                                Example: 
 |                                  0|S1:v1|v1f1,v1f2;0|S1:v2|v2f3;1|S2:v1|v1f1;1|S1:v1|v1f4
 |
 |                    err_msg - buffer to fill error message
 |
 * Output           : Create table fparam_rtc_tbl, set total_fparam_rtc_tbl_entries and fill  
 |                    its member
 |
 * Return Value     : 0 - On success 
 |                   -1 - On Failure  
 |
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
static int create_and_fill_fparam_rtc_tbl(char *msg)
{
  char buf[MAX_FPRAM_BUF_SIZE + 1];
  char tmp_buf[1024 + 1];
  char sess_name[1024 + 1];
  int fprtc_idx = 0;
  int len = 0;
  int next_field = 0;
  char *cur_ptr, *next_ptr, *end_ptr, *tmp_ptr;

  cur_ptr = next_ptr = end_ptr = tmp_ptr = NULL;
  cur_ptr = msg;

  NSDL2_RUNTIME(NULL, NULL, "Method called, msg = [%s]", msg);  

  //1. Get total number of fparam_rtc_tbl_entries 
  while(*cur_ptr && ((next_ptr = strchr(cur_ptr, ';')) != NULL)) 
  {
    total_fparam_rtc_tbl_entries++;
    cur_ptr = next_ptr + 1;
  }

  total_fparam_rtc_tbl_entries++; //Since in last entry no semicolon exist

  NSDL2_RUNTIME(NULL, NULL, "Create fparam_rtc_tbl table, total_fparam_rtc_tbl_entries = %d", total_fparam_rtc_tbl_entries);  
  
  //2. Create fparam_rtc_tbl
  RTC_MALLOC_AND_MEMSET(fparam_rtc_tbl, sizeof(FileParamRTCTable) * total_fparam_rtc_tbl_entries, "fparam_rtc_tbl", -1);

  //3. Parse messgae and fill fparam_rtc_tbl
  cur_ptr = msg; 
  while(*cur_ptr && (fprtc_idx < total_fparam_rtc_tbl_entries)) //0|S1:v1|v1f1,v1f2
  {
    next_ptr = strchr(cur_ptr, ';');
    if(next_ptr == NULL) //handle last entry
      next_ptr = msg + strlen(msg);

    if(next_ptr == NULL) break;
    NSDL2_RUNTIME(NULL, NULL, "next_ptr = [%s]", next_ptr);

    next_field = FIELD_MODE;
    while(next_field != FIELD_DONE)      
    {
      end_ptr = strchr(cur_ptr, '|');
      if((end_ptr == NULL) || (end_ptr > next_ptr))
        end_ptr = next_ptr;

      len = end_ptr - cur_ptr;
      if(len <= 0)
      {
        NSDL2_RUNTIME(NULL, NULL, "Error: create_and_fill_fparam_rtc_tbl() - msg %s not in proper format, mode is missing.", msg);
        snprintf(rtcdata->err_msg, 1024, "Message :- %s is not in proper format, mode is missing, it is a mandatory argument.", msg);
        return -1;
      }
     
      strncpy(buf, cur_ptr, len); 
      buf[len] = 0;  
     
      NSDL2_RUNTIME(NULL, NULL, "next_field = %d, len = %d, buf = %s", next_field, len, buf);
      if(next_field == FIELD_MODE) //0 
      {
        if(ns_is_numeric(buf) == 0)
        {
          snprintf(rtcdata->err_msg, 1024, "Error: Mode must be numeric, but it is given '%s'.", buf);
          return -1;
        }

        fparam_rtc_tbl[fprtc_idx].mode = atoi(buf);
        if(fparam_rtc_tbl[fprtc_idx].mode != 1 && fparam_rtc_tbl[fprtc_idx].mode != 2)
        {
          snprintf(rtcdata->err_msg, 1024, "Error: Mode can be either 1 or 2, but given value is %s.", buf);
          return -1;
        }
        next_field = FIELD_FPARAM;
      }
      else if(next_field == FIELD_FPARAM) //S1:v1
      {
        tmp_ptr = strchr(cur_ptr, ':'); 
        if((tmp_ptr == NULL) || (tmp_ptr > next_ptr))
        {
          snprintf(rtcdata->err_msg, 1024, "Error: Script name is missing.");
          return -1;
        }

        len = tmp_ptr - cur_ptr;
        strncpy(tmp_buf, cur_ptr, len);
        tmp_buf[len] = '\0';

        strcpy(sess_name, tmp_buf);
        fparam_rtc_tbl[fprtc_idx].sess_idx = get_sess_id(tmp_buf);
        if(fparam_rtc_tbl[fprtc_idx].sess_idx == -1)
        {
          snprintf(rtcdata->err_msg, 1024, "Error: Script %s is not defined.", tmp_buf);
          return -1;
        }

        cur_ptr = tmp_ptr + 1;
        if(cur_ptr >= end_ptr)
        {
          NSDL2_RUNTIME(NULL, NULL, "Error: create_and_fill_fparam_rtc_tbl() - FIELD_FPARAM not found - file param group name missed.");
          snprintf(rtcdata->err_msg, 1024, "Error: File param group name is not given.");
          return -1;
        }
        len = end_ptr - cur_ptr; 
        strncpy(tmp_buf, cur_ptr, len);
        tmp_buf[len] = '\0';

        fparam_rtc_tbl[fprtc_idx].fparam_grp_idx = get_fparam_id(tmp_buf, len, fparam_rtc_tbl[fprtc_idx].sess_idx);
        if(fparam_rtc_tbl[fprtc_idx].fparam_grp_idx == -1)
        {
          snprintf(rtcdata->err_msg, 1024, "Error: File parameter group %s is not defined.", tmp_buf);
          return -1;
        }
        next_field = FIELD_DATA_FILE;
      }
      else if(next_field == FIELD_DATA_FILE) //v1f1,v1f2
      {
        fparam_rtc_tbl[fprtc_idx].data_file_list = (char *)malloc(len + 1);
        if(fparam_rtc_tbl[fprtc_idx].data_file_list == NULL)
        {
          snprintf(rtcdata->err_msg, 1024, "Error: create_and_fill_fparam_rtc_tbl() - malloc failed for data file list of len %d.", len);
          return -1;
        }

        strcpy(fparam_rtc_tbl[fprtc_idx].data_file_list, buf);
        next_field = FIELD_DONE;
      }
      else
      {
        snprintf(rtcdata->err_msg, 1024, "Error: Message :- %s is not proper format.", buf);
        return -1;
      }
      cur_ptr = end_ptr + 1;
    }
    fprtc_idx++;
    next_ptr++; //skip ;
  }  

  NSDL2_RUNTIME(NULL, NULL, "fprtc_idx = %d", fprtc_idx);

  return 0;
}

#if 0
/*--------------------------------------------------------------------------------------------------
 * Function name    : strlcpy() 
 | 
 * Synopsis         : 
 |
 * Input            : 
 |
 * Output           : 
 |
 * Return Value     : 
 |
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
static int strlcpy(char *dest, char *src, int nline)
{
  int tot_wbytes = 0; 
  int tot_wline = 0; 
  char *rptr = NULL;

  rptr = src;
  
  NSDL2_RUNTIME(NULL, NULL, "Method called, nline = %d", nline); 
  while(*rptr && (tot_wline < nline))
  {
    if(*rptr == '\n')
      tot_wline++;

    rptr++;
  }

  tot_wbytes = rptr - src;

  strncpy(dest, src, tot_wbytes);

  NSDL2_RUNTIME(NULL, NULL, "tot_wbytes = %d, dest = %*.*s\n", tot_wbytes, tot_wbytes, tot_wbytes, dest);

  return tot_wbytes;
}
#endif

/*--------------------------------------------------------------------------------------------------
 * Function name    : bufwrite_by_files() 
 | 
 * Synopsis         : 
 |
 * Input            : 
 |
 * Output           : 
 |
 * Return Value     : 
 |
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
static int bufwrite_by_files(char *fnames, char *fpath, char *token, char **obuf, int *obuf_size)
{
  char abs_fname[1024];
  int num_files = 0;
  int max_files = 10;
  int i, fd;
  int obuf_offset = 0;
  int tot_rbytes, rbytes;
  char **flist;
  char *ptr, *tok_ptr, *fnames_buf;
  struct stat fst;
  int len = 0;
  
  ptr = tok_ptr = fnames_buf = NULL;
  tot_rbytes = rbytes = 0;
 
  *obuf_size = 0;
  
  NSDL2_RUNTIME(NULL, NULL, "Method called, fnames = %s, fpath = %s, token = %s", fnames, fpath, token);

  if((fnames == NULL) || (fnames && (*fnames == '\0')))
  {
    sprintf(rtcdata->err_msg, "Error: File name list not provide");
    return -1;
  }
 
  /*Get number of files and make file list */
  flist = (char **)malloc(max_files * sizeof(char *));  
  len = strlen(fnames); 
  NSDL2_RUNTIME(NULL, NULL, "len = %d", len);
  fnames_buf = (char *)malloc(len + 1);

  strcpy(fnames_buf, fnames);   

  ptr = fnames_buf;
  while((tok_ptr = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    num_files++;
    if(num_files > max_files)
    {
      max_files += max_files;
      flist = (char **)realloc(flist, max_files * sizeof(char *));
      NSDL2_RUNTIME(NULL, NULL, "Re-allocate flist for len = %d", max_files);
    }

    flist[num_files - 1] = tok_ptr;
  }
 
  NSDL2_RUNTIME(NULL, NULL, "flist Dump: num_files = %d", num_files);

  for(i = 0; i < num_files; i++)
    NSDL2_RUNTIME(NULL, NULL, "flist[%d] = %s", i, flist[i]);

  /* Read file one by one in chunks of 4k and fill buffer */
  for(i = 0; i < num_files; i++)
  {
    if((*flist[i] != '/') && (fpath != NULL))
    {
      if(fpath != NULL)
        sprintf(abs_fname, "%s/%s", fpath, flist[i]);
      else
      {
        sprintf(rtcdata->err_msg, "Error: File '%s' has relative path and base dir is %p", flist[i], fpath);
        return -1;
      }
    }
    else
      sprintf(abs_fname, "%s", flist[i]);

    NSDL2_RUNTIME(NULL, NULL, "Read file = %s", abs_fname);

    if((fd = open(abs_fname, O_RDONLY | O_CLOEXEC | O_LARGEFILE)) == -1)
    {
      sprintf(rtcdata->err_msg, "Error: Failed to open file %s. error(%d) = %s", abs_fname, errno, nslb_strerror(errno));
      return -1;
    }

    if(fstat(fd, &fst) < 0)
    {
      sprintf(rtcdata->err_msg, "Error: Failed to get size of file %s. error(%d) = %s", abs_fname, errno, nslb_strerror(errno));
      return -1;
    }

    /* Provided file exist or not? */
    if(!S_ISREG(fst.st_mode))
    {
      sprintf(rtcdata->err_msg, "Error: File '%s' not exist.", abs_fname);
      return -1;
    }

    obuf_offset = *obuf_size;
    *obuf_size += fst.st_size + 1;  

    *obuf = realloc(*obuf, *obuf_size + 1);
 
    ptr= *obuf + obuf_offset;
    tot_rbytes = 0;

    NSDL2_RUNTIME(NULL, NULL, "Read files into buf, obuf_offset = %d, obuf_size = %d", obuf_offset, *obuf_size);
    while(tot_rbytes < fst.st_size)
    {
      /*read file in chunk of 4096 size*/
      rbytes = read(fd, ptr, 4096);
      ptr += rbytes;
      tot_rbytes += rbytes;
    }

    NSDL2_RUNTIME(NULL, NULL, "tot_rbytes = %d", tot_rbytes);
    /*file is not last file the add \r\r at end*/
    if(i != (num_files - 1))
    {
      if(*(ptr - 1) != '\n')
      {
        *ptr = '\n';
      }
      else
      {
        (*obuf_size)--;
      }
    }
    else
    {
      if(*(ptr - 1) != '\n')
      {
        *ptr = '\n';
         ptr++;
      }
      else
        (*obuf_size)--;
      *ptr = '\0';
    }

    NSDL2_RUNTIME(NULL, NULL, "obuf_size = %d, Close fd = %d", *obuf_size, fd);
    close(fd);
  }

  if(fnames_buf)
    free(fnames_buf);

  return 0;
}


/*--------------------------------------------------------------------------------------------------
 * Function name    : merge_data_files() 
 | 
 * Synopsis         : This function will merge files provided files for each file parameter group
 |
 * Input            : err_msg - buffer to store error message if any occured. 
 |
 * Output           : 0 - On success 
 |                   -1 - On Failure  
 |
 * Modificaton Date : 
 |
 *--------------------------------------------------------------------------------------------------*/
static int merge_data_files()
{
  char *token = ",";
  char sess_path[256];
  int i, ret;
  char *buf_ptr = NULL;

  NSDL2_RUNTIME(NULL, NULL, "Method called, total_fparam_rtc_tbl_entries = %d", total_fparam_rtc_tbl_entries);

  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
  {
    sprintf(sess_path, "%s/scripts/%s/%s/%s", g_ns_wdir, g_project_name, g_subproject_name, 
                                              session_table_shr_mem[fparam_rtc_tbl[i].sess_idx].sess_name);

    ret = bufwrite_by_files(fparam_rtc_tbl[i].data_file_list, sess_path, token, 
                            &fparam_rtc_tbl[i].data_buf, &fparam_rtc_tbl[i].data_buf_size);   

    NSDL2_RUNTIME(NULL, NULL, "ret = %d", ret);
    if(ret != 0)
      return ret; 

    buf_ptr = fparam_rtc_tbl[i].data_buf; 

    fparam_rtc_tbl[i].num_values = get_num_values(buf_ptr, fparam_rtc_tbl[i].data_file_list, 
                                                  group_table_shr_mem[fparam_rtc_tbl[i].fparam_grp_idx].first_data_line);

    NSDL2_RUNTIME(NULL, NULL, "Merging data file done, i = %d, group = %d, num_values = %d, data_buf = [%s]", 
                               i, fparam_rtc_tbl[i].fparam_grp_idx, fparam_rtc_tbl[i].num_values, buf_ptr);
  } 
  
  return 0;
}

#if 0
/*--------------------------------------------------------------------------------------------------
 * Function name    : update_orig_data() 
 | 
 * Synopsis         : This function will update original file 
 |
 * Input            : err_msg - buffer to fill error message  
 |
 * Output           : 0 - On success 
 |                   -1 - On Failure  
 |
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
static int update_orig_data(char *err_msg)
{
  char abs_fname[1024 + 1]; 
  char *token = ",";
  int i = 0 , j = 0;
  int len = 0;
  int fd;
  int group_idx = -1;
  int seq;
  int orig_data_buf_size = 0;
  int num_values;
  int ret = 0;
  char *orig_data_buf = NULL; 
  char *orig_data_buf_ptr = NULL;
  char *new_data_buf_ptr = NULL;
  char *merge_data_buf = NULL;
  char *merge_data_buf_ptr = NULL;
  char *dptr = NULL;
  char dfname[1024 + 1];
  struct stat st;
 
  NSDL2_RUNTIME(NULL, NULL, "Method called, total_fparam_rtc_tbl_entries = %d", total_fparam_rtc_tbl_entries);

  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
  {
    group_idx = fparam_rtc_tbl[i].fparam_grp_idx;
    seq = group_table_shr_mem[group_idx].sequence;
    num_values = 0;

    /* We need to update original data file to apply multiple times RTC */
    /* Write file into TR runtime_changes direcory */
    sprintf(abs_fname, "%s", group_table_shr_mem[group_idx].data_fname);
     
    if(seq != USEONCE)
    {
      dptr = strrchr(abs_fname, '/');
      dptr++;

      strcpy(dfname, dptr);   
      sprintf(abs_fname, "%s/logs/TR%d/runtime_changes/%s:%s_%s", g_ns_wdir, testidx, 
                          session_table_shr_mem[fparam_rtc_tbl[i].sess_idx].sess_name, 
                          variable_table_shr_mem[group_table_shr_mem[group_idx].start_var_idx].name_pointer, dfname);

      /*Check if file exits in TR runtime_changes directory then read from there else read from scripts */
      ret = stat(abs_fname, &st);
     
      if(!ret && S_ISREG(st.st_mode))   
        dptr = abs_fname;
      else
        dptr =  group_table_shr_mem[group_idx].data_fname;
    }
    else //USEONCE 
    {
      dptr =  group_table_shr_mem[group_idx].data_fname;
    }
 
    NSDL2_RUNTIME(NULL, NULL, "i = %d, abs_fname = %s, mode = %d , dfname = %s", i, abs_fname, fparam_rtc_tbl[i].mode,dfname);
    if(fparam_rtc_tbl[i].mode == APPEND_MODE)
    {
      ret = bufwrite_by_files(dptr, NULL, token, &orig_data_buf, &orig_data_buf_size, err_msg);   
      if(ret != 0)
        return ret;
      
      len = orig_data_buf_size + fparam_rtc_tbl[i].data_buf_size + 1; //+1 for '\0' 
      NSDL2_RUNTIME(NULL, NULL, "Allocate mem for merge_data_buf = %p, of size = %d", merge_data_buf, len);
     
      if(!merge_data_buf)
        merge_data_buf = (char *)malloc(len); 
     
      merge_data_buf_ptr = merge_data_buf; 
      new_data_buf_ptr = fparam_rtc_tbl[i].data_buf;  
      orig_data_buf_ptr = orig_data_buf;
     
      //Fill merge_data_buf
      for(j = 0 ; j < global_settings->num_process ; j++)
      {  
        NSDL2_RUNTIME(NULL, NULL, "j = %d, old-start_val = %d, old-total_val = %d, new-start_val = %d, new-total_val = %d", j, 
                                   per_proc_vgroup_table[(j * total_group_entries) + group_idx].start_val,
                                   per_proc_vgroup_table[(j * total_group_entries) + group_idx].total_val,
                                   per_proc_vgroup_table_rtc[(j * total_group_entries) + group_idx].start_val,
                                   per_proc_vgroup_table_rtc[(j * total_group_entries) + group_idx].total_val);
        //Since in case of Random and weight whole data is provided to all the NVMs and hence copy just for 1 proess
        if((j == 0) || (j > 0 && (per_proc_vgroup_table[(j * total_group_entries) + group_idx].start_val != 0)))
        {
          FILL_MERGED_BUF(merge_data_buf_ptr, orig_data_buf_ptr, 
                                 per_proc_vgroup_table[(j * total_group_entries) + group_idx].total_val);
        }
        
        if((j == 0) || (j > 0 && (per_proc_vgroup_table_rtc[(j * total_group_entries) + group_idx].start_val != 0)))
        {
          FILL_MERGED_BUF(merge_data_buf_ptr, new_data_buf_ptr, 
                                 per_proc_vgroup_table_rtc[(j * total_group_entries) + group_idx].total_val);
        }
      }
     
      *merge_data_buf_ptr = '\0';
      len = merge_data_buf_ptr - merge_data_buf;
      free(orig_data_buf);
      free(fparam_rtc_tbl[i].data_buf);
      fparam_rtc_tbl[i].data_buf = merge_data_buf;
      fparam_rtc_tbl[i].data_buf_size = len;
      fparam_rtc_tbl[i].num_values = num_values;
    }

    NSDL2_RUNTIME(NULL, NULL, "Update original data file '%s': total num_values = %d, size = %d and content = [%s]", 
                               group_table_shr_mem[group_idx].data_fname, fparam_rtc_tbl[i].num_values, fparam_rtc_tbl[i].data_buf_size, 
                               fparam_rtc_tbl[i].data_buf);
    //Write into file
    unlink(abs_fname);

    if((fd = open(abs_fname, O_WRONLY|O_CLOEXEC|O_CREAT, 0664)) < 0)
    {
      sprintf(err_msg, "Error: unable to open data file '%s' for updating", group_table_shr_mem[group_idx].data_fname); 
      return -1;
    }

    if(fd)
      write(fd, fparam_rtc_tbl[i].data_buf, fparam_rtc_tbl[i].data_buf_size);

    close(fd);

    merge_data_buf = NULL;
    orig_data_buf = NULL;
    orig_data_buf_size = 0;
  }
  
  return 0;
}
#endif

static void update_orig_data_file()
{
  int grp_idx, grp_rtc_idx;
  char *abs_fname = NULL;
  int fd;

  NSDL2_RUNTIME(NULL, NULL, "Method called, total_fparam_rtc_tbl_entries = %d", total_fparam_rtc_tbl_entries);

  for(grp_rtc_idx = 0; grp_rtc_idx < total_fparam_rtc_tbl_entries; grp_rtc_idx++)
  {
    grp_idx = fparam_rtc_tbl[grp_rtc_idx].fparam_grp_idx;
    abs_fname = group_table_shr_mem[grp_idx].data_fname; 
    
    NSDL2_RUNTIME(NULL, NULL, "grp_rtc_idx = %d, grp_idx = %d, abs_fname = %d, sequence = %d", 
                               grp_rtc_idx, grp_idx, abs_fname, group_table_shr_mem[grp_idx].sequence);

    if(group_table_shr_mem[grp_idx].sequence != USEONCE)
      continue;  
    
    unlink(abs_fname);

    if((fd = open(abs_fname, O_WRONLY|O_CLOEXEC|O_CREAT, 0664)) < 0)
    {
      fprintf(stderr, "Error: update_orig_data_file() - unable to open data file '%s' for updating\n", abs_fname); 
      //return -1;
      continue;
    }

    if(fd)
      write(fd, fparam_rtc_tbl[grp_rtc_idx].data_buf, fparam_rtc_tbl[grp_rtc_idx].data_buf_size);

    close(fd);
  }
}

//This function will make updated data buf 
static int update_orig_data_file_internal()
{
  int proc_idx, ppvgrp_idx;
  int grp_idx, grp_rtc_idx; 
  int var_idx, val_idx;
  int seq;
  int widx = 0, size = 0;
  int free_space = 0, tmp_free_space = 0;
  char *data_buf = NULL;
  PointerTableEntry_Shr *value = NULL;
  PerProcVgroupTable *ppvgrp = NULL;
  
  NSDL2_RUNTIME(NULL, NULL, "Method Called");

  for(grp_rtc_idx = 0; grp_rtc_idx < total_fparam_rtc_tbl_entries; grp_rtc_idx++)
  {
    grp_idx = fparam_rtc_tbl[grp_rtc_idx].fparam_grp_idx;
    data_buf = fparam_rtc_tbl[grp_rtc_idx].data_buf;
    tmp_free_space = free_space = fparam_rtc_tbl[grp_rtc_idx].data_buf_size;
    seq = group_table_shr_mem[grp_idx].sequence;

    NSDL2_RUNTIME(NULL, NULL, "grp_rtc_idx = %d, grp_idx = %d, seq = %d, data_buf = %p, free_space = %d", 
                               grp_rtc_idx, grp_idx, seq, data_buf, free_space);

    if(seq != USEONCE)
      continue;

    data_buf[0] = 0;
    widx = 0;
    
    for(proc_idx = 0; proc_idx < global_settings->num_process; proc_idx++) 
    {
      ppvgrp_idx = (proc_idx * total_group_entries) + grp_idx;
      ppvgrp = &per_proc_vgroup_table_rtc[ppvgrp_idx];

      for(val_idx = 0; val_idx < ppvgrp->total_val; val_idx++)
      {
        for(var_idx = 0; var_idx < ppvgrp->group_table_shr_mem->num_vars; var_idx++)
        {
          value = ppvgrp->variable_table_shr_mem[var_idx].value_start_ptr;  
          if(tmp_free_space < value[val_idx].size)
          {
            NSDL2_RUNTIME(NULL, NULL, "free_space = %d, val_idx = %d, proc_idx = %d, widx = %d", free_space, val_idx, proc_idx, widx);
            RTC_REALLOC(data_buf, free_space + 1048576, "data buf", -1); 
            free_space += 1048576;
            tmp_free_space += 1048576;
          }
          size = sprintf(data_buf + widx, "%s,", value[val_idx].big_buf_pointer);
          widx += size;
          tmp_free_space -= size;
        }

        if(data_buf[widx - 1] == ',')
          data_buf[widx - 1] = '\n';
      } 
    }

    data_buf[widx] = '\0';
    fparam_rtc_tbl[grp_rtc_idx].data_buf = data_buf;
    fparam_rtc_tbl[grp_rtc_idx].data_buf_size = widx;
    
    NSDL2_RUNTIME(NULL, NULL, "Updated data for fparam group %d -> size = %d, [%s]", grp_idx, widx, data_buf);
  }

 return 0;
}

static void create_group_table_entry_rtc()
{
  int i;

  NSDL2_RUNTIME(NULL, NULL, "Method called, groupTable = %p, total_group_entries = %d", groupTable, total_group_entries);

  RTC_MALLOC_AND_MEMSET(groupTable, sizeof(GroupTableEntry) * total_group_entries, "groupTable rtc", -1);

  for(i = 0; i < total_group_entries; i++)
  {
    groupTable[i].type = group_table_shr_mem[i].type;
    groupTable[i].sequence = group_table_shr_mem[i].sequence;
    groupTable[i].weight_idx = -1;
    
    /* TODO:
    if ((groupTable[i].weight_idx == -1) && (group_table_shr_mem[i].sequence != UNIQUE))
      group_table_shr_mem[i].group_wei_uni.weight_ptr = NULL;
    else if (group_table_shr_mem[i].sequence == UNIQUE)
      group_table_shr_mem[i].group_wei_uni.unique_group_id = unique_group_id++;
    else
      group_table_shr_mem[i].group_wei_uni.weight_ptr = WEIGHT_TABLE_MEMORY_CONVERSION(groupTable[i].weight_idx);
    */

    groupTable[i].num_values = group_table_shr_mem[i].num_values;

    groupTable[i].idx = group_table_shr_mem[i].idx;
    groupTable[i].num_vars = group_table_shr_mem[i].num_vars;
    //groupTable[i].sess_idx = BIG_BUF_MEMORY_CONVERSION(gSessionTable[groupTable[i].sess_idx].sess_name);
    groupTable[i].start_var_idx = group_table_shr_mem[i].start_var_idx;
    //groupTable[i].data_fname = BIG_BUF_MEMORY_CONVERSION(groupTable[i].data_fname);
    //groupTable[i].column_delimiter = BIG_BUF_MEMORY_CONVERSION(groupTable[i].column_delimiter);

    memcpy(groupTable[i].encode_chars, group_table_shr_mem[i].encode_chars, TOTAL_CHARS);
    groupTable[i].encode_type = group_table_shr_mem[i].encode_type; 
    //groupTable[i].encode_space_by = BIG_BUF_MEMORY_CONVERSION(groupTable[i].encode_space_by);
    groupTable[i].UseOnceOptiontype = group_table_shr_mem[i].UseOnceOptiontype;
    groupTable[i].UseOnceAbort = group_table_shr_mem[i].UseOnceAbort;
    groupTable[i].first_data_line = group_table_shr_mem[i].first_data_line;
    groupTable[i].absolute_path_flag = group_table_shr_mem[i].absolute_path_flag;
    groupTable[i].max_column_index = group_table_shr_mem[i].max_column_index; 
    groupTable[i].ignore_invalid_line = group_table_shr_mem[i].ignore_invalid_line; 

    //Need to re-think how index var will work
    groupTable[i].index_var = -1; 
  }
}

static void create_var_table_entry_rtc() 
{
  int i;

  NSDL2_RUNTIME(NULL, NULL, "Method called, varTable = %p, total_var_entries = %d", varTable, total_var_entries);

  RTC_MALLOC_AND_MEMSET(varTable, sizeof(VarTableEntry) * total_var_entries, "varTable rtc", -1);
  for(i = 0; i < total_var_entries; i++)
  {
    varTable[i].var_size = variable_table_shr_mem[i].var_size;
    varTable[i].is_file = variable_table_shr_mem[i].is_file;
    varTable[i].column_idx = variable_table_shr_mem[i].column_idx;
  }
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : parse_data_file() 
 | 
 * Synopsis         : This function will parse data file of file parameter group and create required tables 
 |
 * Input            : err_msg - buffer to fill error message  
 |
 * Output           : Following table will created by this function -
 |                      (3) weightTable
 |                      (4) groupTable
 |                      (5) varTable
 |                      (6) fparamValueTable
 |                      (7) bigbuf
 |
 * Return Value     : 0 - On success 
 |                   -1 - On Failure  
 |
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
//input_static_values(staticvar_file, group_idx, sess_idx, var_start, 0, NULL);
static int parse_data_file()
{
  int i;
  int group_idx;
  int sess_idx;
  int var_start;
  char *data_file_buf_rtc;
  char* file_name;
  int runtime = 1;
  int ret;

  NSDL2_RUNTIME(NULL, NULL, "Method called.");

  //Create groupTable and varTable
  create_group_table_entry_rtc();  
  create_var_table_entry_rtc();

  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
  {
    file_name = fparam_rtc_tbl[i].data_file_list; 
    group_idx = fparam_rtc_tbl[i].fparam_grp_idx;
    sess_idx = fparam_rtc_tbl[i].sess_idx;
    var_start = groupTable[group_idx].start_var_idx;
    data_file_buf_rtc = fparam_rtc_tbl[i].data_buf;
    sprintf(script_name, "%s/%s/%s", g_project_name, g_subproject_name, session_table_shr_mem[fparam_rtc_tbl[i].sess_idx].sess_name);
    NSDL2_RUNTIME(NULL, NULL, "i = %d, file_name = %s, group_idx = %d, sess_idx = %d, var_start = %d, data_file_buf_rtc = %p, "
                              "loader_opcode = %d", 
                               i, file_name, group_idx, sess_idx, var_start, data_file_buf_rtc, loader_opcode);
    // BugId: 60638
    // BugId: 60927
    /*if(groupTable[group_idx].sequence == WEIGHTED)
    {
      NSDL1_RUNTIME(NULL, NULL, "Error: parse_data_file() - File parameter weighted random RTC is not supported.");
      sprintf(rtcdata->err_msg, "Error: File parameter weighted random RTC is not supported.");
      return -1;
    }*/

    if(loader_opcode == MASTER_LOADER)
    {
      api_table[group_idx].data_file_size = fparam_rtc_tbl[i].data_buf_size;
      api_table[group_idx].rtc_flag = fparam_rtc_tbl[i].mode;
      ret = ni_input_static_values(file_name, group_idx, sess_idx, runtime, data_file_buf_rtc, 0, NULL, rtcdata->err_msg);
      if(ret)
      {
        NSDL1_RUNTIME(NULL, NULL, "%s", rtcdata->err_msg);
        //sprintf(rtcdata->err_msg, "Error: parse_data_file() - data parsing failed., mode = %d, group_idx = %d", fparam_rtc_tbl[i].mode , group_idx);
        return -1;
      }
    }
    else
    {
      ret = input_static_values(file_name, group_idx, sess_idx, var_start, runtime, data_file_buf_rtc, 0);
      if(ret)
      {
        NSDL1_RUNTIME(NULL, NULL, "Error: parse_data_file() - data parsing failed.");
        return -1;
      }
    }
    is_global_vars_reset_done = 1;
  }

  return 0;
}

#if 0
void set_per_proc_vgroup_runtime_flag()
{
  int i, j;
  int idx;
  int rtc_fgrp_idx;

  NSDL1_RUNTIME(NULL, NULL, "Method called, num_process = %d, total_group_entries = %d", 
                          global_settings->num_process, total_group_entries);

  for(i = 0; i < global_settings->num_process; i++)
  {
    for(j = 0; j < total_group_entries; j++)
    {
      idx = (i * total_group_entries) + j;
      rtc_fgrp_idx = is_rtc_applied_on_group(per_proc_vgroup_table_rtc[idx].num_script_users, j);  
      if(!rtc_fgrp_idx) 
      {
        per_proc_vgroup_table_rtc[idx].rtc_flag = -1;
        continue;
      }
      rtc_fgrp_idx--;
      /* Since in case of append data should fetch from current index hence storing mode*/
      //per_proc_vgroup_table[idx].rtc_fgrp_idx = rtc_fgrp_idx; 
      NSDL1_RUNTIME(NULL, NULL, "j = %d, rtc_fgrp_idx = %d, rtc_flag = %d", j, rtc_fgrp_idx, fparam_rtc_tbl[rtc_fgrp_idx].mode);
      per_proc_vgroup_table_rtc[idx].rtc_flag = fparam_rtc_tbl[rtc_fgrp_idx].mode; 
    }
  }
}
#endif

void dump_rtc_group_table_values()
{
  int i, j;
  int grp_idx, pp_grp_idx;

  NSDL1_RUNTIME(NULL, NULL, "Method called, total_fparam_rtc_tbl_entries = %d, rtcdata->rtclog_fp = %p",
                            total_fparam_rtc_tbl_entries, rtcdata->rtclog_fp);

  //NSDL2_RUNTIME(NULL, NULL, "%s|File Parameter Runtime Data Distribution among NVMs:\n", get_cur_time());

  fprintf(stdout, "File Parameter Runtime Data Distribution among NVMs:\n");

  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
  {
    grp_idx = fparam_rtc_tbl[i].fparam_grp_idx;

      snprintf(rtcdata->msg_buff, RTC_QTY_BUFFER_SIZE, "RTCMode = %s, Script=%s, OrignalDataFile=%s, OldValues = %d,"
                         " NewDataFile=%s, NewValues = %d, TotalValues=%d, Mode=%s, Refresh=%s\n",
                         (fparam_rtc_tbl[i].mode == 1)?"APPEND":"REPLACE", 
                         group_table_shr_mem[grp_idx].sess_name, group_table_shr_mem[grp_idx].data_fname, 
                         group_table_shr_mem[grp_idx].num_values, fparam_rtc_tbl[i].data_file_list, fparam_rtc_tbl[i].num_values, 
                         (fparam_rtc_tbl[i].mode == 1)?(group_table_shr_mem[grp_idx].num_values + fparam_rtc_tbl[i].num_values):
                         fparam_rtc_tbl[i].num_values, 
                         find_mode_from_seq_number(group_table_shr_mem[grp_idx].sequence), 
                         find_type_from_type_number(group_table_shr_mem[grp_idx].type));
      RUNTIME_UPDATE_LOG(rtcdata->msg_buff)

      fprintf(stdout, "RTCMode = %s, Script=%s, OrignalDataFile=%s, OldValues = %d, NewValues = %d, TotalValues=%d, Mode=%s, Refresh=%s\n", 
                      (fparam_rtc_tbl[i].mode == 1)?"APPEND":"REPLACE",
                      group_table_shr_mem[grp_idx].sess_name, group_table_shr_mem[grp_idx].data_fname, 
                      group_table_shr_mem[grp_idx].num_values, fparam_rtc_tbl[i].num_values, 
                      (fparam_rtc_tbl[i].mode == 1)?(group_table_shr_mem[grp_idx].num_values + fparam_rtc_tbl[i].num_values):
                      fparam_rtc_tbl[i].num_values, 
                      find_mode_from_seq_number(group_table_shr_mem[grp_idx].sequence), 
                      find_type_from_type_number(group_table_shr_mem[grp_idx].type));

    for(j = 0 ; j < global_settings->num_process ; j++)
    {
      pp_grp_idx = (j * total_group_entries) + grp_idx;  

      if(get_group_mode(-1) == TC_FIX_CONCURRENT_USERS)
      {
        NSDL2_RUNTIME(NULL, NULL, "    NVM%d: Users=%d, StartVal=%d, NumVal=%d\n", j,
                             per_proc_vgroup_table_rtc[pp_grp_idx].num_script_users,
                             per_proc_vgroup_table_rtc[pp_grp_idx].start_val,
                             per_proc_vgroup_table_rtc[pp_grp_idx].num_val);

        fprintf(stdout, "    NVM%d: Users=%d, StartVal=%d, NumVal=%d\n", j,
                        per_proc_vgroup_table_rtc[pp_grp_idx].num_script_users,
                        per_proc_vgroup_table_rtc[pp_grp_idx].start_val,
                        per_proc_vgroup_table_rtc[pp_grp_idx].num_val);

       //Resolve bug 47886 - FileParameterRTC| Data lines distribution in NVMs is coming late (end of the test) in TestRunOutput.log file 
       fflush(stdout);
      }
      else
      {
        NSDL2_RUNTIME(NULL, NULL, "    NVM%d: StartVal=%d, NumVal=%d\n", j,
                             per_proc_vgroup_table_rtc[pp_grp_idx].start_val,
                             per_proc_vgroup_table_rtc[pp_grp_idx].num_val);

        fprintf(stdout, "    NVM%d: StartVal=%d, NumVal=%d\n", j,
                        per_proc_vgroup_table_rtc[pp_grp_idx].start_val,
                        per_proc_vgroup_table_rtc[pp_grp_idx].num_val);

        //Resolve bug 47886 - FileParameterRTC| Data lines distribution in NVMs is coming late (end of the test) in TestRunOutput.log file 
        fflush(stdout);
      }
    }
  }
}

char *update_fpath_and_make_msg(int gen_idx, int *msg_len)
{
  static char msg[34 * 1024] = "";
  char abs_flist[5 * 1024] = "";
  //char tmp_buf[2 * 1024];
  char *tmp_ptr = NULL;
  //char *comm_ptr = NULL;
  //char *end_ptr = NULL;
  char *sess_name = NULL;
  int opcode = APPLY_FPARAM_RTC; 
  //int add_comm = 0;
  //int break_loop = 0;
  int wbytes = 0;
  int api_id;
  int i;
  int len = 0;
  
  NSDL1_RUNTIME(NULL, NULL, "Method called, total_fparam_rtc_tbl_entries = %d", total_fparam_rtc_tbl_entries);

  *msg_len = 4;

  //Fill opcode
  memcpy(msg + *msg_len, &opcode, 4);
  *msg_len += 4;

  //Fill message
  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
  {
    wbytes = 0;
    api_id = fparam_rtc_tbl[i].fparam_grp_idx;
    sess_name = session_table_shr_mem[fparam_rtc_tbl[i].sess_idx].sess_name; 
    //tmp_ptr = fparam_rtc_tbl[i].data_file_list;
    //end_ptr = tmp_ptr + strlen(tmp_ptr);

    #if 0
    NSDL2_RUNTIME(NULL, NULL, "i = %d, api_id = %d, tmp_ptr = %s", i, api_id, tmp_ptr);
    while(1)
    {
      add_comm = 0;
      comm_ptr = strchr(tmp_ptr, ',');

      //1|S1:S1v1|S1f1 or 1|S1:S1v1|S1f1;1|S1:S1v2|S1f2,S1f3
      if((comm_ptr == NULL)/* || (comm_ptr > end_ptr)*/) //Last file of the group
      {
        break_loop = 1; 
        len = end_ptr - tmp_ptr;
        strncpy(tmp_buf, tmp_ptr, len);
        tmp_buf[len] = '\0';
      } 
      else
      {
        len = comm_ptr - tmp_ptr;
        strncpy(tmp_buf, tmp_ptr, len);
        tmp_buf[len] = '\0';
        tmp_ptr = comm_ptr + 1;
        if((*tmp_ptr != '\0'))
          add_comm = 1;
        else
          break_loop = 1;
      }

      if(*tmp_buf == '/') //If absolute path is given
      {
        comm_ptr = strrchr(tmp_buf, '/'); 
        len = (tmp_buf + strlen(tmp_buf)) - comm_ptr; 
        strncpy(tmp_buf, comm_ptr + 1, len);
        tmp_buf[len] = 0;
      }
     
      NSDL2_RUNTIME(NULL, NULL, "tmp_buf = %s, abs_or_relative_data_file_path = %d, data_file_path = [%s], add_comm = %d, "
                                "gen_idx = %d, gen-work = %s, gen-testidx = %d", 
                                 tmp_buf, api_table[api_id].abs_or_relative_data_file_path, api_table[api_id].data_file_path, add_comm,
                                 gen_idx, generator_entry[gen_idx].work, generator_entry[gen_idx].testidx);

      if(api_table[api_id].abs_or_relative_data_file_path == 0)
      {
        wbytes += sprintf(abs_flist + wbytes, "%s/logs/TR%d/runtime_changes/%s/%s%s", 
                            generator_entry[gen_idx].work, generator_entry[gen_idx].testidx, 
                            get_sess_name_with_proj_subproj(sess_name), tmp_buf, add_comm?",":"");
      }
      else
      {
        wbytes += sprintf(abs_flist + wbytes, "%s/logs/TR%d/runtime_changes/%s/%s%s", 
                            generator_entry[gen_idx].work, generator_entry[gen_idx].testidx, 
                            api_table[api_id].data_file_path, tmp_buf, add_comm?",":""); 
      }

      if(break_loop)
        break; 
    }
    #endif

    NSDL1_RUNTIME(NULL, NULL, "api_table - data_fname = %s", api_table[api_id].data_fname);
    if(api_table[api_id].abs_or_relative_data_file_path == 0)
    {
      tmp_ptr = strrchr(api_table[api_id].data_fname, '/');      
      tmp_ptr++; 
      wbytes += sprintf(abs_flist + wbytes, "%s/logs/TR%d/runtime_changes/%s/%s_rtc", 
                          generator_entry[gen_idx].work, generator_entry[gen_idx].testidx, 
                          get_sess_name_with_proj_subproj_int(sess_name, fparam_rtc_tbl[i].sess_idx, "/"), tmp_ptr);
                          //Previously taking with only script name
                          //get_sess_name_with_proj_subproj(sess_name), tmp_ptr);
    }
    else
    {
      wbytes += sprintf(abs_flist + wbytes, "%s/logs/TR%d/runtime_changes%s_rtc", 
                          generator_entry[gen_idx].work, generator_entry[gen_idx].testidx, 
                          api_table[api_id].data_fname); 
    }

    abs_flist[wbytes] = '\0';
    NSDL1_RUNTIME(NULL, NULL, "abs_flist = [%s]", abs_flist);

    *msg_len += sprintf(msg + *msg_len, "%d|%s:%s|%s;", 
                                    fparam_rtc_tbl[i].mode, get_sess_name_with_proj_subproj_int(sess_name, fparam_rtc_tbl[i].sess_idx, "/"), 
                                    variable_table_shr_mem[group_table_shr_mem[fparam_rtc_tbl[i].fparam_grp_idx].start_var_idx].name_pointer,
                                    abs_flist);
    NSDL1_RUNTIME(NULL, NULL, "msg = [%*.*s]", (*msg_len - 8), (*msg_len - 8), (msg + 8));
  }  
   
  msg[*msg_len - 1] = '\0';

  //Fill len 
  len = *msg_len - 4; //Reduce len part
  memcpy(msg, &len, 4);

  return msg; 
}

int send_apply_fparam_rtc_msg_to_gen(char *msg)
{
  int i;
  int len = 0;
  char *send_msg = NULL;

  NSDL1_RUNTIME(NULL, NULL, "Method called, len = %d, msg_opcode = %d, msg = [%s]",
                                            *((int *)msg), *((int *)msg + 1), *(msg + 8)?(msg + 8):"NULL");

  NSTL1(NULL, NULL, "(Master -> Generator) APPLY_FPARAM_RTC(149) processing provided parameters");
  //This code added to handle rtc epoll wait code
  rtcdata->epoll_start_time = get_ms_stamp();
  rtcdata->cur_state = RTC_PAUSE_STATE;
  for(i = 0; i < sgrp_used_genrator_entries ; i++)
  {
    if (generator_entry[i].flags & IS_GEN_INACTIVE)
    {
      if (g_msg_com_con[i].ip)
      {
        NSDL1_RUNTIME(NULL, NULL, "Connection with the child is already"
                                  "closed so not sending the msg %s", msg_com_con_to_str(&g_msg_com_con[i]));
      }
    }
    else
    {
      send_msg = update_fpath_and_make_msg(i, &len);

      NSDL1_RUNTIME(NULL, NULL, "Sending msg to gen id = %d, %s, len = %d, send_msg = [%s]", 
                                 i, msg_com_con_to_str(&g_msg_com_con[i]), len, (send_msg + 8));
     
      int ret = write_msg(&g_msg_com_con[i], send_msg, len, 0, CONTROL_MODE);
      if(ret == -1){
        sprintf(rtcdata->err_msg, "Error: send_apply_fparam_rtc_msg_to_gen(): Sending msg to gen id = %d, %s", 
                       i, msg_com_con_to_str(&g_msg_com_con[i]));
         
        return -1;
      }
      else
        NSDL1_RUNTIME(NULL, NULL, "MANISH: PASS");
    }
  }

  return 0;
}

void ignore_sigchild()
{
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : handle_fparam_rtc() 
 | 
 * Synopsis         : This function will called when Runtime Change is applied for File Parameter 
 |                    Following task has to be handled here -
 |                    [1] Open runtime_changes.log in append mode to dump status  
 |                    [2] Parse Message get from nsi_rtc_invoker and create table named fparam_rtc_tbl 
 |                    [3] Merge provided files, distribute that file over NVMs and update origial data file     
 |                    [4] Parse data values and create tables - groupTable , varTable, fparamValueTable 
 |                        and file_param_value_big_buf
 |                    [5] Allocate shared memory and attach with NVMs
 |                    [6] Copy data into shared memory 
 |                    [7] Pause the NVM and update following structures -
 |                        1) per_proc_vgroup_table 
 |                        2) group_table_shared_mem
 |                        3) variable_table_shared_mem
 |
 * Input            : msg - message get from tool nsi_rtc_invoker 
 |
 * Output           : Following table will created by this function -
 |                      (1) fparam_rtc_tbl   
 |                      (2) per_proc_vgroup_table_rtc   
 |                      (3) weightTable
 |                      (4) groupTable
 |                      (5) varTable
 |                      (6) fparamValueTable
 |                      (7) bigbuf
 |
 * Return Value     : 0 - On success 
 |                   -1 - On Failure  
 |                   -2 - If one RTC is already in process
 |
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
int handle_fparam_rtc(Msg_com_con *mccptr, char *tool_msg)
{
  //char err_msg[MAX_ERR_MSG_LEN];
  //char rtc_log_file[MAX_FILE_NAME]; 
  int i;
  PerScriptTotal * psTable; 
  char *msg = tool_msg + 8; // skip -> 4 byte for msg len and 4 byte for opcode
 
  //Null terminate messgae at max len
  msg[(*((int *)tool_msg)) - 4] = '\0';

  NSDL1_RUNTIME(NULL, NULL, "Method called, msg_len = %d, msg_opcode = %d, msg = [%s], g_ns_wdir = [%s],"
                            " testidx = %d, fpram_rtc_mccptr = %p", *((int *)tool_msg), *((int *)tool_msg + 1),
                            *msg?msg:"NULL", g_ns_wdir, testidx, rtcdata->invoker_mccptr);

  /* Set rtcdata->fparam_flag flag to 1 and reset on any error */
  RUNTIME_UPDATION_OPEN_LOG
  if(CHECK_RTC_FLAG(RUNTIME_FAIL))
    return -1;

  /* Dump input into runtime_changes.conf */
  NSDL2_RUNTIME(NULL, NULL, "File Parameter RTC applying..., NS parent received message - %s, fpram_rtc_mccptr = %p",
                            msg, rtcdata->invoker_mccptr);

  NSTL1(NULL, NULL, "(%s) APPLY_FPARAM_RTC(149) processing provided parameters",
       ((loader_opcode == CLIENT_LOADER)?"Generator <- Master":(loader_opcode == MASTER_LOADER)?"Master <- Tool":"Parent <- Tool"));

  //RTC can not be appyied before start phase and in post processing phase
  if((ns_parent_state == NS_PARENT_ST_INIT ) || (ns_parent_state == NS_PARENT_ST_TEST_OVER))
  {
    sprintf(rtcdata->err_msg, "Error: RTC cannot be applied before start phase and during post processing.");
    RUNTIME_UPDATION_VALIDATION
    return -1;
  }

  /* File param RTC will not apply if test is already in PAUSE state. */
  if(global_settings->pause_done)
  {
    sprintf(rtcdata->err_msg, "Error: RTC can not be applied as test is in pause state");
    RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
    return -1;
  }

  /* Parse message - message formate: 
      mode|script_name:fparam_group_name|file_list;..
      Example:
        0|S1:v1|v1f1,v1f2;0|S1:v2|v2f3;1|S2:v1|v1f1;1|S1:v1|v1f4
  */
  if(create_and_fill_fparam_rtc_tbl(msg) != 0)
  {
    RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
    return -1;
  }

  NSDL2_RUNTIME(NULL, NULL, "total_fparam_rtc_tbl_entries = %d", total_fparam_rtc_tbl_entries);
  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
  {
    NSDL2_RUNTIME(NULL, NULL, "fparam_rtc_tbl Dump: idx = %d, sess_idx = %d, fparam_grp_idx = %d, mode = %d, data_file_list = %s", 
                               i, fparam_rtc_tbl[i].sess_idx, fparam_rtc_tbl[i].fparam_grp_idx, fparam_rtc_tbl[i].mode, 
                               fparam_rtc_tbl[i].data_file_list); 
  }

  /* Merge data files to make new data file per group */
  if(merge_data_files() != 0) {
    RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
    return -1;
  }

  //Comment below code to Resolve Bug:22833 - File Param RTC: read old data from memory rather than file to improve performance
  #if 0
  if(loader_opcode != MASTER_LOADER)
  {
    psTable = create_per_script_total();
 
    if(divide_values(psTable, 1, 0) == -1)
    {
      sprintf(err_msg, "Total no of data is less then total no nvm or user. Aborting the RTC!!!"); 
      RUNTIME_UPDATION_FAILED(err_msg);
    }
  
    // Read original file form memory rather than file
    if(update_orig_data(err_msg) != 0)
      RUNTIME_UPDATION_FAILED(err_msg); 
  }
  #endif
  
  if(parse_data_file() != 0)
  {
    RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
    return -1;
  }

  NSDL2_RUNTIME(NULL, NULL, "loader_opcode = %d", loader_opcode);
  // Handle controller case - Divide data file over generators
  if(loader_opcode == MASTER_LOADER)
  {
    NSDL2_RUNTIME(NULL, NULL, "In Master case");
    if(ni_divide_values_per_generator(1) == -1)
    {
      sprintf(rtcdata->err_msg, "%s" ,"Error: ni_divide_values_per_generator(1) - failed.\n");
      RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
      return -1;
    }

    //Ship data
    sighandler_t prev_handler;
    prev_handler = signal(SIGCHLD, ignore_sigchild);
    if(run_command_in_thread(FTP_DATA_FILE_RTC, 1) == -1){
      RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
      return -1;
    }
    else if (run_command_in_thread(EXTRACT_DATA_FILE_RTC, 1) == -1){
      RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
      return -1;
    }
    else if((send_apply_fparam_rtc_msg_to_gen(tool_msg)) == -1){
      RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
      return -1;
    }
    (void) signal(SIGCHLD, prev_handler); 

  }
  else
  {
    psTable = create_per_script_total();

    /* Create per_proc_vgroup_static_shr_mem */
    if(divide_values(psTable, 1, 1) == -1)
    {
      sprintf(rtcdata->err_msg, "Total no of data is less then total no nvm or user. Aborting the RTC!!!"); 
      RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
      return -1;
    }
 
    NSDL2_RUNTIME(NULL, NULL, "Runtime shared memory made succefully.");

    //set_per_proc_vgroup_runtime_flag();

    update_orig_data_file_internal(); 
    rtcdata->epoll_start_time = get_ms_stamp();
    rtcdata->cur_state = RTC_PAUSE_STATE;
    if(send_attach_shm_msg_to_nvms() != 0)
    {
      sprintf(rtcdata->err_msg, "File parameter RTC can't be applied untill All the NVM not complete their START Phase."); 
      RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
      return -1;
    }
  }
  return 0;
}

/* ---------------- Parent child communication----------------------- */

int is_all_nvms_done()
{
  int active_nvms;
 
  all_nvms_done++;
  
  /* In case of NC: On Controller total_killed_nvms always will be 0
     In case of Generator/Standalone: total_killed_gen always will be 0
  */
  active_nvms = ((global_settings->num_process - g_data_control_var.total_killed_nvms) - g_data_control_var.total_killed_gen);

  NSDL3_RUNTIME(NULL, NULL, "Method called, all_nvms_done = %d, active_nvms = %d, num_process = %d, total_killed_nvms = %d, " 
                            "total_killed_gen = %d", all_nvms_done, active_nvms, global_settings->num_process, 
                            g_data_control_var.total_killed_nvms, g_data_control_var.total_killed_gen);

  if(all_nvms_done == active_nvms)
  {
    all_nvms_done = 0;
    return 1;
  }

  return 0;
}

static void parent_cleanup_on_fparam_rtc_done()
{
  NSDL3_RUNTIME(NULL, NULL, "Method called");
  /* Reset fpram_rtc_mccptr so that next time RTC can be applied */
  
  FREE_AND_MAKE_NULL(per_proc_vgroup_table_rtc, "per_proc_vgroup_table_rtc", -1);

  if(fparam_rtc_tbl)
  {
    FREE_AND_MAKE_NULL(fparam_rtc_tbl->data_file_list, "fparam_rtc_tbl->data_file_list", -1);
    FREE_AND_MAKE_NULL(fparam_rtc_tbl->data_buf, "fparam_rtc_tbl->data_file_list", -1);
    FREE_AND_MAKE_NULL(fparam_rtc_tbl, "fparam_rtc_tbl", -1);
  }

  FREE_AND_MAKE_NULL(fparamValueTable, "fparamValueTable rtc", -1);
  FREE_AND_MAKE_NULL(weightTable, "weightTable rtc", -1);
  FREE_AND_MAKE_NULL(groupTable, "groupTable rtc", -1);
  FREE_AND_MAKE_NULL(varTable, "varTable rtc", -1);

  nslb_bigbuf_free((bigbuf_t *)&file_param_value_big_buf);
  memset(rtcdata->err_msg, 0, 1024);
  total_fparam_rtc_tbl_entries = 0;
  is_global_vars_reset_done = 0;

  //Reset FRTC Counters 
  all_nvms_done = 0;
  memset(&all_nvms_sent, 0, sizeof(int) * MAX_NVM_NUM);
}

void handle_fparam_rtc_done(int nvm)
{
  char send_msg[1024 + 1];
  int len = 0;
  int opcode = RTC_RESUME_DONE; 
  int ret;

  NSDL3_RUNTIME(NULL, NULL, "Method called, fpram_rtc_mccptr = %p, nvm = %d, RTC flags = %X",
                             rtcdata->invoker_mccptr, nvm, CHECK_RTC_FLAG(RUNTIME_SET_ALL_FLAG));

  if(nvm >= 0)
  {
    if(!is_all_nvms_done())
      return;
  }  
  NSDL3_RUNTIME(NULL, NULL, "loader_opcode = %d, rtcdata->cur_state = %d", loader_opcode, rtcdata->cur_state);

  /* fpram_rtc_mccptr will not NULL, It will NULL if fopen failed */
  if(!rtcdata->invoker_mccptr)
  {
    sprintf(rtcdata->err_msg, "File Parameter runtime changes %s",
             (CHECK_RTC_FLAG(RUNTIME_FAIL))?"not applied":"applied Successfully");
    RUNTIME_UPDATE_LOG(rtcdata->err_msg)  
    RUNTIME_UPDATION_CLOSE_FILES
    RUNTIME_UPDATION_RESET_FLAGS
    return;
  }

  rtcdata->cur_state = RESET_RTC_STATE;

  /* Printing file parameter RTC details on runtime_changes.log */
  if(loader_opcode == MASTER_LOADER)
  {
    NSDL3_RUNTIME(NULL, NULL, "All Generators resumed succesfully"); 
    WRITE_RTC_DATA_INTO_LOG_FILE();
  }
  
  if(CHECK_RTC_FLAG(RUNTIME_FAIL))
    RUNTIME_UPDATE_LOG("File Parameter runtime changes not applied") 
  else
    RUNTIME_UPDATE_LOG("File Parameter runtime changes applied Successfully") 

  fflush(rtcdata->rtclog_fp);
  len = snprintf(send_msg + 8, 1024, "%s", (CHECK_RTC_FLAG(RUNTIME_FAIL))?rtcdata->err_msg:"SUCCESS");

  if(loader_opcode == CLIENT_LOADER)
  {
    send_rtc_msg_to_controller(master_fd, opcode, send_msg + 8, -1);
  }
  else
  {
    NSDL3_RUNTIME(NULL, NULL, "Send msg to nsi_invoker_tool - len = %d, msg = [%s]", len, send_msg + 8);
    ret = write_msg(rtcdata->invoker_mccptr, send_msg + 8, len, 0, CONTROL_MODE);
    if(ret)
      fprintf(stderr, "Error: handle_fparam_rtc_done() - write message failed.\n");
  }

  /* Reset fpram_rtc_mccptr so that next time RTC can be applied */
  RUNTIME_UPDATION_RESET_FLAGS
  RUNTIME_UPDATION_CLOSE_FILES
  //Clean all the data structures 
  if( nvm != -2)
    parent_cleanup_on_fparam_rtc_done();
}

//Called form child after resume form RTC
int rtc_reset_child_on_resumed()
{
  int seq;
  int pproc_grp_idx;
  int grp_idx;
  unique_group_table_type* unique_table_ptr = NULL; 
  
  NSDL2_RUNTIME(NULL, NULL, "Method called, NVM = %d, total_group_entries = %d, unique_table_ptr = %p", 
                          my_port_index, total_group_entries, unique_table_ptr);

  for (grp_idx = 0, pproc_grp_idx = (my_port_index * total_group_entries) + grp_idx; grp_idx < total_group_entries; grp_idx++, pproc_grp_idx++) 
  {
    NSDL2_RUNTIME(NULL, NULL, "grp_idx = %d, pproc_grp_idx = %d, sequence = %d, per_proc_num = %d, rtc_flag = %d",
                               grp_idx, pproc_grp_idx, group_table_shr_mem[grp_idx].sequence, per_proc_vgroup_table[pproc_grp_idx].num_val, 
                               per_proc_vgroup_table[pproc_grp_idx].rtc_flag);

    //If group take participation in RTC then only reset 
    if(per_proc_vgroup_table[pproc_grp_idx].rtc_flag == -1)
      continue;

    seq = group_table_shr_mem[grp_idx].sequence;

    NSDL2_RUNTIME(NULL, NULL, "NVM RTC reset for file parameter, grp_idx = %d, seq = %d", grp_idx, seq);
    
    switch(seq)
    {
      case SEQUENTIAL:
      case USEONCE:
      {
        if((per_proc_vgroup_table[pproc_grp_idx].rtc_flag == REPLACE_MODE) || 
           (per_proc_vgroup_table[pproc_grp_idx].total_val <= seq_group_next[grp_idx]))
          seq_group_next[grp_idx] = 0; 
      }
      break;

      case UNIQUE:
      {
        unique_table_ptr = unique_group_table + group_table_shr_mem[grp_idx].group_wei_uni.unique_group_id; 

        int old_num_values = unique_table_ptr->num_values;
        int old_start_idx = unique_table_ptr->start;
        int old_end_idx = unique_table_ptr->end;
        int old_num_available = unique_table_ptr->num_available;
        int new_num_values = per_proc_vgroup_table[pproc_grp_idx].num_val;
        int new_num_available = 0;
        int new_start_idx = 0;
        int new_end_idx = 0;
        int num_ignored = 0;
        int num_available_top = 0;
        int i = 0;

        NSDL2_RUNTIME(NULL, NULL, "Reset RTC in Unique:, group_table_id = %d, pproc_grp_idx = %d, rtc_flag = %d, "
                                  "old_num_values = %d, old_start_idx = %d, old_end_idx = %d, old_num_available = %d, "
                                  "new_num_values = %d", 
                                   unique_table_ptr->group_table_id, pproc_grp_idx, per_proc_vgroup_table[pproc_grp_idx].rtc_flag,
                                   old_num_values, old_start_idx, old_end_idx, old_num_available,
                                   new_num_values);

        if(per_proc_vgroup_table[pproc_grp_idx].rtc_flag == REPLACE_MODE)//In replace mode
        { 
          /* Case 1: Values */
          if(new_num_values < old_num_values)
          {   
            new_num_available = old_num_available;
            /*Check position of available values*/
            if(old_start_idx > old_end_idx)
            {
              /*number of available values from 0 to end */
              num_available_top = old_end_idx + 1; 
              /*number of available values from start to num_val */
              new_num_available -= num_available_top;
              /*Copy  available values from top*/
              for(i=0;i<num_available_top;i++)
              {
      	        /*Copy value only if it is less the new values else ignored the value*/
                if(unique_table_ptr->value_table[i] < new_num_values)
                {
                  unique_table_ptr->value_table[new_start_idx++] = unique_table_ptr->value_table[i];
                }
                else
                {
                  num_ignored++;
                }
              }
            } 
            /*number of available values from start to num_val */
            for(i=0; i < new_num_available ; i++)
            {
      	      /*Copy value only if it is less the new values else ignored the value*/
              if(unique_table_ptr->value_table[old_start_idx] < new_num_values)
              {
                unique_table_ptr->value_table[new_start_idx++] = unique_table_ptr->value_table[old_start_idx++];
              }
              else
              {
                num_ignored++;
              }
            }
            RTC_REALLOC(unique_table_ptr->value_table, new_num_values * sizeof(int), "unique_table_ptr->value_table", -1);
            new_num_available = old_num_available - num_ignored;
            new_start_idx=0;
            new_end_idx = new_num_available - 1;
          }
          else if (new_num_values > old_num_values)
          {
            RTC_REALLOC(unique_table_ptr->value_table, new_num_values * sizeof(int), "unique_table_ptr->value_table", -1);
            for(i = old_num_values;  i < new_num_values ; i++)
            {
              unique_table_ptr->value_table[i] = i;
            }
            new_num_available = old_num_available + (new_num_values - old_num_values);
            new_start_idx = old_start_idx;
            new_end_idx = (old_end_idx < old_start_idx)? old_end_idx : new_num_values-1;
          }
          else
          {
            new_num_available = old_num_available; 
            new_start_idx = old_start_idx;
            new_end_idx = old_end_idx;
          }  
        }
        else //Append Mode
        {
          RTC_REALLOC(unique_table_ptr->value_table, new_num_values * sizeof(int), "unique_table_ptr->value_table", -1);

          int start, end;
          if(old_end_idx < old_start_idx)
          {
            start = old_num_values;
            end = new_num_values;
          }
          else
          {
            start = old_end_idx + 1;
            end = start + (new_num_values - old_num_values);
            old_start_idx = !old_num_available?start:old_start_idx;
            old_end_idx = end -1;
          }
          for(i = start;  i < end; i++)
          {
            unique_table_ptr->value_table[i] = (old_num_values - start) + i;
          }

          new_num_available = old_num_available +  (new_num_values - old_num_values);
          new_start_idx = old_start_idx;
          new_end_idx = old_end_idx;
        }

        unique_table_ptr->num_values = new_num_values;
        unique_table_ptr->num_available = new_num_available;
        unique_table_ptr->start = new_start_idx;
        unique_table_ptr->end = new_end_idx;
        unique_table_ptr++;

        //Dump Unique data:
        NSDL2_RUNTIME(NULL, NULL, "Uniqu after RTC: num_values = %d, num_available = %d, start = %d, end = %d", 
                                  new_num_values, new_num_available, new_start_idx, new_end_idx);
      }
      break;
    }
  }
  return 0;
}

inline void nsp_frtc_update_useonce_ctrl_last()
{
  int i,j, pproc_grp_idx, len;
  char file_name[MAX_BUF_LEN_5K + 1];
  char buf[MAX_BUF_LEN_5K + 1];
  int gen_idx = (g_generator_idx < 0)?0:g_generator_idx;

  NSDL1_RUNTIME(NULL, NULL, "Method called, total_group_entries = %d, gen_idx = %d", total_group_entries, gen_idx);

  /* Clear .last files and delete control files */
  for (j = 0; j < total_group_entries; j++)
  {
    if(group_table_shr_mem[j].sequence != USEONCE)
      continue;

    /* Clear .<nvm>.last file so that new data distribution can be applied */
    for (i = 0; i < global_settings->num_process; i++)
    {
      pproc_grp_idx = (i * total_group_entries) + j;

      //If rtc not applied on this nvm-group then continue 
      if(per_proc_vgroup_table[pproc_grp_idx].rtc_flag == -1)
        continue;

      nslb_uo_get_last_file_name(group_table_shr_mem[j].data_fname, file_name, i, gen_idx);

      NSDL3_RUNTIME(NULL, NULL, "NVM = %d, pproc_grp_idx = %d, Clear file %s", i, pproc_grp_idx, file_name);
      len = snprintf(buf, MAX_BUF_LEN_5K, "> %s", file_name);
      buf[len] = '\0';
      
      system(buf); 

      //Reviend fd in last file
      lseek(per_proc_vgroup_table[pproc_grp_idx].last_file_fd, 0L, SEEK_SET);

      len = snprintf(buf, MAX_BUF_LEN_5K, "%d|%d|%d|%llu\n", 
                         per_proc_vgroup_table[pproc_grp_idx].cur_val, 
                         //per_proc_vgroup_table[pproc_grp_idx].start_val, 
                         per_proc_vgroup_table[pproc_grp_idx].num_val, 
                         per_proc_vgroup_table[pproc_grp_idx].total_val, get_ms_stamp()); 
      buf[len] = '\0';
      NSDL3_RUNTIME(NULL, NULL, "buf = %s", buf);

      if(write(per_proc_vgroup_table[pproc_grp_idx].last_file_fd, buf, len) < 0)
      {
        fprintf(stderr, "Error: in writing the data in last file. FD = %d\n", per_proc_vgroup_table[pproc_grp_idx].last_file_fd);
        continue;
      }
    }

    nslb_uo_get_ctrl_file_name(group_table_shr_mem[j].data_fname, file_name, gen_idx);//ctrl file is per grp
    NSDL1_RUNTIME(NULL, NULL, "Removing %s file", file_name);
    unlink(file_name);
  }

  /* Make control file */
  save_ctrl_file();
}

void update_fparam_rtc_struct()
{
  int i, j, grp_idx, proc_idx;

  NSDL2_RUNTIME(NULL, NULL, "Method called");

  if(loader_opcode == MASTER_LOADER)
    return;

  /* Update structure per_proc_vgroup_table only if all NVMs pause done done */
  if(!is_all_nvms_done())
    return;

  /* Update Data file for useonce */
  update_orig_data_file();

  //Update table group_table_shr_mem  
  for(i = 0; i < total_fparam_rtc_tbl_entries; i++)
  {
    NSDL2_RUNTIME(NULL, NULL, "Updating group_table_shr_mem for grp_idx = %d, num_values = %d", 
                            fparam_rtc_tbl[i].fparam_grp_idx, fparam_rtc_tbl[i].num_values);
    grp_idx = fparam_rtc_tbl[i].fparam_grp_idx;
    group_table_shr_mem[grp_idx].num_values = fparam_rtc_tbl[i].num_values;

    //per_proc_vgrou_table
    for(j = 0; j < global_settings->num_process; j++) 
    {
      proc_idx = (j * total_group_entries) + grp_idx; 

      NSDL2_RUNTIME(NULL, NULL, "All NVMs pause done. Updating per_proc_vgroup_table - proc_idx = %d, start_val = %d, cur_val = %d, "
                             "num_val = %d, total_val = %d, shm_addr = %p, shm_key = %d, group_table_shr_mem = %p, seq = %d, rtc_flag = %d", 
                              proc_idx, per_proc_vgroup_table_rtc[proc_idx].start_val, per_proc_vgroup_table_rtc[proc_idx].cur_val,
                              per_proc_vgroup_table_rtc[proc_idx].num_val,
                              per_proc_vgroup_table_rtc[proc_idx].total_val, per_proc_vgroup_table_rtc[proc_idx].shm_addr,
                              per_proc_vgroup_table_rtc[proc_idx].shm_key, per_proc_vgroup_table_rtc[proc_idx].group_table_shr_mem,
                              group_table_shr_mem[grp_idx].sequence, per_proc_vgroup_table_rtc[proc_idx].rtc_flag);

      per_proc_vgroup_table[proc_idx].start_val = per_proc_vgroup_table_rtc[proc_idx].start_val; 

      if((group_table_shr_mem[grp_idx].sequence == USEONCE) && (per_proc_vgroup_table_rtc[proc_idx].rtc_flag == APPEND_MODE))
      {
        per_proc_vgroup_table_rtc[proc_idx].num_val -= per_proc_vgroup_table[proc_idx].cur_val; 
        NSDL2_RUNTIME(NULL, NULL, "cur_val = %d, num_val = %d", 
                                   per_proc_vgroup_table[proc_idx].cur_val, 
                                   per_proc_vgroup_table_rtc[proc_idx].num_val);
      }

      per_proc_vgroup_table[proc_idx].num_val = per_proc_vgroup_table_rtc[proc_idx].num_val; 
      per_proc_vgroup_table[proc_idx].total_val = per_proc_vgroup_table_rtc[proc_idx].total_val; 
      per_proc_vgroup_table[proc_idx].shm_addr = per_proc_vgroup_table_rtc[proc_idx].shm_addr; 
      per_proc_vgroup_table[proc_idx].shm_key = per_proc_vgroup_table_rtc[proc_idx].shm_key; 
      per_proc_vgroup_table[proc_idx].rtc_flag = per_proc_vgroup_table_rtc[proc_idx].rtc_flag; 

      per_proc_vgroup_table[proc_idx].group_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].group_table_shr_mem; 
      per_proc_vgroup_table[proc_idx].variable_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].variable_table_shr_mem; 
      per_proc_vgroup_table[proc_idx].weight_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].weight_table_shr_mem; 
      per_proc_vgroup_table[proc_idx].pointer_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].pointer_table_shr_mem; 
      per_proc_vgroup_table[proc_idx].g_big_buf_shr_mem = per_proc_vgroup_table_rtc[proc_idx].g_big_buf_shr_mem; 

      per_proc_vgroup_table[proc_idx].p_group_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].p_group_table_shr_mem;
      per_proc_vgroup_table[proc_idx].p_variable_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].p_variable_table_shr_mem;
      per_proc_vgroup_table[proc_idx].p_weight_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].p_weight_table_shr_mem;
      per_proc_vgroup_table[proc_idx].p_pointer_table_shr_mem = per_proc_vgroup_table_rtc[proc_idx].p_pointer_table_shr_mem;
      per_proc_vgroup_table[proc_idx].p_g_big_buf_shr_mem = per_proc_vgroup_table_rtc[proc_idx].g_big_buf_shr_mem;
    }  
  }  

  /* Handle useonce case */
  nsp_frtc_update_useonce_ctrl_last();

  /* Resume all NVMs*/ 
  process_resume_from_rtc();
}

void *attach_shm_by_shmid(int shmid)
{
  NSDL3_RUNTIME(NULL, NULL, "Method called, shmid = %d", shmid);
  return attach_shm_by_shmid_ex(shmid, AUTO_DEL_SHM);
}

void *attach_shm_by_shmid_ex(int shmid, int auto_del)
{
  void *shm_addr = NULL;
 
  NSDL3_RUNTIME(NULL, NULL, "Method called, shmid = %d", shmid);

  shm_addr = shmat(shmid, NULL, 0);

  if (shm_addr == (void *) -1) {
    NSTL1(NULL, NULL, "ERROR: unable to attach shm for shmid = %d\n", shmid);
  }
 
  //Mark shm for auto-deletion on exit
  if(auto_del)
  {
    if(shmctl(shmid, IPC_RMID, NULL)) {
        NSTL1(NULL, NULL, "ERROR: unable to mark shm removal for shmid = %d\n", shmid);
    }
  }

  return shm_addr;
}

int get_shm_info(int shmid)
{
  struct shmid_ds shm_info;

  memset(&shm_info, 0, sizeof(struct shmid_ds));

  NSDL2_RUNTIME(NULL, NULL, "Method called, shmid = %d", shmid);

  if(shmctl(shmid, IPC_STAT, &shm_info))
     printf ("ERROR: failed to get shm info for shmid = %d\n", shmid);

  NSTL1(NULL, NULL,  "Shm-At Info: shmid = %d, Size of segment (shm_segsz) = %zu, Last attach time (shm_atime) = %zu, "
                     "Last detach time (shm_dtime) = %zu, Last change time (shm_ctime) = %zu, PID of creator (shm_cpid) = %zu, "
                     "PID of last shmat (shm_lpid) = %zu, No. of current attaches (shm_nattch) = %zu",
                      shmid, shm_info.shm_segsz, shm_info.shm_atime, shm_info.shm_dtime, shm_info.shm_ctime,
                      shm_info.shm_cpid, shm_info.shm_lpid, shm_info.shm_nattch);

  return shm_info.shm_segsz;
}

//From nvm to parent
void process_fparam_rtc_attach_shm_msg(parent_child *msg)
{
  parent_child send_msg;

  NSDL3_RUNTIME(NULL, NULL, "Method called, Got message form parent - Attching shared memory by provided id = %d, send_msg.gen_rtc_idx = %d",
                             msg->shm_id, msg->gen_rtc_idx);

  NSTL1(NULL, NULL, "(Parent -> NVM:%d) FPARAM_RTC_ATTACH_SHM_MSG(24) processing shared memory", 
                    my_port_index);

  send_msg.opcode = FPARAM_RTC_ATTACH_SHM_DONE_MSG;
  send_msg.child_id = my_port_index;
  send_msg.grp_idx = msg->grp_idx;
  send_msg.shm_id = msg->shm_id;
  send_msg.shm_addr = attach_shm_by_shmid(msg->shm_id);
  send_msg.gen_rtc_idx = msg->gen_rtc_idx;
  send_child_to_parent_msg("FPARAM_RTC_ATTACH_SHM_DONE_MSG", (char *)&send_msg, sizeof(send_msg), CONTROL_MODE);
}

void process_fparam_rtc_attach_shm_done_msg(parent_msg *msg)
{
  parent_child *msg_ptr = (parent_child *)&msg->top.internal;
  PerProcVgroupTable *per_proc_fparam_grp = NULL;
  GroupTableEntry_Shr *lol_group_table_shr_mem = NULL;
  VarTableEntry_Shr *lol_variable_table_shr_mem = NULL;
  WeightTableEntry *lol_weight_table_shr_mem = NULL;
  PointerTableEntry_Shr *lol_pointer_table_shr_mem = NULL;
  char *lol_g_big_buf_shr_mem = NULL;

  int i, j;
  unsigned long shm_offset = 0;
  int per_proc_num_val = 0;

  NSTL1(NULL, NULL, "%s:%d) FPARAM_RTC_ATTACH_SHM_MSG(24) processing shared memory", 
                     (loader_opcode == CLIENT_LOADER)?"(Generator <- NVM:":"(Parent <- NVM:", msg_ptr->child_id);
 
  NSDL2_RUNTIME(NULL, NULL, "Method called, Got message from child - Rearrange shared memory,"
                            " child_id = %d, grp_idx = %d, shm_id = %d, addr = %p, all_nvms_sent = %d", 
                            msg_ptr->child_id, msg_ptr->grp_idx, msg_ptr->shm_id, msg_ptr->shm_addr,
                            all_nvms_sent[msg_ptr->child_id]);

  if(msg_ptr->gen_rtc_idx != rtcdata->msg_seq_num)
  {
    NSDL2_MESSAGES(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d",
                                msg_ptr->gen_rtc_idx, rtcdata->msg_seq_num);
    NSTL1(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d",
                       msg_ptr->gen_rtc_idx, rtcdata->msg_seq_num);
    return;
  }

  if(rtcdata->cur_state == RESET_RTC_STATE)
  {
    NSTL1(NULL, NULL, "ERROR: FPARAM_RTC_ATTACH_SHM_MSG(24), rtc state = %d, which is always"
                      " be set in case of failure. This will not processed", rtcdata->cur_state);
    return;
  }
  
  //Re-arrange per proc table address 
  per_proc_fparam_grp = &per_proc_vgroup_table_rtc[(msg_ptr->child_id * total_group_entries) + msg_ptr->grp_idx];

  lol_weight_table_shr_mem = per_proc_fparam_grp->weight_table_shr_mem;
  lol_group_table_shr_mem = per_proc_fparam_grp->group_table_shr_mem;
  lol_variable_table_shr_mem = per_proc_fparam_grp->variable_table_shr_mem;
  lol_pointer_table_shr_mem = per_proc_fparam_grp->pointer_table_shr_mem;
  lol_g_big_buf_shr_mem = per_proc_fparam_grp->g_big_buf_shr_mem;

  per_proc_num_val = per_proc_fparam_grp->num_val;

  //Get parent and child memory distance 
  shm_offset = (char *)msg_ptr->shm_addr - (char *)per_proc_fparam_grp->shm_addr;

  NSDL2_RUNTIME(NULL, NULL, "shm_offset = %d, num_vars = %d, per_proc_num_val = %d", 
                              shm_offset, lol_group_table_shr_mem->num_vars, per_proc_num_val);

  //Update pointers of group_table_shr_mem 
  if(lol_group_table_shr_mem->sequence == WEIGHTED)
     lol_group_table_shr_mem->group_wei_uni.weight_ptr = (WeightTableEntry *)((char *)lol_group_table_shr_mem->group_wei_uni.weight_ptr + shm_offset);

  //Update pointers of variable_table_shr_mem
  for (i = 0; i < lol_group_table_shr_mem->num_vars; i++)
  {
    NSDL2_RUNTIME(NULL, NULL, "DIFF: var = %d, Parent = %d, child = %d", i, 
                             ((char *)(lol_variable_table_shr_mem[i].value_start_ptr) - (char *)per_proc_fparam_grp->shm_addr), 
                             ((char *)(lol_variable_table_shr_mem[i].value_start_ptr) + shm_offset)- (char *)msg_ptr->shm_addr);

    lol_variable_table_shr_mem[i].group_ptr = (GroupTableEntry_Shr *)((char *)(lol_variable_table_shr_mem[i].group_ptr) + shm_offset);
    lol_variable_table_shr_mem[i].value_start_ptr = (PointerTableEntry_Shr *)((char *)(lol_variable_table_shr_mem[i].value_start_ptr) + shm_offset); 
  }

  for(j = 0; j < (per_proc_num_val * lol_group_table_shr_mem->num_vars); j++)
  {
    lol_pointer_table_shr_mem[j].big_buf_pointer = ((char *)lol_pointer_table_shr_mem[j].big_buf_pointer) + shm_offset;
  }

  //Update start addr og weight, group, variable and pointer table
  per_proc_fparam_grp->weight_table_shr_mem = (WeightTableEntry *)((char *)lol_weight_table_shr_mem + shm_offset);
  per_proc_fparam_grp->group_table_shr_mem = (GroupTableEntry_Shr *)((char *)lol_group_table_shr_mem + shm_offset);
  per_proc_fparam_grp->variable_table_shr_mem = (VarTableEntry_Shr *)((char *)lol_variable_table_shr_mem + shm_offset);
  per_proc_fparam_grp->pointer_table_shr_mem = (PointerTableEntry_Shr *)((char *)lol_pointer_table_shr_mem + shm_offset);
  per_proc_fparam_grp->g_big_buf_shr_mem = (char *)lol_g_big_buf_shr_mem + shm_offset;

  all_nvms_sent[msg_ptr->child_id]--;   
  NSDL2_RUNTIME(NULL, NULL, "all_nvms_sent = %d", all_nvms_sent[msg_ptr->child_id]);
  if(!all_nvms_sent[msg_ptr->child_id]) //All RTC Message Processed for NVM 
    if(is_all_nvms_done())
      process_pause_for_rtc(RTC_PAUSE, g_rtc_msg_seq_num, NULL);
}

int send_attach_shm_msg_to_nvms()
{
  int k, j;
  int nvm_id;
  int all_nvms_in_rtc = 0;
  parent_child send_msg;

  NSDL2_RUNTIME(NULL, NULL, "Method called, Send message to NVM to attach shared memory");

  NSTL1(NULL, NULL, "(%s) FPARAM_RTC_ATTACH_SHM_MSG(24) processing provided data", 
                     ((loader_opcode == CLIENT_LOADER)?"Generator -> NVMs":"Parent -> NVMs"));
 
   send_msg.opcode = FPARAM_RTC_ATTACH_SHM_MSG;
   send_msg.gen_rtc_idx = ++(rtcdata->msg_seq_num);
  /* If Controller then no need to send this message to geneartros
     control should not come in thi sfunction in case of controller */
  if (loader_opcode == MASTER_LOADER) 
     return 0;

  //Send message to NVM iff start phase for all nvm done
   if(!got_start_phase)
   {
     NSTL1_OUT(NULL, NULL, "Error: file parameter RTC can't be applied untill All the NVM not complete their start phase.");
     return -1;
   }

  /* Here we are sending message to all NVMs. Not checking if any NVM is kiiled or not
     write_msg function handles if fd is -1 then it return */
  for (k = 0; k < global_settings->num_process; k++) 
  {
    int ret = 0;
    int rtc_flag_for_nvm = 0;
    nvm_id = g_msg_com_con[k].nvm_index;
    if(nvm_id < 0)
    {
      NSDL3_RUNTIME(NULL, NULL, "NVM not running - k = %d, nvm_id = %d", k, nvm_id);
      continue;
    }

    for (j = 0; j < total_group_entries; j++)
    {
      //Bug 56270 - Getting Error while applying RTC for updating Data file.
      //if(!is_rtc_applied_on_group(per_proc_vgroup_table_rtc[(nvm_id * total_group_entries) + j].num_script_users, j)) continue;
      ret = is_rtc_applied_on_group(per_proc_vgroup_table_rtc[(nvm_id * total_group_entries) + j].num_script_users, j);

      if(!ret) continue;
      else rtc_flag_for_nvm = 1; //Set flag to know that this nvm is participated in RTC

      send_msg.shm_id = per_proc_vgroup_table_rtc[(nvm_id * total_group_entries) + j].shm_key;
      send_msg.grp_idx = j;
      send_msg.msg_len = sizeof(send_msg) - sizeof(int);

      NSDL3_RUNTIME(NULL, NULL, "Sending FPARAM_RTC_ATTACH_SHM_MSG to child = %d, fpram group = %d, shm_key = %d, k = %d", 
                                 nvm_id, j, send_msg.shm_id, k);

      if((write_msg(&g_msg_com_con[k], (char *)&send_msg, sizeof(send_msg), 0, CONTROL_MODE)) == RUNTIME_SUCCESS)
        all_nvms_sent[nvm_id]++;
    }
    if((ret != 0) || rtc_flag_for_nvm)
      all_nvms_in_rtc++; //Counter to knows the no of nvms participated in RTC
  }
  //Set all_nvms_done counter equals to no of nvms are not participated in RTC
  all_nvms_done = global_settings->num_process - all_nvms_in_rtc;
  NSDL3_RUNTIME(NULL, NULL, "Balram: all_nvms_in_rtc = %d, all_nvms_done = %d", all_nvms_in_rtc, all_nvms_done);

  return 0;
}

