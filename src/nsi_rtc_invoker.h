#ifndef NSI_RTC_INVOKER_H 
#define NSI_RTC_INVOKER_H 

#define _NLF_		           __FILE__, __LINE__, (char *)__FUNCTION__ 

#define DLOG_L1	                   0x000F
#define DLOG_L2    	           0x00F0
#define DLOG_L3  	           0x0F00
#define DLOG_L4 	           0xF000

#define INV_SUCCESS                 0
#define FAILURE                    -1
#define TEST_NOT_RUNNING           -2

//#define RED                        "\x1B[1m\x1B[31m"
//#define RESET 			   "\x1B[0m"

#define MAX_LINE_LENGHT            1024
#define MAX_PATH_LEN               1024
#define MSG_BUF_INIT_DELTA_LEN     4 * 1024

#define OPCODE_LEN                 4
#define MSG_LEN                    4

#define FILE_PARAM_RTC_OPCODE      149
#define NS_RTC_OPCODDE             150
#define MONITOR_RTC_OPCODE         151

#define ARGV_TRNUM                 0x01  //1st bit
#define ARGV_OPCODE                0x02  //2nd bit
#define ARGV_MESG                  0x04  //3rd bit 
#define ARGV_MASK                  0x07  //Mask

#define NS_VUSER_OPTION_GROUP      0x0001 
#define NS_VUSER_OPTION_STATUS     0x0002 
#define NS_VUSER_OPTION_GEN        0x0004 
#define NS_VUSER_OPTION_USER_LIST  0x0008 
#define NS_VUSER_OPTION_OFFSET     0x0010 
#define NS_VUSER_OPTION_LIMIT      0x0020 
#define NS_VUSER_OPTION_QUANTITY   0x0040 
#define NS_VUSER_OPTION_USER_VPTR  0x0080 






#define PROC_RETURN(pass, err_msg) \
{ \
  if(dlog_fp) \
    fclose(dlog_fp); \
  if(msg_buf) \
  { \
    free(msg_buf); \
    msg_buf = NULL; \
  } \
  if(!pass) \
  { \
    printf("SUCCESS\n"); \
    return 0; \
  } \
  else if(pass == TEST_NOT_RUNNING)\
  {\
    fprintf(stderr, "%s\n", err_msg); \
    exit(2); \
  }\
  else\
  { \
    /*RED is remove because in gui its value is print.*/\
    /*fprintf(stderr, RED "%s\nFAILURE\n" RESET, err_msg);*/\
    fprintf(stderr, "%s\n", err_msg); \
    exit(1); \
  } \
}

#define FILL_MSG_BUF(dest, src, len) \
{ \
  memcpy(dest, src, len); \
  dest += len; \
}

void set_rtc_type(int, char *);
#endif
