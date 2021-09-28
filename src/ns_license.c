#include<stdio.h>
#include<stdlib.h>
#include<strings.h>
#include<string.h>
#include "ns_license.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_global_settings.h"
#include "ns_error_msg.h"


/*
 * License is not present - Log event and then no more checking
 * License is not valid - Log event and then no more checking
 * License has expired - Log event and then check for user limits
 *
 *
 *
 */

unsigned int license_vuser = 0;
static unsigned int max_user_used = 0; // Used to save max user used in scenario

int ns_validate_license(char *product_name, char *err_msg)
{
  int ret;

  NSDL2_REPORTING(NULL, NULL, "Method called");
  if((ret = nslb_validate_license(product_name, err_msg)) != 0){
    NS_EL_2_ATTR(EID_MISC,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                __FILE__, (char*)__FUNCTION__, "%s", err_msg);

    NSTL1(NULL, NULL, "%s\n", err_msg);
  }
  return ret; 
}

int ns_lic_chk_users_limit(unsigned int avg_users)
{
  int static has_occured = 0;
  char msg[2048 + 1];
  char allow_msg[1024];
  int ret;

  // set max user used so that we can use it at the end to show max users
  if(max_user_used < avg_users)
    max_user_used = avg_users;
 
  // We already logged event
  if(has_occured)  
    return 0;

  NSDL2_REPORTING(NULL, NULL, "Method called, max_user_used = %u", max_user_used);
  if((ret = nslb_validate_user_limit(max_user_used, &license_vuser)))
  {
    has_occured = 1;
    NSDL2_REPORTING(NULL, NULL, "NetStorm virtual users =%d exceeded the license size limit.", avg_users);
    
    //ret = -2 means we want to stop test on invalid license, so error message should be according to that. 
    ret == -2?strcpy(allow_msg, ""):strcpy(allow_msg, " Allowing test to continue on good faith.");

    //sprintf(msg, "NetStorm VUsers %u exceeded the license size limit of %u VUsers including grace VUsers. "
      //           "Contact Cavisson Account Representative (US +1-800-701-6125) to increase VUsers license limit.%s", 
        //          max_user_used, license_vuser, allow_msg);

    snprintf(msg, 2048, CAV_ERR_1032010, max_user_used, license_vuser, allow_msg);

    NS_EL_2_ATTR(EID_MISC,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                __FILE__, (char*)__FUNCTION__,
               "%s", msg);

    fprintf(stderr, "%s\n", msg);

    NSDL2_REPORTING(NULL, NULL, "NetStorm virtual users %d exceeded the license size limit of %u virtual users.", avg_users, license_vuser);
    return ret;
  }
  return ret;
}
