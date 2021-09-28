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
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <magic.h>

#include "url.h"
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


#define MAX_LINE_LEN 1024*3
typedef struct
{
  ns_bigbuf_t content_type;
  ns_bigbuf_t content;
  int size;
}nsFileInfo;

typedef struct
{
  char *content_type;
  char *content;
  int size;
}nsFileInfo_Shr;

static nsFileInfo *g_static_file_table = NULL;
static nsFileInfo_Shr *g_static_file_table_shr = NULL;
static int g_static_file_table_max=0;
static int g_static_file_table_total=0;
NormObjKey static_file_norm_tbl;
static int static_file_norm_tbl_init=0;

static int init_static_file_norm_table()
{

 if(!static_file_norm_tbl_init)
  {
    nslb_init_norm_id_table_ex(&static_file_norm_tbl, 1024);
    static_file_norm_tbl_init = 1;
  }
  return 0;
}

static int copy_file_into_big_buf(char *file, ns_bigbuf_t *content_offset)
{
  FILE *fp;
  char *fbuf;
  struct stat stat_buf;
  if (lstat(file, &stat_buf) == -1) {
    NS_EXIT(-1, "File %s does not exists",file);
    //is_file = 0;
  } else {
    if (stat_buf.st_size == 0) {
      NS_EXIT(-1, "File %s is of zero size. Exiting.", file);
    }
  }
  MY_MALLOC(fbuf, stat_buf.st_size, "fbuf", -1);
  fp = fopen(file, "r");
  fread(fbuf, stat_buf.st_size, 1, fp);
  fclose(fp);
  *content_offset = copy_into_big_buf(fbuf,stat_buf.st_size);
  FREE_AND_MAKE_NOT_NULL_EX(fbuf, stat_buf.st_size, "fbuf", -1);
  return stat_buf.st_size;
}

char* get_file_name(VUser *vptr, SegTableEntry_Shr* seg_ptr)
{
  char *name = NULL;
  NSDL2_VARS(vptr, NULL, "Method Called, seg_ptr->type = %d, seg_ptr = %p", seg_ptr->type, seg_ptr);
  switch(seg_ptr->type)
  {
    case VAR:
      name = ns_eval_string(seg_ptr->seg_ptr.str_ptr->big_buf_pointer);
    break;
    case STR:
      name = seg_ptr->seg_ptr.str_ptr->big_buf_pointer;
    break;
  }
  NSDL2_VARS(vptr, NULL, "File name  [%s]", name);
  return name;
}

char* get_file_content_type(int index)
{
  char *content_type;
  NSDL2_VARS(NULL, NULL, "Method Called, index = %d",index);
  content_type = g_static_file_table_shr[index].content_type;
  NSDL2_VARS(NULL, NULL, "File content_type  %s",content_type);
  return content_type;
}

char* get_file_content(int index)
{
  char *content = NULL;
  NSDL2_VARS(NULL, NULL, "Method Called, index = %d",index);
  content = g_static_file_table_shr[index].content;
  NSDL2_VARS(NULL, NULL, "File content  %p",content);
  return content;
}

int get_file_size(int index)
{
  int size;
  NSDL2_VARS(NULL, NULL, "Method Called, index = %d",index);
  size = g_static_file_table_shr[index].size;
  NSDL2_VARS(NULL, NULL, "File size  %d",size);
  return size;
}

int get_file_norm_id(char *file_path, int file_path_len)
{
  int norm_id;
  NSDL2_VARS(NULL, NULL, "Method Called, file = %s",file_path);
  norm_id = nslb_get_norm_id(&static_file_norm_tbl, file_path, file_path_len);
  return norm_id;
}

char* get_content_type(char *file)
{
  static char content_type[128];
  const char *mime;
  NSDL2_VARS(NULL, NULL, "Method Called, file = %s", file);
  magic_t magic;
  magic = magic_open(MAGIC_MIME_TYPE);
  magic_load(magic, NULL);
  magic_compile(magic, NULL);
  mime = magic_file(magic, file);
  strcpy(content_type,mime);
  magic_close(magic);
  return content_type;
}


int add_static_file_entry(char *file, int sess_idx)
{
  char *content_type;
  int norm_id;
  char file_path[MAX_LINE_LENGTH];
  char *sess_name = get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/");
  int is_new_file = 0;
  struct stat stat_buf;
  int file_path_len;

  NSDL2_VARS(NULL, NULL, "Method Called, file = %s, sess_idx = %d",file, sess_idx);

  init_static_file_norm_table();

  if(g_static_file_table_max == g_static_file_table_total)
  {
    g_static_file_table_max +=  16;
    MY_REALLOC(g_static_file_table, g_static_file_table_max*sizeof(nsFileInfo), "g_static_file_shr_mem" ,-1);
  }
  //TODO make sure file is relative path
  file_path_len = snprintf(file_path, MAX_LINE_LENGTH, "./scripts/%s/xmpp_files/%s", sess_name, file);    

  if(stat(file_path, &stat_buf) == -1)
  {
    NSDL2_VARS(NULL, NULL, "xmpp file '%s' does not exist", file_path);
    NSTL1(NULL, NULL,"xmpp file '%s' does not exist", file_path);
    return -1;
  }
  //calculate norm_id for existing file path only
  norm_id = nslb_get_or_set_norm_id(&static_file_norm_tbl, file_path, file_path_len, &is_new_file);
  if (is_new_file)
  {
    content_type = get_content_type(file_path);
    g_static_file_table_total++;
    g_static_file_table[norm_id].content_type = copy_into_big_buf(content_type, 0);
    g_static_file_table[norm_id].size = copy_file_into_big_buf(file_path, &g_static_file_table[norm_id].content);
    NSDL2_VARS(NULL, NULL, "content_type = %s, size = %d", content_type, g_static_file_table[norm_id].size);
  }
  return norm_id;
}

int add_all_static_files(int sess_idx)
{

  DIR *dir = NULL;
  struct dirent *dptr = NULL;
  char dir_path[MAX_LINE_LEN + 1]; 
  char *sess_name = get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/");
 
  NSDL2_PARSING(NULL, NULL, "Method called");

  snprintf(dir_path, MAX_LINE_LENGTH, "./scripts/%s/xmpp_files/", sess_name); 

  if ((dir = opendir(dir_path)) == NULL)
  { 
    fprintf(stderr, "Error: unable to open directory %s. errno = %d(%s)", dir_path, errno, nslb_strerror(errno));
    return 0;
  }
  while ((dptr = readdir(dir)) != NULL)
  { 
    if(!strcmp(dptr->d_name, ".") ||  !strcmp(dptr->d_name, ".."))
    {
      continue;  //ignoring . and .. directory
    }
    if (nslb_get_file_type(dir_path, dptr) == DT_REG)
    {
      if(add_static_file_entry(dptr->d_name, sess_idx) < 0)
        NSDL2_PARSING(NULL, NULL, "Can't load file %s\n", dptr->d_name);
    }
  }
  return 0;
}

void create_static_file_table_shr_mem()
{
  
  int i;
  NSDL2_PARSING(NULL, NULL, "Method called");
  if(g_static_file_table_total)
  {
    g_static_file_table_shr = (nsFileInfo_Shr *) do_shmget(g_static_file_table_total*sizeof(nsFileInfo_Shr), "g_static_file_table_shr");
    for(i=0; i<g_static_file_table_total; i++)
    {
      g_static_file_table_shr[i].content_type =  (char*)BIG_BUF_MEMORY_CONVERSION(g_static_file_table[i].content_type);
      g_static_file_table_shr[i].content = (char*)BIG_BUF_MEMORY_CONVERSION(g_static_file_table[i].content);
      g_static_file_table_shr[i].size = g_static_file_table[i].size;
    }
    FREE_AND_MAKE_NOT_NULL_EX(g_static_file_table, g_static_file_table_max*sizeof(nsFileInfo), "g_static_file_table", -1);
  }
}
