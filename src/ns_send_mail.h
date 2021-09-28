#ifndef _ns_send_mail_h
#define _ns_send_mail_h

typedef struct
{
  int idx;
  int mode;              //mode: decide at which phase we send mail
  int sr_flag;           //flag to check if we need to attach summary report file
  char filepath[1024];   //group definitions file with absolute path
  char *to_list[50];
  char *cc_list[50];
  char *atch_list[50];
  char subject[1024];
  char *msg_body; 
}SendMailTableEntry;

extern char g_test_start_time[];
extern int check_and_send_mail(int phase);
extern void  kw_set_send_mail_value(char *buf, char *err_msg, int runtime_flag);
extern SendMailTableEntry *sendMailTableEntry;

#endif
