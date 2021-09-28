
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
#include "nslb_get_norm_obj_id.h"
#include "nslb_util.h"
#include "ns_common.h"
#include "ns_log.h"
#include "ns_exit.h"
#include "ns_test_init_stat.h"
#include "ns_error_msg.h"

#ifndef CAV_MAIN
static NormObjKey ParentnormPageTable;
static NormObjKey ParentnormSessionTable;
extern NormObjKey ParentnormRunProfTable;
extern NormObjKey ParentnormGroupRunProfTable;
#else
static __thread NormObjKey ParentnormPageTable;
static __thread NormObjKey ParentnormSessionTable;
extern __thread NormObjKey ParentnormRunProfTable;
extern __thread NormObjKey ParentnormGroupRunProfTable;
#endif
extern int loader_opcode;
extern int testidx;
extern char master_sub_config[10];

inline void write_buffer_into_csv_file (char *file_name, char *buffer, int size)
{
  char filename_wth_path[512];

  NSDL1_LOGGING(NULL, NULL, "Method called File = %s, buffer = %s, size = %d", file_name, buffer, size);
  snprintf(filename_wth_path, 512, "%s/logs/TR%d/common_files/reports/csv/", g_ns_wdir, testidx);
  mkdir_ex(filename_wth_path);
  snprintf(filename_wth_path, 512, "%s/logs/TR%d/common_files/reports/csv/%s", g_ns_wdir, testidx, file_name);
  FILE *fp = fopen(filename_wth_path, "a+");
  if(!fp)
  {
    NS_EXIT (1, "File = %s, Function = %s, Error in opening file %s to write. Error = %s\n", __FILE__, (char *)__FUNCTION__, filename_wth_path, nslb_strerror(errno));
  }
 
  fwrite(buffer, sizeof(char), size, fp);
  fclose(fp);
}

inline void build_norm_table_from_csv_files()
{
  /* Normalized Table Recovery Map (ntrmap)*/
  struct 
  {
   NormObjKey *key; 
   char csvname[64];
   char name_fieldnum;
   char len_fieldnum;
   char normid_fieldnum;
  } ntrmap[] =

  { 
    //7010,1,0,login,13,script1:login
    {&ParentnormPageTable, "pgt.csv", 5, 4, 1},
    //7010,0,hpd_tours_c_1
    {&ParentnormSessionTable, "sst.csv", 2, -1, 1},
    //6666,0,0,0,1,g1,0,grp:userprof:session
    {&ParentnormRunProfTable, "rpf.csv", 7, -1, 6},
    //6666,0,0,0,1,g1,0,grp:userprof:session
    {&ParentnormGroupRunProfTable, "rpf.csv", 5, -1, 1},
    //1001,3,Tx_AddToBag
  //  {&normTxTable, "trt.csv", 2, -1, 1},
    //MAC_66_WORK4,0,/home/cavisson/work4,192.168.1.66,192.168.1.66,7891
   // {&normGeneratorTable, "generator_table.csv", 0, -1, 1},
  };
  

  nslb_init_norm_id_table(&ParentnormPageTable, 8192);
  nslb_init_norm_id_table(&ParentnormSessionTable, 8192);
  nslb_init_norm_id_table(&ParentnormRunProfTable, 8192);
  nslb_init_norm_id_table(&ParentnormGroupRunProfTable, 8192);

  int num_obj_types = sizeof(ntrmap) / sizeof(ntrmap[0]); // Num of object types
  int obj_iter;
  int error;
  for(obj_iter = 0; obj_iter < num_obj_types; obj_iter++)
  {
    error = 0;
    /* Open csv file */
    char filename[512];
    
    //On generator we will always have csv files from controller
    //On generator csv files will inside the $NS_WDIR/scripts/.meta_data_files directory
    if (loader_opcode == CLIENT_LOADER)
    {
      /*bug id: 101320: moved .meta_data_files to $NS_WDIR/.tmp, as its not a test assets*/
      snprintf(filename, 512, "%s/.tmp/.meta_data_files/%s", g_ns_wdir, ntrmap[obj_iter].csvname);
      /*Bug 39240: When sub config is NVSM and rpf.csv is not found then generator will die
        In NVSM multiple scripts are used whose normalised csv must be present in generators
        hence exiting if not found in generator */
      if(!strcasecmp(master_sub_config, "NVSM"))
        error = 1;
    }
    else
      snprintf(filename, 512, "%s/logs/TR%d/common_files/reports/csv/%s", g_ns_wdir, testidx, ntrmap[obj_iter].csvname);
 
    FILE *fp = fopen(filename, "r");
    if(!fp)
    {
      NSDL1_LOGGING(NULL, NULL, "Could not open file %s,"
                   " for reading while creating in memory normalised table\n", filename);
      if(error)
      {
        NS_EXIT(1, CAV_ERR_1031011, filename, master_sub_config);
      }
      continue;
    }

    /* Find the max number of fields to be read */ 
    int max = 0;
    if(ntrmap[obj_iter].name_fieldnum > max) max = ntrmap[obj_iter].name_fieldnum;
    if(ntrmap[obj_iter].len_fieldnum > max) max = ntrmap[obj_iter].len_fieldnum;
    if(ntrmap[obj_iter].normid_fieldnum > max) max = ntrmap[obj_iter].normid_fieldnum;
    max++; //add 1 to max as max is a number and field num is index
       
    NSDL1_LOGGING(NULL, NULL, "max = %d", max);

    /* Read line by line */
    char line[64*1024];
    while(nslb_fgets(line, 64*1024, fp, 0))
    {
      /* Split the fields of line */
      char *fields[max];
      int num_fields;
      num_fields = get_tokens_with_multi_delimiter(line, fields, ",", max); 
      if(num_fields < max)
      {
         NSDL1_LOGGING(NULL, NULL, "Error in reading file %s," 
                        "while creating in memory normalised table. "
                        "Number of fields are less than expected in the read line '%s'\n", filename, line);
        continue; // Skip this line
      }

      /* Now set the normalized ID */
      /*unsigned int nslb_set_norm_id(NormObjKey *key, char *in_str, int in_strlen, unsigned int normid)*/

      char *newline_ptr = NULL;
      newline_ptr = strstr(fields[max -1], "\n");
      if(newline_ptr != NULL)
        *newline_ptr = '\0'; 

      if(ntrmap[obj_iter].len_fieldnum == -1) //Since len field is not there, use strlen 
        nslb_set_norm_id(ntrmap[obj_iter].key, 
                         fields[(int)(ntrmap[obj_iter].name_fieldnum)], 
                         strlen(fields[(int)(ntrmap[obj_iter].name_fieldnum)]), 
                         atoi(fields[(int)(ntrmap[obj_iter].normid_fieldnum)])); 
      else
        nslb_set_norm_id(ntrmap[obj_iter].key, 
                         fields[(int)(ntrmap[obj_iter].name_fieldnum)], 
                         atoi(fields[(int)(ntrmap[obj_iter].len_fieldnum)]), 
                         atoi(fields[(int)(ntrmap[obj_iter].normid_fieldnum)])); 
    }
    if(fp) 
      fclose(fp); 
    fp = NULL;
  }
}

int get_norm_id_for_session(char *sess_name)
{
  int is_new_id = 0;
  char buffer[4096];
  int buff_len;
  int norm_id = nslb_get_or_gen_norm_id(&ParentnormSessionTable, sess_name, strlen(sess_name), &is_new_id);
  NSDL1_LOGGING(NULL, NULL, " Session norm_id = %d, is_new_id = %d", norm_id, is_new_id);
  if(is_new_id)
  {
    //new entry write into csv file 
    buff_len = sprintf(buffer, "%d,%d,%s\n", testidx, norm_id, sess_name);
    write_buffer_into_csv_file("sst.csv", buffer, buff_len);
  }
  return norm_id;
}

 //There can be chances that user has changed the page name
//In continues monitoring old page name and new page name should be treated as same 
//and norm ids should be same for both page names. New page name will come with old page name with : as separator
int get_norm_id_for_page(char *old_new_page_name, char *sess_name, int sess_norm_id)
{
  int combine_sess_page_name_len;
  int is_new_page_flag = 0; 
  char buffer[1024];
  char session_and_page_name[1024 + 1024 + 1] = {0};
  int buff_len;
  char *ptr, *ptr2;
  int new_page_norm_id;
  int old_page_norm_id;

  ptr = strchr(old_new_page_name, ':');
  if(ptr == NULL)
  {
    combine_sess_page_name_len = sprintf(session_and_page_name, "%s:%s", sess_name, old_new_page_name);
    new_page_norm_id = nslb_get_or_gen_norm_id(&ParentnormPageTable, session_and_page_name, combine_sess_page_name_len, &is_new_page_flag);
    NSDL1_LOGGING(NULL, NULL, " page norm_id = %d, is_new_page_flag = %d", new_page_norm_id, is_new_page_flag);
    if(is_new_page_flag)
    {
      buff_len = sprintf(buffer, "%d,%d,%u,%s,%d,%s\n", testidx, new_page_norm_id, sess_norm_id, old_new_page_name, combine_sess_page_name_len, session_and_page_name);
      write_buffer_into_csv_file("pgt.csv", buffer, buff_len);
    }
  }
  else
  {
    ptr2 = ptr; 
    ptr++;
    *ptr2 = '\0';

    //New page name with old page name. 
    //First find if old page name is already there. 
    combine_sess_page_name_len = sprintf(session_and_page_name, "%s:%s", sess_name, ptr);
    old_page_norm_id = nslb_get_norm_id(&ParentnormPageTable, session_and_page_name, combine_sess_page_name_len);

    //Using new page as pagename to get the norm id
    combine_sess_page_name_len = sprintf(session_and_page_name, "%s:%s", sess_name, old_new_page_name);
    new_page_norm_id = nslb_get_norm_id(&ParentnormPageTable, session_and_page_name, combine_sess_page_name_len);

    //New page id not found --
    //Case1: Old page id found then add old page id with new page name
    //Case2: a: Old page id not found and 
    if(new_page_norm_id == -2) 
    {
      if(old_page_norm_id >= 0)
      {
        //Old page id founded, so just add new page with old page id.
        //buff_len = sprintf(buffer, "%d,%d,%u,%s,%d,%s", testidx, old_page_norm_id, gSessionTable[sess_idx].sess_norm_id, old_new_page_name, combine_sess_page_name_len, session_and_page_name);
        new_page_norm_id = old_page_norm_id;
      }
      else
      {
        //New page norm id not found also old page id not found
        //Writting new page id into resposiotry and adding into csv file
        new_page_norm_id = nslb_get_or_gen_norm_id(&ParentnormPageTable, session_and_page_name, combine_sess_page_name_len, &is_new_page_flag);
      }
      buff_len = sprintf(buffer, "%d,%d,%u,%s,%d,%s\n", testidx, new_page_norm_id, sess_norm_id, old_new_page_name, combine_sess_page_name_len, session_and_page_name);
      write_buffer_into_csv_file("pgt.csv", buffer, buff_len);
    }
  }
  return new_page_norm_id;
}

