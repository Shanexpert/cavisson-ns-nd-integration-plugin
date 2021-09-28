/****************************************************************************
 * Name      : ns_nd_integration.c 
 * Purpose   : NetStorm will use correlation-id concept to generate flow-path to integrate with ND.
 * Code Flow : 
 * Author(s) : 
 * Date      : 7 Feb 2019
 * Copyright : (c) Cavisson Systems
 * Modification History :
 *     Author: 
 *      Date : 
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "nslb_sock.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"

#include "ns_log.h"
#include "ntlm.h"
#include "util.h"
#include "netstorm.h"
#include "ns_trans.h"
#include "ns_trace_level.h"
#include "ns_alloc.h"
#include "ns_exit.h"
#include "ns_nd_integration.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

#define MAX_PARAM_NAME_LENGTH 32+1
#define NAME	0
#define SINAME	1
#define PRESUF	2

static int validate_char_and_find_len(char *str, char flag)
{
  NSDL4_PARSING(NULL, NULL, "At Method, str = %s", str);
  char *s = str;
  while(*str)
  {
    if (flag == NAME)
    {
      if((isalnum(*str)) ||(*str == '-'))
      {
        str++;
      }
      else
      {
        return 0; // error case
      }
    }
    else if(flag == SINAME)
    {
      if((isalnum(*str)) ||(*str == '-') || (*str == '+') || (*str == '>'))
      {
        str++;
      }
      else
        return 0; // error case
    }
    else
    {
      if((isalnum(*str)) ||(*str == '-') || (*str == ';') || (*str == '='))
      {
        NSDL4_PARSING(NULL, NULL, "At If");
        str++;
      }
      else
      {
        return 0; // error case
      }
    }
  }
  NSDL4_PARSING(NULL, NULL, "End of Method");
  return (str - s); // error case
}

static int parse_custom_config(CorrelationIdSettings *cor_gset, char *str, char *buf, char *err_msg, int runtime_flag)
{
  NSDL2_PARSING(NULL, NULL, "Method Called.");
  
  if(!str || !(*str)){
    NSDL2_PARSING(NULL, NULL, "str is NULL. Returning");
    return -1;
  }

  NSDL2_PARSING(NULL, NULL, "Str = [%s]", str);

  //Initialize flags
  cor_gset->custom_corr_id_flag = 0;
  bzero(cor_gset->sourceID, 32);

  char *fields[12];
  int num_fields = get_tokens_with_multi_delimiter(str, fields, ";", 12);
  int len = 0;

  if(!num_fields) // Just a safety, should never happen
  {
    NSDL2_PARSING(NULL, NULL, "No fields found, returning");
    return -1;
  }

  int i;
  for(i = 0; i < num_fields; i++)
  {
    char *ptr = fields[i];
    if(!strncasecmp(ptr, "SI", 2))
    {
      ptr += 2;
      if(*ptr == '=')
      {
        ptr++;
        if(*ptr == ';' || *ptr == '\0')
        {
          NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011228, "", "General Settings");
        }
        else
          strcpy(cor_gset->sourceID, ptr);
      }
      else if(*ptr == ';' || *ptr == '\0')
        sprintf(cor_gset->sourceID, "Cavisson%s",  g_cavinfo.config);
      else
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011132, CAV_ERR_MSG_3);

      len = validate_char_and_find_len(cor_gset->sourceID, SINAME);
      if(!len || (len > MAX_PARAM_NAME_LENGTH))
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011227, "");
    }

    else if(!strcasecmp(ptr, "ID"))
        cor_gset->custom_corr_id_flag |= COR_ID_REQUEST_ID_ENABLED;

    else if(!strcasecmp(ptr, "VU"))
        cor_gset->custom_corr_id_flag |= COR_ID_VIRTUAL_USER_ID_ENABLED;

    else if(!strcasecmp(ptr, "GR"))
        cor_gset->custom_corr_id_flag |= COR_ID_LOCATION_ENABLED;

    else if(!strcasecmp(ptr, "SN"))
        cor_gset->custom_corr_id_flag |= COR_ID_SCRIPT_NAME_ENABLED;

    else if(!strcasecmp(ptr, "TE") || !strcasecmp(ptr, "NA"))
        continue;

    else if(!strcasecmp(ptr, "PC"))
        cor_gset->custom_corr_id_flag |= COR_ID_PAGE_NAME_ENABLED;

    else if(!strcasecmp(ptr, "AN"))
        cor_gset->custom_corr_id_flag |= COR_ID_AGENT_NAME_ENABLED;

    else if(!strcasecmp(ptr, "CI"))
        cor_gset->custom_corr_id_flag |= COR_ID_CLIENT_IP_ENABLED;

    else if(!strcasecmp(ptr, "DI"))
        cor_gset->custom_corr_id_flag |= COR_ID_DEST_IP_ENABLED;

    else if(!strcasecmp(ptr, "TS"))
        cor_gset->custom_corr_id_flag |= COR_ID_TIMESTAMP_ENABLED;
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011132, CAV_ERR_MSG_3);
    }
  }

  NSDL2_PARSING(NULL, NULL, "custom_corr_id_flag = '0x%x'", cor_gset->custom_corr_id_flag);

  NSDL2_PARSING(NULL, NULL, "Method End.");
  return 0;
}

 /*********************************************************************************
 * G_ENABLE_CORRELATION_ID <Group> <Mode> <Header&QueryParameter> <HeaderName> <QueryParameterName> <prefix> <config> <suffix>
 *    Example:
 *    G_ENABLE_CORRELATION_ID ALL 1 1 - - - SI=CavissonNS;NA;TE;SN - 
 *
 * where, 
 *  Group : 
 *       Any valid group name
 *
 *  Mode : Diable/Enable the correlation-id with/without of inline url
 *      	0 : Disable correlation-id (Default)
 * 	        1 : Enable  correlation-id for main URL
 *	            Syntax: G_ENABLE_CORRELATION_ID ALL 1 1 - - - SI=CavissonNS;NA;TE; -
 *      	2 : Enabled for main as well as inline URL
 *	            Syntax: G_ENABLE_CORRELATION_ID ALL 2 1 - - - SI=CavissonNS;NA;TE; -
 *  Header&QueryParameter : Header and Query Parameter insertion mode for correlation-id information 
 *      	1 : Header only(By Default) 
 *                 Syntax: G_ENABLE_CORRELATION_ID ALL 1 1 X-Correlation-ID - - SI=CavissonNS;NA;TE; - 
 *      	2 : URL Query Parameter only 
 *                 Syntax: G_ENABLE_CORRELATION_ID ALL 1 2 - X-Correlation-ID - SI=CavissonNS;NA;TE; - 
 *      	3 : Header as well as URL Query Parameter
 *                 Syntax: G_ENABLE_CORRELATION_ID ALL 1 3 X-Correlation-ID X-Correlation-ID - SI=CavissonNS;NA;TE; - 
 *  Header Name: User defined header name of correlation id, can provide only with mode 1 and 3 of Header&QueryParameter.
 *               By Default X-Correlation-ID. If user wants to provide different Header name.
 *               Syntax: G_ENABLE_CORRELATION_ID ALL 1 1 X-Cav-Cor_ID - - SI=CavissonNS;NA;TE; -
 *  Query Parameter Name: User defined query parameter name of correlation id, can provide only with mode 2 and 3 of Header&QueryParameter.
 *               By Default X-Correlation-ID. If user wants to provide different query parameter name.
 *               Syntax: G_ENABLE_CORRELATION_ID ALL 1 2 - X-Cav-Req-ID - SI=CavissonNS;NA;TE; -
 *  Prefix : Prefix for correlation id
 *           Syntax: G_ENABLE_CORRELATION_ID ALL 1 2 - X-Cav-Req-ID corId SI=CavissonNS;NA;TE; -
 *  Suffix : Suffix for correlation id
 *           Syntax: G_ENABLE_CORRELATION_ID ALL 1 2 - X-Cav-Req-ID SI=CavissonNS;NA;TE; cavId
 *
 *   Custom format for Correlation
 *	SI: SouceID
 *	TS: TimeStamp
 *      ID: Unique RequestID
 *	PC: Page Context
 *	VU: Virtual UserID
 *	GR: Geographic Region
 *	SN: Script Name
 *	NA: Transaction Name
 *	AN: AgentName
 *	TE: Test Name
 *	CI: Client IP
 *	DI: Destination IP
 ********************************************************************************/
int kw_set_g_enable_corr_id(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH] = "";
  char group_name[64] = "ALL";
  char mode;
  char hdr_qury_param;
  char hdr_name[MAX_PARAM_NAME_LENGTH] = "";
  char qury_param_name[MAX_PARAM_NAME_LENGTH] = "";
  char prefix[MAX_PARAM_NAME_LENGTH] = "";
  char custom_corr_args[MAX_DATA_LINE_LENGTH] = "";
  char suffix[MAX_PARAM_NAME_LENGTH] = "";
  int num_fields;
  int len = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called.");

 
  num_fields = sscanf(buf, "%s %s %d %d %s %s %s %s %s", 
                          keyword, group_name, (int *)&mode, (int *)&hdr_qury_param, hdr_name, qury_param_name, prefix, custom_corr_args, suffix);
  
  NSDL3_PARSING(NULL, NULL, "buf = '%s', keyword ='%s', group_name = '%s', mode = '%d', hdr_qury_param = '%d', hdr_name = '%s',"
                            " qury_param_name = %s, prefix = %s, custom_corr_args = %s, suffix = %s", 
                             buf, keyword, group_name, mode, hdr_qury_param, hdr_name, qury_param_name, prefix, custom_corr_args, suffix);
  if(num_fields < 3)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011132, CAV_ERR_MSG_1);

  if(mode > 2)
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011132, CAV_ERR_MSG_3);

  CorrelationIdSettings *cor_gset = &gset->correlationIdSettings;
  
  if(mode == 0)
  {
    NSDL1_PARSING(NULL, NULL, "Mode 0, hence skipping parsing G_ENABLE_CORRELATION_ID keyword.");
    cor_gset->mode = mode;
    cor_gset->header_query_mode = 0;
    return 0;
  }

  //Example : G_ENABLE_CORRELATION_ID ALL 1 1 - - - SI=CavissonNS;NA;TE;SN - 
  cor_gset->mode = mode;
  cor_gset->header_query_mode = hdr_qury_param;

  NSDL4_PARSING(NULL, NULL, "gset = %p, cor_gset = %p, Header Mode = %d", gset, cor_gset, cor_gset->header_query_mode);

  if((cor_gset->header_query_mode == 1 ) || (cor_gset->header_query_mode == 3))
  {
    if(*hdr_name == '-')
      strcpy(hdr_name, "X-Correlation-ID");

    len = validate_char_and_find_len(hdr_name, NAME);  

    NSDL4_PARSING(NULL, NULL, "Header Name = %s, len = %d", hdr_name, len);

    if(len && (len < MAX_PARAM_NAME_LENGTH))
    {
      MY_MALLOC(cor_gset->header_name, len + 1, "Header Name", -1) 
      strcpy(cor_gset->header_name, hdr_name); 
      cor_gset->header_name_len = len;  //This is needed in HTTP 2
    }
    else
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011133, "Header name", "");
  }

  if ((cor_gset->header_query_mode == 2 ) || (cor_gset->header_query_mode == 3))
  {
    if(*qury_param_name == '-')
      strcpy(qury_param_name, "X-Correlation-ID");

    len = validate_char_and_find_len(qury_param_name, NAME);

    NSDL4_PARSING(NULL, NULL, "Query Param Name = %s, len = %d", qury_param_name, len);

    if(len && (len < MAX_PARAM_NAME_LENGTH))
    {
      MY_MALLOC(cor_gset->query_name, len + 1, "Query Parameter Name", -1) 
      strcpy(cor_gset->query_name, qury_param_name);
    }
    else
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011133, "Query Param name", "");
  }

  if(*prefix != '-')
  {
    len = validate_char_and_find_len(prefix, PRESUF);

    NSDL4_PARSING(NULL, NULL, "Prefix = %s, len = %d", prefix, len);

    if(len && (len < MAX_PARAM_NAME_LENGTH))
    {
      MY_MALLOC(cor_gset->prefix, len + 1, "Prefix", -1)
      strcpy(cor_gset->prefix, prefix);
    }
    else
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011133, "Prefix", "");
  }
  else
  {
    len = 1;
    MY_MALLOC(cor_gset->prefix, len + 1, "Prefix", -1)
    strcpy(cor_gset->prefix, "");
  }

  if(*suffix != '-')
  {
    len = validate_char_and_find_len(suffix, PRESUF);  

    NSDL4_PARSING(NULL, NULL, "Suffix = %s, len = %d", suffix, len);

    if(len && (len < MAX_PARAM_NAME_LENGTH))
    {
      MY_MALLOC(cor_gset->suffix, len + 1, "Suffix", -1)
      strcpy(cor_gset->suffix, suffix);
    }
    else
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_CORRELATION_ID_USAGE, CAV_ERR_1011133, "Suffix", "");
  }
  else
  {
    len = 1;
    MY_MALLOC(cor_gset->suffix, len + 1, "Suffix", -1)
    strcpy(cor_gset->suffix, "");
  }

  parse_custom_config(cor_gset, custom_corr_args, buf, err_msg, runtime_flag);

  NSDL2_PARSING(NULL, NULL, "Method End.");
  return 0;
}

/***********************************************************************************************
 |  • NAME:   	
 | 	ns_cor_id_header_opt() - to fill correlation-id header value with custom configurations, which are coming on runtime.
 |
 |  • SYNOPSIS: 
 |      int ns_cor_id_header_opt(VUser *vptr, connection *cptr, char *cor_id_string_val, int size)
 |
 |	Arguments:
 |        vptr  	      - pointer to VUser structure 
 |        cptr  	      - pointer to connection structure 
 |        cor_id_string_val   - Custom Header Name buffer
 |        len		      - len of header buffer
 |        txPtr		      - pointer to TxInfo structure
 |
 |  • RETURN VALUE:
 |	length of whole header(name+value)
 ************************************************************************************************/
int ns_cor_id_header_opt(VUser *vptr, connection *cptr, char *cor_id_string_val, int size, int flag)
{
  TxInfo *txPtr = (TxInfo *) vptr->tx_info_ptr;
  GroupSettings *loc_gset = &runprof_table_shr_mem[vptr->group_num].gset;

  long long uniq_id;
  long long id = txPtr?txPtr->instance:vptr->page_instance;
  int len = 0;
  int tx_inline_id = ((cptr->url_num->proto.http.tx_hash_idx != -1)?cptr->url_num->proto.http.tx_hash_idx:(txPtr?txPtr->hash_code:-1));

  char *tx_name = (tx_inline_id != -1)?nslb_get_norm_table_data(&normRuntimeTXTable, tx_inline_id): NULL;

  if(flag == FLAG_HTTP1)
  {
    len = snprintf(cor_id_string_val, size, "%s: %sSI=%s;NA=%s;TE=%d", 
                               loc_gset->correlationIdSettings.header_name, loc_gset->correlationIdSettings.prefix, 
                               loc_gset->correlationIdSettings.sourceID, tx_name?tx_name:vptr->cur_page->page_name, testidx);
  }
  else
  {
    len = snprintf(cor_id_string_val, size, "%sSI=%s;NA=%s;TE=%d", 
                               loc_gset->correlationIdSettings.prefix, 
                               loc_gset->correlationIdSettings.sourceID, txPtr?tx_name:vptr->cur_page->page_name, testidx);
  }

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_REQUEST_ID_ENABLED){
    //How This number will be unique in case of NC??? 
    // Need to handle ......
    NSDL3_HTTP(vptr, NULL, "ID| my_port_index = %d and sess = %d", my_port_index, vptr->sess_inst);
    uniq_id = (((long long)my_port_index) << 56) + (((long long)vptr->sess_inst) << 24) + ((long long)(id) << 8);

    len += snprintf(cor_id_string_val + len, size, ";ID=%lld", uniq_id);
    uniq_id = 0;
  }

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_VIRTUAL_USER_ID_ENABLED){

    NSDL3_HTTP(vptr, NULL, "VU | my_port_index = %d and user_index = %d", my_port_index, vptr->user_index);
    uniq_id = (((long long)my_port_index) << 32) + vptr->user_index;

    len += snprintf(cor_id_string_val + len, size, ";VU=%lld", uniq_id);
  }

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_LOCATION_ENABLED)
    len += snprintf(cor_id_string_val + len, size, ";GR=%s", vptr->location->name);

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_SCRIPT_NAME_ENABLED)
    len += snprintf(cor_id_string_val + len, size, ";SN=%s", 
                        get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "-"));

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_AGENT_NAME_ENABLED)
    len += snprintf(cor_id_string_val + len, size, ";AN=%s", g_cavinfo.NSAdminIP);

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_PAGE_NAME_ENABLED)
    len += snprintf(cor_id_string_val + len, size, ";PC=%s", vptr->cur_page->page_name);

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_TIMESTAMP_ENABLED)
  {
    time_t t = time(NULL);
    
    len += snprintf(cor_id_string_val + len, size, ";TS=%ld", (t*1000));
  }

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_CLIENT_IP_ENABLED)
  {
    char *ptr = NULL;
    int lngt = sizeof(cptr->sin);
    if(!cptr->sin.sin6_port)
    {
      getsockname(cptr->conn_fd, (struct sockaddr *)&(cptr->sin), (socklen_t *)&lngt);
    }
    ptr = nslb_sock_ntop((struct sockaddr *)&cptr->sin);
    ptr+=5;  //remove IPV4: and IPV6: from output of nslb_get_src_addr()
    len += snprintf(cor_id_string_val + len, size,  ";CI=%s", ptr);
  }

  if(loc_gset->correlationIdSettings.custom_corr_id_flag & COR_ID_DEST_IP_ENABLED)
  {
    char *ptr = NULL;
    ptr = nslb_sock_ntop((struct sockaddr *)&cptr->cur_server);
    ptr+=5;  //remove IPV4: and IPV6: from output of nslb_sock_ntop()

    len += snprintf(cor_id_string_val + len, size, ";DI=%s", ptr);
  }

  len += snprintf(cor_id_string_val + len, size, "%s", loc_gset->correlationIdSettings.suffix);
  if(len >= size)
  {
    NSTL1(vptr, cptr, "Correlation-ID header exceeded size 1024, hence truncated.");
    len = size - 1;
  }
  return len;
}
