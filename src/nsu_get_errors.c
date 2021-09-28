/********************************************************************
 * Name            : nsu_get_errors.c 
 * Purpose         : Output error codes for Url, Page, Session & Transaction,
 * 		     User defined error can be overwritten if defined in tx_error_codes(Trnsaction) or sess_error_codes(Sesson) in NSWIR
 *
 * 		     Url: total 32, defined: 24, undefined:8
 * 		     Page: total 64, 32 of Url & 32 of its own; Undefined its own is 30
 * 		     Transaction: total 96, 64 of page & url & remaining 32 user defined error
 * 		     Session: Total of 16; among 13 are user defined.
 *
 * Initial Version : Wednesday, November 04 2009  
 * Modification    : 22/1/211- to show only used errors.
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "nslb_util.h"
#include "ns_error_codes.h"
#include "nslb_cav_conf.h"

#define MAX_ERROR_CODE_NAME_LEN 384
#define MAX_USER_DEF_ERR        32

#define NS_ERROR_DO_NOT_SHOW_ERR_IDX 0
#define NS_ERROR_SHOW_ERR_IDX 1
static int index_mode; // To show error code with index.

static int global_index = 0; // Error code value
static int user_error_flag; // Only User errors 
static int show_used_only_flag; //Only used errors, no Undef's

static int lim_url_max = TOTAL_URL_ERR;
static int lim_page_max = TOTAL_PAGE_ERR - TOTAL_URL_ERR;

static void usage()
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "nsu_get_errors <obj-type> <show-index-mode 0|1> <show-only-set-by-api (0/1)> <show-used-only (0/1)>\n");
  fprintf(stderr, "Where:\n");
  fprintf(stderr, "  obj-type is: \n");
  fprintf(stderr, "    - 0 for Url/protocol specific errors.\n");
  fprintf(stderr, "    - 1 for Page errors.\n");
  fprintf(stderr, "    - 2 for Transaction errors.\n");
  fprintf(stderr, "    - 3 for Session errors\n");
  fprintf(stderr, "  show-index mode 0 shows only error code names.\n");
  fprintf(stderr, "  show-index mode 1 shows error code names with index.\n");
  fprintf(stderr, "  show-only-set-by-api value of 1 will show only those errors code which can be set by API (e.g. ns_set_tx_status) from script.c file\n");
  fprintf(stderr, "  show-used-only value of 1 shows only used error codes. It will not show error codes resevered for future\n");

  exit(-1);
}

static char tx_user_error_codes[MAX_USER_DEF_ERR][ MAX_ERROR_CODE_NAME_LEN + 1] = { "TxErr64", "TxErr65", "TxErr66", "TxErr67",
                                   "TxErr68", "TxErr69", "TxErr70", "TxErr71",
                                   "TxErr72", "TxErr73", "TxErr74", "TxErr75",
                                   "TxErr76", "TxErr77", "TxErr78", "TxErr79",
                                   "TxErr80", "TxErr81", "TxErr82", "TxErr83",
                                   "TxErr84", "TxErr85", "TxErr86", "TxErr87",
                                   "TxErr88", "TxErr89", "TxErr90", "TxErr91",
                                   "TxErr92", "TxErr93", "TxErr94", "TxErr95"
                                 };

static char sess_user_error_codes[MAX_USER_DEF_ERR][ MAX_ERROR_CODE_NAME_LEN + 1] = {"SsErr4", "SsErr5", "SsErr6",
                                   "SsErr7", "SsErr8", "SsErr9", "SsErr10",
                                   "SsErr11", "SsErr12", "SsErr13", "SsErr14",
                                   "SsErr15"
                                 };

static char *sess_errors_codes[] = { "Success", "MiscErr", "Stopped", "Aborted" }; 

static void print_url_errors()
{
  int index;
  char *url_errors_codes[] = { "Success", "MiscErr", "1xx", "2xx",
                               "3xx", "4xx", "5xx", "PartialBody",
                               "T.O", "ConFail", "ClickAway", "WriteFail",
                               "SSLWriteFail", "SSLHshakeFail", "IncompleteExact", "NoRead",
                               "PartialHdr", "BadBodyNoSize", "BadBodyChunked", "BadBodyConLen",
                               "Reload", "Stopped", "BadResponse", "MailBoxErr",
                               "MailBoxStorageErr", "AuthFail", "DataExhausted", "RedirectLimit",
                               "RangeExhausted", "BindFail", "DNSLookUpFail", "SysErr", "Undef31"
                            };
  if(user_error_flag)
  {
    fprintf(stderr, "Error: No user defined errors for Url or Page !\n");
    exit (-1);
  }

  for (index = 0; index < lim_url_max ; index++)
  {
    if(index_mode == NS_ERROR_DO_NOT_SHOW_ERR_IDX)
      printf("%s\n", url_errors_codes[index]);
    else
      printf("%2d - %s\n", global_index++, url_errors_codes[index]);
  }

  // Adjust it for not used error code as page error code starts after all URL codes 
  // including not used
  if (show_used_only_flag)
    global_index += TOTAL_URL_ERR - TOTAL_USED_URL_ERR;
       // printf("global_index %d\n",global_index);
}

static void print_page_errors()
{
  int index;
  char *page_errors_codes[] = { "UrlErr", "CVFail", "UnCompFail", "SizeTooSmall", 
                                "SizeTooBig", "Abort", "Undef38", "Undef39", 
                                "Undef40", "Undef41", "Undef42", "Undef43",
                                "Undef44", "Undef45", "Undef46", "Undef47", 
                                "Undef48", "Undef49", "Undef50", "Undef51",
                                "Undef52", "Undef53", "Undef54", "Undef55", 
                                "Undef56", "Undef57", "Undef58", "Undef59",
                                "Undef60", "Undef61", "Undef62", "Undef63"
                              };
  print_url_errors();
  
  for (index = 0; index < lim_page_max ; index++)
  {
    if(index_mode == NS_ERROR_DO_NOT_SHOW_ERR_IDX)
      printf("%s\n", page_errors_codes[index]);
    else
	  printf("%2d - %s\n", global_index++, page_errors_codes[index]);
  }
  if (show_used_only_flag)
    global_index += TOTAL_PAGE_ERR - TOTAL_USED_PAGE_ERR;

  // printf("global_index %d\n",global_index);
}

static int load_tx_file(char *file_name )
{
  int error_code;
  char error_msg[384];
  char line[512];
 
  FILE *fp;

  if ((fp = fopen(file_name,"r")) == NULL) {
    if (errno != ENOENT) {
      fprintf(stderr, "input_error_codes(): Error in opening file %s\n", file_name);
      perror("fopen");
      return -1;
    }
  } else {
    while ((fgets(line, 512, fp) != NULL)) {
      if (line[0] == '#') continue;
      if (sscanf(line, "%d %s", &error_code, error_msg) != 2) {
        fprintf(stderr, "input_error_codes(): Wrong format in file %s\n", file_name);
        return -1;
      }

      if ((error_code >= 64) && (error_code < TOTAL_TX_ERR)) {
        strcpy(tx_user_error_codes[error_code - 64], error_msg);
      } else {
        fprintf(stderr, "Invalid error_code %d in file %s (valid range 64 to 95) \n", error_code, file_name);
        return -1;
      }
    }

    fclose(fp);
  }
    return 0;
}

static int load_sess_file(char *file_name )
{
  int error_code;
  char error_msg[384];
  char line[512];
 
  FILE *fp;

  if ((fp = fopen(file_name,"r")) == NULL) {
    if (errno != ENOENT) {
      fprintf(stderr, "input_error_codes(): Error in opening %s\n", file_name);
      perror("fopen");
      return -1;
    }
  } else {
    while ((fgets(line, 512, fp) != NULL)) {
      if (line[0] == '#') continue;
      if (sscanf(line, "%d %s", &error_code, error_msg) != 2) {
        fprintf(stderr, "input_error_codes(): Wrong format in file %s\n", file_name);
        return -1;
      }

      if ((error_code >= 3) && (error_code < TOTAL_SESS_ERR)) {
        strcpy(sess_user_error_codes[error_code - 3], error_msg);
      } else {
        fprintf(stderr, "Invalid error_code %d in file %s (valid range 3 to 15) \n", error_code, file_name);
        return -1;
      }
    }

    fclose(fp);
  }
  return 0;
}

static void print_tx_errors()
{
  int index;
  char file_name[512];
 
  sprintf(file_name, "%s/sys/tx_error_codes", g_ns_wdir);

  if(load_tx_file(file_name) == -1)
  {
     exit(-1);
  }  
  
  // For user settable error codes, we cannot use error code which are propagated from URL and Page
  if(user_error_flag) 
  {
    if(index_mode == NS_ERROR_DO_NOT_SHOW_ERR_IDX)
    {
      printf("%s\n", "AutoStatus");
      printf("%s\n", "Success");
    }
    else
    {
      printf("NS_AUTO_STATUS - %s\n", "AutoStatus");
      printf(" 0 - %s\n", "Success");
      global_index = TOTAL_PAGE_ERR;
    }
  }
  else
    print_page_errors(); // Show URL/Page error codes 

  for(index = TOTAL_PAGE_ERR; index < TOTAL_TX_ERR; index++)
  {
    if(index_mode == NS_ERROR_DO_NOT_SHOW_ERR_IDX)
      printf("%s\n", tx_user_error_codes[index - TOTAL_PAGE_ERR]);
    else
      printf("%2d - %s\n", global_index++, tx_user_error_codes[index - TOTAL_PAGE_ERR]);
  }
 
}

static void print_sess_errors()
{
  int index;
  char file_name[512];
 
  sprintf(file_name, "%s/sys/sess_error_codes", g_ns_wdir);

  if(load_sess_file(file_name) == -1)
  {
    exit(-1);
  }

  if(user_error_flag)
  {
    global_index = TOTAL_SESS_ERR - USER_DEF_SESS_ERR;
    if(index_mode == NS_ERROR_DO_NOT_SHOW_ERR_IDX)
      printf("%s\n", "Success");
    else
      printf(" 0 - %s\n", "Success");
  }
  else
  {
    // Show error codes which are set by netstorm and cannot be used in API
    // (e.g. MiscErr and Stopped
    for(index = 0; index < TOTAL_SESS_ERR - USER_DEF_SESS_ERR; index++)
    {
      if(index_mode == NS_ERROR_DO_NOT_SHOW_ERR_IDX)
        printf("%s\n", sess_errors_codes[index]);
      else
        printf("%2d - %s\n", global_index++, sess_errors_codes[index]);
    }
  }

  // Show error codes which are set by API
  for(index = 0; index < USER_DEF_SESS_ERR; index++)
  {
    if(index_mode == NS_ERROR_DO_NOT_SHOW_ERR_IDX)
      printf("%s\n", sess_user_error_codes[index]);
    else
      printf("%2d - %s\n", global_index++, sess_user_error_codes[index]);
  }
}

int main(int argc, char *argv[])
{
  int c;

  if(argc < 2)
   usage();

  c = atoi(argv[1]); // url, page, tx or sess

  if(argc >= 3) // mode to print with index or not
    index_mode = atoi(argv[2]);

  if(argc >= 4)
    user_error_flag = atoi(argv[3]);

  if(argc == 5) {
    show_used_only_flag = atoi(argv[4]);
    if (show_used_only_flag !=0 && show_used_only_flag != 1){ //sanity check
     usage();
    } 
    
    if (show_used_only_flag) { //set limits for printing url and page errors
        lim_url_max = TOTAL_USED_URL_ERR;
        lim_page_max = TOTAL_USED_PAGE_ERR - TOTAL_URL_ERR;
    }
  }

  if(index_mode != NS_ERROR_DO_NOT_SHOW_ERR_IDX && index_mode != NS_ERROR_SHOW_ERR_IDX)
    usage();

  set_ns_wdir();

  switch(c) {
    case OBJ_URL_ID:
       print_url_errors();
       break;
    case OBJ_PAGE_ID:
       print_page_errors();
       break;
    case OBJ_TRANS_ID:
       print_tx_errors();
       break;
    case OBJ_SESS_ID:
       print_sess_errors();
       break;
    default:
       usage();
  }

 return 0;
}
