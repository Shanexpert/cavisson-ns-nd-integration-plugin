
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
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <libgen.h>

#include "nslb_util.h"
#include "netomni/src/core/ni_script_parse.h"
#include "ns_error_msg.h"
#include "ns_exit.h"

#define MAX_LINE_LEN 4*1024
void nslb_uo_get_ctrl_file_name(char *data_fname, char *ctrl_file, int gen_id)
{
  char locl_data_fname[5 * 1024]; 
  char locl_data_fname2[5 * 1024]; 

  //NSLBDL2_API("Method called. Data file name = %s, gen_id", data_fname, gen_id);

  locl_data_fname[0] = '\0';
  locl_data_fname2[0] = '\0';

  strcpy(locl_data_fname, data_fname);
  strcpy(locl_data_fname2, data_fname);
   
  sprintf(ctrl_file, "%s/.%s.%d.control", dirname(locl_data_fname), basename(locl_data_fname2), gen_id);
  
  //NSLBDL2_API("Control file name = %s", ctrl_file);
}

void nslb_uo_get_gen_ctrl_file_name(char *data_fname, char *ctrl_file)
{
  char locl_data_fname[5 * 1024]; 
  char locl_data_fname2[5 * 1024]; 

  //NSLBDL2_API("Method called. Data file name = %s", data_fname);

  locl_data_fname[0] = '\0';
  locl_data_fname2[0] = '\0';

  strcpy(locl_data_fname, data_fname);
  strcpy(locl_data_fname2, data_fname);
   
  sprintf(ctrl_file, "%s/.%s.gen.control", dirname(locl_data_fname), basename(locl_data_fname2));
  
  //NSLBDL2_API("Generator Control file name = %s", ctrl_file);
}

void nslb_uo_get_last_file_name(char *data_fname, char *last_file, int nvm_idx, int gen_idx)
{
  char locl_data_fname[5 * 1024]; 
  char locl_data_fname2[5 * 1024];   
  int child_idx = ((gen_idx > 0)?(gen_idx << 8):0) + nvm_idx; 

  //NSLBDL2_API("Method called. Data file name = %s", data_fname);

  strcpy(locl_data_fname, data_fname);
  strcpy(locl_data_fname2, data_fname);
   
  sprintf(last_file, "%s/.%s.%d.last", dirname(locl_data_fname), basename(locl_data_fname2), child_idx);
  
  //NSLBDL2_API("Last file name = %s", last_file);
}

static void read_control_file (FILE *fp_ctrl, int *num_nvm, int *frequency, int *total_values_per_nvm, char *data_fname, char *ctrl_file)
{
  char line[MAX_LINE_LEN];
  char *fields[255];
  char *fields_values[255];
  int total_fields = 0; 
  int second_data_line = 0;
  int i;


  //NSLBDL2_API("Method called");

  // Control file is present so extract NUM_NVM and frequency 
  while(nslb_fgets(line, MAX_LINE_LEN, fp_ctrl, 0) != NULL) {
    if(line[0] == '#' || line[0] == '\n')
     continue;
    
    if(second_data_line == 0)
    {
      total_fields = get_tokens(line, fields, "|", 255);
      if(total_fields < 3) {
        NS_EXIT(-1, CAV_ERR_1013028, data_fname, ctrl_file); 
      }
      second_data_line = 1;
      *num_nvm = atoi(fields[1]);
      *frequency = atoi(fields[2]);
    }
    else
    {
      total_fields = get_tokens(line, fields, "|", 255);
      if(total_fields != *num_nvm) 
      {
        NS_EXIT(-1, CAV_ERR_1013029, data_fname, ctrl_file); 
      }
      /*Total fields = total NVM*/
      for(i = 0; i < total_fields; i++)
      {
        get_tokens(fields[i], fields_values, ",", 255); 
        total_values_per_nvm[i] = atoi(fields_values[1]);  
        //printf("total_values_per_nvm[%d] = %d\n", i, total_values_per_nvm[i]);
      }
    }
  }

  //NSLBDL2_API("Total nvm = %d, Updation Frequency = %d", *num_nvm, *frequency);

  return;
}


//This function is same as read_control_file.
static void read_gen_control_file (FILE *fp_ctrl, int *num_gen, int *frequency, int *total_values_per_gen, char *data_fname, char *ctrl_file)
{
  char line[MAX_LINE_LEN];
  char *fields[255];
  char *fields_values[255];
  int total_fields = 0; 
  int second_data_line = 0;
  int i;


  //NSLBDL2_API("Method called");

  // Control file is present so extract NUM_GEN and frequency 
  while(nslb_fgets(line, MAX_LINE_LEN, fp_ctrl, 0) != NULL) {
    if(line[0] == '#' || line[0] == '\n')
     continue;
    
    if(second_data_line == 0)
    {
      total_fields = get_tokens(line, fields, "|", 255);
      if(total_fields < 3) {
        NS_EXIT(-1, CAV_ERR_1013028, data_fname, ctrl_file); 
      }
      second_data_line = 1;
      *num_gen = atoi(fields[1]);
      *frequency = atoi(fields[2]);
    }
    else
    {
      total_fields = get_tokens(line, fields, "|", 255);
      if(total_fields != *num_gen) 
      {
        NS_EXIT(-1, CAV_ERR_1013029, data_fname, ctrl_file); 
      }
      /*Total fields = total GEN*/
      for(i = 0; i < total_fields; i++)
      {
        get_tokens(fields[i], fields_values, ",", 255); 
        total_values_per_gen[i] = atoi(fields_values[1]);  
        //printf("total_values_per_gen[%d] = %d\n", i, total_values_per_gen[i]);
      }
    }
  }

  //NSLBDL2_API("Total generator = %d, Updation Frequency = %d", *num_gen, *frequency);

  return;
}

// This common method has been created for opening file in various mode  
// resolving the issue of root permisson for mantis bug 165,
// If file pointer returns NULL, the abort the test
static int open_file(FILE **fp, char *file, char *mode, int abrt_test)
{
  //NSLBDL1_API("Method called, file name = %s, mode = %s, abrt_test = %d", file, mode, abrt_test);
  *fp = fopen(file, mode);
  if(*fp == NULL){
    if(abrt_test)
    {
      NS_EXIT(-1, CAV_ERR_1000006, file, errno, nslb_strerror(errno)); 
    }
      //END_TEST_RUN;
    return 1;
  }
  return 0;
}

/*This function will return start index of data file*/
static void read_last_data_file(char *data_file, int nvm_idx, int *start_idx, int *unused_val, int gen_idx, int remove)
{
  char last_file_name[5 * 1024];
  FILE *last_fp = NULL;
  char line[MAX_LINE_LEN];
  char *fields[10];
  int total_fields;  

  //NSLBDL2_API("Method called, data file = %s, nvm = %d, gen_idx = %d", data_file, nvm_idx, gen_idx);
 
  nslb_uo_get_last_file_name(data_file, last_file_name, nvm_idx, gen_idx);

  last_fp = fopen(last_file_name, "r");

  if(last_fp == NULL) {
    //fprintf(stderr, "Error: Unable to open last file '%s' for reading.\n", last_file_name);
    return;
  }
  
  while(nslb_fgets(line, MAX_LINE_LEN, last_fp, 0) != NULL) {
    if(line[0] == '#' || line[0] == '\n')
     continue;
    
    total_fields = get_tokens(line, fields, "|", 10);
    if(total_fields != 4) {
      NS_EXIT(-1, CAV_ERR_1013030, data_file, last_file_name);
    }
   
    *start_idx = atoi(fields[0]);
    *unused_val = atoi(fields[1]);
    //*total_val = atoi(fields[2]);
    //NSLBDL2_API("start_idx = %d, unused_val = %d", *start_idx, *unused_val);
    break; //Bcoz we need only first line
  }
  fclose(last_fp);
  if(remove)   
    unlink(last_file_name);
}

/*This function will create the data file based on start val and unused values.*/
void nslb_uo_create_data_file (int start_val, int num_val_remaining, int total_val, FILE *org_data_fp, FILE *used_data_fp, FILE *unused_data_fp, int *line_number, int *num_hdr_line)
{
  //int total_val = start_val + num_val_remaining;
  char line[MAX_LINE_LEN];
  
  //NSLBDL2_API("Method Called, start_val = %d, num_val_remaining = %d, total_val = %d, number of header line = %d, line num = %d",
    //           start_val, num_val_remaining, total_val, *num_hdr_line, *line_number);

  line[MAX_LINE_LEN - 2] = '\0';
  while(nslb_fgets(line, MAX_LINE_LEN, org_data_fp, 0) != NULL)
  {
    if(*num_hdr_line)
    {
      //NSLBDL2_API("This is header line, Line = %s", line);
      if(unused_data_fp != NULL)
       fprintf(unused_data_fp, "%s", line);
      (*num_hdr_line)--;
      continue;
    }  
    if(line[0] == '#' || line[0] == '\n')
      continue;

    if(*line_number < start_val)
    {
      /*This data is used*/
      //NSLBDL2_API("This is used data, data = %s", line);
      fprintf(used_data_fp, "%s", line);
    }
    else
    {
      //NSLBDL2_API("This is unused data = %s", line);
      /*Unused lines should be write into .unused files, if UseOnceWithInTest=NO, 
        if it is YES, then <data_file>.unused file is not created. */
      if(unused_data_fp != NULL)
        fprintf(unused_data_fp, "%s", line);
    }
    //Ayush If line is more than MAX_LINE_LEN then will continue 
    if (line[MAX_LINE_LEN - 2] != '\0' && line[MAX_LINE_LEN - 2] != '\n')
    {
      line[MAX_LINE_LEN - 2] = '\0';
      continue;
    }
    (*line_number)++; 
    //NSLBDL2_API("line_number = %d, total_val = %d", *line_number, total_val);
    if(*line_number == total_val)//This is for next NVM
      return;
  } //While
}

//This function will return 
//0: OK
//-1: error 
//-2: value exhausted, end_test_run
//TODO: save error message to output
int nslb_uo_create_data_file_frm_last_file(char *data_fname, int total_hdr_line, char *error_msg, int api_idx)
{
  int nvm_idx;
  int start_idx = 0;
  int unused_val = 0;
  int line_number;
  FILE *org_data_fp = NULL;
  FILE *unused_data_fp = NULL;
  FILE *used_data_fp = NULL;
  char unused_data_file[5 * 1024];
  char used_data_file[5 * 1024];
  char ctrl_file[5 * 1024];
  FILE *ctrl_fp = NULL;
  int num_nvm, frequency = -1;
  int prev_total_val_nvm;
  int total_val;
  int cur_total_values_per_nvm[255];
  int num_hdr_line = total_hdr_line; 
  char gen_ctrl_file[1024]; 
  FILE *gen_ctrl_fp;
  int num_gen = 1;  //Case of non nc mode.
  int gen_idx = 0;
  int cur_total_values_per_gen[255];
  int prev_total_val_gen = 0;
  int nc_mode = 0;

#define CLOSE_FP_AND_RETURN() \
  if((org_data_fp && used_data_fp && unused_data_fp)) { \
  fclose(org_data_fp);  \
  fclose(used_data_fp); \
  fclose(unused_data_fp);\
  } \
  return -1;

 
  //NSLBDL2_API("Method called, data file name = %s", data_fname);
  //first check for control file for generator.
  nslb_uo_get_gen_ctrl_file_name(data_fname, gen_ctrl_file);
  gen_ctrl_fp = fopen(gen_ctrl_file, "r");
  if(gen_ctrl_fp == NULL)
  {
    //NSLBDL2_API("Generator Control file not found, Assuming non nc mode");
  }
  else {
    //NSLBDL2_API("Found Generator control file. File = %s", ctrl_file);
    nc_mode = 1;
    //TODO: currently we are taking file format same as nvm control file.
    read_gen_control_file(gen_ctrl_fp, &num_gen, &frequency, cur_total_values_per_gen, data_fname, gen_ctrl_file);  
    fclose(gen_ctrl_fp);  
    unlink(gen_ctrl_file);
  }
 
  sprintf(unused_data_file, "%s.unused", data_fname);
  if(!strcasecmp(api_table[api_idx].UseOnceWithinTest, "YES"))
    sprintf(used_data_file, "%s.used", api_table[api_idx].UsedFilePath); 
  else
    sprintf(used_data_file, "%s.used", data_fname);

  open_file(&org_data_fp, data_fname, "r", 1);  
  open_file(&used_data_fp, used_data_file, "w", 1);
  if(!strcasecmp(api_table[api_idx].UseOnceWithinTest, "NO"))
    open_file(&unused_data_fp, unused_data_file, "w", 1);
    
  line_number = 0;
  prev_total_val_nvm = 0;
  prev_total_val_gen = 0; 

  for(gen_idx = 0; gen_idx < num_gen; gen_idx++)
  {
    /*Some generator may not get data so if control file not found for some generator then ignore that.*/
    if(nc_mode == 1 && cur_total_values_per_gen[gen_idx] == 0){
      continue;
    }

    /*Fisrst check for control file
     *If NOT there, dnt do any thing just continue till all group processed and return
     *If YES, create data file from last file */
    nslb_uo_get_ctrl_file_name(data_fname, ctrl_file, gen_idx);
    ctrl_fp = fopen(ctrl_file, "r");
    if(ctrl_fp == NULL)
    {
      //NSLBDL2_API("Control file %s not found, continuing with original data file.", ctrl_file);
      /*Control file is not there. Just return*/
      CLOSE_FP_AND_RETURN(); 
    }
    else
    {
      //NSLBDL2_API("Found control file. File = %s", ctrl_file);
      /*Read control file*/
      read_control_file (ctrl_fp, &num_nvm, &frequency, cur_total_values_per_nvm,  data_fname, ctrl_file);
      fclose(ctrl_fp);
      unlink(ctrl_file);
    }
     
    prev_total_val_nvm = 0; //reset 
    //now handle for nvms.
    for(nvm_idx = 0; nvm_idx < num_nvm; nvm_idx++)
    {
      start_idx = -1; //to handle case when last file not found.
      read_last_data_file(data_fname, nvm_idx, &start_idx, &unused_val, gen_idx, 1);

      if(start_idx == -1)
      { 
        /*This is case when we dont have any thing in last file.(For example: if we have updateion freq = 50
        *but we have used only 10 values and netstorm stoped, at this we will not have any data in last file.
        *Now, we will total values will be start idx, doing this we will have start idx for next NVM.)*/
        start_idx = prev_total_val_gen + prev_total_val_nvm; 
      }
      else {
        //NOTE: this start_idx will be relative to that perticular generator. 
        //we need to get as per total generator.
        //Resolve Bug 19574 : Data coming in .used file is not correct in case of File parameter with USE_ONCE mode.
        //start_idx += prev_total_val_gen;
        start_idx = start_idx + prev_total_val_gen + prev_total_val_nvm;
      }

      /*Now adjust the start idx
       * Here we will increment start idx by the frequency.
       * There may be chances that we are in middle of execution and test run is terminated.
       * In this case did not write the last file but we used the values. SO we just incrementing 
       * start_idx by the frequency*/
      total_val = prev_total_val_gen + prev_total_val_nvm + cur_total_values_per_nvm[nvm_idx];
      //It may be possibel that some NVMs did not get data for use once 
      //This is case when you have taken only (for exapmle) 4 users for USE_ONCE script but
      //in your test Netstorm is running more than 4 NVMs, then USE_ONCE data will be given to only
      //4 NVMs other NVMs will not have data. So, it was calculating values wrongly.
      //FIX: Now we will ignore the NVMs which dont have data
      if (cur_total_values_per_nvm[nvm_idx] == 0)
        continue;

      if(frequency == -1)
      {
        /*This is case when we were writing last data file every time.
         * previously, just skip current start_idx coz this is used data.
         * Bug 60333: currently, using start_idx as limit of used data and beginning of
           unused data as line_number starts from 0, setting frequency same 0 */
        //frequency = 1;
        frequency = 0;   
      }

      if((start_idx + frequency) > total_val)
      {
        sprintf(error_msg, "Start_idx = %d, Frequecy = %d, total_val = %d NVM = %d\nAll values of file parameter using mode 'Use Once' are exhausted. Data file name is %s. Test run cannot continue. Aborting ...", start_idx, frequency, total_val, nvm_idx, data_fname);
        //fprintf(stderr, "%s", error_msg);
        return -2;
      }
      start_idx += frequency;
      //NSLBDL2_API("start_idx = %d, frequecy = %d, total_val = %d nvm_id = %d gen_id = %d, line_number = %d",
                       //start_idx, frequency, total_val, nvm_idx, gen_idx, line_number);
      prev_total_val_nvm += cur_total_values_per_nvm[nvm_idx];
      nslb_uo_create_data_file (start_idx, unused_val, total_val, org_data_fp, used_data_fp, unused_data_fp, &line_number, &num_hdr_line);
    }   
    prev_total_val_gen += cur_total_values_per_gen[gen_idx];
  } 
  fclose(used_data_fp);
  if(unused_data_fp)
  fclose(unused_data_fp);
  fclose(org_data_fp);

  /*Now delete original data file
   *and rename used file as data file */
  if(!strcasecmp(api_table[api_idx].UseOnceWithinTest, "NO")){
    //NSLBDL2_API("Moving %s to %s", unused_data_file, data_fname);
    rename(unused_data_file, data_fname);
  }
  return 0;
}

