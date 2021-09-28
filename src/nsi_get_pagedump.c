/**
 * File: nsi_get_pagedump.c
 * Purpose:
 *      The file reades page_dump.txt and generates output according to 
 * options supplised to it. 
 *
 * -> Should be backward compatible with old format.
 * -> Return unique page names
 * -> Return unique script names
 * -> Return all fields with page status other than "Success"
 * -> Return all fields with session status other than "Success"
 * -> Return all where page name matches
 * -> Return all where script name matches
 * -> Give sorted Data All the time.
 *
 * New : StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|RecordedPageUrl|SessStatus|Req|RepBody|Rep|ParameterSubstitution
 * Old : StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|Req|RepBody|Rep|ParameterSubstitution
 *
 * In release 3.8.5, changes are done to pagedump header format in current version new field has been added TraceLevel
 * For implementation of runtime changes in GUI, trace-level was required to handle PageDump GUI.
 * New : StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|RecordedPageUrl|SessStatus|Req|RepBody|Rep|TraceLevel|ParameterSubstitution
 *
 * Now old format would be one without TraceLevel, now we will be filling that field with max trace level (4)
 * Old : StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|Req|RepBody|Rep|ParameterSubstitution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "nslb_util.h" //for function get_tokens
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include "nslb_big_buf.h"

#define MAX_LINE_LENGTH 2048
#define MAX_LINE_LENGTH2 2097152 /* 2*1024*1024 = 2mb */
#define MAX_VALUE_SIZE 256
#define MAX_VALUE_LENGTH 32

/* These macros used for dump version. Old and  NEW */
#define FORMAT_TYPE_OLD 0
#define FORMAT_TYPE_NEW 1

char wdir[1024]; //Keep NS_WDIR

typedef struct {
  char StartTime[MAX_VALUE_LENGTH];
  int start_time;
  int nvm_id;
  short gen_id;
  char SessionInstance[MAX_VALUE_LENGTH];
  int sess_inst;
  char UserId[MAX_VALUE_LENGTH];
  int user_id;
  int trace_level; /*Added new field for runtime changes*/
  char group[MAX_VALUE_SIZE];
  char ScriptName[MAX_VALUE_SIZE];
  //Ajeet: added for flow file name to support C Type scripts
  char flow_name[MAX_VALUE_SIZE];
  char ScriptURL[MAX_VALUE_SIZE];
  char PageName[MAX_VALUE_SIZE];
  char RecordedPageUrl[MAX_VALUE_SIZE]; 
  char PageDumpUrl[MAX_VALUE_SIZE];
  char PageStatus[MAX_VALUE_LENGTH];
  char SessStatus[MAX_VALUE_LENGTH];
  char Req[MAX_VALUE_SIZE];      /* To parse url_req_*.dat */
  char Rep_body[MAX_VALUE_SIZE];  /* To parse url_rep_body_*.dat */
  char Rep[MAX_VALUE_SIZE];  /* To parse url_rep_*.dat */
  long ParameterSubstitution; /* index in big buffer */
} page_info;


struct options {
  int testrun_number;
  int all_flag;
  int failed_page_flag;
  int failed_sess_flag;
  int page_flag;
  int script_flag;
  char script_name[MAX_VALUE_SIZE];
  char page_name[MAX_VALUE_SIZE];
  unsigned int limit;
  int page_to_show;
  int child_idx;
  int gen_idx; 
  int sess_inst; 
} opts;

page_info *pinfo = NULL;
int num_rows = 0;

/* will be used to store ParameterSubstitution */
static bigbuf_t pd_bigbuf; 

char g_dump_version = -1;       /* can have two values: 0 for Older version 1 for New version */

void usage()
{
  printf("nsi_get_pagedump <options>\n");
  printf("\t<options>:\n");
  printf("\t\t-t or --testrun <testrun number>\n");
  printf("\t\t-g or --gen_id <generator id>\n");
  printf("\t\t-s or --script <script name>           : Prints all script name matches\n");
  printf("\t\t-p or --page <page name>               : Prints all page name matches\n");
  printf("\t\t-f or --prt_failed_obj page            : Prints all failed pages\n");
  printf("\t\t-f or --prt_failed_obj session         : Prints all failed scripts\n");
  printf("\t\t-u or --prt_uniq_obj page              : Prints unique pages\n");
  printf("\t\t-u or --prt_uniq_obj script            : Prints unique scripts\n");
  printf("\t\t-u or --prt_uniq_obj script_page       : Prints unique script_pages\n");
  printf("\t\t-a or --prt_all                        : Prints all\n");
  printf("\t\t-C or --child <Child Index>            : NVM ID\n");
  printf("\t\t-S or --sessioninst <Session Instance>\n");
}


void check_dump_version(char *line)
{
  char *ptr = line;
  char *page_dump_header[20]; // To store field value from page_dump.txt file
  int count = 0;

  count = get_tokens(ptr, page_dump_header, "|", 20);
  if(count != 15 && count != 16){
    fprintf(stderr, "Error: page_dump.txt file is not in its format"); 
    exit(-1);
  }
  
  if (count == 15)
    g_dump_version = FORMAT_TYPE_OLD;
  else 
    g_dump_version = FORMAT_TYPE_NEW; 
  
}



#define CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}

#define ParameterSubstitutionValue(i) NSLB_BIGBUF_GET_VALUE((&pd_bigbuf), pinfo[i].ParameterSubstitution) 

static void fill_data_into_struct(FILE *fp)
{
  /*buf size have been taken 2 mb because max length of line in log file will be 1 mb(limitation in ns) */
  char buf[MAX_LINE_LENGTH2 + 1];
  int i;
  char *page_dump_field[25]; // To store field value from page_dump.txt file in parameter there may be many | so taken array large
  char *ptr = buf;
  char TraceLevel[MAX_VALUE_LENGTH];
  unsigned short gen_nvm_id;
  char net_cloud_file_path[100];
  char file_data[1024];
  char sorted_tr_scenario_path[100];
  FILE *gen_tr_fptr;
  char *gen_field[100];
  char *temp_ptr;
  int gen_count = 0;
  int netcloud_test = 1;
  char gen_name[100][100];
  char cmd[1024]="\0";
  char err_msg[1024] = "\0";

  sprintf(sorted_tr_scenario_path, "%s/logs/TR%d/sorted_scenario.conf", wdir, opts.testrun_number);  
  sprintf(cmd, "grep -w ^NS_GENERATOR %s 1>/dev/null 2>/dev/null", sorted_tr_scenario_path);
  netcloud_test=nslb_system(cmd,1,err_msg);
  // For NetCloud test need to get gen id and gen name for sorting purpose 
  if(netcloud_test == 0) //NetCloud test
  {
    sprintf(net_cloud_file_path, "%s/logs/TR%d/NetCloud/NetCloud.data", wdir, opts.testrun_number);
    if ((gen_tr_fptr = fopen(net_cloud_file_path, "r")) == NULL)
    {
      fprintf(stderr, "Error in opening file %s.\n", net_cloud_file_path);
      return;
    }
    while (fgets(file_data, 1024, gen_tr_fptr) != NULL)
    {
      if(!strncmp (file_data, "NETCLOUD_GENERATOR_TRUN ", strlen("NETCLOUD_GENERATOR_TRUN ")))
      {
        temp_ptr = file_data;
        temp_ptr = temp_ptr + strlen("NETCLOUD_GENERATOR_TRUN ");
        get_tokens(temp_ptr, gen_field, "|", 10);
        strcpy(gen_name[atoi(gen_field[6])], gen_field[1]);
        gen_count++;
      } 
    }
  }

    //StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|SessStatus|Req|RepBody|Rep|ParameterSubstitution     In old version only 14 field in page_dump.txt

  //StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|SessStatus|Req|RepBody|Rep|ParameterSubstitution     In new version 15 field in page_dump.txt
  /* In release 3.8.5, format:
   * StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|SessStatus|Req|RepBody|Rep
   * |ParameterSubstitution     In old version 15 field in page_dump.txt 
   *
   * Added new field TraceLevel for rtc
   * StartTime|SessionInstance|UserId|Group|ScriptName|ScriptURL|PageName|PageDumpUrl|PageStatus|SessStatus|Req|RepBody|Rep
   * |TraceLevel|ParameterSubstitution     In new version 16 field in page_dump.txt
   * */
  while (fgets(buf, MAX_LINE_LENGTH2, fp) != NULL) {

    /*Ignore blank lines*/
    CLEAR_WHITE_SPACE(ptr);
    if(buf[0] == '\n'){
      continue;
    }
    num_rows++;
    if ((pinfo = realloc(pinfo, sizeof(page_info) * num_rows)) == NULL) {
      fprintf(stderr, "Unable to realloc. Out of Memory. Exiting.\n");
      exit(-1);
    }

    i = num_rows - 1;
    strcpy(pinfo[i].StartTime, page_dump_field[0]);
    pinfo[i].start_time = get_time_from_format(pinfo[i].StartTime);

    strcpy(pinfo[i].SessionInstance, page_dump_field[1]);
    sscanf(pinfo[i].SessionInstance, "%d:%d", &(pinfo[i].nvm_id), &(pinfo[i].sess_inst));

    strcpy(pinfo[i].UserId, page_dump_field[2]);
    sscanf(pinfo[i].UserId, "%hu:%d", &gen_nvm_id, &(pinfo[i].user_id));
    // Done to fetcg nvm id and gen id
    pinfo[i].nvm_id=(int)(gen_nvm_id & 0x00FF);
    pinfo[i].gen_id=(int)((gen_nvm_id & 0xFF00) >> 8);
    // Check put for checking test is standalone or netcloud test.In case of standlaone test, gen name will not come 
    // In case of NS gen name:nvmid:userid will come
    if(netcloud_test == 0)
    {
      sprintf(pinfo[i].UserId, "%s:%d:%d", gen_name[pinfo[i].gen_id], pinfo[i].nvm_id, pinfo[i].user_id);
    }
    else
    {
      sprintf(pinfo[i].UserId, "%d:%d", pinfo[i].nvm_id, pinfo[i].user_id); 
    }

    sprintf(pinfo[i].SessionInstance, "%d:%d", pinfo[i].nvm_id, pinfo[i].sess_inst); 

    strcpy(pinfo[i].group, page_dump_field[3]);


    // script name is in <scriptname:flow_name> format
    // In case of legacy, flow name will be NA
    // In old test runs, flow name may not be there

    // pinfo[i].ScriptName will be like AAAA:YYYY or AAAA:NA or AAAA
    strcpy(pinfo[i].ScriptName, page_dump_field[4]);
    
    char *flow_ptr = index(pinfo[i].ScriptName, ':');
    if(flow_ptr) { // flow name is present. It can also be NA for legacy
      strcpy(pinfo[i].flow_name, flow_ptr + 1);
      *flow_ptr = '\0'; // NULL terminate so that script name does not have flow name
      char *tmp_ptr = index(pinfo[i].flow_name, '.');  // Flow name is c file name flow1.c. So remove .c
      if(tmp_ptr)  *tmp_ptr = '\0';
    } else {
      strcpy(pinfo[i].flow_name, "NA"); // Default to NA
    }

    strcpy(pinfo[i].ScriptURL,page_dump_field[5]);
    strcpy(pinfo[i].PageName, page_dump_field[6]);
    
    if(g_dump_version == FORMAT_TYPE_NEW){
      strcpy(pinfo[i].RecordedPageUrl, page_dump_field[7]);
      strcpy(pinfo[i].PageDumpUrl, page_dump_field[8]);
      strcpy(pinfo[i].PageStatus, page_dump_field[9]);
      strcpy(pinfo[i].SessStatus, page_dump_field[10]);
      strcpy(pinfo[i].Req, page_dump_field[11]);
      strcpy(pinfo[i].Rep_body, page_dump_field[12]);
      strcpy(pinfo[i].Rep, page_dump_field[13]);
      strcpy(TraceLevel, page_dump_field[14]);
      sscanf(TraceLevel, "%d", &(pinfo[i].trace_level));
      if((pinfo[i].ParameterSubstitution = nslb_bigbuf_copy_into_bigbuf(&pd_bigbuf, page_dump_field[15], 0)) == -1)
      {
        fprintf(stderr, "Unable to save ParameterSubstitution in bigbuf. Exiting.\n");
        exit(-1);
      } 

      
    }else{
      strcpy(pinfo[i].RecordedPageUrl, page_dump_field[7]);
      strcpy(pinfo[i].PageDumpUrl, page_dump_field[8]);
      strcpy(pinfo[i].PageStatus, page_dump_field[9]);
      strcpy(pinfo[i].SessStatus, page_dump_field[10]);
      strcpy(pinfo[i].Req, page_dump_field[11]);
      strcpy(pinfo[i].Rep_body, page_dump_field[12]);
      strcpy(pinfo[i].Rep, page_dump_field[13]);
      if((pinfo[i].ParameterSubstitution = nslb_bigbuf_copy_into_bigbuf(&pd_bigbuf, page_dump_field[14], 0)) == -1)
      {
        fprintf(stderr, "Unable to save ParameterSubstitution in bigbuf. Exiting.\n");
        exit(-1);
      } 
      /*Adding trace level field in older format with highest trace level 4*/
      pinfo[i].trace_level = 4;

/*      while (ptr = strtok(NULL, "|")) {     // Remove BUG with parameter with | 
        // We are replacing '|' with  'PIPE_REPLACED_%7C' which will be again replaced with '|' in the GUI. 
        strcat(pinfo[i].ParameterSubstitution, "PIPE_REPLACED_%7C"); 
        strcat(pinfo[i].ParameterSubstitution, ptr);
      }
*/
   }
  }
}

void inline
print_header()
{
  printf("Start Time|Session Instance|User ID|Group|Script Name|Script URL|Page Name|PageDumpUrl|Page Status|RecordedPageUrl|Session Status|Req|RepBody|Rep|TraceLevel|Parameter Substitution\n");

}

void populate_page_info_array()
{
  FILE *fp;
  char file_buf[MAX_LINE_LENGTH + 1];
  char buf[MAX_LINE_LENGTH + 1];
  
  sprintf(file_buf, "%s/logs/TR%d/page_dump.txt", wdir, opts.testrun_number);
  fp = fopen(file_buf, "r");

  if (fp == NULL) {
    //If we dont get page_dump.txt file(This can happen in online mode when we come out imidiatily and 
    //pagedump.txt file still not created) then we will show message in GUI saying that "No data found"
    //For this, GUI needs header and pageNumber line 
    printf("PageNumber=1, TotalPages=0, TotalRecords=0\n"); 
    //fprintf(stderr, "Unable to open page dump file (%s)\n", file_buf);
    exit(0);
  }

  /* Get initial header line.  */
  if (fgets(buf, MAX_LINE_LENGTH, fp) == NULL) {
    fprintf(stderr, "File empty.\n");
    exit(-1);
  }
  
  check_dump_version(buf);
  fill_data_into_struct(fp);
  fclose(fp);

  //Check for page_dump.txt.incomplete file
  //if found then read file and fill into struct
  sprintf(file_buf, "%s/logs/TR%d/page_dump_incomplete.txt", wdir, opts.testrun_number);
  fp = fopen(file_buf, "r");

  if (fp == NULL) {
    //fprintf(stderr, "Unable to open incomplete page dump file (%s)\n", file_buf);
    return;
  }

  fill_data_into_struct(fp);
  fclose(fp);
}


/**
 * Sort first on UserId, then session Instance and then Time.
 */
int page_info_comp(const void *p1, const void *p2) 
{
  page_info *pg1 = (page_info *)p1;
  page_info *pg2 = (page_info *)p2;

  if (pg1->gen_id == pg2->gen_id) {
    if (pg1->nvm_id == pg2->nvm_id) {
      if (pg1->user_id == pg2->user_id) {
        if (pg1->sess_inst == pg2->sess_inst) {
          if (pg1->start_time == pg2->start_time) {
           return 0;
         } else if (pg1->start_time > pg2->start_time) {
           return 1;
         } else if (pg1->start_time < pg2->start_time) {
           return -1;
         }
       } else if (pg1->sess_inst > pg2->sess_inst) {
         return 1;
       } else if (pg1->sess_inst < pg2->sess_inst) {
         return -1;
       }
     } else if (pg1->user_id > pg2->user_id) {
       return 1;
     } else if (pg1->user_id < pg2->user_id) {
       return -1;
     }
    } else if (pg1->nvm_id > pg2->nvm_id) {
     return 1;
    } else if (pg1->nvm_id < pg2->nvm_id) {
      return -1;
    }
  } else if (pg1->gen_id > pg2->gen_id) {
      return 1;
  } else if (pg1->gen_id < pg2->gen_id) {
      return -1;
  }
  /* Should not reach here. */
  return 0;
}

void sort_page_info_array()
{
  qsort(pinfo, num_rows, sizeof(page_info), page_info_comp);
}

void get_recorded_url(char *page_name, char *script_name, char *flow_name, char *recorded_url) {
  char *ptr;
  int len;
  char file_buf[MAX_LINE_LENGTH];
  FILE *fp;
  char buf[MAX_LINE_LENGTH];

  char *pg_name;
  char *path;
  char *host;
  char *rpath;
  char full_url[MAX_LINE_LENGTH];
  if(!(strcasecmp(flow_name,"NA"))){ // Legacy script
    sprintf(file_buf, "%s/logs/TR%d/scripts/%s/dump/index", wdir, opts.testrun_number, script_name);
  }else{
    sprintf(file_buf, "%s/logs/TR%d/scripts/%s/dump/%s/index", wdir, opts.testrun_number, script_name, flow_name);
  } 

  memset(recorded_url, 0, MAX_LINE_LENGTH);
  full_url[0] = '\0';
  
  fp = fopen(file_buf, "r");
  if (fp == NULL) {
    exit(-1);
  }

  while (fgets(buf, MAX_LINE_LENGTH, fp) != NULL) {
    if (strtok(buf, ",") == NULL) return;           /* idx */
    if ((pg_name = strtok(NULL, ",")) == NULL) return;
    if ((path = strtok(NULL, ",")) == NULL) return;
    if ((host = strtok(NULL, ",")) == NULL) return;
    if ((rpath = strtok(NULL, ",")) == NULL) return;

    if ((ptr = strtok(NULL, ",")) != NULL) { /* New version of GUI has this field in index file. */
      strcpy(full_url, ptr);
      while ((ptr = strtok(NULL, ",")) != NULL) {
        strcat(full_url, ",");
      }

      len = strlen(full_url);
      if(full_url[len - 1] == '\n')
        full_url[len - 1] = '\0';
    }

    if (strcmp(pg_name, page_name) == 0) {
      //  if flow name is NA
      if(!(strcasecmp(flow_name,"NA"))){ // Legacy script
        if (full_url[0] != '\0')
          sprintf(recorded_url, "scripts/%s/dump/%s", script_name, full_url);
        else
          sprintf(recorded_url, "scripts/%s/dump/%s%s%s", script_name, host, path, pg_name);
          break;
      }else {
         if (full_url[0] != '\0')
           sprintf(recorded_url, "scripts/%s/dump/%s/%s", script_name, flow_name, full_url);
         else
           sprintf(recorded_url, "scripts/%s/dump/%s/%s%s%s", script_name, flow_name, host, path, pg_name);
           break;
       }
   }
 }
  
  fclose(fp);
}

/*limit = Limit is how many rows need to show in a page
 *page_to_show = which page we need to show (from where we need to show rows).This is for pagination in GUI
 * If values are -1 then there is no limit and no pagination
 * show all rows*/


void apply_filters()
{
  int i;
  unsigned int total_rows = 0;
  unsigned int total_pages = 0;
  int limit = opts.limit;
  int page_to_show = opts.page_to_show;
  unsigned int start_record_from;
  unsigned int end_record;

  start_record_from = limit * (page_to_show - 1);
  end_record = start_record_from + limit - 1;
  
  /* Req, RepBody and Rep fields added at header of page_dump.txt */

// old format
// printf("Start Time|Session Instance|User ID|Group|Script Name|Script URL|Page Name|PageDumpUrl|Page Status|RecordedPageUrl|Session Status|Req|RepBody|Rep|Parameter Substitution\n");

//  printf("Start Time|Session Instance|User ID|Group|Script Name|Script URL|Page Name|RecordedPageUrl|PageDumpUrl|Page Status|Session Status|Req|RepBody|Rep|Parameter Substitution\n");
/* In release 3.8.5, new page_dump.txt header
 * Start Time|Session Instance|User ID|Group|Script Name|Script URL|Page Name|PageDumpUrl|Page Status|RecordedPageUrl|Session Status|Req|RepBody|Rep|TraceLevel|Parameter Substitution 
 * */
  //printf("Start Time|Session Instance|User ID|Group|Script Name|Script URL|Page Name|PageDumpUrl|Page Status|RecordedPageUrl|Session Status|Req|RepBody|Rep|TraceLevel|Parameter Substitution\n");


  for (i = 0; i < num_rows; i++) { /* do everything in single iteration because now we have sorted haystack */

    if (opts.script_flag) {
      if (strcmp(opts.script_name, pinfo[i].ScriptName) != 0)
        continue;
    }
    
    if (opts.page_flag) {
      if (strcmp(opts.page_name, pinfo[i].PageName) != 0)
        continue;
    }
    
    if (opts.failed_page_flag) {
      if (strcmp(pinfo[i].PageStatus, "Success") == 0)
        continue;
    }
    
    if (opts.failed_sess_flag) {
      if (strcmp(pinfo[i].SessStatus, "Success") == 0) 
        continue;
    }
 
    if (opts.child_idx != -1) {
      if (opts.child_idx != pinfo[i].nvm_id) 
        continue;
    }
    
    if (opts.gen_idx != -1) {
      if (opts.gen_idx != pinfo[i].gen_id) 
        continue;
    }
 
    if (opts.sess_inst != -1) {
      if (opts.sess_inst != pinfo[i].sess_inst) 
        continue;
    }
 
    /*In release 3.8.5, old and new format both extract RecordedPageUrl field from page_dump.txt file  
    Hence commenting the code
    if(g_dump_version == FORMAT_TYPE_OLD){ 
      get_recorded_url(pinfo[i].PageName, pinfo[i].ScriptName, pinfo[i].flow_name, recorded_url);
      strcpy(pinfo[i].RecordedPageUrl, recorded_url);
    }
   
    //printf(" num_rows = %d, I = %d, Limit = %u, start_record_from = %u, end_record = %u, total_rows = %d\n", num_rows, i, limit, start_record_from, end_record, total_rows);
    */

    if((total_rows >= start_record_from) && (total_rows <= end_record)){
      /* Req, Rep_body and Rep fields added*/
      printf("%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%d|%s", /* Not appending \n as its already there. */
           pinfo[i].StartTime,
           pinfo[i].SessionInstance,
           pinfo[i].UserId,
           pinfo[i].group,
           pinfo[i].ScriptName,
           pinfo[i].ScriptURL,
           pinfo[i].PageName,
           pinfo[i].PageDumpUrl,
           pinfo[i].PageStatus,
           pinfo[i].RecordedPageUrl,
           pinfo[i].SessStatus,
           pinfo[i].Req,
           pinfo[i].Rep_body,
           pinfo[i].Rep,
           pinfo[i].trace_level,
           /* get value from bigbuf */
           ParameterSubstitutionValue(i));
    }
    total_rows++;
  } //For loop

  total_pages = total_rows/limit;
  int remainder = total_rows%limit;
  if(remainder) 
    total_pages++;
  printf("PageNumber=%d, TotalPages=%d, TotalRecords=%d\n", page_to_show, total_pages, total_rows);
}

void print_unique_pages()
{
  char file_buf[MAX_LINE_LENGTH];
  char cmd[MAX_LINE_LENGTH];
  struct stat buff; 
  char err_msg[1024] = "\0";

  sprintf(file_buf, "%s/logs/TR%d/page_dump.txt", wdir, opts.testrun_number);
  printf("Page Name\n");  /* Gui needs hdr */
  fflush(stdout);
  // validation for checking the file_buf is exist or not
  if(stat(file_buf, &buff) < 0) {
   // check for file_buf is not there
   if(errno == ENOENT)
     return;
  }
  sprintf(cmd, "cat %s | grep -v \"StartTime|SessionInstance\" | cut -d'|' -f7 | sort -u", file_buf);
  nslb_system(cmd,1,err_msg);
}

void print_script_with_page()
{
  char file_buf[MAX_LINE_LENGTH];
  char cmd[MAX_LINE_LENGTH];
  struct stat buff;
  char err_msg[1024] = "\0";

  sprintf(file_buf, "%s/logs/TR%d/page_dump.txt", wdir, opts.testrun_number);
  //validation for checking the file_buf is exist or not
  if(stat(file_buf, &buff) < 0) {
    // check for file_buf is not there
    if(errno == ENOENT)
      return;
  }
  sprintf(cmd, "cat %s |grep -v \"StartTime|SessionInstance\" |awk -F'|' '{print $5\":\"$7}'|awk -F':' '{print $1\"|\"$3}'|sort -u", file_buf);
  nslb_system(cmd,1,err_msg);  
}


void print_unique_scripts()
{
  char file_buf[MAX_LINE_LENGTH];
  char cmd[MAX_LINE_LENGTH];
  struct stat buff;
  char err_msg[1024] = "\0";

  sprintf(file_buf, "%s/logs/TR%d/page_dump.txt", wdir, opts.testrun_number);
  printf("Script Name\n");       /* Gui needs hdr */
  fflush(stdout);
  //sprintf(cmd, "cat %s | grep -v \"StartTime|SessionInstance\" | cut -d'|' -f5 | sort -u|cut -d':' -f1 ", file_buf);
  /*Changes done to get unique script name, hence removing flowname and then sort scripts uniquely*/
  //validation for checking the file_buf is exist or not
  if(stat(file_buf, &buff) < 0) {
    // check for file_buf is not there
    if(errno == ENOENT)
      return;
  }
  sprintf(cmd, "cat %s | grep -v \"StartTime|SessionInstance\" | cut -d'|' -f5 | cut -d ':' -f1 | sort -u", file_buf);
  nslb_system(cmd,1,err_msg);
}

static void run_java_pgm(int testidx, int debug_log_value)
{ 

  char cmd_buf[4096];

  //-t is for test run number
  //-m is for reader run mode, from here runmode will be 1
  sprintf(cmd_buf, "%s/bin/nsi_page_log -t %d -m 1", wdir, testidx); 
  if(system(cmd_buf) != 0)
  {
    fprintf(stderr, "Error in running java program.");
    exit(1);
  }
}

static int check_modification_time(int testidx, int debug_log_value)
{
  char page_dump[MAX_LINE_LENGTH];
  char log_file[MAX_LINE_LENGTH];
   
  struct stat page_dump_stat;  
  struct stat log_stat;

  int page_dump_file_found = 1;  
  int log_file_found = 1;  

  sprintf(page_dump, "%s/logs/TR%d/page_dump.txt", wdir, opts.testrun_number);
  sprintf(log_file, "%s/logs/TR%d/log", wdir, opts.testrun_number);
  
  if(stat(page_dump, &page_dump_stat) < 0)
  {
    //Page dump file is not there
    if(errno == ENOENT)
    {
      page_dump_file_found = 0;
    }
  } 

  if(stat(log_file, &log_stat) < 0)
  {
    //log file is not there
    if(errno == ENOENT)
    {
      log_file_found = 0;
    }
  }
 
  //If log file not found then no need to run command
  if(log_file_found == 0)
  {
    return 0;
  }
   
  //If page dump file is not founf but log file is there
  //then need to call java progm
  if(page_dump_file_found == 0 && log_file_found == 1)  
  {
    run_java_pgm(testidx, debug_log_value);
    return 1;
  }
  //Both log file and page dump file found
  //then check for last updation time.
  //If log file updation time is greater than page dump file then
  //start java pgm
  if(page_dump_file_found == 1 && log_file_found == 1)
  {
    if(log_stat.st_mtime > page_dump_stat.st_mtime){
      run_java_pgm(testidx, debug_log_value);
      return 1;
    }
  }
  
  return 1;
}

int
main(argc, argv)
     int argc;
     char *argv[];
{
  char c;
  int  unique_script_flag, unique_page_flag, unique_page_with_script_flag; 
  unique_script_flag=unique_page_flag=unique_page_with_script_flag=0;
  if (getenv("NS_WDIR") != NULL)
    strcpy(wdir, getenv("NS_WDIR"));
  else
    strcpy(wdir, "/home/cavisson/work");

  if (argc < 2) {
    usage();
    exit(-1);
  }

  memset(&opts, 0, sizeof(struct options));
  opts.limit = 0xFFFFFFFF; //by default there is no limit
  opts.child_idx = -1;
  opts.gen_idx = -1;
  opts.sess_inst = -1;
  opts.page_to_show = 1; //By default only 1 page need to show
  

  struct option longopts[] = {
                               {"testrun", 1, NULL, 't'},
                               {"genid", 1, NULL, 'g'},
                               {"prt_all", 0, NULL, 'a'},
                               {"script", 1, NULL, 's'},
                               {"page", 1, NULL, 'p'},
                               {"prt_failed_obj", 1, NULL, 'f'},
                               {"prt_uniq_obj", 1, NULL, 'u'},
                               {"limit", 1, NULL, 'L'},
                               {"page_offset", 1, NULL, 'P'},
                               {"child", 1, NULL, 'C'},
                               {"sessioninst", 1, NULL, 'S'},
                               {0, 0, 0,0}
                             };

#define CHECK_ARG_VALUE \
        if(!optarg) \
        { \
          printf("%s", err_str); \
          usage(); \
          exit (-1); \
        } 
  char err_str[256] = "\0";
  while((c = getopt_long(argc, argv, "t:g:as:p:f:u:L:P:C:S:", longopts, NULL)) != -1)
  {
    switch(c) {
    case 't':
      sprintf(err_str, "Option -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.testrun_number = atoi(optarg);
      break;

    case 'g':
      sprintf(err_str, "Option --genid or -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.gen_idx = atoi(optarg);
    break;

    case 'a':
      opts.all_flag = 1;
      break;

    case 's':
      sprintf(err_str, "Option -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.script_flag = 1;
      strcpy(opts.script_name, optarg);
      break;

    case 'p':
      sprintf(err_str, "Option -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.page_flag = 1;
      strcpy(opts.page_name, optarg);
      break;

    case 'f':
      sprintf(err_str, "Option -%c needs some value\n", c);
      CHECK_ARG_VALUE
      if(strcmp("page", optarg) == 0)
        opts.failed_page_flag = 1;
      else if(strcmp("session", optarg) == 0)
        opts.failed_sess_flag = 1;
      else {
        printf("Invalid paramater %s. It can be either page or session\n", optarg);
        usage();
        exit(-1);
      }
      break;

    case 'u':   /* shut down after this. */
      sprintf(err_str, "Option -%c needs some value\n", c);
      CHECK_ARG_VALUE
      if (strcmp("page", optarg) == 0) {
       unique_page_flag++;
      } else if(strcmp("script", optarg) == 0) {
          unique_script_flag++;
      } else if(strcmp("script_page", optarg) == 0) {
          unique_page_with_script_flag++;
      } else {
        printf("Invalid paramater %s. It can be either page or script or script_page\n", optarg);
        usage();
        exit(-1);
      }
      break;

    case 'L':
      sprintf(err_str, "Option -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.limit = atoi(optarg);
    break;

    case 'P':
      sprintf(err_str, "Option -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.page_to_show = atoi(optarg);
    break;

    case 'S':
      sprintf(err_str, "Option --sessioninst or -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.sess_inst = atoi(optarg);
    break;

    case 'C':
      sprintf(err_str, "Option --child or -%c needs some value\n", c);
      CHECK_ARG_VALUE
      opts.child_idx = atoi(optarg);
    break;

    default:
      fprintf(stderr, "Invalid option -%c\n", c);
      usage();
      exit(-1);
    }
  }
    
  if (opts.testrun_number == 0) { //it is mandatery to give test run number so checking at initial time
    fprintf(stderr,"Error: It is mandatery to give test run number\n");
    usage();
    exit(-1);
  }

//Showing all scripts uniquely
  if(unique_script_flag){
     print_unique_scripts();
     exit(0);                
  }
//showing all pages uniquely
  if(unique_page_flag){
    print_unique_pages();
    exit(0);                
  }
 
  if(unique_page_with_script_flag){
    print_script_with_page();
    exit(0);
  }
  //for Print header
  print_header();

  /*Check for page_dump.txt and page_dump.txt.incomplete file
   * if LOG file modification time > page_dump.txt modification time
   * then call java program to update page_dump.txt file*/

  if(!check_modification_time(opts.testrun_number, 0))
    return 0;
  
  if(nslb_bigbuf_init(&pd_bigbuf)) {
    fprintf(stderr, "Error: Unable to initialize big buf\n");
    exit(-1);
  }
  
  
  /* Populate the pinfo array */
  populate_page_info_array();
  sort_page_info_array();
  apply_filters();             
  nslb_bigbuf_free(&pd_bigbuf); 
  return 0;
}
