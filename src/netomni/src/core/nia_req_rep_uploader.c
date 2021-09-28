#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "nslb_util.h"
#include "nslb_partition.h"
#include "nslb_multi_thread_trace_log.h"
#include "nslb_signal.h"
#include "nslb_server_admin.h"

#define RBU_MAX_2K_LENGTH                   2048
#define RBU_MAX_512BYTE_LENGTH              512 
#define RBU_MAX_16BYTE_LENGTH               16 
#define RBU_MAX_FILE_PATH_LENGTH            1056
#define RBU_MAX_TAR_NAME_LENGTH             256

//trace log key
MTTraceLogKey *trace_log_key;

int test_idx = 0;
int ctrl_test_idx = 0;
long long partition_idx = 0;
TestRunInfoTable_Shr *testruninfo_tbl_shr = NULL;

int trace_level = 1;
int trace_log_size = 10; //MB
int rbu_enable = 0;
int capture_clip_enable = 0;
int g_tracing_mode = 0;
int websocket_enable = 0;
char lighthouse_enable = 0;
char performance_trace_enable = 0;
char gen_name[50] = "";
char tar_name[256] = "";
char harp_tar_name[256] = "";
char compressed_tar_name[256 + 8] = "";
char capture_clip_tar_name[256] = "";
char compressed_capture_clip_tar_name[256 + 8] = "";
char compressed_harp_tar_name[256 + 8] = "";
char tar_path[1024];
char harp_tar_path[1024];
char ns_wdir[512] = "";
char partition_name[50] = "";
char lighthouse_tar_name[RBU_MAX_TAR_NAME_LENGTH + 1] = "";
char compressed_lighthouse_tar_name[RBU_MAX_TAR_NAME_LENGTH + 8] = "";
char performance_trace_tar_name[RBU_MAX_TAR_NAME_LENGTH + 1] = "";
char compressed_performance_trace_tar_name[RBU_MAX_TAR_NAME_LENGTH + 8] = "";

char new_req_rep_path[1024] = "";
char old_req_rep_path[1024] = "";
char new_screen_shot_path[1024] = "";
char old_screen_shot_path[1024] = "";
char new_orig_file_path[1024] = "";
char old_orig_file_path[1024] = "";
char old_harp_file_path[1024] = "";
char new_harp_file_path[1024] = "";
char old_capture_clip_file_path[1024] = "";
char new_capture_clip_file_path[1024] = "";
char old_lighthouse_file_path[RBU_MAX_FILE_PATH_LENGTH + 1] = "";
char new_lighthouse_file_path[RBU_MAX_FILE_PATH_LENGTH + 1] = "";
char old_performance_trace_file_path[RBU_MAX_FILE_PATH_LENGTH + 1] = "";
char new_performance_trace_file_path[RBU_MAX_FILE_PATH_LENGTH + 1] = "";

char ctrl_ip[20] = "";
int port;
int process_level = 0;
int reader_run_mode = 0;

int recovery_mode = 0;
struct sigaction sa;
int test_over_sig = 0;

char ctrl_tr_or_partition[50] = "";
char ctrl_ns_wdir[1024] = "";

static char tr_or_partition[50] = "";
static char base_dir[512] = "";

static char filter_str[RBU_MAX_16BYTE_LENGTH + 1];
static struct dirent **orig_dirent, **body_dirent, **screen_shots_dirent, **har_dirent, **clip_dirent, **lighthouse_dirent;
static struct dirent **performance_trace_dirent;

/* 1. Make hard link to req,rep and rep_body file into new directory. */
/* 2. Unlink old directory's req,rep and rep_body file */ 
#define LINK_UNLINK_REQ_REP_FILES(prefix) \
{ \
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called, websocket_enable = %d, linked_file_idx = %d, d_name = %s, prefix = %s", websocket_enable, linked_file_idx, body_dirent[linked_file_idx]->d_name, prefix); \
  \
  sprintf(file_name_with_new_path, "%s/url_req_%s.dat", new_req_rep_path, prefix); \
  sprintf(file_name_with_old_path, "%s/url_req_%s.dat", old_req_rep_path, prefix); \
  if(link(file_name_with_old_path, file_name_with_new_path) != 0) { \
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "link failed from [%s] to [%s], Error = %s", file_name_with_old_path, file_name_with_new_path, nslb_strerror(errno)); \
  } \
  if(unlink(file_name_with_old_path) != 0) { \
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "unlink failed [%s], Error = %s", file_name_with_old_path, nslb_strerror(errno)); \
  } \
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Moving file %s", file_name_with_old_path); \
  \
  sprintf(file_name_with_new_path, "%s/url_rep_%s.dat", new_req_rep_path, prefix); \
  sprintf(file_name_with_old_path, "%s/url_rep_%s.dat", old_req_rep_path, prefix); \
  if(link(file_name_with_old_path, file_name_with_new_path) != 0) { \
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "link failed from [%s] to [%s], Error = %s",file_name_with_old_path, file_name_with_new_path, nslb_strerror(errno)); \
  } \
  if(unlink(file_name_with_old_path) != 0) { \
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "unlink failed [%s], Error = %s", file_name_with_old_path, nslb_strerror(errno)); \
  } \
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Moving file %s", file_name_with_old_path); \
  \
  sprintf(file_name_with_new_path, "%s/url_rep_body_%s.dat", new_req_rep_path, prefix); \
  sprintf(file_name_with_old_path, "%s/url_rep_body_%s.dat", old_req_rep_path, prefix); \
  if(link(file_name_with_old_path, file_name_with_new_path) != 0) { \
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "link failed from [%s] to [%s], Error = %s",file_name_with_old_path, file_name_with_new_path, nslb_strerror(errno)); \
  } \
  if(unlink(file_name_with_old_path) != 0) { \
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "unlink failed [%s], Error = %s", file_name_with_old_path, nslb_strerror(errno)); \
  } \
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Moving file %s", file_name_with_old_path); \
  \
}

//this function sorts files on the basis of inode
static int my_inode_sort(const struct dirent **a, const struct dirent **b)
{
  if ((*a)->d_ino > (*b)->d_ino)
    return 1;
   else
     return 0;
}

//this function filters files searched by scandir
static int filter_file(const struct dirent *a)
{     
  char *ptr = (char*)a->d_name;
  if(strstr(ptr, filter_str))
    return 1;
  else
    return 0;
} 

static void move_har_files(int linked_file_idx)
{
  char file_name_with_new_path[2048] = "";
  char file_name_with_old_path[2048] = "";
  char prefix[512] = "";
  char *linked_file_name = NULL;

  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called, rbu_enable = %d", rbu_enable);

  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "d_name = %s", har_dirent[linked_file_idx]->d_name);
  linked_file_name = har_dirent[linked_file_idx]->d_name;
  strcpy(prefix, linked_file_name);
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "prefix = %s", prefix);
  sprintf(file_name_with_new_path, "%s/%s", new_harp_file_path, prefix);
  sprintf(file_name_with_old_path, "%s/%s", old_harp_file_path, prefix);
  if(link(file_name_with_old_path, file_name_with_new_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "link failed from [%s] to [%s], Error = %s",file_name_with_old_path, file_name_with_new_path, nslb_strerror(errno));
  }
  if(unlink(file_name_with_old_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "unlink failed [%s], Error = %s", file_name_with_old_path, nslb_strerror(errno));
  }
}

static void move_capture_clip_files(int linked_file_idx)
{
  char file_name_with_new_cc_path[2048] = "";
  char file_name_with_old_cc_path[2048] = "";
  char prefix[512] = "";
  char *linked_file_name = NULL;

  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called, capture_clip_enable = %d", capture_clip_enable);

  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "d_name = %s", clip_dirent[linked_file_idx]->d_name);
  linked_file_name = clip_dirent[linked_file_idx]->d_name;
  strcpy(prefix, linked_file_name);
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "capture click prefix = %s", prefix);
  sprintf(file_name_with_new_cc_path, "%s/%s", new_capture_clip_file_path, prefix);
  sprintf(file_name_with_old_cc_path, "%s/%s", old_capture_clip_file_path, prefix);
  if(link(file_name_with_old_cc_path, file_name_with_new_cc_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "capture clip link failed from [%s] to [%s], Error = %s", 
                    file_name_with_old_cc_path, file_name_with_new_cc_path, nslb_strerror(errno));
  }
  if(unlink(file_name_with_old_cc_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "capture clip unlink failed [%s], Error = %s", 
                    file_name_with_old_cc_path, nslb_strerror(errno));
  }
}

static void move_lighthouse_files(int linked_file_idx)
{
  char file_name_with_new_cc_path[RBU_MAX_2K_LENGTH + 1] = "";
  char file_name_with_old_cc_path[RBU_MAX_2K_LENGTH + 1] = "";
  char prefix[RBU_MAX_512BYTE_LENGTH + 1] = "";
  char *linked_file_name = NULL;

  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called, lighthouse_enable = %d", lighthouse_enable);

  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "d_name = %s", lighthouse_dirent[linked_file_idx]->d_name);
  linked_file_name = lighthouse_dirent[linked_file_idx]->d_name;
  snprintf(prefix, RBU_MAX_512BYTE_LENGTH, "%s", linked_file_name);
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "prefix = %s", prefix);
  snprintf(file_name_with_new_cc_path, RBU_MAX_2K_LENGTH, "%s/%s", new_lighthouse_file_path, prefix);
  snprintf(file_name_with_old_cc_path, RBU_MAX_2K_LENGTH, "%s/%s", old_lighthouse_file_path, prefix);
  if(link(file_name_with_old_cc_path, file_name_with_new_cc_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "lighthouse link failed from [%s] to [%s], Error = %s", 
                    file_name_with_old_cc_path, file_name_with_new_cc_path, nslb_strerror(errno));
  }
  if(unlink(file_name_with_old_cc_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "lighthouse unlink failed [%s], Error = %s", 
                    file_name_with_old_cc_path, nslb_strerror(errno));
  }
}

static void move_performance_trace_files(int linked_file_idx)
{
  char file_name_with_new_cc_path[RBU_MAX_2K_LENGTH + 1] = "";
  char file_name_with_old_cc_path[RBU_MAX_2K_LENGTH + 1] = "";
  char prefix[RBU_MAX_512BYTE_LENGTH + 1] = "";
  char *linked_file_name = NULL;

  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called, performance_trace_enable = %d", performance_trace_enable);
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "d_name = %s", performance_trace_dirent[linked_file_idx]->d_name);

  linked_file_name = performance_trace_dirent[linked_file_idx]->d_name;
  snprintf(prefix, RBU_MAX_512BYTE_LENGTH, "%s", linked_file_name);
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "prefix = %s", prefix);
  snprintf(file_name_with_new_cc_path, RBU_MAX_2K_LENGTH, "%s/%s", new_performance_trace_file_path, prefix);
  snprintf(file_name_with_old_cc_path, RBU_MAX_2K_LENGTH, "%s/%s", old_performance_trace_file_path, prefix);

  if(link(file_name_with_old_cc_path, file_name_with_new_cc_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "performance trace link failed from [%s] to [%s], Error = %s", 
                    file_name_with_old_cc_path, file_name_with_new_cc_path, nslb_strerror(errno));
  }
  if(unlink(file_name_with_old_cc_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "performance trace unlink failed [%s], Error = %s", 
                    file_name_with_old_cc_path, nslb_strerror(errno));
  }
}

//this function checks whether req, rep files corresponding to rep_body file exists or not
//if exists then it adds req, rep, rep_body and .orig file names to buffer.
//in case of screen shot we put the null from last 5th character in prefix and write hardcoded .jpeg on the screen shot file.
//we fully confirmed that then screen shot files are always get with .jpeg extension.
//in case of url_req, url_rep and url_rep_body we put the null from last 4th character in prefix.
//because we confirm that these files will always make with .dat extension.
static void move_files(int orig_file_idx, int linked_file_idx, int screen_shot_flag)
{
  char file_name_with_new_path[1024] = ""; 
  char file_name_with_old_path[1024] = ""; 
  char prefix[256] = "";
  char *linked_file_name = NULL;

  if(screen_shot_flag)
  {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "screen shot d_name = %s", screen_shots_dirent[linked_file_idx]->d_name);
    linked_file_name = screen_shots_dirent[linked_file_idx]->d_name;
    //page_screen_shot_0_0_0_1_0_0_0_1_0.jpeg
    strcpy(prefix, (linked_file_name + strlen("page_screen_shot_")));
    prefix[strlen(prefix) - 5] = 0;
  }
  else 
  {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "body d_name %s", body_dirent[linked_file_idx]->d_name); 
    linked_file_name = body_dirent[linked_file_idx]->d_name;
    strcpy(prefix, (linked_file_name + 13));
    prefix[strlen(prefix) - 4] = 0; 
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "coming in else part..");
  }
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "linked_file_name = %s, prefix = %s ", linked_file_name, prefix);

  //If it is Websocket case or Mixed case (WebSocket & NS), then req_rep files are already moved to .GenTar.
  if(!websocket_enable)
    LINK_UNLINK_REQ_REP_FILES(prefix)

  //Orig file
  sprintf(file_name_with_new_path, "%s/%s", new_orig_file_path, orig_dirent[orig_file_idx]->d_name);
  sprintf(file_name_with_old_path, "%s/%s", old_orig_file_path, orig_dirent[orig_file_idx]->d_name);
  if(link(file_name_with_old_path, file_name_with_new_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "link failed from [%s] to [%s], Error = %s",file_name_with_old_path, file_name_with_new_path, nslb_strerror(errno));
  }
  if(unlink(file_name_with_old_path) != 0) {
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "unlink failed [%s], Error = %s", file_name_with_old_path, nslb_strerror(errno));
  }
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Moving file %s", file_name_with_old_path);

  // this condition run when the screen shot keyword enabled and moved on the controller
  if(screen_shot_flag)
  {
    //screen shot file
    sprintf(file_name_with_new_path, "%s/page_screen_shot_%s.jpeg", new_screen_shot_path, prefix);
    sprintf(file_name_with_old_path, "%s/page_screen_shot_%s.jpeg", old_screen_shot_path, prefix);
    if(link(file_name_with_old_path, file_name_with_new_path) != 0) {
      NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "link failed from [%s] to [%s], Error = %s",file_name_with_old_path, file_name_with_new_path, nslb_strerror(errno));
    }
    if(unlink(file_name_with_old_path) != 0) {
      NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "unlink failed [%s], Error = %s", file_name_with_old_path, nslb_strerror(errno));
    }
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Moving screen shot file %s", file_name_with_old_path);
  }
}

/*  This method finds rep_body files corresponding to orig files */
static int scan_dir_and_move_files() 
{
  int num_body = 0; //number of total rep_body files in dir
  int num_screen_shot = 0;
  int num_harp_file = 0;
  int num_har_file = 0;
  int num_capture_clip = 0;
  int num_lighthouse_file = 0;
  int num_performance_trace_file = 0;
  int i, j;
  int last_match_idx = -1;
  int last_match_ss_idx = -1;
  int last_match_har_idx = -1;
  int last_match_clip_idx = -1;
  int last_match_lighthouse_idx = -1;
  int last_match_performance_trace_idx = -1;
  int last_match_ws_idx = -1;   //WebSocket
  int num_orig = 0, no_trace_file_moved = 0, no_rbu_har_file_moved = 0, no_clip_file_moved = 0, no_ws_req_rep_moved = 0;
  int linked_file_idx = 0;
  char no_lighthouse_file_moved = 0, no_performance_trace_file_moved = 0;
  char file_name_with_new_path[RBU_MAX_2K_LENGTH + 1] = "";
  char file_name_with_old_path[RBU_MAX_2K_LENGTH + 1] = "";
  char prefix[RBU_MAX_512BYTE_LENGTH + 1] = "";
  char *linked_file_name = NULL;

  strcpy(filter_str, "body");  //filter file names using keyword 'body'
  num_body = scandir(old_req_rep_path, &body_dirent, filter_file, my_inode_sort); 

  strcpy(filter_str, "orig"); //filter file names using keyword 'orig'
  num_orig = scandir(old_orig_file_path, &orig_dirent, filter_file, my_inode_sort); 
  
  strcpy(filter_str, "screen_shot"); //filter file names using keyword 'screen_shots'
  num_screen_shot = scandir(old_screen_shot_path, &screen_shots_dirent, filter_file, my_inode_sort);

  strcpy(filter_str, "har"); //filter file names using keyword 'har'
  num_har_file = scandir(old_harp_file_path, &har_dirent, filter_file, my_inode_sort);
 
  strcpy(filter_str, "video_clip"); //filter file names using keyword 'har'
  num_capture_clip = scandir(old_capture_clip_file_path, &clip_dirent, filter_file, my_inode_sort);

  snprintf(filter_str, RBU_MAX_16BYTE_LENGTH, "lighthouse"); //filter file names using keyword 'lighthouse'
  num_lighthouse_file = scandir(old_lighthouse_file_path, &lighthouse_dirent, filter_file, my_inode_sort);

  snprintf(filter_str, RBU_MAX_16BYTE_LENGTH, "json.gz"); //filter file names using keyword 'json.gz'
  num_performance_trace_file = scandir(old_performance_trace_file_path, &performance_trace_dirent, filter_file, my_inode_sort);

  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method Called, num_orig = %d, num_body = %d, num_screen_shot = %d, " 
                 "old_req_rep_path = %s, old_orig_file_path = %s, old_screen_shot_path = %s, num_harp_file = %d, old_harp_file_path = %s, "
                 "num_har_file = %d, old_capture_clip_file_path = %s, num_capture_clip = %d, old_lighthouse_file_path = %s, "
                 "num_lighthouse_file = %d, old_performance_trace_file_path = %s, num_performance_trace_file = %d", 
                 num_orig, num_body, num_screen_shot, old_req_rep_path, old_orig_file_path, old_screen_shot_path, num_harp_file,
                 old_harp_file_path, num_har_file, old_capture_clip_file_path, num_capture_clip, old_lighthouse_file_path,
                 num_lighthouse_file, old_performance_trace_file_path, num_performance_trace_file);

  if(websocket_enable)
  {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "In WebSocket, page_dump is not found, num_body = %d, "
                    "last_match_ws_idx = %d", num_body, last_match_ws_idx);
    for(linked_file_idx = last_match_ws_idx + 1; linked_file_idx < num_body; linked_file_idx++)
    {
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "linked_file_idx = %d, body_dirent[%d]->d_name = %s", 
                      linked_file_idx, linked_file_idx, body_dirent[linked_file_idx]->d_name);

      linked_file_name = body_dirent[linked_file_idx]->d_name;
      strcpy(prefix, (linked_file_name + 13));
      prefix[strlen(prefix) - 4] = 0;

      LINK_UNLINK_REQ_REP_FILES(prefix)
      last_match_ws_idx = linked_file_idx;
    }
  }

  for(i = 0; i < num_orig; i++)  //loop will run for all .orig file
  {
    //loop will run for the screen shot file
    for(j = last_match_ss_idx + 1; j < num_screen_shot; j++)
    {
      if(orig_dirent[i]->d_ino == screen_shots_dirent[j]->d_ino)
      {
        move_files(i, j, 1);
        last_match_ss_idx = j;
        break;
      }
    }
    // TL2 - Checking ....
    // match inode of orig file with inode of rep_body file
    // start comparing from the index where last match was found
    for(j = last_match_idx + 1; j < num_body; j++) 
    {
      if(orig_dirent[i]->d_ino == body_dirent[j]->d_ino)
      {
        move_files(i, j, 0);
        last_match_idx = j;   //save index of last match
        break;
      }
    }
  }

  if(rbu_enable) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "rbu_enable = %d, capture_clip_enable = %d", 
                    rbu_enable, capture_clip_enable);
    for(i = last_match_har_idx + 1; i < num_har_file; i++) {
      move_har_files(i);
      last_match_har_idx = i;
    }
    if(capture_clip_enable) {
      for(i = last_match_clip_idx + 1; i < num_capture_clip; i++) {
        move_capture_clip_files(i);
        last_match_clip_idx = i;
      }
    }
    if(lighthouse_enable) {
      for(i = last_match_lighthouse_idx + 1; i < num_lighthouse_file; i++) {
        move_lighthouse_files(i);
        last_match_lighthouse_idx = i;
      }
    }
    if(performance_trace_enable) {
      for(i = last_match_performance_trace_idx + 1; i < num_performance_trace_file; i++) {
        move_performance_trace_files(i);
        last_match_performance_trace_idx = i;
      }
    }
  }

  if(orig_dirent)
  {
    for(i = 0; i < num_orig; i++) {
      free(orig_dirent[i]);
    }
    free(orig_dirent);
  }

  if(body_dirent)
  {
    for(i = 0; i < num_body; i++) {
      free(body_dirent[i]);
    }
    free(body_dirent);
  }

  if(screen_shots_dirent)
  {
    for(i = 0; i < num_screen_shot; i++) {
      free(screen_shots_dirent[i]);
    }
    free(screen_shots_dirent);
  }

  if(har_dirent)
  {
    for(i = 0; i < num_har_file; i++) {
      free(har_dirent[i]);
    }
    free(har_dirent);
  }

  if(clip_dirent)
  {
    for(i = 0; i < num_capture_clip; i++) {
      free(clip_dirent[i]);
    }
    free(clip_dirent);
  }

  if(lighthouse_dirent)
  {
    for(i = 0; i < num_lighthouse_file; i++) {
      free(lighthouse_dirent[i]);
    }
    free(lighthouse_dirent);
  }

  if(performance_trace_dirent)
  {
    for(i = 0; i < num_performance_trace_file; i++) {
      free(performance_trace_dirent[i]);
    }
    free(performance_trace_dirent);
  }

  if(!websocket_enable || (websocket_enable && last_match_ws_idx == -1))
  {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "For websocket no req_rep files found, to be moved");
    no_ws_req_rep_moved = 1;
  }

  if(!g_tracing_mode || (g_tracing_mode && (last_match_idx == -1 && last_match_ss_idx == -1)))
  {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No files found to be moved"); 
    no_trace_file_moved = 1;
  }

  if(!rbu_enable || ((rbu_enable) && (last_match_har_idx == -1))) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No har files found to be moved");
    no_rbu_har_file_moved = 1;
  }
  
  if(!rbu_enable || !capture_clip_enable || ((capture_clip_enable) && (last_match_clip_idx == -1))) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No capture clips files found to be moved");
    no_clip_file_moved = 1;
  }

  if(!rbu_enable || !lighthouse_enable || ((lighthouse_enable) && (last_match_lighthouse_idx == -1))) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No lighthouse files found to be moved");
    no_lighthouse_file_moved = 1;
  }

  if(!rbu_enable || !performance_trace_enable || ((performance_trace_enable) && (last_match_performance_trace_idx == -1))) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No performance trace files found to be moved");
    no_performance_trace_file_moved = 1;
  }

  if(no_trace_file_moved && no_rbu_har_file_moved && no_clip_file_moved && no_lighthouse_file_moved && no_ws_req_rep_moved &&
     no_performance_trace_file_moved) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No files to be moved");
    return 1;
  }
  return 0;  //some files were moved
}

static void update_tr_or_partition()
{
  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called, partition_idx = %llu, partition_name = %s", partition_idx, partition_name);
  if(partition_idx > 0)
  {
    sprintf(tr_or_partition, "TR%d/%lld", test_idx, partition_idx);
    sprintf(ctrl_tr_or_partition, "TR%d/%lld", ctrl_test_idx, partition_idx);
    sprintf(tar_name, "%s_req_rep_%s.tar", gen_name, partition_name);
    sprintf(compressed_tar_name, "%s.gz", tar_name);
    sprintf(harp_tar_name, "%s_harp_files_%s.tar", gen_name, partition_name);
    sprintf(compressed_harp_tar_name, "%s.gz", harp_tar_name);
    sprintf(capture_clip_tar_name, "%s_capture_clips_%s.tar", gen_name, partition_name);
    sprintf(compressed_capture_clip_tar_name, "%s.gz", capture_clip_tar_name); 
    snprintf(lighthouse_tar_name, RBU_MAX_TAR_NAME_LENGTH, "%s_lighthouse_%s.tar", gen_name, partition_name);
    snprintf(compressed_lighthouse_tar_name, RBU_MAX_TAR_NAME_LENGTH + 8, "%s.gz", lighthouse_tar_name); 
    snprintf(performance_trace_tar_name, RBU_MAX_TAR_NAME_LENGTH, "%s_performance_trace_%s.tar", gen_name, partition_name);
    snprintf(compressed_performance_trace_tar_name, RBU_MAX_TAR_NAME_LENGTH + 8, "%s.gz", performance_trace_tar_name); 
  }
  else
  {
    sprintf(tr_or_partition, "TR%d", test_idx);
    sprintf(ctrl_tr_or_partition, "TR%d", ctrl_test_idx);
    sprintf(tar_name, "%s_req_rep.tar", gen_name);
    sprintf(compressed_tar_name, "%s.gz", tar_name);
    sprintf(harp_tar_name, "%s_harp_files.tar", gen_name);
    sprintf(compressed_harp_tar_name, "%s.gz", harp_tar_name);
    sprintf(capture_clip_tar_name, "%s_capture_clips.tar", gen_name);
    sprintf(compressed_capture_clip_tar_name, "%s.gz", capture_clip_tar_name);
    snprintf(lighthouse_tar_name, RBU_MAX_TAR_NAME_LENGTH, "%s_lighthouse.tar", gen_name);
    snprintf(compressed_lighthouse_tar_name, RBU_MAX_TAR_NAME_LENGTH + 8, "%s.gz", lighthouse_tar_name); 
    snprintf(performance_trace_tar_name, RBU_MAX_TAR_NAME_LENGTH, "%s_performance_trace.tar", gen_name);
    snprintf(compressed_performance_trace_tar_name, RBU_MAX_TAR_NAME_LENGTH + 8, "%s.gz", performance_trace_tar_name); 
  }

  sprintf(tar_path, "%s/logs/TR%d/.GenTar/%s/generic", ns_wdir, test_idx, tr_or_partition);
  sprintf(harp_tar_path, "%s/logs/TR%d/.GenTar/%s/rbu", ns_wdir, test_idx, tr_or_partition);
  
  //Make path of old req, rep,rep body, orig file and screen shot
  sprintf(old_req_rep_path, "%s/logs/%s/ns_logs/req_rep", ns_wdir, tr_or_partition);
  sprintf(old_orig_file_path, "%s/logs/%s/page_dump/docs", ns_wdir, tr_or_partition);
  sprintf(old_screen_shot_path, "%s/logs/%s/rbu_logs/screen_shot", ns_wdir, tr_or_partition);
  sprintf(old_harp_file_path, "%s/logs/%s/rbu_logs/harp_files", ns_wdir, tr_or_partition); 
  sprintf(old_capture_clip_file_path, "%s/logs/%s/rbu_logs/snap_shots", ns_wdir, tr_or_partition); 
  snprintf(old_lighthouse_file_path, RBU_MAX_FILE_PATH_LENGTH, "%s/logs/%s/rbu_logs/lighthouse", ns_wdir, tr_or_partition); 
  snprintf(old_performance_trace_file_path, RBU_MAX_FILE_PATH_LENGTH, "%s/logs/%s/rbu_logs/performance_trace", ns_wdir, tr_or_partition); 

  //Make path of new req, rep,rep body, orig file and screen shot to make tar
  sprintf(new_req_rep_path, "%s/ns_logs/req_rep/", tar_path);
  if(mkdir_ex(new_req_rep_path) == 0) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "error in creating dir = %s, Error = [%s]", new_req_rep_path, nslb_strerror(errno)); 
    exit(-1);
  }
  sprintf(new_orig_file_path, "%s/page_dump/docs/", tar_path);
  if(mkdir_ex(new_orig_file_path) == 0) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "error in creating dir = %s, Error = [%s]", new_orig_file_path, nslb_strerror(errno)); 
    exit(-1);
  }
  sprintf(new_screen_shot_path, "%s/rbu_logs/screen_shot/", tar_path);
  if(mkdir_ex(new_screen_shot_path) == 0) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "error in creating dir = %s, Error = [%s]", new_screen_shot_path, nslb_strerror(errno));
    exit(-1);
  }
  sprintf(new_harp_file_path, "%s/rbu_logs/harp_files/", harp_tar_path);
  if(mkdir_ex(new_harp_file_path) == 0) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "error in creating dir = %s, Error = [%s]", new_harp_file_path, nslb_strerror(errno));
    exit(-1);
  }
  sprintf(new_capture_clip_file_path, "%s/rbu_logs/snap_shots/", harp_tar_path);
  if(mkdir_ex(new_capture_clip_file_path) == 0) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "error in creating dir = %s, Error = [%s]", new_capture_clip_file_path, nslb_strerror(errno));
    exit(-1);
  }
  snprintf(new_lighthouse_file_path, RBU_MAX_FILE_PATH_LENGTH, "%s/rbu_logs/lighthouse/", harp_tar_path);
  if(mkdir_ex(new_lighthouse_file_path) == 0) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "error in creating dir = %s, Error = [%s]", new_lighthouse_file_path, nslb_strerror(errno));
    exit(-1);
  }

  snprintf(new_performance_trace_file_path, RBU_MAX_FILE_PATH_LENGTH, "%s/rbu_logs/performance_trace/", harp_tar_path);
  if(mkdir_ex(new_performance_trace_file_path) == 0) {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "error in creating dir = %s, Error = [%s]", new_performance_trace_file_path, nslb_strerror(errno));
    exit(-1);
  }
}

static void switch_partition(long long cur_partition_idx)
{
  partition_idx = cur_partition_idx;
  sprintf(partition_name, "%lld", cur_partition_idx);

  update_tr_or_partition();
}


#if 0
//this function checks tmp dir to find existing files
//if tar file is present, then ship tar
//if tar is not present and file dir is present, then make tar and ship.
static void check_tmp_dir()
{
  struct dirent **namelist = NULL;
  int num_files, i, tar_found = 0, files_found = 0;
  long long temp = 0;
  char files_name[256] = "";

  num_files = scandir(ns_tmp_dir, &namelist, 0, alphasort);

  for(i = 0; i < num_files; i++)
  {
    //if tar file is found
    if(strstr(namelist[i]->d_name, ".tar"))
    {
      strcpy(compressed_tar_name, namelist[i]->d_name);
      tar_found = 1;
    }
    //if page_dump and ns_logs dirs to be created in a directoty (either TR or partition)
    //checking for partition dir by checking file_name lenght 14.
    else if(strlen(namelist[i]->d_name) == 14 || strstr(namelist[i]->d_name, "TR")) 
    {
      strcpy(files_name, namelist[i]->d_name);
      files_found = 1;
    }
  }
  if(namelist)
    free(namelist);

  if(tar_found)
  {
    sscanf(compressed_tar_name + 8, "%lld", &temp);
    process_level = 1;  //remove dir if exist, ship tar to controller, remove tar
  }
  else if(files_found) 
  {
    temp = atoll(files_name);
    process_level = 2; //make tar, remove dir, ship tar to controller, remove tar
  }
  else
    process_level = 0;   //continue normal recovery

  if(partition_idx > 0 && temp > 0)
    switch_partition(temp);
}
#endif

static void make_tar()
{
  char buf[RBU_MAX_FILE_PATH_LENGTH + 1] = "";
  long long time1, diff = 0;
  int ret;
  struct stat stat_st;

  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Moving to dir %s and harp_tar_path = %s, rbu_enable = %d, "
                  "capture_clip_enable = %d, lighthouse_enable = %d, performance_trace_enable = %d",
                  tar_path, harp_tar_path, rbu_enable, capture_clip_enable, lighthouse_enable, performance_trace_enable);
 
  if(rbu_enable)
  {
    ret = chdir(harp_tar_path);
    if (ret == -1) {
      fprintf(stderr, "Unable to change directory error =[%s]\n", nslb_strerror(errno));
      exit (-1);
    }  
    time1=time(NULL);
    sprintf(buf, "tar -cf %s *", harp_tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
    ret = system(buf);
    if(ret == -1) {
      fprintf(stderr,"tar system command failed %s\n", buf);
      exit(-1);
    }
    if(stat(harp_tar_name, &stat_st) == -1) {
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", harp_tar_name);
    }
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, tar file size = %ld", diff, stat_st.st_size);
  
    sprintf(buf, "gzip %s", harp_tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
    ret = system(buf);
    if(ret == -1) {
      fprintf(stderr,"gzip system command failed %s\n", buf);
      exit(-1);
    }
    if(stat(compressed_harp_tar_name, &stat_st) == -1) {
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", compressed_harp_tar_name);
    }
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, gzip tar file size = %ld", diff, stat_st.st_size);

    if(capture_clip_enable) {
      time1=time(NULL);
      sprintf(buf, "tar -cf %s *", capture_clip_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
      ret = system(buf);
      if(ret == -1) {
        fprintf(stderr,"tar system command failed %s\n", buf);
        exit(-1);
      }
      if(stat(capture_clip_tar_name, &stat_st) == -1) {
        NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", capture_clip_tar_name);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, tar file size = %ld", diff, stat_st.st_size);
     
      sprintf(buf, "gzip %s", capture_clip_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
      ret = system(buf);
      if(ret == -1) {
        fprintf(stderr,"gzip system command failed %s\n", buf);
        exit(-1);
      }
      if(stat(compressed_capture_clip_tar_name, &stat_st) == -1) {
        NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", compressed_capture_clip_tar_name);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, gzip tar file size = %ld", diff, stat_st.st_size);
    }
    if(lighthouse_enable) {
      time1=time(NULL);
      snprintf(buf, RBU_MAX_FILE_PATH_LENGTH, "tar -cf %s *", lighthouse_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
      ret = system(buf);
      if(ret == -1) {
        fprintf(stderr,"tar system command failed %s\n", buf);
        exit(-1);
      }
      if(stat(lighthouse_tar_name, &stat_st) == -1) {
        NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", lighthouse_tar_name);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, tar file size = %ld", diff, stat_st.st_size);
     
      snprintf(buf, RBU_MAX_FILE_PATH_LENGTH, "gzip %s", lighthouse_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
      ret = system(buf);
      if(ret == -1) {
        fprintf(stderr,"gzip system command failed %s\n", buf);
        exit(-1);
      }
      if(stat(compressed_lighthouse_tar_name, &stat_st) == -1) {
        NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", compressed_lighthouse_tar_name);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, gzip tar file size = %ld", diff, stat_st.st_size);
    }
    if(performance_trace_enable) {
      time1=time(NULL);
      snprintf(buf, RBU_MAX_FILE_PATH_LENGTH, "tar -cf %s *", performance_trace_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
      ret = system(buf);
      if(ret == -1) {
        fprintf(stderr,"tar system command failed %s\n", buf);
        exit(-1);
      }
      if(stat(performance_trace_tar_name, &stat_st) == -1) {
        NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", performance_trace_tar_name);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, tar file size = %ld", diff, stat_st.st_size);
     
      snprintf(buf, RBU_MAX_FILE_PATH_LENGTH, "gzip %s", performance_trace_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
      ret = system(buf);
      if(ret == -1) {
        fprintf(stderr,"gzip system command failed %s\n", buf);
        exit(-1);
      }
      if(stat(compressed_performance_trace_tar_name, &stat_st) == -1) {
        NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "File %s not exist", compressed_performance_trace_tar_name);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld, gzip tar file size = %ld", diff, stat_st.st_size);
    }
    chdir(ns_wdir);
  }
  if(g_tracing_mode)
  {
    chdir(tar_path);
    // Run tar and then zip it. DO NOT user tar -cvzf as it takes more time
    //sprintf(buf, "tar -cf %s *; gzip %s", tar_name, tar_name);
    //NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
    //system(buf);
    time1=time(NULL);
    sprintf(buf, "tar -cf %s *", tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
    system(buf);
    diff=time(NULL)-time1;
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld", diff);
    
    time1=time(NULL);
    sprintf(buf, "gzip %s", tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Making tar with cmd %s", buf);
    system(buf);
  
    diff=time(NULL)-time1;
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "diff %lld", diff);
    // TODO - Add error handling 
 
    NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Changing to dir %s", ns_wdir);
    chdir(ns_wdir);
  }
}

static void ship_tar()
{
  char buf[RBU_MAX_2K_LENGTH + 1] = "";
  char path[RBU_MAX_2K_LENGTH + 1] = "";
  long long time1, diff;
  int ret;
  ServerCptr server_ptr;
  server_ptr.server_index_ptr = (ServerInfo *) malloc(sizeof(ServerInfo));
  memset(server_ptr.server_index_ptr, 0, sizeof(ServerInfo));
  server_ptr.server_index_ptr->server_ip = (char *)malloc(sizeof(char) * 20);

  sprintf(server_ptr.server_index_ptr->server_ip, "%s", ctrl_ip);
  sprintf(path, "%s/logs/%s/", ctrl_ns_wdir, ctrl_tr_or_partition);

  if(rbu_enable)
  {
    time1=time(NULL);
    snprintf(buf, RBU_MAX_2K_LENGTH, "%s/%s", harp_tar_path, compressed_harp_tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping harp tar, cmd = %s, path = %s", buf, path);
    ret = nslb_ftp_file(&server_ptr, buf, path, 0);
    diff=time(NULL)-time1;
    if(ret != 0) {
      fprintf(stderr,"shipping tar system command failed %s\n", buf);
      exit(-1);
    }
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping harp tar done in %lld seconds", diff); 
 
    if(capture_clip_enable) {
      time1=time(NULL);
      sprintf(buf, "%s/%s", harp_tar_path, compressed_capture_clip_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping capture clip tar, cmd = %s, path = %s", buf, path);
      ret = nslb_ftp_file(&server_ptr, buf, path, 0);
      diff=time(NULL)-time1;
      if(ret != 0) {
        fprintf(stderr,"shipping tar system command failed %s\n", buf);
        exit(-1);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping capture clip tar done in %lld seconds", diff);
    }
    if(lighthouse_enable) {
      time1=time(NULL);
      snprintf(buf, RBU_MAX_2K_LENGTH, "%s/%s", harp_tar_path, compressed_lighthouse_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping lighthouse tar, cmd = %s, path = %s", buf, path);
      ret = nslb_ftp_file(&server_ptr, buf, path, 0);
      diff=time(NULL)-time1;
      if(ret != 0) {
        fprintf(stderr,"shipping tar system command failed %s\n", buf);
        exit(-1);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping lighthouse tar done in %lld seconds", diff);
    }
    if(performance_trace_enable) {
      time1=time(NULL);
      snprintf(buf, RBU_MAX_2K_LENGTH, "%s/%s", harp_tar_path, compressed_performance_trace_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping performance trace tar, cmd = %s, path = %s", buf, path);
      ret = nslb_ftp_file(&server_ptr, buf, path, 0);
      diff=time(NULL)-time1;
      if(ret != 0) {
        fprintf(stderr,"shipping tar system command failed %s\n", buf);
        exit(-1);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping performance trace tar done in %lld seconds", diff);
    }
  } 
  if(g_tracing_mode)
  {
    time1=time(NULL);
    sprintf(buf, "%s/%s", tar_path, compressed_tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping tar, cmd = %s, path = %s", buf, path);
    nslb_ftp_file(&server_ptr, buf, path, 0);
    diff=time(NULL)-time1;
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Shipping tar done in %lld seconds ", diff);
  }
}

static void extract_tar()
{
  char buf[RBU_MAX_2K_LENGTH + 512] = "";
  long long time1, diff;
  int ret;
  ServerCptr server_ptr;
  server_ptr.server_index_ptr = (ServerInfo *) malloc(sizeof(ServerInfo));
  memset(server_ptr.server_index_ptr, 0, sizeof(ServerInfo));
  server_ptr.server_index_ptr->server_ip = (char *)malloc(sizeof(char) * 20);

  sprintf(server_ptr.server_index_ptr->server_ip, "%s", ctrl_ip);

  time1=time(NULL);
  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called, rbu_enable = %d", rbu_enable);
  if(rbu_enable)
  {
    sprintf(buf, "%s/bin/nii_untar_req_rep -d %s/logs/%s -f %s -u cavisson:cavisson",
            ctrl_ns_wdir, ctrl_ns_wdir, ctrl_tr_or_partition, compressed_harp_tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Extracting harp tar, cmd = %s", buf);
    nslb_encode_cmd(buf);
    ret = nslb_run_users_command(&server_ptr, buf);
    if(ret != 0) {
      fprintf(stderr,"Extract tar system command failed %s, Error = %s\n", buf, server_ptr.cmd_output);
      exit(-1);
    }
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Extracting done");
  
    if(capture_clip_enable) {
      sprintf(buf, "%s/bin/nii_untar_req_rep -d %s/logs/%s -f %s -u cavisson:cavisson",
              ctrl_ns_wdir, ctrl_ns_wdir, ctrl_tr_or_partition, compressed_capture_clip_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Extracting capture clip tar, cmd = %s", buf);
      nslb_encode_cmd(buf);
      ret = nslb_run_users_command(&server_ptr, buf);
      if(ret != 0) {
        fprintf(stderr,"Extract tar system command failed %s\n", buf);
        exit(-1);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Capture clip extracting done");
    }
    if(lighthouse_enable) {
      snprintf(buf, RBU_MAX_2K_LENGTH + 512, "%s/bin/nii_untar_req_rep -d %s/logs/%s -f %s -u cavisson:cavisson",
              ctrl_ns_wdir, ctrl_ns_wdir, ctrl_tr_or_partition, compressed_lighthouse_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Extracting lighthouse tar, cmd = %s", buf);
      nslb_encode_cmd(buf);
      ret = nslb_run_users_command(&server_ptr, buf);
      if(ret != 0) {
        fprintf(stderr,"Extract tar system command failed %s\n", buf);
        exit(-1);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "lighthouse tar extracting done");
    }
    if(performance_trace_enable) {
      snprintf(buf, RBU_MAX_2K_LENGTH + 512, "%s/bin/nii_untar_req_rep -d %s/logs/%s -f %s -u cavisson:cavisson",
              ctrl_ns_wdir, ctrl_ns_wdir, ctrl_tr_or_partition, compressed_performance_trace_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Extracting performance trace tar, cmd = %s", buf);
      nslb_encode_cmd(buf);
      ret = nslb_run_users_command(&server_ptr, buf);
      if(ret != 0) {
        fprintf(stderr,"Extract tar system command failed %s\n", buf);
        exit(-1);
      }
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "performance trace tar extracting done");
    }
  } 
  if(g_tracing_mode)
  { 
    sprintf(buf, "%s/bin/nii_untar_req_rep -d %s/logs/%s -f %s -u cavisson:cavisson",
            ctrl_ns_wdir, ctrl_ns_wdir, ctrl_tr_or_partition, compressed_tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Extracting tar, cmd = %s", buf);
    nslb_encode_cmd(buf);
    nslb_run_users_command(&server_ptr, buf);
    diff=time(NULL)-time1;
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Extracting diff = %lld", diff);
  }
}

void remove_tmp_files()
{
  char cmd[RBU_MAX_2K_LENGTH + 16] = "";
  if(rbu_enable) 
  {
    sprintf(cmd, "rm -f %s/*", new_harp_file_path);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing tmp files, cmd = %s", cmd);
    system(cmd);

    if(capture_clip_enable) {
      sprintf(cmd, "rm -f %s/*", new_capture_clip_file_path);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing tmp files, cmd = %s", cmd);
      system(cmd);
    }
    if(lighthouse_enable) {
      snprintf(cmd, RBU_MAX_2K_LENGTH, "rm -f %s/*", new_lighthouse_file_path);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing tmp files, cmd = %s", cmd);
      system(cmd);
    }
    if(performance_trace_enable) {
      snprintf(cmd, RBU_MAX_2K_LENGTH, "rm -f %s/*", new_performance_trace_file_path);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing tmp files, cmd = %s", cmd);
      system(cmd);
    }
  }
  snprintf(cmd, RBU_MAX_2K_LENGTH + 16, "rm -f %s/* %s/*", new_req_rep_path, new_orig_file_path);
  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing tmp files, cmd = %s", cmd);
  system(cmd);
}

void remove_tar()
{
  char buf[RBU_MAX_FILE_PATH_LENGTH + 256] = "";
  int ret;
  if(rbu_enable) 
  {
    sprintf(buf, "rm -rf %s/%s", harp_tar_path, compressed_harp_tar_name);
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing tar %s from Generator", buf);
    ret = system(buf);
    if(ret == -1)
    fprintf(stderr," remove the tar system command failed %s\n", buf);
    if(capture_clip_enable) {
      sprintf(buf, "rm -rf %s/%s", harp_tar_path, compressed_capture_clip_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing capture clip tar %s from Generator", buf);
      ret = system(buf);
      if(ret == -1)
        fprintf(stderr, "remove the tar(capture clip) system command failed %s\n", buf);
    } 
    if(lighthouse_enable) {
      snprintf(buf, RBU_MAX_FILE_PATH_LENGTH + 256, "rm -rf %s/%s", harp_tar_path, compressed_lighthouse_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing lighthouse tar %s from Generator", buf);
      ret = system(buf);
      if(ret == -1)
        fprintf(stderr, "remove the tar(lighthouse) system command failed %s\n", buf);
    }
    if(performance_trace_enable) {
      snprintf(buf, RBU_MAX_FILE_PATH_LENGTH + 256, "rm -rf %s/%s", harp_tar_path, compressed_performance_trace_tar_name);
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing performance trace tar %s from Generator", buf);
      ret = system(buf);
      if(ret == -1)
        fprintf(stderr, "remove the tar(performance_trace) system command failed %s\n", buf);
    }
  }
  snprintf(buf, RBU_MAX_FILE_PATH_LENGTH + 256, "%s/%s", tar_path, compressed_tar_name);
  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing tar %s from Gen", buf);
  remove(buf);
}

//removing test run dir that was created in generator TR
void remove_test_run_dir_from_GenTar_path()
{
  char cmd[1024] = "";
  int ret;
  sprintf(cmd, "rm -r %s/logs/TR%d/.GenTar/TR%d/", ns_wdir, test_idx, test_idx);
  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Removing  temp dir %s from Gen", cmd);
  ret = system(cmd);
  if(ret == -1)
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Error in removing tmp dir %s", cmd);

  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Uploader successfully exit");
}

int process_dirs(int level)
{
  int ret;

  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called:process_dirs, level = %d", level);

  if((g_tracing_mode == 0 && rbu_enable == 0)) 
  {
    //If both tracing and rbu is diabled then return 1
    return 1; 
  }

  switch(level)
  {
    case 0:
      ret = scan_dir_and_move_files();
      if(ret == 1)
        return 1; //if return value is 1 means no file found to move

      // Fall through
    case 1:
      make_tar();
      remove_tmp_files();
      
      // Fall through
    case 2:
      ship_tar();
      extract_tar();
      remove_tar();
  }
  return 0;
}

static int is_new_partition_prepared(char *new_partition_name)
{
  DIR *dir_fp = NULL, *dir_fp1 = NULL, *dir_fp2 = NULL;
  struct dirent *orig_dirent = NULL;
  struct dirent *har_dirent = NULL;
  struct dirent *cc_dirent = NULL;
  int file_found = 0;
  char orig_path[1024] = "";
  char har_path[1024] = "";
  char capture_clip_path[1024] = "";

  //check if .orig files have been created in new partition
  sprintf(orig_path, "%s/logs/TR%d/%s/page_dump/docs", ns_wdir, test_idx, new_partition_name);
  sprintf(har_path, "%s/logs/TR%d/%s/rbu_logs/harp_files", ns_wdir, test_idx, new_partition_name);
  sprintf(capture_clip_path, "%s/logs/TR%d/%s/rbu_logs/snap_shots", ns_wdir, test_idx, new_partition_name);
 
  dir_fp = opendir(orig_path); //TODO error check
  dir_fp1 = opendir(har_path);
  dir_fp2 = opendir(capture_clip_path); 

  if(dir_fp == NULL)
  {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "dir_fp is not found..");
  }

  if(g_tracing_mode && dir_fp != NULL) 
  {
    do
    {
      orig_dirent = readdir(dir_fp);
 
      if(orig_dirent && strstr(orig_dirent->d_name, ".orig"))
      {
        file_found = 1;
        break;
      }
    }while(orig_dirent != NULL);
    closedir(dir_fp);
  }

  if(rbu_enable && dir_fp1 != NULL) 
  {
    do
    {
      har_dirent = readdir(dir_fp1);

      if(har_dirent && strstr(har_dirent->d_name, ".har"))
      {
        file_found = 1;
        break;
      }
    }while(har_dirent != NULL);  
    closedir(dir_fp1);
  }

  if(rbu_enable && capture_clip_enable && dir_fp2 != NULL)
  {
    do
    {
      cc_dirent = readdir(dir_fp2);

      if(cc_dirent && strstr(cc_dirent->d_name, "video_clip"))
      {
        file_found = 1;
        break;
      }
    }while(cc_dirent != NULL);
    closedir(dir_fp2);
  }
  return file_found;
}

#if 0
static int is_new_partition_prepared_for_har(char *new_partition_name)
{
  DIR *dir_fp = NULL;
  struct dirent *har_dirent = NULL;
  int file_found = 0;
  char har_path[1024] = "";
  //check if .orig files have been created in new partition
  sprintf(har_path, "%s/logs/TR%d/%s/rbu_logs/harp_files", ns_wdir, test_idx, new_partition_name);
  dir_fp = opendir(har_path); //TODO error check
  if(dir_fp == NULL)
  {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "dir_fp is not found..");
  }
  do
  {
    har_dirent = readdir(dir_fp);

    if(har_dirent && strstr(har_dirent->d_name, ".har"))
    {
      file_found = 1;
      break;
    }
  }while(har_dirent != NULL);

  closedir(dir_fp);
  return file_found;
}
#endif

#if 0
static void check_and_switch_partition()
{
  DIR *dir_fp = NULL;
  struct dirent *orig_dirent = NULL;
  int file_found = 0;
  char new_partition_name[50] = "";
  long long new_partition_idx = 0;
  char orig_path[1024] = "";

  //checking if partition is different in shared memory
  if(testruninfo_tbl_shr->partition_idx == partition_idx)
    return;

  //get next partition
  nslb_get_next_partition(ns_wdir, test_idx, partition_name, new_partition_name);   //TODO eeror handling
  if((new_partition_idx = atoll(new_partition_name)) <= 0)
    return;

  //check if .orig files have been created in new partition
  sprintf(orig_path, "%s/logs/TR%d/%s/page_dump/docs", ns_wdir, test_idx, new_partition_name);
  dir_fp = opendir(orig_path); //TODO error check

  do
  {
    orig_dirent = readdir(dir_fp);

    if(orig_dirent && strstr(orig_dirent->d_name, ".orig"))
    {
      file_found = 1;
      break;
    }
  }while(orig_dirent != NULL);

  closedir(dir_fp);

  //if .orig file is found, then switch partition
  if(file_found == 1)
    switch_partition(new_partition_idx);
}

static void run_in_recovery_mode()
{
  int ret;
  long long cur_partition_idx = partition_idx;
  char new_partition[20] = "";

  while(1)
  {
    ret = process_dirs(0);
    if(ret == 1 && cur_partition_idx != partition_idx)
    {
       //restore latest partition
       switch_partition(cur_partition_idx);
       break;
    }
    else
    {
      nslb_get_prev_partition(base_dir, partition_name, new_partition);
      if(atoll(new_partition) > 0)
        switch_partition(atoll(new_partition));
      else
      { 
        //restore latest partition then break
        switch_partition(cur_partition_idx);
        break;
      }
    }
  }
}
#endif

static void usage(char *msg)
{
  fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "ni_ship_gen_data "
                  "-n <Gen TestRunNum> "
                  "-P <Gen Partition_Idx> "
                  "-W <Gen Work dir> "
                  "-k <Shared memory key> "
                  "-r (recovery mode) "
                  "-s <Controller IP Address> "
                  "-p <Controller Port> "
                  "-g <Gen Name> "
                  "-w <Controller wdir> "
                  "-N <Controller TrNum\n"); 

  //exit(-1);
}

static void test_over_sig_handler(int sig) 
{
  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "POST_PROC_SIGNAL received");
  test_over_sig = 1;
}

static void set_up_signal_handler()
{
	//test over signal
  bzero(&sa, sizeof(struct sigaction));
  sa.sa_handler = test_over_sig_handler;
  sigaction(TEST_POST_PROC_SIG, &sa, NULL);
}


#define NIRRS_CONTINUE 0
#define NIRRS_SWITCH_PARTITION 2
#define NIRRS_STOP 1
#define NIRRS_ERROR -1

static int check_and_switch_partition(long long cur_partition_idx, char *next_partition, int *count)
{
  int ret;
  char cur_partition_str[32];
  int partition_flag = (partition_idx>0);
  
  NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Method called cur_partition_idx = %lld", cur_partition_idx);
  if(partition_flag)
    sprintf(cur_partition_str, "%lld", cur_partition_idx);

  //init next_partition buffer.
  if(next_partition) next_partition[0] = 0;

  //Offline mode.
  if(!reader_run_mode)
  {
    NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "offline mode");
    if(partition_flag) {
      //Check for next partition.
      //ret = nslb_get_next_partition(nd_db_upload.ns_wdir, nd_db_upload.tr_num, cur_partition_str, next_partition);
      ret = nslb_get_next_partition(ns_wdir, test_idx, partition_name, next_partition);
      //Note this function can return 0, 1(no next partition), -1(error)
      if(ret < 0) {
        NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_ERROR, "nslb_get_next_partition() Failed");
        return NIRRS_ERROR; 
      }
      if(next_partition[0]) 
      {
        NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, 
          "Offline mode: Partition switched from %s to %s", cur_partition_str, next_partition);
        return NIRRS_SWITCH_PARTITION;
      }  
      else {  //No next partition.
        NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO,
          "Offline mode: No more partition remained for processing, last partition was %s", cur_partition_str);
        return NIRRS_STOP;
      }   
    }
    else {
      NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO,
        "Offline mode: No more data remain");
      return NIRRS_STOP;
    }
  }
  else {  //Online mode.
    NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "online mode");
    if(partition_flag) {
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Current partition idx = %lld, and testruninfo_tbl_shr->partition_idx = %lld", cur_partition_idx, testruninfo_tbl_shr->partition_idx);
      //If partition change in shared memory then check for next partition.
      if(cur_partition_idx != testruninfo_tbl_shr->partition_idx)
      {
        //Note this function can return 0, 1(no next partition), -1(error)
        ret = nslb_get_next_partition(ns_wdir, test_idx, cur_partition_str, next_partition);
        if(ret < 0) {
          NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_ERROR, "nslb_get_next_partition() Failed");
          return NIRRS_ERROR; 
        }
        if(next_partition[0]) 
        {
          NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO,
                          "Partition switched in netstorm, checking for orig files in next partition");

          NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "rbu_enable= %d", rbu_enable);
          //TODO: check if next partition have orig files.
          ret = is_new_partition_prepared(next_partition);
          if(ret == 1)
          {
            NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, 
              "Online mode: Partition switched from %s to %s", cur_partition_str, next_partition);
            return NIRRS_SWITCH_PARTITION;
          }
          else 
          {
            if(*count != 5) {
              //this partition not completed.
              next_partition[0] = 0;
              *count = *count + 1;
              NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "count = %d", *count);
              return NIRRS_CONTINUE;
            }
          }
          if(*count == 5) {
            NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "waiting for 5 minutes if files not found in partion = %s"
                            "and move to next partition if exist count = %d", next_partition, *count);
            *count = 0;
            return NIRRS_SWITCH_PARTITION;
          }
        }
        else {
          NSLB_TRACE_LOG4(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No more partition remained");
          if(test_over_sig) {
            NSLB_TRACE_LOG4(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "going to test stoped");
            return NIRRS_STOP;
          }
        }  
      }
      else {
        if(test_over_sig) {
          NSLB_TRACE_LOG4(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "No next partition not found");
          return NIRRS_STOP;
        } else {
            if(*count != 5) {
              *count = *count + 1;
              NSLB_TRACE_LOG4(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "waiting for file getting in current partition, count = %d", 
                              *count);
              return NIRRS_CONTINUE;
            }
            if(*count == 5 && test_over_sig) {
              NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "wait for 5 minutes where so we exist");
              *count = 0;
              return NIRRS_STOP;
            }
        }
      }
    }
    //Continue cases will come here.
    //here we will check if test is not running then stop processing.
    //If test stop then stop thread.
    else {
      if(test_over_sig && !partition_flag) {
        NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Online mode: signal received in non partition mode.");
        return NIRRS_STOP;
      } else {
        NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "some data will be process and continue.");
        return NIRRS_CONTINUE;
      }
    }
  } 
  return NIRRS_CONTINUE;
}

int main(int argc, char *argv[])
{
  int testruninfo_shr_id, ret, sleep_duration = 60; //1 min
  char c;
  int ppid = -2;  //initializing with -2 as kill(-1, signum) sends signal to all processes
	//sighandler_t prev_handler;

  if(argc < 8)
    usage("Too few arguments");

  while ((c = getopt(argc, argv, "n:P:W:d:k:s:p:g:w:N:t:l:r:o:R:M:C:H:j:S:")) != -1) 
  {
    switch (c) 
    {
      case 'n': // Generator test run number
        test_idx = atoi(optarg);
      break;
    
      case 'P': // Generator parition. It will be same on controller
        strcpy(partition_name, optarg);
        partition_idx = atoll(optarg);
        break;

      case 'W': // Generator work dir
        strcpy(ns_wdir, optarg);
        break;

      case 'k':   //test run info shared memory key
        testruninfo_shr_id = strtoul(optarg, NULL, 10);;
        break;

      case 'r':  //if component is restarted
        recovery_mode = atoi(optarg);
        break;

      case 'o':  //running mode offline/online
        reader_run_mode = atoi(optarg);
        break;

      case 's': // Controller IP
        strcpy(ctrl_ip, optarg);
        break;
 
      case 'p': // Controller Cmon Port
        port = atoi(optarg);
        break;

      case 'g':  //Generator name
        strcpy(gen_name, optarg);
        break;

      case 'w': // Controller work dir
        strcpy(ctrl_ns_wdir, optarg);
        break;

      case 'N': // Controller TR number
        ctrl_test_idx = atoi(optarg);
        break;

      case 't':
        sleep_duration = atoi(optarg);
        break;

      case 'l':
        trace_level = atoi(optarg);
        break;
    
      case 'R':
        rbu_enable = atoi(optarg); 
        break;

      case 'M':
        g_tracing_mode = atoi(optarg);
        break;

      case 'C':
        capture_clip_enable = atoi(optarg);
        break;

      case 'H':
        lighthouse_enable = atoi(optarg);
        break;

      case 'j': //JS Profiler
        performance_trace_enable = atoi(optarg);
        break;

      case 'S':
        websocket_enable = atoi(optarg);
        break;

      default:
        usage("Invalid argument");
    }
  }
    
  trace_log_key = nslb_init_mt_trace_log(ns_wdir, test_idx, partition_idx, "ns_logs/nia_req_rep_uploader_trace.log", trace_level, trace_log_size);
  
  ppid = getppid();
  NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Gen testidx = %d, Gen wdir = %s, "
                   "Controller IP = %s, Controller Port = %d, Gen Name = %s, Controller Work = %s, Controller TRNum = %d, "
                   "trace_level = %d, trace_log_size = %d, ppid = %d, rbu_enable = %d, g_tracing_mode = %d, capture_clip_enable = %d, "
                   "lighthouse_enable = %d, performance_trace_enable = %d, websocket_enable = %d", 
                   test_idx, ns_wdir, ctrl_ip, port, gen_name, ctrl_ns_wdir, ctrl_test_idx, trace_level, trace_log_size, ppid, rbu_enable,
                   g_tracing_mode, capture_clip_enable, lighthouse_enable, performance_trace_enable, websocket_enable); 

  if(reader_run_mode)
  {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Running in online mode, reader_run_mode = %d", reader_run_mode);
  }
  else
  {
    NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Running in offline mode, reader_run_mode = %d, rbu_enable = %d", reader_run_mode, rbu_enable);
  }

  set_up_signal_handler();
  update_tr_or_partition();

  if(reader_run_mode){
    testruninfo_tbl_shr = (TestRunInfoTable_Shr *) shmat(testruninfo_shr_id, NULL, 0); //TODO
    if(testruninfo_tbl_shr == (void *)-1)
    {
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "testruninfo_tble_shr not found");  
      exit(-1);  
    }
  }
  sprintf(base_dir, "%s/logs/TR%d", ns_wdir, test_idx);
//  if(recovery_mode == 1 && partition_idx > 0)
//    run_in_recovery_mode();

  char next_partition[32] = "";
  int count = 0;
  while(1)
  {
    // Check if parent (NS Parent) is running or not
    ret = kill(ppid, 0);
    if(ret == -1 && errno == ESRCH)   //ESRCH error is pid doesn't exist
    {
      NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "Parent pid is killed, ppid = %d", ppid);
      test_over_sig = 1;
    }

    // Block SIGTERM before starting processing
		//prev_handler = signal(SIGTERM, SIG_IGN);
    ret = process_dirs(0);
    //if not data found to process then check for next task.
    if(ret == 1)
    { 
      int task = check_and_switch_partition(partition_idx, next_partition, &count);
      //Handle tasks
      if(task == NIRRS_SWITCH_PARTITION)
      {
        switch_partition(atoll(next_partition));
      }
      else if(task == NIRRS_STOP || task == NIRRS_ERROR)
        break;
    }
    // UnBlock SIGTERM
		//signal(SIGTERM, prev_handler);

    //if(ret == 1 && partition_idx > 0)  //nothing was found to ship TODO
      //check_and_switch_partition();
    //if((test_over_sig == 1 || reader_run_mode == 0) && ret == 1)
    //{
    //  NSLB_TRACE_LOG1(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "test_ove_sig received, "
    //                       "No more files to ship, hence Exiting.");
    //  break;
    //}
    if(reader_run_mode)
      sleep(sleep_duration);
    //debug contuing.
    NSLB_TRACE_LOG3(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "reader run mode continue.");
  }
  NSLB_TRACE_LOG2(trace_log_key, partition_idx, NULL, NSLB_TL_INFO, "test_over_sig = %d", test_over_sig);
  //so we will move har file only when test completed successfully.
  //removing TR%d/partition dir that was cteated in Generator TR
  remove_test_run_dir_from_GenTar_path();
  return 0;
}
