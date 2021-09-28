/* File name : ns_get_log_file_monitor.c
 * Purpose: This file contains methods for Get Log File Monitor. 
 *          These methods parse arguments and creates destination file where monitor dumps data.
 * Author : Krishna
 * Createtion Date:
 * MOdification date:
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <curl/curl.h>
#include "nslb_util.h"
#include "nslb_http_auth.h"
#include "util.h"
#include "ns_msg_com_util.h"
#include "ns_custom_monitor.h"
#include "ns_string.h"
#include "ns_get_log_file_monitor.h"
#include <sys/stat.h>
#include <libgen.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#define MAX_LINE_LEN 1024

/*  This function removes quotes from string.
 *  Input:  source string within quotes.
 *          dest string to save unquoted source string.
 *  Output: Unquoted string is stored in dest.
 *  We are removing quotes because later we may need to concatenate dest file name and source file name;
 *  Then resultant string may have unwanted quotes.
 */
static int remove_quotes(char *src, char *dest)
{
  int len;
  len = strlen(src);
  if(len == 0)
    return -1;
  /* Removing first quotation mark */
  if('"' == src[0] || '\'' == src[0])
    strncpy(dest, src + 1, len);
  else
    strncpy(dest, src, len + 1);

  /* Removing last quotation mark */
  len = strlen(dest);
  if('"' == dest[len -1] || '\'' == dest[len -1])
    dest[len -1] = '\0';
  return 0;
}

/*  Here we are adding an addition option '-x';
 *  If any argument parsing error occurs, '-x' option with error msg will be appended to pgm_args;
 *  This error msg is read by LPS and monitor exits if LPS finds -x option.
 *  Also We are assuming that -x option is appended at end of pgm args;
 *  Hence LPS will treat pgm_args string as error msg from '-x' till end of string.
 */
static int usage(char *pgm_args, char *error)
{
  NSDL2_MON(NULL, NULL, "Method Called.");
  sprintf(pgm_args + strlen(pgm_args), " -x %s", error);
  NSDL2_MON(NULL, NULL, "Error message to be sent to LPS is %s", error);
  NSDL2_MON(NULL, NULL, "Method Exited.");
  return -1;
}

/*  This functions runs a command on remote server using 'nsu_server_admin'.
 *  This function runs 'nsu_server_admin' using popen().
 *  Input:  server ip and command to be executed.
 *  Output: It returns FILE * pointer to pipe of popen.
 *          output of 'nsu_server_admin' can be read using this pipe.
 *  Calling function has to pclose() this pointer.
 */ 
static FILE *run_nsu_server_admin(char *server, char *command)
{
  FILE *fp;
  char buf[MAX_LINE_LEN + 1] = {0};
  sprintf(buf, "nsu_server_admin -s %s -c \'%s\'", server, command);
  fp = popen(buf, "r");
  return fp;
}
/* This validate_dest_file() method checks if given file path exists,
 * if it is a directory or file.
 * If file path doesn't exist it creates path.
 * Input:
 *   ...
 * Return Values:
 *  USER_FILE_NAME:       If User has provided valid dest path with file name and file doesn't exist.          
 *  USER_FILE_NAME_EXIST: If User has provided valid dest path with file name and file exists.          
 *  USER_FILE_PATH:       If User has provided valid dest path but file name is not given.          
 */
static int validate_dest_file(char *pgm_args, char *file_path, char *lps_server, int lps_machine_flag)
{
  int ret_val = -1;
  char err_buf[MAX_LINE_LEN + 1];
  struct stat s;
  NSDL2_MON(NULL, NULL, "Method Called.");
  NSDL2_MON(NULL, NULL, "Destination File Path = %s", file_path);
    
  //Check if destination file is not same for any other get_log_file monitor
/*  for (i = 0; i < total_monitor_list_entries; i++) 
  {
    if(monitor_list_ptr[i].cm_info_mon_conn_ptr->dest_file_name)
    {
      if (strcmp(monitor_list_ptr[i].cm_info_mon_conn_ptr->dest_file_name, file_path) == 0)
      {
        sprintf(err_buf, "Duplicate Destination file name %s", file_path); 
        return(usage(pgm_args, err_buf));
      }
    }
  }*/
  /*  This is the case when LPS is running on different machine than NS */
  if(1 == lps_machine_flag)
  {
    FILE *fp;
    char buf[MAX_LINE_LEN + 1] = {0};

    sprintf(buf, "stat \"%s\"", file_path);
    fp = run_nsu_server_admin(lps_server, buf);  //using 'nsu_server_admin'

    while(fgets(buf, MAX_LINE_LEN, fp))         //reading output of 'nsu_server_admin'
    {
      if(strstr(buf, "No such file or directory")) //if file doesn't exist 
      {
        if('/' == file_path[strlen(file_path) - 1])
          ret_val = USER_FILE_PATH;
        else
          ret_val = USER_FILE_NAME;
        break;
      }
      else if(strstr(buf, "regular file") || strstr(buf, "regular empty file"))  //if file exists
      {
        ret_val = USER_FILE_NAME_EXIST;
        break;
      }
      else if(strstr(buf, "directory"))       //if given path is directory
      {
        ret_val = USER_FILE_PATH_EXIST;
        break;
      }
    }
    pclose(fp);
    if(-1 == ret_val)     //if given path is of other type or 'nsu_server_admin' fails
    {
      sprintf(err_buf, "Destination file name %s is neither a directory nor regular file", file_path);
      return(usage(pgm_args, err_buf));
    }
  }
  /*  This is the case when LPS is running on same machine as NS */
  else
  {
    if(0 == stat(file_path, &s))   //Checking if file path exists
    {  
      if(S_ISDIR(s.st_mode))      //checking if file path is directory
        ret_val = USER_FILE_PATH_EXIST;
      else if(S_ISREG(s.st_mode))   ////checking if file path is a file
        ret_val = USER_FILE_NAME_EXIST;
      else
      {
        sprintf(err_buf, "Destination file name %s is neither a directory nor regular file", file_path); 
        return(usage(pgm_args, err_buf));
      }
    }
    else                        //if file path doesn't exists
    {
      //if file path has '/' at end, it will be considered as dir, file otherwise
      if('/' == file_path[strlen(file_path) - 1])
        ret_val = USER_FILE_PATH;
      else
        ret_val = USER_FILE_NAME;
    }
  }
  return(ret_val);
}

/*  This function creates file at given path in given mode,
 *  it returns negative value if error occurs.
 */
static int create_dest_file(char *dest_file_name, char *mode, int status, char *lps_server, int lps_machine_flag)
{
  FILE *fp;
  NSDL2_MON(NULL, NULL, "Method called");

  /*  This is the case when LPS is running on different machine than NS */
  if(1 == lps_machine_flag)
  {
    char buf[2 * MAX_LINE_LEN] = {0};
    char buf2[MAX_LINE_LEN + 1] = {0};

    if(USER_FILE_PATH_EXIST != status && USER_FILE_NAME_EXIST != status)
    {
      /*  Creating Path */
      strcpy(buf, dest_file_name);
      sprintf(buf2, "%s", dirname(buf));//passing buf because dirname() can modify its argument.
      sprintf(buf, "mkdir -p \"%s\"", buf2);
      fp = run_nsu_server_admin(lps_server, buf); 
      if(strstr(buf, "cannot create directory"))
      {
        pclose(fp);
        return -1;
      } 
      pclose(fp);
    }

    /*  Creating Dest file or truncating existing dest file */
    sprintf(buf, "cp /dev/null \"%s\"", dest_file_name);
    fp = run_nsu_server_admin(lps_server, buf);
    pclose(fp);
  } 
  else
  {
    if(USER_FILE_PATH_EXIST != status && USER_FILE_NAME_EXIST != status)
    {
      /*  Creating Path */
      if(mkdir_ex(dest_file_name) == 0)   //This function creates path.
        return -1;
    }
  
    fp = fopen(dest_file_name, mode);
    if( !fp )
    {
      return -1;
    }
    fclose(fp);
  }
  NSDL2_MON(NULL, NULL, "Method Exited");
  return 0;
}

/*  This  function  will  convert  the given URL encoded input string to a "plain string" */
int decode_file_name(char *input, char *output)
{
  char *ptr;
  ptr = input;
  CURL *handle;

  if(NULL == input || NULL == output)
  {
    return -1;
  }
  /*Java encodes spaces to + sign. Hence decoding. */
  while((ptr = strchr(ptr, '+')) != NULL)
  {
    *ptr = ' ';
     ptr++;
  }
  handle = curl_easy_init();
  if(!handle)
    return -1; 
  ptr = NULL;
  /* curl_unescape() function will be removed in a future release.
   * Hence using curl_easy_unescape() function
   */
  ptr = curl_easy_unescape(handle, input, strlen(input), 0);
  if(!ptr)
    return -1;
  strcpy(output, ptr);
  curl_free(ptr);
  curl_easy_cleanup(handle);
  return 0;
} 

/*  This  function  will  convert  the given input "plain string" to URL encoded string */
static int encode_file_name(char *input, char *output)
{
  char *ptr;
  ptr = input;
  CURL *handle;

  if(NULL == input || NULL == output)
    return -1;
  handle = curl_easy_init();
  if(!handle)
    return -1; 
  ptr = NULL;
  /* curl_escape() function will be removed in a future release.
   * Hence using curl_easy_escape() function
   */
  ptr = curl_easy_escape(handle, input, 0);
  if(!ptr)
    return -1;
  strcpy(output, ptr);
  curl_free(ptr);
  curl_easy_cleanup(handle);
  return 0;
} 



/* This function parses argument string and stores arguments at respective pointers. */
static int extract_getFileMonitor_args(char *pgm_path, char *pgm_args, char *src_file_name_url, char *dest_file_name_url, int *fFlag, int *dFlag, int *cFlag, int *tFlag, int *IpPortFlag, int *ZFlag, int *kFlag ,int *HFlag, char *vector_name, char *server_name)
{
  NSDL2_MON(NULL, NULL, "Method called");
  char lol_pgm_args[MAX_LINE_LEN + 1] = {0}, buf[MAX_LINE_LEN + 1];
  char *field[1000] = {0};
  int num_field;
  char c;
  
  /*  getopts parses the tokenised array from index 1,
   *  even if optind is set to 1.  
   *  Hence we need to store something at index 0 .
   */
  sprintf(lol_pgm_args, "%s ", pgm_path);
  sprintf(lol_pgm_args + strlen(lol_pgm_args), "%s", pgm_args);

  num_field = get_tokens_with_multi_delimiter(lol_pgm_args, field, " ", 1000);

  optind = 0;
  //while ((c = getopt(num_field, field, "c:f:d:tD")) != -1)
  while ((c = getopt(num_field, field, "c:f:d:s:i:tDe:Z:k:H:")) != -1)
  {
    switch (c)
    {
      case 'd':   //To set dest file path 
        if(*dFlag)
          return(usage(pgm_args, "second time use of -d option"));

        strcpy(buf, optarg);
        if(remove_quotes(buf, dest_file_name_url) < 0)
          return(usage(pgm_args, "Cannot remove quotes"));
        NSDL4_MON(NULL, NULL, "dest_file_name_url = %s", dest_file_name_url);
        *dFlag = 1;
        break;
      
      case 'i':
        if(*dFlag)
          break;
        
        strcpy(dest_file_name_url, optarg);
        NSDL4_MON(NULL, NULL, "dest_file_name_url = %s", dest_file_name_url);
        *IpPortFlag = 1;
        break; 

      case 'f':   //To set source file path
        if(*fFlag || *cFlag)
          return(usage(pgm_args, "second time use of -c or -f option"));

        strcpy(buf, optarg);
        if(remove_quotes(buf, src_file_name_url) < 0)
          return(usage(pgm_args, "Cannot remove quotes"));
        NSDL2_MON(NULL, NULL, "src_file_name_url = %s", src_file_name_url);
        *fFlag = 1;
        break;

      case 'c':
        if( *cFlag || *fFlag )
          return(usage(pgm_args, "second time use of -c or -f option"));
        
        *cFlag = 1;
        break;
      case 't':     //To set truncate option
        if( *tFlag )
          return(usage(pgm_args, "second time use of -t option"));

        *tFlag = 1;
        break;
      case 's':
        break;
      case 'e':
        break;
      case 'Z' :
        if ( *ZFlag )
          return(usage(pgm_args, "second time use of -Z option"));
	
         
        *ZFlag = 1;
        break;
      case 'k' :
        if ( *kFlag )
          return(usage(pgm_args, "second time use of -k option"));
         
        *kFlag = 1;
         break;
      case 'H' :
        if ( *HFlag )
          return(usage(pgm_args, "second time use of -H option"));
         
        *HFlag = 1;
        break;
      case '?':
      default:
        return(usage(pgm_args, "invalid argument"));
    }
    
  }
  if(!(*cFlag) && !(*fFlag))
    return(usage(pgm_args, "Mandatory options missing"));

  return 0;
}

/*  This function tells whether LPS is running on same machine as NS or not.
 *  This function matches LPS server ip with ip of all interfaces of NS machine.
 *  Input:  LPS machine ip
 *  Output: 0 if LPS is running on NS machine.
 *          1 if LPS is running on different machine.
 *         -1 if error occurs.
 */
static int is_lps_ns_on_same_ip(char *lps_server)
{
  NSDL2_MON(NULL, NULL, "Method Called.");
  //global_settings->lps_server
  struct ifaddrs *ifaddr, *ifa;
  int family, s, ret_val = 1;
  char host[NI_MAXHOST];

  if(!strncmp(lps_server, "127.", 4))
    ret_val = 0;

  else if (getifaddrs(&ifaddr) == -1) 
    ret_val = -1;

  /*  Walk through linked list, maintaining head pointer so we
   *  can free list later */
  else
  {
    /*  We match lps_server ip with ip of all interfaces of local machine.
     *  0 is returned if match found; -1 is returned if error occurs; 1 is returned otherwise
     */   
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
    {
      family = ifa->ifa_addr->sa_family;

      if (family == AF_INET || family == AF_INET6) 
      {
        s = getnameinfo(ifa->ifa_addr,
            (family == AF_INET) ? sizeof(struct sockaddr_in) :
            sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    
        if (s != 0)
        { 
          ret_val = -1;
          break;
        }
        else if(!strcmp(host, lps_server))
        {
          ret_val = 0;
          break;
        }
      }
    }
    freeifaddrs(ifaddr);
  }

  NSDL2_MON(NULL, NULL, "Method Exited.");
  return ret_val;
}

static int hostname_to_ip(char *hostname, char* ip)
{
  struct hostent *he;
  struct in_addr **addr_list;
  int i, ret_val;
  int herr;
  
  he = nslb_gethostbyname2(hostname, AF_INET, &herr);
  if(!he)
    ret_val = -1;
  else
  {
    addr_list = (struct in_addr **) he->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++)
    {
      //Return the first one;
      strcpy(ip ,inet_ntoa(*addr_list[i]));
      ret_val = 0;
      break;
    }
  }
  return ret_val;
}

static void get_src_file_name(char *src)
{
  char dest[MAX_LINE_LEN + 1] = {0};
  char *ptr = src;
  if('/' == *ptr)
    ptr++;

  strcpy(dest, ptr);
  for(ptr = dest; *ptr; ptr++)
    if('/' == *ptr)
      *ptr = '_';

  strcpy(src, dest);
}
/*  This function takes argument string of cm_get_log_file monitor and parses it.
 *  Then it checks arguments and validates destination file path and creates destination file.
 */
int create_getFileMonitor_dest_file(char *pgm_path, char *pgm_args, char *vector_name, char *server_name, char *dest_file, char *err_buf)
{
  int fFlag = 0, dFlag = 0, cFlag = 0, tFlag = 0, IpPortFlag = 0, ZFlag = 0, kFlag = 0, HFlag  = 0;
  int status, ret = 0;
  char dest_file_name_url[MAX_LINE_LEN + 1] = {0}, dest_file_name[4* MAX_LINE_LEN] = {0};
  char src_file_name_url[MAX_LINE_LEN + 1] = {0}, src_file_name[MAX_LINE_LEN + 1] = {0};
  char mode[3] = "a", ns_wdir[MAX_LINE_LEN + 1] = {0}, buf[MAX_LINE_LEN + 1] = {0};
  char lps_server[MAX_LINE_LEN + 1] = {0};
  int lps_machine_flag;

  NSDL1_MON(NULL, NULL, "Method called. pgm_args = %s, vector_name = %s, server_name = %s", pgm_args, vector_name, server_name);

  strcpy(ns_wdir, getenv("NS_WDIR"));

  /*  Getting ip of lps server in case of hostname is provided  */
  if(hostname_to_ip(global_settings->lps_server, lps_server) < 0)
  {
    sprintf(err_buf, "Cannot resolve hostname of lps server %s", global_settings->lps_server);
    return(usage(pgm_args, err_buf));
  }
  NSDL1_MON(NULL, NULL, "LPS hostname %s resolved to ip %s", global_settings->lps_server, lps_server);
 
  ret = extract_getFileMonitor_args(pgm_path, pgm_args, src_file_name_url, dest_file_name_url, &fFlag, &dFlag, &cFlag, &tFlag,
                                    &IpPortFlag, &ZFlag, &kFlag, &HFlag, vector_name, server_name);

  if(ret < 0)
  {
    NSDL1_MON(NULL, NULL, "Error in Parsing pgm_args = %s, vector_name = %s, server_name = %s",
                                            pgm_args, vector_name, server_name); 
    return -1;
  }
   NSDL1_MON(NULL, NULL, "src_file_name_url = %s, dest_file_name_url = %s fFlag = %d, dFlag = %d, cFlag = %d, tFlag = %d, IpPortFlag = %d,     Zflag = %d, kFlag = %d, HFlag = %d ",  src_file_name_url, dest_file_name_url, fFlag, dFlag, cFlag, tFlag, &IpPortFlag, ZFlag, kFlag, HFlag );

  if(fFlag)
  {  
    //As file names are in url form in mprof; hence decoding.
    if(decode_file_name(src_file_name_url, src_file_name) < 0)
    {
      sprintf(err_buf, "Cannot decode source file %s url", src_file_name_url); 
      return(usage(pgm_args, err_buf));
    }
    NSDL2_MON(NULL, NULL, "src_file_name = %s", src_file_name);
  }

  if((lps_machine_flag = is_lps_ns_on_same_ip(lps_server)) < 0)
  {
    sprintf(err_buf, "Error in getting LPS machine: %s", nslb_strerror(errno));
    return(usage(pgm_args, err_buf));
  }
  NSDL1_MON(NULL, NULL, "LPS is working on %s", lps_machine_flag == 0?"same machine":"different machine");
  //If dest file path is not given, dest file will be created in TR directory.
  if(IpPortFlag)
  {
    status = SEND_ON_IP_PORT;
    strcpy(dest_file_name, dest_file_name_url);
  }
  else if(!dFlag)
  {
    if(0 == lps_machine_flag)
      status = TEST_RUN_DIR;
    else
      status = LPS_LOG_DIR;
  }
  else
  {
    if(decode_file_name(dest_file_name_url, dest_file_name) < 0)
    {
      sprintf(err_buf, "Cannot decode destination file %s url", dest_file_name_url); 
      return(usage(pgm_args, err_buf));
    }
    status = validate_dest_file(pgm_args, dest_file_name, lps_server, lps_machine_flag);
  }
  NSDL2_MON(NULL, NULL, "status of destination file = %d", status);
 
  if(status < 0)
  {
    NSDL1_MON(NULL, NULL, "File path %s of vector name %s server name %s is invalid.\n", dest_file_name, vector_name, server_name);
    return -1;
  }
  /*  if dest file name is not given, we use source file name as dest file name.
   *  if -c option is given instead of -f option, we cannot get source file name.
   *  Hence dest file name must be provided with -c option.
   */
  //TODO get options to support this
  else if((TEST_RUN_DIR == status || LPS_LOG_DIR == status || USER_FILE_PATH == status || USER_FILE_PATH_EXIST == status) && cFlag)
    return(usage(pgm_args, "-c option cannot be used without dest file name"));

  //TODO FILE NAME IS MAX 255. PATH LENGTH is unlimited. This function should take care.
  else if( TEST_RUN_DIR == status )
  {
    /*  There can be more than 1 files present with same name at remote server at different paths.
     *  In this case, if user hasn't provided dest file name, we may update same dest file for more than 1 monitors.
     *  Hence we are using full path name of source file as dest file name,
     *  And converting all '/' characters to '_'.
     */
    get_src_file_name(src_file_name); 
    sprintf(dest_file_name, "%s/logs/%s/server_logs/%s/%s/%s", ns_wdir, global_settings->tr_or_partition, server_name, vector_name, src_file_name);
  }
  else if( LPS_LOG_DIR == status )
  {
    /*  At present, we are not supporting this option;
     *  If LPS is working on different machine than NS, we know lps_port.
     *  To get work path on different nachine, we need to use first nestat -natp to get pid;
     *  then we'll have to use ps to get command path; then we'll be able to get work path.
     *  Also to use netstat -natp, CMON must be running with root privilages.
     */
    //TODO get options to support this
    return(usage(pgm_args, "LPS on different machine cannot be used without -d option."));
  }
  else if(USER_FILE_NAME_EXIST == status && !tFlag)
  {
    /*  If dest file name provided by user already exists,
     *  we check truncate option; if tFlag is not set monitor will exit.
     */
    sprintf(err_buf, "Dest file %s already exists and -t option is not set", dest_file_name); 
    return(usage(pgm_args, err_buf));
  }
  else if(USER_FILE_PATH == status || USER_FILE_PATH_EXIST == status)
  {
    //If user has provided file path, but file name is not provided.
    get_src_file_name(src_file_name);
    
    if('/' != dest_file_name[strlen(dest_file_name)-1]) 
    {
      int len = strlen(dest_file_name); 
      dest_file_name[len]='/';
      dest_file_name[len+1]='\0';
    }
    strcat(dest_file_name, src_file_name);
  }
  //Got file name now creating file
  if((!IpPortFlag) && (create_dest_file(dest_file_name, mode, status, lps_server, lps_machine_flag) < 0))
  {
      sprintf(err_buf, "Cannot create dest file %s", dest_file_name); 
      return(usage(pgm_args, err_buf));
  }
  /*  As we need to add file name in dest file path,
   *  to send to LPS, hence instead of changing content of -d option,
   *  we are adding new option '-z'.
   *  Now LPS will read -z to read destination file name.
   */ 

  NSDL1_MON(NULL, NULL, "dest_file_name = %s", dest_file_name);
  if(status == USER_FILE_NAME || USER_FILE_NAME_EXIST)
  {
    /*  dest_file will be returned to calling function.
     *  dest_file will be stored in dest_file_name var in CM_INFO structure.
     *  Later we compare these values to check if two or more monitors don't have same destination file.
     */ 
    strcpy(dest_file, dest_file_name);
  }
  if(!IpPortFlag)
  {
    /* encoding dest file name to send to LPS */
    strcpy(buf, dest_file_name);
    encode_file_name(buf, dest_file_name); 
  }
  //if dFlag is not set send -S also so that lps can know that this is not user specify dst file 
  if(dFlag)  
    sprintf(pgm_args + strlen(pgm_args), " -z %s", dest_file_name);
  else
    sprintf(pgm_args + strlen(pgm_args), " -z %s -S", dest_file_name);
  NSDL2_MON(NULL, NULL, "pgm_args sent to LPS = %s", pgm_args);
  return 0;
}

