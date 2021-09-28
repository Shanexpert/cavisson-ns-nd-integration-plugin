#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <libgen.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "ns_trace_level.h"

#include "netstorm.h"

#include "ns_summary_rpt.h"
#include "ns_alloc.h"
#include "ns_gdf.h"
#include "ns_percentile.h"
#include "ns_pdf_parse.h"
#include "ns_exit.h"
//#include "ns_utils.h"

#define URL_AVG_RESP_TIME_PDF_ID        1
#define PAGE_AVG_RESP_TIME_PDF_ID       2
#define SESSION_AVG_RESP_TIME_PDF_ID    3
#define TRANS_AVG_RESP_TIME_PDF_ID      4
#define TRANS_TIME_PDF_ID               5


//extern void end_test_run( void );

/*void insert_into_unique_pdf_list(int pdf_id)
{
  int i;

  NSDL2_GDF(NULL, NULL, "Method called. pdf_id = %d, num_unique_pdfs = %d\n",  pdf_id, num_unique_pdfs);

  for (i = 0; i < num_unique_pdfs; i++) {
    if (pdf_id == *(unique_pdfs_list[i]))
      return;
  }
  
   //Fill it in 
  MY_REALLOC_EX(unique_pdfs_list, (num_unique_pdfs + 1) * sizeof(int *), num_unique_pdfs * sizeof(int *),"unique_pdfs_list", num_unique_pdfs);
  MY_MALLOC(unique_pdfs_list[num_unique_pdfs], sizeof(int), "pdf", num_unique_pdfs);
  *(unique_pdfs_list[num_unique_pdfs]) = pdf_id;

  num_unique_pdfs++;
}*/

/**
 * Seperate function so we can switch pdf_id or pdf_name as the key 
 * field to search PDF from file system.
 */
FILE *get_and_open_pdf_file_by_pdf_id(int pdf_id, char *pdf_name)
{
  char file_name[MAX_FILE_NAME];
  char cmd[MAX_LINE_LENGTH];
  FILE *pfd = NULL;
  FILE *fp = NULL;
  char *fname = NULL;
  
  NSDL2_PERCENTILE(NULL, NULL, "Method called, pdf_id = %d", pdf_id);
  NSTL1(NULL, NULL, "Processing pdf file for PDF ID = %d", pdf_id);

  sprintf(cmd, GET_FILE_NAME_FORMAT, getenv("NS_WDIR"), pdf_id);

  NSDL3_PERCENTILE(NULL, NULL, "Running command = %s", cmd);

  if((pfd = popen(cmd, "r")) == NULL)
  {
    NSTL1(NULL, NULL, "Error: failed to run command = %s", cmd);
    goto err;
  }

  fread(file_name, MAX_FILE_NAME - 1, 1, pfd);
  if(ferror(pfd))
  {
    NSTL1(NULL, NULL, "Error: failed to read command output. cmd = %s", cmd);
    goto err;
  }

  NSDL3_PERCENTILE(NULL, NULL, "PDF file name for pdf is %d is %s", pdf_id, file_name);

  fname = strtok(file_name, "\n");

  NSDL3_PERCENTILE(NULL, NULL, "Opening file %s", fname);

  if((fp = fopen(fname, "r")) == NULL)
  {
    NSTL1(NULL, NULL, "Error: failed to open file %s, errno = %d (%s)", fname, errno, nslb_strerror(errno));
    goto err;
  }
  
  strcpy(pdf_name, basename(fname));

  NSDL2_PERCENTILE(NULL, NULL, "Method exit, fp = %p for pdf_name = %s", fp, pdf_name);

  err:
  if(pfd)
    fclose(pfd);
  
  return fp;
}


/* Returns num granules */
int parse_pdf(FILE *pdf_fd, int *min_granules, int *max_granules)
{
  char pdf_line[MAX_LINE_LENGTH];
  char tmpbuff[MAX_LINE_LENGTH];
  //int num_tokens = 0;
  char *buffer[PDF_MAX_FIELDS];
  //  int min_granules, max_granules;
  int num_granules = -1;
  int num_slabs;

/*   if (test_run_pdf_fd == NULL) { /\* First time *\/ */
/*     test_run_pdf_fd = fopen("/tmp/testrun.pdf", "w"); */
/*   } */
  
  while(nslb_fgets(pdf_line, MAX_LINE_LENGTH, pdf_fd, 1)) {
    if (pdf_line[0] == '#' || pdf_line[0] == '\0') /* Blank line */
      continue;
    if(sscanf(pdf_line, "%s", tmpbuff) == -1)  // for blank line with some spaces.
      continue;
    if(!(strncmp(pdf_line, "info|", strlen("info|"))) || !(strncmp(pdf_line, "Info|", strlen("Info|"))))
      continue;
    if(!(strncmp(pdf_line, "pdf|", strlen("pdf|"))) || !(strncmp(pdf_line, "PDF|", strlen("PDF|")))) {
      /* Fill in pdf array */
      get_tokens(pdf_line, buffer, "|", PDF_MAX_FIELDS);
      
      *min_granules = atoi(buffer[2]);
      *max_granules = atoi(buffer[3]);
      num_slabs = atoi(buffer[4]);

      /* Basically min_granules can only fall in range 
       * { min_granules > 0 && min_granules <= max_granules } and
       * max_granules must be divisible by min_granules */
      if (*min_granules > *max_granules || *min_granules <= 0 || 
          *max_granules % *min_granules) /* this is not right */ {
        NS_EXIT(-1, "Min granules can not be greater than Max granules or less than equal to zero and Max granules must be a multiple of Min Granules.");
      }

      num_granules = (*max_granules / *min_granules) + 1;

      if (num_slabs < 0 || num_slabs > num_granules) {
        NS_EXIT(-1, "Num Slabs can not be greater than Num Granules or less than zero.");
      }

      //create_new_pdf_entry();
      break; /* We break here because we assume only one pdf entry per pdf */
    } else {
      NS_EXIT(-1, "Error: Invalid line Between pdf lines {%s}\n", pdf_line);
    }
  }
  
  return num_granules;
}

/*Get pdf_fd andf pdf_id
* Do not close the opened pdf file here since closing
* this file in the proecss_pdf() function. 
* Seek set to the beginning of the pdf file for parsing.
*/
FILE*
get_pdf_fd_id_from_pdf(char *pdf_file, int *pdf_id)
{
  char pdf_line[MAX_LINE_LENGTH];
  char tmpbuff[MAX_LINE_LENGTH];
  char *buffer[PDF_MAX_FIELDS];
  char file_path[MAX_FILE_NAME];
  FILE *pdf_fp = NULL;
 
  //PDF file must be present in $NS_WDIR/pdf dir. 
  if (getenv("NS_WDIR")) {
      sprintf(file_path, "%s/pdf/%s", getenv("NS_WDIR"), pdf_file);
  } else {
      sprintf(file_path, "/home/cavisson/work/pdf/%s", pdf_file);
  } 
  NSDL3_GDF(NULL, NULL, "User defined PDF filepath = %s", file_path); 

  pdf_fp = fopen(file_path, "r");
  if(pdf_fp == NULL) {
    NS_EXIT(-1, "pdf file '%s' open error. ERROR is : %s", file_path, nslb_strerror(errno));
  }
  while(nslb_fgets(pdf_line, MAX_LINE_LENGTH, pdf_fp, 1)) {
    if (pdf_line[0] == '#' || pdf_line[0] == '\0') /* Blank line */
      continue;
    if(sscanf(pdf_line, "%s", tmpbuff) == -1)  // for blank line with some spaces.
      continue;
    if(!(strncmp(pdf_line, "info|", strlen("info|"))) || !(strncmp(pdf_line, "Info|", strlen("Info|")))) 
     continue;
    if(!(strncmp(pdf_line, "pdf|", strlen("pdf|"))) || !(strncmp(pdf_line, "PDF|", strlen("PDF|")))) {
      get_tokens(pdf_line, buffer, "|", PDF_MAX_FIELDS);
      *pdf_id = atoi(buffer[1]);
      break;//exit from while after getting the pdf id.
    } else {
      fclose(pdf_fp);
      NS_EXIT(-1, "Error: Invalid line {%s} of pdf file '%s'\n", pdf_line, pdf_file);
    } 
 } /*while*/

 NSDL3_GDF(NULL, NULL, "pdf_id in user defined pdf file  = %d", *pdf_id); 
 //Point to the start of the file 
 (void)fseek(pdf_fp, 0L, SEEK_SET);
 return pdf_fp;

}

/**
 * Returns -1 if some error occured (Eg. pdf not found). 0 for successful parsing.
 * we have to write the updated pdf_id in case of user defined pdf file for GUI
 * Constraints: 
 *    1. User defined PDF file must be present in /home/cavisson/work/pdf directory.
 *    2. User defined PDF file should contains unique value of pdf_id[second value in PDF line].
 */
int
process_pdf(int *pdf_id, int *min_granules, int *max_granules, char *pdf_name)
{
  FILE *pdf_fd;
  int num_granules = 0;

  NSDL3_GDF(NULL, NULL, "pdf_fd before switch = %d", *pdf_id);
  switch(*pdf_id) {
    case URL_AVG_RESP_TIME_PDF_ID: // url
      pdf_fd = get_pdf_fd_id_from_pdf(global_settings->url_pdf_file, pdf_id);
      strcpy(pdf_name, global_settings->url_pdf_file);
      break;
    case PAGE_AVG_RESP_TIME_PDF_ID: //page
      pdf_fd = get_pdf_fd_id_from_pdf(global_settings->page_pdf_file, pdf_id);
      strcpy(pdf_name, global_settings->page_pdf_file);
      break;
    case SESSION_AVG_RESP_TIME_PDF_ID://sessions
       pdf_fd = get_pdf_fd_id_from_pdf(global_settings->session_pdf_file, pdf_id);
       strcpy(pdf_name, global_settings->session_pdf_file);
       break;
    case TRANS_AVG_RESP_TIME_PDF_ID: //transcation
       pdf_fd = get_pdf_fd_id_from_pdf(global_settings->trans_resp_pdf_file, pdf_id);
       strcpy(pdf_name, global_settings->trans_resp_pdf_file);
       break;
    case TRANS_TIME_PDF_ID: //transaction time
       pdf_fd = get_pdf_fd_id_from_pdf(global_settings->trans_time_pdf_file, pdf_id);
       strcpy(pdf_name, global_settings->trans_time_pdf_file);
       break;
    default:
       NSDL3_GDF(NULL, NULL, "Going in Default case of process pdf");
       if ((pdf_fd = get_and_open_pdf_file_by_pdf_id(*pdf_id, pdf_name)) == NULL) {
	 NS_EXIT(-1, "Unable to open pdf file for pdf_id = %d. Exiting.", *pdf_id);
       }
       break;
  }

  NSDL3_GDF(NULL, NULL, "pdf_id in user defined pdf file  = %d\n", *pdf_id);
 
  //insert_into_unique_pdf_list(*pdf_id);
  num_granules = parse_pdf(pdf_fd, min_granules, max_granules);
  fclose(pdf_fd);
   
  return num_granules;
}
