#ifndef NS_RTE_API_H
#define NS_RTE_API_H
typedef struct ns_rte
{
  int protocol;
  int terminal;
  char kex[128 + 1];
  char ttype[128 + 1];
  int (*connect)(struct ns_rte*, char* , char * , char *);
  int (*login)(struct ns_rte*);
  int (*disconnect)(struct ns_rte*);
  int (*type)(struct ns_rte*, char*);
  int (*wait_text)(struct ns_rte*, char*, int);
  int (*wait_sync)(struct ns_rte*);
  int (*config)(struct ns_rte*, char*);
}ns_rte;

extern int nsi_rte_init(struct ns_rte*,int,int);
extern int nsi_rte_connect(struct ns_rte*,char*,char*,char*);
extern int nsi_rte_login(struct ns_rte*);
extern int nsi_rte_type(struct ns_rte*,char*);
extern int nsi_rte_wait_text(struct ns_rte*,char*,int);
extern int nsi_rte_wait_sync(struct ns_rte*);
extern int nsi_rte_disconnect(ns_rte *rte);
extern int ns_rte_post_proc();
extern int nsi_rte_config(struct ns_rte*,char*);

#define RTE_PROTO_SSH   1
#define RTE_PROTO_3270  2

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

#endif
