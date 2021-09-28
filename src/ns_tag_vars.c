/********************************************************************
Pgrogam Name: ns_tag_vars.c
Purpose     : All code relate to xml var api 
              This file is broken into three parts 
                1.Parsing of tag var
                2.Sharing related logics
                3.Runtime logics

Example     : To understand the whole xml_var functionality lets take a example...
              --------------------------------------------------------------------------------------------------------
		 <Root>
		  <Profile>
		   <Employe id="1">
		    <Name>NIMOO
		     <Friend fid="1" city="noida" class="15" age="27" length="5.6feet" wt="55kg"> Shakti </Friend>
		     <Friend fid="2" city="ghaziabad" class="16 age="28" length="5.8feet" wt="70kg"> Abhishek </Friend>
		     <Friend fid="3" city="delhi" class="16" age="25" length="5.4feet" wt="56"> Manpree </Friend>
		     <Friend fid="4" city="agara" class="17" age="26" length="5.2feet" wt="50kg"> Reshi </Friend>
		     <Friend fid="5" city="allahabad" class="15" age="25" length="5.7feet" wt="58kg"> Richa </Friend>
		    </Name>
		   </Employe>
		  </Profile>
		 </Root>
              --------------------------------------------------------------------------------------------------------

Inital 
Version     : -

Modified    : Manish Kumar Mishra 
              Date: Fri Mar 23 08:51:11 EDT 2012
***************************************************************/

/************************************************************************************************************************
                                          ----:Header Files:----
************************************************************************************************************************/
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
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "url.h"
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
#include "ns_tag_vars.h"
#include "ns_vars.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "poi.h"
#include "amf.h"
#include "ns_cookie.h"
#include "ns_debug_trace.h"
#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_unique_numbers.h"
#include "ns_date_vars.h"
#include "ns_static_use_once.h"
#include "nslb_time_stamp.h"
#include "ns_script_parse.h"
#include "nslb_hash_code.h"
#include "ns_exit.h"
#include "ns_error_msg.h"

//**************************Part (1)- Parsing of xml vars*********************
#ifndef CAV_MAIN
extern int max_tag_entries;
extern int total_tag_entries;
extern int max_tagpage_entries;
extern int total_tagpage_entries;
#else
extern __thread int max_tag_entries;
extern __thread int total_tag_entries;
extern __thread int max_tagpage_entries;
extern __thread int total_tagpage_entries;
#endif

void copy_all_tag_vars(UserVarEntry* uservartable_entry, NodeVarTableEntry_Shr *nodevar_ptr);

int create_tag_table_entry(int* row_num) 
{
  NSDL1_VARS(NULL, NULL, "Method called. total_tag_entries = %d, max_tag_entries = %d", total_tag_entries, max_tag_entries);
 
  if (total_tag_entries == max_tag_entries) {
    MY_REALLOC(tagTable, (max_tag_entries + DELTA_TAG_ENTRIES) * sizeof(TagTableEntry), "tag entries", -1);
    max_tag_entries += DELTA_TAG_ENTRIES;
  }
  *row_num = total_tag_entries++;
  return (SUCCESS);
}


int create_tagpage_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_tagpage_entries = %d, max_tagpage_entries = %d", total_tagpage_entries, max_tagpage_entries);

  if (total_tagpage_entries == max_tagpage_entries) {
    MY_REALLOC(tagPageTable, (max_tagpage_entries + DELTA_TAGPAGE_ENTRIES) * sizeof(TagPageTableEntry), "tagpage entries", -1);
    max_tagpage_entries += DELTA_TAGPAGE_ENTRIES;
  }
  *row_num = total_tagpage_entries++;
  return (SUCCESS);
}

#ifndef CAV_MAIN
extern int total_attrqual_entries;
extern int max_attrqual_entries;
#else
extern __thread int total_attrqual_entries;
extern __thread int max_attrqual_entries;
#endif
int create_attrqual_table_entry(int* row_num) {
  NSDL1_VARS(NULL, NULL, "Method called. total_attrqual_entries = %d and max_attrqual_entries = %d", total_attrqual_entries, max_attrqual_entries);
  if (total_attrqual_entries == max_attrqual_entries) {
    MY_REALLOC(attrQualTable, (max_attrqual_entries + DELTA_ATTRQUAL_ENTRIES) * sizeof(AttrQualTableEntry), "attrqual entries", -1);
    max_attrqual_entries += DELTA_ATTRQUAL_ENTRIES;
  }
  *row_num = total_attrqual_entries++;
  return (SUCCESS);
}

/*****************************************
Parsing of xml var api..
  nsl_xml_var(srchFriend, PAGE=page1_xml, PAGE=page2_xml, NODE=<Root><Profile><Student><Name><Friend>, VALUE=<>, WHERE=<city>="allahabad", WHERE=<class>="15", ORD=2, RedirectionDepth=1;2;3, Convert=HTMLToURL);

 Terms: srch --> xml variable (parameter name), it will uniqu to pet api
        PAGE --> page name on which xml pattern will meet, it can be one, multiple, any, all
        NODE --> define the path of desired node
        VALUE -> define value on which we are intrested (i.e. node value or attribute value)
                 VALUE=<> --> will give the value of node i.e. 'NIMOO' in above defined xml example
                 VALUE=<wt> --> will give value of wt attribute i.e. '58kg' in above defined xml example
       WHERE --> define the more specific value. Actually it the qualifier of node or attribute value
       ORD  -->  define the occurance of particular node pattern 
       RedirectionDepth --> define the depth of node value
****************************************/
int input_tagvar_data(char* line, int line_number, int sess_idx, char *script_filename) {
#define NS_ST_TV_NAME 1
#define NS_ST_TV_ATTR 2
  int state = NS_ST_TV_NAME;
  char tagvar_buf[MAX_LINE_LENGTH];
  char tagvar_buf_local[MAX_LINE_LENGTH];
  char* line_ptr = line; 
  // line_ptr --> srchFriend, PAGE=page1_xml, PAGE=page2_xml, NODE=<Root><Profile><Student><Name><Friend>, VALUE=<>, WHERE=<city>="allahabad", WHERE=<class>="15", ORD=2, RedirectionDepth=1;2;3, Convert=HTMLToURL 
  int done = 0;
  int i;
  int rnum=-1;
  int within_tag;
  int attr_idx;
  int attr_qual;
  char* sess_name = RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name);
  char* attr_end;
  char page_name[MAX_LINE_LENGTH];
  int varpage_idx;
  char* attr_ptr;
  int within_quotes;
  char msg[MAX_LINE_LENGTH];
  int encode_flag_specified = 0;
  char encode_chars_done = 0;

  NSDL1_VARS(NULL, NULL, "Method called. line = [%s], line_number = %d, sess_idx = %d", line, line_number, sess_idx);
  /*bug id: 101320: trace updated to show NS_TA_DIR*/
  snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_xml_var() decalration on line %d of %s/%s/%s: ", 
		  line_number, GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), script_filename);
  //Previously taking with only script name
  //snprintf(msg, MAX_LINE_LENGTH, "Parsing nsl_xml_var() decalration on line %d of scripts/%s/%s: ", line_number, get_sess_name_with_proj_subproj(sess_name), script_filename);
  msg[MAX_LINE_LENGTH-1] = '\0';

  while (!done) {
    NSDL2_VARS(NULL, NULL, "state = %d", state);
    switch (state) {
    case NS_ST_TV_NAME:
      CLEAR_WHITE_SPACE(line_ptr);
      for (i = 0; line_ptr && ((*line_ptr) != ' ') && ((*line_ptr) != '\t') && (*line_ptr != ','); line_ptr++, i++) {
	tagvar_buf[i] = *line_ptr;
      }
      tagvar_buf[i] = '\0'; 
      //tagvar_buf --> srchFriend 
      CLEAR_WHITE_SPACE(line_ptr);
      CHECK_CHAR(line_ptr, ',', msg);
      create_tag_table_entry(&rnum);
      tagTable[rnum].attr_qual_start = -1;
      tagTable[rnum].num_attr_qual = 0;
      tagTable[rnum].value_qual = -1;
      tagTable[rnum].order = 1; /* Default to first ord */
      //tagTable[rnum].notfound_str = -1;
      tagTable[rnum].type = TAG_TYPE_NONE;
      tagTable[rnum].tag_name = -1;
      tagTable[rnum].getvalue = -2;
      //bhushan
      tagTable[rnum].action_on_notfound = VAR_NOTFOUND_IGNORE;
      tagTable[rnum].tagvar_rdepth_bitmask = VAR_IGNORE_REDIRECTION_DEPTH;//take the last bydefault
      tagTable[rnum].convert = VAR_CONVERT_NONE;
//in the case of special char encode, filling the default values
      MY_MALLOC(tagTable[rnum].encode_space_by, strlen("+") , "tagTable[rnum].encode_space_by", -1);
      strcpy(tagTable[rnum].encode_space_by, "+");

      tagTable[rnum].encode_type = ENCODE_NONE;

      memset(tagTable[rnum].encode_chars, 49, TOTAL_CHARS);

      for(i = 'a'; i<='z';i++)
        tagTable[rnum].encode_chars[i] = 0;
      for(i = 'A'; i<='Z';i++)
        tagTable[rnum].encode_chars[i] = 0;
      for(i = '0'; i<='9';i++)
        tagTable[rnum].encode_chars[i] = 0;

      tagTable[rnum].encode_chars['+'] = 0;
      tagTable[rnum].encode_chars['.'] = 0;
      tagTable[rnum].encode_chars['_'] = 0;
      tagTable[rnum].encode_chars['-'] = 0;
//

      if (gSessionTable[sess_idx].tag_start_idx == -1) {
	gSessionTable[sess_idx].tag_start_idx = rnum;
	gSessionTable[sess_idx].num_tag_entries = 0;
      }

      gSessionTable[sess_idx].num_tag_entries++;

      if ((tagTable[rnum].name = copy_into_temp_buf(tagvar_buf, 0)) == -1) {
        NS_EXIT(-1, CAV_ERR_1000018, tagvar_buf);
      }
      NSDL2_VARS(NULL, NULL, "name = [%s], num_tag_entries = %d", tagvar_buf, gSessionTable[sess_idx].num_tag_entries);
      state = NS_ST_TV_ATTR;
      //printf("Processing xml var %s\n", tagvar_buf);
      break;

    case NS_ST_TV_ATTR:
      CLEAR_WHITE_SPACE(line_ptr);
      NSDL2_VARS(NULL, NULL, "line_ptr = [%s]", line_ptr);
      /*Handling comma sign (,) within double quotes */
      //for (within_quotes = 0, i = 0; (*line_ptr != '\0') && ((*line_ptr) != ' ') && ((*line_ptr) != '\t'); line_ptr++, i++) {
      //[ORD=1, VALUE=<>, ActionOnNotFound=Warning, EncodeMode=Specified, CharsToEncode="'\"<>#%{}|\\^~[]`@", EncodeSpaceBy = %20
      //CharsToEncode="'\"<>#%{}|\\^~[]`@"
      for (within_quotes = 0, i = 0; (*line_ptr != '\0') ; line_ptr++, i++) {
	if (*line_ptr == '"' && *(line_ptr - 1) != '\\')
	  within_quotes = !within_quotes;

	//KQ: Is the value always expected within quotes?
	if ((*line_ptr == ',') && (!within_quotes))
	  break;

	tagvar_buf[i] = *line_ptr;
      }
      tagvar_buf[i] = 0; //NUll at last of tag buf

      NSDL2_VARS(NULL, NULL, "tag_var_buf for each time = [%s]", tagvar_buf);
      CLEAR_WHITE_SPACE(line_ptr);

      if (*line_ptr == ',')
	line_ptr++;

      tagvar_buf[i] = '\0';
      //tagvar_buf --> PAGE=page1_xml  (1st pass)
      //               PAGE=page2_xml  (2nd pass)
      //               NODE=<Root><Profile><Student><Name><Friend>  (3rd pass)
      //               VALUE=<> (4th time) 
      //               WHERE=<city>="allahabad" and so...

      //KQ: this arg is not processed yet.
      //parsing of nsl_xml_var have been finished.
      if (*line_ptr == '\0') {
	done = 1;
	//break;
      }

      if ((attr_end = strchr(tagvar_buf, '=')) == NULL) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012135_ID, CAV_ERR_1012135_MSG);
      }
      if (!strncmp(tagvar_buf, "PAGE", strlen("PAGE"))) {
	attr_ptr = attr_end+1; //skip =
	CLEAR_WHITE_SPACE(attr_ptr);
	for (i = 0; (i < MAX_LINE_LENGTH) && ((*attr_ptr != '\0') && ((*attr_ptr) != ' ') && ((*attr_ptr) != '\t') && (*attr_ptr != ')')); i++, attr_ptr++)
	  page_name[i] = *attr_ptr;
	page_name[i] = '\0';

	// At the end of this ns_xml_var() parsing, check would be done for type == TAG_TYPE_NONE
  	// as A PAGE option must be specified.
	//Later on, only TAG_TYPE_ALL will be checked for. If there is a single PAGE=ALL, make sure
	//type is TAG_TYPE_ALL irrespective of the order of page options.
	if (!strcasecmp(page_name, "*"))
	  tagTable[rnum].type = TAG_TYPE_ALL;
	else {
	  if (tagTable[rnum].type == TAG_TYPE_NONE)
	    tagTable[rnum].type = TAG_TYPE_PAGE;

	  create_tagpage_table_entry(&varpage_idx); 

          NSDL3_VARS(NULL, NULL, "varpage_idx = %d", varpage_idx);
	  if (gSessionTable[sess_idx].tagpage_start_idx == -1) {
	    gSessionTable[sess_idx].tagpage_start_idx = varpage_idx;
	    gSessionTable[sess_idx].num_tagpage_entries = 0;
	  }

	  gSessionTable[sess_idx].num_tagpage_entries++;

	  tagPageTable[varpage_idx].tagvar_idx = rnum;
	  if ((tagPageTable[varpage_idx].page_name = copy_into_temp_buf(page_name, 0)) == -1) {
            NS_EXIT(-1, CAV_ERR_1000018, page_name);
	  }

	  tagPageTable[varpage_idx].page_idx = -1;
          NSDL3_VARS(NULL, NULL, "page_name = %d, tagvar_idx = %d, tagpage_start_idx = %d", tagPageTable[varpage_idx].page_name, rnum, varpage_idx);

	}
      } else if (!strncmp(tagvar_buf, "NODE", strlen("NODE"))) {
        if ( tagTable[rnum].tag_name != -1 ) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Node", "XML");
        }
	attr_ptr = attr_end+1;
	CLEAR_WHITE_SPACE(attr_ptr);
	within_tag = 0;
	for (i = 0; (*attr_ptr != ',') && (*attr_ptr != '\0') && (*attr_ptr != ' '); attr_ptr++, i++) {
	  if ((!within_tag) && (*attr_ptr == ' '))
	    break;
	  if (*attr_ptr == '<') {
	    if (within_tag) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012134_ID, CAV_ERR_1012134_MSG, "NODE");
	    } else
	      within_tag = 1;
	  }
	  if (*attr_ptr == '>') {
	    if (!within_tag) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012134_ID, CAV_ERR_1012134_MSG, "NODE");
	    } else
	      within_tag = 0;
	  }
	  tagvar_buf[i] = *attr_ptr;
	}
	if (within_tag) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012134_ID, CAV_ERR_1012134_MSG, "NODE");
	}
	tagvar_buf[i] = '\0';
        //tagvar_buf --> <Root><Profile><Student><Name><Friend> 
	CLEAR_WHITE_SPACE(line_ptr);
	if ((tagTable[rnum].tag_name = copy_into_temp_buf(tagvar_buf, 0)) == -1) {
          NS_EXIT(-1, CAV_ERR_1000018, tagvar_buf);
	}
	break;
      } else if (!strncmp(tagvar_buf, "VALUE", strlen("VALUE"))) {
	if (tagTable[rnum].getvalue != -2) {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Attribute", "XML");
	}
	attr_ptr = attr_end+1;
	CLEAR_WHITE_SPACE(attr_ptr);
	CHECK_CHAR(attr_ptr, '<', msg);
	for (i = 0; (*attr_ptr != '\0') && (*attr_ptr != '>') && (*attr_ptr != ',') && (*attr_ptr != ')'); i++,attr_ptr++) {
	  tagvar_buf[i] = *attr_ptr;
	}
	CHECK_CHAR(attr_ptr, '>', msg);
	tagvar_buf[i] = '\0';
	if (strlen(tagvar_buf)) {
	  if ((tagTable[rnum].getvalue = copy_into_temp_buf(tagvar_buf, 0)) == -1) {
            NS_EXIT(-1, CAV_ERR_1000018, tagvar_buf);
	  }
	} else
	  tagTable[rnum].getvalue = -1;
	break;
      } else if (!strncmp(tagvar_buf, "ORD", strlen("ORD"))) {
	if (tagTable[rnum].order != 1) { /* Fix ME: one can specify ORD=1 twice */
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "ORD", "XML");
	}
	attr_ptr = attr_end+1;
	CLEAR_WHITE_SPACE(attr_ptr);
	for (i = 0; (*attr_ptr != ',') && (*attr_ptr != ' ') && (*attr_ptr != ')') && (*attr_ptr != '\0'); attr_ptr++, i++) {
	    tagvar_buf[i] = *attr_ptr;
	}
	tagvar_buf[i] = '\0';
	CLEAR_WHITE_SPACE(attr_ptr);

        if (!strcasecmp(tagvar_buf, "ANY"))
          tagTable[rnum].order = ORD_ANY;
        else if (!strcasecmp(tagvar_buf, "ALL"))
          tagTable[rnum].order = ORD_ALL;
        else if (!strcasecmp(tagvar_buf, "LAST"))
          tagTable[rnum].order = ORD_LAST;
         else {
          for (i = 0; tagvar_buf[i]; i++) {
            if (!isdigit(tagvar_buf[i])) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012136_ID, CAV_ERR_1012136_MSG, "ANY, ALL, LAST", "XML");
            }
          }
          if(i == 0) {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "ORD", "XML");
          }
          tagTable[rnum].order = atoi(tagvar_buf);
        }
	break;
      }else if (!strncmp(tagvar_buf, "EncodeMode", strlen("EncodeMode"))) {//encode support fields
  attr_ptr = attr_end+1;
  CLEAR_WHITE_SPACE(attr_ptr);
  for (i = 0; (*attr_ptr != ',') && (*attr_ptr != ' ') && (*attr_ptr != ')') && (*attr_ptr != '\0'); attr_ptr++, i++) {
      tagvar_buf[i] = *attr_ptr;
  }
  tagvar_buf[i] = '\0';
  CLEAR_WHITE_SPACE(attr_ptr);

  if (!strcasecmp(tagvar_buf, "All")) {
         tagTable[rnum].encode_type = ENCODE_ALL;
  } else if (!strcasecmp(tagvar_buf, "None")) {
       tagTable[rnum].encode_type = ENCODE_NONE;
      memset( tagTable[rnum].encode_chars, 0, TOTAL_CHARS);
  } else if (!strcasecmp(tagvar_buf, "Specified")) {
      encode_flag_specified = 1;
      tagTable[rnum].encode_type = ENCODE_SPECIFIED;
      memset(tagTable[rnum].encode_chars, 0, TOTAL_CHARS);
  } else {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, tagvar_buf, "EncodeMode", "XML");
  }      
   NSDL3_VARS(NULL, NULL, "EncodeMode option = %d", tagTable[rnum].encode_type );
  break;
      } else if (!strncmp(tagvar_buf, "CharsToEncode", strlen("CharsToEncode"))) {//encode support fields
   attr_end++;
   CLEAR_WHITE_SPACE(attr_end);
   attr_ptr = attr_end+1;

   int bs_flag = 0;
   i = 0;
   while(*attr_ptr){
     if(bs_flag){
			 if(*attr_ptr == '\"') {
				 tagvar_buf[i++] = '\"';
			 } else{
				 tagvar_buf[i++] = '\\';
				 tagvar_buf[i++] = *attr_ptr;  
			 }
       bs_flag = 0;
       attr_ptr++; 
       continue;
     }
     if(!bs_flag && *attr_ptr == '\"') {
       tagvar_buf[i++] = 0;
       break;
     }else if(*attr_ptr == '\\')
       bs_flag = 1;
     else
       tagvar_buf[i++] = *attr_ptr;
     attr_ptr++;
   }
 NSDL3_VARS(NULL, NULL, "attr = %s CharatoEncode = %s\n", attr_ptr ,tagvar_buf); 
 /* while(!isalpha(*attr_ptr))
   attr_ptr++;
  NSDL3_VARS(NULL, NULL, "attttrrr = %s .....\n", attr_ptr);
*/
  CLEAR_WHITE_SPACE(attr_ptr);

  int i;
  if (encode_flag_specified == 0){
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012137_ID, CAV_ERR_1012137_MSG, "XML"); 
  }   
  //strcpy(char_to_encode_buf, value);
  NSDL3_VARS(NULL, NULL, "After tokenized CharatoEncode = %s", tagvar_buf);
           
   
  /*Encode chars can have any special characters including space, single quiote, double quotes. Few examples:
 *     EncodeChars=", \[]"
 *     EncodeChars="~`!@#$%^&*-_+=[]{}\|;:'\" (),<>./?"
 */
          
  for (i = 0; tagvar_buf[i] != '\0'; i++) { 
    if(isalnum(tagvar_buf[i])){
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012138_ID, CAV_ERR_1012138_MSG, tagvar_buf, "XML");
    }

    NSDL3_VARS(NULL, NULL, "i = %d, tagvar_buf[i] = [%c]", i, tagvar_buf[i]);

    tagTable[rnum].encode_chars[(int)tagvar_buf[i]] = 1;
  }

  encode_chars_done = 1;
  break;
  }else if (!strncmp(tagvar_buf, "EncodeSpaceBy", strlen("EncodeSpaceBy"))) {//encode support fields
     NSDL2_VARS(NULL, NULL, "Value in tagvar_buf= [%s]", tagvar_buf);
     attr_end++;
     CLEAR_WHITE_SPACE(attr_end);
     if(*(attr_end) == '"')
        attr_ptr = attr_end+1;
     else 
        attr_ptr = attr_end;

    // CLEAR_WHITE_SPACE(attr_ptr);
     for (i = 0; (*attr_ptr != ',') && (*attr_ptr != ' ') && (*attr_ptr != ')') && (*attr_ptr != '\0'); attr_ptr++, i++) {
      tagvar_buf[i] = *attr_ptr;
  }

  if(tagvar_buf[i-1] == '"')
    tagvar_buf[i-1] = '\0';
  else
    tagvar_buf[i] = '\0';

  CLEAR_WHITE_SPACE(attr_ptr);

  NSDL2_VARS(NULL, NULL, "value= [%s]", tagvar_buf);

  if(strcmp(tagvar_buf, "+") && strcmp(tagvar_buf, "%20")){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012139_ID, CAV_ERR_1012139_MSG, "XML");
  }

  MY_MALLOC(tagTable[rnum].encode_space_by, strlen(tagvar_buf) , "tagTable[rnum].encode_space_by", -1);
  strcpy(tagTable[rnum].encode_space_by, tagvar_buf);
  NSDL2_VARS(NULL, NULL, "encodespaceby in tagtable= [%s]", tagTable[rnum].encode_space_by ); 

/*  if ((tagTable[rnum].encode_space_by = copy_into_big_buf(tagvar_buf, 0)) == -1) {
    fprintf(stderr, "Failed in copying EncodeSpaceBy into big buf\n");
    exit(-1);
  }
*/
  break;
      }else if (!strncasecmp(tagvar_buf, "ActionOnNotFound", strlen("ActionOnNotFound"))) {
	attr_ptr = attr_end+1;
        CLEAR_WHITE_SPACE(attr_ptr);
        for (i = 0; (*attr_ptr != ',') && (*attr_ptr != ' ') && (*attr_ptr != ')') && (*attr_ptr != '\0'); attr_ptr++, i++) {
            tagvar_buf[i] = *attr_ptr;
        }
        tagvar_buf[i] = '\0';
        CLEAR_WHITE_SPACE(attr_ptr); 

        if (!strcasecmp(tagvar_buf, "Error")) {
          tagTable[rnum].action_on_notfound = VAR_NOTFOUND_ERROR;
        } else if (!strcasecmp(tagvar_buf, "Warning")) {
          tagTable[rnum].action_on_notfound = VAR_NOTFOUND_WARNING;
        } else {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, tagvar_buf, "ActionOnNotFound", "XML");
        }
      } else if (!strncasecmp(tagvar_buf, "Convert", strlen("Convert"))) {
        attr_ptr = attr_end+1;
	CLEAR_WHITE_SPACE(attr_ptr);
        for (i = 0; (*attr_ptr != ',') && (*attr_ptr != ' ') && (*attr_ptr != ')') && (*attr_ptr != '\0'); attr_ptr++, i++) {
            tagvar_buf[i] = *attr_ptr;
        }
        tagvar_buf[i] = '\0';
        CLEAR_WHITE_SPACE(attr_ptr);
        NSDL3_VARS(NULL, NULL, "tagvar_buf = [%s]", tagvar_buf); 
        if (!strcasecmp(tagvar_buf, VAR_CONVERT_HTML_TO_URL_STR)) {
          tagTable[rnum].convert = VAR_CONVERT_HTML_TO_URL;
        } else if (!strcasecmp(tagvar_buf, VAR_CONVERT_HTML_TO_TEXT_STR)) {
          tagTable[rnum].convert = VAR_CONVERT_HTML_TO_TEXT;
        } else if (!strcasecmp(tagvar_buf, VAR_CONVERT_TEXT_TO_HTML_STR)) {
          tagTable[rnum].convert = VAR_CONVERT_TEXT_TO_HTML;
        } else if (!strcasecmp(tagvar_buf, VAR_CONVERT_TEXT_TO_URL_STR)) {
          tagTable[rnum].convert = VAR_CONVERT_TEXT_TO_URL;
        } else if (!strcasecmp(tagvar_buf, VAR_CONVERT_URL_TO_TEXT_STR)) {
          tagTable[rnum].convert = VAR_CONVERT_URL_TO_TEXT;
        } else if (!strcasecmp(tagvar_buf, VAR_CONVERT_TEXT_TO_BASE64_STR)) {
          tagTable[rnum].convert = VAR_CONVERT_TEXT_TO_BASE64;
        } else if (!strcasecmp(tagvar_buf, VAR_CONVERT_BASE64_TO_TEXT_STR)) {
          tagTable[rnum].convert = VAR_CONVERT_BASE64_TO_TEXT;
        } else {
           SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, tagvar_buf, "Convert", "XML");
        }
      } else if (!strncasecmp(tagvar_buf, "RedirectionDepth", strlen("RedirectionDepth"))) {
        
        if(!global_settings->g_follow_redirects) 
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012030_ID, CAV_ERR_1012030_MSG);
        }
         
        attr_ptr = attr_end+1;
	CLEAR_WHITE_SPACE(attr_ptr);
        for (i = 0; (*attr_ptr != ',') && (*attr_ptr != ' ') && (*attr_ptr != ')') && (*attr_ptr != '\0'); attr_ptr++, i++) {
          tagvar_buf[i] = *attr_ptr;
        }
        tagvar_buf[i] = '\0';
        CLEAR_WHITE_SPACE(attr_ptr);
 
        if (!strcasecmp(tagvar_buf, "Last")) {
          tagTable[rnum].tagvar_rdepth_bitmask = VAR_IGNORE_REDIRECTION_DEPTH;
        } else if(!strcasecmp(tagvar_buf, "ALL")) {
          tagTable[rnum].tagvar_rdepth_bitmask = VAR_ALL_REDIRECTION_DEPTH;
        } else {
          /*must reset this since default is set -1
          * we are going to set the bit on it.
          */
          tagTable[rnum].tagvar_rdepth_bitmask = 0; 
          if(!strlen(tagvar_buf))
          {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "RedirectionDepth", "XML");
          }
          int local_rdepth = 0;
          char * ptr1, *ptr2;
          ptr1 = ptr2 = NULL;
          ptr1 = tagvar_buf;
          while((ptr2 = strstr(ptr1, ";")) != NULL)
          {
            strncpy(tagvar_buf_local, ptr1, ptr2 - ptr1);
        
            tagvar_buf_local[ptr2 - ptr1] = 0; /* Null terminate the string */
            local_rdepth = atoi(tagvar_buf_local);  
            check_redirection_limit(local_rdepth);

            tagTable[rnum].tagvar_rdepth_bitmask |= (1 << (local_rdepth -1));
            ptr1 += strlen(tagvar_buf_local) + 1;
          }
        
          /* Here we parse the last of the chain. example 3 in chain 1;2;3. Or if no chain
           * is give then the first. */
          int i =0;
          while(1) 
          {
            if(isdigit(ptr1[i])) 
              i++;
            else 
              break;
          }
          if(i == 0)   /* This will return error also in case of spaces in between the chain */
          {
            SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "RedirectionDepth", "XML");
          } else {
            strncpy(tagvar_buf_local, ptr1, i); 
            local_rdepth = atoi(tagvar_buf_local);
            check_redirection_limit(local_rdepth);
          
            tagTable[rnum].tagvar_rdepth_bitmask |= (1 << (local_rdepth -1));
          }
       }
      } else if (!strncmp(tagvar_buf, "WHERE", strlen("WHERE"))) {
	attr_ptr = attr_end+1;
	CLEAR_WHITE_SPACE(attr_ptr);
	switch (*attr_ptr) {
	case '<':
	  attr_ptr++;
	  //Get Attribite Name
	  //It may simply be <>, implying Node value
	  //Qualifier node value is kept at tagTable while all attribute values are kept in attribute Table
	  for (i = 0; *attr_ptr != '>'; attr_ptr++, i++) {
	    if (*attr_ptr == ' ') {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012134_ID, CAV_ERR_1012134_MSG, "WHERE");
	    }
	    tagvar_buf[i] = *attr_ptr;
	  }
	  tagvar_buf[i] = '\0';

	  if (strlen(tagvar_buf)) {   /* is this is an attribute qualifier */
	    create_attrqual_table_entry(&attr_idx);
            NSDL2_VARS(NULL, NULL, "attr_idx = %d, attr_name = [%s]", attr_idx, tagvar_buf);
	    if ((attrQualTable[attr_idx].attr_name = copy_into_temp_buf(tagvar_buf, 0)) == -1) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, tagvar_buf);
	    }
	    attr_qual = 1;
	    if (tagTable[rnum].attr_qual_start == -1)
	      tagTable[rnum].attr_qual_start = attr_idx;
	    tagTable[rnum].num_attr_qual++;
	  } else {  /* is a value qualifier */
	    if (tagTable[rnum].value_qual != -1) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012140_ID, CAV_ERR_1012140_MSG);
	    }
	    attr_qual = 0;
	  }
	  CHECK_CHAR(attr_ptr, '>', msg);
	  CLEAR_WHITE_SPACE(attr_ptr);
	  //KQ: is another relationship supported? is it string cmp or numeric cmp
	  CHECK_CHAR(attr_ptr, '=', msg);
	  CLEAR_WHITE_SPACE(attr_ptr);
	  CHECK_CHAR(attr_ptr, '"', msg);

	  //Get the qualifier value
	  //It maybe attribute or node value
	  for (i = 0; attr_ptr; attr_ptr++, i++) {
	    if (*attr_ptr == '\\') {
	      attr_ptr++;
	      if (attr_ptr) {
		switch (*attr_ptr) {
		case 'n':
		  tagvar_buf[i] = '\n';
		  break;
		case '\\':
		  tagvar_buf[i] = '\\';
		  break;
		case '"':
		  tagvar_buf[i] = '"';
		  break;
		case 't':
		  tagvar_buf[i] = '\t';
		  break;
		case 'b':
		  tagvar_buf[i] = '\b';
		  break;
		case 'v':
		  tagvar_buf[i] = '\v';
		  break;
		case 'f':
		  tagvar_buf[i] = '\f';
		  break;
		case 'r':
		  tagvar_buf[i] = '\r';
		  break;
		default:
                  SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012141_ID, CAV_ERR_1012141_MSG, *attr_ptr);
		}
	      } else {
                SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012133_ID, CAV_ERR_1012133_MSG);
	      }
	    } else if (*attr_ptr == '"')
	      break;
	    else
	      tagvar_buf[i] = *attr_ptr;
	  }
	  tagvar_buf[i] = '\0';

	  if (attr_qual) {
	    if ((attrQualTable[attr_idx].qual_str = copy_into_temp_buf(tagvar_buf, 0)) == -1)
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, tagvar_buf);
	  } else {
	    if ((tagTable[rnum].value_qual = copy_into_temp_buf(tagvar_buf, 0)) == -1)
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, tagvar_buf);
	  }

	  CHECK_CHAR(attr_ptr, '"', msg);
	  CLEAR_WHITE_SPACE(line_ptr);
	  break;

	}
      }
    }

  }
  if (rnum != -1 ) {
    if (tagTable[rnum].type == TAG_TYPE_NONE) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "PAGE", "XML");
    }
    if ( tagTable[rnum].tag_name == -1 ) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "NODE", "XML");
    }
    if (tagTable[rnum].getvalue == -2) {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "VALUE", "XML");
    }
  }

  if((encode_chars_done == 0) && (encode_flag_specified == 1))
  {
    tagTable[rnum].encode_chars[' '] = 1;
    tagTable[rnum].encode_chars[39] = 1; //Setting for (') as it was givin error on compilation 
    tagTable[rnum].encode_chars[34] = 1; //Setting for (") as it was givin error on compilation 
    tagTable[rnum].encode_chars['<'] = 1;
    tagTable[rnum].encode_chars['>'] = 1;
    tagTable[rnum].encode_chars['#'] = 1;
    tagTable[rnum].encode_chars['%'] = 1;
    tagTable[rnum].encode_chars['{'] = 1;
    tagTable[rnum].encode_chars['}'] = 1;
    tagTable[rnum].encode_chars['|'] = 1;
    tagTable[rnum].encode_chars['\\'] = 1;
    tagTable[rnum].encode_chars['^'] = 1;
    tagTable[rnum].encode_chars['~'] = 1;
    tagTable[rnum].encode_chars['['] = 1;
    tagTable[rnum].encode_chars[']'] = 1;
    tagTable[rnum].encode_chars['`'] = 1;
  }
  return 0;
#undef NS_ST_TV_NAME
#undef NS_ST_TV_ATTR
}

//******************End Part (1)- Parsing*******************

//*****************Part (2) - Shared Memory related code************

static int total_tableid_entries;
static int max_tableid_entries;
static int total_globalTHI_entries;
static int max_globalTHI_entries;
static int total_node_entries;
static int max_node_entries;
static int total_nodevar_entries;
static int max_nodevar_entries;

static TableIdTableEntry *tableIdTable;
static THITableEntry* globalTHITable;
static NodeVarTableEntry* nodeVarTable;
static NodeTableEntry* nodeTable;

THITableEntry_Shr* thi_table_shr_mem;
NodeTableEntry_Shr* node_table_shr_mem;
NodeVarTableEntry_Shr* nodevar_table_shr_mem;

int default_tag_index;

static int global_node_cnt = 0;

static int create_temptagtable(TagPageTableEntry *, int*, int, int, int, int);
static int create_THI_table(int, TempTagTableEntry*, int, int);

static int create_node_attr_table(int, int, TempTagTableEntry*, int);

#if 0
static void display_gTHI_table(void);
static void display_node_table(void);
static void display_tpptr_table(int *, int);
static void  display_tagpage_table(TagPageTableEntry *, int);
static void   display_tagvar_table(TagTableEntry * ts, int num_tag_entries);
static void display_tempTag_table(TempTagTableEntry *tempTagTable, int total_temptag_entries);
#endif

#define NS_WEB_SPEC_ERROR_CODE 1
#define NS_WEB_SPEC_START_ST 2
#define NS_WEB_SPEC_FUNCTION_ST 3


static int create_tableid_table_entry(int* row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_tableid_entries == max_tableid_entries) {
    MY_REALLOC(tableIdTable, (max_tableid_entries + DELTA_TABLEID_ENTRIES) * sizeof(TableIdTableEntry), "tableIdTable", -1);
    max_tableid_entries += DELTA_TABLEID_ENTRIES;
  }
  *row_num = total_tableid_entries++;
  return (SUCCESS);
}


static int create_nodevar_table_entry(int* row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_nodevar_entries == max_nodevar_entries) {
    MY_REALLOC(nodeVarTable, (max_nodevar_entries + DELTA_NODEVAR_ENTRIES) * sizeof(NodeVarTableEntry), "nodevar entries", -1);
    max_nodevar_entries += DELTA_NODEVAR_ENTRIES;
  }
  *row_num = total_nodevar_entries++;
  return (SUCCESS);
}


static int create_THI_entries(int num_entries, int* row_num) {
  NSDL2_VARS(NULL, NULL, "Method called, num_entries = %d", num_entries);
  while ((total_globalTHI_entries + num_entries) >= max_globalTHI_entries) {
    MY_REALLOC(globalTHITable, (max_globalTHI_entries + DELTA_GLOBALTHI_ENTRIES) * sizeof(THITableEntry), "globalid entries", -1);
    max_globalTHI_entries += DELTA_GLOBALTHI_ENTRIES;
  }
  *row_num = total_globalTHI_entries;
  total_globalTHI_entries += num_entries;
  return (SUCCESS);
}


static int create_node_entries(int num_entries, int* row_num) {
  NSDL2_VARS(NULL, NULL, "Method called, num_entries = %d", num_entries);
  while ((total_node_entries + num_entries) >= max_node_entries) {
    MY_REALLOC(nodeTable, (max_node_entries + DELTA_NODE_ENTRIES) * sizeof(NodeTableEntry), "nodeTable", -1);
    max_node_entries += DELTA_NODE_ENTRIES;
  }
  *row_num = total_node_entries;
  total_node_entries += num_entries;
  return (SUCCESS);
}


static int find_tableid_table_entry(char* table_name, int length) {
  int i;
  char* table_ptr;

  NSDL2_VARS(NULL, NULL, "Method called, table_name = %s, length = %d, total_tableid_entries = %d", table_name, length, total_tableid_entries);
  for (i = 0; i < total_tableid_entries; i++) {
    table_ptr = RETRIEVE_TEMP_BUFFER_DATA(tableIdTable[i].name);
    if ((!strncmp(table_ptr, table_name, length)) && (strlen(table_ptr) == length))
      return i;
  }

  return -1;
}


int init_tagvar_tables(void) {
  //  total_temptag_entries = 1;  /* there is a default entry, soap:Fault */
  NSDL2_VARS(NULL, NULL, "Method called");
  total_tableid_entries = 0;
  total_nodevar_entries = 0;
  total_globalTHI_entries = 0;
  total_node_entries = 0;

  MY_MALLOC(tableIdTable, INIT_TABLEID_ENTRIES * sizeof(TableIdTableEntry), "tableIdTable", -1);
  MY_MALLOC(nodeVarTable, INIT_NODEVAR_ENTRIES * sizeof(NodeVarTableEntry), "nodeVarTable", -1);
  MY_MALLOC(globalTHITable, INIT_GLOBALTHI_ENTRIES * sizeof(THITableEntry), "globalTHITable", -1);
  MY_MALLOC(nodeTable, INIT_NODE_ENTRIES * sizeof(NodeTableEntry), "nodeTable", -1);
  
  if (tableIdTable && nodeVarTable && globalTHITable && nodeTable) {
    max_tableid_entries = INIT_TABLEID_ENTRIES;
    max_nodevar_entries = INIT_NODEVAR_ENTRIES;
    max_globalTHI_entries = INIT_GLOBALTHI_ENTRIES;
    max_node_entries = INIT_NODE_ENTRIES;
    return (SUCCESS);
  } else {
    fprintf(stderr, "error in initializing the tagvar tables\n");
    write_log_file(NS_SCENARIO_PARSING, "Failed to initialize tag var tables");
    return (FAILURE);
  }
}

int create_shr_tagtables(void) {
  int i;
  //int thi_table_fd, node_table_fd, nodevar_table_fd;

  NSDL2_VARS(NULL, NULL, "Method called, total_node_entries = %d", total_node_entries);
  if (total_node_entries) {
#if 0
    if ((node_table_fd = shmget(shm_base + total_num_shared_segs, sizeof(NodeTableEntry_Shr) * total_node_entries, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
      fprintf(stderr, "Error in allocating shared memory for the node Table\n");
      return -1;
    }

    total_num_shared_segs++;
                                                                                                                             
    if ((int)(node_table_shr_mem = shmat(node_table_fd, NULL, 0)) == -1) {
      fprintf(stderr, "Error in attaching memory for the node Table\n");
      return -1;
    }
#endif
    node_table_shr_mem = do_shmget(sizeof(NodeTableEntry_Shr) * total_node_entries, "node table");
  }

  if (total_nodevar_entries) {
#if 0
    if ((nodevar_table_fd = shmget(shm_base + total_num_shared_segs, sizeof(NodeVarTableEntry_Shr) * total_nodevar_entries, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
      fprintf(stderr, "error in allocating shared memory of the nodevar table\n");
      return -1;
    }

    total_num_shared_segs++;
    
    if ((int)(nodevar_table_shr_mem = (NodeVarTableEntry_Shr*) shmat(nodevar_table_fd, NULL, 0)) == -1) {
      fprintf(stderr, "error in attaching memory for the nodevar table\n");
      return -1;
    } else {
#endif
      nodevar_table_shr_mem = (NodeVarTableEntry_Shr*) do_shmget(sizeof(NodeVarTableEntry_Shr) * total_nodevar_entries, "nodevar table");
      for (i = 0; i < total_nodevar_entries; i++) {
	nodevar_table_shr_mem[i].vuser_vartable_idx = nodeVarTable[i].vuser_vartable_idx;
	nodevar_table_shr_mem[i].check_func = nodeVarTable[i].check_func;
        nodevar_table_shr_mem[i].ord = nodeVarTable[i].ord;
        /* Update the bitmask based on the page idx on that page. */
        //bhushan
        if(nodeVarTable[i].tagvar_rdepth_bitmask != VAR_IGNORE_REDIRECTION_DEPTH)
        { 
         if(nodeVarTable[i].tagvar_rdepth_bitmask == VAR_ALL_REDIRECTION_DEPTH)
           gPageTable[nodeVarTable[i].page_idx].redirection_depth_bitmask |= 0xFFFFFFFF; 
         else
           gPageTable[nodeVarTable[i].page_idx].redirection_depth_bitmask |= nodeVarTable[i].tagvar_rdepth_bitmask ; 
        }
        nodevar_table_shr_mem[i].action_on_notfound = nodeVarTable[i].action_on_notfound;
        nodevar_table_shr_mem[i].convert = nodeVarTable[i].convert;
        nodevar_table_shr_mem[i].tagvar_rdepth_bitmask = nodeVarTable[i].tagvar_rdepth_bitmask;

//copying fields related to special chars encoding in shared memory

        nodevar_table_shr_mem[i].encode_type = nodeVarTable[i].encode_type;
        memcpy(nodevar_table_shr_mem[i].encode_chars, nodeVarTable[i].encode_chars, TOTAL_CHARS);
        if(nodeVarTable[i].encode_space_by != NULL){
        MY_MALLOC(nodevar_table_shr_mem[i].encode_space_by, strlen(nodeVarTable[i].encode_space_by) , " nodevar_table_shr_mem[i].encode_space_by", -1);
        strcpy(nodevar_table_shr_mem[i].encode_space_by, nodeVarTable[i].encode_space_by);
        NSDL2_VARS(NULL, NULL, "encodespaceby in nodevar_table_shr_mem = %s", nodevar_table_shr_mem[i].encode_space_by);
        }else
          nodevar_table_shr_mem[i].encode_space_by = NULL;

        //bhushan allocate the memory for found_flag and initialize it
      
        MY_MALLOC(nodevar_table_shr_mem[i].found_flag, sizeof(char),
                  "nodevar_table_shr_mem[i].found_flag", i);
        *(nodevar_table_shr_mem[i].found_flag) = 0;

        /* Following fields are to handle ORD=ALL and ORD=ANY at runtime */
        if (nodevar_table_shr_mem[i].ord == ORD_ALL || 
            nodevar_table_shr_mem[i].ord == ORD_ANY ||
            nodevar_table_shr_mem[i].ord == ORD_LAST) {
          nodevar_table_shr_mem[i].tmp_array = NULL;
          MY_MALLOC(nodevar_table_shr_mem[i].tmp_array, sizeof(TempArrayNodeVar),
                    "nodevar_table_shr_mem[i].tmp_array", i);
          nodevar_table_shr_mem[i].tmp_array->count_ord = 0;
          nodevar_table_shr_mem[i].tmp_array->total_tmp_tagvar = 0;
          nodevar_table_shr_mem[i].tmp_array->max_tmp_tagvar = 0;
          nodevar_table_shr_mem[i].tmp_array->tempTagArrayVal = NULL;

        } else
          nodevar_table_shr_mem[i].tmp_array = NULL;
      }
    //}
  }
  
  if (total_globalTHI_entries) {
#if 0
    if ((thi_table_fd = shmget(shm_base + total_num_shared_segs, sizeof(THITableEntry_Shr) * total_globalTHI_entries, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
      fprintf(stderr, "error in allocating shared memory for the THI table\n");
      exit(-1);
    }
    
    total_num_shared_segs++;
    
    if ((thi_table_shr_mem = (THITableEntry_Shr*) shmat(thi_table_fd, NULL, 0))) {
#endif
      thi_table_shr_mem = (THITableEntry_Shr*) do_shmget(sizeof(THITableEntry_Shr) * total_globalTHI_entries, "THI Table");
      for (i = 0; i < total_globalTHI_entries; i++) {
	thi_table_shr_mem[i].node_ptr = (globalTHITable[i].node_index!=-1)?node_table_shr_mem+globalTHITable[i].node_index:NULL;
	thi_table_shr_mem[i].tag_hashfn = globalTHITable[i].tag_hashfn;
	//thi_table_shr_mem[i].attr_qual_hashfn = globalTHITable[i].attr_qual_hashfn;
	//thi_table_shr_mem[i].attr_table_size = globalTHITable[i].attr_table_size;
	thi_table_shr_mem[i].prev_ptr = (globalTHITable[i].prev_index!=-1)?((struct THITableEntry_Shr*) (thi_table_shr_mem + globalTHITable[i].prev_index)):NULL;
	thi_table_shr_mem[i].prevcode = globalTHITable[i].prevcode;
        thi_table_shr_mem[i].nodevar_idx_start = globalTHITable[i].nodevar_idx_start;
        NSDL3_VARS(NULL, NULL, "start idx for nodevars = %d\n", 
                   thi_table_shr_mem[i].nodevar_idx_start);
      }
#if 0
    } else {
      fprintf(stderr, "error in getting shared memory for the InuseSvr Table\n");
      exit(-1);
    }
#endif
  }

  for (i = 0; i < total_node_entries; i++) {
    if (!(nodeTable[i].nodevar_idx_start) && !(nodeTable[i].num_nodevars)) {
      node_table_shr_mem[i].child_ptr = NULL;
      node_table_shr_mem[i].nodevar_ptr = NULL;
      node_table_shr_mem[i].num_nodevars = 0;
      node_table_shr_mem[i].attr_qual_hashfn = NULL;
      node_table_shr_mem[i].attr_table_size = 0;
    } else {
      node_table_shr_mem[i].child_ptr = (nodeTable[i].THITable_child_idx!=-1)?(thi_table_shr_mem + nodeTable[i].THITable_child_idx):NULL;
      node_table_shr_mem[i].nodevar_ptr = (nodeTable[i].nodevar_idx_start!=-1)?(nodevar_table_shr_mem + nodeTable[i].nodevar_idx_start):NULL;
      node_table_shr_mem[i].num_nodevars = nodeTable[i].num_nodevars;
      node_table_shr_mem[i].attr_qual_hashfn = nodeTable[i].attr_qual_hashfn;
      node_table_shr_mem[i].attr_table_size = nodeTable[i].attr_table_size;
    }
  }

  FREE_AND_MAKE_NOT_NULL(nodeTable, "nodeTable", -1);
  FREE_AND_MAKE_NOT_NULL(globalTHITable, "globalid entries", -1);
  FREE_AND_MAKE_NOT_NULL(nodeVarTable, "nodevar entries", -1);
  FREE_AND_MAKE_NOT_NULL(tableIdTable, "tableIdTable", -1);
  return 0;
}


int insert_default_ws_errorcodes(void) {
  int errcode_idx;
  
  NSDL2_VARS(NULL, NULL, "Method called");
  if (create_errorcode_table_entry(&errcode_idx) == FAILURE) {
    fprintf(stderr, "insert_default_errorcodes: error in creating error code entry\n");
    return -1;
  }

  errorCodeTable[errcode_idx].error_code = 1;
  if ((errorCodeTable[errcode_idx].error_msg = copy_into_big_buf("Server Fault Error", 0)) == -1) {
    fprintf(stderr, "insert_default_errorcodes: error in copying string into big buf\n");
    return -1;
  }

  if (create_errorcode_table_entry(&errcode_idx) == FAILURE) {
    fprintf(stderr, "insert_default_errorcodes: error in creating error code entry\n");
    return -1;
  }

  errorCodeTable[errcode_idx].error_code = 2;
  if ((errorCodeTable[errcode_idx].error_msg = copy_into_big_buf("Client Fault Error", 0)) == -1) {
    fprintf(stderr, "insert_default_errorcodes: error in copying string into big buf\n");
    return -1;
  }
  
  if (create_errorcode_table_entry(&errcode_idx) == FAILURE) {
    fprintf(stderr, "insert_default_errorcodes: error in creating error code entry\n");
    return -1;
  }
  
  errorCodeTable[errcode_idx].error_code = 3;
  if ((errorCodeTable[errcode_idx].error_msg = copy_into_big_buf("Version Mismatch Fault Error", 0)) == -1) {
    fprintf(stderr, "insert_default_errorcodes: error in copying string into big buf\n");
    return -1;
  }

  if (create_errorcode_table_entry(&errcode_idx) == FAILURE) {
    fprintf(stderr, "insert_default_errorcodes: error in creating error code entry\n");
    return -1;
  }
  
  errorCodeTable[errcode_idx].error_code = 4;
  if ((errorCodeTable[errcode_idx].error_msg = copy_into_big_buf("Must Understand Fault Error", 0)) == -1) {
    fprintf(stderr, "insert_default_errorcodes: error in copying string into big buf\n");
    return -1;
  }

  if (create_errorcode_table_entry(&errcode_idx) == FAILURE) {
    fprintf(stderr, "insert_default_errorcodes: error in creating error code entry\n");
    return -1;
  }
  
  errorCodeTable[errcode_idx].error_code = 99;
  if ((errorCodeTable[errcode_idx].error_msg = copy_into_big_buf("Misc. Web Serv Error", 0)) == -1) {
    fprintf(stderr, "insert_default_errorcodes: error in copying string into big buf\n");
    return -1;
  }

  return 0;
}


#ifdef WS_MODE_OLD
int default_webspecfunc(void* tagattrtable) {
  int error_code = 0;

  NSDL2_VARS(NULL, NULL, "Method called");
  if (strcmp(((UserTagAttrEntry *)tagattrtable)[default_tag_index].value, "SOAP:SERVER") == 0)
    error_code = 1;
  else if (strcmp(((UserTagAttrEntry *)tagattrtable)[default_tag_index].value, "SOAP:CLIENT") == 0)
    error_code = 2;
  else if (strcmp(((UserTagAttrEntry *)tagattrtable)[default_tag_index].value, "SOAP:VERSION MISMATCH") == 0)
    error_code = 3;
  else if (strcmp(((UserTagAttrEntry *)tagattrtable)[default_tag_index].value, "SOAP:MUST UNDERSTAND") == 0)
    error_code = 4;

  return error_code;
}
#endif


//find_num_temptag would return the number of tag elemnets in all the unique node paths.
//eg., if there are 3 entries <A><B> , <A><B><C>, <A><B><C>, there are 2 unique paths wit
//total of 5 tag elements. it would return 5.
int find_num_temptag(int* tp_ptr, int tp_entries) {
  char* last_tag = NULL;
  int return_val = 0;
  char* attrtag_ptr;
  int i;

  NSDL2_VARS(NULL, NULL, "Method called, tp_entries = %d", tp_entries);
  for (i = 0; i < tp_entries; i++) {
    //count only uniq tag paths
    attrtag_ptr = RETRIEVE_TEMP_BUFFER_DATA(tagTable[tp_ptr[i]].tag_name);
    NSDL2_VARS(NULL, NULL, "attrtag_ptr = [%s]", attrtag_ptr);
    if (last_tag && !strcmp(attrtag_ptr, last_tag))
      continue;
    else
      last_tag = attrtag_ptr;

    //count the tag elemnents
    while ((attrtag_ptr = strchr(attrtag_ptr, '<'))) {
      return_val++;
      attrtag_ptr++;
    }
  }
  return return_val;
}

int tagpage_comp(const void* ent1, const void* ent2) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (((TagPageTableEntry*)ent1)->page_idx > ((TagPageTableEntry*)ent2)->page_idx)
    return 1;
  else if (((TagPageTableEntry*)ent1)->page_idx < ((TagPageTableEntry*)ent2)->page_idx)
    return -1;
  else 
    return 0;
}


int tag_sort(const void* ent1, const void* ent2) {
  NSDL2_VARS(NULL, NULL, "Method called");
  return strcmp(RETRIEVE_TEMP_BUFFER_DATA(tagTable[*((int*)ent1)].tag_name), 
		RETRIEVE_TEMP_BUFFER_DATA(tagTable[*((int*)ent2)].tag_name));
}


int create_tagtables() {
  int i;
  int cur_tagpage_idx = 0;
  int cur_tagpage_entries = 0;
  int num_all_tag_entries = 0;
  int* all_tag_entries;
  int num_combined_tagpage_entries = 0;
  int* combined_tagpage_entries;
  int page_idx;
  int all_tag_entries_idx;
  int start_tagpage_idx;
  int total_tp_idx;
  int* total_tp_ptr;
  int tag_entries;
  int total_temptag_entries;
  int tag_root;
  int sess_idx;
  TagPageTableEntry* tag_page_start;
  TagPageTableEntry* tag_page_ptr;
  TagTableEntry* tag_table_start;
  TagTableEntry* tag_table_ptr;
  PageTableEntry* page_start;
  PageTableEntry* page_ptr;
  
  NSDL2_VARS(NULL, NULL, "Method called");
  if (init_tagvar_tables() == FAILURE)
    return -1;

  //For each session
  NSDL4_VARS(NULL, NULL, "total_sess_entries = [%d]", total_sess_entries);
  for (sess_idx = 0; sess_idx < total_sess_entries; sess_idx++) {
    NSDL4_VARS(NULL, NULL, "sess_idx = [%d]", sess_idx);
    tag_page_start = &tagPageTable[gSessionTable[sess_idx].tagpage_start_idx];
    tag_table_start = &tagTable[gSessionTable[sess_idx].tag_start_idx];
    page_start = &gPageTable[gSessionTable[sess_idx].first_page];
    num_all_tag_entries = 0;
    
      //printf("XXXXXX num entries = %d; total_sess_entries = %d\n",  gSessionTable[sess_idx].num_tagpage_entries, total_sess_entries);
    //display_tagvar_table(tag_table_start, gSessionTable[sess_idx].num_tag_entries);
    //Fill the absolute page index in tagPageTable
    for (i = 0, tag_page_ptr = tag_page_start; i < gSessionTable[sess_idx].num_tagpage_entries; i++, tag_page_ptr++) {
      //printf("XXXXXX page name= %s\n", RETRIEVE_TEMP_BUFFER_DATA(tag_page_ptr->page_name));
      if ((page_idx = find_page_idx(RETRIEVE_TEMP_BUFFER_DATA(tag_page_ptr->page_name), sess_idx)) == -1) {
	NS_DUMP_WARNING("Unknown page '%s' is used in the declaration for xml var %s, variable declaration for this page ignored",
		RETRIEVE_TEMP_BUFFER_DATA(tag_page_ptr->page_name),
		RETRIEVE_TEMP_BUFFER_DATA(tagTable[tag_page_ptr->tagvar_idx].name));
      }
      tag_page_ptr->page_idx = page_idx;
    }
    
    //sort tagPageTable on pageindex in ascending order
    //Note that more than 1 var may be init on a page and there may be no var init on a page
    qsort(tag_page_start, gSessionTable[sess_idx].num_tagpage_entries, sizeof(TagPageTableEntry), tagpage_comp);
   
    //display_tagpage_table(tag_page_start, gSessionTable[sess_idx].num_tagpage_entries);
    //set num_all_tag_entries to the number of PAGE=ALL type of xml vars
    for (i = 0, tag_table_ptr = tag_table_start; i < gSessionTable[sess_idx].num_tag_entries; i++, tag_table_ptr++) {
      if (tag_table_ptr->type == TAG_TYPE_ALL)
	num_all_tag_entries++;
    }
    NSDL4_VARS(NULL, NULL, "num_all_tag_entries=%d", num_all_tag_entries); 
    //create all_tag_entries table to contain the table of tag vars that need to be associated with all pages of this session
    if (num_all_tag_entries)
    {
      MY_MALLOC(all_tag_entries, num_all_tag_entries * sizeof(int), "all_tag_entries", -1);
    }
    else
      all_tag_entries = NULL;
    
    all_tag_entries_idx = 0;

    //it has the the table of tag vars (relative to cur session) that need to be associated with all pages
    //KNQ: is having relative-to-session tag-var-index is the intent
    //Changing to contain abolute tag_var idx
    for (i =  0, tag_table_ptr = tag_table_start; i < gSessionTable[sess_idx].num_tag_entries; i++, tag_table_ptr++) {
      if (tag_table_ptr->type == TAG_TYPE_ALL) {
	//all_tag_entries[all_tag_entries_idx] = i;
	all_tag_entries[all_tag_entries_idx] = i + gSessionTable[sess_idx].tag_start_idx;
	all_tag_entries_idx++;
      }
    }
    
    cur_tagpage_idx = gSessionTable[sess_idx].tagpage_start_idx;
    cur_tagpage_entries = cur_tagpage_idx + gSessionTable[sess_idx].num_tagpage_entries;
    //Skip out the tage pages that have invalid page index (becaise of bad page names
    while (cur_tagpage_idx < cur_tagpage_entries) {
	if (tagPageTable[cur_tagpage_idx].page_idx == -1)
	    cur_tagpage_idx++;
	else 
	    break;
    }
    //For each page of the session -
    NSDL4_VARS(NULL, NULL, "num_pages=%d", gSessionTable[sess_idx].num_pages); 
    for (i = 0, page_ptr = page_start; i < gSessionTable[sess_idx].num_pages; i++, page_ptr++) {
      //int last_tag_var_idx, j;
      NSDL4_VARS(NULL, NULL, "cur_tagpage_idx=%d",cur_tagpage_idx);
      start_tagpage_idx = cur_tagpage_idx;
      
      //num_all_tag_entries is the number of xml_vars (tag_vars) associtaed with a page (with PAGE=ALL)
      //num_combined_tagpage_entries is the number of xml_vars (tag_vars) associtaed with a page
      //by way of ALL PAge or specific page associations.
      //set num_combined_tagpage_entries to number of xml_vars associated with this page because of PAGE=ALL
      num_combined_tagpage_entries = num_all_tag_entries;

      //increment  num_combined_tagpage_entries to number of xml_vars associated with this specific page
      //Count the page specific var-index  (may not be uniq)
      // The tagPageTable is being scanned, for the part associated wuth this see only, starting from current page
      //tahePageTbale is sorted in page order. It will match curent page on the top.
      while (cur_tagpage_idx < cur_tagpage_entries) {
	//printf("Check tp ptr entries : cur_page_idx=%d, tagpage_idx=%d on page =%d\n", cur_tagpage_idx, tagPageTable[cur_tagpage_idx].page_idx, (page_ptr - gPageTable));
	if (tagPageTable[cur_tagpage_idx].page_idx == (page_ptr - gPageTable))
	  num_combined_tagpage_entries++;
	else
	  break;
	cur_tagpage_idx++;
      }

      //create total_tp_ptr table to contain the tag vars that need to be associated with all pages+Specific pages
      // for current page in the loop
      //tag entries to contain the number of entries in total_tp_ptr
      //printf("Tag Page entries: combined=%d all_page=%d\n", num_combined_tagpage_entries, num_all_tag_entries);
      if (num_combined_tagpage_entries > num_all_tag_entries) { //If there are page specific tag vars
	MY_MALLOC(combined_tagpage_entries, num_combined_tagpage_entries * sizeof(int), "combined_tagpage_entries", -1);
	if (all_tag_entries) //If there have been some PAGE=ALL entries already
	  memcpy(combined_tagpage_entries, all_tag_entries, num_all_tag_entries * sizeof(int));
	cur_tagpage_idx = start_tagpage_idx;
	total_tp_idx = num_all_tag_entries;
	while (cur_tagpage_idx < cur_tagpage_entries) {
	  if (tagPageTable[cur_tagpage_idx].page_idx == (page_ptr - gPageTable)) {
	    combined_tagpage_entries[total_tp_idx] = tagPageTable[cur_tagpage_idx].tagvar_idx;
	    total_tp_idx++;
	    cur_tagpage_idx++;
	  }
	  else
	    break;
	}
	total_tp_ptr = combined_tagpage_entries;
	tag_entries = num_combined_tagpage_entries;
	assert(tag_entries);
      } else {
	total_tp_ptr = all_tag_entries;
	tag_entries = num_all_tag_entries;
      }
      
      if (tag_entries) {
	//sort the total_tp_ptr in the order of tagnames associated with the current page in the loop
	qsort(total_tp_ptr, tag_entries, sizeof(int), tag_sort);
	
	//find_num_temptag would return the number of tag elemnets in all the unique node paths.
	//eg., if there are 3 entries <A><B> , <A><B><C>, <A><B><C>, there are 2 unique paths wit
	//total of 5 tag elements. it would return 5.
	total_temptag_entries = find_num_temptag(total_tp_ptr, tag_entries);

	if ((tag_root = create_temptagtable(tag_page_start, total_tp_ptr, tag_entries, total_temptag_entries, i+gSessionTable[sess_idx].first_page, sess_idx)) == -1)
        {
          write_log_file(NS_SCENARIO_PARSING, "tag var %s is in incorrect format", tag_page_start);
	  return -1; // can be used ain in create+temptable
        }
	else
	  page_ptr->tag_root_idx = tag_root;
	
        page_ptr->num_tag_entries = tag_entries;

	if (tag_entries > num_all_tag_entries) {
	  FREE_AND_MAKE_NOT_NULL(combined_tagpage_entries, "combined_tagpage_entries", -1);
	}
      } else
	page_ptr->tag_root_idx = -1;
      
      //KQ: What's the intent. Some point of time this mem would anyway be delloc - can be used ain in create+temptable
      total_tableid_entries = 0;  /* basicaly reset the TableIdTable */
    }
    
    if (all_tag_entries) {
      FREE_AND_MAKE_NOT_NULL(all_tag_entries, "all_tag_entries", -1);
    }
  }
  
  if (create_shr_tagtables() == -1)
    return -1;

  return 0;
}


//Find an entry, if not there create one
static int 
get_table_id(char* table_name, int length) 
{
  int rnum;

  NSDL2_VARS(NULL, NULL, "Method called, table_name = %s, length = %d", table_name, length);
  if ((rnum = find_tableid_table_entry(table_name, length)) == -1) {
    if (create_tableid_table_entry(&rnum) != SUCCESS) {
      NS_EXIT(-1, "create_tableid_table_entry() failed.");
    }

    if ((tableIdTable[rnum].name = copy_into_temp_buf(table_name, length)) == -1)
      NS_EXIT(-1, "create_temp_buf_space(): Error allocating more memory for g_temp_buf");
  }

  return rnum;
}

static chkfn_type create_check_func(hashfn_type hash_func, TagTableEntry* tagtable_ptr, int sess_idx) {
  FILE* check_fptr;
  AttrQualTableEntry* attrqual_ptr;
  int i;
  char cmd_buffer[MAX_LINE_LENGTH];
  char file_buffer[MAX_FILE_NAME];
  void* handle;
  char* error;
  chkfn_type return_func;
  NSDL2_VARS(NULL, NULL, "Method called, sess_idx = %d", sess_idx);
  int vuser_var_idx = gSessionTable[sess_idx].vars_trans_table_shr_mem[gSessionTable[sess_idx].var_hash_func(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name), strlen(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name)))].user_var_table_idx;
  //  int vuser_var_idx;
  int attr_value_idx;
  int attr_idx;

  assert(vuser_var_idx != -1);

  //vuser_var_idx = vars_trans_table_shr_mem[var_trans_idx].user_var_table_idx;
    
  sprintf(g_tmp_fname, "%s/check_%d:%s.c", g_ns_tmpdir, sess_idx, RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
  if ((check_fptr = fopen(g_tmp_fname, "w+")) == NULL) {
    fprintf(stderr, "error in creating check.c file\n");
    perror("fopen");
    return NULL;
  }
  
  fprintf(check_fptr, "#include \"user_tables.h\"\n#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n#include<ns_string.h>\n\n");

  if (tagtable_ptr->order == ORD_ALL) 
    fprintf(check_fptr, "extern int create_tag_tmp_table_entry(int *row_num, "
            "NodeVarTableEntry_Shr *nodevar_ptr);\n");

  fprintf(check_fptr, "int check_%s(char** attrTable, UserVarEntry* vartable, char* value, int* order, NodeVarTableEntry_Shr *nodevar_ptr, int pass) {\n", 
	  RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));

  if (tagtable_ptr->order != ORD_ALL)
    fprintf(check_fptr, "\tUserVarEntry* vartable_ptr = vartable + %d;\n", vuser_var_idx);
  //  fprintf(check_fptr, "\tprintf(\"the index is %%d\\n\", %d);\n", vuser_var_idx);

  if (tagtable_ptr->num_attr_qual) {
    attrqual_ptr = &attrQualTable[tagtable_ptr->attr_qual_start];
    for (i = tagtable_ptr->attr_qual_start; i < (tagtable_ptr->attr_qual_start + tagtable_ptr->num_attr_qual); i++, attrqual_ptr++) {
      attr_idx = hash_func(RETRIEVE_TEMP_BUFFER_DATA(attrqual_ptr->attr_name), strlen(RETRIEVE_TEMP_BUFFER_DATA(attrqual_ptr->attr_name)));
      fprintf(check_fptr, "\tif ((!attrTable[%d]) || strcmp(attrTable[%d], \"%s\"))\n\t\treturn -1;\n", attr_idx, attr_idx,
	      RETRIEVE_TEMP_BUFFER_DATA(attrqual_ptr->qual_str));
    }
  }
  
  if (tagtable_ptr->value_qual != -1) {
    fprintf(check_fptr, "\tif ((!value) || strcmp(value, \"%s\"))\n\t\treturn -1;\n", RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->value_qual));
  }

  fprintf(check_fptr, "\torder[%d]++;\n", vuser_var_idx);

  if ((tagtable_ptr->order != ORD_LAST) && (tagtable_ptr->order != ORD_ANY) && (tagtable_ptr->order != ORD_ALL)) {
    //fprintf(check_fptr, "\tif (*order != %d)\n\t\treturn -1;\n", tagtable_ptr->order);
    fprintf(check_fptr, "\tif (order[%d] != %d)\n\t\treturn -1;\n", vuser_var_idx, tagtable_ptr->order);
  } else if (tagtable_ptr->order == ORD_ALL) { /* ORD_ALL */
    /* fall through */
  } else if ((tagtable_ptr->order == ORD_ANY) || (tagtable_ptr->order == ORD_LAST)){ /* ORD_ANY */
    /* fall through */
  } else {
    fprintf(check_fptr, "\tif (order[%d] != 1)\n\t\treturn -1;\n", vuser_var_idx);
  }

  if ((tagtable_ptr->order == ORD_ANY) || (tagtable_ptr->order == ORD_LAST)) {
    if (tagtable_ptr->getvalue == -1) 
    { 
      fprintf(check_fptr, "\tif (value) {\n");
      fprintf(check_fptr, "\t\tif (pass == 1) {\n");
      fprintf(check_fptr, "\t\t\tnodevar_ptr->tmp_array->count_ord++;\n");
      fprintf(check_fptr, "\t\treturn -1;\n"); //bhushan

      fprintf(check_fptr, "\t\t}\n");
      fprintf(check_fptr, "\t\tif (pass == 2) {\n");
      fprintf(check_fptr, "\t\t\tif (nodevar_ptr->tmp_array->count_ord == order[%d]) {\n", vuser_var_idx);
      fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_URL) {
        //here we are using old conversion i.e. html to url is equivalent to text to url
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_HTML) {
        //here we are using old conversion i.e. url to html is equivalent to url to text
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_html(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_HTML) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_html(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_URL) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_base64(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_base64(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(value);\n");
      } else {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = "
               "malloc(vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\t\t\tmemcpy(vartable_ptr->value.value, "
              "value, vartable_ptr->length);\n");
      }
      fprintf(check_fptr, "\t\t\t\tnodevar_ptr->tmp_array->count_ord = 0;\n");
      fprintf(check_fptr, "\t\t\t}\n");
      fprintf(check_fptr, "\t\telse return -1;\n"); //bhushan
      fprintf(check_fptr, "\t\t}\n");
      fprintf(check_fptr, "\t}\n");

    } else {
      attr_value_idx = hash_func(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->getvalue), strlen(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->getvalue)));
      fprintf(check_fptr, "\tif (attrTable[%d]) {\n", attr_value_idx);
      fprintf(check_fptr, "\t\tif (pass == 1) {\n");
      fprintf(check_fptr, "\t\t\tnodevar_ptr->tmp_array->count_ord++;\n");
      fprintf(check_fptr, "\t\t\treturn -1;\n"); //bhushan
      fprintf(check_fptr, "\t\t}\n");
      fprintf(check_fptr, "\t\tif (pass == 2) {\n");
      fprintf(check_fptr, "\t\t\tif (nodevar_ptr->tmp_array->count_ord == order[%d]) {\n", vuser_var_idx);

      fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(attrTable[%d]);\n", attr_value_idx);   
      if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_URL) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_encode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_HTML) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_decode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_HTML) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_encode_html(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_TEXT) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_decode_html(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_URL) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_encode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_TEXT) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_decode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_encode_base64(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT) {
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = ns_decode_base64(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      } else {  
        fprintf(check_fptr, "\t\t\t\tvartable_ptr->value.value = malloc(vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\t\t\tmemcpy(vartable_ptr->value.value, attrTable[%d], vartable_ptr->length);\n", attr_value_idx);      
      }
      fprintf(check_fptr, "\t\t\t\tnodevar_ptr->tmp_array->count_ord = 0;\n");
      fprintf(check_fptr, "\t\t\t}\n");
      fprintf(check_fptr, "\t\telse return -1;\n"); //bhushan
      fprintf(check_fptr, "\t\t}\n");

      fprintf(check_fptr, "\t}\n");
    }
 
  } else if (tagtable_ptr->order == ORD_ALL) {
    if (tagtable_ptr->getvalue == -1) 
    {
      fprintf(check_fptr, "\tif (value) {\n");
      fprintf(check_fptr, "\t\tint rnum;\n");
      fprintf(check_fptr, "\t\tcreate_tag_tmp_table_entry(&rnum, nodevar_ptr);\n");
      //fprintf(check_fptr, "\t\tnodevar_ptr->tempTagArrayVal[rnum].value = value;\n");
      //fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = strdup(value);\n");
      fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(value);\n");
      if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_URL) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_url(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_HTML) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_url(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_URL) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_url(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_TEXT) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_url(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_HTML) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_html(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_TEXT) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_html(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_base64(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_base64(value, nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n");
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      } else {    
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = value;\n");
      }
      fprintf(check_fptr, "\t}\n");
      fprintf(check_fptr, "\telse return -1;\n"); //bhushan
    } else {
      fprintf(check_fptr, "\t\tint rnum;\n");
      fprintf(check_fptr, "\t\tcreate_tag_tmp_table_entry(&rnum, nodevar_ptr);\n");
      attr_value_idx = hash_func(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->getvalue), strlen(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->getvalue)));
      fprintf(check_fptr, "\tif (attrTable[%d]) {\n", attr_value_idx);
      fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(attrTable[%d]);\n", attr_value_idx);
      if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_URL) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_url(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_HTML) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_url(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_URL) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_url(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_TEXT) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_url(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_HTML) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_html(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_TEXT) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_html(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_encode_base64(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT) {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = ns_decode_base64(attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].length = strlen(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value);\n");
      } else {
        fprintf(check_fptr, "\t\tnodevar_ptr->tmp_array->tempTagArrayVal[rnum].value = "
                "malloc(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n");
        fprintf(check_fptr, "\t\tmemcpy(nodevar_ptr->tmp_array->tempTagArrayVal[rnum].value, "
                "attrTable[%d], nodevar_ptr->tmp_array->tempTagArrayVal[rnum].length);\n",
                attr_value_idx);
      }
      fprintf(check_fptr, "\t}\n");
    }

 
  } else {
    fprintf(check_fptr, "\tif (vartable_ptr->value.value) free(vartable_ptr->value.value);\n");

    if (tagtable_ptr->getvalue == -1) {
      fprintf(check_fptr, "\tif (value) {\n");
      fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(value);\n");
      if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_URL) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_HTML) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_HTML) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_html(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_URL) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_url(value, vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_html(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_base64(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_base64(value, vartable_ptr->length, NULL);\n");
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      } else {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = malloc(vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\tmemcpy(vartable_ptr->value.value, value, vartable_ptr->length);\n");
      }
      //fprintf(check_fptr, "\t\tprintf(\"Init [%s] to [%%s]\\n\", value);\n",RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
      fprintf(check_fptr, "\t}\n");
    } else {
      attr_value_idx = hash_func(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->getvalue), strlen(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->getvalue)));
      fprintf(check_fptr, "\tif (attrTable[%d]) {\n", attr_value_idx);
      fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(attrTable[%d]);\n", attr_value_idx);
      if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_URL) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_HTML) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_HTML) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_html(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_HTML_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_html(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_URL) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_URL_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_url(attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_TEXT_TO_BASE64) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_encode_base64(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      }else if(tagtable_ptr->convert == VAR_CONVERT_BASE64_TO_TEXT) {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = ns_decode_base64(attrTable[%d], vartable_ptr->length, NULL);\n", attr_value_idx);
        fprintf(check_fptr, "\t\tvartable_ptr->length = strlen(vartable_ptr->value.value);\n");
      } else {
        fprintf(check_fptr, "\t\tvartable_ptr->value.value = malloc(vartable_ptr->length);\n");
        fprintf(check_fptr, "\t\tmemcpy(vartable_ptr->value.value, attrTable[%d], vartable_ptr->length);\n", attr_value_idx);
      }
      //fprintf(check_fptr, "\t\tprintf(\"Init [%s] to [%%s]\\n\", attrTable[%d]);\n",RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name), attr_value_idx);
      fprintf(check_fptr, "\t}\n");
    }
  }
  fprintf(check_fptr, "\treturn 0;\n}\n");

  fclose(check_fptr);

  sprintf(file_buffer, "%s/check_%d:%s.so", g_ns_tmpdir, sess_idx, RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
  //sprintf(file_buffer, "tmp/check_%s.so", RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
  sprintf(cmd_buffer, "gcc -ggdb -m%d -fpic -shared -Wall -I include -o %s %s/check_%d:%s.c",
                       NS_BUILD_BITS, file_buffer, g_ns_tmpdir, 
                       sess_idx, RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
  //sprintf(cmd_buffer, "gcc -fpic -shared -Wall -I include -o %s tmp/check.c", file_buffer);

  system(cmd_buffer);

  if (!getcwd(file_buffer, MAX_FILE_NAME)) {
    fprintf(stderr, "error in getting pwd\n");
    return NULL;
  }

  sprintf(file_buffer, "%s/%s/check_%d:%s.so", file_buffer, g_ns_tmpdir, sess_idx, RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
  //printf("AKG: so file is %s\n", file_buffer);
  //sprintf(file_buffer, "%s/tmp/check_%s.so", file_buffer, RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
  handle = dlopen (file_buffer, RTLD_LAZY); 
  
  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    printf ("%s\n", error);
    return NULL;
  }
  
  sprintf(cmd_buffer, "check_%s", RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name));
  return_func = dlsym(handle, cmd_buffer);
  
  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    fprintf (stderr, "%s\n", error);
    return NULL;
  }

  return return_func;
}

static hashfn_type 
create_hashfn(int table_id, int* num_hash_entries, int type, int page_num) 
{
  hashfn_type return_func;
  char hash_type[8];
  char so_file[MAX_FILE_NAME];
  char var_name[MAX_FILE_NAME];
  char hash_name[MAX_FILE_NAME];
  char cmd_buffer[MAX_FILE_NAME];

  NSDL2_VARS(NULL, NULL, "Method called, table_id = %d, type = %d, page_num = %d", table_id, type, page_num);
  if (type == 0) 
    sprintf(hash_type, "%s", "tag");
  else if (type == 1)
    sprintf(hash_type, "%s", "attr");
  else {
    NS_EXIT(-1, "create_hashfn(): unknown type %d\n", type);
  }

  /* create unique entries in the tag.txt file */
  sprintf(cmd_buffer, "sort %s/%s.txt -u > %s/%s_u.txt", g_ns_tmpdir, hash_type, g_ns_tmpdir, hash_type);

  //sprintf(cmd_buffer, "sort tmp/%s.txt -u > tmp/%s_u.txt", hash_type, hash_type);
  if (system(cmd_buffer) == -1) {
    fprintf(stderr, "create_hashfn(): Error in calling sort\n");
    return NULL;
  }

  sprintf(hash_name, "%s_hash", hash_type);
  sprintf(so_file, "%s_write.%s.%d.so", hash_type,(table_id==-1)?"root":RETRIEVE_TEMP_BUFFER_DATA(tableIdTable[table_id].name),page_num);
  sprintf(var_name, "%s_u.txt", hash_type);

  *num_hash_entries = generate_hash_table_ex(var_name, hash_name, &return_func, NULL, NULL, NULL, so_file, 0, g_ns_tmpdir);
  
  if(*num_hash_entries < 0)
    return NULL;

  return return_func;
}


static int 
create_temptagtable(TagPageTableEntry * tag_page_start, int* tp_ptr, int tp_entries, int total_temptag_entries, int page_num, int sess_idx) 
{
  int i;
  int temp_index = 0;
  char* tag_start_ptr; //moving ptr for scanning a tag name, points to start of a tag element
  char* tag_name_ptr; //once init, does not move, always points to begining of a tag path
  char* tag_end_ptr; ///moving ptr for scanning a tag name, points to end of a tag element
  int tmp_tp_idx;
  int tmp_tag_idx;
  FILE* attr_fptr;
  int have_attr;
  int qual_idx;
  int qual_attr_num;
  int nodevar_idx = -1;
  int nodevar_idx_start = -1;
  TempTagTableEntry tempTagTable[total_temptag_entries];

  TagPageTableEntry* tag_page_ptr;

  NSDL2_VARS(NULL, NULL, "Method called, tp_entries = %d, total_temptag_entries = %d, page_num = %d, sess_idx = %d", tp_entries, total_temptag_entries, page_num, sess_idx);
  //display_tpptr_table(tp_ptr, tp_entries);
  for (i = 0; i < total_temptag_entries; i++) {
    tempTagTable[i].nodevar_idx_start = -1;
    tempTagTable[i].num_nodevars = 0;
    tempTagTable[i].attr_qual_hashfn = NULL;
    tempTagTable[i].num_attr_table_entries = 0;
  }

  //for (i = 0; i < tp_entries; i++)
  for (i = 0; i < tp_entries; ) 
  {
    tag_name_ptr = RETRIEVE_TEMP_BUFFER_DATA(tagTable[tp_ptr[i]].tag_name);
    NSDL4_VARS(NULL, NULL, "tag_name_ptr = [%s]", tag_name_ptr);
    if (*(tag_name_ptr) != '<') {
      fprintf(stderr, "tag %s is incorrect format\n", tag_name_ptr);
      return -1;
    }
    tag_start_ptr = tag_name_ptr;
    
    while (1) {
      //KNQ: can it be null
      if (tag_start_ptr == NULL) {
	fprintf(stderr, "tag %s is incorrect format\n", tag_name_ptr);
	return -1;
      }

      if (tag_name_ptr == tag_start_ptr)
	tempTagTable[temp_index].table_id = -1;
      else {
	if ((tag_start_ptr - tag_name_ptr) > 0)
	  tempTagTable[temp_index].table_id = get_table_id(tag_name_ptr, tag_start_ptr - tag_name_ptr);
	else {
	  //KNQ: can (tag_start_ptr - tag_name_ptr) < 0 ?
	  fprintf(stderr, "tag %s is incorrect format\n", tag_name_ptr);
	  return -1;
	}
      }

      tag_end_ptr = strchr(tag_start_ptr, '>');

      if (tag_end_ptr == NULL) {
	fprintf(stderr, "tag %s is incorrect format\n", tag_name_ptr);
	return -1;
      }

      if ((tag_end_ptr-tag_start_ptr-1) > 0) {
	if ((tempTagTable[temp_index].tag_name_index = copy_into_temp_buf(tag_start_ptr+1, tag_end_ptr-tag_start_ptr-1)) == -1) {
	  fprintf(stderr, "tag %s could not be copied in temp buf\n", tag_name_ptr);
	  return -1;
	}
      } else {
	fprintf(stderr, "tag %s is incorrect format\n", tag_name_ptr);
	return -1;
      }

      tag_start_ptr = tag_end_ptr+1;

      if ((*tag_start_ptr) == '\0') {  /* this is for the final tag slot */
	sprintf(g_tmp_fname, "%s/attr.txt", g_ns_tmpdir);
        NSDL4_VARS(NULL, NULL, "open file [%s]", g_tmp_fname);
	if ((attr_fptr = fopen(g_tmp_fname, "w+")) == NULL) {
	  fprintf(stderr, "unable to create file %s/attr.txt\n", g_ns_tmpdir);
	  perror("fopen");
	  return -1;
	}
	
	have_attr = 0;

	tmp_tp_idx = i;
	
	/* first get all the attr qualifiers, and the attr value (if applicable) */
	//KQ: tp_ptr table is already uniq'ed. is the loop needed . ? How multiple tag vars with same same node are handled?
	while (tmp_tp_idx < tp_entries) {
	  tmp_tag_idx = tp_ptr[tmp_tp_idx];
	  if (!strcmp(RETRIEVE_TEMP_BUFFER_DATA(tagTable[tmp_tag_idx].tag_name), tag_name_ptr)) {
	    if (tagTable[tmp_tag_idx].getvalue != -1) {
              NSDL4_VARS(NULL, NULL, "tmp_tag_idx = %d, getvalue = [%s]", tmp_tag_idx, RETRIEVE_TEMP_BUFFER_DATA(tagTable[tmp_tag_idx].getvalue));
	      fprintf(attr_fptr, "%s\n", RETRIEVE_TEMP_BUFFER_DATA(tagTable[tmp_tag_idx].getvalue));
	      have_attr = 1;
	      //KNQ: Why have_attr is not 1 if getvalue != -1, (action:add it for this case too)
	    }
	    if (tagTable[tmp_tag_idx].num_attr_qual) {
	      for (qual_idx = tagTable[tmp_tag_idx].attr_qual_start, qual_attr_num = 0; qual_attr_num < tagTable[tmp_tag_idx].num_attr_qual; qual_attr_num++, qual_idx++) {
              NSDL4_VARS(NULL, NULL, "qual_idx = %d, attibute = [%s]", qual_idx, RETRIEVE_TEMP_BUFFER_DATA(attrQualTable[qual_idx].attr_name));
		fprintf(attr_fptr, "%s\n", RETRIEVE_TEMP_BUFFER_DATA(attrQualTable[qual_idx].attr_name));
	      }
	      have_attr = 1;
	    }
	    //break;
	    //KQ: is this brak needed?
	    // KQ: since tp_ptr is sorted by tag name, this loop shpuld exit at first mis-match of tagvar
	  } else
		break;
	  tmp_tp_idx++;
	}
	fclose(attr_fptr);
	
	/* now create the attr hashfunction */
	if (have_attr) {
	  if ((tempTagTable[temp_index].attr_qual_hashfn = create_hashfn(tempTagTable[temp_index].table_id, &tempTagTable[temp_index].num_attr_table_entries, 1, page_num)) == NULL) {
	    fprintf(stderr, "create_temptag_table()(:could  not create hash\n");
	    return -1;
	  } else {
	    tempTagTable[temp_index].num_attr_table_entries++;  /* since we got the max hash code from create_hashfn */
	  }
	} /*else { 
	  tempTagTable[temp_index].attr_qual_hashfn = NULL;
	  tempTagTable[temp_index].num_attr_table_entries = 0;
	}* Already init to null*/

	tmp_tp_idx = i;
	/* now create the check function that first check the qualifiers and then copy the value into the appropiate place */
	while (tmp_tp_idx < tp_entries) {
	  tmp_tag_idx = tp_ptr[tmp_tp_idx];
	  if (!strcmp(RETRIEVE_TEMP_BUFFER_DATA(tagTable[tmp_tag_idx].tag_name), tag_name_ptr)) {
	    if (create_nodevar_table_entry(&nodevar_idx) != SUCCESS) {
	      fprintf(stderr, "create_temptag_table()(:could  not create nodevar table\n");
	      return -1;
	    }


	    //node var table keep the the list of NS vars that are initilized
	    if (tempTagTable[temp_index].nodevar_idx_start == -1) {
	      tempTagTable[temp_index].nodevar_idx_start = nodevar_idx;
              nodevar_idx_start = nodevar_idx;
            }
	    tempTagTable[temp_index].num_nodevars++;

            nodeVarTable[nodevar_idx].vuser_vartable_idx = gSessionTable[sess_idx].vars_trans_table_shr_mem[gSessionTable[sess_idx].var_hash_func(RETRIEVE_TEMP_BUFFER_DATA(tagTable[tmp_tag_idx].name), strlen(RETRIEVE_TEMP_BUFFER_DATA(tagTable[tmp_tag_idx].name)))].user_var_table_idx; //KQ:perfect hash of NS var or the min uniq hash code of ns var? - Its overall hash
            //            nodeVarTable[nodevar_idx].vuser_vartable_idx = gSessionTable[sess_idx].vars_trans_table_shr_mem[gSessionTable[sess_idx].var_hash_func(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name), strlen(RETRIEVE_TEMP_BUFFER_DATA(tagtable_ptr->name)))].user_var_table_idx;

	    if ((nodeVarTable[nodevar_idx].check_func = create_check_func(tempTagTable[temp_index].attr_qual_hashfn, &tagTable[tmp_tag_idx], sess_idx)) == NULL)
	      return -1;
            
            /* set order (ORD) */
            nodeVarTable[nodevar_idx].ord = tagTable[tmp_tag_idx].order;
            //bhushan
            /* Set page index */
           for (i = 0, tag_page_ptr = tag_page_start; i < gSessionTable[sess_idx].num_tagpage_entries; i++, tag_page_ptr++) {
             if (( nodeVarTable[nodevar_idx].page_idx = find_page_idx(RETRIEVE_TEMP_BUFFER_DATA(tagPageTable[tag_page_ptr->tagvar_idx].page_name), sess_idx)) == -1) {
               NS_DUMP_WARNING("unknown page '%s' is used in the declaration for xml var %s, " 
                               "variable declaration for this page ignored", RETRIEVE_TEMP_BUFFER_DATA(tagPageTable[tmp_tag_idx].page_name),
                               RETRIEVE_TEMP_BUFFER_DATA(tagTable[tagPageTable[tmp_tag_idx].tagvar_idx].name));
             }
           }
            nodeVarTable[nodevar_idx].convert = tagTable[tmp_tag_idx].convert;
            nodeVarTable[nodevar_idx].action_on_notfound = tagTable[tmp_tag_idx].action_on_notfound;
            nodeVarTable[nodevar_idx].tagvar_rdepth_bitmask = tagTable[tmp_tag_idx].tagvar_rdepth_bitmask;

//copying encode chars field
            nodeVarTable[nodevar_idx].encode_type = tagTable[tmp_tag_idx].encode_type;
          if(tagTable[tmp_tag_idx].encode_space_by != NULL){ 
            MY_MALLOC(nodeVarTable[nodevar_idx].encode_space_by, strlen(tagTable[tmp_tag_idx].encode_space_by), "nodeVarTable[nodevar_idx].encode_space_by", -1);
            strcpy(nodeVarTable[nodevar_idx].encode_space_by, tagTable[tmp_tag_idx].encode_space_by);
            NSDL4_VARS(NULL, NULL, "encodespaceby in nodeVarTable = [%s]", nodeVarTable[nodevar_idx].encode_space_by);         
          }else
           nodeVarTable[nodevar_idx].encode_space_by = NULL;

            memcpy(nodeVarTable[nodevar_idx].encode_chars, tagTable[tmp_tag_idx].encode_chars, TOTAL_CHARS);
	    //break;
	    //KQ: is this brak needed?
	    // KQ: since tp_ptr is sorted by tag name, this loop shpuld exit at first mis-match of tagvar
	    //KQ: also it's better to just increment i here.
	  }  else
		break;
	  tmp_tp_idx++;
	}
	
	tempTagTable[temp_index].child_table_id = -1;
	temp_index++;
	assert(temp_index <= total_temptag_entries);
	i = tmp_tp_idx;
	//KQ: should i be  set tp tmp_tp_idx
	break;
      } else { /* this is for the intermediate tag slots */
	if (*tag_start_ptr != '<') {
	  fprintf(stderr, "tag %s is incorrect format\n", tag_name_ptr);
	  return -1;
	}
	if ((tag_end_ptr - tag_name_ptr) > 0)
	  tempTagTable[temp_index].child_table_id = get_table_id(tag_name_ptr, tag_end_ptr - tag_name_ptr + 1);
	else {
	  fprintf(stderr, "tag %s is incorrect format\n", tag_name_ptr);
	  return -1;
	}
#if 0
	/* All these fields have already been initialized */
	//Avoid overwriting some intermediate node's data, for a different node
	tempTagTable[temp_index].nodevar_idx_start = -1;
	tempTagTable[temp_index].num_nodevars = 0;
	tempTagTable[temp_index].attr_qual_hashfn = NULL;
	tempTagTable[temp_index].num_attr_table_entries = 0;
#endif
	temp_index++;
	assert(temp_index < total_temptag_entries);
      }
    }
  }

  //display_tempTag_table(tempTagTable, total_temptag_entries);
  return create_THI_table(page_num, tempTagTable, total_temptag_entries, nodevar_idx_start);
}


static int temptag_comp(const void* tag1, const void* tag2) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (((TempTagTableEntry*)tag1)->table_id > ((TempTagTableEntry*)tag2)->table_id)
    return 1;
  if (((TempTagTableEntry*)tag1)->table_id < ((TempTagTableEntry*)tag2)->table_id)
    return -1;
  return strcmp(RETRIEVE_TEMP_BUFFER_DATA(((TempTagTableEntry*)tag1)->tag_name_index), RETRIEVE_TEMP_BUFFER_DATA(((TempTagTableEntry*)tag2)->tag_name_index));
}


 int 
create_THI_table (int page_num, TempTagTableEntry* tempTagTable, int total_temptag_entries, int nodevar_idx_start) 
{
  FILE* tag_file;
  int node_index;
  int THI_index;
  int i, j;
  int table_id;
  char* tag_name;
  int tag_length;
  char have_tags;
  int num_tag_hash_entries;
  int prev_index;
  char* table_name;
  char hash_tag_name[MAX_LINE_LENGTH];
  int total_new_node_entries = 0;
  int global_thi_index;
  int total_THI_entries;
  THITableEntry* THITable;
  //hashfn_type attr_hashfunc = NULL;
  //int attr_table_size = 0;
  
  

  NSDL2_VARS(NULL, NULL, "Method called, page_num = %d, total_temptag_entries = %d, nodevar_idx_start = %d", page_num, total_temptag_entries, nodevar_idx_start);

  //sort in the order of table_id (parent-idx) and tag elemnt name 
  qsort(tempTagTable, total_temptag_entries, sizeof(TempTagTableEntry), temptag_comp);

  total_THI_entries = total_tableid_entries + 2;   /* the 2 is the dummy entry and the top entry */

  if (create_THI_entries(total_THI_entries, &global_thi_index) == FAILURE)
    return -1;

  //There is one THITTable for each sess.
  THITable = &globalTHITable[global_thi_index];

  THITable[0].node_index = global_node_cnt;   /* the first node is for the dummy entry */
  THITable[0].tag_hashfn = NULL;
  THITable[0].prev_index = -1;
  THITable[0].prevcode = 0;
  THITable[0].nodevar_idx_start = nodevar_idx_start;
  
  node_index = global_node_cnt+1;
  total_new_node_entries ++;
  global_node_cnt++;

  i = 0;
  while(i < total_temptag_entries) {  /* first pass of the THIT table that will create the hashfunctions */
    table_id = tempTagTable[i].table_id;
    THI_index = table_id + 2;
    have_tags = 0;

    sprintf(g_tmp_fname, "%s/tag.txt", g_ns_tmpdir);
    if ((tag_file = fopen(g_tmp_fname, "w+")) == NULL) {
      fprintf(stderr, "Error in creating the %s/tag.txt file\n", g_ns_tmpdir);
      return -1;
    }

    while ((tempTagTable[i].table_id == table_id) && (i < total_temptag_entries)) {
      tag_name = RETRIEVE_TEMP_BUFFER_DATA(tempTagTable[i].tag_name_index);
      tag_length = strlen(tag_name);
      
      if (fwrite(tag_name, sizeof(char), tag_length, tag_file) != tag_length) {
	fprintf(stderr, "create_shr_tag_tables(): Error in writing tag into tag.txt file\n");
	return -1;
      }
      if (fwrite("\n", sizeof(char), 1, tag_file) != 1) {
	fprintf(stderr, "create_shr_tag_tables(): Error in writing tag into tag.txt file\n");
	return -1;
      }
      
      have_tags = 1;

      //KQ: looks like,  multiple temptag table  merged into 1 entry of THIT for same table_id
      //KQ: This would only keep the last attr_hash func, if there are multiple entries with attr_hashfunc 
#if 0
      //Moved to NodeTable from THITable
      if (tempTagTable[i].attr_qual_hashfn) {
	if (!attr_hashfunc) {
	  attr_hashfunc = tempTagTable[i].attr_qual_hashfn;
	  attr_table_size = tempTagTable[i].num_attr_table_entries;
	}
      }
#endif
      i++;
    }
    

    fclose(tag_file);

#if 0
    //Moved to NodeTable from THITable
    THITable[THI_index].attr_qual_hashfn = attr_hashfunc;
    THITable[THI_index].attr_table_size = attr_table_size;
    attr_hashfunc = NULL;
    attr_table_size = 0;
#endif

    if (have_tags) {
      if (!(THITable[THI_index].tag_hashfn = create_hashfn(table_id, &num_tag_hash_entries, 0, page_num))) {
	fprintf(stderr, "create_shr_tag_tables(): could not create hash function\n");
	return -1;
      }
      THITable[THI_index].node_index = node_index;
      node_index += num_tag_hash_entries;
      total_new_node_entries += num_tag_hash_entries;
      global_node_cnt += num_tag_hash_entries;
    } else //KQ: Will there be case when have_tags=0
      THITable[THI_index].node_index = -1;

    if (table_id == -1) { /* means that we are working on the root entry 0.*/
      THITable[THI_index].prev_index = global_thi_index;
      THITable[THI_index].prevcode = 0;
    } else {
      //note that i is the temptag table index
      for (j = 0; j < i; j++) {  /* we have to go back into the temptag table to find out where the prev. entry is at */
	if (tempTagTable[j].child_table_id == table_id) {
	  prev_index = THITable[THI_index].prev_index = global_thi_index+tempTagTable[j].table_id+2;

	  /* to get the prevcode, we have to run the prev. hashfn w/ the string of the current tag */
	  table_name = RETRIEVE_TEMP_BUFFER_DATA(tableIdTable[table_id].name);
	  while (strchr(table_name, '<')) {
	    table_name = strchr(table_name, '<');
	    table_name++;
	  }
	  CLEAR_WHITE_SPACE(table_name);
	  strcpy(hash_tag_name, table_name);
	  table_name = hash_tag_name;
	  while (*table_name != '>') table_name++;
	  *table_name = '\0';
	    
	  THITable[THI_index].prevcode = globalTHITable[prev_index].tag_hashfn(hash_tag_name, strlen(hash_tag_name));
	  break;
	}
      }
      if (j == i) {
	fprintf(stderr, "create_shr_tag_tables(): Error in looking for prev_index while creating the THITable\n");
	return -1;
      }
    }
  }

  //display_THI_table();
  if (create_node_attr_table(total_new_node_entries, global_thi_index, tempTagTable, total_temptag_entries) == -1)
    return -1;

  return global_thi_index;
}

#if 0
static void   display_tagvar_table(TagTableEntry * ts, int num_tag_entries) {
int i, j;
AttrQualTableEntry *aq;

  printf("tagvar Table:\n");
  for (i = 0; i < num_tag_entries; i++) {
    //printf("Var=[%s], Type=[%s], NodeName=[%s], getval=[%s], qattr_start=[%d], qattr_num=[%d], value=[%s], order=[%d], action_on_notfound=%s\n", 
    printf("Var=[%s], Type=[%s], NodeName=[%s], getval=[%s], qattr_start=[%d], qattr_num=[%d], value=[%s], order=[%d]\n", 
	RETRIEVE_TEMP_BUFFER_DATA(ts[i].name), 
	(ts[i].type == 1)?"ALL":"Page", 
	RETRIEVE_TEMP_BUFFER_DATA(ts[i].tag_name), 
	(ts[i].getvalue == -1) ? "-": RETRIEVE_TEMP_BUFFER_DATA(ts[i].getvalue), 
	ts[i].attr_qual_start, 
	ts[i].num_attr_qual, 
	(ts[i].value_qual == -1) ? "-": RETRIEVE_TEMP_BUFFER_DATA(ts[i].value_qual), 
	ts[i].order 
	//RETRIEVE_TEMP_BUFFER_DATA(ts[i].action_on_notfound) 
	);
	aq = &attrQualTable[ts[i].attr_qual_start];
	for (j=0; j < ts[i].num_attr_qual; j++) {
		printf("\tAttrName=%s, Qual=%s\n",
			RETRIEVE_TEMP_BUFFER_DATA(aq[j].attr_name), 
			RETRIEVE_TEMP_BUFFER_DATA(aq[j].qual_str) );
	}
  }
  printf("\n");
}

static void  display_tagpage_table(TagPageTableEntry *tag_page_start, int num_tagpage_entries) {
int i;
  printf("tagpage Table:\n");
  for (i = 0; i < num_tagpage_entries; i++) {
    printf("Page=%s pgidx=%d xmlvaridx=%d\n", RETRIEVE_TEMP_BUFFER_DATA(tag_page_start[i].page_name), tag_page_start[i].page_idx, tag_page_start[i].tagvar_idx);
  }
  printf("\n");

}

static void  display_tpptr_table (int* tp_ptr, int tp_entries) {
  int i;

  printf("tpptr Table:\n");
  for (i = 0; i < tp_entries; i++) {
    printf("%s\n", RETRIEVE_TEMP_BUFFER_DATA(tagTable[tp_ptr[i]].tag_name));
  }
  printf("\n");
}


static void display_gTHI_table() {
  int i;

  printf("THI Table:\n");
  for (i = 0; i < total_globalTHI_entries; i++) {
    //printf("idx: %d, node: %d, tag_hashfn: %d, attr_hashfn: %d, prev_index: %d, prevcode: %d\n",
    printf("idx: %d, node: %d, tag_hashfn: %d, prev_index: %d, prevcode: %d\n",
	   i,
	   globalTHITable[i].node_index,
	   globalTHITable[i].tag_hashfn?1:0,
	   //globalTHITable[i].attr_qual_hashfn?1:0,
	   globalTHITable[i].prev_index,
	   globalTHITable[i].prevcode);
  }
  printf("\n");
}

static void display_tempTag_table(TempTagTableEntry *tempTagTable, int total_temptag_entries) {
  int i;

  printf("temp Tag  Table:\n");
  for (i = 0; i < total_temptag_entries; i++) {
    printf("idx: %d, table_id: %d, qualhash: %d, num_attr: %d, nodevar_start: %d num_node_var=%d\n",
	i,
    tempTagTable[i].table_id,
    tempTagTable[i].attr_qual_hashfn?1:0,
    tempTagTable[i].num_attr_table_entries,
    tempTagTable[i].nodevar_idx_start,
    tempTagTable[i].num_nodevars);
  }
  printf("\n");
}


static void display_node_table() {
  int i;

  printf("Node Table:\n");
  for (i = 0; i < total_node_entries; i++) {
    printf("idx: %d, THITable_child_idx: %d, nodevar_idx_start: %d, num_nodevars: %d\n",
	   i,
	   nodeTable[i].THITable_child_idx,
	   nodeTable[i].nodevar_idx_start,
	   nodeTable[i].num_nodevars);      
  }
}
#endif

static int 
create_node_attr_table(int total_node_entries_create, int thi_index_start, TempTagTableEntry* tempTagTable, int total_temptag_entries) 
{
  int start_node_idx;
  int i;
  int table_id;
  int thi_index;
  int node_start_idx;
  NodeTableEntry* node_start_ptr;
  char* tag_name;
  int hash_index;

  NSDL2_VARS(NULL, NULL, "Method called, total_node_entries_create = %d, thi_index_start = %d, total_temptag_entries = %d", total_node_entries_create, thi_index_start, total_temptag_entries);
  if (create_node_entries(total_node_entries_create, &start_node_idx) == FAILURE)
    return -1;

  memset(&nodeTable[start_node_idx], 0, total_node_entries_create*sizeof(NodeTableEntry));
  
  nodeTable[start_node_idx].THITable_child_idx = thi_index_start + 1;
  nodeTable[start_node_idx].nodevar_idx_start = -1;
  nodeTable[start_node_idx].num_nodevars = 0;

  i = 0;
  while (i < total_temptag_entries) {
    table_id = tempTagTable[i].table_id;
    thi_index = thi_index_start + table_id+2;
    
    node_start_idx = globalTHITable[thi_index].node_index;
    node_start_ptr = &nodeTable[node_start_idx];
    while ((tempTagTable[i].table_id == table_id) && (i < total_temptag_entries)) {
      tag_name = RETRIEVE_TEMP_BUFFER_DATA(tempTagTable[i].tag_name_index);
      assert(globalTHITable[thi_index].tag_hashfn);
      hash_index = globalTHITable[thi_index].tag_hashfn(tag_name, strlen(tag_name));
      assert(hash_index != -1);
      

      if ((node_start_ptr+hash_index)-> attr_table_size == 0) { //It is possible that some intermediate node has some nodevar. Avoid overwrite
          (node_start_ptr+hash_index)->attr_qual_hashfn = tempTagTable[i].attr_qual_hashfn;
          (node_start_ptr+hash_index)-> attr_table_size = tempTagTable[i].num_attr_table_entries;
      }
      if (tempTagTable[i].nodevar_idx_start != -1) {
	(node_start_ptr+hash_index)->nodevar_idx_start = -1;
	(node_start_ptr+hash_index)->nodevar_idx_start = tempTagTable[i].nodevar_idx_start;
	(node_start_ptr+hash_index)->num_nodevars = tempTagTable[i].num_nodevars;
      } else if ((node_start_ptr+hash_index)->num_nodevars == 0) { //It is possible that some intermediate node has some nodevar. Avoid overwrite
	(node_start_ptr+hash_index)->nodevar_idx_start = -1;
	//(node_start_ptr+hash_index)->num_nodevars = 0;
      }
      
      if (tempTagTable[i].child_table_id == -1)
	(node_start_ptr+hash_index)->THITable_child_idx = -1;
      else
	(node_start_ptr+hash_index)->THITable_child_idx = thi_index_start + tempTagTable[i].child_table_id + 2;
      i++;
    }
  }

  //display_gTHI_table();
  //display_node_table();

  return 0;
}

void
copy_all_tag_vars(UserVarEntry* uservartable_entry, NodeVarTableEntry_Shr *nodevar_ptr)
{
  int j;
  
  ArrayValEntry* temp_array_ptr;
  int count_ord = nodevar_ptr->tmp_array->total_tmp_tagvar;

  NSDL2_VARS(NULL, NULL, "Method called.");

  // Free old value(s) of variable if any
  if (uservartable_entry->value.array) {
    NSDL3_VARS(NULL, NULL, "Freeing old value(s) of tag var variable");

    for (j = 0; j < uservartable_entry->length; j++) {
      FREE_AND_MAKE_NULL(uservartable_entry->value.array[j].value, "uservartable_entry->value.array[j].value", j);
    }
    FREE_AND_MAKE_NULL(uservartable_entry->value.array, "uservartable_entry->value.array", -1);
    uservartable_entry->length = 0;
  }
    
  NSDL3_VARS(NULL, NULL, "going to put the array values into the user table, total is %d, "
             "count_ord = %d", 
             nodevar_ptr->tmp_array->total_tmp_tagvar, count_ord);
  MY_MALLOC(uservartable_entry->value.array, (count_ord * sizeof(ArrayValEntry)), "uservartable_entry->value.array", -1);

  uservartable_entry->length = count_ord;
  for (j = 0, temp_array_ptr = nodevar_ptr->tmp_array->tempTagArrayVal; j < count_ord; j++, temp_array_ptr++)
  {
    int amount_to_save  = temp_array_ptr->length;
    MY_MALLOC(uservartable_entry->value.array[j].value, (amount_to_save + 1), "uservartable_entry->value.array[j].value", j);  
    memcpy(uservartable_entry->value.array[j].value, temp_array_ptr->value, amount_to_save);

/*     /\* value in temp_array_ptr has to be freed since it was strdup'ed *\/ */
/*     FREE_AND_MAKE_NULL(temp_array_ptr->value, "temp_array_ptr->value", j); */

    NSDL3_VARS(NULL, NULL, "array idx = %d, value = %*.*s, count_ord = %d\n", 
               j, amount_to_save, amount_to_save,
               uservartable_entry->value.array[j].value, count_ord);
    
    uservartable_entry->value.array[j].value[amount_to_save] = '\0';
    uservartable_entry->value.array[j].length = amount_to_save;
    
    NSDL3_VARS(NULL, NULL, "Saved array value for count ord = %d in user table is = %s", count_ord, uservartable_entry->value.array[j].value);
  }

  // Reset this to 0 so that next time, temp array is filled from index 0
  // What ever memory is allocted, it will be reused
  nodevar_ptr->tmp_array->total_tmp_tagvar = 0;
}

//******************End Part (2) - Shared Memory related code*************

//******************Part (3) - Runtime Related code*************************

/* This function is used to set the to_stop flag */
static inline void
tag_var_check_status(VUser *vptr, NodeVarTableEntry_Shr *nodevar_ptr , int *page_fail, int *to_stop)
{
  NSDL1_VARS(vptr, NULL, "Method Called.");
  /* If this xml var is failing and action_on_notfound is equal to Error, then set to_stop to 1
  *  Note - page_fail, serach_var_fail and to_stop should be set one time
  *  Once set, this should remain set
  */
  if(nodevar_ptr->action_on_notfound == VAR_NOTFOUND_ERROR)
  {
    /*In Error case we have to fail the page.
    * In Warning case we just write to the event.log file only.
    */
     NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                __FILE__, (char*)__FUNCTION__,
				"XML variable not found");
    *page_fail = 1;
    *to_stop =  1;
  } else if(nodevar_ptr->action_on_notfound == VAR_NOTFOUND_WARNING) {
     NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MINOR,
                                __FILE__, (char*)__FUNCTION__,
				"XML variable not found");
  }
    // Nothing to do for ignore
  return;
}

static void
find_tag_vars(xmlNode* root_node, VUser* vptr, int pass, int present_depth) {
  xmlNode *cur_node;
  int hashcode;
  THITableEntry_Shr* child_ptr;
  //  UserTagAttrEntry* utatable_entry;
  struct _xmlAttr* cur_attr;
  int nodevar_idx;
  NodeVarTableEntry_Shr* nodevar_ptr;
  char* content;
  int attr_index;
  int return_val;

  NSDL1_VARS(vptr, NULL, "Method Called.");

  for (cur_node = root_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      child_ptr = (vptr->taghashptr->node_ptr + vptr->hashcode)->child_ptr;
      if (!child_ptr) return;
      hashcode = child_ptr->tag_hashfn((char *)cur_node->name, strlen((char *)cur_node->name));
 
      NSDL3_VARS(vptr, NULL, "Hashcode = %d, node name = %s, Node type XML_ELEMENT_NODE", hashcode, cur_node->name);

      //printf("node name is %s, hashcode is %d\n", cur_node->name, hashcode);
      if (hashcode != -1) {
        NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Checking XML parameter. Node Name = %s", cur_node->name); 
        NodeTableEntry_Shr* node_ptr;
	vptr->taghashptr = child_ptr;
	vptr->hashcode = hashcode;
	node_ptr = vptr->taghashptr->node_ptr + vptr->hashcode;
	if ((node_ptr)->nodevar_ptr) {
	  char* attrTable[node_ptr->attr_table_size];
	  int i;

	  //Initialize all attr to Null. Available ones would be over-weriiten by real values
	  for (i = 0; i < node_ptr->attr_table_size; i++)
	    attrTable[i] = NULL;
	  //NodeTableEntry_Shr* node_ptr = vptr->taghashptr->node_ptr + vptr->hashcode;
	  //KNQ: Why not taking content from this node itself , as opposed to that of a children
	  if (cur_node->children)
	    content = (char *)(cur_node->children->content);
	  else
	    content = ""; //Fixed Bug#6524, Earlier we were filling it with NULL

          NS_DT1(vptr, NULL, DM_L1, MM_VARS, "XML parameter found. Content = %s", content);

	  if (node_ptr->attr_qual_hashfn) {
	    if (cur_node->properties) {
	      for (cur_attr = cur_node->properties; cur_attr && (cur_attr->type == XML_ATTRIBUTE_NODE); cur_attr = cur_attr->next) {
		attr_index = node_ptr->attr_qual_hashfn((char *)cur_attr->name, strlen((char *)cur_attr->name));
		if (attr_index != -1) {
		  attrTable[attr_index] = (char *)(cur_attr->children->content);
		}
	      }
	    }
	  }
	  for (nodevar_idx = 0, nodevar_ptr = node_ptr->nodevar_ptr; nodevar_idx < node_ptr->num_nodevars; nodevar_idx++, nodevar_ptr++) {
            if(((present_depth > 0) && nodevar_ptr->tagvar_rdepth_bitmask & (1 << (present_depth -1))) 
               ||
               ((nodevar_ptr->tagvar_rdepth_bitmask == VAR_ALL_REDIRECTION_DEPTH) && (present_depth != VAR_IGNORE_REDIRECTION_DEPTH)) ||
               ((present_depth == VAR_IGNORE_REDIRECTION_DEPTH) && (nodevar_ptr->tagvar_rdepth_bitmask == present_depth))
             )
            {
              /* check_func should be called in pass 2 only if ORD=ANY */
             if ((pass == 1) ||((pass == 2) && ((nodevar_ptr->ord == ORD_ANY) || (nodevar_ptr->ord == ORD_LAST)))) 
             {
               // Calculate random number only in case of pass == 2 and ORD_ANY and on first order. 
                if((pass == 2) && (nodevar_ptr->ord == ORD_ANY) && (!vptr->order_table[nodevar_ptr->vuser_vartable_idx]))
                  nodevar_ptr->tmp_array->count_ord = (ns_get_random(gen_handle) % nodevar_ptr->tmp_array->count_ord) + 1; 
                
                return_val = nodevar_ptr->check_func(attrTable, vptr->uvtable, content, vptr->order_table, nodevar_ptr, pass);
              }
              //If we got xml var then set the found_flag  
              if(return_val == 0)  //bhushan --TODO verify its check fun returns value
                *(nodevar_ptr->found_flag) = 1; 
            }
	  }
       }

	//for (cur_node = root_node; cur_node; cur_node = cur_node->next)
	if (cur_node->children)
	  find_tag_vars(cur_node->children, vptr, pass, present_depth);
        
        vptr->hashcode = vptr->taghashptr->prevcode;
        vptr->taghashptr = vptr->taghashptr->prev_ptr;
      }
    }
  }
}

void process_tag_vars_from_url_resp(VUser *vptr, char *full_buffer, int blen, int present_depth)
{
  //int blen = vptr->bytes;(13/sep/2013: now it will be passed from do_data_processing)

  NodeVarTableEntry_Shr* nodevar_ptr; //*nv_ptr;
  UserVarEntry* vartable_ptr;
  int nodevar_idx; //nv_idx;
  int to_stop, page_fail; // Once set, this should remain set 
  to_stop = page_fail = 0; 

  NSDL1_VARS(vptr, NULL, "Method called.");
#ifndef RMI_MODE
 // if (vptr->cur_page->thi_table_ptr) {
  if (!vptr->cur_page->thi_table_ptr)   /* No TagVar is defined */
    return;

  xmlNode *root_element;
  xmlDocPtr xml_doc;
  //unsigned int siz;
  //int left;


  //	siz = (unsigned int)sbrk(0);
  xml_doc = xmlParseMemory(full_buffer, blen);
  vptr->taghashptr = vptr->cur_page->thi_table_ptr;
  vptr->hashcode = 0;
  root_element = xmlDocGetRootElement(xml_doc);

  find_tag_vars(root_element, vptr, 1 /* Pass 1 */, present_depth);

  NSDL3_VARS(NULL, NULL, "num_tag_entries = %d", vptr->taghashptr->num_tag_entries);
  nodevar_ptr = &(nodevar_table_shr_mem[vptr->taghashptr->nodevar_idx_start]);
  for (nodevar_idx = 0; nodevar_idx < vptr->taghashptr->num_tag_entries; nodevar_idx++, nodevar_ptr++) 
  {
    NSDL3_VARS(NULL, NULL, "nodevar_idx = %d(%p), ord = %d,", nodevar_idx, nodevar_ptr, nodevar_ptr->ord);
    /* This check is required otherwise the value gets overwrite by the copy all in case of LAST
    * Here we comes two time one for given RedirectionDepth and one for LAST
     If RedirectionDepth=1 and ORD=ALL, then these values gets overwirte by the LAST search result.
     To avoid this situation we have to check the following condition 
    */
    if(((present_depth > 0) && nodevar_ptr->tagvar_rdepth_bitmask & (1 << (present_depth -1))) ||
       ((nodevar_ptr->tagvar_rdepth_bitmask == VAR_ALL_REDIRECTION_DEPTH) && (present_depth != VAR_IGNORE_REDIRECTION_DEPTH)) ||
       ((present_depth == VAR_IGNORE_REDIRECTION_DEPTH) && (nodevar_ptr->tagvar_rdepth_bitmask == present_depth))
      ) 
    {
       NSDL3_VARS(NULL, NULL, "nodevar_ptr->ord = %d", nodevar_ptr->ord);
       if (nodevar_ptr->ord == ORD_ALL) 
       {
         /* For ord ALL we have to copy from array */
         NSDL3_VARS(NULL, NULL, "extracting vartable_ptr from idx %d\n", 
                    nodevar_ptr->vuser_vartable_idx);
 
         vartable_ptr = &(vptr->uvtable[nodevar_ptr->vuser_vartable_idx]);          
         copy_all_tag_vars(vartable_ptr, nodevar_ptr);
       } 
       else if ((nodevar_ptr->ord == ORD_ANY) || (nodevar_ptr->ord == ORD_LAST)) 
       {
         if(nodevar_ptr->tmp_array->count_ord > 0)
         {
            memset(vptr->order_table, 0, user_order_table_size);
            find_tag_vars(root_element, vptr, 2 /* Pass 2 */, present_depth);
         }
       } 
      //If xml fails then set the page_fail and to_stop flag on the basis of action is stop
      if(!(*(nodevar_ptr->found_flag))) 
      {
        tag_var_check_status(vptr, nodevar_ptr, &page_fail, &to_stop);
      }
    }
  }
  NSDL3_VARS(NULL, NULL, "After outer for loop: nodevar_idx = %d, nodevar_ptr = %p,", nodevar_idx, nodevar_ptr);

#endif

  //find_tag_vars(root_element);
  if (xml_doc)
    xmlFreeDoc(xml_doc);
  xmlCleanupParser();
  //xmlMemoryDump();

  /* If at least one tag var fails, then we need to fail the page and session.*/
  if(page_fail) {
    set_page_status_for_registration_api(vptr, NS_REQUEST_CV_FAILURE, to_stop, "TagVar");
  }

}

//*****************End Part (3) - Runtime Related code*********************
