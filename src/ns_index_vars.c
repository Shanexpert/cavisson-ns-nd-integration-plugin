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
#include <libgen.h>

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
#include "ns_string.h"
#include "ns_index_vars.h"
#include "nslb_util.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "nslb_hash_code.h"
#include "ns_exit.h"

#ifndef CAV_MAIN
VarTableEntry *indexVarTable;
int max_index_var_entries = 0;
int total_index_var_entries = 0;
#else
__thread VarTableEntry *indexVarTable;
__thread int max_index_var_entries = 0;
//__thread int total_index_var_entries = 0;
#endif


static void
file_to_buffer(char **fbuf, char *file, int size)
{ 
  FILE *fp;
  MY_MALLOC(*fbuf, size, "fbuf", -1);
  fp = fopen(file, "r");
  fread(*fbuf, size, 1, fp);
  fclose(fp);
  NSDL4_VARS(NULL, NULL, "file = %s, fbuf = %*.*s\n", file, size, size, *fbuf); 
}

void copy_indexvar_into_shared_mem (){
 int i;
 if (total_index_var_entries) {
    index_variable_table_shr_mem = (VarTableEntry_Shr*) do_shmget(sizeof(VarTableEntry_Shr) * total_index_var_entries, "Variable Table");
    for (i = 0; i < total_index_var_entries; i++) {
      index_variable_table_shr_mem[i].group_ptr = GROUP_TABLE_MEMORY_CONVERSION(indexVarTable[i].group_idx);
      index_variable_table_shr_mem[i].value_start_ptr = POINTER_TABLE_MEMORY_CONVERSION(indexVarTable[i].value_start_ptr);  
      index_variable_table_shr_mem[i].name_pointer = BIG_BUF_MEMORY_CONVERSION(indexVarTable[i].name_pointer);
      index_variable_table_shr_mem[i].var_size = indexVarTable[i].var_size;
    }
  }
}

#if 0
static int generate_hash_table(char *file_name, char *hash_fun_name, Str_to_hash_code *str_to_hash_code_fun, Hash_code_to_str *hash_to_str_fun, int debug, char *base_dir) {
  char fname[MAX_LINE_LENGTH];
  char get_key[MAX_LINE_LENGTH];
  int total_vars_trans_entries;
  FILE *read_file, *write_file;
  char buffer[MAX_LINE_LENGTH];
  char cmd_buffer[MAX_LINE_LENGTH];
  char file_buffer[MAX_FILE_NAME];
  char *buf_ptr;
  void* handle;
  char* error;

  NSDL2_VARS(NULL,NULL, "Method called. URL file name = %s, hash_fun_name = %s", file_name, hash_fun_name);
  //For function name in hash file
  sprintf(get_key, "%s_get_key", hash_fun_name);
  //Create perfect hash code for the list of variables

  // Added by Archana 01May08 
  // We were using "-k *" option before 2.3.2 release. 
  // This was causing gperf to take 20 minutes or so for hash code generation in some cases
  // To fix this issue, we removed -k option in 2.3.2.
  //  We did bench mark for this change:
  //     Time taken for getting hash code for 480K url is 152 ms and before it was ?? ms 
  //     URL Hit rate before and after is same (96K)
  //sprintf(fname, "gperf -k \"*\" -c -C -G %s/.tmp/%s -N %s > %s/.tmp/%s.c", base_dir, file_name, hash_fun_name, base_dir, hash_fun_name);
  sprintf(fname, "gperf -c -C -G %s/%s -N %s > %s/%s.c", base_dir, file_name, hash_fun_name, base_dir, hash_fun_name);

  NSDL2_VARS(NULL,NULL, "Command for calling gperf = %s ", fname);
  
  if (system(fname) == -1)
  {
    NSDL2_VARS(NULL, NULL, "Error in calling the gperf");
    exit (-1);
  }

  //Transform the generated file.
  sprintf(fname, "%s/%s.c", base_dir, hash_fun_name);
  if ((read_file = fopen(fname, "r")) == NULL)
  {
    NSDL2_VARS(NULL, NULL, "Error in opening %s.c file", hash_fun_name);
    exit (-1);
  }

  sprintf(fname, "%s/%s_write.c", base_dir, hash_fun_name);
  if ((write_file = fopen(fname, "w+")) == NULL)
  {
    NSDL2_VARS(NULL, NULL, "Error in opening %s_write.c file", hash_fun_name);
    exit (-1);
  }

  /************************
  We give the dublicate values in file and make the hash code for that file. When we call the gerf for making the hash code then it shows some errors while it should show the error messages for duplicate values as we done later in this code. On making hash code gperf shows following error


  Key link: "License_No" = "License_No", with key set "LN_ceeinos".
  1 input keys have identical hash values,
  try different key positions or use option -D.
  /var/www3/hpd/.tmp/cr_search_var_hash_code_151_write.c: In function âcr_search_var_hash_code_151_get_keyâ:
  /var/www3/hpd/.tmp/cr_search_var_hash_code_151_write.c:4: error: âMAX_HASH_VALUEâ undeclared (first use in this function)
  /var/www3/hpd/.tmp/cr_search_var_hash_code_151_write.c:4: error: (Each undeclared identifier is reported only once
  /var/www3/hpd/.tmp/cr_search_var_hash_code_151_write.c:4: error: for each function it appears in.)
  /var/www3/hpd/.tmp/cr_search_var_hash_code_151_write.c:5: error: âwordlistâ undeclared (first use in this function)

  This comes when we want hash code for some variable
  Error:hashcode for License_No is out of range (10)


  ************************/


  //Get from hashcode source file - max value oh hash code, check if any duplicate detected by
  //has code genertion,
  //make some changes into hash code src and generate other file with modifications.
  // There is a function genererated that takes an string and retuns the same string if it
  // it is in the hash table, else return 0. This is transformed into another function
  // that takens an string and returns hash code or -1.
  // Also add another fun. that takes a key and returns its string or "" if not valid key.
  char tmp_buf[1024];
  int do_replace = 0;
  sprintf(tmp_buf, "%s (str, len)", hash_fun_name);

  while (nslb_fgets(buffer, MAX_LINE_LENGTH, read_file, 0)) {
    if ((buf_ptr = strstr(buffer, "#define MAX_HASH_VALUE"))) {
      buf_ptr += strlen("#define MAX_HASH_VALUE ");
      total_vars_trans_entries = atoi(buf_ptr) + 1;
    } else if ((buf_ptr = strstr(buffer, "duplicates = "))) {
      buf_ptr += strlen("duplicates = ");
      if (atoi(buf_ptr) != 0) {
        NSDL2_VARS(NULL, NULL, "duplicates in the %s_write.c file", hash_fun_name);
        exit (-1);
      }
    } else if ((buf_ptr = strstr(buffer, "return s"))) {
      strcpy(buf_ptr, "return key;\n");
    } else if ((buf_ptr = strstr(buffer, "return 0")) && do_replace) {
      strcpy(buf_ptr, "return -1;\n");
      do_replace = 0;
    } else if (!strncasecmp(buffer, "const char *", strlen("const char *"))) {
      strcpy(buffer, "int\n");
    } else if (!strncmp(buffer, tmp_buf, strlen(tmp_buf))) {
      do_replace = 1;
    }

    if (fputs(buffer, write_file) < 0) {
      NSDL2_VARS(NULL, NULL, "Error in writing line into %s_write.c file", hash_fun_name );
      exit (-1);
    }
  }

  fprintf(write_file, "\nconst char*\n%s(int hash_code) {\nif ((hash_code <= MAX_HASH_VALUE ) && (hash_code >= 0))\nreturn wordlist[hash_code];\nelse\nreturn \"\";\n}\n", get_key);

  //fputs("\nconst char*\nvar_get_key(int hash_code) {\nif ((hash_code <= MAX_HASH_VALUE ) && (hash_code >= 0))\nreturn wordlist[hash_code];\nelse\nreturn \"\";\n}\n", write_file);

  fclose(read_file);
  fclose(write_file);

  /* compile and link hash_page_write.c */
  sprintf(cmd_buffer, "gcc -g -m%d -fpic -shared -o %s/%s_write.so %s/%s_write.c",
                       NS_BUILD_BITS, base_dir, hash_fun_name, base_dir, hash_fun_name);

  if (system(cmd_buffer) == -1) {
    NSDL2_VARS(NULL, NULL, "Error in calling gcc");
    exit (-1);
  }

  if (!getcwd(file_buffer, MAX_FILE_NAME)) {
    NSDL2_VARS(NULL, NULL, "Error in getting pwd");
    exit (-1);
  }

  sprintf(file_buffer, "%s/%s_write.so", base_dir, hash_fun_name);
  NSDL2_VARS(NULL,NULL, "File for handler = %s", file_buffer);

  handle = dlopen (file_buffer, RTLD_LAZY);
  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    //NSDL2_VARS(NULL, NULL, "%s", error);
    exit (-1);
  }

  //var_hash_func = dlsym(handle, "hash_variables");
  *str_to_hash_code_fun = dlsym(handle, hash_fun_name);

  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    NSDL2_VARS(NULL, NULL, "%s", error);
    exit (-1);
  }

  *hash_to_str_fun = dlsym(handle, get_key);

  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    NSDL2_VARS(NULL, NULL, "%s", error);
    exit (-1);
  }

  NSDL2_VARS(NULL,NULL, "Returning from generate_hash_table function, total entries in hash table = %d, hash func = %p", total_vars_trans_entries, *hash_to_str_fun);
  return total_vars_trans_entries;
}
#endif

int
input_index_values(char* file_name, int group_idx, int sess_idx, int var_start) {

  FILE* index_val_fp;
  //char path_name[MAX_LINE_LENGTH];
  char line[MAX_LINE_LENGTH];
  //int total_weight = 0;
  int line_number = 0;
  int num_values = 0;
  int i, j;
  int var_idx;
  int rnum;
  int val_idx;
  int first_tok;
  char* tok;
  //int weight;
  int pointer_idx;
  //int num;
  int var_size;

  /* index file */
  char idx_file_name[4098];
  FILE *idx_fd;
  Hash_code_to_str_ex var_get_key;
  int num_index_var_hash_codes;
  int hash_idx;
  char hash_code_func_name[4098];
  char column_delim[16];

  strcpy(column_delim, RETRIEVE_BUFFER_DATA(groupTable[group_idx].column_delimiter));

  NSDL2_VARS(NULL,NULL, "Method called. file_name = %s, group_idx = %d, var_start = %d, column_delim = <%s>",
                         file_name, group_idx, var_start, column_delim);

  if ((index_val_fp = fopen(file_name, "r")) == NULL) {
    fprintf(stderr, "Error in opening file %s\n", file_name);
    perror("fopen");
    return -1;
  }

  //Save Input value file name
  if ((groupTable[group_idx].data_fname = copy_into_big_buf(file_name, 0)) == -1) {
    NS_EXIT(-1, "Failed in copying input file name into big buf");
  }

  /* create index hash out of first field in the file. */
  sprintf(idx_file_name, "%s/index_file_param_%d_%d.txt", g_ns_tmpdir, sess_idx, group_idx);

  idx_fd = fopen(idx_file_name, "w");
  if(idx_fd == NULL) {
    NS_EXIT(-1, "Failed to create %s", idx_file_name);
  }

  //groupTable[group_idx].start_var_idx = var_start;

  /*first need to find out the number of total values per variable to preallocate the space on the tables */
  
  int ignore_line_flag = 0;
  int ignore_line_number = 0;  

  char *is_second_tok_exist;
  while (nslb_fgets(line, MAX_LINE_LENGTH, index_val_fp, 0)) {

    NSDL2_VARS(NULL,NULL, "Line for parsing = %s, line_number = %d", line, line_number);
   
    //Ignoring the blank data line
    ignore_line_number++;
    SET_IGNORE_LINE_FLAG(line);
    NSDL4_VARS(NULL, NULL, "ignore_line_flag = %d", ignore_line_flag);
    //printf("**************ignore_line_flag = %d\n", ignore_line_flag);
    if(ignore_line_flag == 1)
    {
      NSDL3_VARS(NULL, NULL, "Ignoring the blank line %d from data file", ignore_line_number);
      ignore_line_flag = 0;
      continue;
    }

    line_number++;
    if(line_number < groupTable[group_idx].first_data_line)
    {
      NSDL2_VARS(NULL,NULL, "This line is header line, Continuing");
      continue;
    }

    /* copy index for hash here. */
    /* Bug fixed : 3667
     * strtok was inserting one blank line after each index in the file(.tmp/netstorm/ns-inst0/index_file_param_0_0.txt).
     * due to which generate_hash_table function gets failed.
     * In order to handle this situation, we have added one more strtok() i.e. is_second_tok_exist = strtok(NULL, column_delim);*/  
     
    NSDL2_VARS(NULL,NULL, "column delimiter = %s", column_delim);
    tok = strtok(line, column_delim);
    is_second_tok_exist = strtok(NULL, column_delim);
    NSDL2_VARS(NULL,NULL, "tok = %s, is_second_tok_exist = %s", tok, is_second_tok_exist);
    if(is_second_tok_exist == NULL)
      fprintf(idx_fd, "%s", tok);
    else
      fprintf(idx_fd, "%s\n", tok);
    num_values++;
  }

  NSDL2_VARS(NULL,NULL, "Total num_values = %d", num_values);
  if (line_number < groupTable[group_idx].first_data_line) {
    fprintf(stderr, "First data line '%d' is must be less than number of data records '%d' of data file '%s'\n", 
                     groupTable[group_idx].first_data_line, line_number, file_name);
    return -1;
  }

  if (num_values == 0) {
    fprintf(stderr, "File %s is empty or values are ignored due to first data line\n", file_name);
    return -1;
  }
  
  fclose(idx_fd);

  // Make hash code function name unique using URL hash code
  sprintf(hash_code_func_name, "index_var_hash_code_%d_%d", sess_idx, group_idx);

  //commented static function generate_hash_table, used library method generate_hash_table_ex instead 
/*  num_index_var_hash_codes = generate_hash_table(basename(idx_file_name), hash_code_func_name, 
                                  &(groupTable[group_idx].idx_var_hash_func), 
                                  &var_get_key, 
                                  1, g_ns_tmpdir);*/

  num_index_var_hash_codes = generate_hash_table_ex(basename(idx_file_name), hash_code_func_name, 
                                  &(groupTable[group_idx].idx_var_hash_func), 
                                  &var_get_key, NULL, NULL,NULL, 
                                  1, g_ns_tmpdir);

  groupTable[group_idx].num_values = num_values;

  NSDL2_VARS(NULL,NULL, "Making pointer table for index var: group_idx = %d, num_values = %d, num_vars = %d, "
                        "num_index_var_hash_codes = %d", 
                         group_idx, num_values, groupTable[group_idx].num_vars, num_index_var_hash_codes); 

  for (i = 0, var_idx = var_start; i < groupTable[group_idx].num_vars; i++, var_idx++) {
    for (j = 0; j < num_index_var_hash_codes; j++) {
      if (create_pointer_table_entry(&rnum) != SUCCESS) {
	fprintf(stderr, "error in creating pointer table entry\n");
	return -1;
      }
      if (j == 0) {
	indexVarTable[var_idx].value_start_ptr = rnum;
      }
    }
  }

  // Read again file to fill pointer table
  rewind(index_val_fp);

  val_idx = 0;
  line_number = 0;
  while (nslb_fgets(line, MAX_LINE_LENGTH, index_val_fp, 0)) {

    NSDL2_VARS(NULL,NULL, "Line = %s, line number = %d", line, line_number);
    
    //Ignoring the blank data line
    SET_IGNORE_LINE_FLAG(line);
    NSDL4_VARS(NULL, NULL, "ignore_line_flag = %d", ignore_line_flag);
    //printf("**************ignore_line_flag = %d\n", ignore_line_flag);
    if(ignore_line_flag == 1)
    {
      NSDL3_VARS(NULL, NULL, "Ignoring the blank line %d from data file", ignore_line_number);
      ignore_line_flag = 0;
      continue;
    }

    if (strchr(line, '\n'))
      *(strchr(line, '\n')) = '\0';

    line_number++;
    // Ignore till FirstData Line comes
    if(line_number < groupTable[group_idx].first_data_line)
    {
      NSDL2_VARS(NULL,NULL, "This line is header line, Continuing");
      continue;
    }
    
    first_tok = 1;
    var_idx = 0;
    hash_idx = -1;

    for (;;) {
      if (first_tok) {
	tok = ns_strtok(line, column_delim);
	if (!tok) {
	  fprintf(stderr, "Error: At line %d, invalid value format, values must be seaparted by <%s>\n", line_number, column_delim);
	  return -1;
	}
	first_tok = 0;

        hash_idx = (groupTable[group_idx].idx_var_hash_func)(tok, strlen(tok));
        continue;
      } else {
	tok = ns_strtok(NULL, column_delim);
	if (!tok) {
	  if (var_idx < groupTable[group_idx].num_vars) {
	    fprintf(stderr, "Error: At line %d, invalid value format. %d values provided. Must have %d values \n", line_number, var_idx ,groupTable[group_idx].num_vars);
	    return -1;
          } else
	    break;
	} else if (var_idx >= groupTable[group_idx].num_vars) {
	  fprintf(stderr, "Error: At line %d, invalid value format. %d or more values provided. Must have %d values \n", line_number, var_idx+1 ,groupTable[group_idx].num_vars);
	  return -1;
	}
      }

      pointer_idx = indexVarTable[var_start+var_idx].value_start_ptr + hash_idx;
      NSDL2_VARS(NULL,NULL, "IndexVar = %s, hash_idx = %d, pointer_idx = %d", tok, hash_idx, pointer_idx);

      //If var has size specification, make sure no value of this var is more than specified size
      if (indexVarTable[var_start+var_idx].var_size > 0) {
	var_size = indexVarTable[var_start+var_idx].var_size;
	if (var_size < strlen(tok)) {
		 // indexVarTable[var_start+var_idx].name_pointer, var_size, tok);
	  NS_EXIT(-1, "%s var has specified max size of %d, canot have larger sized value %s", RETRIEVE_BUFFER_DATA(indexVarTable[var_start+var_idx].name_pointer), var_size, tok);
	}
      } else {
	var_size = 0;
      }

      struct stat stat_buf;
      int is_file = 0;
      char var_value_file[MAX_LINE_LENGTH];

      NSDL2_VARS(NULL,NULL, "is_file = %d,  tok = %s, hash_idx = %d\n", indexVarTable[var_start+var_idx].is_file, tok, hash_idx);
      if (indexVarTable[var_start+var_idx].is_file) {
          snprintf(var_value_file, MAX_LINE_LENGTH, "%s", tok);
        if (lstat(var_value_file, &stat_buf) == -1) {
          NSDL2_VARS(NULL,NULL, "File %s does not exists\n", var_value_file);
          NS_EXIT(-1, "File %s does not exists. Exiting.", var_value_file);
          //is_file = 0;
        } else {
          if (stat_buf.st_size == 0) {
            NS_EXIT(-1, "File %s is of zero size. Exiting.", var_value_file);
          }
          is_file = 1;
        }
      }

      if (is_file) {
        //      if (indexVarTable[var_start+var_idx].is_file) {
        char *fbuf = NULL;
        NSDL2_VARS(NULL,NULL, "is_file = %d", indexVarTable[var_start+var_idx].is_file);
        file_to_buffer(&fbuf, var_value_file, stat_buf.st_size);
        if ((pointerTable[pointer_idx].big_buf_pointer = copy_into_big_buf(fbuf, stat_buf.st_size)) == -1) {
          NS_EXIT(-1, "Failed in copying data into big buf");
        }
        //FREE_AND_MAKE_NOT_NULL(fbuf, "fbuf", -1);
        FREE_AND_MAKE_NULL_EX(fbuf, stat_buf.st_size, "fbuf", -1);
      } else {
        NSDL2_VARS(NULL,NULL, "is_file = %d", indexVarTable[var_start+var_idx].is_file);
        if ((pointerTable[pointer_idx].big_buf_pointer = copy_into_big_buf(tok, var_size)) == -1) {
          NS_EXIT(-1, "Failed in copying data into big buf");
        }
        pointerTable[pointer_idx].size = strlen(tok);
        NSDL2_VARS(NULL,NULL, "pointer_idx = %d, size = %d", pointer_idx, pointerTable[pointer_idx].size);
      }
      var_idx++;
    }

    val_idx++;
  }

  fclose(index_val_fp);
  return 0;
}

int create_index_var_table_entry(int *row_num) 
{
  NSDL2_VARS(NULL,NULL, "Method called.");
  if (total_index_var_entries == max_index_var_entries) {
    //MY_REALLOC(indexVarTable, (max_index_var_entries + DELTA_VAR_ENTRIES) * sizeof(VarTableEntry), "variable entries", -1);
    MY_REALLOC_EX(indexVarTable, (max_index_var_entries + DELTA_VAR_ENTRIES) * sizeof(VarTableEntry), (max_index_var_entries) *sizeof(VarTableEntry), "variable entries", -1);
    max_index_var_entries += DELTA_VAR_ENTRIES;
  }
  *row_num = total_index_var_entries++;
  indexVarTable[*row_num].server_base = -1;
  return (SUCCESS);
}


#define CHK_GET_ARGUMENT_VALUE(ArgName) \
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

/* Example format:
 * nsl_index_file_var (unique_no, FILE=/tmp/jvm, IndexVar=search1, FirstDataLine=1, ColumnDelimiter=,, HeaderLine=0, VAR_VALUE=F2=file);
*/
int
input_indexvar_data(char *line, int line_number, int sess_idx, char *script_filename) {
#define NS_ST_STAT_VAR_NAME 1
#define NS_ST_STAT_VAR_OPTIONS 2
#define NS_ST_STAT_VAR_NAME_OR_OPTIONS 3
  int state = NS_ST_STAT_VAR_NAME;
  char indexvar_buf[MAX_FIELD_LENGTH];
  int create_group = 1;
  int group_idx = -1;
  int var_idx;
  char* line_tok;
  //int scope = -1;
  //int mode = -1;
  char indexvar_file[MAX_FIELD_LENGTH];
  char indexvar_file_orig[MAX_FIELD_LENGTH];
  char indexvar_name[MAX_FIELD_LENGTH];
  char indexvar_datadir[MAX_UNIX_FILE_NAME+1] = {0};
  int file_done = 0;
  int indexvar_name_done = 0;
  int var_start = -1;
  int i;
  char absol_var[MAX_FIELD_LENGTH];
  char script_file_msg[MAX_FIELD_LENGTH];
  DBT key, data;
  char hash_data[8]; 
  char *size_ptr;
  int ret_val; 
  //int rw_data = 0;

  char err_msg[4096];
  struct stat s;
  int file_found = 0; 
  char FirstDataLine[10];
  char column_delimiter[10];

  /*For encode*/
  int encode_flag_specified = 0;
  char EncodeSpaceBy[16];
  char encode_chars_done = 0;
  char char_to_encode_buf[1024];
  int buf_chk_flag = 0;

  NSDL2_VARS(NULL, NULL, "Method called. line = %s", line);

  //Set default values
  strcpy(column_delimiter, ",");
  strcpy(FirstDataLine, "1");
  strcpy(EncodeSpaceBy, "+");

  indexvar_file[0] = '\0';
  sprintf(script_file_msg, "Parsing nsl_index_file_var() decalaration in file (scripts/%s/%s) at line number %d : ", 
	                    get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), 
	                    script_filename, line_number);
          //Previously taking with only script name
	  //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), script_filename, line_number);


  for (line_tok = strtok(line, ","); line_tok; line_tok = strtok(NULL, ",")) {
    CLEAR_WHITE_SPACE(line_tok);

    NSDL2_VARS(NULL, NULL, "line_tok = %s", line_tok);
    //fprintf(stderr, "At Line = %d, line_tok = %s\n", __LINE__, line_tok);
     
    if(line_tok == NULL) {
      continue;
    }
    else if (!strncasecmp(line_tok, "DATADIR", strlen("DATADIR"))) {
      line_tok += strlen("DATADIR");
      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      CLEAR_WHITE_SPACE(line_tok);

      NSDL2_VARS(NULL, NULL, "DATADIR Name = %s", line_tok);
      for (i = 0; *line_tok && (*line_tok != ' ') && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
        indexvar_datadir[i] = *line_tok;
      }
      indexvar_datadir[i] = '\0';
      validate_and_copy_datadir(2, indexvar_datadir, NULL, err_msg);
      NSDL2_VARS(NULL, NULL, "After tokenized DATADIR Name = %s", indexvar_datadir);
      if (indexvar_datadir[0] == '\0') 
        strcpy(indexvar_datadir, "0"); //To indicate that data dir is applied in this API
    }
    else if (!strncasecmp(line_tok, "FILE", strlen("FILE"))) {
      if (state == NS_ST_STAT_VAR_NAME) {
	fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
	return -1;
      }

      if (group_idx == -1) {
	fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg);
	return -1;
      }

      if (file_done) {
	fprintf(stderr, "%s can only have one file per declaration\n", script_file_msg);
	return -1;
      }

      line_tok += strlen("FILE");
      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      CLEAR_WHITE_SPACE(line_tok);

      NSDL2_VARS(NULL, NULL, "File Name = %s", line_tok);
      for (i = 0; *line_tok && (*line_tok != ' ') && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
	indexvar_file_orig[i] = *line_tok;
      }
      indexvar_file_orig[i] = '\0';
      NSDL2_VARS(NULL, NULL, "After tokenized File Name = %s", indexvar_file_orig);

      if(indexvar_file_orig[0] == '/') {  // absolute path given
        strcpy(indexvar_file, indexvar_file_orig);
      }
      else
      {
        int grp_idx = -1;
        if(*indexvar_datadir != '\0')
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
        if ((grp_idx != -1) && *(runProfTable[grp_idx].gset.data_dir)) {
          sprintf(indexvar_file, "%s/data/%s/%s",
                               GET_NS_TA_DIR(),
                               runProfTable[grp_idx].gset.data_dir,
                               indexvar_file_orig);
          if(stat(indexvar_file, &s) == 0) {
            NSDL2_VARS(NULL, NULL, "File '%s' is present.", indexvar_file);
            file_found = 1;
          } else
              NSDL2_VARS(NULL, NULL, "File '%s' is not present.", indexvar_file);
        }

        if ((file_found == 0) && (*indexvar_datadir != '\0'))
        {
        //DATADIR is fetched from file parameter API argument DATADIR
        /* GET_NS_TA_DIR=$NS_WDIR/workspace/$WORKSPACE_NAME/$PROFILE_NAME/cavisson/ */
          sprintf(indexvar_file, "%s/data/%s/%s",
                                 GET_NS_TA_DIR(),
                                 indexvar_datadir,
                                 indexvar_file_orig);
          if(stat(indexvar_file, &s) == 0) {
            NSDL2_VARS(NULL, NULL, "File '%s' is present.", indexvar_file);
            file_found = 1;
          } else
            NSDL2_VARS(NULL, NULL, "File '%s' is not present.", indexvar_file);
        }

        if(file_found == 0)
          sprintf(indexvar_file, "%s/%s/%s",
                             GET_NS_TA_DIR(),
                             get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"),
                             indexvar_file_orig);

      }


      file_done = 1;
      state = NS_ST_STAT_VAR_OPTIONS;
      NSDL2_VARS(NULL, NULL, "File Name = %s", indexvar_file);
      write_log_file(NS_SCENARIO_PARSING, "Index Data File Name with data directory = %s",indexvar_file);
      NSTL1_OUT(NULL, NULL, "Index Data File Name = %s\n", indexvar_file);
      continue;
    } 
    /* Format example: VAR_VALUE=F1=file;F2=value */
    else if (!strncasecmp(line_tok, "VAR_VALUE", strlen("VAR_VALUE"))) {
      NSDL2_VARS(NULL, NULL, "line_tok = %s", line_tok);
      char *ptr, *vv, *start;
      int field;
      char *type;
      ptr = line_tok + strlen("VAR_VALUE");

      for(;ptr[0] != '\0'; ptr++) if (ptr[0] == '=') break; /* Reach '='  */
      ptr++;
      /* Eat white space */
      while(ptr[0] == ' ') ptr++;
      
      while (*ptr != '\0') {  /* Extract individual var values */
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
          if (strcmp(type, "file") == 0) {
            if (field <= groupTable[group_idx].num_vars && field >= 0) {
              /*
              int nv = 0;
              for (i = 0; i < group_idx; i++) {
                nv = nv + groupTable[i].num_vars;
              }
            
              NSDL2_VARS(NULL, NULL, "Setting %d to is_file\n", nv + field);
              indexVarTable[nv + field - 1].is_file = 1;*/
              int var_id = groupTable[group_idx].start_var_idx + field - 1;
              NSDL2_VARS(NULL, NULL, "Setting flag is_file: group_idx = %d, start_var_idx = %d, field = %d, var_idx = %d, var = %s", 
                                  group_idx, groupTable[group_idx].start_var_idx, field, var_id, 
                                  RETRIEVE_BUFFER_DATA(varTable[var_id].name_pointer));
              indexVarTable[var_id].is_file = 1;
            } else {
              NSDL2_VARS(NULL, NULL, "Invalid VAR_VALUE %s", start);
              NS_EXIT(-1, "Invalid VAR_VALUE %s\n", start);
            }
          } else if (strcmp(type, "value") != 0) {
            NS_EXIT(-1, "Invalid type %s specified.\n", type);
          }
        } else {
          NS_EXIT(-1, "Unrecognized VAR_VALUE (%s)\n", start);
        }
      }

      state = NS_ST_STAT_VAR_OPTIONS;
    } else if (!strncasecmp(line_tok, "IndexVar", strlen("IndexVar"))) {
      if (state == NS_ST_STAT_VAR_NAME) {
	fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
	return -1;
      }

      if (group_idx == -1) {
	fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg);
	return -1;
      }

      line_tok += strlen("IndexVar");
      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      CLEAR_WHITE_SPACE(line_tok);

      NSDL2_VARS(NULL, NULL, "index var name = %s", line_tok);
      for (i = 0; *line_tok && (*line_tok != ' ') && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
	indexvar_name[i] = *line_tok;
      }
      indexvar_name[i] = '\0';
      NSDL2_VARS(NULL, NULL, "After tokenized index var name = %s", indexvar_name);

      indexvar_name_done = 1;
      continue;
    }else if (!strncasecmp(line_tok, "ColumnDelimiter", strlen("ColumnDelimiter"))) {
      if (state == NS_ST_STAT_VAR_NAME) {
        fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
        return -1;
      }

      if (group_idx == -1) {
        fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg);
        return -1;
      }

      line_tok += strlen("ColumnDelimiter");
      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      CLEAR_WHITE_SPACE(line_tok);
      NSDL2_VARS(NULL, NULL, "ColumnDelimiter = %s", line_tok);
      if(*line_tok == '\0')
      {
        column_delimiter[0] = ',';
        column_delimiter[1] = '\0';
      }
      else
      {
        for (i = 0; *line_tok && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
          column_delimiter[i] = *line_tok;
        }
        column_delimiter[i] = '\0';
      }
      NSDL2_VARS(NULL, NULL, "After tokenized column delimiter = %s", column_delimiter);
      continue;
    }else if (!strncasecmp(line_tok, "FirstDataLine", strlen("FirstDataLine"))) {
      if (state == NS_ST_STAT_VAR_NAME) {
        fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
        return -1;
      }

      if (group_idx == -1) {
        fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg);
        return -1;
      }

      line_tok += strlen("FirstDataLine");
      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      CLEAR_WHITE_SPACE(line_tok);

      for (i = 0; *line_tok && (*line_tok != ' ') && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
        FirstDataLine[i] = *line_tok;
      }
      FirstDataLine[i] = '\0';
      NSDL2_VARS(NULL, NULL, "After tokenized FirstDataLine = %s", FirstDataLine);

      continue;
    }else if (!strncasecmp(line_tok, "HeaderLine", strlen("HeaderLine"))) {
      if (state == NS_ST_STAT_VAR_NAME) {
        fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
        return -1;
      }

      if (group_idx == -1) {
        fprintf(stderr, "%s must have variables for the decalarion\n", script_file_msg);
        return -1;
      }

      line_tok += strlen("HeaderLine");
      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      CLEAR_WHITE_SPACE(line_tok);

      for (i = 0; *line_tok && (*line_tok != ' ') && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
        //Nothing to do just skip
        //FirstDataLine[i] = *line_tok;
      }
      continue;
    } else if (!strncasecmp(line_tok, "EncodeMode", strlen("EncodeMode"))) {
      if (state == NS_ST_STAT_VAR_NAME) {
	fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
	return -1;
      }

      if (group_idx == -1) {
	fprintf(stderr, "%s must have variables for the group\n", script_file_msg);
	return -1;
      }

      line_tok += strlen("EncodeMode");
      CLEAR_WHITE_SPACE(line_tok);
      //fprintf(stderr, "EncodeMode line_tok = %s\n", line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      CLEAR_WHITE_SPACE(line_tok);
      //NSDL3_VARS(NULL, NULL, "Refresh Name = %s", line_tok);
      for (i = 0; *line_tok && (*line_tok != ' ') && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
	indexvar_buf[i] = *line_tok;
        NSDL3_VARS(NULL, NULL, "i = %d, EncodeMode line for token = %s, *line_tok = %c, indexvar_buf[%d] = %c", i, line_tok, *line_tok, i, indexvar_buf[i]);
      }
      indexvar_buf[i] = '\0';
      NSDL3_VARS(NULL, NULL, "After tokenized EncodeMode Name = %s", indexvar_buf);
      //fprintf(stderr, "After tokenized EncodeMode Name = %s", indexvar_buf);

      if (!strncasecmp(indexvar_buf, "All", 3)) {
        //int i;
	//scope = ENCODE_ALL;
	groupTable[group_idx].encode_type = ENCODE_ALL;
#if 0
        // Set array to 1 for all chars to be encoded
        // First set 1 to all characters
        memset(groupTable[group_idx].encode_chars, 49, TOTAL_CHARS);

        // Now set 0 for 0-9, a-z, A-Z and + . - _
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
      } else if (!strncasecmp(indexvar_buf, "None", 4)) {
	//scope = ENCODE_NONE;
	groupTable[group_idx].encode_type = ENCODE_NONE;
        //memset(groupTable[group_idx].encode_chars, 1, TOTAL_CHARS);
      } else if (!strncasecmp(indexvar_buf, "Specified", 9)) {
	//scope = ENCODE_SPECIFIED;
        encode_flag_specified = 1;
	groupTable[group_idx].encode_type = ENCODE_SPECIFIED;
        memset(groupTable[group_idx].encode_chars, 0, TOTAL_CHARS);
      } else {
	fprintf(stderr, "%s unknown Encode option %s\n", script_file_msg, indexvar_buf);
	return -1;
      }
      //scope_done = 1;

      state = NS_ST_STAT_VAR_OPTIONS;
      CLEAR_WHITE_SPACE(line_tok);
      //fprintf(stderr, "After Encode Name line_tok = %s\n", line_tok);
      //continue;
    } else if (!strncasecmp(line_tok, "CharstoEncode", strlen("CharstoEncode"))) {
      //fprintf(stderr, "Entered in EncodeChars\n");
      if (state == NS_ST_STAT_VAR_NAME) {
	fprintf(stderr, "%s Expecting a variable as the first parameter\n", script_file_msg);
	return -1;
      }

      if (group_idx == -1) {
	fprintf(stderr, "%s must have variables for the group\n", script_file_msg);
	return -1;
      }

      if (encode_flag_specified == 0) 
      {
	fprintf(stderr, "Specified chars must be with Encode=Specified option.");
	return -1;
      }

      line_tok += strlen("CharstoEncode");
      //fprintf(stderr, "EncodeChars, line_tok = %s\n", line_tok);
      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '=', script_file_msg);
      
      //fprintf(stderr, "EncodeChars, After char check line_tok = %s\n", line_tok);
      //CLEAR_WHITE_SPACE(line_tok);
      //NSDL3_VARS(NULL, NULL, "Refresh Name = %s", line_tok);
      // Encode chars can have any special characters including space, single quiote, double quotes. Few examples:
      // EncodeChars=", \[]"
      // EncodeChars="~`!@#$%^&*-_+=[]{}\|;:'\" (),<>./?"
      // TODO

      CLEAR_WHITE_SPACE(line_tok);
      CHECK_CHAR(line_tok, '"', script_file_msg);
      while(1)
      {
        if (buf_chk_flag == 0)
        {
          strcpy(char_to_encode_buf, line_tok);
          //fprintf(stderr, "First time copying char_to_encode_buf = %s\n", char_to_encode_buf);
          buf_chk_flag = 1;
        }
        else
        {
          /*We are here because there is Comma (,) in CharstoEncode token.
           * So first append the , then append the remaining string   */
          strcat(char_to_encode_buf, ",");
          strcat(char_to_encode_buf, line_tok);
          //fprintf(stderr, "Did not get \" in first, copied second token, After copied char_to_encode_buf = %s\n", char_to_encode_buf);
        }

        if(chk_buf(char_to_encode_buf) == 0)
          break;
        else
        {
          line_tok = strtok(NULL, ",");
          if(line_tok)
            continue;
          else
            break;
        }
      }
      
      for (i = 0; char_to_encode_buf[i] != '\0'; i++) {
        if(isalnum(char_to_encode_buf[i]))
        {
          fprintf(stderr, "%s Bad CharstoEncode option %s. Only special characters are allowed\n", script_file_msg, char_to_encode_buf);
          return -1;
        }
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
          break;
        } else {
          //fprintf(stderr, "Setting Encoding for CHAR = %c\n,  
          groupTable[group_idx].encode_chars[(int)char_to_encode_buf[i]] = 1;
        }
      }
      encode_chars_done = 1;
      state = NS_ST_STAT_VAR_OPTIONS;
      //continue;
    }else if (!strncasecmp(line_tok, "EncodeSpaceBy", strlen("EncodeSpaceBy"))) {
      CHK_GET_ARGUMENT_VALUE("EncodeSpaceBy");

      for (i = 0; *line_tok && (*line_tok != ' ') && (*line_tok != ',') && (*line_tok != ')'); line_tok++, i++) {
        EncodeSpaceBy[i] = *line_tok;
        NSDL3_VARS(NULL, NULL, "i = %d, EncodeSpaceBy line for token = %s, *line_tok = %c, EncodeSpaceBy[%d] = %c", i, line_tok, *line_tok, i, EncodeSpaceBy[i]);
      }
      EncodeSpaceBy[i] = '\0';
      NSDL2_VARS(NULL, NULL, "After tokenized EncodeSpaceBy = %s", EncodeSpaceBy);
      if(!strcmp(EncodeSpaceBy, "+") || !strcmp(EncodeSpaceBy, "%20"))
        continue; 
      else {
        fprintf(stderr, "%s Bad charstoEncode declaraction format. '%s' is invalid value for EncodeSpaceBy. \n", script_file_msg, EncodeSpaceBy);
        return -1;
      }
      //continue;
    } else { /* this is for variables inputted */

      if (state == NS_ST_STAT_VAR_OPTIONS) {
	fprintf(stderr, "%s Unrecognized option (%s)\n", script_file_msg, line_tok);
	return -1;
      }

      NSDL2_VARS(NULL, NULL, "Index var line = %s", line_tok);
      for (i = 0; *line_tok && (*line_tok != ' '); line_tok++, i++) {
	indexvar_buf[i] = *line_tok;
        NSDL2_VARS(NULL, NULL, "i = %d, index var line for token = %s, *line_tok = %c, indexvar_buf[%d] = %c", i, line_tok, *line_tok, i, indexvar_buf[i]);
      }
      indexvar_buf[i] = '\0';
      NSDL2_VARS(NULL, NULL, "After tokenized index var indexvar_buf = %s", indexvar_buf);
      CLEAR_WHITE_SPACE(line_tok);

      //Per nsl_index_file_var decalaretion, one group of index vars is decalred.
      //There may be one or more variables in a group
      if (create_group) {
        NSDL2_VARS(NULL, NULL, "Creating group table entry");
	if (create_group_table_entry(&group_idx) != SUCCESS)
	  return -1;
	create_group = 0;
      }

      //Create an antry in indexVarTable for each index variable
      // and set the var_start_dx of sess table to the first variable
      // of the session
      if (create_index_var_table_entry(&var_idx) != SUCCESS)
	return -1;

      //set var_star to the indexVarTable start_index for this index group decl
      if (var_start == -1)
      {
	var_start = var_idx;
        groupTable[group_idx].start_var_idx = var_start;
      }

      if (gSessionTable[sess_idx].index_var_start_idx == -1) {
	gSessionTable[sess_idx].index_var_start_idx = var_idx;
	gSessionTable[sess_idx].num_index_var_entries = 0;
      }

      gSessionTable[sess_idx].num_index_var_entries++;
      // Check if var name contains a size specification.
      // If size is provided, then variable can be read-write (means you can save new value)
      // size is the max size to be written back
      if ((size_ptr = index(indexvar_buf, '/'))) {
	*size_ptr = '\0'; //Remove size from var name
	size_ptr++;
	indexVarTable[var_idx].var_size = atoi(size_ptr);
	if ((indexVarTable[var_idx].var_size <= 0) || (indexVarTable[var_idx].var_size > 4096)) {
	  NS_EXIT(-1, "input_indexvar_data(): size of %s var is %d, should be between 1-4096",
                 indexvar_buf, indexVarTable[var_idx].var_size);
	}
	//rw_data = 1;
	printf ("%s is rw var with size =%d\n", indexvar_buf, indexVarTable[var_idx].var_size);
      } else {
	indexVarTable[var_idx].var_size = -1;
      }

      if(validate_var(indexvar_buf)) {
        NS_EXIT(-1, "%s: Invalid var name '%s'", script_file_msg, indexvar_buf);
      }

      if ((indexVarTable[var_idx].name_pointer = copy_into_big_buf(indexvar_buf, 0)) == -1) {
	NS_EXIT(-1, "input_indexvar_data(): Failed in copying data into big buf");
      }

      NSDL2_VARS(NULL, NULL, "indexvar_buf = %s, indexVarTable idx = %d, group_idx = %d", indexvar_buf, var_idx, group_idx);

      indexVarTable[var_idx].var_type = INDEX_VAR;
      indexVarTable[var_idx].group_idx = group_idx;
      indexVarTable[var_idx].is_file = 0;
      groupTable[group_idx].num_vars++;
      memset(&key, 0, sizeof(DBT));
      //sprintf(absol_var, "%s!%s", indexvar_buf, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
      sprintf(absol_var, "%s!%s", indexvar_buf, 
                                  get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name),
                                  sess_idx, "-"));
      key.data = absol_var;
      key.size = strlen(absol_var);

      sprintf(hash_data, "%d", var_idx);
      memset(&data, 0, sizeof(DBT));
      data.data = hash_data;
      data.size = strlen(hash_data) + 1;

      NSDL2_VARS(NULL, NULL, "group_idx = %d, var_idx = %d, sess_idx = %d, num_var_entries = %d, absol_var = %s, hash_data = %d", group_idx, var_idx, sess_idx, gSessionTable[sess_idx].num_var_entries, absol_var, hash_data);

      ret_val = var_hash_table->put(var_hash_table, NULL, &key, &data, DB_NOOVERWRITE);

      if (ret_val == DB_KEYEXIST) {  /* The variable name is already in the HASH Table.  This is not allowed */
	printf("input_indexvar_data(): Variable name on line %d is already used\n", line_number);
 	return -1;
      } else {
 	if (ret_val != 0) {
 	  fprintf(stderr, "%s Error in entering variable name into hash_table \n", script_file_msg);
 	  perror("hash table put");
 	  return -1;
 	}
      } 
      if (state == NS_ST_STAT_VAR_NAME)
	state = NS_ST_STAT_VAR_NAME_OR_OPTIONS;
    }
  }

  if ((!file_done)  || (!strlen(indexvar_file))) {
    fprintf(stderr, "%s FILE argument is missing from the declaration\n", script_file_msg);
    return -1;
  }


  if ((!indexvar_name_done) || (!strlen(indexvar_name))) {
    fprintf(stderr, "%s IndexVar argument is missing from the declaration\n", 
            script_file_msg);
    return -1;
  }

  groupTable[group_idx].sess_idx = sess_idx;

  if ((groupTable[group_idx].index_var = copy_into_big_buf(indexvar_name, 0)) == -1) {
    NS_EXIT(-1, "input_indexvar_data(): Failed in copying data into big buf");
  }

  if ((groupTable[group_idx].column_delimiter = copy_into_big_buf(column_delimiter, 0)) == -1) {
    NS_EXIT(-1, "input_indexvar_data(): Failed in copying data into big buf");
  }
  groupTable[group_idx].first_data_line = atoi(FirstDataLine);

  if ((groupTable[group_idx].encode_space_by = copy_into_big_buf(EncodeSpaceBy, 0)) == -1) {
    NS_EXIT(-1, "%s: Failed in copying EncodeSpaceBy into big buf", script_file_msg);
  }

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

  NSDL2_VARS(NULL, NULL, "After parsing, column_delimiter = %s, first_data_line = %d", column_delimiter, groupTable[group_idx].first_data_line);
  NSDL2_VARS(NULL, NULL, "Adding indexVar %s for group idx = %d", indexvar_name, group_idx);

  if (input_index_values(indexvar_file, group_idx, sess_idx, var_start) == -1) {
    fprintf(stderr, "%s Error in reading from value file %s \n", script_file_msg, indexvar_file);
    return -1;
  }

  return 0;
#undef NS_ST_STAT_VAR_NAME
#undef NS_ST_STAT_VAR_OPTIONS
#undef NS_ST_STAT_VAR_NAME_OR_OPTIONS
}

// Function to get index var value called from insert segment as well as string api
PointerTableEntry_Shr*
get_index_var_val(VarTableEntry_Shr* idxVarEntry, VUser *vptr, int flag, int cur_seq) {
  GroupTableEntry_Shr* groupTableEntry = idxVarEntry->group_ptr;
  int group_idx = groupTableEntry->idx;
  UserGroupEntry* vugtable = &(vptr->ugtable[group_idx]);
  UserVarEntry* uservar_entry;
  char *index_var_value = NULL;
  int index_var_len = 0;
  char vector_flag = 0;

  NSDL2_VARS(vptr, NULL, "Method Called, var name = %s", idxVarEntry->name_pointer);

  uservar_entry = &vptr->uvtable[groupTableEntry->index_key_var_idx];
 
  // We are using this method in case var type SEGMENT also. In repeat block we can use vector index variable to index the index variable
  // So for this we need to know that index variable is vector or scalar 
  if(vptr->sess_ptr->var_type_table_shr_mem[groupTableEntry->index_key_var_idx])
    vector_flag = 1;

  // This is the case for non repeatable block and index parameter is based on ord specific. 
  if(!vector_flag){
    index_var_value = uservar_entry->value.value;
    index_var_len = uservar_entry->length;
  }
  // This is the case for Repeatable block and ORD ALL case.
  // If current sequence is less than uservar lenth, then all the value will be filled
  // else it will give null value for the index parameter and in the error log message will come "Index Key var value not available at index()"
  else if(cur_seq != SEG_IS_NOT_REPEAT_BLOCK && vector_flag){
    if(cur_seq < uservar_entry->length) {
			index_var_value = uservar_entry->value.array[cur_seq].value; 
			index_var_len = uservar_entry->value.array[cur_seq].length;
    }
    else {
			NSEL_MAJ(vptr, NULL, ERROR_ID, ERROR_ATTR, "Index Key var value not available at index(%d) for index variable %s",
																									cur_seq, idxVarEntry->name_pointer);
			return NULL;
    }
  }
  // This will be the normal case .ie no repeatbale block and ORD all specific and index not found for search/xml parameter.
  // In that case. null value will be given for index parameter and
  // in the error log message will come "Index Key Var is vector type for index variable"
  else {
    NSEL_MAJ(vptr, NULL, ERROR_ID, ERROR_ATTR, "Index Key Var is vector type for index variable %s",
                                                idxVarEntry->name_pointer);
    return NULL;
  }
 
  if(index_var_value) {
     vugtable->cur_val_idx = (groupTableEntry->idx_var_hash_func)(index_var_value, index_var_len);
     NSDL2_VARS(NULL, NULL, "Index key var value = %s, cur_val_idx = %d", index_var_value, vugtable->cur_val_idx);
  } else {
    /*NSEL_MAJ(vptr, NULL, ERROR_ID, ERROR_ATTR, "Index Key Var value is empty for index variable %s",
                                                idxVarEntry->name_pointer);*/

    NS_EL_2_ATTR(EID_MISC,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                __FILE__, (char*)__FUNCTION__, "Index Key Var value is empty for index variable %s.", idxVarEntry->name_pointer);
 
    return NULL;
  }

  if (vugtable->cur_val_idx == -1) {
    NSEL_MAJ(vptr, NULL, ERROR_ID, ERROR_ATTR, "Index Var value not found for variable '%s'",
                                                idxVarEntry->name_pointer);
    return NULL;
  } else {
    NSDL2_VARS(NULL, NULL, "Found cur_val_idx = %d\n", vugtable->cur_val_idx);
    return (idxVarEntry->value_start_ptr + vugtable->cur_val_idx);
  }
}
//--------------------------------------
