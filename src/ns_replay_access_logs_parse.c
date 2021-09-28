#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include <signal.h>

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
#include "ns_replay_access_logs_parse.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_parent.h"
#include "ns_embd_objects.h"
#include "ns_replay_db_query.h"
//#include "netstorm.h"
#include "ns_exit.h"
#include "ns_error_msg.h"

#define MAX_UIF_LINE_LENGTH    65536  // 64 K
#define MAX_DIR_NAME           2*1024  // 2 K

#define DELTA_REQTABLE_ENTRIES 10
// UserId, ReqFile, (TimeStamp, PageId, Offset, Size)
#define MAX_REQ_PER_USER       (2 + (5000*4))                // MAX 5000 PAGES Supported
#define MAX_INLINE_PER_URL       (1000*4)                // MAX 1000 inline Supported per main url
#define MAX_FILE               50

#define ENABLE_REPLAY_RESUME  0
#define DISABLE_REPLAY_RESUME 1
//Global_data *glob_set;
Replay_last_data *g_replay_last_data;

ReplayUserInfo *g_replay_user_info;

ReplayInLineReq *g_inline_replay_req;

ReplayHosts *g_replay_host;

unsigned int total_replay_user_entries = 0;
static unsigned int max_replay_user_entries = 0;

unsigned int total_replay_host_entries = 0;
static unsigned int max_replay_host_entries = 0;

static unsigned int time_adjust_for_replay = 0;  

ReplayReq *g_replay_req;
static unsigned int total_req_row_entries = 0;
static unsigned int max_req_row_entries = 0;

static unsigned int total_inline_row_entries = 0;
static unsigned int max_inline_row_entries = 0;

ReqFile req_file_arr[MAX_FILE];

int replay_format_type; // Not used
char replay_file_dir[MAX_DIR_NAME] = "\0";
//char default_replay_file_dir[MAX_DIR_NAME]="\0";

static short int g_users_replay_resume_option;  // Replay Stop/Resume Feature

unsigned int g_cur_usr_index = 0; // for user generation

static int create_req_table_entry(unsigned int *row_num, unsigned int *total, unsigned int *max, char **ptr, int size, char *name)
{
  NSDL2_REPLAY(NULL, NULL, "Method called");
  if (*total == *max)
  {
    MY_REALLOC_EX(*ptr, (*max + DELTA_REQTABLE_ENTRIES) * size,((*max) * size), name, -1); // Added older size i.e. maximum entries * size of struct(ReplayUserInfo or ReplayReq).
    *max += DELTA_REQTABLE_ENTRIES;
  }
  *row_num = (*total)++;
  return 0;
}

//Kw format=> REPLAY_FILE <FormatType> <ReplayFileDir>
//  FormatType:
//    1 - For webservices
//    2 - For Access Logs
//  ReplayFileDir: Replay data directory (Optional)
/* Order of REPLAY_FILE is kept 1801, as in case of replay mode 11, we are setting  global_settings->replay_mode to 0. 
* global_settings->replay_mode is set 1 in case STYPE = REPLAY_ACCESS_LOGS and order of STYPE is 1800. In case of replay mode 11 Ns do not 
* use replay code for user generation and it take schedule for user generation. This order must not be disturbed */
void kw_set_replay_file(char *buf, int flag)
{
  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  DIR *replay_dir_p;

  num = sscanf(buf, "%s %d %s", keyword, &replay_format_type, replay_file_dir);
  NSDL2_REPLAY(NULL, NULL, "Number of Arguments = %d", num);

  if(num < 2 || num > 3)
  {
    NS_EXIT(-1, "%s need minimum two arguments or maximum 3 arguments.", keyword); 
  }
  // Set replay mode on basis of mode 
  if(replay_format_type > REPLAY_USING_ACCESS_LOGS) { 
    global_settings->db_replay_mode = replay_format_type;
    global_settings->replay_mode = 0;
    if(num == 2){
      /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/ 
      sprintf(replay_file_dir, "%s/%s/%s/%s/%s/.replay/db_replay", GET_NS_TA_DIR(), g_project_name, g_subproject_name, "scritps", g_scenario_name);
      NSDL2_REPLAY(NULL, NULL, "replay_file_dir = %s", replay_file_dir);
    }
    parse_query_file(replay_file_dir);
    return;
  }

  global_settings->replay_mode = replay_format_type;
   
  NSDL2_REPLAY(NULL, NULL, "global_settings->replay_mode =%d", global_settings->replay_mode);

  if((num == 2) || !strcmp(replay_file_dir, "NA"))
  {
    /*bug id: 101320: using Test Assets Dir  instead of g_ns_wdir*/ 
    sprintf(replay_file_dir, "%s/%s/%s/ReplayAccessLogs/%s/data", GET_NS_TA_DIR(), g_project_name, g_subproject_name, g_scenario_name); 
  }

  NSDL2_REPLAY(NULL, NULL, "replay_file_dir =%s", replay_file_dir);
  replay_dir_p = opendir (replay_file_dir);

  if(replay_dir_p == NULL)
  {
    NS_EXIT(-1, "Replay directory %s does not exist.\nTest Run Canceled.", replay_file_dir);
  }
  closedir(replay_dir_p);
}

//Kw format=> REPLAY_FACTOR <UsersPlaybackFactor> <ArrivalTimeFactor> <InterPageTimeFactor>
//int g_users_playback_factor = 100; // Percentage of uses to be played back
//int g_arrival_time_factor = 100;   // User arrival time multiplication factor
//int g_inter_page_time_factor = 100; // Inter page time multiplication factor
//
int g_replay_iteration_count = 1;
int kw_set_replay_factor(char *buf, char *err_msg, int run_time_change_flag)
{
  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  double users_playback_factor;
  double arrival_time_factor;
  double inter_page_time_factor;
  int replay_repeat_count;

  NSDL2_REPLAY(NULL, NULL, "Method called. buf = %s", buf);
  num = sscanf(buf, "%s %lf %lf %lf %d", keyword, &users_playback_factor, &arrival_time_factor, &inter_page_time_factor, &replay_repeat_count);

  if(num == 0)
  {
    sprintf(err_msg, "Replay factor setting keyword (%s) need at least one arguments.", keyword);
    if(run_time_change_flag == 0)
    {
      NS_EXIT(-1, "%s", err_msg);
    }
    return -1;
  }  
  users_playback_factor = users_playback_factor /(double) 100;
  arrival_time_factor = arrival_time_factor /(double)100;
  inter_page_time_factor = inter_page_time_factor /(double) 100;

  global_settings->users_playback_factor = users_playback_factor;
  global_settings->arrival_time_factor = arrival_time_factor;
  global_settings->inter_page_time_factor = inter_page_time_factor;
  if(num == 5)
    g_replay_iteration_count = replay_repeat_count;

  NSDL2_REPLAY(NULL, NULL, "global_settings->users_playback_factor = %f, global_settings->arrival_time_factor = %f,  global_settings->inter_page_time_factor = %f, g_replay_iteration_count = %d", 
     global_settings->users_playback_factor, global_settings->arrival_time_factor,  global_settings->inter_page_time_factor, g_replay_iteration_count);

  return 0;
}

//REPLAY_RESUME_OPTION <0/1>
// 0 - Resume from last stop if any. Otherwise it will start from beginning. (Default)
// 1 - Do not resume from last stop. It will start from the beginning
void kw_set_replay_resume_option(char *buf, int flag)
{
  int num;
  char keyword[MAX_LINE_LENGTH]="\0";

  num = sscanf(buf, "%s %hd", keyword, &g_users_replay_resume_option);
  if(num == 0)
  {
    NS_EXIT(-1, "%s need at least one arguments.", keyword);
  }  
}

static FILE *open_file_fp(char* file_name, char* mode)
{
  FILE* fp = NULL;

  fp = fopen (file_name, mode);
  return fp;
} 

static int open_file_fd(char* file_name, int flag)
{
  int fd = -1;
  NSDL2_REPLAY(NULL, NULL, "Method called, file_name = %s", file_name);

  fd = open(file_name, flag);
  if(fd < 0)
  {
    NS_EXIT(-1, "Can not open file %s.", file_name);
  }  
  return fd;
} 

static int get_fp_by_file_name(char *file_name)
{
  static int max_file = -1;
  int i;

  for(i = 0; i<= max_file; i++)
  {
    if(!strcmp(file_name, req_file_arr[i].request_file_name))
       return req_file_arr[i].request_file_fd;
  }

  max_file++;
  strcpy(req_file_arr[max_file].request_file_name, file_name); 
  req_file_arr[max_file].request_file_fd = open_file_fd(file_name, O_RDONLY|O_CLOEXEC);
  
  return req_file_arr[max_file].request_file_fd;
}

// Call by parent & find the min time_adjust_for_replay among all the last files
void read_ns_index_last()
{
  FILE *last_file_fp;
  char last_file_name[MAX_LINE_LENGTH]="\0";
  char buffer[MAX_LINE_LENGTH + 1]="\0";
  unsigned int cur_time_adjust_for_replay = 0;  
  char cmd[MAX_LINE_LENGTH];
  char *fields[10];  
  int num_fields;
  char err_msg[1024]= "\0";
 
  NSDL2_REPLAY(NULL, NULL, "Method called.");
  int i;

  sprintf(cmd, "mkdir -p %s/logs/TR%d/replay_data", g_ns_wdir, testidx);

  if(nslb_system(cmd,1,err_msg))
  {
    NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
  }


  MY_MALLOC(g_replay_last_data, global_settings->num_process * sizeof(Replay_last_data), "Replay_last_data", -1);

  for(i = 0; i< global_settings->num_process; i++)
  {
    g_replay_last_data[i].line_index_from_last_file = 0;
    g_replay_last_data[i].user_start_time = 0;
    g_replay_last_data[i].page_index_from_last_file = 0; 

    buffer[0]='\0';
    last_file_fp = NULL;

    sprintf(last_file_name, "%s/ns_index_%d.last", replay_file_dir, i);
    last_file_fp = open_file_fp(last_file_name, "r");

    if(!last_file_fp)
    {
 /*   -> if any ns_index_<child>.last file not found,
 *       then we will not RESUME, & will set g_users_replay_resume_option to DISABLE_REPLAY_RESUME. 
 *    -> so thet remmaining files can move.
 */
      g_users_replay_resume_option = DISABLE_REPLAY_RESUME;
      continue;
    }
 
    if(g_users_replay_resume_option == ENABLE_REPLAY_RESUME) 
    {
      while (nslb_fgets(buffer, MAX_LINE_LENGTH, last_file_fp, 1))
      {
        buffer[strlen(buffer)-1] = '\0';
        if((buffer[0] == '#') || (buffer[0] == '\0'))
          continue; 

        num_fields = get_tokens(buffer, fields, ",", 10);   // Max 10 fields supported char *fields[10];
        //Format:
        //line,userid,timestamp,pageid
        if(num_fields > 3)
        {
          g_replay_last_data[i].line_index_from_last_file = atol(fields[0]);                  // Line Index (A line number)
          //fields[1])    // User id
          cur_time_adjust_for_replay = atol(fields[2]);
          if(i != 0)
          {
           if(cur_time_adjust_for_replay < time_adjust_for_replay)
             time_adjust_for_replay = cur_time_adjust_for_replay;
          }
          else 
            time_adjust_for_replay = cur_time_adjust_for_replay;

          g_replay_last_data[i].page_index_from_last_file = atoi(fields[3]);                  // Page index
        
        }
      }
      NSDL2_REPLAY(NULL, NULL, "line_index_from_last_file = %lu, time_adjust_for_replay = %lu, page_index_from_last_file = %lu", 
                            g_replay_last_data[i].line_index_from_last_file,
                            time_adjust_for_replay,
                            g_replay_last_data[i].page_index_from_last_file);
    }
    fclose(last_file_fp);
    sprintf(cmd, "mv %s %s/logs/TR%d/replay_data/", last_file_name, g_ns_wdir, testidx);
    if(nslb_system(cmd,1,err_msg))
    {
      NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
    }
  }
  NSDL4_REPLAY(NULL,NULL, "time_adjust_for_replay = %lu", time_adjust_for_replay);
}

void cmd_handler(int x)
{
  return;
}

void copy_replay_profile()
{
  char cmd[MAX_LINE_LENGTH] = "\0";
  char prof_dir_name[MAX_LINE_LENGTH] = "\0";
  sighandler_t old_handler;
  int ret;
  char err_msg[1024]= "\0";

  NSDL2_REPLAY(NULL, NULL, "Method called");

  sprintf(prof_dir_name, "%s/logs/TR%d/replay_data/profile", g_ns_wdir,testidx);
  if(mkdir(prof_dir_name, 0775) != 0){
    NS_EXIT(-1, CAV_ERR_1000005, prof_dir_name, errno, nslb_strerror(errno));
  }
    
  // Copy profile to replay data
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/ 
  sprintf(cmd, "cp %s/%s/%s/replay_profiles/%s/*.* %s", GET_NS_TA_DIR(),  g_project_name, g_subproject_name, g_scenario_name, prof_dir_name);
  NSDL4_REPLAY(NULL, NULL, "command to copy = %S", cmd);
  old_handler = signal(SIGCHLD, cmd_handler);
  ret = nslb_system(cmd,1,err_msg);
  (void) signal( SIGCHLD, old_handler);

  if(ret != 0){ 
    NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
  }
}

//this parses the user information file index_<child id> (File Location replay_dir) 
// File Format: User_id, timestamp, first_pg_name, first_req_file, timestamp, next_pg_name, next_req_file,
// New File format will be following 
// UserId,File,TS,PgId,Offset,Size:BodySize[TS;HostIdx;Offset;Size;TS;HostIdx;Offset;Size;.....].....
// Tools is ignoring body of inline urls, so we are not parsing it. In case we will get inline body fromat will bw like :
// UserId,File,TS,PgId,Offset,Size:BodySize[TS;HostIdx;Offset;Size:BodySize;TS;HostIdx;Offset;Size:BodySize;.....].....
void ns_parse_usr_info_file(int child_id)
{
  FILE *uif_fp;
  char index_file_name[MAX_LINE_LENGTH]="\0";
  char req_file_name_with_replay_dir[MAX_LINE_LENGTH] = "\0";
  char *fields[MAX_REQ_PER_USER];
  int num_fields = 0;
  int num_pages; // Numer of pagers per user
  unsigned int replay_usr_row = 0;
  unsigned int replay_req_row = 0;
  unsigned int inline_req_row = 0;
  char buffer[MAX_UIF_LINE_LENGTH  + 1]="\0";
  int random_number;
  int i, j;
  int line_num = 0;
  int num_inline_req;
  unsigned int  total_users = 0;
  unsigned int tot_num_pages = 0;  
  char time_1[32]="\0";
  char time_2[32]="\0";
  u_ns_ts_t first_page_start_time_org; 
  //u_ns_ts_t first_page_start_time_new;
  u_ns_ts_t next_page_start_time_org;
  char *cr_ts_fields[3]; 

  if(global_settings->replay_mode == 0)
    return;
  NSDL2_REPLAY(NULL, NULL, "Method called");


#if NS_DEBUG_ON
  char file_name[2048];
  sprintf(file_name, "%s/logs/TR%d/replay_data/ns_index_dump_%d", g_ns_wdir, testidx, my_port_index);
  FILE *dump_fp = fopen(file_name, "w");
  fprintf(dump_fp, "#user_id, request_file, timestamp, page_id, offset, size,timestamp, page_id, offset, size, ...");
#endif


  // Since in replay, there is only one group, we  index using 0
  int get_no_inlined_obj = runprof_table_shr_mem[0].gset.get_no_inlined_obj;
  AutoFetchTableEntry_Shr *auto_fetch_ptr = NULL;
  // In replay auto fetch is with on or off for all pages. So checking using page index 0
  auto_fetch_ptr = runprof_table_shr_mem[0].auto_fetch_table[0];
  int auto_fetch_embedded = (auto_fetch_ptr->auto_fetch_embedded);
  int load_inline_urls = 1;
  if((get_no_inlined_obj > 0) || (auto_fetch_embedded > 0))
  {
    NSDL2_REPLAY(NULL, NULL, "Since get in line is disabled (get_no_inlined_obj = %d) or auto fetch is enabled (auto_fetch_embedded = %d),  inline urls will not be loaded");
    load_inline_urls = 0;
  }


  sprintf(index_file_name, "%s/ns_index_%d", replay_file_dir, child_id);

  uif_fp = open_file_fp(index_file_name, "r");

  if(!uif_fp)
  {
    NS_EXIT(-1, "Unable to open %s.", index_file_name);
  }

  srand(getpid());

  //read_ns_index_last(&line_index_from_last_file, &page_index_from_last_file);
  if(g_users_replay_resume_option == ENABLE_REPLAY_RESUME && 
     g_replay_last_data[my_port_index].line_index_from_last_file != 0 &&
     g_replay_last_data[my_port_index].page_index_from_last_file != -1)
  {
     NSDL2_REPLAY(NULL, NULL, "ReplayAccessLog: NVM %d - Resuming test from line '%lu' of index file of page '%d' ...", my_port_index, g_replay_last_data[my_port_index].line_index_from_last_file, g_replay_last_data[my_port_index].page_index_from_last_file);
     fprintf(stderr,"ReplayAccessLog: NVM %d - Resuming test from line '%u' of index file ...\n",
                     my_port_index, g_replay_last_data[my_port_index].line_index_from_last_file);
  }

  while (nslb_fgets(buffer, MAX_UIF_LINE_LENGTH, uif_fp, 1))
  {
    // here we are calculating line number
    ++line_num;
    // Skip blank and commented lines
    buffer[strlen(buffer)-1] = '\0';
    if((buffer[0] == '#') || (buffer[0] == '\0'))
      continue; 
    num_fields = get_tokens(buffer, fields, ",", MAX_REQ_PER_USER); 
  
    if((num_fields < 4) || ((num_fields % 4) != 2))
    {
      printf("Warning: User (%s) in index file has invalid fields at line '%d'\n", fields[0], line_num);
      continue;
    }

    // Here we calculating total users
    ++total_users ;

    // Here we are resuming the user from last file
    if(g_users_replay_resume_option == ENABLE_REPLAY_RESUME &&
       g_replay_last_data[my_port_index].line_index_from_last_file != 0 && 
       g_replay_last_data[my_port_index].page_index_from_last_file != -1)
    {
      if(g_replay_last_data[my_port_index].line_index_from_last_file > line_num)
         continue;
    }

 /*
 *  field[0] = user id
 *  field[1] = request file name
 *  field[2] = time stamp
 *  field[3] = page id 
 *  field[4] = offset
 *  field[5] = size
 *  field[6] = time stamp 
 *  field[8] = page id 
 *  field[9] = offset
 *  field[10] = size 
 *  ........
 *  ........
 */

    // Apply users_playback_factor 
    // Generate radom numner and if > users_playback_factor and skip this line
    random_number = 1.0 + (int)((100.0) * (rand() / (RAND_MAX + 1.0))); 
    if(random_number > (global_settings->users_playback_factor * 100))
    {
      NSDL3_REPLAY(NULL, NULL, "Continueing due to playback factor.");
      continue;
    }

#if 0
    for(i = 0; i < num_fields; i++) 
      printf("fields[%d] = %s\n", i, fields[i]);
#endif

    num_pages = (num_fields - 2)/ 4; // Number of pages

    create_req_table_entry(&replay_usr_row, &total_replay_user_entries, &max_replay_user_entries, (char **)&g_replay_user_info, sizeof(ReplayUserInfo), "ReplayUserInfo");

    NSDL4_REPLAY(NULL, NULL, "ReplayUserInfo Succesfully allocated, replay_usr_row = %lu, total_replay_user_entries = %lu, max_replay_user_entries = %lu", replay_usr_row, total_replay_user_entries, max_replay_user_entries);
    
     
    MY_MALLOC(g_replay_user_info[replay_usr_row].user_id, strlen(fields[0]) + 1, "user_id", (int)replay_usr_row);
    strcpy(g_replay_user_info[replay_usr_row].user_id, fields[0]);
    sprintf(req_file_name_with_replay_dir, "%s/%s", replay_file_dir, fields[1]);
    g_replay_user_info[replay_usr_row].req_file_fd = get_fp_by_file_name(req_file_name_with_replay_dir);
    g_replay_user_info[replay_usr_row].cur_req = 0;
    g_replay_user_info[replay_usr_row].next_req = 0;
    g_replay_user_info[replay_usr_row].num_req = num_pages;
    g_replay_user_info[replay_usr_row].start_index = total_req_row_entries;
    g_replay_user_info[replay_usr_row].line_num = line_num;
    g_replay_user_info[replay_usr_row].users_timestamp = atol(fields[2]);  // Actual time stamp

#if NS_DEBUG_ON
    fprintf(dump_fp, "\n%s,ns_req_%d", 
                       g_replay_user_info[replay_usr_row].user_id,
                       my_port_index);
#endif

    for(i = 0; i < num_pages; i++)
    {
      create_req_table_entry(&replay_req_row, &total_req_row_entries, &max_req_row_entries, (char **)&g_replay_req, sizeof(ReplayReq), "ReplayReq");

      // Intailize start index and num entries for inline urls
      g_replay_req[replay_req_row].num_inline_entries = 0;

      if(i == 0) // First page
      {
        first_page_start_time_org = atol(fields[(4*i) + 2]) - time_adjust_for_replay; 
        g_replay_req[replay_req_row].start_time = first_page_start_time_org;
      }
      else
      {
        // Apply inter_page_time_factor
        next_page_start_time_org = atol(fields[(4*i) + 2]) - time_adjust_for_replay;
        g_replay_req[replay_req_row].start_time = next_page_start_time_org;
      }
      
      // Set inline start index here
      if(replay_req_row == 0){
        g_replay_req[replay_req_row].inline_start_idx = 0;
        NSDL4_REPLAY(NULL, NULL, "inline_start_idx = %d for reqidx %d", g_replay_req[replay_req_row].inline_start_idx, replay_req_row);
      } else {
        g_replay_req[replay_req_row].inline_start_idx = g_replay_req[replay_req_row - 1].inline_start_idx + g_replay_req[replay_req_row - 1].num_inline_entries;                                    
        NSDL4_REPLAY(NULL, NULL, "inline_start_idx = %d for reqidx %d", g_replay_req[replay_req_row].inline_start_idx, replay_req_row);
      }


      g_replay_req[replay_req_row].page_id = atoi(fields[(4*i) + 3]);
      g_replay_req[replay_req_row].offset  = atol(fields[(4*i) + 4]);

      // Use pipe to separate parameter type from size in Wells Server Replay. In IIS replay, [ is used for inline information separation
      char *pipe_ptr = NULL;  // Used to point parameter information
   
      pipe_ptr = strchr(fields[(4*i) + 5], '|'); 
      if(pipe_ptr) {
        // If pipe_ptr, it means NS has to do parameterization of request
        *pipe_ptr = '\0';
        pipe_ptr++;
        
        get_tokens(pipe_ptr, cr_ts_fields, ":", 3); 

        g_replay_req[replay_req_row].type = (short) atoi(cr_ts_fields[0]);
        g_replay_req[replay_req_row].creation_ts_offset = (unsigned int) strtoul(cr_ts_fields[1], (char **) NULL, 10);
        g_replay_req[replay_req_row].creation_ts_size = (short) atoi(cr_ts_fields[2]);
      } else{
        // No parameterization is done in this case
        g_replay_req[replay_req_row].type = 0; 
        g_replay_req[replay_req_row].creation_ts_offset = 0;
        g_replay_req[replay_req_row].creation_ts_size = 0;
      }

      // For request size, we will save it into two parts one is header size and another is body size
      // These fields for size will be colon seperated, if colon is not present, it means there is one size only
      char *colon_ptr = NULL;
      char *inline_start_ptr; // Used to point inline information about inlines
      colon_ptr = strchr(fields[(4*i) + 5], ':'); 
      if(colon_ptr) {
        *colon_ptr = '\0';
        colon_ptr++;

        inline_start_ptr = strchr(colon_ptr, '['); // search [ to check inline url is present or not
        if(inline_start_ptr){
          *inline_start_ptr = '\0';
          inline_start_ptr++;
        }

        //if(inline_start_ptr)
         // g_replay_req[replay_req_row].body_size = atol(inline_start_ptr);
        //else 
          g_replay_req[replay_req_row].body_size = atol(colon_ptr);
      } else{

        inline_start_ptr = strchr(fields[(4*i) + 5], '['); // search [ to check inline url is present or not
        if(inline_start_ptr){
          *inline_start_ptr = '\0';
          inline_start_ptr++;
        }
        g_replay_req[replay_req_row].body_size = 0;
      }
      g_replay_req[replay_req_row].size = atol(fields[(4*i) + 5]); // Header size


      // Inline parsing start
      if(inline_start_ptr && load_inline_urls) { // InLine URLs are present for this page
        char *inline_filelds[MAX_INLINE_PER_URL]; // TODO - Make it support 64K inline using malloc
        int num_inline_fileds; 
 
        NSDL4_REPLAY(NULL, NULL, "Going to fill inline url for user %s. inline_start_ptr = %s", fields[0], inline_start_ptr);

        num_inline_fileds = get_tokens(inline_start_ptr, inline_filelds, ";", MAX_INLINE_PER_URL);
        
        NSDL4_REPLAY(NULL, NULL, "num_inline_fileds for page %d is %d for user %s", i, num_inline_fileds, fields[0]);
        if((num_inline_fileds < 4) || (num_inline_fileds % 4 != 0)){
          printf("Warning: User (%s) in index file has invalid inline fields at line '%d'\n", fields[0], line_num);
          NSDL4_REPLAY(NULL, NULL, "Warning: User (%s) in index file has invalid inline fields at line '%d'\n", fields[0], line_num);
          continue;
        }
        num_inline_req = (num_inline_fileds / 4);
        g_replay_req[replay_req_row].num_inline_entries = num_inline_req; // Increment num inline req in req table

        NSDL4_REPLAY(NULL, NULL, "inline urls for page %d is %d for user %s", i, num_inline_req, fields[0]);

        for(j = 0; j < num_inline_req; j++) {

          create_req_table_entry(&inline_req_row, &total_inline_row_entries, &max_inline_row_entries, (char **)&g_inline_replay_req, sizeof(ReplayInLineReq), "ReplayInLineReq");

          g_inline_replay_req[inline_req_row].start_time = atol(inline_filelds[4*j]); 
          g_inline_replay_req[inline_req_row].host_idx = atoi(inline_filelds[(4*j) + 1]); 
          g_inline_replay_req[inline_req_row].offset = atoi(inline_filelds[(4*j) + 2]); 
          g_inline_replay_req[inline_req_row].size = atol(inline_filelds[(4*j) + 3]); 
       
          // check whether only header size is given or both header and body size is given
        /*  colon_ptr = strchr(inline_filelds[4*j], ':');
          if(colon_ptr){
            *colon_ptr = '\0';
            colon_ptr++;

            g_inline_replay_req[inline_req_row].body_size = atol(colon_ptr);
          } else 
            g_inline_replay_req[inline_req_row].body_size = 0;

          g_inline_replay_req[inline_req_row].size = atol(inline_filelds[4*j]); */
          
          NSDL4_REPLAY(NULL, NULL, "Req idx %d, host id %d, offset %u", 
                                    replay_req_row,  
                                    g_inline_replay_req[inline_req_row].host_idx,
                                    g_inline_replay_req[inline_req_row].offset);
 
        }
      } // Inline parsing end
      

#if NS_DEBUG_ON
    fprintf(dump_fp, ",%llu,%d,%u,%u", 
                      g_replay_req[replay_req_row].start_time,
                      g_replay_req[replay_req_row].page_id,
                      g_replay_req[replay_req_row].offset,
                      g_replay_req[replay_req_row].size); 
#endif

      NSDL4_REPLAY(NULL, NULL, "User id (%s), start_time = %u, page_id = %d, time_adjust_for_replay = %u, users timestamp = %u,"
                              "g_replay_req[replay_req_row].size = %u, g_replay_req[replay_req_row].body_size = %u", 
                              g_replay_user_info[replay_usr_row].user_id, 
                              g_replay_req[replay_req_row].start_time,
                              g_replay_req[replay_req_row].page_id,
                              time_adjust_for_replay,
                              g_replay_user_info[replay_usr_row].users_timestamp,
                              g_replay_req[replay_req_row].size,
                              g_replay_req[replay_req_row].body_size);
    }
    tot_num_pages += num_pages;
  }

  if((total_replay_user_entries == 0) || (total_req_row_entries == 0))
  {
    fprintf(stderr, "There are no users for NVM \'%d\' for replay.\n", child_id);
    if(total_users != 0)
    {
      if((global_settings->users_playback_factor != 1) && (g_users_replay_resume_option != ENABLE_REPLAY_RESUME))
        fprintf(stderr, "This is due to user replay factor.\n");
      else if((global_settings->users_playback_factor == 1) && (g_users_replay_resume_option == ENABLE_REPLAY_RESUME))
        fprintf(stderr, "This is because no users remains after resuming test\n");
      else 
        fprintf(stderr, "This is due to either user replay factor or no users remain after resuming test\n");
    }
    fprintf(stderr, "Replay index file is \'%s\'\n", index_file_name);
    // end_test_run(); // dont end test run here, we are handling 0 request later in method replay_mode_user_generation 
  }
  convert_to_hh_mm_ss(first_page_start_time_org, time_1);
  //convert_to_hh_mm_ss(first_page_start_time_new, time_2);
  convert_to_hh_mm_ss((first_page_start_time_org * global_settings->arrival_time_factor - time_adjust_for_replay), time_2);
  fprintf(stderr, "ReplayAccessLog: NVM %d - Users=%u(%u), Last user time=%s(%s), Pages=%u\n",
                                 my_port_index,
                                 total_replay_user_entries, total_users,
                                 time_2, time_1,
                                 tot_num_pages);
  NSDL2_REPLAY(NULL, NULL, "ReplayAccessLog: NVM %d - Users=%u(%u), Last user time=%s(%s), Pages=%u\n",
                                 my_port_index,
                                 total_replay_user_entries, total_users,
                                 time_2, time_1,
                                 tot_num_pages);
#if NS_DEBUG_ON
  fprintf(dump_fp, "\n"); 
  if(dump_fp)
    fclose(dump_fp);
#endif
}

/* Return - number of index files present so that we can set num processes
 *
 */

int ntp_get_num_nvm()
{
  int i;
  int num_index_files = 0;
  struct stat stat_buf;
  char index_file_name[4096 + 1];

  NSDL2_REPLAY(NULL, NULL, "Method called");

  for(i = 0; i < 255; i++)
  {
    sprintf(index_file_name, "%s/ns_index_%d", replay_file_dir, i);
    NSDL2_REPLAY(NULL, NULL, "Checking index file %s", index_file_name);
    int ret = stat(index_file_name, &stat_buf);
    if(ret < 0)
    {
      NSDL2_REPLAY(NULL, NULL, "Breaking as index file %s is not present", index_file_name);
      break;
    }
    if (stat_buf.st_size <= 0)
    {
      NSDL2_REPLAY(NULL, NULL, "Breaking as index file %s is empty", index_file_name);
      break;
    }
    num_index_files++;
  }
  NSDL2_REPLAY(NULL, NULL, "Number of index files = %d", num_index_files);
  if(num_index_files == 0)
  {
    NS_EXIT(-1, "No index files are present for replay in replay directory %s.\nTest Run Canceled.", replay_file_dir);
  }

  return(num_index_files);
}

// Read ns_replay_hosts file and load the hosts in to ReplayHosts and also load the hosts into parent host table
void load_replay_host(){
  
  FILE *replay_host_fp;
  char replay_host_file[MAX_DIR_NAME];
  char buf[MAX_LINE_LENGTH] = "\0";
  char *tmp;
  unsigned int replay_host_row;
  int line_num = 0;

  NSDL2_REPLAY(NULL, NULL, "Method called. replay_file_dir = %s", replay_file_dir);

  sprintf(replay_host_file, "%s/ns_replay_hosts", replay_file_dir);

  replay_host_fp = fopen(replay_host_file, "r");

  // If host file is not present 
  if(replay_host_fp == NULL){
    NSDL2_REPLAY(NULL, NULL, "Error in opening replay host file. Error = %s", nslb_strerror(errno));
    return;
  }
 
  while(nslb_fgets(buf, MAX_LINE_LENGTH, replay_host_fp, 1)){
    
    buf[strlen(buf) - 1] = '\0';
 
    line_num++; 
    create_req_table_entry(&replay_host_row, &total_replay_host_entries, &max_replay_host_entries, (char **)&g_replay_host, sizeof(ReplayHosts), "ReplayHosts");
    if(!strncmp(buf, "http", 4)){
      tmp = buf + 4;
      NSDL4_REPLAY(NULL, NULL, "http Found");
      if(*tmp == 's'){
        NSDL2_REPLAY(NULL, NULL, "Host is https");
        g_replay_host[replay_host_row].type = REPLAY_HOST_HTTPS; 
        tmp++;
      } else{ 
        NSDL2_REPLAY(NULL, NULL, "Host is http");
        g_replay_host[replay_host_row].type = REPLAY_HOST_HTTP; 
    
      }

      MY_MALLOC(g_replay_host[replay_host_row].host, strlen(buf) + 1, "Replay Host", replay_host_row);
      strcpy(g_replay_host[replay_host_row].host, buf);
      NSDL2_REPLAY(NULL, NULL, "host %s", g_replay_host[replay_host_row].host);

      // Load host into host table
      get_server_idx(tmp + 3, g_replay_host[replay_host_row].type, line_num);

    } else {
      printf("Warning: Invalid host format in file %s", replay_host_file);
      NS_DUMP_WARNING("Invalid host format in file %s", replay_host_file);
    } 
  }

  fclose(replay_host_fp);
}

// To create list of embeded urls from Inline Replay Request structure for replay
EmbdUrlsProp *get_replay_embd_objects(int replay_user_idx, int *num_url, char *error_msg)
{
  char buf[MAX_LINE_LENGTH] = "\0";
  EmbdUrlsProp *url_prop;
  ReplayUserInfo *ruiptr;
  int num_inline_urls;
  int inline_start_idx;
  int i;
  int req_file_fd;
  unsigned int req_file_offset;
  //unsigned int size;
  FILE *fp;
  char *fields[10];
  int inline_host_idx;
  int num_tokens;
  int j;
  

  ruiptr = &g_replay_user_info[replay_user_idx]; // find user info pointer
  num_inline_urls =  g_replay_req[ruiptr->start_index + ruiptr->cur_req].num_inline_entries; // find no of urls for the main url
  inline_start_idx =  g_replay_req[ruiptr->start_index + ruiptr->cur_req].inline_start_idx; // find start index of inline url
  req_file_fd = ruiptr->req_file_fd;
  fp = fdopen(req_file_fd, "r");

  NSDL2_REPLAY(NULL, NULL, "Method called. replay_user_idx = %d, num_inline_urls = %d, inline_start_idx = %d", replay_user_idx, num_inline_urls, inline_start_idx);
  NSDL2_REPLAY(NULL, NULL, "Method called. ruiptr->start_index = %d, ruiptr->cur_req = %d", ruiptr->start_index, ruiptr->cur_req);

  if(!num_inline_urls){
    *num_url = 0;
    return NULL;
  } 

  MY_MALLOC(url_prop, num_inline_urls * sizeof(EmbdUrlsProp), "EmbdUrlsProp", -1);

  *num_url = num_inline_urls;

  for(i = inline_start_idx,j = 0 ; i < (inline_start_idx + num_inline_urls); i++,j++){
     
    req_file_offset = g_inline_replay_req[i].offset;
    inline_host_idx = g_inline_replay_req[i].host_idx;

    if(inline_host_idx > (total_replay_host_entries - 1)){
      fprintf(stderr, "Host idx is greater than total host entry in replay_host_file. Inline for page idx %d will be ignored.\n", ruiptr->cur_req);
      *num_url = 0;
      return NULL;
    }
    
    NSDL2_REPLAY(NULL, NULL, "req_file_offset = %u, inline_host_idx = %d, g_inline_replay_req index = %d", req_file_offset, inline_host_idx, i);
    if (fseek(fp, req_file_offset, SEEK_SET) == -1)
    {
      NS_EXIT(-1, "Seek failed. offset = %u, err = %s", req_file_offset, nslb_strerror(errno));
    }

    if(!nslb_fgets(buf, MAX_LINE_LENGTH, fp, 1)){
      NS_EXIT(-1, "Filed while reading Inline request for Replay. Err = %s", nslb_strerror(errno));
    }
    NSDL2_REPLAY(NULL, NULL, "line = %s", buf);
    num_tokens = get_tokens(buf, fields, " ", 10);

    NSDL2_REPLAY(NULL, NULL, "num_tokens = %d", num_tokens);
    if(num_tokens != 3){ 
      NS_EXIT(-1, "Invalid Inline Request format for replay. Exiting......");
    }
    if(g_replay_host[inline_host_idx].host){
     
      url_prop[j].embd_type = g_replay_host[inline_host_idx].type;  
  
      MY_MALLOC(url_prop[j].embd_url, (strlen(g_replay_host[inline_host_idx].host) + strlen(fields[1] ) + 1), "url_prop[j].embd_url", j);
      sprintf(url_prop[j].embd_url, "%s%s", g_replay_host[inline_host_idx].host, fields[1]);
      NSDL2_REPLAY(NULL, NULL, "Inline url [%s] filled at index = %d", url_prop[j].embd_url, j);

    } else {
      NS_EXIT(-1, "Invalid host index found for inline url %s replay. Exiting......", fields[1]); 
    }
  }
  return url_prop;
}
