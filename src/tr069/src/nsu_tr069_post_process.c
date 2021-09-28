/*************************************************************************************************************************

 Purpose: To process cpe data after saving
 Flow: main()-->>>get_nvm_data_files()
            -->>>update_nvm_data_files-->merge_data_files(); updated_cp_files(){-->backup_cp_file();}
 Author: Manish Kumar Mishra
 Date: 05August2011

************************************************************************************************************************/


/************************************************************************************************************************                                         File Includes
    
************************************************************************************************************************/

#define MAX_BUF_SIZE 64000 //1MB
//_GNU_SOURCE is defined for O_LARGE_FILE
#define _GNU_SOURCE
#define   _LF_ __LINE__, (char *)__FUNCTION__
#define SUCCESS 0
#define FAILURE 1
#define FINAL_CP_DATA_FILE "new_tr069_cpe_data.dat" 
#define INITAL_CP_DATA_FILE "tr069_cpe_data.dat"

#include<stdio.h>
#include <sys/types.h>
#include<dirent.h>
#include<errno.h>
#include<string.h>
#include<malloc.h>
#include <sys/stat.h>
#include <unistd.h>
#include<stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include<strings.h>
#include <ctype.h>
#include<stdarg.h>
#include <time.h>

#include "../../libnscore/nslb_util.h"

/************************************************************************************************************************
                                         Initialized Data 
    
************************************************************************************************************************/

char *progname =NULL;
static char ns_wdir_lol[2048];
static FILE *debug_fp = NULL;
static FILE *error_fp = NULL;
static int debug_level = 0; //off

/*
#define LOG_LEVEL_1 0x00000001
#define LOG_LEVEL_2 0x00000002
#define LOG_LEVEL_3 0x00000004
#define LOG_LEVEL_4 0x00000008
*/

#define LOG_LEVEL_1 0x000000FF
#define LOG_LEVEL_2 0x0000FF00
#define LOG_LEVEL_3 0x00FF0000
#define LOG_LEVEL_4 0xFF000000
/*************************************************************************************************************************                                          
*************************************************************************************************************************/

//This function will open the log file in append mode
static void open_debug_log(int test_run_num) {
 char debug_log_file[1024];
 /*if(debug_on)*/ {
    sprintf(debug_log_file, "%s/logs/TR%d/nsu_tr069_post_process.log", ns_wdir_lol, test_run_num);
    debug_fp = fopen(debug_log_file, "w");
    //debug_fp = open(debug_log_file, O_APPEND | O_RDWR | O_CREAT | O_LARGEFILE , (mode_t)0600);
    if(debug_fp == NULL) {
      fprintf(stderr, "Unable to open file '%s' for creating debug log file for nsa_log_mgr.\n", debug_log_file);
      exit(1);
    }
  }
}

//This function will print the current date and time in debug log in formate 08/17/11 19:58:42
static char *get_cur_date_time() {
  time_t    tloc;
  struct  tm *lt;
  static  char cur_date_time[100];

  (void)time(&tloc);
  if((lt = localtime(&tloc)) == (struct tm *)NULL)
    strcpy(cur_date_time, "Error|Error");
  else
    sprintf(cur_date_time, "%02d/%02d/%02d %02d:%02d:%02d",
                           lt->tm_mon + 1, lt->tm_mday, (1900 + lt->tm_year)%2000,
                           lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_date_time);
}

//This function will make log file
//Output of this function will be.. 
//08/17/11 19:58:42|176|read_file_and_fill_buf|offset = 0, bytes_to_read = 2582890
static void debug_log(int log_level, int line, char *fname, char *format, ...) {
  va_list ap;
  char buffer[MAX_BUF_SIZE + 1] = "\0";
  int amt_written = 0, amt_written1=0;

  if((debug_level & log_level) == 0) return;

  amt_written1 = sprintf(buffer, "\n%s|%d|%s|", get_cur_date_time(), line, fname);
  va_start(ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_BUF_SIZE] = 0;

  if(amt_written < 0) {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_BUF_SIZE - amt_written1)) {
    amt_written = (MAX_BUF_SIZE - amt_written1);
  }

  if(debug_fp) {
    if((fwrite(buffer, amt_written1+amt_written, 1, debug_fp))<0) {
    //if((write(buffer, amt_written1+amt_written, debug_fp))<0) 
      fprintf(stderr, "%s\n", "Unable to write to debug for ns_tr069_cp_data_post_processing.c");
      exit (-1);
    }
  }  else {
     fprintf(stderr, "%s", buffer + amt_written1);
  }

  /* Log immediately in case of log_always*/
  if(log_level && debug_fp) fflush(debug_fp);
}

//This function will open the error log file in append mode
static void open_error_log(int test_run_num) {
  char error_log_file[1024];
 
  sprintf(error_log_file, "%s/logs/TR%d/nsu_tr069_post_process_error.log", ns_wdir_lol, test_run_num);
  error_fp = fopen(error_log_file, "w");
  if(error_fp == NULL) {
    fprintf(stderr, "Unable to open file '%s' for creating error log file for nsu_tr069_post_process.c \n", error_log_file);
    exit(1);
  }
}

static void error_log(int line, char *fname, char *format, ...) {
  va_list ap;
  char buffer[MAX_BUF_SIZE + 1] = "\0";
  int amt_written = 0, amt_written1=0;

  amt_written1 = sprintf(buffer, "\n%s|%d|%s|", get_cur_date_time(), line, fname);
  va_start(ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_BUF_SIZE] = 0;

  if(amt_written < 0) {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_BUF_SIZE - amt_written1)) {
    amt_written = (MAX_BUF_SIZE - amt_written1);
  }

  if(error_fp) {
    if((fwrite(buffer, amt_written1+amt_written, 1, error_fp))<0) {
      fprintf(stderr, "%s\n", "Unable to write to error for ns_tr069_cp_data_post_processing.c");
      exit (-1);
    }
  }  else {
     fprintf(stderr, "%s", buffer + amt_written1);
  }

  if(error_fp) fflush(error_fp);
}



/* print files in current directory in reverse order */
/*
 * tr069_cpe_data_nvm00.dat
 * tr069_cpe_data_nvm01.dat
 * */
int only_nvm_data_files(const struct dirent *a)
{
  int len;
  //debug_log(0, _LF_, "Method Called: only_nvm_data_files");
  char *tmp = strstr(a->d_name, ".dat");
  if(tmp) *tmp = 0;

  len = strlen(a->d_name);

  if (isdigit(a->d_name[len - 1]) && len > (sizeof("tr069_cpe_data_nvm") - 1))
    return FAILURE;
  else
    return SUCCESS;
}

//Sort nvm files in assending order
#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
int my_alpha_sort(const struct dirent **aa, const struct dirent **bb)
#else
int my_alpha_sort(const void *aa, const void *bb)
#endif
{
  const struct dirent **a = (const struct dirent **) aa;
  const struct dirent **b = (const struct dirent **) bb;

  if (atoi((*a)->d_name + (sizeof("tr069_cpe_data_nvm") - 1)) < atoi((*b)->d_name + (sizeof("tr069_cpe_data_nvm") - 1)))
      return FAILURE;
  else
      return SUCCESS;

}

/*
This function will store the nvm files into an array
Ex: suppose in directory tro69 there are four nvm files (for 2 nvm) 
  tr069_cpe_data_nvm00.dat
  tr069_cpe_data_nvm01.dat

  Then array  will store only two files
  tr069_cpe_data_nvm00
  tr069_cpe_data_nvm01
*/
static int get_nvm_data_files(char *dname, char file_names[][500], int *num_files){
  char dir_name[1024];
  int i = 0;
  struct dirent **namelist;
  int numFile = 0;
  
  debug_log(LOG_LEVEL_1, _LF_, "Method Called: dname=%s", dname);

  strcpy(dir_name, dname);
  debug_log(LOG_LEVEL_2, _LF_, "dir_name=%s", dir_name);
  numFile = scandir(dir_name, &namelist, only_nvm_data_files, my_alpha_sort);
  if (numFile < 0)
  {
    perror("scandir");
    exit(-1);
  }

  while (numFile--) {
    strcpy(file_names[i], namelist[numFile]->d_name);
    debug_log(LOG_LEVEL_2, _LF_, "file_name[%d]=%s", i, file_names[i]);
    free(namelist[numFile]);
    i++;
  }
  free(namelist);

  *num_files = i;
  debug_log(LOG_LEVEL_2, _LF_, "num_files=%d", *num_files);
  debug_log(LOG_LEVEL_1, _LF_, "Method End:");
  return SUCCESS;
}

//This function will merges all NVM data files only
static int  merge_data_files(char *data_file, unsigned long size_data_file, char *data_file_buf, int write_data_fd){
  int read_data_fd;
  long read_bytes, write_bytes;

  debug_log(LOG_LEVEL_1, _LF_, "Method Called: data_file=%s, size_data_file=%d, write_data_fd=%d", data_file, size_data_file, write_data_fd);
  if ((read_data_fd = open(data_file, O_RDONLY | O_LARGEFILE)) < 0){
    error_log(_LF_, "Error: Error in opening file (%s). Error = %s\n", data_file, strerror(errno));
    exit(-1);
  }

  read_bytes = nslb_read_file_and_fill_buf (read_data_fd, data_file_buf, size_data_file);
  //read_bytes = read(read_data_fd, data_file_buf, size_data_file);
  //data_file_buf[size_data_file] = '\0';
  close(read_data_fd);
  //debug_log(LOG_LEVEL_4, _LF_, "data_file_buf = %s", data_file_buf);
  
  write_bytes = write(write_data_fd, data_file_buf, size_data_file);
  debug_log(LOG_LEVEL_2, _LF_, "read_bytes, read_bytes = %ld, write_bytes = %ld", read_bytes, write_bytes);
  debug_log(LOG_LEVEL_1, _LF_, "End Method");
  return SUCCESS; 
}


//Here we will truncate the cp original file and merge the the truncate file into above merged nvm data file 
static int updated_cp_files(char *d_path, unsigned long data_malloc_buf_size, char *data_file_buf, int write_data_fd, unsigned int num_unused_records){

  //char cp_data_file[1024] = "tr069_cpe_data.dat";
  char cp_data_file[1024] = INITAL_CP_DATA_FILE;
  char cmd_buf[1024];
  int read_data_fd;
  //int count = 0;
  int ret; 
  //int len;
  struct stat stat_buf;
  long read_bytes, write_bytes;
  unsigned long unused_size = 0;
  unsigned long size_cp_data_file;
  FILE *cpe_data_ptr;
   
  debug_log(LOG_LEVEL_1, _LF_, "Method called: d_path = %s, data_malloc_buf_size = %lu, write_data_fd = %d",d_path, data_malloc_buf_size, write_data_fd);

  sprintf(cp_data_file, "%s/tr069_cpe_data.dat", d_path);
  debug_log(LOG_LEVEL_2, _LF_, "cp_data_file=%s", cp_data_file);

  sprintf(cmd_buf, "cp %s %s/tr069_cpe_data.dat.bk", cp_data_file, d_path);
  debug_log(LOG_LEVEL_2, _LF_, "Making backup of data file using cmd_buf=%s", cmd_buf);
  if((ret = system(cmd_buf)) != 0){
    error_log(_LF_, "Error: in runing the cmd_buf %s", cmd_buf);
  }
  
  //Now we are calculating the size of cpe data which is not used by nvm
 /* if ((read_data_fd = open(cp_data_file, O_RDONLY | O_LARGEFILE)) < 0){
    error_log(_LF_, "Error: Error in opening file (%s). Error = %s\n",cp_data_file, strerror(errno));
    exit(-1);
  }*/

  cpe_data_ptr = fopen(cp_data_file, "r");
  char *delim = "+";
  debug_log(LOG_LEVEL_2, _LF_, "cpe_data_ptr = %p, num_unused_records = %u", cpe_data_ptr, num_unused_records);
  unused_size = unused_data_size_from_file(cpe_data_ptr, num_unused_records, delim);
  //rewind(read_data_fd);
  debug_log(LOG_LEVEL_2, _LF_, "unused_size = %lu",unused_size);
  fclose(cpe_data_ptr);

  debug_log(LOG_LEVEL_2, _LF_, "Truncating data file (%s) to size = %d", cp_data_file, unused_size);
  if ((ret = truncate(cp_data_file, unused_size)) != 0) { //truncate original cp data file
    error_log(_LF_, "Error: Error in opening file (%s). Error = %s\n",cp_data_file, strerror(errno));
  }
 
  if (stat(cp_data_file, &stat_buf) == -1){
    error_log(_LF_, "cp data file %s does not exit.", cp_data_file);
    return SUCCESS;
  } /*else {
      if (stat_buf.st_size == 0){
        fprintf(stderr, "File %s is of zero size. Exiting.\n", cp_data_file);
        exit(-1);
      }
  }*/ 

  size_cp_data_file = stat_buf.st_size;
  
  debug_log(LOG_LEVEL_2, _LF_, "size_cp_data_file = %lu, data_malloc_buf_size = %lu", size_cp_data_file, data_malloc_buf_size);

  if (size_cp_data_file > data_malloc_buf_size){
    data_file_buf = (char *)realloc(data_file_buf, size_cp_data_file);
    if (data_file_buf == NULL)
    {
      error_log(_LF_, "Error: Out of memory.\n");
      exit(-1);
    }
  }
  if ((read_data_fd = open(cp_data_file, O_RDONLY | O_LARGEFILE)) < 0){
    error_log(_LF_, "Error: Error in opening file (%s). Error = %s\n",cp_data_file, strerror(errno));
    exit(-1);
  } else{ 
    read_bytes = nslb_read_file_and_fill_buf(read_data_fd, data_file_buf, size_cp_data_file);
    close(read_data_fd);
    debug_log(LOG_LEVEL_2, _LF_, "read_bytes = %ld", read_bytes);
  }   
  
  write_bytes = write(write_data_fd, data_file_buf, size_cp_data_file);
  debug_log(LOG_LEVEL_2, _LF_, "write_bytes=%ld", write_bytes);
  debug_log(LOG_LEVEL_1, _LF_, "End Method");
  return SUCCESS;
}


//Delete truncated cp data file after use and rename file cp data  
static void remove_and_rename_final_cp_files(char *d_path) { 
  char buf[1024];
  int ret;

  debug_log(LOG_LEVEL_1, _LF_, "Method called: d_path=%s", d_path);
  sprintf(buf, "rm -f %s/%s", d_path, INITAL_CP_DATA_FILE);
  if((ret = system(buf)) != 0){
    //printf("Error: cmd %s not success", buf);
    error_log(_LF_, "Error: cmd %s not success", buf);
  }
 
  sprintf(buf, "mv %s/%s %s/%s", d_path, FINAL_CP_DATA_FILE, d_path, INITAL_CP_DATA_FILE);
  if((ret = system(buf)) != 0){
    //printf("Error: cmd %s not success", buf);
    error_log(_LF_, "Error: cmd %s not success", buf);
  }

  debug_log(LOG_LEVEL_1, _LF_, "Method End");
}

/* This method will --
   1)Make backup copy of original cp data files with extension ".bk" 
   2)Then it will read the nvm data files which is stored in file_names[i]
   3)Write the read matter of step 2 in a new files "new_tr069_cpe_data.dat"
   4)At last it will truncate the original cp data file with given amount and append the truncated file into new files as generated in step 3
*/
static int update_nvm_data_files(char *d_path, char file_names[][500], unsigned int num_unused_records, int num_files){
  int i = 0;
  char *file_name;
  char data_file[4096];
  char updated_data_file_name[4096];
  unsigned long size_data_file = 0;
  struct stat stat_buf;
  char *data_file_buf = NULL;
  unsigned long data_malloc_buf_size = 0;
  int write_data_fd;  
  int return_status = 0;
  
  debug_log(LOG_LEVEL_1, _LF_, "Method Called: d_path=%s, num_unused_records=%lu, num_files=%d",d_path, num_unused_records , num_files);

  //sprintf(updated_data_file_name, "%s/%s", d_path, "new_tr069_cpe_data.dat");
  sprintf(updated_data_file_name, "%s/%s", d_path, FINAL_CP_DATA_FILE);
  if ((write_data_fd = open(updated_data_file_name, O_APPEND | O_RDWR | O_CREAT | O_LARGEFILE , (mode_t)0600)) < 0){
    error_log(_LF_, "Error: Error in opening file (%s). Error = %s\n", updated_data_file_name, strerror(errno));
    exit(-1); 
  }
  
  debug_log(LOG_LEVEL_1, _LF_, "Starting nvm files merging process.........");
  for(i = 0; i < num_files; i++){
    file_name = file_names[i];

    sprintf(data_file, "%s/%s.dat", d_path, file_name);//home/ctrainee/WORK_Manish_commit/cavisson/src/save/TR069/tr069_nvm_data0.dat
    
    debug_log(LOG_LEVEL_2, _LF_, "data_file=%s", data_file);
    if (stat(data_file, &stat_buf) == -1){
      error_log(_LF_, "Error: NVM data file %s does not exit.", data_file);
      exit(-1);
    }

    if (stat_buf.st_size == 0){
      error_log(_LF_, "Error: File %s is of zero size. Exiting.\n", data_file);
      exit(-1);
    }
    
    size_data_file = stat_buf.st_size;
    debug_log(LOG_LEVEL_2, _LF_, "size_data_file=%lu", size_data_file);
    

    if (data_malloc_buf_size && (size_data_file > data_malloc_buf_size)){
      debug_log(LOG_LEVEL_2, _LF_, "Freeing data_file_buf as new size (%lu) is > old size(%lu)", size_data_file, data_malloc_buf_size);

      debug_log(LOG_LEVEL_2, _LF_, "address of before free data_file_buf = %p", data_file_buf);
      free(data_file_buf);
      data_file_buf = NULL;
      data_malloc_buf_size = 0;
    }
    if (data_malloc_buf_size == 0) {
      debug_log(LOG_LEVEL_2, _LF_, "Malloc() for size_data_file=%lu", size_data_file);

      data_file_buf = (char *)malloc(size_data_file + 1);

      debug_log(LOG_LEVEL_2, _LF_, "address of atfer mallocked  data_file_buf = %p", data_file_buf);
      if (data_file_buf == NULL) {
        error_log(_LF_, "Error: Out of memory.\n");
        exit(-1);
      } 
      data_malloc_buf_size = size_data_file;
    }

    merge_data_files(data_file, size_data_file, data_file_buf, write_data_fd);
    if ((return_status = unlink(data_file)) != 0) {
      error_log(_LF_, "Error: in deleting nvm file (%s). Error = %s\n", data_file, strerror(errno));
    }
    
  }
  
  debug_log(LOG_LEVEL_1, _LF_, "All nvm files have merged.");
  debug_log(LOG_LEVEL_1, _LF_, "Starting cpe data merging");
  updated_cp_files(d_path, data_malloc_buf_size, data_file_buf, write_data_fd, num_unused_records);
  debug_log(LOG_LEVEL_2, _LF_, "cpe data merged.");
  close(write_data_fd);
  remove_and_rename_final_cp_files(d_path);
  debug_log(LOG_LEVEL_1, _LF_, "directory %s has updated.",d_path);
  debug_log(LOG_LEVEL_1, _LF_, "Method End:");
  return SUCCESS; 
}

void Usage(void)
{
  fprintf(stderr, "Usage: %s -d<dir_path> -r<num_unused_record>  -D<degug> -t<test_run_id>\n", progname);
  exit(1);
}


void Help(void)
{
  fprintf(stderr, "Usage: %s -d<dir_path> -r<num_unused_record> -D<degug> -t<test_run_id>\n", progname);
  fprintf(stderr, "\t-d\t input directory name of nvm data files\n");
  fprintf(stderr, "\t-s\t input unused number of cpe data records \n");
  fprintf(stderr, "\t-D\t debug level (1,2,3,4) \n");
  exit(1);
}

//from command line pass path of nvm files i.e. name name of directory with path(eg: /home/ctrainee/WORK_nikita/Manish/NVM_Files )
int main(int argc, char *argv[]){
  
  //char *file_names[500]; // Max NVM can be 250 or so
  char file_names[500][500]; // Max NVM can be 250 or so
  int opt;
  int num_files = 0;
  char dir_path[2048];
  unsigned int num_unused_records;
  int test_run_id = 0;
  progname = argv[0];
   
   while ((opt = getopt(argc, argv, "D:d:r:t:")) != -1) {
    switch (opt) {
      case 'D':
        debug_level = atoi(optarg);
        break;
      case 'd':
        strcpy(dir_path, optarg);
        break;
      case 'r':
        num_unused_records = atol(optarg);
        break;
      case 't':
        test_run_id = atoi(optarg);
        break;
      case ':':
        fprintf(stderr, "Error - Option `%c' needs a value\n\n", optopt);
        Help();
        break;
      default: 
        Usage();
        exit(EXIT_FAILURE);
    }
  }
              
  /*Get present NetStorm working directory*/
  printf("Calling getenv\n");
  if(getenv("NS_WDIR") != NULL){
    strcpy(ns_wdir_lol, getenv("NS_WDIR"));
    printf("ns_wdir_lol = %s\n", ns_wdir_lol);
  }
  else{
    strcpy(ns_wdir_lol, "/home/netstorm/work");
    fprintf(stderr, "Error in getting NS_WDIR. Setting to %s\n", ns_wdir_lol);
  }
  
  open_debug_log(test_run_id);                         
  open_error_log(test_run_id);                         
 
  debug_log(LOG_LEVEL_1, _LF_, "\nStarting ns_tr069_cp_data_post_processing.c :\n"
                     "\tDirectory Name = %s\n"
                     "\tUnused cpe records= %lu\n"
                     "\tDebug = %d",
                   dir_path, num_unused_records, debug_level);
 
  // Get list of all NVM data files
  get_nvm_data_files(dir_path, file_names, &num_files);

  update_nvm_data_files(dir_path, file_names, num_unused_records, num_files);
  
  if(debug_fp) {
    fclose(debug_fp);
  }
  
  if(error_fp) {
    fclose(error_fp);
  }

  return SUCCESS;
}
