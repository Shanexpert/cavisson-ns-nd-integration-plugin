
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include "nslb_alloc.h"
#include "nslb_hash_code.h"
#include "nslb_util.h"

#include "../../../ns_data_types.h"
#include "../../../logging_reader.h"

#include "nslb_log.h"

#include "nslb_dyn_hash.h"

#include "../../../ns_objects_normalization.h"
#define NS_EXIT_VAR
#include "../../../ns_exit.h"

#define MAX_LINE_LEN 10000
FILE *ctrl_urc_fptr = NULL;
FILE *ctrl_urt_fptr = NULL;
char wdir[1024] = "/home/cavisson/work"; 
int url_id;
int normalize_url_id;
int DynamicTableSizeiUrl = 0;
int DynamicTableSizeiTx = 0;
int MaxStaticUrlIds = 0;

#define MAX_URL_LEN 8195

unsigned int url_idx;
unsigned int pg_id;
unsigned int url_hash_id;
unsigned int url_hash_code;
int len; // URL Length
char url_name[MAX_URL_LEN];

//TODO: Itese are dependencies of ns_url_id_normalization.c so added here, need to
//remove these later
int  debug_level_reader = 0;
FILE *debug_file_nsa = NULL;

static void read_gen_urt_file_and_update_ctrl_urt_file(int gen_test_idx, int generator_id, int ctrl_testidx, char *gen_name)
{
  FILE *netcloud_gen_urt_fptr = NULL;
  char file[4096];
  //char wdir[4096];
  char buf[MAX_LINE_LEN];
  char *ptr = NULL, *ptr2 = NULL;
  int is_new_record = 0;
  //int max_static_is_in_gen = -1; //It should start from -1 as static id start from 0
  char *ptr_from_page_id = NULL;
  unsigned int ihashValue = 0; 

  sprintf(file, "%s/logs/TR%d/NetCloud/%s/TR%d/reports/csv/urt.csv", wdir, ctrl_testidx, gen_name, gen_test_idx);
  if (!(netcloud_gen_urt_fptr = fopen(file, "r"))) {   
    perror("ns_gen_url_id_normalization");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", file, gen_test_idx);
  }
 
  while (nslb_fgets(buf, MAX_LINE_LEN, netcloud_gen_urt_fptr, 0))
  {
    //2549,1,hash_id,hashcode,len,0,/tours/Merc10-dev/images/banner_animated.gif
   //TestRun, UrlIndex, PageIndex, UrlHashIndex, UrlHashCode, UrlLen, UrlName
    ptr = strchr(buf, ',');
    if(ptr == NULL)
      continue;
    ptr++;
    //This is done for optimization, we will directly write whole buffer after this ","
    ptr2 = strchr(ptr, ',');
    if(ptr2 == NULL)
      continue;
    *ptr2 = '\0';
    url_idx = atoi(ptr);
    *ptr2 = ','; //Restore ,

    ptr2++; //Now its pointing to PageIndex
    ptr_from_page_id = ptr2;
    //fprintf(stderr, "Achint - Page idx %d, %s, Gen testidx %d\n", __LINE__, ptr2, gen_test_idx);

    ptr = strchr(ptr2, ',');
    if(ptr == NULL)
      continue;
    ptr++;// Ponting to UrlHashIndex
    //fprintf(stderr, "Achint - Hash idx %d, %s\n", __LINE__, ptr);
    
    ptr2 = strchr(ptr, ',');
    if(ptr2 == NULL)
      continue;
    *ptr2 = '\0';
    
    url_hash_id = atoi(ptr);
    *ptr2 = ','; //Restore ,

    ptr2++; //pointing hashcode
    //fprintf(stderr, "Achint - Hash code %d, %s\n", __LINE__, ptr);

    ptr = strchr(ptr2, ',');
    if(ptr == NULL)
      continue;
    *ptr = '\0';
    
    url_hash_code = atoi(ptr2);
    *ptr = ','; //Restore ,

    ptr2 = ptr;
    ptr2++; //pointing len
    //fprintf(stderr, "Achint - len %d, %s\n", __LINE__, ptr);

    ptr = strchr(ptr2, ',');
    if(ptr == NULL)
      continue;
    *ptr = '\0';

    len = atoi(ptr2);
    *ptr = ','; //Restore ,
      
    ptr2 = ptr;
    ptr2++; //pointing url
    //fprintf(stderr, "Achint - url %d, %s\n", __LINE__, ptr);

    if(len < MAX_URL_LEN)
      memcpy(url_name, ptr2, len); // Using memcpy as it is faster than strcpy
    else
    {
      len = MAX_URL_LEN;
      memcpy(url_name, ptr2, len); // Using memcpy as it is faster than strcpy
    }
    url_name[len] = '\0'; // NULL Terminate

    if(url_hash_code == 0) 
    {
      //Here we are also finding the hash value and hash id of the static urls. 
      
      //This is static url id. We should not copy this URL to urt.csv file
      //max_static_is_in_gen++;
      //Doing - 1 for MaxStaticUrlIds because ids starts from 0
      //if(max_static_is_in_gen > (MaxStaticUrlIds - 1))

      url_hash_id = nslb_hash_get_hash_index((unsigned char *)url_name, len, &(ihashValue), DynamicTableSizeiUrl);
      url_hash_code = ihashValue ; 
      /*
      if(max_static_is_in_gen > (MaxStaticUrlIds))
      {

        //Error case. For a generator max static ids should not increase the 
        //MaxStaticUrlIds which is max static ids on Controller.
        fprintf(stderr, "Maximum static ids(%d) for generator %d are increased from the total static ids(%d) provided by the controller. It should not happen. Exiting... \n", max_static_is_in_gen, generator_id, MaxStaticUrlIds);
        exit(0);
      }
      */
      //This is static URL so need no to log this in urt.csv file
      //continue;
    }

    //fprintf(stderr, " url_id = %d, url_hash_id = %d, url_hash_code = %d, len = %d, url_name = %s\n", url_id,  url_hash_id, url_hash_code, len, url_name);
    //normalize_url_id = get_url_norm_id_for_generator(& generator_id, url_id, &is_new_record);
    //get_url_norm_id_for_generator(char *gen_name, int gen_len, int gen_id, int gen_url_index, int *is_new_url)
    normalize_url_id = get_url_norm_id_for_generator(url_name, len, generator_id, url_idx, &is_new_record);
    if(is_new_record){
      fprintf(ctrl_urt_fptr, "%d,%d,%s", gen_test_idx, normalize_url_id, ptr_from_page_id);
      is_new_record = 0;
    }
  } //Generator urt.csv while loop
  fclose(netcloud_gen_urt_fptr);
  netcloud_gen_urt_fptr = NULL;
}

static void read_gen_urc_file_and_update_ctrl_urc_file(int gen_test_idx, int generator_id, int ctrl_testidx, char *gen_name)
{
  FILE *netcloud_gen_urc_fptr = NULL;
  char file[4096];
  char buf[MAX_LINE_LEN];
  char *ptr = NULL;
  char *ptr2 = NULL;

  sprintf(file, "%s/logs/TR%d/NetCloud/%s/TR%d/reports/csv/urc.csv", wdir, ctrl_testidx, gen_name, gen_test_idx);
  if (!(netcloud_gen_urc_fptr = fopen(file, "r"))) {   
    perror("ns_gen_url_id_normalization");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", file, gen_test_idx);
  }
  while (nslb_fgets(buf, MAX_LINE_LEN, netcloud_gen_urc_fptr, 0)) {
    //2549,23,1,0,8,7,0,2,0,0,0,0,1,6679,0,0,6679,6679,6679,6679,6679,6679,0,6679,302,0,510,0,0,157,0,0,0,18,1,0,-1,0,1
    ptr = strchr(buf, ',');
    if(ptr == NULL)
      continue;
    ptr++;
    //This is done for optimization, we will directly write whole buffer after this ","
    ptr2 = strchr(ptr, ',');
    if(ptr2 == NULL)
      continue;
    *ptr2 = '\0';
    //After above statement ptr will point to the url id
    url_id = atoi(ptr);

    ptr2++;

    //fprintf(stderr, " url_id = %d\n", url_id);
    normalize_url_id = get_norm_id_from_nvm_table(url_id, generator_id, 0);
    //fprintf(stderr, " normalize_url_id = %d\n", normalize_url_id);
    fprintf(ctrl_urc_fptr, "%d,%d,%s", gen_test_idx, normalize_url_id, ptr2);
  } //Generator urc.csv while loop
  fclose(netcloud_gen_urc_fptr);
  netcloud_gen_urc_fptr = NULL;
}


int  main (int argc, char** argv)
{
  char file[4098];
  char buf[MAX_LINE_LEN];
  int buf_len;
  FILE *netcloud_data_fptr = NULL;
  char *fields[100];
  char keyword[256];
  char data[4096];
  int c;
  int ctrl_testidx = 0;
  int generator_id = -1;
  int gen_test_idx = 0;

  while ((c = getopt(argc, argv, "D:W:l:d:t:Z:i:")) != -1) {
    switch (c) {

    case 't':
      ctrl_testidx = atoi(optarg);
      break;

   case 'W':
      strcpy(wdir, optarg);
      break;

   case 'Z':
      DynamicTableSizeiUrl = atoi(optarg);
      break;

   case 'i':
      MaxStaticUrlIds = atoi(optarg);
      break;

    case '?':
      printf("ns_gen_url_id_normalization -t <controller TR>");
      exit(-1);
    }
  }

  //Read NetCloud.data file and find Generator TR
  sprintf(file, "%s/logs/TR%d/NetCloud/NetCloud.data", wdir, ctrl_testidx);
  if (!(netcloud_data_fptr = fopen(file, "r"))) {   
    perror("ns_gen_url_id_normalization");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", file, ctrl_testidx);
  }

  sprintf(file, "%s/logs/TR%d/reports/csv/urt.csv", wdir, ctrl_testidx);
  if (!(ctrl_urt_fptr = fopen(file, "w"))) {   
    perror("ns_gen_url_id_normalization");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", file, ctrl_testidx);
  }

  sprintf(file, "%s/logs/TR%d/reports/csv/urc.csv", wdir, ctrl_testidx);
  if (!(ctrl_urc_fptr = fopen(file, "w"))) {   
    perror("ns_gen_url_id_normalization");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", file, ctrl_testidx);
  }

  object_norm_init(NULL, 0, 0);
  generator_id = 0;

  while (nslb_fgets(buf, MAX_LINE_LEN, netcloud_data_fptr, 0))
  {
    buf_len = strlen(buf);
    if (buf_len > 0)
      buf[buf_len - 1] = '\0';

    sscanf(buf, "%s %s", keyword, data);
    if(strcmp(keyword, "NETCLOUD_GENERATOR_TRUN")) 
      continue;
    
    get_tokens_ex2(data, fields, "|", 100);
    gen_test_idx = atoi(fields[0]);
    //open gen's urt.csv and urc.csv and update into controller csv files
    
    read_gen_urt_file_and_update_ctrl_urt_file(gen_test_idx, generator_id, ctrl_testidx, fields[1]);
    read_gen_urc_file_and_update_ctrl_urc_file(gen_test_idx, generator_id, ctrl_testidx, fields[1]);
    
    generator_id++;
  } //NetCloud.data while loop
  fclose(ctrl_urt_fptr);
  ctrl_urt_fptr = NULL;
  fclose(ctrl_urc_fptr);
  ctrl_urc_fptr = NULL;
  return 0;
}
