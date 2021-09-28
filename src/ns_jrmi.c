#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <regex.h>
#include <libgen.h>
#include <time.h>
#include <sys/stat.h>

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
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"

#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_page.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_script_parse.h"

#include "ns_http_script_parse.h" 
#include "ns_ldap.h"
#include "ns_jrmi.h"
#include "ns_dns.h"
#include "ns_log_req_rep.h"
#include "ns_alloc.h"
#include "ns_java_obj_mgr.h"
#include "ns_global_settings.h"
#include "nslb_time_stamp.h"
#include <endian.h>
#include "ns_http_process_resp.h"
#include "ns_trace_log.h"
#include "ns_http_script_parse.h"
#include "ns_page_dump.h"
#include "ns_vuser_trace.h"
#include "ns_group_data.h"

#define DUMP_JRMI_DATA(len, data, file_name){\
        FILE *fp = fopen(file_name, "w");\
        if(fp == NULL)\
          printf("Error in opening kk.bin file. Error = [%s]\n", nslb_strerror(errno));\
        int amt_written = fwrite(data, len, 1, fp);\
        if(amt_written < 0)\
         printf("error = %s\n", nslb_strerror(errno) );\
        fclose(fp);\
  }

int g_jrmi_cavgtime_idx = -1;
#ifndef CAV_MAIN
int g_jrmi_avgtime_idx = -1;
JRMIAvgTime *jrmi_avgtime = NULL;
#else
__thread int g_jrmi_avgtime_idx = -1;
__thread JRMIAvgTime *jrmi_avgtime = NULL;
#endif
JRMICAvgTime *jrmi_cavgtime;

char *jrmiRepPointer =NULL;
int jrmiRepLength;

char *jrmi_buff = NULL;
int jrmi_content_size = 0;

char static_xml_body[]="";
char state_array[][50] = {"handshake", "handshake", "identifier", "static_call", "static_call", "call", "call", "ping", "ping", "dgack", "end"};
 
#define JRMI_BUFFER_SIZE (1024*1024) //TODO: check the size
#define JRMI_RES_BUFFER (64*1024*1024) //TODO: check the size

char buf[JRMI_RES_BUFFER]; //check with size ??????
#define COPY_BYTES(msg, len, data, data_len){\
     memcpy(msg + len, data, data_len);\
     len += data_len;\
   }


#define FILL_MAGIC_NUMBER(msg, len){\
          msg[len++] = 0x4a;   \
          msg[len++] = 0x52;   \
          msg[len++] = 0x4d;   \
          msg[len++] = 0x49;  \
    }
 
#define FILL_VERSION(msg, len){\
         msg[len++] = 0x00;   \
         msg[len++] = 0x02;   \
    }

#define FILL_PROTOCOL(msg, len) msg[len++] = 0x4b;

#define FILL_SERVER(msg, len, data, data_len){\
     short s_len = htons(server_len);\
     COPY_BYTES(msg, len, &s_len, 2);\
     COPY_BYTES(msg, len, data, data_len);\
   }

void jrmi_long(char *bytes, unsigned long val){
 bytes[1] = (val>>48) & 0x00FF000000000000; 
 bytes[2] = (val>>40) & 0x0000FF0000000000; 
 bytes[3] = (val>>32) & 0x000000FF00000000; 
 bytes[4] = (val>>24) & 0x00000000FF000000; 
 bytes[5] = (val>>16) & 0x0000000000FF0000;
 bytes[6] = (val>>8) & 0x000000000000FF00;
 bytes[7] = val & 0x00000000000000FF;
  
}

/**********************************************************************
  Name         : get_jrmi_hrd_from_resp_buf 

  Purpose      : to get jrmi headers from jrmi response and make XML

  resp_buf = 51 ac ed 00 05 77 0f 01  61 c2 a2 b9 00 00 01 4d Q....w.. a......M
    00000021  9e 7a 00 94 80 07 73 7d  00 00 00 02 00 0f 6a 61 .z....s} ......ja
    00000031  76 61 2e 72 6d 69 2e 52  65 6d 6f 74 65 00 13 41 va.rmi.R emote..A
    00000041  64 64 69 74 69 6f 6e 61  6c 49 6e 74 65 72 66 61 dditiona lInterfa
    ........ 
 
  hrd_buf - output buffer in which jrmi headers are stored in xml format out will be -
  <RmiResponseHeader>
    <Status> 8th byte from start (Eg: 01)</Status>
    <UID.Number> 4 bytes after status (Eg: 61 c2 a2 b9)</UID.Number>
    <UID.Time> 8 bytes after numbers </UID.Time>
    <Count> 2 bytes after time </Count>
  </RmiResponseHeader>
   
***********************************************************************/
static int get_jrmi_hrd_from_resp_buf(char *resp_buf, char *hrd_buf)
{
  char *in_buf_ptr = resp_buf;
  unsigned char status;
  unsigned int uid_num;
  unsigned long int uid_time;
  unsigned short int count;
  int len = 0;

  NSDL2_JRMI(NULL, NULL, "Method Called");

  in_buf_ptr += 7;  // point to status
  status = (unsigned char)in_buf_ptr[0];

  in_buf_ptr += 1;  //point to uid number
  memcpy(&uid_num, in_buf_ptr, 4);

  in_buf_ptr += 4; //point to uid time
  memcpy(&uid_time, in_buf_ptr, 8);

  in_buf_ptr += 8; //point to count
  memcpy(&count, in_buf_ptr, 2);

  NSDL2_JRMI(NULL, NULL, "status = [%0x], uid_num = [%0x], uid_time = [%lx], count = [%hx]", status, uid_num, uid_time, count);

  len = sprintf(hrd_buf, "<RmiResponseHeader>\n"
                           "  <Status>%u</Status>\n"
                           "  <UID.Number>%u</UID.Number>\n"
                           "  <UID.Time>%lu</UID.Time>\n"
                           "  <Count>%hu</Count>\n"
                         "</RmiResponseHeader>\n", status, htobe32(uid_num), htobe64(uid_time), htobe16(count));

  NSDL2_JRMI(NULL, NULL, "JRMI Resp hrd_buf = [%s], len = %d", hrd_buf, len);

  return len;
}

void jrmi_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;

  NSDL2_JRMI(NULL, cptr, "Method Called, cptr=%p conn state=%d, request_type = %d", 
            cptr, cptr->conn_state,
            cptr->url_num->request_type);
  
  Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);

}

void debug_log_jrmi_req(connection *cptr, int complete_data, int first_trace_write_flag, int con_type, char *for_which)
{

  //TODO: handle new ns_logs location in case of partition and non partition case ......................................................
  VUser* vptr = cptr->vptr;
  char *buf = cptr->free_array;
  int bytes_to_log = cptr->content_length;
  int request_type = cptr->url_num->request_type;

  NSDL2_JRMI(NULL, cptr, "Method called , size = [%d] conn_type = [%d]", bytes_to_log, con_type); 
  if(request_type != JRMI_REQUEST)
    return;

  if((global_settings->replay_mode)) return;

  {
    NSDL3_HTTP(vptr, cptr, "Method called. bytes_size = %d", bytes_to_log);
    char log_file[4096] = "\0";
   // FILE *log_fp;
    int log_fd = -1;
    //char line_break[] = "\n------------------------------------------------------------\n";
    //Need to check if buf is null since following error is coming when try to write null
    //Error: Can not write to url request file. err = Operation now in progress, bytes_to_log = 0, buf = (null)
    //also check if bytes_to_log is 0, it possible when buf = ""
    if((buf == NULL) || (bytes_to_log == 0)) return;  

    // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)

   SAVE_REQ_REP_FILES
   if(con_type){
      sprintf(log_file, "%s/logs/%s/url_req_jrmi_%s_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, for_which, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
   }else{
      sprintf(log_file, "%s/logs/%s/url_req_jrmi_%s_reg_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, for_which, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
   }
    // Do not change the debug trace message as it is parsed by GUI
   if(first_trace_write_flag)
     NS_DT4(vptr, cptr, DM_L1, MM_JRMI, "Request is in file '%s'", log_file);

   if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_CLOEXEC|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n");
   else
   {
     write(log_fd, buf, bytes_to_log);
     //if (complete_data) write(log_fd, line_break, strlen(line_break));
     close(log_fd);
   }

/*
   log_fp = fopen(log_file, "a+");
   if (log_fp == NULL)
   {
     fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
     return;
   }

    //write for both ssl and non ssl url
    if(fwrite(buf, bytes_to_log, 1, log_fp) != 1)
    {
      fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf);
      return;
    }
    if (complete_data) fwrite(line_break, strlen(line_break), 1, log_fp);

    if(fclose(log_fp) != 0)
    {
      fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
      return;
    }*/
  }
}

static void  kw_set_jrmicall_timeout_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of JRMI_CALL_TIMEOUT keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL, "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL, "  Usage: JRMI_CALL_TIMEOUT <timeout> <port>\n");
  NS_EXIT(-1, "%s Usage: JRMI_CALL_TIMEOUT <timeout> <port>", buf);
}

int kw_set_jrmicall_timeout(char *buf, int* global_set , int* global_port)
{
  char keyword[1024];
  char tmp_data[1024];
  char tmp[1024];
  int num;
  char port[10] = {0}; 

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s %s", keyword, tmp_data, port, tmp);
  NSDL2_PARSING(NULL, NULL, "num =%d", num);
  if (num < 2){
     kw_set_jrmicall_timeout_usage("Invaid number of arguments", buf);
  }

  if(tmp_data == NULL) {
     kw_set_jrmicall_timeout_usage("JRMI timeout is not given", buf);
  }

  if(ns_is_numeric(tmp_data) == 0) {
    kw_set_jrmicall_timeout_usage("Invalid timeout given", buf);
  }

  *global_set = atoi(tmp_data);
  if(*global_set <= 0)
    kw_set_jrmicall_timeout_usage("Timeout should be greater than 0.", buf);
  
  if(port[0]){
    if (ns_is_numeric(port) == 0) {
      kw_set_jrmicall_timeout_usage("Inavlid port given", buf);
    }
    *global_port = atoi(port);
    if(*global_port <= 0)
      kw_set_jrmicall_timeout_usage("Port should be greater than 0.", buf);
  }
  NSDL2_PARSING(NULL, NULL, "global_settings->jrmi_call_timeout = [%d] ,global_settings->jrmi_port = [%d] ", *global_set, *global_port);
  return 0;
}

int kw_set_jrmi_timeout(char *buf, int *to_change, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;
  int num_args;

  num_args = sscanf(buf, "%s %s %d", keyword, grp, &num_value);

  if(num_args != 3) {
    fprintf(stderr, "Two arguments expected for %s\n", keyword);
    return 1;
  }

  if (num_value <= 0) {
    fprintf(stderr, "Keyword (%s) value must be greater than 0.\n", keyword);
    return 1;
  }

  *to_change = num_value;
  return 0;
}

void add_jrmi_timeout_timer(connection *cptr, u_ns_ts_t now) {
 
  int timeout_val;
  ClientData client_data;
  VUser *vptr;

  client_data.p = cptr; 
  vptr = cptr->vptr;

  NSDL2_FTP(NULL, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);
  
  timeout_val = runprof_table_shr_mem[vptr->group_num].gset.jrmi_timeout; 

  NSDL3_JRMI(NULL, cptr, "timeout_val = %d", timeout_val);

  cptr->timer_ptr->actual_timeout = timeout_val;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, jrmi_timeout_handle, client_data, 0 );
}

/*
   Method to create handshake message
   magicNumber 4a 52 4d 49 
   version 2 
   protocol
      ox4b Stream protocol   (filling it as default)
      ox4c Single Op Protocol
      ox4d  Multilex Protocol
*/ 
int make_handshake_msg(connection *cptr, action_request_Shr* request, char *req_buf){

  int msg_len = 0;
  NSDL4_JRMI(NULL, cptr, "Method called");
  
  FILL_MAGIC_NUMBER(req_buf, msg_len);
  FILL_VERSION(req_buf, msg_len);
  FILL_PROTOCOL(req_buf, msg_len); //filling default protocol as stream protocol, TODO: take from request.proto.jrmi.protocol// 

 // DUMP_JRMI_DATA(msg_len, req_buf, "handshake.dat")
  return msg_len;
}

/*
   Method to create identifier message 
Length(2 bytes)+IP (12 byte)+length(2 byte){is 00 00} + port (2 byte


   EndpointIdentifer
     Length(2 bytes)+IP (12 byte)+length(2 byte){is 00 00} + port (2 byte)

   Example
      000c 31302e332e3137312e313732 0000 0000 

  **convert port to network order ??do we need
*/

int make_identifier_msg(connection *cptr, action_request_Shr* request, char *req_buf){

  int msg_len = 0;
  NSDL4_JRMI(NULL, cptr, "Method called");

  //int server_len = strlen(request->proto.jrmi.server); //take from request table, as it is filled by server response
  short server_len = 9; //take from request table, as it is filled by server response
  //int port = request->proto.jrmi.port; //take from request table, as it is filled by server response

  //server_len = htonl(server_len); //TODO: do we need it convert in network order

  //FILL_SERVER(req_buf, msg_len, request->proto.jrmi.server, server_len);
  FILL_SERVER(req_buf, msg_len, "127.0.1.1", server_len);

  req_buf[msg_len++] = 0x00; 
  req_buf[msg_len++] = 0x00;
  req_buf[msg_len++] = 0x00; 
  req_buf[msg_len++] = 0x00;


  NSDL4_JRMI(NULL, cptr, "msg_len = [%d]", msg_len);
  //DUMP_DATA(msg_len, req_buf, "identifier.dat")
  return msg_len;
}

int fill_jrmi_buff_from_file(char *req_buf, char *file_name){

  NSDL4_JRMI(NULL, NULL, "Method called, filename = [%s]", file_name);

  int fd = -1;
  struct stat st;
  int read_bytes = 0;

  stat(file_name, &st);
  int size = st.st_size;

  if((fd = open(file_name, O_RDONLY|O_CLOEXEC)) == -1){
    fprintf(stderr, "error in opening file, error = [%s]\n", nslb_strerror(errno));
    end_test_run();
    //exit(1);
  }

  read_bytes = read(fd, req_buf, size);
  if((read_bytes == -1) || (read_bytes != size)){
   fprintf(stderr, "error in opening reading from %s file]\n", file_name);
   end_test_run();
   //exit(1);
  }
  return read_bytes; 
}
/*

*/
int make_call_msg(connection *cptr, action_request_Shr* request, char *req_buf, int static_or_not){

  int msg_len = 0;
  char *jrmi_buff;
  int encoded_buff_len = 0;
  int operation = 2;
  int hdr_len = 0x22;
  unsigned long long hash_num;
  unsigned long long obj_id;
  unsigned int number; 
  unsigned short count; 
  unsigned long long time;
  char seg_value[1024];
  int len;

  NSDL4_JRMI(NULL, cptr, "Method called, static_or_not = %d", static_or_not);

  //fill header parts from segment

  if(!static_or_not){
  //object id
    if((len = get_value_from_segments(cptr, seg_value, &cptr->url_num->proto.jrmi.object_id, "object_id", 1024)) <= 0){
      fprintf(stderr, "Error in getting value rom segment for object id.\n");
      end_test_run();  
    }
    obj_id = atoll(seg_value);
    NSDL4_JRMI(NULL, cptr, "JRMI:object_id = [%lld]", obj_id);
    obj_id = htobe64(obj_id);
    
  //hash number 
    if((len = get_value_from_segments(cptr, seg_value, &cptr->url_num->proto.jrmi.method_hash, "method hash", 1024)) <= 0){
      fprintf(stderr, "Error in getting value rom segment for hash number.\n");
      end_test_run();  
    }
    hash_num = atoll(seg_value);
    hash_num = htobe64(hash_num);
    NSDL4_JRMI(NULL, cptr, "JRMI:hash_number = [%lld]   num_entries = [%d]", hash_num, cptr->url_num->proto.jrmi.method_hash.num_entries);
 
  //number
    if((len = get_value_from_segments(cptr, seg_value, &cptr->url_num->proto.jrmi.number, "number", 1024)) <= 0){
      fprintf(stderr, "Error in getting value rom segment for number.\n");
      end_test_run();  
    }
    number = atoi(seg_value);
    NSDL4_JRMI(NULL, cptr, "JRMI:number = [%d] num_entries [%d]", number, cptr->url_num->proto.jrmi.number.num_entries);
    number = htobe32(number);
  //time
    if((len = get_value_from_segments(cptr, seg_value, &cptr->url_num->proto.jrmi.time, "time", 1024)) <= 0){
      fprintf(stderr, "Error in getting value rom segment for time.\n");
      end_test_run();  
    }
    time = atoll(seg_value);
    NSDL4_JRMI(NULL, cptr, "JRMI:time = [%lld]", time);
    time = htobe64(time);

  //operation
    if((len = get_value_from_segments(cptr, seg_value, &cptr->url_num->proto.jrmi.operation, "operation", 1024)) <= 0){
      fprintf(stderr, "Error in getting value rom segment for time.\n");
      end_test_run();  
    }
    operation = atoi(seg_value);
    operation = htobe32(operation);
    NSDL4_JRMI(NULL, cptr, "JRMI:operation = [%d]", operation);

  //count
     if((len = get_value_from_segments(cptr, seg_value, &cptr->url_num->proto.jrmi.count, "count", 1024)) <= 0){
       fprintf(stderr, "Error in getting value rom segment for object id.\n");
      // end_test_run();  
     }
     count = atoi(seg_value);
     NSDL4_JRMI(NULL, cptr, "JRMI:count = [%d]", count);
     count = htobe16(count); 
  }

  req_buf[msg_len++] = 0x50; //fill the first byte

  
#if 0
  int total_len = 0;

  /****************send it to java server and get xml response****************/
  //NSDL4_JRMI(cptr, NULL, "Before encoding xml buffer = %s", XXXXXXXXXXXX); //xml body
  MY_MALLOC(jrmi_buff, (xxxx + 13) * 2, "jmri buff", -1);

  if( total_len = create_java_obj_msg(1, body_buff, jrmi_buff, body_len, &encoded_buff_len, 1) > 0){

    if(send_java_obj_mgr_data(jrmi_buff, encoded_buff_len) != 0){
      fprintf(stderr, "Error in sending data to java object manager.\n");
      end_test_run();
    }

    memset(jrmi_buff, 0, encoded_buff_len);
    if(read_java_obj_msg(jrmi_buff, &content_size) != 0){
      fprintf(stderr, "Error in reading data to java object manager.\n");
      end_test_run();
    }
  }else{
    fprintf(stderr, "Error in crating message data for java object manager.\n");
    end_test_run();
  }

  //log response
  /*******************************************************/
#endif
  char filename[1024];
  MY_MALLOC(jrmi_buff, 1024*1024, "jrmi buff", -1);
  if(static_or_not){

    sprintf(filename, "%s/static_jrmi.dat", g_ns_wdir);
    NSDL4_JRMI(NULL, cptr, "filename [%s]", filename);
    encoded_buff_len = fill_jrmi_buff_from_file(jrmi_buff, filename);

    COPY_BYTES(req_buf, msg_len, jrmi_buff, encoded_buff_len);
    FREE_AND_MAKE_NULL(jrmi_buff, " Free'g jrmi buff", -1 );
  }
  else{
    NSDL4_JRMI(NULL, cptr, "going to get non static body from segment address = [%p]",  &(request->proto.jrmi.post_ptr));
    int xml_len;
    char encoded_buff[1024*1024];
    //Take non sattic body from url_num
    if((xml_len = get_value_from_segments(cptr, jrmi_buff, cptr->url_num->proto.jrmi.post_ptr, "call_data", 1024*10)) <= 0){
      NSDL4_JRMI(NULL, cptr, "error in getting value from segment");
      return -1;
    }

   //send this xml buffer to java object manager and get binary data
   if(!cptr->url_num->proto.jrmi.no_param){ //converter code

     int total_len = 0;
     if( (total_len = create_java_obj_msg(1, jrmi_buff/*(in)*/, encoded_buff/*out*/, &xml_len/*in_len*/, &encoded_buff_len/*out_len*/, 1)) > 0){
       if(send_java_obj_mgr_data(encoded_buff, encoded_buff_len, 1) != 0){
         fprintf(stderr, "Error in sending data to java object manager.\n");
         end_test_run(); 
       }

       memset(encoded_buff, 0, encoded_buff_len); 
       if(read_java_obj_msg(encoded_buff, &encoded_buff_len, 1) != 0){
         fprintf(stderr, "Error in reading data to java object manager.\n");
         end_test_run(); 
       }
     //DUMP_JRMI_DATA(encoded_buff_len, encoded_buff, "test.dat"); //just for debug purpose
     }else{
       fprintf(stderr, "Error in creating message data for java object manager.\n");
       end_test_run(); 
     }
     memcpy(req_buf + msg_len, encoded_buff, 4);
   }else {
     NSDL4_JRMI(NULL, cptr, "No Param flag found, going to get first four bytes");
     memcpy(req_buf + msg_len, jrmi_buff, 4);

   }//converter code end

   //memcpy(req_buf + msg_len, encoded_buff, 4);
   msg_len += 4; // for starting ac ed

   int dummy = 0x77;
   memcpy(req_buf + msg_len, &dummy, 1);
   msg_len++;

   memcpy(req_buf + msg_len, &hdr_len, 1);
   msg_len++;

   //copy all values from segment 
   memcpy(req_buf + msg_len, &obj_id, 8); //obj_id
   msg_len += 8;

   memcpy(req_buf + msg_len, &number, 4);  //number
   msg_len += 4;

   memcpy(req_buf + msg_len, &time, 8);   //time
   msg_len += 8;

   memcpy(req_buf + msg_len, &count, 2);   //count
   msg_len += 2;

   //operation
   memcpy(req_buf + msg_len, &operation, 4);   //operation
   msg_len += 4;

   memcpy(req_buf + msg_len, &hash_num , 8);   //Hash
   msg_len += 8;

   if(!cptr->url_num->proto.jrmi.no_param){ //converter code
     NSDL4_JRMI(NULL, cptr, "No Param flag not found, copy encoded message");
     COPY_BYTES(req_buf, msg_len, encoded_buff + 4, encoded_buff_len - 4);
    } else { 
     NSDL4_JRMI(NULL, cptr, "No Param flag found, copy message");
     COPY_BYTES(req_buf, msg_len, jrmi_buff + 40, xml_len - 40);
    }  
  }

  return msg_len;

//#endif
  /*if(!static_or_not){
    struct timeval want_time;
    unsigned long timestamp;

    unsigned char bytes[8];
    gettimeofday(&want_time, NULL);

    timestamp = (want_time.tv_sec)*1000 + (want_time.tv_usec / 1000);
    NSDL4_JRMI(NULL, cptr, "timestamp [%llu]", timestamp);
    //timestamp = jrmi_read_long(timestamp);
    //jrmi_long(bytes, timestamp);

    //NSDL4_JRMI(NULL, cptr, "timestam [%lld]", timestamp);
    // timestamp = htonl(timestamp);
    timestamp =  htobe64(timestamp);
    NSDL4_JRMI(NULL, cptr, "timestam [%lx]", timestamp);
    memcpy(req_buf + 20, &timestamp , 8);   //copy timestamp taken from response 
  }*/
  // DUMP_DATA(msg_len, req_buf, "call.dat")

}

int make_ping_msg(connection *cptr, action_request_Shr* request, char *req_buf){

  int msg_len = 0;
  NSDL4_JRMI(NULL, cptr, "Method called");

  req_buf[msg_len++] = 0x52;

  return msg_len;
}

int make_dgack_msg(connection *cptr, action_request_Shr* request, char *req_buf){

  int msg_len = 0;
  NSDL4_JRMI(NULL, cptr, "Method called");

  req_buf[msg_len++] = 0x54;
  COPY_BYTES(req_buf, msg_len, cptr->last_iov_base, 14);
//  FREE_AND_MAKE_NULL(cptr->last_iov_base, "free'ng jrmi uniq id data", -1);
  return msg_len;
}

void set_jrmi_proto_state(connection *cptr)
{
  NSDL4_JRMI(NULL, cptr, "Method called");
 
  if(cptr->proto_state == ST_JRMI_HANDSHAKE){
    NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_HANDSHAKE to ST_JRMI_IDENTIFIER");
    cptr->proto_state = ST_JRMI_HANDSHAKE_RES;
  }else if(cptr->proto_state == ST_JRMI_IDENTIFIER){ // ST_JRMI_IDENTIFIER state will come for both type of connection as we send identifier
    NSDL4_JRMI(NULL, cptr, "flow should not come here"); // for both connections
    if(cptr->flags &= NS_CPTR_JRMI_REG_CON){
      NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_IDENTIFIER to ST_JRMI_STATIC_CALL");
      cptr->proto_state = ST_JRMI_STATIC_CALL; 
    }else{ 
      NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_IDENTIFIER to ST_JRMI_NSTATIC_CALL");
      cptr->proto_state = ST_JRMI_NSTATIC_CALL; 
    }
  } else if (cptr->proto_state == ST_JRMI_STATIC_CALL){
    NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_STATIC_CALL to ST_JRMI_STATIC_CALL_RES");
    cptr->proto_state = ST_JRMI_STATIC_CALL_RES;
  } else if (cptr->proto_state == ST_JRMI_NSTATIC_CALL){
    NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_NSTATIC_CALL to ST_JRMI_NSTATIC_CALL_RES");
    cptr->proto_state = ST_JRMI_NSTATIC_CALL_RES;
  } else if (cptr->proto_state == ST_JRMI_STATIC_CALL_RES || cptr->proto_state == ST_JRMI_NSTATIC_CALL_RES){
    NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_STATIC_CALL_RES/ST_JRMI_NSTATIC_CALL_RES to ST_JRMI_PING");
    cptr->proto_state = ST_JRMI_PING;
  } else if(cptr->proto_state == ST_JRMI_PING){
    NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_PING to  ST_JRMI_PING_RES");
    cptr->proto_state = ST_JRMI_PING_RES;
  } else if(cptr->proto_state == ST_JRMI_DGACK){
    NSDL4_JRMI(NULL, cptr, "State change ST_JRMI_DGACK to ST_JRMI_END");
    cptr->proto_state = ST_JRMI_END;
  }
}

void handle_jrmi_write(connection *cptr, u_ns_ts_t now){

  int bytes_sent;
  char *buf = cptr->free_array + cptr->tcp_bytes_sent;
  //VUser *vptr = cptr->vptr;

  NSDL2_DNS(NULL, cptr, "Method called cptr=%p conn state=%d, proto_state = %d, now = %u, bytes left to send=[%d]", 
							                   cptr, cptr->conn_state, cptr->proto_state, now, cptr->bytes_left_to_send);

  if((bytes_sent = write(cptr->conn_fd, buf, cptr->bytes_left_to_send)) < 0){
    fprintf(stderr, "Sending jrmi request failed, fd=[%d], Error=[%s]\n", cptr->conn_fd, nslb_strerror(errno)); 
    end_test_run(); // TODO
    //retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
    return;
  }

  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0) {
    //retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return;
  }

  if (bytes_sent < cptr->bytes_left_to_send) {
    cptr->bytes_left_to_send -= bytes_sent;
    jrmi_avgtime->jrmi_tx_bytes += bytes_sent;
    cptr->tcp_bytes_sent += bytes_sent;
    cptr->conn_state = CNST_JRMI_WRITING;
    return;
  }

  //log jrmi request
#ifdef NS_DEBUG_ON
  if(cptr->flags & NS_CPTR_JRMI_REG_CON)
    debug_log_jrmi_req(cptr, 1, 0, 0, state_array[cptr->proto_state]); //for registry
  else 
    debug_log_jrmi_req(cptr, 1, 0, 1, state_array[cptr->proto_state]); //for api
#else
  if(runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.trace_level){
    if(cptr->flags & NS_CPTR_JRMI_REG_CON)
      debug_log_jrmi_req(cptr, 1, 0, 0, state_array[cptr->proto_state]); //for registry
    else 
      debug_log_jrmi_req(cptr, 1, 0, 1, state_array[cptr->proto_state]); //for api
  }
#endif

  //FREE_AND_MAKE_NULL(cptr->free_array,"cptr->free_array from jrmi", -1); //TODO:

  // Increase both the counters for jrmi and tcp
  jrmi_avgtime->jrmi_tx_bytes += bytes_sent;
  cptr->tcp_bytes_sent += bytes_sent;
  cptr->bytes_left_to_send -= bytes_sent;

  set_jrmi_proto_state(cptr);
  on_request_write_done (cptr);
  cptr->tcp_bytes_sent = 0;
  cptr->total_bytes = cptr->content_length = 0;
}

void debug_log_jrmi_req_res_xml(connection *cptr, char *buf, int size, char *for_which)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  NSDL2_DNS(NULL, cptr, "Method called , size = [%d] for_which =[%s]", size, for_which); 

  if(request_type != JRMI_REQUEST)
    return;

  {
    char log_file[1024];
    int log_fd;

    SAVE_REQ_REP_FILES
    sprintf(log_file, "%s/logs/%s/%s_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
            g_ns_wdir, req_rep_file_path, for_which, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));

    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_JRMI, "Response body is in file '%s'", log_file);

    if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n");
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }
}

void debug_log_jrmi_res(connection *cptr, char *buf, int size, int complete_data, int con_type, char *for_which)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;
  //char line_break[] = "\n------------------------------------------------------------\n";

  NSDL2_JRMI(NULL, cptr, "Method called , size = [%d] conn_type = [%d]", size, con_type); 

  if(request_type != JRMI_REQUEST)
    return;

  {
    char log_file[1024];
    int log_fd;

    // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    //sprintf(log_file, "%s/logs/TR%d/url_rep_%d_%ld_%ld_%d_0.dat", g_ns_wdir, testidx, my_port_index, vptr->user_index, cur_vptr->sess_inst, vptr->page_instance);
  

   SAVE_REQ_REP_FILES
   if(con_type){
     sprintf(log_file, "%s/logs/%s/url_rep_jrmi_%s_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
            g_ns_wdir, req_rep_file_path, for_which, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));
   }else{
     sprintf(log_file, "%s/logs/%s/url_rep_jrmi_%s_reg_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
            g_ns_wdir, req_rep_file_path, for_which, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));

   }
    // Do not change the debug trace message as it is parsed by GUI
   if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_JRMI, "Response is in file '%s'", log_file);

   if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n");
   else
   {
     write(log_fd, buf, size);
    // if (complete_data) write(log_fd, line_break, strlen(line_break));
     close(log_fd);
   }
  }
}

void setup_new_connection_for_rmi(VUser *vptr, connection *reg_cptr, int port){

  connection *cptr;

  NSDL2_JRMI(NULL,  NULL, "Method called");

  cptr = get_free_connection_slot(vptr);

  NSDL2_JRMI(NULL, NULL, "After getting cptr. cptr = %p, port = [%d]", cptr, port);
  cptr->num_retries = 0;

  SET_URL_NUM_IN_CPTR(cptr, reg_cptr->url_num);
  cptr->proto_state = ST_JRMI_HANDSHAKE;
 
  cptr->conn_link = reg_cptr;  
  reg_cptr->conn_link = cptr; 

  cptr->url_num->proto.jrmi.port = port;
}

int find_port_in_resp(char *jrmi_buff){

  return 0;
}

int process_static_call_resp(connection *cptr, char *buf){

  //int ret;
  //char *jrmi_buff;
  //char *res_buf;
  VUser *vptr = (VUser*)cptr->vptr;
  NSDL2_JRMI(NULL, cptr, "Method called");

  // Copy response from link list to a buffer
  //if(copy_from_cptr_to_buf(cptr, buf) == -1) //buf is of 100k, we may need more than this 
  // return;

 // res_buf = buf;
  #if 0
  /****************send it to java server and get xml response****************/
  NSDL4_JRMI(cptr, NULL, "Before encoding xml buffer = %s", res_buf); //xml body
  MY_MALLOC(jrmi_buff, (cptr->content_length + 13) * 2, "jrmi buff", -1);

  if( total_len = create_java_obj_msg(1, res_buf + 1, jrmi_buff, &cptr->content_length, &encoded_buff_len, 1) > 0){

    if(send_java_obj_mgr_data(jrmi_buff, encoded_buff_len) != 0){
      fprintf(stderr, "Error in sending data to java object manager.\n");
      end_test_run();
    }

    memset(jrmi_buff, 0, encoded_buff_len);
    if(read_java_obj_msg(jrmi_buff, &content_size) != 0){
      fprintf(stderr, "Error in reading data to java object manager.\n");
      end_test_run();
    }
  }else{
    fprintf(stderr, "Error in creating message data for java object manager.\n");
    end_test_run();
  }
#endif
  //int port = find_port_in_resp(jrmi_buff);
  
  memcpy(cptr->last_iov_base, buf + 8, 14);   //get unique id from call response 
  // short port = cptr->url_num->proto.jrmi.port;

  // From here we will go to get new cptr for api connections and do rmi handshake on that connection 
  setup_new_connection_for_rmi(vptr, cptr, global_settings->jrmi_port);
  return 0;
}

/*
  Method to make jrmi messages
*/
char *req_buf = NULL;
void make_jrmi_msg( connection *cptr, u_ns_ts_t now)
{
  action_request_Shr* request = cptr->url_num;
  int msg_len = 0;

  NSDL4_JRMI(NULL, cptr, "Calling jrmi_make_msg");

  if(!cptr->last_iov_base){
    MY_MALLOC_AND_MEMSET(cptr->last_iov_base, 14 + 1, "jrmi unique id", -1);
  }


  if(!req_buf)
    MY_MALLOC(req_buf, JRMI_BUFFER_SIZE, "malloc'd jrmi request buffer", -1);

  switch(cptr->proto_state){

    case ST_JRMI_HANDSHAKE:
         msg_len = make_handshake_msg(cptr, request, req_buf); //set ptoto_state = ST_JRMI_IDENTIFIER
         break;

    case ST_JRMI_IDENTIFIER: {
         msg_len = make_identifier_msg(cptr, request, req_buf);
         
         //DUMP_JRMI_DATA(msg_len, req_buf, "identifier.dat")
         cptr->free_array = req_buf;
         cptr->content_length = cptr->bytes_left_to_send = msg_len;// Will be use in sending data
 
         handle_jrmi_write(cptr, now);
         //send msg  //dont set read state

         if(cptr->flags &= NS_CPTR_JRMI_REG_CON)
           msg_len = make_call_msg(cptr, request, req_buf, 1);   //set ptoto_state = ST_JRMI_STATIC_CALL_RES
         else 
           msg_len = make_call_msg(cptr, request, req_buf, 0);   //set ptoto_state = ST_JRMI_STATIC_CALL_RES
           
         break;
       }  
   /* case ST_JRMI_STATIC_CALL_RES:
         process_static_call_resp(cptr, now);
         break;
    */ 
    case ST_JRMI_STATIC_CALL:
         msg_len = make_call_msg(cptr, request, req_buf, 1);   //set ptoto_state = ST_JRMI_PING  static_or_not = 0
         break;
    case ST_JRMI_NSTATIC_CALL:
         msg_len = make_call_msg(cptr, request, req_buf, 0);   //set ptoto_state = ST_JRMI_PING  static_or_not = 1
         break;

    case ST_JRMI_PING:
         msg_len = make_ping_msg(cptr, request, req_buf);  //set ptoto_state = ST_JRMI_DGACK
         break;

    case ST_JRMI_DGACK:
         msg_len = make_dgack_msg(cptr, request, req_buf); // proto_state = ST_JRMI_END
         break;

    default:
         //TODO: log error and event
        fprintf(stderr, "Unrecognized state in jrmi.\n");
        end_test_run();
  }

  cptr->free_array = req_buf;
  cptr->content_length = cptr->bytes_left_to_send = msg_len;// Will be use in sending data 
  handle_jrmi_write(cptr, now);

  // As we have two connection here one is for registry and one is for API, now we have second cptr in connection link of cptr, as we have 
  // done all messages for registry cptr, now we will start api execution by starting handshake on API  cptr 
  if((cptr->flags &= NS_CPTR_JRMI_REG_CON) && (cptr->proto_state == ST_JRMI_END)){        
    NSDL2_JRMI(NULL, cptr, "Registry cptr is in ST_JRMI_END state, Going to start new connection for API execution,"
							"registry_cptr = %p, New cptr = %p", cptr, cptr->conn_link);

    if(cptr->conn_link){
      NSDL2_JRMI(NULL, cptr, "Connection link found, going to call start_scoket for new cptr");

      start_socket(cptr->conn_link, now);

     }
  } else if (cptr->proto_state == ST_JRMI_END){
      NSDL2_JRMI(NULL, cptr, "API connection ST_JRMI_END state found. Going to call Close_connection");

      NSDL2_JRMI(NULL, cptr, "cptr->buf_head = %p", cptr->buf_head);
    
    Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);


  }
}

/* Function calls retry_connection() which will ensure normal http like retries for ftp also. */
static inline void
handle_jrmi_bad_read (connection *cptr, u_ns_ts_t now, char *buf)
{
  NSDL2_JRMI(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
 
  int timer_type = cptr->timer_ptr->timer_type;

#ifdef NS_DEBUG_ON
  if(cptr->flags & NS_CPTR_JRMI_REG_CON) 
    debug_log_jrmi_res(cptr, buf, cptr->total_bytes, 1, 0, state_array[cptr->proto_state]); 
  else
    debug_log_jrmi_res(cptr, buf, cptr->total_bytes, 1, 1, state_array[cptr->proto_state]); 

#endif

  if(cptr->last_iov_base){
  //  FREE_AND_MAKE_NULL(cptr->last_iov_base, "free'ng, cptr->last_iov_base in jrmi", -1);
  }
  if (cptr->req_code_filled == 0) {
    if(timer_type == AB_TIMEOUT_KA)
       close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
       /* means that this connection is a keep alive or new and the server closed it without sending any data*/
       retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
  } else { // TODO check if complete length is recived or not
    Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES); 
  }
}

int process_res_protoack (connection *cptr, char *res_buf){

  NSDL2_JRMI(NULL, cptr, "Method called");
  char *res_ptr = res_buf;
 
 
  if(res_ptr[0] != 0x4e)
    return -1;
 
  cptr->proto_state = ST_JRMI_IDENTIFIER;
  return 0; 
}

#define JRMI_BUFF_SIZE (1024*1024)

int process_res_call (connection *cptr, char *res_buf, u_ns_ts_t now){

  char *res_ptr = res_buf;
  VUser *vptr = cptr->vptr;
  int encoded_buff_len = 0;
  int total_len = 0;
  char hrd_buf[1024 + 1];
  int len = 0;

  NSDL2_JRMI(NULL, cptr, "Method called, res_ptr[0] = [%0x]", res_ptr[0]);

  if(res_ptr[0] != 0x51)
    return -1;


  len = get_jrmi_hrd_from_resp_buf(res_buf, hrd_buf); 
  NSDL2_JRMI(NULL, cptr, "Header len = %d", len);

  memcpy(cptr->last_iov_base, buf + 8, 14);   //get unique id from call response 

  /****************send it to java server and get xml response****************/
  if(global_settings->use_java_obj_mgr){
    if(!jrmi_buff)
     MY_MALLOC(jrmi_buff, JRMI_BUFF_SIZE, "jmri buff", -1); //??is this size is enough

    if( (total_len = create_java_obj_msg(1, res_buf + 1, jrmi_buff, &cptr->content_length, &encoded_buff_len, 2)) > 0){

      if((send_java_obj_mgr_data(jrmi_buff, encoded_buff_len, 0)) != 0){
        fprintf(stderr, "Error in sending data to java object manager.\n");
        init_java_obj_mgr_con(vptr, global_settings->java_object_mgr_port);
       // end_test_run();
      }

      memset(jrmi_buff, 0, encoded_buff_len);
      if(read_java_obj_msg(jrmi_buff + len, &jrmi_content_size, 0) != 0){ 
        NSDL2_JRMI(NULL, cptr, "Error in reading data to java object manager.");
        //fprintf(stderr, "Error in reading data to java object manager.\n");
        memcpy(jrmi_buff, res_buf, cptr->content_length);
        jrmi_buff[cptr->content_length] = '\0';
        jrmi_content_size = cptr->content_length;
        init_java_obj_mgr_con(vptr, global_settings->java_object_mgr_port);
        //end_test_run();
      }
    }else{
      fprintf(stderr, "Error in creating message data for java object manager.\n");
      //end_test_run();
    }

    memcpy(jrmi_buff, hrd_buf, len);  

#ifdef NS_DEBUG_ON  
    debug_log_jrmi_req_res_xml(cptr, jrmi_buff, jrmi_content_size + len, "url_rep_body_jrmi_call");
#else
   if(runprof_table_shr_mem[vptr->group_num].gset.trace_level)
     debug_log_jrmi_req_res_xml(cptr, jrmi_buff, jrmi_content_size + len, "url_rep_body_jrmi_call");
#endif

    //FREE_AND_MAKE_NULL(jrmi_buff, "free'ng jrmi buff", -1);
  }

  if(res_ptr[7] == 0x02){
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_BAD_BYTES);
    return -1;
  }

  return 0; 
}

int process_res_ping (connection *cptr, char *res_buf){

  NSDL2_JRMI(NULL, cptr, "Method called");
  char *res_ptr = res_buf;

  if(res_ptr[0] != 0x53)
    return -1;

  return 0;
}

int process_jrmi_response(connection *cptr, u_ns_ts_t now){

  int ret;

  NSDL2_JRMI(NULL, cptr, "Method called");

  // Copy response from link list to a buffer
  if(copy_from_cptr_to_buf(cptr, buf) == -1) //buf is of 100k, we may need more than this 
    return -1;
 
  switch(cptr->proto_state){
     case ST_JRMI_HANDSHAKE_RES:
          ret = process_res_protoack(cptr, buf);
          break;

     case ST_JRMI_NSTATIC_CALL_RES:
          ret = process_res_call(cptr, buf, now);
          cptr->proto_state = ST_JRMI_PING;
          break;

     case ST_JRMI_STATIC_CALL_RES:

         process_static_call_resp(cptr, buf);
         cptr->proto_state = ST_JRMI_PING;

         break;
     case ST_JRMI_PING_RES:
          ret = process_res_ping(cptr, buf);
          cptr->proto_state = ST_JRMI_DGACK;
          break;
  } 

  if(ret == -1){
   // fprintf(stderr, "Mismatch in JRMI proto state (%d) and response type (%2x)recieved\n", cptr->proto_state, (unsigned char)buf[0]);
   // end_test_run(); //TODO: log event
    return ret;
  }
  return 0;
}

void delete_jrmi_timeout_timer(connection *cptr) {

  NSDL2_JRMI(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL2_JRMI(NULL, cptr, "Deleting Idle timer.");
    dis_timer_del(cptr->timer_ptr);
  }
}

void jrmi_complete_read(connection* cptr, u_ns_ts_t now){

  NSDL2_JRMI(NULL, NULL, "Method called");
  char buf[65536 + 1];
  int ret;
  VUser *vptr = cptr->vptr;

  if(copy_from_cptr_to_buf(cptr, buf) == -1) 
    return;

  delete_jrmi_timeout_timer(cptr); 
  cptr->tcp_bytes_recv += cptr->content_length;
  jrmi_avgtime->jrmi_rx_bytes += cptr->content_length;
  jrmi_avgtime->jrmi_total_bytes += cptr->content_length;

  if(SHOW_GRP_DATA) { \
    JRMIAvgTime *local_jrmi_avg = NULL; \
    GET_JRMI_AVG(vptr, local_jrmi_avg); \
    local_jrmi_avg->jrmi_rx_bytes +=  cptr->content_length; \
    local_jrmi_avg->jrmi_total_bytes +=  cptr->content_length; \
  }

  cptr->bytes = cptr->total_bytes;
 
#ifdef NS_DEBUG_ON
  if(cptr->flags & NS_CPTR_JRMI_REG_CON) 
    debug_log_jrmi_res(cptr, buf, cptr->total_bytes, 1, 0, state_array[cptr->proto_state]); 
  else
    debug_log_jrmi_res(cptr, buf, cptr->total_bytes, 1, 1, state_array[cptr->proto_state]); 
#else
  if(runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.trace_level){
    if(cptr->flags & NS_CPTR_JRMI_REG_CON) 
      debug_log_jrmi_res(cptr, buf, cptr->total_bytes, 1, 0, state_array[cptr->proto_state]); 
    else
      debug_log_jrmi_res(cptr, buf, cptr->total_bytes, 1, 1, state_array[cptr->proto_state]); 
  }
#endif
  ret = process_jrmi_response(cptr, now);

  if(ret == -1)return;
  cptr->req_code_filled = -1; 
  make_jrmi_msg(cptr, now);
 
}

void jrmi_call_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;

  NSDL2_JRMI(NULL, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);

  jrmi_complete_read(cptr, now); 
}

void add_jrmi_call_timer(connection *cptr, u_ns_ts_t now) {
 
  int timeout_val;
  ClientData client_data;

  client_data.p = cptr; 

  NSDL2_JRMI(NULL, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);
  
  timeout_val = global_settings->jrmi_call_timeout;

  NSDL3_JRMI(NULL, cptr, "timeout_val = %d", timeout_val);

  cptr->timer_ptr->actual_timeout = timeout_val;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, jrmi_call_timeout_handle, client_data, 0 );
}

static inline void
handle_jrmi_data_read_complete (connection *cptr, u_ns_ts_t now)
{
  NSDL2_JRMI(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);
}

int jrmi_read_short(const char *buf, int len)
{       
  NSDL2_JRMI(NULL, NULL, "Method called");
        
  short i =0, val, b16=0,b8=0;
  if(len == 1){ b8 = buf[i++] & 0xFF;}
  if(len == 2){ b16 = buf[i++] & 0xFF; b8 = buf[i++] & 0xFF;}
          
  val = ((b16 << 8) + b8) & 0xFFFF;
  return (val);
}  
 

int handle_jrmi_read( connection *cptr, u_ns_ts_t now ) 
{
#ifdef ENABLE_SSL
  /* See the comment below */ // size changed from 32768 to 65536 for optimization : Anuj 28/11/07
  char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#else
  char buf[4096 + 1];     /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#endif
  int bytes_read;
  int request_type;
  //char handle = 0;
  //int req_len = 0;
  //int val;

  NSDL2_JRMI(NULL, cptr, "conn state=%d, now = %u, cptr->url_num = %p", cptr->conn_state, now, cptr->url_num);
  //NSDL2_JRMI(NULL, cptr, "conn state=%d, now = %u", cptr->conn_state, now);

  //request_type = cptr->url_num->request_type;
  request_type = JRMI_REQUEST;
  if (request_type != JRMI_REQUEST) { 
    /* Something is very wrong we should not be here. */
    fprintf(stderr, "Request type is not jrmi but still we are in an jrmi state. We must not come here. Something is seriusly wrong\n");
    end_test_run();
  }

  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */

  while (1) {
    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;

    bytes_read = read(cptr->conn_fd, buf, bytes_read);
    NSDL2_JRMI(NULL, cptr, "bytes_read = %d", bytes_read);

#ifdef NS_DEBUG_ON
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
#endif

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        if(cptr->proto_state == ST_JRMI_NSTATIC_CALL_RES){
          delete_jrmi_timeout_timer(cptr);
          add_jrmi_call_timer(cptr, now);
          cptr->content_length = cptr->total_bytes;  //TODO:  find way to get content length in call response case??????????
        }
       
        return 1;
      } else {
        NSDL3_JRMI(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_jrmi_bad_read (cptr, now, buf);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_jrmi_bad_read (cptr, now, buf);
      //handle_server_close (cptr, now);
      return -1;
    }
  
    copy_retrieve_data(cptr, buf, bytes_read, cptr->total_bytes);
    cptr->total_bytes +=  bytes_read;

    if(cptr->req_code_filled < 0) {

      if(cptr->buf_head->buffer[0] == 0x53){ //ping response
        //cptr->content_length = 6; //1(code) + 5(null bytes)
        cptr->content_length = cptr->total_bytes;  //TODO:  find way to get content length in call response case??????????
        cptr->req_code_filled = 1;
      }else if((unsigned char)cptr->buf_head->buffer[0] == (unsigned char)0x4e){  //protocol acknowlege
         if(cptr->total_bytes < 3){
           continue;
         }else{
           short val = jrmi_read_short(cptr->buf_head->buffer + 1, 2); 
           cptr->content_length = 1 /*ack */ + 2 /*host len*/ + val /* host */ + 2 + 2/* port */;    
           cptr->req_code_filled = 1;  
           NSDL2_JRMI(NULL, cptr, "val = %d", val);
         }
      }else if ((unsigned char)cptr->buf_head->buffer[0] == (unsigned char)0x51){   //call response case
        NSDL2_JRMI(NULL, cptr, "Call Response");
        cptr->req_code_filled = 1;
        if(cptr->proto_state == ST_JRMI_STATIC_CALL_RES)
          cptr->content_length = cptr->total_bytes;
        else
          continue;
        
      }else{ //unrecognized reponse
         fprintf(stderr, "ERROR: Unrecognized jrmi response.\n");
         end_test_run();
      }
    }
    
    NSDL2_JRMI(NULL, cptr, "cptr->total_bytes = %d, cptr->content_length = %d ", cptr->total_bytes, cptr->content_length);

    // Complete response is read, break the loop of read
    if(cptr->total_bytes == cptr->content_length){ //how to know the complete length????
      break;
    } 
  }


  jrmi_complete_read(cptr, now);
  return 0;
}

connection *jrmi_check_registry_con(VUser *vptr, char *host_name, int host_len, u_ns_ts_t now){

  connection *cur_node;
  if(host_len == 0) host_len = strlen(host_name);

  NSDL2_JRMI(vptr, NULL, "Method called. host_name = %s", host_name);

  if(vptr->head_cinuse){ // If we have any connection in inuse list then check for connection there 
    NSDL2_JRMI(vptr, NULL, "Inuse list head found");

    cur_node = vptr->head_cinuse;

    while (cur_node != NULL){ 
 //     NSDL4_JRMI(vptr, NULL, "host_name = %s, cur_node->old_svr_entry->server_name = %s", host_name, cur_node->old_svr_entry->server_name);
      //if(!strncmp(cur_node->old_svr_entry->server_name, host_name, host_len)){
      if(cur_node->flags &= NS_CPTR_JRMI_REG_CON)
        return cur_node; 
      cur_node = (connection *)cur_node->next_inuse;
    }
  }
  return NULL;
}

// Search in inuse list for API connection by matching method name 
connection *jrmi_check_method_con(VUser *vptr, action_request_Shr *url_num){

  connection *cur_node;

  //char cur_method[1024] = "0"; //check for size
  //char link_method[1024] = "0";

  //int tmp_len = 0;

  //if((tmp_len = get_value_from_segments(cptr, cur_method, &url_num->proto.jrmi.method, "METHOD", 1024)) <= 0)
   //   return -1;

  NSDL2_JRMI(vptr, NULL, "Method called. cur method = [%s]", url_num->proto.jrmi.method);
  
  if(vptr->head_cinuse){ // If we have any connection in inuse list then check for connection there 
    NSDL2_JRMI(vptr, NULL, "Inuse list head found");

    cur_node = vptr->head_cinuse;
    while (cur_node != NULL){
     // if((tmp_len = get_value_from_segments(cptr, link_method, &cur_node->url_num->proto.jrmi.method, "METHOD", 1024)) <= 0)
       // return -1;
       
      NSDL4_JRMI(vptr, NULL, "cur_node->url_num->jrmi->method = [%s]", cur_node->url_num->proto.jrmi.method);
      if(cur_node->flags &= NS_CPTR_JRMI_REG_CON){
        cur_node = (connection *)(cur_node->next_inuse);
        continue;
      }
      if(!strcmp(url_num->proto.jrmi.method, cur_node->url_num->proto.jrmi.method)){
        NSDL2_JRMI(vptr, NULL, "Found cptr for method [%s] link_method = [%s]", url_num->proto.jrmi.method, cur_node->url_num->proto.jrmi.method);
        return cur_node; 
      }
      cur_node = (connection *)(cur_node->next_inuse);
    }
  }
  return NULL;
}



/* In case of jrmi protocall we can have following caese:
* First case: Registry connection does not exist, this case arise when we will extecute first API or due to any errors registry connection has
* been broken. In this case we will satrt flow from here for registry connection and after sending staticirequest and getting portin response
* we will setup  API connection on that port and start execution on that API connection after doing RMI handshake
* Second Case: Regitry connection is already threre, then we will search for API connection for that method now there can be two cases. First 
* if API connection is already present then we will start execution on this connection. In second case if API connection for that method is not* present then we will send static XML on registry connection for that method and get API connection port and setup new API connection and 
* start execution on that connection */ 

void jrmi_con_setup(VUser *vptr, action_request_Shr *url_num, u_ns_ts_t now){

  connection *reg_cptr;
  connection *cptr;
  reg_cptr = jrmi_check_registry_con(vptr, url_num->index.svr_ptr->server_hostname, url_num->index.svr_ptr->server_hostname_len, now);

  if(reg_cptr == NULL){ // Registry connection not found
    NSDL1_HTTP(vptr, NULL, "Registry cptr not found. Going to setup registry connection");
    reg_cptr = get_free_connection_slot(vptr);
    reg_cptr->num_retries = 0;
    NSDL1_HTTP(vptr, NULL, "After get_free_connection_slot cptr=%p", reg_cptr);
    SET_URL_NUM_IN_CPTR(reg_cptr, url_num);
    reg_cptr->proto_state = ST_JRMI_HANDSHAKE;
    reg_cptr->flags |= NS_CPTR_JRMI_REG_CON; // Set this connection as registry con
    start_socket(reg_cptr, now);  
  } else { // Registry connection found, now check for API connection
    cptr = jrmi_check_method_con(vptr, url_num);

    if(cptr){ // cptr found in current list going to reuse that
      NSDL1_HTTP(vptr, NULL, "Registry cptr found. API cptr also found going to start api calls on this cptr");
      cptr->conn_link = reg_cptr;
      SET_URL_NUM_IN_CPTR(cptr, url_num); // Set new url_num so that it can be used at setup of API connection
      cptr->proto_state = ST_JRMI_NSTATIC_CALL; // we will start by sending API xml as we have rmi handshake already done on this connection
      make_jrmi_msg(cptr, now);
    } else { //cptr not found for API method, now we will use registry cptr to send static xml and get port for that method 
      NSDL1_HTTP(vptr, NULL, "Registry cptr found. API cptr not found. Going to send static xml on registry cptr");
      reg_cptr->proto_state = ST_JRMI_STATIC_CALL; // Now we will use reg_cptr to get port for new method 
      SET_URL_NUM_IN_CPTR(reg_cptr, url_num); // Set new url_num so that it can be used at setup of API connection
      make_jrmi_msg(reg_cptr, now);
    } 
  }
}

int ns_parse_jrmi(char *api_name, char *api_to_run, FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
   int url_idx;  
   int jrmi_server_flag;
   int jrmi_method_flag;
   int protocol_flag;
   int body_begin_flag;
   int jrmi_port_flag;
   int jrmi_object_flag;
   int jrmi_number_flag;
   int jrmi_time_flag;
   int jrmi_count_flag; 
   int jrmi_method_hash_flag;  
   int jrmi_operation_hash_flag;  
   char *start_quotes;
   //char *starting_quotes;
   char *close_quotes;
   char pagename[MAX_LINE_LENGTH + 1];
   char attribute_name[128 + 1];
   char attribute_value[MAX_LINE_LENGTH + 1];
   int ret;
   char *page_end_ptr;

   NSDL2_JRMI(NULL, NULL,"method called ");

   NSDL2_JRMI(NULL, NULL, "Method Called. File: %s", flow_filename);
 
   jrmi_server_flag = jrmi_method_flag = protocol_flag = body_begin_flag = jrmi_port_flag = jrmi_object_flag = jrmi_number_flag = jrmi_time_flag = jrmi_count_flag = jrmi_method_hash_flag =  jrmi_operation_hash_flag = 0;  

   if (create_requests_table_entry(&url_idx) != SUCCESS) // Fill request type inside create table entry
   {
     SCRIPT_PARSE_ERROR(NULL, "get_url_requets(): Could not create jrmi request entry while parsing line %d in file %s\n", script_ln_no, flow_filename);
   }
 
   proto_based_init(url_idx, JRMI_REQUEST);

   init_post_buf();

   NSDL2_JRMI(NULL, NULL, "url_idx = %d, total_request_entries = %d, total_jrmi_request_entries = %d",
                          url_idx, total_request_entries, total_jrmi_request_entries); 

   ret = extract_pagename(flow_fp, flow_filename, script_line, pagename, &page_end_ptr);
   if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

   // For JRMI, we are internally using ns_web_url API

   if((parse_and_set_pagename(api_name, api_to_run, flow_fp, flow_filename, script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

   gPageTable[g_cur_page].first_eurl = url_idx;
   gPageTable[g_cur_page].num_eurls++; // Increment urls

   close_quotes = page_end_ptr;
   start_quotes = NULL;
   
   ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
  
   if(ret == NS_PARSE_SCRIPT_ERROR)
   {
     SCRIPT_PARSE_ERROR(script_line, "Syntax error");
     return NS_PARSE_SCRIPT_ERROR;
   }

   strcpy(requests[url_idx].proto.jrmi.method,"M1");
   requests[url_idx].proto.jrmi.jrmi_protocol = STREAM;

   NSDL2_JRMI(NULL,NULL,"JRMI: Value of method %s", requests[url_idx].proto.jrmi.method);
   while (1)
   {
     ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);
     if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;
      
     if (!strcmp(attribute_name, "RMI_SERVER"))             //Parameterization is not allowed for this argument 
     {
       if(jrmi_server_flag) 
       { 
         NSDL2_JRMI(NULL,NULL,"value of jrmi_server_flag [%d] ",jrmi_server_flag); 
         SCRIPT_PARSE_ERROR(NULL, "RMI_SERVER can be given once.");
       }

       jrmi_server_flag = 1;
       requests[url_idx].index.svr_idx = get_server_idx(attribute_value, requests[url_idx].request_type, script_ln_no);
       NSDL2_JRMI(NULL, NULL, "JRMI: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].index.svr_idx);

       /*Added for filling all server in gSessionTable*/
       CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from ns_parse_jrmi");
     }
     else if (!strcasecmp(attribute_name, "METHOD"))             //Parameterization is  allowed for this argument 
     {
       if(jrmi_method_flag) 
       { 
         NSDL2_JRMI(NULL,NULL,"value of jrmi_method_flag [%d] ",jrmi_method_flag); 
         SCRIPT_PARSE_ERROR(NULL, "RMI_METHOD can be given once.");
       }

       jrmi_method_flag = 1;
       strcpy(requests[url_idx].proto.jrmi.method, attribute_value);
       //segment_line(&(requests[url_idx].proto.jrmi.method), attribute_value, 1, script_ln_no, sess_idx, "");         
       NSDL2_JRMI(NULL,NULL,"JRMI: Value of %s = %s", attribute_name, requests[url_idx].proto.jrmi.method);  
 
     } else if (!strcasecmp(attribute_name, "PORT")){             //Parameterization is not allowed for this argument 
       if(jrmi_port_flag) 
       { 
         NSDL2_JRMI(NULL,NULL,"value of port_flag [%d] ",jrmi_port_flag); 
         SCRIPT_PARSE_ERROR(NULL, "PORT can be given once.");
       }

       jrmi_port_flag = 1;
       int tmp_port = atoi(attribute_value);
       if(tmp_port <= 0){
         SCRIPT_PARSE_ERROR(NULL, "PORT should be greater than 0");
       }
       requests[url_idx].proto.jrmi.port = tmp_port;
       NSDL2_JRMI(NULL, NULL, "JRMI: Value of %s = %d ", attribute_name, requests[url_idx].proto.jrmi.port);
     }
     else if (!strcasecmp(attribute_name, "ObjectId.ObjectNum"))
     {
       if(jrmi_object_flag)
       {
         NSDL2_JRMI(NULL, NULL ,"value of jrmi_object_flag [%d] ",jrmi_object_flag);
         SCRIPT_PARSE_ERROR(NULL,"ObjectId.ObjectNum can be given once");
       }
       
       jrmi_object_flag = 1;
       segment_line(&(requests[url_idx].proto.jrmi.object_id), attribute_value, 0, script_ln_no, sess_idx, "");
       NSDL2_JRMI(NULL, NULL, "JRMI: Value of object id  %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.jrmi.object_id);
     }
     else if (!strcasecmp(attribute_name, "ObjectId.UId.Number"))
     {
       if(jrmi_number_flag)
       {
         NSDL2_JRMI(NULL, NULL ,"value of jrmi_number_flag [%d] ",jrmi_number_flag);
         SCRIPT_PARSE_ERROR(NULL,"ObjectId.UId.Number can be given once");
       }
       
       NSDL2_JRMI(NULL, NULL ,"value of jrmi_number_flag [%d] ",jrmi_number_flag);
       jrmi_number_flag = 1;
       segment_line(&(requests[url_idx].proto.jrmi.number), attribute_value, 0, script_ln_no, sess_idx, "");
       NSDL2_JRMI(NULL, NULL, "JRMI: Value of number %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.jrmi.number);
     }
     else if (!strcasecmp(attribute_name, "ObjectId.UId.Count"))
     {
       if(jrmi_count_flag)
       {
         NSDL2_JRMI(NULL, NULL ,"value of jrmi_count_flag [%d] ",jrmi_count_flag);
         SCRIPT_PARSE_ERROR(NULL,"ObjectId.UId.Count can be given once");
       }
       
       jrmi_count_flag = 1;
       segment_line(&(requests[url_idx].proto.jrmi.count), attribute_value, 0, script_ln_no, sess_idx, "");
       NSDL2_JRMI(NULL, NULL, "JRMI: Value of %s = %s, count = %d", attribute_name, attribute_value, requests[url_idx].proto.jrmi.count);
    }
    else if (!strcasecmp(attribute_name, "ObjectId.UId.Time"))
    {
       if(jrmi_time_flag)
       {
         NSDL2_JRMI(NULL, NULL ,"value of jrmi_time_flag [%d] ",jrmi_time_flag);
         SCRIPT_PARSE_ERROR(NULL,"ObjectId.UId.Time can be given once");
       }

       jrmi_time_flag = 1;
       segment_line(&(requests[url_idx].proto.jrmi.time), attribute_value, 0, script_ln_no, sess_idx, "");
       NSDL2_JRMI(NULL, NULL, "JRMI: Value of  %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.jrmi.time);
    }
    else if (!strcasecmp(attribute_name, "MethodHash"))
    {
       if (jrmi_method_hash_flag)
       {
         NSDL2_JRMI(NULL, NULL ,"value of jrmi_method_hash_flag [%d] ",jrmi_method_hash_flag);
         SCRIPT_PARSE_ERROR(NULL,"METHOD_HASH can be given once");
       }

       jrmi_method_hash_flag = 1;
       segment_line(&(requests[url_idx].proto.jrmi.method_hash), attribute_value, 0, script_ln_no, sess_idx, "");
       NSDL2_JRMI(NULL, NULL ," requests[url_idx].proto.jrmi.method_hash =[%d] ",requests[url_idx].proto.jrmi.method_hash.num_entries);
    }
    else if (!strcasecmp(attribute_name, "Operation"))
    {
       if (jrmi_operation_hash_flag)
       {
         NSDL2_JRMI(NULL, NULL ,"value of jrmi_operation_hash_flag [%d] ",jrmi_operation_hash_flag);
         SCRIPT_PARSE_ERROR(NULL,"Operation can be given once");
       }

       jrmi_operation_hash_flag = 1;
       segment_line(&(requests[url_idx].proto.jrmi.operation), attribute_value, 0, script_ln_no, sess_idx, "");
       NSDL2_JRMI(NULL, NULL ," requests[url_idx].proto.jrmi.operation =[%d] ",requests[url_idx].proto.jrmi.operation.num_entries);
    }
    else if (!strcasecmp(attribute_name, "PROTOCOL"))
     {
       if (protocol_flag)
       { 
         NSDL2_JRMI(NULL,NULL,"value of jrmi_method_flag [%d] ",protocol_flag);
         SCRIPT_PARSE_ERROR(NULL, "RMI_PROTOCOL can be given once.");
       }
       protocol_flag = 1;

       if (!strcasecmp(attribute_value, "SINGLE"))
         requests[url_idx].proto.jrmi.jrmi_protocol = SINGLE;
       else  if (!strcasecmp(attribute_value, "STREAM"))
         requests[url_idx].proto.jrmi.jrmi_protocol = STREAM;
       else if (!strcasecmp(attribute_value, "MULTIPLEX"))
         requests[url_idx].proto.jrmi.jrmi_protocol = MULTIPLEX;
       else{
          SCRIPT_PARSE_ERROR(NULL, "Unexpected option for PROTOCOL option can be either ,SINGLE,STREAM,MULTIPLEX");
       }

       NSDL2_JRMI(NULL, NULL, "JRMI: Value of %s = %s, for PROTOCOL = %d", attribute_name, attribute_value, requests[url_idx].proto.jrmi.jrmi_protocol);
      }
      else if (!strcmp(attribute_name,"BODY"))
      {
        if (body_begin_flag)
        {
          NSDL2_JRMI(NULL,NULL,"value of jrmi_method_flag [%d] ",body_begin_flag);
          SCRIPT_PARSE_ERROR(NULL, "Body can be given once .");
        }
        body_begin_flag = 1;

        if (extract_body(flow_fp, script_line, &close_quotes, 0, flow_filename, 0, outfp) == NS_PARSE_SCRIPT_ERROR)
          return NS_PARSE_SCRIPT_ERROR;

        if (set_body(url_idx, sess_idx,  page_end_ptr, close_quotes, flow_filename) == NS_PARSE_SCRIPT_ERROR)
          return NS_PARSE_SCRIPT_ERROR;

        CLEAR_WHITE_SPACE(script_line);
        //starting_quotes = script_line;
        NSDL2_JRMI(NULL, NULL, "starting quotes = %s", script_line);
  
        break;
      }
      
      ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
      if (ret == NS_PARSE_SCRIPT_ERROR)
      {
        NSDL2_JRMI(NULL, NULL, "Next attribute is not found");
        break;
      }
   } //end of while 
       
  if(start_quotes == NULL)
  {
    SCRIPT_PARSE_ERROR(script_line, "start_quotes point to NULL");
  }

  // validate all arguments 
  if(!jrmi_server_flag) {
     SCRIPT_PARSE_ERROR(NULL, "RMI_SERVER must be given for RMI SESSION for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
 if( !jrmi_time_flag){
     SCRIPT_PARSE_ERROR(NULL, "PROTOCOL must be given for RMI SESSION for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  return 0;
}

static char *map_jnvm_jrmi_resp_file(int size, char *res_file){

  char *mp = NULL;
  int fd = -1;
//struct stat st;
  int file_size;

  NSDL2_API(NULL, NULL, "Method called"); 
 
 /* if(stat(res_file, &st) == -1){
    fprintf(stderr, "Unable to stat jrmi response file [%s], error=[%s]\n", res_file, nslb_strerror(errno));
    end_test_run();
  }*/

  jrmiRepLength = file_size = size;
  
  fd = open(res_file, O_CREAT|O_RDWR|O_CLOEXEC,S_IRWXU);
  if(fd == -1){
    NSDL2_API(NULL, NULL, "Unable to open file = [%s] error=[%s]", res_file, nslb_strerror(errno));
    end_test_run();
  }

  //printf("page size = %ld\n", sysconf(_SC_PAGE_SIZE));
  mp = (char *)mmap(NULL, file_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  if((void *)mp == MAP_FAILED)
  {
    NSDL2_API(NULL, NULL, "error in mapping file=[%s] Error = %s\n", res_file, nslb_strerror(errno));
    end_test_run();
  }
  NSDL2_API(NULL, NULL, "fd = [%d] length = [%d] mapped address = [%p]", fd, jrmiRepLength, mp);  
  return mp;
}

int process_njvm_resp_buffer(Msg_com_con *mccptr, char* page_name, int status, VUser* vptr, int new){

  u_ns_ts_t now  = 0;
  //u_ns_ts_t download_time;
  char res_file[1024];
  struct stat st;

  NSDL2_API(NULL, NULL, "Method called, vptr->vuser_state = %hd", vptr->vuser_state);

  SAVE_REQ_REP_FILES

  sprintf(res_file, "%s/logs/%s/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst,  vptr->page_instance,
          vptr->group_num,GET_SESS_ID_BY_NAME(vptr),
           (GET_PAGE_ID_BY_NAME(vptr) - vptr->sess_ptr->first_page->page_id));

   NSDL2_JRMI(NULL, NULL, "response file is res_file[%s] ", res_file);
 
  if(new > -1){
    vptr->next_pg_id = new;
    now = get_ms_stamp();
    vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
    vptr->flags |= NS_JNVM_JRMI_RESP;

    on_page_start(vptr, now);
  }

  if(page_name){
    if(strcmp(page_name,  vptr->cur_page->page_name)){
      NSDL2_API(NULL, NULL, "WARNING: Proper match not found for start step (%s) and end step (%s)", page_name, vptr->cur_page->page_name);
    }
  }

  connection* cptr = vptr->last_cptr;
  action_request_Shr *url_num = vptr->first_page_url;

/*  sprintf(res_file, "%s/logs/%s/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));*/



  if(stat(res_file, &st) == -1){
    NSDL2_JRMI(vptr, NULL, "Unable to stat jrmi response file [%s], error=[%s]\n", res_file, nslb_strerror(errno));
    NS_EL_2_ATTR(EID_FOR_API,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__,
               (char*)__FUNCTION__,
               "Warning:Unable to stat jrmi response file  =  (%s) error = [%s] ", res_file, nslb_strerror(errno));

    //fprintf(stderr, "Unable to stat jrmi response file [%s], error=[%s]\n", res_file, nslb_strerror(errno));
    //end_test_run();
    return -1;
  }

  if((!access((const char*)res_file, F_OK)) && (st.st_size)){
    jrmiRepPointer = map_jnvm_jrmi_resp_file(st.st_size, res_file);

    cptr = get_free_connection_slot(vptr);
    NSDL2_JRMI(vptr, NULL, "After get_free_connection_slot cptr=%p , mapped address=[%p] content=[%s]", cptr, jrmiRepPointer, jrmiRepPointer);

    vptr->last_cptr = cptr; // Set it so that in API we can get cptr

    SET_URL_NUM_IN_CPTR(cptr, url_num);
    cptr->url_num->request_type = JNVM_JRMI_REQUEST;
    cptr->request_type = JNVM_JRMI_REQUEST;
    cptr->num_retries = 0;

    // To init used_param table for page dump and user trace 
    init_trace_up_t(vptr); 

    reset_cptr_attributes(cptr);
    vptr->page_status = cptr->req_ok = status;

    cptr->bytes = jrmiRepLength;
    vptr->url_num = url_num;

    if(status != NS_REQUEST_OK) //will never go in this case, later we have to check for status
      abort_page_based_on_type(cptr, vptr, MAIN_URL, 0, status);
 
    now = get_ms_stamp();
 
    cptr->ns_component_start_time_stamp = now;
/*   download_time = now - vptr->pg_begin_at;
 
   if(NS_IF_TRACING_ENABLE_FOR_USER || NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL){
      NSDL2_JRMI(vptr, NULL, "Method called, User tracing enabled");
      ut_update_page_values(vptr, download_time, now);
   }
 */
   NSDL2_JRMI(vptr, NULL, "page_begin_at=[%lld], now=[%lld]", vptr->pg_begin_at, now);
   handle_page_complete(cptr, vptr, 1, now, cptr->url_num->request_type);
    
   free_connection_slot(cptr, now);
  }else{
    NSDL2_JRMI(vptr, NULL, "File not found. File name = %s", res_file);
     NS_EL_2_ATTR(EID_FOR_API,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__,
               (char*)__FUNCTION__,
               "Warning:File not found . File name =  (%s)",res_file);
    return -1;
  }
  return 0;
}

