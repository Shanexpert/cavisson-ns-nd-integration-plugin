/******************************************************************
 * Name                : ns_sync_point_api_parse.c
 * Author              : Prachi
 * Purpose             :
 * Initial Version     : Wednesday, November 28 2012
 * Modification History:
 *
 *******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include "nslb_hash_code.h"
#include "ns_url_hash.h"
#include "ns_data_types.h"
#include "ns_auto_fetch_parse.h"
#include "ns_global_settings.h"
#include "ns_trans_parse.h"
#include "ns_sync_point.h"
#include "nslb_hash_code.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_script_parse.h"

int total_sp_api_hash_entries = 0;
int total_sp_api_found = 0;

SPApiTableLogEntry *spApiTable  = NULL;

char *tmp_file_name = "sync_points_api_dup.txt";
char *final_file_name = "sync_points_api.txt";

Str_to_hash_code api_hash_func_ptr;
Hash_code_to_str api_var_get_key;

/******************* SYNC POINT API PARSING SECTION BEGINS************************************************************/

//Parsing script and writing all the sync point api in a file.
//Further we will use this file to generate hash.
static void write_sp_api_in_file(char *sp_api_name)
{
  FILE* file_ptr;
  char tmp_file[1024];
  
  NSDL4_SP(NULL, NULL, "Method Called. sp_api_name = %s", sp_api_name);

  sprintf(tmp_file, "%s/%s", g_ns_tmpdir, tmp_file_name);

  if ((file_ptr = fopen(tmp_file, "a")) == NULL) {
    fprintf(stderr, "Error in opening file %s\n", tmp_file);
    perror("fopen");
  }

  fprintf(file_ptr, "%s\n", sp_api_name);

  fclose(file_ptr);

  NSDL4_SP(NULL, NULL, "Method Existing.");
}


// Assumption is that only one API is in one line.
// Called from ns_script_parse.c and ns_sync_point_api_parse.c
int parse_sp_api(char *buffer, char *fname, int line_num)
{
  char* buf_ptr;
  char* fields[100];

   NSDL4_SP(NULL, NULL, "Method Called. buffer = %s, fname = %s, line_num = %d", buffer, fname, line_num);

  /* ns_define_syncpoint is used to define a sync_point (not starting accumulating users at it). This
     is used when we want to set up a sync_point using a variable. So we need to
     define these sync_point so that these names can be added in the hash table
     For example:
       ns_define_syncpoint("define_sp");
       char sy_name[64] = "define_sp";
       ns_sync_point(sy_name);

     ns_sync_point is used to start accumulating users at this point.
     For example:
       ns_sync_point("cavisson");
       ns_sync_point("testing"); */

  if ((buf_ptr = strstr (buffer, "ns_sync_point")))
  {
    //total_sp_api_found++; //TODO: ++ only when it is ns_sync_point, not in case of ns_define_syncpoint

    if(get_tokens(buf_ptr, fields, "\"", 100) < 3)
    {
      int nargs = get_args(buf_ptr, fields);
      if (nargs == -1 || nargs != 1) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012007_ID, CAV_ERR_1012007_MSG);
      }
    }
    else
    { 
      total_sp_api_found++; //TODO: ++ only when it is ns_sync_point, not in case of ns_define_syncpoint
      write_sp_api_in_file(fields[1]);
    }
  }
 
  NSDL4_SP(NULL, NULL, "Method Exiting.");
  return 0;
}

// For getting sp_api_names from the script. Called from url.c
int get_sp_api_names(FILE* c_file)
{
  int line_num = 0;
  char buffer[MAX_LINE_LENGTH];

  NSDL4_SP(NULL, NULL, "Method Called.");
  
  while (nslb_fgets(buffer, MAX_LINE_LENGTH, c_file, 1))
  {
    NSDL4_SP(NULL, NULL, "buffer = %s, line_num = %d", buffer, line_num);
    line_num++;
    parse_sp_api(buffer, "script.c", line_num);
  }
  
  NSDL4_SP(NULL, NULL, "Method Existing.");
  return 0;
}

/******************* SYNC POINT API PARSING SECTION ENDS ************************************************************/

//Validating sync point api name.
static void sp_api_val_name (char *sp_api_name)
{
  NSDL4_SP(NULL, NULL, "Method Called. sp_api_name = %s", sp_api_name);

  int sp_api_len = strlen(sp_api_name);

  if (sp_api_len > SP_API_NAME_MAX_LEN)
  {
    NS_EXIT(-1, "Error: sp_api_val_name()- Length of sp api name (%s) is (%d) larger than %d characters", sp_api_name, sp_api_len, SP_API_NAME_MAX_LEN);
  }

  if (match_pattern(sp_api_name, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
  {
    NS_EXIT(-1, "Error: sp_api_val_name() - 1. Name of sp api should contain only alphanumeric character with including <91>_<92> but first character should be alpha, Name of sp_api given by User is = %s", sp_api_name);
  }
  
  NSDL4_SP(NULL, NULL, "Method Existing.");
}

void remove_duplicates_sp_api()
{
  char cmd[1024]="\0";
  char err_msg[1024]="\0";

  //sprintf(cmd, "`sort -u %s/%s >> %s/%s`", g_ns_tmpdir, dup_file_name, g_ns_tmpdir, file_name);
  sprintf(cmd, "`sort -u %s/%s >> %s/%s`", g_ns_tmpdir, tmp_file_name, g_ns_tmpdir, final_file_name);
  nslb_system(cmd,1,err_msg);
}

//Generating hash of all the api written in file sync_points_api.txt
void gen_hash_of_api()
{
  NSDL4_SP(NULL, NULL, "Method Called.");
  
  remove_duplicates_sp_api();

  total_sp_api_hash_entries = generate_hash_table_ex(final_file_name, "sync_api_hash_variables", &api_hash_func_ptr, &api_var_get_key, NULL, NULL, NULL, 0, g_ns_tmpdir);

  NSDL4_SP(NULL, NULL, "total_sp_api_hash_entries = %d", total_sp_api_hash_entries);
 
  if(total_sp_api_hash_entries == 0) {
    NSDL4_SP(NULL, NULL, "Exiting because total_sp_api_hash_entries = %d", total_sp_api_hash_entries);
    NS_EXIT(-1, "Exiting because total_sp_api_hash_entries = %d", total_sp_api_hash_entries);
  }

  NSDL4_SP(NULL, NULL, "Method Existing.");
}

//This function is used to get hash of the sync point passed to it.
int get_sp_api_hash_code(char *sync_pt_name)
{
  int hash;
  NSDL4_SP(NULL, NULL, "Method Called. sync_pt_name = %s", sync_pt_name);

  hash = api_hash_func_ptr(sync_pt_name, strlen(sync_pt_name));
  NSDL4_SP(NULL, NULL, "Method Exiting. hash = %d", hash);
  return(hash);
}

//Adding sync point api in spApiTable, using its hash as index in this table.
static void add_sp_api_name (char *sp_api_name, int sp_api_idx)
{
  int sp_api_hash;

  NSDL4_SP(NULL, NULL, "Method Called. sp_api_name = %s, sp_api_idx = %d", sp_api_name, sp_api_idx);

  //getting hash
  sp_api_hash = get_sp_api_hash_code(sp_api_name);
  NSDL4_SP(NULL, NULL, "sp_api_hash = %d", sp_api_hash);

  MY_MALLOC(spApiTable[sp_api_idx].sp_api_name, strlen(sp_api_name) + 1, "sync point api name", -1);
  strcpy(spApiTable[sp_api_idx].sp_api_name, sp_api_name);
  spApiTable[sp_api_idx].sp_grp_tbl_idx = -1;
  spApiTable[sp_api_idx].api_hash_idx = sp_api_hash;

  NSDL1_SP(NULL, NULL, "Method Exiting. spApiTable[%d].sp_api_name = %s", sp_api_idx, spApiTable[sp_api_idx].sp_api_name);
}

/* This function will do following task:
 *  Generate hash of all the api written in file sync_points_api.txt by calling -> gen_hash_of_api().
 *  Read file sync_points_api.txt line by line.
 *  Validate each sync point api name.
 *  Add the validated api in spApiTable using its hash as index.*/ 
void fill_sp_api_table()
{
  FILE* file_ptr;
  char tmp_file[1024];
  char buf[1024 + 1];
  int count = 0;

  NSDL4_SP(NULL, NULL, "Method Called.");

  gen_hash_of_api();

  MY_MALLOC_AND_MEMSET(spApiTable, (sizeof(SPApiTableLogEntry) * total_sp_api_found), "SP API table creation", -1);

  sprintf(tmp_file, "%s/%s", g_ns_tmpdir, tmp_file_name);

  if ((file_ptr = fopen(tmp_file, "r")) == NULL) {
    fprintf(stderr, "Error in opening file %s\n", tmp_file);
    perror("fopen");
  }

  while(nslb_fgets(buf, 1024, file_ptr, 1) != NULL) {
    NSDL4_SP(NULL, NULL, "buf = %s", buf);
    buf[strlen(buf) - 1] = '\0'; 

    sp_api_val_name(buf);
    add_sp_api_name(buf, count);
    count++;
  }
  fclose(file_ptr);

  NSDL4_SP(NULL, NULL, "Method Exiting.");
}
