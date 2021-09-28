/********************************************************************************
 * File Name            : nii_get_pct_dat.c
 * Author(s)            : Bhuvendra Bhardwaj
 * Date                 : 27 Sep 2013
 * Copyright            : (c) Cavisson Systems
 * Purpose              : This takes the pctMessage.dat file from TR<trnum> directories of all the generators, merges their data and                                     creates the aggregated file with the same name in the TR directory of the Controller. 
 * Usage                : nii_get_pct_data -t <contoller testrun> [-d <debug>] 
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
  ********************************************************************************/


#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "nslb_util.h"
#include "nslb_common.h"
#define NS_EXIT_VAR
#include "../../../ns_exit.h"

#define __FLN__  __FILE__, __LINE__, (char *)__FUNCTION__
#define LOG_NII_DEBUG if(g_debug_level) fprintf

static int g_tr_num;  // controller TR
static char * g_ns_wdir_lol; // working directory
static char g_tr_dir[512] = ""; // Directory of controller TR
char scen_file[512]; //path of shorted_scenario.conf file
FILE *scenario_fp;
static int g_debug_level = 0; // 1 for debug ON and 0 for OFF

long long int data_pkt_size; //size of data packet excluding header
int num_gen; //Number of generator TR
char nc_file[512] = ""; //contain the path of netcloud.data file
int req_seq = 1; //requested sequence number which is used to match with each generator TR, if matched merge the data

char gen_tr_idx[255][255];
char gen_name[255][255];

/*Function to validate Whether TR is Controller TR or not */
static inline void validate_controller_tr()
{
  struct stat f_stat;
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Called\n", __FLN__); 
  //validate if given tr is present or not
  sprintf(g_tr_dir, "%s/webapps/logs/TR%d", g_ns_wdir_lol, g_tr_num);
  if(stat(g_tr_dir, &f_stat)) {
    NS_EXIT(-1, "TR %d not present, try with differnt TR number\n", g_tr_num);
  }    
  //check whether NetCloud.data  file present or not in NetCloud folder
  sprintf(scen_file, "%s/sorted_scenario.conf", g_tr_dir);
  if(NULL == (scenario_fp = fopen(scen_file, "r"))) {
    NS_EXIT(-1, "Failed to open scenario file %s for TR %d, error:%s", scen_file, g_tr_num, nslb_strerror(errno));
  }
 
  //setting the path of Netcloud.data File 
  sprintf(nc_file,"%s/NetCloud/NetCloud.data",g_tr_dir);
  if(stat(nc_file, &f_stat)) {
    NS_EXIT(-1, "NetCloud.data file is not present in TR%d, try with differnt TR number", g_tr_num);
  }
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Exits\n", __FLN__); 
}

/*Function to validate Whether all generator TR are present or not */
static inline void validate_generator_tr()
{
  struct stat nc_size;
  struct stat gen_tr_stat;
  char gen_g_tr_dir[1024];  //buffer to save directory path of generator TR
  char *ptr = NULL;
  char ni_gen_flag = 0;
  char line[4 * 1024 + 1];
  //In order to extract generator name and test run number
  FILE* fp = NULL;
  int rnum = 0, i;
  char *field[20];

  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Called\n", __FLN__); 
  sprintf(scen_file, "%s/sorted_scenario.conf", g_tr_dir); //setting the path of sorted_scenario.conf file
  if(NULL == (scenario_fp = fopen(scen_file, "r"))) {
    NS_EXIT(-1, "Failed to open scenario file %s for TR %d, error:%s", scen_file, g_tr_num, nslb_strerror(errno));
  }
  //searching the keyword "NS_GET_GEN_TR" in sorted_scenario.conf file and checking whether it is "off" or "on"
  while(fgets(line, 4*1024, scenario_fp)) {
    if(!strncmp(line, "NS_GET_GEN_TR", 13)) {
      LOG_NII_DEBUG(stdout, "%s \n", line);
      ptr = line + 13;
      while ((*ptr == ' ') || (*ptr == '\t')) ptr++;
      LOG_NII_DEBUG(stdout, "ptr = %s \n", ptr);
      if(atoi(ptr)) {
        ni_gen_flag = 1;
        break;
      }
    }  
  }
  if(!ni_gen_flag){
    NS_EXIT(-1, "NS_GET_GEN_TR keyword not present in sorted_scenario file %s, try with different TR", scen_file);
  }

  //Checking the size of NetCloud.data file, it should contain atleast one line
  if(!stat(nc_file, &nc_size))
  {
    if(!nc_size.st_size) {
      NS_EXIT(-1, "NetCloud.data file is Empty");
    }
  }
  //Check whether file exists
  if ((fp = fopen(nc_file, "r")) == NULL)
  {
    NS_EXIT(-1, "NetCloud file (%s) does not exist.", nc_file);
  }

  while (fgets(line, 4*1024, fp) != NULL) 
  {
    if(!strncmp (line, "NETCLOUD_GENERATOR_TRUN ", strlen("NETCLOUD_GENERATOR_TRUN ")))
    {
      ptr = line;
      ptr = ptr + strlen("NETCLOUD_GENERATOR_TRUN ");
      get_tokens(ptr, field, "|", 20);   
      strcpy(gen_tr_idx[rnum], field[0]);
      strcpy(gen_name[rnum], field[1]);
      rnum++;
    }
  }
  //Fill number of generators in num_gen
  num_gen = rnum;
  // Checking each of Generator TR is present or not in controller
  LOG_NII_DEBUG(stdout, "Number of generator are: %d\n",num_gen);
  for(i =0; i<num_gen; i++)
  {
    sprintf(gen_g_tr_dir, "%s/logs/TR%d/NetCloud/%s/TR%s",g_ns_wdir_lol, g_tr_num, gen_name[i], gen_tr_idx[i]);
    if(stat(gen_g_tr_dir, &gen_tr_stat)) {
      NS_EXIT(-1, "Generated TR %s not present", gen_tr_idx[i]);
    }
    else LOG_NII_DEBUG(stdout, "Generated TR %s is present\n", gen_tr_idx[i]);
  }  
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Exits\n", __FLN__); 
}

/*Function to validate whether PERCENTILE keyword is on or not in scenerio */
static inline void validate_percentilekeyword()
{
  char *ptr = NULL;
  char ni_pct_flag = 0;
  char line[4 * 1024 + 1];

  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Called\n", __FLN__);
  while(fgets(line, 4*1024, scenario_fp)) {
    if(!strncmp(line, "PERCENTILE_REPORT", 17)) {
      ptr = line + 17;                             //incrementing pointer to check keyword is set or not
      while ((*ptr == ' ') || (*ptr == '\t')) ptr++;
      if(atoi(ptr)) {
        ni_pct_flag = 1;
        break;
      }
    }
  }
  if(!ni_pct_flag){
    NS_EXIT(-1, "PERCENTILE_REPORT keyword is not present in scenario file %s, try with different TR", scen_file);
  }
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Exit\n", __FLN__);  
}

/*Function to validate the data packet size in testrun.pdf is same or not for all Generated TR */
static inline void validate_data_pkt_size_in_testrun_pdf()
{
  char cmd[1024];
  char cmd_out[1024];

  long long int gen_tr_data_pkt_size;
  int i;

  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Called\n", __FLN__); 
  for(i = 0; i<num_gen; i++)
  {
    sprintf(cmd, "grep ^Info %s/logs/TR%d/NetCloud/%s/TR%s/testrun.pdf | cut -d '|' -f 4", g_ns_wdir_lol, g_tr_num, gen_name[i], gen_tr_idx[i]);
    int ret;
    ret = nslb_run_cmd_and_get_last_line(cmd, 1024, cmd_out);
    if(ret) {
      NS_EXIT(-1, "Fail: Error in run Command, cmd = '%s'" , cmd);
    }
    gen_tr_data_pkt_size = atoll(cmd_out);
    if(i == 0)
      data_pkt_size = gen_tr_data_pkt_size;
    else
    {
      if(data_pkt_size != gen_tr_data_pkt_size)
        break;
    }
  }
  if(i != num_gen)
  {
    NS_EXIT(-1, "The Data packet size of TR%s is not equal in size with others", gen_tr_idx[i]);
  }
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Exit\n", __FLN__);
}

/*Function To Get data from File */
static inline int get_file_data(FILE *fp_data, char *read_buff, long long data_read_size) 
{
  long long int read_bytes = 0;
  long long total_read = 0;

  //fprintf(stderr, "fp_data = %p\n", fp_data);
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Called\n", __FLN__);
  while(data_read_size){
    read_bytes = fread(read_buff + total_read, 1, data_read_size, fp_data);
    LOG_NII_DEBUG(stdout, "Read bytes---------------->%lld\n", read_bytes);
    if(read_bytes <= 0)
      break;
    data_read_size -= read_bytes;
    total_read += read_bytes;
  }
  //fprintf(stderr, "fp_data = %p\n", fp_data);
  

  // If any error on EOF find return -1
  if(read_bytes <= 0){
     LOG_NII_DEBUG(stdout ,"Error: Error in reading file.(%s)", nslb_strerror(errno));
     return -1;
  }
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Exit\n", __FLN__);
  return total_read;
}

#define SEQUENCE_IDX 0
#define HEADER_SIZE sizeof(pdf_data_hdr)
#define LONG_LONG_SIZE sizeof(long long)

/*Function to Combine percentile of all TR's  */
static void read_and_process_pct_data()
{
  int i,j;
  long long datasize = data_pkt_size - HEADER_SIZE;  //total data size 

  int tot_index = datasize / LONG_LONG_SIZE;

  FILE *pctmsg_fp[num_gen];
  FILE *cnt_fp;
  long long *tmp_ptr;
  long long *final_tmp_ptr;
  char file_name[1024];
  char get_eof[num_gen];


  long long *data = malloc(data_pkt_size); //create the memory block to read the data from generator pctMessagefile
  long long *final_data = malloc(data_pkt_size); //create the memory block to write the data from controller pctMessagefile

  char break_while_loop = 0;
  memset(data, 0, data_pkt_size);
  //memset(final_data, 0, LONG_LONG_SIZE * tot_index);
  
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Called\n", __FLN__);

  for(i = 0; i < num_gen; i++)
  {
    // open all generator files
    sprintf(file_name, "%s/logs/TR%d/NetCloud/%s/TR%s/pctMessage.dat",g_ns_wdir_lol, g_tr_num, gen_name[i], gen_tr_idx[i]);
    if(NULL == (pctmsg_fp[i] = fopen(file_name, "r"))) {
     NS_EXIT(-1, "Failed to open scenario file %s for TR %s, error:%s", file_name, gen_tr_idx[i], nslb_strerror(errno));
    }
  } 
  // open the controller pct file in write mode
  sprintf(file_name, "%s/logs/TR%d/pctMessage.dat",g_ns_wdir_lol,g_tr_num);
  if(NULL == (cnt_fp = fopen(file_name, "w"))) {
    NS_EXIT(-1, "Failed to open percentile data output file %s for writing, TR %d, error:%s", file_name, g_tr_num, nslb_strerror(errno));
  }

  LOG_NII_DEBUG(stdout, "Start Processing"); 
  while(!break_while_loop){
    break_while_loop = 1;
    memset(final_data, 0, data_pkt_size);

    for(i = 0; i < num_gen; i++)
    {
      get_eof[i] = 0;
      if(get_file_data(pctmsg_fp[i], (char *)data, data_pkt_size) == -1)
      {
        LOG_NII_DEBUG(stdout, "%s|%d|%s Got EOF for TR %s, going to close pctmsg_fp[%d] = %p\n", __FLN__, gen_tr_idx[i], i, pctmsg_fp[i]);
        get_eof[i] = 1;
      }
      else
        break_while_loop = 0;
      //case: when requested sequence is not matched with packet sequence no.
      if(data[SEQUENCE_IDX] != req_seq)
      {
        LOG_NII_DEBUG(stdout, "Case of non match seq no exist!!!!\n"); 
        LOG_NII_DEBUG(stdout, "%d:data[SEQUENCE_IDX]  = %lld, req_seq = %d\n", i, data[SEQUENCE_IDX], req_seq);
        //case: When first data packet is missing
        if(req_seq == 1){
          memset(data, 0, data_pkt_size);
          fseek(pctmsg_fp[i], -(data_pkt_size), SEEK_CUR);
        }
        else
        {
          //case: When data packet is missing at EOF
          if(get_eof[i] == 1)
            fseek(pctmsg_fp[i], -(data_pkt_size), SEEK_CUR);
          //case: When data packet is missing in middle
          else
            fseek(pctmsg_fp[i], -(2 * data_pkt_size), SEEK_CUR);
          if(get_file_data(pctmsg_fp[i], (char *)data, data_pkt_size) == -1){
            LOG_NII_DEBUG(stdout, "%s|%d|%s Got EOF for TR %s, going to close pctmsg_fp[%d] = %p\n", __FLN__, gen_tr_idx[i], i, pctmsg_fp[i]);
            get_eof[i] = 1;
          }
        }
      }
      LOG_NII_DEBUG(stdout, "%d:data[SEQUENCE_IDX]  = %lld, req_seq = %d\n", i, data[SEQUENCE_IDX], req_seq);
      tmp_ptr = data + HEADER_SIZE; //moving the pointer to data by incrementing it to by header size for reading
      final_tmp_ptr = final_data + HEADER_SIZE; //moving the pointer to data by incrementing it to by header size for writing
      // Added the the data packets
      for(j = 0; j < tot_index; j++)
      {
        final_tmp_ptr[j] += tmp_ptr[j]; 
      }
    }
 

    if(!break_while_loop) {
      LOG_NII_DEBUG(stdout, "process seq = %d break while loop = %d\n", req_seq, break_while_loop);
      final_data[SEQUENCE_IDX] = req_seq;
      fwrite(final_data, data_pkt_size, 1, cnt_fp); 
    }
    req_seq++;
  }
  fclose(cnt_fp);
  cnt_fp = NULL;
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Exit\n", __FLN__); 
}

//function To set END keyword with number of sequence of data in testrun.pdf of controller TR
static inline void edit_end_marker_in_controller_testrun_pdf()
{

  char cmd[1024];
  char cmd_out[1024];
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Called\n", __FLN__);
  sprintf(cmd, "sed -i 's/END|[0-9]*/END\\|%d/g' %s/logs/TR%d/testrun.pdf", req_seq - 2, g_ns_wdir_lol, g_tr_num);
  LOG_NII_DEBUG(stderr, "%s|%d|%s|running command %s", __FLN__, cmd);
  if(nslb_run_cmd_and_get_last_line(cmd, 1024, cmd_out)) {
    //LOG_NII_DEBUG(stderr, "Fail: Error in run Command, cmd = '%s'" , cmd);
    NS_EXIT(-1, "Fail: Error in run Command, cmd = '%s'" , cmd);
  }
  LOG_NII_DEBUG(stdout, "%s|%d|%s Method Exit\n", __FLN__);
}

//Function to validate that pctMessage.dat file of generator TR's should be in multiple of data packet size
static inline void validate_pct_message_file()
{
  int i;
  struct stat s;
  char file_name[1024];
  LOG_NII_DEBUG(stdout, "%s|%d|%s|Method Called\n", __FLN__);

  for(i = 1; i < num_gen; i++)
  {
    sprintf(file_name, "%s/logs/TR%d/NetCloud/%s/TR%s/pctMessage.dat",g_ns_wdir_lol, g_tr_num, gen_name[i], gen_tr_idx[i]);
    if(!stat(file_name, &s))
    {
      if(s.st_size%data_pkt_size != 0)
      {
        NS_EXIT(-1, "pctMessage.dat file in not proper format for Generator TR%s, at path '%s'.", gen_tr_idx[i], file_name);
      }
    }
    else {
      NS_EXIT(-1, "pctMessage.dat file does not present for Generator TR%s, at path '%s'.", gen_tr_idx[i], file_name);
    }
  }
  LOG_NII_DEBUG(stdout, "%s|%d|%s|Method Exit\n", __FLN__);
}

#define PRINT_USAGES_AND_EXIT() fprintf(stderr, "usages: tool_name --testrun <test run number> [--debug <debug level>]\n");exit(-1);
int main(int argc, char **argv)
{
  
  //parse input
  char c;
  char tr_flag = 0;
  struct option longopts[] = {
                               {"testrun", 1, NULL, 't'},
                               {"debug",  1, NULL, 'd'},
                               {0, 0, 0,0}
                             };
  while ((c = getopt_long(argc, argv, "t:d:", longopts, NULL)) != -1)
  {
    switch (c) {
      case 't':
        g_tr_num = atoi(optarg);
        if(g_tr_num <= 0) {
          fprintf(stderr, "Invalid value of --testrun\n");
          PRINT_USAGES_AND_EXIT();
        }
        tr_flag = 1;
        break;
      case 'd':
        g_debug_level = atoi(optarg);
        break;
      default:
        fprintf(stderr, "Invalid argument\n");
        PRINT_USAGES_AND_EXIT();
    }
  }
  if(!tr_flag) {
    fprintf(stderr, "--testrun argument missing\n");
    PRINT_USAGES_AND_EXIT();
  }
  
  if(NULL == (g_ns_wdir_lol = getenv("NS_WDIR"))){
    NS_EXIT(-1, "Environment variable \'NS_WDIR\' not defined");
  }
  LOG_NII_DEBUG(stdout, "NS_GET_GEN_TR = %s", g_ns_wdir_lol);
  validate_controller_tr();  
  validate_percentilekeyword(); 
  validate_generator_tr();
  validate_data_pkt_size_in_testrun_pdf();
  validate_pct_message_file();
  read_and_process_pct_data();
  edit_end_marker_in_controller_testrun_pdf();
  return 0; 
}
