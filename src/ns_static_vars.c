/*-------------------------------------------------------------------------------------------------------------------------------
 * Name		: ns_static_vars.c

 * Purpose	: This file will handle following task -
		  [1] Parse ns_static_var() API and make/use following tables
			(i)   groupTable -> to store complete API info
			(ii)  varTable -> to store info related to particular API
			(iii) pointerTable -> to store data record big buffer pointer
				Note:- from NS 4.1.6 to support RTC, insted of pointerTable made fparamValueTable
			(iv)  weighTable -> to store data record weightage
			(v)   g_big_buf -> to store data record 
				Note:- From NS 4.1.6 to support RTC, make new bigbug to store file parameter data values 
				named as file_param_value_big_buf

		  [2] Logically the above data structures linked to each other in following manner 
			  
			groupTable <------> varTable -------> pointerTable ------> g_big_buf
				   -------> weightTable

			Here: <----> means bi-directional linking i.e. you can traverse from one DS to other DS and vise versa
			      -----> means uni-directional linking i.e you can only go form one DS to other in uni-dir
 
 *		  [3] Allocate single shared memory segment for - weightTable, groupTable, varTable
 			Note: from NS 4.1.6 to support RTC, shared memory made per NVM per File paramameter group
			      
 * Author	: - 
 
 * Modification
    Author/Date : [1] Tanmay, Manish: 20 July 2016 -> Redesign to support RTC 
 *-------------------------------------------------------------------------------------------------------------------------------*/


/**
 * All static vars related code is now moved here.
 *
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <sys/ioctl.h>
#include <assert.h>
//#include <linux/cavmodem.h>
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>

#include "url.h"
#include "util.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "init_cav.h"
#include "ns_parse_src_ip.h"
#include "nslb_sock.h"
#include "ns_trans_parse.h"
#include "ns_custom_monitor.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
//#include "ns_handle_read.h"
#include "ns_goal_based_sla.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "nslb_util.h"
#include "ns_static_use_once.h"
#include "wait_forever.h"
#include "nslb_static_var_use_once.h"
#include "divide_users.h"
#include "divide_values.h"
#include "nslb_big_buf.h"
#include "ns_static_vars.h"
#include "ns_static_vars_rtc.h"
#include "ns_sql_vars.h"
#include "ns_trace_level.h"
#include "netomni/src/core/ni_script_parse.h"
#include "ns_parent.h"
#include "ns_exit.h"
#include "ns_string.h"
#include "ns_script_parse.h"
#include "ns_cmd_vars.h"
#include "ns_runtime.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

#ifndef CAV_MAIN
long g_static_vars_shr_mem_size = 0;
PerProcVgroupTable *g_static_vars_shr_mem = NULL;
char *g_static_vars_shr_mem_ptr = NULL;
/* Make big buf for File parameter */
bigbuf_t file_param_value_big_buf = {0};
#else
__thread long g_static_vars_shr_mem_size = 0;
__thread PerProcVgroupTable *g_static_vars_shr_mem = NULL;
__thread char *g_static_vars_shr_mem_ptr = NULL;
/* Make big buf for File parameter */
__thread bigbuf_t file_param_value_big_buf = {0};
#endif

int g_static_vars_shr_mem_key = 0;
/**
 * Parsing Related functions.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

inline void
AddPointerTableEntry (int *rnum, char *buf, int len)
{
  NSDL1_VARS(NULL, NULL, "Method called.");

    if ((create_pointer_table_entry(rnum) != SUCCESS)) {
      NS_EXIT(-1, "Could not get new pointer table entry\n");
    }

    if ((pointerTable[*rnum].big_buf_pointer = copy_into_big_buf(buf, len)) == -1) {
      NS_EXIT(-1, "AddPointerTableEntry: failed to copy into big buffer");
    }

    pointerTable[*rnum].size = len;
}

int create_weight_table_entry(int *row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called.");

  if (total_weight_entries == max_weight_entries) {
    //MY_REALLOC(weightTable, (max_weight_entries + DELTA_WEIGHT_ENTRIES) * sizeof(WeightTableEntry), "weight entries", -1);
    MY_REALLOC_EX(weightTable, (max_weight_entries + DELTA_WEIGHT_ENTRIES) * sizeof(WeightTableEntry), max_weight_entries * sizeof(WeightTableEntry), "weight entries", -1);
    max_weight_entries += DELTA_WEIGHT_ENTRIES;
  }
  *row_num = total_weight_entries++;
  return (SUCCESS);
}

int create_pointer_table_entry(int *row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called, row_num = %d, total_pointer_entries = %d, max_pointer_entries = %d", 
                            *row_num, total_pointer_entries, max_pointer_entries);
  if (total_pointer_entries == max_pointer_entries) {
    //MY_REALLOC(pointerTable, (max_pointer_entries + DELTA_POINTER_ENTRIES) * sizeof(PointerTableEntry), "pointer entries", -1);
    MY_REALLOC_EX(pointerTable, (max_pointer_entries + DELTA_POINTER_ENTRIES) * sizeof(PointerTableEntry), max_pointer_entries * sizeof(PointerTableEntry), "pointer entries", -1);
    max_pointer_entries += DELTA_POINTER_ENTRIES;
  }
  *row_num = total_pointer_entries++;
  pointerTable[*row_num].size = -1;
  return (SUCCESS);
}


static int create_fparamValue_table_entry_ex(int num_values) 
{
  int mem_size = 0;

  NSDL1_VARS(NULL, NULL, "Method called, num_values = %d, total_fparam_value = %d, max_fparam_entries = %d", 
                            num_values, total_fparam_entries, max_fparam_entries);

  //Manish: why we are allocating extra data of size DELTA_POINTER_ENTRIES
  mem_size = max_fparam_entries + num_values + DELTA_POINTER_ENTRIES;

  if(max_fparam_entries < (total_fparam_entries + num_values)) 
  {
    MY_REALLOC_EX(fparamValueTable, sizeof(PointerTableEntry) * mem_size, max_fparam_entries * sizeof(PointerTableEntry), "fparam entries", -1);
    max_fparam_entries += (num_values + DELTA_POINTER_ENTRIES);
  }

  total_fparam_entries += num_values;
  return (SUCCESS);
}

static int create_var_table_entry(int *row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called, row_num = %d", *row_num);
  NSDL3_VARS(NULL, NULL, "Method called, total_var_entries = %d, max_var_entries = %d", total_var_entries, max_var_entries);
  if (total_var_entries == max_var_entries) {
    //MY_REALLOC(varTable, (max_var_entries + DELTA_VAR_ENTRIES) * sizeof(VarTableEntry), "variable entries", -1);
    MY_REALLOC_EX(varTable, (max_var_entries + DELTA_VAR_ENTRIES) * sizeof(VarTableEntry), max_var_entries * sizeof(VarTableEntry), "variable entries", -1);
    max_var_entries += DELTA_VAR_ENTRIES;
  }
  *row_num = total_var_entries++;
  varTable[*row_num].server_base = -1;
  return (SUCCESS);
}

int create_group_table_entry(int *row_num) {
  int i;
  NSDL1_VARS(NULL, NULL, "Method called, row_num = %d", *row_num);
  NSDL3_VARS(NULL, NULL, "total_group_entries = %d, max_group_entries = %d", total_group_entries, max_group_entries);
  if (total_group_entries == max_group_entries) {
    //MY_REALLOC(groupTable, (max_group_entries + DELTA_GROUP_ENTRIES) * sizeof(GroupTableEntry), "group entries", -1);
    MY_REALLOC_EX(groupTable, (max_group_entries + DELTA_GROUP_ENTRIES) * sizeof(GroupTableEntry), max_group_entries * sizeof(GroupTableEntry), "group entries", -1);
    max_group_entries += DELTA_GROUP_ENTRIES;
  }
  *row_num = total_group_entries++;
  groupTable[*row_num].max_column_index = 0;
  groupTable[*row_num].weight_idx = -1;
  groupTable[*row_num].idx = *row_num;
  groupTable[*row_num].num_vars = 0; //Added after valgrind reported bug
  groupTable[*row_num].index_var = -1;
  groupTable[*row_num].type = SESSION; //This is Refresh
  groupTable[*row_num].sequence = SEQUENTIAL; // This is mode
  groupTable[*row_num].encode_type = ENCODE_ALL; // This is mode
  groupTable[*row_num].UseOnceOptiontype = USE_ONCE_EVERY_USE; 
  groupTable[*row_num].copy_file_to_TR_flag = 1;  //defalut is Yes 
  groupTable[*row_num].start_var_idx = -1;
  groupTable[*row_num].sql_or_cmd_var = -1;
  groupTable[*row_num].is_save_into_file_flag = -1;
  groupTable[*row_num].persist_flag = -1;
  groupTable[*row_num].data_dir = -1;

  // Set array to 1 for all chars to be encoded
  // First set 1 to all characters
  memset(groupTable[*row_num].encode_chars, 49, TOTAL_CHARS);
  // Now set 0 for 0-9, a-z, A-Z and + . - _
  for(i = 'a'; i<='z';i++)
    groupTable[*row_num].encode_chars[i] = 0;
  for(i = 'A'; i<='Z';i++)
    groupTable[*row_num].encode_chars[i] = 0;
  for(i = '0'; i<='9';i++)
    groupTable[*row_num].encode_chars[i] = 0;

  groupTable[*row_num].encode_chars['+'] = 0;
  groupTable[*row_num].encode_chars['.'] = 0;
  groupTable[*row_num].encode_chars['_'] = 0;
  groupTable[*row_num].encode_chars['-'] = 0;
  return (SUCCESS);
}
static void
file_to_buffer(char **fbuf, char *file, int size)
{
  FILE *fp;
  MY_MALLOC(*fbuf, size + 1, "fbuf", -1);
  fp = fopen(file, "r");
  fread(*fbuf, size, 1, fp);
  fclose(fp);

  *(*fbuf + size) = '\0';

  // TODO - if file is binary, then it will not come properly in debug log
  NSDL1_VARS(NULL, NULL, "filename = %s, filesize = %d, File Contents = %*.*s", file, size, size, size, *fbuf);
}

int get_data_file_wth_full_path (char *in_file_name, char *out_file_name, int sess_idx, int group_idx, int runtime)
{
  int num;
  char err_msg[1024]= "\0";
  struct stat s;
 
  NSDL1_VARS(NULL, NULL, "Method called in_file_name = [%s], out_file_name = [%s], .copy_file_to_TR_flag = %d", 
                          in_file_name, out_file_name, groupTable[group_idx].copy_file_to_TR_flag);
  if (in_file_name[0] == '/') { //If absolute path name is given
    char cmd[MAX_LINE_LENGTH]; //this is to copy file_name to testrun scrits dir if abs path is given
    num = snprintf(out_file_name, MAX_LINE_LENGTH, "%s", in_file_name);

    /*If data file have full path then it will copy into TR only if copyFileToTR=Yes */
    if ((groupTable[group_idx].copy_file_to_TR_flag == 1) && (groupTable[group_idx].absolute_path_flag == 1))
    {
      if(((groupTable[group_idx].sql_or_cmd_var == 1) && (groupTable[group_idx].is_save_into_file_flag == -1)) || ((groupTable[group_idx].sql_or_cmd_var == 1) && (groupTable[group_idx].persist_flag == -1)))
        return num;
  
      if(stat(out_file_name, &s) != 0)
      {
        //NS_EXIT(-1, CAV_ERR_1000016, out_file_name);

        if(!runtime){
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012124_ID, CAV_ERR_1012124_MSG, out_file_name, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));    
        }
        SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012124_ID, CAV_ERR_1012124_MSG, out_file_name, session_table_shr_mem[sess_idx].sess_name);    
      }

      sprintf(cmd, "mkdir -p logs/TR%d/data/`dirname %s`; cp %s logs/TR%d/data/%s 2>/dev/null", testidx, out_file_name, out_file_name, testidx, out_file_name);
      if(nslb_system(cmd,1,err_msg) != 0) {
        NS_EXIT (-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
      }
    }
  }
  else 
  {
    if (runtime)
    { /*bug id: 101320: ToDo: TBD with DJA*/
      num = snprintf(out_file_name, MAX_LINE_LENGTH, "./%s/%s/%s", GET_NS_RTA_DIR(), 
                      get_sess_name_with_proj_subproj_int(session_table_shr_mem[sess_idx].sess_name, sess_idx, "/"),
                      in_file_name);
    }
    else
    {
      num = snprintf(out_file_name, MAX_LINE_LENGTH, "./scripts/%s/%s", 
                      get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"),
                      in_file_name);
    }
  }

  return num;
}


/*
 * In strtok a sequence of two or more contiguous delimiter characters in the parsed string is considered to be a single delimiter
 * but in file parameter we treat it as empty value, so this function will overcome this limit, further if string starts with delimiter
 * it will treat first field as empty and same for the last position. e.g ,val2,,,val5,
 * field[0]=
 * field[1]=val2
 * field[2]=
 */
int get_custom_tokens(char *in_buf, char *sep, int max_col, char **store_buf)
{
  char *first_pos = in_buf;
  char *second_pos = in_buf;
  int val_len = 0;
  int num_val = 0;

  NSDL3_VARS(NULL, NULL, "in_buffer=[%s]", in_buf);
  while(1)
  {
    if(*second_pos == *sep || *second_pos == '\0')
    {
      val_len = second_pos - first_pos;
      NSDL3_VARS(NULL, NULL, "first=[%s], second=[%s],val_len=[%d]", first_pos, second_pos, val_len);
      MY_MALLOC(store_buf[num_val], val_len+1, "store_buf", -1);
      memset(store_buf[num_val], 0, val_len+1);

      NSDL3_VARS(NULL, NULL, "val_len=[%d]", val_len);
      if(val_len > 0 )
       strncpy(store_buf[num_val], (first_pos), val_len);
      else
       strcpy(store_buf[num_val], ""); //TODO: Can we remove this line??
      

      store_buf[num_val][val_len] = '\0';
      NSDL3_VARS(NULL, NULL, "in_method_val=[%s]", store_buf[num_val]);
      num_val++;

      if(*second_pos == 0) break;

      second_pos  = second_pos + 1;
      first_pos= second_pos;
      continue;
                    
    }
    second_pos  = second_pos + 1;
  }

  NSDL3_VARS(NULL, NULL, "last__val=[%s]", store_buf[num_val -1]);
  return num_val;
}


int get_num_values(char *buf, char *dfname, int first_data_line)
{
  //char line_buf[MAX_LINE_LENGTH];
  int num_val = 0;
  int line_number = 0;
  int ignore_line_flag = 0;
  int first_line = 1;
  char *line = buf;
  char *tmp_ptr = NULL;
  
  NSDL2_VARS(NULL, NULL, "Method called, get number of values per variable, dfname = %s buf = %p, first_data_line = %d", 
                          dfname, buf, first_data_line); 
  
  while((tmp_ptr = strpbrk(line, "\r\n")) != NULL) 
  {
    line_number++;

    //strncpy(line_buf, line, (tmp_ptr - line));
    //line_buf[tmp_ptr - line] = '\0';

    /*Here we are dealing with dos and linux file format both.
      Since dos file end with \r\n and linux file end with \n so pointing tmp_ptr as requirment*/
    if(*tmp_ptr == '\r')
      tmp_ptr += 2;   //skip \r \n
    if(*tmp_ptr == '\n')
      tmp_ptr++;

    NSDL2_VARS(NULL, NULL, "Counting num values, line_number = %d, first_data_line = %d", 
                            line_number, first_data_line);

    if(line_number < first_data_line) {
      NSDL2_VARS(NULL, NULL, "This line is header line, Continuing");
      line = tmp_ptr;
      continue;
    }

    /*Setting ignore_line_flag for ignoring blank line from data file*/
    SET_IGNORE_LINE_FLAG(line);
    NSDL4_VARS(NULL, NULL, "ignore_line_flag = %d", ignore_line_flag); 
    if(ignore_line_flag == 1)
    {
      NSDL3_VARS(NULL, NULL, "Ignoring blank line from data file");
      ignore_line_flag = 0;
      line = tmp_ptr;
      continue;    
    }

    /* Earliar FS was given into data file now we have
     * enhanced this static var & and can be given in declaration */
    if (first_line) {
      if (!strncmp(line, "FS=", strlen("FS="))) {
      	//get_fs = 1;
        fprintf(stderr, "Error: Field Seperator/Deliminator is no longer supported in data file '%s'"
                        "Delete FS from data file & specify in static var declaration using"
                        "ColumnDeliminatior=<deliminator>",
                         dfname);
      	return -1;
      }
      first_line = 0;
    }
    num_val++;
    line = tmp_ptr;
  }

  return num_val; 
}

void get_str_mode(char *str_mode, char *req_ext, int mode)
{
  NSDL1_VARS(NULL, NULL, "Method called, mode = %d", mode);

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

   default: NSDL1_VARS(NULL, NULL, "Given mode %d is not a valid mode", mode);
  }

  NSDL1_VARS(NULL, NULL, "str_mode = %s and req_ext = %s", str_mode, req_ext);
}

#define MAX_NUM_FILE 20
#define FILE_EXT_LEN     4

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
  tmp_file = (char*) malloc(len + 1);
  strcpy(tmp_file, file);
  len = 0;

  NSDL1_VARS(NULL, NULL, "Method called, file = %s, seq = %d ", file, seq);
  
  num_files = get_tokens(tmp_file, flist, ",", MAX_NUM_FILE);
  NSDL1_VARS(NULL, NULL, "num_files = %d",  num_files);

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
      NSDL1_VARS(NULL, NULL, "Info: Length of file extension should be %d, but it is %d", FILE_EXT_LEN, len);
      //return 2;
      continue;
    }

    strncpy(fext, sptr, len);

    fext[len] = '\0';

    if(((strcmp(fext, ".seq")) && (strcmp(fext, ".unq")) && (strcmp(fext, ".use")) && (strcmp(fext, ".ran")) && (strcmp(fext, ".wtr"))))
    {
      NSDL1_VARS(NULL, NULL, "File extension is %s . It is not a valid file extension", fext);
      //return 2;
      continue;
    }

    NSDL1_VARS(NULL, NULL, "File extension is %s and mode is %d", fext, seq);

    if(((!strcmp(fext, ".seq")) && (seq == SEQUENTIAL)) ||
      ((!strcmp(fext, ".unq")) && (seq == UNIQUE))     ||
      ((!strcmp(fext, ".use")) && (seq == USEONCE))    ||
      ((!strcmp(fext, ".ran")) && (seq == RANDOM))     ||
      ((!strcmp(fext, ".wtr")) && (seq == WEIGHTED)))
       match = 1;
    
    NSDL1_VARS(NULL, NULL, "match = %d", match);
    
    if(match == 0)
     return 0;
    
    match = 0;
  }
  
  return 1;
}

/*
Name   : remove_duplicate_record_from_file
Bug    :  25298 - Need option to do not use duplicate record in case of Row Selection Mode-Unique
Return : 0 On Success
         -1 On failuer 
*/

#define CMD_BUF_SIZE (MAX_FILE_NAME * 4) + 64
int remove_duplicate_record_from_file(char *file_name){
  struct stat stat_st;
  FILE *cmd_fp;
  char cmd[CMD_BUF_SIZE + 1]; 
  
  if(stat(file_name, &stat_st) == -1){
    NSTL1_OUT(NULL, NULL, "File '%s' does not exist. Exiting.\n", file_name);
    return -1;
  }
  if(stat_st.st_size == 0){
    NSTL1_OUT(NULL, NULL, "File %s is of zero size. Exiting.\n", file_name);
    return -1;
  }
  snprintf(cmd, CMD_BUF_SIZE, "sort %s | uniq > %s.usorted; mv %s.usorted %s", file_name, file_name, file_name, file_name);
  NSDL3_VARS(NULL, NULL,"cmd = %s", cmd);
  cmd_fp = popen(cmd, "r");
  if (cmd_fp != NULL){
    NSDL2_VARS(NULL, NULL,"Successful execution of command %s\n", cmd);
    NSTL1_OUT(NULL, NULL, "All duplicate records have been removed from '%s' file.And file has been sorted\n", file_name);
    pclose(cmd_fp);
  }
  return 0;
}

/*******************************************
Description: This function will read data file of script (eg: $NS_WDIR/scripts/default/default/hpd_tours/user_info.dat)
             The contents of the file will be like..
             a11,a12,a13...
             a21,a22,a23...
             a31,a32,a34...
             .... ...  ....
             .... ...  ....

            Here comma(,) is delimeter known as coloum delimeter it can be any single character 
            (eg: @ , # * % a-z 1-9) or             any string (eg: ,@ abc 123 ...) and data 
            a11, a12.. may be value or path of a file.

            First of all we will dump whole file into memory and then find out the number of values 
            to create pointer table

            After creating pointer table we will tokenise the values from memory on the basis of 
            delimiter and store the data into big buf and the address of that location will fill 
            into pointer table. 
*******************************************/
int input_static_values(char* file_name, int group_idx, int sess_idx, int var_start, int runtime, char *data_file_buf_rtc, int sql_or_cmd_var) {
  char path_name[MAX_LINE_LENGTH];
  //char line_buf[MAX_LINE_LENGTH];
  char *line_buf = NULL;
  char *line = NULL;
  int total_weight = 0;
  int line_number = 0;
  int num_values_per_var = 0;
  int var_idx = 0;
  int rnum;
  int val_idx;
  int first_tok;
  int weight;
  int pointer_idx;
  char file_sep[16];
  char msg_buf[MAX_LINE_LENGTH];
  int num, var_size;

  struct stat stat_st;
  long data_file_size = 0;
  int read_data_fd = 0;
  int idx = 0;
  char *var_field[MAX_COLUMNS];
  int num_var_tokens = 0;
  int l = 0;
  //char error_msg[1024];//Send buffer to fill error message  
  char fext[FILE_EXT_LEN + 1] = ""; //Extension must be len = 3 , .seq, .unq, .wtr etc
  char str_mode[20] = "";
  char req_ext[20] = "";
  char *var_name = NULL;
  char err_msg_buf[1024+1];
  char *err_msg = !runtime?err_msg_buf:rtcdata->err_msg;
  //char *sess_name = !runtime?RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name):session_table_shr_mem[sess_idx].sess_name;
  char *sess_name = !runtime?
                    get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"):
                    get_sess_name_with_proj_subproj_int(session_table_shr_mem[sess_idx].sess_name, sess_idx, "/");
 
  char *column_delimiter = !runtime?RETRIEVE_BUFFER_DATA(groupTable[group_idx].column_delimiter):
                            group_table_shr_mem[group_idx].column_delimiter; 
  
  NSDL1_VARS(NULL, NULL, "Method called. file_name = %s, group_idx = %d, sess_idx = %d, var_start = %d, "
                         "file_param_value_big_buf.buffer = %p, sess_name = %s, column_delimiter = %s, sql_or_cmd_var = %d, runtime = %d", 
                         file_name, group_idx, sess_idx, var_start, file_param_value_big_buf.buffer, sess_name, column_delimiter, sql_or_cmd_var, runtime);
 
 
  var_name = !runtime?RETRIEVE_BUFFER_DATA(varTable[var_start + var_idx].name_pointer):
                   variable_table_shr_mem[var_start + var_idx].name_pointer; 

  if(!sql_or_cmd_var)
  {
    if(!is_file_ext_match_wth_mode(file_name, fext, groupTable[group_idx].sequence))
    {
      get_str_mode(str_mode, req_ext, groupTable[group_idx].sequence);
      if(!runtime){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012119_ID, CAV_ERR_1012119_MSG, file_name, fext, str_mode, sess_name, var_name, str_mode, req_ext);    
      }
      SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012119_ID, CAV_ERR_1012119_MSG, file_name, fext, str_mode, sess_name, var_name, str_mode, req_ext);
      return -1;
    }
  }
  
  /* Allocate big buffer for File parameter to store File parameter values */
  if(!file_param_value_big_buf.buffer)
  {
    memset(&file_param_value_big_buf, 0, sizeof(file_param_value_big_buf));
    nslb_bigbuf_init(&file_param_value_big_buf);  
  }

  //If this function called form tool nsi_rtc_invoker then no need to read file 
  strcpy(file_sep, column_delimiter);

  /*****************************************************************************
   Read file provided in FILE attribute of API, if 
     1. Parameter is Static Var and test is STANDALONE and Non-RTC case
     2. Paraeter is  SQL Var and test is NetCloud mode and Parent is Generator 
        (i.e loader_opcode = CLIENT_LOADER)
   ****************************************************************************/
   num = get_data_file_wth_full_path (file_name, path_name, sess_idx, group_idx, runtime);
   NSDL3_VARS(NULL, NULL, "file_sep = %s, path_name = %s", file_sep, path_name);
   sprintf(msg_buf, "input_staticvar_values() for file %s", path_name);
   
   if (num >= MAX_LINE_LENGTH) {
     path_name[MAX_LINE_LENGTH-1] = 0;
     if(!runtime){
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012093_ID, CAV_ERR_1012093_MSG, num);    
     }
     SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012093_ID, CAV_ERR_1012093_MSG, num);
     return -1;
   }

  if((!runtime && (!sql_or_cmd_var || (loader_opcode == CLIENT_LOADER))))
  {
    NSDL1_VARS(NULL, NULL, "Allocate memory for file_param_value_big_buf = %p", file_param_value_big_buf.buffer);
  

    /*Making local copy as we are using file_sep many places and column delimiter is saved in 
     big buffer.So, in calls of copy_in_big_buf Pointer may get changed. So Making local copy*/
    /*Need to check if there is control file 
     or not.If yes then create new data file otherwise dont do any thing.*/ 
    /*NetCloud: In case of controller data file will be created by scenario distribution tool*/
   /* Commenting it because, <data_file>.used and <data_file>.unsed file required at the end of test
     and it created through divide_data_files() */
   /* if (loader_opcode == STAND_ALONE)
    {
      if (groupTable[group_idx].sequence == USEONCE) {
        if(!strcasecmp(RETRIEVE_BUFFER_DATA(groupTable[group_idx].UseOnceWithinTest), "NO"))
        {
          if(nslb_uo_create_data_file_frm_last_file(path_name, (groupTable[group_idx].first_data_line - 1), error_msg) == -2)  
          {
             Fix done for bug# 7843, here if data file creation fails then parent should exit rather 
             calling kill_all_children as v_port_table is not populated yet 
            NS_EXIT (-1, "%s\n", error_msg);
          }
        } 
      }
    }*/

    /*Finding the size of data file*/ 
    if(stat(path_name, &stat_st) == -1)
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000016 + CAV_ERR_HDR_LEN, path_name);
    }
    else
    {
      if(stat_st.st_size == 0){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, path_name);
      }
    }
    data_file_size = stat_st.st_size;
    
    /*Now dump the above data of file into memory*/
    NSDL3_VARS(NULL, NULL, "Before allocating memory  malloced_sized = %ld", malloced_sized); 
    if (malloced_sized < data_file_size)
    {
      NSDL3_VARS(NULL, NULL, "malloced_sized = %ld, data_file_size = %ld", malloced_sized, data_file_size);
      MY_REALLOC_EX(data_file_buf, data_file_size + 1 + 1, malloced_sized, "data file buf", -1);
      malloced_sized = data_file_size;
    }
#if 0
    if(malloced_sized == 0) /*first time*/
    {
      NSDL3_VARS(NULL, NULL, "malloced_sized = %d, data_file_size = %d", malloced_sized, data_file_size);
      data_file_buf = (char *)malloc(data_file_size + 1 + 1); // Add +1 to Resolve bug 19082
      if (data_file_buf == NULL) {
        fprintf(stderr, "Error: Out of memory.\n");
        return -1;
      }
      malloced_sized = data_file_size;
    }
    else if (malloced_sized < data_file_size)
    {
      NSDL3_VARS(NULL, NULL, "malloced_sized = %d, data_file_size = %d", malloced_sized, data_file_size);
      data_file_buf = (char *)realloc(data_file_buf, (data_file_size + 1 + 1)); // Add +1 to Resolve bug 19082
      if (data_file_buf == NULL) {
        fprintf(stderr, "Error: Out of memory.\n");
        return -1;
      }
   
      malloced_sized = data_file_size;
    }
#endif
    NSDL3_VARS(NULL, NULL, "After allocating memory malloced_sized = %ld", malloced_sized); 

    if ((read_data_fd = open(path_name, O_RDONLY | O_CLOEXEC | O_LARGEFILE)) < 0){
      NS_EXIT(-1, CAV_ERR_1000006, path_name, errno, nslb_strerror(errno));
    }

    data_file_buf[0] = '\0';
    nslb_read_file_and_fill_buf_ex (read_data_fd, data_file_buf, data_file_size, '1');
    close(read_data_fd);

    //Save Input value file name
   /* if ((groupTable[group_idx].data_fname = copy_into_big_buf(path_name, 0)) == -1) {
      fprintf(stderr, "%s: Failed in copying input file name into big buf\n", msg_buf);
      exit(-1);
    }*/
  }
  else if(runtime)
  {
    //For RTC, reset these global vars for only first file parameter group of given RTC 
    if(!is_global_vars_reset_done)
    {
      max_fparam_entries = 0; 
      total_fparam_entries = 0;
      max_weight_entries = 0;
      total_weight_entries = 0;
    }
  }

  #ifdef NS_DEBUG_ON
    char *data_fname = !runtime?RETRIEVE_BUFFER_DATA(groupTable[group_idx].data_fname):group_table_shr_mem[group_idx].data_fname;
  #endif

  /*MM: We need this variable in VAR_VALUE parsing hence set it at the time of creating varTable*/
  //groupTable[group_idx].start_var_idx = var_start;

  int ignore_line_flag = 0;
  char *tmp_ptr = NULL;
  
  if(!runtime && (!sql_or_cmd_var || loader_opcode == CLIENT_LOADER))
    line = data_file_buf;
  else
    line = data_file_buf_rtc;

  int first_data_line = !runtime?groupTable[group_idx].first_data_line:group_table_shr_mem[group_idx].first_data_line;
  /*first need to find out the number of total values per variable to preallocate the space on the tables */
  num_values_per_var = get_num_values(line, path_name, first_data_line);

  NSDL2_VARS(NULL, NULL, "num_values_per_var = %d", num_values_per_var);
  if (num_values_per_var == 0) {
    if (!runtime){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, file_name);
    }
    SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, file_name);
    return -1;
  }

  groupTable[group_idx].num_values = num_values_per_var;

  NSDL2_VARS(NULL, NULL, "var_idx = %d, var_start = %d, groupTable[group_idx].num_vars = %d", 
                           var_idx, var_start, groupTable[group_idx].num_vars);

  int total_num_values_per_api = groupTable[group_idx].num_vars * num_values_per_var;
  int cm_pointer_entries_per_api = total_fparam_entries;

  /*Create file parameter value table */
  if (create_fparamValue_table_entry_ex(total_num_values_per_api) != SUCCESS) {
    snprintf(err_msg, 1024, "%s : error in creating pointer table entry.Script = %s api having first parameter '%s'", 
                                        msg_buf, sess_name, var_name);
    fprintf(stderr, "%s", err_msg);
    return -1;
  }

  NSDL3_VARS(NULL, NULL, "var_start = %d, num_vars = %d, num_values = %d, cm_pointer_entries_per_api = %d", 
                          var_start, groupTable[group_idx].num_vars, groupTable[group_idx].num_values, cm_pointer_entries_per_api);

  /*set value_start_ptr for every variable*/
  for (idx = 0, var_idx = var_start; idx < groupTable[group_idx].num_vars; idx++, var_idx++) {
    varTable[var_idx].value_start_ptr = cm_pointer_entries_per_api + (idx * groupTable[group_idx].num_values);
    NSDL4_VARS(NULL, NULL, "idx = %d, value_start_ptr = %d", idx, varTable[var_idx].value_start_ptr);
  }

  NSDL4_VARS(NULL, NULL, "total_fparam_entries = %d", total_fparam_entries);

  val_idx = 0;
  line_number = 0;
  tmp_ptr = NULL;
  
  if(!runtime && (!sql_or_cmd_var || loader_opcode == CLIENT_LOADER))
    line = data_file_buf;
   else
    line = data_file_buf_rtc;
 
  NSDL2_VARS(NULL, NULL, "Read data file = %s and fill table fparamValueTable.", data_fname);
  while ((tmp_ptr = strpbrk(line, "\r\n")) != NULL) {
    /*Here we are dealing with dos and linux file format both.
      Since dos file end with \r\n and linux file end with \n so terminating \r and \n as requirment*/
    if(*tmp_ptr == '\r')
    {
      *tmp_ptr = '\0';
      tmp_ptr++; //skip \r
    }

    //after skip \r we need to skip \n again
    if(*tmp_ptr == '\n')
    {
      *tmp_ptr = '\0';
      tmp_ptr++;    //skip only \n
    }

    //strcpy(line_buf, line);
    line_buf = line;
    line = tmp_ptr;

    NSDL3_VARS(NULL, NULL, "Line no. = %d, first_data_line = %d, line = %s", 
                            line_number, groupTable[group_idx].first_data_line, line_buf);

    line_number++;
    if(line_number < groupTable[group_idx].first_data_line) {
      NSDL2_VARS(NULL, NULL, "This line is header line, Continuing");
      continue;
    }

    /*Setting ignore_line_flag for ignoring blank line from data file*/
    SET_IGNORE_LINE_FLAG(line_buf);
    NSDL4_VARS(NULL, NULL, "ignore_line_flag = %d", ignore_line_flag); 
    if(ignore_line_flag == 1)
    {
      NSDL3_VARS(NULL, NULL, "Ignoring blank line from data file");
      ignore_line_flag = 0;
      continue;    
    }

 /*   int include_last_comma = 0;
    int line_len = strlen(line_buf);
   
    if(line_buf[line_len - 1] == ',')
      include_last_comma = 1; 
 */
    first_tok = 1;
    var_idx = 0;
 
    NSDL3_VARS(NULL, NULL, "line_buf = [%s]", line_buf); 
    //Tokenizing the data file lines
    num_var_tokens = get_custom_tokens(line_buf, file_sep, MAX_COLUMNS, var_field);

    if(num_var_tokens >= MAX_COLUMNS)
    { 
      if(!runtime){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012094_ID, CAV_ERR_1012094_MSG, path_name);    
      }
      SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012094_ID, CAV_ERR_1012094_MSG, path_name);
      return -1;
    }
  //Fixes for BUG 6542, as get_tokens ignore thelasr comma we need to add extra field and set its value empty
  /*  NSDL3_VARS(NULL, NULL, "line_len = [%d], include_last_comma = [%d]", line_len,include_last_comma);
    if(include_last_comma){
      num_var_tokens++;
      var_field[num_var_tokens -1] = ""; 
    }*/

    NSDL3_VARS(NULL, NULL, "num_var_tokens = %d, groupTable[group_idx].max_column_index = %d, groupTable[group_idx].ignore_invalid_line = %d", 
                            num_var_tokens, groupTable[group_idx].max_column_index, groupTable[group_idx].ignore_invalid_line);

    //Checking if the maximum column index given in variables is present or not in datafile while IgnoreInvalidData=YES is given in API
    if((num_var_tokens < groupTable[group_idx].max_column_index) && groupTable[group_idx].ignore_invalid_line)
    {
      //Decrementing the numvalues of the API in the group table as we are inoring the line in which the column index is not present.
      groupTable[group_idx].num_values = groupTable[group_idx].num_values - 1;

      fprintf(stdout, "Ignoring line number %d of the data file as you are asking "
                      "for the column index %d which exceeds the column limit %d of data file line\n", 
                       line_number, groupTable[group_idx].max_column_index, num_var_tokens);

      //Printing error if no line in data file contains the maximum index given in the variable.
      if(groupTable[group_idx].num_values == 0)
      {
        if(!runtime){
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012095_ID, CAV_ERR_1012095_MSG, path_name,groupTable[group_idx].max_column_index);
        }
        SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012095_ID, CAV_ERR_1012095_MSG, path_name, groupTable[group_idx].max_column_index);
        return -1;
      }

      NSDL3_VARS(NULL, NULL, "Ignoring line number %d of data file.", line_number);
      continue;
    }
    NSDL2_VARS(NULL, NULL, "VARIABLES: l_value = [%d], num_vars = [%d]", l, groupTable[group_idx].num_vars);
    while (l < groupTable[group_idx].num_vars) {
      var_name = !runtime?RETRIEVE_BUFFER_DATA(varTable[var_start + var_idx].name_pointer):
                   variable_table_shr_mem[var_start + var_idx].name_pointer; 
      NSDL2_VARS(NULL, NULL, "var_start = %d, var_idx = %d, group_idx = %d, value_start_ptr = %p, "
                             "var_type = %hd, var_size = %hd, name_pointer = %s", 
                              var_start, var_idx, group_idx, varTable[var_start+var_idx].value_start_ptr, 
                              varTable[var_start+var_idx].var_type, 
                              varTable[var_start+var_idx].var_size, var_name); 
      NSDL2_VARS(NULL, NULL, "Starting to fill the pointer table......");
      NSDL2_VARS(NULL, NULL, "sequence= [%d], first_tok = [%d]", groupTable[group_idx].sequence, first_tok);
      //Checking if it is first token and the sequence given is weighted
      if ((groupTable[group_idx].sequence == WEIGHTED) && first_tok) {
        first_tok = 0;
        NSDL2_VARS(NULL, NULL, "tok = %s, file_sep = %s", var_field[0], file_sep);
        if(ns_is_numeric(var_field[0]) == 0)
        {
          if(!runtime){
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012096_ID, CAV_ERR_1012096_MSG);
          }
          SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012096_ID, CAV_ERR_1012096_MSG);
          return -1;
        }
        weight = atoi(var_field[0]);
        NSDL3_VARS(NULL, NULL, "weight = %d, line_number = %d", weight, line_number);

        if (create_weight_table_entry(&rnum) != SUCCESS)
          return -1;

        if (!total_weight)
          groupTable[group_idx].weight_idx = rnum;

        total_weight += weight;
        weightTable[rnum].value_weight = (unsigned short) total_weight;
        continue;
      } else {
	if (varTable[var_idx+var_start].column_idx > num_var_tokens) {
          if(!runtime){ 
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012097_ID, CAV_ERR_1012097_MSG, path_name);
          }
          SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012097_ID, CAV_ERR_1012097_MSG, path_name);
          return -1;
	} else if (var_idx >= groupTable[group_idx].num_vars) {
          if(!runtime){
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012086_ID, CAV_ERR_1012086_MSG, var_idx+1, groupTable[group_idx].num_vars);
          }
          SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012086_ID, CAV_ERR_1012086_MSG, var_idx+1, groupTable[group_idx].num_vars);
          return -1;
	}
        NSDL2_VARS(NULL, NULL, "Not a weighted sequence, first_tok flag = %d, tok[%d] = %s, file_sep = %s, var_idx = %d", first_tok, varTable[var_idx+var_start].column_idx - 1, var_field[varTable[var_idx+var_start].column_idx - 1], file_sep, var_idx);
      }
      

      pointer_idx = varTable[var_start+var_idx].value_start_ptr + val_idx;

      NSDL2_VARS(NULL, NULL, "pointer_idx = %d, var_start = %d, var_idx = %d", pointer_idx, var_start, var_idx);
      //If var has size specification, make sure no value of this var is more than specified size
      if (varTable[var_start+var_idx].var_size > 0) {
	var_size = varTable[var_start+var_idx].var_size;
	if (var_size < strlen(var_field[varTable[var_idx+var_start].column_idx - 1])) {
          if(!runtime){
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012085_ID, CAV_ERR_1012085_MSG, var_name, var_size, var_field[varTable[var_idx+var_start].column_idx - 1]);
          }
          SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012085_ID, CAV_ERR_1012085_MSG, var_name, var_size, var_field[varTable[var_idx+var_start].column_idx - 1]);
          return -1;
	}
      } else {
	var_size = 0;
      }

      struct stat stat_buf;
      struct stat s;
      int is_file = 0;
      char var_value_file[MAX_LINE_LENGTH];
      char err_msg[1024]= "\0";
      int i;
      int file_found = 0;
      char *data_dir = NULL;
      NSDL4_VARS(NULL, NULL, "is_file = %d,  tok[%d] = %s\n", 
                              varTable[var_start+var_idx].is_file, 
                              varTable[var_idx+var_start].column_idx - 1, 
                              var_field[varTable[var_idx+var_start].column_idx - 1]);

      if (varTable[var_start + var_idx].is_file) {
        if (var_field[varTable[var_idx+var_start].column_idx - 1][0] == '/') {
          char cmd[MAX_LINE_LENGTH]; //this is to copy file_name to testrun scrits dir if abs path is given
          num = snprintf(var_value_file, MAX_LINE_LENGTH, "%s", var_field[varTable[var_idx+var_start].column_idx - 1]);
    
          if(groupTable[group_idx].copy_file_to_TR_flag == 1)
          {
            if(stat(var_value_file, &s) != 0)
            {
              // NS_EXIT(-1, CAV_ERR_1000016, var_value_file);
              if(!runtime){
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012124_ID, CAV_ERR_1012124_MSG, var_value_file, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
              }
              SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012124_ID, CAV_ERR_1012124_MSG, var_value_file, session_table_shr_mem[sess_idx].sess_name);
            }

            sprintf(cmd, "mkdir -p logs/TR%d/data/`dirname %s`; cp %s logs/TR%d/data/%s 2>/dev/null", 
                 testidx, var_value_file, var_value_file, testidx, var_value_file);
            if(nslb_system(cmd,1,err_msg) != 0) {
              NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
            }
          } 
        } 
        else {
          int grp_idx = -1;
          if(varTable[var_idx+var_start].group_idx != -1)
          {
            data_dir = RETRIEVE_BUFFER_DATA(groupTable[varTable[var_idx+var_start].group_idx].data_dir);
          }
          if(data_dir && *data_dir != '\0')
          {
            for (i = 0; i < total_runprof_entries; i++) {
              if (runProfTable[i].sessprof_idx == sess_idx) {
                grp_idx = i;
                break;
              }
            }
          }
          //DATADIR is fetched from scenario keyword G_DATADIR
          /* GET_NS_TA_DIR=$NS_WDIR/workspace/$WORKSPACE_NAME/$PROFILE_NAME/cavisson/ */
        if (grp_idx != -1 && *(runProfTable[grp_idx].gset.data_dir)) {
          sprintf(var_value_file, "%s/data/%s/%s",
                           GET_NS_TA_DIR(),
                           runProfTable[i].gset.data_dir,
                           var_field[varTable[var_idx+var_start].column_idx - 1]);
          if(stat(var_value_file, &s) == 0) {
            NSDL2_VARS(NULL, NULL, "File '%s' is present.", var_value_file);
            file_found = 1;
          } else
            NSDL2_VARS(NULL, NULL, "File '%s' is not present.", var_value_file);
        }

        if ((file_found == 0) && (data_dir && *data_dir != '\0'))
        {
          //DATADIR is fetched from file parameter API argument DATADIR
          /* GET_NS_TA_DIR=$NS_WDIR/workspace/$WORKSPACE_NAME/$PROFILE_NAME/cavisson/ */
          sprintf(var_value_file, "%s/data/%s/%s",
                             GET_NS_TA_DIR(),
                             data_dir,
                             var_field[varTable[var_idx+var_start].column_idx - 1]);
          if(stat(var_value_file, &s) == 0) {
            NSDL2_VARS(NULL, NULL, "File '%s' is present.", var_value_file);
            file_found = 1;
          } else
            NSDL2_VARS(NULL, NULL, "File '%s' is not present.", var_value_file);
        }

        if(file_found == 0) {
          /*bug id: 101320: ToDo: TBD with DJA*/
          snprintf(var_value_file, MAX_LINE_LENGTH, "./%s/%s/%s", GET_NS_RTA_DIR(), 
		   sess_name, 
                   var_field[varTable[var_idx+var_start].column_idx - 1]);    
        }

//Previously taking with only script name
/*          snprintf(var_value_file, MAX_LINE_LENGTH, "./scripts/%s/%s", 
		   get_sess_name_with_proj_subproj(sess_name), 
                   var_field[varTable[var_idx+var_start].column_idx - 1]);    */
      }
        if (lstat(var_value_file, &stat_buf) == -1) {
          NS_EXIT(-1, CAV_ERR_1000016, var_value_file);
          //is_file = 0;
        } else {
          if (stat_buf.st_size == 0) {
            if(!runtime) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, var_value_file);
            }
            SCRIPT_PARSE_NO_RETURN_EX(NULL, CAV_ERR_1012002_ID, CAV_ERR_1000017 + CAV_ERR_HDR_LEN, var_value_file);
            return -1;
          }

          is_file = IS_FILE;
        }
      }

      NSDL3_VARS(NULL, NULL, "is_file = %d", is_file);
      if (is_file) {
        char *fbuf = NULL;

        fparamValueTable[pointer_idx].size = stat_buf.st_size;
        file_to_buffer(&fbuf, var_value_file, stat_buf.st_size);

        if (varTable[var_start + var_idx].is_file != IS_FILE_PARAM)  //When file data is not parametrized then copy direct it into big buff.  
        { 
          if ((fparamValueTable[pointer_idx].big_buf_pointer = 
                             nslb_bigbuf_copy_into_bigbuf(&file_param_value_big_buf, fbuf, stat_buf.st_size)) == -1) 
          {
            NS_EXIT(-1, CAV_ERR_1000018, fbuf);
          }
          FREE_AND_MAKE_NOT_NULL_EX(fbuf, stat_buf.st_size, "fbuf", -1);
        }
        else                                                         //In case of parameter inside data file for that will make segment table.
          fparamValueTable[pointer_idx].big_buf_pointer = (ns_bigbuf_t)fbuf;

      } else {
        NSDL3_VARS(NULL, NULL, "value = %s, var_size = %d", var_field[varTable[var_idx + var_start].column_idx - 1], var_size);
        NSDL3_VARS(NULL, NULL, "file_param_value_big_buf = %p", file_param_value_big_buf.buffer);

        /*Check and decrypt the value*/
        NSDL3_VARS(NULL, NULL, "varTable[var_idx + var_start].encrypted = %d", varTable[var_idx + var_start].encrypted);
        if(varTable[var_idx + var_start].encrypted)
        {
          strcpy(var_field[varTable[var_idx + var_start].column_idx - 1], ns_decrypt(var_field[varTable[var_idx + var_start].column_idx - 1]));
          NSDL3_VARS(NULL, NULL, "Decrypted value = %s", var_field[varTable[var_idx + var_start].column_idx - 1]);
        }

        if ((fparamValueTable[pointer_idx].big_buf_pointer = 
                           nslb_bigbuf_copy_into_bigbuf(&file_param_value_big_buf, 
                                                        var_field[varTable[var_idx + var_start].column_idx - 1], 
                                                        var_size)) == -1) {
          NSDL3_VARS(NULL, NULL, "%s: Failed in copying data into big buf", msg_buf);
          NS_EXIT(-1, CAV_ERR_1000018, var_field[varTable[var_idx + var_start].column_idx - 1]);
        }
        //fparamValueTable[pointer_idx].size = -1;
        fparamValueTable[pointer_idx].size = strlen(nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pointer_idx].big_buf_pointer));
        NSDL3_VARS(NULL, NULL, "data_size = %d, data = [%s]", fparamValueTable[pointer_idx].size, 
                                    nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pointer_idx].big_buf_pointer));
      }
      var_idx++;
      l++;
    }
    l = 0;
    val_idx++;

    int i;
    for(i=0 ; i <= num_var_tokens - 1; i++)
     free(var_field[i]);
  }

  NSDL3_VARS(NULL, NULL, "val_idx = %d, var_idx = %d, total_weight = %d", val_idx, var_idx, total_weight);
  #if NS_DEBUG_ON
  /*Data Dump */
  int pTbl_idx = 0;
  var_idx = 0;
  int pointer;
  int i,j;
  
  for(i = 0,var_idx = var_start; i < groupTable[group_idx].num_vars; i++,var_idx++)
  {
    var_name = !runtime?RETRIEVE_BUFFER_DATA(varTable[var_idx].name_pointer):variable_table_shr_mem[var_idx].name_pointer; 
    NSDL4_VARS(NULL, NULL, "VarTable Data Dump: var_idx = %d, value_start_ptr = %d, is_file = %d, name = %s", 
                            var_idx, varTable[var_idx].value_start_ptr, 
                            varTable[var_idx].is_file, var_name);

    pointer = cm_pointer_entries_per_api + (i * groupTable[group_idx].num_values);
    NSDL4_VARS(NULL, NULL, "pointer = %d", pointer); 
    for(j =0, pTbl_idx = pointer; j < groupTable[group_idx].num_values; j++, pTbl_idx++)
    {
       if (varTable[var_idx].is_file == IS_FILE){
         NSDL4_VARS(NULL, NULL, "fparamValueTable Table Data Dump: size = %d, fparamValueTable[%d].big_buf_pointer = %s", 
                      fparamValueTable[pTbl_idx].size, pTbl_idx, 
                      nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pTbl_idx].big_buf_pointer));
       }
    }   
  }
  #endif
  return 0;
}

/*#define CHK_GET_ARGUMENT_VALUE(ArgName) \
  if (state == NS_ST_STAT_VAR_NAME) { \
    fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg); \
    return -1; \
  } \
  if (group_idx == -1) { \
    fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg); \
    return -1; \
  } \
 \
  line_tok += strlen(ArgName); \
  CLEAR_WHITE_SPACE(line_tok); \
  CHECK_CHAR(line_tok, '=', script_file_msg); \
  CLEAR_WHITE_SPACE(line_tok); \
  NSDL2_VARS(NULL, NULL, "%s = %s", ArgName, line_tok);
*/

/*This function is to check if we got
 * full buffer or we need to need to append next token in buff*/
int chk_buf(char *line)
{
  int len = strlen(line);

  //fprintf(stderr, "Buffer Len = %d, buffer = %s\n", len, line);
  
  if(line[len - 1] == '"' && line[len - 2] != '\\')
    //Got last "
    return 0;
  else
    //Did not get last "
    return 1;
}

static int set_buffer_for_remove_dup_records(char* value, char* buffer, int group_idx, char* script_file_msg, int* state, int* remove_dup_records_done, char* remove_dup_records_val)
{ 
  if(*state == NS_ST_STAT_VAR_NAME) {
    NSTL1(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if (*remove_dup_records_done){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "RemoveDupRecords", "File");
  }
  strcpy(buffer, value);
  NSDL3_VARS(NULL, NULL, "After tokenized remove_dup_records = %s", buffer);

  if((!strcasecmp(buffer, "YES")) || (!strcasecmp(buffer, "NO"))){
    strcpy(remove_dup_records_val, buffer);
  }
  else{
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "RemoveDupRecords", "File");
  }
 
  *remove_dup_records_done = 1;
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}
#if 0
static int set_buffer_for_left_boundary(char* value, int group_idx, char* script_file_msg, int* state, int* left_boundary_done, char* left_boundary_val)
{
  if(*state == NS_ST_STAT_VAR_NAME) {
  fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
  return -1;
  }

  if (group_idx == -1) {
    fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg);
    return -1;
  }

  if (*left_boundary_done){
    fprintf(stderr, "%s  'left_boundary_done' can be given only once.\n", script_file_msg);
    return -1;
  }
  if (!value || (*value != '\0')) {
    strncpy(left_boundary_val, value, 16);
    left_boundary_val[strlen(left_boundary_val)] = '\0';
    NSDL3_VARS(NULL, NULL, "After tokenized left_boundary_done = %s", left_boundary_val);
  }else {
    fprintf(stderr, "Left Boundary is NULL\n");
    return -1;
  }

  *left_boundary_done = 1;
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}

static int set_buffer_for_right_boundary(char* value, int group_idx, char* script_file_msg, int* state, int* right_boundary_done, char* right_boundary_val)
{
  if(*state == NS_ST_STAT_VAR_NAME) {
  fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
  return -1;
  }

  if (group_idx == -1) {
    fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg);
    return -1;
  }

  if (*right_boundary_done){
    fprintf(stderr, "%s  'right_boundary_done' can be given only once.\n", script_file_msg);
    return -1;
  }

  strncpy(right_boundary_val, value, 16);
  right_boundary_val[strlen(right_boundary_val)] = '\0';
  NSDL3_VARS(NULL, NULL, "After tokenized 'right_boundary_done' = %s", right_boundary_val);

  *right_boundary_done = 1;
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}
#endif
static int set_buffer_for_use_once_within_test(char* value, char* buffer, int group_idx, char* script_file_msg, int* state,                     int* use_once_within_test_done,char* use_once_within_test_val)
{
  if(*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if (*use_once_within_test_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "UseOnceWithinTest", "File");
  }

  strcpy(buffer, value);
  NSDL3_VARS(NULL, NULL, "After tokenized use_once_within_test = %s", buffer);

  if((!strcasecmp(buffer, "YES")) || (!strcasecmp(buffer, "NO"))){
    strcpy(use_once_within_test_val, buffer);
  }
  else{
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "UseOnceWithinTest", "File");
  }

  *use_once_within_test_done = 1;
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}

/* It will validate data directory & copy in to the destination */
int validate_and_copy_datadir(int mode, char *value, char *dest, char *err_msg)
{
  char datadir_withpath[MAX_DATA_LINE_LENGTH];
  struct stat s;
 
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  if (strlen(value) > MAX_UNIX_FILE_NAME) {
    strncpy(err_msg, "Data directory name length cannot be more than 255 in UNIX.", 4096);
    if (mode) {
      *value = '\0';
      return 0;
    } 
    else
      return -1;
  }
  sprintf(datadir_withpath, "%s/data/%s",
                           GET_NS_TA_DIR(),
                           value);

  if(stat(datadir_withpath, &s) < 0) {
    snprintf(err_msg, 4096, "Data Directory '%s' is not present.", value);
    NSDL2_VARS(NULL, NULL, "err_msg = [%s]", err_msg);
    if (mode) {
      *value = '\0';
      return 0;
    }
    else
      return -1;
  }
  if((*value == '\0') || !(strncmp(value,"NA", strlen("NA")))) {
    snprintf(err_msg, 4096, "Data Directory '%s' cannot be empty or NA.", value);
    NSDL2_VARS(NULL, NULL, "err_msg = [%s]", err_msg);
    if (mode) {
      *value = '\0';
      return 0;
    }
    else
      return -1;
  }
  if (mode != 2)
    strcpy(dest, value);

  NSDL2_VARS(NULL, NULL, "value = [%s], dest = %s", value, dest);

  return 0;
}

/* Format eg: FILE=/home/cavisson/work3/File/file_name or FILE=file_name */
static int set_staticvar_file(char *value, char *staticvar_datadir, char *staticvar_file, int sess_idx, int *state, int group_idx, int *file_done, 
                                                                                char *script_file_msg, int *absolute_path_flag, int api_type)
{
  int i;
  int grp_idx = -1;
  struct stat s;
  int file_found = 0;
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], sess_idx = %d, group_idx = %d, file_done = %d, script_file_msg = %s", value, sess_idx, group_idx, *file_done, script_file_msg);

  if (*file_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "FILE", "File");
  }

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if(*value == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "FILE", "File");
  }
  
  if(*value == '/') {  // absolute path given
     strcpy(staticvar_file, value);
     *absolute_path_flag = 1;
  }
  else
  {
    if(*staticvar_datadir != '\0')
    {
      for (i = 0; i < total_runprof_entries; i++) {
        if (runProfTable[i].sessprof_idx == sess_idx) {
          grp_idx = i;
          break;  
        }
      }
    }

    NSDL2_VARS(NULL, NULL, "grp_idx=%d, staticvar_file=%s, value=%s, runProfTable[grp_idx].gset.data_dir=%s, staticvar_datadir=%s ", grp_idx, staticvar_file, value, runProfTable[grp_idx].gset.data_dir, staticvar_datadir );

    //DATADIR is fetched from scenario keyword G_DATADIR
    /* GET_NS_TA_DIR=$NS_WDIR/workspace/$WORKSPACE_NAME/$PROFILE_NAME/cavisson/ */
    if ((grp_idx != -1) && *(runProfTable[grp_idx].gset.data_dir)) {
    sprintf(staticvar_file, "%s/data/%s/%s",
                           GET_NS_TA_DIR(), 
                           runProfTable[grp_idx].gset.data_dir,
                           value);
    if(stat(staticvar_file, &s) == 0) {
      NSDL2_VARS(NULL, NULL, "File '%s' is present.", staticvar_file);
      file_found = 1;
    } else
        NSDL2_VARS(NULL, NULL, "File '%s' is not present.", staticvar_file); 
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
        NSDL2_VARS(NULL, NULL, "File '%s' is present.", staticvar_file);
        file_found = 1;  
      } else
        NSDL2_VARS(NULL, NULL, "File '%s' is not present.", staticvar_file);       
    }

    if(file_found == 0)
      sprintf(staticvar_file, "%s/%s/%s",
                         GET_NS_TA_DIR(),
                         get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"),
                         value);

  }
  
  *file_done = 1;
  *state = NS_ST_STAT_VAR_OPTIONS;

  NSDL2_VARS(NULL, NULL, "File Name = %s", staticvar_file);
  NSTL1_OUT(NULL, NULL, "Data File Name = %s\n", staticvar_file);
  write_log_file(NS_SCENARIO_PARSING, "Data File Name with data directory = %s",staticvar_file);
  NSDL2_VARS(NULL, NULL, "End Of the function set_static_var_file");
  return 0;
}

static int set_buffer(char *value, char *buffer, int *state, int group_idx, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(buffer, value);
  NSDL3_VARS(NULL, NULL, "After tokenized Refresh Name = %s", buffer);

  if (!strncasecmp(buffer, "SESSION", strlen("SESSION"))) {
     groupTable[group_idx].type = SESSION;
  } else if (!strncasecmp(buffer, "USE", strlen("USE"))) {
     groupTable[group_idx].type = USE;
  } else if (!strncasecmp(buffer, "ONCE", strlen("ONCE"))) {
     groupTable[group_idx].type = ONCE;
  } else {
     SCRIPT_PARSE_ERROR_EXIT_EX(buffer, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "REFRESH", "File");
  }
  
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}

static int set_buffer_for_saveUseOnceOption(char *value, char *buffer, int *state, int group_idx, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(buffer, value);
  NSDL3_VARS(NULL, NULL, "After tokenized SaveUseOnceOption Name = %s", buffer);

  if (!strcasecmp(buffer, "EVERY_USE")) {
    groupTable[group_idx].UseOnceOptiontype = USE_ONCE_EVERY_USE;
  } else {
    groupTable[group_idx].UseOnceOptiontype = atoi(buffer);
  }
  
  *state = NS_ST_STAT_VAR_OPTIONS;

  return 0;
}

static int set_buffer_for_useonce_err(char *value, char *buffer, int *state, int group_idx, char *script_file_msg, int *useonce_err){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(buffer, value);
  NSDL3_VARS(NULL, NULL, "After tokenized on_useonce_error = %s", buffer);
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
  
  *state = NS_ST_STAT_VAR_OPTIONS;
  return 0;
}

static int set_buffer_for_encodeMode(char *value, char *buffer, int *state, int group_idx, char *script_file_msg, int *encode_flag_specified){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(buffer, value);
  NSDL3_VARS(NULL, NULL, "After tokenized EncodeMode Name = %s", buffer);
  if (!strcasecmp(buffer, "All")) {
    //int i;
    //scope = ENCODE_ALL;
    groupTable[group_idx].encode_type = ENCODE_ALL;
  #if 0
    Set array to 1 for all chars to be encoded
    First set 1 to all characters
    memset(groupTable[group_idx].encode_chars, 49, TOTAL_CHARS);

    Now set 0 for 0-9, a-z, A-Z and + . - _
    for(i = 'a'; i<='z';i++)
      groupTable[group_idx].encode_chars[i] = 0;

    for(i = 'A'; i<='Z';i++)
      groupTable[group_idx].encode_chars[i] = 0;

    for(i = '0'; i<='9';i++)
      groupTable[group_idx].encode_chars[i] = 0;

      groupTable[group_idx].encode_chars['+'] = 0;
      groupTable[group_idx].encode_chars['.'] = 0;
      groupTable[group_idx].encode_chars['_'] = 0;
      groupTable[group_idx].encode_chars['-'] = 0;
   #endif
  } else if (!strcasecmp(buffer, "None")) {
      //scope = ENCODE_NONE;
      groupTable[group_idx].encode_type = ENCODE_NONE;
      memset(groupTable[group_idx].encode_chars, 0, TOTAL_CHARS);
  } else if (!strcasecmp(buffer, "Specified")) {
      //scope = ENCODE_SPECIFIED;
      *encode_flag_specified = 1;
      groupTable[group_idx].encode_type = ENCODE_SPECIFIED;
      memset(groupTable[group_idx].encode_chars, 0, TOTAL_CHARS);
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(buffer, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "EncodeMode", "File");
  }
  
  *state = NS_ST_STAT_VAR_OPTIONS;
      //scope_done = 1;
  return 0;
}

static int set_char_to_encode_buf(char *value, char *char_to_encode_buf, int *state, int group_idx, char *script_file_msg,int encode_flag_specified, char *encode_chars_done){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  
  int i;
  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }
 
  if (encode_flag_specified == 0){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012137_ID, CAV_ERR_1012137_MSG, "File");
  }

  strcpy(char_to_encode_buf, value);
  NSDL3_VARS(NULL, NULL, "After tokenized CharatoEncode = %s", char_to_encode_buf);
  
  //Encode chars can have any special characters including space, single quiote, double quotes. Few examples:
  //EncodeChars=", \[]"
  //ncodeChars="~`!@#$%^&*-_+=[]{}\|;:'\" (),<>./?"
  //TODO
      
  for (i = 0; char_to_encode_buf[i] != '\0'; i++) {
    if(isalnum(char_to_encode_buf[i])){
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012138_ID, CAV_ERR_1012138_MSG, char_to_encode_buf, "File");
    }

    NSDL3_VARS(NULL, NULL, "i = %d, char_to_encode_buf[i] = [%c]", i, char_to_encode_buf[i]);
   
    /*Manish: since in new parsing api design we already skip \ in case of \\ and \"*/
    #if 0
    if (char_to_encode_buf[i] == '\\') {
      i++;
      if (char_to_encode_buf[i]) {
        switch (char_to_encode_buf[i]) {
          case '\\':
            groupTable[group_idx].encode_chars['\\'] = 1;
            break;
          case '"':
            groupTable[group_idx].encode_chars['"'] = 1;
            break;
            default:
            fprintf(stderr, "%s Bad charstoEncode declaraction format. unrecognised '%c' \n", script_file_msg, char_to_encode_buf[i]);
            return -1;
        }
      }
    } else if (char_to_encode_buf[i] == '\"') {
        i++;
        break;
    } else {
        groupTable[group_idx].encode_chars[(int)char_to_encode_buf[i]] = 1;
    }
    #endif

    groupTable[group_idx].encode_chars[(int)char_to_encode_buf[i]] = 1;
  }

  *state = NS_ST_STAT_VAR_OPTIONS;
  *encode_chars_done = 1;

  return 0;
}
static int set_buffer_for_mode(char *value, char *buffer, int *state, int group_idx, int var_start, char *script_file_msg, int *mode_done, int *mode, int *column_flag){
  int i = 0;
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if (*mode_done) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "MODE", "File");
  }


  strcpy(buffer, value);
 
  NSDL3_VARS(NULL, NULL, "After tokenized Mode Name = %s", buffer);

  if (!strcmp(buffer, "WEIGHTED_RANDOM")) {
    *mode = WEIGHTED;
    groupTable[group_idx].sequence = WEIGHTED;
    i = var_start;
    NSDL3_VARS(NULL, NULL, "var_start = %d", var_start);
    while(i < (groupTable[group_idx].num_vars + var_start))
    {
      NSDL3_VARS(NULL, NULL, "i = %d, groupTable[%d].num_vars = %d", i, group_idx, groupTable[group_idx].num_vars);
      if(*column_flag == 2)
      {
        varTable[i].column_idx = varTable[i].column_idx + 1;/*Since colon not given in variables so increasing the column index by one 
                                                              sequentially as first column is fixed for weight in WEIGHTED RANDOM MODE.*/
        NSDL3_VARS(NULL, NULL, "column_flag = %d, varTable[i].column_idx = %d", column_flag, varTable[i].column_idx);
      }
      else if(*column_flag == 1)
      {
        NSDL3_VARS(NULL, NULL, "column_flag = %d ", *column_flag);
        if(varTable[i].column_idx == 1)
        {
          NSDL3_VARS(NULL, NULL, "Column flag is set and mode is given weighted so can't have column index as 1 as it is fixed  for weighted");
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012091_ID, CAV_ERR_1012091_MSG);
        }
      }
      i++;
    }
  } else if (!strcasecmp(buffer, "RANDOM")) {
      *mode = RANDOM;
      groupTable[group_idx].sequence = RANDOM;
  } else if (!strcasecmp(buffer, "SEQUENTIAL")) {
      *mode = SEQUENTIAL;
      groupTable[group_idx].sequence = SEQUENTIAL;
  } else if (!strcasecmp(buffer, "UNIQUE")) {
      *mode = UNIQUE;
      groupTable[group_idx].sequence = UNIQUE;
  } else if (!strcasecmp(buffer, "USE_ONCE")) {
      *mode = USEONCE;
      groupTable[group_idx].sequence = USEONCE;
  } else if (!strcasecmp(buffer, "UNIQUEWITHINGEN")) {//RBU: In release 3.9.7 a new mode has been added for Generator, this will be treated as UNIQUE across the NVMs
     *mode = UNIQUE;
     groupTable[group_idx].sequence = UNIQUE;
  } else { 
      SCRIPT_PARSE_ERROR_EXIT_EX(buffer, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "MODE", "File");
  }
  
  *mode_done = 1;
  *state = NS_ST_STAT_VAR_OPTIONS;


  NSDL2_VARS(NULL, NULL, "End of the method : groupTable[group_idx].sequence = [%d]", groupTable[group_idx].sequence);
  return 0;
}

/* Format example: VAR_VALUE=F1=file;F2=value */
static int set_static_var_var_value(char* value, char* line_tok, int group_idx, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], group_idx = [%d]", value, group_idx); //F1=file;F2=value
  NSDL2_VARS(NULL, NULL, "line_tok = [%s]", line_tok);  //VAR_VALUE
  char *ptr, *vv, *start;
  int field;
  char *type;
  ptr = value;
  int var_id;
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
    NSDL2_VARS(NULL, NULL, "vv = %s, field = %d", vv, field);
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
        if ((field <= groupTable[group_idx].num_vars) && (field >= 0)) 
        {
          //MM: Resolve Bug:7597  no need to get total number of variables because we have stat_var_idx and num_var for each group idx
          /*
          int nv = 0;
          for (i = 0; i < group_idx; i++) {
            nv = nv + groupTable[i].num_vars;
          }

          NSDL2_VARS(NULL, NULL, "Setting %d to is_file\n", nv + field);
          varTable[nv + field - 1].is_file = 1;
          */

          var_id = groupTable[group_idx].start_var_idx + field - 1;
          NSDL2_VARS(NULL, NULL, "Setting flag is_file: group_idx = %d, start_var_idx = %d, field = %d, var_idx = %d, var = %s", 
                                  group_idx, groupTable[group_idx].start_var_idx, field, var_id, 
                                  RETRIEVE_BUFFER_DATA(varTable[var_id].name_pointer));
          if (strcasecmp(type, "file") == 0)
            varTable[var_id].is_file = IS_FILE;
          else
            varTable[var_id].is_file = IS_FILE_PARAM;
        } 
        else 
        {
          NS_EXIT(-1, "Invalid VAR_VALUE %s", start);
        }
      } 
      else if (strcasecmp(type, "value") != 0) 
      {
        NS_EXIT(-1, "Invalid type %s specified.", type);
      }
    }
    else 
    {
      NS_EXIT(-1, "Unrecognized VAR_VALUE (%s)", start);
    }
  }
  return 0;
}

static int set_static_var_column_delim(char* value, char* column_delimiter, int *state, int group_idx, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  if(*value == '\0'){
    column_delimiter[0] = ',';
    column_delimiter[1] = '\0';
  }
  else {
    strcpy(column_delimiter, value);
  }
  NSDL2_VARS(NULL, NULL, "After tokenized column delimiter = %s", column_delimiter);
  return 0;
}

static int set_static_firstDataLine(char* value, char* firstdataline, int *state, int group_idx, char *script_file_msg){
  int i =0;
 
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(firstdataline, value);
  NSDL2_VARS(NULL, NULL, "After tokenized FirstDataLine = %s", firstdataline);
 
  //Manish:  Fix bug 3460 
  //FirstDataLine should be a decimal number
  for (i = 0; firstdataline[i]; i++) 
  {
    if (!isdigit(firstdataline[i]))  
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(value, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "FirstDataLine");
    }
  }
  
  return 0;
}

static int set_static_headerLine(char* value, char *headerLine, int *state, int group_idx, char *script_file_msg){
  int i = 0;
 
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  //Nothing to do just skip
  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(headerLine, value);
  NSDL2_VARS(NULL, NULL, "After Toknising headerLine = [%s]", headerLine);

  //Manish:  Fix bug 3459 
  //FirstDataLine should be a decimal number
  for (i = 0; headerLine[i]; i++) 
  {
    if (!isdigit(headerLine[i]))  
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(value, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "HeaderLine");
    }
  }

  NSDL2_VARS(NULL, NULL, "Method End.");
  return 0;
}

static int set_static_encodeSpaceBy(char* value, char* EncodeSpaceBy, char *line_tok, int *state, int group_idx, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s], line_tok = [%s]", value,line_tok);

  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "File");
  }

  if (group_idx == -1) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012087_ID, CAV_ERR_1012087_MSG);
  }

  strcpy(EncodeSpaceBy, value);
  NSDL2_VARS(NULL, NULL, "After tokenized EncodeSpaceBy = [%s]", EncodeSpaceBy);

  //Manish: Fix Bug 2775
  //support only two +, %20
  if(!strcmp(EncodeSpaceBy, "+") || !strcmp(EncodeSpaceBy, "%20"))
    return 0;
  else
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012139_ID, CAV_ERR_1012139_MSG, "File"); 

  return 0;
}

/*This function will check whether passed variable value is encrypted or not*/
static int is_encrypted(char *input)
{
 char *c;
 NSDL2_VARS(NULL, NULL, "Method Called, input=%s", input);
 c = strstr(input,":E");
 if(c)
 {
   if((c+2) && *(c+2) == ':')
     sprintf(c,"%s",(c+2));
   else
   *c = '\0';

   NSDL2_VARS(NULL, NULL, "%s is encrypted", input);
   return 1;
 }
 return 0;
}

int set_last_option(char *value, char *line_tok, char *staticvar_buf, int *group_idx, int *var_start, int sess_idx, char *script_file_msg, int *state, int *rw_data, int *create_group, int *var_idx, int *column_flag, unsigned short reltv_var_idx){

  char absol_var[MAX_FIELD_LENGTH];
  DBT key, data;
  char *size_ptr;
  int ret_val;
  char hash_data[8];
  char *column_ptr;

  NSDL2_VARS(NULL, NULL, "Method Called, line_tok = [%s], value = [%s], group_idx = %d, var_start = %d, "
                         "sess_idx = %d, rw_data = %d, create_group = %d, var_idx = %d, reltv_var_idx = %hd", 
                         line_tok, value, *group_idx, *var_start, sess_idx, *rw_data, *create_group, *var_idx, reltv_var_idx);

  if (*state == NS_ST_STAT_VAR_OPTIONS) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012130_ID, CAV_ERR_1012130_MSG, line_tok, "File");
  }
  
  strcpy(staticvar_buf, line_tok);
  NSDL2_VARS(NULL, NULL, "After tokenized static var staticvar_buf = %s", staticvar_buf);

  /* For validating the variable we are calling the validate_var funcction */
/*
  if(validate_var(staticvar_buf)) {
    printf("%s: Invalid var name '%s'\n", script_file_msg, staticvar_buf);
    exit(-1);
  }
*/

  if (*create_group) {
    NSDL2_VARS(NULL, NULL, "Creating group table entry for group_idx = %d", *group_idx);
    if (create_group_table_entry(group_idx) != SUCCESS)
      return -1;
      *create_group = 0;
  }

    /*Create an antry in varTable for each static variable
 *       and set the var_start_dx of sess table to the first variable of the session*/
    if (create_var_table_entry(var_idx) != SUCCESS)
      return -1;
    // set var_star to the varTable start_index for this index group decl
    if (*var_start == -1)
    {
      *var_start = *var_idx;
       groupTable[*group_idx].start_var_idx = *var_idx;
       NSDL2_VARS(NULL, NULL, "Set start_var_idx = %d for group_idx = %d", groupTable[*group_idx].start_var_idx, *group_idx);
    }

    if (gSessionTable[sess_idx].var_start_idx == -1) {
        gSessionTable[sess_idx].var_start_idx = *var_idx;
        gSessionTable[sess_idx].num_var_entries = 0;
    }


    gSessionTable[sess_idx].num_var_entries++;
   
    /* Manish: Refer doc - FileParameterRTCDesign */
    varTable[*var_idx].self_idx_wrt_fparam_grp = reltv_var_idx;

    //Set encrypted flag based on variable
    varTable[*var_idx].encrypted = is_encrypted(staticvar_buf);

    /* Checking the presence of ':' in the variable name if found then set the column number else set the column number sequentially.*/
    if((column_ptr = index(staticvar_buf, ':')))
    {
      NSDL2_VARS(NULL, NULL, "Colon found in variable name given so getting column number.....");
      if(*column_flag == 2)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012098_ID, CAV_ERR_1012098_MSG);
      }

      *column_ptr = '\0';
      column_ptr++;
      if(ns_is_numeric(column_ptr) == 0)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "File parameter variable");
      }
      if(atoi(column_ptr) == 0)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012100_ID, CAV_ERR_1012100_MSG, staticvar_buf, column_ptr);
      }
      varTable[*var_idx].column_idx = atoi(column_ptr);

      *column_flag = 1;//Means we have parsed a variable with colon
      NSDL2_VARS(NULL, NULL, "varTable[%d].column_idx = %d", *var_idx, varTable[*var_idx].column_idx);
    }
    else
    {
      NSDL2_VARS(NULL, NULL, "Colon not found so setting the variable name column index sequentially.");
      if(*column_flag == 1)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012098_ID, CAV_ERR_1012098_MSG);
      }

      varTable[*var_idx].column_idx = *var_idx - *var_start + 1;
      *column_flag = 2;//Means we have parsed a variable without colon
      NSDL2_VARS(NULL, NULL, "varTable[%d].column_idx = %d", *var_idx, varTable[*var_idx].column_idx);
    }

    //Checking for the highest column index of the api variables given and storing it so as we want to akip the line based on that column index
    if(varTable[*var_idx].column_idx > groupTable[*group_idx].max_column_index)
    {
      groupTable[*group_idx].max_column_index = varTable[*var_idx].column_idx;
      NSDL2_VARS(NULL, NULL, "groupTable[*group_idx].max_column_index = %d, group_idx = %d, var_idx = %d", groupTable[*group_idx].max_column_index, *group_idx, *var_idx);
    }
    /* Check if var name contains a size specification.
     * If size is provided, then variable can be read-write (means you can save new value)
     * size is the max size to be written back*/

    if ((size_ptr = index(staticvar_buf, '/'))) {
      *size_ptr = '\0'; //Remove size from var name
      size_ptr++;
      varTable[*var_idx].var_size = atoi(size_ptr);
      if ((varTable[*var_idx].var_size <= 0) || (varTable[*var_idx].var_size > 4096)) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012088_ID, CAV_ERR_1012088_MSG, staticvar_buf, varTable[*var_idx].var_size);
      }
      *rw_data = 1;
      printf ("%s is rw var with size =%d\n", staticvar_buf, varTable[*var_idx].var_size);
    } else {
        varTable[*var_idx].var_size = -1;
      }

    if(validate_var(staticvar_buf)) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012009_ID, CAV_ERR_1012009_MSG, staticvar_buf);
    }

    if ((varTable[*var_idx].name_pointer = copy_into_big_buf(staticvar_buf, 0)) == -1) {
      NS_EXIT(-1, CAV_ERR_1000018, staticvar_buf);
    }

    NSDL2_VARS(NULL, NULL, "staticvar_buf = %s, staticvarTable idx = %d, group_idx = %d", staticvar_buf, *var_idx, *group_idx);

    varTable[*var_idx].var_type = VAR;
    varTable[*var_idx].group_idx = *group_idx;
    varTable[*var_idx].is_file = 0;
    groupTable[*group_idx].num_vars++;
    memset(&key, 0, sizeof(DBT));
    //sprintf(absol_var, "%s!%s", staticvar_buf, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
    sprintf(absol_var, "%s!%s", staticvar_buf, 
                                get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "-"));
    key.data = absol_var;
    key.size = strlen(absol_var);

    sprintf(hash_data, "%d", *var_idx);
    memset(&data, 0, sizeof(DBT));
    data.data = hash_data;
    data.size = strlen(hash_data) + 1;

    NSDL2_VARS(NULL, NULL, "group_idx = %d, var_idx = %d, sess_idx = %d, num_var_entries = %d, absol_var = %s, hash_data = %s", *group_idx, *var_idx, sess_idx, gSessionTable[sess_idx].num_var_entries, absol_var, hash_data);

    ret_val = var_hash_table->put(var_hash_table, NULL, &key, &data, DB_NOOVERWRITE);

    if (ret_val == DB_KEYEXIST) {  /* The variable name is already in the HASH Table.  This is not allowed */
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012101_ID, CAV_ERR_1012101_MSG, staticvar_buf, ret_val);
    } else {
        if (ret_val != 0) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012102_ID, CAV_ERR_1012102_MSG, staticvar_buf);
        }
      }
   if (*state == NS_ST_STAT_VAR_NAME)
        *state = NS_ST_STAT_VAR_NAME_OR_OPTIONS;
  NSDL2_VARS(NULL, NULL, "End of the method set_last_option");
  return 0;
}

static int set_copyFileToTR(char *value, char *buffer, int group_idx, char *script_file_msg, int *copyFileToTR_flag){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  strcpy(buffer, value);
   
  NSDL3_VARS(NULL, NULL, "buffer = %s", buffer);
  
  if (!strcasecmp(buffer, "YES")) {
    groupTable[group_idx].copy_file_to_TR_flag = 1;
    *copyFileToTR_flag = 1; 
  } else if (!strcasecmp(buffer, "NO")) {
      groupTable[group_idx].copy_file_to_TR_flag = -1;
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "CopyFileToTR", "File");
  }
 
  NSDL2_VARS(NULL, NULL, "End of the method : groupTable[group_idx].copy_file_to_TR_flag = [%d]", groupTable[group_idx].copy_file_to_TR_flag);
  return 0;
}

static int set_ignoreInvalidData(char *value, char *buffer, int group_idx, char *script_file_msg){
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  strcpy(buffer, value);
   
  NSDL3_VARS(NULL, NULL, "buffer = %s", buffer);
  
  if (!strcasecmp(buffer, "YES")) {
    groupTable[group_idx].ignore_invalid_line = 1;
  } else if (!strcasecmp(buffer, "NO")) {
    groupTable[group_idx].ignore_invalid_line = 0;
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, buffer, "IgnoreInvalidData", "File");
  }
 
  NSDL2_VARS(NULL, NULL, "End of the method : groupTable[group_idx].ignore_invalid_line = [%d], group_idx = %d", groupTable[group_idx].ignore_invalid_line, group_idx);
  return 0;
}

/************************************ 
* Description: Parse api "nsl_static_var" 
* Inputs:      line: nsl_static_var(name2,address2,image2, FILE=user.dat, REFRESH=SESSION, MODE=SEQUENTIAL, VAR_VALUE=F2=file, Encode=All|None|Specified, EncodeChars=<chars>, EncodeSpaceBy=+ or %20);
*             line_numbre: line number on scrip.capture/regristration.spec
*             sess_idx: session id of user
*             script_file_name: project/subproject/script_name
*
*Example format: nsl_static_var(name2,address2,image2, FILE=user.dat, REFRESH=SESSION, MODE=SEQUENTIAL, VAR_VALUE=F2=file, Encode=All|None|Specified, EncodeChars=<chars>, EncodeSpaceBy=+ or %20); 
************************************/
int
input_staticvar_data(char* line, int line_number, int sess_idx, char *script_filename, int api_type) 
{
  int state = NS_ST_STAT_VAR_NAME;
  char staticvar_buf[MAX_FIELD_LENGTH] = {0};
  int create_group = 1;
  int group_idx = -1;
  int var_idx;
  char* line_tok;
  //--int scope = -1;
  int mode = -1;
  char staticvar_file[MAX_FIELD_LENGTH] = {0};
  char staticvar_datadir[MAX_UNIX_FILE_NAME + 1] = {0};
  int file_done = 0;
  //--int scope_done = 0;
  int mode_done = 0;
  int use_once_within_test_done = 0;
  int remove_dup_records_done = 0;
  //int left_boundary_done = 0;
  //int right_boundary_done = 0;
  int var_start = -1;
  int i;
  char buffer[MAX_FIELD_LENGTH];
  //--char absol_var[MAX_FIELD_LENGTH];
  char script_file_msg[MAX_FIELD_LENGTH];
  //--DBT key, data;
  //--char hash_data[8], *size_ptr;
  //--int ret_val;
  int rw_data = 0;
  char column_delimiter[16];
  char firstdataline[12];
  char headerLine[12] = {0};
  int encode_flag_specified = 0;
  char EncodeSpaceBy[16];
  char encode_chars_done = 0;
  char char_to_encode_buf[1024];
  char use_once_within_test_val[16] = {0};
  char remove_dup_records_val[16] = {0};
  //char left_boundary_val[16 + 1] = {0};
  //char right_boundary_val[16 + 1] = {0};
  //--int buf_chk_flag = 0;

  //Set default value
  strcpy(column_delimiter, ",");
  strcpy(firstdataline, "1");
  strcpy(EncodeSpaceBy, "+");
  strcpy(use_once_within_test_val, "NO");
  strcpy(remove_dup_records_val, "NO");
  //strcpy(left_boundary_val, "{");
  //strcpy(right_boundary_val, "}");
  int copyFileToTR_flag = 0;
  int absolute_path_flag = 0;

  NSApi api_ptr;
  int j, ret;
  char *value;
  int exitStatus;
  char err_msg[MAX_ERR_MSG_SIZE + 1];
  char file_name[MAX_ARG_VALUE_SIZE +1];
  int column_flag = 0;  //Flag taken to check if all the variable name are given with column index or not.
  int on_use_once_err = ABORT_USEONCE_SESSION; //default value to abort page in useonce case if no values are present
  unsigned short reltv_var_idx = 0;

  /* SQL Param local variables */
  short int sql_args_done_flag = 0x0000;
  int sql_or_cmd_var = 0;
  char host[512] = {0};
  char port[256] = {0};
  SQLVarTableEntry sqlVarTable = {0};
  SQLConAttr sqlAttr = {0};

  // Dynamic var local variable
  CMDVarTableEntry cmdVarTable = {0};
  short int cmd_args_done_flag = 0x0000;

  NSDL2_VARS(NULL, NULL, "Method called. line = %s, line_number = %d, sess_idx = %d, api_type = %d", line, line_number, sess_idx, api_type);

  if(api_type == NS_STATIC_VAR_NEW)
    sprintf(script_file_msg, "Parsing nsl_static_var() decalaration in file (scripts/%s/%s) at line number %d : ",
            get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), 
	    script_filename, line_number);
            //Previously taking with only script name
            //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), script_filename, line_number);
  else if(api_type == NS_SQL_VAR)
    sprintf(script_file_msg, "Parsing nsl_sql_var() decalaration in file (scripts/%s/%s) at line number %d : ",
            get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), 
	    script_filename, line_number);
            //Previously taking with only script name
            //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), script_filename, line_number);
  else if(api_type == NS_CMD_VAR)
    sprintf(script_file_msg, "Parsing nsl_cmd_var() decalaration in file (scripts/%s/%s) at line number %d : ",
            get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), 
	    script_filename, line_number);

  /*bug id: 101320: ToDo: TBD with DJA*/
  sprintf(file_name, "%s/%s/%s", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name),  sess_idx, "/"), script_filename);
  //Previously taking with only script name
  //sprintf(file_name, "scripts/%s/%s", get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), script_filename);
  file_name[strlen(file_name)] = '\0';

  NSDL1_VARS(NULL, NULL, "api_ptr = %p, file_name = %s", &api_ptr, file_name);
  //ret = parse_api(&api_ptr, line, file_name, err_msg, line_number);
  ret = parse_api_ex(&api_ptr, line, file_name, err_msg, line_number, 1, 0);
  if(ret != 0)
  {
    fprintf(stderr, "Error in parsing api %s\n%s\n", api_ptr.api_name, err_msg);
    return -1;
  }

  #if 0 
 /*  For testing purpose */
  fprintf(stderr, "API Line = %s\n.api_ptr.api_name = [%s]\napi_ptr.num_tokens = %d\n",line, api_ptr.api_name, api_ptr.num_tokens);
  /for(i = 0; i < api_ptr.num_tokens; i++) {
    fprintf(stderr, "\t\t%d. Keyword = [%s] : Value = [%s]\n", i, api_ptr.api_fields[i].keyword, api_ptr.api_fields[i].value);
  }
  #endif

   
  NSDL2_VARS(NULL, NULL, "api_ptr.num_tokens = %d, ", api_ptr.num_tokens);
  for(j = 0; j < api_ptr.num_tokens; j++) {
    line_tok = api_ptr.api_fields[j].keyword;
    value = api_ptr.api_fields[j].value;

    NSDL3_VARS(NULL, NULL, "line_tok = [%s], value = [%s], reltv_var_idx = %hd", line_tok, value, reltv_var_idx);

    if(!strcasecmp(line_tok, "DATADIR")){
      validate_and_copy_datadir(1, value, staticvar_datadir, err_msg);
      if (staticvar_datadir[0] == '\0')
        strcpy(staticvar_datadir, "0"); //To indicate that data dir is applied in this API
    } else if (!strcasecmp(line_tok, "FILE")) {
      set_staticvar_file(value, staticvar_datadir, staticvar_file, sess_idx, &state, group_idx, &file_done, script_file_msg, &absolute_path_flag,
                                                                                                                                       api_type);    
    } else if (!strcasecmp(line_tok, "REFRESH")) {
        set_buffer(value, buffer, &state, group_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "SaveUseOnceOption")) {
        set_buffer_for_saveUseOnceOption(value, buffer, &state, group_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "EncodeMode")) {
        set_buffer_for_encodeMode(value, buffer, &state, group_idx, script_file_msg, &encode_flag_specified);
    } else if (!strcasecmp(line_tok, "CharstoEncode")) {
        set_char_to_encode_buf(value, char_to_encode_buf, &state, group_idx, script_file_msg, encode_flag_specified, &encode_chars_done);
    } else if (!strcasecmp(line_tok, "MODE")) {
        set_buffer_for_mode(value, buffer, &state, group_idx, var_start, script_file_msg, &mode_done, &mode, &column_flag);
    } else if (!strcasecmp(line_tok, "OnUseOnceError")) {
        set_buffer_for_useonce_err(value, buffer, &state, group_idx, script_file_msg, &on_use_once_err);
    } else if ((!strcasecmp(line_tok, "VAR_VALUE")) && (api_type != NS_SQL_VAR)) {
        set_static_var_var_value(value, line_tok, group_idx, script_file_msg);
        state = NS_ST_STAT_VAR_OPTIONS;
    } else if (!strcasecmp(line_tok, "ColumnDelimiter")) {
        set_static_var_column_delim(value, column_delimiter, &state, group_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "FirstDataLine")) {
        set_static_firstDataLine(value, firstdataline, &state, group_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "HeaderLine")) {
        set_static_headerLine( value, headerLine, &state, group_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "EncodeSpaceBy")) {
        set_static_encodeSpaceBy(value, EncodeSpaceBy, line_tok, &state, group_idx, script_file_msg);
    } else if (!strcasecmp(line_tok, "CopyFileToTR")) {
        set_copyFileToTR(value, buffer, group_idx, script_file_msg, &copyFileToTR_flag); 
    } else if ((!strcasecmp(line_tok, "IgnoreInvalidData")) && (api_type != NS_SQL_VAR)) {
        set_ignoreInvalidData(value, buffer, group_idx, script_file_msg); 
    }else if(!strcasecmp(line_tok, "UseOnceWithinTest")){
        set_buffer_for_use_once_within_test(value, buffer, group_idx, script_file_msg, &state, &use_once_within_test_done, use_once_within_test_val);
    }else if(!strcasecmp(line_tok, "RemoveDupRecords")){
        set_buffer_for_remove_dup_records(value, buffer, group_idx, script_file_msg, &state, &remove_dup_records_done, remove_dup_records_val);
    /*In geneator machine no need to execute query, so no need to parse ns_sql_var() on generator machine. In NS mode it will parse. */
    }
#if 0
else if (!strcasecmp(line_tok, "LB")) {
        set_buffer_for_left_boundary(value, group_idx, script_file_msg, &state, &left_boundary_done, left_boundary_val);
        state = NS_ST_STAT_VAR_OPTIONS;
    }
   else if (!strcasecmp(line_tok, "RB")) {
        set_buffer_for_right_boundary(value, group_idx, script_file_msg, &state, &right_boundary_done, right_boundary_val);
        state = NS_ST_STAT_VAR_OPTIONS;
    }
#endif
       else if (api_type == NS_SQL_VAR) { // handling SQL Param parsing
        sql_or_cmd_var = 1;
        if((exitStatus = parse_sql_params(line_tok, value, &sqlVarTable, &state, script_file_msg, host, port, &sql_args_done_flag, staticvar_buf, &group_idx, &var_start, sess_idx, &rw_data, &create_group, &var_idx, &column_flag, &reltv_var_idx)) == -1)
          return exitStatus;
    }
       else if (api_type == NS_CMD_VAR)
       { // handling CMD Param parsing
         sql_or_cmd_var = 2;
         if((exitStatus = parse_cmd_var_params(line_tok, value, &cmdVarTable, &state, script_file_msg, &cmd_args_done_flag, staticvar_buf, &group_idx, &var_start, sess_idx, &rw_data, &create_group, &var_idx, &column_flag, &reltv_var_idx)) == -1)
           return exitStatus;
       }
     else{ /* this is for variables inputted */
        exitStatus =  set_last_option(value, line_tok, staticvar_buf, &group_idx, &var_start, sess_idx, script_file_msg, &state, &rw_data, &create_group, &var_idx, &column_flag, reltv_var_idx);
        reltv_var_idx++;
        if (exitStatus == -1)
          return exitStatus;
    }
  }

  /*
  if ((scope == -1) || (mode == -1)) {
    fprintf(stderr, "%s REFRESH or MODE argument is missing from the declaration\n", script_file_msg );
    return -1;
  }*/

  //Manish: fix bug 2752 and bug 3497
  if(atoi(firstdataline) <= atoi(headerLine))
  { 
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012107_ID, CAV_ERR_1012107_MSG, atoi(firstdataline), atoi(headerLine));
  }

  if ((rw_data == 1) && (mode == WEIGHTED)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012108_ID, CAV_ERR_1012108_MSG);
  }

  if((api_type == NS_SQL_VAR) && (loader_opcode == STAND_ALONE))
  {
    ns_sql_param_validate_keywords(&sql_args_done_flag, script_file_msg);
  }

  if((api_type == NS_CMD_VAR) && (loader_opcode == STAND_ALONE))
  {
    ns_cmd_param_validate_keywords(&cmd_args_done_flag, script_file_msg);
  }
  
  if (((api_type == NS_STATIC_VAR_NEW) || (sql_args_done_flag & SQL_PARAM_SAVEINTOFILE_PARSING_DONE)) && ((!file_done) || (!strlen(staticvar_file))))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012089_ID, CAV_ERR_1012089_MSG, "FILE");
  }
  /* In case of SQL Parameter if data file name is not given by user then on generator machine it will created at path 
     $NS_WDIR/scripts/<project>/<sub_project>/<script_dir>/sql_param_data_file_<api_id>  */
  if((api_type == NS_SQL_VAR) && (loader_opcode == CLIENT_LOADER) && (staticvar_file[0] == '\0'))
  {
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    sprintf(staticvar_file, "%s/%s/sql_param_data_file_%d", GET_NS_TA_DIR(), get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), (total_group_entries -1));
    //Previously taking with only script name
    //sprintf(staticvar_file, "%s/scripts/%s/sql_param_data_file_%d", g_ns_wdir, get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), (total_group_entries -1));
    NSDL3_VARS(NULL, NULL, "staticvar_file = %s", staticvar_file);
  }
 
  if (!strcasecmp(remove_dup_records_val, "YES")){
    NSDL3_VARS(NULL, NULL, "Value of remove_dup_records = %s", remove_dup_records_val);
    remove_duplicate_record_from_file(staticvar_file); 
  }
  
  NSDL2_VARS(NULL, NULL, "copyFileToTR_flag = %d, absolute_path_flag = %d", copyFileToTR_flag, absolute_path_flag);
  if (copyFileToTR_flag && !absolute_path_flag) {
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012110_ID, CAV_ERR_1012110_MSG);
  }
  /*In SQL Parameter copyFileToTR will work if IsSaveIntoFile is set to YES*/
  if((copyFileToTR_flag) && ((sql_or_cmd_var == 1) && (groupTable[group_idx].is_save_into_file_flag == -1)))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012090_ID, CAV_ERR_1012090_MSG);
  }
  /*In CMD var Parameter copyFileToTR will work if IsSaveToFIle is set to YES*/
  if((copyFileToTR_flag) && ((sql_or_cmd_var == 2) && (groupTable[group_idx].persist_flag == -1)))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012090_ID, CAV_ERR_1012090_MSG);
  }
  groupTable[group_idx].absolute_path_flag = (char) absolute_path_flag;
  groupTable[group_idx].sess_idx = sess_idx;

  if ((groupTable[group_idx].column_delimiter = copy_into_big_buf(column_delimiter, 0)) == -1) {
    NS_EXIT(-1, CAV_ERR_1000018, column_delimiter);
  }

  if ((groupTable[group_idx].encode_space_by = copy_into_big_buf(EncodeSpaceBy, 0)) == -1) {
    NS_EXIT(-1, CAV_ERR_1000018, EncodeSpaceBy);
  }

  if ((groupTable[group_idx].data_fname = copy_into_big_buf(staticvar_file, 0)) == -1) {
    NS_EXIT(-1, CAV_ERR_1000018, staticvar_file);
  }

  if (((groupTable[group_idx].data_dir = copy_into_big_buf(staticvar_datadir, 0)) == -1)) {
    NS_EXIT(-1, CAV_ERR_1000018, staticvar_datadir);
  }

  if(api_type == NS_SQL_VAR)
  { //For SQL Parameter value of 'UseOnceWithinTest' is forcefully set to 'YES', as it is not supported for nsl_sql_var() in current design. 
    if ((groupTable[group_idx].UseOnceWithinTest = copy_into_big_buf("YES", 0)) == -1) 
    {
      NS_EXIT(-1, CAV_ERR_1000018, "YES");
    }
  }
  else
  {
    if ((groupTable[group_idx].UseOnceWithinTest = copy_into_big_buf(use_once_within_test_val, 0)) == -1) 
    {
      NS_EXIT(-1, CAV_ERR_1000018, use_once_within_test_val);
    }
  }

  if((mode == USEONCE) || (mode == UNIQUE)){    //if mode is USEONCE then only we will save value for  on_useonce_abort
    groupTable[group_idx].UseOnceAbort = on_use_once_err;
  }

  groupTable[group_idx].first_data_line = atoi(firstdataline);

  /* Default value of encodeChars. This is for if we have
     encode type = specified and dont have chars to encode option*/
  if((encode_chars_done == 0) && (encode_flag_specified == 1))
  {
    groupTable[group_idx].encode_chars[' '] = 1;
    groupTable[group_idx].encode_chars[39] = 1; //Setting for (') as it was givin error on compilation 
    groupTable[group_idx].encode_chars[34] = 1; //Setting for (") as it was givin error on compilation 
    groupTable[group_idx].encode_chars['<'] = 1;
    groupTable[group_idx].encode_chars['>'] = 1;
    groupTable[group_idx].encode_chars['#'] = 1;
    groupTable[group_idx].encode_chars['%'] = 1;
    groupTable[group_idx].encode_chars['{'] = 1;
    groupTable[group_idx].encode_chars['}'] = 1;
    groupTable[group_idx].encode_chars['|'] = 1;
    groupTable[group_idx].encode_chars['\\'] = 1;
    groupTable[group_idx].encode_chars['^'] = 1;
    groupTable[group_idx].encode_chars['~'] = 1;
    groupTable[group_idx].encode_chars['['] = 1;
    groupTable[group_idx].encode_chars[']'] = 1;
    groupTable[group_idx].encode_chars['`'] = 1;
  }
  
  if(api_type == NS_STATIC_VAR_NEW)
  {
    sql_or_cmd_var = 0;
  }
  else if(api_type == NS_SQL_VAR) // Handle SQL Parameter i.e api_type = NS_SQL_VAR
  {
    if(loader_opcode == STAND_ALONE)
    {
      if((ns_sql_param_validate_host_and_port(host, port, script_file_msg, &sqlVarTable)) == -1)
        return -1;
      if((set_ns_sql_odbc_con_attr(&sqlVarTable, &sqlAttr)) == -1)
      return -1;
     
      if((ns_sql_get_data_from_db(&sqlVarTable, &sqlAttr, RETRIEVE_BUFFER_DATA(groupTable[group_idx].column_delimiter), err_msg) == -1))
      {
        fprintf(stderr, "%s \n", err_msg);
        return -1;
      }
      //Resolve Bug 34799 - SQL_Parameter| Performance issue when we are providing 244 MB of data in SQL Parameter. 
      if((run_mode_option & RUN_MODE_OPTION_COMPILE))
        return 0;
    }
   
    /* In case of generator no need to dump query result into given file. This job will be done on controller machine */
    if(loader_opcode != CLIENT_LOADER)
    {  
      /* In NC mode, query is executed by ni_sql_get_data_from_db() and this function is called prior to input_staticvar_data(). So reuse the 
       buffer 'data_file_buf' of api_table */
      if(loader_opcode == MASTER_LOADER)
      {
        sqlVarTable.query_result = api_table[group_idx].data_file_buf;
        sqlVarTable.query_result_size = api_table[group_idx].data_file_size;
      }

      if((sql_args_done_flag & SQL_PARAM_SAVEINTOFILE_PARSING_DONE))
        ns_sql_save_query_result_into_file(&sqlVarTable, staticvar_file, groupTable, group_idx);
    }
          
    groupTable[group_idx].sql_or_cmd_var = IS_SQL_MODE;  
  }
  else  //Handle for NS_CMD_VAR , i.e. api_type = NS_CMD_VAR
  {
    if(loader_opcode == STAND_ALONE)
    {
      ns_cmd_var_get_data_from_prog(&cmdVarTable, RETRIEVE_BUFFER_DATA(groupTable[group_idx].column_delimiter), err_msg, group_idx, staticvar_file, groupTable);
    }
    groupTable[group_idx].sql_or_cmd_var = IS_CMD_MODE;
  }

  char *data_file_buf_rtc = NULL;
  if(api_type == NS_SQL_VAR)
    data_file_buf_rtc = sqlVarTable.query_result;
  else if(api_type == NS_CMD_VAR)
    data_file_buf_rtc = cmdVarTable.prog_result;

  if(input_static_values(staticvar_file, group_idx, sess_idx, var_start, 0, data_file_buf_rtc, sql_or_cmd_var) == -1) 
  {
    fprintf(stderr, "%s Error in reading value from data file %s \n", script_file_msg, staticvar_file);
    return -1;
  }

  if(data_file_buf_rtc)
    FREE_AND_MAKE_NOT_NULL(data_file_buf_rtc, "data_file_buf_rtc", -1);

  //If given parameter have USEONCE mode then check for same data file.
  if(groupTable[group_idx].sequence == USEONCE)
  {
    char fname[5 * 1024];
    get_data_file_wth_full_path(staticvar_file, fname, sess_idx, group_idx, 0);

    for(i = 0; i < total_group_entries - 1; i++)
    {
      if(groupTable[i].sequence == USEONCE) //Mode is USEONCE then only check the script name
      {
        if(!strcmp(fname, RETRIEVE_BUFFER_DATA(groupTable[i].data_fname)))
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012118_ID, CAV_ERR_1012118_MSG, fname);
        }
      }
    }
  }

#if 0
  //Only for Fix Concurrent Users + non-pct mode
  //Set the number of users using the script belonging to this stat var group
  if ((testCase.mode == TC_FIX_CONCURRENT_USERS) && (global_settings->use_pct_prof == 0)) {
    int num_users = 0;
    for (i = 0; i < total_runprof_entries; i++) {
      if (runProfTable[i].sessprof_idx == sess_idx) {
	num_users += runProfTable[i].quantity;
      }
    }
    groupTable[group_idx].num_users = num_users;
  }
#endif

  return 0;
}

/**
 * Shr mem stuff:
 */

#if 0
//static int get_big_buf_size(int sv_grp_id, PerProcVgroupTable *per_proc_fparam_grp)
static int get_big_buf_size(int sv_grp_id, int nvm_id)
{
  int shr_big_buf_size = 0;
  int i = 0, j = 0;
  int var_idx = -1;
  int val_idx = -1;
  //int num_dr = per_proc_fparam_grp->total_val;
  //int pp_start = per_proc_fparam_grp->start_val;

  //NSDL2_VARS(NULL, NULL, "Method called, sv_grp_id = %d, pp_start = %d, num_dr = %d", sv_grp_id, pp_start, num_dr);

  int old_vals = 0;
  int new_vals = 0;
  PointerTableEntry_Shr *value = NULL;
  PerProcVgroupTable *per_proc_fparam_grp = &per_proc_vgroup_table[(nvm_id * total_group_entries) + sv_grp_id];
  PerProcVgroupTable *per_proc_fparam_grp_rtc = &per_proc_vgroup_table_rtc[(nvm_id * total_group_entries) + sv_grp_id];
  int mode = per_proc_fparam_grp_rtc->rtc_flag ;
  int pp_start = per_proc_fparam_grp->start_val;
  int pp_start_rtc = per_proc_fparam_grp_rtc->start_val;

  NSDL2_VARS(NULL, NULL, "Method called, sv_grp_id = %d, nvm_id = %d, mode = %d", sv_grp_id, nvm_id, mode);
  NSDL2_VARS(NULL, NULL, "per_proc_vgroup_table(OBD): start_val = %d, num_val = %d, total_val = %d, "
                         "per_proc_vgroup_table_rtc(NEW): start_val = %d, num_val = %d, total_val = %d", 
                          per_proc_fparam_grp->start_val, per_proc_fparam_grp->num_val, per_proc_fparam_grp->total_val,
                          per_proc_fparam_grp_rtc->start_val, per_proc_fparam_grp_rtc->num_val, per_proc_fparam_grp_rtc->total_val);

  for(i = 0; i < groupTable[sv_grp_id].num_vars; i++)
  {
    var_idx = groupTable[sv_grp_id].start_var_idx + i;
    NSDL3_VARS(NULL, NULL, "var_idx = %d", var_idx);

    if(mode == APPEND_MODE)
    {
      //Since parent can't direct access NVM's memory and hence take memory diff and then access memory
      long shm_offset = (char*)per_proc_fparam_grp->p_pointer_table_shr_mem -
                                 (char*)per_proc_fparam_grp->pointer_table_shr_mem ;

      PointerTableEntry_Shr* value_start_ptr =
                  (VarTableEntry_Shr *)((char*)(per_proc_fparam_grp->p_variable_table_shr_mem[var_idx].value_start_ptr) + shm_offset);

      NSDL2_VARS(NULL, NULL, "value_start_ptr = %p, shm_offset = %ld", value_start_ptr, shm_offset);

      if(shm_offset != 0)
        pp_start = 0;

      //Get Old val size
      for(j = 0; j < per_proc_fparam_grp->total_val; j++)
      {
        value = value_start_ptr + pp_start + j;  
        shr_big_buf_size += value->size + 1; //1 for null
        NSDL2_VARS(NULL, NULL, "Old Data: j = %d, size = %d, val = %s", j, value->size, value->big_buf_pointer);
      }   
    }

    //get new val size
    for(j = 0; j < per_proc_fparam_grp_rtc->total_val; j++)
    {
      val_idx = varTable[var_idx].value_start_ptr + pp_start_rtc + j; 
      shr_big_buf_size += fparamValueTable[val_idx].size + 1; //1 for null  
      NSDL3_VARS(NULL, NULL, "New Data: j = %d, val_idx = %d, size = %d, val = %s", 
                              j, val_idx, fparamValueTable[val_idx].size, 
                              nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[val_idx].big_buf_pointer));
    }
  } 

  #if 0
  for(i = 0; i < groupTable[sv_grp_id].num_vars; i++)
  {
    var_idx = groupTable[sv_grp_id].start_var_idx + i;
    NSDL3_VARS(NULL, NULL, "VARIBAL: var_idx = %d", var_idx);
    for(j = 0; j < num_dr; j++)
    {
      val_idx = varTable[var_idx].value_start_ptr + pp_start + j; 
      NSDL3_VARS(NULL, NULL, "DATA: val_idx = %d", val_idx);
      shr_big_buf_size += fparamValueTable[val_idx].size + 1; //1 for null  
    }
  }
  #endif
  
  NSDL2_VARS(NULL, NULL, "shr_big_buf_size = %d", shr_big_buf_size);
  return shr_big_buf_size;
}
#endif

#define GET_START_IDX_OF_DATA_LINE_FOR_NVM(old_vals, new_vals, old_start_idx, new_start_idx) {\
  /* If mode is append and rtc data is less than old one */ \
if((new_vals < old_vals))  \
{\
  if(new_start_idx + new_vals < old_vals)  \
  {\
    old_vals = new_vals;   \
    old_start_idx = new_start_idx;  \
  }\
  else if(new_start_idx  < old_vals)  \
  {\
    old_vals -= new_start_idx;   \
    old_start_idx = new_start_idx;    \
  }\
  else   \
  {\
    new_start_idx -=  old_vals;  \
    old_vals = 0;               \
  }\
}\
else\
{\
  if(old_start_idx || (!old_start_idx && !new_start_idx))   \
  {\
    new_start_idx -= old_start_idx;   \
    old_start_idx = 0;  \
  }\
  else \
  {\
    new_start_idx -= old_vals;  \
    old_vals = 0;\
  }\
}\
new_vals -= old_vals;\
}

static int get_big_buf_size(int sv_grp_id,int nvm_id)
{
  int shr_big_buf_size = 0;
  int i = 0, j = 0;
  int var_idx = -1;
  int val_idx = -1;
  int new_vals = 0;
  int old_vals = 0;
  PointerTableEntry_Shr *value = NULL;
  PerProcVgroupTable *per_proc_fparam_grp = &per_proc_vgroup_table[(nvm_id * total_group_entries) + sv_grp_id]; 
  PerProcVgroupTable *per_proc_fparam_grp_rtc = &per_proc_vgroup_table_rtc[(nvm_id * total_group_entries) + sv_grp_id]; 
  int mode = per_proc_fparam_grp_rtc->rtc_flag ;
  int pp_start;
  int pp_start_rtc;


  NSDL2_VARS(NULL, NULL, "Method called, sv_grp_id = %d, nvm_id = %d, mode = %d", sv_grp_id, nvm_id, mode);


  if(mode == APPEND_MODE)
  {
    old_vals = per_proc_fparam_grp->total_val;
    pp_start = per_proc_fparam_grp->start_val;
  }
  else
  {
    old_vals = 0;
    pp_start = 0;
  }

  new_vals = per_proc_fparam_grp_rtc->total_val;
  pp_start_rtc = per_proc_fparam_grp_rtc->start_val;

  NSDL3_VARS(NULL, NULL, "Before Calculation : old_vals = %d, new_vals=%d, old_start = %d, new_start = %d",
                          old_vals, new_vals, pp_start, pp_start_rtc);
 
  GET_START_IDX_OF_DATA_LINE_FOR_NVM (old_vals, new_vals, pp_start, pp_start_rtc);
 
  NSDL3_VARS(NULL, NULL, "After Calculation  : old_vals = %d, new_vals=%d, old_start = %d, new_start = %d",
                          old_vals, new_vals, pp_start, pp_start_rtc);

  for(i = 0; i < groupTable[sv_grp_id].num_vars; i++)
  {
    var_idx = groupTable[sv_grp_id].start_var_idx + i;
    NSDL3_VARS(NULL, NULL, "i = %d, var_idx = %d", i, var_idx);
    long shm_offset = (char*)per_proc_fparam_grp->p_pointer_table_shr_mem - 
		               (char*)per_proc_fparam_grp->pointer_table_shr_mem ;
    PointerTableEntry_Shr* value_start_ptr = 
		(PointerTableEntry_Shr *)((char*)(per_proc_fparam_grp->p_variable_table_shr_mem[i].value_start_ptr) + shm_offset);

    NSDL2_VARS(NULL, NULL, "value_start_ptr = %p, pp_start = %d, pp_start_rtc = %d, shm_offset = %d", 
                            value_start_ptr, pp_start, pp_start_rtc, shm_offset);
    for(j = 0; j < per_proc_fparam_grp_rtc->total_val; j++)
    {
      if(j < old_vals)
      { 
        value = value_start_ptr + pp_start + j;
        shr_big_buf_size += value->size + 1; //1 for null  
        NSDL2_VARS(NULL, NULL, "Old Data: j = %d, size = %d, val = %s", j, value->size, (value->big_buf_pointer + shm_offset));
      }
      else
      {
        val_idx = varTable[var_idx].value_start_ptr + pp_start_rtc + ( j - old_vals); 
        shr_big_buf_size += fparamValueTable[val_idx].size + 1; //1 for null  
        NSDL3_VARS(NULL, NULL, "New Data: j = %d, val_idx = %d, size = %d, val = %s", 
                              j, val_idx, fparamValueTable[val_idx].size, 
                              nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[val_idx].big_buf_pointer));
      }
    }
  }

  NSDL2_VARS(NULL, NULL, "shr_big_buf_size = %d", shr_big_buf_size);

  return shr_big_buf_size;
}

inline VarTableEntry_Shr *get_fparam_var(VUser *vptr, int sess_id, int hash_code)
{
  int fparam_grp_idx = -1;
  int fparam_var_idx = -1;
  VarTableEntry_Shr *var = NULL; 
  VarTransTableEntry_Shr *var_trans_tbl = NULL;
  
  NSDL2_VARS(vptr, NULL, "Method Called, vptr = %p, sess_id = %d, hash_code = %d, my_port_index = %u, total_group_entries = %d", 
                          vptr, sess_id, hash_code, my_port_index, total_group_entries);

  if(vptr != NULL)
    var_trans_tbl = &vptr->sess_ptr->vars_trans_table_shr_mem[hash_code];
  else if(sess_id >= 0)
    var_trans_tbl = &session_table_shr_mem[sess_id].vars_trans_table_shr_mem[hash_code]; 
  else 
    return NULL;

  fparam_grp_idx = var_trans_tbl->fparam_grp_idx; 
  fparam_var_idx = var_trans_tbl->var_idx;

  if(vptr != NULL)
    var = &per_proc_vgroup_table[(my_port_index * total_group_entries) + fparam_grp_idx].variable_table_shr_mem[fparam_var_idx];
  else if(sess_id >= 0)
    var = &variable_table_shr_mem[group_table_shr_mem[fparam_grp_idx].start_var_idx + fparam_var_idx];

  NSDL2_VARS(NULL, NULL, "Method end, fparam_grp_idx = %d, fparam_var_idx = %d, var = %p, var name = %s", 
                          fparam_grp_idx, fparam_var_idx, var, var->name_pointer);
  return var;
}

/* This function will data of particular NVM only */
void dump_per_proc_vgroup_table_internal(PerProcVgroupTable *lol_proc_vgroup)
{
  int j,k,l;
  GroupTableEntry_Shr *file_grp;
  VarTableEntry_Shr *var;
  PointerTableEntry_Shr *val_ptr;
  PerProcVgroupTable *proc_vgroup = NULL;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, NVM = %d, total_group_entries = %d", my_child_index, total_group_entries);
  for(j = 0; j < total_group_entries; j++)
  {
    proc_vgroup = &lol_proc_vgroup[j];

    NSDL2_SCHEDULE(NULL, NULL, "j = %d, per proc num values = %d, index_key_var_idx = %d",
                                j, proc_vgroup->num_val, group_table_shr_mem[j].index_key_var_idx);
    //Skip if group not participate in NVM distribution or if group is of index type
    if((proc_vgroup->num_val == 0) || (group_table_shr_mem[j].index_key_var_idx != -1))
      continue;

    NSDL2_SCHEDULE(NULL, NULL, "ProcVgroup id = %d, start_val = %d, num_val = %d, total_val = %d, num_script_users = %d, "
                               "last_file_fd = %d, shm_addr = %p, shm_key = %d, g_big_buf_shr_mem = %p, "
                               "group_table_shr_mem = %p, variable_table_shr_mem = %p, weight_table_shr_mem = %p, "
                               "pointer_table_shr_mem = %p",
                                my_port_index, proc_vgroup->start_val, proc_vgroup->num_val,
                                proc_vgroup->total_val, proc_vgroup->num_script_users,
                                proc_vgroup->last_file_fd, proc_vgroup->shm_addr,
                                (int)proc_vgroup->shm_key, proc_vgroup->g_big_buf_shr_mem,
                                proc_vgroup->group_table_shr_mem, proc_vgroup->variable_table_shr_mem,
                                proc_vgroup->weight_table_shr_mem, proc_vgroup->pointer_table_shr_mem); 

    file_grp = proc_vgroup->group_table_shr_mem;

    NSDL2_SCHEDULE(NULL, NULL, "fparam id = %d Details: type = %d, sequence = %d, num_values = %d, num_vars = %d, "
                               "sess_name = [%s], data_fname = [%s], column_delimiter = [%s], start_var_idx = %d", 
                                file_grp->idx, file_grp->type, file_grp->sequence, file_grp->num_values, file_grp->num_vars,
                                file_grp->sess_name, file_grp->data_fname, file_grp->column_delimiter, file_grp->start_var_idx);

    for(k = 0; k < file_grp->num_vars; k++)
    {
      var = &proc_vgroup->variable_table_shr_mem[k];
      val_ptr = var->value_start_ptr;

      NSDL2_SCHEDULE(NULL, NULL, "Var id = %d Details:  group_ptr = %p, value_start_ptr = %p, variable name = [%s], var_size = %d, "
                                 "uvtable_idx = %d",
                                  k, var->group_ptr, var->value_start_ptr, var->name_pointer, var->var_size, var->uvtable_idx);  

      for(l = 0; l < proc_vgroup->num_val; l++, val_ptr++)
      {
        if (var->is_file == IS_FILE)
          NSDL2_SCHEDULE(NULL, NULL, "Data: data size = [%d], data = [%s]", val_ptr->size, val_ptr->big_buf_pointer); 
      }
    }
  }
}

void dump_per_proc_vgroup_table(PerProcVgroupTable *lol_per_proc_vgroup_table)
{
  int i;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, per_proc_vgroup_table (%p) Data Dump:- num_process = %d", 
                              lol_per_proc_vgroup_table, global_settings->num_process);
  for(i = 0; i < global_settings->num_process; i++)
  {
    NSDL2_SCHEDULE(NULL, NULL, "NVM: %d", i); 
    dump_per_proc_vgroup_table_internal(&lol_per_proc_vgroup_table[i * total_group_entries]);
  } 
}

void alloc_static_vars_shr_mem()
{
  int nvm_id, sv_grp_id;
  int tot_num_val = 0;

  g_static_vars_shr_mem_size = 0;

  NSDL2_VARS(NULL, NULL, "Method called, num_process = %d, total_group_entries = %d, sizeof(GroupTableEntry_Shr) = %ld, "
                         "sizeof(VarTableEntry_Shr) = %ld", 
                          global_settings->num_process, total_group_entries, sizeof(GroupTableEntry_Shr), 
                          sizeof(VarTableEntry_Shr));

  for (nvm_id = 0; nvm_id < global_settings->num_process; nvm_id++)
  {
    for (sv_grp_id = 0; sv_grp_id < total_group_entries; sv_grp_id++)
    {
      if((((global_settings->protocol_enabled & RBU_API_USED) && global_settings->rbu_enable_auto_param) && 
          (chk_rbu_group(sv_grp_id) == 1)) ||
         (group_table_shr_mem[sv_grp_id].index_key_var_idx != -1))
         continue;

      tot_num_val = group_table_shr_mem[sv_grp_id].num_values;

      if(groupTable[sv_grp_id].weight_idx != -1)
        g_static_vars_shr_mem_size += sizeof(WeightTableEntry) * tot_num_val;

      g_static_vars_shr_mem_size += sizeof(GroupTableEntry_Shr) + 
                      sizeof(VarTableEntry_Shr) * group_table_shr_mem[sv_grp_id].num_vars;

      NSDL3_VARS(NULL, NULL, "g_static_vars_shr_mem_size = %ld for NVM = %d, File Param group = %d, num_vars = %d", 
                              g_static_vars_shr_mem_size, nvm_id, sv_grp_id, group_table_shr_mem[sv_grp_id].num_vars);
    }
  }

  //Allocating shared memory
  g_static_vars_shr_mem = do_shmget_with_id(g_static_vars_shr_mem_size, "StaticVar Shared Memory", &g_static_vars_shr_mem_key);

  g_static_vars_shr_mem_ptr = (char *)g_static_vars_shr_mem;
  memset(g_static_vars_shr_mem_ptr, 0, g_static_vars_shr_mem_size);

  NSDL2_VARS(NULL, NULL, "StaticVar Shared memory: g_static_vars_shr_mem = %p, g_static_vars_shr_mem_size = %ld, "
                         "g_static_vars_shr_mem_key = %d", 
                          g_static_vars_shr_mem, g_static_vars_shr_mem_size, g_static_vars_shr_mem_key);
}

/* -------------------------------------------------------------------------------------------------
   Name		: per_proc_create_staticvar_shr_mem()
   
   Purpose	: allocating single shared memory segment for groupTable, varTable, weightTable, 
                  pointerTable, and big_buf. And copying data into this from non-shared memory.

   Input	: nvm_id - NVM(process) Id 
                  sv_grp_id - File parameter group id
  
   Output	: On success - This function will create shared memory for per process(i.e. NVM) and 
                    set related pointers of per_proc_vgroup_table 
                  On Failure - exit the process (i.e. NVM) 

   Date		: 11 july 2016
 
   Author	: Tanmay/Manish
   ------------------------------------------------------------------------------------------------*/
void per_proc_create_staticvar_shr_mem(int nvm_id, int sv_grp_id, int runtime)
{
  char msg[512 + 1] = "";
  int i = 0;                                 /* tmp integer for looping like task */
  int shr_mem_size = 0;                      /* total size of shared memory sgement */
  char *ns_static_vars_shr_mem = NULL;       /* to point head of malloced shared memory */
  char *ns_static_vars_shr_mem_ptr = NULL;   /* tmp pointer to travels thorugh malloced shared memory */

  GroupTableEntry_Shr *loc_group_table_shr_mem = NULL;
  VarTableEntry_Shr *loc_variable_table_shr_mem = NULL;
  WeightTableEntry *loc_weight_table_shr_mem = NULL;
  PointerTableEntry_Shr *loc_pointer_table_shr_mem = NULL;
  char *loc_g_big_buf_shr_mem = NULL;

  PerProcVgroupTable *per_proc_fparam_grp = NULL;
  int per_proc_num_val = 0;
  int tot_num_val = 0;
  int pp_start_val_idx = 0;
  int mode = -1;
  int bb_size = 0;

  if(!runtime)
  {
    per_proc_fparam_grp = &per_proc_vgroup_table[(nvm_id * total_group_entries) + sv_grp_id];
    tot_num_val = groupTable[sv_grp_id].num_values;
  }
  else
  {
    per_proc_fparam_grp = &per_proc_vgroup_table_rtc[(nvm_id * total_group_entries) + sv_grp_id];
    tot_num_val = group_table_shr_mem[sv_grp_id].num_values + groupTable[sv_grp_id].num_values;
    mode = per_proc_fparam_grp->rtc_flag;
  }

  per_proc_num_val = per_proc_fparam_grp->total_val;

  pp_start_val_idx = (nvm_id == 0)?0:
                         (per_proc_fparam_grp->start_val == 0)?0:per_proc_fparam_grp->start_val;


  NSDL1_VARS(NULL, NULL, "Method called, Copy data into shared memory for NVM = %d, Static Var (grp id = %d) = %s, "
                         "Number of variables = %d, Total number of data records = %d, "
                         "per_proc_num_val = %d, total_group_entries = %d, start_val = %d, runtime = %d, pp_start_val_idx = %d" ,
                          nvm_id, sv_grp_id,
                          !runtime?RETRIEVE_BUFFER_DATA(varTable[groupTable[sv_grp_id].start_var_idx].name_pointer):
                          variable_table_shr_mem[groupTable[sv_grp_id].start_var_idx].name_pointer,
                          groupTable[sv_grp_id].num_vars, tot_num_val,
                          per_proc_num_val, total_group_entries, per_proc_fparam_grp->start_val, runtime, pp_start_val_idx);

  NSDL1_VARS(NULL, NULL, "groupTable[%d].weight_idx = %d", sv_grp_id, groupTable[sv_grp_id].weight_idx);
  if(groupTable[sv_grp_id].weight_idx != -1)
  {
    shr_mem_size += sizeof(WeightTableEntry) * tot_num_val;
    per_proc_num_val = tot_num_val;
  }

  shr_mem_size += sizeof(GroupTableEntry_Shr) +
                  sizeof(VarTableEntry_Shr) * group_table_shr_mem[sv_grp_id].num_vars;

  if(runtime)
  {
    shr_mem_size += sizeof(PointerTableEntry_Shr) * group_table_shr_mem[sv_grp_id].num_vars * per_proc_num_val;

    bb_size = get_big_buf_size(sv_grp_id, nvm_id);
    NSDL2_VARS(NULL, NULL, "bb_size = %d", bb_size);

    shr_mem_size += bb_size;
  }

  NSDL2_VARS(NULL, NULL, "Total shr_mem_size = %d for NVM = %d, File Param group = %d, g_static_vars_shr_mem_ptr = %p",
                          shr_mem_size, nvm_id, sv_grp_id, g_static_vars_shr_mem_ptr);

  if(!runtime)
  {
    ns_static_vars_shr_mem = g_static_vars_shr_mem_ptr;
    g_static_vars_shr_mem_ptr += shr_mem_size;

    per_proc_fparam_grp->shm_addr = ns_static_vars_shr_mem;
    per_proc_fparam_grp->shm_key = -1;

    NSDL2_VARS(NULL, NULL, "ns_static_vars_shr_mem = %p, Used = %ld",
                            ns_static_vars_shr_mem,
                            ((char *)g_static_vars_shr_mem_ptr - (char*)g_static_vars_shr_mem));
  }
  else
  {
    sprintf(msg, "ns_static_vars_shr_mem for nvm %d, sv_grp_id = %d", nvm_id, sv_grp_id);

    //Allocating shared memory
    per_proc_fparam_grp->shm_addr = do_shmget_with_id(shr_mem_size, msg, &per_proc_fparam_grp->shm_key);
    ns_static_vars_shr_mem = (char *)per_proc_fparam_grp->shm_addr;
    memset(ns_static_vars_shr_mem, 0, shr_mem_size);
  }

  ns_static_vars_shr_mem_ptr = ns_static_vars_shr_mem;

  NSDL2_VARS(NULL, NULL, "ns_static_vars_shr_mem = %p, weight_idx = %d",
                          ns_static_vars_shr_mem, groupTable[sv_grp_id].weight_idx);

  /* Fill pointer table and big buf */
  int per_proc_num_val_old = 0;
  int per_proc_num_val_new = 0;
  int pp_start_old = 0;
  int pp_start_new = 0;
  PointerTableEntry_Shr *value = NULL;
  PerProcVgroupTable *per_proc_fparam_grp_old = NULL; 
  if(runtime)
  {
    per_proc_num_val_new = per_proc_num_val;
    pp_start_new =  per_proc_fparam_grp->start_val;
 
    per_proc_fparam_grp_old = &per_proc_vgroup_table[(nvm_id * total_group_entries) + sv_grp_id];
    if(mode == APPEND_MODE)
    {
      per_proc_num_val_old = per_proc_fparam_grp_old->total_val;
      pp_start_old = per_proc_fparam_grp_old->start_val;
    }
    else
    {
      per_proc_num_val_old = 0;
      pp_start_old = 0;
    }
    NSDL1_VARS(NULL, NULL, "Before calculation : per_proc_num_val_old = %d, per_proc_num_val_new = %d,"
                           "pp_start_old = %d, pp_start_new = %d",
                            per_proc_num_val_old, per_proc_num_val_new, pp_start_old, pp_start_new);

    GET_START_IDX_OF_DATA_LINE_FOR_NVM (per_proc_num_val_old, per_proc_num_val_new, pp_start_old, pp_start_new);

    NSDL1_VARS(NULL, NULL, "After calculation : per_proc_num_val_old = %d per_proc_num_val_new = %d,"
                           "start_val_old = %d start_val_new = %d", per_proc_num_val_old, per_proc_num_val_new,
                            pp_start_old, pp_start_new);

  }

  /*Set: per_proc_fparam_grp->weight_table_shr_mem*/
  if(groupTable[sv_grp_id].weight_idx != -1)
  {
    per_proc_fparam_grp->weight_table_shr_mem = (WeightTableEntry*)ns_static_vars_shr_mem_ptr;
    if (runtime)
    {
      memcpy(per_proc_fparam_grp->weight_table_shr_mem, per_proc_fparam_grp_old->p_weight_table_shr_mem,
                                                        sizeof(WeightTableEntry) * per_proc_num_val_old);
      memcpy((char *)per_proc_fparam_grp->weight_table_shr_mem + (sizeof(WeightTableEntry) * per_proc_num_val_old), 
                                                        &weightTable[groupTable[sv_grp_id].weight_idx],
                                                        sizeof(WeightTableEntry) * per_proc_num_val_new);
      if (mode == APPEND_MODE)
      {
        for(int i = per_proc_num_val_old; i < per_proc_num_val; i++)
        { 
          per_proc_fparam_grp->weight_table_shr_mem[i].value_weight += per_proc_fparam_grp_old->p_weight_table_shr_mem[per_proc_num_val_old - 1].value_weight;
        }
      }
    }
    else
    {
      memcpy(per_proc_fparam_grp->weight_table_shr_mem, &weightTable[groupTable[sv_grp_id].weight_idx],
                                                        sizeof(WeightTableEntry) * per_proc_num_val);
    }
    loc_weight_table_shr_mem = per_proc_fparam_grp->weight_table_shr_mem;
    per_proc_fparam_grp->p_weight_table_shr_mem = loc_weight_table_shr_mem;
    ns_static_vars_shr_mem_ptr += sizeof(WeightTableEntry) * per_proc_num_val;
  }

  /*Set: per_proc_fparam_grp->group_table_shr_mem */
  per_proc_fparam_grp->group_table_shr_mem = (GroupTableEntry_Shr *)ns_static_vars_shr_mem_ptr;
  loc_group_table_shr_mem = per_proc_fparam_grp->group_table_shr_mem;
  per_proc_fparam_grp->p_group_table_shr_mem = loc_group_table_shr_mem;
  ns_static_vars_shr_mem_ptr += sizeof(GroupTableEntry_Shr);

  /*Set: per_proc_fparam_grp->variable_table_shr_mem */
  per_proc_fparam_grp->variable_table_shr_mem = (VarTableEntry_Shr *)ns_static_vars_shr_mem_ptr;
  loc_variable_table_shr_mem = per_proc_fparam_grp->variable_table_shr_mem;
  per_proc_fparam_grp->p_variable_table_shr_mem = loc_variable_table_shr_mem;
  ns_static_vars_shr_mem_ptr += sizeof(VarTableEntry_Shr) * groupTable[sv_grp_id].num_vars;

  if(runtime)
  {
    /*Set: per_proc_fparam_grp->pointer_table_shr_mem */
    per_proc_fparam_grp->pointer_table_shr_mem = (PointerTableEntry_Shr *)ns_static_vars_shr_mem_ptr;
    loc_pointer_table_shr_mem = per_proc_fparam_grp->pointer_table_shr_mem;
    per_proc_fparam_grp->p_pointer_table_shr_mem = loc_pointer_table_shr_mem;
    ns_static_vars_shr_mem_ptr += sizeof(PointerTableEntry_Shr) * groupTable[sv_grp_id].num_vars * per_proc_num_val;

    /*Set: per_proc_fparam_grp->g_big_buf_shr_mem */
    per_proc_fparam_grp->g_big_buf_shr_mem = (char *) ns_static_vars_shr_mem_ptr;
    loc_g_big_buf_shr_mem = per_proc_fparam_grp->g_big_buf_shr_mem;
    per_proc_fparam_grp->p_g_big_buf_shr_mem = loc_g_big_buf_shr_mem;
  }

  NSDL2_VARS(NULL, NULL, "loc_weight_table_shr_mem = %p, loc_group_table_shr_mem = %p, loc_variable_table_shr_mem = %p, loc_g_big_buf_shr_mem = %p",
                          loc_weight_table_shr_mem, loc_group_table_shr_mem, loc_variable_table_shr_mem, loc_g_big_buf_shr_mem);

  /*Copy data form non-shared memory groupTable to per_proc_fparam_grp->group_table_shr_mem */
  loc_group_table_shr_mem->type = groupTable[sv_grp_id].type;
  loc_group_table_shr_mem->sequence = groupTable[sv_grp_id].sequence;

  if(loc_group_table_shr_mem->sequence == UNIQUE)
    loc_group_table_shr_mem->group_wei_uni.unique_group_id = group_table_shr_mem[sv_grp_id].group_wei_uni.unique_group_id;
  else
    loc_group_table_shr_mem->group_wei_uni.weight_ptr = loc_weight_table_shr_mem;

  loc_group_table_shr_mem->idx = groupTable[sv_grp_id].idx;
  loc_group_table_shr_mem->num_values = groupTable[sv_grp_id].num_values;
  //TODO: need to re-think
  loc_group_table_shr_mem->start_var_idx = groupTable[sv_grp_id].start_var_idx;
  loc_group_table_shr_mem->num_vars = groupTable[sv_grp_id].num_vars;

  memcpy(loc_group_table_shr_mem->encode_chars, groupTable[sv_grp_id].encode_chars, TOTAL_CHARS);
  loc_group_table_shr_mem->encode_type = groupTable[sv_grp_id].encode_type;

  loc_group_table_shr_mem->UseOnceOptiontype = groupTable[sv_grp_id].UseOnceOptiontype;
  loc_group_table_shr_mem->UseOnceAbort = groupTable[sv_grp_id].UseOnceAbort;
  loc_group_table_shr_mem->first_data_line = groupTable[sv_grp_id].first_data_line;
  loc_group_table_shr_mem->absolute_path_flag = groupTable[sv_grp_id].absolute_path_flag;

  if(!runtime)
  {
    loc_group_table_shr_mem->sess_name = BIG_BUF_MEMORY_CONVERSION(gSessionTable[groupTable[sv_grp_id].sess_idx].sess_name);
    loc_group_table_shr_mem->data_fname = BIG_BUF_MEMORY_CONVERSION(groupTable[sv_grp_id].data_fname);
    loc_group_table_shr_mem->column_delimiter = BIG_BUF_MEMORY_CONVERSION(groupTable[sv_grp_id].column_delimiter);
    loc_group_table_shr_mem->encode_space_by = BIG_BUF_MEMORY_CONVERSION(groupTable[sv_grp_id].encode_space_by);
  }
  else
  {
    loc_group_table_shr_mem->sess_name = group_table_shr_mem[sv_grp_id].sess_name;
    loc_group_table_shr_mem->data_fname = group_table_shr_mem[sv_grp_id].data_fname;
    loc_group_table_shr_mem->column_delimiter = group_table_shr_mem[sv_grp_id].column_delimiter;
    loc_group_table_shr_mem->encode_space_by = group_table_shr_mem[sv_grp_id].encode_space_by;
    if(mode == APPEND_MODE)
      loc_group_table_shr_mem->num_values += group_table_shr_mem[fparam_rtc_tbl[i].fparam_grp_idx].num_values;
  }

  /* Since index var needs only groupTable shared memory and in 4.1.6 we are not deleting this so no need to do following */
  #if 0
  /* Following things are only for INDEX_VAR */
  NSDL2_VARS(NULL, NULL, "index_var = %d", groupTable[sv_grp_id].index_var);
  if(groupTable[sv_grp_id].index_var  != -1) {
    int index_key_var_hash_idx, index_key_var_type;
    char *index_var = BIG_BUF_MEMORY_CONVERSION(groupTable[sv_grp_id].index_var);
    if (gSessionTable[groupTable[sv_grp_id].sess_idx].var_hash_func) {
      index_key_var_hash_idx = gSessionTable[groupTable[sv_grp_id].sess_idx].var_hash_func(index_var, strlen(index_var));
    } else {
      index_key_var_hash_idx = -1;
    }
    if(index_key_var_hash_idx == -1) {
      fprintf(stderr, "Index Key var '%s' is not declared for session %s.\n",
                   index_var, loc_group_table_shr_mem->sess_name); 
      exit(-1);
    }

    loc_group_table_shr_mem->index_key_var_idx = 
             gSessionTable[groupTable[sv_grp_id].sess_idx].vars_trans_table_shr_mem[index_key_var_hash_idx].user_var_table_idx;

    index_key_var_type = gSessionTable[groupTable[sv_grp_id].sess_idx].vars_trans_table_shr_mem[index_key_var_hash_idx].var_type;

    switch (index_key_var_type) {
     case VAR:
     case COOKIE_VAR:
       fprintf(stderr, "index key var (%s) can not be a type of STATIC or COOKIE VAR for session %s.\n",
                        index_var,
                        loc_group_table_shr_mem->sess_name); 
       exit(-1);
    }

    loc_group_table_shr_mem->idx_var_hash_func = groupTable[sv_grp_id].idx_var_hash_func;
  }
  #endif

  /* Next goes the variable table */
  int pt_idx = 0, j, var_idx = 0;
  for (i = 0; i < groupTable[sv_grp_id].num_vars; i++)
  {
    var_idx = i + groupTable[sv_grp_id].start_var_idx;
    pt_idx = i * per_proc_num_val;

    /* Fill varTable*/
    loc_variable_table_shr_mem[i].group_ptr = loc_group_table_shr_mem;

    loc_variable_table_shr_mem[i].value_start_ptr = per_proc_fparam_grp->pointer_table_shr_mem + pt_idx;

    NSDL1_VARS(NULL, NULL, "i = %d, var_idx = %d, pt_idx = %d, value_start_ptr = %p",
                            i, var_idx, pt_idx, loc_variable_table_shr_mem[i].value_start_ptr);

    loc_variable_table_shr_mem[i].name_pointer = !runtime?BIG_BUF_MEMORY_CONVERSION(varTable[var_idx].name_pointer):
                                                  variable_table_shr_mem[var_idx].name_pointer;
    loc_variable_table_shr_mem[i].var_size = varTable[var_idx].var_size;
    loc_variable_table_shr_mem[i].is_file = varTable[var_idx].is_file;
    loc_variable_table_shr_mem[i].column_idx = varTable[var_idx].column_idx;
    loc_variable_table_shr_mem[i].uvtable_idx = variable_table_shr_mem[var_idx].uvtable_idx;

    //Fill pointer table only for runtime 
    if(!runtime)
    {
      loc_variable_table_shr_mem[i].value_start_ptr = variable_table_shr_mem[var_idx].value_start_ptr + pp_start_val_idx;
      NSDL1_VARS(NULL, NULL, "MM: i = %d, var_idx = %d, pt_idx = %d, value_start_ptr = %s, pp_start_val_idx = %d",
                              i, var_idx, pt_idx, loc_variable_table_shr_mem[i].value_start_ptr, pp_start_val_idx);
      continue;
    }

    long shm_offset = (char*)per_proc_fparam_grp_old->p_pointer_table_shr_mem -
                               (char*)per_proc_fparam_grp_old->pointer_table_shr_mem ;
    PointerTableEntry_Shr* value_start_ptr =
                (PointerTableEntry_Shr *)((char*)per_proc_fparam_grp_old->p_variable_table_shr_mem[i].value_start_ptr + shm_offset);

    NSDL1_VARS(NULL, NULL, "Fill pointer table and bigbuff: value_start_ptr = %p, shm_offset = %lld", value_start_ptr, shm_offset);

    for(j = 0; j < per_proc_num_val ; j++, loc_pointer_table_shr_mem++)
    {
      if(j < per_proc_num_val_old )
      {
        NSDL1_VARS(NULL, NULL, "Read Old Data %d",j);
        value = value_start_ptr + pp_start_old + j;
        memcpy(loc_g_big_buf_shr_mem, (value->big_buf_pointer + shm_offset), value->size + 1);
        loc_pointer_table_shr_mem->big_buf_pointer = loc_g_big_buf_shr_mem;
        loc_pointer_table_shr_mem->size = value->size;
      }
      else
      {
        NSDL1_VARS(NULL, NULL, "Read New Data %d",j);
        pt_idx = varTable[var_idx].value_start_ptr + pp_start_new + (j - per_proc_num_val_old);
        memcpy(loc_g_big_buf_shr_mem,
                 nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pt_idx].big_buf_pointer),
                 fparamValueTable[pt_idx].size + 1);
        loc_pointer_table_shr_mem->big_buf_pointer = loc_g_big_buf_shr_mem;
        loc_pointer_table_shr_mem->size = fparamValueTable[pt_idx].size;
      }

      loc_g_big_buf_shr_mem +=  loc_pointer_table_shr_mem->size + 1; //+1 for null added at big buf
      NSDL1_VARS(NULL, NULL, "size = %d, big_buf_pointer = %s",
                              loc_pointer_table_shr_mem->size,
                              loc_pointer_table_shr_mem->big_buf_pointer);
    }

    //Working code
    #if 0
    PointerTableEntry_Shr *value = NULL;
    PerProcVgroupTable *per_proc_fparam_grp_old = &per_proc_vgroup_table[(nvm_id * total_group_entries) + sv_grp_id];
   
    int pp_start_old = per_proc_fparam_grp_old->start_val;
    int pp_start_new =  per_proc_fparam_grp->start_val;
    
    int num_old_val = (mode == APPEND_MODE)?per_proc_fparam_grp_old->total_val:0;  
    int num_new_val = per_proc_fparam_grp->total_val;
  
    long shm_offset = (char*)per_proc_fparam_grp_old->p_pointer_table_shr_mem -
                               (char*)per_proc_fparam_grp_old->pointer_table_shr_mem ;

    PointerTableEntry_Shr* value_start_ptr =
                (PointerTableEntry_Shr *)((char*)per_proc_fparam_grp_old->p_variable_table_shr_mem[var_idx].value_start_ptr + shm_offset);

    NSDL1_VARS(NULL, NULL, "Fill pointer table - pp_start_old = %d, pp_start_new = %d, "
                           "num_old_val = %d, num_new_val = %d, shm_offset = %d, value_start_ptr = %p", 
                            pp_start_old, pp_start_new, num_old_val, num_new_val, shm_offset, value_start_ptr);
    if(shm_offset != 0) 
      pp_start_old = 0;

    for(j = 0; j < per_proc_num_val; j++, loc_pointer_table_shr_mem++)
    {
      //Fill Old Data
      if((mode == APPEND_MODE) && (j < num_old_val))
      {
        NSDL1_VARS(NULL, NULL, "Read Old Data %d",j);
        value = value_start_ptr + pp_start_old + j;   
        memcpy(loc_g_big_buf_shr_mem, (value->big_buf_pointer + shm_offset), value->size + 1);
        loc_pointer_table_shr_mem->big_buf_pointer = loc_g_big_buf_shr_mem;
        loc_pointer_table_shr_mem->size = value->size;
      }
      //Fill New Data
      else
      {
        NSDL1_VARS(NULL, NULL, "Read New Data %d",j);
        pt_idx = varTable[var_idx].value_start_ptr + pp_start_new + (j - num_old_val);
        memcpy(loc_g_big_buf_shr_mem,
                 nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pt_idx].big_buf_pointer),
                 fparamValueTable[pt_idx].size + 1);
        loc_pointer_table_shr_mem->big_buf_pointer = loc_g_big_buf_shr_mem;
        loc_pointer_table_shr_mem->size = fparamValueTable[pt_idx].size;
      }

      loc_g_big_buf_shr_mem +=  loc_pointer_table_shr_mem->size + 1; //+1 for null added at big buf
      NSDL1_VARS(NULL, NULL, "size = %d, big_buf_pointer = %s",
                              loc_pointer_table_shr_mem->size,
                              loc_pointer_table_shr_mem->big_buf_pointer);
    }
    #endif

    #if 0
    for(j = 0; j < per_proc_num_val; j++, loc_pointer_table_shr_mem++)
    {
      pt_idx = varTable[var_idx].value_start_ptr + per_proc_fparam_grp->start_val + j;
      memcpy(loc_g_big_buf_shr_mem, 
               nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pt_idx].big_buf_pointer), 
               fparamValueTable[pt_idx].size + 1); 
      loc_pointer_table_shr_mem->big_buf_pointer = loc_g_big_buf_shr_mem;
      loc_pointer_table_shr_mem->size = fparamValueTable[pt_idx].size; 
      loc_g_big_buf_shr_mem +=  loc_pointer_table_shr_mem->size + 1; //+1 for null added at big buf
      NSDL1_VARS(NULL, NULL, "size = %d, fparamValueTable[%d].big_buf_pointer = %s", 
                              loc_pointer_table_shr_mem->size, pt_idx,
                              nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pt_idx].big_buf_pointer));
    }
    #endif
  }

  /* Fill big buf data */
  //shr_mem_size = loc_g_big_buf_shr_mem - per_proc_fparam_grp->g_big_buf_shr_mem;
  //pt_idx = varTable[groupTable[sv_grp_id].start_var_idx].value_start_ptr; 
  //memcpy(per_proc_fparam_grp->g_big_buf_shr_mem, 
  //       nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pt_idx].big_buf_pointer), shr_mem_size); 

  #if 0
  //Dump for testing 
  VarTableEntry_Shr *var = NULL;
  PointerTableEntry_Shr *value = NULL;
  for(i = 0; i < groupTable[sv_grp_id].num_vars; i++)
  {
    NSDL1_VARS(NULL, NULL, "MM: i = %d, ", i);
    var = &per_proc_fparam_grp->variable_table_shr_mem[i];
    value = var->value_start_ptr;
    for(j = 0; j < per_proc_num_val; j++, value++)
    {
      NSDL1_VARS(NULL, NULL, "j = %d, size = %d, data = %s", 
                                j, value->size, value->big_buf_pointer); 
    }
  }

  /*char *ptr = per_proc_fparam_grp->g_big_buf_shr_mem;
  NSDL1_VARS(NULL, NULL, "shr_mem_size = %d", shr_mem_size);
  for(i=0; i < shr_mem_size; i++, ptr++)
  {
    NSDL1_VARS(NULL, NULL, "MANISH: i = %d, size = %d, ptr = [%c], ptr = %s", i, strlen(ptr), *ptr, ptr?ptr:"NULL");
  }*/
  #endif

  NSDL1_VARS(NULL, NULL, "per_proc_vgroup_table Dump: "
                         "start_val = %d, num_val = %d, total_val = %d, num_script_users = %d, last_file_fd = %d, "
                         "fparam id = %d",
                          per_proc_fparam_grp->start_val, per_proc_fparam_grp->num_val,
                          per_proc_fparam_grp->total_val, per_proc_fparam_grp->num_script_users,
                          per_proc_fparam_grp->last_file_fd, per_proc_fparam_grp->group_table_shr_mem->idx
            );
}

void create_shm_for_static_index_var()
{
  int shr_mem_size = 0;
  void *ns_static_vars_shr_mem = NULL;
  void *ns_static_vars_shr_mem_ptr = NULL;

  NSDL2_VARS(NULL, NULL, "total_weight_entries = %d, total_group_entries = %d, total_var_entries = %d", total_weight_entries, total_group_entries, total_var_entries);
  if(total_weight_entries)
    shr_mem_size += sizeof(WeightTableEntry) * total_weight_entries;
  if(total_group_entries)
    shr_mem_size += sizeof(GroupTableEntry_Shr) * total_group_entries;
  if(total_var_entries)
    shr_mem_size += sizeof(VarTableEntry_Shr) * total_var_entries;

  NSDL2_VARS(NULL, NULL, "Allocate shared memory for Static var of shr_mem_size = %d", shr_mem_size);

  if(shr_mem_size <= 0)
  {
    NSDL2_VARS(NULL, NULL, "Info: not creating shared memory for WeightTableEntry, GroupTableEntry_Shr "
                          "and VarTableEntry_Shr as shr_mem_size = %d", shr_mem_size);
    return;
  }

  //Allocating shared memory
  ns_static_vars_shr_mem = do_shmget(shr_mem_size, "ns_static_vars_shr_mem");
  memset(ns_static_vars_shr_mem, 0, shr_mem_size);

  ns_static_vars_shr_mem_ptr = ns_static_vars_shr_mem;
  NSDL2_VARS(NULL, NULL, "ns_static_vars_shr_mem = %p, ns_static_vars_shr_mem_ptr = %p", ns_static_vars_shr_mem, ns_static_vars_shr_mem_ptr);

  if(total_weight_entries)
  {
    weight_table_shr_mem = ns_static_vars_shr_mem_ptr;
    memcpy(weight_table_shr_mem, weightTable, sizeof(WeightTableEntry) * total_weight_entries);
    ns_static_vars_shr_mem_ptr += sizeof(WeightTableEntry) * total_weight_entries;
  }
  if(total_group_entries)
  { 
    group_table_shr_mem = ns_static_vars_shr_mem_ptr;
    ns_static_vars_shr_mem_ptr += sizeof(GroupTableEntry_Shr) * total_group_entries;
  }
  if(total_var_entries)
  { 
    variable_table_shr_mem = ns_static_vars_shr_mem_ptr;
  }
  
  NSDL2_VARS(NULL, NULL, "group_table_shr_mem = %p, variable_table_shr_mem = %p",
                          group_table_shr_mem, variable_table_shr_mem);
}


/* Manish: from 4.1.6, to support RTC , remove weightTable from here
           For detail refer FileParameterRTC Design document */
void copy_staticvar_into_shared_mem()
{
  int i, sess_idx, hash_code;
  //void *ns_static_vars_shr_mem = NULL;
  //void *ns_static_vars_shr_mem_ptr = NULL;
  //int shr_mem_size = 0;

  NSDL1_VARS(NULL, NULL, "Method called. total_weight_entries = %d and total_group_entries = %d, total_var_entries = %d"
                         "size_of_WeightTableEntry = %d, size_of_GroupTableEntry_Shr = %d, size_of_VarTableEntry_Shr = %d", 
                          total_weight_entries, total_group_entries, total_var_entries, sizeof(WeightTableEntry), 
                          sizeof(GroupTableEntry_Shr), sizeof(VarTableEntry_Shr));

  NSDL2_SCHEDULE(NULL, NULL, "Allocating shared memory for fparamValueTable_shr_mem, total_fparam_entries = %d", total_fparam_entries);
  if(total_fparam_entries)
  {
    char *bb_ptr = NULL;
    NSDL2_SCHEDULE(NULL, NULL, "file_param_value_big_buf: buffer = %p, offset = %d, bufsize = %d", 
                                file_param_value_big_buf.buffer, 
                                file_param_value_big_buf.offset, file_param_value_big_buf.bufsize);

    file_param_value_big_buf_shr_mem = (char*) do_shmget(file_param_value_big_buf.offset, "file_param_value_big_buf_shr_mem");
    fparamValueTable_shr_mem = (PointerTableEntry_Shr*) do_shmget(sizeof(PointerTableEntry_Shr) * total_fparam_entries, "fparamValueTable_shr_mem");

    bb_ptr = file_param_value_big_buf_shr_mem;
    int g,v,pointer_idx;
    for(g = 0; g < total_group_entries; g++)
    {
      /* BugID: 72767 - total_group_entries has both group of static and index var. 
                        Since fparamValueTable_shr_mem is only for static var skipping 
                        index var groups */
      if(groupTable[g].index_var != -1)
        continue;

      for(v = 0; v < groupTable[g].num_vars; v++)
      {
        NSDL4_SCHEDULE(NULL, NULL, "is_file = %d", varTable[groupTable[g].start_var_idx + v].is_file);
        if(varTable[groupTable[g].start_var_idx + v].is_file != IS_FILE_PARAM)
        {
          for(i = 0; i < groupTable[g].num_values; i++)
          {
            pointer_idx = varTable[groupTable[g].start_var_idx + v].value_start_ptr  + i; 
            memcpy(bb_ptr, 
                 nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pointer_idx].big_buf_pointer),
                 fparamValueTable[pointer_idx].size + 1);
            fparamValueTable_shr_mem[pointer_idx].big_buf_pointer = bb_ptr; 
            fparamValueTable_shr_mem[pointer_idx].size = fparamValueTable[pointer_idx].size;
            bb_ptr += fparamValueTable[pointer_idx].size + 1;
            NSDL4_SCHEDULE(NULL, NULL, " pointer-> i = %d, size = %d, value = [%s]", 
                                  i, fparamValueTable_shr_mem[i].size, fparamValueTable_shr_mem[i].big_buf_pointer);
          }
        }
        else
        { 
          for(i = 0; i < groupTable[g].num_values; i++)
          {
            NSDL4_SCHEDULE(NULL, NULL, "pointer-> i = %d, v = [%d], start_var_idx = %d, value_start_ptr = %d", 
                                  i, v, groupTable[g].start_var_idx, varTable[groupTable[g].start_var_idx + v].value_start_ptr);
            pointer_idx = varTable[groupTable[g].start_var_idx + v].value_start_ptr  + i; 
            fparamValueTable_shr_mem[pointer_idx].seg_start = SEG_TABLE_MEMORY_CONVERSION(fparamValueTable[pointer_idx].seg_start); 
            fparamValueTable_shr_mem[pointer_idx].num_entries = fparamValueTable[pointer_idx].num_entries;
          }
        }
      }
    }
  }
  
  #if 0
  if(total_weight_entries)
    shr_mem_size += sizeof(WeightTableEntry) * total_weight_entries;
  if(total_group_entries)
    shr_mem_size += sizeof(GroupTableEntry_Shr) * total_group_entries;
  if(total_var_entries)
    shr_mem_size += sizeof(VarTableEntry_Shr) * total_var_entries;

  NSDL2_VARS(NULL, NULL, "Allocate shared memory for Static var of shr_mem_size = %d", shr_mem_size);

  //Allocating shared memory
  ns_static_vars_shr_mem = do_shmget(shr_mem_size, "ns_static_vars_shr_mem");
  memset(ns_static_vars_shr_mem, 0, shr_mem_size);
  
  ns_static_vars_shr_mem_ptr = ns_static_vars_shr_mem;
  NSDL2_VARS(NULL, NULL, "ns_static_vars_shr_mem = %p, ns_static_vars_shr_mem_ptr = %p", ns_static_vars_shr_mem, ns_static_vars_shr_mem_ptr);

  if(total_weight_entries)
  {
    weight_table_shr_mem = ns_static_vars_shr_mem_ptr;
    memcpy(weight_table_shr_mem, weightTable, sizeof(WeightTableEntry) * total_weight_entries);
    ns_static_vars_shr_mem_ptr += sizeof(WeightTableEntry) * total_weight_entries;
  }
  if(total_group_entries)
  {
    group_table_shr_mem = ns_static_vars_shr_mem_ptr;
    ns_static_vars_shr_mem_ptr += sizeof(GroupTableEntry_Shr) * total_group_entries;
  }
  if(total_var_entries)
  {
    variable_table_shr_mem = ns_static_vars_shr_mem_ptr; 
  }
  
  #endif

  NSDL2_VARS(NULL, NULL, "group_table_shr_mem = %p, variable_table_shr_mem = %p", 
                          group_table_shr_mem, variable_table_shr_mem);
 
  /* Now the group table */
  if (total_group_entries) {
    for (i = 0; i < total_group_entries; i ++) {
      NSDL1_VARS(NULL, NULL, "i = %d,  type = %d, weight_idx = %d, sequence = %d,  index_var = %d", 
                              i, groupTable[i].type, groupTable[i].weight_idx, groupTable[i].sequence, groupTable[i].index_var);
      group_table_shr_mem[i].type = groupTable[i].type;
      group_table_shr_mem[i].sequence = groupTable[i].sequence;
      if ((groupTable[i].weight_idx == -1) && (group_table_shr_mem[i].sequence != UNIQUE))
        group_table_shr_mem[i].group_wei_uni.weight_ptr = NULL;
      else if (group_table_shr_mem[i].sequence == UNIQUE)
        group_table_shr_mem[i].group_wei_uni.unique_group_id = unique_group_id++;
      else
        group_table_shr_mem[i].group_wei_uni.weight_ptr = WEIGHT_TABLE_MEMORY_CONVERSION(groupTable[i].weight_idx);

      group_table_shr_mem[i].num_values = groupTable[i].num_values;

      group_table_shr_mem[i].idx = groupTable[i].idx;
      group_table_shr_mem[i].num_vars = groupTable[i].num_vars;
      group_table_shr_mem[i].sess_name = BIG_BUF_MEMORY_CONVERSION(gSessionTable[groupTable[i].sess_idx].sess_name);
      group_table_shr_mem[i].start_var_idx = groupTable[i].start_var_idx;
      group_table_shr_mem[i].data_fname = BIG_BUF_MEMORY_CONVERSION(groupTable[i].data_fname);
      group_table_shr_mem[i].column_delimiter = BIG_BUF_MEMORY_CONVERSION(groupTable[i].column_delimiter);

      memcpy(group_table_shr_mem[i].encode_chars, groupTable[i].encode_chars, TOTAL_CHARS);
      group_table_shr_mem[i].encode_type = groupTable[i].encode_type; 
      group_table_shr_mem[i].encode_space_by = BIG_BUF_MEMORY_CONVERSION(groupTable[i].encode_space_by);
      group_table_shr_mem[i].UseOnceOptiontype = groupTable[i].UseOnceOptiontype;
      group_table_shr_mem[i].UseOnceAbort = groupTable[i].UseOnceAbort;
      group_table_shr_mem[i].first_data_line = groupTable[i].first_data_line;
      group_table_shr_mem[i].absolute_path_flag = groupTable[i].absolute_path_flag;
      group_table_shr_mem[i].sql_or_cmd_var = groupTable[i].sql_or_cmd_var;
      group_table_shr_mem[i].sess_idx = groupTable[i].sess_idx;

      //TODO: understand what is going on ......
      /* Following things are only for INDEX_VAR */
      if(groupTable[i].index_var  != -1) {
        int index_key_var_hash_idx, index_key_var_type;
        char *index_var = BIG_BUF_MEMORY_CONVERSION(groupTable[i].index_var);
	if (gSessionTable[groupTable[i].sess_idx].var_hash_func) {
	  index_key_var_hash_idx = gSessionTable[groupTable[i].sess_idx].var_hash_func(index_var, strlen(index_var));
	} else {
	  index_key_var_hash_idx = -1;
	}
        if(index_key_var_hash_idx == -1) {
          NS_EXIT(-1, CAV_ERR_1031036, index_var, group_table_shr_mem[i].sess_name);
        }
        group_table_shr_mem[i].index_key_var_idx = 
                               gSessionTable[groupTable[i].sess_idx].vars_trans_table_shr_mem[index_key_var_hash_idx].user_var_table_idx;
        index_key_var_type = gSessionTable[groupTable[i].sess_idx].vars_trans_table_shr_mem[index_key_var_hash_idx].var_type;
        switch (index_key_var_type) {
         case VAR:
         case COOKIE_VAR:
           NS_EXIT(-1, CAV_ERR_1031037, index_var, group_table_shr_mem[i].sess_name);
        }
        group_table_shr_mem[i].idx_var_hash_func = groupTable[i].idx_var_hash_func;
      }
      else
      {
        //Handle core dump while copying local index param memory into shared memory bug id 83724
        strcpy(group_table_shr_mem[i].UseOnceWithinTest, RETRIEVE_BUFFER_DATA(groupTable[i].UseOnceWithinTest));
        group_table_shr_mem[i].index_key_var_idx = -1;
      }

      group_table_shr_mem[i].max_column_index = groupTable[i].max_column_index; 
      group_table_shr_mem[i].ignore_invalid_line = groupTable[i].ignore_invalid_line; 
    }
  }

  /* Next goes the variable table */
  if (total_var_entries) {
    for (i = 0; i < total_var_entries; i++) {
      variable_table_shr_mem[i].group_ptr = GROUP_TABLE_MEMORY_CONVERSION(varTable[i].group_idx);
      NSDL1_VARS(NULL, NULL, "i = %d, value_start_ptr = %d", i, varTable[i].value_start_ptr);
      variable_table_shr_mem[i].value_start_ptr = FPARAMVALUE_TABLE_MEMORY_CONVERSION(varTable[i].value_start_ptr);
      variable_table_shr_mem[i].name_pointer = BIG_BUF_MEMORY_CONVERSION(varTable[i].name_pointer);
      variable_table_shr_mem[i].var_size = varTable[i].var_size;
      variable_table_shr_mem[i].is_file = varTable[i].is_file;
      variable_table_shr_mem[i].column_idx = varTable[i].column_idx;
      sess_idx = groupTable[varTable[i].group_idx].sess_idx;
      variable_table_shr_mem[i].column_idx = varTable[i].column_idx;
      hash_code = gSessionTable[sess_idx].var_hash_func(variable_table_shr_mem[i].name_pointer, strlen(variable_table_shr_mem[i].name_pointer));
      variable_table_shr_mem[i].uvtable_idx = gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].user_var_table_idx;;
    }
  }
}


//parsing of file 
int kw_save_nvm_file_param_val(char *buf, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[10];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num_args;
  int mode = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = [%s].", buf);
  
  num_args = sscanf(buf, "%s %s %s", keyword, mode_str, tmp);

  NSDL2_PARSING(NULL, NULL, "num_args= %d , key=[%s], mode=[%s]", num_args, keyword, mode_str);

  if(num_args != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SAVE_NVM_FILE_PARAM_VAL_USAGE, CAV_ERR_1011161, CAV_ERR_MSG_1);
  }

  if(!ns_is_numeric(mode_str))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SAVE_NVM_FILE_PARAM_VAL_USAGE, CAV_ERR_1011161, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SAVE_NVM_FILE_PARAM_VAL_USAGE, CAV_ERR_1011161, CAV_ERR_MSG_3);
  }

  global_settings->save_nvm_file_param_val = mode;

  NSDL2_VARS(NULL, NULL, "global_settings->nvm_file_param_val = %d", global_settings->save_nvm_file_param_val);
  return 0;
}

