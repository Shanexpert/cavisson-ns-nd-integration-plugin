/******************************************************************
 * Name    : ns_user_profile.c
 * Author  : Nikita Pandey
 * Purpose :Initalization of all user profile data and access functions.
 * Modification History: Code splitted from util.c, ns_parse_scen_conf.c, ns_kw_set_non_rtc.c
 *                       on Dec 21, 11 (Release 3.8.1)
 *                       Prachi- Changes format of keyword: UBROWSER on Nov 20, 2012 (Release 3.9.0) 
 *******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "util.h"
#include "ns_log.h"

#include "tmr.h"
#include "timing.h"

#include "logging.h"
#include "ns_alloc.h"

#include "ns_schedule_phases.h"

#include "netstorm.h"

#include "ns_wan_env.h"
#include "ns_alloc.h"

#include "ns_user_profile.h"
#include "nslb_util.h"
#include "ns_connection_pool.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "wait_forever.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

//extern void end_test_run( void );

// Keyword parsing and creation of non shared memory data strcutures
static int create_linechar_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_linechar_entries == max_linechar_entries) {
    MY_REALLOC_EX (lineCharTable, (max_linechar_entries + DELTA_LINECHAR_ENTRIES) * sizeof(LineCharTableEntry), max_linechar_entries * sizeof(LineCharTableEntry), "lineCharTable", -1);
    if (!lineCharTable) {
      fprintf(stderr,"create_linechar_table_entry(): Error allocating more memory for linechar entries\n");
      return(FAILURE);
    } else max_linechar_entries += DELTA_LINECHAR_ENTRIES;
  }
  *row_num = total_linechar_entries++;
  return (SUCCESS);
}

/*int find_freqattr_idx(char* name) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_freqattr_entries; i++)
    if (!strncmp(RETRIEVE_BUFFER_DATA(freqAttrTable[i].name), name, strlen(name)))
      return i;

  return -1;
}

int find_machattr_idx(char* name) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_machattr_entries; i ++)
    if (!strncmp(RETRIEVE_BUFFER_DATA(machAttrTable[i].name), name, strlen(name)))
      return i;

  return -1;
}
*/

int create_accattr_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_accattr_entries == max_accattr_entries) {
    MY_REALLOC_EX (accAttrTable, (max_accattr_entries + DELTA_ACCATTR_ENTRIES) * sizeof(AccAttrTableEntry), max_accattr_entries * sizeof(AccAttrTableEntry), "accAttrTable", -1);
    if (!accAttrTable) {
      fprintf(stderr,"create_accattr_table_entry(): Error allocating more memory for accattr entries\n");
      return(FAILURE);
    } else max_accattr_entries += DELTA_ACCATTR_ENTRIES;
  }
  *row_num = total_accattr_entries++;
  return (SUCCESS);
}

int create_browser_screen_sixe_map_table_entry(int *row_num){
  NSDL2_WAN(NULL, NULL, "Method called total_br_sc_sz_map_entries = %d", total_br_sc_sz_map_entries);
  if (total_br_sc_sz_map_entries == max_br_sc_sz_entries) {
    MY_REALLOC_EX (brScSzTable, (max_br_sc_sz_entries + DELTA_SCREEN_SIZE_ENTRIES) * sizeof(BRScSzMapTableEntry), max_br_sc_sz_entries * sizeof(BRScSzMapTableEntry), "BRScSzMapTableEntry", -1);
    if (!brScSzTable) {
      fprintf(stderr,"create_browser_screen_sixe_map_table_entry(): Error allocating more memory for accloc entries\n");
      return(FAILURE);
    } else max_br_sc_sz_entries += DELTA_SCREEN_SIZE_ENTRIES;
  }
  *row_num = total_br_sc_sz_map_entries++;
  return (SUCCESS);
}

int create_screen_size_table_entry(int *row_num){
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_screen_size_entries == max_screen_size_entries) {
    MY_REALLOC_EX (scSzeAttrTable, (max_screen_size_entries + DELTA_SCREEN_SIZE_ENTRIES) * sizeof(ScreenSizeAttrTableEntry), max_screen_size_entries * sizeof(ScreenSizeAttrTableEntry), "ScreenSizeAttrTableEntry", -1);
    if (!scSzeAttrTable) {
      fprintf(stderr,"create_screen_size_table_entry(): Error allocating more memory for accloc entries\n");
      return(FAILURE);
    } else max_screen_size_entries += DELTA_SCREEN_SIZE_ENTRIES;
  }
  *row_num = total_screen_size_entries++;
  return (SUCCESS);
}

int create_pf_bw_screen_size_table_entry(int *row_num){
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_pf_bw_screen_size_entries == max_pf_bw_screen_size_entries) {
    MY_REALLOC_EX (pfBwScSzTable, (max_pf_bw_screen_size_entries + DELTA_SCREEN_SIZE_ENTRIES) * sizeof(PfBwScSzTableEntry), max_pf_bw_screen_size_entries * sizeof(PfBwScSzTableEntry), "PfBwScSzTableEntry", -1);
    if (!pfBwScSzTable) {
      fprintf(stderr,"create_pf_bw_screen_size_table_entry(): Error allocating more memory for accloc entries\n");
      return(FAILURE);
    } else max_pf_bw_screen_size_entries += DELTA_SCREEN_SIZE_ENTRIES;
  }
  *row_num = total_pf_bw_screen_size_entries++;
  return (SUCCESS);
}

int create_accloc_table_etnry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_accloc_entries == max_accloc_entries) {
    MY_REALLOC_EX (accLocTable, (max_accloc_entries + DELTA_ACCLOC_ENTRIES) * sizeof(AccLocTableEntry), max_accloc_entries * sizeof(AccLocTableEntry), "accLocTable", -1);
    if (!accLocTable) {
      fprintf(stderr,"create_accloc_table_entry(): Error allocating more memory for accloc entries\n");
      return(FAILURE);
    } else max_accloc_entries += DELTA_ACCLOC_ENTRIES;
  }
  *row_num = total_accloc_entries++;
  return (SUCCESS);
}

int create_browattr_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_browattr_entries == max_browattr_entries) {
    MY_REALLOC_EX (browAttrTable, (max_browattr_entries + DELTA_BROWATTR_ENTRIES) * sizeof(BrowAttrTableEntry), max_browattr_entries * sizeof(BrowAttrTableEntry), "browAttrTable", -1);
    if (!browAttrTable) {
      fprintf(stderr,"create_browattr_table_entry(): Error allocating more memory for browattr entries\n");
      return(FAILURE);
    } else max_browattr_entries += DELTA_BROWATTR_ENTRIES;
  }
  *row_num = total_browattr_entries++;
  return (SUCCESS);
}

/*int create_freqattr_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_freqattr_entries == max_freqattr_entries) {
    MY_REALLOC_EX (freqAttrTable, (max_freqattr_entries + DELTA_FREQATTR_ENTRIES) * sizeof(FreqAttrTableEntry), max_freqattr_entries * sizeof(FreqAttrTableEntry), "freqAttrTable", -1);
    if (!freqAttrTable) {
      fprintf(stderr,"create_freqattr_table_entry(): Error allocating more memory for freqattr entries\n");
      return(FAILURE);
    } else max_freqattr_entries += DELTA_FREQATTR_ENTRIES;
  }
  *row_num = total_freqattr_entries++;
  return (SUCCESS);
}
*/

int create_locattr_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_locattr_entries == max_locattr_entries) {
    MY_REALLOC_EX (locAttrTable, (max_locattr_entries + DELTA_LOCATTR_ENTRIES) * sizeof(LocAttrTableEntry), max_locattr_entries * sizeof(LocAttrTableEntry), "locAttrTable", -1);
    if (!locAttrTable) {
      fprintf(stderr,"create_locattr_table_entry(): Error allocating more memory for locattr entries\n");
      return(FAILURE);
    } else max_locattr_entries += DELTA_LOCATTR_ENTRIES;
  }
  *row_num = total_locattr_entries++;
  return (SUCCESS);
}

/*int create_machattr_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_machattr_entries == max_machattr_entries) {
    MY_REALLOC_EX (machAttrTable, (max_machattr_entries + DELTA_MACHATTR_ENTRIES) * sizeof(MachAttrTableEntry), max_machattr_entries * sizeof(MachAttrTableEntry), "machAttrTable", -1);
    if (!machAttrTable) {
      fprintf(stderr,"create_machattr_table_entry(): Error allocating more memory for machattr entries\n");
      return(FAILURE);
    } else max_machattr_entries += DELTA_MACHATTR_ENTRIES;
  }
  *row_num = total_machattr_entries++;
  return (SUCCESS);
}
*/

int create_userindex_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_userindex_entries == max_userindex_entries) {
  MY_REALLOC_EX (userIndexTable, (max_userindex_entries + DELTA_USERINDEX_ENTRIES) * sizeof(UserIndexTableEntry), max_userindex_entries * sizeof(UserIndexTableEntry), "userIndexTable", -1);
    if (!userIndexTable) {
      fprintf(stderr,"create_userindex_table_entry(): Error allocating more memory for userindex entries\n");
      return(FAILURE);
    } else max_userindex_entries += DELTA_USERINDEX_ENTRIES;
  }
  *row_num = total_userindex_entries++;
  userIndexTable[*row_num].UPLoc_start_idx = -1;
  userIndexTable[*row_num].UPLoc_length = 0;
  userIndexTable[*row_num].UPAcc_start_idx = -1;
  userIndexTable[*row_num].UPAcc_length = 0;
  userIndexTable[*row_num].UPBrow_start_idx = -1;
  userIndexTable[*row_num].UPBrow_length = 0;
  //userIndexTable[*row_num].UPFreq_start_idx = -1;
  //userIndexTable[*row_num].UPFreq_length = 0;
  //userIndexTable[*row_num].UPMach_start_idx = -1;
  //userIndexTable[*row_num].UPMach_length = 0;
  userIndexTable[*row_num].UPBR_start_idx = -1;
  userIndexTable[*row_num].UPBR_length = 0;
  userIndexTable[*row_num].UPAccLoc_start_idx = -1;
  userIndexTable[*row_num].UPAccLoc_length = 0;
  return (SUCCESS);
}

int create_userprof_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_userprof_entries == max_userprof_entries) {
    MY_REALLOC_EX (userProfTable, (max_userprof_entries + DELTA_USERPROF_ENTRIES) * sizeof(UserProfTableEntry), max_userprof_entries * sizeof(UserProfTableEntry), "userProfTable", -1);
    if (!userProfTable) {
      fprintf(stderr,"create_userprof_table_entry(): Error allocating more memory for userprof entries\n");
      return(FAILURE);
    } else max_userprof_entries += DELTA_USERPROF_ENTRIES;
  }
  *row_num = total_userprof_entries++;
  return (SUCCESS);
}

int insert_linechar_table_entry (char *src, char *dst, int fw_lat, int rv_lat, int fw_loss, int rv_loss)
{
int i;
int source_idx, destination_idx;
int rnum;
char msg_buf[1024];

        NSDL2_WAN(NULL, NULL, "Method called");
	sprintf(msg_buf, "insert_linechar_table_entry(): Parsing ULOCATION %s %s : ", src, dst);

	if ((source_idx = find_locattr_idx(src)) == -1) {
	  fprintf(stderr, "%s Unknown location '%s'\n", msg_buf, src);
	  return(-1);
	}
	if ((destination_idx = find_locattr_idx(dst)) == -1) {
	  fprintf(stderr, "%s Unknown location '%s'\n", msg_buf, dst);
	  return(-1);
	}

	for (i=0; i < total_linechar_entries; i++) {
	    if (((lineCharTable[i].source == source_idx ) && (lineCharTable[i].destination == destination_idx)) ||
		 ((lineCharTable[i].source == destination_idx ) && (lineCharTable[i].destination == source_idx))) {
	  	fprintf(stderr, "%s Entry exists for this pair of locations\n", msg_buf);
	  	return(-1);
	    }
	}

	if (create_linechar_table_entry(&rnum) != SUCCESS) {
	  fprintf(stderr, "%s  Error in getting linechar_table entry\n", msg_buf);
	  return(-1);
	}

	lineCharTable[rnum].source = source_idx;
	lineCharTable[rnum].destination = destination_idx;
	lineCharTable[rnum].fw_lat = fw_lat;
	lineCharTable[rnum].rv_lat = rv_lat;
	lineCharTable[rnum].fw_loss = fw_loss;
	lineCharTable[rnum].rv_loss = rv_loss;

	//Add an entry in the reverse direction
	if (create_linechar_table_entry(&rnum) != SUCCESS) {
	  fprintf(stderr, "%s  Error in getting linechar_table entry\n", msg_buf);
	  return(-1);
	}

	lineCharTable[rnum].source = destination_idx;
	lineCharTable[rnum].destination = source_idx;
	lineCharTable[rnum].fw_lat = rv_lat;
	lineCharTable[rnum].rv_lat = fw_lat;
	lineCharTable[rnum].fw_loss = rv_loss;
	lineCharTable[rnum].rv_loss = fw_loss;

	return (0);
}

int find_accattr_idx(char* name) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called, name = %s", name);
  for(i = 0; i < total_accattr_entries; i++)
    if (!strcmp(RETRIEVE_BUFFER_DATA(accAttrTable[i].name), name))
      return i;
  return -1;
}
int find_browattr_idx(char* name) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_browattr_entries; i++)
    if (!strncmp(RETRIEVE_BUFFER_DATA(browAttrTable[i].name), name, strlen(name)))
      return i;
  return -1;
}
void kw_set_location(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num, rnum;

  if ((num = sscanf(buf, "%s %s", keyword, text)) != 2)
  {
    NS_EXIT(-1, "Need ONE fields after key LOCATION");
  }
 
  if (create_locattr_table_entry(&rnum) != SUCCESS)
  {
    NS_EXIT(-1, "Error in getting locattr_table entry");
  }

  if ((locAttrTable[rnum].name = copy_into_big_buf(text, 0)) == -1)
  {
     NS_EXIT(-1, "Error in copying new LOCATION name into big_buf.");
  }
}

void kw_set_ulocation(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char source[MAX_DATA_LINE_LENGTH];
  char destination[MAX_DATA_LINE_LENGTH];
  int num, fw_lat, rv_lat, fw_loss, rv_loss;

  if ((num = sscanf(buf, "%s %s %s %d %d %d %d", keyword, source, destination, &fw_lat, &rv_lat, &fw_loss, &rv_loss)) != 7) 
  {
    NS_EXIT(-1, "Need SIX fields after key ULOCATION.");
  }

  if (insert_linechar_table_entry (source , destination, fw_lat, rv_lat, fw_loss, rv_loss))
  {
    NS_EXIT(-1, "Failed in Parsing  ULOCATION.");
  }
}


void kw_set_uplocation(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char source[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num, pct, idx, rnum; 

  if ((num = sscanf(buf, "%s %s %s %d", keyword, text, source, &pct)) != 4) 
  {
    NS_EXIT(-1, "Need THREE fields after key UPLOCATION.");
  }

  if (strchr(text, ',')) 
  {
    NS_EXIT(-1, "User Profile Names can't have commas in them.");
  }

  if ((idx = find_userindex_idx(text)) == -1) 
  {
    if (create_userindex_table_entry(&idx) != SUCCESS)
    {
      NS_EXIT(-1, "Error in getting userindex_table entry.");
    }
    if ((userIndexTable[idx].name = copy_into_big_buf(text, 0)) == -1) 
    {
      NS_EXIT(-1, "Error in copying new UP name into big_buf.");
    }
  }
  if (create_userprof_table_entry(&rnum) != SUCCESS)
  {
    NS_EXIT(-1, "Error in getting sessprof_table entry");
  }

  userProfTable[rnum].userindex_idx = idx;
  userProfTable[rnum].type = USERPROF_LOCATION;
  if ((idx = find_locattr_idx(source)) == -1)
  {
    NS_EXIT(-1, "Unknown location attribute: %s", source);
  }
  userProfTable[rnum].attribute_idx = idx;
  userProfTable[rnum].pct = pct;
}
 

int kw_set_uaccess(char *buf, int flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num, rnum; 
  int fw_bandwidth, rv_bandwidth, compression;
  
  if ((num = sscanf(buf, "%s %s %d %d %d", keyword, text, &fw_bandwidth, &rv_bandwidth, &compression)) != 5) 
  {
    NS_KW_PARSING_ERR(buf, flag, err_msg, UACCESS_USAGE, CAV_ERR_1011235, CAV_ERR_MSG_1);
  }

  create_accattr_table_entry(&rnum);
 
  if ((accAttrTable[rnum].name = copy_into_big_buf(text, 0)) == -1) 
  {
    NS_KW_PARSING_ERR(buf, flag, err_msg, UACCESS_USAGE, CAV_ERR_1000018, text);
  }

  accAttrTable[rnum].fw_bandwidth = fw_bandwidth;
  accAttrTable[rnum].rv_bandwidth = rv_bandwidth;
  accAttrTable[rnum].compression = compression;
  accAttrTable[rnum].shared_modem = 0;
  
  return 0;
}


void kw_set_upaccess(char *buf, int flag)
{
  int num, pct, idx, rnum;
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH];

  if ((num = sscanf(buf, "%s %s %s %d", keyword, text, name, &pct)) != 4) 
  {
    NS_EXIT(-1, "Need THREE fields after key UPACCESS.");
  }

  if (strchr(text, ','))
  {
    NS_EXIT(-1, "User Profile Names can't have commas in them.");
  }

  if ((idx = find_userindex_idx(text)) == -1)
  {
    if (create_userindex_table_entry(&idx) != SUCCESS) 
    {
      NS_EXIT(-1, "Error in getting userindex_table entry.");
    }
    if ((userIndexTable[idx].name = copy_into_big_buf(text, 0)) == -1) 
    {
      NS_EXIT(-1, "Error in copying new UP name into big_buf.");
    }
  }

  if (create_userprof_table_entry(&rnum) != SUCCESS)
  {
     NS_EXIT(-1, "Error in getting sessprof_table entry.");
  }

  userProfTable[rnum].userindex_idx = idx;
  userProfTable[rnum].type = USERPROF_ACCESS;
  if ((idx = find_accattr_idx(name)) == -1) 
  {
    NS_EXIT(-1, "Unknown access attribute: %s", name);
  }
  if (accAttrTable[idx].shared_modem) 
  {
    NS_EXIT(-1, "UPACCESS keyword can't contain a shared modem.");
  }
  userProfTable[rnum].attribute_idx = idx;
  userProfTable[rnum].pct = pct;
}

void kw_set_upal(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH];
  char source[MAX_DATA_LINE_LENGTH];
 
  int num, pct, idx, rnum, new_acc;

  if ((num = sscanf(buf, "%s %s %s %s %d", keyword, text, name, source, &pct)) != 5) 
  {
    NS_EXIT(-1, "Need FOUR fields after key UPAL.");
  }

  if (strchr(text, ',')) 
  {
    NS_EXIT(-1, "User Profile Names can't have commas in them.");
  }

  if ((idx = find_userindex_idx(text)) == -1)
  {
    if (create_userindex_table_entry(&idx) != SUCCESS)
    {
      NS_EXIT(-1, "Error in getting userindex_table entry.");
    }
    if ((userIndexTable[idx].name = copy_into_big_buf(text, 0)) == -1)
    {
      NS_EXIT(-1, "Error in copying new UP name into big_buf.");
    }
  }
  if (create_accloc_table_etnry(&rnum) != SUCCESS) 
  {
    NS_EXIT(-1, "Error in getting user_acc_loc_table entry.");
  }
  accLocTable[rnum].userindex_idx = idx;

  if ((idx = find_accattr_idx(name)) == -1)
  {
    NS_EXIT(-1, "Unknown access attribute: %s", name);
  }

  /* We have to create a new entry for the shared access.  If not, then all UPAL entries that use the same modem will share it */
  if (create_accattr_table_entry(&new_acc) != SUCCESS) 
  {
    NS_EXIT(-1, "Error in getting accattr_table entry.");
  }
  if ((accAttrTable[new_acc].name = copy_into_big_buf(RETRIEVE_BUFFER_DATA(accAttrTable[idx].name), 0)) == -1) 
  {
    NS_EXIT(-1, "Error in copying new UACCESS name into big_buf.");
  }
  accAttrTable[new_acc].fw_bandwidth = accAttrTable[idx].fw_bandwidth;
  accAttrTable[new_acc].rv_bandwidth = accAttrTable[idx].rv_bandwidth;
  accAttrTable[new_acc].compression = accAttrTable[idx].compression;
  accAttrTable[new_acc].shared_modem = 1;

  accLocTable[rnum].access = new_acc;

  if ((idx = find_locattr_idx(source)) == -1) 
  {
    NS_EXIT(-1, "Unknown location name: %s", source);
  }
  accLocTable[rnum].location = idx;
  accLocTable[rnum].pct = pct;
}

//OLD -> UBROWSER InternetExplorer9.0 UA_IE 60000 Mozilla/5.0
//
//Format:
// UBROWSER <BrowerName>  <MaxConPerServerHTTP1.0> <MaxConPerServerHTTP1.1> <MaxConPerUser> <Keep Alive Timeout in ms> <User Agent String>
//
//Example:
//
//UBROWSER InternetExplorer9.0 6 6 24 60000 Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; WOW64; Trident/5.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; InfoPath.2; .NET4.0C)
//
//Changed Format: (20/11/2012) 
//
//UBROWSER <BrowerName>  <MaxConPerServerHTTP1.0> <MaxConPerServerHTTP1.1> <MaxProxyPerServerHTTP1.0> <MaxProxyPerServerHTTP1.1> <MaxConPerUser> <Keep Alive Timeout in ms> <User Agent String> 
//
//Example:
//
//UBROWSER Opera9.0 8 2 5 5 20 300000 Opera/9.21 (Windows NT 5.2; U; en)

void ubrowser_usages(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid format of UBROWSER keyword");
  NS_EXIT(-1, "%s\nUsage: UBROWSER <BrowerName>  <MaxConPerServerHTTP1.0> <MaxConPerServerHTTP1.1> <MaxProxyPerServerHTTP1.0> <MaxProxyPerServerHTTP1.1> <MaxConPerUser> <Keep Alive Timeout in ms> <User Agent String>", err);
}

void kw_set_ubrowser(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char browser_name[MAX_DATA_LINE_LENGTH];
  char max_con_per_server_http1_0[MAX_DATA_LINE_LENGTH];
  char max_con_per_server_http1_1[MAX_DATA_LINE_LENGTH];
  char max_proxy_per_server_http1_0[MAX_DATA_LINE_LENGTH];
  char max_proxy_per_server_http1_1[MAX_DATA_LINE_LENGTH];
  char max_con_per_vuser[MAX_DATA_LINE_LENGTH];
  char keep_alive_time[MAX_DATA_LINE_LENGTH];
  char uagent_string[MAX_DATA_LINE_LENGTH];
  char* buf_ptr;
  int num, rnum;

  if ((num = sscanf(buf, "%s %s %s %s %s %s %s %s %s", keyword, browser_name, max_con_per_server_http1_0, max_con_per_server_http1_1, max_proxy_per_server_http1_0, max_proxy_per_server_http1_1, max_con_per_vuser, keep_alive_time, uagent_string)) != 9) 
  {
    ubrowser_usages(NULL);
  }
  if(ns_is_numeric(max_con_per_server_http1_0) == 0)
  {
    ubrowser_usages("MaxConPerServerHTTP1.0 is not numeric");
  }
  if(ns_is_numeric(max_con_per_server_http1_1) == 0)
  {
    ubrowser_usages("MaxConPerServerHTTP1.1 is not numeric");
  }
  if(ns_is_numeric(max_proxy_per_server_http1_0) == 0)
  {
    ubrowser_usages("MaxProxyPerServerHTTP1.0 is not numeric");
  }
  if(ns_is_numeric(max_proxy_per_server_http1_1) == 0)
  {
    ubrowser_usages("MaxProxyPerServerHTTP1.1 is not numeric");
  }
  if(ns_is_numeric(max_con_per_vuser) == 0)
  {
    ubrowser_usages("MaxConPerUser is not numeric");
  }
  if(ns_is_numeric(keep_alive_time) == 0)
  {
    ubrowser_usages("Keep Aliv Timeout is not numeric");
  }

  buf_ptr = strstr(buf, " ");
  buf_ptr++;  // Point to browser name
  buf_ptr = strstr(buf_ptr, " ");
  buf_ptr++;  // Point to MaxConPerServerHTTP1.0
  buf_ptr = strstr(buf_ptr, " ");
  buf_ptr++;  // Point to MaxConPerServerHTTP1.1
  buf_ptr = strstr(buf_ptr, " ");
  buf_ptr++;  // Point to MaxProxyPerServerHTTP1.0
  buf_ptr = strstr(buf_ptr, " ");
  buf_ptr++;  // Point to MaxProxyPerServerHTTP1.1
  buf_ptr = strstr(buf_ptr, " ");
  buf_ptr++;  // Point to MaxConPerUser
  buf_ptr = strstr(buf_ptr, " ");
  buf_ptr++;  // Point to Keep Alive Timeout in ms
  buf_ptr = strstr(buf_ptr, " ");
  buf_ptr++;  // Point to uagent_string

  if (create_browattr_table_entry(&rnum) != SUCCESS)
  {
    NS_EXIT(-1, "Error in getting browattr_table entry.");
  }
  if ((browAttrTable[rnum].name = copy_into_big_buf(browser_name, 0)) == -1)
  {
    NS_EXIT(-1, "Error in copying new UACCESS name into big_buf.");
  }
 
  browAttrTable[rnum].ka_timeout = atoi(keep_alive_time);
  browAttrTable[rnum].max_con_per_vuser = atoi(max_con_per_vuser);
  browAttrTable[rnum].per_svr_max_conn_http1_0 = atoi(max_con_per_server_http1_0);
  browAttrTable[rnum].per_svr_max_conn_http1_1 = atoi(max_con_per_server_http1_1);
  browAttrTable[rnum].per_svr_max_proxy_http1_0 = atoi(max_proxy_per_server_http1_0);
  browAttrTable[rnum].per_svr_max_proxy_http1_1 = atoi(max_proxy_per_server_http1_1);

 validate_browser_connection_values(browAttrTable[rnum].per_svr_max_conn_http1_0, browAttrTable[rnum].per_svr_max_conn_http1_1, browAttrTable[rnum].per_svr_max_proxy_http1_0, browAttrTable[rnum].per_svr_max_proxy_http1_1, browAttrTable[rnum].max_con_per_vuser, browser_name);

  if((global_settings->whitelist_hdr) && (global_settings->whitelist_hdr->mode) &&
     (!strcasecmp(global_settings->whitelist_hdr->name, "User-Agent"))) 
  {
    snprintf(uagent_string, MAX_DATA_LINE_LENGTH, "%s; %s\r\n", buf_ptr, global_settings->whitelist_hdr->value);
    buf_ptr = uagent_string;
  }
  else
  {
    if ((strlen(buf_ptr) + (buf_ptr - buf) + 2) > MAX_DATA_LINE_LENGTH) 
    {
      NS_EXIT(-1, "User Agent is too big.");
    }
 
    strcat(buf_ptr, "\r\n");
  }

  if ((browAttrTable[rnum].UA = copy_into_big_buf(buf_ptr, 0)) == -1) 
  {
    NS_EXIT(-1, "Error in copying new UBROWSER UA into big_buf.");
  }

  NSDL2_WAN(NULL, NULL, "Method Exiting, keyword = %d, browser_name = %s, max_con_per_server_http1_0 = %s, max_con_per_server_http1_1 = %s, max_proxy_per_server_http1_0 = %s, max_proxy_per_server_http1_1 = %s, max_con_per_vuser = %d, keep_alive_time = %d, uagent_string = %s", keyword, browser_name, max_con_per_server_http1_0, max_con_per_server_http1_1, max_proxy_per_server_http1_0, max_proxy_per_server_http1_1, atoi(max_con_per_vuser), atoi(keep_alive_time), buf_ptr);
}


int kw_set_upbrowser(char *buf, int flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH];
  int num, pct, idx, rnum; 

  if ((num = sscanf(buf, "%s %s %s %d", keyword, text, name, &pct)) != 4) 
  {
    NS_KW_PARSING_ERR(buf, flag, err_msg, UPBROWSER_USAGE, CAV_ERR_1011240, CAV_ERR_MSG_1);
  }

  if (strchr(text, ','))
  {
    NS_KW_PARSING_ERR(buf, flag, err_msg, UPBROWSER_USAGE, CAV_ERR_1011240, "User Profile Names can't have commas in them");
  }

  if ((idx = find_userindex_idx(text)) == -1)
  {
    create_userindex_table_entry(&idx);
    if ((userIndexTable[idx].name = copy_into_big_buf(text, 0)) == -1)
    {
      NS_KW_PARSING_ERR(buf, flag, err_msg, UPBROWSER_USAGE, CAV_ERR_1000018, text);
    }
  }
  create_userprof_table_entry(&rnum);
  userProfTable[rnum].userindex_idx = idx;
  userProfTable[rnum].type = USERPROF_BROWSER;
  if ((idx = find_browattr_idx(name)) == -1)
  {
    NS_KW_PARSING_ERR(buf, flag, err_msg, UPBROWSER_USAGE, CAV_ERR_1011281, name, "");
  }
  userProfTable[rnum].attribute_idx = idx;
  userProfTable[rnum].pct = pct;

  return 0;
}

void ubscreen_size_usages(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid format of UPSCREEN_SIZE keyword");
  NS_EXIT(-1, "%s\nUsage: UPSCREEN_SIZE <profile> <Browser> <width> <height> <pct> <width> <height> <pct> ......", err);
}

#ifndef CAV_MAIN 
static int all_sc_sz_parsed = 0;
#else
static __thread int all_sc_sz_parsed = 0;
#endif

# define MAX_TOKENS 303
// Format: UPSCREEN_SIZE <profile> <Browser> <width> <heigth> <pct> <width> <heigth> <pct>..... 
// Note sum of pct for a browser for a profile should be 100
void kw_set_upscreen_size(char *buf, int flag){

  char err_msg[MAX_DATA_LINE_LENGTH];
  int  rnum, attr_rnum, prof_idx, brow_idx, num_tokens; 
  char *field[MAX_TOKENS];

  NSDL2_WAN(NULL, NULL, "Method called. buf = %s", buf);

  num_tokens = get_tokens(buf, field, " ",  MAX_TOKENS);
  if(num_tokens < 6){
    sprintf(err_msg, "UPSCREEN_SIZE needs atleast five arguments, Arg given = %d", num_tokens);
    ubscreen_size_usages(err_msg);
  }

  // profile name can be all or one of the given profile
  if(!strcmp(field[1], "ALL")){
    prof_idx = -1;
  } else if((prof_idx = find_userindex_idx(field[1])) == -1){
    sprintf(err_msg, "read_keywords(): unknown profile %s in keyword  UPSCREEN_SIZE\n", field[1]);
    ubscreen_size_usages(err_msg);
  }

  // browser name can be all or one of the given browser
  if((!strcmp(field[2], "ALL"))){
    brow_idx = -1;
  } else if((brow_idx = find_browattr_idx(field[2])) == -1){
    sprintf(err_msg, "read_keywords(): unknown profile %s in keyword  UPSCREEN_SIZE\n", field[2]);
    ubscreen_size_usages(err_msg);
  }

  // If ALL ALL is parsed once, dont parse it again and return from here
  if((prof_idx == -1) && (brow_idx == -1))
  {
    #ifndef CAV_MAIN
    if(all_sc_sz_parsed){
      sprintf(err_msg, "ALL ALL case is already parsed in keyword  UPSCREEN_SIZE\n");
      ubscreen_size_usages(err_msg);
    }
    all_sc_sz_parsed = 1;
    #endif
  }

  if((num_tokens-3)%3 != 0)
  {
    sprintf(err_msg, "Screen size should be in format height width pct, num_token  %d", num_tokens - 3);
    ubscreen_size_usages(err_msg);
  }

  //validating total percentage: Sum of all percentage should be 100
  int tmp_num_token = num_tokens - 3;
  int pct_loc = 5;
  int sum = 0;

  while(tmp_num_token){
    sum = sum + atoi(field[pct_loc]);
    pct_loc = pct_loc + 3;
    tmp_num_token = tmp_num_token - 3;
  }

  if(sum != 100){
    sprintf(err_msg, "Total pct %d should be 100", sum);
    ubscreen_size_usages(err_msg);
  } 

  int i, j = 3; 
  for(i = 0; i < (num_tokens-3)/3; i++){
  
   if(create_screen_size_table_entry(&attr_rnum) != SUCCESS)
    {
      NS_EXIT(-1, "Error in getting screen_size_table_entry.");
    }
  
    scSzeAttrTable[attr_rnum].width = atoi(field[j]);
    if(scSzeAttrTable[attr_rnum].width < 100 || scSzeAttrTable[attr_rnum].width > 10000){
      sprintf(err_msg, "width at argument %d should be in the range of 10-10,000", j + 3);
      ubscreen_size_usages(err_msg);
    }
    j++;

    scSzeAttrTable[attr_rnum].height = atoi(field[j]);
    if(scSzeAttrTable[attr_rnum].height < 100 || scSzeAttrTable[attr_rnum].height > 10000){
      sprintf(err_msg, "height at argument %d should be in the range of 10-10000", j + 3);
      ubscreen_size_usages(err_msg);
    }
    j++;

    scSzeAttrTable[attr_rnum].pct = atoi(field[j]);

    if(scSzeAttrTable[attr_rnum].pct < 1 || scSzeAttrTable[attr_rnum].pct > 100){
      sprintf(err_msg, "pct at argument %d should be in between 1 to 100", j + 3);
      ubscreen_size_usages(err_msg);
    }
    j++;

    // craete profile browser screen size table
    if(create_pf_bw_screen_size_table_entry(&rnum) != SUCCESS){
      NS_EXIT(-1, "Error in getting pf_bw_screen_size_table_entry.");
    }
  
    pfBwScSzTable[rnum].scsz_idx = attr_rnum;
    pfBwScSzTable[rnum].prof_idx = prof_idx; 
    pfBwScSzTable[rnum].brow_idx = brow_idx; 
    pfBwScSzTable[rnum].pct = scSzeAttrTable[attr_rnum].pct;
  }
}

/*
void kw_set_umachine(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  char type[MAX_DATA_LINE_LENGTH];
  int num, rnum; 

  if ((num = sscanf(buf, "%s %s %s", keyword, text, type)) != 3)
  {
    fprintf(stderr, "read_keywords(): Need THREE fields after key UMACHINE\n");
    exit(-1);
  }

  if (create_machattr_table_entry(&rnum) != SUCCESS)
  {
    fprintf(stderr, "read_keywords(): Error in getting machattr_table entry\n");
    exit(-1);
  }

  if ((machAttrTable[rnum].name = copy_into_big_buf(text, 0)) == -1) 
  {
    fprintf(stderr, "read_keywords(): Error in copying name into big_buf\n");
    exit(-1);
  }

  if (strncasecmp(type, "MACH_TYPICAL", strlen("MACH_TYPICAL")) == 0)
    freqAttrTable[rnum].type = MACH_TYPICAL;
  else if (strncasecmp(type, "MACH_POWERFUL", strlen("MACH_POWERFUL")) == 0)
    freqAttrTable[rnum].type = MACH_POWERFUL;
  else if (strncasecmp(type, "MACH_LOWEND", strlen("MACH_LOWEND")) == 0)
    freqAttrTable[rnum].type = MACH_LOWEND;
  else
  {
    fprintf(stderr, "read_keywords(): Unknown machine type: %s\n", type);
    exit(-1);
  }
}

void kw_set_upmachine(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH];

  int pct, idx, rnum, num; 

  if ((num = sscanf(buf, "%s %s %s %d", keyword, text, name, &pct)) != 4) 
  {
    fprintf(stderr, "read_keywords(): Need THREE fields after key UPMACHINE\n");
    exit(-1);
  }

  if (strchr(text, ','))
  {
    fprintf(stderr, "read_keywords(): User Profile Names can't have commas in them\n");
    exit(-1);
  }

  if ((idx = find_userindex_idx(text)) == -1) 
  {
    if (create_userindex_table_entry(&idx) != SUCCESS) 
    {
      fprintf(stderr, "read_keywords(): Error in getting userindex_table entry\n");
      exit(-1);
    }
    if ((userIndexTable[idx].name = copy_into_big_buf(text, 0)) == -1) 
    {
      fprintf(stderr, "read_keywords(): Error in copying new UP name into big_buf\n");
      exit(-1);
    }
  }
  if (create_userprof_table_entry(&rnum) != SUCCESS)
  {
    fprintf(stderr, "read_keywords(): Error in getting sessprof_table entry\n");
    exit(-1);
  }
  userProfTable[rnum].userindex_idx = idx;
  userProfTable[rnum].type = USERPROF_MACHINE;
  if ((idx = find_machattr_idx(name)) == -1)
  {
    fprintf(stderr, "read_keywords(): Unknown machine attribute: %s\n", name);
    exit(-1);
  }
  userProfTable[rnum].attribute_idx = idx;
  userProfTable[rnum].pct = pct;
}

*/
/*void kw_set_ufreq(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  char type[MAX_DATA_LINE_LENGTH];

  int num, rnum;

  if ((num = sscanf(buf, "%s %s %s", keyword, text, type)) != 3) 
  {
    fprintf(stderr, "read_keywords(): Need THREE fields after key UFREQ\n");
    exit(-1);
  }

  if (create_freqattr_table_entry(&rnum) != SUCCESS)
  {
    fprintf(stderr, "read_keywords(): Error in getting freqattr_table entry\n");
    exit(-1);
  }
  if ((freqAttrTable[rnum].name = copy_into_big_buf(text, 0)) == -1) 
  {
    fprintf(stderr, "read_keywords(): Error in copying name into big_buf\n");
    exit(-1);
  }

  if (strncasecmp(type, "FREQ_VERY_ACTIVE", strlen("FREQ_VERY_ACTIVE")) == 0)
    freqAttrTable[rnum].type = FREQ_VERY_ACTIVE;
  else if (strncasecmp(type, "FREQ_ACTIVE", strlen("FREQ_ACTIVE")) == 0)
    freqAttrTable[rnum].type = FREQ_ACTIVE;
  else if (strncasecmp(type, "FREQ_INACTIVE", strlen("FREQ_INACTIVE")) == 0)
    freqAttrTable[rnum].type = FREQ_INACTIVE;
  else
  {
    fprintf(stderr, "read_keywords(): unknown frequency type: %s\n", type);
    exit(-1);
  }
}
*/

/*void kw_set_upfreq(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH];

  int num, rnum, pct, idx;

  if ((num = sscanf(buf, "%s %s %s %d", keyword, text, name, &pct)) != 4) 
  {
    fprintf(stderr, "read_keywords(): Need THREE fields after key UPFREQ\n");
    exit(-1);
  }

  if (strchr(text, ',')) 
  {
    fprintf(stderr, "read_keywords(): User Profile Names can't have commas in them\n");
    exit(-1);
  }

  if ((idx = find_userindex_idx(text)) == -1)
  {
    if (create_userindex_table_entry(&idx) != SUCCESS) 
    {
      fprintf(stderr, "read_keywords(): Error in getting userindex_table entry\n");
      exit(-1);
    }
    if ((userIndexTable[idx].name = copy_into_big_buf(text, 0)) == -1) 
    {
      fprintf(stderr, "read_keywords(): Error in copying new UP name into big_buf\n");
      exit(-1);
    }
  }
  if (create_userprof_table_entry(&rnum) != SUCCESS) 
  {
    fprintf(stderr, "read_keywords(): Error in getting sessprof_table entry\n");
    exit(-1);
  }
  userProfTable[rnum].userindex_idx = idx;
  userProfTable[rnum].type = USERPROF_FREQ;
  if ((idx = find_freqattr_idx(name)) == -1) 
  {
    fprintf(stderr, "read_keywords(): Unknown freq attribute: %s\n", name);
    exit(-1);
  }
  userProfTable[rnum].attribute_idx = idx;
  userProfTable[rnum].pct = pct;
}
*/

int useraccloc_comp(const void* prof1, const void* prof2) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (((AccLocTableEntry *)prof1)->userindex_idx > (((AccLocTableEntry *)prof2)->userindex_idx))
    return 1;
  else if (((AccLocTableEntry *)prof1)->userindex_idx == (((AccLocTableEntry *)prof2)->userindex_idx))
    return 0;
  else
    return -1;
}

int profileBrowserScSz_comp(const void* pfbr1, const void *pfbr2){
  NSDL2_WAN(NULL, NULL, "Method called");
  if(((PfBwScSzTableEntry *) pfbr1)-> prof_idx > ((PfBwScSzTableEntry *) pfbr2)-> prof_idx)
    return -1;
  else if(((PfBwScSzTableEntry *) pfbr1)-> prof_idx == ((PfBwScSzTableEntry *) pfbr2)-> prof_idx){
    if(((PfBwScSzTableEntry *) pfbr1)-> brow_idx > ((PfBwScSzTableEntry *) pfbr2)-> prof_idx)
      return -1;
    else if(((PfBwScSzTableEntry *) pfbr1)-> brow_idx == ((PfBwScSzTableEntry *) pfbr2)-> prof_idx){
      return 0; 
    } 
  }
  return 1;
}

int userprof_comp(const void* prof1, const void* prof2) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (((UserProfTableEntry *)prof1)->userindex_idx > (((UserProfTableEntry *)prof2)->userindex_idx))
    return 1;
  else if (((UserProfTableEntry *)prof1)->userindex_idx < (((UserProfTableEntry *)prof2)->userindex_idx))
    return -1;
  else if (((UserProfTableEntry *)prof1)->type > (((UserProfTableEntry *)prof2)->type))
    return 1;
  else if (((UserProfTableEntry *)prof1)->type < (((UserProfTableEntry *)prof2)->type))
    return -1;
  else if (((UserProfTableEntry *)prof1)->attribute_idx > (((UserProfTableEntry *)prof2)->attribute_idx))
    return 1;
  else if (((UserProfTableEntry *)prof1)->attribute_idx < (((UserProfTableEntry *)prof2)->attribute_idx))
    return -1;
  else return 0;
}


// This method creates browser screen size mapping able for all the browsers of all profiles given
// This method first searches in profile browser screen size table for screen size entry, if it is not found then it searches if any entry
// is given for specific profile, if it also not found then it fill default (ALL, ALL) value in table
int create_profile_browser_sc_sz_table(int browser_idx, int prof_idx, int idx){

  int i, rnum;
  int total_pct = 0;
  int total_entries = 0;
  NSDL2_WAN(NULL, NULL, "Method called, profile = %s, browser = %s", 
    RETRIEVE_BUFFER_DATA(userIndexTable[prof_idx].name),  RETRIEVE_BUFFER_DATA(browAttrTable[browser_idx].name));

  NSDL2_WAN(NULL, NULL, "Searching for profile and browser specific screen sizes");
  // Check in profile browser screen size table, if screen size are given for profile and browser, then take those entries 
  for(i = userIndexTable[prof_idx].UPBR_start_idx; i < userIndexTable[prof_idx].UPBR_start_idx + userIndexTable[prof_idx].UPBR_length; i++){
    if((pfBwScSzTable[i].prof_idx == prof_idx) && (pfBwScSzTable[i].brow_idx == browser_idx)){
      NSDL2_WAN(NULL, NULL, "Profile and browser specific screen sizes found");
      create_browser_screen_sixe_map_table_entry(&rnum);  
      brScSzTable[rnum].screenSize_idx = pfBwScSzTable[i].scsz_idx; 
      brScSzTable[rnum].browserAttr_idx = browser_idx;
      brScSzTable[rnum].prof_idx = idx; 
      total_pct += pfBwScSzTable[i].pct;  
      total_entries++;
    }
  }   
  if((total_pct > 0) && (total_pct < 100)){
    NS_EXIT(-1, "screen size for browser %s of profile %s is not given hundred percent.", RETRIEVE_BUFFER_DATA(browAttrTable[browser_idx].name), RETRIEVE_BUFFER_DATA(userIndexTable[i].name)); 
  // Check in profile browser screen size table, if screen size are given for profile (all browser), then take those entries 
  } else if(total_pct == 0){
    NSDL2_WAN(NULL, NULL, "Browser specific screen not found. Going to search in profile specific screen sizes, start_idx = %d, length = %d", userIndexTable[prof_idx].UPBR_start_idx, userIndexTable[prof_idx].UPBR_length);
    for(i = userIndexTable[prof_idx].UPBR_start_idx; i < userIndexTable[prof_idx].UPBR_start_idx + userIndexTable[prof_idx].UPBR_length; i++){
      if((pfBwScSzTable[i].prof_idx == prof_idx) && (pfBwScSzTable[i].brow_idx == -1)){
        NSDL2_WAN(NULL, NULL, "filling profile specific values for screen size");
        create_browser_screen_sixe_map_table_entry(&rnum);
        brScSzTable[rnum].screenSize_idx = pfBwScSzTable[i].scsz_idx; 
        brScSzTable[rnum].browserAttr_idx = browser_idx;
        brScSzTable[rnum].prof_idx = idx;     
        total_pct += pfBwScSzTable[i].pct;  
        total_entries++;
      }
    }
  }
  // if no entry is given for screen size with profile and browser, then take default browser screen size value for remaining browser
  if(total_pct == 0) {
    NSDL2_WAN(NULL, NULL, "Profile specific screen also not found. Going to set default values in profile browser screen sizes");
    for(i = 0; i<total_pf_bw_screen_size_entries; i++){
      if(pfBwScSzTable[i].prof_idx == -1) {
        NSDL2_WAN(NULL, NULL, "filling default values for screen size. .pct = %d", pfBwScSzTable[i].pct);
        create_browser_screen_sixe_map_table_entry(&rnum);
        brScSzTable[rnum].screenSize_idx = pfBwScSzTable[i].scsz_idx; 
        brScSzTable[rnum].browserAttr_idx = browser_idx;
        brScSzTable[rnum].prof_idx = idx;     
        total_pct += pfBwScSzTable[i].pct;  
        total_entries++;
      }
    }
  }
  if(total_pct != 100){
    NS_EXIT(-1, "screen size for browser %s of profile %s is not given hundred percent.", RETRIEVE_BUFFER_DATA(browAttrTable[browser_idx].name), RETRIEVE_BUFFER_DATA(userIndexTable[i].name)); 
  }
  NSDL2_WAN(NULL, NULL, "total_entries = %d", total_entries);
  return total_entries;
}

static inline void set_up_screen_size_for_disable(){

  int attr_rnum, rnum;

  if(total_pf_bw_screen_size_entries == 0){

    NSDL2_WAN(NULL, NULL, "rbu screen size simulation is off and default valuse for screen size is also not provided." 
                                                                              "Creating a single entry for rbu simulation");
    // create csreen size attribute table entry
    create_screen_size_table_entry(&attr_rnum);
    scSzeAttrTable[attr_rnum].width = 0;
    scSzeAttrTable[attr_rnum].height = 0;
    scSzeAttrTable[attr_rnum].pct = 100;

    // create profile browser screen size table entry and fill pct 100 
    create_pf_bw_screen_size_table_entry(&rnum);
 
    pfBwScSzTable[rnum].scsz_idx = attr_rnum;
    pfBwScSzTable[rnum].prof_idx = -1;
    pfBwScSzTable[rnum].brow_idx = -1;
    pfBwScSzTable[rnum].pct = scSzeAttrTable[attr_rnum].pct;
 
  // if keyword is present then make total_pf_bw_screen_size_entries and set its width and height 0, and pct to 100
  } else { 
    NSDL2_WAN(NULL, NULL, "rbu screen size simulation is off and default valuse for screen size is provided." 
                          " making total_pf_bw_screen_size_entries to zero and set width, height to 0 and pct to 100");
    total_pf_bw_screen_size_entries = 1;
    scSzeAttrTable[pfBwScSzTable[0].scsz_idx].width = 0;  
    scSzeAttrTable[pfBwScSzTable[0].scsz_idx].height = 0;  
    scSzeAttrTable[pfBwScSzTable[0].scsz_idx].pct = 100;  

    pfBwScSzTable[0].prof_idx = -1; 
    pfBwScSzTable[0].brow_idx = -1; 
    pfBwScSzTable[0].pct = 100; 
    
  }
}

int sort_userprof_tables(void) {
  int test_pct = 0;
  int last_attribute_idx;
  int i;
  int userIndexTable_idx;
  int last_userindex_idx = -1;

  NSDL2_WAN(NULL, NULL, "Method called");
  qsort(accLocTable, total_accloc_entries, sizeof(AccLocTableEntry), useraccloc_comp);

  for (i = 0; i < total_accloc_entries; i++) {
    userIndexTable_idx = accLocTable[i].userindex_idx;
    if (last_userindex_idx != userIndexTable_idx) {
      if (test_pct > 100) {
	fprintf(stderr, "UPAL entrie pcts can, at most, add up to 100\n");
	return -1;
      }
      test_pct = 0;
      last_userindex_idx = userIndexTable_idx;
    }

    if (userIndexTable[userIndexTable_idx].UPAccLoc_start_idx == -1) {
      userIndexTable[userIndexTable_idx].UPAccLoc_start_idx = i;
    }

    userIndexTable[userIndexTable_idx].UPAccLoc_length++;
    test_pct += accLocTable[i].pct;
  }

  if (test_pct > 100) {
    fprintf(stderr, "UPAL entrie pcts can, at most, add up to 100\n");
    return -1;
  }

  qsort(userProfTable, total_userprof_entries, sizeof(UserProfTableEntry), userprof_comp);

  test_pct = 0;
  last_attribute_idx = -1;
  for (i = 0; i < total_userprof_entries; i ++) {

    if (userProfTable[i].type == USERPROF_LOCATION) {
      userIndexTable_idx = userProfTable[i].userindex_idx;
      if (last_attribute_idx == userProfTable[i].attribute_idx) {
	fprintf(stderr, "For user profile %s, the LOCATION attribute has multiple entries of %s\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(locAttrTable[userProfTable[i].attribute_idx].name));
	return -1;
      }
      if (userIndexTable[userIndexTable_idx].UPLoc_start_idx == -1) {
	userIndexTable[userIndexTable_idx].UPLoc_start_idx = i;
      }
      userIndexTable[userIndexTable_idx].UPLoc_length++;
      test_pct += userProfTable[i].pct;
      last_attribute_idx = userProfTable[i].attribute_idx;

      if ((i == (total_userprof_entries-1)) || ((i < (total_userprof_entries - 1)) && ((userProfTable[i+1].type != USERPROF_LOCATION)))) {
	if (test_pct != 100) {
	  fprintf(stderr, "For user profile %s, the LOCATION attribute %s does not add up to 100\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(locAttrTable[userProfTable[i].attribute_idx].name));
	  return -1;
	}
	last_attribute_idx = -1;
	test_pct = 0;
      }
    }

    if (userProfTable[i].type == USERPROF_ACCESS) {
      userIndexTable_idx = userProfTable[i].userindex_idx;
      if (last_attribute_idx == userProfTable[i].attribute_idx) {
	fprintf(stderr, "For user profile %s, the ACCESS attribute has multiple entries of %s\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(accAttrTable[userProfTable[i].attribute_idx].name));
	return -1;
      }
      if (userIndexTable[userIndexTable_idx].UPAcc_start_idx == -1) {
	userIndexTable[userIndexTable_idx].UPAcc_start_idx = i;
      }
      userIndexTable[userIndexTable_idx].UPAcc_length++;
      test_pct += userProfTable[i].pct;
      last_attribute_idx = userProfTable[i].attribute_idx;

      if ((i == (total_userprof_entries-1)) || ((i < (total_userprof_entries - 1)) && ((userProfTable[i+1].type != USERPROF_ACCESS)))) {
	if (test_pct != 100) {
	  fprintf(stderr, "For user profile %s, the ACCESS attribute %s does not add up to 100\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(accAttrTable[userProfTable[i].attribute_idx].name));
	  return -1;
	}
	last_attribute_idx = -1;
	test_pct = 0;
      }
    }

    if (userProfTable[i].type == USERPROF_BROWSER) {
      userIndexTable_idx = userProfTable[i].userindex_idx;
      if (last_attribute_idx == userProfTable[i].attribute_idx) {
	fprintf(stderr, "For user profile %s, the BROWSER attribute has multiple entries of %s\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(browAttrTable[userProfTable[i].attribute_idx].name));
	return -1;
      }
      if (userIndexTable[userIndexTable_idx].UPBrow_start_idx == -1) {
	userIndexTable[userIndexTable_idx].UPBrow_start_idx = i;
      }
      userIndexTable[userIndexTable_idx].UPBrow_length++;
      test_pct += userProfTable[i].pct;
      last_attribute_idx = userProfTable[i].attribute_idx;

      if ((i == (total_userprof_entries-1)) || ((i < (total_userprof_entries - 1)) && ((userProfTable[i+1].type != USERPROF_BROWSER)))) {
	if (test_pct != 100) {
	  fprintf(stderr, "For user profile %s, the BROWSER attribute %s does not add up to 100\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(browAttrTable[userProfTable[i].attribute_idx].name));
	  return -1;
	}
	last_attribute_idx = -1;
	test_pct = 0;
      }
    }

/*    if (userProfTable[i].type == USERPROF_FREQ) {
      userIndexTable_idx = userProfTable[i].userindex_idx;
      if (last_attribute_idx == userProfTable[i].attribute_idx) {
	fprintf(stderr, "For user profile %s, the FREQUENCE attribute has multiple entries of %s\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(freqAttrTable[userProfTable[i].attribute_idx].name));
	return -1;
      }
      if (userIndexTable[userIndexTable_idx].UPFreq_start_idx == -1) {
	userIndexTable[userIndexTable_idx].UPFreq_start_idx = i;
      }
      userIndexTable[userIndexTable_idx].UPFreq_length++;
      test_pct += userProfTable[i].pct;
      last_attribute_idx = userProfTable[i].attribute_idx;

      if ((i == (total_userprof_entries-1)) || ((i < (total_userprof_entries - 1)) && ((userProfTable[i+1].type != USERPROF_FREQ)))) {
	if (test_pct != 100) {
	  fprintf(stderr, "For user profile %s, the FREQUENCE attribute %s does not add up to 100\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(freqAttrTable[userProfTable[i].attribute_idx].name));
	  return -1;
	}
	last_attribute_idx = -1;
	test_pct = 0;
      }
    }

    if (userProfTable[i].type == USERPROF_MACHINE) {
      userIndexTable_idx = userProfTable[i].userindex_idx;
      if (last_attribute_idx == userProfTable[i].attribute_idx) {
	fprintf(stderr, "For user profile %s, the MACHINE attribute has multiple entries of %s\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(machAttrTable[userProfTable[i].attribute_idx].name));
	return -1;
      }
      userIndexTable_idx = userProfTable[i].userindex_idx;
      if (userIndexTable[userIndexTable_idx].UPMach_start_idx == -1) {
	userIndexTable[userIndexTable_idx].UPMach_start_idx = i;
      }
      userIndexTable[userIndexTable_idx].UPMach_length++;
      test_pct += userProfTable[i].pct;
      last_attribute_idx = userProfTable[i].attribute_idx;

      if ((i == (total_userprof_entries-1)) || ((i < (total_userprof_entries - 1)) && ((userProfTable[i+1].type != USERPROF_MACHINE)))) {
	if (test_pct != 100) {
	  fprintf(stderr, "For user profile %s, the MACHINE attribute %s does not add up to 100\n", RETRIEVE_BUFFER_DATA(userIndexTable[userIndexTable_idx].name), RETRIEVE_BUFFER_DATA(machAttrTable[userProfTable[i].attribute_idx].name));
	  return -1;
	}
	last_attribute_idx = -1;
	test_pct = 0;
      }
    }
*/
  }
  // Sort profile browser screen size table by profile, browser
  qsort(pfBwScSzTable, total_pf_bw_screen_size_entries, sizeof(PfBwScSzTableEntry), profileBrowserScSz_comp);

  last_attribute_idx = -1;
  for(i = 0; i <total_pf_bw_screen_size_entries; i++){
    NSDL2_WAN(NULL, NULL, "pfBwScSzTable[%d].prof_idx  = %d, pfBwScSzTable[i].brow_idx = %d", i, pfBwScSzTable[i].prof_idx, pfBwScSzTable[i].brow_idx);

  } 

  // If screen size simulation is off, then make total_pf_bw_screen_size_entries 0 and fill width and height 0, pct 100
  // This will prevent to made extra entries
  if(!global_settings->rbu_screen_sim_mode){
    set_up_screen_size_for_disable();
  }

  // fill UPBR_start_idx and UPBR_length temrory to make browser  screen size mapping table
  for(i = 0; i < total_pf_bw_screen_size_entries; i++){
    //
    userIndexTable_idx = pfBwScSzTable[i].prof_idx;      
    // Skip entry which have prof_idx -1, as it is default value
    if(userIndexTable_idx == -1)   // ALL ALL case
      continue;

    if(last_attribute_idx != userIndexTable_idx){

      userIndexTable[userIndexTable_idx].UPBR_start_idx = i;
      userIndexTable[userIndexTable_idx].UPBR_length = 0;
      last_attribute_idx = userIndexTable_idx;      
    }
    userIndexTable[userIndexTable_idx].UPBR_length++ ;
  }

# ifdef NS_DEBUG_ON  
  for(i = 0; i < total_pf_bw_screen_size_entries; i++){
    NSDL2_WAN(NULL, NULL, "userIndexTable[i].UPBR_start_idx = %d, userIndexTable[i].UPBR_length = %d", userIndexTable[i].UPBR_start_idx, userIndexTable[i].UPBR_length);
  }
#endif


  int map_entries = 0; 
  int start_idx, j; 
  // Create browser screen mapping size table and fill browser screen size mapping table start_ix and length  
  for(i = 0; i < total_userindex_entries; i++){
    start_idx =  map_entries;
    for(j = userIndexTable[i].UPBrow_start_idx; j < (userIndexTable[i].UPBrow_start_idx + userIndexTable[i].UPBrow_length); j++){ 

      map_entries += create_profile_browser_sc_sz_table(userProfTable[j].attribute_idx, i, j);
 
      NSDL2_WAN(NULL, NULL, "browAttrTable[i].name = %s, pct = %d", RETRIEVE_BUFFER_DATA(browAttrTable[userProfTable[j].attribute_idx].name), userProfTable[j].pct);
    }
    userIndexTable[i].UPBR_start_idx = start_idx;
    userIndexTable[i].UPBR_length = map_entries - start_idx; 
    NSDL2_WAN(NULL, NULL, "userIndexTable[i].UPBR_start_idx = %d, userIndexTable[i].UPBR_length = %d, i = %d", userIndexTable[i].UPBR_start_idx, userIndexTable[i].UPBR_length, i);
  }
  return 0;
}



void insert_default_location_values(void) {
  int default_idx;
  int i, j;
  int total_useraccloc_pct;
  int num_upaccloc;

  NSDL2_WAN(NULL, NULL, "Method called");
  default_idx = find_userindex_idx("Internet");
  if (default_idx == -1) {
    NS_EXIT(-1, "Default user profile 'Internet' not defined");
  }

  /* if the start_idx is -1, then means we enter in the default user profile, which is the user profile Internet */
  for (i = 0; i < total_userindex_entries; i++) {

    if (userIndexTable[i].UPAccLoc_start_idx != -1) {
      for (total_useraccloc_pct = 0, j = userIndexTable[i].UPAccLoc_start_idx, num_upaccloc = 0; num_upaccloc < userIndexTable[i].UPAccLoc_length; j++, num_upaccloc++) {
	total_useraccloc_pct += accLocTable[j].pct;
      }
    }

    if ((userIndexTable[i].UPAccLoc_start_idx == -1) || (total_useraccloc_pct != 100)) {
      if (userIndexTable[i].UPLoc_start_idx == -1) {
	userIndexTable[i].UPLoc_start_idx = userIndexTable[default_idx].UPLoc_start_idx;
	userIndexTable[i].UPLoc_length = userIndexTable[default_idx].UPLoc_length;
      }
      if (userIndexTable[i].UPAcc_start_idx == -1) {
	userIndexTable[i].UPAcc_start_idx = userIndexTable[default_idx].UPAcc_start_idx;
	userIndexTable[i].UPAcc_length = userIndexTable[default_idx].UPAcc_length;
      }
    }
    if (userIndexTable[i].UPBrow_start_idx == -1) {
      userIndexTable[i].UPBrow_start_idx = userIndexTable[default_idx].UPBrow_start_idx;
      userIndexTable[i].UPBrow_length = userIndexTable[default_idx].UPBrow_length;
    }
    /*if (userIndexTable[i].UPFreq_start_idx == -1) {
      userIndexTable[i].UPFreq_start_idx = userIndexTable[default_idx].UPFreq_start_idx;
      userIndexTable[i].UPFreq_length = userIndexTable[default_idx].UPFreq_length;
    }
    if (userIndexTable[i].UPMach_start_idx == -1) {
      userIndexTable[i].UPMach_start_idx = userIndexTable[default_idx].UPMach_start_idx;
      userIndexTable[i].UPMach_length = userIndexTable[default_idx].UPMach_length;
    }*/
  }
}


/*********shared memory**********/

int userprofshr_comp(const void* prof1, const void* prof2) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (((UserProfTableEntry_Shr *)prof1)->uprofindex_idx > (((UserProfTableEntry_Shr *)prof2)->uprofindex_idx))
    return 1;
   if (((UserProfTableEntry_Shr *)prof1)->uprofindex_idx < (((UserProfTableEntry_Shr *)prof2)->uprofindex_idx))
    return -1;
   if (((UserProfTableEntry_Shr *)prof1)->pct < (((UserProfTableEntry_Shr *)prof2)->pct))
    return 1;
   if (((UserProfTableEntry_Shr *)prof1)->pct > (((UserProfTableEntry_Shr *)prof2)->pct))
    return -1;
  else return 0;
}




int find_userindex_idx(char* name) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_userindex_entries; i++)
  {
    if (!strcmp(RETRIEVE_BUFFER_DATA(userIndexTable[i].name), name))
      return i;
  }

  return -1;
}

#ifndef CAV_MAIN
int total_userprofshr_entries=0;
#else
__thread int total_userprofshr_entries=0;
#endif
void insert_user_proftable_shr_mem(void) {
  int i, j;
 void* prof_table_shr_mem;
 int rnum = 0;
  LocAttrTableEntry_Shr *locAttr;
  AccAttrTableEntry_Shr *accAttr;
  BrowAttrTableEntry_Shr *browAttr;
  ScreenSizeAttrTableEntry_Shr *scszAttr;
  //FreqAttrTableEntry_Shr *freqAttr;
  //MachAttrTableEntry_Shr *machAttr;
  //int accloc_num, loc_num, acc_num, brow_num, freq_num, mach_num, screen_size_num;
  int accloc_num, loc_num, acc_num, screen_size_num;
  //int acclocpct, locpct, accpct, browpct, freqpct, machpct, scszpct;
  int acclocpct, locpct, accpct, browpct, scszpct;
  double total_pct;
  int prof_tables_size;
  //int prof_table_fd;
  unsigned int old_pct = 0;
  int length;
  int userprof_num;
  int total_useraccloc_pct;
  int remainder_pct;
  int total_accloc_pct;
  int num_upaccloc;

  NSDL2_WAN(NULL, NULL, "Method called");

  for (i = 0; i < total_userindex_entries; i++) {
    if (userIndexTable[i].UPAccLoc_start_idx != -1) {
      total_userprofshr_entries += (userIndexTable[i].UPAccLoc_length * userIndexTable[i].UPBR_length);
      for (total_useraccloc_pct = 0, j = userIndexTable[i].UPAccLoc_start_idx, num_upaccloc = 0; num_upaccloc < userIndexTable[i].UPAccLoc_length; j++, num_upaccloc++) {
        total_useraccloc_pct += accLocTable[j].pct;
      }
      //total_useraccloc_pct = accLocTable[userIndexTable[i].UPAccLoc_start_idx + (userIndexTable[i].UPAccLoc_length - 1)].pct;
      if (total_useraccloc_pct < 100) {
        total_userprofshr_entries += (userIndexTable[i].UPLoc_length * userIndexTable[i].UPAcc_length * userIndexTable[i].UPBR_length);
      }
    }else {
      total_userprofshr_entries += (userIndexTable[i].UPLoc_length * userIndexTable[i].UPAcc_length * userIndexTable[i].UPBR_length);
    }
  }

  prof_tables_size = WORD_ALIGNED(sizeof(UserProfTableEntry_Shr) * total_userprofshr_entries) +
                     WORD_ALIGNED(sizeof(UserProfIndexTableEntry_Shr) * total_userindex_entries);
  
#if 0
  if ((prof_table_fd = do_shmget(shm_base + total_num_shared_segs, prof_tables_size, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
    fprintf(stderr, "error in allocating shared memory for the UserProfShr Table\n");
    return -1;
  }

  total_num_shared_segs++;

  if ((prof_table_shr_mem = shmat(prof_table_fd, NULL, 0)) == NULL) {
    fprintf(stderr, "error in getting shared memory for the Session Profile Table\n");
    return -1;
  }
#endif

  // Allocating an extra entry to keep cumlative count of profile used 
  MY_MALLOC(prof_pct_count_table, sizeof(ProfilePctCountTable) * (total_userprofshr_entries + 1), "prof_pct_count_table", -1);  

  prof_table_shr_mem = do_shmget(prof_tables_size, "Session Profile Table");

  userprof_table_shr_mem = prof_table_shr_mem;

  /* Inserting the userprof table and userprofindex table */
  prof_table_shr_mem += WORD_ALIGNED(sizeof(UserProfTableEntry_Shr) * total_userprofshr_entries);

  userprofindex_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(UserProfIndexTableEntry_Shr) * total_userindex_entries);

  for (i = 0; i < total_userindex_entries; i++) {
    userprofindex_table_shr_mem[i].userprof_start = &userprof_table_shr_mem[rnum];
    length = 0;
    userprofindex_table_shr_mem[i].name = RETRIEVE_SHARED_BUFFER_DATA(userIndexTable[i].name);
    total_accloc_pct = 0;

    if (userIndexTable[i].UPAccLoc_start_idx != -1) {
      for (accloc_num = 0; accloc_num < userIndexTable[i].UPAccLoc_length; accloc_num++) {
        locAttr = &locattr_table_shr_mem[accLocTable[userIndexTable[i].UPAccLoc_start_idx + accloc_num].location];
        accAttr = &accattr_table_shr_mem[accLocTable[userIndexTable[i].UPAccLoc_start_idx + accloc_num].access];
        acclocpct = accLocTable[userIndexTable[i].UPAccLoc_start_idx + accloc_num].pct;
        total_accloc_pct += acclocpct;

/*	for (brow_num = 0; brow_num < userIndexTable[i].UPBrow_length; brow_num++) {
	  browAttr = &browattr_table_shr_mem[userProfTable[userIndexTable[i].UPBrow_start_idx + brow_num].attribute_idx];
	  browpct = userProfTable[userIndexTable[i].UPBrow_start_idx + brow_num].pct;
*/
   for(screen_size_num = 0; screen_size_num < userIndexTable[i].UPBR_length; screen_size_num++) {
	   scszAttr = &scszattr_table_share_mem[brScSzTable[userIndexTable[i].UPBR_start_idx + screen_size_num].screenSize_idx];
	   browAttr = &browattr_table_shr_mem[brScSzTable[userIndexTable[i].UPBR_start_idx + screen_size_num].browserAttr_idx];
	   scszpct = scszattr_table_share_mem[userIndexTable[i].UPBR_start_idx + screen_size_num].pct;
	   browpct = userProfTable[brScSzTable[userIndexTable[i].UPBR_start_idx + screen_size_num].browserAttr_idx].pct;

/*	  for (freq_num = 0; freq_num < userIndexTable[i].UPFreq_length; freq_num++) {
	    freqAttr = &freqattr_table_shr_mem[userProfTable[userIndexTable[i].UPFreq_start_idx + freq_num].attribute_idx];
	    freqpct = userProfTable[userIndexTable[i].UPFreq_start_idx + freq_num].pct;

	    for (mach_num = 0; mach_num < userIndexTable[i].UPMach_length; mach_num ++) {
	      machAttr = &machattr_table_shr_mem[userProfTable[userIndexTable[i].UPMach_start_idx + mach_num].attribute_idx];
	      machpct = userProfTable[userIndexTable[i].UPMach_start_idx + mach_num].pct;
*/
	      userprof_table_shr_mem[rnum].uprofindex_idx = i;
	      userprof_table_shr_mem[rnum].location = locAttr;
	      userprof_table_shr_mem[rnum].access = accAttr;
	      userprof_table_shr_mem[rnum].browser = browAttr;
	      // userprof_table_shr_mem[rnum].frequency = freqAttr;
	      // userprof_table_shr_mem[rnum].machine = machAttr;
		    userprof_table_shr_mem[rnum].screen_size = scszAttr;

	      //total_pct = (double) acclocpct * (double) 100 * (double) browpct * (double) freqpct * (double) machpct *(double)scszpct;
	      total_pct = (double) acclocpct * (double) 100 * (double) browpct  *(double)scszpct;

	      userprof_table_shr_mem[rnum].pct = (unsigned int) (total_pct / TRUNCATE_NUMBER);

        // set acc_pct 0 for prof count pct table
        prof_pct_count_table[rnum].acc_pct = 0; 

	      rnum++;
	      length++;
/*	    }
	  }
*/
	}
      }

      if (total_accloc_pct <= 100) {
	remainder_pct = 100 - total_accloc_pct;
      }
    } else {
      remainder_pct = 100;
    }

    if (remainder_pct) {
      for (loc_num = 0; loc_num < userIndexTable[i].UPLoc_length; loc_num++) {
	locAttr = &locattr_table_shr_mem[userProfTable[userIndexTable[i].UPLoc_start_idx + loc_num].attribute_idx];
	locpct = userProfTable[userIndexTable[i].UPLoc_start_idx + loc_num].pct;

	for (acc_num = 0; acc_num < userIndexTable[i].UPAcc_length; acc_num++) {
	  accAttr = &accattr_table_shr_mem[userProfTable[userIndexTable[i].UPAcc_start_idx + acc_num].attribute_idx];
	  accpct = userProfTable[userIndexTable[i].UPAcc_start_idx + acc_num].pct;

/*	  for (brow_num = 0; brow_num < userIndexTable[i].UPBrow_length; brow_num++) {
	    browAttr = &browattr_table_shr_mem[userProfTable[userIndexTable[i].UPBrow_start_idx + brow_num].attribute_idx];
	    browpct = userProfTable[userIndexTable[i].UPBrow_start_idx + brow_num].pct;
*/
	  for (screen_size_num = 0; screen_size_num < userIndexTable[i].UPBR_length; screen_size_num++) {
	    scszAttr = &scszattr_table_share_mem[brScSzTable[userIndexTable[i].UPBR_start_idx + screen_size_num].screenSize_idx];
	    browAttr = &browattr_table_shr_mem[brScSzTable[userIndexTable[i].UPBR_start_idx + screen_size_num].browserAttr_idx];
	    scszpct = scszattr_table_share_mem[brScSzTable[userIndexTable[i].UPBR_start_idx + screen_size_num].screenSize_idx].pct;
	    browpct = userProfTable[brScSzTable[userIndexTable[i].UPBR_start_idx + screen_size_num].prof_idx].pct;
      NSDL2_WAN(NULL, NULL, "browpct = %d, scszpct = %d, brScSzTable index = %d", browpct, scszpct, userIndexTable[i].UPBR_start_idx + screen_size_num);

      

/*	    for (freq_num = 0; freq_num < userIndexTable[i].UPFreq_length; freq_num++) {
	      freqAttr = &freqattr_table_shr_mem[userProfTable[userIndexTable[i].UPFreq_start_idx + freq_num].attribute_idx];
	      freqpct = userProfTable[userIndexTable[i].UPFreq_start_idx + freq_num].pct;

	      for (mach_num = 0; mach_num < userIndexTable[i].UPMach_length; mach_num ++) {
		machAttr = &machattr_table_shr_mem[userProfTable[userIndexTable[i].UPMach_start_idx + mach_num].attribute_idx];
		machpct = userProfTable[userIndexTable[i].UPMach_start_idx + mach_num].pct;
*/
		userprof_table_shr_mem[rnum].uprofindex_idx = i;
		userprof_table_shr_mem[rnum].location = locAttr;
		userprof_table_shr_mem[rnum].access = accAttr;
		userprof_table_shr_mem[rnum].browser = browAttr;
		//userprof_table_shr_mem[rnum].frequency = freqAttr;
		//userprof_table_shr_mem[rnum].machine = machAttr;
		userprof_table_shr_mem[rnum].screen_size = scszAttr;

		//total_pct = ((double) remainder_pct * (double) locpct * (double) accpct * (double) browpct * (double) freqpct * (double) machpct * (double ) scszpct)/100;
		total_pct = ((double) remainder_pct * (double) locpct * (double) accpct * (double) browpct * (double ) scszpct)/100;

		userprof_table_shr_mem[rnum].pct = (unsigned int) (total_pct / TRUNCATE_NUMBER);

    // set acc_pct 0 for prof count pct table
    prof_pct_count_table[rnum].acc_pct = 0;  

# ifdef NS_DEBUG_ON
    NSDL2_WAN(NULL, NULL, "Profile = %s, location = %s, access = %s, browser = %s, screen_size = %dx%d, pct = %d", 
                    userprofindex_table_shr_mem[userprof_table_shr_mem[rnum].uprofindex_idx].name,
                    userprof_table_shr_mem[rnum].location->name, 
                    userprof_table_shr_mem[rnum].access->name, 
                    userprof_table_shr_mem[rnum].browser->name, 
                    userprof_table_shr_mem[rnum].screen_size->width, 
                    userprof_table_shr_mem[rnum].screen_size->width, 
                    userprof_table_shr_mem[rnum].pct); 
# endif
		rnum++;
		length++;
/*	      }
	    }
*/
	  }
	}
      }
    }
    userprofindex_table_shr_mem[i].length = length;
  }

  qsort(userprof_table_shr_mem, total_userprofshr_entries, sizeof(UserProfTableEntry_Shr), userprofshr_comp);

  int total_pct_idx =0;
  // Change all pct to cum pct
  for (i = 0; i < total_userindex_entries; i++) {
    old_pct = 0;

    // Set start indext of total pct count table in index tablei, this index will be used in init_user_session to get the start index
    total_pct_idx = ((char *)userprofindex_table_shr_mem[i].userprof_start - (char *)userprof_table_shr_mem) / sizeof(UserProfTableEntry_Shr); 
    userprofindex_table_shr_mem[i].prof_pct_start_idx = total_pct_idx;
    NSDL4_WAN(NULL, NULL, "i = %d, userprofindex_table_shr_mem[i].prof_pct_start_idx = %d, ptr = %p", i, userprofindex_table_shr_mem[i].prof_pct_start_idx, (char *)(userprofindex_table_shr_mem + i)); 

    for (userprof_num = 0; userprof_num < userprofindex_table_shr_mem[i].length; userprof_num++) {
      // Get indes of total count pct table and set count to 0 and pct and acc_pct to corresponding values 
      total_pct_idx = ((char *)(userprofindex_table_shr_mem[i].userprof_start + userprof_num) - (char *)userprof_table_shr_mem) / sizeof(UserProfTableEntry_Shr) ; 
      prof_pct_count_table[total_pct_idx].count = 0; 
      prof_pct_count_table[total_pct_idx].pct = (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->pct; 

      // get acc_pct
      (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->pct += old_pct;

      prof_pct_count_table[total_pct_idx].acc_pct = (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->pct;
 
      old_pct = (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->pct;
# ifdef NS_DEBUG_ON
      NSDL2_WAN(NULL, NULL, "profile = %s, location = %s, access = %s, browser = %s, screen_size = %dx%d, cum_pct = %d, total_pct_idx = %d, count_pct = %d", 
                      userprofindex_table_shr_mem[i].name,  
                      (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->location->name, 
                      (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->access->name, 
                      (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->browser->name, 
                      (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->screen_size->width,                 
                      (userprofindex_table_shr_mem[i].userprof_start + userprof_num)->screen_size->height, old_pct, total_pct_idx,
                      prof_pct_count_table[total_pct_idx].acc_pct );
# endif

    }
  }
  // set cumlative used count to zero 
  prof_pct_count_table[total_userprofshr_entries].count = 0;

}

int find_inuseuser_idx(int location_idx) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called, location_idx = %d", location_idx);
  for (i = 0; i < total_inuseuser_entries; i++)
    if (inuseUserTable[i].location_idx == location_idx)
      return i;

  return -1;
}

int create_inuseuser_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_inuseuser_entries == max_inuseuser_entries) {
    MY_REALLOC_EX (inuseUserTable, (max_inuseuser_entries + DELTA_INUSEUSER_ENTRIES) * sizeof(InuseUserTableEntry), max_inuseuser_entries * sizeof(InuseUserTableEntry),"inuseUserTable", -1);
    if (!inuseUserTable) {
      fprintf(stderr,"create_inuseuser_table_entry(): Error allocating more memory for inuseuser entries\n");
      return(FAILURE);
    } else max_inuseuser_entries += DELTA_INUSEUSER_ENTRIES;
  }
  *row_num = total_inuseuser_entries++;
  return (SUCCESS);
}


int find_inusesvr_idx(int location_idx) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called, location_idx = %d, total_inusesvr_entries = %d", location_idx, total_inusesvr_entries);
  for (i = 0; i < total_inusesvr_entries; i++)
    if (inuseSvrTable[i].location_idx == location_idx)
      return i;

  return -1;
}

void create_largeloc_data_array(void) {
  int i, j;
  NSDL2_WAN(NULL, NULL, "Method called");
  LineCharTableEntry* linechar_ptr = &lineCharTable[0];

  MY_MALLOC (large_location_array, total_locattr_entries * total_locattr_entries * sizeof(short), "large_location_array", -1);

  for (i = 0; i < total_locattr_entries; i++) {
    for (j = 0; j < total_locattr_entries; j++) {
      large_location_array[ i*total_locattr_entries+j ] = -1;
    }
  }

  for (i = 0; i < total_linechar_entries; i++, linechar_ptr++)
    large_location_array[ linechar_ptr->source*total_locattr_entries+linechar_ptr->destination ] = i;
}

int create_inusesvr_table_entry(int *row_num) {
  NSDL2_WAN(NULL, NULL, "Method called");
  if (total_inusesvr_entries == max_inusesvr_entries) {
    MY_REALLOC_EX (inuseSvrTable, (max_inusesvr_entries + DELTA_INUSESVR_ENTRIES) * sizeof(InuseSvrTableEntry), max_inusesvr_entries * sizeof(InuseSvrTableEntry), "inuseSvrTable", -1);
    if (!inuseSvrTable) {
      fprintf(stderr,"create_inusesvr_table_entry(): Error allocating more memory for inusesvr entries\n");
      return(FAILURE);
    } else max_inusesvr_entries += DELTA_INUSESVR_ENTRIES;
  }
  *row_num = total_inusesvr_entries++;
  return (SUCCESS);
}


int location_data_compute(void) {
  int rnum;
  int i, j;
  //int run_prof_idx;
  int run_prof_length;
  RunProfTableEntry *runprof_ptr;
  int user_prof_idx;
  int user_location_start;
  int user_location_length;
  UserProfTableEntry *user_location_ptr;
  int user_accloc_start;
  int user_accloc_length;
  AccLocTableEntry *user_accloc_ptr;
  int loc_idx = -1;

  run_prof_length = total_runprof_entries;
  runprof_ptr = &runProfTable[0];

  NSDL2_WAN(NULL, NULL, "Method called");
  for (i = 0; i < run_prof_length; i++, runprof_ptr++) {
    /* We need to check for both PCT as well as Quantity
     * Beacuse If scenario is configured for number mode the quantity will be filled
     * & scenario is configured for pct mode then percentage is filled and also
     * pecentage to quantity  conversion is done in functiond validate phases()
     */
    if (runprof_ptr->quantity == 0 && runprof_ptr->percentage == 0)
      continue;
    user_prof_idx = runprof_ptr->userprof_idx;
    user_location_start = userIndexTable[user_prof_idx].UPLoc_start_idx;
    user_location_length = userIndexTable[user_prof_idx].UPLoc_length;
    user_location_ptr = &userProfTable[user_location_start];
    for (j = 0; j < user_location_length; j++, user_location_ptr++) {
      if (user_location_ptr->pct == 0)
	continue;

      if ((loc_idx = find_inuseuser_idx(user_location_ptr->attribute_idx)) == -1) {
	if ((create_inuseuser_table_entry(&rnum) != SUCCESS)) {
	  fprintf(stderr, "location_data_compute(): Error in creating a inuse_user table entry\n");
          write_log_file(NS_SCENARIO_PARSING, "Failed to create inuse_user table entry for user attribute idx = %d",
                                               user_location_ptr->attribute_idx);
	  return -1;
	}

	inuseUserTable[rnum].location_idx = user_location_ptr->attribute_idx;
      }
    }

    user_accloc_start = userIndexTable[user_prof_idx].UPAccLoc_start_idx;
    user_accloc_length = userIndexTable[user_prof_idx].UPAccLoc_length;
    user_accloc_ptr = &accLocTable[user_accloc_start];
    for (j = 0; j < user_accloc_length; j++, user_accloc_ptr++) {
      if (user_accloc_ptr->pct == 0)
	continue;

      if ((loc_idx = find_inuseuser_idx(user_accloc_ptr->location)) == -1) {
	if ((create_inuseuser_table_entry(&rnum) != SUCCESS)) {
	  fprintf(stderr, "location_data_compute(): Error in creating a inuse_user table entry\n");
          write_log_file(NS_SCENARIO_PARSING, "Failed to create inuse_user table entry for location idx = %d",
                                               user_accloc_ptr->location);
	  return -1;
	}

	inuseUserTable[rnum].location_idx = user_accloc_ptr->location;
      }
    }
  }
  fill_server_loc_idx();  

  create_largeloc_data_array();
  return 0;
}


void display_user_prof_table() {
   int i;
   NSDL2_WAN(NULL, NULL, "Method called");
     if ((u_ns_ptr_t) userprof_table_shr_mem != -1) {
    NSDL3_WAN(NULL, NULL, "User Prof Table\n");
    for (i = 0; i<total_userprofshr_entries; i++)
      //NSDL3_WAN(NULL, NULL, "index:%d\t userprof_idx: %d\t Location:%s\t Access:%s\t Browser:%s\t Freq:%s\t Machine:%s Pct:%d\n", i, userprof_table_shr_mem[i].uprofindex_idx, userprof_table_shr_mem[i].location->name,
      NSDL3_WAN(NULL, NULL, "index:%d\t userprof_idx: %d\t Location:%s\t Access:%s\t Browser:%s\t Pct:%d\n", i, userprof_table_shr_mem[i].uprofindex_idx, userprof_table_shr_mem[i].location->name,
             userprof_table_shr_mem[i].access->name, userprof_table_shr_mem[i].browser->name,
             //userprof_table_shr_mem[i].frequency->name, userprof_table_shr_mem[i].machine->name,
             userprof_table_shr_mem[i].pct);
    NSDL3_WAN(NULL, NULL, "\n");
  }
}


void dump_user_profile_data(void) {
  int i;

  NSDL2_WAN(NULL, NULL, "Method called");


  NSDL3_WAN(NULL, NULL, "Location Attribute Table");
  for (i = 0; i < total_locattr_entries; i++)
    NSDL3_WAN(NULL, NULL, "index: %d\t name:%s\n", i, RETRIEVE_BUFFER_DATA(locAttrTable[i].name));
  NSDL3_WAN(NULL, NULL, "\n");


  NSDL3_WAN(NULL, NULL, "Access Attribute Table\n");
  for (i = 0; i < total_accattr_entries; i++)
    NSDL3_WAN(NULL, NULL, "index: %d\t name:%s\t fw_bandwidth:%d\t rv_bandwidth: %d\n", i, RETRIEVE_BUFFER_DATA(accAttrTable[i].name), accAttrTable[i].fw_bandwidth, accAttrTable[i].rv_bandwidth);
  NSDL3_WAN(NULL, NULL, "\n");


  NSDL3_WAN(NULL, NULL, "Browser Attribute Table\n");
  for (i = 0; i < total_browattr_entries; i ++)
    NSDL3_WAN(NULL, NULL, "index: %d\t name: %s\t UA: %s\n", i, RETRIEVE_BUFFER_DATA(browAttrTable[i].name), RETRIEVE_BUFFER_DATA(browAttrTable[i].UA));
  NSDL3_WAN(NULL, NULL, "\n");


  /*NSDL3_WAN(NULL, NULL, "Machine Attribute Table\n");
  for (i = 0; i < total_machattr_entries; i ++)
    NSDL3_WAN(NULL, NULL, "index: %d\t name: %s\t type: %d\n", i, RETRIEVE_BUFFER_DATA(machAttrTable[i].name), machAttrTable[i].type);
  NSDL3_WAN(NULL, NULL, "\n");


  NSDL3_WAN(NULL, NULL, "Frequency Attribute Table\n");
  for (i = 0; i < total_freqattr_entries; i++)
    NSDL3_WAN(NULL, NULL, "index: %d\t name: %s\t type: %d\n", i, RETRIEVE_BUFFER_DATA(freqAttrTable[i].name), freqAttrTable[i].type);
  NSDL3_WAN(NULL, NULL, "\n");*/


  NSDL3_WAN(NULL, NULL, "UserProfIndex Table\n");
  for (i = 0; i < total_userindex_entries; i++) {
    NSDL3_WAN(NULL, NULL, "index: %d\t name: %s\t UPLoc_start_idx: %d\t UPLoc_length: %d\t UPAcc_start_idx: %d\t UPAcc_length: %d", i, RETRIEVE_BUFFER_DATA(userIndexTable[i].name), userIndexTable[i].UPLoc_start_idx, userIndexTable[i].UPLoc_length, userIndexTable[i].UPAcc_start_idx, userIndexTable[i].UPAcc_length);
    //NSDL3_WAN(NULL, NULL, "\t UPBrow_start_idx: %d\t UPBrow_length: %d\t UPFreq_start_idx: %d\t UPFreq_length: %d\t UPMach_start_idx:%d\t UPMach_length:%d\n", userIndexTable[i].UPBrow_start_idx, userIndexTable[i].UPBrow_length, userIndexTable[i].UPFreq_start_idx, userIndexTable[i].UPFreq_length, userIndexTable[i].UPMach_start_idx, userIndexTable[i].UPMach_length);
    NSDL3_WAN(NULL, NULL, "\t UPBrow_start_idx: %d\t UPBrow_length: %d\n", userIndexTable[i].UPBrow_start_idx, userIndexTable[i].UPBrow_length);
  }
  NSDL3_WAN(NULL, NULL, "\n");
}


void  copy_user_profile_attributes_into_shm()
{
  int i,j;
  int attr_tables_size;
  int line_char_array_size;
  LineCharEntry* line_char_array_ptr;
  int user_location_idx;
  int svr_location_idx;
  int line_char_idx;
  void *attr_table_shr_mem;
  

  /* Insert in the attribute tables */
  if ((sizeof(AccAttrTableEntry) != sizeof(AccAttrTableEntry_Shr)) ||
      (sizeof(BrowAttrTableEntry) != sizeof(BrowAttrTableEntry_Shr)) ||
      //(sizeof(FreqAttrTableEntry) != sizeof(FreqAttrTableEntry_Shr)) ||
      //(sizeof(MachAttrTableEntry) != sizeof(MachAttrTableEntry_Shr)) || 
      (sizeof(ScreenSizeAttrTableEntry) != sizeof(ScreenSizeAttrTableEntry_Shr))) {
    NS_EXIT(-1, "COPYING THE ATTRIBUTE TABLES WILL CAUSE MEMORY DAMAGE");
  }

  line_char_array_size = total_inuseuser_entries * total_inusesvr_entries * sizeof(LineCharEntry);

  attr_tables_size = WORD_ALIGNED(total_locattr_entries * sizeof(LocAttrTableEntry_Shr)) +
    WORD_ALIGNED(line_char_array_size) +
    WORD_ALIGNED(total_accattr_entries * sizeof(AccAttrTableEntry)) +
    WORD_ALIGNED(total_browattr_entries * sizeof(BrowAttrTableEntry)) +
    //WORD_ALIGNED(total_freqattr_entries * sizeof(FreqAttrTableEntry)) +
    WORD_ALIGNED(total_screen_size_entries * sizeof(ScreenSizeAttrTableEntry)) ;
    // total_machattr_entries * sizeof(MachAttrTableEntry);

  if (attr_tables_size) {
      attr_table_shr_mem = do_shmget(attr_tables_size, "attribute Tables");
      locattr_table_shr_mem = (LocAttrTableEntry_Shr*) attr_table_shr_mem;
      for (i = 0; i < total_locattr_entries; i++) {
	locattr_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(locAttrTable[i].name);
	locattr_table_shr_mem[i].linechar_array = NULL;
      }

      attr_table_shr_mem += WORD_ALIGNED(total_locattr_entries * sizeof(LocAttrTableEntry_Shr));
      line_char_array_ptr = (LineCharEntry*)attr_table_shr_mem;
      for (i = 0; i < total_inuseuser_entries; i++) {
	user_location_idx = inuseUserTable[i].location_idx;
	locattr_table_shr_mem[user_location_idx].linechar_array = line_char_array_ptr;
	for (j = 0; j < total_inusesvr_entries; j++, line_char_array_ptr++) {
	  svr_location_idx = inuseSvrTable[j].location_idx;
	  line_char_idx = large_location_array[user_location_idx * total_locattr_entries + svr_location_idx];
	  if (line_char_idx == -1) {
	    NS_EXIT(1, "no entry for the linechar from source:%s to destination:%s", BIG_BUF_MEMORY_CONVERSION(locAttrTable[user_location_idx].name), BIG_BUF_MEMORY_CONVERSION(locAttrTable[svr_location_idx].name));
	  }
	  locattr_table_shr_mem[user_location_idx].linechar_array[j].fw_lat = lineCharTable[line_char_idx].fw_lat;
	  locattr_table_shr_mem[user_location_idx].linechar_array[j].rv_lat = lineCharTable[line_char_idx].rv_lat;
	  locattr_table_shr_mem[user_location_idx].linechar_array[j].fw_loss = lineCharTable[line_char_idx].fw_loss;
	  locattr_table_shr_mem[user_location_idx].linechar_array[j].rv_loss = lineCharTable[line_char_idx].rv_loss;
	}
      }

      attr_table_shr_mem += WORD_ALIGNED(line_char_array_size);
      accattr_table_shr_mem = (AccAttrTableEntry_Shr*) attr_table_shr_mem;
      memcpy(accattr_table_shr_mem, accAttrTable, total_accattr_entries * sizeof(AccAttrTableEntry));

      if (loader_opcode != MASTER_LOADER) 
      {
        if (ns_cavmodem_init() == -1)
        {
          NS_EXIT(-1, "Failed to intialize cavmodem");
        }
      }
      for (i = 0; i < total_accattr_entries; i++) {
	accattr_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(accAttrTable[i].name);
	/* we are also going to put into the kernel all the shared modems */
        if (loader_opcode != MASTER_LOADER)
        { 
          if(global_settings->wan_env) { // To check wether wan_env is enabled before getting shared modem (Fix done for bug-id 2225) 
            if(cav_open_shared_modem(&accAttrTable[i], &accattr_table_shr_mem[i]) < 0)
            {
               NS_EXIT(-1, "To check wether wan_env is enabled before getting shared modem");
            }
          }
        }
      } 
      attr_table_shr_mem += WORD_ALIGNED(total_accattr_entries * sizeof(AccAttrTableEntry));
      browattr_table_shr_mem = (BrowAttrTableEntry_Shr*) attr_table_shr_mem;
      memcpy(browattr_table_shr_mem, browAttrTable, total_browattr_entries * sizeof(BrowAttrTableEntry));
      for (i = 0; i < total_browattr_entries; i++) {
	browattr_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(browAttrTable[i].name);
	browattr_table_shr_mem[i].UA = BIG_BUF_MEMORY_CONVERSION(browAttrTable[i].UA);
        browattr_table_shr_mem[i].ka_timeout = browAttrTable[i].ka_timeout;
      }

      attr_table_shr_mem += WORD_ALIGNED(total_browattr_entries * sizeof(BrowAttrTableEntry));
/*      freqattr_table_shr_mem = (FreqAttrTableEntry_Shr*) attr_table_shr_mem;
      memcpy(freqattr_table_shr_mem, freqAttrTable, total_freqattr_entries * sizeof(FreqAttrTableEntry));
      for (i = 0; i < total_freqattr_entries; i++)
	freqattr_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(freqAttrTable[i].name);

      attr_table_shr_mem += WORD_ALIGNED(total_freqattr_entries * sizeof(FreqAttrTableEntry));
      machattr_table_shr_mem = (MachAttrTableEntry_Shr*) attr_table_shr_mem;
      memcpy(machattr_table_shr_mem, machAttrTable, total_machattr_entries * sizeof(MachAttrTableEntry));
      for (i = 0; i < total_machattr_entries; i++)
	machattr_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(machAttrTable[i].name); 
*/

      //attr_table_shr_mem += WORD_ALIGNED(total_screen_size_entries * sizeof(ScreenSizeAttrTableEntry));
      scszattr_table_share_mem = (ScreenSizeAttrTableEntry_Shr*) attr_table_shr_mem;
      memcpy(scszattr_table_share_mem, scSzeAttrTable, total_screen_size_entries * sizeof(ScreenSizeAttrTableEntry));
  }

}

int ns_get_ua_string_ext (char *ua_input_buffer, int ua_input_buffer_len, VUser *vptr)
{
  char *ptr;
  int len;
 
  NSDL1_WAN(vptr, NULL, "Method called, ua_input_buffer_len = %d, vptr = %p", ua_input_buffer_len, vptr);

  //Verify string length given by user, it should be less than 32K 
  if (ua_input_buffer_len > 32000)
  {
     NS_EL_2_ATTR(EID_FOR_API, vptr->user_index, vptr->sess_inst, EVENT_API, 4,
                         vptr->sess_ptr->sess_name, vptr->cur_page->page_name, "In ns_get_ua_string API, user agent string length given by user is %d whereas user agent string length should be less than 32K. Hence setting user agent string length to 32K", ua_input_buffer_len);
     ua_input_buffer_len = 32000;  
  }  

  //If HTTPData struct exists then we need to verify whether ua_string set by API   
  if(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL) 
     && (vptr->httpData->ua_handler_ptr->ua_string != NULL)) 
  {
    //UA Strinf set by API
    NSDL2_WAN(vptr, NULL, "UA string was set by API. So copying this. UA string = %s", vptr->httpData->ua_handler_ptr->ua_string);
    //Copy ua string from UA_handler struct into local char pointer
    ptr = vptr->httpData->ua_handler_ptr->ua_string;
 
    //Verify ua string buffer length, which ever length is shorter we will copy UA string according to that
    NSDL2_WAN(vptr, NULL, "UA string length provided by user = %d whereas UA string length within UA_handler structure = %d ", ua_input_buffer_len, vptr->httpData->ua_handler_ptr->ua_len);
    if((ua_input_buffer_len - 1) < vptr->httpData->ua_handler_ptr->ua_len)   
      len = ua_input_buffer_len - 1;
    else
      len = vptr->httpData->ua_handler_ptr->ua_len;
    NSDL2_WAN(vptr, NULL, "UA string will be copied according to length = %d ", len);
  }
  else
  {
    NSDL1_WAN(vptr, NULL, "UA string will be set by Browser'shared memory. UA string = %s", vptr->browser->UA);
 
    //String length of UA string
    int ua_browser_struct_len = strlen(vptr->browser->UA);

    //Copy ua string from Browser shared memomry struct to local char pointer
    ptr = vptr->browser->UA;

    //Verify ua string buffer length, which ever length is shorter we will copy UA string according to that
    NSDL2_WAN(vptr, NULL, "UA string length provided by user = %d whereas UA string length within Browser shared memory structure = %d ", ua_input_buffer_len, ua_browser_struct_len);
    if((ua_input_buffer_len - 1) < ua_browser_struct_len)
      len = ua_input_buffer_len - 1;
    else
      len = ua_browser_struct_len;

    NSDL2_WAN(vptr, NULL, "UA string will be copied according to length = %d ", len); 
  }   

  //Copy UA string into buffer given by user 
  bcopy (ptr, ua_input_buffer, len);
  ua_input_buffer[len + 1] = '\0';
  NSDL2_WAN(vptr, NULL, "UA string buffer = %s and length = %d", ua_input_buffer, len + 1);
  return (len); 
}

void ns_set_ua_string_ext (char *ua_input_buffer, int ua_input_buffer_len, VUser *vptr)
{
  NSDL4_WAN(vptr, NULL, "Method called, ua_input_buffer_len = %d, vptr = %p", ua_input_buffer_len, vptr);

  if(ua_input_buffer_len <= 0)
  {
    return;
  }

  //Verify string length given by user, it should be less than 32K 
  if (ua_input_buffer_len > 32000)
  {
     NS_EL_2_ATTR(EID_FOR_API, vptr->user_index, vptr->sess_inst, EVENT_API, 4,
                         vptr->sess_ptr->sess_name, vptr->cur_page->page_name, "In ns_get_ua_string API, user agent string length given by user is %d whereas user agent string length should be less than 32K. Hence setting user agent string length to 32K", ua_input_buffer_len);
     ua_input_buffer_len = 32000;
  }

  //Validation Check 1: If vptr->httpData does not exists then we return 
  if(vptr->httpData == NULL)
  {
    NSDL2_WAN(vptr, NULL, "vptr->httpData does not exists");
    NS_EL_2_ATTR(EID_FOR_API, vptr->user_index, vptr->sess_inst, EVENT_API, 4,
                         vptr->sess_ptr->sess_name, vptr->cur_page->page_name, "Something went wrong this should not happen in NS_SET_UA_STRING_API");
    return;
  }

  /* If vptr->httpData->ua_handler_ptr does not exists then we simply return from the function
   * Otherwise set the user agent string and length in UA_handler structure*/ 
  if(vptr->httpData->ua_handler_ptr == NULL)
  {
    NSDL2_WAN(vptr, NULL, "Malloc ua_handler_ptr");
    MY_MALLOC(vptr->httpData->ua_handler_ptr, sizeof(UA_handler),  "vptr->httpData->ua_handler_ptr", 1);
    vptr->httpData->ua_handler_ptr->malloced_len = 0;
    //vptr->httpData->ua_handler_ptr->ua_len = 0;
    vptr->httpData->ua_handler_ptr->ua_string = NULL;
  }

  /* If malloced length is less than length given by user then we need to malloc buffer 
   * otherwise we set UA string and set length*/
  NSDL2_WAN(vptr, NULL, "Malloced UA string length = %d", vptr->httpData->ua_handler_ptr->malloced_len);

  //Assuming  user is sending length without NULL byte
  if(vptr->httpData->ua_handler_ptr->malloced_len < ua_input_buffer_len)
  {
    NSDL2_WAN(vptr, NULL, "Reallocating UA string buffer in httpData structure with length provided by user = %d", ua_input_buffer_len);
    // Allocate ua_string two extra bytes for header end (\r\n) 
    MY_REALLOC_EX(vptr->httpData->ua_handler_ptr->ua_string , ua_input_buffer_len + 3, vptr->httpData->ua_handler_ptr->ua_len, "vptr->httpData->ua_handler_ptr->ua_string", 1);
    vptr->httpData->ua_handler_ptr->malloced_len = ua_input_buffer_len;
  }

  //If User Agent string exists then copy UA string and update length in UA_handler structure
  NSDL2_WAN(vptr, NULL, "Copying UA string and update length in UA_handler structure");
  strncpy(vptr->httpData->ua_handler_ptr->ua_string, ua_input_buffer, ua_input_buffer_len); 
  
  if(vptr->httpData->ua_handler_ptr->ua_string[ua_input_buffer_len - 2] != '\r'){ 
    NSDL2_WAN(vptr, NULL, "User-Agent string do not have CRLF. Insert CRLF end to string");
    vptr->httpData->ua_handler_ptr->ua_string[ua_input_buffer_len] = '\r';
    vptr->httpData->ua_handler_ptr->ua_string[ua_input_buffer_len + 1] = '\n';
    ua_input_buffer_len += 2;
  }
  vptr->httpData->ua_handler_ptr->ua_string[ua_input_buffer_len] = '\0';
  vptr->httpData->ua_handler_ptr->ua_len = ua_input_buffer_len;
  NSDL2_WAN(vptr, NULL, "Setting UA string = %s with length = %d", vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->ua_len);

  return;
}


// This method checks if browser percentage in internet brofile is given hundred percent or not
static inline void validate_internet_profile(){

  int default_idx, i, total_pct = 0;
  NSDL2_WAN(NULL, NULL, "Method called");

  default_idx = find_userindex_idx("Internet");
  if (default_idx == -1) {
    NS_EXIT(-1, "Default user profile 'Internet' not defined");
  }

  // Check browser percentage 
  for(i = userIndexTable[default_idx].UPBrow_start_idx; i < (userIndexTable[default_idx].UPBrow_start_idx + userIndexTable[default_idx].UPBrow_length); i++){
    total_pct += userProfTable[i].pct;
  }

  if(total_pct != 100){
    NS_EXIT(-1, "Total Percentage of browser in default profile Internet is %d, it should be 100", total_pct);
  }

  total_pct = 0;

  // Check access percentage 
  for(i = userIndexTable[default_idx].UPAcc_start_idx; i < (userIndexTable[default_idx].UPAcc_start_idx + userIndexTable[default_idx].UPAcc_length); i++){
    total_pct += userProfTable[i].pct;
  }

  if(total_pct != 100){
    NS_EXIT(-1, "Total Percentage of access in default profile Internet is %d, it should be 100", total_pct);
  }
  
  total_pct = 0;

  // Check access percentage 
  for(i = userIndexTable[default_idx].UPLoc_start_idx; i < (userIndexTable[default_idx].UPLoc_start_idx + userIndexTable[default_idx].UPLoc_length); i++){
    total_pct += userProfTable[i].pct;
  }

  if(total_pct != 100){
    NS_EXIT(-1, "Total Percentage of Location in default profile Internet is %d, it should be 100", total_pct);
  }
}

// This method validates if default profile is given browser hundred percent and insert default values to other profile, if not given.
int validate_and_process_user_profile(){

  validate_internet_profile();

  insert_default_location_values();

  if (location_data_compute() == -1) {
    fprintf(stderr, "Error in location_data_compute\n");
    return -1;
 }
  return 0;
}

// PCT_CAL cal will be used to calculate the used percenatge, here we are taking acci*loc*brow*screen (100*100*100*100) and trunctaed by 100
#define PCT_CAL 1000000 

// This method returns the index of prof pct count table. This index is equivalent to user prof table index
int get_profile_idx(int start_idx, int len, int rnd_num){

  int i = 0, k;
  int sel_idx = -1;
  int sum_count;
  int place_found = 0;

  NSDL2_WAN(NULL,NULL, "Method called. start_idx = %d, len = %d, rnd_num = %d", start_idx, len, rnd_num);

  // Select index on basis of random number
  for( i = start_idx; i < (start_idx + len); i++ ){
    NSDL2_WAN(NULL,NULL, "i = %d, prof_pct_count_table[i].acc_pct = %d", i, prof_pct_count_table[i].acc_pct);
    if(rnd_num < prof_pct_count_table[i].acc_pct){
      sel_idx = i;
      break;
    }
  }

  // If random no is not coming under acc_pct, it will happen very rare
  if( sel_idx == -1){
    sel_idx = (start_idx + len) - 1;
    NSDL2_WAN(NULL, NULL, "no idx selected by random no setting it len -1  = %d", sel_idx);
  }

  // Get total count from the last entry 
  sum_count = prof_pct_count_table[total_userprofshr_entries].count;

  NSDL2_WAN(NULL, NULL, "idx selected by random no = %d", sel_idx);

  NSDL2_WAN(NULL, NULL, "Percent used for sel_idx = %d, Given percentage = %d", ((prof_pct_count_table[sel_idx].count * PCT_CAL)/(sum_count+1)), prof_pct_count_table[sel_idx].pct);

  if(prof_pct_count_table[sel_idx].count == 0){ // if count is zero, then we will not check for percentage  
    NSDL2_WAN(NULL, NULL, "sel_idx %d selected has count 0", sel_idx);
  }  
  // If percentage of selected index is already reached, then select another index where percentage is not reached 
  else if(((prof_pct_count_table[sel_idx].count * PCT_CAL) / (sum_count+1)) >= prof_pct_count_table[sel_idx].pct){ 
    NSDL2_WAN(NULL, NULL,"sel_idx %d used %d times\n", sel_idx, prof_pct_count_table[sel_idx].count);

    // searching in browser table from sel_idx to 0 idx
    for(k = sel_idx - 1; k >= start_idx; --k){
      if((prof_pct_count_table[k].count == 0) || (((prof_pct_count_table[k].count * PCT_CAL) / (sum_count+1)) < prof_pct_count_table[k].pct)){
        sel_idx = k;
        place_found = 1;  
        NSDL2_WAN(NULL, NULL, "First Loop: Index selected %d and count for this index is %d", sel_idx, prof_pct_count_table[sel_idx].count);
        break;
      }
    } 
    // If index where percentage is not found from sel_idx to start_idx, then search from sel_idx to start_idx + len 
    if(!place_found){
      for(k = sel_idx+1; k < (start_idx + len); ++k){
        if((prof_pct_count_table[k].count == 0) || (((prof_pct_count_table[k].count * PCT_CAL) / (sum_count+1)) < prof_pct_count_table[k].pct)){
          sel_idx = k;
          NSDL2_WAN(NULL, NULL, "Second Loop: Index selected %d and count for this index is %d.", 
                                                          sel_idx, prof_pct_count_table[sel_idx].count);
          break;
        }
      }
    }
  }
  NSDL2_WAN(NULL, NULL, "Final selected index %d", sel_idx);
  prof_pct_count_table[sel_idx].count++; // Increment count for selected entry
  prof_pct_count_table[total_userprofshr_entries].count++; // Increment count for total use
  return sel_idx;
}

void kw_set_ser_prof_selc_mode(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[MAX_DATA_LINE_LENGTH];
  char temp[MAX_DATA_LINE_LENGTH];
  char usages[] = "Usages: USER_PROFILE_SELECTION_MODE\n <mode(0/1)>\n \t0-Random Selection\n\t1-Uniform Selection\n";

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf); 
  
  int num_args = sscanf(buf, "%s %s %s", keyword, mode_str, temp);

  if(num_args != 2) {
    NS_EXIT(-1, "No of argument is not one.", usages);
  }

  if(!ns_is_numeric(mode_str))  {
    NS_EXIT(-1, "Mode \'%s\' is Invalid.%s", mode_str, usages); 
  }

  int mode = atoi(mode_str);

  if(mode != 0 && mode != 1) {
    NS_EXIT(-1, "Mode \'%d\' is Invalid.%s", mode, usages);
  }

  global_settings->user_prof_sel_mode = mode;

  NSDL2_PARSING(NULL, NULL, "USER_PROFILE_SELECTION_MODE, mode = %d", global_settings->user_prof_sel_mode); 
}
