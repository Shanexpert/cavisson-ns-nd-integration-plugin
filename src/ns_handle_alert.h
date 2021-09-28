/********************************************************************************
 * File Name            : ns_schedule_fcs.h
 * Purpose              : contains global variables and structures used in FCS
 *                        
 * Modification History : Gaurav|01/09/2017
 ********************************************************************************/
#ifndef NS_NETCLOUD_ALERT_H

#define NS_NETCLOUD_ALERT_H

#include "nslb_mem_pool.h"
#include "nslb_dashboard_alert.h"
#include "nslb_alert.h"

//#define NETCLOUD_POLICY_NAME "NetCloudAlert"
#define ALERT_POLICY_NAME "CustomPolicy"
#define ALERT_MSG_SIZE 1024

extern NetstormAlert *g_ns_alert;
extern char *g_alert_info;
extern int handle_alert_msg(char* msg, int severity, char* policyName);
extern int nsi_send_alert(int alert_type, int alert_method, char *content_type, char *alert_msg, int length);
extern void ns_alert_config();
extern void copy_alert_info_into_shr_mem();
extern int process_alert_server_config_rtc();
extern void ns_process_apply_alert_rtc();
#define MAX_ALERT_TYPE 6

//Only 8 bits supported as alert type is of char type
#define ALERT_MM_NORMAL     0x0001
#define ALERT_MM_MINOR      0x0002
#define ALERT_MM_MAJOR      0x0004
#define ALERT_MM_CRITICAL   0x0008
#define ALERT_MM_INFO       0x0010
#define ALERT_MM_ALL        0x003F


#endif

