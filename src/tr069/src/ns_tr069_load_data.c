#include "ns_tr069_includes.h"
#include "ns_tr069_data_file.h"
#include "ns_tr069_load_data.h"
#include "../../libnscore/nslb_util.h"

#define NS_TR069_SCHEMA_TYPE_FILE   "tr069_schema_path_type.dat"
#define NS_TR069_FULL_PATH_FILE     "tr069_full_paths.dat"

#define CPE_DATA_DELIM  "+"


int IGDDeviceInfoManufactureridx    = -1;
int IGDDeviceInfoManufacturerOUIidx = -1;
int IGDDeviceInfoProductClassidx    = -1;
int IGDDeviceInfoSerialNumberidx    = -1;
int IGDPeriodicInformEnableidx      = -1;
int IGDPeriodicInformIntervalidx    = -1;

int tr069_acs_url_idx = -1;
int IGDMgSvrURLidx    = -1;

int total_param_path_entries     = -1;
int total_full_param_path_entries = -1;

short         total_inform_parameters = 0;
short *tr069_inform_paramters_indexes = NULL;

TR069ParamPath_t    *tr069_param_path_table      = NULL;
TR069FullParamPath_t *tr069_full_param_path_table = NULL;

// Big buff logic copied from copy_into_big_buf
char* g_tr069_big_buf;
char* g_tr069_buf_ptr;

ns_bigbuf_t max_tr069_buffer_space;
ns_bigbuf_t used_tr069_buffer_space;

inline void init_g_tr069_big_buf(int size) {

  NSDL2_TR069(NULL, NULL, "Method called");

  MY_MALLOC (g_tr069_big_buf, size, "g_tr069_big_buf", -1);
  g_tr069_buf_ptr = g_tr069_big_buf;
  max_tr069_buffer_space = size;
  used_tr069_buffer_space = 0;
}

static int create_tr069_big_buf_space(void) {
  char* old_big_buf_ptr = g_tr069_big_buf;

  NSDL3_MISC(NULL, NULL, "Method called. used_tr069_buffer_space = %d, max_tr069_buffer_space = %d,"
                         " g_tr069_big_buf = %p, g_tr069_buf_ptr = %p",
                         used_tr069_buffer_space, max_tr069_buffer_space, g_tr069_big_buf, g_tr069_big_buf);

  MY_REALLOC (g_tr069_big_buf, max_tr069_buffer_space + DELTA_TR069_BIGBUFFER, "g_tr069_big_buf", -1);

  if (!g_tr069_big_buf){
    fprintf(stderr, "%s(): Error allocating more memory for g_tr069_big_buf\n", (char*)__FUNCTION__);
    return(FAILURE);
  } else {
    if (old_big_buf_ptr != g_tr069_big_buf) {
      g_tr069_buf_ptr = g_tr069_big_buf + used_tr069_buffer_space;
    }  
    max_tr069_buffer_space += DELTA_TR069_BIGBUFFER;
  }
  return (SUCCESS);
}

static int enough_tr069_memory(int space) {
  NSDL3_MISC(NULL, NULL, "Method called. space = %d, used_tr069_buffer_space = %d,"
                         " max_tr069_buffer_space = %d, g_tr069_big_buf = %p, g_tr069_buf_ptr = %p",
                         space, used_tr069_buffer_space, max_tr069_buffer_space, g_tr069_big_buf, g_tr069_buf_ptr);

  return (g_tr069_buf_ptr + space < g_tr069_big_buf + max_tr069_buffer_space);
}

ns_bigbuf_t
copy_into_tr069_big_buf(char* data, ns_bigbuf_t size) {
  ns_bigbuf_t data_loc = used_tr069_buffer_space;

  NSDL3_TR069(NULL, NULL, "Method called. size = %d, used_tr069_buffer_space = %d,"
                          " max_tr069_buffer_space = %d, g_tr069_big_buf = %p, g_tr069_buf_ptr = %p",
                          size, used_tr069_buffer_space, max_tr069_buffer_space, g_tr069_big_buf, g_tr069_buf_ptr);

  if (size == 0) {
    size = strlen(data);
    NSDL4_MISC(NULL, NULL, "Size of data = %d, data = %s", size, data);
  }

  while (!enough_tr069_memory(size + 1)) {
    if (create_tr069_big_buf_space() != SUCCESS) {
      NSDL2_TR069(NULL, NULL, "Returning as unable to get tr069_big_buf_space."); 
      return -1;
    }
  }

  memcpy(g_tr069_buf_ptr, data, size);
  g_tr069_buf_ptr[size] = '\0';

  g_tr069_buf_ptr += size + 1;
  used_tr069_buffer_space += size + 1;

  NSDL2_TR069(NULL, NULL, "Returning index = %d.", data_loc); 
  return data_loc;
}
// End here 

static inline void tr069_set_acs_url_idx() {

  int request_type;
  int port;
  char hostname[1024];
  char request_line[1024];

  NSDL2_TR069(NULL, NULL, "Method called");
  
  if(!extract_hostname_and_request(global_settings->tr069_acs_url, hostname, request_line, &port, &request_type,
                                   NULL, -1)) {
    if(port == -1)  {
      if (request_type == HTTPS_REQUEST) {// https
         port = 443;
      } else if(request_type == HTTP_REQUEST) { /* IPV4 */
          port = 80;
      } else {
        fprintf(stderr, "Error: Unable to get port for acs url [%s]\n", global_settings->tr069_acs_url); 
        exit(-1);
      }
    }
    unsigned short rec_server_port; //Sending Dummy
    int hostname_len = find_host_name_length_without_port(hostname, &rec_server_port);
    if ((tr069_acs_url_idx = find_gserver_shr_idx(hostname, port, hostname_len)) == -1) {
      fprintf(stderr, "Error: Unable to get index for acs url [%s]\n", global_settings->tr069_acs_url); 
      exit(-1);
    }
  }

  NSDL2_TR069(NULL, NULL, "ACS URL = [%s] at index = [%d]", global_settings->tr069_acs_url, tr069_acs_url_idx);
}

static inline void tr069_init_reusable_indexes() {

  NSDL2_TR069(NULL, NULL, "Method called");

  IGDMgSvrURLidx = tr069_path_name_to_index("InternetGatewayDevice.ManagementServer.URL",
                                      strlen("InternetGatewayDevice.ManagementServer.URL"));

  IGDDeviceInfoManufactureridx    = tr069_path_name_to_index("InternetGatewayDevice.DeviceInfo.Manufacturer", 
                                                      strlen("InternetGatewayDevice.DeviceInfo.Manufacturer")); 
  IGDDeviceInfoManufacturerOUIidx = tr069_path_name_to_index("InternetGatewayDevice.DeviceInfo.ManufacturerOUI",
                                                      strlen("InternetGatewayDevice.DeviceInfo.ManufacturerOUI"));
  IGDDeviceInfoProductClassidx    = tr069_path_name_to_index("InternetGatewayDevice.DeviceInfo.ProductClass", 
                                                      strlen("InternetGatewayDevice.DeviceInfo.ProductClass"));
  IGDDeviceInfoSerialNumberidx    = tr069_path_name_to_index("InternetGatewayDevice.DeviceInfo.SerialNumber", 
                                                      strlen("InternetGatewayDevice.DeviceInfo.SerialNumber"));

  IGDPeriodicInformEnableidx      = tr069_path_name_to_index("InternetGatewayDevice.ManagementServer.PeriodicInformEnable",  
                                                      strlen("InternetGatewayDevice.ManagementServer.PeriodicInformEnable"));

  IGDPeriodicInformIntervalidx    = tr069_path_name_to_index("InternetGatewayDevice.ManagementServer.PeriodicInformInterval",  
                                                      strlen("InternetGatewayDevice.ManagementServer.PeriodicInformInterval"));
  tr069_set_acs_url_idx();
}


#if 0
int tr069_param_path_table_comp(const void* ent1, const void* ent2) {

    NSDL2_TR069(NULL, NULL, "Method called");

    if (((TR069ParamPath_t*)ent1)->path_hashcode < ((TR069ParamPath_t*)ent2)->path_hashcode)
       return -1;
    else if (((TR069ParamPath_t*)ent1)->path_hashcode > ((TR069ParamPath_t*)ent2)->path_hashcode)
       return 1;
    else
       return 0;
}
#endif

static int tr069_fill_cpe_data(int user_idx, TR069FullParamPath_t *full_path_table_ptr, char *buffer) {

  TR069ParamPath_t *path_ptr;
  NSDL2_TR069(NULL, NULL, "Method Called"); 
  int len_written =0 ;

  path_ptr = &tr069_param_path_table[(user_idx * total_param_path_entries) + full_path_table_ptr->path_table_index];

  NSDL2_TR069(NULL, NULL, "User idx = %d, path_table_index = %d",
                           user_idx, full_path_table_ptr->path_table_index); 
  len_written = sprintf(buffer, "%d=", full_path_table_ptr->path_table_index); 

  NSDL2_TR069(NULL, NULL, "path_type_flags = 0x%x", path_ptr->path_type_flags);
  /* Earliar i was saving only Scaler but we should write scalar also
   * i.e InternetGatewayDevice.WANDevice.1.WANConnectionDevice.1.WANPPPConnection.1.ExternalIPAddress*/
  if(/*path_ptr->path_type_flags & NS_TR069_PARAM_SCALAR */ 1) {
     //NSDL2_TR069(NULL, NULL, "Scalar"); 
     if(path_ptr->path_type_flags & (NS_TR069_PARAM_STRING | NS_TR069_PARAM_DATE_TIME)) {
       len_written += sprintf(buffer + len_written, "%s|%d|%s\n",
                       RETREIVE_TR069_BUF_DATA(path_ptr->data.value->value_idx),
                       path_ptr->data.value->notification,
                       "NA");
     } else {
       len_written += sprintf(buffer + len_written, "%lu|%d|%s\n", 
                      path_ptr->data.value->value_idx,
                      path_ptr->data.value->notification,
                      "NA");
     }
  } else {
     NSDL2_TR069(NULL, NULL, "Something else"); 
  }

  NSDL2_TR069(NULL, NULL, "len_written = %d, buffer = [%s]",
                           len_written, buffer); 
  return len_written;
}

inline void tr069_dump_cpe_data_for_each_users(int num_users, char *tr069_data_writeback_location) {

  int user_idx;
  int idx;
  //FILE *nvm_data_fp, *nvm_data_index_fp;
  FILE *nvm_data_fp;
  TR069FullParamPath_t    *ptr;
  char buf[4096];
  int len_written = 0;
  char nvm_data_file[1024];
  //char nvm_data_index_file[1024];
  unsigned long offset = 0;

  if(!(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED)) {
     return;
  }

  NSDL2_TR069(NULL, NULL, "Method Called, num_users = %d", num_users); 

  sprintf(nvm_data_file, "%s/tr069_cpe_data_nvm%02d.dat", tr069_data_writeback_location, my_port_index);
  //sprintf(nvm_data_index_file, "%s/tr069_cpe_data_nvm%02d.dat.index", tr069_data_writeback_location, my_port_index);

  nvm_data_fp       =  fopen(nvm_data_file, "w");
  //nvm_data_index_fp =  fopen(nvm_data_index_file, "w");

  if(!nvm_data_fp) {
    fprintf(stderr, "TR069 Error: Unable to write data to [%s].\n", nvm_data_file);
    END_TEST_RUN 
  }

  /*if(!nvm_data_index_fp) {
    fprintf(stderr, "TR069 Error: Unable to write data to [%s].\n", nvm_data_file);
    END_TEST_RUN 
  }*/

  fprintf(stderr, "TR069Info: NVM %d: Saving data to [%s] ...\n", my_port_index, nvm_data_file);

  for(user_idx = 0; user_idx < num_users; user_idx++) {
    offset += len_written;
    len_written = 0;
    //fprintf(nvm_data_index_fp, "%lu\n", offset);
    for(idx = 0; idx < total_full_param_path_entries; idx++) {
      ptr = &tr069_full_param_path_table[idx];
      len_written += tr069_fill_cpe_data(user_idx, ptr, buf + len_written);
    }
    len_written += sprintf(buf + len_written, "%s\n", CPE_DATA_DELIM);
    fwrite(buf, strlen(buf), 1, nvm_data_fp);
  }

  if(nvm_data_fp) {
    fclose(nvm_data_fp);
  }
 /* if(nvm_data_index_fp) {
    fclose(nvm_data_index_fp);
  }*/
}

#if 1
static inline void dump_parent_path_table() {

  IW_UNUSED(int len);
  int idx;

  IW_UNUSED(len = 0);
  NSDL2_TR069(NULL, NULL, "Method Called"); 

  for(idx = 0; idx < total_param_path_entries; idx++) {
    NSDL2_TR069(NULL, NULL, "%d = [%s]", idx, tr069_get_name_from_index(idx, &len));
  }
}
#endif

static inline void 
tr069_load_full_path_data_for_vuser(TR069ParamPath_t *tr069_vuser_param_path_table,
                                    TR069DataValue_t *tr069_vuser_data_ptr,
                                    char *in_buf) { 
  int num_tokens;
  char *fields[20];
  char *value = NULL;
  char *tmp;
  int idx;

  NSDL2_TR069(NULL, NULL, "Method Called, in_buf = [%s]", in_buf); 

  tmp = index(in_buf, '=');
  if(tmp) { 
    value = tmp + 1 ;
    *tmp =  0;
  } else {
    NSDL2_TR069(NULL, NULL, "Returning as no '=' found in [%s]", in_buf); 
    return;
  }

  // It has the index of tr069_vuser_param_path_table
  idx = atoi(in_buf);
  CLEAR_WHITE_SPACE(value);

  num_tokens = get_tokens(value, fields, "|", 20);

  if(num_tokens < 1) {
    fprintf(stderr, "Error: Data fields has less tokens.\n");
    exit(1);
  }

  if(tr069_vuser_param_path_table[idx].path_type_flags & NS_TR069_PARAM_STRING ||
     tr069_vuser_param_path_table[idx].path_type_flags & NS_TR069_PARAM_DATE_TIME) {

     tr069_vuser_data_ptr->value_idx = copy_into_tr069_big_buf(fields[0], TR069_MAX_PARAMETER_VALUE_LEN);
     tr069_vuser_data_ptr->data_len  = strlen(fields[0]);
  } else if (tr069_vuser_param_path_table[idx].path_type_flags & NS_TR069_PARAM_UINT) {

    tr069_vuser_data_ptr->value_idx = atoi(fields[0]); 
    tr069_vuser_data_ptr->data_len  = 0; 
  } else if (tr069_vuser_param_path_table[idx].path_type_flags & NS_TR069_PARAM_BOOLEAN) {

    tr069_vuser_data_ptr->value_idx   = atoi(fields[0]) ? 1:0; 
    tr069_vuser_data_ptr->data_len  = 0; 
  } else { // Object 

    fprintf(stderr, "Error: TR069 Object not supported.\n");
    tr069_vuser_data_ptr->data_len  = 0; 
  }

  if(num_tokens >= 2) {
    tr069_vuser_data_ptr->notification = atoi(fields[1]);
  }

  tr069_vuser_param_path_table[idx].data.value = tr069_vuser_data_ptr;
  NSDL2_TR069(NULL, NULL, "Data saved at = [%p]", tr069_vuser_data_ptr); 
}

/*Assumption is data file shoud have same parameter as in full param data file*/
inline void tr069_load_data_for_vuser(TR069ParamPath_t    *tr069_vuser_param_path_table, 
                                      TR069FullParamPath_t *tr069_vuser_param_full_path_table,
                                      TR069DataValue_t    *tr069_vuser_data_ptr, 
                                      int user_idx, FILE *fp) {

  int len;
  char read_line[NS_TR069_BUF_LEN + 1];
  char *read_line_ptr;
  int idx = 0;

  NSDL2_TR069(NULL, NULL, "Method Called");

  while(nslb_fgets(read_line, NS_TR069_BUF_LEN, fp, 0)) {
    NSDL2_TR069(NULL, NULL, "read_line = [%s]", read_line);
    len = strlen(read_line); 
    read_line[len - 1] = '\0';   // Remove newline char

    if(read_line[0] == '#' || read_line[0] == '\0')  { 
       continue;   // Ignore comments & empty lines
    }

    read_line_ptr = read_line;
    
    NSDL2_TR069(NULL, NULL, "User: %d, read_line_ptr = [%s]", user_idx, read_line_ptr);

    if(!strncmp(read_line_ptr, CPE_DATA_DELIM, strlen(CPE_DATA_DELIM)) || idx >= total_full_param_path_entries) {
       break;
    }

    tr069_load_full_path_data_for_vuser(tr069_vuser_param_path_table,
                                        &tr069_vuser_data_ptr[idx],
                                        read_line_ptr);
    idx++;
  }
  NSDL2_TR069(NULL, NULL, "idx = %d, total_full_param_path_entries = %d", 
                           idx, total_full_param_path_entries); 

  if(idx < total_full_param_path_entries) {
     fprintf(stderr, "Error: For NVM:%d user id (%d) data must be (%d) but it is (%d)\n",
                     my_port_index, user_idx, total_full_param_path_entries, idx);
     END_TEST_RUN
  }
}

// Must be call from new_user
void inline tr069_vuser_init(VUser *vptr) {
  NSDL2_TR069(vptr, NULL, "Method Called");
}


static FILE *tr069_set_up_nvm_data_offset(int num_users) {

  char nvm_data_file[2048];
  FILE *nvm_data_fp = NULL;
  //unsigned long size = 0; 

 
  NSDL2_TR069(NULL, NULL, "Method Called, num_users = %d, total_entries = %d, start_offset = %d",
                           num_users, tr069_user_cpe_data[my_port_index].total_entries,
                           tr069_user_cpe_data[my_port_index].start_offset); 

  sprintf(nvm_data_file, "%s/tr069_cpe_data.dat", global_settings->tr069_data_dir);

  nvm_data_fp = fopen(nvm_data_file, "r");

  if(!nvm_data_fp) {
    fprintf(stderr, "Unable to open %s\n", nvm_data_file);
    END_TEST_RUN 
  }

  if(tr069_user_cpe_data[my_port_index].total_entries != num_users) {
     fprintf(stderr, "TR069 Data Loading Error: "
                     "Total entries distributed (%u) is not same as number of users(%d).\n",
                     tr069_user_cpe_data[my_port_index].total_entries,
                     num_users); 
     END_TEST_RUN 
  }

  unused_data_size_from_file(nvm_data_fp, tr069_user_cpe_data[my_port_index].start_idx, CPE_DATA_DELIM);

  return nvm_data_fp;
}

/* Each NVM will allocate for the number of users & copy */
/* Each users table start pointer will start from (user_id * total_param_path_entries)*/
static inline void tr069_tables_dup_for_all_vusers(int num_users){ 

  int user_idx;
  FILE *nvm_data_fp = NULL;

  TR069ParamPath_t     *path_start_ptr       = NULL;
  TR069DataValue_t     *data_value_table     = NULL; 
  TR069DataValue_t     *data_value_table_ptr = NULL; 
  //int total_cpe_parameter_values  = num_users * total_full_param_path_entries; 
  //int total_cpe_device_parameters = num_users * 4; // 4 are device paramters  

  /*  total_cpe_parameter_values = num_users * total_full_param_path_entries
   *  total_cpe_device_parametersnum_users * 4  // 4 are device paramters
   *  total size = (total_cpe_parameter_values + total_cpe_device_parametersnum_users) * 64
   *  */
  int  init_g_tr069_big_buf_size = (num_users * (total_full_param_path_entries + 4)) * TR069_MAX_PARAMETER_VALUE_LEN;
  int path_table_size, full_path_table_size, data_value_table_size;
  int tot_path_table_size, tot_data_value_table_size; 

  path_table_size         = (total_param_path_entries      * sizeof(TR069ParamPath_t));
  full_path_table_size    = (total_full_param_path_entries * sizeof(TR069FullParamPath_t));
  data_value_table_size   = (total_full_param_path_entries * sizeof(TR069DataValue_t));

  tot_path_table_size         = num_users * path_table_size; 
  tot_data_value_table_size   = num_users * data_value_table_size;
  NSDL2_TR069(NULL, NULL, "Method Called, num_users = %d", num_users); 

  nvm_data_fp  = tr069_set_up_nvm_data_offset(num_users);

  init_g_tr069_big_buf(init_g_tr069_big_buf_size);

  MY_MALLOC(data_value_table, tot_data_value_table_size, "Allocating TR069DataValue_t for each user", -1);
  memset(data_value_table,   -1, tot_data_value_table_size);

  user_idx = 0;  // FOr first user evrything is created by PARENT
  data_value_table_ptr = data_value_table;
  tr069_load_data_for_vuser(tr069_param_path_table, tr069_full_param_path_table,
                            data_value_table_ptr, user_idx, nvm_data_fp);

  if(num_users > 1) {
    MY_REALLOC(tr069_param_path_table, tot_path_table_size, "Allocating TR069ParamPath_t for each user", -1);

    // Realloc can change the tr069_param_path_table
    path_start_ptr = tr069_param_path_table;

    for(user_idx = 1; user_idx < num_users; user_idx++) {

       NSDL2_TR069(NULL, NULL, "memcopying for user_idx = %d", user_idx); 
       path_start_ptr += total_param_path_entries;
       data_value_table_ptr += total_full_param_path_entries;
   
       memcpy(path_start_ptr, tr069_param_path_table, path_table_size);

       tr069_load_data_for_vuser(path_start_ptr, tr069_full_param_path_table,
                                      data_value_table_ptr, user_idx, nvm_data_fp);
    }
  }

  if(nvm_data_fp) {
     fclose(nvm_data_fp);
  }

  fprintf(stderr, "TR069Info: NVM: %d Memory consumed by [%'d] users = [%'lu]\n",
                   my_port_index, num_users,
                   (long unsigned int)(init_g_tr069_big_buf_size + tot_path_table_size + full_path_table_size
                   + tot_data_value_table_size + (num_users * sizeof(VUser))));


#if NS_DEBUG_ON
  char tr069_data_wb_loc[1024];
  sprintf(tr069_data_wb_loc, "%s/logs/TR%d/ns_files/", g_ns_wdir, testidx);
  tr069_dump_cpe_data_for_each_users(num_users, tr069_data_wb_loc);
#endif
}

inline void tr069_nvm_init(int num_users) {

  u_ns_ts_t now = get_ms_stamp();
  NSDL2_TR069(NULL, NULL, "Method Called, num_users = %d", num_users); 

  if(!(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED)) { 
    return;
  }

  tr069_init_addr_for_admin_ip();
 
  tr069_tables_dup_for_all_vusers(num_users);

  fprintf(stderr, "TR069Info: NVM: %d init Time Taken = [%'llu] for users [%'d]\n", my_port_index, (get_ms_stamp() - now), num_users);
}

/* This function fill TR069PathTrans by matching path name into full path name table
 * e.g:
 * if path table has
 *  a.
 * and full path table has
 *  0 a.b
 *  1 a.c
 *  2 e.p
 *  than path table will have start index 0  & num indexes 2 
 *  as we a.b and a.c are the subset of a.
 * */ 
static inline void tr069_arrange_path_tables() {

  int path_name_len, full_path_name_len;
  char *path_name, *full_path_name;
 
  int path_idx;
  int full_path_idx;

  NSDL2_TR069(NULL, NULL, "Method Called, total_param_path_entries = [%d],"
                          " total_full_param_path_entries = [%d]",
                          total_param_path_entries, total_full_param_path_entries);

  for(path_idx = 0; path_idx < total_param_path_entries; path_idx++) {
    NSDL2_TR069(NULL, NULL, "Param Path Idx = %d", path_idx);
    // Get path name
    path_name = tr069_get_name_from_index(path_idx, &path_name_len);
    NSDL2_TR069(NULL, NULL, "path_idx = %d, path_name = [%s]", 
                             path_idx, path_name);

    for(full_path_idx = 0; full_path_idx < total_full_param_path_entries; full_path_idx++) {
       NSDL2_TR069(NULL, NULL, "Param Full Path Idx = %d", full_path_idx);
       // Get full path name
       full_path_name = tr069_get_name_from_index(tr069_full_param_path_table[full_path_idx].path_table_index, &full_path_name_len);
       NSDL2_TR069(NULL, NULL, "full_path_idx = %d, full_path_name = [%s]", 
                               full_path_idx, full_path_name);

       if(!strncmp(path_name, full_path_name, path_name_len)) {
          if(tr069_param_path_table[path_idx].data.full_path.num_indexes == 0) {
            tr069_param_path_table[path_idx].data.full_path.start_index = full_path_idx;
          } 
          tr069_param_path_table[path_idx].data.full_path.num_indexes += 1;
       } else {
         //break;
       }
    }
  }

#if 0
  // Sort w.r.t hash codes so that we can bsearch
  qsort(tr069_param_path_table, total_param_path_entries, sizeof(TR069ParamPath_t), tr069_param_path_table_comp);

  // Now put index of tr069_param_path_table in tr069_full_param_path_table instead of path_table_index
  for(full_path_idx = 0; full_path_idx < total_full_param_path_entries; full_path_idx++) {
    for(path_table_idx = 0; path_table_idx < total_param_path_entries; path_table_idx++) {
      if(tr069_param_path_table[path_table_idx].path_hashcode == tr069_full_param_path_table[full_path_idx].path_table_index) {
         tr069_full_param_path_table[full_path_idx].path_table_index = path_table_idx;
      }
    }
  }
#endif
}

/*I dont have this function in so file*/
char *tr069_get_name_from_index(int param_path_idx, int *param_len) {

  int param_hash_code;
  char *param_name;
  *param_len = 0;

  NSDL2_TR069(NULL, NULL, "Method Called, param_path_idx = [%d]", param_path_idx); 

  // Get hash code first
  param_hash_code = tr069_index_to_hash_code(param_path_idx);

  if(param_hash_code < 0) {
    fprintf(stderr, "Error: For parameter path index (%d) got invalid hash code ().\n",
                    param_hash_code);
    exit(-1);
  }

  NSDL2_TR069(NULL, NULL, "param_hash_code = [%d]", param_hash_code); 
  // Get parameter name using hash code
  param_name = tr069_hash_code_to_path_name(param_hash_code);
  *param_len = strlen(param_name);

  NSDL2_TR069(NULL, NULL,  "param_name = [%s], param_len = [%d]", param_name, *param_len);
  return param_name;
}

/* This function:
 * 1. Read the tr069_schema_path_type.dat (which contains total number of parameters & their properties)
 * 2. Read the first line & get the total parameters and allocate memmory 
 * 3. Read the other lines and fill data.
 */
static void tr069_create_params_path_table(char *schema_path_type_file) {

  FILE *fp;
  char *fields[16]; 
  int num_fields;
  char read_line[NS_TR069_BUF_LEN + 1];
  char *read_line_ptr, *ptr;
  int len, idx = 0;
  int line_number;

  NSDL2_TR069(NULL, NULL, "Method Called, schema_path_type_file = [%s]", schema_path_type_file); 
  // Now put all path types
  line_number = idx = 0;
  fp = fopen(schema_path_type_file, "r");
 
  if(!fp) {
    fprintf(stderr, "TR069: Error in opening file [%s].\n", schema_path_type_file);
    END_TEST_RUN 
  }
  // Put all the hash codes of every paths
  while(nslb_fgets(read_line, NS_TR069_BUF_LEN, fp, 0)) {
    line_number++;
    len = strlen(read_line); 
    read_line[len - 1] = '\0';   // Remove newline char

    if(read_line[0] == '\0')  { 
       continue;   // Ignore comments & empty lines
    }
    
    read_line_ptr = read_line;
    NSDL2_TR069(NULL, NULL, "total_param_path_entries = [%d], read_line_ptr = [%s]", total_param_path_entries, read_line_ptr);
    if(total_param_path_entries == -1) {
       if(!strncmp(read_line_ptr, "#Total=", strlen("#Total="))) {
         ptr = read_line_ptr + strlen("#Total=");
         total_param_path_entries = atoi(ptr);
         MY_MALLOC(tr069_param_path_table, total_param_path_entries * sizeof(TR069ParamPath_t), 
                   "tr069_param_path_table", -1);
         memset(tr069_param_path_table, 0, total_param_path_entries * sizeof(TR069ParamPath_t));
         MY_MALLOC(tr069_inform_paramters_indexes, total_param_path_entries * sizeof(short), 
                   "tr069_inform_paramters_indexes", -1);
         memset(tr069_inform_paramters_indexes, -1, total_param_path_entries * sizeof(short));
       }
       continue;
    }

    if(total_param_path_entries <= 0 ) {
      fprintf(stderr, "total_param_path_entries must be > 0 in [%s] file.\n",
                       schema_path_type_file);
      exit(-1);
    }
    if(idx >= total_param_path_entries  ) {
      fprintf(stderr, "Got more than (%d) entries in file [%s].\n",
                       idx, schema_path_type_file);
      exit(-1);
    }

    //type,mode,datatype,min,max,inform
    num_fields = get_tokens(read_line, fields, ",", 16);

    if(num_fields < 6) {
      fprintf(stderr, "Error: File [%s] has lesser fields at line number (%d).\n",
                       schema_path_type_file, line_number);
      exit(-1);
    }

    // type scalar or vector
    if(!strcmp(fields[0], "Scalar")) {
      tr069_param_path_table[idx].path_type_flags |= NS_TR069_PARAM_SCALAR; 
    } else if(!strcmp(fields[0], "Vector")) {
      tr069_param_path_table[idx].path_type_flags &= ~NS_TR069_PARAM_SCALAR; 
    } else {
      fprintf(stderr, "Error: File [%s] has invalid type [%s] at line number (%d)\n.",
                       schema_path_type_file, fields[0], line_number);
      exit(-1);
    }

    // mode read or write
    if(!strcmp(fields[1], "Read")) {
      tr069_param_path_table[idx].path_type_flags |= NS_TR069_PARAM_RONLY; 
    } else if(!strcmp(fields[1], "Write")) {
      tr069_param_path_table[idx].path_type_flags &= ~NS_TR069_PARAM_RONLY; 
    } else {
      fprintf(stderr, "Error: File [%s] has invalid mode [%s] at line number (%d).\n",
                       schema_path_type_file, fields[1], line_number);
      exit(-1);
    }

    // data type object,unsignedInt, string, dateTime or boolean
    if(!strcmp(fields[2], "object")) {
      tr069_param_path_table[idx].path_type_flags |= NS_TR069_PARAM_OBJECT; 
    } else if(!strcmp(fields[2], "unsignedInt") || !strcmp(fields[2], "int")) {
      tr069_param_path_table[idx].path_type_flags |= NS_TR069_PARAM_UINT; 
    } else if(!strcmp(fields[2], "string")) {
      tr069_param_path_table[idx].path_type_flags |= NS_TR069_PARAM_STRING; 
    } else if(!strcmp(fields[2], "dateTime")) {
      tr069_param_path_table[idx].path_type_flags |= NS_TR069_PARAM_DATE_TIME; 
    } else if(!strcmp(fields[2], "boolean")) {
      tr069_param_path_table[idx].path_type_flags |= NS_TR069_PARAM_BOOLEAN; 
    } else {
      fprintf(stderr, "Error: File [%s] has invalid data type [%s] at line number (%d)\n.",
                       schema_path_type_file, fields[2], line_number);
      exit(-1);
    }

    // min value 
    tr069_param_path_table[idx].min_value = atoi(fields[3]);

    // max_value 
    tr069_param_path_table[idx].max_value = atoi(fields[4]);

    if(strcmp(fields[5], "NA")) {
      if(!strcmp(fields[5], "Yes")) {
        tr069_param_path_table[idx].path_type_flags |= NS_TR069_INFORM_PARAMETER; 
        tr069_inform_paramters_indexes[total_inform_parameters++] = idx;
      } else if(!strcmp(fields[5], "No")) {
        tr069_param_path_table[idx].path_type_flags &= ~NS_TR069_INFORM_PARAMETER; 
      } else {
        fprintf(stderr, "Error: File [%s] has invalid inform  notification [%s] at line number (%d)\n.",
                         schema_path_type_file, fields[5], line_number);
        exit(-1);
      }
    }
    idx++;
  }

  NSDL2_TR069(NULL, NULL, "total_inform_parameters = %d, total_param_path_entries = %d",
                           total_inform_parameters, total_param_path_entries);
  if(total_param_path_entries <= 0 ) {
    fprintf(stderr, "total_param_path_entries must be > 0 in [%s] file.\n",
                     schema_path_type_file);
    exit(-1);
  }
  fclose(fp);
}

/* This function:
 * 1. Read the full paths from tr069_full_paths.dat
 * 2. Read the first line & get the total parameters (full paths) and allocate memmory 
 * 3. Read the first fields with = delimeter which is a index to tr069_param_path_table 
 */
static void tr069_create_params_full_path_table(char *full_path_file) {

  FILE *fp;
  char read_line[NS_TR069_BUF_LEN + 1];
  char *ptr, *read_line_ptr;
  int len, idx = 0;
  int line_number = 0;
  int index_to_param_path_table;

  NSDL2_TR069(NULL, NULL, "Method Called, full_path_file = [%s]", full_path_file);

  fp = fopen(full_path_file, "r");
 
  if(!fp) {
    fprintf(stderr, "TR069: Error in opening file [%s].\n", full_path_file);
    END_TEST_RUN 
  }

  while(nslb_fgets(read_line, NS_TR069_BUF_LEN, fp, 0)) {
    line_number++;
    len = strlen(read_line); 
    read_line[len - 1] = '\0';   // Remove newline char

    if(read_line[0] == '\0')  { 
       continue;   // Ignore comments & empty lines
    }

    read_line_ptr = read_line;

    if(total_full_param_path_entries == -1) {
      if(!strncmp(read_line_ptr, "#Total=", strlen("#Total="))) {
        ptr = read_line_ptr + strlen("#Total=");
        total_full_param_path_entries = atoi(ptr);
        MY_MALLOC(tr069_full_param_path_table,
                  total_full_param_path_entries * sizeof(TR069FullParamPath_t), 
                  "tr069_full_param_path_table", -1);
        memset(tr069_full_param_path_table, 0, 
               total_full_param_path_entries * sizeof(TR069FullParamPath_t));
      }
      continue;
    }

    if(total_full_param_path_entries <= 0 ) {
      fprintf(stderr, "total_full_param_path_entries must be > 0 in [%s] file.",
                       full_path_file);
      exit(-1);
    }

    if(idx >= total_full_param_path_entries ) {
      fprintf(stderr, "Got more than (%d) entries in file [%s].",
                       idx, full_path_file);
      exit(-1);
    }

    ptr = index(read_line, '=');

    if(ptr) {
     *ptr = 0;
    } else {
      fprintf(stderr, "Error: File [%s] at line (%d) has invalid format of data.\n",
                       full_path_file, line_number);
      exit(-1);
    }

    NSDL2_TR069(NULL, NULL, "read_line = [%s]", read_line);
    index_to_param_path_table = atoi(read_line); 

    if(index_to_param_path_table < 0) {
      fprintf(stderr, "Error: File [%s] at line (%d) has invalid value [%s]\n",
                       full_path_file, line_number, read_line);
      exit(-1);
    }

    tr069_full_param_path_table[idx].path_table_index = index_to_param_path_table; 
    tr069_param_path_table[index_to_param_path_table].path_type_flags |= NS_TR069_PARAM_FULL_PATH;

    idx++;
  }

  if(total_full_param_path_entries <= 0 ) {
    fprintf(stderr, "total_full_param_path_entries must be > 0 in [%s] file.",
                     full_path_file);
    exit(-1);
  }
  fclose(fp);
}

#if 0
static void create_schema_path_file(char *file_name) {
  char cmd[10 * 1024];
  FILE *cmd_fp;
    
  NSDL2_TR069(NULL, NULL, "Method called, file_name = %s", file_name);
    
  sprintf(cmd, "cp %s %s/%s/schema_path.txt", file_name, g_ns_wdir, g_ns_tmpdir);
    
  NSDL2_TR069(NULL, NULL, "cmd = [%s]", cmd);

  if ((cmd_fp = popen(cmd, "r")) == NULL) {
        fprintf(stderr, "%s: error in calling popen for %s\n", (char*)__FUNCTION__, cmd);
        exit (-1);
  }
   
  pclose(cmd_fp);
  NSDL2_TR069(NULL, NULL, "cmd = [%s] DONE", cmd);
}
#endif
      

#if 0
/**
 * Purpose         : This method create hash code table from unique event id's
 * Arguments       : None
 * Return Value    : None
 * **/
static void tr069_create_path_hash_table(char *schema_path_file) {

  int num_hash;

  char tr069_params_paths[NS_TR069_PATH_LEN + 1];

  create_schema_path_file(schema_path_file);
  sprintf(tr069_params_paths, "%s/%s", g_ns_wdir, g_ns_tmpdir);

  NSDL2_TR069(NULL, NULL, "Method Called, schema_path_file = [%s], tr069_params_paths = [%s]",
                           schema_path_file, tr069_params_paths);

  // Create a uniq name if more schemas to be load
  //sprintf(tr069_params_paths, "tr069_params_paths.txt");

  num_hash = generate_hash_table_ex("schema_path.txt", "tr069_params_paths_fn",
                                    &tr069_params_path_hash_func,
                                    &tr069_params_path_get_key, 0, tr069_params_paths);
}
#endif

static void tr069_create_shared_object(char *schema_name) {

  FILE *cmd_fp = NULL;
  char tr069_hash_lib_c[NS_TR069_PATH_LEN + 1];
  char tr069_hash_lib_so[NS_TR069_PATH_LEN + 1];
  char cmd[NS_TR069_PATH_LEN + 1];
  void* handle;
  char *error;

  NSDL2_TR069(NULL, NULL, "Method Called");

  sprintf(tr069_hash_lib_c, "%s/tr069_hash_code.c", global_settings->tr069_data_dir);
  sprintf(tr069_hash_lib_so, "%s/%s/tr069_hash_code_%s.so", g_ns_wdir, g_ns_tmpdir, schema_name);

  /* compile and link hash_page_write.c */
  sprintf(cmd, "gcc -g -m%d -fpic -shared -o %s %s",
                       NS_BUILD_BITS, tr069_hash_lib_so, tr069_hash_lib_c); 

  if ((cmd_fp = popen(cmd, "r")) == NULL) {
     fprintf(stderr, "Error: Unable to fork command [%s] to create shared object for tr069_schema.\n", cmd);
     exit (-1);
  }

  if(pclose(cmd_fp)) {
     fprintf(stderr, "Error: Unable to run command [%s] to create shared object for tr069_schema.\n", cmd);
     exit (-1);
  }

  // Open shared object
  handle = dlopen (tr069_hash_lib_so, RTLD_LAZY);
  if ((error = dlerror())) {
    fprintf(stderr, "Error: Unable open sharde opject for [%s], Error = %s.\n",
                     tr069_hash_lib_so, error);
    exit (-1);
  }

  tr069_path_name_to_index     = dlsym(handle, "StringToIndex");
  if ((error = dlerror())) {
    fprintf(stderr, "Error: Unable get function (StringToIndex) from [%s], Error = %s.\n",
                     tr069_hash_lib_so, error);
    exit (-1);
  }

  tr069_hash_code_to_path_name = dlsym(handle, "HashToString");
  if ((error = dlerror())) {
    fprintf(stderr, "Error: Unable get function (HashToString) from [%s], Error = %s.\n",
                     tr069_hash_lib_so, error);
    exit (-1);
  }

  tr069_hash_code_to_index     = dlsym(handle, "HashToIndex");
  if ((error = dlerror())) {
    fprintf(stderr, "Error: Unable get function (HashToIndex) from [%s], Error = %s.\n",
                     tr069_hash_lib_so, error);
    exit (-1);
  }

  tr069_index_to_hash_code     = dlsym(handle, "IndexToHash");
  if ((error = dlerror())) {
    fprintf(stderr, "Error: Unable get function (IndexToHash) from [%s], Error = %s.\n",
                     tr069_hash_lib_so, error);
    exit (-1);
  }

  tr069_index_to_path_name     = dlsym(handle, "IndexToString");
  if ((error = dlerror())) {
    fprintf(stderr, "Error: Unable get function (IndexToString) from [%s], Error = %s.\n",
                     tr069_hash_lib_so, error);
    exit (-1);
  }
}

void tr069_parent_init() {

  u_ns_ts_t now = get_ms_stamp();
  char sch_type_file_path[2048];
  char sch_full_file_path[2048];
  NSDL2_MISC(NULL, NULL, "Method called");

  if(!(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED)) { 
    return;
  }

  tr069_create_shared_object("netstorm");

  sprintf(sch_type_file_path, "%s/%s", global_settings->tr069_data_dir, NS_TR069_SCHEMA_TYPE_FILE);
  // Read hash codes from tr069_schema_path.dat file
  tr069_create_params_path_table(sch_type_file_path);
 
  sprintf(sch_full_file_path, "%s/%s", global_settings->tr069_data_dir, NS_TR069_FULL_PATH_FILE);
  tr069_create_params_full_path_table(sch_full_file_path);

#if NS_DEBUG_ON
  dump_parent_path_table();
#endif
  tr069_arrange_path_tables();

  tr069_init_reusable_indexes();

  fprintf(stderr, "TR069Info: Parent init time taken = [%llu]\n", (get_ms_stamp() - now));
}
