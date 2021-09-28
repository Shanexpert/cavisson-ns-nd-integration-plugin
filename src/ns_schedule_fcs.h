/********************************************************************************
 * File Name            : ns_schedule_fcs.h
 * Purpose              : contains global variables and structures used in FCS
 *                        
 * Modification History : Gaurav|01/09/2017
 ********************************************************************************/
#ifndef NS_SCHEDULE_FCS_H

#define NS_SCHEDULE_FCS_H

#include "nslb_mem_pool.h"

typedef struct {
  NSLB_MP_COMMON;
  VUser *sav_vptr;
} Pool_vptr;

extern nslb_mp_handler *sess_pool;
extern int kw_set_enable_schedule_fcs(char *buf, int runtime_flag, char *err_msg);
extern void divide_on_fcs_mode();
extern void handle_alert_request();
extern void check_fcs_user_and_pool_size();

#endif
