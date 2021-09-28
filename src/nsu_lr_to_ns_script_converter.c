/*
 Name:
 Purpose: LR to NS script converter
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <regex.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "nslb_util.h"
#include "nslb_map.h"
#include "nslb_alloc.h"
#include <libgen.h>
#include "ns_alloc.h"

#define MAX_FLOW 1024
#define FIELD_LENGTH 256

#define INIT_PARAM_SIZE   10
#define DELTA_PARAM_SIZE   5

#define PARAM_LEN_SIZE 100000
#define SEARCH_PARAM     0
#define CHECKPOINT       1
#define SEARCH_PARAM_EX  2
#define SEARCH_PARAM_REG_EX  3


#define CLEAR_WHITE_SPACE_AND_TAB_FROM_END(ptr) { int end_len = strlen(ptr); \
                                          while((ptr[end_len - 1] == ' ') || ptr[end_len - 1] == '\t' ||  ptr[end_len - 1] == '\n' || ptr[end_len - 1] == '\r') { \
                                            ptr[end_len - 1] = '\0';\
                                            end_len = strlen(ptr);\
                                          }\
                                        }

#define SKIP_BLANK_LINE(ptr){ CLEAR_WHITE_SPACE_AND_TAB_FROM_END(ptr); \
                               if(ptr[0] == '\0')\
                                 continue;\
                              }


char c_flow_arr[MAX_FLOW][FIELD_LENGTH];
int total_c_script_files = 0;
int total_c_file_to_process=0;
char runlogic[64];
FILE *ns_reg_fp= NULL;

int max_entries = 0;
int param_api_counter = 0;
NSApi *api_attr;

void nsu_lr_to_ns_script_converter_usage(char *err)
{
  fprintf(stderr, "%s\n", err);
  fprintf(stderr, "nsu_lr_to_ns_converter -l <path of the lr script> -n <path of ns script>\n");
  fprintf(stderr, "Options:   -l : Input path of the lr script\n");
  fprintf(stderr, "           -n : Input path of the ns script\n");
  exit(-1);
}

char * copy_function(char *start_ptr)
{
  char *start_copy_ptr = NULL, *end_copy_ptr = NULL, *buf_to_copy_ptr = NULL;
  int copy_length;

  start_copy_ptr = strchr(start_ptr, '"');
  end_copy_ptr = strrchr(start_ptr, '"');

  start_copy_ptr++;

  copy_length = end_copy_ptr - start_copy_ptr;
  buf_to_copy_ptr = (char*) malloc(copy_length +1);
  snprintf(buf_to_copy_ptr, copy_length +1, "%s", start_copy_ptr);
  
  start_ptr = end_copy_ptr;
  return buf_to_copy_ptr;
}

FILE *open_file(char *file, char *mode)
{
  FILE *fp = NULL;
  if((fp = fopen(file, mode)) == NULL)
  {
    fprintf(stderr, "Cannot open file = %s, mode = %s, err = %s\n", file, mode, nslb_strerror(errno));
    exit(-1);
  }
  return fp;
}

#if 0
#define MAX_ARG_NAME_SIZE 64
#define MAX_API_NAME_SIZE MAX_ARG_NAME_SIZE
#define MAX_ARG_VALUE_SIZE 4096
#define MAX_ARGUMENT_FILED 0xff

typedef struct NSApiFields_table {
   char keyword[MAX_ARG_NAME_SIZE + 1];
   char value  [MAX_ARG_VALUE_SIZE + 1];
}NSApiFields;
         
typedef struct NSApi_table {
   unsigned char  num_tokens; 
   char  api_name[MAX_API_NAME_SIZE + 1];
   NSApiFields api_fields[MAX_ARGUMENT_FILED + 1];
}NSApi;
#endif


void convert_LR_line_to_NS_line(char *lr_line, char *lr_sub_line, char *ns_sub_line, char *ns_line)
{ 
  char prev_buf[1024 *2] = {0};
  char temp_buf[1024 *4] = {0};
  char *ptr = NULL; 
  char *start_ptr = NULL;
  start_ptr = lr_line;
  
  if((ptr = strstr(lr_line, lr_sub_line)) != NULL) {
    snprintf(prev_buf, ptr - start_ptr + 1, "%s", start_ptr);
    ptr += strlen(lr_sub_line);
    sprintf(temp_buf, "%s%s%s", prev_buf, ns_sub_line, ptr);
    strcpy(ns_line, temp_buf);
  }
}


char * convert_SEARCH_PARAM_keyword(char *param_keyword, char *param_value)
{
  if(strstr(param_keyword, "BIN"))
    convert_LR_line_to_NS_line(param_keyword, "BIN", "BINARY", param_keyword);
  if(strstr(param_keyword, "/DIG"))
    convert_LR_line_to_NS_line(param_keyword, "/DIG", "", param_keyword);
  if(strstr(param_keyword, "/ALNUMIC"))
    convert_LR_line_to_NS_line(param_keyword, "/ALNUMIC", "", param_keyword);
  if(strstr(param_keyword, "/ALNUMUC"))
    convert_LR_line_to_NS_line(param_keyword, "/ALNUMUC", "", param_keyword);
  if(strstr(param_keyword, "/ALNUMLC"))
    convert_LR_line_to_NS_line(param_keyword, "/ALNUMLC", "", param_keyword);
  else if(strstr(param_keyword, "IgnoreRedirections")) {
    convert_LR_line_to_NS_line(param_keyword, "IgnoreRedirections", "", param_keyword);
    convert_LR_line_to_NS_line(param_value, "Yes", "LAST", param_value);
  }
  else if(strstr(param_keyword, "NotFound")) {
    if(strstr(param_value, "ERROR")) {
      convert_LR_line_to_NS_line(param_keyword, "NotFound", "ActionOnNotFound", param_keyword);
      convert_LR_line_to_NS_line(param_value, "ERROR", "Error", param_value);
    }else
      convert_LR_line_to_NS_line(param_keyword, "NotFound", "", param_keyword);
  }
  else if(strstr(param_keyword, "Search")) {
    if(strstr(param_value, "Headers"))
     convert_LR_line_to_NS_line(param_value, "Headers", "Header", param_value);
    if(strstr(param_value, "NoResource"))
     convert_LR_line_to_NS_line(param_keyword, "Search", "", param_keyword);
  }
  else if(strstr(param_keyword, "RelFrameId"))
    convert_LR_line_to_NS_line(param_keyword, "RelFrameId", "", param_keyword);

  return NULL;
}

char * convert_SEARCH_PARAM_EX_keyword(char *param_keyword, char *param_value)
{
  if(strstr(param_keyword, "BIN"))
    convert_LR_line_to_NS_line(param_keyword, "BIN", "BINARY", param_keyword);
  else if(strstr(param_keyword, "NotFound")) {
    if(strstr(param_value, "ERROR")) {
      convert_LR_line_to_NS_line(param_keyword, "NotFound", "ActionOnNotFound", param_keyword);
      convert_LR_line_to_NS_line(param_value, "ERROR", "Error", param_value);
    }else
      convert_LR_line_to_NS_line(param_keyword, "NotFound", "", param_keyword);
  }
  else if(strstr(param_keyword, "RegExp"))
    convert_LR_line_to_NS_line(param_keyword, "RegExp", "RE", param_keyword);
  else if(strstr(param_keyword, "SEARCH_FILTERS"))
    convert_LR_line_to_NS_line(param_keyword, "SEARCH_FILTERS", "", param_keyword);
  else if(strstr(param_keyword, "Scope")) {
    convert_LR_line_to_NS_line(param_keyword, "Scope", "Search", param_keyword);
    if(strstr(param_value, "HEADERS"))
       convert_LR_line_to_NS_line(param_value, "HEADERS", "Header", param_value);
     else if(strstr(param_value, "COOKIES"))
       convert_LR_line_to_NS_line(param_keyword, "Scope", "", param_keyword);
     else if(strstr(param_value, "BODY"))
       convert_LR_line_to_NS_line(param_value, "BODY", "Body", param_value);
     else if(strstr(param_value, "ALL"))
       convert_LR_line_to_NS_line(param_value, "ALL", "All", param_value);
  }
  else if(strstr(param_keyword, "HeaderNames"))
    convert_LR_line_to_NS_line(param_keyword, "HeaderNames", "", param_keyword);
  else if(strstr(param_keyword, "RelFrameId"))
    convert_LR_line_to_NS_line(param_keyword, "RelFrameId", "", param_keyword);
  else if(strstr(param_keyword, "IgnoreRedirections")) {
    convert_LR_line_to_NS_line(param_keyword, "IgnoreRedirections", "RedirectionDepth", param_keyword);
    convert_LR_line_to_NS_line(param_value, "Yes", "LAST", param_value);
  }
  else if(strstr(param_keyword, "RequestUrl"))
    convert_LR_line_to_NS_line(param_keyword, "RequestUrl", "", param_keyword);
  else if(strstr(param_keyword, "ContentType"))
    convert_LR_line_to_NS_line(param_keyword, "ContentType", "", param_keyword);

  return NULL;
}

char * convert_CHECKPOINT_keyword(char *param_keyword, char *param_value)
{
  if(strstr(param_keyword, "BIN"))
    convert_LR_line_to_NS_line(param_keyword, "BIN", "", param_keyword);
  if(strstr(param_keyword, "/DIG"))
    convert_LR_line_to_NS_line(param_keyword, "/DIG", "", param_keyword);
  if(strstr(param_keyword, "/ALNUMIC"))
    convert_LR_line_to_NS_line(param_keyword, "/ALNUMIC", "", param_keyword);
  if(strstr(param_keyword, "/ALNUMUC"))
    convert_LR_line_to_NS_line(param_keyword, "/ALNUMUC", "", param_keyword);
  if(strstr(param_keyword, "/ALNUMLC"))
    convert_LR_line_to_NS_line(param_keyword, "/ALNUMLC", "", param_keyword);
  else if(strstr(param_keyword, "Fail")){
    convert_LR_line_to_NS_line(param_keyword, "Fail", "FAIL", param_keyword); 
    if(strstr(param_keyword, "Found"))
      convert_LR_line_to_NS_line(param_value, "Found", "FOUND", param_value);
    else if(strstr(param_keyword, "NotFound"))
      convert_LR_line_to_NS_line(param_value, "NotFound", "NOTFOUND", param_value);    
  }
  else if(strstr(param_keyword, "Search")) {
    convert_LR_line_to_NS_line(param_keyword, "Search", "Search_IN", param_keyword);
    if(strstr(param_value, "Headers"))
      convert_LR_line_to_NS_line(param_value, "Headers", "HEADER", param_value);
    if(strstr(param_value, "Body"))
      convert_LR_line_to_NS_line(param_value, "Body", "BODY", param_value);
    if(strstr(param_value, "All"))
      convert_LR_line_to_NS_line(param_value, "All", "ALL", param_value);
  }
  else if(!strcmp(param_keyword, "Text")){
    convert_LR_line_to_NS_line(param_keyword, "Text", "TEXT", param_keyword);
  }
  return NULL;
}

void save_ns_param(char *page_name)
{
  int i,j;
  char *param_string;
  int total_len = 0, len, length, attr_len;
  param_string = (char *)malloc(PARAM_LEN_SIZE);
  total_len = length = PARAM_LEN_SIZE;
  int param_type;

  for (i = 0; i < param_api_counter; i++)
  {
    if (!strcmp(api_attr[i].api_name, "web_reg_save_param_regexp")) { 
      param_type=SEARCH_PARAM_REG_EX; 
      len = sprintf(param_string, "nsl_search_var(");
    }else if (!strcmp(api_attr[i].api_name, "web_reg_save_param_ex")) {
      param_type=SEARCH_PARAM_EX;
      len = sprintf(param_string, "nsl_search_var(");
    }else if (!strcmp(api_attr[i].api_name, "web_reg_save_param")) {
      param_type = SEARCH_PARAM;
      len = sprintf(param_string, "nsl_search_var(");        
    }else if (!strcmp(api_attr[i].api_name, "web_reg_find")) {
      param_type=CHECKPOINT;
      len = sprintf(param_string, "nsl_web_find(");
    } 
      total_len -= len;
    
    for(j = 0; j < api_attr[i].num_tokens; j++)
    {
      attr_len = strlen(api_attr[i].api_fields[j].keyword);
      attr_len += strlen(api_attr[i].api_fields[j].value);
      attr_len += (j || param_type)?5:2;

      if (attr_len > total_len)
      {
        param_string = (char *)realloc(param_string, (length + PARAM_LEN_SIZE));
        length += PARAM_LEN_SIZE;
        total_len += PARAM_LEN_SIZE;
      }
      if(param_type == SEARCH_PARAM || param_type == SEARCH_PARAM_REG_EX || param_type == SEARCH_PARAM_EX)
      { 
        if (!j)
        {
          if(param_type == SEARCH_PARAM_REG_EX || param_type == SEARCH_PARAM_EX)
            len += snprintf(param_string + len, (total_len - len), "%s, PAGE=%s", api_attr[i].api_fields[j].value, page_name);
          else
            len += snprintf(param_string + len, (total_len - len), "%s, PAGE=%s", api_attr[i].api_fields[j].keyword, page_name);
        }
        else{
          if(param_type == SEARCH_PARAM_REG_EX)
            convert_SEARCH_PARAM_EX_keyword(api_attr[i].api_fields[j].keyword, api_attr[i].api_fields[j].value);
          else if(param_type == SEARCH_PARAM_EX)
            convert_SEARCH_PARAM_EX_keyword(api_attr[i].api_fields[j].keyword, api_attr[i].api_fields[j].value);
          else if(param_type == SEARCH_PARAM)
            convert_SEARCH_PARAM_keyword(api_attr[i].api_fields[j].keyword, api_attr[i].api_fields[j].value);

          if(api_attr[i].api_fields[j].keyword[0] != '\0')
            len += snprintf(param_string + len, (total_len - len), ", %s=\"%s\"", api_attr[i].api_fields[j].keyword, api_attr[i].api_fields[j].value);
          else
            continue;
         }
      }
      else if( param_type == CHECKPOINT)
      {
        convert_CHECKPOINT_keyword(api_attr[i].api_fields[j].keyword, api_attr[i].api_fields[j].value);
        if(api_attr[i].api_fields[j].keyword[0] != '\0')
          len += snprintf(param_string + len, (total_len - len), "%s=\"%s\", ", api_attr[i].api_fields[j].keyword, api_attr[i].api_fields[j].value);
        else
          continue;
      }
      total_len -= len;
    }
    if(param_type == SEARCH_PARAM || param_type == SEARCH_PARAM_EX || param_type == SEARCH_PARAM_REG_EX) 
      len += snprintf(param_string + len, (total_len - len), ");\n");
    else
      len += snprintf(param_string + len, (total_len - len), " PAGE=%s);\n", page_name);
      
    fprintf(ns_reg_fp, "%s", param_string);
  }
  memset(api_attr, 0, (INIT_PARAM_SIZE * sizeof(NSApi)));
}

int read_line_argument_and_fill_attr_table(char *buf, int index)
{
  char *cur_ptr, *tmp_ptr;
  char tmp_buf[1024];
  int idx = 0;
  int starting_quotes = 0;
  static int equal_found = 0;
 
  if(buf[0] == '\0')
    return 0; 
  cur_ptr = buf;
  CLEAR_WHITE_SPACE(cur_ptr);
  CLEAR_WHITE_SPACE_AND_TAB_FROM_END(cur_ptr);
  
  if (equal_found)
    goto value;
  while (1)
  {
    if(*cur_ptr != ',' && *cur_ptr != '=' && *cur_ptr  != '\0')
    {
      if (*cur_ptr == '"' && !starting_quotes)
      {
        starting_quotes = 1;
        cur_ptr++;
        CLEAR_WHITE_SPACE(cur_ptr);
        if(*cur_ptr == '=')
        {
          equal_found = 1;
          goto value;
        }

        continue;
      }
 
      if (*cur_ptr == '"' && *(cur_ptr - 1) != '\\')
      {
        cur_ptr++;
        CLEAR_WHITE_SPACE(cur_ptr);
        continue;
      }
      tmp_buf[idx++] = *cur_ptr++;
 
      if(*cur_ptr == '=')
        equal_found = 1;
 
      continue;
    }

    if(!idx)
    {
      idx = 0;
      tmp_buf[idx] = '\0';
      cur_ptr++;
      continue;
    }
    tmp_buf[idx] = '\0';
    tmp_ptr = tmp_buf;

    CLEAR_WHITE_SPACE(tmp_ptr);
    CLEAR_WHITE_SPACE_AND_TAB_FROM_END(tmp_ptr);

    strcpy(api_attr[index].api_fields[api_attr[index].num_tokens].keyword, tmp_ptr);

    if(*cur_ptr == '\0' || equal_found == 0)
      goto next;
    cur_ptr++; // Point after = sign

    value:

    idx = 0;
    tmp_buf[idx] = '\0';

    while(*cur_ptr != '\0')
    {
      if(*cur_ptr == '"' && *(cur_ptr - 1) != '\\'){
        cur_ptr++;
        CLEAR_WHITE_SPACE(cur_ptr);
        starting_quotes = !starting_quotes; 
        if((*cur_ptr != '\0') && (*cur_ptr == ','))
        {
          equal_found = 0;
          break;
        }
      }
      if (*cur_ptr == ',' && !starting_quotes)
      {
        equal_found = 0;
        break;
      }
      tmp_buf[idx++] = *cur_ptr++;

      if (*cur_ptr == ',' && starting_quotes)
        tmp_buf[idx++] = *cur_ptr++;
    }
    tmp_buf[idx] = '\0';
    strcat(api_attr[index].api_fields[api_attr[index].num_tokens].value, tmp_buf);
    tmp_ptr = NULL; 

    next:
    if (*cur_ptr == '\0')
      break;
    if (equal_found == 0)
    api_attr[index].num_tokens++;

    if (*cur_ptr == ',')
    {
      idx = 0;
      tmp_buf[idx] = '\0';
      cur_ptr++;
      starting_quotes = 0;
    }
    if (*cur_ptr == '\0')
      break;
  }
  return 0;
}

char* change_special_char_in_name(char *name, char* send_name_buf)
{
  char *ptr2, *ptr;

  ptr=name;

  if(!isalnum(*ptr)){
    ptr++;
    sprintf(send_name_buf,"P_%s",ptr);
    ptr=send_name_buf;
  }
  ptr2 =ptr;

  while(*ptr != '\0')
  { 
    if(!isalnum(*ptr))
      *ptr='_';
    ptr++;
  }
  return ptr2;
}


void create_runlogic_file(char *runlogic, char *ns_script_path)
{
  char filepath[512];
  char buffer[2048 + 1];
  int flag = 0, i;
  char *ptr, *str, *token_ptr;
  FILE *run_fp;
  run_fp = fopen(runlogic, "r");
  if(!run_fp)
  {
    fprintf(stderr, "Runlogic file %s does not exists\n", runlogic); 
    exit(-1);
  }
  while(fgets(buffer, 2048, run_fp))
  {
    if(!strncmp(buffer, "Name=\"Run\"", 10) || flag)
    {
      flag = 1;
      if(!strncmp(buffer, "RunLogicActionOrder=\"", 21))
        break;
      else
        continue;
    }
  }
  fclose(run_fp);
  //save order of flow files till 100 flows
  if((ptr = strchr(buffer, '"')))
  {
    *ptr = '\0';
    ptr++;
    if((str = strchr(ptr, '"')))
      *str = '\0';
  }
  else
  {
    fprintf(stderr, "Unable to get runlogic file from lr script\n"); 
    exit(-1);
  }
  str = ptr;
  while((token_ptr = strtok_r(ptr, ",", &ptr)))
  {
    strcpy(c_flow_arr[total_c_script_files++], token_ptr);
    if(total_c_script_files == MAX_FLOW)
      break;
  }
  strcpy(c_flow_arr[total_c_script_files++], "vuser_init");
  strcpy(c_flow_arr[total_c_script_files++], "vuser_end");
  sprintf(filepath, "%s/runlogic.c", ns_script_path);
  run_fp = fopen(filepath, "w");
  if(!run_fp)
  {
    fprintf(stderr, "Error in open file %s\n", filepath);
    exit(-1);
  }
  fprintf(run_fp, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include \"ns_string.h\"\n\n");
  fprintf(run_fp, "extern int init_script();\nextern int exit_script();\n\n");
  fprintf(run_fp, "typedef void FlowReturn;\n\n");
  for(i=0; i<total_c_script_files - 2; i++)
    fprintf(run_fp, "extern FlowReturn %s();\n", c_flow_arr[i]);
  
  fprintf(run_fp, "\nvoid runlogic()\n{\n\tNSDL2_RUNLOGIC(NULL, NULL, \"Executing init_script()\");\n\n");
  fprintf(run_fp, "\tinit_script();\n\n\tNSDL2_RUNLOGIC(NULL, NULL, \"Executing sequence block - Block1\");\n\t{\n");
  
  for(i=0; i<total_c_script_files - 2; i++)
    fprintf(run_fp, "\t\tNSDL2_RUNLOGIC(NULL, NULL, \"Executing flow - %s\");\n\t\t%s();\n\n", c_flow_arr[i], c_flow_arr[i]);

  fprintf(run_fp, "\t}\n\tNSDL2_RUNLOGIC(NULL, NULL, \"Executing ns_exit_session()\");\n\tns_exit_session();\n}\n");
  fclose(run_fp);
}

void check_run_logic_file_in_LR(char *LRScriptPath, char *NSScriptPath)
{
  DIR *d;
  struct dirent *dir;
  char buff[24] = {0};
  char *ptr = NULL;
  struct stat s;

  if(stat(LRScriptPath, &s))
  {
    fprintf(stderr, "Error: Provided Load Runner script path does not exist. Please provide a valid script\n");
    exit(-1);
  }

  if(stat(NSScriptPath, &s)) {
    if(NSScriptPath[strlen(NSScriptPath) - 1] != '/')
      strcat(NSScriptPath, "/"); //adding / at last
    mkdir_ex(NSScriptPath);
  }

  d = opendir(LRScriptPath);
  if(d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      if(dir->d_name[0] == '.')
        continue;
      int len = strlen(dir->d_name) - 4;
      sprintf(buff, "%s", dir->d_name + len);
      if((ptr = strstr(buff, ".usp")))
      {
        strcpy(runlogic, dir->d_name);
        break;
      }
    }
    closedir(d);
  }
}

void check_commented_line(char *buf, int *comment, int *multiline_comment)
{
  char tmp_buf[1024 *4]= {0};
  char *start_ptr= NULL;

  strcpy(tmp_buf, buf);
  CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(tmp_buf);
  start_ptr = tmp_buf;
  CLEAR_WHITE_SPACE(start_ptr);
  sprintf(tmp_buf, "%s", start_ptr);

  if (*multiline_comment == 1){
      if (strncmp(tmp_buf + (strlen(tmp_buf) - 2), "*/", 2) == 0)   // Check for end of multi-line comment 
        *multiline_comment = 0;
      *comment = 1;
  }
  else if( tmp_buf[0] == '/' && tmp_buf[1] == '/') //single line comment found
      *comment = 1;
  else if( tmp_buf[0] == '/' && tmp_buf[1] == '*') //multiline comment start found
  {
    if (strncmp(tmp_buf + (strlen(tmp_buf) - 2), "*/", 2) != 0)
      *multiline_comment=1;
    *comment = 1;
  }
}



void convert_data_into_ns_comp(FILE *ns_fp, FILE *lr_fp)
{
  char new_buf[50000] = {0};
  char prev_buf[1024] = {0};
  char buf[1024 *4] = {0}, header_buf[1024] = {0};
  char *ptr = NULL, *start_ptr = NULL, *end_ptr= NULL, *header_key = NULL, *auto_header_ptr=NULL, *second_ptr = NULL, *start_found =NULL, *end_found = NULL;
  char save_submit_data_flag = 0;
  char save_submit_data_buf[50000];
  char *save_submit_data_start_ptr;
  int half_header_flag = 0,web_half_header_flag=0, header_count= 0;
  NSLBMap *header_map;

  char tmp_buf[1024 * 4];
  int multiline_found = 0, Body_multiliner=0, URL_multiliner =0;
  int multiline_comment = 0,comment =0;
  int itemdata_found = 0;

  header_map = nslb_map_init(NSLB_MAP_INIT_SIZE, NSLB_MAP_DELTA_SIZE);

  api_attr = (NSApi *)malloc(INIT_PARAM_SIZE * sizeof(NSApi));
  max_entries = INIT_PARAM_SIZE;
  memset(api_attr, 0, (INIT_PARAM_SIZE * sizeof(NSApi)));

  while(fgets(buf, 1024, lr_fp) != NULL)
  {
    start_ptr = buf;

    comment = 0;
    check_commented_line(buf, &comment, &multiline_comment);

    if(comment){
      fputs(buf, ns_fp);
      continue;
    }

    if(save_submit_data_flag)
    {
      if(strstr(buf, "\"Resource=") || strstr(buf, "\"EncType=") || strstr(buf, "\"Mode=")|| strstr(buf, "\"TargetFrame="))
        continue;
      else if((save_submit_data_start_ptr = strstr(buf, "\"Action="))){
        strncat(save_submit_data_buf, start_ptr, save_submit_data_start_ptr - start_ptr);
        save_submit_data_start_ptr += strlen("\"Action=");
        sprintf(save_submit_data_buf, "%s\"URL=%s", save_submit_data_buf, save_submit_data_start_ptr);
      } 
      else if((save_submit_data_start_ptr = strstr(buf, "LAST);"))) {
        strncat(save_submit_data_buf, start_ptr, save_submit_data_start_ptr - start_ptr);
        save_submit_data_start_ptr += 4;
        if (itemdata_found)
          sprintf(save_submit_data_buf, "%sITEMDATA_END\n\t\t%s", save_submit_data_buf, save_submit_data_start_ptr);
        else
          sprintf(save_submit_data_buf, "%s%s", save_submit_data_buf, save_submit_data_start_ptr);
        fputs(save_submit_data_buf, ns_fp);
        save_submit_data_flag = 0;
        itemdata_found = 0;
      } else if((save_submit_data_start_ptr = strstr(buf, "web_url")) != NULL) {
        strncat(save_submit_data_buf, start_ptr, save_submit_data_start_ptr - start_ptr);
        save_submit_data_start_ptr += strlen("web_url");
        sprintf(save_submit_data_buf, "%sns_web_url%s", save_submit_data_buf, save_submit_data_start_ptr);
        if (param_api_counter)
        {
        header_key=copy_function(new_buf);
        save_ns_param(header_key);
        param_api_counter=0;
        }
      } else if((save_submit_data_start_ptr = strstr(buf, "\"RecContentType="))) {
        strncat(save_submit_data_buf, start_ptr, save_submit_data_start_ptr - start_ptr);
        save_submit_data_start_ptr += strlen("\"RecContentType=");
        sprintf(save_submit_data_buf, "%s\"HEADER=Content-Type: %s", save_submit_data_buf, save_submit_data_start_ptr);
      } else if((save_submit_data_start_ptr = strstr(buf, "\"Referer="))) {
          strncat(save_submit_data_buf, start_ptr, save_submit_data_start_ptr - start_ptr);
          save_submit_data_start_ptr += 9;
          sprintf(save_submit_data_buf, "%s\"HEADER=Referer: %s", save_submit_data_buf, save_submit_data_start_ptr);
     
          start_ptr = save_submit_data_buf;
          
          if((ptr = strstr(start_ptr, "ENDITEM")) != NULL) {
          snprintf(prev_buf, ptr - start_ptr + 1, "%s", start_ptr);
          ptr += 8;
          sprintf(save_submit_data_buf, "%s%s", prev_buf, ptr);
          }

          start_ptr= save_submit_data_buf;
          if((ptr = strstr(save_submit_data_buf, "Url"))){
            snprintf(prev_buf, ptr - start_ptr + 1, "%s", start_ptr);
            ptr += strlen("Url");
            sprintf(save_submit_data_buf, "%sURL%s", prev_buf, ptr);
          }
      }
      else if((save_submit_data_start_ptr = strstr(buf, "EXTRARES,"))){
        strncat(save_submit_data_buf, start_ptr, save_submit_data_start_ptr - start_ptr);
        save_submit_data_start_ptr += strlen("EXTRARES,");
        if(itemdata_found)
          sprintf(save_submit_data_buf, "%sITEMDATA_END,\nINLINE_URLS,%s", save_submit_data_buf, save_submit_data_start_ptr);
        else
        {
          sprintf(save_submit_data_buf, "%sINLINE_URLS,%s", save_submit_data_buf, save_submit_data_start_ptr);
          itemdata_found = 1;
        }
      }
      else if(strstr(buf, "ITEMDATA,")){
        if(itemdata_found)
          sprintf(save_submit_data_buf, "%sITEMDATA_END,\n%s", save_submit_data_buf, buf);
        else
        {
          sprintf(save_submit_data_buf, "%s%s", save_submit_data_buf, buf);
          itemdata_found = 1;
        }
      }
      else if((save_submit_data_start_ptr = strstr(buf, "ENDITEM"))){
        strncat(save_submit_data_buf, start_ptr, save_submit_data_start_ptr - start_ptr);
        save_submit_data_start_ptr += strlen("ENDITEM,");
        sprintf(save_submit_data_buf, "%s%s", save_submit_data_buf, save_submit_data_start_ptr);
      } 
      else{
        sprintf(save_submit_data_buf, "%s%s", save_submit_data_buf, buf);
      }
      continue;
    }
    if((ptr = strstr(buf, "vuser_init")) != NULL) {
      ptr += strlen("vuser_init");
      sprintf(new_buf, "int init_script%s", ptr);
    }
    else if((ptr = strstr(buf, "vuser_end")) != NULL) {
      ptr += strlen("vuser_end");
      sprintf(new_buf, "int exit_script%s", ptr);
    }
    else if((ptr = strstr(buf, "lr_start_transaction")) != NULL) {
      convert_LR_line_to_NS_line(start_ptr, "lr_start_transaction", "ns_start_transaction", new_buf);
      start_ptr = new_buf;
      if((ptr = strstr(start_ptr, "lr_eval_string")) != NULL)
        convert_LR_line_to_NS_line(start_ptr, "lr_eval_string", "ns_eval_string", new_buf);
    }
    else if((ptr = strstr(buf, "lr_start_sub_transaction")) != NULL) {
      convert_LR_line_to_NS_line(start_ptr, "lr_start_sub_transaction", "ns_start_transaction", new_buf); 
      start_ptr = new_buf;
      if((ptr = strstr(start_ptr, "lr_eval_string")) != NULL) 
        convert_LR_line_to_NS_line(start_ptr, "lr_eval_string", "ns_eval_string", new_buf);
      start_ptr = new_buf;
      if((ptr = strstr(new_buf,","))!= NULL){ 
        snprintf(prev_buf, ptr - start_ptr + 1, "%s", start_ptr); 
        end_ptr= strchr(new_buf,')');
        sprintf(new_buf, "%s%s", prev_buf, end_ptr);
      }
    }
    else if(((ptr = strstr(buf, "lr_end_transaction")) != NULL ) || ((ptr = strstr(buf, "lr_end_sub_transaction")) != NULL )) {
      if(!strncmp(ptr, "lr_end_transaction", strlen("lr_end_transaction")))
        convert_LR_line_to_NS_line(start_ptr, "lr_end_transaction", "ns_end_transaction", new_buf);
      else
        convert_LR_line_to_NS_line(start_ptr, "lr_end_sub_transaction", "ns_end_transaction", new_buf);
      start_ptr = new_buf;
      if((ptr = strstr(start_ptr, "LR_PASS")) != NULL) 
        convert_LR_line_to_NS_line(start_ptr, "LR_PASS", "NS_SUCCESS_STATUS", new_buf);
      if((ptr = strstr(start_ptr, "LR_AUTO")) != NULL)
        convert_LR_line_to_NS_line(start_ptr, "LR_AUTO", "NS_AUTO_STATUS", new_buf);
    }
    else if((ptr = strstr(buf, "web_url")) != NULL) { 
      convert_LR_line_to_NS_line(start_ptr, "web_url", "ns_web_url", new_buf); 
      
      header_key=copy_function(new_buf);
      second_ptr=change_special_char_in_name(header_key,tmp_buf);

      start_ptr = new_buf;
      ptr = strstr(new_buf, "\"");
      snprintf(prev_buf, ptr - start_ptr + 2, "%s", new_buf);
      sprintf(new_buf, "%s%s\",\n", prev_buf, second_ptr);

      if (param_api_counter)
      {
        save_ns_param(second_ptr);
        param_api_counter=0;
      }
    }
    else if((ptr = strstr(buf, "web_custom_request")) != NULL) {
      convert_LR_line_to_NS_line(start_ptr, "web_custom_request", "ns_web_url", new_buf);
      
      header_key=copy_function(new_buf);
      second_ptr=change_special_char_in_name(header_key, tmp_buf);
       
      start_ptr = new_buf;
      ptr = strstr(new_buf, "\"");
      snprintf(prev_buf, ptr - start_ptr + 2, "%s", new_buf);
      sprintf(new_buf, "%s%s\",\n", prev_buf, second_ptr);

      if (param_api_counter)
      {
        save_ns_param(second_ptr);
        param_api_counter=0;
      }
    }
    else if((ptr = strstr(buf, "web_submit_data")) != NULL) {
      convert_LR_line_to_NS_line(start_ptr, "web_submit_data", "ns_web_url", new_buf); 
      
      header_key=copy_function(new_buf);
      second_ptr=change_special_char_in_name(header_key, tmp_buf);

      start_ptr = new_buf;
      ptr = strstr(new_buf, "\"");
      snprintf(prev_buf, ptr - start_ptr + 2, "%s", new_buf);
      sprintf(new_buf, "%s%s\",\n", prev_buf, second_ptr);

      if (param_api_counter)
      {
        save_ns_param(second_ptr);
        param_api_counter=0;
      }
      fputs(new_buf, ns_fp);
      save_submit_data_flag = 1;
      save_submit_data_buf[0] = '\0';
      new_buf[0] = '\0';
    }
    else if((ptr = strstr(buf, "LR_AUTO")) != NULL)
      convert_LR_line_to_NS_line(start_ptr, "LR_AUTO", "NS_AUTO_STATUS", new_buf);
    else if((ptr = strstr(buf, "lr_save_string")) != NULL) {
      convert_LR_line_to_NS_line(start_ptr, "lr_save_string", "ns_save_string", new_buf);
      start_ptr= new_buf;
      if((ptr = strstr(start_ptr, "lr_eval_string")) != NULL)
        convert_LR_line_to_NS_line(start_ptr, "lr_eval_string", "ns_eval_string", new_buf);
    }
    else if((ptr = strstr(buf, "lr_think_time")) != NULL) {
      convert_LR_line_to_NS_line(start_ptr, "lr_think_time", "ns_page_think_time", new_buf);      
      start_ptr= new_buf;
        if((ptr = strstr(new_buf, "thinktime")) != NULL)
          convert_LR_line_to_NS_line(start_ptr, "thinktime", "0.0", new_buf);
    }
    else if((ptr = strstr(buf, "lr_exit")) != NULL)
      convert_LR_line_to_NS_line(start_ptr, "lr_exit", "ns_exit_session", new_buf);
    else if((ptr = strstr(buf, "lr_save_searched_string")) != NULL) {
      convert_LR_line_to_NS_line(start_ptr, "lr_save_searched_string", "ns_save_searched_string", new_buf);
      start_ptr = new_buf;
      if((ptr = strstr(start_ptr, "lr_eval_string")) != NULL)
        convert_LR_line_to_NS_line(start_ptr, "lr_eval_string", "ns_eval_string", new_buf);
    }
    else if((ptr = strstr(buf, "web_add_header")) != NULL)
    {
        if((end_found=strchr(ptr, ')'))!= NULL){
          start_found = strchr(ptr, '(');
          start_found++;
          *end_found ='\0';
          sprintf(new_buf, "\tns_web_add_header(%s,0);\n", start_found);
        }
        else
        {
          start_found = strchr(ptr, '(');
          CLEAR_WHITE_SPACE_AND_TAB_FROM_END(start_found);
          sprintf(header_buf, "\tns_web_add_header%s", start_found);      
          web_half_header_flag = 1;
          continue;
        }
    } 
    else if(web_half_header_flag) {
        if((end_found=strchr(buf, ')')) == NULL)
        {
          ptr = strstr(buf, "\"");
          sprintf(header_buf, "%s%s", new_buf, ptr);
          CLEAR_WHITE_SPACE_AND_TAB_FROM_END(header_buf);
          header_buf[strlen(header_buf) - 1] = '\0';
          continue;
        }
        else
        {
          CLEAR_WHITE_SPACE(start_ptr);
          end_found = strchr(start_ptr, ')');
          *end_found='\0';
          sprintf(new_buf, "%s%s, 0);\n", header_buf,start_ptr );
          web_half_header_flag = 0;
        }
    }
    else if((ptr = strstr(buf, "lr_replace")) != NULL)
      convert_LR_line_to_NS_line(start_ptr, "lr_replace", "ns_replace", new_buf);
    else if((ptr = strstr(buf, "lr_user_data_point")) != NULL)
      convert_LR_line_to_NS_line(start_ptr, "lr_user_data_point", "ns_add_user_data_point", new_buf);
    else if((ptr = strstr(buf, "lr_advance_param")) != NULL)
      convert_LR_line_to_NS_line(start_ptr, "lr_advance_param", "ns_advance_param", new_buf);
    else if((ptr = strstr(buf, "ExitAction")) != NULL)
      convert_LR_line_to_NS_line(start_ptr, "ExitAction", "ns_exit_session", new_buf);
    else if((ptr = strstr(buf, "lr_eval_string")) != NULL)
      convert_LR_line_to_NS_line(start_ptr, "lr_eval_string", "ns_eval_string", new_buf);
    else if((ptr = strstr(buf, "\"Referer="))) {
      snprintf(prev_buf, ptr - start_ptr + 1, "%s", start_ptr);
      ptr += strlen("\"Referer=");
      if(*ptr == '"')
        continue;
      sprintf(new_buf, "%s\"HEADER=Referer: %s", prev_buf, ptr);
      start_ptr= new_buf;
        if((ptr = strstr(new_buf, "ENDITEM")) != NULL){
          snprintf(prev_buf, ptr - start_ptr + 1, "%s", start_ptr);
          ptr += strlen("ENDITEM,");
          sprintf(new_buf, "%s%s", prev_buf, ptr);
        }
      start_ptr= new_buf;
        if((ptr = strstr(new_buf, "Url")) != NULL)
          convert_LR_line_to_NS_line(start_ptr, "Url", "URL", new_buf);
    }
    else if((ptr = strstr(buf, "\"RecContentType=")))
      convert_LR_line_to_NS_line(start_ptr, "\"RecContentType=", "\"HEADER=Content-Type: ", new_buf);
    else if((ptr = strstr(buf, "web_add_auto_header")) != NULL)
    {
        char *temp ;
        
        NSLB_MALLOC(temp, 1024, "header memory", -1, NULL );
        if(strchr(ptr, ')')){
          second_ptr= copy_function(start_ptr +1 );

          start_found = strchr(second_ptr, '"');
          end_found = strrchr(second_ptr, '"');

          snprintf(new_buf, start_found - second_ptr + 1,"%s", second_ptr);
          
          strcpy(header_buf, new_buf);
          strcat(new_buf, ":");
          strcat(new_buf, end_found +1);

          sprintf(temp, "\"HEADER=%s\",\n", new_buf);
          nslb_map_insert_lr(header_map, header_buf, (void*)temp);
          continue;
        }
        else
        {
          header_key= copy_function(start_ptr);
          sprintf(header_buf, "\"HEADER=%s", header_key);
          half_header_flag = 1;
          continue;
        }
    }
    else if(half_header_flag)
    {
        char *temp ;
        NSLB_MALLOC(temp, 1024, "header memory", -1, NULL );
        auto_header_ptr= copy_function(start_ptr);
        sprintf(temp, "%s:%s\",\n", header_buf, auto_header_ptr);
        header_count=nslb_map_insert_lr(header_map, header_key, (void*)temp);
        half_header_flag = 0;
        continue;
    }
    else if(strstr(buf, "web_revert_auto_header")) {
        header_key = copy_function(start_ptr);
        header_count= nslb_map_delete(header_map, header_key);
       continue;
    }
    else if(strstr(buf, "web_set_sockets_option") || strstr(buf, "\"Resource=") || strstr(buf, "\"EncType=") || strstr(buf, "\"Mode=") || strstr(buf, "\"TargetFrame="))
        continue;
    else if((ptr = strstr(buf, "web_reg_save_param")) || (ptr = strstr(buf, "web_reg_find"))) 
    {
      if (param_api_counter >= max_entries)
      {
        api_attr = (NSApi *)realloc(api_attr, ((max_entries + DELTA_PARAM_SIZE) * sizeof(NSApi)));
        max_entries += DELTA_PARAM_SIZE;
      }
      start_ptr = ptr;
      if(strstr(buf,"web_reg_find"))
      {
        start_ptr+=12;
        strcpy(api_attr[param_api_counter].api_name, "web_reg_find");
      }
      else if(strstr(buf,"web_reg_save_param_regexp")) 
      {
        start_ptr += 25; 
        strcpy(api_attr[param_api_counter].api_name, "web_reg_save_param_regexp");
      }
      else if(strstr(buf,"web_reg_save_param_ex"))
      {
        start_ptr += 21;
        strcpy(api_attr[param_api_counter].api_name, "web_reg_save_param_ex");
      }
      else if(strstr(buf,"web_reg_save_param"))
      {
        start_ptr += 18;
        strcpy(api_attr[param_api_counter].api_name, "web_reg_save_param");
      }
  
      if (*start_ptr == '(') 
      {
        *start_ptr = '\0'; 
        start_ptr++;
      }
      CLEAR_WHITE_SPACE(start_ptr);
      CLEAR_WHITE_SPACE_AND_TAB_FROM_END(start_ptr);
      
      end_ptr = strstr(start_ptr, "LAST");
      if (end_ptr != NULL)
      {
        *end_ptr = '\0';
      }
      else
        multiline_found = 1;
 
      strcpy(tmp_buf, start_ptr);
      read_line_argument_and_fill_attr_table(tmp_buf, param_api_counter);
      if (!multiline_found) 
        param_api_counter++;
      continue;
    }
    else if (multiline_found)
    {
      CLEAR_WHITE_SPACE(start_ptr);
      CLEAR_WHITE_SPACE_AND_TAB_FROM_END(start_ptr);
      end_ptr = strstr(start_ptr, "LAST");
      
      if (end_ptr != NULL)
      {
        *end_ptr = '\0';
        multiline_found = 0;
        if (start_ptr == NULL)
        {
          param_api_counter++;
          continue;
        }
      }
      read_line_argument_and_fill_attr_table(start_ptr, param_api_counter);
      
      if (!multiline_found) 
        param_api_counter++;
      continue;
    }
    else if((ptr = strstr(buf, "\"URL")) != NULL){
      strcpy(new_buf, buf);
      CLEAR_WHITE_SPACE_AND_TAB_FROM_END(buf);
      if (buf[strlen(buf) - 1] != ',')
      {
        buf[strlen(buf) - 1] = '\0';
        strcpy(new_buf, buf);
        URL_multiliner = 1;
        continue;
      }
      else
      {
        start_ptr = new_buf;  
        URL_multiliner = 0;
        header_count= nslb_get_map_last_index(header_map);
        for(int idx =0; idx <= header_count; ++idx)
        {
          auto_header_ptr = GET_MAP_VALUE(header_map, idx);
          if (auto_header_ptr != NULL)
            sprintf(start_ptr, "%s\t\t%s", start_ptr, auto_header_ptr);
        }
        strcpy(new_buf,start_ptr);
      }
    }
    else if(URL_multiliner)
    {
      SKIP_BLANK_LINE(start_ptr);
      ptr = strstr(buf, "\"");
      ptr++;
      sprintf(new_buf, "%s%s", new_buf, ptr);
      CLEAR_WHITE_SPACE_AND_TAB_FROM_END(new_buf);
      if (new_buf[strlen(new_buf) - 1] != ',')
      {
        new_buf[strlen(new_buf) - 1] = '\0';
        continue;
      }
      else
      {
        URL_multiliner = 0;
        start_ptr = new_buf;
        header_count= nslb_get_map_last_index(header_map);
        for(int idx =0; idx <= header_count; ++idx)
        {
          auto_header_ptr = GET_MAP_VALUE(header_map, idx);
          if (auto_header_ptr != NULL)
            sprintf(start_ptr, "%s\t\t%s", start_ptr, auto_header_ptr);
        }
        strcpy(new_buf,start_ptr);
      }
    }
    else if((!multiline_found) && ((ptr = strstr(buf, "LAST")) != NULL)) {
      if (itemdata_found)   
        convert_LR_line_to_NS_line(start_ptr, "LAST", "ITEMDATA_END", new_buf);
      else{
        convert_LR_line_to_NS_line(start_ptr, "LAST", "", new_buf);
        itemdata_found = 0;
      }
    }
    else if((ptr = strstr(buf, "EXTRARES,")) != NULL){
     if (itemdata_found)   
        convert_LR_line_to_NS_line(start_ptr, "EXTRARES,", "ITEMDATA_END,\n\tINLINE_URLS,", new_buf);
      else{
        convert_LR_line_to_NS_line(start_ptr, "EXTRARES,", "INLINE_URLS,", new_buf);
        itemdata_found = 1;
      }
    }
    else if((ptr = strstr(buf, "ITEMDATA,"))!= NULL){
      if(itemdata_found)
        sprintf(new_buf, "ITEMDATA_END,\n%s", buf);
      else{
        sprintf(new_buf, "%s", buf);
        itemdata_found = 1;
      }
    }
    else if((ptr = strstr(buf, "ENDITEM")) != NULL){
      convert_LR_line_to_NS_line(start_ptr, "ENDITEM,", "", new_buf);
      if((ptr = strstr(buf, "Url")) != NULL)
        convert_LR_line_to_NS_line(start_ptr, "Url", "URL", new_buf);
    }
    else if((ptr = strstr(buf, "\"Body=")) != NULL){
      if(itemdata_found){
        convert_LR_line_to_NS_line(start_ptr, "\"Body=", "ITEMDATA_END,\nBODY_BEGIN,\n\t\"", new_buf);
        itemdata_found = 0;
      }
      else
        convert_LR_line_to_NS_line(start_ptr, "\"Body=", "BODY_BEGIN,\n\t\"", new_buf);
      CLEAR_WHITE_SPACE_AND_TAB_FROM_END(new_buf);
      if (new_buf[strlen(new_buf) - 1] != ',')
      {
        new_buf[strlen(new_buf) - 1] = '\0';
        Body_multiliner = 1;
        continue;
      }
      else
      {
        Body_multiliner = 0;
        strcat(new_buf,"\n\tBODY_END\n");
      }
    }
    else if(Body_multiliner)
    {
      SKIP_BLANK_LINE(start_ptr);
      ptr = strstr(buf, "\"");
      ptr++;
      sprintf(new_buf, "%s%s", new_buf, ptr);
      CLEAR_WHITE_SPACE_AND_TAB_FROM_END(new_buf);
      if (new_buf[strlen(new_buf) - 1] != ',')
      {
        new_buf[strlen(new_buf) - 1] = '\0';
        continue;
      }
      else
      {
        Body_multiliner = 0;
        strcat(new_buf,"\n\tBODY_END\n");
      }
    }
    else {
      fputs(buf, ns_fp);
      continue;
    }
    if(!save_submit_data_flag)
      fputs(new_buf, ns_fp);
  }
  fclose(ns_fp);
  fclose(lr_fp);
  nslb_map_destroy(header_map);
}

int get_all_c_type_files(char *lr_script_path, char *ns_script_path)
{
  DIR *d;
  struct dirent *dir;
  char cmd[512];
  int total_c_script_files=0, len =0;

  d = opendir(lr_script_path);
  if(d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      if(dir->d_type == DT_DIR)
      {  
         if(dir->d_name[0] =='.')
           continue;
         printf("\nDIR NAME [%s]\n", dir->d_name);
         sprintf(cmd,"cp -r %s/%s %s/%s", lr_script_path, dir->d_name, ns_script_path, dir->d_name);
         system(cmd);
      }
      else if(dir->d_type == DT_REG)
      {
        if(strstr(dir->d_name, ".usp"))
         continue;

        len = strlen(dir->d_name);
        if ((dir->d_name[ len -2] == '.') && (dir->d_name[ len - 1] == 'c'))
        {
          strcpy(c_flow_arr[total_c_script_files++], dir->d_name);
          if(total_c_script_files == MAX_FLOW)
          {
            printf("flow file in script is more than %d.", MAX_FLOW);   
          }
        }
        else
        {
          sprintf(cmd,"cp %s/%s %s/%s", lr_script_path, dir->d_name, ns_script_path, dir->d_name);
          system(cmd);
        }
      }
    }
    closedir(d);
  }
  total_c_file_to_process= total_c_script_files;
  return 0;
}

int main(int argc, char *argv[])
{
  char lr_script_path[1024] = {0};
  char lr_filename[1024] = {0};
  char cmd[512]={0};
  char ns_script_path[1024] = {0};
  char ns_filename[1024] = {0};
  int i, c, lr_script_flag = 0, ns_script_flag = 0;
  FILE *lr_script_fp = NULL;
  FILE *ns_script_fp = NULL;

  if(argc > 5)
  {
    fprintf(stderr, "Please provide valid options like: nsu_lr_to_ns_script_converter -l <lr script path> -n <ns script path>\n");
    exit(-1);
  }

  while ((c = getopt(argc, argv, "l:n:?")) != -1) {
    switch (c) {
    case 'l':
      if(lr_script_flag) {
        fprintf(stderr, "Error: Multiple -l option passed\nPlease provide valid options like: nsu_lr_to_ns_script_converter -l <lr script path> -n <ns script path>\n");
        exit(-1);
      }
      strcpy(lr_script_path, optarg);
      lr_script_flag = 1;
      break;

    case 'n':
       if(ns_script_flag) {
        fprintf(stderr, "Error: Multiple -n option passed\nPlease provide valid options like: nsu_lr_to_ns_script_converter -l <lr script path> -n <ns script path>\n");
        exit(-1);
      }
      strcpy(ns_script_path, optarg);
      ns_script_flag = 1;
      break;

     case '?':
      nsu_lr_to_ns_script_converter_usage("Invalid arguments.");
    }
  }

  if((lr_script_flag != 1) || (ns_script_flag != 1))
    nsu_lr_to_ns_script_converter_usage("Mandatory argument missing!");

  //get all c files on path provided by -l
  check_run_logic_file_in_LR(lr_script_path, ns_script_path);
  //create runlogic.c from defaults.cfg
  sprintf(lr_filename, "%s/%s", lr_script_path, runlogic);
  create_runlogic_file(lr_filename, ns_script_path);

  sprintf(ns_filename, "%s/registrations.spec", ns_script_path);
  ns_reg_fp = open_file(ns_filename, "w");

  get_all_c_type_files(lr_script_path, ns_script_path);

#if 0
  for(i = 0; i < total_c_file_to_process ; i++)
  {
    fprintf(stderr, "%d: %s\n" ,i, c_flow_arr[i]);
  }
#endif

  for(i = 0; i < total_c_file_to_process ; i++)
  {
    sprintf(lr_filename, "%s/%s", lr_script_path, c_flow_arr[i]);
    lr_script_fp = open_file(lr_filename, "r");

    if(!strcmp(c_flow_arr[i], "vuser_init.c"))
      sprintf(ns_filename, "%s/init_script.c", ns_script_path);
    else if(!strcmp(c_flow_arr[i], "vuser_end.c"))
      sprintf(ns_filename, "%s/exit_script.c", ns_script_path);
    else
      sprintf(ns_filename, "%s/%s", ns_script_path, c_flow_arr[i]);

    ns_script_fp = open_file(ns_filename, "w");

    fprintf(ns_script_fp, "/*-----------------------------------------------------------------------------\n"
                          "\tName: %s\n\tRecorded By: Converted via tool\n\tDate of recording:\n\tFlow details:\n"
                          "\tBuild details:\n\tModification History:\n-------------------------------------------"
                          "----------------------------------*/\n#include <stdio.h>\n#include <stdlib.h>\n"
                          "#include <string.h>\n#include \"ns_string.h\"\n\n", c_flow_arr[i]);
    convert_data_into_ns_comp(ns_script_fp, lr_script_fp);
  }
  fclose(ns_reg_fp);

  // to copy whole LR script in NS script with .<LR_script_name> as backup.
  if(lr_script_path[strlen(lr_script_path) - 1] == '/')
    lr_script_path[strlen(lr_script_path) - 1]='\0';
  sprintf(cmd,"cp -r %s %s/.%s", lr_script_path ,ns_script_path , basename(lr_script_path));
  system(cmd);

  printf("Converted! Script = %s\n", ns_script_path);

  return 0;
}
