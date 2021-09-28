
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <sys/ioctl.h>
#include <assert.h>
//#include <linux/cavmodem.h>
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>

#include "url.h"
//#include "tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
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
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "init_cav.h"
#include "ns_parse_src_ip.h"
#include "nslb_sock.h"
#include "ns_trans_parse.h"
#include "ns_custom_monitor.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
//#include "ns_handle_read.h"
#include "ns_goal_based_sla.h"
#include "ns_tag_vars.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "poi.h"
#include "amf.h"
#include "ns_string.h"
#include "ns_cookie.h"
#include "ns_vars.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_index_vars.h"
#include "ns_vars.h"
#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_event_log.h" //for EL_ etc macros 
#include "ns_unique_numbers.h"
#include "ns_date_vars.h"
#include "ns_static_use_once.h"
#include "nslb_time_stamp.h"
#include "ns_script_parse.h"
#include "ns_event_id.h"
#include "ns_unique_range_var.h"
#include "nslb_hash_code.h"
#include "ns_uri_encode.h"

#ifndef CAV_MAIN
extern int cur_post_buf_len;
#else
extern __thread int cur_post_buf_len;
#endif

#define VALUE 0
#define COUNT 1
#define NUM   2

#define NO_CAVREPEAT_BLOCK    0
#define CAVREPEAT_BLOCK_START 1
#define CAVREPEAT_BLOCK_END   2

int process_repeat_block (char *line, char **tmp_ptr, int *segment_idx, int rnum, int sess_idx, int line_number, char *fname);
int check_if_repeat_block(char *line_ptr);

// This will be called in multipart only 
void segment_line_noparam_multipart (StrEnt* segtable, char* line, int sess_idx)
{
  int rnum;
  int point_rnum;

  NSDL1_VARS(NULL, NULL, "Method Called.");

  if (create_seg_table_entry(&rnum) != SUCCESS) {

    NS_EXIT(-1, "segment_line(): could not get new seg table entry\n");
  }
  AddPointerTableEntry (&point_rnum, line, cur_post_buf_len);

  segtable->num_entries++;
  segTable[rnum].type = STR;
  segTable[rnum].offset = point_rnum;
}

void segment_line_noparam (StrEnt* segtable, char* line,int sess_idx)
{
  //int first_segment = 1;
  int rnum;
  int point_rnum;

  NSDL1_VARS(NULL, NULL, "Method Called.");

  if (create_seg_table_entry(&rnum) != SUCCESS) {

    NS_EXIT(-1, "segment_line(): could not get new seg table entry\n");
  }
  segTable[rnum].sess_idx = sess_idx;

  segtable->seg_start = rnum;
  //first_segment = 0;
  segtable->num_entries++;

  AddPointerTableEntry (&point_rnum, line, cur_post_buf_len);

  segTable[rnum].type = STR;
  segTable[rnum].offset = point_rnum;
}

typedef struct openSegDetail{
  int idx;  /*repeatBlock table idx*/
  short parent_idx;
}openSegDetail;



#define MAX_NEXTED_SEGMENT 500
#define COUNT_IDX  -1
// Rename segment_line to segment_line_int for supproting cavinclude and cavinclude_noparam in a single multipart request
// Rename segment_line_int to segment_line_int_int for passing one more argument of int array
void
segment_line_int_int(StrEnt* segtable, char* line, int url, int line_number, int sess_idx, char *fname, int first_segment, int *hash_arr)
{
  char* tmp_line = line;
  //int first_segment = 1;
  int ret_val, hash_arr_idx=0;
  char var[MAX_LINE_LENGTH];
  char absol_var[MAX_LINE_LENGTH];
  int var_length;
  DBT key;
  DBT data;
  int rnum;
  int cookie_idx;
  //  int dynvar_idx;
  int tagvar_idx;
  int nslvar_idx;
  int searchvar_idx;
  int jsonvar_idx;   //for JSON var
  int randomvar_idx = -1;
  int randomstring_idx = -1;
  int uniquevar_idx = -1;
  int unique_range_var_idx = -1;
  int datevar_idx = -1;
  int clustvar_idx;
  int groupvar_idx;
  char* var_end;
  int hash_code;
  int point_rnum;
  int str_len;
  char *next_start;
  int open_seg_count = 0;
  int segment_idx;
  openSegDetail open_seg[MAX_NEXTED_SEGMENT] = {{-1, -1}};
  char new_url[35000 * 3] = "";
  //char uri[35000] = "";

  /*Variables for _index support*/
  int ord_all_idx = 0;  /*  Index of parameter */
  char index_flag = 0;  /* flag to check if parameter used with index*/
  char *vec_idx_ptr;
  int new_var_length;
  char *tmp_ptr;
  int new_len = 0;
  int first_str_seg = 0;

  NSDL1_VARS(NULL, NULL, "Method Called.");

  while (tmp_line && (*tmp_line != '\0')) 
  {
    NSDL4_VARS(NULL, NULL, "tmp_line = %s", tmp_line);
   
    // Repeat block end need to be check first, as we need to take decision whether to create new entry in seg table 
    // Added to support repeat block in netstorm 
    if (line[0] == '{' && (check_if_repeat_block(line) == CAVREPEAT_BLOCK_END) && open_seg_count > 0)
    {
      NSDL1_VARS(NULL, NULL, "Found CAVREPEAT_BLOCK_END");
      repeatBlock[open_seg[open_seg_count - 1].idx].agg_repeat_segments += repeatBlock[open_seg[open_seg_count - 1].idx].num_repeat_segments;
      if(open_seg[open_seg_count - 1].parent_idx != -1)
      {
        repeatBlock[open_seg[open_seg_count - 1].parent_idx].agg_repeat_segments += repeatBlock[open_seg[open_seg_count - 1].idx].agg_repeat_segments;
      }
      // Reset open seg array so that it can be reused
      open_seg[open_seg_count - 1].idx = -1;
      open_seg[open_seg_count - 1].parent_idx = -1;
      open_seg_count --;
      tmp_line = line = line + 21 + 1; // +1 is done for skipping bracket 
      continue;
    }

    if (create_seg_table_entry(&rnum) != SUCCESS) {
      NS_EXIT(-1, "segment_line(): could not get new seg table entry\n");
    }
    segTable[rnum].sess_idx = sess_idx;

    if (first_segment) {
      segtable->seg_start = rnum;
      first_segment = 0;
    }

    segtable->num_entries++;
   
    if (line[0] == '{')
    {
     int ret;
     NSDL4_VARS(NULL, NULL, "Start of var detected");
     ret = check_if_repeat_block(line); // check for repeat block start
     if(ret == CAVREPEAT_BLOCK_START)
     {
       int tmp_ret;
       NSDL4_VARS(NULL, NULL, "Start of REPEAT block detected");
       if((tmp_ret = process_repeat_block(line, &tmp_ptr, &segment_idx, rnum, sess_idx, line_number, fname)) != -1){
         open_seg[open_seg_count].idx = segTable[segment_idx].offset;
         //initialize counters
         repeatBlock[open_seg[open_seg_count].idx].num_repeat_segments = 0;
         repeatBlock[open_seg[open_seg_count].idx].agg_repeat_segments = 0;
         if(open_seg_count >= 1){
           open_seg[open_seg_count].parent_idx = open_seg[open_seg_count - 1].idx;
           repeatBlock[open_seg[open_seg_count].parent_idx].num_repeat_segments++;
         }
         open_seg_count++;
       
         line = tmp_line = tmp_ptr + 1; // Increment tmp_ptr to skip '}'
         continue;
       } 
     }
     // 2nd coniditon is for Checking if there is any start braces to handle following case
     // "TASK search  free_text=&quot;{login_name_var}&quot;;"
     // Note Another  may be after the variable
     if ((var_end = strchr(line, '}')) && (((next_start = strchr(line + 1, '{')) == NULL)  || (next_start > var_end))) {
	      var_length = var_end - line - 1;
	      if(var_length > MAX_LINE_LENGTH)
              {
                var_length = MAX_LINE_LENGTH - 1;
                NSTL1(NULL, NULL, "var_length after truncate = %d", var_length);
              }
              memcpy(var, line+1, var_length);
	      var[var_length] = '\0';
        ord_all_idx = index_flag = 0;
        NSDL1_VARS(NULL, NULL, "var found. var name = %s", var);
	   if (gSessionTable[sess_idx].var_hash_func) {
	    hash_code = gSessionTable[sess_idx].var_hash_func(var, var_length);
	  // if hash_code is -1. then  var is not declared in the script which is OK
      if(hash_code == -1)
      {
      //Check if parameter is used with _index(index can be 1-.. and count)
      if((vec_idx_ptr = rindex(var, '_')) != NULL){
        new_var_length = vec_idx_ptr - var;
        if((hash_code = gSessionTable[sess_idx].var_hash_func(var, new_var_length)) != -1){
          short var_type = gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type;
          
          /*Here we have another check because right now we are supporting index and count in NSL var*/
          if(((var_type != SEARCH_VAR) && (var_type != JSON_VAR) && (var_type != NSL_VAR) && (var_type != TAG_VAR)) ||
                 //(Check if var is of SEARCH/NSL/TAG type or not)
          !(gSessionTable[sess_idx].var_type_table_shr_mem[gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].user_var_table_idx]) 
            ) {
            NSDL1_VARS(NULL, NULL, "Var %*s used with index, is of scalar type or not NSL Var.Treating as String", var_length, var);
            //setting hash_code to -1
            hash_code = -1;
          }
          else {
            vec_idx_ptr += 1; /* pointed to next to underscore(_) */
            if(!ns_is_numeric(vec_idx_ptr)){
              if(!strcmp(vec_idx_ptr, "count")){
                index_flag = 1;
                ord_all_idx = COUNT_IDX;
                var_length = new_var_length;
              }else
                hash_code = -1;
            }else if(atoi(vec_idx_ptr) <= 0)  /*index should be more than 0*/
              hash_code = -1;
            else {
              index_flag = 1;
              ord_all_idx = atoi(vec_idx_ptr);
              NSDL2_VARS(NULL, NULL, "Var %*s used with index %d", new_var_length, var, atoi(vec_idx_ptr));
              var_length = new_var_length;
            }
          }
        } 
      }
      if(hash_code == -1)
   	    NSDL1_VARS(NULL, NULL, "var not declared");
      }

    #if 0
    /*Check if variable is of NSL_VAR and it is vector and used without index then treat it as string */
    if(hash_code != -1 && gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type == NSL_VAR && 
              gSessionTable[sess_idx].var_type_table_shr_mem[gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].user_var_table_idx] 
              && !index_flag){
      NSDL1_VARS(NULL, NULL, "Var %s is and Vector Parameter and used without Index, will be treated as String", var);
      hash_code = -1;
    } 
    #endif
    } else {
       NSDL1_VARS(NULL, NULL, "Hash function does not exist for sess_idx = %d.", sess_idx);
       hash_code = -1;
    }

    if(hash_arr)
    {
      NSDL1_VARS(NULL, NULL, "hash_arr_idx =  %d", hash_arr_idx);
      if(hash_arr_idx < ITEMDATA_HASH_ARRAY_SIZE - 1){
        hash_arr[hash_arr_idx] = hash_code;
        NSDL1_VARS(NULL, NULL, "Hash code for idx %d is %d", hash_arr_idx, hash_code);
      } 
      else NSDL1_VARS(NULL, NULL, "Number of hash exceeded max size of %d", ITEMDATA_HASH_ARRAY_SIZE);
      hash_arr_idx++;
    }

    if (hash_code != -1) 
    {
      short is_file =0;
      switch (gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type) 
      {
        case VAR:
        case INDEX_VAR:
          is_file= varTable[gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].fparam_grp_idx].is_file;
	  NSDL1_VARS(NULL, NULL, "AG:: is_file of var %s is %d. gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].fparam_grp_idx = %d sess_idx = %d, hash_code = %d", var, is_file, gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].fparam_grp_idx, sess_idx, hash_code);
          if ((is_file == IS_FILE_PARAM) && (line_number < 0))
            NS_EXIT(-1, "Variable '%s' can not use inside data file. It is already FILE_PARAM type", var);
           
	  memset(&key, 0, sizeof(DBT));
	  memset(&data, 0, sizeof(DBT));

	  sprintf(absol_var, "%s!%s", var, 
                                      get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), 
                                                                          sess_idx, "-"));
	  //sprintf(absol_var, "%s!%s", var, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
	  key.data = absol_var;
	  key.size = strlen(absol_var);

	  ret_val = var_hash_table->get(var_hash_table, NULL, &key, &data, 0);

	  if (ret_val == DB_NOTFOUND) {
	    NS_EXIT(-1, "%s(): Variable %s not defined. Used on line=%d in file %s\n",
                    (char*)__FUNCTION__, var, line_number, fname);
	  }

	  if (ret_val != 0) {
	    NS_EXIT(-1, "%s(): Hash Table Get failed for NS var '%s'on line=%d in file %s\n",
                    (char*)__FUNCTION__, var, line_number, fname);
	  }

	  segTable[rnum].type = gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type;
	  segTable[rnum].offset = (int) atoi((char*)data.data);

	  NSDL1_VARS(NULL, NULL, "Parsing static var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  break;

	case COOKIE_VAR:
          // For Auto Cookie Mode, we are not supporting COOKIE Vars for now. So code will come
          // here only for Manual Cookies
	  cookie_idx = Create_cookie_entry(var, sess_idx);
	  segTable[rnum].type = COOKIE_VAR;
	  segTable[rnum].offset = cookie_idx;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing cookie var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

	case SEARCH_VAR:
          // Added var_length as now we are supporting underscore
	  if ((searchvar_idx = find_searchvar_idx(var, var_length, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the search var %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }

	  segTable[rnum].type = SEARCH_VAR;
	  segTable[rnum].offset = searchvar_idx;    /* this value is changed later to the usertable variable index*/
      
          if(index_flag)
            segTable[rnum].data = copy_into_big_buf((char *)&ord_all_idx, sizeof(int));
          else
            segTable[rnum].data = -1;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing search var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;
      
        case JSON_VAR:
          // Added var_length as now we are supporting underscore
          if ((jsonvar_idx = find_jsonvar_idx(var, var_length, sess_idx)) == -1) {
            NS_EXIT(-1, "%s(): Could not find the json var %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
          }

          segTable[rnum].type = JSON_VAR;
          segTable[rnum].offset = jsonvar_idx;    /* this value is changed later to the usertable variable index*/
      
          if(index_flag)
            segTable[rnum].data = copy_into_big_buf((char *)&ord_all_idx, sizeof(int));
          else
            segTable[rnum].data = -1;

          tmp_line = strchr(tmp_line, '}') + 1;
          line = tmp_line;
          NSDL1_VARS(NULL, NULL, "Parsing JSON var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break; 

	case TAG_VAR:
          // Added var_length as now we are supporting underscore
	  if ((tagvar_idx = find_tagvar_idx(var, var_length, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the tag var variable %s on line = %d file %s\n",
                             (char*)__FUNCTION__, var, line_number, fname);
	  }

	  segTable[rnum].type = TAG_VAR;
	  segTable[rnum].offset = tagvar_idx;  /* this value is changed later to the usertable variable index */
      
          if(index_flag)
            segTable[rnum].data = copy_into_big_buf((char *)&ord_all_idx, sizeof(int));
          else
            segTable[rnum].data = -1;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing tag(xml) var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

        case NSL_VAR:
	  if ((nslvar_idx = find_nslvar_idx(var, var_length, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the nsl var variable %s on line = %d file %s\n",
                        (char*)__FUNCTION__, var, line_number, fname);
	  }

	  segTable[rnum].type = NSL_VAR;
	  segTable[rnum].offset = nslvar_idx;  /* this value is changed later to the usertable variable index */

          if(index_flag)
            segTable[rnum].data = copy_into_big_buf((char *)&ord_all_idx, sizeof(int));
          else 
            segTable[rnum].data = -1;

   	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing NSL var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

        case CLUST_VAR:
	  if ((clustvar_idx = find_clustvar_idx(var)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the clust var %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }

	  segTable[rnum].type = CLUST_VAR;
	  segTable[rnum].offset = clustvar_idx;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing cluster var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

        case GROUP_VAR:
	  if ((groupvar_idx = find_groupvar_idx(var)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the group var %s on line = %d file %s \n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }

	  segTable[rnum].type = GROUP_VAR;
	  segTable[rnum].offset = groupvar_idx;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing group var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

	case GROUP_NAME_VAR:
	  segTable[rnum].type = GROUP_NAME_VAR;
	  segTable[rnum].offset = 0;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing group name var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

	case CLUST_NAME_VAR:
	  segTable[rnum].type = CLUST_NAME_VAR;
	  segTable[rnum].offset = 0;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing cluster name var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

	case USERPROF_NAME_VAR:
	  segTable[rnum].type = USERPROF_NAME_VAR;
	  segTable[rnum].offset = 0;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing user profile name var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

	case HTTP_VERSION_VAR:
	  segTable[rnum].type = HTTP_VERSION_VAR;
	  segTable[rnum].offset = 0;

	  tmp_line = strchr(tmp_line, '}') + 1;
	  line = tmp_line;
	  NSDL1_VARS(NULL, NULL, "Parsing http version var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

        case RANDOM_VAR:
	  if ((randomvar_idx = find_randomvar_idx(var, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the Random var %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }
          segTable[rnum].type = RANDOM_VAR;
          segTable[rnum].offset = randomvar_idx;
          tmp_line = strchr(tmp_line, '}') + 1;
          line = tmp_line;
          NSDL1_VARS(NULL, NULL, "Parsing random var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break;

        case RANDOM_STRING:
          if ((randomstring_idx = find_randomstring_idx(var, sess_idx)) == -1) {
            NS_EXIT(-1,  "%s(): Could not find the Random string %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
          }
          segTable[rnum].type = RANDOM_STRING;
          segTable[rnum].offset = randomstring_idx;
          tmp_line = strchr(tmp_line, '}') + 1;
          line = tmp_line;
          NSDL1_VARS(NULL, NULL, "Parsing random string , runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break;
  
        case UNIQUE_VAR:
          if ((uniquevar_idx = find_uniquevar_idx(var, sess_idx)) == -1) {
            NS_EXIT(-1, "%s(): Could not find the Unique var %s on line = %d file %s\n",
                             (char*)__FUNCTION__, var, line_number, fname);
          }
          segTable[rnum].type = UNIQUE_VAR;
          segTable[rnum].offset = uniquevar_idx;
          tmp_line = strchr(tmp_line, '}') + 1;
          line = tmp_line;
          NSDL1_VARS(NULL, NULL, "Parsing unique var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break;

        case UNIQUE_RANGE_VAR:
          if ((unique_range_var_idx = find_unique_range_var_idx(var, sess_idx)) == -1) {
            NS_EXIT(-1, "%s(): Could not find the Unique range var %s on line = %d file %s\n",
                             (char*)__FUNCTION__, var, line_number, fname);
          }
          segTable[rnum].type = UNIQUE_RANGE_VAR;
          segTable[rnum].offset = unique_range_var_idx;
          tmp_line = strchr(tmp_line, '}') + 1;
          line = tmp_line;
          NSDL1_VARS(NULL, NULL, "Parsing unique range var, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break;

        case DATE_VAR:
          if ((datevar_idx = find_datevar_idx(var, sess_idx)) == -1) {
            NS_EXIT(-1, "%s(): Could not find the date var  %s on line = %d file %s\n",
                             (char*)__FUNCTION__, var, line_number, fname);
          }
          segTable[rnum].type = DATE_VAR;
          segTable[rnum].offset = datevar_idx;
          tmp_line = strchr(tmp_line, '}') + 1;
          line = tmp_line;
          NSDL1_VARS(NULL, NULL, "Parsing date var , runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break;
 
        default:
	  NS_EXIT(-1, "%s(): Unsupported Var type  var %s on line = %d file %s \n",
                          (char*)__FUNCTION__, var, line_number, fname);
      }

      // Repeat Block segment
      if(open_seg_count)
        repeatBlock[open_seg[open_seg_count - 1].idx].num_repeat_segments++;

      continue;
    } 
    else 
    {
      hash_arr_idx++;
      if(url) 
      {
        first_str_seg++;
        new_len = ns_encode_char_in_url(line, var_length + 2, new_url, first_str_seg);
	NSDL1_VARS(NULL, NULL, "new_url %s new_len = %d", new_url, new_len); 
	AddPointerTableEntry (&point_rnum, new_url, new_len);
      }
      else 
       AddPointerTableEntry (&point_rnum, line, var_length + 2);

      NSDL1_VARS(NULL, NULL, "Adding STR segment. Len = %d, point_rnum = %d, Segment = %*.*s", 
		              var_length, point_rnum, var_length, var_length, line);
 
      segTable[rnum].type = STR;
      segTable[rnum].offset = point_rnum;

      tmp_line += (var_length + 2);
      line = tmp_line;

      // Repeat Block segment
      if(open_seg_count)
        repeatBlock[open_seg[open_seg_count - 1].idx].num_repeat_segments++;

      continue;
    }
  } else {
        NSDL1_VARS(NULL, NULL, "This is not a var");
	tmp_line++;
      }
    }

    /* case for regular string segment */
    if (tmp_line)
      if ((tmp_line = strstr(tmp_line, "{"))) {
	       str_len = tmp_line - line;
      }

    if (!tmp_line)
      str_len = strlen(line);

    segTable[rnum].type = STR;
    //encode_char_inurl(line, &new_len);
    // For Repeat Block 
    if(open_seg_count)
      repeatBlock[open_seg[open_seg_count - 1].idx].num_repeat_segments++;

    hash_arr_idx++;
    if(url) {
      first_str_seg++;
      new_len = ns_encode_char_in_url(line, str_len, new_url, first_str_seg);
      NSDL1_VARS(NULL, NULL, "new_url %s", new_url); 

      AddPointerTableEntry (&point_rnum, new_url, new_len);
    }
    else {
      AddPointerTableEntry (&point_rnum, line, str_len);
    }
    //Bug#2442  changing format specifier of segment from %*.*s to %s as it was explicitily 
    //forcing to write the full buffer in logs and the process hanged at vsnprintf trying to write the 
    //complete buffer in case buffer is too large say 200MB.
    NSDL1_VARS(NULL, NULL, "Adding STR segment. Len = %d, point_rnum = %d", str_len, point_rnum);
    NSDL4_VARS(NULL, NULL, "Segment = %s", line);

    segTable[rnum].offset = point_rnum;

    line = RETRIEVE_BUFFER_DATA(pointerTable[segTable[rnum].offset].big_buf_pointer);
    while  ((line = strchr(line, '\n'))) {
      line_number+=1;
      line++;
    }
    line = tmp_line;
    NSDL4_VARS(NULL, NULL, "End line = %s", line);
  }

  // close open repeat segment(if any)
  while(open_seg_count){
    repeatBlock[open_seg[open_seg_count - 1].idx].agg_repeat_segments += repeatBlock[open_seg[open_seg_count - 1].idx].num_repeat_segments;
    if(open_seg[open_seg_count - 1].parent_idx != -1)
    {
      repeatBlock[open_seg[open_seg_count - 1].parent_idx].agg_repeat_segments += repeatBlock[open_seg[open_seg_count - 1].idx].agg_repeat_segments;
    }
    open_seg_count--;
  }
}

//segment_line_int is renamed to segment_line_int_int to pass it one more argument of int array without affecting existing functionalities 
void segment_line_int(StrEnt* segtable, char* line, int url, int line_number, int sess_idx, char *fname, int first_segment){
  segment_line_int_int(segtable, line, url, line_number, sess_idx, fname, first_segment, NULL);
}
// segment_line is renamed to segment_line_int so that we can use it as it diffrently in multipart 
void segment_line(StrEnt* segtable, char* line, int url, int line_number, int sess_idx, char *fname){
  segment_line_int(segtable, line, url, line_number, sess_idx, fname, 1);
}

int check_if_repeat_block(char *line_ptr)
{
  char *line = line_ptr;
  line++; // To skip {
  NSDL1_VARS(NULL, NULL, "Method called Line = %s", line);
  if(!strncmp(line, "$CAVREPEAT_BLOCK_START", 22))
    return CAVREPEAT_BLOCK_START;
  else if(!strncmp(line, "$CAVREPEAT_BLOCK_END}", 21))
    return CAVREPEAT_BLOCK_END;
  else
    return NO_CAVREPEAT_BLOCK;
}


int process_repeat_block(char *line, char **tmp_ptr, int *segment_idx, int rnum, int sess_idx, int line_number, char *fname)
{
  char search_buf[MAX_LINE_LENGTH];
  char tmp_buf[MAX_LINE_LENGTH];
  char rep_seprator[1024] = "\0";
  int hash_code_var;
  int rep_seg_idx = -1;
  char local_repeat_count_type;
  int local_repeat_count;
  int i;

  NSDL1_VARS(NULL, NULL, "Method called, line = %s", line);

  if(!strncmp(line, "{$CAVREPEAT_BLOCK_START", 23))
  {
    line = line + 23;
    NSDL1_VARS(NULL, NULL, "Found CAVREPEAT_BLOCK_START");
    
    // Clear white before , 
    CLEAR_WHITE_SPACE(line); 

    if(line[0] != ',' || line [0] == '\0')
    {
      NS_DUMP_WARNING("Bad format of repeatable block, ','  not found after start block format at line = %d "
                      "in file %s This will be treated as string", line_number, fname);
      return(-1);
    } 
    line++;
 
    // clear white space after ,
    CLEAR_WHITE_SPACE(line);
   
    // Seperator Parsing Start: separator can be a string 
    // Format: {$CAVREPEAT_BLOCK_START, Sep(;),Value(body)}{body}{$CAVREPEAT_BLOCK_END} 
    if(!strncasecmp(line, "sep(", 4)) { // Sep found
      line += 4;
      NSDL3_VARS(NULL, NULL, "Found repeat seprator syntex");
      for(i = 0; (*line != ')' && *line ); line++, i++){
        rep_seprator[i] = *line; 
      }
      if(*line == '\0'){
        NS_DUMP_WARNING("Bad format of repeatable block, separator end ')' not found at line = %d " 
                        "in file %s This will be treated as string", line_number, fname);
        return(-1);
      }
      line++; // skip ')'

      rep_seprator[i] = '\0';

      // clear space after separator
      CLEAR_WHITE_SPACE(line);

      NSDL1_VARS(NULL, NULL, "Found separator in repeat block, value = \'%s\'", rep_seprator);
      if( *line  == ',')
        line++;
      else {
        NS_DUMP_WARNING("Bad format of repeatable block, ',' not found after separator at line = %d " 
                        "in file %s This will be treated as string", line_number, fname);
        return(-1);
      }
      CLEAR_WHITE_SPACE(line);
    }
    // Seperator Parsing End 
    
    // Repeat block condition value parsing start 
    for(i = 0; (*line != '(' && *line ); line++, i++) {
      search_buf[i] = *line;
    }
    search_buf[i] = '\0';
    NSDL3_VARS(NULL, NULL, "rnum = %d, search_buf = %s", rnum, search_buf);
    
    if(*line == '\0') {
      NS_DUMP_WARNING("Bad format of repeatable block, no start bracket found for repeat block condition at line = %d " 
                      "in file %s Thisi will be treated as string", line_number, fname);
      return(-1);
    }

    // Parse repeat condition, It can any one of be value, num and count.
    if (strcasecmp(search_buf, "Value") == 0) // Value case
    {
      local_repeat_count_type = VALUE;
      if(*line == '(')
        line++;
      else{
        NS_DUMP_WARNING("Bad format of repeatable block, no start bracket found for repeat block condition at line = %d "
                        "in file %s This will be treated as string", line_number, fname);
        return(-1);
      }
 
      for (i = 0; (*line != ')') && *line ; line++, i++) {
        search_buf[i] = *line;
      }

      if(*line != ')') {
        NS_DUMP_WARNING("Bad format of repeatable block, no end bracket found in Value case for repeat block condition at line = %d "
                        "in file %s This will be treated as string", line_number, fname);
        return(-1);
      }
       
      search_buf[i] = '\0';
      hash_code_var = gSessionTable[sess_idx].var_hash_func(search_buf, strlen(search_buf));
      //sprintf(tmp_buf, "{%s}", search_buf);
      strcpy(tmp_buf, search_buf);

      NSDL3_VARS(NULL, NULL,"Found count type = Value, var = %s, hash code = %d, tmp_buf= %s", search_buf, hash_code_var, tmp_buf);
    }
    else if (strcasecmp(search_buf, "Count") == 0) // Count case
    {
      NSDL4_VARS(NULL, NULL,"Found count type = Count");

      local_repeat_count_type = COUNT;
      line = strchr(line, '(');
      line++;
      for (i = 0; (*line != ')') && *line; line++, i++) {
        search_buf[i] = *line;
      }
      
      if(*line != ')') {
        NS_DUMP_WARNING("Bad format of repeatable block, no end bracket ')' in Count case for repeat block condition at line = %d "
                        "in file %s This will be treated as string", line_number, fname);
        return(-1);
      }
      search_buf[i] = '\0';

      hash_code_var = gSessionTable[sess_idx].var_hash_func(search_buf, strlen(search_buf));
      NSDL4_VARS(NULL, NULL, "hash_code_var= %d", hash_code_var);
      if(hash_code_var != -1)
      {
        short var_type = gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code_var].var_type;
        if(var_type != SEARCH_VAR && var_type != JSON_VAR && var_type != NSL_VAR && var_type != TAG_VAR) {
          NS_DUMP_WARNING("This variable not supported in Count case for repeat block = %d in file %s." 
                          "This will be treated as string", line_number, fname);
          return(-1);
        }
        int uv_idx = gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code_var].user_var_table_idx;
        // Check for vector varaible
        if(gSessionTable[sess_idx].var_type_table_shr_mem[uv_idx] != 1) {
          NS_DUMP_WARNING("Only Vector variable are supported in Count case for repeat block = %d in file %s." 
                          "This will be treated as string", line_number, fname);
          return(-1);
        }
      }
      else
      {
        NS_DUMP_WARNING("Variable = %s not found for repeat block = %d in file %s." 
                        "This will be treated as string", search_buf, line_number, fname);
        return(-1); 
      }
      NSDL4_VARS(NULL, NULL, "Found count type = Count, var = %s, hash code = %d", search_buf, hash_code_var);
    }
    else if (strcasecmp(search_buf, "Num") == 0)
    {
      NSDL4_VARS(NULL, NULL, "Found count type = NUM");
      local_repeat_count_type = NUM;
      line++; // Increment to escape "(" symbol
      for (i = 0; (*line != ')') && *line; line++, i++) {
        if (isdigit(*line))
          search_buf[i] = *line;
        else {
          NS_DUMP_WARNING("Invalid value of NUM. It must be a numeric number at line = %d in file %s." 
                          "This will be treated as string", line_number, fname);
          return(-1);
        }
      } // For loop
      if(*line != ')') {
        NS_DUMP_WARNING("Bad format of repeatable block, no end bracket ')' found in num case for repeat block condition " 
                        "at line = %d in file %s This will be treated as string", line_number, fname);
        return(-1);
      }
      local_repeat_count = atoi(search_buf);

      NSDL2_VARS(NULL, NULL, "Found count type = NUM and Repeat count = %d", repeatBlock[rep_seg_idx].repeat_count);
    } else {
    
      return(-1); 
    }
   
    line++; // to skip ) bracket, after value, num and count
    NSDL4_VARS(NULL, NULL, " After skiping ) line = %s", line);
    CLEAR_WHITE_SPACE(line);
     
    if(*line != '}'){
      NS_DUMP_WARNING("Bad format repeat block, end bracket '}' for START_REPEAT_BLOCK ot found at line = %d " 
                      "in file %s This will be treated as string", line_number, fname);
      return(-1);
    }

    // Create repeat segment entry 
    if(create_repeat_block_table_entry(&rep_seg_idx) == -1){
      fprintf(stderr, "Failed to create repeat_block entry\n");
      return(-1);
    }
    // Memset repeatBlock to 0
    memset(&repeatBlock[rep_seg_idx], 0, sizeof(RepeatBlock));
    //set rep_seg_idx
    segTable[rnum].offset = rep_seg_idx;
    // Set type SEGMENT
    segTable[rnum].type = SEGMENT;

    if(rep_seprator[0] != '\0'){   
      //handle escape character of seprator.
      nslb_convert_unescape_chars(rep_seprator, rep_seprator); 
  
      if((repeatBlock[rep_seg_idx].rep_sep = copy_into_big_buf(rep_seprator, 0)) == -1){
        fprintf(stderr, "Failed to copy repeat seprator to big buf\n");
        return(-1);
      } 
      repeatBlock[rep_seg_idx].rep_sep_len = strlen(rep_seprator); 
     } else{
       NSDL4_VARS(NULL, NULL, "Setting rep seperator big buf index to -1");
       repeatBlock[rep_seg_idx].rep_sep = -1;
       repeatBlock[rep_seg_idx].rep_sep_len = 0;
     }
   
    repeatBlock[rep_seg_idx].repeat_count_type= local_repeat_count_type;

    if(repeatBlock[rep_seg_idx].repeat_count_type == COUNT)
      repeatBlock[rep_seg_idx].hash_code = hash_code_var;
    else if(repeatBlock[rep_seg_idx].repeat_count_type == VALUE){
      if((repeatBlock[rep_seg_idx].data = copy_into_big_buf(tmp_buf, 0)) == -1){
        fprintf(stderr, "Failed to copy tmp_buf to big buf\n");
        return(-1);
      }
    } else
      repeatBlock[rep_seg_idx].repeat_count = local_repeat_count; 

    *segment_idx = rnum;
    *tmp_ptr = line;
    NSDL4_VARS(NULL, NULL, " After parsing repeat block tmp_ptr = %s", *tmp_ptr);
  } 
  return 0; 
}

inline static char *
get_short (unsigned short *out, char *in)
{
  NSDL1_VARS(NULL, NULL, "Method Called.");
  bcopy (in, (char *)out, 2);
  in += 2;
  return in;
}

/* Message format buffer filles bye read amf 
 * For Non Parametrized:
 *  
 *   ----------------------------------------------------------
 *   | NumSeg | SegLen | Version | NumHdrs | NumBodies | Size | --> 
 *   -----------------------------------------------------------
 *      --------------------------------------------------------------------------------------------
 *   --> String (Class Name) | Size | String (Uri Name) | Len | Type Code | SegLen | VarName| Seglen ....
 *      --------------------------------------------------------------------------------------------
 *
 * For Parametrized:
 *   ----------------------------------------------------------
 *   | NumSeg | SegLen | Version | NumHdrs | NumBodies | Size | --> 
 *   -----------------------------------------------------------
 *      -----------------------------------------------------------------------------
 *   --> String (Class Name) | Size | String (Uri Name) | Len | Type Code | Size | data|
 *      -----------------------------------------------------------------------------
 */

// Return version or -1 on error
int
amf_segment_line(StrEnt* segtable, char* line, int noparam_flag,
                 int line_number, int sess_idx, char *fname)
{
  int i,ret_val;
  char var[MAX_LINE_LENGTH];
  char absol_var[MAX_LINE_LENGTH];
  int var_length;
  DBT key;
  DBT data;
  int rnum;
  int cookie_idx;
  //  int dynvar_idx;
  int tagvar_idx;
  int nslvar_idx;
  int searchvar_idx;
  int jsonvar_idx;  //for JSON Var
  int randomvar_idx = -1;
  int randomstring_idx = -1;
  int uniquevar_idx = -1;
  int datevar_idx = -1;

  int clustvar_idx;
  int groupvar_idx;
  int hash_code;
  int point_rnum;
  //int str_len;
  char msgout[64*1024], *mptr;
  int mlen=64*1024;
  unsigned short num_seg, seg_size, nwvar_length;
  char amf_type_var;
  int version = -1;

  NSDL2_VARS(NULL, NULL, "Method Called. AMF Message = %s", line);

  
  // Read amf xml message from line and binary AMF output in msgout in segmented format
  if(amf_encode ( 1, line, strlen(line), msgout , &mlen, 1, noparam_flag, &version) == NULL)
  {
    fprintf(stderr, "Error in AMF xml format. Treating this as non amf body\n");
    return -1; 
  }

  // TODO - Study what is this for  - skip_amf_debug ();

  //str_len = 64*1024 - mlen;
#if 0
#ifdef NS_DEBUG_ON
  FILE *fpout;
  show_buf (msgout, str_len);
        fpout = fopen ("amf.out", "w+");
        if (!fpout) {
            printf ("unable to open file amf.out \n");
            exit(1);
        }
        //show_buf(message, cur);
        for (i=0; i < str_len; i++) {
            if (i !=0 && i%16 == 0)
                fprintf (fpout, "\n");
            fprintf (fpout, "%02x ", (unsigned char)msgout[i]);
         }
        fclose(fpout);
#endif
#endif

  // Get number of segments. If no paramterization is done, then num_seg will be always 1
  mptr = get_short(&num_seg, msgout);
  NSDL2_VARS(NULL, NULL, "AMF - Number of segments = %d", num_seg);

  for (i =0; i < num_seg; i++) {
    mptr = get_short(&seg_size, mptr);
    if (create_seg_table_entry(&rnum) != SUCCESS) {
      NS_EXIT(-1, "AMF - segment_line(): could not get new seg table entry\n");
    }
    segTable[rnum].sess_idx = sess_idx;

    if (i == 0)
      segtable->seg_start = rnum;

    segtable->num_entries++;

    if (i%2 == 0) {//Even segs are strings
      amf_type_var = mptr[seg_size-1]; //last byte of last STR seg contains var encoding type for next variable
      AddPointerTableEntry (&point_rnum, mptr, seg_size);

      segTable[rnum].type = STR;
      segTable[rnum].offset = point_rnum;

      NSDL2_VARS(NULL, NULL, "AMF - segment %d is FIX: size=%d", i, seg_size);
#ifdef NS_DEBUG_ON
      show_buf(mptr, seg_size);
#endif
      mptr += seg_size;
    } else {//Odd are variables
      strncpy(var, mptr, seg_size);
      NSDL2_VARS(NULL, NULL, "AMF - segment %d is VAR %s:", i, var);
      var_length = seg_size -1; //not counting terminating NULL
      if (gSessionTable[sess_idx].var_hash_func)
	hash_code = gSessionTable[sess_idx].var_hash_func(var, var_length);
      else
	hash_code = -1;

      if (hash_code != -1) {
	switch (gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type) {
	case VAR:
        case INDEX_VAR:
	  memset(&key, 0, sizeof(DBT));
	  memset(&data, 0, sizeof(DBT));

	  //sprintf(absol_var, "%s!%s", var, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
	  sprintf(absol_var, "%s!%s", var, get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name),
                                                                               sess_idx, "-"));
	  key.data = absol_var;
	  key.size = strlen(absol_var);

	  ret_val = var_hash_table->get(var_hash_table, NULL, &key, &data, 0);

	  if (ret_val == DB_NOTFOUND) {
	    NS_EXIT(-1, "%s(): Variable %s not defined. Used on line=%d in file %s\n",
                    (char*)__FUNCTION__, var, line_number, fname);
	  }

	  if (ret_val != 0) {
	    NS_EXIT(-1, "%s(): Hash Table Get failed for NS var '%s'on line=%d in file %s\n",
                    (char*)__FUNCTION__, var, line_number, fname);
	  }

	  segTable[rnum].type = gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type;
	  segTable[rnum].offset = (int) atoi((char*)data.data);

	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type VAR/Index. runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
	  break;

	case COOKIE_VAR:
          // For Auto Cookie Mode, we are not supporting COOKIE Vars for now. So code will come
          // here only for Manual Cookies
	  cookie_idx = Create_cookie_entry(var, sess_idx);
	  segTable[rnum].type = COOKIE_VAR;
	  segTable[rnum].offset = cookie_idx;
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type COOKIE. runm=%d var=%s cookie_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case SEARCH_VAR:
	  if ((searchvar_idx = find_searchvar_idx(var, var_length, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the search var %s on line = %d file %s\n",
                    (char*)__FUNCTION__, var, line_number, fname);
	  }
	  segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used
	  segTable[rnum].type = SEARCH_VAR;
	  segTable[rnum].offset = searchvar_idx;    /* this value is changed later to the usertable variable index*/
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type SEARCH_VAR. runm=%d var=%s searchvar_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

        case JSON_VAR:
          if ((jsonvar_idx = find_jsonvar_idx(var, var_length, sess_idx)) == -1) {
            NS_EXIT(-1, "%s(): Could not find the JSON var %s on line = %d file %s\n",
                    (char*)__FUNCTION__, var, line_number, fname);
          }
          segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used
          segTable[rnum].type = JSON_VAR;
          segTable[rnum].offset = jsonvar_idx;    /* this value is changed later to the usertable variable index*/
          NSDL2_VARS(NULL, NULL, "AMF - Variable is of type JSON_VAR. runm=%d var=%s jsonvar_idx=%d", rnum, var, segTable[rnum].offset);
          break;

	case TAG_VAR:
	  if ((tagvar_idx = find_tagvar_idx(var, var_length, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the tag var variable %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }
	  segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used
	  segTable[rnum].type = TAG_VAR;
	  segTable[rnum].offset = tagvar_idx;  /* this value is changed later to the usertable variable index */
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type TAG_VAR. runm=%d var=%s tagvar_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case NSL_VAR:
	  if ((nslvar_idx = find_nslvar_idx(var, var_length, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the nsl var variable %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }
	  segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used 
	  segTable[rnum].type = NSL_VAR;
	  segTable[rnum].offset = nslvar_idx;  /* this value is changed later to the usertable variable index */
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type NSL_VAR (declared var). runm=%d var=%s nslvar_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case CLUST_VAR:
	  if ((clustvar_idx = find_clustvar_idx(var)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the clust var %s on line = %d file %s\n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }
	  segTable[rnum].type = CLUST_VAR;
	  segTable[rnum].offset = clustvar_idx;
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type CLUST_VAR. runm=%d var=%s clustvar_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case GROUP_VAR:
	  if ((groupvar_idx = find_groupvar_idx(var)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the group var %s on line = %d file %s \n",
                            (char*)__FUNCTION__, var, line_number, fname);
	  }
	  segTable[rnum].type = GROUP_VAR;
	  segTable[rnum].offset = groupvar_idx;
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type GROUP_VAR. runm=%d var=%s groupvar_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case GROUP_NAME_VAR:
	  segTable[rnum].type = GROUP_NAME_VAR;
	  segTable[rnum].offset = 0;
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type GROUP_NAME_VAR. runm=%d var=%s group_name_var_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case CLUST_NAME_VAR:
	  segTable[rnum].type = CLUST_NAME_VAR;
	  segTable[rnum].offset = 0;
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type CLUST_NAME_VAR. runm=%d var=%s clust_name_var_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case USERPROF_NAME_VAR:
	  segTable[rnum].type = USERPROF_NAME_VAR;
	  segTable[rnum].offset = 0;
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type USERPROF_NAME_VAR. runm=%d var=%s userprof_name_var_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

	case HTTP_VERSION_VAR:
	  segTable[rnum].type = HTTP_VERSION_VAR;
	  segTable[rnum].offset = 0;
	  NSDL2_VARS(NULL, NULL, "AMF - Variable is of type HTTP_VERSION_VAR. runm=%d var=%s http_version_var_idx=%d", rnum, var, segTable[rnum].offset);
	  break;

        case RANDOM_VAR:
	  if ((randomvar_idx = find_randomvar_idx(var, sess_idx)) == -1) {
	    NS_EXIT(-1, "%s(): Could not find the Random var %s on line = %d file %s\n",
                             (char*)__FUNCTION__, var, line_number, fname);
	  }
          segTable[rnum].type = RANDOM_VAR;
          segTable[rnum].offset = randomvar_idx;
          NSDL1_VARS(NULL, NULL, "AMF - Variable is of type RANDOM_VAR, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break;

        case RANDOM_STRING:
           if ((randomstring_idx = find_randomstring_idx(var, sess_idx)) == -1) {
             NS_EXIT(-1, "%s(): Could not find the Random string %s on line = %d file %s\n",
                              (char*)__FUNCTION__, var, line_number, fname);
           }
           segTable[rnum].type = RANDOM_STRING;
           segTable[rnum].offset = randomstring_idx;
           NSDL1_VARS(NULL, NULL, "AMF - Variable is of type RANDOM_STRING, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
           break;
  
        case UNIQUE_VAR:
          if ((uniquevar_idx = find_uniquevar_idx(var, sess_idx)) == -1) {
            NS_EXIT(-1, "%s(): Could not find the Unique var %s on line = %d file %s\n",
                             (char*)__FUNCTION__, var, line_number, fname);
          }
          segTable[rnum].type = UNIQUE_VAR;
          segTable[rnum].offset = uniquevar_idx;
          NSDL1_VARS(NULL, NULL, "AMF - Variable is of type UNIQUE_VAR, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
          break;

       case DATE_VAR:
           if ((datevar_idx = find_datevar_idx(var, sess_idx)) == -1) {
             NS_EXIT(-1, "%s(): Could not find the date var  %s on line = %d file %s\n",
                             (char*)__FUNCTION__, var, line_number, fname);
           }
           segTable[rnum].type = DATE_VAR;
           segTable[rnum].offset = datevar_idx;
           NSDL1_VARS(NULL, NULL, "AMF - Variable is of type DATE_VAR, runm=%d var=%s offset=%d", rnum, var, segTable[rnum].offset);
           break;
	default:
	  NS_EXIT(-1, "%s(): Unsupported Var type  var %s on line = %d file %s \n",
                           (char*)__FUNCTION__, var, line_number, fname);
	}
      } else {
	if (amf_type_var != 0x02) { // string-marker in AMF0
	  NS_EXIT(-1, "%s():  var %s is not a variable and non-string amf n file %s\n",
                          (char*)__FUNCTION__, var, fname);
	}
	var_length += 2; //with curly brackets
	nwvar_length = htons(var_length);
	bcopy ((char *)&nwvar_length, var, 2);
	sprintf (var+2, "{%s}", mptr);

	AddPointerTableEntry (&point_rnum, var, var_length+2); //var in UTF

	segTable[rnum].type = STR;
	segTable[rnum].offset = point_rnum;
      }
      mptr += seg_size;
    }
  }

  return version;
}

extern void 
//save_contents(char *lbuf, connection *cptr)
save_contents(char *lbuf, VUser *vptr, connection *cptr);

#define NS_ALWAYS_UG_VALID -1
int get_value_index(GroupTableEntry_Shr* groupTableEntry, VUser *vptr, char* var_name)
{
  int vidx;
  int group_idx = groupTableEntry->idx;
  int sequence = groupTableEntry->sequence;
  int type = groupTableEntry->type;
  int num_entries = groupTableEntry->num_vars;
  //unsigned int num_values = groupTableEntry->num_values;
  unsigned int num_values; 
  UserGroupEntry* vugtable = &(vptr->ugtable[group_idx]);
  int rand_num;
  WeightTableEntry* weight_start;
  unique_group_table_type* uniq_group_ptr;
  static int freq_count = 0;
  char error_msg[1024];//Buffer to fill error message

  PerProcVgroupTable *per_proc_fparam_grp = &per_proc_vgroup_table[(my_port_index * total_group_entries)+ group_idx]; 

  num_values = per_proc_fparam_grp->num_val;

  NSDL1_VARS(vptr, NULL, "Method Called, Get value index: NVM = %d, file param group id = %d, type = %hd, seq = %hd "
                         "per_proc_num_val=%u, total_num_val = %d, num_vars = %d, var_name = %s, remaining_access = %d, "
                         "seq_group_next = %d, vugtable->cur_val_idx = %d",
                          my_child_index, group_idx, groupTableEntry->type, groupTableEntry->sequence, num_values,
                          groupTableEntry->num_values, groupTableEntry->num_vars, var_name, 
                          vugtable->remaining_valid_accesses, seq_group_next[group_idx], vugtable->cur_val_idx);

  if (vugtable->remaining_valid_accesses == 0) {
    switch (sequence) {
    case SEQUENTIAL:
      vidx = vugtable->cur_val_idx = seq_group_next[group_idx];
      vidx++;
      vidx = vidx % num_values;
      seq_group_next[group_idx] = vidx;

      NSDL2_VARS(vptr, NULL, "Seq: vidx=%d, cur_val_idx=%d, next = %d", vidx, vugtable->cur_val_idx, seq_group_next[group_idx]);
      break;

    case USEONCE:
      if(per_proc_fparam_grp->num_val == 0)
      {
        if(groupTableEntry->UseOnceAbort == ABORT_USEONCE_PAGE){
          NSDL2_VARS(vptr, NULL, "USEONCE PAGE case value = [%d]", groupTableEntry->UseOnceAbort);
          vptr->page_status = NS_USEONCE_ABORT; 
        }else if(groupTableEntry->UseOnceAbort == ABORT_USEONCE_SESSION){
          NSDL2_VARS(vptr, NULL, "USEONCE SESSION case value = [%d]", groupTableEntry->UseOnceAbort);
          vptr->page_status = NS_USEONCE_ABORT; 
          vptr->sess_status = NS_USEONCE_ABORT; 
        }else if(groupTableEntry->UseOnceAbort == ABORT_USEONCE_USER){
          NSDL2_VARS(vptr, NULL, "USEONCE USER case value = [%d]", groupTableEntry->UseOnceAbort);
          vptr->page_status = NS_USEONCE_ABORT; 
          vptr->sess_status = NS_USEONCE_ABORT; 
          vptr->flags |= NS_VUSER_RAMPING_DOWN;
          VUSER_INC_EXIT(vptr);
        }else{
          sprintf(error_msg, "All values of file parameter (%s) using mode 'Use Once' are exhausted. "
                             "Data file name is (%s).Test run cannot continue. Aborting ...\n", 
                              var_name, groupTableEntry->data_fname); 
          NSDL2_VARS(vptr, NULL, "USEONCE: error %s", error_msg);
          NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                __FILE__, (char*)__FUNCTION__,
				"%s", error_msg);

          END_TEST_RUN_MSG
        } 
        return -2;
      }

      vidx = vugtable->cur_val_idx = seq_group_next[group_idx];
      vidx++;
      per_proc_fparam_grp->cur_val = vidx;
      // Since in case of RTC we need start value hence don't reduce start value
      //per_proc_fparam_grp->start_val = vidx;
      //vidx = vidx % num_values;
      seq_group_next[group_idx] = vidx;
      per_proc_fparam_grp->num_val--;
      freq_count++;

      NSDL2_VARS(vptr, NULL, "USE_ONCE: vidx=%d, cur_val_idx=%d, next = %d", vidx, vugtable->cur_val_idx, seq_group_next[group_idx]);
      NSDL2_VARS(vptr, NULL, "groupTableEntry->UseOnceOptiontype = %d, freq_count = %d", 
                              groupTableEntry->UseOnceOptiontype, freq_count);

      if(groupTableEntry->UseOnceOptiontype == USE_ONCE_EVERY_USE)
        write_last_data_file(per_proc_fparam_grp->cur_val, 
                             per_proc_fparam_grp->num_val, 
                             per_proc_fparam_grp->total_val, get_ms_stamp(), 
                             per_proc_fparam_grp->last_file_fd, 
                             group_table_shr_mem[group_idx].data_fname, group_table_shr_mem[group_idx].absolute_path_flag);
      else
      {
        /*This is case when we need to write data on every specified interval*/
        if(freq_count == group_table_shr_mem[group_idx].UseOnceOptiontype)
        {
          write_last_data_file(per_proc_fparam_grp->cur_val, 
                               per_proc_fparam_grp->num_val, 
                               per_proc_fparam_grp->total_val, get_ms_stamp(), 
                               per_proc_fparam_grp->last_file_fd, 
                               group_table_shr_mem[group_idx].data_fname, group_table_shr_mem[group_idx].absolute_path_flag);
          freq_count = 0;
        }   
      }
      break;

    case WEIGHTED:
      weight_start = groupTableEntry->group_wei_uni.weight_ptr;
      rand_num = ns_get_random(gen_handle) % weight_start[num_values - 1].value_weight;
      NSDL3_VARS(vptr, NULL, "Weighted Random number = %d, max weight = %d", 
                  rand_num, weight_start[num_values - 1].value_weight);
      for (vidx=0; vidx<num_values; vidx++, weight_start++) {
	if (rand_num < weight_start->value_weight) {
	  vugtable->cur_val_idx = vidx;
          NSDL2_VARS(vptr, NULL, "Weighted: vidx=%d, cur_val_idx=%d", vidx, vugtable->cur_val_idx);
	  break;
	}
      }
      break;
    case UNIQUE:
      NSDL2_VARS(vptr, NULL, "unique: unique_group_id = %d, unique_group_table = %p, cur_val_idx = %d", 
                              groupTableEntry->group_wei_uni.unique_group_id, unique_group_table, vugtable->cur_val_idx);

      uniq_group_ptr = unique_group_table + groupTableEntry->group_wei_uni.unique_group_id;
      if (vugtable->cur_val_idx != -1)
	free_unique_val(uniq_group_ptr, vugtable->cur_val_idx);
      
      int ret = alloc_unique_val(uniq_group_ptr, var_name);
      NSDL2_VARS(vptr, NULL, "Unique: ret = %d", ret);

      if(ret == -2)
      {
        if(group_table_shr_mem[group_idx].UseOnceAbort == ABORT_USEONCE_PAGE){
          NSDL2_VARS(vptr, NULL, "UNIQUE PAGE case value = [%d]", group_table_shr_mem[group_idx].UseOnceAbort);
          vptr->page_status = NS_USEONCE_ABORT; 
        }else if(group_table_shr_mem[group_idx].UseOnceAbort == ABORT_USEONCE_SESSION){
          NSDL2_VARS(vptr, NULL, "UNIQUE SESSION case value = [%d]", group_table_shr_mem[group_idx].UseOnceAbort);
          vptr->page_status = NS_USEONCE_ABORT; 
          vptr->sess_status = NS_USEONCE_ABORT; 
        }else if(group_table_shr_mem[group_idx].UseOnceAbort == ABORT_USEONCE_USER){
          NSDL2_VARS(vptr, NULL, "UNIQUE SESSION case value = [%d]", group_table_shr_mem[group_idx].UseOnceAbort);
          vptr->page_status = NS_USEONCE_ABORT; 
          vptr->sess_status = NS_USEONCE_ABORT;
          vptr->flags |= NS_VUSER_RAMPING_DOWN;
          VUSER_INC_EXIT(vptr);
        }else{
          sprintf(error_msg, "All values of file parameter (%s) using mode 'Unique' are exhausted. "
                             "Data file name is (%s).Test run cannot continue. Aborting ...\n", 
                              var_name, group_table_shr_mem[group_idx].data_fname); 

          NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                __FILE__, (char*)__FUNCTION__,
                              "%s", error_msg);

          END_TEST_RUN_MSG
        } 
        return -2;
      }
      else
        vugtable->cur_val_idx = ret; 

     break;
 
    default: /* case of RANDOM */
      vugtable->cur_val_idx = (int) (ns_get_random(gen_handle) % num_values);
      NSDL2_VARS(vptr, NULL, "default: cur_val_idx=%d", vugtable->cur_val_idx);
      break;
    }

    if (type == SESSION || type == ONCE)
      vugtable->remaining_valid_accesses = NS_ALWAYS_UG_VALID;
    else
      vugtable->remaining_valid_accesses = num_entries - 1;
  } else if (type == USE)
    vugtable->remaining_valid_accesses--;

  NSDL1_VARS(vptr, NULL, "value index: vugtable->cur_val_idx = %d", vugtable->cur_val_idx);
  return vugtable->cur_val_idx;
}

inline PointerTableEntry_Shr* get_var_val(VUser* vptr, int var_val_flag, int var_hashcode)
{
  int vidx;
  int remaining_valid_accesses;
  int fill_uvtable = 0;
  PointerTableEntry_Shr *value_ptr;
  VarTableEntry_Shr* var_ptr;

  NSDL1_VARS(vptr, NULL, "Method Called, vptr = %p, var_val_flag = %d, var_hashcode = %d", 
                                         vptr, var_val_flag, var_hashcode);

  var_ptr = get_fparam_var(vptr, -1, var_hashcode);

  if(var_val_flag)
  {
    //To handle ns_advance_param() case
    remaining_valid_accesses = vptr->ugtable[var_ptr->group_ptr->idx].remaining_valid_accesses;

    vidx = get_value_index(var_ptr->group_ptr, vptr, var_ptr->name_pointer);

    NSDL2_VARS(vptr, NULL, "filled_flag = %d, remaining_valid_accesses = %d", 
                            vptr->uvtable[var_ptr->uvtable_idx].filled_flag, remaining_valid_accesses);

    if((vidx != -2) && ((vptr->uvtable[var_ptr->uvtable_idx].filled_flag == 0) || (remaining_valid_accesses == 0)))
      fill_uvtable = 1;
  }
  else
  {
    //Manish: In case of UNIQUE if ns_advance_param() API not used and if ns_web_url() used and conection failed then core is created.
    // Core is created due to cur_val_idx = -1, so handle this situation setting vidx = 0
    vidx = (vptr->ugtable[var_ptr->group_ptr->idx].cur_val_idx == -1)?0:vptr->ugtable[var_ptr->group_ptr->idx].cur_val_idx;

   if(vptr->uvtable[var_ptr->uvtable_idx].filled_flag == 0)
      fill_uvtable = 1;
  }

  /* Handle following case:
     1. Refresh = SESSION, get value first time (may be in ns_web_url() API, ns_eval_string() API or ns_advance_param() API)
         ==> In this new data value should fetch
     2. Refresh = SESSION, get value next time in ns_web_url() API or ns_eval_sting() API 
         ==> In this old one data value should fetch
     3. Refresh = SESSION, get value next time in ns_advance_param() API
         ==> In this new data value should fetch
     4. Refresh = USE 
         ==> In this case always new data value should fetch (Note: new data value will be featch only when all the variable used thier values)
  */
  NSDL2_VARS(vptr, NULL, "cur_val_idx = %d, fill_uvtable = %d, vidx = %d", 
                          vptr->ugtable[var_ptr->group_ptr->idx].cur_val_idx, fill_uvtable, vidx);
  if(fill_uvtable)
  {
    NSDL2_VARS(vptr, NULL, "Store file param data value in uvtable at index = %d", var_ptr->uvtable_idx);
    int i;
    VarTableEntry_Shr *var_start_ptr = (VarTableEntry_Shr *)(var_ptr->group_ptr + 1); 
    for(i = 0; i < var_ptr->group_ptr->num_vars; i++)
    {
      NSDL2_VARS(vptr, NULL, "is_file = %d", var_start_ptr[i].is_file);
      value_ptr = var_start_ptr[i].value_start_ptr + vidx;
      
      if(var_start_ptr[i].is_file == IS_FILE_PARAM)    //In case fo file_param data pull from segment table else from big_buf
      {
        StrEnt_Shr seg_tab_ptr;
        char *to_fill;
        int total_len = 0, next_idx, j;
        PointerTableEntry_Shr *seg_val;

        
        // Initializing IO vector with default size
        ns_tls_init_seg_vector(IOVECTOR_SIZE);
 
        seg_tab_ptr.seg_start = value_ptr->seg_start;
        seg_tab_ptr.num_entries = value_ptr->num_entries;

        NSDL2_VARS(vptr, NULL, "seg_start = %p, num_entries = %d", value_ptr->seg_start, value_ptr->num_entries);
        if((next_idx = insert_segments(vptr, NULL, &seg_tab_ptr, &g_tls.ns_iovec,
                                          NULL, 0, var_val_flag, 1, NULL, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
        {
          NSDL2_VARS(vptr, NULL, "Error in insert_segments() return value = %d\n", next_idx);
          return NULL;
        } 
         
        for (j = 0; j < next_idx; j++) 
          total_len += g_tls.ns_iovec.vector[j].iov_len;

        seg_val = (PointerTableEntry_Shr *)vptr->uvtable[var_start_ptr[i].uvtable_idx].value.value;

        NSDL2_VARS(vptr, NULL, "next_idx = %d, total_len = %d", next_idx, total_len);

        if(seg_val)
        {
          FREE_AND_MAKE_NULL(seg_val->big_buf_pointer, "Realocate memory to file parameter", -1);
          FREE_AND_MAKE_NULL(seg_val, "Realocate memory to file parameter", -1);
        }

        MY_MALLOC(seg_val, sizeof(PointerTableEntry_Shr), "Realocate memory to file parameter", -1);
        MY_MALLOC(seg_val->big_buf_pointer, total_len, "Realocate memory to file parameter", -1);

        to_fill = seg_val->big_buf_pointer;
        for (j = 0; j < next_idx; j++) {
          bcopy(g_tls.ns_iovec.vector[j].iov_base, to_fill, g_tls.ns_iovec.vector[j].iov_len);
          to_fill += g_tls.ns_iovec.vector[j].iov_len;
        }

        NS_FREE_RESET_IOVEC(g_tls.ns_iovec);

        *to_fill = '\0'; // NULL terminate
        seg_val->size = total_len; 

        value_ptr = seg_val;

        NSDL2_VARS(vptr, NULL, "Concated value = [%s], total_len = %d", seg_val->big_buf_pointer, value_ptr->size);
      }

      vptr->uvtable[var_start_ptr[i].uvtable_idx].value.value = (char *)value_ptr;
      vptr->uvtable[var_start_ptr[i].uvtable_idx].length = value_ptr->size;
      vptr->uvtable[var_start_ptr[i].uvtable_idx].filled_flag = 1;
    }
  }
  
  NSDL2_VARS(vptr, NULL, "uv_table_idx = %d, value = [%p]", 
                            var_ptr->uvtable_idx, vptr->uvtable[var_ptr->uvtable_idx].value.value);

  return (PointerTableEntry_Shr *)vptr->uvtable[var_ptr->uvtable_idx].value.value; 
}

int get_var_element(FILE* fp, char* buf, int* line_number, char* fname) {
  //char token[MAX_LINE_LENGTH];
  //char value[MAX_LINE_LENGTH];
  char line[MAX_LINE_LENGTH];
  char msg[MAX_LINE_LENGTH];
  int type = NS_NONE;
  char* line_ptr;
  int str_len;
  //int i;
  //int paranth_found;
  //int within_quotes;

  NSDL1_VARS(NULL, NULL, "Method Called.");
  sprintf (msg, "Parsing %s on file %d", fname, *line_number);

  //Get a line. Pass out empty lines
  while (1) {
    if (!nslb_fgets(line, MAX_LINE_LENGTH, fp, 0)) {
      // Change to return finish for c type script as registrations are in separate file
      return NS_FINISH_LINE;
      // return NS_NONE;
    } else {
      (*line_number)++;
      line[strlen(line) - 1] = '\0';

      line_ptr = line;
      CLEAR_WHITE_SPACE(line_ptr);
    }
    // Changed by Neeraj on Aug 25, 2011 as GUI will fix the spelling mistakes
    // if (!strcmp(line_ptr, "//End of NS Variable decalarations. Do not remove or modifify this line"))
    if (!strncmp(line_ptr, "//End of NS Variable", strlen("//End of NS Variable")))
      break;
    //if (!strncmp(line_ptr, "//", 2))
      //continue;
    IGNORE_COMMENTS(line_ptr);
    if (strlen(line_ptr))
      break;
  }

  //if (strlen(line) > 0)
  // line[strlen(line) - 1] = '\0';

  str_len = strlen(line_ptr);
  memmove(line, line_ptr, str_len+1);
  line_ptr = line;

  if (!strncmp(line_ptr, "nsl_static_var", strlen("nsl_static_var"))) {
    type = NS_STATIC_VAR_NEW;
    //Manish:--line_ptr += strlen("nsl_static_var");
  } else if (!strncmp(line_ptr, "nsl_index_file_var", strlen("nsl_index_file_var"))) {
    type = NS_INDEX_VAR;
    line_ptr += strlen("nsl_index_file_var");
  } else if (!strncmp(line_ptr, "nsl_decl_var", strlen("nsl_decl_var"))) {
    type = NS_NSL_VAR_SCALAR;
    line_ptr += strlen("nsl_decl_var");
  } else if (!strncmp(line_ptr, "nsl_decl_array", strlen("nsl_decl_array"))) {
    type = NS_NSL_VAR_ARRAY;
    line_ptr += strlen("nsl_decl_array");
  } else if (!strncmp(line_ptr, "nsl_xml_var", strlen("nsl_xml_var"))) {
    type = NS_TAG_VAR_NEW;
    line_ptr += strlen("nsl_xml_var");
  } else if (!strncmp(line_ptr, "nsl_search_var", strlen("nsl_search_var"))) {
    type = NS_SEARCH_VAR_NEW;
    //Manish:--line_ptr += strlen("nsl_search_var");
  } else if (!strncmp(line_ptr, "nsl_json_var", strlen("nsl_json_var"))) {
    type = NS_JSON_VAR_NEW;
  } else if (!strncmp(line_ptr, "nsl_random_number_var", strlen("nsl_random_number_var"))) {
    type = NS_RANDOM_VAR_NEW;
    line_ptr += strlen("nsl_random_number_var");
  } else if (!strncmp(line_ptr, "nsl_random_string_var", strlen("nsl_random_string_var"))) {
    type = NS_RANDOM_STRING_NEW;
    //line_ptr += strlen("nsl_random_string_var");
  } else if (!strncmp(line_ptr, "nsl_unique_number_var", strlen("nsl_unique_number_var"))) {
    type = NS_UNIQUE_VAR_NEW;
    line_ptr += strlen("nsl_unique_number_var");
  } else if (!strncmp(line_ptr, "nsl_date_var", strlen("nsl_date_var"))) {
    type = NS_DATE_VAR_NEW;
    //line_ptr += strlen("nsl_date_var");
  } else if (!strncmp(line_ptr, "nsl_web_find", strlen("nsl_web_find"))) {
    type = NS_CHECKPOINT_NEW;
    //Manish: line_ptr += strlen("nsl_web_find");
  } else if (!strncmp(line_ptr, "nsl_find", strlen("nsl_find"))) {
    type = NS_CHECKPOINT_NEW;
    line_ptr += strlen("nsl_find");
  } else if (!strncmp(line_ptr, "nsl_unique_range_var", strlen("nsl_unique_range_var"))) {
    type = NS_UNIQUE_RANGE_VAR;
  } else if (!strncmp(line_ptr, "//End of NS Variable", strlen("//End of NS Variable"))) {
    return NS_FINISH_LINE;
    // -- Add Achint- For global cookie - 10/04/2007
  }else if (!strncmp(line_ptr, "ns_add_cookie", strlen("ns_add_cookie"))) {
    type = NS_GLOBAL_COOKIE;
    line_ptr += strlen("ns_add_cookie");
  } else if (!strncmp(line_ptr, "nsl_check_reply_size", strlen("nsl_check_reply_size"))) {
    type = NS_CHECK_REPLY_SIZE_NEW;
    line_ptr += strlen("nsl_check_reply_size");
#ifdef RMI_MODE
  } else if (!strncmp(line_ptr, "$CAVU", strlen("$CAVU"))) {
    type = NS_GROUP_VAR_UTF;
  } else if (!strncmp(line_ptr, "$CAVL", strlen("$CAVL"))) {
    type = NS_GROUP_VAR_LONG;
#endif
  //} else if (!strcmp(line_ptr, "//End of NS Variable")) {
   // return NS_FINISH_LINE;
  } else if (!strncmp(line_ptr, "nsl_sql_var", strlen("nsl_sql_var"))) {
    type = NS_SQL_VAR;
    line_ptr += strlen("nsl_sql_var"); 
  } else if (!strncmp(line_ptr, "nsl_cmd_var", strlen("nsl_cmd_var"))) {
    type = NS_CMD_VAR;
    line_ptr += strlen("nsl_cmd_var");
  }
  else {
    fprintf (stderr, "Unrecognized declaration found <%s> \n", line_ptr);
    return -1;
  }

  switch(type) {
  case NS_NSL_VAR_SCALAR:
  case NS_NSL_VAR_ARRAY:
  case NS_STATIC_VAR_NEW:
  case NS_INDEX_VAR:
  case NS_TAG_VAR_NEW:
  case NS_SEARCH_VAR_NEW:
  case NS_RANDOM_VAR_NEW:
  case NS_DATE_VAR_NEW:
  case NS_RANDOM_STRING_NEW:
  case NS_UNIQUE_VAR_NEW:
  case NS_CHECKPOINT_NEW:
  case NS_GLOBAL_COOKIE:
  case NS_CHECK_REPLY_SIZE_NEW:
  case NS_JSON_VAR_NEW:
  case NS_UNIQUE_RANGE_VAR:
  case NS_SQL_VAR:
  case NS_CMD_VAR:
  /*  if(type == NS_INDEX_VAR || type == NS_STATIC_VAR_NEW || type == NS_DATE_VAR_NEW || type == NS_RANDOM_VAR_NEW || type == NS_SEARCH_VAR_NEW){
      strcpy(buf, line_ptr);
    }*/

    if(type == NS_STATIC_VAR_NEW || type == NS_SEARCH_VAR_NEW || type == NS_RANDOM_STRING_NEW || type == NS_CHECKPOINT_NEW || type == NS_NSL_VAR_ARRAY || type == NS_DATE_VAR_NEW || type == NS_JSON_VAR_NEW || type == NS_UNIQUE_RANGE_VAR || type == NS_SQL_VAR || type == NS_CMD_VAR || type == NS_NSL_VAR_SCALAR){
      strcpy(buf, line_ptr);
    } else{ 
        CLEAR_WHITE_SPACE(line_ptr);
        CHECK_CHAR(line_ptr, '(', msg);
        CLEAR_WHITE_SPACE(line_ptr);
        NSDL3_VARS(NULL, NULL, "line_ptr = [%s]", line_ptr); 
        str_len = strlen(line_ptr);
        while ((line_ptr[str_len-1] == ' ') || (line_ptr[str_len-1] == '\t')) {
          str_len--;
    }
    if ((line_ptr[str_len-1] != ';') && (line_ptr[str_len-2] != ')')) {
      fprintf(stderr, "Wrong format for variable declaration on line %d. Expecting decalartion termination by );\n", *line_number);
      return -1;
    }
    line_ptr[str_len - 2] = '\0'; 
    strcpy(buf, line_ptr);
    NSDL3_VARS(NULL, NULL, "buf = [%s]", buf);
    break;
    }

  } 
  
  NSDL1_VARS(NULL, NULL, "Returning %s, type = %d", buf, type);

  return type;
}

int create_nsvar_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method Called.");
  if (total_nsvar_entries == max_nsvar_entries) {
    //MY_REALLOC(nsVarTable, (max_nsvar_entries + DELTA_NSVAR_ENTRIES) * sizeof(NsVarTableEntry), "nsvar entries", -1);
    MY_REALLOC_EX(nsVarTable, (max_nsvar_entries + DELTA_NSVAR_ENTRIES) * sizeof(NsVarTableEntry), max_nsvar_entries * sizeof(NsVarTableEntry), "nsvar entries", -1);
    max_nsvar_entries += DELTA_NSVAR_ENTRIES;
  }
  *row_num = total_nsvar_entries++;
  return (SUCCESS);
}

int
read_urlvar_file(FILE* fp, int session_idx, int* line_number, char* fname) {
  char line[MAX_LINE_LENGTH];
  int ret;
  int done = 0;
  char script_filename[MAX_FILE_NAME_LEN + 1];

  NSDL1_VARS(NULL, NULL, "Method Called. fname = %s", fname);

  while (!done) {
    // get each variable inputs after taking out braces in declaration
    ret = get_var_element(fp, line, line_number, fname);
    script_ln_no = *line_number;
    NSDL1_VARS(NULL, NULL, "line=[%s]", line);
    if (ret == -1) {
      fprintf(stderr, "Parsing error : File=%s line=%d\n", fname, *line_number);
      return -1;
    }

    if(gSessionTable[session_idx].script_type == NS_SCRIPT_MODE_LEGACY)
      strcpy(script_filename, "script.capture");
    else if(gSessionTable[session_idx].script_type == NS_SCRIPT_MODE_USER_CONTEXT)
      strcpy(script_filename, NS_REGISTRATION_FILENAME);

    switch (ret) {
    case NS_NSL_VAR_SCALAR:
      if(input_nsl_var(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;

    case NS_NSL_VAR_ARRAY:
      if(input_nsl_array_var(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;

    case NS_TAG_VAR_NEW:
      if (input_tagvar_data(line, *line_number, session_idx, script_filename) == -1)
	return -1;
      break;

    case NS_SEARCH_VAR_NEW:
      if (input_searchvar_data(line, *line_number, session_idx, script_filename) == -1)
	return -1;
      break;
       
    case NS_JSON_VAR_NEW:
      if (input_jsonvar_data(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;  

    case NS_RANDOM_VAR_NEW:
      if (input_randomvar_data(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;

    case NS_UNIQUE_VAR_NEW:
      if (input_uniquevar_data(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;
 
    case NS_UNIQUE_RANGE_VAR:
      if(input_unique_rangevar_data(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;

    case NS_RANDOM_STRING_NEW:
      if (input_randomstring_data(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;
   
    case NS_DATE_VAR_NEW:
      if (input_datevar_data(line, *line_number, session_idx, script_filename) == -1)
        return -1;
      break;


    case NS_CHECKPOINT_NEW:
      //In Case of RBU with JTS, ignoring checkpoint,
      // So, applying check point only when either RBU or JTS is mode.
      if (!((global_settings->protocol_enabled & RBU_API_USED ) && (gSessionTable[session_idx].script_type == NS_SCRIPT_TYPE_JAVA))) {
        if (input_checkpoint_data(line, *line_number, session_idx, script_filename) == -1)
	  return -1;
      }
      break;

    case NS_STATIC_VAR_NEW:
    case NS_SQL_VAR:
      if (input_staticvar_data(line, *line_number, session_idx, script_filename, ret) == -1)
	return -1;
      break;
    case NS_CMD_VAR:
      write_log_file(NS_SCENARIO_PARSING, "Parsing %s API", line);
      if(input_staticvar_data(line, *line_number, session_idx, script_filename, ret) == -1)
	return -1;
      break;
    case NS_INDEX_VAR:
      if (input_indexvar_data(line, *line_number, session_idx, script_filename) == -1)
	return -1;
      break;
      // -- Add Achint- For global cookie - 10/04/2007
    case NS_GLOBAL_COOKIE:
      //Check if AUTO_COOKIE is enabled or not.
      if(global_settings->g_auto_cookie_mode != AUTO_COOKIE_DISABLE) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012051_ID, CAV_ERR_1012051_MSG);
      }
      if(input_global_cookie(line, *line_number, session_idx, fname, script_filename) == -1)
        return -1;
      break;
    case NS_CHECK_REPLY_SIZE_NEW:
       if(input_check_reply_size_data(line, *line_number, session_idx, script_filename) == -1)
         return -1;
       break;
    case NS_FINISH_LINE:
      done = 1;
    }
  }

  return 0;
}


int create_variable_functions(int sess_idx) {
  FILE* var_fptr;
  int j;
  char fname[MAX_LINE_LENGTH];
  int num_vars_trans_entries;
  char var_name[MAX_LINE_LENGTH];
  int num_var_table_idx = 0;
  char last_var_name[MAX_LINE_LENGTH];
  char name[MAX_LINE_LENGTH];
  int var_idx;
  char err_msg[MAX_ERR_MSG_LINE_LENGTH + 1];

  NSDL1_VARS(NULL, NULL, "Method Called.");
  //Craete Hash code of all NS vars for this session & globals vars
  //Dump all variables names in a file
  sprintf(fname, "%s/variable_names.txt", g_ns_tmpdir);
  if ((var_fptr = fopen(fname, "w+")) == NULL) {
    NSTL1(NULL, NULL, "Error in creating variable_name file\n");
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, fname, errno, nslb_strerror(errno));
  }

  //Add reserved variables
  fprintf(var_fptr, "cav_sgroup_name\n");
  fprintf(var_fptr, "cav_scluster_name\n");
  fprintf(var_fptr, "cav_user_profile\n");
  fprintf(var_fptr, "cav_http_ver_var\n"); //Anuj for HTTP_VERSION testing

  for (j = gSessionTable[sess_idx].nslvar_start_idx; j < gSessionTable[sess_idx].nslvar_start_idx + gSessionTable[sess_idx].num_nslvar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[j].name));

  for (j = gSessionTable[sess_idx].tag_start_idx; j < gSessionTable[sess_idx].tag_start_idx + gSessionTable[sess_idx].num_tag_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_TEMP_BUFFER_DATA(tagTable[j].name));

  for (j = gSessionTable[sess_idx].var_start_idx; j < gSessionTable[sess_idx].var_start_idx + gSessionTable[sess_idx].num_var_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(varTable[j].name_pointer));

  // INDEX VAR
  for (j = gSessionTable[sess_idx].index_var_start_idx; j < gSessionTable[sess_idx].index_var_start_idx + gSessionTable[sess_idx].num_index_var_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(indexVarTable[j].name_pointer));

  for (j = 0; j < total_clustvar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(clustVarTable[j].name));

  for (j = 0; j < total_groupvar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(groupVarTable[j].name));

  for (j = gSessionTable[sess_idx].cookievar_start_idx; j < gSessionTable[sess_idx].cookievar_start_idx + gSessionTable[sess_idx].num_cookievar_entries; j++) {
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(cookieTable[j].name));
  }

  for (j = gSessionTable[sess_idx].searchvar_start_idx; j < gSessionTable[sess_idx].searchvar_start_idx + gSessionTable[sess_idx].num_searchvar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(searchVarTable[j].name));

  //For JSON Var
  for (j = gSessionTable[sess_idx].jsonvar_start_idx; j < gSessionTable[sess_idx].jsonvar_start_idx + gSessionTable[sess_idx].num_jsonvar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(jsonVarTable[j].name));

  for (j = gSessionTable[sess_idx].randomvar_start_idx; j < gSessionTable[sess_idx].randomvar_start_idx + gSessionTable[sess_idx].num_randomvar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(randomVarTable[j].name));


  for (j = gSessionTable[sess_idx].randomstring_start_idx; j < gSessionTable[sess_idx].randomstring_start_idx + gSessionTable[sess_idx].num_randomstring_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(randomStringTable[j].name));
    
  for (j = gSessionTable[sess_idx].uniquevar_start_idx; j < gSessionTable[sess_idx].uniquevar_start_idx + gSessionTable[sess_idx].num_uniquevar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(uniqueVarTable[j].name));

for (j = gSessionTable[sess_idx].datevar_start_idx; j < gSessionTable[sess_idx].datevar_start_idx + gSessionTable[sess_idx].num_datevar_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(dateVarTable[j].name));


  //For unique range var
  for (j = gSessionTable[sess_idx].unique_range_var_start_idx; j < gSessionTable[sess_idx].unique_range_var_start_idx + 
       gSessionTable[sess_idx].num_unique_range_var_entries; j++)
    fprintf(var_fptr, "%s\n", RETRIEVE_BUFFER_DATA(uniquerangevarTable[j].name));

  fclose(var_fptr);

  //sort all the variable names
  sprintf(fname, "sort %s/variable_names.txt > %s/variable_names2.txt", g_ns_tmpdir, g_ns_tmpdir);
  //if (system("sort tmp/variable_names.txt > tmp/variable_names2.txt") == -1)
  if(nslb_system(fname,1,err_msg) != 0)
//   if (system(fname) == -1)
  {
    //write_log_file(NS_SCENARIO_PARSING, "Failed to sort variable names");
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000019]: ", CAV_ERR_1000019 + CAV_ERR_HDR_LEN, fname, errno, nslb_strerror(errno));
  }

  //Check for any duplicate variable names
  sprintf(fname, "%s/variable_names2.txt", g_ns_tmpdir);
  if ((var_fptr = fopen(fname, "r")) == NULL) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, fname, errno, nslb_strerror(errno));
    //write_log_file(NS_SCENARIO_PARSING, "Failed to open sorted variable names file");
  }

  last_var_name[0] = '\0';

  while (nslb_fgets(var_name, MAX_LINE_LENGTH, var_fptr, 0)) {
    var_name[strlen(var_name)-1] = '\0';
    if (!strcmp(var_name, last_var_name)) {
      //write_log_file(NS_SCENARIO_PARSING, "Variable '%s' is declared multiple times for the session %s", var_name,
       //             get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "-"));
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012010_ID, CAV_ERR_1012010_MSG, var_name);
    }
    strcpy(last_var_name, var_name);
  }

  fclose(var_fptr);

  //Create perfect hash code for the list of variables
  char so_file[1024];
  sprintf(so_file, "hash_variables_write.%d.so", sess_idx); //pass this so file to common library function to generate hash

  num_vars_trans_entries = generate_hash_table_ex_ex("variable_names.txt", "hash_variables", &gSessionTable[sess_idx].var_hash_func, &gSessionTable[sess_idx].var_get_key, NULL, NULL, so_file, 0, g_ns_tmpdir, err_msg);

  if(num_vars_trans_entries < 0)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012011_ID, CAV_ERR_1012011_MSG, err_msg);
    //write_log_file(NS_SCENARIO_PARSING, "Failed to generate hash table for variable_names.txt, error:%s", err_msg);
    //return num_vars_trans_entries;
  }

  gSessionTable[sess_idx].numUniqVars = 
  gSessionTable[sess_idx].num_nslvar_entries +
  gSessionTable[sess_idx].num_tag_entries +
  gSessionTable[sess_idx].num_searchvar_entries +
  gSessionTable[sess_idx].num_randomvar_entries +  
  gSessionTable[sess_idx].num_randomstring_entries +
  gSessionTable[sess_idx].num_uniquevar_entries + gSessionTable[sess_idx].num_datevar_entries +
  gSessionTable[sess_idx].num_jsonvar_entries + gSessionTable[sess_idx].num_unique_range_var_entries + gSessionTable[sess_idx].num_var_entries;  
  void *ns_vars_shr_mem = NULL;
  void *ns_vars_shr_mem_ptr = NULL;
  int shr_mem_size = 0;

  NSDL1_VARS(NULL, NULL, "Filling vars_trans_table_shr_mem, num_vars_trans_entries = %d", num_vars_trans_entries);
  if(num_vars_trans_entries)
  {
    /*Finding total shared memory size*/
    NSDL3_VARS(NULL, NULL, "numUniqVars = %d, num_vars_trans_entries = %d, " 
                           "size_of_char = %d, size_of_int = %d, size_of_VarTransTableEntry_Shr = %d", 
                            gSessionTable[sess_idx].numUniqVars, num_vars_trans_entries, 
                            sizeof(char), sizeof(int), sizeof(VarTransTableEntry_Shr));

    shr_mem_size += num_vars_trans_entries * sizeof(VarTransTableEntry_Shr);

    if(gSessionTable[sess_idx].numUniqVars)    
      shr_mem_size += gSessionTable[sess_idx].numUniqVars * sizeof(char) + gSessionTable[sess_idx].numUniqVars * sizeof(int);

    NSDL3_VARS(NULL, NULL, "Allocate shared memory for vars_trans_table_shr_mem of shr_mem_size = %d", shr_mem_size);

    write_log_file(NS_SCENARIO_PARSING, "Allocating shared memory for script '%s' parameters of size %d",
                                        RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), shr_mem_size);
    /*Allocating shared memory*/
    ns_vars_shr_mem = do_shmget(shr_mem_size, "ns_vars_shr_mem");
    memset(ns_vars_shr_mem, 0, shr_mem_size);

    ns_vars_shr_mem_ptr = ns_vars_shr_mem;
    NSDL3_VARS(NULL, NULL, "vars_trans_table_shr_mem base address  = %p", ns_vars_shr_mem_ptr);
 
    gSessionTable[sess_idx].vars_trans_table_shr_mem = ns_vars_shr_mem_ptr;
    ns_vars_shr_mem_ptr += num_vars_trans_entries * sizeof(VarTransTableEntry_Shr);

    if(gSessionTable[sess_idx].numUniqVars)
    {
      gSessionTable[sess_idx].var_type_table_shr_mem = ns_vars_shr_mem_ptr;
      ns_vars_shr_mem_ptr += gSessionTable[sess_idx].numUniqVars * sizeof(char);
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem = ns_vars_shr_mem_ptr;
    } 

    NSDL3_VARS(NULL, NULL, "Fill Transition table: vars_trans_table_shr_mem = %p, var_type_table_shr_mem = %p"
                           " vars_rev_trans_table_shr_mem = %p", 
                            gSessionTable[sess_idx].vars_trans_table_shr_mem, 
                            gSessionTable[sess_idx].var_type_table_shr_mem, 
                            gSessionTable[sess_idx].vars_rev_trans_table_shr_mem);

    memset(gSessionTable[sess_idx].vars_trans_table_shr_mem, 0, num_vars_trans_entries * sizeof(VarTransTableEntry_Shr));

    sprintf(name, "cav_sgroup_name");
    var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = GROUP_NAME_VAR;
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = 0;

    sprintf(name, "cav_scluster_name");
    var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = CLUST_NAME_VAR;
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = 0;

    sprintf(name, "cav_user_profile");
    var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = USERPROF_NAME_VAR;
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = 0;

    sprintf(name, "cav_http_ver_var"); //Anuj for HTTP_VERSION testing
    var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = HTTP_VERSION_VAR;
    gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = 0;

    for (j = gSessionTable[sess_idx].nslvar_start_idx; 
         j < gSessionTable[sess_idx].nslvar_start_idx + gSessionTable[sess_idx].num_nslvar_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      if (nsVarTable[j].type == NS_VAR_SCALAR) {
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 0;
      } else {
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 1;
	}
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = NSL_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].retain_pre_value = nsVarTable[j].retain_pre_value;
      num_var_table_idx++;
    }

    for (j = gSessionTable[sess_idx].tag_start_idx; 
         j < gSessionTable[sess_idx].tag_start_idx + gSessionTable[sess_idx].num_tag_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_TEMP_BUFFER_DATA(tagTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));

      if (tagTable[j].order == ORD_ALL)
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 1;
      else
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 0;

      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = TAG_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      num_var_table_idx++;
    }


    for (j = 0; j < total_clustvar_entries; j++) {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(clustVarTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = CLUST_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = j;
    }

    for (j = 0; j < total_groupvar_entries; j++) {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(groupVarTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = GROUP_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = j;
    }

    for (j = gSessionTable[sess_idx].cookievar_start_idx; 
         j < gSessionTable[sess_idx].cookievar_start_idx + gSessionTable[sess_idx].num_cookievar_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(cookieTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = COOKIE_VAR;

      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = -1; // in_cookie_hash(name, strlen(name));
    }

    for (j = gSessionTable[sess_idx].searchvar_start_idx; 
         j < gSessionTable[sess_idx].searchvar_start_idx + gSessionTable[sess_idx].num_searchvar_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(searchVarTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = (searchVarTable[j].ord == -1)?1:0;
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = SEARCH_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].retain_pre_value = searchVarTable[j].retain_pre_value;
      NSDL2_VARS(NULL, NULL, "SHARAD var_idx = %d, user_var_table_idx = %d", 
                              var_idx, gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx);
      num_var_table_idx++;

    }

    // For Unique Range var
    for (j = gSessionTable[sess_idx].unique_range_var_start_idx; 
         j < gSessionTable[sess_idx].unique_range_var_start_idx + gSessionTable[sess_idx].num_unique_range_var_entries; j++)
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(uniquerangevarTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = UNIQUE_RANGE_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      NSDL2_VARS(NULL, NULL, " UNIQUE_RANGE var_idx = %d, user_var_table_idx = %d", gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx, 
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx); 
      num_var_table_idx++;
    } 

    
    //For JSON Var 
    for (j = gSessionTable[sess_idx].jsonvar_start_idx; 
         j < gSessionTable[sess_idx].jsonvar_start_idx + gSessionTable[sess_idx].num_jsonvar_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(jsonVarTable[j].name));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = (jsonVarTable[j].ord == -1)?1:0;
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = JSON_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      NSDL2_VARS(NULL, NULL, "var_idx = %d, user_var_table_idx = %d", 
                              var_idx, gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx);
      num_var_table_idx++;
    }

    for (j = gSessionTable[sess_idx].randomvar_start_idx; 
         j < gSessionTable[sess_idx].randomvar_start_idx + gSessionTable[sess_idx].num_randomvar_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(randomVarTable[j].name));
      // use this to indicate that random vars are scalar 
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 0;
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = RANDOM_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      num_var_table_idx++;
    }

    for (j = gSessionTable[sess_idx].randomstring_start_idx; 
         j < gSessionTable[sess_idx].randomstring_start_idx + gSessionTable[sess_idx].num_randomstring_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(randomStringTable[j].name));
      // use this to indicate that random vars are scalar
      gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 0;
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = RANDOM_STRING;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      num_var_table_idx++;
    }

     for (j = gSessionTable[sess_idx].datevar_start_idx; 
          j < gSessionTable[sess_idx].datevar_start_idx + gSessionTable[sess_idx].num_datevar_entries; j++) 
     {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(dateVarTable[j].name));
      // use this to indicate that random vars are scalar
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 0;
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = DATE_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      num_var_table_idx++;
    }

    for (j = gSessionTable[sess_idx].uniquevar_start_idx; 
         j < gSessionTable[sess_idx].uniquevar_start_idx + gSessionTable[sess_idx].num_uniquevar_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(uniqueVarTable[j].name));
      // use this to indicate that unique vars are scalar
      gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 0;
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = UNIQUE_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = j;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx;
      num_var_table_idx++;
    }
    //num_var_table_idx is max of minimim uniq idx for scrtach, tag and search vars for a session
    //max_var+table_ids is the same acrsoss all session

    //assert (gSessionTable[sess_idx].numUniqVars == num_var_table_idx);

    /* To support uvtable in file parameter (static and index) filling transition table after assert.
       For file parameter no need to allocate memory for var_type_table_shr_mem and vars_rev_trans_table_shr_mem hence
       not adding these into numUniqVars */
    // Index Var
    for (j = gSessionTable[sess_idx].index_var_start_idx; 
         j < gSessionTable[sess_idx].index_var_start_idx + gSessionTable[sess_idx].num_index_var_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(indexVarTable[j].name_pointer));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = INDEX_VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = j;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].fparam_grp_idx = indexVarTable[j].group_idx;
    }

    //Manish: filling static var
    for(j = gSessionTable[sess_idx].var_start_idx; 
        j < gSessionTable[sess_idx].var_start_idx + gSessionTable[sess_idx].num_var_entries; j++) 
    {
      sprintf(name, "%s", RETRIEVE_BUFFER_DATA(varTable[j].name_pointer));
      var_idx = gSessionTable[sess_idx].var_hash_func(name, strlen(name));
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_type = VAR;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].fparam_grp_idx = varTable[j].group_idx;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].var_idx = varTable[j].self_idx_wrt_fparam_grp;
      gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = num_var_table_idx; 
      //gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = varTable[j].self_idx_wrt_fparam_grp;
      //gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx = j;
      if(varTable[j].is_file == IS_FILE_PARAM)
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 2;
      else
        gSessionTable[sess_idx].var_type_table_shr_mem[num_var_table_idx] = 3;

      gSessionTable[sess_idx].vars_rev_trans_table_shr_mem[num_var_table_idx] = var_idx;

      if(groupTable[varTable[j].group_idx].type == ONCE)
        gSessionTable[sess_idx].vars_trans_table_shr_mem[var_idx].retain_pre_value = RETAIN_PRE_VALUE;

      NSDL2_VARS(NULL, NULL, "Fill Trans table for Static Var: var name = %s, hash_code(var_idx) = %d, j = %d, "
                             "fparam_grp_idx = %d, self_idx_wrt_fparam_grp(var_idx) = %d, num_var_table_idx = %d", 
                             name, var_idx, j, varTable[j].group_idx, varTable[j].self_idx_wrt_fparam_grp, num_var_table_idx);
      num_var_table_idx++;
    }

    // We malloc uv table for max number of vars among all scripts 
    if (max_var_table_idx < num_var_table_idx)
      max_var_table_idx = num_var_table_idx;
  }

  return 0;
}

/* This is common code used by every registration api
*  This function is used:
*    1. To set the page status.
*    2. To set the session status.
*    3. To clear the flag.
*/
void 
set_page_status_for_registration_api(VUser *vptr, int pg_status, int to_stop, char *api_name)
{
  NSDL2_VARS(NULL, NULL, "Method called");

  vptr->page_status = pg_status;
  vptr->sess_status = NS_REQUEST_ERRMISC;
  // It action on at least one failed checkreply is Stop, then we need to stop
  // WE are using one bit in the flags to mark it. It will be checked later
  // This is done so that we can execute check_page before we stop the session
  if(!to_stop) { // Not to be stopped
     NSDL4_VARS(vptr, NULL, "Page failed due to %s failure but action is continue, so setting flag to NS_ACTION_ON_FAIL_CONTINUE", api_name);
     vptr->flags |= NS_ACTION_ON_FAIL_CONTINUE;
   }
   else {
     vptr->flags &= ~NS_ACTION_ON_FAIL_CONTINUE; // Must reset as it may be set by other Reg API
     NSDL4_VARS(vptr, NULL, "Page failed due to %s failure and action is to Stop on the failed, so clearing NS_ACTION_ON_FAIL_CONTINUE bit in flags", api_name);
   }
}

/* This is common function used by Search and XmlVar registration api
*  This function is used:
*     To set the redirection depth bitmask.
*     For RedirectionDepth=ALL, set the all bits of the bitmask.
*/

inline int
set_depth_bitmask(unsigned int redirection_depth_bitmask, int redirection_depth)
{

  NSDL2_VARS(NULL, NULL, "Method called");

  unsigned int mask = 0;
  /*bit number should be [depth - 1]
  * if depth = 1 then set first bit[starts from zero]
  * If depth =2 then set the second bit.
  * If depth = ALL then set all the bits, this will be used to call call copy_retrieve_data()
  */
 if(redirection_depth == VAR_ALL_REDIRECTION_DEPTH)
    mask = 0xFFFFFFFF;
 else
    mask = redirection_depth;

  NSDL2_VARS(NULL, NULL, "Set redirection_depth_bitmask = 0x%x", mask | redirection_depth_bitmask);
  return (mask | redirection_depth_bitmask);
}

//This function is used to check if the redirectiondepth is invalid or not
//Return 1 if this is invalid value else 0 on if this is valid.
inline int
check_redirection_limit(int check_buf) 
{
  if((check_buf > MAX_REDIRECTION_DEPTH_LIMIT) || (check_buf <= MIN_REDIRECTION_DEPTH_LIMIT))
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012032_ID ,CAV_ERR_1012032_MSG);

  return 0;
}

