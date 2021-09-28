#include "ns_tr069_includes.h"

#define NS_ACS_CONF			"etc/tr069/conf/tr069_acs.conf"
#define NS_TR069_REGISTRATION_FILENAME	"etc/tr069/conf/tr069_registrations.spec"

typedef struct tr069_api_t {
  char tr069_api[128];
  char tr069_api_template[1024];
  char page_name[64];
  int  same_page_used_count;
} tr069_api_t;

static tr069_api_t tr069_api_list[NS_TR069_NUM_API] =  { 
  {"ns_tr069_cpe_invoke_inform",			"etc/tr069/templates/tr069_inform_req_template.xml", "TR069Inform", 0},
  {"ns_tr069_cpe_invite_rpc",				"etc/tr069/templates/tr069_invite_rpc.xml", "TR069InviteACS", 0},
  {"ns_tr069_cpe_execute_get_rpc_methods", 		"etc/tr069/templates/tr069_get_rpc_methods_rep_template.xml", "TR069GetRPCMethods", 0},
  {"ns_tr069_cpe_execute_set_parameter_values", 	"etc/tr069/templates/tr069_set_parameter_values_rep_template.xml", "TR069SetParamValues", 0},
  {"ns_tr069_cpe_execute_get_parameter_values", 	"etc/tr069/templates/tr069_get_parameter_values_rep_template.xml", "TR069GetParamValues", 0},
  {"ns_tr069_cpe_execute_get_parameter_names",		"etc/tr069/templates/tr069_get_parameter_names_rep_template.xml", "TR069GetParamNames", 0},
  {"ns_tr069_cpe_execute_set_parameter_attributes",	"etc/tr069/templates/tr069_set_parameter_attributes_rep_template.xml", "TR069SetParamAttributes", 0},
  {"ns_tr069_cpe_execute_get_parameter_attributes",	"etc/tr069/templates/tr069_get_parameter_attributes_rep_template.xml", "TR069GetParamAttributes", 0},
  {"ns_tr069_cpe_execute_add_object",			"etc/tr069/templates/tr069_add_object_rep_template.xml", "TR069AddObject", 0},
  {"ns_tr069_cpe_execute_delete_object",		"etc/tr069/templates/tr069_delete_object_rep_template.xml", "TR069DeleteObject", 0},
  {"ns_tr069_cpe_execute_download",			"etc/tr069/templates/tr069_download_rep_template.xml", "TR069Download", 0},
  {"ns_tr069_cpe_execute_reboot",			"etc/tr069/templates/tr069_reboot_rep_template.xml", "TR069Reboot", 0},
  {"ns_tr069_cpe_transfer_complete",		        "etc/tr069/templates/tr069_transfer_complete_rep_template.xml", "TR069TransferComplete", 0}
};


/*Must be used everyhere where we want to extract API name*/
#define MAX_API_NAME_LENGTH 1024

//ACS URL (http/https, host/port, url path) -> tr069_acs.conf has
//ACS_URL=http://www.acs.com/urlpath

void kw_set_tr069_acs_url(char* conf_line, char *acs_url, int flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;

  if ((num = sscanf(conf_line, "%s %s", keyword, acs_url)) != 2) {
    fprintf(stderr, "%s(): Need one argument.\n", (char*)__FUNCTION__);
    exit(-1);
  }
}

void kw_set_tr069_options(char* conf_line, int *options, int flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  int val1 = 0;
  int val2 = 0;

  if ((num = sscanf(conf_line, "%s %d %d", keyword, &val1, &val2)) < 2) {
    fprintf(stderr, "%s(): Need at least one argument.\n", (char*)__FUNCTION__);
    exit(-1);
  }

  if(val1) {
    *options |= NS_TR069_OPTIONS_AUTH; 
  }

  if(val2) {
    *options |= NS_TR069_OPTIONS_TREE_DATA; 
  }
}

//TR069_CPE_REBOOT_TIME <min> <max>,eg TR069_CPE_REBOOT_TIME 1000 5000
//time taken by cpe to reboot as per request from ACS using REBOOT RPC 
//Netstorm will add simulate reboot with time taken as random number between minimum time and maximum time
//Time can be given in milisecond.eg 1500 sec

int kw_set_tr069_cpe_reboot_time(char *buf, int *min_time, int *max_time, int flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  char  min_val[MAX_DATA_LINE_LENGTH];
  char  max_val[MAX_DATA_LINE_LENGTH];
  
  NSDL2_TR069(NULL, NULL,"method called");
  if  ((num = sscanf(buf, "%s %s %s", keyword, min_val, max_val)) <3) {     
     fprintf(stderr, "%s(): Need at least two argument.\n", (char*)__FUNCTION__);
     exit(-1); 
  } 

  if(!ns_is_numeric(min_val)) {
      fprintf(stderr, "min value = %s enter for TR069_CPE_REBOOT_TIME is not integer \n", min_val);
      exit(-1);
  }

  *min_time = atoi(min_val);
  NSDL2_TR069(NULL, NULL, "Minimum  Time = %d ", *min_time);

  if(!ns_is_numeric(max_val)) {
      fprintf(stderr, "max value = %s enter for TR069_CPE_REBOOT_TIME is not integer \n", max_val);
      exit(-1);
  }

   *max_time = atoi(max_val);
   NSDL2_TR069(NULL, NULL, "Maximum  Time = %d ", *max_time);
   
  if(*min_time > *max_time){
   fprintf(stderr, "%s(): minimum time should not be greater than maximum time.\n", (char*)__FUNCTION__);
   exit(-1);
  } 
  return 0;
}

//TR069_CPE_DOWNLOAD_TIME <min> <max>,eg TR069_CPE _DOWNLOAD_TIME 1000 5000
//time taken by cpe to download as per request from ACS using DOWNLOAD RPC
//Netstorm will add simulate download with time taken as random number between minimum time and maximum time
//Time can be given in milisecond.eg 1500 milisec

int kw_set_tr069_cpe_download_time(char *buf, int *min_time, int *max_time, int flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  char  min_val[MAX_DATA_LINE_LENGTH];
  char  max_val[MAX_DATA_LINE_LENGTH];

  NSDL2_TR069(NULL, NULL,"method called");
  if  ((num = sscanf(buf, "%s %s %s", keyword, min_val, max_val)) <3) {
     fprintf(stderr, "%s(): Need at least two argument.\n", (char*)__FUNCTION__);
     exit(-1);
  }

  if(!ns_is_numeric(min_val)) {
      fprintf(stderr, "min value = %s enter for TR069_CPE_DOWNLOAD_TIME is not integer \n", min_val);
      exit(-1);
  }

  *min_time = atoi(min_val);
  NSDL2_TR069(NULL, NULL, "Minimum  Time = %d ", *min_time);

  if(!ns_is_numeric(max_val)) {
      fprintf(stderr, "max value = %s enter for TR069_CPE_DOWNLOAD_TIME is not integer \n", max_val);
      exit(-1);
  }

   *max_time = atoi(max_val);
   NSDL2_TR069(NULL, NULL, "Maximum  Time = %d ", *max_time);

  if(*min_time > *max_time){
   fprintf(stderr, "%s(): minimum time should not be greater than maximum time.\n", (char*)__FUNCTION__);
   exit(-1);
  }
  return 0;
}

int kw_set_tr069_cpe_periodic_inform_time(char *buf, int *min_time, int *max_time, int flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  char  min_val[MAX_DATA_LINE_LENGTH];
  char  max_val[MAX_DATA_LINE_LENGTH];

  NSDL2_TR069(NULL, NULL,"method called");
  if  ((num = sscanf(buf, "%s %s %s", keyword, min_val, max_val)) <3) {
     fprintf(stderr, "%s(): Need at least two argument.\n", (char*)__FUNCTION__);
     exit(-1);
  }

  if(!ns_is_numeric(min_val)) {
      fprintf(stderr, "min value = %s enter for TR069_CPE_PERIDIC_INFORM_TIME is not integer \n", min_val);
      exit(-1);
  }

  *min_time = atoi(min_val);
  NSDL2_TR069(NULL, NULL, "Minimum  Time = %d ", *min_time);

  if(!ns_is_numeric(max_val)) {
      fprintf(stderr, "max value = %s enter for TR069_CPE_PERIODIC_INFORM_TIME is not integer \n", max_val);
      exit(-1);
  }

   *max_time = atoi(max_val);
   NSDL2_TR069(NULL, NULL, "Maximum  Time = %d ", *max_time);

  if(*min_time > *max_time){
   fprintf(stderr, "%s(): minimum time should not be greater than maximum time.\n", (char*)__FUNCTION__);
   exit(-1);
  }
  return 0;
}
 
void kw_set_tr069_cpe_data_dir(char *buf, char *tr069_file_dir, int flag) {

  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  char tmp_buf[MAX_LINE_LENGTH]="\0";
 
  num = sscanf(buf, "%s %s", keyword, tmp_buf);

  if(num != 2) {
    fprintf(stderr, "%s need exactly three arguments.\n", keyword);
    exit(-1);
  }  

  if(tmp_buf[0] == '/') {
    strcpy(tr069_file_dir, tmp_buf); 
  } else {
    sprintf(tr069_file_dir, "%s/data/tr069/%s", g_ns_wdir, tmp_buf);
  }

  if(access(tr069_file_dir, R_OK|W_OK|X_OK)) {
    fprintf(stderr, "Unable to access directory [%s].\n", tr069_file_dir);
    exit(-1);
  }
}
#if 0
static void tr069_parse_acs_conf() {
  FILE *fp;
  int num;
  char keyword[1024];
  char acs_conf_file_path[1024];
  char conf_line[1024];
  char text[4096];

  NSDL2_TR069(NULL, NULL, "Method Called");

  sprintf(acs_conf_file_path, "%s/%s", g_ns_wdir, NS_ACS_CONF);

  fp = fopen(NS_ACS_CONF, "r");

  if(!fp) {
    fprintf(stderr, "Unable to read ACS configuration [%s] file\n", NS_ACS_CONF);
    return;
  } else {
    while (nslb_fgets(conf_line, MAX_CONF_LINE_LENGTH, fp, 1) != NULL) {
     conf_line[strlen(conf_line)-1] = '\0';
     if((conf_line[0] == '#') || (conf_line[0] == '\0')) {
       continue;
     }

     if ((num = sscanf(conf_line, "%s %s", keyword, text)) != 2) {
       printf("%s(): At least two fields required  <%s>\n", (char*)__FUNCTION__, conf_line);
       continue;
     }
     if (strcasecmp(keyword, "ACS_URL") == 0) {
       kw_set_tr069_acs_url(conf_line);
     }
    }
  }
}
#endif

/*Can be used to extract api name if used give a general name to it*/
static char *tr069_extract_api_name(char *line, char *tr069_api_name, int max_len, int *bytes_parsed) {

  char *line_ptr;
  char *ptr;
  int len;

  line_ptr = strstr(line, "ns_tr069_");

  if(!line_ptr) {
    NSDL2_TR069(NULL, NULL, "Did not found [ns_tr069_] in line = [%s]", line);
    return NULL;
  }

  NSDL2_TR069(NULL, NULL, "Method Called, line_ptr = [%s]", line_ptr);
  ptr = index(line_ptr, '(');

  if(ptr) {
   len = ptr - line_ptr; 
   strncpy(tr069_api_name, line_ptr, len);
   tr069_api_name[len] = '\0';
   *bytes_parsed += len;
   NSDL2_TR069(NULL, NULL, "Parsed tr069_api_name = [%s]", tr069_api_name);
   return tr069_api_name;
  } else {
    return NULL;
  }
}

/*Common Function to set up data structure page/url tables*/
//parse_url_parameter()
static int parse_tr069_parameters(FILE *flow_fp, char *flow_file, char *starting_quotes,
                                int sess_idx, int url_idx, char *template_file_name) {

  char attribute_value[MAX_LINE_LENGTH];
  char header_buf[MAX_REQUEST_BUF_LENGTH];
  char body_buffer[1024];

  NSDL2_TR069(NULL, NULL, "Method Called, sess_idx = %d, url_idx = %d", sess_idx, url_idx);

  // Set Url 
  strcpy(attribute_value, global_settings->tr069_acs_url);
  if(set_url(attribute_value, flow_file, sess_idx, url_idx, 0, 1, NULL) == NS_PARSE_SCRIPT_ERROR)
     return NS_PARSE_SCRIPT_ERROR;

  requests[url_idx].proto.http.type = MAIN_URL;

  // Set method
  NSDL2_SCRIPT("Method [%s] found at script_line %d\n", attribute_value, script_ln_no);
  strcpy(attribute_value, "POST");
  set_http_method(attribute_value, flow_file, url_idx, script_ln_no);

  // Add header
  strcpy(header_buf, "{TR069ReqHeadersDP}");
  //header_buf[0] = '\0';
  strcat(header_buf, "\r\n");
  NSDL2_SCRIPT("Segmenting header_buf = [%s]", header_buf);
  segment_line(&(requests[url_idx].proto.http.hdrs), header_buf, 0, script_ln_no, sess_idx, flow_file);

  // Set Body
  if(template_file_name[0] != '\0') {
    sprintf(body_buffer, "$CAVINCLUDE$=%s\",\n", template_file_name);
  } else {
    body_buffer[0] = '\0';
  }

  if (copy_to_post_buf(body_buffer, strlen(body_buffer))) {
    fprintf(stderr, "Unable to save [%s] into post buf\n", body_buffer);
    return -1;
  }

  post_process_post_buf(url_idx, sess_idx, &script_ln_no, flow_file);
  
  NSDL2_SCRIPT("Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int tr069_get_template_idx(char *tr069_api_name) {
 
  int idx;
  NSDL2_TR069(NULL, NULL, "Method Called, tr069_api_name = [%s]", tr069_api_name);
  
  for(idx = 0; idx < NS_TR069_NUM_API; idx++) {
     if(strcmp(tr069_api_name, tr069_api_list[idx].tr069_api) == 0) return idx;
  }

  return -1;
}

void read_tr069_registration_vars(int sess_idx) {

  char registration_file[1024];
  FILE *reg_fp;
  int reg_ln_no = 0;

  NSDL2_TR069(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);
  sprintf(registration_file, "%s/%s", g_ns_wdir, NS_TR069_REGISTRATION_FILENAME);
  reg_fp = fopen(registration_file, "r");

  if(reg_fp) {
   if(read_urlvar_file(reg_fp, sess_idx, &reg_ln_no, registration_file)) {
     fprintf(stderr, "read_urlvar_file: failed\n");
     exit(1);
   }
   fclose(reg_fp);
  }
}

int parse_tr069(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{ 
  int url_idx;
  char tr069_api_name[MAX_API_NAME_LENGTH + 1];
  char template_file_name[MAX_FILE_NAME + 1];
  int bytes_eaten = 0;
  //static int pg_id = 0;
  char pagename[MAX_LINE_LENGTH + 1];
  char *page_end_ptr = NULL;
  //static int parse_acs_url_done = 0;
  char *ptr = script_line;

  NSDL2_TR069(NULL, NULL, "Method Called");

  if(global_settings->tr069_acs_url[0] == '\0') {
    fprintf(stderr, "Error: TR069_ACS_URL must be given for tr069 Protocol.\n");
    return NS_PARSE_SCRIPT_ERROR;
  }

  if(global_settings->tr069_data_dir[0] == '\0') {
    fprintf(stderr, "Error: TR069_CPE_DATA_DIR must be given for tr069 Protocol.\n");
    return NS_PARSE_SCRIPT_ERROR;
  }
#if 0
  if(!parse_acs_url_done) {  
    tr069_parse_acs_conf();
    parse_acs_url_done = 1;
  }
#endif

  //if(!(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED)) {
     //global_settings->protocol_enabled |= TR069_PROTOCOL_ENABLED;
  //}

  //Extract PageName from script_line
  //Page name would always be the first mandatory argument in ns_web_url
  //ret = extract_pagename(flow_fp, flow_file, script_line, pagename, &page_end_ptr);
  //if(ret == NS_PARSE_SCRIPT_ERROR)
  //return NS_PARSE_SCRIPT_ERROR;
 
  CLEAR_WHITE_SPACE(ptr);
  if(!tr069_extract_api_name(ptr, tr069_api_name, MAX_API_NAME_LENGTH, &bytes_eaten)) {
    fprintf(stderr, "Failed to extract api name from script [%s] at line [%d]\n", 
                        RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), script_ln_no);
    return NS_PARSE_SCRIPT_ERROR;
  }


  if(strstr(tr069_api_name, "ns_tr069_wait") || 
     strstr(tr069_api_name , "ns_tr069_register_rfc") ||
     strstr(tr069_api_name , "ns_tr069_get_periodic_inform_time") ||
     strstr(tr069_api_name , "ns_tr069_get_rfc")) {
    if(write_line(outfp, script_line, 0) == NS_PARSE_SCRIPT_ERROR)
      SCRIPT_PARSE_ERROR(ptr, "Error Writing in File ");
    return NS_PARSE_SCRIPT_SUCCESS;  // Not include as a page
  }

  int idx = tr069_get_template_idx(tr069_api_name);

  if(idx == -1) {
    fprintf(stderr, "No template found for api = [%s]\n", tr069_api_name);
    exit(-1);
  }

  // Page name generated based on the API
  if(tr069_api_list[idx].same_page_used_count == 0)
    sprintf(pagename, "%s", tr069_api_list[idx].page_name);
  else
    sprintf(pagename, "%s_%d", tr069_api_list[idx].page_name, tr069_api_list[idx].same_page_used_count);

  tr069_api_list[idx].same_page_used_count++;

  if((parse_and_set_pagename(tr069_api_name, tr069_api_name, flow_fp,
			     flow_file, script_line, outfp,
		             flow_outfile, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR) {
    return NS_PARSE_SCRIPT_ERROR;
  }

  init_url(&url_idx, 0, flow_file, 0);

  sprintf(template_file_name, "%s/%s", g_ns_wdir, tr069_api_list[idx].tr069_api_template);
  if(parse_tr069_parameters(flow_fp, flow_file, page_end_ptr, sess_idx, url_idx, template_file_name) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  //tr069_process_api_args(line + bytes_parsed, &bytes_parsed);
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*End of File*/
