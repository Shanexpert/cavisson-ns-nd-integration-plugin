

#include "ns_check_point_vars.h"

/*************************************************************************************
  Name: ns_check_point_api.c.
  Author(s) : Nishi Mishra
  Purpose:
           ns_chekpoint_api:- This API is used for applying checkpoint on Url's.


**************************************************************************************/


/******************************************************************************************
Purpose  : This API is used to add Checkpoint to check any string in url's response 
           through posturl callback

Input    : pfx : This is the prefix of the string.
           pfx_re_ic : This is used in case of regular expression, it has 3 values,
                        NS_CP_NO_REGEX_IC 0 // No regex, no ignore case
                        NS_CP_REGEX       1
                        NS_CP_IC          2
                        NS_CP_REGEX_IC    3

           sfx : This is the suffix of the string,in case user want to search a 
                 string without any pfx or sfx, then he will provide pfx only.
           sfx_re_ic : This is used in case of regular expression, it has 3 values,
                        NS_CP_NO_REGEX_IC 0 // No regex, no ignore case
                        NS_CP_REGEX       1
                        NS_CP_IC          2
                        NS_CP_REGEX_IC    3

           fail : This is the failure condition, it has 2 value NS_CP_FAIL_ON_FOUND
                  or NS_CP_FAIL_ON_NOT_FOUND.
           action : This is the action performed on failure, it has 2 value 
                    NS_CP_ACTION_STOP or NS_CP_ACTION_CONTINUE.
           save_count_param:This is used to store the occurance of string.
*******************************************************************************************/

/* Pending

1. Regex support

*/

int ns_checkpoint(char *pfx, short pfx_re_ic, char *sfx, short sfx_re_ic, int fail, int action, char* save_count_param)
{
  VUser *vptr = cur_cptr->vptr;

  int count = 0;
  int page_fail = 0;
  int to_stop = 0;

  CheckPointTableEntry_Shr checkpoint_ptr; // This is used to setup checkpoint so that we can using existing method


  NSDL2_API(vptr, NULL, "Method called save_count_param = %s", save_count_param);

  // If fail is other than NS_CP_FAIL_ON_FOUND and NS_CP_FAIL_ON_NOT_FOUND, give error 
  if(fail != NS_CP_FAIL_ON_FOUND && fail != NS_CP_FAIL_ON_NOT_FOUND && fail != NS_CP_IGNORE)
  {
    fprintf(stderr, "Fail condition should be either NS_CP_FAIL_ON_FOUND, NS_CP_FAIL_ON_NOT_FOUND or NS_CP_IGNORE\n");
    return -1;
  }      

  // If action is other than NS_CP_ACTION_STOP and NS_CP_ACTION_CONTINUE
  if(action != NS_CP_ACTION_STOP && action != NS_CP_ACTION_CONTINUE )
  {
    fprintf(stderr, "Action on fail should be either 0 or 1\n");
    return -1;
  }

  //Check prefix 
  if(!pfx || *pfx == '\0')
  {
    fprintf(stderr, "Provide prefix for the checkpoint\n");
    return -1; 
  }

  //Check suffix
  if(sfx && *sfx == '\0')
  {
    sfx = NULL;
    sfx_re_ic = 0;
  }
  //Check cpunt parameter
  if(save_count_param && *save_count_param == '\0')
    save_count_param = NULL;
 
  //Check ignore fail condition
  if(fail == NS_CP_IGNORE && save_count_param == NULL) 
  {
     fprintf(stderr,"Warning:No Count Var is defined so setting to fail condition to NOT_FOUND & fail action to STOP\n");
     NS_DUMP_WARNING("No Count Var is defined so setting to fail condition to NOT_FOUND & fail action to STOP");
     fail = NS_CP_FAIL_ON_NOT_FOUND;
     action = NS_CP_ACTION_STOP;
  }

  NSDL2_API(vptr, NULL, "pfx = %s, sfx = %s , fail = %d action = %d", pfx, sfx , fail, action);

  
  memset(&checkpoint_ptr, 0 , sizeof(CheckPointTableEntry_Shr));

  // Initializing the value of the member of CheckPointTableEntry_Shr table. 
  checkpoint_ptr.text_pfx = pfx; 
  checkpoint_ptr.text_sfx = sfx; 
  checkpoint_ptr.id = "Checkpoint API" ; 
  checkpoint_ptr.fail = fail; 
  checkpoint_ptr.action_on_fail = action;
  checkpoint_ptr.report = NS_CP_REPORT_FAILURE;
  
  checkpoint_ptr.save_count_var = save_count_param; // Can be NULL if not to be saved

  //Check of regex & ignore case for prefix
  if(pfx_re_ic)
  {
     checkpoint_ptr.ignorecase_textpfx = pfx_re_ic & NS_CP_IC;
     checkpoint_ptr.regexp_textpfx =  pfx_re_ic & NS_CP_REGEX;
     my_regcomp(&checkpoint_ptr.preg_textpfx, checkpoint_ptr.ignorecase_textpfx, pfx, "Error in Checkpoint API Pfx Regex");
  }

  //Check of regex & ignore case for sefix
  if(sfx_re_ic)
  {
     checkpoint_ptr.ignorecase_textsfx = sfx_re_ic & NS_CP_IC;
     checkpoint_ptr.regexp_textsfx =  sfx_re_ic & NS_CP_REGEX;
     my_regcomp(&checkpoint_ptr.preg_textsfx, checkpoint_ptr.ignorecase_textsfx, sfx, "Error in Checkpoint API Sfx Regex");

  }
 

  // Searching the string in url's response 
  apply_checkpoint(&checkpoint_ptr, &count, &page_fail, &to_stop, url_resp_buff, url_resp_size, vptr, cur_cptr);
  NSDL2_API(vptr, NULL, "Checkpoint API Status. count =%d page_fail = %d to_stop = %d", count, page_fail,to_stop);

  if(page_fail)
  {
    ContinueOnPageErrorTableEntry_Shr *ptr;
    ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];


    /* In case of check point failure, we need to decide if to continue or not based on check point stop and continue on page error setting
       to_stop	CPE	Action
         1        0      Stop
         1        1      Continue
         0        0      Continue
         0        1      Continue
    */

    to_stop = (ptr->continue_error_value == 0)?to_stop : 0;
  
    NSDL2_API(vptr, NULL, "in ns_inline_checkpoint count =%d page_fail = %d to_stop = %d", count, page_fail,to_stop);

    //  Set page status to CV failure and session status to MIsc Error
    set_page_status_for_registration_api(vptr, NS_REQUEST_CV_FAILURE, to_stop, "CheckPoint API");

    // set req_ok to cv failure, as we have to stop the transaction to complete in case of checkpoint
    cur_cptr->req_ok = NS_REQUEST_CV_FAILURE;  // ISSUE - This error is not valid for URL

    if(to_stop)
    { 
      NSDL4_VARS(vptr, NULL, "Checkpoint failure and action is to Stop on the failed");
      vptr->urls_awaited  -= vptr->urls_left ;
      vptr->urls_left = 0;

      //Removing all the scheduled url from inuse list if url status is free
      connection *cptr = vptr->head_cinuse;
      while (cptr)
      {
        NSDL2_API(vptr, NULL, "Checkpoint api - Check if cptr has scheduled inline URL. urls awaited = %d, urls left = %d, timer type=%d, connection state = %d, url = %s", vptr->urls_awaited, vptr->urls_left, cptr->timer_ptr->timer_type, cptr->conn_state, cptr->url);
        if ((cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) && ((cptr->conn_state == CNST_FREE) || (cptr->conn_state == CNST_REUSE_CON )))
        {
          NSDL2_API(vptr, NULL, "Removing timer");
          dis_timer_del(cptr->timer_ptr);
          vptr->urls_awaited--;
        }
        cptr = (connection *)cptr->next_inuse;
      }
      NSDL2_API(vptr, NULL, "urls awaited = %d urls left = %d", vptr->urls_awaited, vptr->urls_left);
    }
  }
  return 0;

}
// End of File
