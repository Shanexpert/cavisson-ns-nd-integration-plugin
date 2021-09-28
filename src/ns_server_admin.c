/* Name            : ns_server_admin.c
 * Purpose         : To upgrade Cmon server, show versions, restart cmon.
 * Initial Version : Monday, May 04 2009 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>
#include <time.h>
#include <pwd.h>
#include <getopt.h>
#include <openssl/md5.h>

#include "nslb_sock.h"
#include "ns_server_admin_utils.h"
#include "ns_data_types.h"
#include "ns_server_admin_audit.h"
#include "ns_exit.h"

#define STAR "\n***************************************************************************"

#define MAX_ONCE_WRITE 16384   //16 K
#define MAX_LINE_LEN 10000     //Changes MAX_LINE_LEN 512 -> 10000 Because when we pass long command in nsu_server_admin, then it core dumps. 
#define MAX_MSG_SIZE 5120 

//Macros for hashkey code
#define MAX_HASHKEY_LEN 256
#define GEN_KEY_LEN 20
#define TIME_STAMP_LEN 12
#define TIME_BUFF_LEN 128


// Control Message
#define SESSION_STARTED "SESSION_STARTED"

#define FTP_FILE_CREATED "FTP_FILE_CREATED"

#define CMD_EXECUTED_SUCCESSFULY "Command executed successfully"
#define CMD_EXECUTION_ERROR "Error: Command execution error"

static int cmd_ok_msg_len = 30; // Length of CMD_EXECUTED_SUCCESSFULY + new line
//static int cmd_error_msg_len = 31; // Length of CMD_EXECUTION_ERROR + new line

//#define  _LF_ __LINE__, (char *)__FUNCTION__ 

#define FREE_AND_MAKE_NULL(to_free, msg, index)  \
{                        \
  if (to_free)   \
  { \
    free(to_free);  \
    debug_log(0, _LF_, "MY_FREE'ed (%s) done. Freeing ptr = %p for index %d", msg, to_free, index); \
    to_free = NULL; \
  } \
}


#define MY_MALLOC(new, size, msg, index) {                              \
    if (size < 0)                                                       \
      {                                                                 \
        fprintf(stderr, "Trying to malloc a negative size (%d) for index %d\n", (int)size, index); \
      }                                                                 \
    else if (size == 0)                                                 \
      {                                                                 \
        new = NULL;                                                     \
      }                                                                 \
    else                                                                \
      {                                                                 \
        new = (void *)malloc( size );                                   \
        if ( new == (void*) 0 )                                         \
        {                                                               \
          NS_EXIT (-1, "Out of Memory: %s for index %d\n", msg, index);                                         \
        }                                                               \
      }                                                                 \
  }


#define MY_REALLOC(buf, size, msg, index)  \
{ \
    if (size <= 0) {  \
      NS_EXIT (-1, "Trying to realloc a negative or 0 size (%d) for index  %d\n", (int)size, index);  \
    } else {  \
      buf = (void*)realloc(buf, size); \
      if ( buf == (void*) 0 )  \
      {  \
         NS_EXIT(-1, "Out of Memory: %s for index %d\n", msg, index);  \
      }  \
    } \
  }

#define FREE_AND_MAKE_NULL(to_free, msg, index)  \
{                        \
  if (to_free)   \
  { \
    free(to_free);  \
    debug_log(0, _LF_, "MY_FREE'ed (%s) done. Freeing ptr = %p for index %d", msg, to_free, index); \
    to_free = NULL; \
  } \
}

#define MAX_USERNAME_LEN 256
unsigned short server_prt = 7891;
int h_errno;
int errno;
static int debug_on;
static int package_fd = -1, upgrade_shell_fd = -1;
static int package_size, upgrade_shell_size;
static int a_flag = 1, p_flag, s_flag, c_flag, j_flag, m_flag, r_flag, S_flag, x_flag, k_flag, v_flag, K_flag, raw_format_for_gui  = 1, ftp_flag, ignore_server_flag = 1, download_flag, R_flag, download_location_flag, output_format_flag = 1, u_flag = 0, o_flag = 0, t_flag = 0, I_flag = 0, write_in_file_flag = 0;
static FILE *debug_fp;
static char g_ns_wdir[MAX_USERNAME_LEN] = {0};
static char username[MAX_USERNAME_LEN] = {0};
static char origin_cmon[MAX_USERNAME_LEN] = {0};

char ip_port[MAX_USERNAME_LEN]="\0";
static char upgrade_shell_without_path[MAX_LINE_LEN]="\0";
static char package_name_without_path[MAX_LINE_LEN]="\0";

/*If Window server then : is seperated by %3A else same as install_dir*/
char encoded_install_dir[MAX_LINE_LEN + MAX_LINE_LEN + 1] = "\0";

static char hash_key[MAX_HASHKEY_LEN] = {0};
static char download_file_location[MAX_LINE_LEN] = {0};

static int read_file_data_from_cs(ServerCptr *ptr, int replace_flag, char *download_location, char *file_name);

//ServerInfo *servers_list = NULL;
int total_no_of_servers = 0;

// This replaces the string into a buffer
// AA:AA ==> AA%3AAA
// Where buf =  AA:AA 
// from = :
// to = %3A
void replace_strings1(char *buf, char* from, char* to) {
  char tmp_buf[MAX_BUF_SIZE];
  strcpy(tmp_buf, buf);
  buf[0] = '\0';
  char *p;
  char *q = tmp_buf;
  int len = strlen(from);

  while((p = strstr(q, from)) != NULL) {
    strncat(buf, q, p - q);
    strcat(buf, to);
    q = p + len;
  }
  strcat(buf, q);
}

static void usage()
{
   fprintf(stderr, "Usage:\n");
   fprintf(stderr, "nsu_server_admin <-s|-a> [-P] <operation> <arguments>\n");
   fprintf(stderr, "  [-a or -s]               : All Servers or Specific Server Name/I.P.\n");
   fprintf(stderr, "Where operation are:\n");
   fprintf(stderr, "  [-i|ignore_server_entry] : CavAgent port\n");
   fprintf(stderr, "  [-P]                     : CavAgent port\n");
   fprintf(stderr, "  [-c|--run-cmd]           : Run command\n");
   fprintf(stderr, "    Eg: 'ps -ef' Note- command should be in single quote\n");
   fprintf(stderr, "  [-F|--upload]            : Ftp file name.\n");
   fprintf(stderr, "    Eg: -F my_ftp_file -D /dest/path/to/upload\n");
   fprintf(stderr, "    Note - Make sure destination directory does not contain file name\n");
   fprintf(stderr, "         - file will be uploaded as name my_ftp_file on given server\n");
   fprintf(stderr, "      Eg: -F my_ftp_file -D /dest/path/to/upload/new_my_ftp_file    [Wrong]\n");
   fprintf(stderr, "  [-D|--dest]              : Ftp file destination.\n");
   fprintf(stderr, "    Note - this operation will used with -F operation only\n");
   fprintf(stderr, "  [-G|--download]          : Download file name.\n");
   fprintf(stderr, "  [-f|--file]              : Download file destination.\n");
   fprintf(stderr, "  [-d]                     : Enable debug\n");
   fprintf(stderr, "  [-j|--show_java_core]    : Show Java Core dumps on specified server (Not For Windows).\n");
   fprintf(stderr, "  [-p|--upgrade]           : Package tar/zip file name.\n");
   fprintf(stderr, "  [-m|--monitors]          : Show monitors running on specified servers (Not For Windows).\n") ;
   fprintf(stderr, "  [-r|--restart]           : Restart cmon on specified server.\n"); 
   fprintf(stderr, "  [-S|--show]              : Show cmon (Not For Windows).\n"); 
   fprintf(stderr, "  [-x|--start]             : start cmon (Not For Windows).\n"); 
   fprintf(stderr, "  [-k|--stop]              : stop cmon.\n"); 
   fprintf(stderr, "  [-v|--version]           : Show cmon's version on specified server.\n"); 
   fprintf(stderr, "  [-R|--replace]           : Replace existing file or not.\n"); 
   fprintf(stderr, "  [-o|--origin_cmon]       : Origin Cmon name.\n"); 
   fprintf(stderr, "  [-I|--cmon_info]         : Cmon info.\n"); 
   fprintf(stderr, "  [-C|--check_port]        : Validate ip:port[:timeout(in seconds)]\n");
   fprintf(stderr, "  [-w|--write_in_file]     : Used when output is to write in file rather than console\n");
   exit(-1);
}

char *get_ns_wdir()
{
  if(getenv("NS_WDIR"))
    return(getenv("NS_WDIR"));
  else
    return("/home/cavisson/work");
}

static void check_user(char *opt)
{
  struct passwd *pw;
  pw = getpwuid(getuid());
  if (pw == NULL)
  {
    NS_EXIT (-1, "Error: Unable to get the real user name");
  }
  if (strcmp(pw->pw_name, "root") && strcmp(pw->pw_name, "admin"))
  {
    NS_EXIT (-1, "Error: To %s server(s) this command must be run as 'root' or 'admin' user only. Currently being run as '%s'\n", opt, pw->pw_name);
  }
}

static char *get_cur_date_time()
{
  time_t  tloc;
  struct  tm *lt;
  static  char cur_date_time[100];

  (void)time(&tloc);
  if((lt = localtime(&tloc)) == (struct tm *)NULL)
    strcpy(cur_date_time, "Error");
  else
    sprintf(cur_date_time, "%02d/%02d/%02d %02d:%02d:%02d", lt->tm_mon + 1, lt->tm_mday, (1900 + lt->tm_year)%2000, lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_date_time);
}

//this method is created because of showing o/p of an command, in debug_log buffer size is the issue for the o/p
// TODO - Handle binary data
static void debug_log_long_data(int line, char *fname, char *data)
{
  char buffer[MAX_BUF_SIZE + 1] = "\0";

  if(debug_on)
  {
    int len = sprintf(buffer, "\n%s|%d|%s|", get_cur_date_time(), line, fname);
    if((fwrite(buffer, len, 1, debug_fp))<0)
    {
      NS_EXIT (-1, "Unable to write to debug");
    }
    if((fwrite(data, strlen(data), 1, debug_fp))<0)
    {
      NS_EXIT (-1, "Unable to write to debug");
    }
  }

  if(output_format_flag == 0)
    fprintf(stdout, "%s", data);
}

static void debug_log(int type, int line, char *fname, char *format, ...)
{
  va_list ap;
  char buffer[MAX_BUF_SIZE + 1];
  int amt_written = 0, amt_written1=0;

  amt_written1 = sprintf(buffer, "\n%s|%d|%s|", get_cur_date_time(), line, fname);
  va_start(ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_BUF_SIZE] = 0;

  if(debug_on)
  {
    if((fwrite(buffer, amt_written1+amt_written, 1, debug_fp))<0)
    {
      NS_EXIT (-1, "Unable to write to debug");
    }
  }

  if(type == 1)
  {
    fprintf(stdout, "%s\n", buffer + amt_written1);
  }
}

static int send_msg_to_createserver(ServerCptr *ptr, char *SendMsg)
{
  debug_log(0, _LF_, "Method Called, Sending %s to CavMonAgent.", SendMsg);

  if (send(ptr->fd, SendMsg, strlen(SendMsg), 0) != strlen(SendMsg))
  {
    debug_log(0, _LF_, "Method Called, Sending failed %s to CavMonAgent.", SendMsg);
    return SERVER_ADMIN_FAILURE;
  }
  return SERVER_ADMIN_SUCCESS;
}

static int read_control_msg_from_cs(ServerCptr *ptr, char *expected_msg, int show_error)    //show_error is choosen as for restarting cmon we dont want to show any error message
{
  char cmon_msg[MAX_MSG_SIZE + 1] = "\0";
  char *cmd_line, *line_ptr;
  int bytes_read = 0, total_read = 0;
 
  debug_log(0, _LF_, "Method Called. expected_msg = %s, show_error = %d", expected_msg, show_error);

  while(1)
  {
    if((bytes_read = read (ptr->fd, cmon_msg + total_read, MAX_MSG_SIZE - total_read)) < 0)
      continue;
    else if(bytes_read == 0)
    {
      debug_log(show_error, _LF_, "Connection closed from other side.");
      return SERVER_ADMIN_FAILURE;
    }

    total_read += bytes_read;
    cmon_msg[total_read] = '\0';
   
    line_ptr = cmon_msg;
    char *tmp_ptr;
    if ((tmp_ptr = strstr(line_ptr, "\n"))) //Extracting cmd line 
    {
      int msg_len = 0;
      *tmp_ptr = 0; // NULL terminate 
      cmd_line = line_ptr;

      if(!strcmp(cmd_line, expected_msg))
      {
        debug_log(0, _LF_, "Recieved %s", expected_msg);
        return SERVER_ADMIN_SUCCESS;
      }
      debug_log(show_error, _LF_, "Unexpected control message recived. Msg = %s\n", cmd_line);
      return SERVER_ADMIN_FAILURE;
    }
    // Wait till we get complete line
    bcopy(line_ptr, cmon_msg, total_read + 1); // Must be NULL terminated
  }
  return SERVER_ADMIN_SUCCESS;
}

static void fill_cmd_output_buf(ServerCptr *ptr, char *data, int new_line_flag)
{
  int msg_len;
  msg_len = strlen(data);
  ptr->cmd_output_size = ptr->cmd_output_size + msg_len + 1;  //+1 for adding \n
  MY_REALLOC(ptr->cmd_output, ptr->cmd_output_size, "ptr->cmd_output", -1);
  //if(new_line_flag)
    //sprintf(ptr->cmd_output, "%s%s\n", ptr->cmd_output, data);
  //else
    sprintf(ptr->cmd_output, "%s%s", ptr->cmd_output, data);
}

static int read_command_output_from_cs(ServerCptr *ptr, int show_error)    //show_error is choosen as for restarting cmon we dont want to show any error message
{
  char cmon_msg[MAX_MSG_SIZE + 1];
  char *cmd_line, *line_ptr;
  int bytes_read = 0;

  int bytes_to_print = 0;
  char tmp_buf[62 + 1]; // This to keep track of last line to check Cmd success or error message
  char last_line[31 + 1]; // This to keep track of last line to check Cmd success or error message
  int last_line_len = 0;
  int total_len = 0;
  int last_line_to_print = 0, msg_len;
  last_line[0] = '\0';

  debug_log(0, _LF_, "Method Called");

  // Command OK or Error protocol:
  // Command output from cmon has following control messages at the end of reply 
  //  CMD_EXECUTED_SUCCESSFULY or CMD_EXECUTION_ERROR (followed by newline)
  // We need to read and then keep last 31 chars (max of ok or err msg) so that
  // if read is parital, we can keep in last_line and then compare at the end
  //
  // Also these control message is NOT to be printed
  //
  while(1)
  {
    //if((bytes_read = read (ptr->fd, cmon_msg, 7)) < 0)  // added for partial msg testing 
    if((bytes_read = read (ptr->fd, cmon_msg, MAX_MSG_SIZE)) < 0)
      continue;

    if(bytes_read == 0)
    {
      debug_log(show_error, _LF_, "Connection closed from other side.");
      return SERVER_ADMIN_FAILURE;
    }

    cmon_msg[bytes_read] = '\0';

    line_ptr = cmon_msg;

    if(bytes_read >= 31)
    {
      // First print any thing saved in last_line
      if(last_line_len != 0)
      {
        debug_log_long_data(_LF_, last_line); 

        fill_cmd_output_buf(ptr, last_line, 0);

        last_line_len = 0;
        last_line[0] = '\0';
      }

      // How many bytes can be safely printed
      bytes_to_print = bytes_read - 31;

      // First Copy in last_line so that we do not lose due to null temination
      bcopy(line_ptr + bytes_to_print, last_line, 31);
      last_line_len = 31;
      last_line[31] = '\0';
      
      cmon_msg[bytes_to_print] = '\0';
      debug_log_long_data(_LF_, cmon_msg);

      fill_cmd_output_buf(ptr, cmon_msg, 0);
    }
    else
    {
       total_len = bytes_read + last_line_len;
       if(total_len >= 31) // Here it will come only if last_line_len > 0 and total is >= 31
       {
         // Print part from last_line
         last_line_to_print = total_len - 31; // This cannot be -ve
         //copy to tmp_buf and null teminate
         bcopy(last_line, tmp_buf, last_line_to_print);
         //bcopy(last_line + last_line_to_print, tmp_buf, last_line_len );
         tmp_buf[last_line_to_print] = '\0';
         //print
         debug_log_long_data(_LF_, tmp_buf);

         fill_cmd_output_buf(ptr, tmp_buf, 0);

         // shift lastLine
         memmove(last_line, last_line + last_line_to_print, last_line_len - last_line_to_print);
         // Append
         //bcopy(line_ptr, last_line + last_line_len, bytes_read);
         bcopy(line_ptr, last_line + (last_line_len - last_line_to_print), bytes_read);
         last_line_len = 31;
         last_line[31] = '\0';
       }
       else 
       {
         // Append in last_line
         bcopy(line_ptr, last_line + last_line_len, bytes_read);
         last_line_len += bytes_read;
         last_line[last_line_len] = '\0';
       } 
    }

    // Check last_line
    if(strstr(last_line, CMD_EXECUTED_SUCCESSFULY))
    {
      // print from last_line before msg
      if(last_line_len > cmd_ok_msg_len)
      {
        // Since CMD_EXECUTED_SUCCESSFULY size is 30 and we are keeping 31, there will be one char pending to print
        last_line_to_print = last_line_len - cmd_ok_msg_len;
        last_line[last_line_to_print] = '\0';
        debug_log_long_data(_LF_, last_line);

        fill_cmd_output_buf(ptr, last_line, 1);
      }
      return SERVER_ADMIN_SUCCESS;
    }
    else if(strstr(last_line, CMD_EXECUTION_ERROR))
    {
      return SERVER_ADMIN_FAILURE;
    }
  }
  // Code will never come here
  return SERVER_ADMIN_FAILURE;
}

static void close_session_and_fd(ServerCptr *ptr)
{
  char end_session_msg[MAX_USERNAME_LEN] = {0};

  if(o_flag)
    sprintf(end_session_msg, "CLOSE_SESSION:ORIGIN_CMON=%s\n", origin_cmon); //TODO: DISCUSS colon'separator' here
  else
    sprintf(end_session_msg, "CLOSE_SESSION:\n");

   //send_msg_to_createserver(ptr, "CLOSE_SESSION:\n");
   send_msg_to_createserver(ptr, end_session_msg);
   close(ptr->fd);
}

static int is_session_started(ServerCptr *ptr)
{
  char start_session_msg[MAX_USERNAME_LEN] = {0};

  debug_log(0, _LF_, "Method Called");

  if(o_flag)
    sprintf(start_session_msg, "START_SESSION:ORIGIN_CMON=%s\n", origin_cmon);

  // This was added to restart cmon forcly, becauase in cmon added a server session limit(100) since release 4.1.5 build 52 and release 4.1.6 build 8. When the limit is reached we cannot start any session on cmon. that's why restart option is given START_SESSION_FORCED, so that session can be started forcly.
  else if(r_flag)
    sprintf(start_session_msg, "START_SESSION_FORCED:\n");

  else
    sprintf(start_session_msg, "START_SESSION:\n");
  
  //if (send_msg_to_createserver(ptr, "START_SESSION:\n") == SERVER_ADMIN_FAILURE)
  if (send_msg_to_createserver(ptr, start_session_msg) == SERVER_ADMIN_FAILURE)
  {
    return SERVER_ADMIN_FAILURE;
  }
  if(read_control_msg_from_cs(ptr, SESSION_STARTED, 1) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Session not started");
    return SERVER_ADMIN_FAILURE;
  }

  return SERVER_ADMIN_SUCCESS;
}

static unsigned int get_file_size(char *file_name, int *fd)
{
  struct stat stat_buf;

  debug_log(0, _LF_, "Method Called, file name = %s", file_name);

 if(*fd <= 0) //fd is not open, open it
  {
    if((*fd = open(file_name, O_RDONLY|O_CLOEXEC)) < 0)
    {
      NS_EXIT (-1, "Error: Error in opening %s\n", file_name);
    }
  }

  if((stat(file_name, &stat_buf) == 0))
  {
    unsigned int size = stat_buf.st_size;
    return (size);
  }
  else
  {
    NS_EXIT(-1,  "Stat failed to get file size %s\n.", file_name);
  }
}

static int send_ftp_header(ServerCptr *ptr, char *msg, int len)
{

  debug_log(0, _LF_, "Method Called, Sending header '%s' of size %d", msg, len);

  if(write(ptr->fd, msg, len) < 0)
  {
    fprintf(stderr, "Unable to write.\n");
    return SERVER_ADMIN_FAILURE;
  }

  if(read_control_msg_from_cs(ptr, FTP_FILE_CREATED, 1) == SERVER_ADMIN_FAILURE)
  {
     fprintf(stderr, "Expected message FTP_FILE_CREATED not recieved.\n");
     return SERVER_ADMIN_FAILURE;
  }

  return SERVER_ADMIN_SUCCESS;
}

static int start_ftp_file(ServerCptr *ptr, char *file_name, unsigned int file_size, int file_fd, char *dest)
{
  char header[256]="\0";
  char buffer[MAX_ONCE_WRITE + 1]="\0";
  int header_len;
  int read_bytes;
  unsigned int total_read_bytes = 0 ;

  debug_log(0, _LF_, "Method Called, file name = %s, file size = %lu", file_name, file_size);

  //TODO: UPLOADFILE
  header_len = sprintf(header, "FTPFile:%s/%s:%u\n", dest, basename(file_name), file_size);

  //seek to start of file as this method will call again & again for each fd
  if(lseek(file_fd, 0, SEEK_SET) < 0)
  {
    NS_EXIT (-1, "Seek failed !");
  }  

  if(send_ftp_header(ptr, header, header_len) == SERVER_ADMIN_FAILURE)
  { 
     debug_log(1, _LF_, "Can not send bytes to Server.");
     return SERVER_ADMIN_FAILURE;
  }


  //now send bytes to Server as FTP_FILE_CREATED recieved
  while(total_read_bytes < file_size) 
  {
     buffer[0]='\0';

     if((read_bytes = read(file_fd, buffer, MAX_ONCE_WRITE)) < 0)
     {
       debug_log(1, _LF_, "Unable to read data from file '%s'", file_name);
       return SERVER_ADMIN_FAILURE;
     }
     if(read_bytes == 0)
     {
         continue;
     }
     if(write(ptr->fd, buffer, read_bytes) < read_bytes)
     {
       debug_log(1, _LF_, "Unable to send data to cmon");
       return SERVER_ADMIN_FAILURE;
     }
     total_read_bytes = total_read_bytes + read_bytes;
  }
 
  if(read_control_msg_from_cs(ptr, CMD_EXECUTED_SUCCESSFULY, 1) == SERVER_ADMIN_FAILURE)
  {
     fprintf(stderr, "File may not be written completely");
     return SERVER_ADMIN_FAILURE;
  }
  return SERVER_ADMIN_SUCCESS;
}

static int run_cmd_and_read_op(char *command, ServerCptr *ptr, int show_error)
{
  debug_log(0, _LF_, "Method Called, command = %s", command);

  //set size to 0
  ptr->cmd_output_size = 0;
  //if output is not null free it and make it null.
  if(ptr->cmd_output)
  {
    FREE_AND_MAKE_NULL(ptr->cmd_output, "ptr->cmd_output", -1);
  }
 
  //allocating 1 byte & initialising with null
  //MY_REALLOC(ptr->cmd_output, 1, "ptr->cmd_output", -1);
  MY_MALLOC(ptr->cmd_output, 1, "ptr->cmd_output", -1);
  ptr->cmd_output[0]='\0';
  ptr->cmd_output_size = 1;

  if (send_msg_to_createserver(ptr, command) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Sending failed");
    return SERVER_ADMIN_FAILURE;
  }

  if(!write_in_file_flag)
  {
    if(read_command_output_from_cs(ptr, show_error) == SERVER_ADMIN_FAILURE)
    {
      if(show_error)
        fprintf(stderr, "Error in reading output from CavMonAgent.\n");
      return SERVER_ADMIN_FAILURE;
    }
  }
  else
  {
    if(read_file_data_from_cs(ptr, R_flag, download_file_location, "COMMAND_FILE") == SERVER_ADMIN_FAILURE)
    {
      fprintf(stderr, "Error in reading output from CavMonAgent.\n");
      return SERVER_ADMIN_FAILURE;
    } 
  }
  return SERVER_ADMIN_SUCCESS;
}

static int make_connections(ServerCptr *ptr, int show_error)
{
  char error_msg[MAX_BUF_SIZE]="\0";

  if(!raw_format_for_gui)
  {
    if(show_error)
      debug_log(1, _LF_, "Making connection to server %s\n", ptr->server_index_ptr->server_ip);
    else
      debug_log(1, _LF_, "Checking cmon for server %s\n", ptr->server_index_ptr->server_ip);
  }

  //if((ptr->fd = nslb_tcp_client(ptr->server_index_ptr->server_ip, 7891)) < 0)
  //if((ptr->fd = nslb_tcp_client_ex(ptr->server_index_ptr->server_ip, 7891, 30, error_msg)) < 0) 
  if((ptr->fd = nslb_tcp_client_ex(ptr->server_index_ptr->server_ip, server_prt, 30, error_msg)) < 0) 
  {
    debug_log(1, _LF_, "Error: Error in making connection to server %s, Ignoring this server", ptr->server_index_ptr->server_ip);
    debug_log(1, _LF_, "Error: %s", error_msg);
    //if(!raw_format_for_gui)
    //  printf("%s\n", error_msg);
    return SERVER_ADMIN_FAILURE;
  }
  else
  {
    if(!raw_format_for_gui)
      debug_log(1, _LF_, "Connection established to server %s.", ptr->server_index_ptr->server_ip);
    else
      debug_log(0, _LF_, "Connection established to server %s.", ptr->server_index_ptr->server_ip);
  }

  if(is_session_started(ptr) == SERVER_ADMIN_FAILURE)
  {
    debug_log(0, _LF_, "Closing fd as session is not started for %s.", ptr->server_index_ptr->server_ip);
    close_session_and_fd(ptr); 
    return SERVER_ADMIN_FAILURE;
  }
  return SERVER_ADMIN_SUCCESS;
}

static int start_cmon_using_remote(ServerCptr *ptr)
{  
   char remote_cmd[MAX_LINE_LEN] = "\0";
   char line[MAX_BUF_SIZE + 1]= "\0";
   FILE *fp;

   if(ptr->server_index_ptr->is_ssh == 'N')
   {
     debug_log(1, _LF_, "ssh is not enabled for this server(%s), cmon can't be start", ptr->server_index_ptr->server_ip);
     return SERVER_ADMIN_FAILURE;
   }

   if(make_connections(ptr, 0) == SERVER_ADMIN_SUCCESS)
   {
     debug_log(1, _LF_, "cmon is already running on server %s", ptr->server_index_ptr->server_ip);
     return SERVER_ADMIN_SUCCESS;
   }
  
   // ./nsi_cmon_remote_start /opt/cavisson/monitors 192.168.18.101 root abeona
   sprintf(remote_cmd, "%s/bin/nsi_cmon_remote_start %s %s %s %s", g_ns_wdir, ptr->server_index_ptr->install_dir, ptr->server_index_ptr->server_ip, ptr->server_index_ptr->user_name, ptr->server_index_ptr->password); 

  fp = popen(remote_cmd, "r");
  if(fp == NULL)
  {
    perror("popen"); //ERROR: popen failed
    NS_EXIT(-1,"Failed to open %s/bin/nsi_cmon_remote_start %s %s %s %s", g_ns_wdir, ptr->server_index_ptr->install_dir, ptr->server_index_ptr->server_ip, ptr->server_index_ptr->user_name, ptr->server_index_ptr->password);
  }
  while(fgets(line, MAX_BUF_SIZE, fp)!= NULL )
  {
    fprintf(stdout, "%s", line);
  }

  if(make_connections(ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "cmon not started on server %s", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }
  else
    debug_log(1, _LF_, "cmon started on server %s", ptr->server_index_ptr->server_ip);

  return SERVER_ADMIN_SUCCESS;
}

static int stop_cmon(ServerCptr *ptr)
{ 
  char cmd[MAX_LINE_LEN] = "\0";

  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(cmd, "RUN_CMD:%s/bin/cmon:stop\n", encoded_install_dir);

  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "cmon is already stopped on server %s", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }

  run_cmd_and_read_op(cmd, ptr, 0);

  if(make_connections(ptr, 0) == SERVER_ADMIN_SUCCESS)
  {
    debug_log(1, _LF_, "Unable to stop cmon on server %s", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }
  else
    debug_log(1, _LF_, "cmon is stopped on server %s", ptr->server_index_ptr->server_ip);

  return SERVER_ADMIN_SUCCESS; 
}

static int check_port(ServerCptr *ptr, char *ip_port)
{
  char ip_port_msg[MAX_USERNAME_LEN] = {0};
  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(ip_port_msg, "CHECK_PORT:%s\n", ip_port);
  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Check unsuccessful for ip: %s", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }
  if(run_cmd_and_read_op(ip_port_msg, ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }
  return SERVER_ADMIN_SUCCESS;
  
}

static int show_java_cores(ServerCptr *ptr)
{ 
  char cmd[MAX_LINE_LEN] = "\0";

  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(cmd, "RUN_CMD:%s/bin/cmon:java_heap\n", ptr->server_index_ptr->install_dir);

  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Unable to show java heap for server %s", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }

  if(run_cmd_and_read_op(cmd, ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }

  /*if(raw_format_for_gui != 0)
  {
    if(ptr->cmd_output)
       fprintf(stdout, "%s", ptr->cmd_output);
  }*/

  return SERVER_ADMIN_SUCCESS;
}

static int get_cmon_info(ServerCptr *ptr)
{ 
  char cmd[MAX_LINE_LEN] = "\0";

  sprintf(cmd, "GET_CMON_INFO:\n");

  if(make_connections(ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Error: %s|Cmon info is not available. Error in making connection.", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }

  if(run_cmd_and_read_op(cmd, ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }

  if(output_format_flag == 1)
    printf("%s|%s", ptr->server_index_ptr->server_ip, ptr->cmd_output);

  return SERVER_ADMIN_SUCCESS;
}

static int show_cmon_version(ServerCptr *ptr)
{ 
  char cmd[MAX_LINE_LEN] = "\0";

  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(cmd, "RUN_CMD:%s/bin/cmon:version\n", encoded_install_dir);

  if(make_connections(ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Error: %s|Version is not available. Error in making connection.", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }

  if(run_cmd_and_read_op(cmd, ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }


  //if((raw_format_for_gui != 0) && (output_format_flag == 1))
  if(output_format_flag == 1)
    printf("%s|%s", ptr->server_index_ptr->server_ip, ptr->cmd_output);

  return SERVER_ADMIN_SUCCESS;
}

int run_users_command(ServerCptr *ptr, char *cmd_args)
{ 
  char cmd[MAX_LINE_LEN] = "\0";
  char cmd_list_file[MAX_LINE_LEN] = "/opt/cavisson/monitors/logs/commandlist.dat";

  sprintf(cmd, "RUN_CMD:%s\n", cmd_args);
  debug_log(0, _LF_, "Running command '%s' for server", cmd, ptr->server_index_ptr->server_ip);

  if(validate_command(cmd_args) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Command '%s' Blocked for server %s", cmd_args, ptr->server_index_ptr->server_ip);
    return BLOCKED;
  }
  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Unable to run command '%s' for server %s", cmd_args, ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }

  //Changed 3rd argument of run_cmd_and_read_op() from 1 -> 0, on 08/02/2013 3.9.0 SysBuild#14
  //In order to remove msg: Error in reading output from CavMonAgent, from nsi_take_java_thread_dump error case output.  
  if(run_cmd_and_read_op(cmd, ptr, 0) == SERVER_ADMIN_FAILURE) 
  {  
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }


  /*if(raw_format_for_gui != 0)
  {
    if(ptr->cmd_output)
      fprintf(stdout, "%s", ptr->cmd_output);
  }*/

  close_session_and_fd(ptr);
  return SERVER_ADMIN_SUCCESS;
}

static int show_monitor_running(ServerCptr *ptr)
{ 
  char cmd[MAX_LINE_LEN] = "\0";

  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(cmd, "RUN_CMD:%s/bin/cmon:show_monitors\n", encoded_install_dir);

  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
    return SERVER_ADMIN_FAILURE;
 
  if(run_cmd_and_read_op(cmd, ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }

  if(ptr->cmd_output)
  {
    /*if(raw_format_for_gui != 0)
    {
       fprintf(stdout, "%s", ptr->cmd_output);
    }
    debug_log_long_data(_LF_, ptr->cmd_output);*/
    if(strstr(ptr->cmd_output, "Following Monitors are running:"))
      return SERVER_ADMIN_CONDITIONAL_SUCCESS;
  }
 
  return SERVER_ADMIN_SUCCESS;
}

/* This function ftp file to server on the given path */
// Returns SERVER_ADMIN_SUCCESS or SERVER_ADMIN_FAILURE
int ftp_file(ServerCptr *ptr, char *file_name, char *path, int machine_type)
{ 
  char dest[MAX_LINE_LEN]= "\0";
  char cmd[MAX_LINE_LEN]= "\0";
  int ftp_file_fd = -1;
  int ftp_file_size = 0;
  int ret;
  int chmod_ret = 0;

  debug_log(0, _LF_, "FTPing file %s to server %s at destination directory %s", file_name, ptr->server_index_ptr->server_ip, path);

  ftp_file_size = get_file_size(file_name, &ftp_file_fd);

  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
    return SERVER_ADMIN_FAILURE;

  // if path is not given then we will copy to installation dir 
  if(path[0] == '\0')
    ret = start_ftp_file(ptr, file_name, ftp_file_size, ftp_file_fd, encoded_install_dir);
  else
    ret = start_ftp_file(ptr, file_name, ftp_file_size, ftp_file_fd, path);


  if(ret == SERVER_ADMIN_SUCCESS)
  {
    debug_log(0, _LF_, "File FTPed successfully");

    if(!machine_type)  //chmod only for non window machine
    {
      if(path[0] == '\0')
        sprintf(cmd, "RUN_CMD:chmod: +x %s/%s\n", encoded_install_dir, basename(file_name));
      else
        sprintf(cmd, "RUN_CMD:chmod: +x %s/%s\n", path, basename(file_name));

      chmod_ret = run_cmd_and_read_op(cmd, ptr, 0); //changing mode of file uploaded

      if(chmod_ret == SERVER_ADMIN_SUCCESS)
        debug_log(0, _LF_, "Permission changed successfully");
      else
        debug_log(1, _LF_, "Error: Error in changing permission");
    }
  }
 
  close_session_and_fd(ptr);
  //if(ret == SERVER_ADMIN_SUCCESS)
    //debug_log(1, _LF_, "File FTPed successfully");
  return(ret);
}

#define MAX_FILE_DOWNLOAD_SIZE 64*1024
#define FILE_FOUND_LENGTH_WITH_COLON 11
#define FILE_FOUND_STRING "File_Found:"
#define SECURE_CMON_MSG "Error: Secure Cmon"

static int read_file_data_from_cs(ServerCptr *ptr, int replace_flag, char *download_location, char *file_name)
{
  debug_log(0, _LF_, "Method Called.");

  char file_data[MAX_FILE_DOWNLOAD_SIZE + 1] = {0};
  char mode[2 + 1] = "w+";
  static char file_found_flag = 0;
  int bytes_read = 0;
  int bytes_write = 0;
  int length = 0;
  int error_flag = 0;
  unsigned long download_file_size = 0;
  char *ptr_to_colon = NULL;
  FILE *download_fp = NULL;

  //Here we are downloading file data from server accoring to bytes(file size) provided by server.
  //We are not considering msgs like: CMD_EXECUTED_SUCCESSFULY / CMD_EXECUTION_ERROR
  //When we read all the bytes (file size) then return SERVER_ADMIN_SUCCESS 

  /* Code to read data from server starts*/
  while(1)
  {
    if((bytes_read = read (ptr->fd, file_data + bytes_write, MAX_FILE_DOWNLOAD_SIZE)) < 0)
    {
      if((errno == EAGAIN) || (errno == EINTR))
      {
        //partial
        continue;
      }
      else
      {
        fprintf(stderr, "Error in reading message. Error = '%s'\n", nslb_strerror(errno)); 
        return SERVER_ADMIN_FAILURE;  
      }
    }
   
    //printf("\n bytes_read = %d, bytes_write, = %d, file_data = %s, file_found_flag = %d \n", 
      //         bytes_read, bytes_write, file_data, file_found_flag);

   if(debug_on)
      debug_log(0, _LF_, "bytes_read = %d, file_data = %s, file_found_flag = %d", bytes_read, file_data, file_found_flag);

    if(bytes_read == 0)
    {
      debug_log(1, _LF_, "Connection closed from other side.");
  
      return SERVER_ADMIN_FAILURE;
    }

    //file_data[bytes_read] = '\0';
    
    if((file_found_flag == 0))
    {
      if (write_in_file_flag)
      {
        download_fp = fopen(download_location, "a+");
        if (!download_fp)
        {
          fprintf(stderr, "Error in creating ftp file '%s'. Error = '%s'\n", download_location, nslb_strerror(errno));
          return SERVER_ADMIN_FAILURE;
        }
        file_found_flag = 1;
        goto jmp;
      }
      ptr_to_colon = strchr(file_data, '\n');
      //didn't got /n loop in read
      if (!ptr_to_colon)
        continue;
      length = ptr_to_colon - file_data;

      if(debug_on)
       debug_log(0, _LF_, "length = %d, file_data = %s, ptr_to_colon = %s, download_file_size = %d", length, file_data, ptr_to_colon, download_file_size);
      
      //we already got the whole line so just get size of file
      if(ptr_to_colon = strstr(file_data, FILE_FOUND_STRING)) //FILE FOUND
      {
        ptr_to_colon = ptr_to_colon + FILE_FOUND_LENGTH_WITH_COLON;
        download_file_size = atol(ptr_to_colon); //get file size
        if(debug_on)
          debug_log(0, _LF_, "file_data = %s, ptr_to_colon = %s, download_file_size = %d", file_data, ptr_to_colon, download_file_size);

        if(replace_flag) //truncate
          strcpy(mode, "w+");
        else //append
          strcpy(mode, "a+");

        if(debug_on)
          debug_log(0, _LF_, "mode = %s", mode);

        download_fp = fopen(download_location, mode); 
        if (!download_fp)
        {
          fprintf(stderr, "Error in creating ftp file '%s'. Error = '%s'\n", download_location, nslb_strerror(errno));
          return SERVER_ADMIN_FAILURE;
        }

        file_found_flag = 1;
        if(bytes_read - (length + 1) <= 0)
          continue;
        memmove(file_data, file_data + length + 1, bytes_read - length);
        file_data[bytes_read - length] = '\0';
        bytes_write = 0;

        if(debug_on)
          debug_log(0, _LF_, "file_data = %s, ptr_to_colon = %s, download_file_size = %d, file_found_flag = %d, streln(file_data) = %d", file_data, ptr_to_colon, download_file_size, file_found_flag, strlen(file_data));
      }
      else if(strstr(file_data, SECURE_CMON_MSG) != NULL)//Secure cmon error      
      {
        error_flag = 1;
        ptr_to_colon = strchr(file_data, '\n');
        if(ptr_to_colon)
          *ptr_to_colon = '\0'; 
      }
      else
      {
        bytes_write = bytes_write + bytes_read;
        continue;
      }
    }  

   /*if((file_found_flag == 0) && (strlen(file_data) == 0))
   {
     if(debug_on)
       debug_log(0, _LF_, "file_data is NULL and file_found_flag is %d. hence continue.....", file_found_flag);

     file_found_flag = 1;
     continue;
   }*/

jmp: if(debug_on)
       debug_log(0, _LF_, "before writing: download_file_size = %d, bytes_read = %d, file_data = %s, file_found_flag = %d", download_file_size, bytes_read, file_data, file_found_flag);
    if((download_file_size > 0) || write_in_file_flag)
    {
      if(!write_in_file_flag)
      {
        if(download_file_size < bytes_read)
          bytes_read = download_file_size;
      }
      else
      {
        if(strstr(&(file_data)[bytes_read - 30], CMD_EXECUTED_SUCCESSFULY) != NULL)
        {
          bytes_read = bytes_read - 30;
          if(fwrite(file_data, sizeof(char), bytes_read, download_fp) != bytes_read) //TODO: write
          {
            fprintf(stderr, "Error: Can not write to ftp file '%s'\n", download_location);
            return SERVER_ADMIN_FAILURE;
          }
          if (download_fp)
            fclose(download_fp);
 
          debug_log(1, _LF_, "File %s downloaded successfully.", file_name);
          return SERVER_ADMIN_SUCCESS;
        }
      } 
      debug_log(0, _LF_, "download_file_size = %d, file_data = %s, bytes_read = %d", download_file_size, file_data, bytes_read);

      if(fwrite(file_data, sizeof(char), bytes_read, download_fp) != bytes_read) //TODO: write
      {
        fprintf(stderr, "Error: Can not write to ftp file '%s'\n", download_location);
        return SERVER_ADMIN_FAILURE;
      }

      fflush(download_fp);
      if(!write_in_file_flag)
        download_file_size -= bytes_read;

      //if remaining size is 0 then close file ptr
      //if(download_file_size == 0) 
      //{
        //fclose(download_fp);
       //if(debug_on) 
         //debug_log(1, _LF_, "File %s downloaded successfully.", file_name);
        //return SERVER_ADMIN_SUCCESS; 
      //}
    }  

    if(error_flag)
    {
      debug_log(1, _LF_, "File %s not downloaded.\n%s", file_name, file_data);     

      return SERVER_ADMIN_SECURE_CMON;
    }

    if(debug_on)
      debug_log(0, _LF_, "download_file_size = %d, file_data = %s", download_file_size, file_data);
    
    if((download_file_size == 0) && (!write_in_file_flag))
    {
      if (download_fp)
        fclose(download_fp);

      debug_log(1, _LF_, "File %s downloaded successfully.", file_name);

      return SERVER_ADMIN_SUCCESS; 
    }
  }
  /* Code to read data from server ends*/
}

static char *make_file_path(char *pathname, char *file_name)
{
  struct stat info;
  static char cmd[MAX_LINE_LEN];

  debug_log(0, _LF_, "pathname = %s, file_name = %s", pathname, file_name);

  if(stat( pathname, &info ) != 0 )
  {
    debug_log(0, _LF_, "Cannot access %s. Hence downloading %s in currect directory.", pathname, file_name);
    sprintf(cmd, "%s", basename(file_name));
  }
  else if( info.st_mode & S_IFDIR ) //dir
  {
    debug_log(0, _LF_, "This is a dir.");
    sprintf(cmd, "%s/%s", pathname, basename(file_name));
  }
  else //not dir
  {
    debug_log(0, _LF_, "This is a not dir.");
    sprintf(cmd, "%s", pathname);
  }

  debug_log(0, _LF_, "cmd = %s", cmd);
  return(cmd);
}

static int download_file(ServerCptr *ptr, char *file_name, int machine_type, int replace_flag, char *download_location, int download_location_flag)
{ 
  char cmd[MAX_LINE_LEN]= "\0";
  char *file_location = NULL;
  int ret = 0;

  debug_log(1, _LF_, "Downloading file %s from server %s to location %s.", file_name, ptr->server_index_ptr->server_ip, download_location);

  //make download file destination path, "file_location" will be location of downloaded file
  file_location = make_file_path(download_location, file_name);
  debug_log(0, _LF_, "file_location = %s", file_location);

  //If replace flag is disabled and file exists  
  if((!replace_flag) && (access(file_location, F_OK) != -1))
  {
    NS_EXIT (-1, "File %s already exists.\n", file_location);
  }

  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
    return SERVER_ADMIN_FAILURE;

  sprintf(cmd, "DownloadFile:%s\n", file_name);

  if (send_msg_to_createserver(ptr, cmd) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "Sending file download msg failed");
    return SERVER_ADMIN_FAILURE;
  }

  //if(read_file_data_from_cs(ptr, replace_flag, file_location, file_name) == SERVER_ADMIN_FAILURE)
  ret = read_file_data_from_cs(ptr, replace_flag, file_location, file_name);
  if(ret == SERVER_ADMIN_FAILURE)
  {
    fprintf(stderr, "Error in reading output from CavMonAgent.\n");
    return SERVER_ADMIN_FAILURE;
  }
  
  return ret;
  //return SERVER_ADMIN_SUCCESS;
}

static int restart_windows_cmon_and_verify(ServerCptr *ptr, int make_con) 
{ 
  char cmd[MAX_LINE_LEN] = "\0";

  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(cmd, "RUN_CMD:%s/bin/cmon:restart\n", encoded_install_dir);

  debug_log(0, _LF_, "Running command '%s' for server", cmd, ptr->server_index_ptr->server_ip);

  if(make_con) 
  {
    if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
      return SERVER_ADMIN_FAILURE;
  }

  if(run_cmd_and_read_op(cmd, ptr, 0) == SERVER_ADMIN_FAILURE)
    close_session_and_fd(ptr);

  // Windows needs it
  sleep(25);

  if(make_connections(ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "cmon not restarted for server(%s)", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }
  else
    debug_log(1, _LF_, "cmon restarted for server(%s)", ptr->server_index_ptr->server_ip);

  return SERVER_ADMIN_SUCCESS;
}
//make_con = 1 ; connection in not there make a conn
//make_con = 0 ; connection is already there do not make a conn
static int restart_cmon_and_verify(ServerCptr *ptr, int make_con, char *upgrade_shell) 
{ 
  char dest[MAX_LINE_LEN]= "\0";
  char cmd[MAX_LINE_LEN] = "\0";  

  if(make_con) 
  {
    if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
      return SERVER_ADMIN_FAILURE;
  }

  /*Doing FTP of shell nsu_cmon_upgrade because command (cmon restart) is executed from nsu_cmon_upgrade.*/
  sprintf(dest, "%s/bin", encoded_install_dir);
  start_ftp_file(ptr, upgrade_shell, upgrade_shell_size, upgrade_shell_fd, dest);

  sprintf(cmd, "RUN_CMD:chmod: +x %s/%s\n", dest, upgrade_shell_without_path);
  run_cmd_and_read_op(cmd, ptr, 0); //changing mode of shell
  

  /* Earlier we were passing "ptr->server_index_ptr->server_ip" instead of "get_server_name_without_port(ptr->server_index_ptr->server_ip)" as fourth argument.
   * Issue was: when server_ip contains both ip:port, it creates problem in CavMonAgent.
   * Because here "Colon" after cmon is for CavMonAgent to indicate that these are arguments.
   * Example:
   * when we pass cmd: 
   * RUN_CMD:/home/cavisson/cavisson/monitors/bin/nsu_cmon_upgrade:-u /home/cavisson/cavisson/monitors -s LINUX116_PORT_7898_DIFF_UID:7898 -r
   * Above cmd gets changed to:
   * Running command - /home/cavisson/cavisson/monitors/bin/nsu_cmon_upgrade -u /home/cavisson/cavisson/monitors -s LINUX116_PORT_7898_DIFF_UID  
   * i.e. it removes command after :7898 which was the issue.
   * Also, we are passing -s option here just to do echo $SERVER_NAME in shell nsu_cmon_upgrade.*/

  sprintf(cmd, "RUN_CMD:%s/bin/%s:-u %s -s %s -r\n", encoded_install_dir,
                upgrade_shell_without_path, encoded_install_dir, get_server_name_without_port(ptr->server_index_ptr->server_ip));

  debug_log(0, _LF_, "Running command '%s' for server", cmd, ptr->server_index_ptr->server_ip);


  if(run_cmd_and_read_op(cmd, ptr, 0) == SERVER_ADMIN_FAILURE)
    close_session_and_fd(ptr);

  sleep(5);

  if(make_connections(ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "cmon not restarted for server(%s)", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_FAILURE;
  }
  else
    debug_log(1, _LF_, "cmon restarted for server(%s)", ptr->server_index_ptr->server_ip);

  return SERVER_ADMIN_SUCCESS;
}

static int upgrade_windows_server(ServerCptr *ptr, char *package_name, char *upgrade_shell)
{ 
  char dest[MAX_LINE_LEN]= "\0";
  char cmd[MAX_LINE_LEN]= "\0";

  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
    return SERVER_ADMIN_FAILURE;

  start_ftp_file(ptr, package_name, package_size, package_fd, encoded_install_dir);

  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(cmd, "RUN_CMD:%s/bin/cmon:upgrade -p %s\n", encoded_install_dir, basename(package_name));

  if(run_cmd_and_read_op(cmd, ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }

  /*if(raw_format_for_gui != 0)
  {
    if(ptr->cmd_output)
      fprintf(stdout, "%s", ptr->cmd_output);
  }*/
  close_session_and_fd(ptr);
  return SERVER_ADMIN_SUCCESS;
}


static int upgrade_server(ServerCptr *ptr, char *package_name, char *upgrade_shell)
{
  char dest[MAX_LINE_LEN]= "\0";
  char cmd[MAX_LINE_LEN]= "\0";

  
  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
     return 1;

  start_ftp_file(ptr, package_name, package_size, package_fd, encoded_install_dir);
  sprintf(dest, "%s/bin", encoded_install_dir); 

  start_ftp_file(ptr, upgrade_shell, upgrade_shell_size, upgrade_shell_fd, dest);
 
  sprintf(cmd, "RUN_CMD:chmod: +x %s/%s\n", dest, upgrade_shell_without_path);
  run_cmd_and_read_op(cmd, ptr, 0); //changing mode of shell

  //For comments refer to function restart_cmon_and_verify()
  sprintf(cmd, "RUN_CMD:%s/%s:-u %s -p %s -s %s\n", dest, upgrade_shell_without_path, encoded_install_dir, package_name_without_path, get_server_name_without_port(ptr->server_index_ptr->server_ip));

  if(run_cmd_and_read_op(cmd, ptr, 0) == SERVER_ADMIN_FAILURE)
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }

  /*if(raw_format_for_gui != 0)
  {
    if(ptr->cmd_output)
      fprintf(stdout, "%s", ptr->cmd_output);
  }*/

   if(restart_cmon_and_verify(ptr, 0, upgrade_shell) == SERVER_ADMIN_FAILURE)
     return SERVER_ADMIN_FAILURE;
   close_session_and_fd(ptr);
  return SERVER_ADMIN_SUCCESS;
}

static int show_cmon_running(ServerCptr *ptr)
{ 
  char cmd[MAX_LINE_LEN] = "\0";

  if(make_connections(ptr, 1) == SERVER_ADMIN_FAILURE)
  {
    debug_log(1, _LF_, "%s|cmon not running.", ptr->server_index_ptr->server_ip);
    return SERVER_ADMIN_SUCCESS; //make_connections fails still returning success here becoz here we are showing cmon is running or not: 
                    // (1) if cmon not running -> then make_connection() will fail but the command status will be pass
                    // (2) if cmon is running -> then make_connection() will pass and the command status will be pass
  }

  //sprintf(cmd, "RUN_CMD:%s/bin/cmon:show\n", ptr->server_index_ptr->install_dir);
  // Note: Colon after cmon is for CavMonAgent to indicate that these are aguments
  sprintf(cmd, "RUN_CMD:%s/bin/cmon:show\n", encoded_install_dir);

  if(run_cmd_and_read_op(cmd, ptr, 0) == SERVER_ADMIN_FAILURE) 
  {
    close_session_and_fd(ptr);
    return SERVER_ADMIN_FAILURE;
  }

  if(ptr->cmd_output)
  {
    //if(raw_format_for_gui == 0)
    //  debug_log_long_data(_LF_, ptr->cmd_output);
    //else
    //if((raw_format_for_gui != 0) && (output_format_flag == 1))
    //if(output_format_flag == 1)
    if((raw_format_for_gui != 0) && (output_format_flag == 1))
    {
       char *temp = index(ptr->cmd_output, '\n');
      if(temp != NULL)
        *temp = 0;

       printf("%s|%s\n", ptr->server_index_ptr->server_ip, ptr->cmd_output);
    }
  }
  return SERVER_ADMIN_SUCCESS;
}

static void generate_hashkey(long long timestamp, char *generated_key)
{
  char buff[TIME_STAMP_LEN+1] = {0};
  unsigned char digest[MD5_DIGEST_LENGTH];
  char string[GEN_KEY_LEN+1] = "17w5r20y";
  int i;
  MD5_CTX mdContext;

  if(generated_key[0] != '\0')
    memset(generated_key, '\0', MAX_HASHKEY_LEN);

  sprintf(buff, "%lld", timestamp); 
  strncat(string, buff, TIME_STAMP_LEN);
  MD5_Init (&mdContext);
  MD5_Update(&mdContext, string, strlen(string));
  MD5_Final (digest,&mdContext);
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
  {
    sprintf(generated_key+(i*2),"%02x", digest[i]);
  }
}

static int compare_hash_key(char *hash_key)
{
    int i;
    long    tloc;
    struct  tm *lt;
    int ret_val;
    long long timestamp;
    char buff[TIME_BUFF_LEN] = {0};
    char generated_key[MAX_HASHKEY_LEN] = {0};

    /*  Getting current time  */
    (void)time(&tloc);
    lt = localtime(&tloc);
    
    if (lt == (struct tm *)NULL)
      ret_val = -1;
    else
    {
      /*  Getting time as formatted string  */
      /*  "%Y%m%d%H%M" returns "YYYYMMDDHHMM" format  */
      if(strftime(buff, TIME_BUFF_LEN, "%Y%m%d%H%M", lt) == 0)
        ret_val = -1;
      else
      {
        timestamp=atoll(buff);
        ret_val = 0;
      }
    }

    generate_hashkey(timestamp,generated_key); 
    if(strncmp(generated_key,hash_key,MD5_DIGEST_LENGTH))
    {
      timestamp=timestamp-1; 
      
      generate_hashkey(timestamp,generated_key);
      
      if(strncmp(generated_key,hash_key,MD5_DIGEST_LENGTH))
      {
        NS_EXIT(0, "Hash key not matched exiting the program");
      }
    }
    return(ret_val);
}

#ifdef TEST
int main(int argc, char *argv[])
{
  int index, ret;
  char error_string[MAX_LINE_LEN] = "\0";
  char cmd[MAX_LINE_LEN] = "\0";
  char package_name[MAX_LINE_LEN] = "\0";
  char ftp_file_name[MAX_LINE_LEN] = "\0";
  char download_file_name[MAX_LINE_LEN] = "\0";
  char download_location[MAX_LINE_LEN] = "\0";
  char dest_file_path[MAX_LINE_LEN] = "\0";
  char cmd_args[MAX_LINE_LEN] = "\0";
  char upgrade_shell[MAX_LINE_LEN] = "\0";
  char debug_log_file[64]="\0";
  ServerCptr cmon_ptr;
  char **servers_given;   //max 20 servers can be upgrade at a time
  char dest[MAX_LINE_LEN]= "\0";
  char arg;
  char strt_msg[MAX_LINE_LEN]="\0";
  char topology_name[MAX_LINE_LEN]="\0";
  char vector_separator = '_';
  int ftp_flag = 0;
  int download_flag = 0;
  int download_location_flag = 0;
  int dest_flag = 0;
  int cp_flag = 0;
  int machine_flag = 0; // Default Linux
  int server_index = -1;
  // Windows 1 linux else
  //unsigned short server_prt = 0;
    //server_prt = 7891;
  int tier_idx=-1;
  if(argc < 2) 
   usage();

  struct option longopts[] = {
                              {"a", 0, NULL, 'a'},         //all
                              {"upgrade", 1, NULL, 'p'},   //package name
                              {"s", 1, NULL, 's'},         //servers
                              {"g", 0, NULL, 'g'},         //gui
                              {"show_java_core", 0, NULL, 'j'},         //java heap
                              {"upload",  1, NULL, 'F'},       //FTP ONLY
                              {"download",  1, NULL, 'G'},       //download ONLY
                              {"file",  1, NULL, 'f'},       //download destination
                              {"P",  1, NULL, 'P'},       //port
                              {"ignore_server_entry",  0, NULL, 'i'},       //server not existing in server.dat
                              {"dest", 1, NULL, 'D'},       //FTP destination
                              {"debug", 0, NULL, 'd'},     //debug
                              {"run-cmd", 1, NULL, 'c'},   //command
                              {"monitors", 0, NULL, 'm'},  //monitors
                              {"restart", 0, NULL, 'r'},   //restart
                              {"show", 0, NULL, 'S'},      //show
                              {"start", 0, NULL, 'x'},     //start
                              {"stop", 0, NULL, 'k'},      //stop
                              {"version", 0, NULL, 'v'},   //version
                              {"replace", 0, NULL, 'R'},       //REPLACE
                              {"origin_cmon", 1, NULL, 'o'},   //ORIGIN CMON
                              {"topology_name", 1, NULL, 'o'}, //TOPOLOGY NAME
                              {"cmon_info", 0, NULL, 'I'}, //CMON INFO
                              {"check_port",1,NULL,'C'}, //IP:Port 
                              {"key",1,NULL,'K'}, //HASHKEY 
                              {0, 0, 0, 0}
                             };


  while((arg = getopt_long(argc, argv, "ap:s:c:F:G:f:P:D:djmrSxkvgiRu:o:t:IC:K:w:", longopts, NULL)) != -1)
  {
    switch(arg)
     {
      case 'a':
       a_flag++;
       if(s_flag)
       {
          fprintf(stderr, "-a & -s option can't be use together.\n");
          usage();
       }
       break;

      case 'p':
       check_user("upgrade");
       //fprintf(stderr, "Upgrading Servers ...\n");
       sprintf(strt_msg, "Upgrading Servers ...\n");
       p_flag = 1;
       strcpy(package_name, optarg);
       replace_strings1(package_name, ":", "%3A");
       break;

      case 'g':
       raw_format_for_gui = 1;
       break;

      case 'F':
       ftp_flag = 1;
       strcpy(ftp_file_name, optarg);
       replace_strings1(ftp_file_name, ":", "%3A");
       break;

     case 'R':
       //sprintf(strt_msg, "Replace file ...\n");
       R_flag++;
       break;

      case 'f':
       download_location_flag = 1;
       strcpy(download_location, optarg);
       replace_strings1(download_location, ":", "%3A");
       break;

      case 'G':
       download_flag = 1;
       strcpy(download_file_name, optarg);
       replace_strings1(download_file_name, ":", "%3A");
       break;

      case 'D':
       dest_flag = 1;
       strcpy(dest_file_path, optarg);
       replace_strings1(dest_file_path, ":", "%3A");
       break;

     case 's':
       if(a_flag >1)   //>1 means a_flag is set by -a opt, b'cos it has initialised with 1
       {
          fprintf(stderr, "-a & -s option can't be use together.\n");
          usage();
       }
       a_flag = 0;
       if(s_flag == 0)
         MY_MALLOC(servers_given, (sizeof(char *) * 50), "server_given", -1);   //asuming 50 server can be given with -s 
       MY_MALLOC(servers_given[s_flag], strlen(optarg) + 1, "Server", -1);
       strcpy(servers_given[s_flag], optarg);
       s_flag++;
       break;
  
     case 'P':
       server_prt = atoi(optarg); 
       break;
 
     case 'i':
       ignore_server_flag = 1;
       break;
      
     case 'c':
       //fprintf(stderr, "Running command '%s'...\n", optarg);
       sprintf(strt_msg, "Running command '%s'...\n", optarg);
       strcpy(cmd_args, optarg);
       replace_strings1(cmd_args, ":", "%3A");

         int i = 0;
         int len = strlen(cmd_args);
         while(cmd_args[i] != ' ' && i< len)
           i++;
         if(i< len)
           cmd_args[i]=':';
       //printf("cmd_args = %s\n", cmd_args);
       c_flag++;
       break;

     case 'j':
       //fprintf(stderr, "Showing java heap dumps ...\n");
       sprintf(strt_msg, "Showing java heap dumps ...\n");
       j_flag++;
       break;

     case 'd':
       debug_on = 1;
       break;
     
     case 'm':
       //fprintf(stderr, "Showing all running monitors ...\n");
       sprintf(strt_msg, "Showing all running monitors ...\n");
       m_flag++;
       break;

     case 'r':
       check_user("restart");
       //fprintf(stderr, "Restarting cmon ...\n");
       sprintf(strt_msg, "Restarting cmon ...\n");
       r_flag++;
       break;

     case 'S':
       //fprintf(stderr, "Showing cmon ...\n");
       sprintf(strt_msg, "Showing cmon ...\n");
       S_flag++;
       break;

     case 'x':
       check_user("start");
       //fprintf(stderr, "Starting cmon ...\n");
       sprintf(strt_msg, "Starting cmon ...\n");
       x_flag++;
       break;

     case 'k':
       check_user("stop");
       //fprintf(stderr, "Stopping cmon ...\n");
       sprintf(strt_msg, "Stopping cmon ...\n");
       k_flag++;
       break;

     case 'v':
       //sprintf(cmd_args, "%s -v ", cmd_args);
       //fprintf(stderr, "Getting cmon versions ...\n");
       sprintf(strt_msg, "Getting cmon versions ...\n");
       v_flag++;
       break;
    case 'u':
        strcpy(username, optarg);
        u_flag = 1;
        break;
    case 'o':
        strcpy(origin_cmon, optarg); 
        o_flag = 1;
        break;
    case 't':
        strcpy(topology_name, optarg);
        t_flag = 1;
        break;
    case 'I':
      I_flag = 1;
      break;
    case 'C':
      strcpy(ip_port, optarg);
      int next,val;
      char port[50]="\0";
      char timeout[50]={0};
      char *pos,*timeout_pos;
      pos=strchr(ip_port,':');
       if(pos != NULL)
       {
          next=pos-ip_port+1;
          if(ip_port[next] == '\0')
          {
            fprintf(stderr, "No port present. Port number is mandatory\n");
            usage();
            break;
          }
          pos++;
          timeout_pos=strchr(pos,':');
          if(timeout_pos)
          {
            next=timeout_pos-pos;
            pos[next]='\0';
            strcpy(port,pos);
            pos[next]=':';
            strcpy(timeout,&pos[next+1]);
            if(!ns_is_numeric(timeout))
            {
              fprintf(stderr, "Timeout is not numeric\n");
              usage();
              break;
            }
          }
          else
          {
            strcpy(port,pos);
          }
          if(!ns_is_numeric(port))
          {
            fprintf(stderr, "Port is not numeric\n");
            usage();
            break;
          }
          val = atoi(port);
          if(val<1 || val>65535)
          {
            fprintf(stderr, "Port should be within 1 to 65535\n");
            usage();
	    break;
          } 
          else
          {
            cp_flag = 1;  
            break;
          } 
        }
        else
        {
          fprintf(stderr, "No port present. Port is mandatory\n");
          usage();
          break;   
        }
      
      break;   
    case 'K':
      strcpy(hash_key, optarg);
      K_flag=1;
      break;
    case 'w':
      strcpy(download_file_location, optarg);
      write_in_file_flag=1;
      break;
    case '?':
    default:
      usage();
    }
  } 

  //Read either topology or server.dat depending upon 't_flag'
  if(t_flag)
  {
    read_topology(topology_name, vector_separator, error_string, -1);
    if(error_string[0] != '\0') 
    {
      NS_EXIT(-1, "Error : %s", error_string);
    }
  }

  if((I_flag || v_flag || (S_flag && !machine_flag) || (r_flag && !machine_flag)) && (raw_format_for_gui != 0))
    output_format_flag = 1;

  memset(&cmon_ptr, 0, sizeof(cmon_ptr));
 
  strcpy(g_ns_wdir, get_ns_wdir());

  if(!u_flag)
  {
    struct passwd *pw;
    pw = getpwuid(getuid());
    if (pw == NULL)
      strncpy(username, g_ns_wdir, MAX_USERNAME_LEN);     //if getpwuid() fails
    else
      strncpy(username, pw->pw_name, MAX_USERNAME_LEN);
  }

  sprintf(debug_log_file, "%s/server/servers.log", g_ns_wdir);
  sprintf(upgrade_shell, "%s/bin/nsu_cmon_upgrade", g_ns_wdir);

  if(debug_on)
  {
    debug_fp = open_file(debug_log_file, "a+");
    fwrite(STAR, strlen(STAR), 1, debug_fp);
  }

  if (ignore_server_flag) {
    int z;
    for(z=0 ; z<s_flag; z++)
    topolib_fill_server_info_in_ignorecase(servers_given[z], "Cavisson",topo_idx);
  }

  //Set ServerInfo struct ptr
  //servers_list = (ServerInfo *) topolib_get_server_info();
  //Total no. of servers in ServerInfo structure
  //total_no_of_servers = topolib_get_total_no_of_servers();

  if(total_no_of_servers == 0)
  {
    NS_EXIT (-1, "No server is found in the topology .");
  }

#if 0
  int k;
  for(k=0 ; k<total_no_of_servers; k++)
  {
    printf("XXX=%s\n", servers_list->server_ip);
    servers_list++; 

  }
  return;
#endif

  // if -a is given copy all server to servers_given
  if(a_flag)
  {
    s_flag = total_no_of_servers;
    MY_MALLOC(servers_given, (sizeof(char *) * s_flag), "server_given", -1);
    for(index = 0; index < topo_info[topo_idx].total_tier; index++)
    {
      for(int server=0;server<topo_info[topo_idx].topo_tier_info[index].total_server;server++)
      {
      MY_MALLOC(servers_given[index], strlen(topo_info[topo_idx].topo_tier_info[index].topo_server[server].topo_servers_list->server_ip) + 1, "server", -1);
      strcpy(servers_given[index], topo_info[topo_idx].topo_tier_info[index].topo_server[server].topo_servers_list->server_ip);
    }
   }
  

  if(!raw_format_for_gui)
   fprintf(stderr, "%s", strt_msg);

  if(p_flag)
  {
    if(strstr(package_name, ".tar"))
    { 
       char *temp;
       if((temp = strstr(package_name, ".gz")) != NULL)
       {
         char unzip_cmd[512]="\0";
         sprintf(unzip_cmd, "if [ -f %s ];then gunzip %s;else echo %s not exist; exit 1; fi", package_name, package_name, package_name);
         if(system(unzip_cmd))
         {
           NS_EXIT(-1, "Unable to unzip %s.\n", package_name);
         }
         *temp=0;
       }
    } else if(strstr(package_name, ".zip")) {
      // Do nothing
    } else {
      fprintf(stderr, "Error: Invalid package.\n");
      NS_EXIT(-1, "Error: Invalid package.");
    }
    

    package_size = get_file_size(package_name, &package_fd);
    upgrade_shell_size = get_file_size(upgrade_shell, &upgrade_shell_fd);
    debug_log(0, _LF_, "package_size = %lu, upgrade_shell_size = %lu", package_size, upgrade_shell_size);

    strcpy(package_name_without_path, basename(package_name));
  }
  
  strcpy(upgrade_shell_without_path, basename(upgrade_shell));

  int ret_value = 0; // Set to success
  
  for(index = 0; index < s_flag; index++)
  { 
    if(!raw_format_for_gui)
      fprintf(stdout, "\n");
    ret = find_tier_idx_and_server_idx(servers_given[index], &cmon_ptr, NULL, NULL, 0, 0, &server_index,&tier_idx);
    if(ret == -1) 
    {
      if (ignore_server_flag == 0) //with server entry in server.dat 
      {
        ret_value = -1;
        //continueing either server not found or server is agent less.
        debug_log(!raw_format_for_gui, _LF_, "Server(%s) not found. Ignored.\n", servers_given[index]);
        if(raw_format_for_gui)
          printf("%s|Server not found.\n", servers_given[index]);
        continue;
      }
      else //without server entry in server.dat
      {
        ServerCptr *ptr = &cmon_ptr;
        ptr->server_index_ptr = &topo_info[topo_idx].topo_tier+info[index].topo_server[server];
      }
    }
    else if(ret == 1 && !x_flag)
    {
      debug_log(!raw_format_for_gui, _LF_, "Server(%s) is agentless. Ignored.\n", servers_given[index]);
      if(raw_format_for_gui)
        printf("%s|Server is agentless.\n", servers_given[index]);
      continue;
    }

    if(!strcmp(cmon_ptr.server_index_ptr->machine_type, "Windows")) {
       strcpy(encoded_install_dir, cmon_ptr.server_index_ptr->install_dir);
       replace_strings1(encoded_install_dir, ":", "%3A");
       machine_flag = 1;
    } else {
       strcpy(encoded_install_dir, cmon_ptr.server_index_ptr->install_dir);
       machine_flag = 0;
    }

    if(p_flag) {
      if(machine_flag) // Windows
         ret = upgrade_windows_server(&cmon_ptr, package_name, upgrade_shell); 
      else
         ret = upgrade_server(&cmon_ptr, package_name, upgrade_shell); 
      audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Server Upgrade", machine_flag, raw_format_for_gui, NULL, NULL);
    } else if(r_flag) {
      upgrade_shell_size = get_file_size(upgrade_shell, &upgrade_shell_fd);

      if(machine_flag)  { // Windows
         ret = restart_windows_cmon_and_verify(&cmon_ptr, 1); 
	       fprintf(stderr, "For windows function to show currect running cmon detail is not available, Please do show cmon manually !\n");
      }
      else {
         ret = restart_cmon_and_verify(&cmon_ptr, 1, upgrade_shell);
         show_cmon_running(&cmon_ptr);
      }
      audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Restart CMON", machine_flag, raw_format_for_gui, NULL, NULL);
    } else if(S_flag) {
      if(machine_flag){
         ret = SERVER_ADMIN_FAILURE;
	       fprintf(stderr, "For windows this function is not available, Please do it manually !\n");
      }
      else 
        ret = show_cmon_running(&cmon_ptr);
      audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Show CMON", machine_flag, raw_format_for_gui, NULL, NULL);
    } else if(x_flag)
        audit_log(start_cmon_using_remote(&cmon_ptr), username, (&cmon_ptr)->server_index_ptr->server_ip, "Start CMON", machine_flag, raw_format_for_gui, NULL, NULL);
    else if(k_flag)
      audit_log(stop_cmon(&cmon_ptr), username, (&cmon_ptr)->server_index_ptr->server_ip, "Stop CMON", machine_flag, raw_format_for_gui, NULL, NULL);
    else if(j_flag) {
      if(machine_flag){
        ret = SERVER_ADMIN_FAILURE;
	      fprintf(stderr, "For windows this function is not available, Please do it manually !\n");
      }
      else
        ret = show_java_cores(&cmon_ptr); 
      audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Show Java Cores", machine_flag, raw_format_for_gui, NULL, NULL);
    } else if(v_flag) {
      audit_log(show_cmon_version(&cmon_ptr), username, (&cmon_ptr)->server_index_ptr->server_ip, "Show CMON Version", machine_flag, raw_format_for_gui, NULL, NULL);
    } else if(I_flag) {
      audit_log(get_cmon_info(&cmon_ptr), username, (&cmon_ptr)->server_index_ptr->server_ip, "Get CMON Info", machine_flag, raw_format_for_gui, NULL, NULL);
    }else if(cp_flag) {
     if(machine_flag) {
       ret = SERVER_ADMIN_FAILURE;
       fprintf(stderr, "For windows this function is not available, Please do it manually !\n");
     }
     else 
       ret = check_port(&cmon_ptr,ip_port);
       audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Check for IP:PORT", machine_flag, raw_format_for_gui, NULL, NULL);
    } else if(c_flag) {
      if(strstr(cmd_args,"nsu_instance_control") != NULL)
      {
        if(K_flag)
        {
          ret = compare_hash_key(hash_key);
          if(ret == -1)
            fprintf(stderr, "Enable to genrate the timestamp");
        }
        else
        {
          NS_EXIT(0, "This is GUI supported command. This command only be run through GUI. For specified command you have to pass HashKey with -K option which is generated by GUI.\n");
        }
      }
      ret = run_users_command(&cmon_ptr, cmd_args);  
      if(ret != SERVER_ADMIN_SUCCESS)
      {
        ret_value = -1; // Set for error
        audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, cmd_args, machine_flag, raw_format_for_gui, NULL, NULL);
        if(!raw_format_for_gui)
          fprintf(stderr,"Error in running command %s on server %s \n",cmd_args,(&cmon_ptr)->server_index_ptr->server_ip);
        
        continue;
      }
      audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, cmd_args, machine_flag, raw_format_for_gui, NULL, NULL);
   } else if(m_flag) {
      if(machine_flag) {
        ret = SERVER_ADMIN_FAILURE;
	      fprintf(stderr, "For windows this function is not available, Please do it manually !\n");
      }
      else
        ret = show_monitor_running(&cmon_ptr);
      audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Show Running Monitors", machine_flag, raw_format_for_gui, NULL, NULL);
    } else if(ftp_flag) {
        ret = ftp_file(&cmon_ptr, ftp_file_name, dest_file_path, machine_flag);
        if(ret != SERVER_ADMIN_SUCCESS) 
          ret_value = -1; // Set for error
        audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Upload file", machine_flag, raw_format_for_gui, ftp_file_name, dest_file_path);
    } else if(download_flag) {
      ret = download_file(&cmon_ptr, download_file_name, machine_flag, R_flag, download_location, download_location_flag);
      if(ret != SERVER_ADMIN_SUCCESS)
        ret_value = -1; // Set for error
      audit_log(ret, username, (&cmon_ptr)->server_index_ptr->server_ip, "Download file", machine_flag, raw_format_for_gui, download_file_name, download_location);
    } else {
	     fprintf(stderr, "Option -f & --replace can be use with -G only!\n");
       usage();
    }
  }
 
  FREE_AND_MAKE_NULL(servers_given, "Free given server", -1);
  close(package_fd);
  close(upgrade_shell_fd);

  if(debug_on)
   fclose(debug_fp);
  return ret_value;
}
#endif
