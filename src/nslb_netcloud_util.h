#ifndef NS_NETCLOUD_LIB_H  
#define NS_NETCLOUD_LIB_H

#include "nc_admin.h"
//Status 
#define SUCCESS_RETURN 0
#define FAILURE_RETURN -1
#define MAX_LENGTH_OF_LINE 1024 

#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__
#define NCATDL(...)  ncat_debug_logs(_FLN_, __VA_ARGS__)
#define MAX_DEBUG_LOG_BUF_SIZE 64000

#define NCLBDL(NC_LB_flag, msg...) \
  if(NC_LB_flag) \
     NCATDL(msg); \
  else             \
     NSLBDL1_MISC(msg); \

typedef struct generatorList
{
  char gen_name[256];
  char gen_ip[128];
  char cmon_port[16];
  char gen_location[64];
  char gen_work[256];
  char gen_type[64];
  char controller_ip[128];
  char controller_name[256];
  char controller_work[256];
  char team[256];
  char name_server[128];
  char data_center[128];
  char future1[128];
  char future2[128];
  char future3[128];
  char future4[128];
  char future5[128];
  char future6[128];
  char future7[128];
  char comments[1024];
} generatorList;

typedef struct UsedGeneratorList {
   char generator_name[512];
   int used_gen_status;
} UsedGeneratorList;

extern generatorList *gen_detail;
extern int used_generator_entries;
extern int nslb_get_gen_for_given_controller(char *controller_work, char *generator_file, char *err_msg, int flag);
extern int nslb_validate_gen_are_unique_as_per_gen_name(char *controller_work, char *controller_ip, char *err_msg, int flag);
extern int nslb_get_controller_ip_and_gen_list_file(char *controller_ip, char *scenario_file, char *gen_file, char *g_ns_wdir, int testidx, char *tr_or_partition, char *err_msg, int *default_gen_file);
extern void ncat_debug_logs(char *filename, int line, char *fname, char *format, ...);

#endif
