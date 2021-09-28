#include "ns_msg_def.h"

#define _NLF_                      __FILE__, __LINE__, (char *)__FUNCTION__ 

#define DLOG_L1                    0x000F
#define DLOG_L2                    0x00F0
#define DLOG_L3                    0x0F00
#define DLOG_L4                    0xF000

#define DEFAULT_TIME_OUT	   10
#define MAX_TR_LIST		   16
#define MAX_DATA_BUF		   1024
#define MAX_LINE_LENGTH     	   1024
#define PORT_LEN		   5
#define MAX_PATH_LEN		   256
#define MAX_DATA_NUM		   41

#define INIT_TIME		   0
#define RUNTIME			   1

#define _NLF_                      __FILE__, __LINE__, (char *)__FUNCTION__

//Debug log masking
#define DLOG_L1                    0x000F
#define DLOG_L2                    0x00F0
#define DLOG_L3                    0x0F00
#define DLOG_L4                    0xF000

#define READ_SUCCESSFUL		   1
#define CONNECTION_CLOSED	   0
#define READ_ERROR		   -1

#define NAN_VALUE		   0.0/0.0

typedef struct TestTrafficInfo
{
  int test_run;
  int con_fd;
  int progress_interval;
  char data_filled;
  TestTrafficStatsRes tts_data;
  struct TestTrafficInfo *next;
}TestTrafficInfo;

