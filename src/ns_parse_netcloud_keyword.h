#ifndef NS_PARSE_NC_KW_H
#define NS_PARSE_NC_KW_H 

#define NS_GEN_START_TEST_DEFAULT_TIMEOUT_FOR_NON_DEBUG 900
#define NS_GEN_START_TEST_DEFAULT_TIMEOUT_FOR_DEBUG     1800

#define DLT_OLD_TESTRUN 0
typedef struct
{
  char mode;        //mode to continue test
  char num_sample_delay_allowed; //gen timeout when delay in receiving this number of samples
  char percent_started;  //when gen timeout then continue test with this percent of generators started
  char percent_running;  //when gen timeout then continue test with this percent of generators running
  int start_timeout;  //timeout for starting test on generator in secs
} ContinueTestOnGenFailure;

extern void kw_set_controller_server_ip (char *buf, int flag, char *out_buff, int *out_port);
extern int kw_set_ns_gen_fail(char *buf, char *err_msg, int runtime_flag);
extern void kw_enable_tmpfs(char *buf);
extern void clean_up_tmpfs_file(int mode);
extern void ns_clean_current_instance();

#endif
