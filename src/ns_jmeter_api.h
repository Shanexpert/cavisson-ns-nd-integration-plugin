#ifndef __ns_jmeter_api_h
#define __ns_jmeter_api_h


#define JMETER_MAX_CMD_LEN             32768   //32 *1024
#define JMETER_MAX_JTL_FNAME_LEN       1024

#define JMETER_EXE_NAME                "jmeter"

#define JMETER_NS_LOGS_PATH \
  char ns_logs_file_path[256 + 1]; \
  if(vptr->partition_idx <= 0) { \
    sprintf(ns_logs_file_path, "TR%d/ns_logs", testidx); \
  } \
  else { \
    sprintf(ns_logs_file_path, "TR%d/%lld/ns_logs", testidx, vptr->partition_idx); \
  }

#define IS_JMETER_TIMEOUT \
{ \
  cur_time = time(NULL); \
  elaps_time = cur_time - start_time; \
  NSDL2_HTTP(vptr, NULL, "elaps_time = %lld, wait_time = %d\n", elaps_time, wait_time); \
  if(elaps_time > wait_time) \
  { \
    NSDL2_HTTP(vptr, NULL, "JMeter timeout"); \
    is_timeout = 1; \
    break; \
  }\
  else \
  { \
    VUSER_SLEEP(vptr, 600); \
    continue; \
  } \
}


typedef struct
{
  char *cmd_buf;  // Will store cmd_buf
  int jmeter_pid; //Store jmeter process pid
}jmeter_attr_t;

#endif
