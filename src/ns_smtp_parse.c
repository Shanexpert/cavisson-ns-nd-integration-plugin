/************************************************************************************
 * Name            : ns_smtp_parse.c 
 * Purpose         : This file contains all the smtp parsing related function of netstorm
 * Initial Version : Wednesday, January 06 2010 
 * Modification    : -
 ***********************************************************************************/


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
#include "ns_smtp_parse.h"
#include "ns_smtp.h"
#include "ns_script_parse.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

#define MAX_EMAIL_IDS 512

#define FLG_TO_EMAIL   0
#define FLG_CC_EMAIL   1
#define FLG_BCC_EMAIL  2
#define FLG_BODY       3
#define FLG_ATTACHMENT 4

char attachment_boundary[64];
char attachment_end_boundary[64];
char smtp_body_hdr_begin[1024 * 2];
int smtp_body_hdr_begin_len;

#ifndef CAV_MAIN
extern int cur_post_buf_len;
#else
extern __thread int cur_post_buf_len;
#endif

// KEYWORD GROUP VALUE
int kw_set_smtp_timeout(char *buf, int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;
  int num_args;

  num_args = sscanf(buf, "%s %s %d", keyword, grp, &num_value);

  if(num_args != 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SMTP_TIMEOUT_USAGE, CAV_ERR_1011121, CAV_ERR_MSG_1);
  }

  if (num_value <= 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SMTP_TIMEOUT_USAGE, CAV_ERR_1011121, CAV_ERR_MSG_9);
  }

  *to_change = num_value;
  return 0;
}

// Reads CAVINCLUDE file & copy to post buf
static void read_body_file_and_copy_to_post_buf(char *fname)
{
  int fd;
  int rlen;
  char fbuf[8192] = "\0";

  NSDL2_SMTP(NULL, NULL, "Method Called, fname = %s", fname);

  fd = open (fname, O_RDONLY|O_CLOEXEC);
  if (!fd) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, fname, errno, nslb_strerror(errno));
  }


  while (1) {
   rlen = read (fd, fbuf, 8192);
   if (rlen > 0) {
     if (copy_to_post_buf(fbuf, rlen)) {
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012239_ID, CAV_ERR_1012239_MSG, fname);
     }
     continue;
   } else if (rlen == 0) {
     break;
     } else {
	//perror("reading CAVINCLUDE BODY");
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000020]: ", CAV_ERR_1000020 + CAV_ERR_HDR_LEN, fname);
     }
  }
  close (fd);
}

static int create_and_copy_body_hdr(char *cap_fname, int *line_num)
{
  char smtp_body_hdr[MAX_LINE_LENGTH]; // headers + body

  NSDL2_SMTP(NULL, NULL,"Methos called, cap_fname = %s, line_num = %d", cap_fname, *line_num);
  sprintf(smtp_body_hdr,
   "--%s\r\n"
   "Content-Type: text/plain;\r\n"
   "Content-Transfer-Encoding: 7 bit\r\n\r\n",
   attachment_boundary);

  if (copy_to_post_buf(smtp_body_hdr, strlen(smtp_body_hdr))) {
       printf("Get_Url_Options(): Request BODY is too big at line=%d file %s\n", *line_num, cap_fname);
       return -1;
  }

  return 0;
} 


// Create attachment header
static void create_and_copy_attachment_hdr(char *fname, char *content)
{
  char attachment_hdr[2048];
  char content_type_hdr[512];
  char *file_name = basename(fname);

  NSDL2_SMTP(NULL, NULL, "Method Called, File = %s", fname);

  if(!file_name) {
    NS_EXIT(-1, "Unable to get file name from path '%s'\n", fname);
  }

/*  if(strstr(content, "-1"))
    strcpy(content_type_hdr, "Content-Type: text/plain;\r\n");
  else */
    sprintf(content_type_hdr, "Content-Type: %s;\r\n", content);

  sprintf(attachment_hdr,
  "--%s\r\n"
  "Content-Transfer-Encoding: base64\r\n%s"
  "    name=%s\r\n"
  "Content-Disposition: attachment;\r\n"
  "    filename=%s\r\n\r\n",
  attachment_boundary, content_type_hdr, file_name, file_name);

  NSDL3_SMTP(NULL, NULL, "File = %s, attachment_hdr = %s", fname, attachment_hdr);

  if (copy_to_post_buf(attachment_hdr, strlen(attachment_hdr))) {
    NS_EXIT(-1, "smtp_post_process_post_buf(): Unable to allocate memory for attachment headers for file %s\n", fname);
  }
}

/**
static void create_and_copy_attachment_footer(char *fname)
{
  char attachment_footer[1024];
  char *file_name = basename(fname);
  int attachment_footer_len;

  attachment_footer_len = sprintf(attachment_footer, "--%s--\r\n", ATTACHMENT_BOUNDARY);

  if (copy_to_post_buf(attachment_footer, attachment_footer_len)) {
    printf("smtp_post_process_post_buf(): Unable to allocate memory for attachment footer for file %s\n", file_name);
    exit (-1);
  }
}
*/

// Base 64 encodeing for attachment files
static void encode_attachment_and_copy_to_post_buf(char *infile)
{
  FILE *infile_fp;
  unsigned char in[3], out[4];
  int i, len, blocksout = 0;
  struct stat buf;
  

  int linesize = B64_DEF_LINE_SIZE;

  NSDL2_SMTP(NULL, NULL, "Method Called, infile = %s", infile);

  infile_fp = fopen( infile, "rb" );

  if(stat(infile, &buf)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000038]: ", CAV_ERR_1000038 + CAV_ERR_HDR_LEN, infile, errno, nslb_strerror(errno));
  }

  if (!S_ISREG(buf.st_mode)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000045]: ", CAV_ERR_1000045 + CAV_ERR_HDR_LEN, infile);
  }
 
  if(infile_fp == NULL) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000006]: ", CAV_ERR_1000006 + CAV_ERR_HDR_LEN, infile, errno, nslb_strerror(errno));
  }
 
  while( !feof( infile_fp ) ) {
      len = 0;
      for( i = 0; i < 3; i++ ) {
          in[i] = (unsigned char) getc( infile_fp );
          if( !feof( infile_fp ) ) {
            len++;
          }
          else {
              in[i] = 0;
          }
      }
      if( len ) {
          encodeblock( in, out, len );

          if (copy_to_post_buf((char *)out, 4)) {
             SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012239_ID, CAV_ERR_1012239_MSG, infile);
          }
          
          blocksout++;
      }
      if( blocksout >= (linesize/4) || feof( infile_fp ) ) {
          if( blocksout ) {
            if (copy_to_post_buf(SMTP_CMD_CRLF, strlen(SMTP_CMD_CRLF))) {
              SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012239_ID, CAV_ERR_1012239_MSG, infile);
            }
          }
          blocksout = 0;
      }
  }

  fclose(infile_fp);
}

static void smtp_post_process_post_buf(int req_index, int sess_idx, int * line_number, char *cap_fname, int who, char *content_type)
{
   char *fname, fbuf[8192];
   int rnum, noparam_flag = 0;
   int rand_bytes_min, rand_bytes_max;

   // it is used for body header
   // Here is 2 case body is read from file or inline body

    NSDL2_SMTP(NULL, NULL, "Method Called, req_index = %d, line_number %d, who = %d, cur_post_buf_len = %d, File = %s",
                                           req_index, *line_number, who, cur_post_buf_len, cap_fname);

    if (cur_post_buf_len <= 0) return; //No BODY, exit

    //Check if BODY is provided using $CAVINCLUDE$= directive
    if ((strncasecmp (g_post_buf, "$CAVINCLUDE$=", 13) == 0) ||  (strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0) || who == FLG_ATTACHMENT) {

     if(strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)
     {
        fname = g_post_buf + 21;
        noparam_flag = 1;
     }
     else if(strncasecmp (g_post_buf, "$CAVINCLUDE$=", 13) == 0)
       fname = g_post_buf + 13;
     else if(who == FLG_ATTACHMENT) { // Attachments
       fname = g_post_buf;  // Its a Attachment
       noparam_flag = 1;
     }

     if (fname[0] != '/') {
       /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
       sprintf (fbuf, "%s/%s/%s", GET_NS_TA_DIR(),
 	     get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), fname);
             //Previously taking with only script name
 	     //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), fname);
       fname = fbuf;
     } else {
       sprintf (fbuf, "%s", fname);
       NSDL2_SMTP(NULL, NULL, "cavinclude file name = %s", fbuf);
       fname = fbuf;
     }

     NSDL2_SMTP(NULL, NULL, "fname = %s", fname);
     // It removes file name from buffer
     init_post_buf();

     if(who ==  FLG_ATTACHMENT) {// if Attachment is then make & copy attachment hdr 
        create_and_copy_attachment_hdr(fname, content_type);
        encode_attachment_and_copy_to_post_buf(fname);
        //create_and_copy_attachment_footer(fname);
     }
     else if (who == FLG_BODY){ // body
        create_and_copy_body_hdr(cap_fname, line_number);
        read_body_file_and_copy_to_post_buf(fname);
     }
   }
    
    if (strncasecmp (g_post_buf, "$CAVRANDOM_BYTES$=", 18) == 0) {
      rand_bytes_min = atoi(g_post_buf+18);
      rand_bytes_max = atoi(strstr(g_post_buf+18, ",") + 1);
      
      NSDL2_FTP(NULL, NULL, "rand_min = %d, rand_max = %d", rand_bytes_min, rand_bytes_max);
      if (rand_bytes_max < rand_bytes_min) {
        NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "CAVRANDOM_BYTES: max can not be less than min");
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012221_ID, CAV_ERR_1012221_MSG);
      }
      if (rand_bytes_min <= 0) {
        NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "CAVRANDOM_BYTES: min can not be less than or equal to 0");
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012222_ID, CAV_ERR_1012222_MSG);
      }

      requests[req_index].proto.smtp.enable_rand_bytes = 1;
      requests[req_index].proto.smtp.rand_bytes_min = rand_bytes_min;
      requests[req_index].proto.smtp.rand_bytes_max = rand_bytes_max;

      return;
    }

   if (!cur_post_buf_len)  return;

   create_post_table_entry(&rnum);

   if(who == FLG_BODY)
     requests[req_index].proto.smtp.body_idx = rnum;  // Its a body idx for SMTP
   else if(who == FLG_ATTACHMENT && requests[req_index].proto.smtp.attachment_idx == -1)
     requests[req_index].proto.smtp.attachment_idx = rnum;  // its a first attachment idx for SMTP
   else if(who == FLG_TO_EMAIL && requests[req_index].proto.smtp.to_emails_idx == -1) {
     noparam_flag = 0;
     requests[req_index].proto.smtp.to_emails_idx = rnum;
   }
   else if(who == FLG_CC_EMAIL && requests[req_index].proto.smtp.cc_emails_idx == -1) {
     requests[req_index].proto.smtp.cc_emails_idx = rnum;
     noparam_flag = 0;
   }
   else if(who == FLG_BCC_EMAIL && requests[req_index].proto.smtp.bcc_emails_idx == -1) {
     requests[req_index].proto.smtp.bcc_emails_idx = rnum;
     noparam_flag = 0;
   }

   if (noparam_flag)
     segment_line_noparam(&postTable[rnum], g_post_buf, sess_idx);
   else
     segment_line(&postTable[rnum], g_post_buf, 0, *line_number, sess_idx, cap_fname);
}


// Tokinize the email ids by ',' & StrEnt  using segment_line
static short int save_emails(int req_index , char *headers, char *email_id,  int *line_number, int sess_idx, char *cap_fname, int who)
{
  char *fields[MAX_EMAIL_IDS]; 
  char temp_buf[1024];
  int num_ids, i; //, len;

  NSDL2_SMTP(NULL, NULL, "Method Called, request index = %d, headers = %s, who = %d, file = %s", req_index, headers, who, cap_fname);

  if(email_id[0] == '\0')
     return 0;
   
  //len = strlen(email_id);

  if(who == FLG_TO_EMAIL) {
    sprintf(headers, "%sTo: %s\r\n", headers, email_id);
  }
  else if(who == FLG_CC_EMAIL) {
    sprintf(headers, "%sCc: %s\r\n", headers, email_id);
  }

  num_ids = get_tokens(email_id, fields, ",", MAX_EMAIL_IDS);

  for(i = 0; i< num_ids; i++) {
    temp_buf[0] = '\0';
    sprintf(temp_buf, "RCPT TO: %s\r\n", fields[i]);
    NSDL3_SMTP(NULL, NULL, "i = %d, temp_buf = %s", i, temp_buf);

    init_post_buf();
    if (copy_to_post_buf(temp_buf, strlen(temp_buf))) {
       printf("parse_smtp_send(): Attachment is too big at line=%d file %s\n", *line_number, cap_fname);
       return -1;
    } 

    smtp_post_process_post_buf(req_index, sess_idx, line_number, cap_fname, who, NULL);
  }

  return num_ids;
}

#define MAX_EMAIL_ATTACHMENTS MAX_EMAIL_IDS

// Save attachment, even fields (0, 2, 4, 6) are attachment name & odd fields (1, 3, 5, 7) are MIME type.
static short int save_attachment(int req_index, char *attachment_fname_buf, int *line_number, int sess_idx, char *cap_fname)
{
  char *fields[MAX_EMAIL_ATTACHMENTS]; 
  int num_ids, i; //, len;
  char temp_buf[1024];
  int num_attach = 0;

  NSDL2_SMTP(NULL, NULL, "Method Called, request index = %d, attachment_fname_buf = %s, File = %s", req_index, attachment_fname_buf, cap_fname); 

  //len = strlen(attachment_fname_buf);

  num_ids = get_tokens(attachment_fname_buf, fields, ",", MAX_EMAIL_ATTACHMENTS);

  for(i = 0; i< num_ids; i = i + 2) {
    ++num_attach;
    temp_buf[0] = '\0';
    sprintf(temp_buf, "%s", fields[i]);
    NSDL3_SMTP(NULL, NULL, "i = %d, num_attach = %d, Attachment = %s", i, num_attach, temp_buf);

    init_post_buf();
    if (copy_to_post_buf(temp_buf, strlen(temp_buf))) {
       printf("parse_smtp_send(): Attachment is too big at line=%d file %s\n", *line_number, cap_fname);
       return -1;
    } 

    smtp_post_process_post_buf(req_index, sess_idx, line_number, cap_fname, FLG_ATTACHMENT, fields[i + 1]); // 0 for attachment
  }

  return num_attach;
}

/* This method validate , as last char of line.
 * If ); found in the line then mark fn_end as true.
 */
int search_comma_as_last_char(char *ptr, int *fn_end)
{
   CLEAR_WHITE_SPACE_FROM_END(ptr);
   int len = strlen(ptr);

   NSDL2_SMTP(NULL, NULL, "Method Called, ptr = %s", ptr); 

   if(ptr[len - 1] == ',') {
     //ptr[len] = '\0';
     return 0;
   } else if ((ptr[len - 1] == ';' && ptr[len - 2 ] == ')')) {
     ptr[len - 2 ] = ',';
     ptr[len - 1 ] = '\0';
     *fn_end = 1;
     return 0;
   } else {
     fprintf(stderr, "Expected ',' at the end of %s line.\n", ptr); 
     return 1;
   }
}

// Validate Passowrd, TO_EMAILS or FROM_EMAILS or BODY
static void validate_smtp_fields(char *cap_fname, int ii)
{
  NSDL2_SMTP(NULL, NULL, "Method Called, req index = %d, File = %s", ii, cap_fname); 

  if(requests[ii].proto.smtp.user_id.num_entries) {
    if(requests[ii].proto.smtp.passwd.num_entries == 0) {
     NS_EXIT(-1, "Password requered for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
    }
  }
 
  if(requests[ii].proto.smtp.num_to_emails == 0) {
    NS_EXIT(-1,"TO_EMAILS required for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if(requests[ii].proto.smtp.from_email.num_entries == 0) {
    NS_EXIT(-1,"FROM_EMAILS required for page %s", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if((requests[ii].proto.smtp.body_idx == -1) && 
     !(requests[ii].proto.smtp.enable_rand_bytes)) {
    NS_EXIT(-1, "BODY required for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
}

/* SMTP Send example
smtp_send ( send_email_invite,
      SMTP_SERVER=smtp.mydom.com, 
      USER_ID=rob.smith,
      HEADER=hjsdhf
      HEADER=dfkjgdfj
      PASSWORD=rpasswd,
      FROM_EMAIL=rob.smith@mydom.com, 
      TO_EMAILS=joe@gmail.com,shyam@yahoo.com, 
      CC_EMAILS=cindy@kdom.com,
      SUBJECT=Invitation for annual event, 
      BODY=$CAVINCLUDE$=invite_template_file.txt,  // Or it is inline single line
      ATTACHMENT=email/user/inviteEmail.pdf,application/pdf, 
      ATTACHMENT=email/resources/inviteConf.jpg,image/jpg
      MESSAGE_COUNT=50,100
    );
*/
int parse_smtp_send(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname) {

  int ii, len;
  int function_ends = 0;
  int body_flag, subject_flag, from_email_flag,to_email_flag, msg_count_flag, smtp_server_flag, user_flag, passwd_flag;
  char *line_ptr;
  char line[MAX_LINE_LENGTH];
  char subject[2048];
  char headers[MAX_LINE_LENGTH + MAX_LINE_LENGTH] = "\0";
  char to_emails_buf[MAX_LINE_LENGTH] = "\0";
  char cc_emails_buf[MAX_LINE_LENGTH] = "\0";
  char bcc_emails_buf[MAX_LINE_LENGTH] = "\0";
  char attachemnt_fname_buf[MAX_LINE_LENGTH] = "\0";
  char temp_buf[1024];
  char date_buf[64];
  char *tmp;
  time_t tloc;

  NSDL2_SMTP(NULL, NULL, "Method Called. File: %s", cap_fname);

  (void)time(&tloc);

  sprintf(attachment_boundary, "NetStormAttachmentTestidx-%d", testidx);
  sprintf(attachment_end_boundary, "\r\n--NetStormAttachmentTestidx-%d--", testidx);
  
  body_flag = subject_flag = from_email_flag = to_email_flag =  msg_count_flag = smtp_server_flag = user_flag = passwd_flag = 0;

  strcpy(date_buf, ctime(&tloc));

  if(date_buf[strlen(date_buf) - 1] == '\n')
     date_buf[strlen(date_buf) - 1] = '\0';
  
  sprintf(headers, "Date: %s\r\n", date_buf);

  NSDL2_SCHEDULE(NULL, NULL, "file:%s", cap_fname);

  if (create_requests_table_entry(&ii) != SUCCESS) {   // Fill request type inside create table entry
      NS_EXIT(-1, "get_url_requets(): Could not create smtp request entry while parsing line %d in file %s", *line_num, cap_fname);
  }
  proto_based_init(ii, SMTP_REQUEST);

  NSDL2_SMTP(NULL, NULL, "ii = %d, total_request_entries = %d, total_smtp_request_entries = %d",
                          ii, total_request_entries, total_smtp_request_entries);

  while (nslb_fgets(line, MAX_LINE_LENGTH, cap_fp, 1)) {
    NSDL3_SMTP(NULL, NULL, "line = %s", line);

    (*line_num)++;
    line_ptr = line;

    CLEAR_WHITE_SPACE(line_ptr);
    IGNORE_COMMENTS(line_ptr);
   
    if (*line_ptr == '\n')
     continue;

    /* remove the newline character from end of line. */
    if (strchr(line_ptr, '\n')) {
      if (strlen(line_ptr) > 0)
        line_ptr[strlen(line_ptr) - 1] = '\0';
    }

    if(search_comma_as_last_char(line_ptr, &function_ends))
      NS_EXIT(-1, "search_comma_as_last_char() failed");

    if(function_ends && line_ptr[0] == ',')
      break;

    if (!strncmp(line_ptr, "ATTACHMENT", strlen("ATTACHMENT"))) {  // Parametrization = No
      line_ptr += strlen("ATTACHMENT");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after ATTACHMENT at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);

      // Nothing is given in ATTACHMENT
      if(line_ptr[0] == ',')
       continue;
 
      tmp = line_ptr;
      int num_fields = 0;

      // a,b,
      while((tmp = index(tmp, ',')) != NULL) {
        tmp++;
        num_fields++;
      }
      
      if(num_fields == 1) 
       sprintf(attachemnt_fname_buf, "%s%s%s", attachemnt_fname_buf, line_ptr, "text/plain;,");
      else if(num_fields == 2) 
       strcat(attachemnt_fname_buf, line_ptr);
      else {
         NS_EXIT(-1, "Invalid format at line %d", *line_num);
      }

    } else if (!strncmp(line_ptr, "SMTP_SERVER", strlen("SMTP_SERVER"))) {  // Parametrization = No
      if(smtp_server_flag) {
        NS_EXIT(-1, "SMTP_SERVER can be given once.");
      }
      smtp_server_flag = 1;
        
      line_ptr += strlen("SMTP_SERVER");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after SMTP_SERVER at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      requests[ii].index.svr_idx = get_server_idx(line_ptr, requests[ii].request_type, *line_num);

    } else if (!strncmp(line_ptr, "HEADER", strlen("HEADER"))) {  // Parametrization = No
      line_ptr += strlen("HEADER");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after HEADER at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      tmp = rindex(line_ptr, ',');
      *tmp = 0;

      if(line_ptr[0] != '\0')
        sprintf(headers, "%s%s\r\n", headers, line_ptr);

    } else if (!strncmp(line_ptr, "USER_ID", strlen("USER_ID"))) {  // Parametrization = Yes
      if(user_flag) {
        NS_EXIT(-1, "USER_ID can be given once.");
      }
      user_flag = 1;
      line_ptr += strlen("USER_ID");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after USER_ID at line %d.\n", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.smtp.user_id), line_ptr, 0, *line_num, sess_idx, cap_fname);

    } else if (!strncmp(line_ptr, "PASSWORD", strlen("PASSWORD"))) {  // Parametrization = Yes
      if(passwd_flag) {
        NS_EXIT(-1, "PASSWORD can be given once.");
      }
      passwd_flag = 1;
      line_ptr += strlen("PASSWORD");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after PASSWORD at line %d", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.smtp.passwd), line_ptr, 0, *line_num, sess_idx, cap_fname);

    } else if (!strncmp(line_ptr, "FROM_EMAIL", strlen("FROM_EMAIL"))) {  // Parametrization = Yes
      if(from_email_flag) {
        NS_EXIT(-1, "FROM_EMAIL can be given once.");
      }
      from_email_flag = 1;
      line_ptr += strlen("FROM_EMAIL");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after FROM_EMAIL at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      sprintf(headers, "%sFrom: %s\r\nMIME-Version: 1.0\r\n", headers, line_ptr);
      sprintf(temp_buf, "MAIL FROM: %s\r\n", line_ptr);
      segment_line(&(requests[ii].proto.smtp.from_email), temp_buf, 0, *line_num, sess_idx, cap_fname);

    } else if (!strncmp(line_ptr, "TO_EMAILS", strlen("TO_EMAILS"))) { // Parametrization = Yes
      
      if (to_email_flag)
        { 
          NS_EXIT(-1, "TO_EMAILS can not be given more than once");
          //script_parse_error(NULL,"TO_EMAILS can not be given more than once \n");
          
         }
      to_email_flag=1;
      len = strlen("TO_EMAILS");
      line_ptr += len;

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after TO_EMAILS at line %d", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      strcat(to_emails_buf, line_ptr);

    } else if (!strncmp(line_ptr, "CC_EMAILS", strlen("CC_EMAILS"))) { // Parametrization = Yes
      len = strlen("CC_EMAILS");
      line_ptr += len;

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         fprintf(stderr, "= expected after CC_EMAILS at line %d.\n", *line_num);
         NS_EXIT(-1, "= expected after CC_EMAILS at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      if(line_ptr[0] != ',')
        strcat(cc_emails_buf, line_ptr);

    } else if (!strncmp(line_ptr, "BCC_EMAILS", strlen("BCC_EMAILS"))) {  // Parametrization = Yes
      len = strlen("BCC_EMAILS");
      line_ptr += len;

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after BCC_EMAILS at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);

      strcat(bcc_emails_buf, line_ptr);

    } else if (!strncmp(line_ptr, "SUBJECT", strlen("SUBJECT"))) { // Parametrization = Yes
      if(subject_flag) {
        NS_EXIT(-1, "SUBJECT can be given once");
      }
      subject_flag = 1;
      len = strlen("SUBJECT");
      line_ptr += len;

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after SUBJECT at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);

      tmp = rindex(line_ptr, ',');
      if(tmp)
        *tmp = 0;
 
      if(line_ptr[0] != '\0')
        sprintf(subject, "Subject: %s\r\n", line_ptr);

    } else if (!strncmp(line_ptr, "BODY", strlen("BODY"))) {  // Parametrization = Yes
      if(body_flag) {
        NS_EXIT(-1, "BODY can be given once.");
      }
      body_flag = 1;
      line_ptr += strlen("BODY");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after BODY at line %d", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';

      // it will clean the buf & index
      init_post_buf();

      // Copy header if CAVINCLUDE or CAVINCLUDE_NOPARAM not given, in case it is given the header is copied jsut before reading the file
      if ((strncasecmp (line_ptr, "$CAVINCLUDE$=", 13)) &&  (strncasecmp (line_ptr, "$CAVINCLUDE_NOPARAM$=", 21)) && 
          (strncasecmp (line_ptr, "$CAVRANDOM_BYTES$=", 18)))
        create_and_copy_body_hdr(cap_fname, line_num);

      if (copy_to_post_buf(line_ptr, strlen(line_ptr))) {
          NS_EXIT(-1, "Get_Url_Options(): Request BODY is too big at line=%d file %s", *line_num, cap_fname);
      }
      // Last Arg to identifiy it is called for BODY
      smtp_post_process_post_buf(ii, sess_idx, line_num, cap_fname, FLG_BODY, NULL);
    } else if (!strncmp(line_ptr, "MESSAGE_COUNT", strlen("MESSAGE_COUNT"))) { // Parametrization = Yes
      if(msg_count_flag) {
        NS_EXIT(-1, "MESSAGE_COUNT can be given once.");
      }
      msg_count_flag = 1;
      len = strlen("MESSAGE_COUNT");
      line_ptr += len;

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after MESSAGE_COUNT at line %d", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';  //last , removal

      int num_tokens;
      char *fields[4];

      num_tokens = get_tokens(line_ptr, fields, ",", 4);
  
      if(num_tokens < 1 || num_tokens > 2) {
        NS_EXIT(-1, "Either count allowed or min-count,max-count allowed.");
      }
 
      if(fields[0]) {
       requests[ii].proto.smtp.msg_count_min = requests[ii].proto.smtp.msg_count_max = atoi(fields[0]);
      }
        
      if(fields[1]) {
        requests[ii].proto.smtp.msg_count_max = atoi(fields[1]);
      }

      if(requests[ii].proto.smtp.msg_count_min <= 0 ||  requests[ii].proto.smtp.msg_count_max <= 0) {
         NS_EXIT(-1, "Min/Max value for message count must be greater then 0.");
      }

      if(requests[ii].proto.smtp.msg_count_min > requests[ii].proto.smtp.msg_count_max) {
         NS_EXIT(-1, "Min value for message count can not be greater then max valuse.");
      }

    } else {
      fprintf(stderr, "Line %d not expected\n", *line_num);
      return -1;
    }

    if(function_ends)
     break;
  }

  requests[ii].proto.smtp.num_to_emails   = save_emails(ii, headers, to_emails_buf, line_num, sess_idx, cap_fname, FLG_TO_EMAIL); // to emails
  requests[ii].proto.smtp.num_cc_emails   = save_emails(ii, headers, cc_emails_buf, line_num, sess_idx, cap_fname, FLG_CC_EMAIL); // cc   
  requests[ii].proto.smtp.num_bcc_emails  = save_emails(ii, headers, bcc_emails_buf, line_num, sess_idx, cap_fname, FLG_BCC_EMAIL); // bcc

  sprintf(headers, 
  "%s%sContent-Type: multipart/mixed;\r\n"     
  "    boundary=%s\r\n" 
  "Content-Disposition: inline\r\n",
  headers, subject, attachment_boundary);

  segment_line(&(requests[ii].proto.smtp.hdrs), headers, 0, *line_num, sess_idx, cap_fname);

  if((requests[ii].proto.smtp.num_attachments = save_attachment(ii, attachemnt_fname_buf, line_num, sess_idx, cap_fname)) < 0)
    NS_EXIT(-1, "function failed save attachment()");

  NSDL2_SMTP(NULL, NULL, "request index = %d, to_emails = %hd, cc_emails = %hd, bcc_emails = %hd, attachments = %hd", 
                          ii,
                          requests[ii].proto.smtp.num_to_emails,
                          requests[ii].proto.smtp.num_cc_emails,
                          requests[ii].proto.smtp.num_bcc_emails,
                          requests[ii].proto.smtp.num_attachments
                          );

  if(!smtp_server_flag) {
     NS_EXIT(-1, "SMTP_SERVER must be given for SMTP SESSION for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if(!function_ends) {
    fprintf(stderr, "End of function smtp_send not found\n");
    return -1;
  }

  gPageTable[g_cur_page].first_eurl = ii;

  validate_smtp_fields(cap_fname, ii);

  return 0;
}




int parse_ns_smtp_send(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx) 
{
  int url_idx;
  int body_flag, subject_flag, from_email_flag, to_email_flag, msg_count_flag, smtp_server_flag, user_flag, passwd_flag, starttls_flag;
  char subject[2048];
  char headers[MAX_LINE_LENGTH + MAX_LINE_LENGTH] = "\0";
  char to_emails_buf[MAX_LINE_LENGTH] = "\0";
  char cc_emails_buf[MAX_LINE_LENGTH] = "\0";
  char bcc_emails_buf[MAX_LINE_LENGTH] = "\0";
  char attachemnt_fname_buf[MAX_LINE_LENGTH] = "\0";
  char temp_buf[1024];
  char date_buf[64];
  char *tmp;
  time_t tloc;

  char *start_quotes;
  char *close_quotes;
  char *page_end_ptr;
  char pagename[MAX_LINE_LENGTH + 1];
  char attribute_name[MAX_LINE_LENGTH + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  int ret;

  NSDL2_SMTP(NULL, NULL, "Method Called. File: %s", flow_filename);

  (void)time(&tloc);

  sprintf(attachment_boundary, "NetStormAttachmentTestidx-%d", testidx);
  sprintf(attachment_end_boundary, "\r\n--NetStormAttachmentTestidx-%d--", testidx);
  
  body_flag = subject_flag = from_email_flag = msg_count_flag = smtp_server_flag = user_flag = passwd_flag = to_email_flag = starttls_flag = 0;

  strcpy(date_buf, ctime(&tloc));

  if(date_buf[strlen(date_buf) - 1] == '\n')
     date_buf[strlen(date_buf) - 1] = '\0';
  
  sprintf(headers, "Date: %s\r\n", date_buf);

  NSDL2_SCHEDULE(NULL, NULL, "file:%s", flow_filename);

  create_requests_table_entry(&url_idx);  // Fill request type inside create table entry

  proto_based_init(url_idx, SMTP_REQUEST);
  init_post_buf();

  NSDL2_SMTP(NULL, NULL, "index = %d, total_request_entries = %d, total_smtp_request_entries = %d",
                          url_idx, total_request_entries, total_smtp_request_entries);

  ret = extract_pagename(flow_fp, flow_filename, script_line, pagename, &page_end_ptr);
  if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;
  
 // For SMTP, we are internally using ns_web_url API
  if((parse_and_set_pagename("ns_smtp_send", "ns_web_url", flow_fp, flow_filename, script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  gPageTable[g_cur_page].first_eurl = url_idx;
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  close_quotes = page_end_ptr;
  start_quotes = NULL;

  ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
   
  //This will return if start quotes of next argument is not found or some other printable
  //is found including );
  if(ret == NS_PARSE_SCRIPT_ERROR)
  {
    SCRIPT_PARSE_ERROR(script_line, "Syntax error");
    return NS_PARSE_SCRIPT_ERROR;
  }

  while (1) 
  {
    NSDL3_SMTP(NULL, NULL, "line = %s", script_line);
    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 1);
    if(ret == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    if (!strcmp(attribute_name, "ATTACHMENT")) 
    {  // Parametrization = Not allowed for this argument
      tmp = attribute_value;
      int num_fields = 0;

      while((tmp = index(tmp, ',')) != NULL) 
      {
        tmp++;
        num_fields++;
      }
     
      if(num_fields == 0)
      {
        if(attribute_value == NULL) 
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012330_ID, CAV_ERR_1012330_MSG, "ATTACHMENT");
        } 
        sprintf(attachemnt_fname_buf, "%s%s,%s", attachemnt_fname_buf, attribute_value, "text/plain;,");
      }
      else
      {
        if(attribute_value[strlen(attribute_value)-1]==',')
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012070_ID, CAV_ERR_1012070_MSG);
        }  
        tmp = strtok(attribute_value,",");
        while(tmp)
        {
         // strcat(tmp,",text/plain;,");
          strcat(attachemnt_fname_buf, tmp);
          strcat(attachemnt_fname_buf,",text/plain;,");
          tmp=strtok(NULL,",");
        }
      }
     /* else 
      {
        SCRIPT_PARSE_ERROR(NULL, "Invalid format at line %d\n", script_ln_no);
      }*/
      NSDL2_SMTP(NULL, NULL, "SMTP: Value of %s = %s", attribute_name, attribute_value);
    } 

    else if (!strcmp(attribute_name, "SMTP_SERVER") || !strcmp(attribute_name, "SMTPS_SERVER")) 
    { // Parametrization = Not allowed for this argument

      if (!strcmp(attribute_name, "SMTP_SERVER")){
        proto_based_init(url_idx, SMTP_REQUEST); 
      }else{
        proto_based_init(url_idx, SMTPS_REQUEST);
      }

      if(smtp_server_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "SMTP_SERVER");
      }
      smtp_server_flag = 1;
        
      requests[url_idx].index.svr_idx = get_server_idx(attribute_value, requests[url_idx].request_type, script_ln_no);
      NSDL2_SMTP(NULL, NULL, "SMTP: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].index.svr_idx);

      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from parse_ns_smtp_send");

    } else if (!strcmp(attribute_name, "STARTTLS")){ // Parametrization is  not allowed for this argument
      if(starttls_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "STARTTLS");
      }
      if(!strcasecmp(attribute_value, "YES")){
        requests[url_idx].proto.smtp.authentication_type = 1;
        gServerTable[requests[url_idx].index.svr_idx].server_port = 587; 
      }else if(!strcasecmp(attribute_value, "NO")){
        requests[url_idx].proto.smtp.authentication_type = 0;
      }else{
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012220_ID, CAV_ERR_1012220_MSG, attribute_value, "STARTTLS", "YES", "NO");
      }
      starttls_flag = 1;
      NSDL2_IMAP(NULL, NULL,"Value of  %s = %s", attribute_value, attribute_name);
    } 
    //Header is Optional Argument
    else if (!strcmp(attribute_name, "HEADER")) 
    { // Parametrization = Not allowed for this argument
      sprintf(headers, "%s%s\r\n", headers, attribute_value);
      NSDL2_SMTP(NULL, NULL, "SMTP: Value of %s = %s", attribute_name, attribute_value);
    } 

    else if (!strcmp(attribute_name, "USER_ID")) 
    {  // Parametrization = allowed for this argument
      if(user_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "USER_ID");
      }
      user_flag = 1;
      segment_line(&(requests[url_idx].proto.smtp.user_id), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_SMTP(NULL, NULL, "SMTP: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.smtp.user_id);
    } 

    else if (!strcmp(attribute_name, "PASSWORD"))  
    { // Parametrization = allowed for this argument
      if(passwd_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "PASSWORD");
      }
      passwd_flag = 1;
      segment_line(&(requests[url_idx].proto.smtp.passwd), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_SMTP(NULL, NULL, "SMTP: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.smtp.passwd);
    }  

    else if (!strcmp(attribute_name, "FROM_EMAIL")) 
    {  // Parametrization = allowed for this argument
      if(from_email_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "FROM_EMAIL");
      }
      from_email_flag = 1;
      sprintf(headers, "%sFrom: %s\r\nMIME-Version: 1.0\r\n", headers, attribute_value);
      sprintf(temp_buf, "MAIL FROM: %s\r\n", attribute_value);
      segment_line(&(requests[url_idx].proto.smtp.from_email), temp_buf, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_SMTP(NULL, NULL, "SMTP: Value of %s = %s, svr_idx = %d", attribute_name, attribute_value, requests[url_idx].proto.smtp.from_email);
    } 

    else if (!strcmp(attribute_name, "TO_EMAILS")) 
    {  // Parametrization = allowed for this argument
      //TO__Email, CC_Email, Bcc_email  can  received in multiple lineis
      if(to_email_flag)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "TO_EMAILS");
      }
      to_email_flag = 1;
      strcat(to_emails_buf, attribute_value);
      NSDL2_SMTP(NULL, NULL,"SMTP: Value of  %s = %s", attribute_value, attribute_name);
    } 

    else if (!strcmp(attribute_name, "CC_EMAILS")) 
    { // Parametrization = allowed for this argument
      strcat(cc_emails_buf, attribute_value);
      NSDL2_SMTP(NULL, NULL,"SMTP: Value of  %s = %s", attribute_value, attribute_name);
    } 

    else if (!strcmp(attribute_name, "BCC_EMAILS")) 
    { // Parametrization = allowed for this argument
      strcat(bcc_emails_buf, attribute_value);
      NSDL2_SMTP(NULL, NULL,"SMTP: Value of  %s = %s", attribute_value, attribute_name);
    } 

    else if (!strcmp(attribute_name, "SUBJECT")) 
    { // Parametrization = allowed for this argument
      if(subject_flag)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "SUBJECT");
      }
      subject_flag = 1;
      sprintf(subject, "Subject: %s\r\n", attribute_value);
      NSDL2_SMTP(NULL, NULL,"SMTP: Value of  %s = %s", attribute_value, attribute_name);
    } 

    else if (!strcmp(attribute_name, "BODY"))
    { 
      // Parametrization = allowed for this argument
      init_post_buf();

      if(body_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "BODY");
      }
      body_flag = 1;
 
      // Copy header if CAVINCLUDE or CAVINCLUDE_NOPARAM not given, in case it is given the header is copied jsut before reading the file
      if ((strncasecmp (attribute_value, "$CAVINCLUDE$=", 13))  &&  (strncasecmp (attribute_value, "$CAVINCLUDE_NOPARAM$=", 21))  && 
          (strncasecmp (attribute_value, "$CAVRANDOM_BYTES$=", 18))){
        create_and_copy_body_hdr(flow_filename, &script_ln_no);
      }
      if (copy_to_post_buf(attribute_value, strlen(attribute_value))) {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012235_ID, CAV_ERR_1012235_MSG, flow_filename);
      }
        // Last Arg to identifiy it is called for BODY
      smtp_post_process_post_buf(url_idx, sess_idx, &script_ln_no, flow_filename, FLG_BODY, NULL);
      NSDL2_SMTP(NULL, NULL,"SMTP: Value of  %s = %s", attribute_value, attribute_name);
    } 
  
    else if (!strcmp(attribute_name, "MESSAGE_COUNT"))
    { // Parametrization = allowed for this argument
      if(msg_count_flag) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "MESSAGE_COUNT");
      }
      msg_count_flag = 1;
      int num_tokens;
      char *fields[4];

      num_tokens = get_tokens(attribute_value, fields, ",", 4);
  
      if(num_tokens < 1 || num_tokens > 2) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012242_ID, CAV_ERR_1012242_MSG);
      }
 
      if(fields[0]) 
      {
        requests[url_idx].proto.smtp.msg_count_min = requests[url_idx].proto.smtp.msg_count_max = atoi(fields[0]);
      }
        
      if(fields[1]) 
      {
        requests[url_idx].proto.smtp.msg_count_max = atoi(fields[1]);
      }

      if(requests[url_idx].proto.smtp.msg_count_min <= 0 ||  requests[url_idx].proto.smtp.msg_count_max <= 0) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012240_ID, CAV_ERR_1012240_MSG);
      }

      if(requests[url_idx].proto.smtp.msg_count_min > requests[url_idx].proto.smtp.msg_count_max) 
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012241_ID, CAV_ERR_1012241_MSG);
      }
      NSDL2_SMTP(NULL, NULL,"SMTP: Value of  %s = %s", attribute_value, attribute_name);
    } 
    else 
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012219_ID, CAV_ERR_1012219_MSG, attribute_value);
    }
 
    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
   
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_SMTP(NULL, NULL, "Next attribute is not found");
      break;
    }
  }

  if(start_quotes == NULL)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012209_ID, CAV_ERR_1012209_MSG, "SMTP");
  }
  else
  {
      //Checking Function Ending
      if(!strncmp(start_quotes, ");", 2))
      {
        NSDL2_SMTP(NULL, NULL, "End of function ns_smtp_send found %s", start_quotes);
      }
      else
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012210_ID, CAV_ERR_1012210_MSG, start_quotes);
      }
    }
  requests[url_idx].proto.smtp.num_to_emails   = save_emails(url_idx, headers, to_emails_buf, &script_ln_no, sess_idx, flow_filename, FLG_TO_EMAIL); // to emails
  requests[url_idx].proto.smtp.num_cc_emails   = save_emails(url_idx, headers, cc_emails_buf, &script_ln_no, sess_idx, flow_filename, FLG_CC_EMAIL); // cc   
  requests[url_idx].proto.smtp.num_bcc_emails  = save_emails(url_idx, headers, bcc_emails_buf, &script_ln_no, sess_idx, flow_filename, FLG_BCC_EMAIL); // bcc

  sprintf(headers, 
  "%s%sContent-Type: multipart/mixed;\r\n"     
  "    boundary=%s\r\n" 
  "Content-Disposition: inline\r\n",
  headers, subject, attachment_boundary);

  segment_line(&(requests[url_idx].proto.smtp.hdrs), headers, 0, script_ln_no, sess_idx, flow_filename);

  if((requests[url_idx].proto.smtp.num_attachments = save_attachment(url_idx, attachemnt_fname_buf, &script_ln_no, sess_idx, flow_filename)) < 0)

  NSDL2_SMTP(NULL, NULL, "request url_idx = %d, to_emails = %hd, cc_emails = %hd, bcc_emails = %hd, attachments = %hd", 
                          url_idx,
                          requests[url_idx].proto.smtp.num_to_emails,
                          requests[url_idx].proto.smtp.num_cc_emails,
                          requests[url_idx].proto.smtp.num_bcc_emails,
                          requests[url_idx].proto.smtp.num_attachments
                          );

  if(!smtp_server_flag) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "SMTP_SERVER", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
 }

 if(!user_flag && passwd_flag) {
   SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "USER_ID", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name) );
 }

 if(user_flag && !passwd_flag) { 
   SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "PASSWORD", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name) );
 }

 if(!from_email_flag) {
   SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "FROM_EMAIL", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
 }

 if(!to_email_flag) {
   SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "TO_EMAILS", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
 }

 if(!body_flag) {
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_ID, "BODY", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
 }

  return NS_PARSE_SCRIPT_SUCCESS;
}
