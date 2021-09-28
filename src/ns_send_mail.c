#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ns_log.h"
#include "nslb_util.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_send_mail.h"
#include "ns_license.h"
#include "ns_exit.h"

#define LINE_MAX_LENGTH 64*128 + 64 + 10

//Method to parse mail configuration file
/*******format will be***********/
/*
To:john@cavisson.com,tommy@cavisson.com
Cc:sandy@gmail.com
Attachment:k.txt,k1.txt,k2.txt
Sub:mail testing
Body:this is just a testing mail.
*/

#define PART_SEPERATOR 100001 
int parse_and_fill_struct(SendMailTableEntry *send_mail_table, int *to_num, int *cc_num, int *atch_num, char *err_msg)
{
  char buf[LINE_MAX_LENGTH]; //max 64 ids, each of max 128 bytes, 64 ',' and 10 for rest
  char *tmp_line = buf;
  char *line;
  char mail_conf_file[512];
  FILE *fp;

  NSDL2_PARSING(NULL, NULL, "Method called");

  if(getenv("NS_WDIR") == NULL){
    NS_EXIT(1, "NS_WDIR is not set for this controller, Unable to read mail.conf file\n"); //Do we need to stop test and exit
  }

  sprintf(mail_conf_file, "%s/ns_mail.conf", getenv("NS_WDIR"));
 
  if((fp = fopen(mail_conf_file, "r"))){
    while(nslb_fgets(tmp_line, LINE_MAX_LENGTH, fp, 0)){
      CLEAR_WHITE_SPACE(tmp_line);

      NSDL2_PARSING(NULL, NULL, "line = [%s], line_len=[%d]", tmp_line, strlen(tmp_line));
      if(tmp_line[0] == '\n' || tmp_line[0] == '\0' || tmp_line[0] == '#')
        continue;

      tmp_line[strlen(tmp_line) - 1] = '\0';

      if(strncasecmp(tmp_line, "To:", 3) == 0){
        NSDL2_PARSING(NULL, NULL, "To:found");
        NSLB_MALLOC(line, LINE_MAX_LENGTH, "LINE FOR MAIL", -1, NULL);
        strcpy(line, tmp_line);
        line += 3;
        CLEAR_WHITE_SPACE(line);
        NSDL2_PARSING(NULL, NULL, "LINE=[%s]", line);
         
        if((*to_num = get_tokens(line, send_mail_table->to_list, ",", 50)) < 1){
          strcpy(err_msg, "Error: Atleast 1 mailid should be there for To:");
          return -1;
        }
        
      }else if(strncasecmp(tmp_line, "Cc:", 3) == 0){
        NSDL2_PARSING(NULL, NULL, "Cc:found");
        NSLB_MALLOC(line, LINE_MAX_LENGTH, "LINE FOR MAIL", -1, NULL);
        strcpy(line, tmp_line);
        line += 3;
        CLEAR_WHITE_SPACE(line);
        *cc_num = get_tokens(line, send_mail_table->cc_list, ",", 50);
        
      }else if(strncasecmp(tmp_line, "Attachment:", 11) == 0){
        NSDL2_PARSING(NULL, NULL, "Attachment:found");
        NSLB_MALLOC(line, LINE_MAX_LENGTH, "LINE FOR MAIL", -1, NULL);
        strcpy(line, tmp_line);
        line += 11;
        CLEAR_WHITE_SPACE(line);
        *atch_num = get_tokens(line, send_mail_table->atch_list, ",", 50);
        
      }else if(strncasecmp(tmp_line, "Sub:", 4) == 0){
        NSDL2_PARSING(NULL, NULL, "Sub:found");
        tmp_line += 4;
        CLEAR_WHITE_SPACE(tmp_line);
        strcpy(send_mail_table->subject, tmp_line);
        
      }else if(strncasecmp(tmp_line, "Body:", 5) == 0){
        NSDL2_PARSING(NULL, NULL, "Body:found");
        tmp_line += 5;
        CLEAR_WHITE_SPACE(tmp_line);
        NSLB_MALLOC(send_mail_table->msg_body, strlen(tmp_line) + 1, "sendMailBody", -1, NULL);
        strcpy(send_mail_table->msg_body, tmp_line); 
      }else{
        sprintf(err_msg, "Unknown field [%s] is recieved in file [%s]", tmp_line, send_mail_table->filepath);
        return -1;
      }
    }
  }else{
    sprintf(err_msg, "Error: Unable to open file [%s], error = [%s]", send_mail_table->filepath, nslb_strerror(errno));
    return -1;
  }
  int i;
  for(i=0; i<*to_num; i++)
     NSDL2_PARSING(NULL, NULL, "to = [%s]", send_mail_table->to_list[i]);

  for(i=0; i<*atch_num; i++)
     NSDL2_PARSING(NULL, NULL, "atch = [%s]", send_mail_table->atch_list[i]);
  for(i=0; i<*cc_num; i++)
     NSDL2_PARSING(NULL, NULL, "cc = [%s]", send_mail_table->cc_list[i]);
  NSDL2_PARSING(NULL, NULL, "send_mail_table->msg_body = [%s], send_mail_table->subject = [%s]", send_mail_table->msg_body, send_mail_table->subject);
  return 0;
}

static int search_and_extract_group_list(FILE *fp, char *grp_name, char **list_string)
{

  char buf[2048] = "";
  char *line = buf;

  NSDL2_PARSING(NULL, NULL, "Method called, group_name = [%s]", grp_name);

    while(nslb_fgets(line, 2048, fp, 0)){

      CLEAR_WHITE_SPACE(line);

      if(*line == '#' || *line == '\0' || *line == '\n')
        continue;

      line[strlen(line)-1] = '\0';

      if(!strncmp(line, grp_name, strlen(grp_name))){
         line += strlen(grp_name);
         CLEAR_WHITE_SPACE(line);
         line++;
         CLEAR_WHITE_SPACE(line);

         NSLB_MALLOC(*list_string, strlen(line) + 1, "GROUP_LIST", -1, NULL);
     
         strcpy(*list_string, line);
         return 0;
      }
   }
   return -1;
}

void add_recipients(FILE *fp, char **add_list,  struct curl_slist **recipients, int num)
{
  char *group_list = NULL;
  char tmp[512];
  char *fields[50];
  int num_tok, i, ret;

  NSDL2_PARSING(NULL, NULL, "Method called, atch_num = [%d]", num);

  for(i=0; i<num ; i++){
    if(!strchr(add_list[i], '@')){
       ret = search_and_extract_group_list(fp, add_list[i], &group_list);
       NSDL2_PARSING(NULL, NULL, "group_list = [%s]", group_list);
       
       if(!ret){
         num_tok = get_tokens(group_list, fields, ",", 50);
         if(num_tok > 0)
         {
           for(i=0; i<num_tok; i++){
             if(strchr(fields[i], '@')){
                sprintf(tmp, "<%s>", fields[i]);
                NSDL2_PARSING(NULL, NULL, "tmp_in_group = [%s]", tmp);
                *recipients = curl_slist_append(*recipients, tmp);
              }
            }//end of for loop
         }
       }
    }else{
       sprintf(tmp, "<%s>", add_list[i]);
       NSDL2_PARSING(NULL, NULL, "in_simple_list = [%s]", tmp);
       *recipients = curl_slist_append(*recipients, tmp);
    }
  }
}

//base64 encoding
static char char_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static int mod_table[] = {0, 2, 1};

char *base64_encode(const char *in, int in_len, int *out_len) {
    int i, j;
    *out_len = 4 * ((in_len + 2) / 3);

    char *b64_encoded = malloc(*out_len);
    if (b64_encoded == NULL) return NULL;

    for (i = 0, j = 0; i < in_len;) {

        int octet_a = i < in_len ? (unsigned char)in[i++] : 0;
        int octet_b = i < in_len ? (unsigned char)in[i++] : 0;
        int octet_c = i < in_len ? (unsigned char)in[i++] : 0;

        int triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        b64_encoded[j++] = char_table[(triple >> 3 * 6) & 0x3F];
        b64_encoded[j++] = char_table[(triple >> 2 * 6) & 0x3F];
        b64_encoded[j++] = char_table[(triple >> 1 * 6) & 0x3F];
        b64_encoded[j++] = char_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[in_len % 3]; i++)
        b64_encoded[*out_len - 1 - i] = '=';

    return b64_encoded;
}

int compose_mail_and_write_to_file(int fd, SendMailTableEntry *send_mail_table, int atch_num)
{
  char buf[4096];
  char *ptr = buf;
  int amt_written;
  int ret;
  int total_bytes = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, atch_num = [%d]", atch_num);
 
  amt_written = sprintf(ptr, "%s\r\n", "User-Agent: Netstorm");   
  ptr += amt_written;
  total_bytes += amt_written;

  amt_written = sprintf(ptr, "Date: %s\r\n",  g_test_start_time); 
  ptr += amt_written;
  total_bytes += amt_written;
 

  if(send_mail_table->mode == 1){
    amt_written = sprintf(ptr, "Subject: Netstorm test started %d.\r\n", testidx);
    ptr += amt_written;
    total_bytes += amt_written;
  }else if(send_mail_table->mode == 4){
    amt_written = sprintf(ptr, "Subject: Netstorm test completed successfully.\r\n");
    ptr += amt_written;
    total_bytes += amt_written;
  }

  amt_written = sprintf(ptr, "Importance: Low\r\nX-Priority: 5\r\nX-MSMail-Priority: Low\r\nMIME-Version: 1.0\r\n"); 
  ptr += amt_written;
  total_bytes += amt_written;

  amt_written = sprintf(ptr, "Content-Type: multipart/mixed; boundary=\"=PART=SEPARATOR=_%d_=\"\r\n\r\n--=PART=SEPARATOR=_%d_=\r\n", PART_SEPERATOR, PART_SEPERATOR);
  ptr += amt_written;
  total_bytes += amt_written;

   amt_written = sprintf(ptr, "Content-Type: text/plain\r\nContent-Transfer-Encoding: 8bit\r\nContent-Disposition: inline\r\n\r\nHi,\nNetstorm Test Started.\nTestRun: %d\nDate: %s\n\n%s\nThanks & Regards,\nNetstorm Team.\r\n\r\n", testidx, g_test_start_time, send_mail_table->msg_body);
   ptr += amt_written;
   total_bytes += amt_written;

  if((atch_num < 1) && (send_mail_table->mode == 1)){
    amt_written = sprintf(ptr, "--=PART=SEPARATOR=_%d_=--\r\n", PART_SEPERATOR);
    ptr += amt_written;
    total_bytes += amt_written;

    if((ret = write(fd, buf, total_bytes)) != total_bytes){
      fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
      return -1;
    }else{
      close(fd);
      return 0;
    }
  } else{
    amt_written = sprintf(ptr, "--=PART=SEPARATOR=_%d_=\r\n", PART_SEPERATOR);
    ptr += amt_written;
    total_bytes += amt_written; 
    if((ret = write(fd, buf, total_bytes)) != total_bytes){
      fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
      return -1;
    }
  }

  FILE *fp = NULL;
  char *data = NULL;
  int out_len, i;
  char *encoded_data = NULL;
  struct stat st;
  char filename[512];
 
  ptr = buf;
  total_bytes = 0;

  sprintf(filename, "%s/logs/%s/summary.report", getenv("NS_WDIR"), global_settings->tr_or_partition);

  if((send_mail_table->mode == 4)){
    if((stat(filename, &st)) == -1){
      fprintf(stderr, "Unable to stat file [%s]\n", filename);
      return -1;
    }

    if((fp = fopen(filename, "r"))){
       NSLB_MALLOC(data, st.st_size, "ATACH_DATA", -1, NULL);
       if(fread(data, st.st_size, 1, fp) == 0){
         NSDL2_PARSING(NULL, NULL, "Error in reading data from file [%s]", filename);
         if(data)
           free(data);
         return -1;
       }else{
         encoded_data = base64_encode(data, strlen(data) , &out_len); 
         encoded_data[out_len] = '\0';
         amt_written = sprintf(ptr, "Content-Type: application/octet-stream; Name=\"%s\"\r\nContent-Transfer-Encoding: base64\r\n\r\n", filename);
         total_bytes += amt_written;

         if((ret = write(fd, ptr, total_bytes)) != total_bytes){
             fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
             return -1;
         }

         if((ret = write(fd, encoded_data, strlen(encoded_data))) != strlen(encoded_data)){
            fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
            return -1;
         }
       }  
    }

   ptr = buf;
   total_bytes = 0;

    if(atch_num < 1){
       amt_written = sprintf(ptr, "\r\n\r\n--=PART=SEPARATOR=_%d_=--\r\n", PART_SEPERATOR);  
       if((ret = write(fd, ptr, amt_written)) != amt_written){
         fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
         return -1;
       }
      return 0;
    }else{
       amt_written = sprintf(ptr, "\r\n\r\n--=PART=SEPARATOR=_%d_=\r\n", PART_SEPERATOR);
       if((ret = write(fd, ptr, amt_written)) != amt_written){
         fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
         return -1;
       }
    }
  }

  if(atch_num > 0){
    for(i = 0 ; i<atch_num; i++){
     if(stat(send_mail_table->atch_list[i], &st) != -1){
       if((fp = fopen(send_mail_table->atch_list[i], "r"))){
         NSLB_MALLOC(data, st.st_size, "ATACH_DATA", -1, NULL); 
         if(fread(data, st.st_size, 1, fp) == 0){
            NSDL2_PARSING(NULL, NULL, "Error in reading data from file [%s]", send_mail_table->atch_list[i]);
            if(data)
             free(data);
         }else{
            encoded_data = base64_encode(data, st.st_size, &out_len); 
            encoded_data[out_len] = '\0';
            amt_written = sprintf(ptr, "Content-Type: application/octet-stream; Name=\"%s\"\r\nContent-Transfer-Encoding: base64\r\n\r\n", send_mail_table->atch_list[i]);
    
            if((ret = write(fd, ptr, amt_written)) != amt_written){
               fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
               return -1;
            }
           // ptr += amt_written;

            if((ret = write(fd, encoded_data, strlen(encoded_data))) != strlen(encoded_data)){
               fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
               return -1;
            }
            
            if(i == (atch_num-1))
               amt_written = sprintf(ptr, "\r\n\r\n--=PART=SEPARATOR=_%d_=--\r\n", PART_SEPERATOR);
            else
               amt_written = sprintf(ptr, "\r\n\r\n--=PART=SEPARATOR=_%d_=\r\n", PART_SEPERATOR);

            if((ret = write(fd, ptr, amt_written)) != amt_written){
               fprintf(stderr, "Unable to write complete data in file for fd = [%d]\n", fd);
               return -1;
            }

            if(encoded_data)
              free(encoded_data);
            if(data)
              free(data);
         }
       }       
     }
   }//end of for loop    
  }//end of atch_num condition 
  return 0; 
}

#define FROM "<noreply@noreply.com>"

int send_mail(SendMailTableEntry *send_mail_table, int to_num, int cc_num, int atch_num, char *err_msg)
{
  int ret;
  FILE *fp = NULL;
  int data_fd = -1;
  FILE *data_fp = NULL;
  char file_name[512];
  //curl varaibles and structure initialization

  CURL *curl;
  CURLcode res = CURLE_OK;
  struct curl_slist *recipients = NULL;
  //struct upload_status upload_ctx;
 // upload_ctx.lines_read = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, to_num = [%d], cc_num = [%d], atch_num = [%d]", to_num, cc_num, atch_num);

  sprintf(file_name, "/tmp/mail_msg_%d", getpid());
 
  if((fp = fopen(send_mail_table->filepath, "r")) == NULL)
     NSDL2_PARSING(NULL, NULL, "Error in opening group definition file [%s]", send_mail_table->filepath);

  curl = curl_easy_init();

  if(curl) {
    /* Set username and password */
    //curl_easy_setopt(curl, CURLOPT_USERNAME, "netstorm@cavisson.com");
    //curl_easy_setopt(curl, CURLOPT_PASSWORD, "netstorm");

    curl_easy_setopt(curl, CURLOPT_URL, "smtp://127.0.0.1:25");

   //adding reciepients to to_list
    add_recipients(fp, send_mail_table->to_list, &recipients, to_num);
    add_recipients(fp, send_mail_table->cc_list, &recipients, cc_num);

    if(fp)
      fclose(fp);

    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM);

    //compose file data with attachments
    if((data_fd = open(file_name,  O_CREAT | O_CLOEXEC| O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) != -1){
       if((ret = compose_mail_and_write_to_file(data_fd, send_mail_table, atch_num)) == -1)
          return -1;
    }else{
      fprintf(stderr, "Unable to create file [%s], can't send mail. Error-[%s]\n", file_name, nslb_strerror(errno));
      return -1;
    }

    if((data_fp = fopen(file_name, "r")) == NULL)
    {
      fprintf(stderr, "Unable to open file [%s]\n", file_name);
      return -1; 
    }

    curl_easy_setopt(curl, CURLOPT_READDATA, data_fp);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* Send the message */
    res = curl_easy_perform(curl);

    //close all open fds and fps
    fclose(data_fp);
    close(data_fd);
    unlink(file_name);

    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* Free the list of recipients */
    curl_slist_free_all(recipients);

    /* Always cleanup */
    curl_easy_cleanup(curl);
  }

  return 0;
}


#define DELTA_SM_ENTRIES 100

SendMailTableEntry *sendMailTable = NULL;
int max_sm_entries = 0;
int total_sm_entries = 0;

//sample program, later we need to call it from proper place
int check_and_send_mail(int phase)
{
  int i, to_num = 0, cc_num = 0 , atch_num = 0, ret;
  char err_msg[1024];

  NSDL2_PARSING(NULL, NULL, "Method called, phase = [%d]", phase);
 
  for(i=0; i<4; i++){
    if(sendMailTable){
      if(sendMailTable[i].mode == phase){
        ret = parse_and_fill_struct(&sendMailTable[i], &to_num, &cc_num, &atch_num, err_msg);
        if(ret == -1){
          fprintf(stderr, "%s\n", err_msg);
          return -1;
        }
        ret = send_mail(&sendMailTable[i], to_num, cc_num, atch_num, err_msg);
        if(ret == -1){
          fprintf(stderr, "%s\n", err_msg);
          return -1;
        }
      } 
    }
  }
  //free(sendMailTable);
  return 0;
}

static void kw_set_sm_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of NS_SEND_MAIL keyword: %s", err_msg);
  NSTL1_OUT(NULL, NULL, "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL, "  Usage: NS_SEND_MAIL <mode> <summary_report_flag> <groups filepath>");
  NSTL1_OUT(NULL, NULL, "  Where:");
  NSTL1_OUT(NULL, NULL, "         mode : Is used to specify, if, at which phase we want to send mail");
  NS_EXIT(-1, "%s\nUsage: NS_SEND_MAIL <mode> <summary_report_flag> <groups filepath>", err_msg);
}

static int create_sendmail_table_entry(int *row_num) 
{
  NSDL2_PARSING(NULL, NULL, "Method called, total_sm_entries = %d, max_sm_entries = %d", total_sm_entries, max_sm_entries);
  if (total_sm_entries == max_sm_entries) 
  {
    NSLB_REALLOC (sendMailTable, (max_sm_entries + DELTA_SM_ENTRIES) * sizeof(SendMailTableEntry), "sendMailTable entries", -1, NULL);
    max_sm_entries += DELTA_SM_ENTRIES;
  }
  *row_num = total_sm_entries++;
  sendMailTable[*row_num].idx = *row_num;
  sendMailTable[*row_num].mode = -1;
  sendMailTable[*row_num].sr_flag = 0;
  sendMailTable[*row_num].filepath[0]= '\0';

  NSDL2_PARSING(NULL, NULL, "Method Ended sucessfully, total_sm_entries = %d, max_sm_entries = %d, row_num = %d", total_sm_entries, max_sm_entries, *row_num);
  return 0;
}

#define NONE 0
#define PHASE_START 1
#define PHASE_RAMPUP 2
#define PHASE_RAMPDOWN 3
#define PHASE_ENDS 4
#define MAX_DATA_LINE_LENGTH 512

void  kw_set_send_mail_value(char *buf, char *err_msg, int runtime_flag)
{

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  int num, rnum;
  int sr_flag = 0;
  char keyword[MAX_DATA_LINE_LENGTH];
  char filepath[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char mode[MAX_DATA_LINE_LENGTH];


  if ((num =sscanf(buf, "%s %s %d %s %s", keyword, mode, &sr_flag, filepath, tmp)) < 2)
      kw_set_sm_usage("Minimum two fields are required.", buf);

  if (create_sendmail_table_entry(&rnum) != 0)
    kw_set_sm_usage("Failed to create sendMailTable entry", buf);

  if(sr_flag < 0 || sr_flag > 1) kw_set_sm_usage("Only 0/1 value is allowed for summary report flag.", buf);

  sendMailTable[rnum].sr_flag = sr_flag;

  if (!strcasecmp(mode, "NONE"))
    sendMailTable[rnum].mode = NONE;
  else if (!strcasecmp(mode, "START"))
    sendMailTable[rnum].mode = PHASE_START; 
  else if (!strcasecmp(mode, "RAMPUP"))       // Not implenmented yet
    sendMailTable[rnum].mode = PHASE_RAMPUP;
  else if (!strcasecmp(mode, "RAMPDOWN"))    // Not implenmented yet
    sendMailTable[rnum].mode = PHASE_RAMPDOWN;
  else if (!strcasecmp(mode, "END"))
    sendMailTable[rnum].mode = PHASE_ENDS;
  else
    kw_set_sm_usage("Unknown mode option is given with SEND_MAIL keyword", buf);

  if((sendMailTable[rnum].mode < 0) || (sendMailTable[rnum].mode > 4))
   kw_set_sm_usage("Mode value should be in between 0 to 4", buf);

  if(((sendMailTable[rnum].mode > 0) && (filepath[0] == '\0')) || ((sendMailTable[rnum].mode == 0) && (filepath[0] != '\0')))
    kw_set_sm_usage("Filepath should be given if mode is greater than 0, if 0 no need.", buf);

  if(sendMailTable[rnum].mode > 0)
    strcpy(sendMailTable[rnum].filepath, filepath);
  
  NSDL1_PARSING(NULL, NULL, "sendMailTable[rnum].id = [%d], sendMailTable[rnum].mode = [%d], sendMailTable[rnum].sr_flag = [%d], sendMailTable[rnum].filepath = [%s]", sendMailTable[rnum].idx, sendMailTable[rnum].mode, sendMailTable[rnum].sr_flag, sendMailTable[rnum].filepath);

}

