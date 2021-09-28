#ifndef NS_NETHAVOC_HANDLER_H
#define NS_NETHAVOC_HANDLER_H
//Macros

#define NUM_MANDATORY_VALUES      6
#define NH_PHASE_NAME_SIZE        48
#define NH_SCENARIO_NAME_SIZE     64 

#define INVALID_PHASE_NAME        -1 


//socket fd macros
#define SOCKET_FD_SUCCESS      0 
#define SOCKET_FD_FAILURE     -1 

//Table macros
#define DELTA_ENTRIES          2  //delta value for mallocing
#define NH_HASH_TABLE_SIZE     5  //initial hash table size 

//Buffer Size Macros 
#define MAX_BUFF_SIZE          2048
#define TOKEN_BUFF_SIZE        512 
#define URL_BUFF_SIZE          128 
#define MAX_LOCAL_BUFF_SIZE    128 

//Nethavoc enable/disable macros
#define DISABLE_NETHAVOC       0 
#define ENABLE_NETHAVOC        1 

#define NH_SCENARIO_INTEGRATION_PARSING_DONE 0

//send api macros
#define POST_REQUEST       2
#define APPLICATION_JSON   "application/json" 

//phase state macros
#define NS_PHASE_START 1
#define NS_PHASE_END   2

//protocol macros
#define HTTP  0
#define HTTPS 1

//nethavoc scenario settings
typedef struct Nh_scenario_settings
{
  char nh_scenario_name[NH_SCENARIO_NAME_SIZE + 1];
  char ns_phase_name[NH_PHASE_NAME_SIZE + 1];
  int  delay;
  int  stop_scenario_at_phase_end;
}Nh_scenario_settings;

typedef struct Nh_global_settings
{
  char is_nethavoc_enable;
  char server_ip[MAX_LOCAL_BUFF_SIZE + 1];
  char server_port[MAX_LOCAL_BUFF_SIZE + 1];
  char url[URL_BUFF_SIZE + 1];
  char token[TOKEN_BUFF_SIZE + 1];
  int  protocol;
}Nh_global_settings;

extern int  kw_set_nh_scenario(char *buf, char *err_msg);
extern void nethavoc_init();
extern int  nethavoc_send_api(char *phase_name, int phase_state);
extern void nethavoc_cleanup(); 

#endif
