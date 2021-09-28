#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
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
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_vars.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"

#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h" //for ns_handle
#include "amf.h" //for amf_asc_ptr
#include "ns_debug_trace.h"
#include "poi.h" //for ns_get_random
#include "ns_string.h" //ns_encode_url etc
#include "ns_event_log.h"
#include "nslb_util.h" //for get_tokens()

#include "ns_random_vars.h"
#include "ns_date_vars.h"
#include "ns_exit.h"
#include "ns_script_parse.h"

#ifndef CAV_MAIN
DateVarTableEntry* dateVarTable;
int max_datevar_entries = 0;
static int total_datevar_entries;
static int max_holiday_entries = 0;
static int total_holiday_entries = 0;
static int sec_per_nvm;
Cav_holi_days *g_holi_days;
#else
//__thread DateVarTableEntry* dateVarTable;
__thread int max_datevar_entries = 0;
__thread int total_datevar_entries;
static __thread int max_holiday_entries = 0;
static __thread int total_holiday_entries = 0;
static __thread int sec_per_nvm;
__thread Cav_holi_days *g_holi_days;
#endif
static time_t get_wrking_non_wrking_day(time_t date_in_sec, int next, int day_type);


int create_holiday_table_entry(int* row_num) {
  if (total_holiday_entries == max_holiday_entries) {
    MY_REALLOC(g_holi_days, (max_holiday_entries + DELTA_HOLIDAY_ENTRIES) * sizeof(Cav_holi_days), "Cav_holi_days", -1);
    if (!g_holi_days) {
      fprintf(stderr, "create_holiday_table_entry(): Error allocating more memory for holiday entries\n");
      return (FAILURE);
    } else max_holiday_entries += DELTA_HOLIDAY_ENTRIES;
  }
  *row_num = total_holiday_entries++;
  return (SUCCESS);
}


static int create_datevar_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_datevar_entries = %d, max_datevar_entries = %d", total_datevar_entries, max_datevar_entries);

  if (total_datevar_entries == max_datevar_entries) {
    MY_REALLOC (dateVarTable, (max_datevar_entries + DELTA_DATEVAR_ENTRIES) * sizeof(DateVarTableEntry), "datevar entries", -1);
    max_datevar_entries += DELTA_DATEVAR_ENTRIES;
  }
  *row_num = total_datevar_entries++;
  return (SUCCESS);
}

void init_datevar_info(void)
{
  NSDL1_VARS(NULL, NULL, "Method called.");
  total_datevar_entries = 0;
 // total_datepage_entries = 0;

  MY_MALLOC (dateVarTable, INIT_DATEVAR_ENTRIES * sizeof(DateVarTableEntry), "dateVarTable", -1);

  if(dateVarTable)
  {
    max_datevar_entries = INIT_DATEVAR_ENTRIES;
  }
  else
  {
    max_datevar_entries = 0;
    NS_EXIT(-1, CAV_ERR_1031013, "DateVarTableEntry");
  }
}

// 5.19:90:00
static void validate_offset_format(char* ptr){
  char *tmp_ptr = ptr;
  int flag = 1;

  while(*ptr != '\0'){
    if(flag){
      if(isdigit(*ptr)){
        ptr++;
        continue;
      }else{
        if(*ptr != '.'){
          NS_EXIT(1, "Error: validate_offset_format()- Invalid format [%s] of time offset. There should be '.' after day value\n", tmp_ptr);
        }
        else{
          flag = 0;
          ptr++;
        }
      }
    }
    if(isdigit(*ptr) || *ptr == ':'){
       ptr++;
       continue;
    }
    else{
      NS_EXIT(1, "Error: validate_offset_format()- Invalid format [%s] of time offset. There should be only ':' in between time values", tmp_ptr);
    }  
  }
}

/*
 * routine to read and parse the line -- nsl_random_number_var() in script.capture
 * format -- nsl_random_number_var(var_name, min, max, format)
 * var_name - variable name to store the random number + format
 * min - min value of random no
 * max - max value of random no
 * format - the random no is printed in the
 * format specified into the variable and stored as such.
 */

int input_datevar_data(char* line, int line_number, int sess_idx, char *script_filename)
{
#define NS_ST_SE_NAME 1
#define NS_ST_SE_OPTIONS 2
#define NS_ST_STAT_VAR_NAME 1

  int state = NS_ST_SE_NAME;
  int rnum, ret;
  int flag_sign = 0;

  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char msg[MAX_LINE_LENGTH];
  int j;
  NSApi api_ptr;  // For parsing from parse_api

  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];

  NSDL1_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d", line, line_number, sess_idx);

  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_date_var() declaration on line %d of scripts/%s/%s: ", line_number,
																						get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
																						//get_sess_name_with_proj_subproj(sess_name), script_filename);
  msg[MAX_LINE_LENGTH-1] = '\0';

  //parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 1);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }
  for(j = 0; j < api_ptr.num_tokens; j++) {
    NSDL2_VARS(NULL, NULL, "state = %d", state);
    switch (state) {

      case NS_ST_SE_NAME:
        create_datevar_table_entry(&rnum);

        /* First fill the default values */
        dateVarTable[rnum].name = -1;
        dateVarTable[rnum].format = -1;
        dateVarTable[rnum].day_offset = 0;
        dateVarTable[rnum].time_offset = 0;
        dateVarTable[rnum].min_days = 0;
        dateVarTable[rnum].max_days = 0;
        dateVarTable[rnum].day_type = ALL_DAYS;
        dateVarTable[rnum].unique_date = NO;
        dateVarTable[rnum].refresh = SESSION;
        dateVarTable[rnum].sess_idx = sess_idx;

        if (gSessionTable[sess_idx].datevar_start_idx == -1) {
          gSessionTable[sess_idx].datevar_start_idx = rnum;
          gSessionTable[sess_idx].num_datevar_entries = 0;
        }

        /* For validating the variable we are calling the validate_var funcction */
        if(validate_var(api_ptr.api_fields[j].keyword)) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, api_ptr.api_fields[j].keyword);
        }

        gSessionTable[sess_idx].num_datevar_entries++;

        if ((dateVarTable[rnum].name = copy_into_big_buf(api_ptr.api_fields[j].keyword, 0)) == -1) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, api_ptr.api_fields[j].keyword);
        }
        
        //fprintf(stderr, "Date var = %s.\n", datevar_buf);
        state = NS_ST_SE_OPTIONS;
        break;

      case NS_ST_SE_OPTIONS:
        if(dateVarTable[rnum].name == -1)
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "DateTime");
        }
        if (!strncasecmp(api_ptr.api_fields[j].keyword, "format", strlen("format"))) {

          dateVarTable[rnum].format_len = strlen(api_ptr.api_fields[j].value);

          if ((dateVarTable[rnum].format = copy_into_big_buf(api_ptr.api_fields[j].value, 0)) == -1) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, api_ptr.api_fields[j].value);
          }
        }
        else if (!strncasecmp(api_ptr.api_fields[j].keyword, "MinDays", strlen("MinDays"))) {
          if ((nslb_atoi(api_ptr.api_fields[j].value, &dateVarTable[rnum].min_days) < 0) || (dateVarTable[rnum].min_days < 0))
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", "Invalid format for MinDays and value can not be less than zero");
          NSDL2_VARS(NULL, NULL, "dateVarTable[rnum].min_days = %d", dateVarTable[rnum].min_days);
        }
        else if (!strncasecmp(api_ptr.api_fields[j].keyword, "MaxDays", strlen("MaxDays"))) {
          if ((nslb_atoi(api_ptr.api_fields[j].value, &dateVarTable[rnum].max_days) < 0) || (dateVarTable[rnum].max_days < 0))
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", "Invalid format for MaxDays and value can not be less than zero");
          NSDL2_VARS(NULL, NULL, "dateVarTable[rnum].max_days = %d", dateVarTable[rnum].max_days);
        }
        else if (!strncasecmp(api_ptr.api_fields[j].keyword, "offset", strlen("offset"))) {
          int time = 0;
          char *tmp_offset = api_ptr.api_fields[j].value;
          char *time_ptr;

          if(*tmp_offset == '+' || *tmp_offset == '-')
          {
            *tmp_offset == '-'?(flag_sign = 1):(flag_sign = 0);
            tmp_offset++;
          }
 
          dateVarTable[rnum].day_offset = atoi(tmp_offset); 
          /*Convert day offset into seconds*/
          //dateVarTable[rnum].day_offset = (dateVarTable[rnum].day_offset * 24 * 60 * 60);

          validate_offset_format(tmp_offset);
          time_ptr = strchr(tmp_offset, '.');

          if(time_ptr != NULL)
          {
            time_ptr++;
            time = get_time_from_format(time_ptr);
          } 
          time = time/1000; // Because we are getting time in milli seconds 
          
          if(flag_sign){
            time = -time;
            dateVarTable[rnum].day_offset = -(dateVarTable[rnum].day_offset);
          }

          dateVarTable[rnum].time_offset = time;
          NSDL2_VARS(NULL, NULL, "dateVarTable[rnum].day_offset = %d, dateVarTable[rnum].time_offset = %d",
                                  dateVarTable[rnum].day_offset, dateVarTable[rnum].time_offset);
        } else if (!strncasecmp(api_ptr.api_fields[j].keyword, "REFRESH", strlen("REFRESH"))) {
          if (state == NS_ST_STAT_VAR_NAME) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "DateTime");
          }

          if (!strcmp(api_ptr.api_fields[j].value, "SESSION")) {
            dateVarTable[rnum].refresh = SESSION;
          } else if (!strcmp(api_ptr.api_fields[j].value, "USE")) {
            dateVarTable[rnum].refresh = USE;
          } else {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, api_ptr.api_fields[j].value, "Update Value On", "DateTime");
          }
        }else if (!strncasecmp(api_ptr.api_fields[j].keyword, "DayType", strlen("DayType"))) {

           FILE *fp = NULL;
           char holi_days_file[1024];
           char line [4096];
           char *fields[2];
           int row_num;

          if (state == NS_ST_STAT_VAR_NAME) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "DateTime");
          }

          sprintf(holi_days_file, "%s/sys/holidays.dat", g_ns_wdir); 
          
          if (!strcmp(api_ptr.api_fields[j].value, "ALL")) {
            dateVarTable[rnum].day_type = ALL_DAYS;
          }else if (!strcmp(api_ptr.api_fields[j].value, "WORKING")) {
            dateVarTable[rnum].day_type = WORKING_DAYS;
          }else if (!strcmp(api_ptr.api_fields[j].value, "NONWORKING")) {
            dateVarTable[rnum].day_type = NON_WORKING_DAYS;
          }else {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, api_ptr.api_fields[j].value, "Day Type", "DateTime");
          }
          /*Fill holiday file into memory*/
          fp = fopen(holi_days_file, "r");
          if( fp == NULL)
            NSDL3_VARS(NULL, NULL, "Holidays file %s is not present", holi_days_file);
          else
          {
            while(nslb_fgets(line, 4096, fp, 0))
            {
              get_tokens(line, fields, "|", 2);
              create_holiday_table_entry(&row_num);
              strcpy(g_holi_days[row_num].date, fields[0]);
              strcpy(g_holi_days[row_num].description, fields[1]);
              //g_holi_days[row_num].date_in_num = get_date_in_number(fields[0]);
            }
          } 
        }
        else if (!strncasecmp(api_ptr.api_fields[j].keyword, "UNIQUE", strlen("UNIQUE"))) {
          if (state == NS_ST_STAT_VAR_NAME) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "DateTime");
          }
          //fprintf(stderr, "Unique  = %s\n", datevar_buf);
          if (!strcmp(api_ptr.api_fields[j].value, "YES")) {
            dateVarTable[rnum].unique_date = YES;
          } else if (!strcmp(api_ptr.api_fields[j].value, "NO")) {
            dateVarTable[rnum].unique_date = NO;
          } else {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, api_ptr.api_fields[j].value, "Unique for each Session", "DateTime");
          }
        }       
    }
  } //while
  if (dateVarTable[rnum].format == -1)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Format", "DateTime");
  }

  //One of from MinDays or MaxDays are not set
  if(((dateVarTable[rnum].min_days) && (!dateVarTable[rnum].max_days)) || ((!dateVarTable[rnum].min_days) && (dateVarTable[rnum].max_days)))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012103_ID, CAV_ERR_1012103_MSG, "In random date selection, 'MinDays' & 'MaxDays' value can not be zero.");
  }

  if(dateVarTable[rnum].max_days < dateVarTable[rnum].min_days)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012103_ID, CAV_ERR_1012103_MSG, "'MaxDays' value can not be less than 'MinDays' value.");
  }
  //Case: MinDays=10 & MaxDays=10, 
  //Igrnoing case: MinDays=0, MaxDays=0 
  if((dateVarTable[rnum].max_days == dateVarTable[rnum].min_days) && (dateVarTable[rnum].max_days + dateVarTable[rnum].min_days))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012103_ID, CAV_ERR_1012103_MSG, "'MaxDays' value can not be equal to 'MinDays' value.");
  }

  //Both offset and range value are set 
  if((dateVarTable[rnum].min_days) && (dateVarTable[rnum].time_offset || dateVarTable[rnum].day_offset))
  {
    NS_DUMP_WARNING("In case of random date selection, offset(Day and Time) value should be zero.");
    dateVarTable[rnum].day_offset = 0;
    dateVarTable[rnum].time_offset = 0; 
  }

  //If MaxDays value is greater than 365 days then setting its value to 365
  if(dateVarTable[rnum].max_days > ONE_YEAR)
  {
    NS_DUMP_WARNING("In random date selection, 'MaxDays' value is greater than One year, so setting its value to 365.");
    dateVarTable[rnum].max_days = ONE_YEAR;
  }
  NSDL3_VARS(NULL, NULL, "Format Len = %d, refresh = %d, day_offset = %d, time_offset = %d, day_type = %d, unique_date = %d, i"
                 "Min_days = %d, Max_days = %d", dateVarTable[rnum].format_len, dateVarTable[rnum].refresh, dateVarTable[rnum].day_offset,
                 dateVarTable[rnum].time_offset, dateVarTable[rnum].day_type, dateVarTable[rnum].unique_date, dateVarTable[rnum].min_days,
                 dateVarTable[rnum].max_days);
        
#undef NS_ST_SE_NAME
#undef NS_ST_SE_OPTIONS
  return(0);
}

/*
 * copies values in the datevar table into shared memory
 */

DateVarTableEntry_Shr *copy_datevar_into_shared_mem(void)
{
  int i;
  DateVarTableEntry_Shr *datevar_table_shr_mem_local = NULL;

  NSDL1_VARS(NULL, NULL, "Method called. total_datevar_entries = %d", total_datevar_entries);
  /* insert the SearchVarTableEntry_Shr and the PerPageSerVarTableEntry_shr */

  if (total_datevar_entries ) {
    datevar_table_shr_mem_local = do_shmget(total_datevar_entries * sizeof(DateVarTableEntry_Shr), "date var tables");

    for (i = 0; i < total_datevar_entries; i++) {
     //check for another variables 
      datevar_table_shr_mem_local[i].var_name = BIG_BUF_MEMORY_CONVERSION(dateVarTable[i].name);
      datevar_table_shr_mem_local[i].format_len = dateVarTable[i].format_len;
      datevar_table_shr_mem_local[i].day_offset = dateVarTable[i].day_offset;
      datevar_table_shr_mem_local[i].time_offset = dateVarTable[i].time_offset;
      datevar_table_shr_mem_local[i].refresh = dateVarTable[i].refresh;
      datevar_table_shr_mem_local[i].day_type = dateVarTable[i].day_type;
      datevar_table_shr_mem_local[i].unique_date = dateVarTable[i].unique_date;
      datevar_table_shr_mem_local[i].min_days = dateVarTable[i].min_days;
      datevar_table_shr_mem_local[i].max_days = dateVarTable[i].max_days;
      datevar_table_shr_mem_local[i].sess_idx = dateVarTable[i].sess_idx;
      datevar_table_shr_mem_local[i].date_time_data = NULL;

      if (dateVarTable[i].format == -1)
        datevar_table_shr_mem_local[i].format = NULL;
      else
        datevar_table_shr_mem_local[i].format = BIG_BUF_MEMORY_CONVERSION(dateVarTable[i].format);

      datevar_table_shr_mem_local[i].uv_table_idx =
        gSessionTable[dateVarTable[i].sess_idx].vars_trans_table_shr_mem[gSessionTable[dateVarTable[i].sess_idx].var_hash_func(datevar_table_shr_mem_local[i].var_name, strlen(datevar_table_shr_mem_local[i].var_name))].user_var_table_idx;
      assert (datevar_table_shr_mem_local[i].uv_table_idx != -1);
    }
  }
  return (datevar_table_shr_mem_local);
}

/*This function to update the uniq date time data struct*/
static void update_uniq_date_time_date(Cav_uniq_date_time_data *date_time_data)
{
  NSDL2_VARS(NULL,NULL, "Method called, Curr Secs = %d, end sec = %d", date_time_data->cur_sec, date_time_data->end_sec);

  date_time_data->cur_sec += sec_per_nvm * global_settings->num_process;
  date_time_data->end_sec += sec_per_nvm * global_settings->num_process;
  
  NSDL2_VARS(NULL,NULL, "After updating, Curr Secs = %d, end sec = %d", date_time_data->cur_sec, date_time_data->end_sec);
}

char *get_formated_cur_date(int format_len, char *format, int day_offset, int time_offset, int day_type, int is_uniq,
                                     Cav_uniq_date_time_data *date_time_data, int *total_len) 
{
  
  char *outstr; 
  time_t t1, t;
  struct tm *tmp;
  struct tm l_tm;
  time_t secs = 0;
  char local_format[512];
  struct tm tm_struct;

  NSDL2_VARS(NULL, NULL, "Method called, format = %s, day_offset = %d, Time offset = %d, day_type = %d, Uniq = %d", format, day_offset, time_offset, day_type, is_uniq);

  if(format) {
    strcpy(local_format, format);
  } else {
    strcpy(local_format, "%a %b  %d %H:%M:%S %Y");
  }

  secs = day_offset * 24 * 60 * 60 + time_offset;

  t1 = time(NULL);

  t = t1 + secs;

  NSDL2_VARS(NULL, NULL, "t1 = %u, t = %u, day_offset = %d, format = %s", t1, t, day_offset, local_format);
  
  if(day_type == WORKING_DAYS || day_type == NON_WORKING_DAYS)
  {
    if(day_offset < 0 || time_offset < 0)
      t = get_wrking_non_wrking_day(t, -1, day_type);
    else
      t = get_wrking_non_wrking_day(t, 1, day_type);
  }
  /*Till here we will have t fileed with new secs.
   * But if parameter is given as unique then we have to use last
   * saved time. So we need to replace the t last saved secs.*/
  if(is_uniq == YES)
  {
    /*If API is called first time then save the current time.
     *We will add secs in this time to get uniq time*/
    if(date_time_data->init_time == -1)
    {
      date_time_data->init_time = t;
    }

    /*Parameter has uniq date feature*/
    if(date_time_data->cur_sec == date_time_data->end_sec)
    {
      update_uniq_date_time_date(date_time_data);
    }

    t = date_time_data->init_time + date_time_data->cur_sec; 

    //fprintf(stderr, "Init time = %u, Cur Time = %d, Child NVM = %d, date_time_data->end_sec = %d, T = %d\n", date_time_data->init_time, date_time_data->cur_sec, my_port_index, date_time_data->end_sec, (int)t); 

    date_time_data->cur_sec++;
    NSDL4_VARS(NULL, NULL, "Init time = %u, Cur Time = %d, Child NVM = %d, date_time_data->end_sec = %d, T = %d\n", date_time_data->init_time, date_time_data->cur_sec, my_child_index, date_time_data->end_sec, (int)t); 
  }
    
  MY_MALLOC (outstr, (format_len + 200), "outstr", -1);
  memset(outstr, 0, (format_len + 200));

  /*Dont move below these lines. 
 * Dont put any debug messagaes between the call of localtime
 * function and  memcpy function. As time functions return the static buffer
 * and when we call any debug message that static buffer again overwrite 
 * with latest value.*/
  tmp = nslb_localtime(&t, &tm_struct, 1);
  if (tmp == NULL) 
  {
    perror("localtime");
    strcpy(outstr, "cav_cur_date: error");
    return outstr;
    //exit(EXIT_FAILURE);
  }
  memcpy(&l_tm, tmp, sizeof(struct tm));

  NSDL4_VARS(NULL, NULL, "NVM = %d, tmp->tm_sec = %u; tmp->tm_min =%d, tmp->tm_hour = %d, tmp->tm_mday =%d, tmp->tm_mon =%d, tmp->tm_year =%d, tmp->tm_wday =%d, tmp->tm_yday =%d, tmp->tm_isdst =%d", my_child_index, tmp->tm_sec, tmp->tm_min, tmp->tm_hour, tmp-> tm_mday, tmp->tm_mon, tmp->tm_year, tmp->tm_wday, tmp->tm_yday, tmp->tm_isdst);    



  //NSDL4_VARS(NULL, NULL, "tmp->tm_sec = %u; tmp->tm_min =%d, tmp->tm_hour = %d, tmp->tm_mday =%d, tmp->tm_mon =%d, tmp->tm_year =%d, tmp->tm_wday =%d, tmp->tm_yday =%d, tmp->tm_isdst =%d", tmp->tm_sec, tmp->tm_min, tmp->tm_hour, tmp-> tm_mday, tmp->tm_mon, tmp->tm_year, tmp->tm_wday, tmp->tm_yday, tmp->tm_isdst);    

  //fprintf(stderr, "format = %s\n", local_format);
  char msecs_delimeter;
  long long msecs = -1;
  int mseconds = -1;
  if(!strcmp(local_format,"%ms"))
  {
    msecs = nslb_get_cur_time_in_ms() + (secs * 1000);  
    NSDL2_VARS(NULL, NULL, "Milli secs format are using and current time is %lld", msecs); 
    *total_len = sprintf(outstr,"%lld",msecs); 
    return outstr;   
  }
  if(local_format[format_len-3] == '%' && local_format[format_len-2] == 'M' && local_format[format_len-1] == 'S')
  {
    msecs = nslb_get_cur_time_in_ms();
    mseconds = msecs % 1000;
    format_len -= 4;
    msecs_delimeter = local_format[format_len];
    local_format[format_len] = '\0';
  }
  *total_len = strftime(outstr, (format_len + 200), local_format, &l_tm);
  if (*total_len == 0) {
    fprintf(stderr, "strftime returned 0");
    strcpy(outstr, "cav_cur_date: error");
    return outstr;
    //exit(EXIT_FAILURE);
  }
  if(mseconds != -1)
  {
    *total_len += sprintf(&outstr[*total_len],"%c%03d",msecs_delimeter,mseconds); 
  } 
  NSDL2_VARS(NULL,NULL, "outstr = %s, NVM = %d", outstr, my_child_index);
  //fprintf(stderr, "======================>outstr = %s, NVM = %d\n", outstr, my_port_index);
  return outstr;
}

#if 0
char *get_cur_date(size_t *len)
{
  time_t tm;
  char *t;
  tm = time(NULL);
  t = ctime(&tm);
  *len = strlen(t);
  NSDL2_VARS(NULL, NULL, "Method called. returning %s", t);
  return t;
}
#endif

char* get_date_var_value(DateVarTableEntry_Shr* var_ptr, VUser* vptr, int var_val_flag, int* total_len)
{
  char* date_val;
  char *err = "Variable is not initialize";
  short err_len = 26;
  Cav_uniq_date_time_data *date_time_data = &var_ptr->date_time_data[my_port_index];

  NSDL1_VARS(NULL, NULL, "Method called var_ptr %p vptr %p var_val_flag %d",var_ptr, vptr, var_val_flag);

  if (var_val_flag) { //fill new value
    if (var_ptr->refresh == SESSION)  { //SESSION
       NSDL1_VARS(NULL, NULL, "Entering method %s, with value  = %s, filled_flag = %d", __FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value, vptr->uvtable[var_ptr->uv_table_idx].filled_flag);

      if (vptr->uvtable[var_ptr->uv_table_idx].filled_flag == 0) { //first call. assign value
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Calling get_date_var first time");
        if (var_ptr->min_days && var_ptr->max_days)
           var_ptr->day_offset = ns_get_random_number_int(var_ptr->min_days, var_ptr->max_days); 
        date_val = get_formated_cur_date(var_ptr->format_len, var_ptr->format, var_ptr->day_offset, var_ptr->time_offset, var_ptr->day_type, var_ptr->unique_date, date_time_data, total_len);
        NSDL1_VARS(NULL, NULL, "Got Date var  = %s, Length = %d", date_val,*total_len);
        vptr->uvtable[var_ptr->uv_table_idx].value.value  = date_val;
        vptr->uvtable[var_ptr->uv_table_idx].length = *total_len;
        vptr->uvtable[var_ptr->uv_table_idx].filled_flag = 1;
      }else{ // need to return length as we're using this on return
        NSDL1_VARS(NULL, NULL, "Refresh is SESSION, Value already set");
        *total_len = vptr->uvtable[var_ptr->uv_table_idx].length;
      }
    }else{ //USE
      NSDL1_VARS(NULL, NULL, "Refresh is USE, Calling get_date_var");
       if(var_ptr->min_days && var_ptr->max_days)
         var_ptr->day_offset = ns_get_random_number_int(var_ptr->min_days, var_ptr->max_days); 
       date_val = get_formated_cur_date(var_ptr->format_len, var_ptr->format, var_ptr->day_offset, var_ptr->time_offset, var_ptr->day_type, var_ptr->unique_date, date_time_data, total_len);
       /*Checking here for filled flag, if its set to 1 it means we have already
        * malloced the buffer.Its possible if we are using same variable multiple
        * times with USE in same session. So, we need to first free old memory
        * then assign new memory*/
      if (vptr->uvtable[var_ptr->uv_table_idx].filled_flag == 1)
      {
        FREE_AND_MAKE_NOT_NULL(vptr->uvtable[var_ptr->uv_table_idx].value.value,
            "vptr->uvtable[var_ptr->uv_table_idx].value.value", var_ptr->uv_table_idx);
      }
      vptr->uvtable[var_ptr->uv_table_idx].value.value  = date_val;
      vptr->uvtable[var_ptr->uv_table_idx].length = *total_len;
      vptr->uvtable[var_ptr->uv_table_idx].filled_flag = 1;
    }
  }else{
    *total_len = vptr->uvtable[var_ptr->uv_table_idx].length;
  }

  
  if(vptr->uvtable[var_ptr->uv_table_idx].value.value)
  {
    NSDL1_VARS(NULL, NULL, "Returning from method %s, with value  = %s",__FUNCTION__, vptr->uvtable[var_ptr->uv_table_idx].value.value);
    return (vptr->uvtable[var_ptr->uv_table_idx].value.value);
  }
  else
  {
    NSDL1_VARS(NULL, NULL, "Returning from method %s, with value = %s",__FUNCTION__, err);
    *total_len = err_len;
    return err;
  }
}
  
int find_datevar_idx(char* name, int sess_idx) 
{
  int i;

  NSDL1_VARS(NULL, NULL, "Method called. name = %s, sess_idx = %d", name, sess_idx);

  if (gSessionTable[sess_idx].datevar_start_idx == -1)
    return -1;
  for (i = gSessionTable[sess_idx].datevar_start_idx; i <
    gSessionTable[sess_idx].datevar_start_idx +
      gSessionTable[sess_idx].num_datevar_entries; i++) {
        if (!strcmp(RETRIEVE_BUFFER_DATA(dateVarTable[i].name), name))
          return i;
  } //For loop

  return -1;
}

static int is_wrking_or_non_wrking_day(time_t date_in_sec)
{
  struct tm *tmp;
  char outstr[200]; 
  int i;
  
  NSDL1_VARS(NULL, NULL, "Method Called");
  tmp = localtime(&date_in_sec);
  strftime(outstr, 200, "%m/%d/%Y", tmp);
  
  if(tmp->tm_wday == SATURDAY || tmp->tm_wday == SUNDAY)
    return NON_WORKING_DAYS;

  for(i = 0; i < total_holiday_entries; i++)
  {
    NSDL3_VARS(NULL, NULL, "Outstr = %s, g_holi_days[i].date = %s", outstr, g_holi_days[i].date);
    if(!strcmp(outstr, g_holi_days[i].date))
      return NON_WORKING_DAYS;
  } 
  /*Given day is working day*/
  return WORKING_DAYS;
}

/* date_in_sec - Current day + offset
   next - +1 or -1 (depending upon condition )
   Set  Working or non working days as 0 and 1
   This function return the actual required day.*/
static time_t get_wrking_non_wrking_day(time_t date_in_sec, int next, int day_type)
{
  time_t date_in_sec2 = date_in_sec;

  NSDL1_VARS(NULL, NULL, "Method Called");
  while(day_type != is_wrking_or_non_wrking_day(date_in_sec2))
  {
    date_in_sec2 = date_in_sec2 + (next)*24*60*60;
  }
  return date_in_sec2;
}
 
void fill_uniq_date_var_data ()
{
  int i, j;
  //Long sec_per_nvm;
  int start_sec = 0;

  #define DAY_IN_SEC 24 * 60 *60
  
  sec_per_nvm = DAY_IN_SEC / global_settings->num_process;

  for(i = 0; i < total_datevar_entries; i++)
  {
    if(datevar_table_shr_mem[i].unique_date)
    {
      MY_MALLOC(datevar_table_shr_mem[i].date_time_data, global_settings->num_process * sizeof(Cav_uniq_date_time_data), "Uniq_date_time_data", -1);
      for(j = 0; j < global_settings->num_process; j++)
      {
        datevar_table_shr_mem[i].date_time_data[j].init_time = -1;
        datevar_table_shr_mem[i].date_time_data[j].end_sec = start_sec + sec_per_nvm;
        datevar_table_shr_mem[i].date_time_data[j].cur_sec = start_sec;
        start_sec = start_sec + sec_per_nvm + 1;
        NSDL1_VARS(NULL, NULL, "J = %d, Fun = %s, Cur sec = %d, process = %d\n", j, __FUNCTION__, datevar_table_shr_mem[i].date_time_data[j].cur_sec, global_settings->num_process);
      }
    }
  }
}

/* Function clear(s) value of date var only in case user wants to retain value of user varible(s) for next sesssions . 
   Value of Date will always be changed .Hence we won't retain value of date variable */
void clear_uvtable_for_date_var(VUser *vptr)
{

  int sess_idx = vptr->sess_ptr->sess_id;
  int i, uv_idx;
  UserVarEntry *cur_uvtable = vptr->uvtable; 

  NSDL2_VARS(vptr, NULL, "Method called, session_idx = %d, sess_name = %s", sess_idx, vptr->sess_ptr->sess_name);  

  if (sess_idx == -1)
    return;

  for (i =0 ; i<= total_datevar_entries; i++){

    NSDL3_VARS(vptr, NULL, "datevar_table_shr_mem = %d, sess_idx = %d, uv_idx = %d",
                      i, datevar_table_shr_mem[i].sess_idx, datevar_table_shr_mem[i].uv_table_idx);

    /*Check for those entry which have same sess_idx as of current one*/
    if (datevar_table_shr_mem[i].sess_idx != sess_idx){
      continue;
    } 
    /* Calculate uv_idx */
    if (datevar_table_shr_mem[i].var_name)
    {
      uv_idx = datevar_table_shr_mem[i].uv_table_idx;
      if (uv_idx < 0){
        continue;
      }
      if (cur_uvtable[uv_idx].value.value) 
        FREE_AND_MAKE_NULL(cur_uvtable[uv_idx].value.value, "cur_uvtable[uv_idx].value.value", uv_idx);
      cur_uvtable[uv_idx].length = 0;
      cur_uvtable[uv_idx].filled_flag = 0;
    }
  }
}
// End of File
