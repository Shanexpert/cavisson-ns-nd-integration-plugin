#ifndef DL_COLL
#define DL_COLL

#define DELTA_GENERATOR_ENTRIES 32
#define MAX_PCT_TOKENS  3

typedef struct{
  int fd;               //connection fd to the generators
  union{
    char *buf;    //Buffer to keep partial data
    char *array[MAX_PCT_TOKENS];  //to have pctMessage data.
  }partial_buf;
  //If difference of these will be MAX_PCT_TOKENS that means all buffer are occupied.
  int pct_buf_startidx; //point to first used buffer.
  int pct_buf_endidx; //point to end used buffer.
  int read_amount;      //to keep track of how much data was read  
  
  char data_file_path[1024];
  int gen_idx;
  long long offset;
  int cmon_session_fd;  //Just to check file size on generator.
  int raw_data_fd;
  int pct_msg_data_fd;
}GenData;


typedef struct{
  char gen_ip[128];
  char gen_name[512];
  char work_dir[512];
  int tr_num;
  int cmon_port;
}UsedGen;

typedef struct{
  long long partition_idx;
  long long *offset;
}DataOffsetInfo;

#endif
