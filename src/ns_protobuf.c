/********************************************************************************************************************
 * File Name      : ns_protobuf.c                                                                                   |
 |                                                                                                                  | 
 * Synopsis       : This file contain functions related to Protobuf. The main purpose of this file is               |
 |                  to Read XML file form memory, encode node values into Protobuf format and create entery into    |
 |                  segment table so that VUser can make Protobuf encoded POST body without any performance issue.  |
 |                  
 * Author(s)      : Manish Kr. Mishra                                                                               |
 |                                                                                                                  |
 * Reviewer(s)    : Devendar Jain                                                                                   |
 |                                                                                                                  |
 * Date           : Sat Apr 6 01:19:38 IST 2019                                                                     |
 |                                                                                                                  |
 * Copyright      : (c) Cavisson Systems                                                                            |
 |                                                                                                                  |
 * Mod. History   :                                                                                                 |
 *******************************************************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <error.h>
#include <errno.h>

#include "nslb_util.h"
#include "nslb_big_buf.h"
#include "nslb_static_var_use_once.h"
#include "nslb_string_util.h"
#include "protobuf/nslb_protobuf_adapter.h"

#include "netomni/src/core/ni_script_parse.h"
#include "netomni/src/core/ni_scenario_distribution.h"

#include "ns_static_use_once.h"
#include "nslb_time_stamp.h"
#include "ns_common.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"

#include "util.h"
#include "ns_test_gdf.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_static_vars.h"
#include "wait_forever.h"
#include "ns_msg_com_util.h"
#include "ns_runtime_changes.h"
#include "ns_static_vars_rtc.h"

#include "netstorm.h"
#include "child_init.h"
#include "unique_vals.h"
#include "ns_child_msg_com.h"
#include "ns_master_agent.h"
#include "ns_trace_level.h"
#include "ns_script_parse.h"

#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_unique_numbers.h"
#include "ns_date_vars.h"
#include "url.h"

/*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
/* ####################### Section: LocalMacros: Start ################################*/
#define CHK_FILE_IS_FILE_EXIST(file) \
{ \
  struct stat stat_st; \
  if(*file == '/') \
    snprintf(abs_proto_path, NS_PB_MAX_PARAM_LEN, "%s", file); \
  else \
    snprintf(abs_proto_path, NS_PB_MAX_PARAM_LEN, "%s/%s/%s", GET_NS_TA_DIR(), \
                 get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), file); \
  if(stat(abs_proto_path, &stat_st) == -1) \
  { \
    NSTL1_OUT(NULL, NULL, "Error: (ns_protobuf_parse_urlAttr) - File '%s' does not exist. Exiting.\n", abs_proto_path); \
    return NS_PARSE_SCRIPT_ERROR; \
  } \
  if(stat_st.st_size == 0) \
  { \
    NSTL1_OUT(NULL, NULL, "Error: (ns_protobuf_parse_urlAttr) - File %s is of zero size. Exiting.\n", abs_proto_path); \
    return NS_PARSE_SCRIPT_ERROR; \
  } \
}

#define CREATE_PROTBUF_MSG_OBJ(msg, file, type, fname, line_number, script_path) \
{ \
  NSDL2_PARSING(NULL, NULL, "Creating Message Obj for '%s'", file); \
  msg = nslb_create_protobuf_msgobj(file, type, script_path, g_ns_wdir); \
  if(msg == NULL) \
  { \
    snprintf(err_msg, err_msg_len, "Error: (ns_protobuf_segment_line) - Message Object creation failed during parsing of flow file '%s:%d' " \
                                   "for Proto file '%s' and Message type '%s'", \
                                    fname, line_number, file, type); \
    return -1; \
  } \
}  
/* ####################### Section: LocalMacros: Start ################################*/


/* ####################### Section: ScriptParsing: Start ################################*/

int ns_protobuf_parse_urlAttr(char *attr_name, char *attr_val, char *flow_file, int url_idx, 
    int sess_idx, int script_ln_no, int pb_attr_flag)
{
  char abs_proto_path[NS_PB_MAX_PARAM_LEN + 1];
  //struct stat stat_st;

  NSDL1_PARSING(NULL, NULL, "Method Called val = %s, flow_file = %s, url_idx = %d, sess_idx, = %d, "
                            "script_ln_no = %d, pb_attr_flag = %d", 
                            attr_val?attr_val:NULL, flow_file, url_idx, sess_idx, script_ln_no, pb_attr_flag);

  //If atrribute value not given
  if(!attr_val)
  {
    NSTL1_OUT(NULL, NULL, "Error: Value for attribute '%s' could not be parsed from flow file '%s'.", 
                               attr_name, flow_file?flow_file:"NULL");
    return NS_PARSE_SCRIPT_ERROR;
  }

  CLEAR_WHITE_SPACE(attr_val);
  CLEAR_WHITE_SPACE_FROM_END(attr_val);

  if(attr_val[0] == '\0')
  {
    NSTL1_OUT(NULL, NULL, "Error: Value for attribute '%s' could not be parsed from flow file '%s'.", 
                               attr_name, flow_file?flow_file:"NULL");
    return NS_PARSE_SCRIPT_ERROR;
  }

  //Proto
  if(!strcasecmp(attr_val, "NA"))
  {
    NSTL1_OUT(NULL, NULL, "Error: Value for attribute '%s' is mandatory. Provide correct value for this attribute.", attr_val);
    return NS_PARSE_SCRIPT_ERROR;
  }

  /* Since Thses fields are opyional so we should not store parametrise field into segment table*/
  switch(pb_attr_flag)
  {
    case NS_PROTBUF_REQ_FILE: 
    {
      //Check Request Proto file existance and make absolute abs_proto_path form attr_val 
      CHK_FILE_IS_FILE_EXIST(attr_val);

      if((requests[url_idx].proto.http.protobuf_urlattr.req_pb_file = copy_into_big_buf(abs_proto_path, 0)) == -1) 
      {
        NSTL1_OUT(NULL, NULL, "Error: failed copying data '%s' into big buffer", abs_proto_path);
        return NS_PARSE_SCRIPT_ERROR;
      }

      NSDL2_PARSING(NULL, NULL, "NS_PROTBUF_REQ_FILE: abs_proto_path = %s", abs_proto_path); 
    }
    break;

    case NS_PROTBUF_RESP_FILE: 
    {
      //Check Response Proto file existance and make absolute abs_proto_path form attr_val 
      CHK_FILE_IS_FILE_EXIST(attr_val);

      if((requests[url_idx].proto.http.protobuf_urlattr.resp_pb_file = copy_into_big_buf(abs_proto_path, 0)) == -1) 
      //if((requests[url_idx].proto.http.protobuf_urlattr.resp_pb_file = copy_into_big_buf(attr_val, 0)) == -1) 
      {
        NSTL1_OUT(NULL, NULL, "Error: failed copying data '%s' into big buffer", abs_proto_path);
        return NS_PARSE_SCRIPT_ERROR;
      }
      NSDL2_PARSING(NULL, NULL, "NS_PROTBUF_RESP_FILE: abs_proto_path = %s", abs_proto_path); 
    }
    break;

    case NS_PROTOBUF_REQ_MESSAGE_TYPE: 
    {
      if((requests[url_idx].proto.http.protobuf_urlattr.req_pb_msg_type = copy_into_big_buf(attr_val, 0)) == -1) 
      {
        NSTL1_OUT(NULL, NULL, "Error: failed copying data '%s' into big buffer", attr_val);
        return NS_PARSE_SCRIPT_ERROR;
      }

      NSDL2_PARSING(NULL, NULL, "NS_PROTOBUF_REQ_MESSAGE_TYPE = %s", attr_val);
    }
    break;

    case NS_PROTOBUF_RESP_MESSAGE_TYPE: 
    {
      if((requests[url_idx].proto.http.protobuf_urlattr.resp_pb_msg_type = copy_into_big_buf(attr_val, 0)) == -1) 
      {
        NSTL1_OUT(NULL, NULL, "Error: failed copying data '%s' into big buffer", attr_val);
        return NS_PARSE_SCRIPT_ERROR;
      }
      NSDL2_PARSING(NULL, NULL, "NS_PROTOBUF_RESP_MESSAGE_TYPE = %s", attr_val);
    }
    break;

    default:
    {
      NSTL1_OUT(NULL, NULL, "Error: '%s' value could not be parsed from flowfile %s", 
                             attr_name, flow_file?flow_file:"NULL");
      return NS_PARSE_SCRIPT_ERROR;
    }
  }

  NSDL2_PARSING(NULL, NULL, "Method End");

  return NS_PARSE_SCRIPT_SUCCESS;
}


/*--------------------------------------------------------------------------------------------------
 * Function name    : fill_segtable_form_segbuf() 
 | 
 * Synopsis         : The purpose of this function is to parse provided segbuf and fill it into segtable.
 |
 * Input            : segtable - pointer to segTable which has to be filled by this function
 |                    segbuf   - single chuncked segmented buffer  
 |                    sess_idx - provide session id 
 |                    err_msg  - buffer to fill for error message
 |                    err_msglen - provide length of 'err_msg'
 |
 * Output           : Provided 'segtable' will be filled and new entries created for follwoing DS 
 |                      (1) segTable 
 |                      (2) pointerTable
 | 
 |                     Suppose Url1 = http://localhost:80/hpd_tours/index.html?name={emp_name}
 |                         and Url1 = http://localhost:80/tiny    
 |                   
 |                         here Url1 has 2 segments, Seg1 => http://localhost:80/hpd_tours/index.html?name=
 |                                                   length = 46
 |                   
 |                                              and  Seg2 => {emp_name}
 |                                                   length = 10 
 |                   
 |                         And Url2 has 1 segment,   Seg1 => http://localhost:80/tiny                      
 |                                                   length = 24  
 |                     
 |                           requests              postTable              segTable               pointerTable          bigBuf
 |                           -------------         ----------------       ----------------      -------------------   +------+
 |                      Url1 |post_idx   |------> 0|seg_start     |----->0|type = STR    |      |big_buf_pointer=0|-->|      |
 |                           |           |         |num_entries=2 |    |  |offset        |----> |size=46          |   +------+
 |                           |           |         |              |    |  |sess_idx      |      -------------------   |      |
 |                           |           |         |              |    |  |data          |                            +------+
 |                           -------------         ----------------    |  ----------------        varTable            |      |
 |                      Url2 |post_idx   |------> 1|seg_start     |--  | 1|type = VAR    |      ------------------    +------+
 |                           |           |         |num_entries   | |  |  |offset        |----> |                |    |      |
 |                           |           |         |              | |  |  |sess_idx      |      |                |    +------+
 |                           |           |         |              | |  |->|data          |      ------------------    |      |
 |                           -------------         ---------------- |     ----------------                            +------+
 |                                                                  ---->2|type          |                            |      |
 |                                                                        |offset        |                            +------+
 |                                                                        |sess_idx      |----> same as Seg1          |      |
 |                                                                        |data          |                            +------+
 |                                    		                          ----------------       
 |
 * Return Value     : 0 - On success 
 |                   -1 - On Failure  
 |
 * Modificaton Date : Sat Apr  6 19:39:25 IST 2019 
 |
 *--------------------------------------------------------------------------------------------------*/
static int fill_segtable_form_segbuf(StrEnt* segtable, unsigned char *segbuf, int line_number, 
                                     int sess_idx, char *fname, char *err_msg, int err_msglen)
{
  unsigned char *segbuf_ptr;
  unsigned short nseg, slen, sfield, sfieldtype, stype;
  int i, rnum, ptable_rnum, param_len, ret_val, idx;
  char param_name[NS_PB_MAX_PARAM_LEN + 1];
  char abs_param_name[NS_PB_MAX_PARAM_LEN + 1];  //Param_Name!Proj/SubProj/Script_Name
  DBT key;
  DBT data;
  int hash_code;

  segbuf_ptr = segbuf;

  NSDL2_PARSING(NULL, NULL, "Method called, segtable = %p, segbuf = %p, sess_idx = %d", segtable, segbuf, sess_idx);

  nslb_read_short(&nseg, segbuf_ptr);

  NSDL2_PARSING(NULL, NULL, "Number of total Segment = %d", nseg);
  
  for(i = 0; i < nseg; i++) 
  {
    if(create_seg_table_entry(&rnum) != SUCCESS) 
    {
      snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): unable to create segTable, "
                                    "total_seg_entries = %d", total_seg_entries);
      return -1;
    }

    NSDL3_PARSING(NULL, NULL, "i = %d, seg idx = %d, sess_idx = %d, segbuf_ptr = %p", i, rnum, sess_idx, segbuf_ptr);
    segTable[rnum].sess_idx = sess_idx;

    if(i == 0)
      segtable->seg_start = rnum;

    segtable->num_entries++;

    /*segbuf_ptr = nslb_read_short(&stype, segbuf_ptr);
    segbuf_ptr = nslb_read_short(&sfield, segbuf_ptr);
    segbuf_ptr = nslb_read_short(&sfieldtype, segbuf_ptr);
    segbuf_ptr = nslb_read_short(&slen, segbuf_ptr);*/

    nslb_read_short(&stype, segbuf_ptr + 2);
    nslb_read_short(&sfield, segbuf_ptr + 4);
    nslb_read_short(&sfieldtype, segbuf_ptr + 6);
    nslb_read_short(&slen, segbuf_ptr + 8);
    
    segTable[rnum].pb_field_number = sfield;
    segTable[rnum].pb_field_type = sfieldtype;

    NSDL2_PARSING(NULL, NULL, "i = %d, sess_idx = %d, Segment-Idx = %hu, Segment-Type = %hu, Segment-Field = %hu, "
                              "Segment-FiledType = %hu, Segment-Len = %hu", 
                               i, sess_idx, rnum, stype, sfield, sfieldtype, slen);

    if(stype == NSLB_PROTO_MESSAGE_START)
    { 
      // Message Start marker is treated as new type of segment PROTOBUF_MSG
      // Because in merging segment we need to alloc memory 
      AddPointerTableEntry(&ptable_rnum, ((char *)segbuf_ptr + 2), (SEG_BUF_HDR_LEN + (slen?slen:NSLB_PB_MSG_MARKER_LEN)));

      segTable[rnum].type = PROTOBUF_MSG;
      segTable[rnum].offset = ptable_rnum;

      NSDL2_PARSING(NULL, NULL, "Segment-Type = PROTOBUF_MSG, offset = %d, segbuf_ptr = %s", ptable_rnum, segbuf_ptr);

      //#ifdef NS_DEBUG_ON
      //show_buf(segbuf_ptr, slen);
      //#endif
    } 
    else if((stype == NSLB_PROTO_NoN_PARAM) || (stype == NSLB_PROTO_MESSAGE_END))
    { 
      //Message End marker will be treated as STR
      AddPointerTableEntry(&ptable_rnum, ((char *)segbuf_ptr + 2), (SEG_BUF_HDR_LEN + (slen?slen:NSLB_PB_MSG_MARKER_LEN)));

      segTable[rnum].type = STR;
      segTable[rnum].offset = ptable_rnum;

      NSDL2_PARSING(NULL, NULL, "Segment-Type = STR, offset = %d, segbuf_ptr = %s", ptable_rnum, segbuf_ptr);

      //#ifdef NS_DEBUG_ON
      //show_buf(segbuf_ptr, slen);
      //#endif
    } 
    else 
    { //Odd are variables
      nslb_strncpy(param_name, NS_PB_MAX_PARAM_LEN, (char *)(segbuf_ptr + SEG_BUF_HDR_LEN + 2), slen);
      NSDL2_PARSING(NULL, NULL, "Segment is parametrised, param_name = %s", param_name);

      param_len = slen;
      if(gSessionTable[sess_idx].var_hash_func)
	hash_code = gSessionTable[sess_idx].var_hash_func(param_name, param_len);
      else
	hash_code = -1;

      NSDL2_PARSING(NULL, NULL, "hash_code = %d", hash_code);
      if(hash_code != -1) 
      {
        NSDL2_PARSING(NULL, NULL, "hash_code = %d", hash_code); 
        
	switch (gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type) 
        {
          case VAR:
          case INDEX_VAR:
          {
	    memset(&key, 0, sizeof(DBT));
	    memset(&data, 0, sizeof(DBT));
           
	    snprintf(abs_param_name, NS_PB_MAX_PARAM_LEN, "%s!%s", 
                            param_name, 
                            get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "-"));

	    key.data = abs_param_name;
	    key.size = strlen(abs_param_name);
           
	    ret_val = var_hash_table->get(var_hash_table, NULL, &key, &data, 0);
           
	    if(ret_val == DB_NOTFOUND) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Variable %s not defined. Used on line=%d in file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
           
	    if(ret_val != 0) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Hash Table Get failed for NS var '%s'on line=%d in file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
           
	    segTable[rnum].type = gSessionTable[sess_idx].vars_trans_table_shr_mem[hash_code].var_type;
	    segTable[rnum].offset = (int) atoi((char*)data.data);
           
	    NSDL2_PARSING(NULL, NULL, "Variable is of type VAR/Index. runm=%d var=%s offset=%d", rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case COOKIE_VAR:
          {
            // For Auto Cookie Mode, we are not supporting COOKIE Vars for now. So code will come
            // here only for Manual Cookies
	    idx = Create_cookie_entry(param_name, sess_idx);
	    segTable[rnum].type = COOKIE_VAR;
	    segTable[rnum].offset = idx;
	    NSDL2_PARSING(NULL, NULL, "Variable is of type COOKIE. runm=%d var=%s cookie_idx=%d", rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case SEARCH_VAR:
          {
	    if((idx = find_searchvar_idx(param_name, param_len, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the search var %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
	    segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used
	    segTable[rnum].type = SEARCH_VAR;
	    segTable[rnum].offset = idx;    /* this value is changed later to the usertable variable index*/
	    NSDL2_PARSING(NULL, NULL, "Variable is of type SEARCH_VAR. runm=%d var=%s searchvar_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

          case JSON_VAR:
          {
            if((idx = find_jsonvar_idx(param_name, param_len, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the JSON var %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
            }
            segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used
            segTable[rnum].type = JSON_VAR;
            segTable[rnum].offset = idx;    /* this value is changed later to the usertable variable index*/
            NSDL2_PARSING(NULL, NULL, "Variable is of type JSON_VAR. runm=%d var=%s jsonvar_idx=%d", rnum, param_name, segTable[rnum].offset);
          }
          break;

  	  case TAG_VAR:
          { 
	    if ((idx = find_tagvar_idx(param_name, param_len, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the tag var variable %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
	    segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used
	    segTable[rnum].type = TAG_VAR;
	    segTable[rnum].offset = idx;  /* this value is changed later to the usertable variable index */
	    NSDL2_PARSING(NULL, NULL, "Variable is of type TAG_VAR. runm=%d var=%s tagvar_idx=%d", rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case NSL_VAR:
          {
	    if((idx = find_nslvar_idx(param_name, param_len, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the nsl var variable %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
	    segTable[rnum].data = -1; // Set this as we are checking data in insert_segments to get the index used 
	    segTable[rnum].type = NSL_VAR;
	    segTable[rnum].offset = idx;  /* this value is changed later to the usertable variable index */
	    NSDL2_PARSING(NULL, NULL, "Variable is of type NSL_VAR (declared var). runm=%d var=%s nslvar_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

  	  case CLUST_VAR:
          {
	    if((idx = find_clustvar_idx(param_name)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the nsl var variable %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
	    segTable[rnum].type = CLUST_VAR;
	    segTable[rnum].offset = idx;
	    NSDL2_PARSING(NULL, NULL, "Variable is of type CLUST_VAR. runm=%d var=%s clustvar_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case GROUP_VAR:
          {
	    if((idx = find_groupvar_idx(param_name)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the nsl var variable %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
	    segTable[rnum].type = GROUP_VAR;
	    segTable[rnum].offset = idx;
	    NSDL2_PARSING(NULL, NULL, "Variable is of type GROUP_VAR. runm=%d var=%s groupvar_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case GROUP_NAME_VAR:
          {
	    segTable[rnum].type = GROUP_NAME_VAR;
	    segTable[rnum].offset = 0;
	    NSDL2_PARSING(NULL, NULL, "Variable is of type GROUP_NAME_VAR. runm=%d var=%s group_name_var_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case CLUST_NAME_VAR:
          {
	    segTable[rnum].type = CLUST_NAME_VAR;
	    segTable[rnum].offset = 0;
	    NSDL2_PARSING(NULL, NULL, "Variable is of type CLUST_NAME_VAR. runm=%d var=%s clust_name_var_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case USERPROF_NAME_VAR:
          {
	    segTable[rnum].type = USERPROF_NAME_VAR;
	    segTable[rnum].offset = 0;
	    NSDL2_PARSING(NULL, NULL, "Variable is of type USERPROF_NAME_VAR. runm=%d var=%s userprof_name_var_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

	  case HTTP_VERSION_VAR:
          {
	    segTable[rnum].type = HTTP_VERSION_VAR;
	    segTable[rnum].offset = 0;
	    NSDL2_PARSING(NULL, NULL, "Variable is of type HTTP_VERSION_VAR. runm=%d var=%s http_version_var_idx=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
	  break;

          case RANDOM_VAR:
          {
	    if((idx = find_randomvar_idx(param_name, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the nsl var variable %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
	    }
            segTable[rnum].type = RANDOM_VAR;
            segTable[rnum].offset = idx;
            NSDL2_PARSING(NULL, NULL, "Variable is of type RANDOM_VAR, runm=%d var=%s offset=%d", rnum, param_name, segTable[rnum].offset);
          }
          break;

          case RANDOM_STRING:
          {
            if((idx = find_randomstring_idx(param_name, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the nsl var variable %s on line = %d file %s",
                                            param_name, line_number, fname);
              return -1;
            }
            segTable[rnum].type = RANDOM_STRING;
            segTable[rnum].offset = idx;
            NSDL2_PARSING(NULL, NULL, "Variable is of type RANDOM_STRING, runm=%d var=%s offset=%d", 
                                       rnum, param_name, segTable[rnum].offset);
          }
          break;
  
          case UNIQUE_VAR:
          {
            if((idx = find_uniquevar_idx(param_name, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the nsl var variable %s on line = %d file %s",
                                            param_name, line_number, fname);
              return -1;
            }
            segTable[rnum].type = UNIQUE_VAR;
            segTable[rnum].offset = idx;
            NSDL2_PARSING(NULL, NULL, "Variable is of type UNIQUE_VAR, runm=%d var=%s offset=%d", rnum, param_name, segTable[rnum].offset);
          }
          break;

          case DATE_VAR:
          {
            if((idx = find_datevar_idx(param_name, sess_idx)) == -1) 
            {
              snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Could not find the nsl var variable %s on line = %d file %s",
                                             param_name, line_number, fname);
              return -1;
            }
            segTable[rnum].type = DATE_VAR;
            segTable[rnum].offset = idx;
            NSDL2_PARSING(NULL, NULL, "Variable is of type DATE_VAR, runm=%d var=%s offset=%d", rnum, param_name, segTable[rnum].offset);
          }
          break;

	  default:
          {
            snprintf(err_msg, err_msglen, "Error: fill_segtable_form_segbuf(): Unsupported Var type  var %s on line = %d file %s",
                                           param_name, line_number, fname);
            return -1;
          }
	}
      } /*else {
	param_len += 2; //with curly brackets
	nwvar_length = htons(param_len);
	bcopy ((char *)&nwvar_length, var, 2);
	sprintf (var+2, "{%s}", mptr);

	AddPointerTableEntry (&point_rnum, var, param_len+2); //var in UTF

	segTable[rnum].type = STR;
	segTable[rnum].offset = point_rnum;
      }*/
    }

    NSDL2_PARSING(NULL, NULL, "segbuf_ptr = %p, slen = %hu, SEG_BUF_HDR_LEN = %d", segbuf_ptr, slen, SEG_BUF_HDR_LEN);
    segbuf_ptr += SEG_BUF_HDR_LEN + (slen?slen:NSLB_PB_MSG_MARKER_LEN);
  }

  NSDL2_PARSING(NULL, NULL, "Method end.");
  return 0;
}

int ns_protobuf_segment_line(int url_idx, StrEnt* segtable, char* line, int noparam_flag,
                 int line_number, int sess_idx, char *fname, char *err_msg, int err_msg_len)
{
  unsigned char *obuf = NULL; 
  int obuf_size = 0;
  int obuf_len;
  int len;
  ProtobufUrlAttr *pb;

  NSDL2_PARSING(NULL, NULL, "Method called, segtable = %p, line = %p, noparam_flag = %d, line_number = %d, "
                            "sess_idx = %d, fname = %s, err_msg_len = %d, line = %s", 
                             segtable, line, noparam_flag, line_number, sess_idx, fname, err_msg_len, line);

  char *sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char *script_name = get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/");

  //Validations
  if(!line || !fname)
  {
    snprintf(err_msg, err_msg_len,  "Input is not provided correctly for protobuf request, Inputs: line = %p, fname = %p", 
                                     line, fname); 
    return -1;
  }

  pb = &requests[url_idx].proto.http.protobuf_urlattr;

  //Create Message Object for Request
  NSDL2_PARSING(NULL, NULL, "req_pb_file = %ld, req_pb_msg_type = %ld, resp_pb_file = %ld, resp_pb_msg_type = %ld", 
                             pb->req_pb_file, pb->req_pb_msg_type, pb->resp_pb_file, pb->resp_pb_msg_type);

  if(pb->req_pb_file != -1 && pb->req_pb_msg_type != -1)
  {
    char script_path[NS_PB_MAX_PARAM_LEN + 1];
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    snprintf(script_path, NS_PB_MAX_PARAM_LEN, "%s/%s", GET_NS_TA_DIR(), \
                 get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));
    CREATE_PROTBUF_MSG_OBJ(pb->req_message, RETRIEVE_BUFFER_DATA(pb->req_pb_file), RETRIEVE_BUFFER_DATA(pb->req_pb_msg_type), fname, line_number, script_path);
  }
  else 
  {
    snprintf(err_msg, err_msg_len, "Request Proto File and "
                                   "Message type is not given for  flow file '%s:%d' ",
                                    fname, line_number);
    return -2;
  }

  //Encode Post body into Google's Protocol Buffer and make a Single Chunck Segmented buffer 
  len = strlen(line);

  obuf_len = nslb_encode_protobuf(line, len, noparam_flag, pb->req_message, &obuf, &obuf_size, err_msg, NS_PB_MAX_ERROR_MSG_LEN); 
  
  NSDL2_PARSING(NULL, NULL, "After protobuf encoding, obuf_len = %d", obuf_len);
  if(obuf_len == -1)
  {
    snprintf(err_msg, err_msg_len,  "Unable to read and encode XML Req Body into Protocol Buffer for Script-Id = %d, "
                                    "Script-Name = %s, Script LineNumber = %d, Flow-Name = %s",
                                     sess_idx, script_name, line_number, fname); 
    return -1;
  }

  //#ifdef NS_DEBUG_ON
  //nslb_dump_segbuf(obuf);
  //nslb_hexdump(obuf, obuf_len);
  //#endif

  //Parse buffer found by nsi_encode_protobuf(), and fill segtable
  return fill_segtable_form_segbuf(segtable, obuf, line_number, sess_idx, fname, err_msg, NS_PB_MAX_ERROR_MSG_LEN);
}

void *ns_create_protobuf_msgobj(char *proto_file, char *msg_type, char *script_path)
{
  return nslb_create_protobuf_msgobj(proto_file, msg_type, script_path, g_ns_wdir);
}

int make_file_path(char *proto_or_xml_fname, char *abs_proto_fname, VUser *vptr, int proto_or_xml)
{
  int sess_id;
  char *sess_name;
  char *script_name;

  sess_name = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_name;
  sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
  script_name = get_sess_name_with_proj_subproj_int(sess_name, sess_id, "/");

  if(proto_or_xml_fname[0] == '/')
  {
    if(proto_or_xml)
      snprintf(abs_proto_fname, 2048, "%s", proto_or_xml_fname + 1); //Skipping /
    else
      snprintf(abs_proto_fname, 2048, "%s", proto_or_xml_fname);
  }
  else
  {
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    if(proto_or_xml){
      snprintf(abs_proto_fname, 2048, "%s/%s/%s", GET_NS_TA_DIR() + 1, script_name, proto_or_xml_fname); //skipping '/'
    }
    else{
      snprintf(abs_proto_fname, 2048, "%s/%s/%s", GET_NS_TA_DIR(), script_name, proto_or_xml_fname);  
    }
  }

  NSDL2_PARSING(NULL, NULL, "abs_proto_fname = %s, proto_or_xml = %d", abs_proto_fname, proto_or_xml); 
  return 0;
}

int read_xml_file(char *xml_fname, char **in_xml, long *in_xml_len)
{
  int fd;

  if(xml_fname[0])
  {
    struct stat st;

    if(stat(xml_fname, &st) == -1)
    {
      fprintf(stderr, "Error: Provide file '%s' does not exist, errno = %d, error = %s.\n", xml_fname, errno, nslb_strerror(errno));
      return -1;
    }

    if(st.st_size == 0)
    {
      fprintf(stderr, "Error: File '%s' exist with size 0.\n", xml_fname);
      return -1;
    }

    *in_xml_len = st.st_size;

    MY_MALLOC(*in_xml, *in_xml_len + 1, "xml data buffer", -1);

    if((fd = open(xml_fname, O_RDONLY | O_CLOEXEC | O_LARGEFILE)) < 0)
    {
      fprintf(stderr, "Error: to open file '%s'. errno = %d, error = %s\n", xml_fname, errno, nslb_strerror(errno));
      return -1;
    }

    nslb_read_file_and_fill_buf(fd, *in_xml, *in_xml_len);

    NSDL2_PARSING(NULL, NULL, "in_xml = %s, in_xml_len = %d", *in_xml, *in_xml_len);   
  }
  return 0;
}

static int create_vectors_from_segbuf(unsigned char *segbuf_ptr, struct iovec *seg_vector)
{
  unsigned short nseg, len, fieldnum, seg_type, fieldtype;
  int i;

  //printf("Method called, segbuf_ptr = %p, seg_vector = %p\n", segbuf_ptr, seg_vector);

  if(!segbuf_ptr)
    return -1;

  nslb_read_short(&nseg, segbuf_ptr);

  //printf("Number of Segments = %d\n", nseg);

  for (i = 0; i < nseg; i++)
  {
    nslb_read_short(&seg_type, segbuf_ptr + 2);
    nslb_read_short(&fieldnum, segbuf_ptr + 4);
    nslb_read_short(&fieldtype, segbuf_ptr + 6);
    nslb_read_short(&len, segbuf_ptr + 8);

    //printf("i = %d, seg_type = %d, fieldnum = %d, fieldtype = %d, len = %d, lptr = %p\n", i, seg_type, fieldnum, fieldtype, len, (segbuf_ptr + 8));

    seg_vector[i].iov_base = segbuf_ptr + 2;
    seg_vector[i].iov_len = len + SEG_BUF_HDR_LEN;

    segbuf_ptr += (len?len:8) + SEG_BUF_HDR_LEN;
  }

  return nseg;
}


int parse_protobuf_encoded_segbuf(void *message, unsigned char *segbuf_ptr, unsigned char *out_buf, unsigned int obuf_size)
{
  struct iovec vectors[1024];
  int num_vectors;
  int start_idx = 0;

  unsigned long msg_len;

  //printf("Method called, message = %p, segbuf_ptr = %p, out_buf = %p, obuf_size = %d\n",
    //                      message, segbuf_ptr, out_buf, obuf_size);

  //Create vector form segmented buffer
  num_vectors = create_vectors_from_segbuf(segbuf_ptr, vectors);

  //Make Final Potobuf Encoded buffer
  msg_len = merge_and_make_protobuf_encoded_data(message, vectors, &start_idx, num_vectors, out_buf, obuf_size);

  return msg_len;
}

/* ####################### Section: ScriptParsing: End   ################################*/
