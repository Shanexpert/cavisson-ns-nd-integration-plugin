#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define RTE_KEY_MAP_SIZE    16
#define MAX_RTE_CMD_LENGTH  128
#define MAX_RTE_LINE_LENGTH 1024
#define MAX_RTE_BUF_SIZE    4096

typedef struct ns_3270_key_map
{
  char name[32];
  short len;
  char cmd[32];
}ns3270keyMap;

#define RTE_THINK_TIME(vptr, rte, interval) \
{\
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) \
  { \
    vptr->flags |= NS_USER_PTT_AS_SLEEP; \
    ns_rte_page_think_time_as_sleep(vptr, interval);\
    vptr->flags &= ~NS_USER_PTT_AS_SLEEP; \
  } \
  else \
    usleep(interval); \
} 


int ns_rte_3270_connect(ns_rte *rte, char *host, char *user, char *password);
int ns_rte_3270_disconnect(ns_rte *rte);
int ns_rte_3270_send_text(ns_rte *rte , char *text);
int ns_rte_3270_wait_text(ns_rte *rte, char *text, int timeout);
int ns_rte_3270_wait_sync(ns_rte *rte);
int ns_rte_page_think_time_as_sleep(VUser *vptr, int page_think_time);
