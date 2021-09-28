/***********************************************************************************************
 * Name    :    ns_parse.c
 * Author  :    Sanjay
 * Purpose :    Contain functions ns_parse_use_src_ip() that will be
                used to parse the keyword USE_SRC_IP and SRC_IP_LIST.

Syntax of USE_SRC_IP is
	USE_SRC_IP src_ip_mode ip_list_mode

Possible values of use_ip_mode are
	1: All Vusers share a pool of IP's
	2: All Vusers use an uniq use IP's
	0: All Users use only netstorm's primary IP (default). This mode needs no ip_list_mode
	3: All Vusers share a pool of IP's and next argument gives the list of IP's to use
	4: All Vusers use an uniq IP and next argument gives the list of IP's to use

Possible values if ip-list-mode are :
	1-x: Use x IP's from assigned aliased IP's
	999999999: Use all IP's from assigned aliased IP's
	0: Use the IP's specified by SRC_IP_LIST keyword(s).

Syntax of SRC_IP_LIST is
SRC_IP_LIST start-ip num-ips
SRC_IP_LIST can repeat more than 1 time
Each SRC_IP_LIST is specifies IP list  as start-ip and number-of-ip  pairs.
SRC_IP_LIST is used only when ip-list-mode fiels of USR_SRC_IP is 0.

Examples:
To specify - use all assigned aliased IP's as a shared pool of source IP addressed for all Vusers -
USE_SRC_IP 1 999999999

To specify - use 5 assigned aliased IP's as a shared pool of source IP addressed for all Vusers -
USE_SRC_IP 1 5

Note: An error message would be returned, if assigned aliased IP's are less than requested IP's

To specify - use specified IP's  as a shared pool of source IP addressed for all Vusers -
USE_SRC_IP 1 0 192.168.0.32:12 192.168.1.21:1

Note: If the requested IP ranges  does not fall in assigned range, an error would be retuned.

To specify - use all assigned aliased IP's for unique source IP addressed for all Vusers -
USE_SRC_IP 2 999999999

Note: An error would be retuned, if the assigned alias IP addresses are less than virtual users

To specify - use 10 of the  assigned aliased IP's for unique source IP addressed for all Vusers -
USE_SRC_IP 2 10

Note: An error would be retuned, if the specified number of IP's is less than assigned alias
IP addresses or virtual users

To specify - use specified IP's  as unique source IP addressed for all Vusers -
USE_SRC_IP 2 0 192.168.0.32:12 192.168.1.21:1
SRC_IP_LIST 192.168.0.32 12
SRC_IP_LIST 192.168.1.21 1

Note: If the requested IP ranges do not fall in assigned range, an error would be retuned.
Also, if the specified addresses are less than number of virtual users, an error would be returned.

 * Output  :    Creates a file logs/TRxxx/ip_address_file. Replace xxx
                by test-id that is avaibale in global variable testidx.
				This file will have a list of IP's that should be used by test.
                One IP address on 1 line. Such as
                192.168.0.3
                192.167.3.1
                :
 * Modification History:
 *   01/05/06: Sanjay - Initial Version
 *   23/05/06: Sanjay - Modified ns_parse_use_src_ip() function to parse new
                        keyword SRC_IP_LIST.
 *   14/03/18: Meenakshi - Modified USE_SRC_IP, SRC_IP_LIST, IP_VERSION_MODE, USE_SAME_NETID_SRC
                           to group specific
***************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include <sys/stat.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "url.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_global_settings.h"
#include "ipmgmt_utils.h"
#include "nsu_ipv6_utils.h"
#include "ns_parse_scen_conf.h"
#include "ns_parse_src_ip.h"
#include "ns_exit.h"
#include "ns_trace_level.h"
#include "src_ip.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "wait_forever.h"

//unsigned char *group_ip_entries_index_table = NULL;
//void ns_parse_use_src_ip (char * entry);

//int src_ip_mode;

#if 0
#ifdef TEST
int testidx=8000; // Used for Test
char *g_ns_wdir="/home/cavisson/work";
short g_src_ip_mode;
int ip_list_mode_file;
extern char *g_ip_fname="/tmp/ip_address_file";
int main (int argc, char *argv[])
{
    char cmd_buf[4096];
    char entry[100]="USE_SRC_IP 1 0";
    int ret=ns_parse_use_src_ip(entry,100);
    strcpy(entry, "SRC_IP_LIST 8.0.0.2 90");
    ret=ns_parse_use_src_ip(entry,100);
    //printf ("ret = %d",ret);

    sprintf (cmd_buf, "%s/bin/nsi_is_ip_ok %d 20 %hd %d", testidx, g_src_ip_mode, ip_list_mode_file);
    if(system(cmd_buf) != 0)
		return -1;
    return ret;
}
#else
// deifined by netstorm
extern int testidx;
extern char g_ns_wdir[];
extern short g_src_ip_mode;
extern char *g_ip_fname;
extern int ip_list_mode_file;
#endif
#endif

//static int FLag_IP_VERSION_MODE=0;
#define GET_ARG_TOKEN(ptr,msg) if (!(ptr = strtok (NULL, " "))) {\
	printf(msg);\
	return;\
    }

#define ERROR_RETURN(msg) {\
	printf(msg);\
	return;\
    }

//Parses USE_SRC_IP and SRC_IP_LIST keyword entries in scenario config file
//Input: entry pointes to keyword entry and num_vusers is the number of vusers for the test
//Output: returns 0 success and-1 on failures
//Creates a file logs/TRxxx/ip_address_file. Replace xxx by test-id that is avaibale in global
//variable testidx. This file will have a list of IP's that should be used by test.
//One IP address on 1 line. Such as
//192.168.0.3
//192.167.3.1
//:
//For src_ip_mode 0, ip_address_file is not created.



/*****************************************************************************************************
  SYNTAX for G_USE_SRC_IP 
  G_USE_SRC_IP <group_name> use_ip_mode  ip_list_mode
  Where use_ip_mode could be 0, 1, 2, 3, 4 
  0 = it will use ns primary ip . 
  1, 3  = All users share a pool of ip(s). 
  2, 4  = Al uses uses uniques ip(s). 
  Note for mode 3 nd 4 user specifies its own ip file whereas for 1 ,2 it will read aliased ip 

  where ip_list_mode could be :
  1-n use n ip(s) from assigned aliased ip's.
  999999999 Use all IP's from assigned aliased IP's.
  0:USe the IP's specified by G_SRC_IP_LIST 
  ip_entries are done in file $NS_WDIR/logs/testidx/ip_address_file in case group_name is ALL
  ip_entries are done in file $NS_WDIR/logs/testidx/ip_address_file_<grp_name> in case group_name is not ALL

Note:- $NS_WDIR/logs/testidx/ip_address_file is made only in case entry(G_USE_SRC_IP or G_SRC_IP_LIST) 
       is specified in scenario.
Note:- $NS_WDIR/logs/testidx/ip_address_file is not made in case of primary mode(G_USE_SRC_IP 0)
******************************************************************************************************/
int kw_set_use_src_ip (char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char g_ip_fname[MAX_DATA_LINE_LENGTH];
  char ip_list_mode_file[MAX_DATA_LINE_LENGTH] = "";
  char cmd_buf[4096];
  char ip_mode[MAX_DATA_LINE_LENGTH] = "";
  char default_src_ip_list_file[MAX_DATA_LINE_LENGTH] = "";
  short src_ip_mode = 0;
  int num, ip_list_mode;

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s", buf);
  if(loader_opcode == MASTER_LOADER)
  {
    NSDL2_PARSING(NULL, NULL, "Not parsing Keyword G_USE_SRC_IP as it is Master");
    return 0;
  }

  if ((num = sscanf(buf, "%s %s %s %s", keyword, sg_name, ip_mode, ip_list_mode_file)) < 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_USE_SRC_IP_USAGE, CAV_ERR_1011225, CAV_ERR_MSG_1);
  }

  NSDL2_PARSING(NULL, NULL, "num = %d ", num);

  val_sgrp_name(buf, sg_name, 0);//validate group name

  src_ip_mode = (short)atoi(ip_mode);
  if(!strcmp(sg_name, "ALL"))
    sprintf (g_ip_fname, "%s/logs/TR%d/ip_address_file", g_ns_wdir, testidx);
  else
    sprintf (g_ip_fname, "%s/logs/TR%d/ip_address_file_%s", g_ns_wdir, testidx, sg_name);

  if(src_ip_mode > USE_UNIQUE_IP_FROM_FILE)
    NSTL1(NULL, NULL, "src_ip_mode cannot be greater than 4");
 
  switch (src_ip_mode) {
    case USE_PRIMARY_IP:
      gset->src_ip_mode = SRC_IP_PRIMARY; //No further processing required as primary IP is to be used
      return 0;
    case USE_SHARED_IP:
    case USE_SHARED_IP_FROM_FILE:
      gset->src_ip_mode = SRC_IP_SHARED;
    break;
    case USE_UNIQUE_IP:
    case USE_UNIQUE_IP_FROM_FILE:
      gset->src_ip_mode = SRC_IP_UNIQUE;
    break;
    default:
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_USE_SRC_IP_USAGE, CAV_ERR_1011225, CAV_ERR_MSG_3);
  }

  //Get ip_list_mode Arguments
  if(ip_list_mode_file[0] == '\0')
   NSTL1(NULL, NULL, "ip_list_mode not provided with USE_SRC_IP keyword");

  if (src_ip_mode > USE_UNIQUE_IP) {
    //ptr contains the list of ip's
    sprintf (cmd_buf, "cp %s %s", ip_list_mode_file, g_ip_fname);
    if (system(cmd_buf) != 0) {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_USE_SRC_IP_USAGE, CAV_ERR_1000019, cmd_buf, errno, nslb_strerror(errno));
    }
    return 0;
  }

  ip_list_mode = atoi(ip_list_mode_file);  // ip_list_mode_file contains ip_list_mode  It can be 1-x , 999999999, 0.

  //Fetch ip from from G_SRC_IP_LIST if list is provided for that 'group' if not take it from 'ALL' otherwise throw an error
  if (ip_list_mode == 0) { // take ip from src_ip_list keyword

    NSDL2_PARSING(NULL, NULL, "gset->Flag_SRC_IP_LIST = %d, default_Flag_SRC_IP_LIST = %d", gset->Flag_SRC_IP_LIST, default_Flag_SRC_IP_LIST);

    sprintf (default_src_ip_list_file, "%s/logs/TR%d/ip_address_file_default", g_ns_wdir, testidx);
    if(!gset->Flag_SRC_IP_LIST)
    {
      if (!default_Flag_SRC_IP_LIST)
      {
        NSTL1(NULL, NULL, "Error: SRC_IP_LIST not exists for group '%s'", sg_name);
        exit(-1);
      }  
      sprintf (cmd_buf, "cp  %s %s", default_src_ip_list_file, g_ip_fname); 
      if (system(cmd_buf) != 0)
      {
        NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_USE_SRC_IP_USAGE, CAV_ERR_1000019, cmd_buf, errno, nslb_strerror(errno));
      }
    }
     //Set ip_address_file name global var. Check failure code: not enough IP'
  } else {
    //In  case ip_list_mode_file is not zero it will read specified ip(s) from sys/ip_entries.
    sprintf (cmd_buf, "%s/bin/nsu_show_address -v %d -l %d > %s",
    g_ns_wdir, gset->ip_version_mode, ip_list_mode, g_ip_fname);
    NSDL2_PARSING(NULL, NULL, "cmd_buf = %s", cmd_buf);
    if (system(cmd_buf) != 0){
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_USE_SRC_IP_USAGE, CAV_ERR_1011225, "Number of IP used is greater than assign IP's in system");
    }
  }
  return 0;
}

/************************************************************************************************************
  SYNTAX for G_SRC_IP_LIST 
  G_SRC_IP_LIST <group_name> start_ip num_ip
  where start_ip and num_ip are passed to nsi_validate_ip

  ip_entries are done in file $NS_WDIR/logs/testidx/ip_address_file in case group_name is ALL
  ip_entries are done in file $NS_WDIR/logs/testidx/ip_address_file_<grp_name> in case group_name is not ALL
************************************************************************************************************/
int kw_set_src_ip_list(char *buf, GroupSettings *gset, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char g_ip_fname[MAX_DATA_LINE_LENGTH];
  char sip[MAX_DATA_LINE_LENGTH] = "";
  char cmd_buf[4096];
  int nip = 0, num = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s, gset->Flag_SRC_IP_LIST = %d", buf, gset->Flag_SRC_IP_LIST);
  if(loader_opcode == MASTER_LOADER)
  {
    NSDL2_PARSING(NULL, NULL, "Not parsing Keyword G_SRC_IP_LIST as it is Master");
    return 0;
  }

  if ((num = sscanf(buf, "%s %s %s %d", keyword, sg_name, sip, &nip)) < 4)
  {
    sprintf(err_msg, "Need 3 fields after keyword G_SRC_IP_LIST, num= %d, keyword = %s, sg_name = %s, sip = %s, nip = %d", 
                                            num, keyword, sg_name, sip, nip);
    return -1;
  }

  //Previously gset->Flag_SRC_IP_LIST check was parsing because G_USE_SRC_IP parsing first then G_SRC_IP_LIST
  //if (gset->src_ip_mode != SRC_IP_PRIMARY && (gset->Flag_SRC_IP_LIST || group_default_settings->Flag_SRC_IP_LIST))
  {
    val_sgrp_name(buf, sg_name, 0);//validate group name
    if(!strcmp(sg_name, "ALL"))
    {
      default_Flag_SRC_IP_LIST = 1;
      sprintf (g_ip_fname, "%s/logs/TR%d/ip_address_file_default", g_ns_wdir, testidx);
    }
    else
    {
      sprintf (g_ip_fname, "%s/logs/TR%d/ip_address_file_%s", g_ns_wdir, testidx, sg_name);
      gset->Flag_SRC_IP_LIST = 1;
    }

    if(sip[0] == '\0')
      NSTL1(NULL, NULL, "start_ip not provided with SRC_IP_LIST keyword");
  
    if(nip <= 0)
      NSTL1(NULL, NULL, "num ip not provided with SRC_IP_LIST keyword");

    sprintf (cmd_buf, "%s/bin/nsi_validate_ip -l %s-%d >> %s", g_ns_wdir, sip, nip, g_ip_fname);

    NSDL2_PARSING(NULL, NULL, "cmd_buf = %s", cmd_buf);
    if (system(cmd_buf) != 0) {
      NSTL1_OUT(NULL, NULL, "ERROR: Execution of (%s) failed", cmd_buf);
      exit(-1);
    }
    NSDL2_PARSING(NULL, NULL, "SRC_IP_LIST file exists for group %s at path %s", sg_name, g_ip_fname);
  }

  return 0;
} 
      
/*------------------------------------------------------------------------------------------------------------------------
 * Purpose   : This function will parse keyword SRC_PORT_MODE <mode> <num_retrires> <action> 
 *
 * Input     : buf   : SRC_PORT_MODE <mode> <num_retrires> <action> 
 *                     Mode              - 0 (Default)
 *                     num_retries       - number of retries"
                                           1000      (Default)"
 *                     action            - Action will say if should continue or not 
 *                     Eg: SRC_PORT_MODE 0 1000 0  <By Default>
 *
 *-----------------------------------------------------------------------------------------------------------------------*/
void kw_set_src_port_mode(char *buf) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[MAX_DATA_LINE_LENGTH];
  char num_retries[MAX_DATA_LINE_LENGTH] = "0";
  char action[MAX_DATA_LINE_LENGTH] = "0";
  char temp[MAX_DATA_LINE_LENGTH] = {0};
  char usages[MAX_DATA_LINE_LENGTH];
  int imode = 0;
  int inum_retries = 1000;
  int iaction = 0;
  int num_args = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called");

  // Making usages 
  sprintf(usages, "Usages:\n"
                  "SRC_PORT_MODE <mode> <num_retrires> <action> \n"
                  "Where:\n"
                  "  mode         - mode for src_port_mode\n"
                  "  num_retries        - provide number of retries\n"
                  "                       1000       (Default)\n"
                  "  action             - action need to be taken after socket bind failure and retry exhaust\n"
                  "                     - 0 : Continue (Default)\n"
                  "                     - 1 : Stop \n"
                  "  Eg: SRC_PORT_MODE 0 2000 1 \n");

  num_args = sscanf(buf, "%s %s %s %s %s", keyword, mode_str, num_retries, action, temp);

  if(num_args < 3 || num_args > 4)
  {
    NSDL2_PARSING(NULL, NULL, "Error: provided number of argument (%d) is wrong.\n%s", num_args - 1, usages);
    NSTL1(NULL, NULL, "Error: provided number of argument (%d) is wrong.\n%s", num_args - 1, usages);
    return;
  }

  NSDL2_PARSING(NULL, NULL, "keyword = [%s], mode_str = [%s], num_retries = [%s], action = [%s]",
                             keyword, mode_str, num_retries, action);   

  if(ns_is_numeric(mode_str))
  {
    imode = atoi(mode_str);
  }
  else
  {
    NSTL1(NULL, NULL, "Error: src_port_mode '%s' should be numeric.\n%s", mode_str, usages);
    return;
  }

  if(ns_is_numeric(num_retries))
  {
    inum_retries = atoi(num_retries);
  }
  else
  {
    NSTL1(NULL, NULL, "Error: num_retries '%s' should be numeric.\n%s", num_retries, usages);
    return;
  }

  if(ns_is_numeric(action))
  {
    iaction = atoi(action);
  }
  else
  {
    NSTL1(NULL, NULL, "Error: action '%s' should be numeric.\n%s", action, usages);
    return;
  }

  global_settings->num_retry_on_bind_fail = inum_retries;
  global_settings->action_on_bind_fail = iaction;
  global_settings->src_port_mode = imode;

  NSDL2_PARSING( NULL, NULL, "On end : src_port_mode = %d, num_retry_on_bind_fail = %d, action_on_bind_fail = %d",
                               global_settings->src_port_mode, global_settings->num_retry_on_bind_fail, global_settings->action_on_bind_fail);
}

// Creates an entry for global ips data structure
static int create_ip_table_entry(int *row_num) {
  NSDL2_IPMGMT(NULL, NULL, "Method called");
  if (total_ip_entries == max_ip_entries) {
    MY_REALLOC_EX (ips, (max_ip_entries + DELTA_IP_ENTRIES) * sizeof(IP_data), max_ip_entries * sizeof(IP_data), "ips", -1);
    if (!ips) {
      NSTL1_OUT(NULL, NULL, "create_ip_table_entry(): Error allocating more memory for ip entries");
      return(FAILURE);
    } else max_ip_entries += DELTA_IP_ENTRIES;
  }
  *row_num = total_ip_entries++;
  return (SUCCESS);
}

/**********************************************************************************************************
This function reads $NS_WDIR/logs/TR<id>/ip_address_file in case of ALL and 
$NS_WDIR/logs/TR<id>/ip_address_file_<group_name> in case of specific groups(G1, G2 etc)

global ips data structure is filled here for all groups(ALL, G1, G2 etc). gset->ips is filled 
for group for which ip_address_file_<group_name> is present.
***********************************************************************************************************/
void read_ip_file() {
  FILE *fp;
  int rnum = -1;
  char buf[MAX_DATA_LINE_LENGTH+1];
  char ip_char[MAX_DATA_LINE_LENGTH];
  char g_ip_fname[MAX_FILE_NAME];
  int ip_type;
  struct stat stat_buf;
  int grp_idx = 0;
  int i;
  GroupSettings *gset ;

  NSDL2_IPMGMT(NULL, NULL, "Method called");
  if(loader_opcode == MASTER_LOADER)
  {
    NSDL2_PARSING(NULL, NULL, "Not reading ip_address_file as it is Master");
    return;
  }

  for(grp_idx  = -1; grp_idx < total_runprof_entries; grp_idx++) 
  { 
    if(grp_idx < 0)
    {
      sprintf(g_ip_fname, "%s/logs/TR%d/ip_address_file", g_ns_wdir, testidx);
      gset = group_default_settings;
    }
    else 
    {
      sprintf(g_ip_fname, "%s/logs/TR%d/ip_address_file_%s", g_ns_wdir, testidx, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name));
      gset = &runProfTable[grp_idx].gset;  
    } 
    NSDL2_IPMGMT(NULL, NULL, "g_ip_fname = %s", g_ip_fname);
    int ret = stat(g_ip_fname, &stat_buf);
    if(ret < 0)
    {
      NSDL2_IPMGMT(NULL, NULL, "Continuing for other group as ip_address_file %s is not present", g_ip_fname);
      continue;
    }
    if (stat_buf.st_size <= 0)
    {
      NSDL2_IPMGMT(NULL, NULL, "Continuing as other group as ip_address_file %s is empty", g_ip_fname);
      continue;
    }

    //initialize
    gset->num_ip_entries = 0;
    if ((fp = fopen(g_ip_fname, "r")) == NULL) {
      NSDL2_MISC(NULL, NULL, "read_ip_file(): Error opening %s - using default", g_ip_fname);
      continue;
    }
    //gset->ips = ips + total_ip_entries; 
    while (nslb_fgets(buf, MAX_DATA_LINE_LENGTH, fp, 1) != NULL) {
      buf[strlen(buf)-1] = '\0';
      sscanf(buf, "%s", ip_char);
      ip_type = check_ip_type(ip_char);
      if (ip_type != gset->ip_version_mode)
      {
        NSTL1(NULL, NULL, "Warning :  ip_type  and ip_version_modes mismatch. Ip_type is [%d] and IP_VERSION_MODE is  [%d]."
                              " Therefore discarding this entry. \nTo use ipv6, we need to specify IP_VERSION_MODE 2. \n"
                              "TO use ipv4, we need to specify IP_VERSION_MODE 1.", ip_type, gset->ip_version_mode); 
        NS_DUMP_WARNING("Ip_type  and Ip_version_modes mismatch. Ip_type is [%d] and IP_VERSION_MODE is  [%d]."
                              " Therefore discarding this entry. To use ipv6, we need to specify IP_VERSION_MODE 2."
                              "TO use ipv4, we need to specify IP_VERSION_MODE 1.", ip_type, gset->ip_version_mode); 
        continue; 
      } 
      if (create_ip_table_entry(&rnum) == FAILURE) return;
      else {
        if (!nslb_fill_sockaddr(&(ips[rnum].ip_addr), ip_char, 0)) {
          NSTL1_OUT(NULL, NULL, "read_ip_file(): Invalid address, ignoring <%s>", ip_char);
        }
        //In error case we are filling the ip. Will fix this later
          ips[rnum].ip_id = rnum;
          strcpy(ips[rnum].ip_str, ip_char);
      }
      gset->num_ip_entries++;
    }
    NSDL2_MISC(NULL, NULL, "gset->num_ip_entries = %d, scen_group_name = %s", gset->num_ip_entries, 
                            RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name));
      
    fclose(fp);
    if (rnum == -1) {
      NSTL1_OUT(NULL, NULL, "Error: There is some problem in ip configuration . Either IP_VERSION_MODE 1 "
                            "is used with ipv6 or IP_VERSION_MODE 2 is used with ipv4.");
      exit(-1);
    }
  }

  group_default_settings->ips = ips;
  int loc_total_ip_entries = group_default_settings->num_ip_entries;

  /* Use cases to initialize default/group's gset->ips with global ips
  Default ipfile present & has ip entries= 10  ==> (group_default_settings) gset->ips = ips[0]
  Group0 ipfile present & has ip enrties = 20  ==> (runProfTable[0].gset) gset->ips = ips[9]
  Group1 ipfile present & has ip enrties = 0   ==> (runProfTable[1].gset) gset->ips = ips[0]
  Group2 ipfile present & has ip enrties = 100 ==> (runProfTable[2].gset) gset->ips = ips[20]
  */

  MY_MALLOC(group_ip_entries_index_table, total_ip_entries * total_runprof_entries, "group_ip_entries_index_table", -1); 
  for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
  {
    gset = &runProfTable[grp_idx].gset; 
    if(!gset->num_ip_entries)
    {
      gset = group_default_settings;
    }
    else
    {
      gset->ips = ips + loc_total_ip_entries;
      loc_total_ip_entries += gset->num_ip_entries;
    }

    for(i=0; i<gset->num_ip_entries; i++)
    {
       NSDL2_MISC(NULL, NULL, "group_ip_entries_index_table index is = %d, total_group_ip_entries = %d, grp_idx = %d", 
                                   ((grp_idx * total_ip_entries ) + gset->ips[i].ip_id), total_group_ip_entries, grp_idx);
       group_ip_entries_index_table[(grp_idx * total_ip_entries ) + gset->ips[i].ip_id] = total_group_ip_entries++;  
    } 
  } 
}

/***********************************************************************
This function will select ipversion mode as specified by the user.
(1) Read file sys/ip_entries  
(2) validate whether specified mode is valid or not. 
(3) validate whether assigned ip's are respective to mode or not.
(4) This function will return 0 on success and -1 in error case . 
****************************************************************************/
int kw_set_ip_version_mode(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  short mode = 0; 
  int num = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %hd", keyword, sg_name, &mode)) < 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_IP_VERSION_MODE_USAGE, CAV_ERR_1011246, CAV_ERR_MSG_1);
  }
 
  char sMode ;  // Mode based on what IP versions are availanble in IP entries file
  char is_ipv4 = 0 , is_ipv6 =0 ; 

  if ((mode < IP_VERSION_MODE_AUTO) || (mode > IP_VERSION_MODE_IPV6))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_IP_VERSION_MODE_USAGE, CAV_ERR_1011246, CAV_ERR_MSG_3);
  }
  
  gset->ip_version_mode = mode;
  NSDL4_PARSING(NULL, NULL, " gset->ip_version_mode [%d]",  gset->ip_version_mode);
  
  if (read_ip_entries())
  {
    gset->ip_version_mode = IP_VERSION_MODE_IPV4;
    return 0;
  }

   //is_ipv4 = assignedIPinfo.client_ipv4 | assignedIPinfo.server_ipv4;
   //is_ipv6 = assignedIPinfo.client_ipv6 | assignedIPinfo.server_ipv6;
   //sMode = is_ipv4 ? IP_VERSION_MODE_IPV4 : is_ipv6 ? IP_VERSION_MODE_IPV6 : IP_VERSION_MODE_IPV4;
   if (assignedIPinfo.client_ipv4 || assignedIPinfo.server_ipv4)
     is_ipv4 = 1;

   if (assignedIPinfo.client_ipv6 || assignedIPinfo.server_ipv6)
     is_ipv6 = 1;
   
   if(is_ipv4)
     sMode = IP_VERSION_MODE_IPV4;
   else if (is_ipv6)
     sMode = IP_VERSION_MODE_IPV6;
   else {
     NSDL4_PARSING(NULL, NULL, "No ipv4 or ipv6 entries found in files . In this case system will use primary ip(s).. Therefore returning sucess in this case "); 
     return 0;
   }
     
  
  switch (mode) {
    NSDL4_PARSING(NULL, NULL, " gset->ip_version_mode [%d]",  mode);
    case IP_VERSION_MODE_AUTO: 
      gset->ip_version_mode = sMode;
      if (is_ipv4 && is_ipv6){
        NSTL1(NULL, NULL, "Warning: In case of Auto mode if both ipv4 and ipv6 ip(s) are assigned to system. Default Ipv4 mode is selected");
        NS_DUMP_WARNING("In case of auto mode if both IPv4 and IPv6 are assigned to system then IPv4 mode is selected");
      }
      break;
  
    case IP_VERSION_MODE_IPV6:
      if (!is_ipv6) {
        NSTL1(NULL, NULL, "Error: Ipv6 ip(s) are not assigned. We cannot use IP_VERSION_MODE 2 without assigning ipv6");
      }
    break;

    case IP_VERSION_MODE_IPV4:    
      if (!is_ipv4) {
        NSTL1(NULL, NULL, "Error : Ipv4 ip(s) are not assigned. We cannot use IP_VERSION_MODE 1 without assigning ipv4");
      }
    break;
  }
      
  return 0; 
}

/*****************************************************************************************************************************************
This keyword has three modes
  use_same_netid_src : 0 : default mode. No server address checking with client address, any client may hit any server
  use_same_netid_src : 1 : Server address (to be hit, actual servers) need not be a netocean address, But only clients 
                           from same subnet may hit the server. 
  use_same_netid_src : 2 : Server address (to be hit, actual servers) must be netocean address, only clients from same 
                           subnet may hit the server. 
*****************************************************************************************************************************************/
int kw_set_use_same_netid_src(char *buf, short *use_same_netid_src, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int num = 0;
  short same_netid_src = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %hd", keyword, sg_name, &same_netid_src)) < 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_USE_SAME_NETID_SRC_USAGE, CAV_ERR_1011230, CAV_ERR_MSG_1);
  }

  if(same_netid_src < 0 || same_netid_src > 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_USE_SAME_NETID_SRC_USAGE, CAV_ERR_1011230, CAV_ERR_MSG_3);
  }

  *use_same_netid_src = same_netid_src; 
  NSDL2_IPMGMT(NULL, NULL, "*use_same_netid_src = %d", *use_same_netid_src);

  return 0;
}
