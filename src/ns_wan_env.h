#include<stdio.h>
#include<stdlib.h>
#include <linux/socket.h>
#include <netinet/tcp.h>


extern int v_cavmodem_fd; 

extern int ns_cavmodem_init();
extern void cav_open_modem(VUser *vptr); 
extern int cav_open_shared_modem(AccAttrTableEntry *accAttrTable, AccAttrTableEntry_Shr *accattr_table_shr_mem);
extern void clear_shared_modems(void);
extern void cav_close_modem(VUser *vptr);
extern void set_socket_for_wan(connection *cptr, PerHostSvrTableEntry_Shr* svr_entry);
extern void kw_set_wan_env(char *value);
extern int kw_set_adverse_factor(char *buff, char *err_msg, int runtime_flag);
extern int kw_set_wan_jitter(char *buff, char *err_msg, int runtime_flag);
extern inline void init_wan_setsockopt();
