
#include "ns_tr069_includes.h"
#include "../../libnscore/nslb_util.h"
#include "ns_tr069_data_file.h"

unsigned int *idx_table_ptr = NULL;
//unsigned int tr069_total_idx_entries = 0;
unsigned int tr069_total_data_count = 0;
tr069_user_cpe_data_t *tr069_user_cpe_data = NULL;

/*Function returns number of lines in a file*/
//file is with absolute file path
unsigned int get_data_count(char *file)
{
  struct stat stat_buf;
  int fd;
  char fbuf[MAX_LINE_LENGTH+1];
  int byte_read;
  unsigned int count;
  
  NSDL2_TR069(NULL, NULL, "Method called, File = %s", file);

  if(stat(file, &stat_buf) == -1)
  {
    fprintf(stderr, "File %s does not exists. Exiting.\n", file);
    NSDL2_TR069(NULL,NULL, "File %s does not exists\n", file);
    return 0;
  } else {
    if(stat_buf.st_size == 0){
      fprintf(stderr, "File %s is of zero size. Exiting.\n", file);
      exit(-1);
    }
  }
  fd = open(file, O_RDONLY);
  byte_read = read(fd, fbuf, MAX_LINE_LENGTH);
  fbuf[byte_read] = '\0';
  close(fd);
  if(!ns_is_numeric(fbuf))  
    count = atoi(fbuf);
  else{
    fprintf(stderr, "file content = %s is not numeric\n", fbuf);
    exit(-1);
    }
  NSDL2_TR069(NULL,NULL, "count = %d", count);
  return count;
}

#if 0
unsigned int get_number_of_lines(char *file)
{
  struct stat stat_buf;
  int fd;
  unsigned int total_lines = 0;
  char *ptr = NULL;
  char fbuf[4096 + 1];
  unsigned int bytes_to_read;
  int bytes_read;
 
  NSDL2_TR069(NULL, NULL, "Method called, File = %s", file);

  if (stat(file, &stat_buf) == -1) {
    fprintf(stderr, "File %s does not exists. Exiting.\n", file);
    NSDL2_TR069(NULL,NULL, "File %s does not exists\n", file);
    return 0;
    //exit(-1);
  } else {
    if (stat_buf.st_size == 0) {
      fprintf(stderr, "File %s is of zero size. Exiting.\n", file);
      exit(-1);
    }
  }
 
  fd = open(file, O_RDONLY);
  bytes_to_read = stat_buf.st_size;
  while(bytes_to_read){
    bytes_read = read(fd, fbuf, 4096);
    fbuf[bytes_read] = '\0';
    NSDL4_TR069(NULL, NULL, "File buf = %s", fbuf);
    ptr = fbuf;
    while(*ptr){
      if(*ptr == '\n'){
        total_lines++;
      }
      ptr++; 
    }
    bytes_to_read -= bytes_read;
  }
  close(fd);
  
  fprintf(stderr, "Total lines in the file = %d\n", total_lines);
  NSDL2_TR069(NULL, NULL, "total lines = %u", total_lines);
  return total_lines;
}
#endif

static void divide_data_btw_nvm (unsigned int tr069_total_data_count)
{
  int nvm_idx;
  unsigned int next_idx = 0;

  RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;
  NSDL2_TR069(NULL, NULL, "Method called, total entries = %u", tr069_total_data_count);
  
  tr069_user_cpe_data = (tr069_user_cpe_data_t*) do_shmget(sizeof(tr069_user_cpe_data_t) * global_settings->num_process, "tr069_user_cpe_data");
  for (nvm_idx = 0; nvm_idx < global_settings->num_process; nvm_idx++) {
    if(nvm_idx == 0){ //First index is the total entries 
      //tr069_user_cpe_data[nvm_idx].start_offset = idx_table_ptr[(tr069_total_idx_entries - rstart->quantity)];
      tr069_user_cpe_data[nvm_idx].start_idx = (tr069_total_data_count - rstart->quantity);
      tr069_user_cpe_data[nvm_idx].total_entries = v_port_table[nvm_idx].num_vusers; 
    }
    else
    {
      // tr069_user_cpe_data[nvm_idx].start_offset = idx_table_ptr[next_idx];
      tr069_user_cpe_data[nvm_idx].start_idx = next_idx;
      tr069_user_cpe_data[nvm_idx].total_entries = v_port_table[nvm_idx].num_vusers; 
    }
    next_idx = tr069_user_cpe_data[nvm_idx].start_idx + tr069_user_cpe_data[nvm_idx].total_entries;
    fprintf(stderr, "NVM = %d, Start idx = %u, total data entries = %u\n", nvm_idx, tr069_user_cpe_data[nvm_idx].start_idx, tr069_user_cpe_data[nvm_idx].total_entries);
    NSDL2_TR069(NULL, NULL, "NVM = %d, Start idx = %u, total data entries = %u, next_idx = %u", nvm_idx, tr069_user_cpe_data[nvm_idx].start_idx, tr069_user_cpe_data[nvm_idx].total_entries, next_idx);
    //NSDL2_TR069(NULL, NULL, "NVM = %d, Start idx = %u, total users = %d, next_idx = %u", nvm_idx, tr069_user_cpe_data[nvm_idx].start_idx, v_port_table[nvm_idx].num_vusers, next_idx);
  }
  NSDL2_TR069(NULL, NULL, "tr069_total_data_count = %d", tr069_total_data_count);
  //TODO: After this do we need idx_table_ptr ?? 
}

#define NS_TR069_DATA_COUNT_PATH_FILE "tr069_cpe_data.dat.count"

void tr069_read_count_file()
{
  FILE *count_file_fp = NULL;
  //char line[MAX_LINE_LENGTH];
  //int idx = 0;
  //char cmd_out[256];
  //char *ptr = NULL;
  //char cmd_buf[4096];
  //unsigned int tr069_total_idx_entries = 0;
  char count_file[4096];


  RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;

  NSDL2_TR069(NULL, NULL, "Method called");
  
  sprintf(count_file, "%s/%s", global_settings->tr069_data_dir, NS_TR069_DATA_COUNT_PATH_FILE);
  
  NSDL2_TR069(NULL, NULL, "IDX file = %s", count_file);

  count_file_fp = fopen(count_file, "r");
  if(count_file_fp == NULL)
  {
    fprintf(stderr, "Error in opening file = %s\n", count_file);
    END_TEST_RUN  
  }
  
  NSDL2_TR069(NULL, NULL, "File (%s) opened successfully", count_file);
  /*
  //Read total number of lines in file
  sprintf(cmd_buf, "cat %s | wc -l", idx_file);
  NSDL2_TR069(NULL, NULL, "Command to run = %s", cmd_buf);

  memset(cmd_out, 0, 4096);
  if(run_cmd_read_last_line(cmd_buf, 4, cmd_out) == 1)
  { 
    fprintf(stderr, "Error in running command = %s\n", cmd_buf);
    exit(-1);
  }
 
  NSDL2_TR069(NULL, NULL, "Command output = %s", cmd_out);

  //cmd_out will have <567 sample_file>
  //Remove space from the cmd_out 
  ptr = strchr(cmd_out, ' '); 
  if(ptr != NULL){
    *ptr = '\0';
  }
  */
  
  //Read .count file instead of reading lines from file
  tr069_total_data_count = get_data_count(count_file);
  //tr069_total_idx_entries = get_number_of_lines(idx_file);
  //tr069_total_idx_entries = atoi(cmd_out);  
  
  NSDL2_TR069(NULL, NULL, "Total entries (%d) in file = %s", tr069_total_data_count, count_file);
  if(tr069_total_data_count == 0){
    fprintf(stderr, "No data in file (%s).\n", count_file);
    return;
  }
  //Check for total users, If total entries in file
  //are less than the total users then give error.
  if(tr069_total_data_count < rstart->quantity)
  {
    fprintf(stderr, "Total data segments (%d) is less than the total numbers of users(%d).", tr069_total_data_count, rstart->quantity);
    exit(-1);
  }
  
#if 0
  MY_MALLOC(idx_table_ptr, (tr069_total_idx_entries * sizeof(int)), "idx_table_ptr", -1); 
  while (nslb_fgets(line, MAX_LINE_LENGTH, idx_file_fp, 0)) {
    idx_table_ptr[idx] = atoi(line);
    idx++;
  }
  NSDL2_TR069(NULL, NULL, "IDX = %d", idx);
#endif
  fclose(count_file_fp);
  divide_data_btw_nvm(tr069_total_data_count);
}


#if NS_DEBUG_ON
void dump_nvm_data ()
{
  int nvm_idx;

  NSDL2_TR069(NULL, NULL, "Method called");
  for (nvm_idx = 0; nvm_idx < global_settings->num_process; nvm_idx++) {
    NSDL2_TR069(NULL, NULL, "NVM = %d, Start idx = %d, total entries = %d", tr069_user_cpe_data[nvm_idx].start_idx, tr069_user_cpe_data[nvm_idx].total_entries);
  }
}
#endif
