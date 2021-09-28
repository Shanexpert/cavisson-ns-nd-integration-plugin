
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
#include "nslb_util.h"
#include "ns_static_use_once.h"
#include "ns_string.h"
#include "ns_event_log.h"
#include "ns_log.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_data_types.h"
#include "nslb_static_var_use_once.h"
#include "wait_forever.h"
#include "ns_static_use_once.h"
#include "ns_exit.h"
#include "ns_trace_level.h"
#include "nslb_cav_conf.h"
//#define MAX_LINE_LENGTH 4048

//Because default value of g_generator_idx is -1.
#define G_GEN_IDX (g_generator_idx < 0)?0:g_generator_idx

// This common method has been created for opening file in various mode  
// resolving the issue of root permisson for mantis bug 165,
// If file pointer returns NULL, the abort the test
static int open_file(FILE **fp, char *file, char *mode, int abrt_test)
{
  NSDL1_VARS(NULL, NULL, "Method called, file name = %s, mode = %s, abrt_test = %d", file, mode, abrt_test);
  *fp = fopen(file, mode);
  if(*fp == NULL){
    fprintf(stderr, "Unable to open file '%s' due to error: %s\n", file, nslb_strerror(errno));    
    if(abrt_test)
      NS_EXIT(-1, "Exiting as child is not created upto this time"); // Exit as child is not created upto this time 
      //END_TEST_RUN;
    return 1;
  }
  return 0;
}

//Note: this should only be called in case of generator.
static inline void replace_ctrl_wdir(char *file_name, char *ctrl_wdir, char *out_file)
{
  //find fourth /.
  NSDL4_VARS(NULL, NULL, "file_name = %s, ctrl_wdir = %s", file_name, ctrl_wdir);
  if(file_name[0] == '/')
  { 
    int count = 0;
    int len = 0;
    while(file_name[len])  
    {
      if(file_name[len] == '/')  count++;
      if(count == 4)
      {
       len++; 
       break;
      }
      len++; 
    }
    sprintf(out_file, "%s/%s", ctrl_wdir, &file_name[len]);
  }
  else {  //file name can be given relative to ns_wdir directory. eg. ./script/default/default/scr1/data.txt
    sprintf(out_file, "%s/%s", ctrl_wdir, file_name); 
  }
  NSDL4_VARS(NULL, NULL, "Original file = %s and changed file = %s", file_name, out_file);
}

/*This function will get called at starting time
 * of execution of netstorm*/
void save_ctrl_file()
{
  int total_nvm = global_settings->num_process;
  int i, j;
  char ctrl_data_file[5 *1024];
  FILE *fp_ctrl = NULL;
  int to_write = 0;
  char buf[30 * 255];

  memset(buf, 0, (30 * 255));

  NSDL2_VARS(NULL, NULL, "Method called, total nvm = %d", total_nvm);

  for (i = 0; i < total_group_entries; i++) {
   
    if(group_table_shr_mem[i].sequence != USEONCE)
    {
      NSDL2_VARS(NULL, NULL, "Total group entries = %d, Not USE ONCE case skipping.",
		              total_group_entries);
      continue;
    }

    nslb_uo_get_ctrl_file_name(group_table_shr_mem[i].data_fname, ctrl_data_file, G_GEN_IDX);
    NSDL2_VARS(NULL, NULL, "Method called. control file = %s", ctrl_data_file);

    open_file(&fp_ctrl, ctrl_data_file, "w", 1);
    chmod(ctrl_data_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    NSDL2_VARS(NULL,NULL, "Control file  = %s.\n", ctrl_data_file);

    fprintf(fp_ctrl, "%d|%d|%d|%d\n", testidx, total_nvm, group_table_shr_mem[i].UseOnceOptiontype, group_table_shr_mem[i].num_values);

    for(j = 0; j < total_nvm; j++) {
      //fprintf(stderr, "per_proc_vgroup_table[(j * total_group_entries) + i].start_val = %d, per_proc_vgroup_table[(j * total_group_entries) + i].num_val = %d\n", per_proc_vgroup_table[(j * total_group_entries) + i].start_val, per_proc_vgroup_table[(j * total_group_entries) + i].num_val);

      sprintf(buf, "%s%d,%d|", buf, per_proc_vgroup_table[(j * total_group_entries) + i].start_val, per_proc_vgroup_table[(j * total_group_entries) + i].num_val);
    }

     to_write = strlen(buf);
     buf[to_write - 1] = '\n';
     buf[to_write] = '\0';

     fprintf(fp_ctrl,"%s", buf);
     fclose(fp_ctrl);

    //If netcloud test then write on controller using ns_save_data_ex api.
    if(loader_opcode == CLIENT_LOADER)
    {
      if(!group_table_shr_mem[i].absolute_path_flag)
      {
        char abs_data_fname[1024];
        char *ns_controller_wdir = getenv("NS_CONTROLLER_WDIR");
        //relative file path given need to change as per controller ns_wdir.
        replace_ctrl_wdir(ctrl_data_file, ns_controller_wdir, abs_data_fname);
        ns_save_data_ex(abs_data_fname, NS_ADD_DATA_IN_FILE, "%d|%d|%d|%d\n%s", testidx, total_nvm, group_table_shr_mem[i].UseOnceOptiontype, group_table_shr_mem[i].num_values, buf);
      }
      else {
        ns_save_data_ex(ctrl_data_file, NS_ADD_DATA_IN_FILE, "%d|%d|%d|%d\n%s", testidx, total_nvm, group_table_shr_mem[i].UseOnceOptiontype, group_table_shr_mem[i].num_values, buf);
      }
    }
     
     //Must reset buffer as we are using same buffer for all NVMs
     buf[0] = '\0';
  }
}

void write_last_data_file (int used_index, int val_remaining, int total_val, u_ns_ts_t time, int fd, char *data_fname, char absolute_flag)
{  
  char buf[5 * 1024];

  NSDL2_VARS(NULL, NULL, "Method Called, used_index = %d, val_remaining = %d, total_val = %d, time = %u, fd = %d, data file = %s",  
                          used_index, val_remaining, total_val, time, fd, data_fname);

  //In case of generator we will use ns_save_data_ex api to write to controller.
  //This will be used by nvm and nvm don't have loader_opcode.
  if(send_events_to_master)
  {
    char last_file[1024] = "";
    if(!absolute_flag)
    {
      char abs_data_fname[1024];
      char *ns_controller_wdir = getenv("NS_CONTROLLER_WDIR");
      //relative file path given need to change as per controller ns_wdir.
      replace_ctrl_wdir(data_fname, ns_controller_wdir, abs_data_fname);
      nslb_uo_get_last_file_name(abs_data_fname, last_file, (int)my_port_index, g_generator_idx); 
    } 
    else 
      nslb_uo_get_last_file_name(data_fname, last_file, (int)my_port_index, g_generator_idx);
 
    ns_save_data_ex(last_file, NS_ADD_DATA_IN_FILE, "%d|%d|%d|%llu\n", used_index, val_remaining, total_val, time); 
  }

  lseek(fd, 0L, SEEK_SET);
  sprintf(buf, "%d|%d|%d|%llu\n", used_index, val_remaining, total_val, time);
  NSDL2_VARS(NULL, NULL, "Buf = %s", buf);
    
  if(write(fd, buf, strlen(buf)) < 0)
  {
    fprintf(stderr, "Error in writing the data in last file. FD = %d\n",fd);
    perror("write");
    return;
  }
}

void remove_ctrl_and_last_files()
{
  int i,j;
  char last_file_name[5 * 1024];
  char ctrl_data_file[5 *1024];

  NSDL2_VARS(NULL, NULL, "Method called");
  for (j = 0; j < total_group_entries; j++)
  {
    if(group_table_shr_mem[j].sequence != USEONCE)
      continue;
    for (i = 0; i < global_settings->num_process; i++)
    {
      nslb_uo_get_last_file_name(group_table_shr_mem[j].data_fname, last_file_name, i, G_GEN_IDX);//last file is per grp and per NVM
      NSDL2_VARS(NULL, NULL, "Removing %s file", last_file_name);
      unlink(last_file_name);
    }
    nslb_uo_get_ctrl_file_name(group_table_shr_mem[j].data_fname, ctrl_data_file, G_GEN_IDX);//ctrl file is per grp
    NSDL2_VARS(NULL, NULL, "Removing %s file", ctrl_data_file);
    unlink(ctrl_data_file);
  }
}


void divide_data_files ()
{
  int i, j;
  int start_val, num_val;
  int line_number;
  FILE *org_data_fp = NULL;
  FILE *unused_data_fp = NULL;
  FILE *used_data_fp = NULL;
  char unused_data_file[5 * 1024] = {0};
  char used_data_file[5 * 1024] = {0};
  char used_data_file_path[1024] = {0};
  char cmd[5 * 1024] = {0};
  char *ptr = NULL;
  int total_val;
  int num_hdr_line;
  char tmp;
  char err_msg[1024]= "\0";
 
  NSDL2_VARS(NULL, NULL, "Method called, total_group_entries = %d", total_group_entries);

  for(j = 0; j < total_group_entries; j++)
  {
    NSDL2_VARS(NULL, NULL, "group_table_shr_mem[j].sequence = %d", group_table_shr_mem[j].sequence);
    if(group_table_shr_mem[j].sequence != USEONCE)
      continue;

    if((group_table_shr_mem[j].data_fname[0] == '\0'))
     continue;

    sprintf(unused_data_file, "%s.unused", group_table_shr_mem[j].data_fname);

    if(!strcasecmp(group_table_shr_mem[j].UseOnceWithinTest, "NO")) 
      sprintf(used_data_file, "%s.used", group_table_shr_mem[j].data_fname);
    else
    {
      //data_fname - contain absolute path of file
      if((ptr = strrchr(group_table_shr_mem[j].data_fname, '/')))
      {
        tmp = *ptr;
        *ptr = '\0';
      }

      sprintf(used_data_file_path, "%s/logs/TR%d/scripts/%s/.use_once/%s", 
               g_ns_wdir, testidx, 
               get_sess_name_with_proj_subproj_int(session_table_shr_mem[group_table_shr_mem[j].sess_idx].sess_name, group_table_shr_mem[j].sess_idx, "/"), 
               (group_table_shr_mem[j].absolute_path_flag==1)?group_table_shr_mem[j].data_fname:""); 

      NSDL2_VARS(NULL, NULL, "used_data_file_path = %s", used_data_file_path);
      sprintf(cmd, "mkdir -p %s", used_data_file_path);
      
      if(nslb_system(cmd,1,err_msg) != 0) {
        NSTL1_OUT(NULL, NULL, "Failed to execute command %s", cmd);
        continue;
      }

      sprintf(used_data_file, "%s/%s.used", used_data_file_path, (ptr + 1));
      *ptr = tmp;
    } 
  
    NSDL2_VARS(NULL, NULL, "group_table_shr_mem[%d].data_fname = %s, unused_data_file = %s, used_data_file = %s", 
                            j, group_table_shr_mem[j].data_fname, unused_data_file, used_data_file);
    
    open_file(&org_data_fp, group_table_shr_mem[j].data_fname, "r", 1);
    open_file(&used_data_fp, used_data_file, "w", 1);
    //In case of UseOnceWithInTest = Yes, <data_file>.unused file is not created
    if(!strcasecmp(group_table_shr_mem[j].UseOnceWithinTest, "NO"))
      open_file(&unused_data_fp, unused_data_file, "w", 1);

    line_number = 0;
    total_val = 0;
    start_val = 0;
    num_val = 0;
    NSDL2_VARS(NULL, NULL, "First data line = %d", group_table_shr_mem[j].first_data_line);
    num_hdr_line = group_table_shr_mem[j].first_data_line - 1;
    for(i = 0; i < global_settings->num_process; i++)
    {
      NSDL2_VARS(NULL, NULL, "Before per_proc: nvm = %d, start_val = %d, cur_val = %d, num_val = %d, total_val = %d, "
                             "Set: = start_val = %d, num_val = %d, total_val = %d", 
                              i, per_proc_vgroup_table[(i * total_group_entries)+ j].start_val,
                              per_proc_vgroup_table[(i * total_group_entries)+ j].cur_val, 
                              per_proc_vgroup_table[(i * total_group_entries)+ j].num_val, 
                              per_proc_vgroup_table[(i * total_group_entries)+ j].total_val,
                              start_val, num_val, total_val);

      //start_val += per_proc_vgroup_table[(i * total_group_entries)+ j].start_val;
      start_val += per_proc_vgroup_table[(i * total_group_entries)+ j].cur_val;
      num_val = per_proc_vgroup_table[(i * total_group_entries)+ j].num_val;
      total_val += per_proc_vgroup_table[(i * total_group_entries)+ j].total_val;

      NSDL2_VARS(NULL, NULL, "After : nvm = %d, start_val = %d, num_val = %d, total_val = %d", i, start_val, num_val, total_val);
      NSDL2_VARS(NULL, NULL, "Creating %s file", used_data_file);
      
      //Manish Fix bug 3265
      if (per_proc_vgroup_table[(i * total_group_entries)+ j].total_val == 0) continue; 
      nslb_uo_create_data_file (start_val, num_val, total_val, org_data_fp, used_data_fp, unused_data_fp, &line_number, &num_hdr_line);
      start_val = total_val;
    } //For loop NVM
    
    fclose(used_data_fp);
    if(unused_data_fp)
      fclose(unused_data_fp);
    fclose(org_data_fp);
    /*Now delete original data file
     *and rename used file as data file */
    if(!strcasecmp(group_table_shr_mem[j].UseOnceWithinTest, "NO")){
      NSDL2_VARS(NULL, NULL, "Moving %s to %s", used_data_file, group_table_shr_mem[j].data_fname);
      int ret = rename(unused_data_file, group_table_shr_mem[j].data_fname);
      if(ret < 0) 
        NSTL1_OUT(NULL, NULL, "Unable to rename file %s to %s, error = %s", unused_data_file, 
                               group_table_shr_mem[j].data_fname, nslb_strerror(errno)); 
    }
    org_data_fp = used_data_fp = unused_data_fp = NULL;
  }//For loop total_group_entries
}

/*This function will get called
 * at starting of parsing data file*/
void open_last_file_fd()
{
  int i,j;
  char last_file_name[5 * 1024];
  int fd;

  NSDL2_VARS(NULL, NULL, "Method called");

  for (j = 0; j < total_group_entries; j++)
  {
    if(group_table_shr_mem[j].sequence != USEONCE)
      continue;

    for (i = 0; i < global_settings->num_process; i++)
    {
      nslb_uo_get_last_file_name(group_table_shr_mem[j].data_fname, last_file_name, i, G_GEN_IDX);//last file is per grp and per NVM
      fd = open(last_file_name, O_CREAT|O_WRONLY|O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); 
      //fd = open(last_file_name, O_CREAT|O_WRONLY, 00666);
      NSDL2_VARS(NULL, NULL, "Last file = %s, FD = %d", last_file_name, fd);
      per_proc_vgroup_table[(i * total_group_entries)+ j].last_file_fd = fd;
    }
  }
}

/*This function will get called
 * at starting of parsing data file*/
void close_last_file_fd()
{
  int i,j;
  //char last_file_name[5 * 1024];
  //int fd;

  NSDL2_VARS(NULL, NULL, "Method called");

  for (j = 0; j < total_group_entries; j++)
  {
    for (i = 0; i < global_settings->num_process; i++)
    {
      close(per_proc_vgroup_table[(i * total_group_entries)+ j].last_file_fd);
      /*
      get_last_file_name(group_table_shr_mem[j].data_fname, last_file_name, i, G_GEN_IDX);//last file is per grp and per NVM
      fd = open(last_file_name, O_CREAT|O_WRONLY); 
      per_proc_vgroup_table[(i * total_group_entries)+ j].last_file_fd = fd;
      */
    }
  }
}
