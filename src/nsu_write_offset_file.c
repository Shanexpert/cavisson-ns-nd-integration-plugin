/*
* Name:       nsu_write_offset_file
*
* Purpose:    This tool writes data in various offset files used for recovery.
*             Tool might be useful in following cases -
*                 1.  DBU did not upload data for last 6 days. 
*                     User wants to upload data of last day only, and there is no need of last 6 days data.
*                     Tool can create offset file in partition so that DBU will start uploading from that partition.
*
*                 2.  User has manually uploaded data, and needs to create offset files to avoid duplicacy.
*
* Usage  :    nsu_write_offset_file <"OptionsSet1" OR "OptionsSet2"> <Common Options>
*
*             ================================== OPTION SET 1 ====================================
*               If user needs to create only 1 offset file, use -f and -s
*               -f <offset/data file name> -s <size in bytes>
*               Tool will auto detect whether provided file is offset file aur data file.
*               If file name has extension .offset, then tool will write size in that file;
*               else tool will create offset file named '.filename.offset'
*         
*
*                                               xxxx OR xxxx
*
*
*             ================================== OPTION SET 2 ====================================
*             -n <TR> -p <Partition> -m <module> -o <Options> -w <0/1/2> -a
*             -n <TR>        : Test Run Number
*             -p <partition> : If dynamic file offsets are to be crated, then provide partition.
*                              If Partition is not provided, then tool will write metadata offset files.
*             -m <module>    : Module is one of <NSDBU/NDDBU/NDP>
*             -o <Options>   :
*                            : In NSDBU and NDP
*                                To create offset file for all data files : 'ALL'.
*                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDP -o ALL -w 0
*                                To create only one offset file : 'DataFileName'
*                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NSDBU -o src.csv -w 0
*
*                            : In NDDBU
*                                To create offset of all csv in all instance : 'ALL'
*                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o ALL -w 0
*                                To create offset of all csv in one instance : 'InstanceName'
*                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o T_S_A -w 0
*                                To create offset of one csv in all instance : 'ALL/csvName'
*                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o ALL/NDFlowPath.csv -w 0
*                                To create offset of one csv in one instance : 'InstanceName/csvName'
*                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o T_S_A/NDFlowPath.csv -w 0
*
*             -w <0/1/2>     : To create empty offset files, existing offset files will not be overwritten : 0
*                            : To create empty offset files, existing offset files will be overwritten     : 1
*                            : To create offset files of data file size                                    : 2
*
*                         xxxxxxxxxxxx   ALL OPTIONS EXCEPT PARTITION IN OPTION SET 2 ARE MENDATORY   xxxxxxxxxxx
*
*             =========================================== COMMON OPTIONS =============================================
*             -e             : To write data in offset file in binary format : 0
*                            : To write data in offset file in ascii format  : 1 
*                            : Default for NSDBU and NDDBU is 0 and for NDP is 1
*                            : Default for -f and -s option is binary format.
*
* NOTE   :    This tool uses scandir to get all instance or csv, Hence please remove your backup files and directories. 
*
* Author :    Krishna Tayal
* Date   :    Nov 09, 2015       
*/



#define _BSD_SOURCE
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<dirent.h>
#include<errno.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<libgen.h>
#include "nslb_util.h"

#define UNKNOWN -1
#define NSDBU    0
#define NDDBU    1
#define NDP      2

int testrun = -1;
int module = UNKNOWN;
char *extension = NULL;
long long int partition_idx = -1;
char partition_name[32] = {0};
int write_csv_size_in_offset_file = 0;
int write_ascii_format = -1;

char instance_name[1024] = {0};
char data_file_name[1024] = {0};
char *controller_name = NULL;

char nFlag = 0;
char mFlag = 0;
char pFlag = 0;
char oFlag = 0;
char wFlag = 0;
char sFlag = 0;
char fFlag = 0;
char eFlag = 0;


void usage(char *err)
{
  fprintf(stderr, "\n%s\n\n", err);
  fprintf(stderr,
  " Name:       nsu_write_offset_file\n\n"
  " Purpose:    This tool writes data in various offset files used for recovery.\n"
  "             Tool might be useful in following cases -\n"
  "                 1.  DBU did not upload data for last 6 days. \n"
  "                     User wants to upload data of last day only, and there is no need of last 6 days data.\n"
  "                     Tool can create offset file in partition so that DBU will start uploading from that partition.\n\n"
  "                 2.  User has manually uploaded data, and needs to create offset files to avoid duplicacy.\n\n"
  " Usage  :    nsu_write_offset_file <\"OptionsSet1\" OR \"OptionsSet2\"> <Common Options>\n\n"
  "             ================================== OPTION SET 1 ====================================\n"
  "               If user needs to create only 1 offset file, use -f and -s\n"
  "               -f <offset/data file name> -s <size in bytes>\n"
  "               Tool will auto detect whether provided file is offset file aur data file.\n"
  "               If file name has extension .offset, then tool will write size in that file;\n"
  "               else tool will create offset file named '.filename.offset'\n\n\n"
  "                                               xxxx OR xxxx\n\n\n"
  "             ================================== OPTION SET 2 ====================================\n"
  "             -n <TR> -p <Partition> -m <module> -o <Options> -w <0/1/2> -a\n"
  "             -n <TR>        : Test Run Number\n"
  "             -p <partition> : If dynamic file offsets are to be crated, then provide partition.\n"
  "                              If Partition is not provided, then tool will write metadata offset files.\n"
  "             -m <module>    : Module is one of <NSDBU/NDDBU/NDP>\n"
  "             -o <Options>   :\n"
  "                            : In NSDBU and NDP\n"
  "                                To create offset file for all data files : 'ALL'.\n"
  "                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDP -o ALL -w 0\n"
  "                                To create only one offset file : 'DataFileName'\n"
  "                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NSDBU -o src.csv -w 0\n\n"
  "                            : In NDDBU\n"
  "                                To create offset of all csv in all instance : 'ALL'\n"
  "                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o ALL -w 0\n"
  "                                To create offset of all csv in one instance : 'InstanceName'\n"
  "                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o T_S_A -w 0\n"
  "                                To create offset of one csv in all instance : 'ALL/csvName'\n"
  "                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o ALL/NDFlowPath.csv -w 0\n"
  "                                To create offset of one csv in one instance : 'InstanceName/csvName'\n"
  "                                  Ex - nsu_write_offset_file -n 1234 -p 20151011123456 -m NDDBU -o T_S_A/NDFlowPath.csv -w 0\n\n"
  "             -w <0/1/2>     : To create empty offset files, existing offset files will not be overwritten : 0\n"
  "                            : To create empty offset files, existing offset files will be overwritten     : 1\n"
  "                            : To create offset files of data file size                                    : 2\n\n"
  "                         xxxxxxxxxxxx   ALL OPTIONS EXCEPT PARTITION IN OPTION SET 2 ARE MENDATORY   xxxxxxxxxxx\n\n"
  "             =========================================== COMMON OPTIONS =============================================\n"
  "             -e             : To write data in offset file in binary format : 0\n"
  "                            : To write data in offset file in ascii format  : 1\n" 
  "                            : Default for NSDBU and NDDBU is 0 and for NDP is 1\n"
  "                            : Default for -f and -s option is binary format.\n\n"
  " NOTE   :    This tool uses scandir to get all instance or files. "
  " Hence please remove your backup files and directories from csv, raw_data and instance directories before using this tool.\n\n\n"); 
                     
  exit(-1);
}


void get_offset_file_name(char *src, char *dest)
{
  char *ptr = NULL;

  ptr = strstr(src, ".offset");

  if(ptr && (ptr[7] == '\0'))
    sprintf(dest, "%s", src);
  else
    sprintf(dest, ".%s.offset", src);
}

int validate_file_name(char *file_name)
{
  char *ptr = NULL;

  ptr = strstr(file_name, extension); 

  if(ptr && *(ptr + strlen(extension)) == '\0')
    return 1;
  else
    return 0;
}

void write_in_offset_file(char *file_path, char *file_name, long long size)
{
  //int ret;
  int fd;
  char offset_file_name[1024] = {0};
  char tmp[1024] = {0};
  struct stat s;


  if((size < 0) && (validate_file_name(file_name) == 0))
    return;

  get_offset_file_name(file_name, offset_file_name);

  sprintf(tmp, "%s/%s", file_path, offset_file_name);

  if(write_csv_size_in_offset_file > 0 || fFlag)
    fd = open(tmp, O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0666);
  else
    fd = open(tmp, O_CREAT|O_RDWR|O_CLOEXEC, 0666);

  if(fd < 0)
  {
    fprintf(stderr, "Unable to open file '%s', Error is '%s' \n", offset_file_name, nslb_strerror(errno));  //TODO
    exit(-1);
  }

  if(stat(file_path, &s) == 0)
    chown(tmp, s.st_uid, s.st_gid);

  if(write_csv_size_in_offset_file == 2)
  {
    sprintf(tmp, "%s/%s", file_path, file_name);
    if(stat(tmp, &s) == 0)
      size = s.st_size;
    else
      fprintf(stderr, "csv file '%s' does not exist, still empty offset file might be created\n", tmp);
  }

  if(size >= 0)
  {
    if(write_ascii_format)
    {
      sprintf(tmp, "%lld", size);
      write(fd, tmp, strlen(tmp));
    }
    else
    {
      write(fd, (char*)&(size), sizeof(long long));
    }
  }

  close(fd);
}


int find_data_file_and_write_offset(char *csv_dir)
{
  int n;
  struct dirent **entry;
  
  if(data_file_name[0] != '\0' && strcasecmp(data_file_name, "ALL") != 0) //TODO
  {
    write_in_offset_file(csv_dir, data_file_name, -1);
    return 0;
  }

  n = scandir(csv_dir, &entry, 0, alphasort);
  
  if (n < 0)
  {
    perror("scandir");
    return -1;
  }
  
  while(n--)
  {
    if((nslb_get_file_type(csv_dir, entry[n]) == DT_REG ||entry[n]->d_type == DT_LNK) && entry[n]->d_name[0] != '.')
      write_in_offset_file(csv_dir, entry[n]->d_name, -1);

    free(entry[n]);
  }
  
  free(entry);
  return 0;
}

void get_all_instance_and_write_offset(char *csv_dir)
{
  int n;
  char tmp[1024] = {0};
  struct dirent **namelist = NULL;

  n = scandir(csv_dir, &namelist, 0, alphasort);

  if (n < 0)
  {
    perror("scandir");
    exit(-1);
  }

  while (n--)
  {
    if(nslb_get_file_type(csv_dir, namelist[n]) == DT_DIR && strcmp(namelist[n]->d_name, ".") != 0 && strcmp(namelist[n]->d_name, "..") != 0)
    {
      sprintf(tmp, "%s/%s", csv_dir, namelist[n]->d_name);
      find_data_file_and_write_offset(tmp);
    }
    else if(nslb_get_file_type(csv_dir, namelist[n]) == DT_REG)
    {
      write_in_offset_file(csv_dir, namelist[n]->d_name, -1);
    }
    free(namelist[n]);
  }

  free(namelist);
}

void write_all_csv_offset()
{
  char csv_dir[1024] = {0};

  if(module == NSDBU)
  {
    if(partition_name[0] != '\0')
      sprintf(csv_dir, "%s/logs/TR%d/%s/reports/csv/", controller_name, testrun, partition_name);
    else
      sprintf(csv_dir, "%s/logs/TR%d/common_files/reports/csv/", controller_name, testrun);

    find_data_file_and_write_offset(csv_dir);
  }
  else if(module == NDP)
  {
    sprintf(csv_dir, "%s/logs/TR%d/%s/nd/raw_data/", controller_name, testrun, partition_name);
    find_data_file_and_write_offset(csv_dir);
  }
  else if(module == NDDBU)
  {
    if(strcasecmp(instance_name, "ALL") == 0)
    {
      sprintf(csv_dir, "%s/logs/TR%d/%s/nd/csv/", controller_name, testrun, partition_name);
      get_all_instance_and_write_offset(csv_dir);
    }
    else
    {
      sprintf(csv_dir, "%s/logs/TR%d/%s/nd/csv/%s", controller_name, testrun, partition_name, instance_name);
      find_data_file_and_write_offset(csv_dir);
    }
  }
}

void check_testrun_exist()
{
  char buf[1024] = {0};
  struct stat s;
  
  sprintf(buf, "%s/logs/TR%d", controller_name, testrun);

  if(stat(buf,&s) < 0)
  {
    fprintf(stderr, "Testrun '%s' does not exist, Error is '%s'\n", buf, nslb_strerror(errno));
    exit (-1);
  }
  
  if ((s.st_mode & S_IFDIR) != S_IFDIR)
  {
    fprintf(stderr, "Testrun %d is not a directory\n", testrun);
    exit (-1);
  }
}

void check_partition_exist()
{
  char buf[1024] = {0};
  struct stat s;
  
  sprintf(buf, "%s/logs/TR%d/%s", controller_name, testrun, partition_name);

  if(stat(buf, &s) < 0)
  {
    fprintf(stderr, "partition '%s' is not exist, Error is '%s' \n", buf, nslb_strerror(errno));
    exit (-1);
  }

  if ((s.st_mode & S_IFDIR) != S_IFDIR)
  {
    fprintf(stderr, "partition %s is not a directory \n", partition_name);
    exit (-1);
  }
}

void save_module_and_extension(char *module_name)
{
  if(strcasecmp(module_name, "NSDBU") == 0)
  {
    module = NSDBU;
    extension = ".csv";
  }
  else if(strcasecmp(module_name, "NDDBU") == 0)
  {
    module = NDDBU;
    extension = ".csv";
  }
  else if(strcasecmp(module_name, "NDP") == 0)
  {
    module = NDP;
    extension = ".txt";
  }
  else
  {
    usage("Error: Invalid module name, choose one of NSDBU/NDDBU/NDP");
  }
}

void parse_csv_options(char *csv_options)
{
  char tmp[1024] = {0};

  if(module == NSDBU || module == NDP)
  {
    if(strcasecmp(csv_options, "ALL") != 0)
      strcpy(data_file_name, csv_options);
  }
  else if(module == NDDBU)
  {
    strcpy(tmp, csv_options);
    strcpy(instance_name, dirname(tmp));

    if(strcmp(instance_name, ".") == 0)
    {
      strcpy(instance_name, csv_options);
    }
    else
    {
      strcpy(tmp, csv_options);
      strcpy(data_file_name, basename(tmp));
    }
  }
}

int main(int argc, char *argv[])
{ 
  //struct dirent **namelist;
  int option;
  //char buffer[256], csv_dir[256];
  long long csv_size = -1;
  char csv_path[1024] = {0};
 

  while ((option = getopt(argc, argv, "n:m:p:o:w:s:f:e:")) != -1) 
  {
    switch(option)
    {
      case 'n': testrun = atoi(optarg);
                nFlag = 1;
                break;

      case 'm': save_module_and_extension(optarg);
                mFlag = 1;
                break;

      case 'p': strcpy(partition_name, optarg);
                pFlag = 1;
                break;

      case 'o': parse_csv_options(optarg);
                oFlag = 1;
                break;

      case 'w': write_csv_size_in_offset_file = atoi(optarg);
                wFlag = 1;
                break;

      case 's': csv_size = atoll(optarg);
                if(csv_size < 0)
                  exit(-1);
                sFlag = 1;
                break;

      case 'f': strcpy(csv_path, optarg);
                fFlag = 1;
                break;

      case 'e': write_ascii_format = atoi(optarg);
                eFlag = 1;
                break;

    }
  }
   
  if(nFlag && testrun <= 0)
    usage("Error: Test run is not provided.");

  if(pFlag && partition_name[0] == '\0')
    usage("Error: Invalid Partition");

  if(mFlag && module == UNKNOWN)
    usage("Error: Provide valid module name NS/ND");

  if(!oFlag && !fFlag)
    usage("Error: Too few arguments.");

  if((oFlag && !wFlag) || (!oFlag && wFlag))
    usage("Error: -o and -w must be used together");
  
  if((fFlag && !sFlag) || (!fFlag && sFlag))
    usage("Error: -f and -s must be used together");

  if(oFlag && fFlag)
    usage("Error: -o and -f cannot be used together");

  if(module == NDP && partition_name[0] == '\0')
    usage("Error: Partition is mandatory with module NDP");

  if(wFlag && (write_csv_size_in_offset_file < 0 || write_csv_size_in_offset_file > 2))
    usage("Error: Valid values for -w option are 0, 1 or 2");

  if(!eFlag)
  {
    if(fFlag)     //In case using -f and -s, default is binary format
      write_ascii_format = 0;
    else if(module == NDP)   //ascii for NDP
      write_ascii_format = 1;
    else if(module == NSDBU || module == NDDBU)  //Binary for NSDBU and NDDBU
      write_ascii_format = 0;
  }

  if(write_ascii_format < 0 || write_ascii_format > 1)
    usage("Error: Valid values for -e option are 0 or 1"); 


  if(fFlag)
  {
    char tmp[1024] = {0};
    char filename[1024] = {0};
    char filedir[1024] = {0};
    strcpy(tmp, csv_path);
    strcpy(filename, basename(tmp)); 
    strcpy(tmp, csv_path);
    strcpy(filedir, dirname(tmp)); 
    write_in_offset_file(filedir, filename, csv_size);
  }
  else
  {
    controller_name = getenv("NS_WDIR");
    if(controller_name == NULL)
    {
      fprintf(stderr, "Error in getting controller name\n");
      exit(-1);
    }

    check_testrun_exist();

    if(pFlag)
      check_partition_exist();

    write_all_csv_offset();
  } 
 
  return 0;
}
