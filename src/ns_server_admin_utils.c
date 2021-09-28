#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_server_admin_utils.h"
#include "ns_struct_con_type.h"
#include "nslb_util.h"
#include "ns_monitor_profiles.h"
#include "url.h"
#include "ns_mon_log.h"
#include "ns_event_log.h"
#include "ns_trace_level.h"
#include "ns_event_id.h"
#include "ns_string.h"
extern char g_machine[128];
/*#define DELTA_CMON_SERVER 5


#define FREE_AND_MAKE_NULL(to_free, msg, index)  \
{                        \
  if (to_free)   \
  { \
    free(to_free);  \
    to_free = NULL; \
  } \
}


#define MY_MALLOC(new, size, msg, index) {                              \
    if (size < 0)                                                       \
      {                                                                 \
        fprintf(stderr, "Trying to malloc a negative size (%d) for index %d\n", (int)size, index); \
      }                                                                 \
    else if (size == 0)                                                 \
      {                                                                 \
        new = NULL;                                                     \
      }                                                                 \
    else                                                                \
      {                                                                 \
        new = (void *)malloc( size );                                   \
        if ( new == (void*) 0 )                                         \
        {                                                               \
          fprintf(stderr, "Out of Memory: %s for index %d\n", msg, index); \
          exit (-1);                                         \
        }                                                               \
      }                                                                 \
  }


#define MY_REALLOC(buf, size, msg, index)  \
{ \
    if (size <= 0) {  \
      fprintf(stderr, "Trying to realloc a negative or 0 size (%d) for index  %d\n", (int)size, index); \
      exit (-1);  \
    } else {  \
      buf = (void*)realloc(buf, size); \
      if ( buf == (void*) 0 )  \
      {  \
        fprintf(stderr, "Out of Memory: %s for index %d\n", msg, index); \
        exit (-1);  \
      }  \
    } \
  }

int total_cmon_servers = 0;
static int max_cmon_servers = 0; */

/*int create_cmon_server_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name)
{
  if (*total == *max)
  {
    MY_REALLOC(*ptr, (*max + DELTA_CMON_SERVER) * size, name, -1);
    *max += DELTA_CMON_SERVER;
  }
  *row_num = (*total)++;
  return 0;
}*/

/*void fill_ServerInfo(char *fields[], ServerInfo *local_server)
{
  //debug_log(0, _LF_, "Method Called");

  MY_MALLOC(local_server->server_ip, strlen(fields[0]) + 1, "Server Name", -1);
  strcpy(local_server->server_ip, fields[0]);

  local_server->is_ssh = fields[1][0];

  MY_MALLOC(local_server->user_name, strlen(fields[2]) + 1, "User Name", -1);
  strcpy(local_server->user_name, fields[2]);

  MY_MALLOC(local_server->password, strlen(fields[3]) + 1, "password", -1);
  strcpy(local_server->password, fields[3]);


  MY_MALLOC(local_server->java_home, strlen(fields[4]) + 1, "java_home", -1);
  strcpy(local_server->java_home, fields[4]);

  MY_MALLOC(local_server->install_dir, strlen(fields[5]) + 1, "install_dir", -1);
  strcpy(local_server->install_dir, fields[5]);

  local_server->is_agent_less = fields[6][0];

  MY_MALLOC(local_server->machine_type, strlen(fields[7]) + 1, "machine_type", -1);
  strcpy(local_server->machine_type, fields[7]);

  //Init
  local_server->con_type = NS_STRUCT_HEART_BEAT;
  local_server->con_state = HEART_BEAT_CON_INIT;
  local_server->bytes_remaining = 0;
  local_server->send_offset = 0;
  local_server->cntrl_conn_state = -1;
  local_server->control_fd = -1;
  local_server->origin_cmon = NULL; //For Heroku
} */

char *cmon_settings = NULL;  //This will take crae of COMN_SETTING ALL(server) case
#if 0
FILE *open_file(char* file_name, char* mode)
{
  FILE* fp = NULL;

  fp = fopen (file_name, mode);
  return fp;
}
#endif

/*void free_servers_list()
{
  int i;
  ServerInfo *to_free_ptr = servers_list;

  for (i = 0; i < total_no_of_servers; i++, to_free_ptr++)
  {
    FREE_AND_MAKE_NULL(to_free_ptr->server_ip, "to_free_ptr->server_ip", -1);
    FREE_AND_MAKE_NULL(to_free_ptr->user_name, "to_free_ptr->user_name", -1);
    FREE_AND_MAKE_NULL(to_free_ptr->password, "to_free_ptr->password", -1);
    FREE_AND_MAKE_NULL(to_free_ptr->java_home, "to_free_ptr->java_home", -1);
    FREE_AND_MAKE_NULL(to_free_ptr->install_dir, "to_free_ptr->install_dir", -1);
    FREE_AND_MAKE_NULL(to_free_ptr->machine_type, "to_free_ptr->machine_type", -1);
    FREE_AND_MAKE_NULL(to_free_ptr->cmon_settings, "to_free_ptr->cmon_settings", -1);
    FREE_AND_MAKE_NULL(to_free_ptr->origin_cmon, "to_free_ptr->origin_cmon", -1);
  }

  FREE_AND_MAKE_NULL(servers_list, "servers_list", -1);
}*/

//read servers.dat & fill into structure
/*void read_server_file()
{
  char file_name[64] = "\0";
  char line[MAX_BUF_SIZE] = "\0";
  FILE *fp = NULL; 
  char *fields[12];
  int num_fields;
  int row_num = 0;

  //debug_log(0, _LF_, "Method Called");
  if (getenv("NS_WDIR") != NULL)
    sprintf(file_name, "%s/server/servers.dat", getenv("NS_WDIR"));
  else
    strcpy(file_name, "/home/cavisson/work/server/servers.dat");
   
  fp = open_file(file_name, "r");

  if(fp == NULL)
    return;


  while (fgets(line, MAX_LINE_LEN, fp))
  {
    line[strlen(line) - 1] = '\0';
    if (line[0] == '#' || line[0] == '\0')
      continue;
    num_fields = get_tokens(line, fields, "|");

    //Server|Ssh|User|Password|Java Home|Installation Dir|Agentless|Machine Type|Future1|Future2|Future3
    if(num_fields < 11 )
    {
      fprintf(stderr, "Warning: Server '%s' has (%d) fields which is less then expected (11) fields in '%s'. Ignored.\n", fields[0], num_fields, file_name);
      continue;
    }

    create_cmon_server_table_entry(&row_num, &total_cmon_servers, &max_cmon_servers, (char **)&servers_list, sizeof(ServerInfo), "Cmon server list");
    memset((servers_list + row_num), 0, sizeof(ServerInfo));
    fill_ServerInfo(fields, servers_list + row_num);
  }
  //debug_log(1, _LF_, "Total Servers %d.", total_cmon_servers);
} */

/*
  1. server contains @ else copy server in input_server_ip
  2. does server conatin > , set input_tier & input_server_ip
  3. set cs_ip & origin_cmon in case of origin cmon
  4. Does input_server_ip contain ":", if yes set ip in input_server_ip & port in input_port
  5. if input_port not equal to g_cmon_port , set tmp_server_ip_and_port as ip:port
  6  loop all servers, check input_server_ip & input port exists (server_name / server_disp_name), if no continue
  7. if yes,
          if origin_cmon then add origin_cmon entry obtain  origin_cmon_indx
          if input tier defined & server_idx > 0 & server's tier == tier_idx
          if topo_server_idx > 0 & autoscale bit set , check status , if status 0 continue
          check 127.0.0.1 server, if server_disp_name Controller skip
          check hpd port != cmon_port
          is agentless check
          set server_idx
          return server_idx
  8. if not found return -1
*/
//search server in structure
int find_tier_idx_and_server_idx(char *server, char *cs_ip, char *origin_cmon, char hv_separator, int hpd_port, int *topo_server_idx,int *tier_idx)
{
  
  char *input_tmp_arr[2] ;
  int input_port = g_cmon_port;
  int server_idx;
  char input_server_ip[1024 + 1];
  char origin_cmon_name[1024 + 1] = "";
  char origin_tier_name[1024 + 1];
  char *at_the_rate_ptr = NULL;
  char hv_sep[2];
  char input_tier[1024] = "";
  char tier_heroku[1024] = "";
  char tmp_server_ip_and_port[1024];
  //strstr expects a string in second arg, passing address of char will not have terminating charcter.
  //So creating new string with /0 character.
  //strcpy(hv_sep, &hv_separator);
  hv_sep[0] = hv_separator;
  hv_sep[1] = '\0';
  char *buff[20];
  char *fields[20];
  int num_token; 
  char *fields_heroku[20];
  char input_vector[1024]; 
  input_server_ip[0] = '\0';

  strcpy(input_vector,server);
  memset(input_tmp_arr, 0, sizeof(input_tmp_arr)); 


  //server format can be:
  // server
  // heroku@server
  //from 4.6.0 onwards, following format will be supported 
  // tier>server
  // Tier>heroku@tier>server
  // tier>server:port
  // tier>heroku@tier>server:port
  
  //heroku case
  if((strstr(server,"@")))
  {
    num_token=get_tokens(input_vector,buff,"@",5); // Tier>heroku | Tier>Server:port
    if(num_token >=2)
    {
      if((at_the_rate_ptr= strstr(server,hv_sep)) != NULL) 
      {             //We got Tier>heroku
        get_tokens(buff[0],fields_heroku,hv_sep,5);   //buff[0]= Tier ,buff[1] == heroku
        sprintf(origin_cmon_name,"%s",fields_heroku[1]);                     
        sprintf(origin_tier_name,"%s",fields_heroku[0]);                     
      }
      else
      {
        sprintf(origin_cmon_name,"%s",buff[0]);                       //We got only heroku,so we will insert heroku in  origin_cmon_name
        sprintf(tier_heroku,"%s","Cavisson");                          //Assuming Tier as cavisson
      }
      num_token=get_tokens(buff[1],fields,hv_sep,5);                  //We got Tier>Server in buff[1] that we are further tokenizing 
      sprintf(input_tier,"%s",fields[0]);
      strcpy(input_server_ip,fields[1]);
    }

  }
  else if(strstr(server,hv_sep))
  {
    num_token = get_tokens(input_vector, buff, hv_sep,5);             //Means we got only Tier>Server 
    if (num_token >= 2)
    {
      sprintf(input_tier,"%s" ,buff[0]);
      strcpy(input_server_ip,buff[1]);
    }
  }
  
  else if(!strstr(server,hv_sep) && !strstr(server,"@") && !strstr(server,":"))  //Means we got only server "NDAppliance" or 60_13
  {
    sprintf(input_tier,"%s","Cavisson");
    sprintf(input_server_ip,"%s",server);
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,"Tier name is not mentioned,so assumed tier as Cavisson");

  
  }

  else if(!strstr(server,hv_sep) && !strstr(server,"@") && strstr(server,":")) //We got 127.0.0.1:7891
  {
      strcpy(input_tier,"Cavisson");
      strcpy(input_server_ip,server);
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,"Tier name is not mentioned,so assumed tier as Cavisson");
  }


  if((cs_ip != NULL) && (origin_cmon != NULL))
  {
    strcpy(cs_ip, input_server_ip);
    strcpy(origin_cmon, origin_cmon_name);
   }

  if(strstr(input_server_ip, ":"))
  {
    char local_input_server_ip[1024] = {0};
    strcpy(local_input_server_ip, input_server_ip);

    get_tokens(local_input_server_ip, input_tmp_arr, ":", 2);
    if(input_tmp_arr[0])
      strcpy(input_server_ip, input_tmp_arr[0]);
    if(input_tmp_arr[1])
      input_port = atoi(input_tmp_arr[1]);
   }
  
   if(input_port != g_cmon_port)
   {
     sprintf(tmp_server_ip_and_port, "%s:%d", input_server_ip, input_port);
   }
   else
   {
     strcpy(tmp_server_ip_and_port, input_server_ip);
   }
   *tier_idx=topolib_get_tier_id_from_tier_name(input_tier,topo_idx);
   if(*tier_idx < 0)
     return -1;
      //find in input_server_ip matches server display name. It will not 
   server_idx = nslb_get_norm_id(&topo_info[topo_idx].topo_tier_info[*tier_idx].server_key,tmp_server_ip_and_port, strlen(tmp_server_ip_and_port));
   if(server_idx < 0)
     return -1;
  
  if(!topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_ptr->server_flag & DUMMY)
  {
    if((topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_ptr->auto_scale & 0X02))
    {  //2nd bit is set only for monitors discovered from cmon.
       if(topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_ptr->status == 0)
       {
         *topo_server_idx=server_idx;
       }
    }
    else if((strcmp(topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->server_ip,                "127.0.0.1")) ||(strcmp(topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_disp_name, g_machine)))   
    {  
       if(strcmp(topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_disp_name, "Controller"))
         {
            *topo_server_idx=server_idx;
             return 0;
         }
   
    }   
      //do not increment this for NV monitors, because no need to make control connection with hpd
   if(hpd_port != input_port)
   {
     topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->cmon_monitor_count++;  // Marked to Used by any of Monitors
   }

   if(topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[server_idx].server_ptr->is_agentless[0] == 'Y')
   {
         //debug_log(-1, _LF_, "Error: Server is '%s' agent less.", server);
     return 1;
   }
  }
 
   
  if(server_idx != -1)
  { 
    *topo_server_idx=server_idx;
    return 0;
  }
 else
 {
  return -1;
 }
}



//Manish: this function will search out given ip and port alredy exist in server table or not??
//If alredy exist then return 1 otherwise 0
int search_server_in_topo_servers_list(char *tiername, char *server)
{
  int tier_idx,server_idx;
  tier_idx=topolib_get_tier_id_from_tier_name(tiername,topo_idx);  
  if(tier_idx >-1)
  {
    server_idx=nslb_get_norm_id(&topo_info[topo_idx].topo_tier_info[tier_idx].server_key,server,strlen(server));
    if(server_idx>=0)
      return 1;
    else
      return 0;
  }
  else
    return 0;
}

//Added By Manish: to add server if server is not in the server list
int add_server_in_server_list(char *server_ip,char *vector,int topo_idx)
{
 char *input_tmp_arr[2] ;
 char input_server_ip[46 + 1];
 char tier[40];
 int tier_idx;
 char server[40];
 int server_idx;
 int is_server_in_server_list = 0; //Assume server is not in server list
 int num_token;
 char *buff[20];
 char tier_server[80];

 sprintf(tier_server, vector);
 
 num_token=get_tokens(tier_server,buff ,">",3); 
 if(num_token >= 2)
 { 
   strcpy(tier,buff[0]);
   strcpy(server,buff[1]);
 }
 else
  strcpy(tier,vector);
 
  memset(input_tmp_arr, 0, sizeof(input_tmp_arr));  

  strcpy(input_server_ip, server_ip);

  if(strstr(input_server_ip, ":"))
  {
    get_tokens(input_server_ip, input_tmp_arr, ":", 2);

    //input_server_ip already has server ip and strtok writes '\0' in place of ':'
    //copy port
    /*
    if(input_tmp_arr[1])
    {
     int input_port = atoi(input_tmp_arr[1]);
    }
    */
  }
  tier_idx=topolib_get_tier_id_from_tier_name(tier,topo_idx); 
  server_idx=nslb_get_norm_id(&topo_info[topo_idx].topo_tier_info[tier_idx].server_key,server,strlen(server)); 
    if(server_idx >=0) 
      is_server_in_server_list = 1;
    else //Yes server is in the server list 
      is_server_in_server_list = 0; 
  
  

  //If provided server is not in server list then add that server into server list
  if(!is_server_in_server_list)
  {

    //Here we are going to make an entry in topo_servers_list, its entry is already made in topo_server_info then also we were setting topo_server_idx -1. Now total_server_table_entries is passed to this function, which will be stored as topo_server_idx because it's the last entry added in that structure.
    topolib_fill_auto_mon_server_info(server,tier,topo_idx,input_server_ip);
  }
  
  return 0;
}

