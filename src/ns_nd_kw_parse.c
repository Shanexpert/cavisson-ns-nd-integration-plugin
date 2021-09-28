/********************************************************************************
 * File Name            : ns_nd_kw_parse.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 2012/1/23
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains parsing functions for G_ENABLE_NET_DIAGNOSTICS 
 *                        keyword.
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 **********************************************************************************/
#include <sys/time.h>
#include "ns_cache_include.h"
#include "ns_server_admin_utils.h"
#include "nslb_util.h"
#include "ns_nd_kw_parse.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include <v1/topolib_structures.h>
static unsigned int req_instance = 0; //Used to generate sequence, filled in last 3 bytes.

/**********************************************************************************************************************
 * Description       : enable_nd_usage() macro used to print usage for G_NET_DIAGNOSTICS keyword and exit.
 *                     Called from kw_set_g_enable_net_diagnostics().
 * Input Parameters
 *       err         : Print error message.
 * Output Parameters : None
 * Return            : None
 *************************************************************************************************************************/

static void enable_nd_usage(char *err) 
{
  NSDL1_PARSING(NULL, NULL, "Method called.");
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of G_ENABLE_NET_DIAGNOSTICS keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: G_ENABLE_NET_DIAGNOSTICS <group name> <option> <header name>\n");
  NSTL1_OUT(NULL, NULL, "    Group name can be ALL or any group name used in scenario group\n");
  NSTL1_OUT(NULL, NULL, "    Option: \n");
  NSTL1_OUT(NULL, NULL, "      0 - Disabling net diagnostics(default) \n");
  NSTL1_OUT(NULL, NULL, "      1 - Enabling net diagnostics \n");
  NSTL1_OUT(NULL, NULL, "    Header name is name of the header, sent for all request for which net diagnostics keyword enabled. Here CavNDFPInstance (default header name).\n");
  NS_EXIT(-1, "%s\nUsage: G_ENABLE_NET_DIAGNOSTICS <group name> <option> <header name>", err);
}

/*********************************************************************************************************
 * Description        : kw_set_g_enable_net_diagnostics() method used to parse G_ENABLE_NET_DIAGNOSTICS keyword,
 *                      This method is called from parse_group_keywords() in ns_parse_scen_conf.c.
 * Format             : G_NET_DIAGNOSTICS <group name> <option>
 * Input Parameter
 *           buf      : Providing entire buffer(including keyword).
 *           gset     : Pointer to update value of enable_net_diagnostic.
 *           err_msg  : Error message.
 * Output Parameter   : Set enable_net_diagnostic in struct GroupSetting.
 * Return             : Returns 0 for success and exit if fails.
 **************************************************************************************************************/

int kw_set_g_enable_net_diagnostics(char *buf, GroupSettings *gset, char *err_msg) 
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  float option = 0;
  char nd_opt[MAX_DATA_LINE_LENGTH]; 
  nd_opt[0] = 0;// Default value is 0
  char hdr_name[32 + 1];
  hdr_name[0] = '\0';
  char *hdr;
  NSDL4_PARSING(NULL, NULL, "Method called, buf = %s, global_settings->net_diagnostics_mode = %d", buf, global_settings->net_diagnostics_mode);
  hdr = hdr_name;
  if(*hdr == '\0') {
    strcpy(hdr_name, "CavNDFPInstance");// Default header name is CavNDFPInstance.
    NSDL2_PARSING(NULL, NULL, "Default value of header name, hdr_name = %s", hdr_name);
  }

  num = sscanf(buf, "%s %s %s %s %s", keyword, sgrp_name, nd_opt, hdr_name, tmp);

  if((num < 3)||(num > 4)) 
   enable_nd_usage("Invalid number of arguments.");
  
  if(ns_is_float(nd_opt) == 0)
    enable_nd_usage("Net Diagnostics enabling option can have only integer or float value.");

  option = atof(nd_opt);

  if(option < 0.0 || option > 100.0)
    enable_nd_usage("Invalid value for Net Diagnostics enabling option. Must be between 0 and 100");
  
  if(option > 0.0)
  {
    //gset->enable_net_diagnostic = 1; // setting value of net diagnostics enabling option.
    //NSDL4_PARSING(NULL, NULL, "option = %f, gset->enable_net_diagnostic = %d", option, gset->enable_net_diagnostic);
    //TODO: Add warning msg
    NS_DUMP_WARNING("G_ENABLE_NET_DIAGNOSTICS keyword is now having absolute value and further it may also not support.");
  }
#if 0
  else
  {
    gset->enable_net_diagnostic = 0; 
    NSDL4_PARSING(NULL, NULL, "gset->enable_net_diagnostic = %d", gset->enable_net_diagnostic);
    return 0;
  }
  /* If any group has ND enabled, then the message from NS to LPS for start_net_dignostics 
   * is to be sent. For this enable_net_diagnostic field of group_default_settings is checked 
   * in ns_parent.c
   */
  if(option > 0.0)
   group_default_settings->enable_net_diagnostic = 1;
  
  if(validate_var(hdr_name))
    enable_nd_usage("Invalid header name");

  strcpy(gset->net_diagnostic_hdr, hdr_name); // setting value of net diagnostics header.

  gset->nd_pct = (unsigned short) (option * 100); // setting value of nd percentage

  NSDL2_PARSING(NULL, NULL, "Exiting method, gset->enable_net_diagnostic = %d, gset->net_diagnostic_hdr = %s", gset->enable_net_diagnostic, gset->net_diagnostic_hdr);  
#endif
  return 0; //success case
}

/********************************************************************************
 * This function is used to compute flowpath instance for net diagnostics header.
 * Returns generated flow path instance for every url request.
 * 
 * Net diagnostics Header:-
 * Flowpath Instance computation logic shd be changed. Similar to ND.
 * Format: 8 bytes(64 bits)
 * MSB 2 bits - 00 (NS generated)
 * NVM ID - 14 bits
 *  Timestamp - 40 bits
 * Running Counter - 8 bits
 *  ________________________
 * |B7|B6|B5|B4|B3|B2|B1|B0|
 * | _|__|__|__|__|__|__|__|
 * 
 * eg:
 * CavNDFPInstance: 017F010000000000001
 * 
 *****************************************************************************/

inline long long compute_flowpath_instance()
{
  long long fp_instance_num;
  u_ns_ts_t timestamp;
  struct timeval want_time;

  NSDL2_HTTP(NULL, NULL, "Method called. Testrun Number = %d, NVM Id = %d.", testidx, my_port_index);

  gettimeofday(&want_time, NULL);

  timestamp = (want_time.tv_sec * 1000) + (want_time.tv_usec / 1000) \
               - ((global_settings->unix_cav_epoch_diff)?((global_settings->unix_cav_epoch_diff) * 1000):1388534400000LL);


  //Compute flow path instance
  fp_instance_num = ((((long long)my_port_index) & 0x3FFF)  << 48) + ((timestamp & 0xFFFFFFFFFFLL) << 8) + (long long)(req_instance & 0xFF);
  req_instance++; //incrementing counter for next url request.
  if(req_instance > 0xFF)
    req_instance = 0;
  
  NSDL3_HTTP(NULL, NULL, "FlowPathInstance computed for url request = %lld, Hexa Representation = %016llx\n", fp_instance_num, fp_instance_num);
  
  return(fp_instance_num);
}

#if 0
/********************************************************************************
 * This function is used to compute flowpath instance for net diagnostics header.
 * Returns generated flow path instance for every url request.
 * 
 * Net diagnostics Header:-
 * Format: 8 bytes(64 bits)
 * TestRun Number =(B7-B5) 3bytes(24 bits)
 * NVM Id =(B4) 1byte(8 bits)
 * Sequence =(B3-B0) 4bytes(32 bits) Remaining sequence number is incremented on
 * every URL request (per NVM).
 *  ________________________
 * |B7|B6|B5|B4|B3|B2|B1|B0|
 * | _|__|__|__|__|__|__|__|
 * 
 * eg:
 * CavNDFPInstance: 017F010000000000001
 * 
 *****************************************************************************/
inline long long compute_flowpath_instance()
{
  long long fp_instance_num;

  NSDL2_HTTP(NULL, NULL, "Method called. Testrun Number = %d, NVM Id = %d.", testidx, my_port_index);

  //Compute flow path instance
  fp_instance_num = (((long long)testidx) << 40) + (((long long)my_port_index) << 32) + (long long)(req_instance);
  req_instance ++; //incrementing counter for next url request.
  
  NSDL3_HTTP(NULL, NULL, "FlowPathInstance computed for url request = %lld, Hexa Representation = %016llx\n", fp_instance_num, fp_instance_num);
  
  return(fp_instance_num);
}
#endif


/*
//For print the use of NET_DIAGNOSTICS_SERVER keyword
static void net_diagnostics_server_usages(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of NET_DIAGNOSTICS_SERVER keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: NET_DIAGNOSTICS_SERVER <SERVER> <PORT> <ND_PROFILE NAME> <MODE>\n");
  NSTL1_OUT(NULL, NULL, "  This keyword is use to set the ND Agent IP address and Port no.\n");
  NS_EXIT(-1, "%s\nUsage: NET_DIAGNOSTICS_SERVER <SERVER> <PORT> <ND_PROFILE NAME> <MODE>", err);
}
*/
// NET_DIAGNOSTICS_SERVER <MODE> <SERVER> <PORT>
int kw_set_net_diagnostics_server(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char nd_ip[MAX_DATA_LINE_LENGTH] = "127.0.0.1";
  char ndc_ip[MAX_DATA_LINE_LENGTH] = "\0";
  char nd_port[MAX_DATA_LINE_LENGTH] = "\0";
  char profile_name[MAX_DATA_LINE_LENGTH] = "\0";
  char mode[MAX_DATA_LINE_LENGTH] = "\0";
  char tmp_buf[MAX_DATA_LINE_LENGTH] = "\0";
  char err_msg[4096];
  char *val;
  char *port_start;
  char *colon_ptr;
  char fname[MAX_DATA_LINE_LENGTH] = "\0";
  int ret = 0;

  int num;

  sprintf(fname, "%s/ndc/conf/%s", g_ns_wdir, NDC_CONF);

  NSDL4_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  if ((num = sscanf(buf, "%s %s %s %s %s", keyword, mode, nd_port, profile_name, tmp_buf)) < 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, NET_DIAGNOSTICS_SERVER_USAGE, CAV_ERR_1011359, keyword);    
  }

  // TODO: add validation of IP
  val = nd_port;
  if(val != NULL)
  {
    CLEAR_WHITE_SPACE(val);
    if ((port_start = rindex(nd_port, ':')))
    {
      *port_start = '\0';
      strcpy(nd_ip, nd_port);//Copying IP 
      strcpy(nd_port, (port_start + 1));
    }

    NSDL3_PARSING(NULL, NULL, "nd_port = %s, nd_ip = %s", nd_port, nd_ip);
    if(val == NULL)
    { 
      NS_KW_PARSING_ERR(keyword, 0, err_msg, NET_DIAGNOSTICS_SERVER_USAGE, CAV_ERR_1060058);
    }
   //Now, if user gives NA in place of port in NET_DIAGNOSTIC_SERVER keyword, then 
   // port will be read from ndc.conf file

    if(strcmp(nd_port, "NA") == 0)
    {
      ret = nslb_parse_keyword(fname, "PORT", nd_port); 

      if(ret == -1)
      {
        if(nslb_parse_keyword(fname, "NDC_PORT", nd_port) == -1)
        {  
          NS_KW_PARSING_ERR(keyword, 0, err_msg, NET_DIAGNOSTICS_SERVER_USAGE, CAV_ERR_1060057);
        }
      }

      if((colon_ptr = strchr(nd_port, ':')))
      {
        *colon_ptr = '\0';
        colon_ptr++;
        sprintf(ndc_ip, "%s", nd_port);

        if(strcmp(nd_ip, ndc_ip))
        {
          strcpy(global_settings->net_diagnostics_server, ndc_ip);
          NSDL1_PARSING(NULL, NULL, "Warning: ServerIP is not same in scenario and conf file.Hence, using conf file IP  %s", ndc_ip);
          NS_DUMP_WARNING("ServerIP is not same in scenario from conf file.Hence, setting its IP with conf file IP %s", ndc_ip);
        }
        else
          strcpy(global_settings->net_diagnostics_server, nd_ip);

        sprintf(nd_port, "%s", colon_ptr);
        global_settings->net_diagnostics_port = (int )atoi(nd_port);
      }
      else
      {
        strcpy(global_settings->net_diagnostics_server, nd_ip);
        global_settings->net_diagnostics_port = (int )atoi(nd_port);
      }
    }
    else if(ns_is_numeric(nd_port) == 0)
    {
      NS_KW_PARSING_ERR(keyword, 0, err_msg, NET_DIAGNOSTICS_SERVER_USAGE, CAV_ERR_1060054); 
    }   
    else
    {
      global_settings->net_diagnostics_port = (int )atoi(nd_port);
      strcpy(global_settings->net_diagnostics_server, nd_ip);
    }
  }
  else{
    strcpy(nd_port,"0");
    strcpy(global_settings->net_diagnostics_server, nd_ip);
    global_settings->net_diagnostics_port = (int )atoi(nd_port);
    NSDL1_PARSING(NULL,NULL,"Auto scale mode is OFF");
  }

  val = mode;
  if(*val != '\0')
  {
    CLEAR_WHITE_SPACE(val);
    NSDL3_PARSING(NULL, NULL, "mode = %s", mode);
    if(*val == '\0')
    {
      NS_KW_PARSING_ERR(keyword, 0, err_msg, NET_DIAGNOSTICS_SERVER_USAGE, CAV_ERR_1060056);
    } 
    //if(ns_is_numeric(mode) == 0)
    //  net_diagnostics_server_usages("mode is not numeric");
    global_settings->net_diagnostics_mode = (short )atoi(mode);
  }
  else 
    global_settings->net_diagnostics_mode = 0;
  NSDL3_PARSING(NULL, NULL, "NET_DIAGNOSTICS_SERVER = %s, %d, %s, %d", nd_ip, atoi(nd_port), profile_name, atoi(mode));


  if(*profile_name == '\0')
    strcpy(global_settings->nd_profile_name, "default");
  else
  {
    char *ptr = strchr(profile_name, '/');
    if(ptr)
    {
      NS_KW_PARSING_ERR(keyword, 0, err_msg, NET_DIAGNOSTICS_SERVER_USAGE, CAV_ERR_1060055);
    } 
    strcpy(global_settings->nd_profile_name, profile_name);
  }
  //if(global_settings->net_diagnostics_mode < 0 || global_settings->net_diagnostics_mode > 1)
  //    net_diagnostics_server_usages("nd_port is not numeric");

  //Manish: Add server entry into server table if already not exist into server.dat file
  char server_ip_with_port[1024];
  char tier_name[1024]="Cavisson";
  
  sprintf(server_ip_with_port, "%s:%d", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
  
  ret = search_server_in_topo_servers_list(tier_name,server_ip_with_port);
  NSDL4_PARSING(NULL, NULL, "ret = %d", ret);
  if(!ret)
  {
    if(global_settings->net_diagnostics_mode != 0)
      topolib_fill_server_info_in_ignorecase(server_ip_with_port,tier_name,topo_idx);
  }
  strcpy(global_settings->net_diagnostic_hdr, "CavNDFPInstance"); // setting value of net diagnostics header.
 
  NSDL4_PARSING(NULL, NULL, "Method exiting, global_settings->net_diagnostics_server = %s , global_settings->net_diagnostics_port = %d, iglobal_settings->nd_profile_name = %s, global_settings->net_diagnostics_mode = %d", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, global_settings->nd_profile_name, global_settings->net_diagnostics_mode);
  return 0;
}

