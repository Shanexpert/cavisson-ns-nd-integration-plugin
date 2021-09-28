#ifndef NS_HEALTH_MONITOR_H 
#define NS_HEALTH_MONITOR_H 

#define SYS_HEALTHY      1
#define SYS_NOT_HEALTHY  0

extern int is_sys_healthy(int init_flag, avgtime *avg);

#endif
