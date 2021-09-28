#ifndef NS_DOS_ATTACK_INCLUDES_H 
#define NS_DOS_ATTACK_INCLUDES_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>  //not needed on IRIX 
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>
#include <ctype.h>
#include <dlfcn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <regex.h>

#include "../url.h"
#include "../ns_byte_vars.h"
#include "../ns_nsl_vars.h"
#include "../libnscore/nslb_util.h"
#include "../libnscore/nslb_hash_code.h"
#include "../ns_search_vars.h"
#include "../ns_cookie_vars.h"
#include "../ns_check_point_vars.h"
#include "../ns_static_vars.h"
#include "../ns_check_replysize_vars.h"
#include "../ns_server.h"
#include "../util.h"
#include "../timing.h"
#include "../tmr.h"
#include "../ns_trans_parse.h"
#include "../logging.h"
#include "../ns_ssl.h"
#include "../ns_fdset.h"
#include "../ns_goal_based_sla.h"
#include "../ns_schedule_phases.h"
#include "../netstorm.h"
#include "../ns_vars.h"
#include "../ns_log.h"
#include "../ns_alloc.h"

#include "../ns_script_parse.h"
#include "../ns_vuser_tasks.h"
#include "../ns_http_script_parse.h"
#include "../ns_sock_listen.h"
#include "../ns_data_types.h"
#include "../init_cav.h"

#endif
