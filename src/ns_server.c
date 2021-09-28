#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>

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

#include "url.h"
#include "nslb_cav_conf.h"
#include "nslb_sock.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "poi.h"
#include "ns_alloc.h"
#include "wait_forever.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "tr069/src/ns_tr069_lib.h"
#include "ns_dynamic_hosts.h"
#include "ns_proxy_server.h"
#include "nslb_time_stamp.h"
#include "ns_exit.h"
#include "ns_trace_level.h"
#include "ns_user_profile.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_auto_fetch_parse.h"

#ifndef CAV_MAIN
int gNAServerHost = -1;
static char *g_svr_host_table_shr_mem;
static int all_svr_host_table_total_size = 0;
static int total_shm_size = 0;
static int host_table_shm_size = 0;
int total_totsvr_entries = 0;

int static_host_table_shm_size = 0;
StaticHostTable *g_static_host_table_shr_mem = NULL;
static int is_static_host_shm_created;
#else
static __thread char *g_svr_host_table_shr_mem;
static __thread int all_svr_host_table_total_size = 0;
static __thread int total_shm_size = 0;
static __thread int host_table_shm_size = 0;
__thread int total_totsvr_entries = 0;

__thread int static_host_table_shm_size = 0;
__thread int gNAServerHost = -1;
__thread StaticHostTable *g_static_host_table_shr_mem = NULL;
__thread int is_static_host_shm_created = 0;
#endif

//InuseSvrTableEntry *inuseSvrTable;

#define DELTA_TOT_ACT_SVR_ENTRIES 64
#define DELTA_TOT_REC_HOST_ENTRIES 8

#define DELTA_TOT_STATIC_HOST_ENTRY 64

int get_is_static_host_shm_created() {return is_static_host_shm_created;}

static int create_per_host_svr_entry(PerGrpHostTableEntry *perHostPtr, int *row_num)
{
  NSDL2_HTTP(NULL, NULL, "Method called");
  if (perHostPtr->total_act_svr_entries == perHostPtr->max_act_svr_entries)
  {
    MY_REALLOC_AND_MEMSET_EX (perHostPtr->server_table, (perHostPtr->max_act_svr_entries + DELTA_TOT_ACT_SVR_ENTRIES) * sizeof(PerHostSvrTableEntry), (perHostPtr->max_act_svr_entries * sizeof(PerHostSvrTableEntry)), "PerHostSvrTableEntry", -1);
    if (!perHostPtr->server_table)
    {
      NS_EXIT(-1, "create_per_host_svr_entry(): Error allocating more memory for perGrpHostTable");
      return(FAILURE);
    } else perHostPtr->max_act_svr_entries += DELTA_TOT_ACT_SVR_ENTRIES;
  }
  *row_num = perHostPtr->total_act_svr_entries++;
  perHostPtr->server_table[*row_num].host_id = perHostPtr->host_idx;
  NSDL2_HTTP(NULL, NULL, "total_act_svr_entries = %d, server_table->host_id = %d", perHostPtr->total_act_svr_entries, perHostPtr->server_table[*row_num].host_id);
  return (SUCCESS);
}

int create_per_grp_host_entry(GrpSvrHostSettings *grpSvrHostPtr , int *row_num) 
{
  NSDL2_HTTP(NULL, NULL, "Method called, grpSvrHostPtr->total_rec_host_entries = %d", grpSvrHostPtr->total_rec_host_entries);
  if (grpSvrHostPtr->total_rec_host_entries == grpSvrHostPtr->max_rec_host_entries)
  {
    MY_REALLOC_EX (grpSvrHostPtr->host_table, (grpSvrHostPtr->max_rec_host_entries + DELTA_TOT_REC_HOST_ENTRIES) * sizeof(PerGrpHostTableEntry), (grpSvrHostPtr->max_rec_host_entries * sizeof(PerGrpHostTableEntry)), "PerGrpHostTableEntry", -1); 
    if (!grpSvrHostPtr->host_table)
    {
      NS_EXIT(-1, "create_per_grp_host_entry(): Error allocating more memory for perGrpHostTable");
      return(FAILURE);
    } else grpSvrHostPtr->max_rec_host_entries += DELTA_TOT_REC_HOST_ENTRIES;
  }
  *row_num = grpSvrHostPtr->total_rec_host_entries++;
  return (SUCCESS);
}

int create_svr_table_entry(int *row_num) 
{
  NSDL2_HTTP(NULL, NULL, "Method called");
  if (total_svr_entries == max_svr_entries) {
    MY_REALLOC_EX (gServerTable, (max_svr_entries + DELTA_SVR_ENTRIES) * sizeof(SvrTableEntry), (max_svr_entries * sizeof(SvrTableEntry)), "gServerTable", -1);  // added prev size(maximum server entries * size of SvrTableEntry table)
    if (!gServerTable) 
    {
      NS_EXIT(-1, "create_svr_table_entry(): Error allocating more memory for server entries");
      return(FAILURE);
    } else max_svr_entries += DELTA_SVR_ENTRIES;
  }
  *row_num = total_svr_entries++;
  gServerTable[*row_num].idx = *row_num;
  gServerTable[*row_num].request_type = -1;
  gServerTable[*row_num].type = -1;
  gServerTable[*row_num].tls_version = -1;
  gServerTable[*row_num].main_url_host = -1;
  return (SUCCESS);
}

int create_and_fill_sess_host_table_entry(SessTableEntry *sess_table, int sess_idx, unsigned long host_idx)
{
  int row_num =0;
  NSDL2_HTTP(NULL, NULL, "Method called, sess_idx = %d, host_idx = %lu", sess_idx, host_idx);

  if(sess_table->total_sess_host_table_entries == sess_table->max_sess_host_table_entries)
  {
    MY_REALLOC(sess_table->host_table_entries, (sess_table->max_sess_host_table_entries + DELTA_SVR_ENTRIES) * sizeof(sessHostTableEntry), "sessHostTableEntry", -1); 
    if (!sess_table->host_table_entries)
    {
      NS_EXIT(-1, "create_and_fill_sess_host_table_entry(): Error allocating more memory for session host table entries");
      return(FAILURE);
    } else sess_table->max_sess_host_table_entries += DELTA_SVR_ENTRIES;
  }  

  row_num= sess_table->total_sess_host_table_entries++;
  sess_table->host_table_entries[row_num].sess_host_idx = host_idx;
  NSDL2_HTTP(NULL, NULL, "Entry filled for sess_idx = %d, row_num = %d, host_table_entries = %d", 
                          sess_idx, row_num, sess_table->host_table_entries[row_num].sess_host_idx);
  return (SUCCESS);
}

/* In function if host name contains port then we need to find host name length without port 
 * Purpose: While comparing recorded hosts, name length should be used without port
 * Return: Returns host name length and fills port*/
int find_host_name_length_without_port(char *host_name, unsigned short *port)
{
  char *hend_ptr;

  //Calculate host name length 
  int host_name_length = strlen(host_name);

  NSDL1_HTTP(NULL, NULL, "Method called, host_name = %s, host_name_len = %d", host_name, host_name_length);

  //separate  hostname and the port
  hend_ptr = index (host_name, ':'); //returns ptr to the 1st occurrence of ":" in hostname

  if (hend_ptr)
  { 
    host_name_length = hend_ptr - host_name; //Subtract port (including colon) from given hostname
    hend_ptr++;
    *port = atoi(hend_ptr);
    NSDL2_HTTP(NULL, NULL, "Host name given with port, host name length = %d, port = %hd", host_name_length, *port);
    return (host_name_length);
  } 
  //Host name without port number
  *port = 0;
  NSDL2_HTTP(NULL, NULL, "Host name given without port, host name length = %d, port = %hd", host_name_length, *port);
  return (host_name_length);
}


//Calculate shared mem size
void cal_shr_mem_size()
{
  int g;
  int svr_table_shm_size = 0;
  
  GrpSvrHostSettings *svr_host_settings;

  NSDL2_HTTP(NULL, NULL, "Method called");

  //For each group
  NSDL2_HTTP(NULL, NULL, "Calling for Group Settings");
  for(g = 0; g < total_runprof_entries; g++)   //group
  {
    svr_host_settings = &runProfTable[g].gset.svr_host_settings;
    CAL_SHR_SIZE(svr_host_settings);
  } 

  NSDL2_HTTP(NULL, NULL, "total_shm_size = %d, host_table_shm_size = %d, svr_table_shm_size = %d", 
                          total_shm_size, host_table_shm_size, svr_table_shm_size);
}

typedef struct 
{
 char buffer[MAX_CONF_LINE_LENGTH];
}AllSvrHostTable;

static AllSvrHostTable *all_svr_host_table=NULL;

static void add_all_svr_host_entry(char *buffer)
{
  static int all_svr_host_table_max_size = 0;
  NSDL2_HTTP(NULL, NULL, "buffer = %s", buffer);

  if(all_svr_host_table_total_size == all_svr_host_table_max_size)
  {
    all_svr_host_table_max_size += DELTA_TOT_REC_HOST_ENTRIES;
    MY_REALLOC(all_svr_host_table, all_svr_host_table_max_size * sizeof(AllSvrHostTable) , "add_all_svr_host_entry", -1);
  }
  strcpy(all_svr_host_table[all_svr_host_table_total_size].buffer, buffer);
  all_svr_host_table_total_size++;
}

static char *
segregate_port (int idx, char *hosttext, int *port_flag, int *hport)
{
  char *hptr;
  char *ptr;
  int is_ipv6 = 0;
  char hosttext_buf[MAX_DATA_LINE_LENGTH + 1];

  NSDL1_HTTP(NULL, NULL, "Method Called. hosttext=%s", hosttext);

  strncpy(hosttext_buf, hosttext, MAX_DATA_LINE_LENGTH);
  hosttext_buf[MAX_DATA_LINE_LENGTH] = '\0';

  if ((ptr = index(hosttext, ':')) && (index(ptr+1, ':')))
    is_ipv6 = 1;

  if((hptr = nslb_split_host_port(hosttext, hport)) == NULL)
  {
    NSTL1(NULL, NULL, "Invalid input = [%s]", hosttext_buf);
    return NULL;
  }
  
  // Setting it to 1 as we have to append port in actual server table
  *port_flag = 1;

  if (*hport == 0) {
    *port_flag = 0;
    switch(gServerTable[idx].request_type) {
      case HTTP_REQUEST:
      case WS_REQUEST:
        if(is_ipv6) {
          *hport = 6880;
          *port_flag = 1;
        } else {
          *hport = 80;
        }
        NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", *hport);
        break;
      case HTTPS_REQUEST:
      case WSS_REQUEST:
        if (is_ipv6) { 
          *hport = 6443;
          *port_flag = 1;
        }  else {
          *hport = 443;
        }
        NSDL1_HTTP(NULL, NULL, "Setting HTTPS Port=%d", *hport);
        break;
      case SMTP_REQUEST:
        if (is_ipv6) { 
          *hport = 6825;
        }  else {
          *hport = 25;
        }
        NSDL1_HTTP(NULL, NULL, "Setting SMTP Port=%d", *hport);
        break;
      case SMTPS_REQUEST:
        if (is_ipv6) { 
          *hport = 6825;
        }  else {
          *hport = 465;
        }
        NSDL1_HTTP(NULL, NULL, "Setting SMTPS Port=%d", *hport);
        break;
      case POP3_REQUEST:
        if (is_ipv6) { 
          *hport = 6811;
        }  else {
          *hport = 110;
        }
        NSDL1_HTTP(NULL, NULL, "Setting POP3 Port=%d", *hport);
        break;
      case SPOP3_REQUEST:
        if (is_ipv6) { 
          *hport = 6811;
        }  else {
          *hport = 995;
        }
        NSDL1_HTTP(NULL, NULL, "Setting POP3 Port=%d", *hport);
        break;
      case FTP_REQUEST:
        if (is_ipv6) { 
          *hport = 6821;
        }  else {
          *hport = 21;
        }
        NSDL1_HTTP(NULL, NULL, "Setting FTP Port=%d", *hport);
        break;
      case DNS_REQUEST:
        if (is_ipv6) { 
          *hport = 6853;
        }  else {
          *hport = 53;
        }
        NSDL1_HTTP(NULL, NULL, "Setting DNS Port=%d", *hport);
        break;
      /*case IMAP_REQUEST:
        if (is_ipv6) { 
          *hport = 6143;
        }  else {
          *hport = 143;
        }
        NSDL1_HTTP(NULL, NULL, "Setting IMAP Port=%d", *hport);
        break;
      case IMAPS_REQUEST:
        if (is_ipv6) { 
          *hport = 6993;
        }  else {
          *hport = 993;
        }
        NSDL1_HTTP(NULL, NULL, "Setting IMAPS Port=%d", *hport);
        break;*/
#ifdef RMI_MODE
      case JBOSS_CONNECT_REQUEST:
        *hport = 1099;
        break;
#endif
      default:
	NS_EXIT(-1, "read_keywords(): In G_SERVER_HOST, unknown request type\n");
    }
  }
  return hptr;
}

static void
add_actual_server (int grp_idx, int recorded_server_index, char *actual_server, int actual_server_port, char *actual_server_location, int pflag, PerGrpHostTableEntry *host_table_ptr)
{
  char sname[1024];
  int act_entries = 0;
  char err_msg[1024];
        
  NSDL1_HTTP(NULL, NULL, "Method Called. recorded_server_index = %d, actual_server = %s, port = %d, "
                         "actual_server_location = %s, pflag = %d, host_table_ptr = %p", 
                          recorded_server_index, actual_server, actual_server_port, actual_server_location, pflag,
			  host_table_ptr);

  if (create_per_host_svr_entry(host_table_ptr, &act_entries) == -1) 
  {
    NS_EXIT(-1, "read_keywords(): Error in creating new create_per_host_svr_table\n");
  }

  PerHostSvrTableEntry *svr_table_ptr = &host_table_ptr->server_table[act_entries];

  int ret = is_valid_ip(actual_server);
  if(!ret)
  {
    NSDL3_HTTP(NULL, NULL, "Setting server_flags to domain for actual host.");
    svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_IS_DOMAIN;
  }
  if (pflag)
  {
    if(ret == IPv6) //For IPV6 we need to give IP in []
      snprintf (sname, 1023, "[%s]:%d", actual_server, actual_server_port);
    else // In IPV4 and domain just give : and port
      snprintf (sname, 1023, "%s:%d", actual_server, actual_server_port);
  }
  else
    snprintf (sname, 1023, "%s", actual_server);

  sname[1023] = '\0';

  strcpy(svr_table_ptr->server_name, sname);

  svr_table_ptr->server_name_len = strlen(svr_table_ptr->server_name);
  if (strchr(actual_server, ',')) 
  {
    NS_EXIT(-1, "read_keywords(): Can't have commas in the server names\n");
  }

  if (ns_get_host(grp_idx, &(svr_table_ptr->saddr), actual_server, actual_server_port, err_msg) == -1)
  {
    if(grp_idx >= 0) {
      NS_EXIT(-1, CAV_ERR_1031024, actual_server, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), err_msg);
    }
    else {
      NS_EXIT(-1, CAV_ERR_1031059, actual_server, err_msg);
    }
  }

  //Bug 53370 - Test not running when G_SERVER_HOST and G_STATIC_HOST keywords are used for a specific group.
  svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_ALREADY_RESOLVED;

  NSDL1_HTTP(NULL, NULL, "actual_server_location = %s", actual_server_location);

  if(!actual_server_location || (!strcmp(actual_server_location,"-")))
     actual_server_location = default_svr_location;

  if((svr_table_ptr->loc_idx = find_locattr_idx(actual_server_location)) == -1) {
    NS_EXIT(-1, "unknown location %s\n", actual_server_location);
  }
  
  strcpy(svr_table_ptr->loc_name, actual_server_location);

  NSDL1_HTTP(NULL, NULL, " svr_table_ptr = %p, loc_idx = %d, server_name = %s, loc_name = %s", svr_table_ptr, svr_table_ptr->loc_idx, svr_table_ptr->server_name, svr_table_ptr->loc_name);
}


void ns_parse_server_host(int grp_idx, PerGrpHostTableEntry *host_table_ptr, char *buf, int idx)
{
  int as_must=1; //One Actual Server must be provided
  char text[1024], fname[1024], *tok;
  int port, port2;
  FILE *fp;
  char *end_ip, *start_ip, cmd_buf[1024], text_buf[MAX_DATA_LINE_LENGTH + 1];
  int check_validity =0;
  int pflag = 0;
    
  NSDL1_HTTP(NULL, NULL, "Method Called. ns_parse_server_host, grp_svr_host = %p, buf = %s, idx = %d", host_table_ptr, buf, idx);

  tok = strtok(buf, " ");
  if (!strcmp(g_cavinfo.config, "NS>NO"))
    check_validity = 1;

  sprintf (fname, "%s/logs/TR%d/dst_ip_file", g_ns_wdir, testidx);

  while ((tok = strtok(NULL, " "))) 
  {
    strcpy(text, tok);       //actual server

    NSDL1_HTTP(NULL, NULL, "text = %s", text);

    tok = strtok(NULL, " "); //location

    if (is_ip_numeric(text) && (end_ip = index (text, '-')))
    {
      *end_ip = '\0';
      end_ip++;
      NSDL4_HTTP(NULL, NULL, "In if text = [%s], end_ip = [%s]", text, end_ip);

      strncpy(text_buf, text, MAX_DATA_LINE_LENGTH);
      text_buf[MAX_DATA_LINE_LENGTH] = '\0';
       
      if((start_ip = segregate_port (idx, text, &pflag, &port)) == NULL)
      {
	NS_EXIT( -1, "Invalid input [%s]\n", text_buf);
      }

      strncpy(text_buf, end_ip, MAX_DATA_LINE_LENGTH);
      text_buf[MAX_DATA_LINE_LENGTH] = '\0';

      if((end_ip = segregate_port (idx, end_ip, &pflag, &port2)) == NULL)
      {
	NS_EXIT( -1, "Invalid input [%s]\n", text_buf);
      }

      NSDL1_HTTP(NULL, NULL, "Method Called. end_ip [%s] , start_ip [%s] ", end_ip , start_ip);
      if (port != port2)
      {
	NS_EXIT( -1, "read_keywords():G_SERVER_HOST, both hosts in a range must specify same port\n");
      }
#if 0

      if (check_validity)
        sprintf (cmd_buf, "%s/bin/nsi_validate_ip -s -l -r %s-%s > %s", g_ns_wdir, text, end_ip, fname);
      else
        sprintf (cmd_buf, "%s/bin/nsu_seq_ip %s %s > %s", g_ns_wdir, text, end_ip, fname);

#endif

      sprintf (cmd_buf, "%s/bin/nsu_seq_ip %s %s > %s", g_ns_wdir, start_ip, end_ip, fname);	
      if (system(cmd_buf) != 0)
      {
        NS_EXIT( -1, "read_keywords(): in G_SERVER_HOST, validation cmd (%s) failed\n", cmd_buf);
      }
    // validate using nsi_validate_ip -s -l and add_all
    }
    else
    {
      NSDL4_HTTP(NULL, NULL, "In else text = [%s]", text);
      strncpy(text_buf, text, MAX_DATA_LINE_LENGTH);
      text_buf[MAX_DATA_LINE_LENGTH] = '\0';
      if((start_ip = segregate_port (idx, text, &pflag, &port)) == NULL)
      {
        NS_EXIT( -1, "Invalid input = [%s]\n", text_buf);
      }

      NSDL1_HTTP(NULL, NULL, "Method Called. start_ip [%s] ", start_ip);
      if (!strcasecmp (start_ip, "ALL"))
      { 
        //All
        //get_range_using nsu_show_addr -s -l and add_all
        if (check_validity)
          sprintf (cmd_buf, "%s/bin/nsu_show_address -s -l 999999999 > %s", g_ns_wdir, fname);
        else
        {
          NS_EXIT( -1, "read_keywords(): in G_SERVER_HOST, ALL as Actual host valid only for NS>NO configuration\n");
        }
        if(system(cmd_buf) != 0)
        {
          NS_EXIT( -1, "read_keywords(): in G_SERVER_HOST, getting all Server IP cmd (%s) failed\n", cmd_buf);
        }
      }
      else
      {
       add_actual_server (grp_idx, idx, start_ip, port, tok, pflag, host_table_ptr);

       if (as_must) as_must = 0;
         continue;
      }
    }
    if (!(fp = fopen (fname, "r"))) 
    {
      NS_EXIT(-1, "read_keywords(): in G_SERVER_HOST, failed to open address list file %s\n", fname);
    }
    while (fgets (text, 1024, fp))
    {
      text[strlen(text)-1] = '\0'; //Remove new line
      add_actual_server (grp_idx, idx, text, port, tok, pflag, host_table_ptr);

      if (as_must) 
        as_must = 0;
    }
    fclose(fp);
  }

  if (as_must)
  {
    NS_EXIT(-1, "read_keywords(): In G_SERVER_HOST, atleast one actual mapped host must be provided\n");
  }
}

//Adding server entry in PerHostSvrTable_Entry
static inline void add_svr_host_entry(GroupSettings *gset, int grp_idx, char *buffer)
{
  int idx = 0, hostname_len = 0, row_num = 0, host_idx;
  unsigned short server_port = 0;
  char tmp_buf [MAX_CONF_LINE_LENGTH] = "";
  char recorded_host[1024] = "";
  int matched = 0;

  NSDL1_HTTP(NULL, NULL, "Method called buffer = %s", buffer);

  sscanf(buffer, "%s",recorded_host);

  hostname_len = find_host_name_length_without_port(recorded_host, &server_port);

  NSDL1_HTTP(NULL, NULL, "hostname_len = %d, server_port = %d", hostname_len, server_port);

  while(1) 
  {
    //we are copying SERVER_HOST keyword line as every time when strtok called tmp_buf get tokenized, we need complete buf 
    strcpy(tmp_buf, buffer);
    //this searches the recorded_host in gSeverTable from idx(index)
    find_gserver_idx(recorded_host, server_port, &idx, hostname_len);
    if(idx == -1)
    {
      NSDL1_HTTP(NULL, NULL, "Recorded Host (%s) not found. Returning.", recorded_host);
      NS_DUMP_WARNING("In G_SERVER_HOST keyword, host '%s' is not a valid recorded host. Ignored.", recorded_host);
      break;
    }
    else if(grp_idx != -1)
    {
      matched = 0;
      //Checking whether host_idx is present in grp or not
      for(host_idx = 0; host_idx <gSessionTable[runProfTable[grp_idx].sessprof_idx].total_sess_host_table_entries; host_idx++)
      {
        NSDL1_HTTP(NULL, NULL, "sess_host_idx = %d, idx = %d", 
                                gSessionTable[runProfTable[grp_idx].sessprof_idx].host_table_entries[host_idx].sess_host_idx, idx);

        if(gSessionTable[runProfTable[grp_idx].sessprof_idx].host_table_entries[host_idx].sess_host_idx == idx)
        {
          matched = 1;
          break;  
        } 
      }
      if(!matched)
      {
        NSDL4_HTTP(NULL, NULL, "In G_SERVER_HOST keyword, host '%s' is not a valid recorded host for Group[%d]. Ignored\n",
                           	recorded_host, grp_idx);
        idx++; //inrementing idx as on nxt search in find_gserver_idx it start with this inremented index
        continue;  
      }
    }
   
    gServerTable[idx].type = SERVER_ANY;
    
    /* Allocate memory for PerGrpHostTableEntry */
    if (create_per_grp_host_entry(&gset->svr_host_settings, &row_num) == -1)
    {
      NS_EXIT(-1, CAV_ERR_1000002);
    }
    PerGrpHostTableEntry *host_table_ptr = &gset->svr_host_settings.host_table[row_num];
    host_table_ptr->host_idx = idx; 
    host_table_ptr->total_act_svr_entries = 0; 
    host_table_ptr->max_act_svr_entries = 0;
    host_table_ptr->server_table=NULL;
    host_table_ptr->grp_dynamic_host = 0;
    
    NSDL1_HTTP(NULL, NULL, "total_rec_host_entries = %d", gset->svr_host_settings.total_rec_host_entries); 
    /* actual_buf has the complete G_SERVER_HOST keyword line as passed in kw_set_g_server_host
    following method add actual server */
    ns_parse_server_host(grp_idx, host_table_ptr, tmp_buf, idx);
    NSDL4_HTTP(NULL, NULL, "total_act_svr_entries = %d", host_table_ptr->total_act_svr_entries);
    idx++; //inrementing idx as on nxt search in find_gserver_idx it start with this inremented index
  }
}

static void process_all_svr_host_table()
{
  int i = 0; //row_num,idx;
  GroupSettings *gset = group_default_settings;

  NSDL1_HTTP(NULL, NULL, "Method called, all_svr_host_table_total_size = %d, group_default_settings = %p, buffer = %s", 
                          all_svr_host_table_total_size, group_default_settings, all_svr_host_table[i].buffer);

  for(i=0; i< all_svr_host_table_total_size; i++)
  {
    add_svr_host_entry(gset, -1, all_svr_host_table[i].buffer);
  }
  FREE_AND_MAKE_NOT_NULL(all_svr_host_table, "add_all_svr_host_entry", -1);
  all_svr_host_table_total_size = 0;
}

int insert_totsvr_shr_mem(void) {
  int i,g;
  int use_dns_enabled = 0;
  u_ns_ts_t now;
  int svr_table_shm_size = 0;

  NSDL2_HTTP(NULL, NULL, "Method called");

  PerGrpHostTableEntry_Shr *host_table_ptr = NULL;
  PerHostSvrTableEntry_Shr *svr_table_ptr = NULL;
  GrpSvrHostSettings *svr_host_settings = NULL; 
  GrpSvrHostSettings_Shr *svr_host_settings_shr = NULL;

  for(i = 0; i < total_runprof_entries; i++){
    if(runprof_table_shr_mem[i].gset.use_dns){
      use_dns_enabled = 1;
      break;
    }  

    NSDL2_HTTP(NULL, NULL, "Calling for Group Settings");
    svr_host_settings = &runProfTable[i].gset.svr_host_settings;
    CAL_SHR_SIZE(svr_host_settings);

    NSDL2_HTTP(NULL, NULL, "total_shm_size = %d, host_table_shm_size = %d, svr_table_shm_size = %d", 
                            total_shm_size, host_table_shm_size, svr_table_shm_size);
  }

  //Calculate shared memory size of GrpSvrHostTable
  //cal_shr_mem_size();

  if(!total_shm_size) {
    NSDL2_HTTP(NULL, NULL, "total_shm_size is zero");
    return 0;
  }

  if(use_dns_enabled) {
    MY_MALLOC (g_svr_host_table_shr_mem, total_shm_size, "PerGrpHostTableEntry_Shr", -1);
  }
  else {
   g_svr_host_table_shr_mem = (char *) do_shmget(total_shm_size, "PerGrpHostTableEntry_Shr");
  }
  totsvr_table_shr_mem = (PerHostSvrTableEntry_Shr *)(g_svr_host_table_shr_mem + host_table_shm_size);
  host_table_ptr = (PerGrpHostTableEntry_Shr *)g_svr_host_table_shr_mem;
  svr_table_ptr = totsvr_table_shr_mem;

  now = get_ms_stamp();
 
  /*-----------------------------GRP_ENTRIES---------------------------------------*/
  for(g=0; g < total_runprof_entries; g++)
  {
    svr_host_settings = &runProfTable[g].gset.svr_host_settings;
    svr_host_settings_shr = &runprof_table_shr_mem[g].gset.svr_host_settings;
    svr_host_settings_shr->host_table = host_table_ptr;

    NSDL3_HTTP(NULL, NULL, "For GROUP[%d], total_rec_host_entries = %d, svr_host_settings_shr->host_table = %p", g, svr_host_settings_shr->total_rec_host_entries, svr_host_settings_shr->host_table);
    /*---------------------------HOST_ENTRIES -------------------------------------*/
    FILL_SHR_MEM(svr_host_settings);
  }
  return 0;
}
/*******************************************************************************************************************************
 * In this function we are copying G_SERVER_HOST ALL settings in group, after that we are again check its address (saddr)
 * because if someone has assigned domain to ip by applying keyword G_STATIC_HOST and it must be validated through
 * ns_get_host()
 ********************************************************************************************************************************/
void copy_all_data_in_grp(PerGrpHostTableEntry *all_grp_host_ptr, PerGrpHostTableEntry *grp_host_table_ptr, int grp_idx)
{
  int rnum = 0, k, tmp_svr_port = 0, pflag = 0;
  char *hptr = NULL;
  char local_server_name[1024];
  char err_msg[1024]; 


  NSDL3_HTTP(NULL, NULL, "Method called all_grp_host_ptr = %p, grp_host_table_ptr = %p, grp_idx = %d", all_grp_host_ptr, grp_host_table_ptr, grp_idx);
  NSDL3_HTTP(NULL, NULL, "Copy ALL data in Group[%d], total_act_svr_entries = %d", grp_idx, all_grp_host_ptr->total_act_svr_entries);
  for(k=0; k< all_grp_host_ptr->total_act_svr_entries; k++) 
  {
    if (create_per_host_svr_entry(grp_host_table_ptr, &rnum) == FAILURE) 
    {
      NS_EXIT(-1, "Failed to create per host entry while copying all data to group entry");
    }
    PerHostSvrTableEntry *svr_table_ptr = &grp_host_table_ptr->server_table[rnum];

    NSDL3_HTTP(NULL, NULL, "For ALL serv_idx = %d, server_name = %s, svr_table_ptr = %p, rnum = %d",
                            k, all_grp_host_ptr->server_table[k].server_name, svr_table_ptr, rnum);
    strcpy(svr_table_ptr->server_name, all_grp_host_ptr->server_table[k].server_name); 
    svr_table_ptr->server_name_len = all_grp_host_ptr->server_table[k].server_name_len;
    svr_table_ptr->host_id = all_grp_host_ptr->server_table[k].host_id; 
    strcpy(svr_table_ptr->loc_name, all_grp_host_ptr->server_table[k].loc_name); 
    svr_table_ptr->loc_idx = all_grp_host_ptr->server_table[k].loc_idx; 
    svr_table_ptr->net_idx = all_grp_host_ptr->server_table[k].net_idx; 
    NSDL2_HTTP(NULL, NULL , "host_id = %d", svr_table_ptr->host_id);

    if(runProfTable[grp_idx].proxy_idx != -1) 
      hptr = "127.0.0.1";
    else
    {
      strcpy(local_server_name, svr_table_ptr->server_name);
      //Added, to find default port (port filled in tmp_svr_port, so that port is correctly filled in sin6_port)
      hptr = segregate_port(svr_table_ptr->host_id, local_server_name, &pflag, &tmp_svr_port);
    }
 
    NSDL2_HTTP(NULL, NULL , "hptr is [%s] address of tmp_svr_port %d ", hptr , tmp_svr_port);

    if (ns_get_host(grp_idx, &(svr_table_ptr->saddr), hptr, tmp_svr_port, err_msg) == -1) 
    {
      NSDL2_HTTP(NULL, NULL , "hptr is [%s] and servernameptr is [%s]", hptr, svr_table_ptr->server_name); 
      NS_EL_2_ATTR(EID_NS_INIT, -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__, (char*)__FUNCTION__,
                                "Error: Host <%s> specified by Host header is not a valid hostname. Hosterr '%s'", hptr, err_msg);
      NS_EXIT(-1, CAV_ERR_1031024, hptr, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), err_msg);
      //NS_EXIT(-1, "Host <%s> specified by Host header is not a valid hostname used in <%s> group. Error: %s",
        //             hptr, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), err_msg);
    }
    NSDL2_HTTP(NULL, NULL , "saddr is [%s]", nslb_sockaddr_to_ip((struct sockaddr *)&(svr_table_ptr->saddr), 1)); 
    svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_ALREADY_RESOLVED;     
    NSDL2_HTTP(NULL, NULL, "Actual server_name = %s in grp, server_flags = %d", svr_table_ptr->server_name, svr_table_ptr->server_flags);
  }
}
/************************************************************************************************************
  Purpose: This function is called from ns_get_host. In this function check that host name is present
           in static host table or not. If host name found then respective ip address is get from 
           static host table.
 Input:    per_grp_static_host:- It is a pointer of PerGrpStaticHostTable. In case of dynamic hosts
           this is a pointer of shared memory of static host table.
 addr:     If host name and ip is filled in this structure.
 server_port: Port 
 hptr : It is host name
*************************************************************************************************************/
int find_ip(PerGrpStaticHostTable *per_grp_static_host, struct sockaddr_in6 *addr, int server_port, char *hptr)
{
  struct sockaddr_in *sin;
  int family = 0;
  char *ip_addr = NULL;
  int i = 0;

  NSDL2_HTTP(NULL, NULL, "Method Called, default_port = %d hptr = %s", server_port, hptr);

  memset((char *)addr, 0, sizeof(struct sockaddr_in6));

  for (i = 0; i < per_grp_static_host->total_static_host_entries; i++) 
  {
    NSDL2_HTTP(NULL, NULL, "static host name = %s", per_grp_static_host->static_host_table[i].host_name);
    if (!strcmp(per_grp_static_host->static_host_table[i].host_name, hptr))
    {
      ip_addr = per_grp_static_host->static_host_table[i].ip;

      NSDL2_HTTP(NULL, NULL, "hptr = %s, ip_addr = %s, server_port %d idx = %d", 
                              hptr, ip_addr, htons(server_port), idx);
      if (per_grp_static_host->static_host_table[i].family == AF_INET)
      {
        sin = (struct sockaddr_in *)addr;
        NSDL2_HTTP(NULL, NULL, "sin address = %p", sin);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(server_port);;
        inet_pton(AF_INET, ip_addr, (char *)&(sin->sin_addr)); 
        NSDL2_HTTP(NULL, NULL, "sin->sin_addr = %s sin->sin_port = %d", (char *)&(sin->sin_addr), sin->sin_port);
        return (sizeof(struct sockaddr_in));
      }
      else if (per_grp_static_host->static_host_table[i].family == AF_INET6)
      {
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(server_port);
        inet_pton(AF_INET6, ip_addr, (char *)&(addr->sin6_addr)); 
        return (sizeof(struct sockaddr_in6));
      }
      else
      {
        NS_EXIT(-1, "Error: in getting server socket address of server '%s' - Unsupported protocol address family %d", 
        hptr, family);
        return -1;
      }
   
    }
  }  
  return 0; 
}
/*********************************************************************************************************
  Purpose:- ns_get_host function is used for resolving host name to ip address.
            This function is also called for dynamic hosts. For dynamic hosts data is fetched from shared
            memory.
  Input:- grp_idx is valid group idx.
  server_name:- host_name
  server_port: port
**********************************************************************************************************/
int ns_get_host(int grp_idx, struct sockaddr_in6 *addr, char *server_name, int server_port, char *err_msg)
{
  char *hptr = server_name;
  int ret  = 0, port = 0;

  PerGrpStaticHostTable *per_grp_static_host = NULL;

  NSDL2_HTTP(NULL, NULL, "Method Called, grp_idx = %d server_name = %s, server_port = %d", grp_idx, server_name, server_port);

  /* In case of dynamic hosts ip is taken from shared memory*/
  if(is_static_host_shm_created == 0)
    per_grp_static_host = &runProfTable[grp_idx].gset.per_grp_static_host_settings;
  else
    per_grp_static_host = &runprof_table_shr_mem[grp_idx].gset.per_grp_static_host_settings;

  if((hptr = nslb_split_host_port (server_name, &port)) == NULL)
    return -1;

  if(grp_idx != -1) {
    ret = find_ip(per_grp_static_host, addr, server_port, hptr);
    if(ret == -1)
      return ret; 

    if(ret > 0) {
      NSDL2_HTTP(NULL, NULL, "Host name found in group");
      return 0;
    }
  }

  per_grp_static_host = &group_default_settings->per_grp_static_host_settings;
  ret = find_ip(per_grp_static_host, addr, server_port, hptr);
  if(ret == -1)
    return ret;
 
  if(ret > 0)
    return 1;

  ret = nslb_fill_sockaddr_ex(addr, server_name, server_port, err_msg);
  if(ret)
    return 2;
    
  return -1; //Error case. It should not come here
}

/*-----------------------------------------------------------------------------------------------------------------------------------
 * Function name: insert_default_svr_location 
 *
 * Purpose      : 1) It is filling default_svr_location to SanFrancisco
 *                2) It parses "G_SERVER_HOST ALL" entries and fill the data into structures i.e. 
 *		     PerGrpHostTableEntry and PerHostSvrTableEntry 
 *                3) This will traverse to every grp and check whether Recorded_host entry is present in
 *                   - Grp, then continue from there, as data is already filled while parsing of keyword "G_SERVER_HOST <Grp_name>"
 *                   - ALL, then copy all Recorded_host entries in grp also
 *                   - If entry is not present in Grp and ALL both then this will mapped to itself and fill the data into structure.
 *----------------------------------------------------------------------------------------------------------------------------------*/
void insert_default_svr_location() 
{
  int i;
  int rnum = 0, tmp_svr_port = 0, max_host_idx = 0;
  char *server_name_ptr = NULL;
  int grp_idx, host_idx;
  char proxy[] = "127.0.0.1";
  char *hptr = NULL;
  PerGrpHostTableEntry *grp_host_ptr = NULL;
  GrpSvrHostSettings *svr_host_settings = NULL;
  PerGrpHostTableEntry *host_table_ptr = NULL;
  char err_msg[1024];

  NSDL2_HTTP(NULL, NULL, "Method called, default_svr_location = %s", default_svr_location);

  if (!default_svr_location) {
    MY_MALLOC (default_svr_location, strlen("SanFrancisco") + 1, "default_svr_location", -1);
    strcpy(default_svr_location, "SanFrancisco");
  }

  if(all_svr_host_table_total_size)
  {
    process_all_svr_host_table();
  }

  //Create mapping of all servers and store it in g_default_svr_host_settings,
  NSDL2_HTTP(NULL, NULL, "total_runprof_entries = %d", total_runprof_entries);
 
  for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
  {
    //max_host_idx = (gSessionTable[runProfTable[grp_idx].sessprof_idx].total_sess_host_table_entries + total_svr_entries);
    max_host_idx = (gSessionTable[runProfTable[grp_idx].sessprof_idx].total_sess_host_table_entries + total_add_rec_host_entries);
    NSDL2_HTTP(NULL, NULL, "For Group[%d], max_host_idx = %d, total_sess_host_table_entries = %d, total_svr_entries = %d", 
                            grp_idx, max_host_idx, gSessionTable[runProfTable[grp_idx].sessprof_idx].total_sess_host_table_entries, 
                            total_svr_entries);

    for(host_idx = 0; host_idx < max_host_idx; host_idx++)
    {
      hptr = NULL;
      int grp_dynamic_host = 0;
    
      if ((host_idx < gSessionTable[runProfTable[grp_idx].sessprof_idx].total_sess_host_table_entries))
        i = gSessionTable[runProfTable[grp_idx].sessprof_idx].host_table_entries[host_idx].sess_host_idx;
      else
      { 
        i = host_idx - gSessionTable[runProfTable[grp_idx].sessprof_idx].total_sess_host_table_entries;
        if (i >= total_add_rec_host_entries)
          grp_dynamic_host = 1;
      }
      NSDL2_HTTP(NULL, NULL, "host_idx = %d, hostname = %s, port = %d, grp_dynamic_host = %d",
                              i, RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname), gServerTable[i].server_port, grp_dynamic_host);
     
      if(gServerTable[i].server_hostname == gNAServerHost) 
      {
        NSDL2_HTTP(NULL, NULL, "Continue as Server Host is parametrized");
        continue;
      }

      svr_host_settings = &runProfTable[grp_idx].gset.svr_host_settings;
      FIND_GRP_HOST_ENTRY(svr_host_settings, i, grp_host_ptr);

      if(grp_host_ptr)
      {
        //Found host in grp host table
        NSDL3_HTTP(NULL, NULL, "Entry found in Group[%d], host_idx = %d, host_name = %s", grp_idx, i,
                                RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));
        continue;
      }
      NSDL3_HTTP(NULL, NULL, "Entry NOT found in Group[%d], host_idx = %d, host_name = %s", grp_idx, i,
                              RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));

      server_name_ptr = RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname);
      gServerTable[i].type = SERVER_ANY;   //ASK

      // Allocate memory for PerGrpHostTableEntry 
      if (create_per_grp_host_entry(svr_host_settings, &rnum) == -1)
      {
        NS_EXIT(-1, "Failed to allocate memory for group host entries");
      }
      host_table_ptr = &svr_host_settings->host_table[rnum];
      host_table_ptr->host_idx = i;
      host_table_ptr->total_act_svr_entries = 0;
      host_table_ptr->max_act_svr_entries = 0;
      host_table_ptr->server_table = NULL;
      host_table_ptr->grp_dynamic_host = grp_dynamic_host;

      //Check entry in ALL_HOST_ENTRY 
      svr_host_settings = &group_default_settings->svr_host_settings;
      FIND_GRP_HOST_ENTRY(svr_host_settings, i, grp_host_ptr);
      NSDL3_HTTP(NULL, NULL, "For ALL, grp_idx = %d, grp_host_ptr = %p, host_idx = %d, server_name = %s",
                              grp_idx, grp_host_ptr, i, RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));
      if(grp_host_ptr)
      {
        //Found host in all host table
        NSDL3_HTTP(NULL, NULL, "Entry found in ALL for grp_idx = %d, host_idx = %d, host_name = %s, server_name = %s",
                                grp_idx, i,
                                RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname), grp_host_ptr->server_table[i].server_name);

        //Copy "ALL" data in grp also and resolve host
        copy_all_data_in_grp(grp_host_ptr, host_table_ptr, grp_idx);
        continue;
      }

      NSDL3_HTTP(NULL, NULL, "Entry NOT found in ALL for grp_idx = %d, host_idx = %d, host_name = %s", grp_idx, i,
                                RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));
      NSDL3_HTTP(NULL, NULL, "Create default grp entry");
      if (create_per_host_svr_entry(host_table_ptr, &rnum) == FAILURE) {
        NS_EXIT(-1, "Failed to allocate memory for host server entries");
      }

      PerHostSvrTableEntry *svr_table_ptr = &host_table_ptr->server_table[rnum];
      NSDL3_HTTP(NULL, NULL, "host_table_ptr = %p, svr_table_ptr = %p", host_table_ptr, svr_table_ptr);

      int ret = is_valid_ip(server_name_ptr);
      NSDL2_HTTP(NULL, NULL , "Host ip type is = %d", ret);
      if(!ret)
      {
        NSDL3_HTTP(NULL, NULL, "Setting server_flags for domain.");
        svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_IS_DOMAIN;
      }

      //Narendra: Bugid#7622
      char host_name_with_port[1024];
      int default_port = 0;
      if((gServerTable[i].server_port ==  80 && gServerTable[i].request_type == HTTP_REQUEST) || 
         (gServerTable[i].server_port == 443 && gServerTable[i].request_type == HTTPS_REQUEST) ||
         (gServerTable[i].server_port ==  80 && gServerTable[i].request_type == WS_REQUEST) ||  
         (gServerTable[i].server_port == 443 && gServerTable[i].request_type == WSS_REQUEST))
         default_port = 1;
      
      if (gServerTable[i].server_port && !default_port)
      {
        if(ret == IPv6) //For IPV6 we need to give IP in []
          snprintf (host_name_with_port, 1023, "[%s]:%d", server_name_ptr, gServerTable[i].server_port);
        else // In IPV4 and domain just give : and port
          snprintf (host_name_with_port, 1023, "%s:%d", server_name_ptr, gServerTable[i].server_port);
        //now save this to big buf.
        strcpy(svr_table_ptr->server_name, host_name_with_port);
      }
      else
        strcpy(svr_table_ptr->server_name, server_name_ptr);
  
      svr_table_ptr->server_name_len = strlen(svr_table_ptr->server_name);
      svr_table_ptr->net_idx = -1;
 
      //Bug 46172 - Not able to start test from Script Manager 
      NSDL2_HTTP(NULL, NULL , "proxy_idx = %d", runProfTable[grp_idx].proxy_idx);
      if(runProfTable[grp_idx].proxy_idx != -1) {
        hptr = proxy;
      }
      else
        hptr = server_name_ptr; 

      /* Case:1 Main Url or get no -> Do lookup, If failed exit
       * Case:2 Inline Url or get no inline is off -> Do lookup, If failed exit
       * Case:2 Embedded url, get no inline is enabled for all groups-> Dont't lookup, mark as unresolved server 
       */      
       
      NSDL2_HTTP(NULL, NULL , "hptr is [%s] and servernameptr is [%s]  ",hptr, server_name_ptr); 
      if (ns_get_host(grp_idx, &(svr_table_ptr->saddr), hptr, gServerTable[i].server_port, err_msg) == -1) {
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                   __FILE__, (char*)__FUNCTION__,
                                   "Error: Host <%s> specified by Host header is not a valid hostname used in <%s> group. Hosterr '%s'",
                                   hptr, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), err_msg);
        if(!gServerTable[i].main_url_host)
        {
          if((((grp_idx + 1) < g_auto_fetch_info_total_size) && (g_auto_fetch_info[grp_idx + 1])) || g_auto_fetch_info[0])
          {
            NSTL1(NULL, NULL, "Warning: Host <%s> specified by Host header is not a valid hostname used in <%s> group. Hosterr '%s'",
                    hptr, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), err_msg);
            NSTL1(NULL, NULL, "Ignoring Host validation Error for Inline URL as Fetch inline resources based on main URL response is enabled.");
            continue;
          }
        } 
        NS_EXIT(-1, CAV_ERR_1031024, hptr, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), err_msg);
      } 
      NSDL2_HTTP(NULL, NULL , "saddr is [%s]", nslb_sockaddr_to_ip((struct sockaddr *)&(svr_table_ptr->saddr), 1)); 
      svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_ALREADY_RESOLVED;     
      NSDL2_HTTP(NULL, NULL , "server_flags = %d", svr_table_ptr->server_flags);
 
      if ((svr_table_ptr->loc_idx = find_locattr_idx(default_svr_location)) == -1) {
        NS_EXIT(-1, CAV_ERR_1031025, default_svr_location);
      }
      strcpy(svr_table_ptr->loc_name, default_svr_location);
    }
  }

  /* Filling the entry of all hosts in g_default_svr_host_settings.*/
  //Check entry in ALL_HOST_ENTRY 
  GrpSvrHostSettings * svr_host_settings_all = &group_default_settings->svr_host_settings;
  PerGrpHostTableEntry *host_table_ptr_all = svr_host_settings_all->host_table;
  NSDL2_HTTP(NULL, NULL, "Finding entry svr_host_settings = %p, svr_host_settings->total_rec_host_entries = %d", 
                          svr_host_settings_all, svr_host_settings_all->total_rec_host_entries);
    
  for(i = 0; i < svr_host_settings_all->total_rec_host_entries; i++, host_table_ptr_all++)
  {
    NSDL3_HTTP(NULL, NULL, "Checking host_idx = %d, host_name = %s", host_table_ptr_all->host_idx, 
                            RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
    {
      //Check entry in g_default_settings (mapped to itself) 
      svr_host_settings = &runProfTable[grp_idx].gset.svr_host_settings;
      FIND_GRP_HOST_ENTRY(svr_host_settings, host_table_ptr_all->host_idx, grp_host_ptr);
      NSDL3_HTTP(NULL, NULL, "grp_host_ptr = %p", grp_host_ptr);
      if(grp_host_ptr)
      {
        //Found host in grp host table
        NSDL3_HTTP(NULL, NULL, "Entry found in Group[%d], host_idx = %d, host_name = %s", grp_idx,  host_table_ptr_all->host_idx,
                                RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));
        continue;
      }
      NSDL3_HTTP(NULL, NULL, "Entry NOT found in Group[%d], host_idx = %d, host_name = %s", grp_idx,  host_table_ptr_all->host_idx,
                              RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));

      // Allocate memory for PerGrpHostTableEntry 
      if (create_per_grp_host_entry(svr_host_settings, &rnum) == -1)
      {
        NS_EXIT(-1, "Failed to create per group host entry");
      }
      host_table_ptr = &svr_host_settings->host_table[rnum];
      host_table_ptr->host_idx = host_table_ptr_all->host_idx;
      host_table_ptr->total_act_svr_entries = 0;
      host_table_ptr->max_act_svr_entries = 0;
      host_table_ptr->server_table = NULL;
      host_table_ptr->grp_dynamic_host = 1;  //This host is from other grp, so it is dynamic


      //Copy "ALL" data in grp also and resolve host
      copy_all_data_in_grp(host_table_ptr_all, host_table_ptr, grp_idx);
    }
  } 

  /* Bug 48920 - Error is coming when Grp1 has static host (H1) and in Grp2 same host (H1) is coming as dynamic host.*/
  /* Filling the entry of all hosts in g_default_svr_host_settings.*/
  for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
  {
    for(i=0; i<total_svr_entries; i++)
    {
      //Check entry in g_default_settings (mapped to itself) 
      svr_host_settings = &runProfTable[grp_idx].gset.svr_host_settings;
      FIND_GRP_HOST_ENTRY(svr_host_settings, i, grp_host_ptr);
        
      NSDL3_HTTP(NULL, NULL, "grp_host_ptr = %p", grp_host_ptr);
      if(grp_host_ptr)
      {
        //Found host in grp host table
        NSDL3_HTTP(NULL, NULL, "Entry found in Group[%d], host_idx = %d, host_name = %s", grp_idx, i,
                                RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));
        continue;
      }
      NSDL3_HTTP(NULL, NULL, "Entry NOT found in Group[%d], host_idx = %d, host_name = %s", grp_idx, i,
                              RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname));

      // Allocate memory for PerGrpHostTableEntry 
      if (create_per_grp_host_entry(svr_host_settings, &rnum) == -1)
      {
        NS_EXIT(-1, "Failed to create per group host entry");
      }
      host_table_ptr = &svr_host_settings->host_table[rnum];
      host_table_ptr->host_idx = i;
      host_table_ptr->total_act_svr_entries = 0;
      host_table_ptr->max_act_svr_entries = 0;
      host_table_ptr->server_table = NULL;
      host_table_ptr->grp_dynamic_host = 1;  //This host is from other grp, so it is dynamic
      
      if (create_per_host_svr_entry(host_table_ptr, &rnum) == FAILURE) {
        NS_EXIT(-1, "Failed to create per host table");
      }
      PerHostSvrTableEntry *svr_table_ptr = &host_table_ptr->server_table[rnum];
    
      NSDL3_HTTP(NULL, NULL, "host_table_ptr = %p, svr_table_ptr = %p", host_table_ptr, svr_table_ptr); 
    
      if(gServerTable[i].server_hostname == gNAServerHost) 
      {
        NSDL2_HTTP(NULL, NULL, "Continue as Server Host is parametrized");
        continue;
      }

      server_name_ptr = RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname);
      
      int ret = is_valid_ip(server_name_ptr);
      NSDL2_HTTP(NULL, NULL , "Host ip type is = %d", ret);
      if(!ret)
      {
        NSDL3_HTTP(NULL, NULL, "Setting server_flags for domain.");
        svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_IS_DOMAIN;
      }
      
      //Narendra: Bugid#7622
      char host_name_with_port[1024];
      int default_port = 0;
      if((gServerTable[i].server_port ==  80 && gServerTable[i].request_type == HTTP_REQUEST) || 
         (gServerTable[i].server_port == 443 && gServerTable[i].request_type == HTTPS_REQUEST) ||
         (gServerTable[i].server_port ==  80 && gServerTable[i].request_type == WS_REQUEST) ||  
         (gServerTable[i].server_port == 443 && gServerTable[i].request_type == WSS_REQUEST))
         default_port = 1;
      
      if (gServerTable[i].server_port && !default_port)
      {
        if(ret == IPv6) //For IPV6 we need to give IP in []
          snprintf (host_name_with_port, 1023, "[%s]:%d", server_name_ptr, gServerTable[i].server_port);
        else // In IPV4 and domain just give : and port
          snprintf (host_name_with_port, 1023, "%s:%d", server_name_ptr, gServerTable[i].server_port);
        //now save this to big buf.
        strcpy(svr_table_ptr->server_name, host_name_with_port);
      }
      else
        strcpy(svr_table_ptr->server_name, server_name_ptr);

      svr_table_ptr->server_name_len = strlen(svr_table_ptr->server_name);
      svr_table_ptr->net_idx = -1;
    
      hptr = nslb_split_host_port(server_name_ptr, &tmp_svr_port); 
      NSDL2_HTTP(NULL, NULL , "hptr is [%s] address of tmp_svr_port %d ", hptr , tmp_svr_port);
      
      /* Case:1 Main Url or get no -> Do lookup, If failed exit
       * Case:2 Inline Url or get no inline is off -> Do lookup, If failed exit
       * Case:2 Embedded url, get no inline is enabled for all groups-> Dont't lookup, mark as unresolved server 
       */
      NSDL2_HTTP(NULL, NULL , "hptr is [%s] and servernameptr is [%s]  ",hptr, server_name_ptr); 
      if (ns_get_host(grp_idx, &(svr_table_ptr->saddr), hptr, gServerTable[i].server_port, err_msg) == -1) {
        NSDL2_HTTP(NULL, NULL, "Host <%s> specified by Host header is not a valid hostname used in <%s> group. HostErr '%s'",
                     hptr, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), err_msg);
      }
      else
      { 
        NSDL2_HTTP(NULL, NULL , "saddr is [%s]", nslb_sockaddr_to_ip((struct sockaddr *)&(svr_table_ptr->saddr), 1));
        svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_ALREADY_RESOLVED;
      }
      NSDL2_HTTP(NULL, NULL , "server_flags = %d", svr_table_ptr->server_flags);

      if ((svr_table_ptr->loc_idx = find_locattr_idx(default_svr_location)) == -1) {
        NS_EXIT(-1, CAV_ERR_1031025, default_svr_location);
      }
      strcpy(svr_table_ptr->loc_name, default_svr_location);
    }
  } 

  //If Browser based api not used then default_svr_location freed here otherwise not freed because its value will be used in
  // get_wan_args_for_browser() function
  //if(!(global_settings->protocol_enabled & EXTERNAL_BROWSER_API_USED))
  if(!global_settings->enable_ns_firefox)
  {
    NSDL3_HTTP(NULL, NULL, "Freeing default_svr_location"); 
    FREE_AND_MAKE_NULL(default_svr_location, "default_svr_location", -1); 
  }
}

void remap_all_urls_to_other_host(VUser *vptr, char *hostname, int port) {

 int gserver_shr_idx;
 int rand_num;
 unsigned short rec_server_port;

  NSDL2_HTTP(vptr, NULL, "Method called, hostname = [%s], port = [%d]", hostname, port);
  int hostname_len = find_host_name_length_without_port(hostname, &rec_server_port);

  if ((gserver_shr_idx = find_gserver_shr_idx(hostname, port, hostname_len)) != -1) {
    PerGrpHostTableEntry_Shr *grp_host_ptr;
    FIND_GRP_HOST_ENTRY_SHR(vptr->group_num, gserver_shr_idx, grp_host_ptr);
    if(!grp_host_ptr)
    {
      NSDL2_HTTP(vptr, NULL, "grp_host_ptr is NULL");
      return;
    }
   
    PerHostSvrTableEntry_Shr *act_svr_table = grp_host_ptr->server_table;
    rand_num = ns_get_random(gen_handle) % grp_host_ptr->total_act_svr_entries;
    vptr->ustable[tr069_acs_url_idx].svr_ptr =  act_svr_table + rand_num;
    NSDL2_HTTP(vptr, NULL, "tr069_acs_url_idx = %d, svr_ptr = %p",
                              tr069_acs_url_idx, act_svr_table + rand_num);
  }
}
 /*actual_svr_idx = ns_get_random(gen_handle) % svr_ptr->num_svrs;\ */
#define RANDOMLY_FIND_SVR_IDX() \
 actual_svr_idx = ns_get_random(gen_handle) % total_act_svr_entries;\
 vptr->ustable[svr_ptr->idx].svr_ptr = act_svr_table + actual_svr_idx;\
 NSDL3_HTTP(vptr, NULL, "actual_svr_idx = %d, act_svr_table = %p, vptr->ustable[%d].svr_ptr = %p", \
                              actual_svr_idx, act_svr_table, svr_ptr->idx, vptr->ustable[svr_ptr->idx].svr_ptr);\
 if (global_settings->server_select_mode == SAME_ACTUAL_SERVER) \
 {\
   vptr->server_entry_idx = actual_svr_idx; \
   NSDL3_HTTP(vptr, NULL, "Actual server index, vptr->server_entry_idx = %d", vptr->server_entry_idx);\
 }


//Convert Recorded to actual server, SvrTableEntry_Shr is in the util.
PerHostSvrTableEntry_Shr* get_svr_entry(VUser* vptr, SvrTableEntry_Shr* svr_ptr) {
  NSDL2_HTTP(vptr, NULL, "Method called, vptr->ustable[%d].svr_ptr = %p, svr_ptr = %p, "
                         "svr_ptr->server_hostname = [%s], svr_ptr->server_port = [%d], vptr->server_entry_idx = %d", 
                          svr_ptr->idx, vptr->ustable[svr_ptr->idx].svr_ptr, svr_ptr, svr_ptr->server_hostname, svr_ptr->server_port, 
                          vptr->server_entry_idx);
  int actual_svr_idx = -1;
  int total_act_svr_entries = -1;

  //Mapping is done once for a session
  //ustable:svr_ptr -> GrpSvrHostTableEntry_Shr -> Actual host
  //svr_ptr ->  SvrTableEntry_Shr -> Recorded host
  if (vptr->ustable[svr_ptr->idx].svr_ptr == NULL) { //Mapping already done for this session
   
    if(svr_ptr->totsvr_table_ptr)
    { 
      NSDL2_HTTP(vptr, NULL, "Dynamic Host %s(%d) found for Group %s(%d)", svr_ptr->server_hostname, svr_ptr->idx,
                              runprof_table_shr_mem[vptr->group_num].scen_group_name, vptr->group_num);
      vptr->ustable[svr_ptr->idx].svr_ptr = svr_ptr->totsvr_table_ptr;
    }
    else
    {
      NSDL2_HTTP(vptr, NULL, "Static Host");
      PerGrpHostTableEntry_Shr *grp_host_ptr;

      FIND_GRP_HOST_ENTRY_SHR(vptr->group_num, svr_ptr->idx, grp_host_ptr);
      if(!grp_host_ptr)
      {
        NSTL1(vptr, NULL, "Error: Recorded Host %s(%d) not found in Group %s(%d)", svr_ptr->server_hostname, svr_ptr->idx,
                           runprof_table_shr_mem[vptr->group_num].scen_group_name, vptr->group_num);
        return NULL; //TODO
      }

      PerHostSvrTableEntry_Shr *act_svr_table = grp_host_ptr->server_table;
      total_act_svr_entries = grp_host_ptr->total_act_svr_entries;
     
      NSDL3_HTTP(vptr, NULL, "svr_ptr->type = %d, total_act = %d", svr_ptr->type, total_act_svr_entries);
      //Case 1) Recorded host is mapped to single actual host, hence returning GrpSvrHostTableEntry_Shr pointer
      if (total_act_svr_entries == 1) {
        NSDL3_HTTP(vptr, NULL, "Recorded host is mapped to single actual host, total_act_svr_entries = %d, "
                               "hence returning PerHostSvrTableEntry_Shr pointer = %p", 
                                total_act_svr_entries, act_svr_table);
     
        vptr->ustable[svr_ptr->idx].svr_ptr = &act_svr_table[0];
      }
      //Case 2) Recorded host mapped to more than one actual hosts (act_svr_entries > 1),
      // a) For first recorded host we need to find actual table index randomly or if server_host_entry is -1 
      // or SERVER_SELECT_MODE is disable
      else if (global_settings->server_select_mode == ANY_ACTUAL_SERVER || vptr->server_entry_idx == -1)
      {
        RANDOMLY_FIND_SVR_IDX()   
      }
      // b) If vptr->server_entry_idx contains previous actual host's index and server select mode is SAME_ACTUAL_SERVER 
      else 
      {
        NSDL3_HTTP(vptr, NULL, "Previous actual server index: vptr->server_entry_idx = %d, per_grp_host_table_shr_mem->total_act_svr_entries = %d, global_settings->server_select_mode = %d", vptr->server_entry_idx, total_act_svr_entries, global_settings->server_select_mode);
        if ((vptr->server_entry_idx >= total_act_svr_entries)) 
        { 
          NSDL3_HTTP(vptr, NULL, "Need to find actual host randomly, Mode: global_settings->server_select_mode = %d, VUser: vptr->server_entry_idx = %d", global_settings->server_select_mode, vptr->server_entry_idx);
          RANDOMLY_FIND_SVR_IDX()
        } else {
          vptr->ustable[svr_ptr->idx].svr_ptr = act_svr_table + vptr->server_entry_idx;
          //No need to update server_entry_idx as it will be resuse if number of servers for new 
          //recored host are equal to the current server idx
          NSDL3_HTTP(vptr, NULL, "Updated Actual server index: vptr->server_entry_idx = %d", vptr->server_entry_idx);
        }
      }
    } 
  }
  NSDL3_HTTP(vptr, NULL, "server_name = %s", vptr->ustable[svr_ptr->idx].svr_ptr->server_name);
  return vptr->ustable[svr_ptr->idx].svr_ptr;
}

int find_gserver_shr_idx(char* name, int port, int hostname_len) {
  int i, rec_hostname_len;
  unsigned short rec_server_port;

  NSDL2_HTTP(NULL, NULL, "Method called, name = %s, port = %d, num_dyn_host_left = [%d]", name, port, num_dyn_host_left);
  //Here we are searching svr entries with total_svr_entries - num_dyn_host_left. 
  //If dynamic host keyword disable then num_dyn_host_left = 0
  //else we are suppose search svr entry table including dynamic host
  for (i = 0; i < total_svr_entries - num_dyn_host_left; i++) {
    rec_hostname_len = find_host_name_length_without_port(gserver_table_shr_mem[i].server_hostname, &rec_server_port);
    NSDL4_HTTP(NULL, NULL, "Index i = %d hostname_len = %d, recoreded host = %s, rec_hostname_len = %d", i, hostname_len, gserver_table_shr_mem[i].server_hostname, rec_hostname_len);
    if ((rec_hostname_len == hostname_len) && 
         (!strncmp(gserver_table_shr_mem[i].server_hostname, name, hostname_len))) {
      NSDL2_HTTP(NULL, NULL, "Matching (%s:%d) with extracted(%s:%d)", 
                  gserver_table_shr_mem[i].server_hostname,
                  gserver_table_shr_mem[i].server_port,
                  name, port);
      if (gserver_table_shr_mem[i].server_port  == port) 
        return i;
    }
  }
  return -1;
}

/*
1) port_flag is output var and is set to 1, if port was specified as part of hostname
   else it is set to 0. Main purpose is to put the hostname with port as servername, if port
   specifically specified as opposed to using default
2) For IPv6 default port is not used.
3) For keyword "ALL" in G_SERVER_HOST port is not handled.

*/
void find_gserver_idx(char* name, int port, int *search_idx, int hostname_len) {
  int i = 0;
  int rec_hostname_len = 0;
  unsigned short rec_server_port;
  NSDL2_HTTP(NULL, NULL, "Method called, name = %s, port = %d, name_len = %d, search_idx = %d, total_svr_entries = %d", 
                          name, port, hostname_len, *search_idx, total_svr_entries);

  for (i = *search_idx; i < total_svr_entries; i++) {
    rec_hostname_len = find_host_name_length_without_port((RETRIEVE_BUFFER_DATA((unsigned int) gServerTable[i].server_hostname)), &rec_server_port);

    if ((rec_hostname_len == hostname_len) && 
         !strncmp(RETRIEVE_BUFFER_DATA((unsigned int) gServerTable[i].server_hostname), name, hostname_len)) {
        if (port == 0) {
                if (gServerTable[i].request_type == HTTPS_REQUEST || gServerTable[i].request_type == WSS_REQUEST)
                  port = 443;
                else if (gServerTable[i].request_type == HTTP_REQUEST || gServerTable[i].request_type == WS_REQUEST)
                  port = 80;
                else if (gServerTable[i].request_type == SMTP_REQUEST)
                  port = 25;
                else if (gServerTable[i].request_type == SMTPS_REQUEST)
                  port = 465;
                else if (gServerTable[i].request_type == POP3_REQUEST)
                  port = 110;
                else if (gServerTable[i].request_type == SPOP3_REQUEST)
                  port = 995;
                else if (gServerTable[i].request_type == FTP_REQUEST)
                  port = 21;
                else if (gServerTable[i].request_type == DNS_REQUEST)
                  port = 53;
                else if (gServerTable[i].request_type == IMAP_REQUEST)
                  port = 143;
                else if (gServerTable[i].request_type == IMAPS_REQUEST)
                  port = 993;
#ifdef RMI_MODE
                else if (gServerTable[i].request_type == JBOSS_CONNECT_REQUEST)
                  port = 1099;
#endif
        }
        if (gServerTable[i].server_port == port)
        {
            *search_idx=i;
            NSDL2_HTTP(NULL, NULL, "Recorded Host (%s) matches in gServerTable on index (%d) with port %d", name, *search_idx, port);
            return;
        }
    }
  }
    
  *search_idx = -1;
}


//Parses G_SERVER_HOST entry. buf_ptr contains everything after keyword
/*
1) G_SERVER_HOST GRP_NAME RECORDED_SERVER_IP SERVER_IP LOCATION

2) G_SERVER_HOST GRP_NAME RECORDED_SERVER_IP SERVER_IP LOCATION SERVER_IP LOCATION.....

3) G_SERVER_HOST GRP_NAME RECORDED_SERVER_IP SERVER_IP:PORT LOCATION

4) G_SERVER_HOST GRP_NAME RECORDED_SERVER_IP SERVER_START_IP-SERVER_END_IP LOCATION

5) G_SERVER_HOST GRP_NAME RECORDED_SERVER_IP SERVER_START_IP-SERVER_END_IP:PORT LOCATION

6) G_SERVER_HOST GRP_NAME RECORDED_SERVER_IP ALL LOCATION

7) G_SERVER_HOST GRP_NAME RECORDED_SERVER_IP ALL:PORT LOCATION

*/

int kw_set_g_server_host(char *buf, GroupSettings *gset, int grp_idx, char *err_msg, int runtime_changes)
{
  char keyword [MAX_DATA_LINE_LENGTH] = "";
  char actual_buf [MAX_CONF_LINE_LENGTH] = "";
  char location [MAX_DATA_LINE_LENGTH] = "";
  char group_name [512 + 1] = "";
  int num_args = 0;
  char tmp_buf[MAX_CONF_LINE_LENGTH] = "";
  char *value_buf = '\0';
  char recorded_host[1024] = "";

  NSDL1_HTTP(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s %s %s %s", keyword, group_name, recorded_host, actual_buf, location, tmp_buf);
 
  //If location is not provided by default it take SanFrancisco
  if(num_args < 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SERVER_HOST_USAGE, CAV_ERR_1011120, CAV_ERR_MSG_1);
  }

  //Removing newline from end 
  if (strchr(buf, '\n'))
    *(strchr(buf, '\n')) = '\0';

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  value_buf = strstr(buf, recorded_host);
  strcpy(actual_buf, value_buf);
  NSDL2_HTTP(NULL, NULL, "actual_buf = %s", actual_buf);

  /* Earliar on successful search(server_name & port) find_server_idx was returning match index of gServerTable.
  *  if same server name is there with diffrent port. it did not put entry in actual server table.
  *  So now we search in whole gServerTable, on successful match also.
  *  Every time search is started with next to previous search.
  */

   /*"ALL" is parsed before script parsing, hence we are not able to find the recorded host's idx in script
     hence this is called after parsing of script.
   */
   if(!strcmp(group_name,"ALL"))
     add_all_svr_host_entry(actual_buf);
   else 
     add_svr_host_entry(gset, grp_idx, actual_buf);
  return 0;
}

short get_parameterized_server_idx(char *hostname, int request_type, int line_number)
{   
  NSDL4_HTTP(NULL, NULL, "Entering get_server_idx %s, request_type = %d, with line=%d", hostname, request_type, line_number);

  int i=0; 

  if (create_svr_table_entry(&i) != SUCCESS)
     return -1;

  if(gNAServerHost == -1)
  {
    gNAServerHost = copy_into_big_buf("NA", 0);
  }

  gServerTable[i].server_hostname = gNAServerHost;
  gServerTable[i].server_port = 0;
  gServerTable[i].request_type = request_type;
  g_cur_server++;

  NSDL4_HTTP(NULL, NULL, "get_svr_idx = %d", i);
  return (i);
}

short
get_server_idx(char *hostname, int request_type, int line_number)
{
  int ii;
  int server_port;
  unsigned short rec_server_port;
  int hostname_len, recorded_hostname_len;
  char *hptr;
  char *tmp_ptr , *tmp_hostname; 
  int ret;

  //struct  hostent *hp;
  NSDL4_HTTP(NULL, NULL, "Entering get_server_idx %s, request_type = %d, with line=%d", hostname, request_type, line_number);

  if (!(hostname))
  {
    NS_EXIT(1, "USE_HOST mode is Host Header but no Host header on line =%d\n", line_number);
  }

  // This is done to support ipv6 hostname in script . This is done temporarily to validate ip . Original hostname is not modified 
  // We will copy hostname in a tmp_hostname to use it later .   
  MY_MALLOC (tmp_hostname, strlen(hostname) + 1, "tmp_hostname", -1);
  // Copy hostname to tmp_hostname 
  strcpy(tmp_hostname, hostname);
  // Check if ipv6 starts with '[' 
  if (tmp_hostname[0] == '[')
  {
    hptr = tmp_hostname + 1; 
    // We also need to remove ']'. This is required because we need to validate ipv6 address . Which is not possible with '[]'
    if ((tmp_ptr = rindex(tmp_hostname , ']'))) // This means ] is present
      *tmp_ptr = '\0';
  } 
  else { 
    hptr = tmp_hostname; 
  }
  NSDL4_HTTP(NULL, NULL, "Hostname after parsing %s . This is done temporarily to validate ip . orginal host name is not modifyied . hostname is %s ", hptr , hostname);

  // Check for ip version types (ipv4 or ipv6)
  ret = is_valid_ip(hptr);
  // Free tmp_hostname here 

  // Seperate out host name and port hptr will now point to host and server_port will store the port given by user .  
  hptr = nslb_split_host_port(hostname, &server_port);

  if(!hptr)
    NS_EXIT(-1, "Error: Invalid input = [%s]", tmp_hostname);

  free(tmp_hostname);
  //host len we need to comapre the new host name with existting hosts. Also this lenth doesnot include port
  hostname_len = strlen(hptr); 
  NSDL4_HTTP(NULL, NULL, "hostname = [%s], server_port = [%d] ret is [%d]  ", hptr, server_port, ret );

  // If port is not specified we will set default port for ipv4 as well as ipv6 
  if (server_port == 0) 
  {
    switch (request_type)
    {
     case HTTP_REQUEST:
          if (ret == IPv6){
            server_port = 6880;
          } else {
            server_port = 80;
          }
          NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
          break;
     case HTTPS_REQUEST:
         if (ret == IPv6){
           server_port = 6443;
         } else {
           server_port = 443;
         }
         NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
         break;

#ifdef RMI_MODE

     case JBOSS_CONNECT_REQUEST:
          server_port = 1099;
          break;
#endif
     case SMTP_REQUEST:
         if (ret == IPv6){
           server_port = 6825;
         } else {
          server_port = 25;
         }
         NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
         break;
     case SMTPS_REQUEST:
         if (ret == IPv6){
           server_port = 6825;
         } else {
          server_port = 465;
         }
         NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
         break;

     case POP3_REQUEST:
         if (ret == IPv6){
           server_port = 6811;
         } else {
          server_port = 110;
         }
         NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
         break;

     case SPOP3_REQUEST:
         if (ret == IPv6){
           server_port = 6811;
         } else {
          server_port = 995;
         }
         NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
         break;

     case FTP_REQUEST:
         if (ret == IPv6){
           server_port = 6821;
         } else {
          server_port = 21;
         }
         NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
         break;

     case DNS_REQUEST:
         if (ret == IPv6){
           server_port = 6853;
         } else {
          server_port = 53;
         }
         NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
         break;

     case LDAP_REQUEST:
          if (ret == IPv6){
            server_port = 6389; 
          } else{ 
           server_port = 389;
          }
          NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
          break;

     case LDAPS_REQUEST:
          if (ret == IPv6){
            server_port = 6636; 
          } else{ 
           server_port = 636;
          }
          NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
          break;
           NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);

     case IMAP_REQUEST:
          if (ret == IPv6){
            server_port = 6143; 
          } else{ 
           server_port = 143;
          }
          NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
          break;

     case IMAPS_REQUEST:
          if (ret == IPv6){
            server_port = 6993; 
          } else{ 
           server_port = 993;
          }
          NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
          break;
      
     case JRMI_REQUEST:
          server_port = 1099;
          break; 

     case WS_REQUEST:
         if (ret == IPv6){
            server_port = 6880;
          } else {
          server_port = 80;
          }
          NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
          break;

     case WSS_REQUEST:
         if (ret == IPv6){
            server_port = 6443;
          } else {
           server_port = 443;
          }
          NSDL1_HTTP(NULL, NULL, "Setting HTTP Port=%d", server_port);
          break; 
    }
  }

  NSDL1_HTTP(NULL, NULL, "g_cur_server = %d", g_cur_server);
  // check if the hostname already exit in the server table
  for (ii = 0; ii <= g_cur_server; ii++)
  {
    NSDL2_WS(NULL, NULL,"ii = %d, server_hostname = %s, request_type = %d, server_port = %d", 
                         ii, RETRIEVE_BUFFER_DATA((unsigned int) gServerTable[ii].server_hostname), gServerTable[ii].request_type,
                          gServerTable[ii].server_port);
    recorded_hostname_len = find_host_name_length_without_port((RETRIEVE_BUFFER_DATA((unsigned int) gServerTable[ii].server_hostname)), &rec_server_port);
    if (((recorded_hostname_len == hostname_len) && 
         !strncmp(RETRIEVE_BUFFER_DATA((unsigned int) gServerTable[ii].server_hostname), hostname, hostname_len)) &&
              (gServerTable[ii].server_port == server_port))
    {
      NSDL2_WS(NULL, NULL,"Request type is = %d, g_cur_server = %d, gServerTable[ii].request_type = %d", request_type, g_cur_server, gServerTable[ii].request_type);
      
      if (gServerTable[ii].request_type == request_type)
        break;
      else if(((gServerTable[ii].request_type == HTTP_REQUEST || gServerTable[ii].request_type == WS_REQUEST || gServerTable[ii].request_type == SOCKET_REQUEST) && (request_type == HTTP_REQUEST || request_type == WS_REQUEST || request_type == SOCKET_REQUEST)) || ((gServerTable[ii].request_type == HTTPS_REQUEST || gServerTable[ii].request_type == WSS_REQUEST || gServerTable[ii].request_type == SSL_SOCKET_REQUEST) &&(request_type == HTTPS_REQUEST || request_type == WSS_REQUEST || request_type == SSL_SOCKET_REQUEST)))
        continue;
      else
      {
        NS_EXIT(-1, "Error: Same port %d is used for both http and https for recorded host %s.", gServerTable[ii].server_port, hostname);
      }
    }
  }
  if (ii > g_cur_server) 
  {
    //create new server entry
    if (create_svr_table_entry(&ii) != SUCCESS)
       return -1;
    if ((gServerTable[ii].server_hostname = copy_into_big_buf(hptr, 0)) == -1)
    {
      NS_EXIT(-1, "get_server_idx: failed to copy into big buffer\n");
    }
    gServerTable[ii].server_port = (unsigned short)server_port;
 
    //printf("Anuj: get_server_idx(): The hostname=%s, server_port=%d\n", hostname, server_port);
    //printf("Anuj: get_server_idx(): The hostname=%s, server_port=%d\n", RETRIEVE_BUFFER_DATA((unsigned int) gServerTable[ii].server_hostname), gServerTable[ii].server_port);
    // Resolve address

#if 0

     bzero( (caddr_t) &gServerTable[ii].saddr, sizeof(struct sockaddr_in) );
     gServerTable[ii].saddr.sin_family = AF_INET;
     if (hend_ptr) *hend_ptr = '\0';
     if ((hp = gethostbyname(hostname)))
     {
       bcopy(hp->h_addr, (char *)&(gServerTable[ii].saddr.sin_addr.s_addr), hp->h_length);
     }
     else
     {
       printf("Host <%s> specified by Host Header is not a valid hostname.\n", hostname);
       exit(1);
     }
     gServerTable[ii].saddr.sin_port = htons(gServerTable[ii].server_port);

#endif
     gServerTable[ii].request_type = request_type;
     g_cur_server++;
  }
  //sprintf(hostname, "%s:%d", hostname, server_port);
  return ii;
}

//FREE non_shared memory
void free_host_table_mem()
{
  int host_idx, total_rec_host_entries, g;
  GrpSvrHostSettings *svr_host_settings = NULL;
  PerGrpHostTableEntry *host_table = NULL;

  NSDL3_HTTP(NULL, NULL, "Method called");

  //For Each Group
  for(g = 0; g < total_runprof_entries; g++)   //group
  {
    svr_host_settings = &runProfTable[g].gset.svr_host_settings;
    NSDL2_HTTP(NULL, NULL, "Free memory for Group Settings = %p", svr_host_settings);
    if(svr_host_settings != NULL)
      FREE_NON_SHR_MEM(svr_host_settings);
  }
}

void fill_loc_idx_svr_host(GrpSvrHostSettings *svr_host_settings_ptr)
{
  int h,s, loc_idx = -1;
  PerHostSvrTableEntry *svr_table_ptr = NULL;

  NSDL4_HTTP(NULL, NULL, "Method called, svr_host_settings_ptr = %p", svr_host_settings_ptr);
  for(h = 0; h < svr_host_settings_ptr->total_rec_host_entries; h++)
  {
    svr_table_ptr = svr_host_settings_ptr->host_table[h].server_table;
    for(s = 0; s < svr_host_settings_ptr->host_table[h].total_act_svr_entries; s++, svr_table_ptr++)
    {
      NSDL4_HTTP(NULL, NULL, "server_name = %s, loc_name = %s, loc_idx = %d", svr_table_ptr->server_name, svr_table_ptr->loc_name, svr_table_ptr->loc_idx);
      if(svr_table_ptr->loc_idx == -1)
        svr_table_ptr->loc_idx = find_locattr_idx(svr_table_ptr->loc_name);
      NSDL4_HTTP(NULL, NULL, "svr_table_ptr->loc_idx = %d", svr_table_ptr->loc_idx);
    
      if (svr_table_ptr->loc_idx == -1) 
        NS_EXIT(-1, "unknown location %s for server %s", svr_table_ptr->loc_name, svr_table_ptr->server_name);
    
      if ((loc_idx = find_inusesvr_idx(svr_table_ptr->loc_idx)) == -1) 
      {
        if ((create_inusesvr_table_entry(&loc_idx) != SUCCESS)) 
        {
          NS_EXIT(-1, "location_data_compute(): Error in creating a inuse_user table entry");
          return;
        }
        NSDL4_HTTP(NULL, NULL, "loc_idx = %d",loc_idx);
        inuseSvrTable[loc_idx].location_idx = svr_table_ptr->loc_idx;
        svr_table_ptr->loc_idx = loc_idx;
      } 
      else 
        svr_table_ptr->loc_idx = loc_idx;

      NSDL4_HTTP(NULL, NULL, "svr_table_ptr = %p, loc_name = %s, loc_idx = %d",svr_table_ptr, svr_table_ptr->loc_name, svr_table_ptr->loc_idx);
    }
  }
  return;
}

void fill_server_loc_idx()
{
  int g;
  GrpSvrHostSettings *svr_host_settings_ptr = NULL;

  NSDL4_HTTP(NULL, NULL, "Method called");
   
  //ALL loc_idx  
  svr_host_settings_ptr = &group_default_settings->svr_host_settings;
  NSDL4_HTTP(NULL, NULL, "Calling for ALL");
  fill_loc_idx_svr_host(svr_host_settings_ptr);

  for(g=0; g < total_runprof_entries; g++)
  {
    svr_host_settings_ptr = &runProfTable[g].gset.svr_host_settings;
    NSDL4_HTTP(NULL, NULL, "Calling for GRP %d",g);
    fill_loc_idx_svr_host(svr_host_settings_ptr);
  }
  return;
}

PerHostSvrTableEntry_Shr* find_actual_server_shr(char* serv, int grp_idx, int host_idx)
{
  int svr_idx;
  unsigned short server_port;
  int hostname_len;
  NSDL4_HTTP(NULL, NULL, "Method called - serv %s grp_idx %d host_idx %d", serv, grp_idx, host_idx);
 
  PerGrpHostTableEntry_Shr *grp_host_ptr;

  FIND_GRP_HOST_ENTRY_SHR(grp_idx, host_idx, grp_host_ptr); 
  if(!grp_host_ptr)
  {
    NSDL2_HTTP(NULL, NULL, "Couldn't find recorded host %d not found in group %d",host_idx, grp_idx);
    return NULL;
  }

  if(!serv)
  {
    NSDL2_HTTP(NULL, NULL, "Mapped to server is NULL");
    return &grp_host_ptr->server_table[0]; 
  }
  
  hostname_len = find_host_name_length_without_port(serv, &server_port);
  if ( (svr_idx = find_gserver_shr_idx(serv, server_port, hostname_len)) == -1) {
    NSDL2_HTTP(NULL, NULL, "Couldn't find  map_to_server %s not found", serv);
    return NULL;
  }
  FIND_GRP_HOST_ENTRY_SHR(grp_idx, svr_idx, grp_host_ptr);
  if(!grp_host_ptr)
  {
    NSDL2_HTTP(NULL, NULL, "Couldn't find  map_to_server host %d not found in group %d", svr_idx, grp_idx);
    return NULL;
  }
  NSDL2_HTTP(NULL, NULL, "server_name = %s, saddr = %s", grp_host_ptr->server_table[0].server_name, nslb_sockaddr_to_ip((struct sockaddr *)&grp_host_ptr->server_table[0].saddr, 1)); 
  return &grp_host_ptr->server_table[0]; 
}


void calc_static_host_shm_size()
{
  int g = 0;
  PerGrpStaticHostTable *static_host_settings = NULL;

  //For All
  NSDL2_HTTP(NULL, NULL, "Calling for group_default_settings");
  static_host_settings = &group_default_settings->per_grp_static_host_settings;
  CAL_STATIC_HOST_SHR_SIZE(static_host_settings);
  NSDL2_HTTP(NULL, NULL, "static_host_table_shm_size = %d", static_host_table_shm_size);
  
  //For each group
  NSDL2_HTTP(NULL, NULL, "Calling for Group Settings");
  for(g = 0; g < total_runprof_entries; g++)   //group
  {
    static_host_settings = &runProfTable[g].gset.per_grp_static_host_settings;
    CAL_STATIC_HOST_SHR_SIZE(static_host_settings);
  } 

}
 
void insert_totstatic_host_shr_mem()
{
  NSDL2_HTTP(NULL, NULL, "Method Called");
  PerGrpStaticHostTable *per_group_static_host_settings = NULL;
  StaticHostTable *tot_static_host_shr_mem = NULL;
  StaticHostTable *all_static_host_shr_mem = NULL;
  PerGrpStaticHostTable *per_group_static_host_settings_shr = NULL;
  int g;
  int total_static_host_entries = 0;
  int all_static_host_entries = 0;

  calc_static_host_shm_size(); 
  NSDL2_HTTP(NULL, NULL, "static_host_table_shm_size = %d", static_host_table_shm_size);
  if(!static_host_table_shm_size) {
    NSDL2_HTTP(NULL, NULL, "static_host_table_shm_size is zero");
    return ;
  }

  g_static_host_table_shr_mem = (StaticHostTable *) do_shmget(static_host_table_shm_size, "StaticHostTable_Shr");
  tot_static_host_shr_mem = g_static_host_table_shr_mem;

  //For All
  per_group_static_host_settings = &group_default_settings->per_grp_static_host_settings;
  total_static_host_entries = per_group_static_host_settings->total_static_host_entries;
  all_static_host_entries = total_static_host_entries; 
  all_static_host_shr_mem = tot_static_host_shr_mem;

  FILL_STATIC_HOST_SHR_MEM(per_group_static_host_settings);
  per_group_static_host_settings->static_host_table = all_static_host_shr_mem;
  per_group_static_host_settings->total_static_host_entries = all_static_host_entries;

  // For each group 
  /*-----------------------------GRP_ENTRIES---------------------------------------*/
  for(g=0; g < total_runprof_entries; g++)
  {
    per_group_static_host_settings = &runProfTable[g].gset.per_grp_static_host_settings;
    per_group_static_host_settings_shr = &runprof_table_shr_mem[g].gset.per_grp_static_host_settings;
    total_static_host_entries = per_group_static_host_settings->total_static_host_entries;
    if(total_static_host_entries)
    {
      per_group_static_host_settings_shr->static_host_table = tot_static_host_shr_mem;
      per_group_static_host_settings_shr->total_static_host_entries = total_static_host_entries;
      FILL_STATIC_HOST_SHR_MEM(per_group_static_host_settings);
    }
  }
  is_static_host_shm_created = 1;
}

int create_per_grp_static_host_table_entries(PerGrpStaticHostTable *per_grp_static_host_settings, int *rnum)
{   
  NSDL1_PARSING(NULL, NULL, "Method Called");
  
  if(per_grp_static_host_settings->total_static_host_entries == per_grp_static_host_settings->max_static_host_entries)
  {
    MY_REALLOC_EX (per_grp_static_host_settings->static_host_table, (per_grp_static_host_settings->max_static_host_entries + DELTA_TOT_STATIC_HOST_ENTRY) * sizeof(StaticHostTable), (per_grp_static_host_settings->max_static_host_entries * sizeof(StaticHostTable)), "StaticHostTable", -1); 
   per_grp_static_host_settings->max_static_host_entries += DELTA_TOT_STATIC_HOST_ENTRY;
  }
  *rnum = per_grp_static_host_settings->total_static_host_entries++;
  NSDL2_PARSING(NULL, NULL, "rnum = %d", *rnum);
    
  return (SUCCESS);
} 

int kw_set_g_static_host(char *buf, GroupSettings *gset, int grp_idx, char *err_msg, int runtime_changes) 
{
  char keyword[MAX_DATA_LINE_LENGTH] = "";
  char host_name[MAX_DATA_LINE_LENGTH] = "";
  char group_name[MAX_DATA_LINE_LENGTH] = "";
  char ip[MAX_DATA_LINE_LENGTH] = "";
  char tmp[MAX_DATA_LINE_LENGTH] = "";
  int rnum = 0;
  int family = 0;
  int i;

  NSDL2_PARSING(NULL, NULL, "Method called.buf = [%s]", buf);
  int num_args = 0;

  num_args = sscanf(buf, "%s %s %s %s %s", keyword, group_name, host_name, ip, tmp);

  if(num_args != 4) {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_STATIC_HOST_USAGE, CAV_ERR_1011117, CAV_ERR_MSG_1);
  }
  
  val_sgrp_name(buf, group_name, 0);

  //validate ip address
  int ret = is_valid_ip(ip);
  if(ret == IPv4)
  {
    family =  AF_INET;
  }
  else if (ret == IPv6)
    family =  AF_INET6;
  else {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_STATIC_HOST_USAGE, CAV_ERR_1011119, CAV_ERR_MSG_3);
  }

  PerGrpStaticHostTable *per_grp_static_host_settings = &gset->per_grp_static_host_settings;
 
  for( i = 0; i < per_grp_static_host_settings->total_static_host_entries; i++)
  {
    if (!strcmp(host_name, per_grp_static_host_settings->static_host_table[i].host_name))
    {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_STATIC_HOST_USAGE, CAV_ERR_1011118, CAV_ERR_MSG_3);
    }
  }  
  create_per_grp_static_host_table_entries(per_grp_static_host_settings, &rnum);

  NSDL2_PARSING(NULL, NULL, "host_name = %s ip = %s per_grp_static_host_settings = %p", host_name, ip, per_grp_static_host_settings);

  snprintf(per_grp_static_host_settings->static_host_table[rnum].host_name, MAX_DATA_LENGTH, "%s", host_name);
 
  snprintf(per_grp_static_host_settings->static_host_table[rnum].ip, 64, "%s", ip);

  per_grp_static_host_settings->static_host_table[rnum].family =  family;

  NSDL2_PARSING(NULL, NULL, "rnum = %d host_name = %s ip = %s family = %d", rnum, per_grp_static_host_settings->static_host_table[rnum].host_name,per_grp_static_host_settings->static_host_table[rnum].ip, family);
  
  return 0;
}
