/* Name            : ns_server_admin_audit.c
 * Purpose         : To log run command status in $NS_WDIR/logs/.run_command/run_command.log
 * Initial Version : Saturday, Jan 11 2014
 * Author          : Krishna Tayal
*/

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "ns_server_admin_utils.h"
#include "ns_server_admin_audit.h"
		
#define DEFAULT_MAX_AUDIT_LOG_FILE_SIZE 10*1024*1024 // 10 MB 

/* Log Format in NS_WDIR/logs/.run_command/run_command.log:
 * Mon Jan 13 11:43:48 2014|root|ns_server_admin|192.168.1.66:7891|pwd|FAILED|pwd executed by root is FAILED 
 * */

void audit_log(int flag, char *user, char *server_ip, char *command, int machine_flag, int raw_format_for_gui, char *src, char *dest)
{
  FILE *fp;
  time_t rawtime;
  char file_path[MAX_BUF_SIZE] = {0};
  char dir_name[MAX_BUF_SIZE] = {0};
  char time_buf[MAX_BUFFER_SIZE]={0}, status[MAX_BUFFER_SIZE] = {0}, cmd[MAX_LINE_LEN]={0}, message[MAX_BUF_SIZE] = {0}, msg_buf[MAX_BUF_SIZE] = {0};
  char *ptr, *ns_wdir = NULL;
  char prev_file_path[MAX_BUF_SIZE + 1] = {0};
  int len, file_not_present = 0;
  struct stat stat_buf;
  
  ns_wdir = get_ns_wdir();

  sprintf(dir_name, "%s/logs/.run_command", ns_wdir);

  if (mkdir(dir_name, 0777)) 
  {
    //mkdir will fail if dir already exists, this is not an error, hence need not to return.
    if(strstr(nslb_strerror(errno), "File exists") == NULL)
    {
      fprintf(stderr, "Cannot create hidden directory %s. Command logging will not done.: %s", dir_name, nslb_strerror(errno));
      return;
    }
  }
  else    //if directory is created; change permission to all
    chmod(dir_name, S_IWUSR|S_IWGRP|S_IWOTH|S_IREAD|S_IRGRP|S_IROTH|S_IXGRP|S_IXOTH|S_IXUSR);

  sprintf(file_path, "%s/run_command.log", dir_name);

  //rollover log file
  if(stat(file_path, &stat_buf) == 0)
  {
    if(stat_buf.st_size >= DEFAULT_MAX_AUDIT_LOG_FILE_SIZE)
    {
    sprintf(prev_file_path, "%s.prev", file_path);

    if(rename(file_path, prev_file_path) < 0)
      fprintf(stderr, "Error in renaming '%s' file, err = %s\n", file_path, nslb_strerror(errno));
    }
  }
  else
    file_not_present = 1;

  fp = fopen(file_path, "a");
  if(!fp)
    return;
  //If File is created, changepermission to all.
  if(file_not_present)
    chmod(file_path, S_IWUSR|S_IWGRP|S_IWOTH|S_IREAD|S_IRGRP|S_IROTH|S_IXGRP|S_IXOTH|S_IXUSR);
  

  time(&rawtime);
  if(!ctime_r(&rawtime, time_buf))
    strcpy(time_buf, "NULL");
  else
  {
    len = strlen(time_buf);
    time_buf[len-1] = '\0';  //ctime_r returns time with new_line character.
  }

  if(flag < SERVER_ADMIN_SUCCESS)
    strcpy(status, "FAILED");
  else if((SERVER_ADMIN_SUCCESS == flag) || (SERVER_ADMIN_CONDITIONAL_SUCCESS == flag))
    strcpy(status, "SUCCESS");
  else if(BLOCKED == flag)
    strcpy(status, "BLOCKED");
  else if(flag == SERVER_ADMIN_SECURE_CMON)
    strcpy(status, "SECURE_CMON");
  else
    strcpy(status, "UNKNOWN");

  strcpy(cmd, command);
  ptr = cmd;
  while(*ptr)
  {
    if(':' == *ptr)
      *ptr = ' ';
    ptr++;
  }

  //Fri Jan 10 09:39:15 2014|netstorm|192.168.1.66:7891|Command|ps|BLOCKED| ls executed by netstorm is blocked
  if(strstr(command, "Upload file") != NULL)
  {
    if(dest[0] == '\0')
      sprintf(msg_buf, "Uploaded file %s at path %s", src, encoded_install_dir);
    else
      sprintf(msg_buf, "Uploaded file %s at path %s", src, dest);
  }
  else if(strstr(command, "Download file") != NULL)
  {
    if(flag == SERVER_ADMIN_SECURE_CMON)
      sprintf(msg_buf, "File %s not downloaded,", src);
    else
    {
      if(dest[0] == '\0')
        sprintf(msg_buf, "Downloaded file %s at path %s", src, encoded_install_dir);
      else
        sprintf(msg_buf, "Downloaded file %s at path %s", src, dest);
    }
  }
  else
    sprintf(msg_buf, "%s", cmd);

    //final msg
    sprintf(message, "%s executed by %s is %s %s", msg_buf, user, status, machine_flag?"on Windows":"");

  fprintf(fp, "%s|%s|%s|%s:%d|%s|%s|%s\n", time_buf, user, raw_format_for_gui?"GUI":"CMD", server_ip, server_prt, cmd, status, message);

  fclose(fp);
}
