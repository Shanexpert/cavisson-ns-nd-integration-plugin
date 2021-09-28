/*18/OCT/2013
Thread Mode API were added for some API's that run in user context. Currently we handled the API which uses current VUser pointer.
Thread mode API get the current VUser pointer from the threadspecific data.
We initialize the structure "Msg_com_con" for each thread. This structure hold information of current VUser
and some other information. Pointer to Msg_com_con is stored in buffer_key (key created using pthread_key_create API).
Msg_com_con structure for thread is accessed using pthread_getspecific API.

Also for Thread Mode API, static buffer "string_buffer" is placed in VUser structure. This buffer is allocated on start of session
and are freed on session exit.
typedef struct {
  char* string_buffer; // Buffer used in eval string API
  int string_buf_size; //  size of above buffer
} VUserThdData;

With this implementation we have _internal static function (example "ns_eval_string_flag_internal") for an API, which contains functional implementation. This internal function
has two wrapper function accesible to user
1.Non thread mode (example ns_eval_string_flag)
*/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <regex.h>
#include <pwd.h>
#include <time.h>
#include <stdint.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h> 

#include "url.h"
#include "ns_tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_compression.h"
#include "init_cav.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "comp_decomp/nslb_comp_decomp.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_cookie.h"
#include <curl/curl.h>
#include "decomp.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_user_monitor.h"
#include "ns_gdf.h"
#include "ns_alloc.h" 
#include "ns_event_log.h"
#include "ns_trans.h"
#include "nslb_time_stamp.h"
#include "ipmgmt_utils.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "nslb_sock.h"
#include "logging.h"
#include "ns_trans.h"
#include "ns_page_think_time.h"
#include "ns_replay_access_logs.h"
#include "ns_index_vars.h"
#include "ns_string.h"
#include "ns_random_vars.h"
#include "ns_event_id.h"
#include "ns_embd_objects.h"
#include "ns_auto_fetch_embd.h"
#include "ns_vuser_tasks.h"
#include "ns_vuser_ctx.h"
#include "tr069/src/ns_tr069_acs_rpc.h"
#include "tr069/src/ns_tr069_cpe_rpc_object.h"
#include "tr069/src/ns_tr069_cpe_rpc_others.h"
#include "tr069/src/ns_tr069_cpe_rpc_param.h"
#include "tr069/src/ns_tr069_acs_con.h"
#include "dos_attack/ns_dos_syn_attack.h"
#include "ns_child_thread_util.h"
#include "nslb_encode_decode.h"
#include "ns_vuser_thread.h"
#include "ns_server_mapping.h"
#include "ns_session.h"
#include <openssl/md5.h>
#include "ns_page_dump.h"
#include "ns_sync_point.h"
#include "nslb_comman_api.h"
#include "ns_user_profile.h"
#include "nslb_search_vars.h"
#include "ns_rbu_api.h"
#include "ns_rte_api.h"
#include "ns_user_define_headers.h"
#include "ns_websocket.h"
#include "nslb_encode.h"
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/sha.h>
#include "nslb_uservar_table.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_url_resp.h"
#include "ns_debug_trace.h"
#include "ns_vars.h"
#include "ns_page.h"
#include "ns_date_vars.h"
#include "ns_random_string.h"
#include "wait_forever.h"
#include "ns_replay_db_query.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_unique_numbers.h"
#include "nslb_comman_api.h"
#include "ns_rbu_page_stat.h"
#include "ns_static_vars_rtc.h"
#include "ns_trace_level.h"
#include "ns_websocket_reporting.h"
#include "ns_group_data.h"
#include "ns_exit.h"
#include "nslb_passwd.h"
#include "ns_vuser_tasks.h"
#include "ns_parent.h"
#include "protobuf/nslb_protobuf_adapter.h"
#include "nslb_alert.h"
#include "ns_handle_alert.h"
#include "nslb_cav_conf.h"
#include "ns_socket.h"
#include "ns_sock_com.h"

#define SET_MIN(a , b)                          \
  if ( (b) < (a)) (a) = (b) 

#define SET_MAX(a , b)                          \
  if ( (b) > (a)) (a) = (b)

#define ENCODE_STR(ptr, len, encode_flag) \
  if(ptr != NULL && len != 0) \
  { \
    if(encode_flag) \
    { \
      ptr = ns_encode_url(ptr, len); \
      len = strlen(ptr); \
      memcpy(buf_ptr, ptr, len); \
      ns_encode_decode_url_free(ptr); \
    } \
    else \
      memcpy(buf_ptr, ptr, len); \
    buf_ptr += len; \
    total_copied += len; \
  }

/* Macro_Name: GET_VPTR
   Purpose   : this macro will get vptr pointer for thread as well as context switch mode. 
   Input Args: vptr - this will be used to store vptr pointer 
               X    - this will contain return value for failure case. 
                      From some place this macro function return with int 
                      on the other hand from some other places it return with NULL/char* 
   Design    : ....
*/
/*#define GET_VPTR(vptr, X) \
{ \
  if(buffer_key == 0xFFFFFFFF) \
    vptr = vptr; \
  else \
  { \
    Msg_com_con *tmp_nvm_info_tmp; \
    tmp_nvm_info_tmp = (Msg_com_con *)pthread_getspecific( buffer_key ); \
    if(tmp_nvm_info_tmp == NULL) \
    { \
      NSTL1(NULL, NULL, "tmp_nvm_info_tmp is getting NULL, hence returning"); \
      return X; \
    } \
    vptr = tmp_nvm_info_tmp->vptr; \
  } \
}*/

//static char *severity_str[]={"Clear", "Info", "Warning", "Minor", "Major", "Critical"};

extern connection *cur_cptr;
unsigned char my_port_index = 255; /* will remain -1 for parent */
unsigned char my_child_index = 255; /* will remain -1 for parent */
#ifndef CAV_MAIN
Global_data *global_settings = NULL;
VarTableEntry_Shr* variable_table_shr_mem;
extern RunProfTableEntry_Shr *runprof_table_shr_mem;
#else
__thread Global_data *global_settings = NULL;
__thread VarTableEntry_Shr* variable_table_shr_mem;
extern __thread RunProfTableEntry_Shr *runprof_table_shr_mem;
#endif
ChildGlobalData child_global_data;
extern int get_schedule_phase_type_int(VUser *vptr);
extern char *ns_get_schedule_phase_name_int(VUser *vptr);
extern void insert_into_map(char *, char*);
extern short gRunPhase;
extern int msg_num;
int use_geoip_db=0;

static __thread char* string_buffer;
static __thread int string_buf_size;

char *ns_get_schedule_phase_name()
{
  VUser *vptr = TLS_GET_VPTR();

  
  NSDL2_API(vptr, NULL, "Method called.");

  return ns_get_schedule_phase_name_int(vptr);
}


int ns_get_schedule_phase_type()
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");

  return get_schedule_phase_type_int(vptr);
}

int string_init()
{
  //IW_UNUSED(VUser *vptr = TLS_GET_VPTR()); 
  NSDL2_API(NULL, NULL, "Method called");
  MY_MALLOC(string_buffer, sizeof(char)*1024, "string_buffer", 1);
  if (!string_buffer) {
    string_buf_size = 0;
    return -1;
  } else {
    string_buf_size = 1024;
    return 0;
  }
}

char *ns_get_guid()
{
  static __thread unsigned int guid_idx=0;
  static __thread char g_guid_buf[32];

  NSDL2_API(NULL, NULL, "Method called");
  snprintf(g_guid_buf, 31, "%0X-%0X-%0X", (unsigned)time(NULL), (unsigned)getpid(), (unsigned)guid_idx++);
  g_guid_buf[31] = '\0';
  
  return (g_guid_buf);
}
char *
ns_get_cookie_val (int cookie_idx)
{
  NSDL2_COOKIES(NULL, NULL, "Method called. cookie_idx = %d", cookie_idx);
  VUser *vptr = TLS_GET_VPTR();
  if(global_settings->g_auto_cookie_mode != AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_get_cookie_val() cannot be used in Auto Cookie Mode\n");
    return NULL;
  }
  
  NSDL2_API(NULL, NULL, "Method called");
  return (ns_get_cookie_val_non_auto_mode(cookie_idx, vptr));
}

char *
ns_get_cookie_val_ex (char *cookie_name, char *domain, char *path)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s", cookie_name, domain, path);

  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_get_cookie_val() cannot be used if Auto Cookie Mode is disabled\n");
    return NULL;
  }
  return (ns_get_cookie_val_auto_mode(cookie_name, domain, path, vptr));
}

int ns_set_cookie_val (int cookie_idx, char *cookie_val)
{
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_COOKIES(NULL, NULL, "Method called. cookie_idx = %d, cookie_val = %s", cookie_idx, cookie_val);
  if(global_settings->g_auto_cookie_mode != AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_set_cookie_val() cannot be used in Auto Cookie Mode\n");
    return -1;
  }

  return (ns_set_cookie_val_non_auto_mode(cookie_idx, cookie_val, vptr));
}

static int
ns_set_cookie_val_ex_internal (char *cookie_name, char *domain, char *path, char *cookie_val, VUser *my_vptr)
{
  NSDL2_COOKIES(my_vptr, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s, Cookie Value = %s", cookie_name, domain, path, cookie_val);
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_set_cookie_val() cannot be used if Auto Cookie Mode is disabled\n");
    return -1;
  }
  return (ns_set_cookie_val_auto_mode(cookie_name, domain, path, cookie_val, my_vptr));
}

int ns_set_cookie_val_ex (char *cookie_name, char *domain, char *path, char *cookie_val)
{
  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s, Cookie Value = %s", cookie_name, domain, path, cookie_val);
  VUser *vptr = TLS_GET_VPTR();

  return ns_set_cookie_val_ex_internal (cookie_name, domain, path, cookie_val, vptr);
}

int ns_get_cookies_disallowed ()
{
  NSDL2_COOKIES(NULL, NULL, "Method called");
  
  VUser *vptr = TLS_GET_VPTR();
  
  
  NSDL2_API(vptr, NULL, "Method called.");

  return ((vptr->flags)& NS_COOKIES_DISALLOWED);
}

char *ns_get_all_cookies(char *cookie_buf, int cookie_max_buf_len) {

  NSDL2_COOKIES(NULL, NULL, "Method called");
  
  VUser *vptr = TLS_GET_VPTR();

  
  NSDL2_API(vptr, NULL, "Method called.");


  return ((char*)get_all_cookies(vptr, cookie_buf, cookie_max_buf_len));
}

int ns_set_cookies_disallowed (int val)
{
  NSDL2_COOKIES(NULL, NULL, "Method called. value = %d", val);

  VUser *vptr = TLS_GET_VPTR();

  
  NSDL2_API(vptr, NULL, "Method called.");

  if (val)
    vptr->flags |= NS_COOKIES_DISALLOWED;
  else
    vptr->flags &= ~NS_COOKIES_DISALLOWED;

  return 0;
}

int ns_get_auto_cookie_mode()
{
  NSDL2_COOKIES(NULL, NULL, "Method called.");
  return (ns_get_cookie_mode_auto());
}

int ns_get_ua_string (char *ua_input_buffer, int ua_input_buffer_len)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  
  int ret_buffer_size;
  ret_buffer_size = ns_get_ua_string_ext (ua_input_buffer, ua_input_buffer_len, vptr); 

  NSDL2_API(vptr, NULL, "UA string filled by API %s length = %d", ua_input_buffer, ret_buffer_size);
  return(ret_buffer_size);
}

int
ns_set_ua_string (char *ua_static_ptr, int ua_static_ptr_len)
{ 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL4_API(vptr, NULL, "Method called, ua_static_ptr_len = %d", ua_static_ptr_len);
  
  ns_set_ua_string_ext (ua_static_ptr, ua_static_ptr_len, vptr);
  return 0;
}

int
ns_set_ip_address (char * ip_char)
{
  int ret;

  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(NULL, NULL, "Method called");

#if 0
      if ((ip_addr = inet_addr(ip_char)) < 0) {
	fprintf(stderr,"ns_set_ip_address(): Invalid address, ignoring <%s>\n", ip_char);
	return -1;
      } else {
	vptr->user_ip->ip_addr.s_addr = ip_addr;
	return 0;
      }
#endif

	switch (vptr->user_ip->ip_addr.sin6_family) {
	    case AF_INET: {
		struct sockaddr_in	*sin = (struct sockaddr_in *) &vptr->user_ip->ip_addr;
		ret = inet_pton (AF_INET, ip_char, &(sin->sin_addr));
		break;
	    }
	    case AF_INET6: {
		struct sockaddr_in6	*sin = &vptr->user_ip->ip_addr;
		ret = inet_pton (AF_INET6, ip_char, &(sin->sin6_addr));
		break;
	    }
	    default:  {
		fprintf(stderr,"ns_set_ip_address(): Invalid address Family, ignoring <%s>\n", ip_char);
		return -1;
	    }
	}

	if (ret > 0) {
	    return 0;
	} else {
	    fprintf(stderr,"ns_set_ip_address(): Invalid address Family, ignoring <%s>\n", ip_char);
	    return -1;
	}
}


/******************************************************************************************
*This function is used to check available shared memory in the system to execute test     *
******************************************************************************************/
void check_shared_mem(long int size)
{
  
  NSDL2_API(NULL, NULL, "Method called, size = %ld", size);
  unsigned long total_shm = 0;
  unsigned long total_existing_shm = 0;
  unsigned long total_avail_shm = 0;
  int ret = 0;
  unsigned long shm_pg = 0;
  int page_size = getpagesize(); 
  struct shm_info shmbuffer;
  struct shminfo shmbuff;

  if ((ret = shmctl(0, SHM_INFO, (struct shmid_ds*)&shmbuffer)) == -1)
  {
    NSTL1(NULL, NULL, "ERROR: SHMCTL failed : %s", nslb_strerror(errno)); 
    return;
  }

  shm_pg  = shmbuffer.shm_tot;
  
  NSDL2_API(NULL, NULL, "segment_size = %d, shm_pg = %d, page_size = %d",  shmbuffer.used_ids, shm_pg, page_size);
  
  if ((ret = shmctl(0, IPC_INFO, (struct shmid_ds*)&shmbuff)) == -1)
  {
    NSTL1(NULL, NULL, "ERROR: SHMCTL failed  %s", nslb_strerror(errno)); 
    return;
  }

  total_shm = (shmbuff.shmall * page_size) /1024;
  total_existing_shm = ( shm_pg * page_size) / 1024;
  total_avail_shm = total_shm - total_existing_shm; 
  
  NSDL2_API(NULL, NULL, "size = %lu,  total_shm = %lu,  total_existing_shm = %lu,  total_avail_shm = %lu", 
							size, total_shm, total_existing_shm, total_avail_shm);

  if(total_shm < total_avail_shm)
     fprintf(stderr,"ERROR: unable to allocate shm for size=%ld, total shared memory = %lu,"
                                   "used shared memory = %lu , available shared memory = %lu", 
                                        size, total_shm, total_existing_shm, total_avail_shm); 
  return;
}


void *
//do_shmget(key_t key, int size, int shmflg)
do_shmget_with_id(long int size, char *msg, int *shmid)
{
  NSDL2_API(NULL, NULL, "Method called, size = %ld, msg = %s, *shmid = %d", size, msg, *shmid);
  #ifndef CAV_MAIN
  return do_shmget_with_id_ex(size, msg, shmid, AUTO_DEL_SHM);
  #else
  void *addr;
  MY_MALLOC_AND_MEMSET(addr, size, msg, -1);
  *shmid = 0;
  return addr;
  #endif
}

void *do_shmget_with_id_ex(long int size, char *msg, int *shmid, int auto_del)
{
  void *addr;

  NSDL2_API(NULL, NULL, "Method called, size = %ld, msg = %s, *shmid = %d, auto_del = %d", size, msg, *shmid, auto_del);

  if (size == 0)
    return NULL;

  g_shared_mem_alloc_count++;
  g_c_shared_mem_alloc_count++;

  g_shared_mem_allocated += size;
  g_c_shared_mem_allocated += size;

  NSTL1(NULL, NULL, "Allocating shared memory for '%s' of size %ld, ", msg, size);

  *shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | 0666);
  
  if (*shmid == -1) {
    check_shared_mem(size);
    NS_EXIT (-1, "ERROR: unable to allocate shm for '%s' of size=%ld err=%s\n", msg, size, nslb_strerror(errno));
  }

  addr = shmat(*shmid, NULL, 0);
  if (addr == (void *) -1) {
    NS_EXIT (-1, "ERROR: unable to attach shm for '%s' of size=%ld err=%s\n", msg, size, nslb_strerror(errno));
  }

  //Mark shm for auto-deletion on exit
  if(auto_del)
  {
    if(shmctl (*shmid, IPC_RMID, NULL)) {
      NS_EXIT (-1, "ERROR: unable to mark shm removal for '%s' of size=%ld err=%s\n", msg, size, nslb_strerror(errno));
    }
  }

  NSTL1(NULL, NULL, "Successfully allocated shared memory for '%s' of size %ld. "
                    "Total no. of shared memory sgements = %lld, Total shared memory size = %lldB", 
                     msg, size, g_c_shared_mem_alloc_count, g_c_shared_mem_allocated);

  return addr;
}

void *do_shmget(long int size, char *msg) 
{
  NSDL2_API(NULL, NULL, "Method called");
  int shmid;
  void *addr = do_shmget_with_id(size, msg, &shmid);

  return addr;
}

static inline int increase_string_buf(VUser *api_vptr) {
  
  NSDL2_API(api_vptr, NULL, "Method called");
  if(api_vptr->thdd_ptr)
  {
    MY_REALLOC_EX(api_vptr->thdd_ptr->eval_buf,api_vptr->thdd_ptr->eval_buf_size + 1024, api_vptr->thdd_ptr->eval_buf_size, "api_vptr->thdd_ptr->eval_buf", -1);
    
    api_vptr->thdd_ptr->eval_buf_size += 1024;
  } else {
    MY_REALLOC_EX(string_buffer, string_buf_size + 1024, string_buf_size, "string_buffer", -1);
    string_buf_size += 1024;
  } 
  return 0;
}


static inline char* check_string_buf(int total_copied, int copy_length, char* buf_ptr,VUser *api_vptr) {
  char* new_buf_ptr = buf_ptr;
  NSDL2_API(api_vptr, NULL, "total_copied = %d, copy_length = %d, buf_ptr = %s, string_buf_size = %d", 
                             total_copied, copy_length, buf_ptr, string_buf_size);
  if(api_vptr->thdd_ptr)
  {
    while ((total_copied + copy_length + 1) >= api_vptr->thdd_ptr->eval_buf_size) {
      increase_string_buf(api_vptr); //This function never fails. If fails then test will be stoped
      new_buf_ptr = api_vptr->thdd_ptr->eval_buf + total_copied;
    }
  }
  else
  {
    while ((total_copied + copy_length + 1) >= string_buf_size) {
      increase_string_buf(api_vptr); //This function never fails. If fails then test will be stoped
      new_buf_ptr = string_buffer + total_copied;
    }
  }
  return new_buf_ptr;
}

static inline int valid_array_idx(char* index_val) 
{
  char* ptr;

  NSDL2_API(NULL, NULL, "Method called");
  if (!strcmp(index_val, "count"))
    return 1;
  else {
    for (ptr = index_val; (*ptr)!='\0'; ptr++) {
      if (!isdigit(*ptr))
	return -1;
    }
    return 0;
  }

  return -1;
}

/*
Purpose: This function checks if variable is array or not
Returns:
-1 : Not a variable or not a array variable or not of any of three types of variable (NSL, TAG, SEARCH)
     or variable has invalid index e.g address_23a
hash code of variable : If above conditions are not true. In this case,
   *array_idx is set to -1; if variable is for count (e.g. address_count)
   *array_idx is set to 1 to N; if array variable (e.g. address_1)
*/
inline int is_array_variable(char* var_start, char* var_end, int var_length, int* array_idx) 
{
  char* underscore_ptr;
  char* last_unsc_ptr;
  int var_hashcode;
  int index_length;
  char index_val[16];
  int is_length;

  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method called");
  underscore_ptr = last_unsc_ptr = var_start;
  do {
    underscore_ptr = memchr(underscore_ptr, '_', var_end - underscore_ptr);
    if (underscore_ptr) {
      last_unsc_ptr = underscore_ptr;
      underscore_ptr++;
      if (underscore_ptr >= var_end)
	break;
    }
  } while (underscore_ptr);

  var_length = last_unsc_ptr-var_start;

  if ((var_hashcode = vptr->sess_ptr->var_hash_func(var_start, var_length)) != -1) {
    int uservar_idx = vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].user_var_table_idx;
    int var_type = vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].var_type;
    switch (var_type) {
    case NSL_VAR:
    case TAG_VAR:
    case SEARCH_VAR:
    case JSON_VAR:
    if (vptr->sess_ptr->var_type_table_shr_mem[uservar_idx]) { /* make sure its an array variable */
      if (last_unsc_ptr != var_start) {
	index_length = var_end - last_unsc_ptr - 1;
	if (index_length) {
	  memcpy(index_val, last_unsc_ptr+1, index_length);
	  index_val[index_length] = '\0';

	  NSDL3_API(vptr, NULL, "index_val = %s", index_val);

	  if ((is_length = valid_array_idx(index_val)) == 1) {
	    (*array_idx) = -1;
	    return var_hashcode;
	  } else if (is_length == 0) {
	    (*array_idx) = atoi(index_val);
	    return var_hashcode;
	  }
	}
      }
    }
    }
  }
  return -1;
}
// Nikita: Bug fixed 3230
char *ns_encode_url(const char *string, int inlength)
{
  int lol_inlength; 
  lol_inlength = inlength;
  if(lol_inlength < 0)
  {
    fprintf(stderr, "Warning(in api ns_encode_url): Length should be zero or any number that is greater then zero. Given length is %d, making length to zero.\n", lol_inlength);
    lol_inlength = 0;
  }
  return(curl_escape(string, lol_inlength));
}

char *ns_decode_url(const char *string, int length)
{
  int lol_length;
  lol_length = length;
  if(lol_length < 0)
  {
    fprintf(stderr, "Warning(in api ns_decode_url): Length should be zero or any number that is greater then zero. Given length is %d, making length to zero.\n", lol_length);
    lol_length = 0;
  }

  return(curl_unescape(string, lol_length));
}

void ns_encode_decode_url_free(char *ptr) 
{
  curl_free(ptr);
}

//This api will decode html --> text; eg: &lt; into <, &gt; into > ...... 
char *ns_decode_html(char *in_str, int in_str_len, char *out_str)
{ 
  NSDL2_API(NULL, NULL, "in_str = [%s], len = [%d], out_str = [%p]", in_str, in_str_len, out_str);
  return (nslb_decode_html(in_str, in_str_len, out_str));
}


char *ns_encode_html(char *in_str, int in_str_len, char *out_str)
{
  NSDL2_API(NULL, NULL, "in_str = [%s], len = [%d], out_str = [%p]", in_str, in_str_len, out_str);
  return (nslb_encode_html(in_str, in_str_len, out_str));
}
/*
char *ns_encode_base64(char *in_str, int in_str_len, char *out_str)
{
  NSDL2_API(vptr, NULL, "in_str = [%s], len = [%d], out_str = [%p]", in_str, in_str_len, out_str);
  return (nslb_encode_base64(in_str, in_str_len, out_str));
}


char *ns_decode_base64(char *in_str, int in_str_len, char *out_str)
{
  NSDL2_API(vptr, NULL, "in_str = [%s], len = [%d], out_str = [%p]", in_str, in_str_len, out_str);
  return (nslb_decode_base64(in_str, in_str_len, out_str));
}
*/

// This API will take parameter name, buffer in which it will copy the parameter value, and buffer's length
// Case: If parameter is scalar then value of parameter will be copied to buffer and legnth will be set into size  
char* ns_get_param_val_flag(char* string, char *buf, int *size)
{
  VarTransTableEntry_Shr* var_ptr;
  char* find_ptr;
  char* var_end;
  int var_hashcode;
  int var_length;
  char* var_start;
  char* ptr = buf;
  int uservar_idx;
  int i;
  int remain_len          = *size;
  int flag                = 0;
  int copy_len            = 0;
  int total_len           = 0;
  int tmp_len             = 0;
  char *temp_ptr          = NULL;
  char *tmp_val_ptr       = NULL;
  char* string_ptr        = string;

  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method called. string = %s", string);

  while ((find_ptr = strchr(string_ptr, '{')) != NULL)
  {
    NSDL2_API(vptr, NULL, "find_ptr = %s, string_ptr = %s", find_ptr, string_ptr);
    
    string_ptr = find_ptr + 1;
    var_end = strchr(find_ptr, '}');

     if (!var_end)
    {
      var_hashcode = -1;
      var_length = strlen(find_ptr);
      NSDL3_API(vptr, NULL, "End of variable not found, so it is string. String Length = %d, String = %*.*s",                                                                                                                      var_length, var_length, var_length, find_ptr);
    }
    else
    {
      var_start    = find_ptr + 1;
      var_length   = var_end - var_start;
      var_hashcode = vptr->sess_ptr->var_hash_func(var_start, var_length);

      NSDL3_API(vptr, NULL, "Found end of variable. var_hashcode = %d, var_length = %d, var name = %*.*s",                                                                                                           var_hashcode, var_length, var_length, var_length, var_start);

  
      if (var_hashcode != -1)
      {
        uservar_idx = vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].user_var_table_idx;
        var_ptr = &vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode];
      }
  
    if((var_ptr->var_type == SEARCH_VAR && vptr->uvtable[uservar_idx].flags == VAR_IS_VECTOR && var_hashcode != -1))
      {
          ArrayValEntry* arrayval_ptr;
          arrayval_ptr = vptr->uvtable[uservar_idx].value.array;
         
          temp_ptr = ptr;

          for(i = 0; i< vptr->uvtable[uservar_idx].length; i++)
          {
            if(vptr->uvtable[uservar_idx].value.array[i].length != 0)
            {
              remain_len = remain_len - (vptr->uvtable[uservar_idx].value.array[i].length + 1);
              if(!flag)
              {
                if(remain_len >= 0)
                {              
                  sprintf(temp_ptr,"%s,",arrayval_ptr[i].value);
                  copy_len = copy_len + vptr->uvtable[uservar_idx].value.array[i].length + 1;
                  temp_ptr = temp_ptr + arrayval_ptr[i].length + 1;
                }
                else
                 flag = 1; 
              }
              total_len = total_len + vptr->uvtable[uservar_idx].value.array[i].length + 1;
           }
         }
        ptr[copy_len - 1] = '\0'; 
        }
      else
      {
        tmp_val_ptr = ns_eval_string(string);
        tmp_len = strlen(tmp_val_ptr);
        if(tmp_len <= remain_len)
        {
          strcpy(ptr, tmp_val_ptr);
          copy_len = tmp_len;
        }
        else
        {
          total_len = tmp_len;
          flag = 1; 
        } 
      }
    } 
  }
    NSDL3_API(vptr, NULL, "total_len = %d\n, remain_len= %d\n, copy_len = %d\n", total_len, remain_len, copy_len);

    if(flag == 1)
      *size = total_len;
    else
      *size = copy_len;

    return ptr;
}

char* ns_encode_specific_eval_string(char* string, int encode_flag_specific , char* specific_chars, char* EncodeSpaceBy)
{
  char buf[128];
  char* str;
  char char_to_encode_buf[128];
  int i;
  int length;
  int need_to_rem_newline=0;
  int out_len;
  int asc;

  if (EncodeSpaceBy == NULL && specific_chars == NULL)
  {
    return ns_encode_eval_string(string);
  }
  else if (specific_chars == NULL)
  { 
    return NULL;  
  }

  if (EncodeSpaceBy == NULL) {
    EncodeSpaceBy = "+";
  }
  NSDL3_VARS(NULL, NULL, "EncodeSpaceBy = [%s]", EncodeSpaceBy);
  str = ns_eval_string(string);
  length = strlen(str);
  out_len = length + 1;
  NSDL3_VARS(NULL, NULL, "string aftr eval is = [%s]", str);
  strcpy(char_to_encode_buf, specific_chars);
  memset(buf, 0, 128);
  for (i = 0; char_to_encode_buf[i] != '\0'; i++) {
    if(isalnum(char_to_encode_buf[i])){
      fprintf(stderr, "%s Bad CharstoEncode option . Only special characters are allowed\n", char_to_encode_buf);
      return NULL;
    }

  NSDL3_VARS(NULL, NULL, "i = %d, char_to_encode_buf[i] = [%c]", i, char_to_encode_buf[i]);
  asc = (int)char_to_encode_buf[i];
  buf[asc] = 1;
  NSDL3_VARS(NULL, NULL, "i = %d, asc = %d, buf[%d] = [%d]", i, asc,asc, buf[asc]);
  }
  for (i=0; i<128;i++)
  {
    NSDL3_VARS(NULL, NULL, "i = %d, buf[%d] = [%d]", i,i, buf[i]);
  }

  return ns_escape_ex(str, length, &out_len, buf , EncodeSpaceBy, NULL, need_to_rem_newline);
}
 
/************************************************/
//If flag = 1, then eval string will be encode the value of variable only. else eval string pass as is
//static char* ns_eval_string_flag(char* string, int encode_flag) 
//changed below to pass back length for binary strings and  removed static
char* ns_eval_string_flag_internal(char* string, int encode_flag, long *size, VUser *api_vptr) 
{
  char *buf_ptr = NULL;
  char* string_ptr = string;
  int total_copied = 0;
  int copy_length;
  char* find_ptr;
  char* var_end;
  int var_hashcode;
  int var_length;
  char* var_start;
  VarTransTableEntry_Shr* var_ptr;
  UserVarEntry* value_ptr;
  PointerTableEntry_Shr* ptrtbl_ptr;
  UserCookieEntry* cookie_val_ptr;
  ClustValTableEntry_Shr* clust_val;
  GroupValTableEntry_Shr* group_val;
  //VarTableEntry_Shr* fparam_var;
  //char* underscore_ptr;
  //char* last_unsc_ptr;
  //int index_length;
  //char index_val[16];
  //int is_length;
  int array_idx; // -1 if it is for the length
  int is_array;
  char *ptr;
  int len;
  //int fparam_grp_id = -1;

   //Resolve bug 6808
   //As in thread  mode function ns_rbu_on_session_start() is called before memory allocation of thdd_ptr, so core is coming here. 
   //To solve this add a condition of checking value of thdd_ptr before use this . This check is done in all places of this function where thdd_ptr is used .
  if(api_vptr->thdd_ptr)
  {
    buf_ptr = api_vptr->thdd_ptr->eval_buf;
  }
  else
  {
    buf_ptr = string_buffer;
  }

  NSDL2_API(api_vptr, NULL, "Method called. string = %s, encode_flag = %d", string, encode_flag); 
  while ((find_ptr = strchr(string_ptr, '{'))) 
  {
    NSDL2_API(api_vptr, NULL, "find_ptr = %s, string_ptr = %s", find_ptr, string_ptr);
    // Upto "{" is the string part. So copy as is
    copy_length = find_ptr - string_ptr;
    buf_ptr = check_string_buf(total_copied, copy_length, buf_ptr,api_vptr);
    NSDL3_API(api_vptr, NULL, "Found string part. copy_length = %d, total_copied = %d, String = %*.*s", copy_length, total_copied, copy_length, copy_length, string_ptr);
    memcpy(buf_ptr, string_ptr, copy_length);
    buf_ptr += copy_length;
    total_copied += copy_length;

    NSDL3_API(api_vptr, NULL, "Found start of variable. copy_length = %d, total_copied = %d buf_ptr = %s ", copy_length, total_copied, buf_ptr);
    var_end = strchr(find_ptr, '}');
    if (!var_end) 
    {
      var_hashcode = -1;
      var_length = strlen(find_ptr);
      NSDL3_API(api_vptr, NULL, "End of variable not found, so it is string. String Length = %d, String = %*.*s", var_length, var_length, var_length, find_ptr);
    } 
    else 
    {
      var_start = find_ptr+1;
      var_length = var_end - var_start;

      var_hashcode =api_vptr->sess_ptr->var_hash_func(var_start, var_length);
      NSDL3_API(api_vptr, NULL, "Found end of variable. var_hashcode = %d, var_length = %d, var name = %*.*s", var_hashcode, var_length, var_length, var_length, var_start);

      is_array = 0;
      //Changes made after valgrind reported bug
      if (var_hashcode == -1) 
      {  /* gotta check if it is an array variable */
        NSDL3_API(api_vptr, NULL, "Hash code of variable not found, checking if it is array type variable");

      	var_hashcode = is_array_variable(var_start, var_end, var_length, &array_idx);
        if (var_hashcode != -1)
        {
          NSDL3_API(api_vptr, NULL, "Variable is of array type, var_hashcode = %d, var name = %*.*s", var_hashcode, var_length, var_length, var_start);
          if (array_idx == 0) //not valid_array_idx and index_val is not "count"
          {
            NSDL3_API(api_vptr, NULL, "0 is not a valid array index and index value is not count, var name = %*.*s", var_length, var_length, var_start);
            var_hashcode = -1;
          }
          else is_array = 1; 
        }

        if (var_hashcode == -1) // Must check again as it is reset above
        {
          NSDL3_API(api_vptr, NULL, "Not valid array type variable, var_hashcode = %d, var name = %*.*s", var_hashcode, var_length, var_length, var_start);
         //Nikita: Bug fixed 1454
         //The Problem was occured due to resetting of var_length.
          //var_length = strlen(find_ptr);
          NSDL3_API(api_vptr, NULL, "if not array, then var_length = %d", var_length);
        }
      }
    }

    if (var_hashcode != -1) 
    {
      /* got a variable name */
      int uservar_idx =api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].user_var_table_idx;
      if (!is_array) {
	var_ptr = &api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode];
        NSDL4_API(api_vptr, NULL, "Variable is not array type. Var Type = %d, var name = %*.*s uservar_idx = %d", 
                                     var_ptr->var_type, var_length, var_length, var_start, uservar_idx);
	switch (var_ptr->var_type) 
        {
	  case SEARCH_VAR:
          case JSON_VAR:
          case NSL_VAR:
          case TAG_VAR:
          case RANDOM_VAR:
          case RANDOM_STRING:
          case UNIQUE_VAR:
          case UNIQUE_RANGE_VAR:
	    value_ptr = &api_vptr->uvtable[uservar_idx];
            if(api_vptr->uvtable[uservar_idx].flags == VAR_IS_VECTOR){
	      ptr = NULL;
	      len = 0;
            } else{
	      ptr = value_ptr->value.value;
	      len = value_ptr->length;

              if (!ptr &&api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].var_type == NSL_VAR)
                ptr = get_nsl_var_default_value(api_vptr->sess_ptr->sess_id, uservar_idx, &len, api_vptr->group_num); 

              // To provide value of the variable when using eval_string before using the variable in any segment
              if(!ptr && api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].var_type == UNIQUE_VAR){
                ptr = get_unique_var_value(&uniquevar_table_shr_mem[var_ptr->var_idx], api_vptr, 1, &len);
                NSDL4_API(api_vptr, NULL, "Unique Var: in case when eval string is using unique var before any segment.");
              }

              NSDL4_API(api_vptr, NULL, "default value of ptr is (%s) and length is (%d)", ptr, len);
            }
	    NSDL3_API(api_vptr, NULL, "length of evaluated string = %d, ptr = [%s]", len, ptr);
	    NSDL3_API(api_vptr, NULL, "value_ptr = %p,api_vptr=%p, uservar idx = %d", value_ptr, api_vptr, uservar_idx);
	    break;

	  case VAR:
            //From 4.1.6 - since variable table shared memory has removed hence get pointer table by transition table
            NSDL3_API(api_vptr, NULL, "VAR: var_hashcode = %d, var_type = %d, fparam_grp_idx = %d, uservar_idx = %d, "
                                      "var_idx = %d, my_port_index = %d, total_group_entries = %d", 
                                       var_hashcode, var_ptr->var_type, var_ptr->fparam_grp_idx, uservar_idx, 
                                       var_ptr->var_idx, my_port_index, total_group_entries);

	    ptrtbl_ptr = get_var_val(api_vptr, 0, var_hashcode);
	    ptr = ptrtbl_ptr->big_buf_pointer;
	    len = ptrtbl_ptr->size;
	    break;

	  case INDEX_VAR:
	    ptrtbl_ptr = get_index_var_val(index_variable_table_shr_mem + uservar_idx, api_vptr, 0, SEG_IS_NOT_REPEAT_BLOCK); 
            if(ptrtbl_ptr) {
              ptr = ptrtbl_ptr->big_buf_pointer;
	      len = ptrtbl_ptr->size;
            } else {
              ptr = NULL; 
	      len = 0; 
            }
	    break;

          case DATE_VAR:
            value_ptr = &api_vptr->uvtable[uservar_idx];
            ptr = value_ptr->value.value;
            len = value_ptr->length;
            NSDL3_API(api_vptr, NULL, "length = %d", len);
            NSDL3_API(api_vptr, NULL, "value_ptr = %p,api_vptr=%p, uservar idx = %d", value_ptr, api_vptr, uservar_idx);
            break;

	  case COOKIE_VAR:
            // For Auto Cookie Mode, we are not supporting COOKIE Vars for now. So code will come
            // here only for Manual Cookies
	    cookie_val_ptr = &api_vptr->uctable[uservar_idx];
	    ptr = cookie_val_ptr->cookie_value;
	    len = cookie_val_ptr->length;
	    break;
	  case GROUP_VAR:
      	    group_val = &rungroup_table_shr_mem[ uservar_idx * total_runprof_entries +api_vptr->group_num];
	    ptr = group_val->value;
	    len = group_val->length;
	    break;
	  case CLUST_VAR:
      	    clust_val = &clust_table_shr_mem[ uservar_idx * total_clust_entries +api_vptr->clust_id];
	    ptr = clust_val->value;
	    len = clust_val->length;
	    break;
	  case GROUP_NAME_VAR:
      	    ptr = runprof_table_shr_mem[api_vptr->group_num].scen_group_name;
	    len = strlen(ptr);
	    break;
	  case CLUST_NAME_VAR:
      	    ptr = runprof_table_shr_mem[api_vptr->group_num].cluster_name;
	    len = strlen(ptr);
	    break;
	  case USERPROF_NAME_VAR:
      	    ptr = runprof_table_shr_mem[api_vptr->group_num].userindexprof_ptr->name;
	    len = strlen(ptr);
	    break;
	  default:
	    NSDL3_API(api_vptr, NULL, "ns_eval_string_flag; Unknown Variable type %d, var name = %*.*s", 
                                       var_ptr->var_type, var_length, var_length, var_start);
	    ptr="";
	    len=0;
	}

	buf_ptr = check_string_buf(total_copied, len, buf_ptr,api_vptr);
       
        NSDL4_API(api_vptr, NULL, "ptr = %p, len = %d", ptr, len);
        ENCODE_STR(ptr, len, encode_flag);
        #if 0 
        if(ptr != NULL && len != 0)
        {
          NSDL4_API(api_vptr, NULL, "encode_flag = %d, ptr = [%s], len = %d", encode_flag, ptr, len);
          if(encode_flag) // If to be encoded, encode 
          {
            ptr = ns_encode_url(ptr, len); 
            len = strlen(ptr);
            NSDL4_API(api_vptr, NULL, "Encoded variable value = %s, variable length = %d", ptr, len);
            memcpy(buf_ptr, ptr, len);
            ns_encode_decode_url_free(ptr);
          }
          else 
            memcpy(buf_ptr, ptr, len);
	  buf_ptr += len;
	  total_copied += len;
        }
        #endif
      }
      else 
      {
	if (array_idx == -1)  // for getting count of array variable
        {
	  int al_size = 0;
	  int array_length;
	  int temp_array_length;
	  int uservar_idx =api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].user_var_table_idx;

	  array_length =api_vptr->uvtable[uservar_idx].length;
	  NSDL3_API(api_vptr, NULL, "array_length = %d", array_length);
	  temp_array_length = array_length;
          if(temp_array_length == 0) al_size = 1;
	  while (temp_array_length) //calculate the no. of digits in array_length (1, 2, or 3 ...)
          {
	    al_size++;
	    temp_array_length/=10;
	  }
	  buf_ptr = check_string_buf(total_copied, al_size, buf_ptr,api_vptr);
	  sprintf(buf_ptr, "%d", array_length);
	  buf_ptr += al_size;
	  total_copied += al_size;
	} 
        else 
        {
	  if(array_idx >api_vptr->uvtable[uservar_idx].length)
          {
            fprintf(stderr, "Invalid index of the variable (%*.*s). Index = %d, Count = %d\n", var_length, var_length, var_start, array_idx,
                      api_vptr->uvtable[uservar_idx].length);
          }
          else
          {
	  ArrayValEntry* arrayval_ptr = &api_vptr->uvtable[uservar_idx].value.array[array_idx-1];
	  ptr = arrayval_ptr->value;
	  len = arrayval_ptr->length;
          /*Check if variable is NSL_VAR and ptr is null */
          if(!ptr &&api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].var_type == NSL_VAR)
            ptr = get_nsl_var_default_value(api_vptr->sess_ptr->sess_id, uservar_idx, &len, api_vptr->group_num); 
          NSDL4_API(api_vptr, NULL, "Variable is array type. var name = %*.*s, Variable value = %s, variable length = %d", var_length, var_length, var_start, ptr, len);

	  buf_ptr = check_string_buf(total_copied, len, buf_ptr,api_vptr);
          NSDL4_API(api_vptr, NULL, "ptr = %p, len = %d", ptr, len);
          ENCODE_STR(ptr, len, encode_flag);
          #if 0
          if(encode_flag) // If to be encoded, encode 
          {
            ptr = ns_encode_url(ptr, len); 
            len = strlen(ptr);
            NSDL4_API(api_vptr, NULL, "Encoded variable value = %s, variable length = %d", ptr, len);
            memcpy(buf_ptr, ptr, len);
            ns_encode_decode_url_free(ptr);
          }
          else
            memcpy(buf_ptr, ptr, len);
	  buf_ptr += len;
	  total_copied += len;
          #endif
          }
	}
      }
      string_ptr = var_end + 1;
    } 
    else 
    {  /* This is string */
      copy_length = var_length + 2;
      buf_ptr = check_string_buf(total_copied, copy_length, buf_ptr,api_vptr);
      NSDL3_API(api_vptr, NULL, "Found string. String Length = %d, total_copied = %d, String = %*.*s", 
                                  copy_length, total_copied, copy_length, copy_length, find_ptr);
      memcpy(buf_ptr, find_ptr, copy_length);
      buf_ptr += copy_length;
      string_ptr = find_ptr +  copy_length;
      total_copied += copy_length;
    }
  }

  // This will take care of string with no variable or last part of string after last variable if any
  copy_length = strlen(string_ptr);
  NSDL3_API(api_vptr, NULL, "copy_length = %d", copy_length);
  if (copy_length)
  {
    buf_ptr = check_string_buf(total_copied, copy_length, buf_ptr,api_vptr);
    NSDL3_API(api_vptr, NULL, "copy_length = %d, total_copied = %d, String = %*.*s", copy_length, total_copied, copy_length, copy_length, buf_ptr);
    memcpy(buf_ptr, string_ptr, copy_length);
    buf_ptr += copy_length;
  }
  *buf_ptr = '\0';

  if(api_vptr->thdd_ptr)
  {
    NSDL3_API(api_vptr, NULL, "Returning string = %s, length = %d, g_debug_script = %d", api_vptr->thdd_ptr->eval_buf, strlen(api_vptr->thdd_ptr->eval_buf), g_debug_script);
    *size = buf_ptr - api_vptr->thdd_ptr->eval_buf;
    return api_vptr->thdd_ptr->eval_buf;
  }
  else
  {
    NSDL3_API(api_vptr, NULL, "Returning string = %s, string = %s, length = %d, g_debug_script = %d", string_buffer, string, strlen(string_buffer), g_debug_script);
    *size = buf_ptr - string_buffer;
    return string_buffer;
  }
}


/*ns_advance_param_internal() API advances to the next available value in the parameter data file.
* Next value is based on the MODE of the parameter. 
*Arguments:
*  param_name: Name of the parameter in double quotes or a C variable containing 
*  the name of the parameter.  Name should be without curly brackets.
*Return Value: This function returns 0 on success and -1 on failure. 
* Possible errors are:
*   Parameter name is not correct
*   Parameter name is not a file parameter
* Modification Date:
*   27-Aug-2016    : Meenakshi - Adding Date var support in ns_advance_param
*/
int ns_advance_param_internal(const char *param_name, VUser *api_vptr)
{
  VarTransTableEntry_Shr* var_ptr;
  //VarTableEntry_Shr* fparam_grp_var;
  int len;
  IW_UNUSED(char *api_value);
  IW_UNUSED(PointerTableEntry_Shr * ptrtblentry_ptr = NULL);

  int var_hashcode;
  
  NSDL2_API(api_vptr, NULL, "Method called. param_name = %s, api_vptr = %p, NVM = %d", param_name, api_vptr, my_child_index);

  if((strchr(param_name, '{') != NULL ) || (strchr(param_name, '}') != NULL))
  {
    fprintf(stderr, "Parameter name should not be with curly brackets. Parameter name:%s\n", param_name);
    return -1;
  }

  //Find the hash value
  var_hashcode = api_vptr->sess_ptr->var_hash_func(param_name, strlen(param_name));
  NSDL3_API(api_vptr, NULL, "var_hashcode = %d", var_hashcode);
  if (var_hashcode == -1) {
    fprintf(stderr, "Invalid parameter name. Parameter name:%s\n", param_name);
    return -1;
  }

  /* got a variable name */
  NSDL3_API(api_vptr, NULL, "uservar_idx = %d", api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode].user_var_table_idx);
  var_ptr = &api_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode];

  switch(var_ptr->var_type)
  {
    case VAR:
      NSDL4_API(api_vptr, NULL, "for file parameter");
      //TODO: what will happend if NULL is returned
      //fparam_grp_var = get_fparam_var(api_vptr, -1, var_hashcode);
 
      UserGroupEntry* vugtable = &(api_vptr->ugtable[var_ptr->fparam_grp_idx]);
 
      /*
      * Since our goal is to advance the parameter value index to the next value 
      * (next value obviously has to be done based on the mode)
      * we need to reset remaining_valid_accesses to 0 as for
      * Refresh=SESSION, value will not get refreshed if this is not 0
      * For Refresh=USE also it will work as currently, only one var value be in the file for USE
      */ 
      vugtable->remaining_valid_accesses = 0;
      /*Pass 1 as last argument as we need to advance to next value*/
 
      IW_NDEBUG_UNUSED(ptrtblentry_ptr, get_var_val(api_vptr, 1, var_hashcode)); 

      if((api_vptr->page_status == NS_USEONCE_ABORT)) 
      {
        NS_DT1(api_vptr, NULL, DM_L1, MM_VARS, "Aborting session due to USE_ONCE data exhaust."); 

        if(runprof_table_shr_mem[api_vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
        {
           vut_add_task(api_vptr, VUT_END_SESSION);
           switch_to_nvm_ctx(api_vptr, "USE_ONCE data exhaust");
        }
        else /* This is done to handle case of thread mode and JAVA type script Bug - 69602*/
        {
           u_ns_ts_t now = get_ms_stamp();
           nsi_end_session(api_vptr, now);

           //if(runprof_table_shr_mem[api_vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
           /* Return value::
                   1.  To inform thread for not sending NS_ADAVANCE_PARAM_REP
                   2.  To inform JAVA about exit of session and not sending ADVANCE_PARAM_REP as ns_advance_param response.
                          This response is send from nsi_end_session  */
           return -2;
        }
        return -1; // For other cases return -1
      }
      NSDL3_API(api_vptr, NULL, "ptrtblentry_ptr = %p", ptrtblentry_ptr);
      break;

    case DATE_VAR:
      NSDL4_API(api_vptr, NULL, "for date parameter");

      IW_NDEBUG_UNUSED(api_value, get_date_var_value(&datevar_table_shr_mem[var_ptr->var_idx], api_vptr, 1, &len));
      NSDL4_API(api_vptr, NULL, "value_len = %d, value = %*.*s", len, len, len, api_value);
      break;

    case RANDOM_VAR:
      NSDL4_API(api_vptr, NULL, "for random int parameter");

      IW_NDEBUG_UNUSED(api_value, get_random_var_value(&randomvar_table_shr_mem[var_ptr->var_idx], api_vptr, 1, &len));
      NSDL4_API(api_vptr, NULL, "value_len = %d, value = %*.*s", len, len, len, api_value);
      break;    
 
    case RANDOM_STRING:
      NSDL4_API(api_vptr, NULL, "for random string parameter");

      IW_NDEBUG_UNUSED(api_value, get_random_string_value(&randomstring_table_shr_mem[var_ptr->var_idx], api_vptr, 1, &len));
      NSDL2_API(api_vptr, NULL, "value_len = %d, value = %*.*s", len, len, len, api_value);
      break; 

    case UNIQUE_RANGE_VAR:{    
      NSDL4_API(api_vptr, NULL, "for unique range parameter");
      IW_NDEBUG_UNUSED(api_value, get_unique_range_var_value(api_vptr, 1, &len, var_ptr->var_idx));
      NSDL2_API(api_vptr, NULL, "value_len = %d, value = %*.*s", len, len, len, api_value);
      break; 
     }

    case UNIQUE_VAR:
      NSDL4_API(api_vptr, NULL, "for unique parameter");
      IW_NDEBUG_UNUSED(api_value, get_unique_var_value(&uniquevar_table_shr_mem[var_ptr->var_idx], api_vptr, 1, &len));
      NSDL2_API(api_vptr, NULL, "value_len = %d, value = %*.*s", len, len, len, api_value);
      break; 

    default:
      fprintf(stderr, "Parameter is not a file or date or random value or random string or unique parameter (static variable). Parameter name:%s\n", param_name);
      return -1;
  }

  //Added in case of NetStormScriptDebugger mode
  if(g_debug_script)
  {
    char *name = (char *)param_name;
    NSDL3_API(api_vptr, NULL, "NetstormScriptDebugger is enabled");
    ns_eval_string(name);
  }
  return 0;
}

static inline void check_tran_buf(int start_tran_length,int end_tran_length, VUser *api_vptr) {
  NSDL2_API(api_vptr, NULL, "end_tran_length = %d, start_tran_length = %d, tx_name_size = %d",end_tran_length, start_tran_length, api_vptr->thdd_ptr->tx_name_size);
  if(api_vptr->thdd_ptr)
  {
    if(start_tran_length) {
      while ((start_tran_length + 1) >= api_vptr->thdd_ptr->tx_name_size) {
        MY_REALLOC_EX(api_vptr->thdd_ptr->tx_name,api_vptr->thdd_ptr->tx_name_size + 1024, api_vptr->thdd_ptr->tx_name_size, "api_vptr->thdd_ptr->tx_name", -1);
        api_vptr->thdd_ptr->tx_name_size += 1024;
      }
    }
    if(end_tran_length) {
      while ((end_tran_length + 1) >= api_vptr->thdd_ptr->end_as_tx_name_size) {
        MY_REALLOC_EX(api_vptr->thdd_ptr->end_as_tx_name,api_vptr->thdd_ptr->end_as_tx_name_size + 1024, api_vptr->thdd_ptr->end_as_tx_name_size, "api_vptr->thdd_ptr->end_as_tx_name", -1);
        api_vptr->thdd_ptr->end_as_tx_name_size += 1024;
      }
    }
  }
}

int ns_advance_param(const char *param_name)
{
  VUser *vptr = TLS_GET_VPTR();
  int ret = -1;

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)   /* Non-Thread Mode */
  {
     NSDL2_API(NULL, NULL, "Method called, param_name = %s", param_name?param_name:"NULL");
     ret =  ns_advance_param_internal(param_name, vptr);
  }
  else // Thread mode. Send message to NVM  to call this API
  {
    NSDL2_API(NULL, NULL, "Method called(Thread Mode), param_name = %s", param_name?param_name:"NULL");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_ADVANCE_PARAM_REQ;
    check_tran_buf(strlen(param_name),0,vptr);
    strcpy(vptr->thdd_ptr->tx_name, param_name);//As each thread would have its own VPTR so using tx_name to pass param_name
    ret = vutd_send_msg_to_nvm(NS_API_ADVANCE_PARAM_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }

 return ret;

}

char* ns_eval_string_flag(char* string, int encode_flag, long *size)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called. string = %s, encode_flag = %d", string, encode_flag);
  
  return ns_eval_string_flag_internal(string, encode_flag, size, vptr);
}


char* ns_encode_eval_string(char* string) 
{
  long len;
  NSDL2_API(NULL, NULL, "Method called. string = %s", string);
  return(ns_eval_string_flag(string, 1, &len));
}

/*
 * This api ns_get_param_val will take parameter name, buffer and length as its argument.
 * Variable_Name : This is the parameter name.
 * Buffer        : It may be NULL. In case of NULL we take static buffer.
 * Length        : This is the max length of parameter value which user want.
 *                 In case of it is less than parameter value we reset it with the parameter value length,
 *                 else it will be parameter value length.
 * In case of search parameter ORD = ALL it will give parameter value up to the given length and if not sufficient we truncate it.
 * For all other parameter we get their usual value and length will be set accordingly. 
 */

#define PARAMETER_BUFFER_SIZE (64*1024)
static __thread char static_buf[PARAMETER_BUFFER_SIZE] = "";

char* ns_get_param_val(char* variable_name, char* buf, int* len)
{
  char *tmp_buf = NULL;
  if(buf == NULL)
  {
    tmp_buf = static_buf;
  } 
  else {
    tmp_buf = buf;
  }
  return(ns_get_param_val_flag(variable_name, tmp_buf, len));
}

char* ns_eval_string(char* string) 
{
  long len;
  return(ns_eval_string_flag(string, 0, &len));
}

#define NS_SAVE_STRING_DELTA_FOR_ALLOC 128
//return 0 on success, -1 if not allowed, -2 if size is more than max size
//static int ns_save_string_flag(const char* param_value, int value_length, const char* param_name, int encode_flag)
//removed static for below
//
int ns_save_string_flag_internal(const char* param_value, int value_length, const char* param_name, int encode_flag, VUser *my_vptr, int not_binary_flag)
{
  int var_hashcode;
  VarTransTableEntry_Shr* var_ptr;
  UserVarEntry* value_ptr;
  //int value_length;
  VarTableEntry_Shr* abs_var_ptr;
  int max_size;

  if(param_value == NULL){
   NSTL1_OUT(NULL, NULL, "Error : ns_save_string_flag_internal() - Given value for variable '%s' is NULL, so it can't be stored", param_name);
   return -1; 
  }
    
  if(not_binary_flag) //since binary value need not to be log so use this flag
    NSDL2_API(my_vptr, NULL, "Method called. param_value = %s, value_length = %d param_name = %s, encode_flag = %d", param_value, value_length, param_name, encode_flag); 
  else
    NSDL2_API(my_vptr, NULL, "Method called. value_length = %d param_name = %s, encode_flag = %d", value_length, param_name, encode_flag); 
    

  var_hashcode = my_vptr->sess_ptr->var_hash_func(param_name, strlen(param_name));

  NSDL3_API(my_vptr, NULL, "var_hashcode = %d", var_hashcode);

  if (var_hashcode != -1) 
  {
    var_ptr = &my_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode];
    NSDL3_API(my_vptr, NULL, "var_type=%d", var_ptr->var_type);
    switch (var_ptr->var_type) 
   {
    case SEARCH_VAR:
    case JSON_VAR:
    case TAG_VAR:
    case NSL_VAR:
      //If Array var, set is not allowed
      if (my_vptr->sess_ptr->var_type_table_shr_mem[var_ptr->user_var_table_idx])
      {
        error_log("Error: Array variable set is not allowed.");
       	return -1;
      }
      NSDL3_API(my_vptr, NULL, "Going to copy into the variable");
      value_ptr = &my_vptr->uvtable[var_ptr->user_var_table_idx];

      if(var_ptr->var_type != NSL_VAR) {
        if (value_ptr->value.value) // Free old value of variable if any
        	//FREE_AND_MAKE_NULL(value_ptr->value.value, "value_ptr->value.value", -1);
        	FREE_AND_MAKE_NULL_EX(value_ptr->value.value, strlen(value_ptr->value.value), "value_ptr->value.value", -1);
        value_ptr->max_buf_space = 0; 
      }

      if(value_length < 0) {
        value_length = strlen(param_value);
      }

      if(encode_flag)
      {
        param_value = ns_encode_url(param_value, value_length);
        value_length = strlen(param_value);
        if(not_binary_flag)
          NSDL4_API(my_vptr, NULL, "Encoded saved variable value = %s, value length = %d", param_value, value_length);
        else
          NSDL4_API(my_vptr, NULL, "Encoded saved value length = %d", value_length);
      }
      
      // We can use value_ptr->length for maximum allocated size as value_ptr->value.value is '\0' terminated
      value_ptr->length = value_length;

      NSDL3_API(my_vptr, NULL, "value = [%p], max_buf_space = %d, value_length = %d",
                                 value_ptr->value.value,
                                 value_ptr->max_buf_space, value_length);
   
      if(value_ptr->value.value) {
        if((value_length + 1) > value_ptr->max_buf_space) {
          MY_REALLOC_EX(value_ptr->value.value, value_length + 1, value_ptr->max_buf_space, "value_ptr->value.value", 1);
          value_ptr->max_buf_space = value_length + 1;
        }
      } else {
        MY_MALLOC(value_ptr->value.value , value_length + 1, "value_ptr->value.value", 1);
        value_ptr->max_buf_space = value_length + 1;
      }

      if (!value_ptr->value.value) {
        error_log("Error: malloc failed for variable value.");
      	value_ptr->length = 0;
      	return -1;
      } else {
        NSDL3_API(my_vptr, NULL, "copying value to %p",value_ptr->value.value);
        
      	memcpy(value_ptr->value.value, param_value, value_ptr->length);
      	value_ptr->value.value[value_length] = '\0';

        if(not_binary_flag)
          NSDL3_API(my_vptr, NULL, "Final saved variable value = %s,"
                                  " length of the saved value is %d and"
                                  " length of variable value = %d",
                                  param_value, value_ptr->length, strlen(value_ptr->value.value));
        else
          NSDL3_API(my_vptr, NULL, "Final length of the saved value is %d and"
                                  " length of variable value = %d",
                                    value_ptr->length, strlen(value_ptr->value.value)); 

        if(encode_flag) 
          ns_encode_decode_url_free((char*)(param_value));

        return 0;
      }
      break;
    case VAR:
      //abs_var_ptr = variable_table_shr_mem + (var_ptr->user_var_table_idx);
      abs_var_ptr = get_fparam_var(my_vptr, -1, var_hashcode);
      max_size = abs_var_ptr->var_size;
      if (max_size > 0) 
     {
        PointerTableEntry_Shr* ptrtbl_ptr;
	int var_len = strlen(param_value);
	if (max_size < var_len)
	  return -2;

        if(encode_flag)
        {
          param_value = ns_encode_url(param_value, var_len);
          var_len = strlen(param_value);
          if(not_binary_flag)
            NSDL4_API(my_vptr, NULL, "Encoded saved variable value = %s, variable length = %d", param_value, var_len);
          else
            NSDL4_API(my_vptr, NULL, "Encoded saved variable length = %d", var_len);
             
        }
	//ptrtbl_ptr = get_var_val(abs_var_ptr, my_vptr, 0);
	ptrtbl_ptr = get_var_val(my_vptr, 0, var_hashcode);

	memcpy(ptrtbl_ptr->big_buf_pointer, param_value, var_len);
	ptrtbl_ptr->big_buf_pointer[var_len] = '\0';
	ptrtbl_ptr->size = var_len;

        if(not_binary_flag)
          NSDL3_API(my_vptr, NULL, "Final saved variable value = %s and length of the saved value = %d", param_value, var_len);
        else
          NSDL3_API(my_vptr, NULL, "Final saved length of the saved value = %d", var_len);
          

        if(encode_flag) 
          ns_encode_decode_url_free((char*)(param_value));
	break;
      } 
     else 
        return -1;
    case COOKIE_VAR:
    default:
      return -1;
    }
  } 
  else {
    char *vec_idx_ptr;
    int param_new_length;
    int param_idx;
    
    if((vec_idx_ptr = rindex(param_name, '_')) != NULL){
      param_new_length = vec_idx_ptr - param_name;
      vec_idx_ptr ++; /* move to index */
      /* If valid index found */
      if((var_hashcode = my_vptr->sess_ptr->var_hash_func(param_name, param_new_length)) != -1){
        var_ptr = &my_vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode]; 
        /*Check if param is of NSL var type and Array Type*/
        if(var_ptr->var_type == NSL_VAR && my_vptr->sess_ptr->var_type_table_shr_mem[var_ptr->user_var_table_idx]){
          /*Check if index is valid or not*/
          if(!ns_is_numeric(vec_idx_ptr) || atoi(vec_idx_ptr) <= 0){
            error_log("Error: Invalid index(%s) used with parameter %*.*s in api ns_save_string_internal()", vec_idx_ptr, param_new_length, param_new_length, param_name);
            return -1;
          }
          param_idx = atoi(vec_idx_ptr);
          value_ptr = &my_vptr->uvtable[var_ptr->user_var_table_idx];
          /*Check if index is more than size of variable*/
          if(param_idx > value_ptr->length){
            error_log("Error: Index(%d) out of bound for parameter %*.*s used in api ns_save_string_internal()", param_idx, param_new_length, param_new_length, param_name);
            return -1;
          }
          if(value_length <= 0) {
            value_length = strlen(param_value);
          }
          if(encode_flag)
          {
            param_value = ns_encode_url(param_value, value_length);
            value_length = strlen(param_value);
            if(not_binary_flag)
              NSDL4_API(my_vptr, NULL, "Encoded saved variable value = %s, value length = %d", param_value, value_length);
            else
              NSDL4_API(my_vptr, NULL, "Encoded saved value length = %d", value_length);
              
          }
          //right now we are freeing value each time but it will be better to use max_buf_size kind of thing ...
          if(value_ptr->value.array[param_idx - 1].value)
            FREE_AND_MAKE_NULL(value_ptr->value.array[param_idx - 1].value, "value_ptr->value.array[param_idx -1].value", -1);
          MY_MALLOC(value_ptr->value.array[param_idx - 1].value, value_length + 1, "value_ptr->value.array[param_idx - 1].value", -1);
          memcpy(value_ptr->value.array[param_idx - 1].value, param_value, value_length);
          value_ptr->value.array[param_idx - 1].value[value_length] = 0;  
          value_ptr->value.array[param_idx - 1].length = value_length;
          NSDL2_API(my_vptr, NULL, "Param name %*.*s, index = %d, value saved = %s, value length = %d", param_new_length, param_new_length,
             param_name, param_idx, value_ptr->value.array[param_idx - 1].value, value_ptr->value.array[param_idx - 1].length);

          return 0;
        } 
      }
    }
    NSDL1_API(my_vptr, NULL, "Parameter %s not declared used in ns_save_string_internal() api", param_name);
    
    return -1;
  }
  return 0;
}

int ns_save_string_flag(const char* param_value, int value_length, const char* param_name, int encode_flag)
{
  NSDL2_API(NULL, NULL, "Method called");

  VUser *vptr = TLS_GET_VPTR();
  

  return ns_save_string_flag_internal(param_value, value_length, param_name, encode_flag, vptr, 1);
}

int ns_encode_save_string(const char* param_value, const char* param_name)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");
  
  return(ns_save_string_flag_internal(param_value, -1, param_name, 1, vptr, 1));
}

int ns_save_string(const char* param_value, const char* param_name)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");

  return(ns_save_string_flag_internal(param_value, -1, param_name, 0, vptr, 1));
}

// Purpose: Copy string from source buffer to destination buffer. 
//          If source length is more that destination buffer length, then string is truncated
// Arguments:
//     dest: Destination buffer
//     src: Source buffer
//     dest_len: Destination buffer length including one bytes for NULL termination
// Return:
//     0: If string is copied without truncation
//     1: If string is copied with truncation as destination buffer is shorter than source buffer
//     -1: If destination length is <= 0. In this case, dest is made empty string
//
static int ns_chk_strcpy_internal(char *dest, char *src, int dest_len, VUser *my_vptr)
{
int src_len = strlen(src);

  NSDL2_API(my_vptr, NULL, "Method called. src_len = %d, dest_buf_len = %d, src = %s", src_len, dest_len, src);

  if(src_len < dest_len) // the length of the string is shorter than destination buffer
  {
    strcpy(dest, src);
    return 0;
  }

  if(dest_len <= 0)
  {
    NSDL1_API(my_vptr, NULL, "Destination length (%d) is not correct", dest_len);
    dest[0] = '\0';
    return -1;
  }

  NSDL1_API(my_vptr, NULL, "Destination is truncated as source length (%d) is greater than destination buffer length (%d)", src_len, dest_len);

  // Copy dest_len - 1 characters as we need to NULL terminate
  strncpy(dest, src, (dest_len -1));
  dest[dest_len - 1] = '\0';
  
  return 1;
}

int ns_chk_strcpy(char *dest, char *src, int dest_len)
{
  VUser *vptr = TLS_GET_VPTR(); 
  

  NSDL2_API(vptr, NULL, "Method called");
  
  return ns_chk_strcpy_internal(dest, src, dest_len, vptr); 
}

int ns_log_string(int level, char* buffer) 
{
  int amt_written = 0;

  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method called");
  if ((level < 1) || (level > 3))
    return -1;

  if(buffer != NULL) // Safety check as buffer can be NULL
    amt_written = strlen(buffer);

  if (amt_written > 4096) 
  {
     buffer[4096] = '\0';
     amt_written = 4096;
  }

  if (runprof_table_shr_mem[vptr->group_num].gset.log_level < level)
    return 0;

  if ((runprof_table_shr_mem[vptr->group_num].gset.log_dest == 0 ) || (runprof_table_shr_mem[vptr->group_num].gset.log_dest == 2))
    printf("%s\n", buffer);

  if ((runprof_table_shr_mem[vptr->group_num].gset.log_dest == 1 ) || (runprof_table_shr_mem[vptr->group_num].gset.log_dest == 2))
    log_message_record(msg_num++, get_ms_stamp(), buffer, amt_written);

  return 0;
}


int ns_log_msg(int level, char* format, ...) {
  va_list ap;
  int amt_written = 0;
  char buffer[4096];
  NSDL2_API(NULL, NULL, "Method called");
#if 0
  char* p;
  int ival;
  double dval;
  char* sval;

  va_start(ap, format);
  for (p = format; *p; p++) {
    switch (*p) {
    case '%':
      p++;
      switch (*p) {
      case 'd':
	ival = va_arg(ap, int);
	amt_written = sprintf(buffer, "%s%d", buffer, ival);
	break;
      case 'f':
	dval = va_arg(ap, double);
	amt_written = sprintf(buffer, "%s%f", buffer, dval);
	break;
      case 's':
	sval = va_arg(ap, char*);
	amt_written = sprintf(buffer, "%s%s", buffer, sval);
	break;
      default:
	va_end(ap);
	return -1;
      }
      break;
    default:
      buffer[amt_written++] = *p;
      buffer[amt_written] = 0;
      break;
    }
  }
  va_end(ap);
#endif
  va_start(ap, format);
  if((amt_written = vsnprintf(buffer, 4095, format, ap)) > 4095)
    amt_written = 4095;
  va_end(ap);

  buffer[amt_written] = 0;
  return ns_log_string(level, buffer);
}

// Changed by anuj after making the tx_end.
int ns_start_transaction_internal(char* tx_name, int sp_chk_flag)
{
  int ret;
  
  VUser *vptr = TLS_GET_VPTR();
  

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    NSDL2_API(vptr, NULL, "Method called for non thread mode, tx_name = %s", tx_name);
    if(sp_chk_flag && global_settings->sp_enable)
    {
      NSDL2_API(vptr, NULL, "SyncPoint is enabled");
      ret = ns_trans_chk_for_sp(tx_name, vptr);
      if(ret == 0) //If return 0 then this user was in sync point
        return 0; 
    }
    return (tx_start_with_name (tx_name, vptr));
  }
  else   //Thread mode
  {
    NSDL2_API(NULL, NULL, "Method called in thread mode tx_name = %s", tx_name);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_START_TX_REQ;
    
    api_req_opcode.child_id = sp_chk_flag;//Pass sp_chk_flag 
    check_tran_buf(strlen(tx_name), 0, vptr);
    strcpy(vptr->thdd_ptr->tx_name, tx_name);
    NSDL2_API(NULL, NULL, "tx_name = %s", vptr->thdd_ptr->tx_name);
    return(vutd_send_msg_to_nvm(NS_API_START_TX_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));
  }
}

int ns_start_transaction(char* tx_name)
{
  NSDL2_API(NULL, NULL, "Method called");

  return ns_start_transaction_internal(tx_name, 1);
}

int ns_start_transaction_ex(char* tx_name)
{
  NSDL2_API(NULL, NULL, "Method called");

  return ns_start_transaction_internal(ns_eval_string(tx_name), 0);
}

// Changed by anuj after making the tx_end.
int ns_end_transaction(char* tx_name, int status)
{
  int ret = 0;
  VUser *vptr = TLS_GET_VPTR();
  
  
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    NSDL2_API(NULL, NULL, "Method called in non_thread_mode, status = %d, tx_name = %s", status); 
    ret = tx_end(tx_name, status, vptr);
    return ret;
  }
  else   /* Thread Mode */
  {
    NSDL2_API(NULL, NULL, "Method called in thread mode, tx_name = %s", tx_name);
    
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_END_TX_REQ;
    
    vptr->thdd_ptr->status = status;
    check_tran_buf(strlen(tx_name), 0, vptr);
    strcpy(vptr->thdd_ptr->tx_name, tx_name);
 
    return(vutd_send_msg_to_nvm(NS_API_END_TX_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));
  }
}

int ns_end_transaction_ex(char* tx_name, int status)
{
  NSDL2_API(NULL, NULL, "Method called, status = %d", status);
  return ns_end_transaction(ns_eval_string(tx_name), status);
}

// This api is added here to support define transaction in legacy script
int ns_define_transaction(char *tx_name)
{
   VUser *vptr = TLS_GET_VPTR();
   NSDL2_API(vptr, NULL, "Method called.");
   if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT ||                                                                                                     runprof_table_shr_mem[vptr->group_num].gset.script_mode ==
                                                     NS_SCRIPT_MODE_SEPARATE_THREAD )
   {
      fprintf(stderr, "Error: In c type script, it should not come here, error in parsing script .\n");
   }
   return 0;
}

void ns_define_syncpoint(char *sp_name)
{
  return;
}

// For ending the transaction with the different name
int ns_end_transaction_as(char* tx_name, int status, char* end_tx_name)
{
  VUser *vptr = TLS_GET_VPTR();
  

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    NSDL2_API(NULL, NULL, "Method called in non_thread_mode tx_name = %s, end_tx_name = %s", tx_name, end_tx_name);
    return (tx_end_as (tx_name, status, end_tx_name, vptr));
  }
  else    /*Thread mode*/
  {
    NSDL2_API(NULL, NULL, "Method called in thread mode tx_name = %s, end_tx_name = %s", tx_name, end_tx_name);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_END_TX_AS_REQ;
    
    vptr->thdd_ptr->status = status;
    check_tran_buf(strlen(tx_name), strlen(end_tx_name), vptr);
    strcpy(vptr->thdd_ptr->tx_name, tx_name);
    strcpy(vptr->thdd_ptr->end_as_tx_name, end_tx_name);
    return(vutd_send_msg_to_nvm(NS_API_END_TX_AS_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));
  }
}

// for ending the transaction with the different name
int ns_end_transaction_as_ex(char* tx_name, int status, char* end_tx_name)
{
  NSDL2_API(NULL, NULL, "Method called");
  char end_tx_name_str[1024+1];
  strncpy(end_tx_name_str, ns_eval_string(end_tx_name), 1024);
  end_tx_name_str[1024] = '\0';
  return(ns_end_transaction_as(ns_eval_string(tx_name), status, end_tx_name_str));
}


// This API will be used for getting the total time  in milliseconds taken by the tranastion till when it called
int ns_get_transaction_time(char *tx_name)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    NSDL2_API(NULL, NULL, "Method called");
 
    return (tx_get_time (tx_name, vptr));
  }
  else
  {
    NSDL2_API(NULL, NULL, "Method called(Thread)");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_GET_TX_TIME_REQ;

    check_tran_buf(strlen(tx_name),0,vptr); //If API is first call in flow, memory unallocated
    strcpy(vptr->thdd_ptr->tx_name, tx_name);
 
    return(vutd_send_msg_to_nvm(NS_API_GET_TX_TIME_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));
  }
}

// This API is used for setting the status of a tx
// Must be called before end of the transaction
int ns_set_tx_status (char* tx_name, int status)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  { 
    NSDL2_API(NULL, NULL, "Method called");
    return (tx_set_status_by_name (tx_name, status, vptr));
  }
  else
  {
    NSDL2_API(NULL, NULL, "Method called(Thread)");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SET_TX_STATUS_REQ;

    check_tran_buf(strlen(tx_name),0,vptr); //If API is first call in flow, memory unallocated
    strcpy(vptr->thdd_ptr->tx_name, tx_name);
    vptr->thdd_ptr->status = status;
 
    return(vutd_send_msg_to_nvm(NS_API_SET_TX_STATUS_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));
  }
}

// This API is used for getting the status of a tx
int ns_get_tx_status (char* tx_name)
{
  VUser *vptr = TLS_GET_VPTR();
  
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    NSDL2_API(NULL, NULL, "Method called");
    return (tx_get_status(tx_name, vptr));
  }
  else
  {
    NSDL2_API(NULL, NULL, "Method called(Thread)");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_GET_TX_STATUS_REQ;

    check_tran_buf(strlen(tx_name),0,vptr);
    strcpy(vptr->thdd_ptr->tx_name, tx_name);

    return (vutd_send_msg_to_nvm(NS_API_GET_TX_STATUS_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));
  }
}

/*
#define DATE_NOW 0
#define TIME_NOW 0
#define ONE_DAY 86400
#define ONE_HOUR 3600
#define ONE_MIN 60

#define MAX_DATATIME_LEN 80
void lr_save_datatime(const char* format, int offset, char* name) {
  static char return_buf[MAX_DATETIME_LEN+1];
  static char** ab_day= {"Sun", "Mon", "Tues", "Wed", "Thur", "Fri", "Sat"};
  static char** day = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  static int day_length[] = {6, 6, 7, 9, 8, 6, 8};
  static char** ab_month = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  static char** month = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
  static int month_length[] = {7, 8, 5, 5, 3, 4, 4, 6, 9, 7, 8, 8};
  int i = 0;
  time_t current_time;
  struct tm* time_struct;
  int var_hashcode;
  int amt_written = 0;

  return_buf[0] = '\0';

  var_hashcode = var_hash_func(param_value, strlen(param_name));

  if (var_hashcode == -1)
    return;

  time(&current_time);
  current_time += offset;
  time_struct = localtime(&current_time);

  while (format) {
    switch(*format) {
    case '%':
      format++;
      if (format) {
	switch(*format) {
	case 'a':
	  snprintf(return_buf, "%s%s", return_buf, day[time_struct->tm_wday]);
	  break;
	case 'D':

	}
      }
    }
  }
}
*/

#if 0
int lr_save_searched_string(char* buffer, int buf_size, unsigned int occurrence, char* search_string,
			    int offset, unsigned int string_len, char* param_name) {
  char* search_ptr;
  int occur = 0;
  int search_str_len;
  int param_hashcode;
  UserVarEntry* user_var_ptr;
  char* end_ptr = buffer + buf_size;

  if (search_str)
    search_str_len = strlen(search_string);
  else
    return -1;

  if (parame_hashcode)
    param_hashcode = var_hash_func(param_name, strlen(parame_name));
  else
    return -1;

  if (param_hashcode == -1)
    return -1;
  else {
    vusertable_idx = vars_trans_table_shr_mem[var_hashcode].user_var_table_idx;
    if (type_table_shr_mem[vusertable_idx]) { /* make sure its not an array variable */
      return -1;
    }
    user_var_ptr = &vptr->uvtable[vusertable_idx];
  }

  while ((search_ptr = strstr(buffer, search_string))) {
    if (occur == occurrence) {

      save_ptr = search_ptr + offset;
      if (save_ptr > end_ptr)
	return 0;

      if (user_var_ptr->value.value) {
	FREE_AND_MAKE_NULL(user_var_ptr->value.value, "user_var_ptr->value.value", -1);
	user_var_ptr->value.value = (char*)malloc();
      }
    }
  }
}
#endif

//this api is used to log event to event.log file
int inline ns_log_event(int severity, char *format, ...) 
{
  NSDL2_API(NULL, NULL, "Method called");

/* BugId:7378 When we run a test with netstorm binary we have user stack of size 16KB and
              we were used buffer of 64KB size, this causes core dump.
             Solution: Reduced buffer size MAX_EVENT_LOG_BUF_SIZE (64KB) to 4096 Bytes. */
//  char buffer[MAX_EVENT_LOG_BUF_SIZE]="\0";
  char buffer[4096]="\0";
  int amt_written, amt_written1, tot_write;
  va_list ap;
  char *cur_page_name = "NA";
 
  VUser *vptr = TLS_GET_VPTR();
  

  amt_written = amt_written1 = 0;
 
  va_start (ap, format);
//  amt_written = vsnprintf(buffer + amt_written1, MAX_EVENT_LOG_BUF_SIZE - amt_written1, format, ap);
  amt_written = vsnprintf(buffer + amt_written1, 4096 - amt_written1, format, ap);
  va_end(ap);
  tot_write = amt_written1 + amt_written;
  buffer[tot_write] = 0;

  if(vptr->cur_page != NULL) // To handle script without any pages
    cur_page_name = (vptr->cur_page->page_name);

  NS_EL_2_ATTR(EID_FOR_API, vptr->user_index, vptr->sess_inst, EVENT_API, severity, vptr->sess_ptr->sess_name, cur_page_name,"%s", buffer);
  return 0;
}

// API to Set or Get Page Status 
int inline
get_page_status_internal (VUser *my_vptr) {
  NSDL2_API(NULL, NULL, "Method called");
  return my_vptr->page_status;
}

int inline
ns_get_page_status(void) {
  VUser *vptr = TLS_GET_VPTR();
  NSDL2_API(vptr, NULL, "Method called");
  return get_page_status_internal(vptr);
}

// API to Set or Get Session Status 
int inline
get_sess_status_internal (VUser *my_vptr) {
  NSDL2_API(NULL, NULL, "Method called");
  return my_vptr->sess_status;
}

int inline ns_get_session_status(void) {
  VUser *vptr = TLS_GET_VPTR();
  NSDL2_API(vptr, NULL, "Method called");
  return (get_sess_status_internal(vptr));
}

int inline
set_sess_status_internal(int status, VUser *my_vptr) {
  NSDL2_API(my_vptr, NULL, "Method called");
  if ((status < TOTAL_SESS_ERR) && ((status >= TOTAL_SESS_ERR - USER_DEF_SESS_ERR) || status == 0)) {
    my_vptr->sess_status = status;
    return 0;
  } else {
    return -1;
  }
}


int inline ns_set_session_status(int status)
{
  VUser *vptr = TLS_GET_VPTR();

  NSDL2_API(vptr, NULL, "Method called");
  return (set_sess_status_internal(status,vptr));
}


// Return session status as a string
char *ns_get_session_status_name(int status)
{
  NSDL2_API(NULL, NULL, "Method called");

  return(get_session_error_code_name(status));
}

// Return URL, Page and Tx  status as a string
char *ns_get_status_name(int status)
{
  NSDL2_API(NULL, NULL, "Method called");

  return(get_error_code_name(status));
}
int is_rampdown_mode_internal(VUser *my_vptr) {
  NSDL2_API(NULL, NULL, "Method called");
  /*
  if (gRunPhase == NS_RUN_PHASE_RAMP_DOWN || gRunPhase == NS_RUN_PHASE_RAMP_DOWN_CONTROLLED)
	return 1;
  else
	return 0;
  */
//Nikita: Bug fixed 3433
//If users are marked for ramping down then we will retun 1 otherwise 0.
 // return(vptr->flags & NS_VUSER_RAMPING_DOWN);
  if(my_vptr->flags & NS_VUSER_RAMPING_DOWN)
    return 1;
  else
    return 0;
}

int ns_is_rampdown_user_internal(VUser *my_vptr){
  NSDL2_API(my_vptr, NULL, "Method called");
  /*
  if (gRunPhase == NS_RUN_PHASE_RAMP_DOWN_CONTROLLED)
        return 1;
  else
        return 0;
  */
  //return(my_vptr->flags & NS_VUSER_RAMPING_DOWN);
  if(my_vptr->flags & NS_VUSER_RAMPING_DOWN)
    return 1;
  else
    return 0;
}

int ns_is_rampdown_user(void){
  
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(NULL, NULL, "Method called");

 return ns_is_rampdown_user_internal(vptr);
}

double ns_get_double_val(char *var)
{
  char buf[4096];

  if(var == NULL)
  {
    printf("ns_get_double_val: Variable name shouldn't be NULL (%s)\n", var);
    return -1;
  }  
  if (strlen(var) >= 4096)
  {
    printf("ns_get_double_val: too big NS variable name (%s)\n", var);
    return -1;
  }
  if (strchr (var , '{'))
  {
      printf ("Invalid NS variable name (%s) contains {\n", var);
      return -1;
  }
  strcpy (buf,"{");
  strcat (buf, var);
  strcat (buf, "}");
  return (atof(ns_eval_string(buf)));
}

int ns_get_int_val_internal(char *var, VUser *my_vptr) {
  
  char buf[4096];
  NSDL2_API(NULL, NULL, "Method called");
  long len;
  if(!var)
  {
    printf("ns_get_int_val: Input varible shouldn't be NULL");
    return -1;
  }
  if (strlen(var) >= 4096) {
    printf("ns_get_int_val: too big NS variable name (%s)\n", var);
    return -1;
  }
  if (strchr (var , '{')) {
    printf ("Invalid NS variable name (%s) contains {\n", var);
    return -1;
  }
  strcpy (buf,"{");
  strcat (buf, var);
  strcat (buf, "}");
  return (atoi(ns_eval_string_flag_internal(buf, 0, &len, my_vptr)));
}

int ns_get_int_val(char *var) 
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");
  return ns_get_int_val_internal(var, vptr);
}

int ns_set_int_val(char *var, int val) 
{
  char buf[32];
  VUser *vptr = TLS_GET_VPTR(); 
  sprintf(buf, "%d", val);
  
  NSDL2_API(NULL, NULL, "Method called");
  return(ns_save_string_flag_internal(buf, -1, var, 0, vptr, 1));  
}

int ns_set_double_val(const char* param_name, double value)
{
  char param_value[256 + 1];
  sprintf(param_value,"%f",value);
  
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(NULL, NULL, "Method called");
  return(ns_save_string_flag_internal(param_value, -1, param_name, 0, vptr, 1));
}

char inline * ns_get_user_ip_internal(VUser *my_vptr) {
  	NSDL2_API(NULL, NULL, "Method called");
	//return(inet_ntoa(vptr->user_ip->ip_addr));
        return ((char *)nslb_sock_ntop((struct sockaddr *)&my_vptr->user_ip->ip_addr));
}

char inline * ns_get_user_ip() 
{
  VUser *vptr = TLS_GET_VPTR();
   
  NSDL2_API(NULL, NULL, "Method called");   
  
  return ns_get_user_ip_internal(vptr);
}


int inline ns_get_nvmid() 
{
  NSDL2_API(NULL,NULL, "Method called");
  return(my_child_index);
}

unsigned int inline ns_get_userid_internal(VUser *my_vptr) {
  	NSDL2_API(NULL, NULL, "Method called");
	return(my_vptr->user_index);
}

unsigned int inline ns_get_userid() 
{
  VUser *vptr = TLS_GET_VPTR();
    
  NSDL2_API(NULL, NULL, "Method called");
  return ns_get_userid_internal(vptr);
}

unsigned int inline ns_get_sessid_internal(VUser *my_vptr) {
  	NSDL2_API(my_vptr, NULL, "Method called");
	return(my_vptr->sess_inst);
}

unsigned int inline ns_get_sessid() 
{
  VUser *vptr = TLS_GET_VPTR();
   
  NSDL2_API(NULL, NULL, "Method called");
  return ns_get_sessid_internal(vptr);
}

int inline ns_get_testid() 
{
  NSDL2_API(NULL, NULL, "Method called");
  return(testidx);
}  
int inline ns_get_controller_testid()
{
  NSDL2_API(NULL, NULL, "Method called");
  if (g_controller_testrun[0])
    return(atoi(g_controller_testrun));

  NSDL3_API(NULL, NULL, "NS_CONTROLLER_TEST_RUN env variable is not set.");
  return 0;
}

//converts unsigned int address to dotted notation
//Example: addr may be 0xC0A80112 return would be 192.168.1.18
//buffer is statically allocated. would be overwritten on next call.
/*
char * ns_char_ip (unsigned int addr)
{
  static char str_address[16];
  unsigned int a, b, c,d;
  NSDL2_API(vptr, NULL, "Method called");
  a = (addr >>24) & 0x000000FF;
  b = (addr >>16) & 0x000000FF;
  c = (addr >>8) & 0x000000FF;
  d = (addr) & 0x000000FF;
  sprintf(str_address, "%d.%d.%d.%d", a,b,c,d);
  return str_address;
}
*/

#define USE_SHM
//#include "ip_based_on_loc.c"

int ns_url_get_http_status_code_internal(VUser *vptr)
{
   NSDL2_API(vptr, NULL, "Method called");
   if(cur_cptr == NULL)
   {
     return (vptr->http_resp_code);
   }
   return(cur_cptr->req_code);
}

int ns_url_get_http_status_code()
{
  VUser *vptr = TLS_GET_VPTR();
  
  
  NSDL2_API(NULL, NULL, "Method called");
  return ns_url_get_http_status_code_internal(vptr);
}


int ns_url_get_resp_size_internal(VUser *vptr)
{
  NSDL2_API(vptr, NULL, "Method called");
  return ((vptr->bytes) + ((vptr->response_hdr)?vptr->response_hdr->used_hdr_buf_len:0));
}

int ns_url_get_resp_size()
{
  VUser *vptr = TLS_GET_VPTR();
   
  NSDL2_API(NULL, NULL, "Method called");
  
  return ns_url_get_resp_size_internal(vptr);
}

int ns_url_get_body_size_internal(VUser *vptr)
{
  NSDL2_API(vptr, NULL, "Method called");
  return vptr->bytes;
}

int ns_url_get_body_size()
{  
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method called");
  return ns_url_get_body_size_internal(vptr);
}

int ns_url_get_hdr_size_internal(VUser *vptr)
{
  NSDL2_API(NULL, NULL, "Method called");
  return (vptr->response_hdr)?vptr->response_hdr->used_hdr_buf_len:0;
}

int ns_url_get_hdr_size()
{
   VUser *vptr = TLS_GET_VPTR();
   
   NSDL2_API(NULL, NULL, "Method called");
   
   return ns_url_get_hdr_size_internal(vptr); 
}

char *ns_url_get_resp_msg_internal(VUser *vptr, int *size)
{
  NSDL2_API(NULL, NULL, "Method called");
  *size = ns_url_get_resp_size_internal(vptr);
  if(!*size)
  {
     return NULL;
  }
  return(vptr->url_resp_buff);
}

char *ns_url_get_resp_msg(int *size)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");
  return ns_url_get_resp_msg_internal(vptr, size);
}

char *ns_url_get_hdr_msg_internal(VUser *vptr, int *size)
{
  NSDL2_API(NULL, NULL, "Method called");
  *size = ns_url_get_hdr_size_internal(vptr);
  if(!*size)
  {
     return NULL;
  }
  return (vptr->response_hdr->hdr_buffer);
}

char *ns_url_get_hdr_msg(int *size)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");

  return ns_url_get_hdr_msg_internal(vptr, size); 
}

char *ns_url_get_body_msg_internal(VUser *vptr, int *size)
{
  NSDL2_API(NULL, NULL, "Method called");
  int hdr_size = 0;
  char* url_resp_buffer = ns_url_get_resp_msg_internal(vptr, size);
  hdr_size = ns_url_get_hdr_size_internal(vptr);
  *size -= hdr_size;
  NSDL2_API(NULL, NULL, "hdr_size = %d", hdr_size);
  return(url_resp_buffer + hdr_size);
}

char *ns_url_get_body_msg(int *size)
{ 
  VUser *vptr = TLS_GET_VPTR();
  
  
  NSDL2_API(NULL, NULL, "Method called");
  return ns_url_get_body_msg_internal(vptr, size);
}

char *ns_get_redirect_url ()
{
  NSDL2_API(NULL, NULL, "Method called");
  if(cur_cptr == NULL) {
    fprintf(stderr, "ns_get_redirect_url() with NULL cur_cptr\n");
    return NULL;
  }
  return cur_cptr->location_url;
}

char *ns_get_link_hdr_value()
{
  NSDL2_API(NULL, NULL, "Method called");
  if(cur_cptr == NULL) {
    fprintf(stderr, "ns_get_link_hdr_value() with NULL cur_cptr\n");
    return NULL;
  }
  return cur_cptr->link_hdr_val;
}

// Atul End
int ns_get_pg_think_time_internal(VUser *my_vptr)
{
  NSDL2_API(NULL, NULL, "Method called");
  if(my_vptr == NULL)
  {
    fprintf(stderr, "ns_get_pg_think_time() called with NULL my_vptr\n");
    return -1;
  }
  return(my_vptr->pg_think_time);
}

int ns_get_pg_think_time()
{
  VUser *vptr = TLS_GET_VPTR();

  NSDL2_API(NULL, NULL, "Method called");

  return ns_get_pg_think_time_internal(vptr);
}


// Start : Added by Anuj - 25/03/08
// It will return the num_process in the netstorm execution - nvm - netstorm virtual machine
int ns_get_num_nvm()
{
  NSDL2_API(NULL, NULL, "Method called");
  return (global_settings->num_process);
}


// It will return the keep_alive_pct used by the netstorm at the time of calling this API
int ns_get_num_ka_pct()
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");
  
  return (runprof_table_shr_mem[vptr->group_num].gset.ka_pct);
}


// It will return the minimum num_ka used by the netstorm at the time of calling this API
int ns_get_min_ka()
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");
  
  if ((runprof_table_shr_mem[vptr->group_num].gset.num_ka_min == 999999999) &&
      (runprof_table_shr_mem[vptr->group_num].gset.num_ka_range == 0))
    return (0);
  return (runprof_table_shr_mem[vptr->group_num].gset.num_ka_min);
} 

// It will return the maximum num_ka used by the netstorm at the time of calling this API
static int ns_get_max_ka_internal(VUser *my_vptr)
{
  NSDL2_API(my_vptr, NULL, "Method called");
  if ((runprof_table_shr_mem[my_vptr->group_num].gset.num_ka_min == 999999999) && 
      (runprof_table_shr_mem[my_vptr->group_num].gset.num_ka_range == 0)) 
    return (0);

  return ((runprof_table_shr_mem[my_vptr->group_num].gset.num_ka_range) + 
          (runprof_table_shr_mem[my_vptr->group_num].gset.num_ka_min));
}

int ns_get_max_ka()
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");

  return ns_get_max_ka_internal(vptr);
}


extern int add_user_data_point(int rptGroupID, int rptGraphID, double value);

int ns_add_user_data_point(int rptGroupID, int rptGraphID, double value)
{
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)   /* Non-Thread Mode */
  {
    return add_user_data_point(rptGroupID, rptGraphID, value);
  }
  else
  {
    VUser *vptr = TLS_GET_VPTR();
    NSDL2_API(NULL, NULL, "Method Called(Thread)");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_USER_DATA_POINT_REQ;

    vptr->thdd_ptr->tx_name_size = rptGroupID;
    vptr->thdd_ptr->end_as_tx_name_size = rptGraphID;
    vptr->thdd_ptr->page_think_time = value;

    return(vutd_send_msg_to_nvm(NS_API_USER_DATA_POINT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));
  }
}

static int ns_add_cookie_val_ex_internal(char *cookie_name, char *domain, char *path, char *cookie_val, VUser *my_vptr)
{
  NSDL2_COOKIES(my_vptr, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s, Cookie Value = %s", cookie_name, domain, path, cookie_val);

  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_add_cookie_val_ex() cannot be used if Auto Cookie Mode is disabled\n");
    return -1;
  }
  
  return ns_add_cookie_val_auto_mode(cookie_name, domain, path, cookie_val, my_vptr);
}

int ns_add_cookie_val_ex(char *cookie_name, char *domain, char *path, char *cookie_val)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_COOKIES(NULL, NULL, "Method called. Cookie Name = %s, Domain = %s, Path = %s, Cookie Value = %s", cookie_name, domain, path, cookie_val);
  
  return ns_add_cookie_val_ex_internal(cookie_name, domain, path, cookie_val,vptr);
}

static inline void read_cookie_attr(char *buffer, char **p_cursor, char *ptr_semicolon)
{
  *p_cursor = strchr(*p_cursor, '=');
  //if(cursor && cursor < ptr_semicolon)
  if(*p_cursor)
  {
    (*p_cursor)++;
    while(**p_cursor == ' ' || **p_cursor == '\t')
      (*p_cursor)++;
    if(ptr_semicolon && *p_cursor < ptr_semicolon)
    {
      strncpy(buffer, *p_cursor, ptr_semicolon - *p_cursor);
      buffer[ptr_semicolon - *p_cursor] = '\0';
    }
    else if(!ptr_semicolon)
    {
      strcpy(buffer, *p_cursor);
    }
  }
  if(ptr_semicolon)
    *p_cursor = ptr_semicolon + 1;
}

/***********************************************************************
 * int ns_add_cookies(char *cookie_buf)
 *
 * This is a convenience API to set the cookies coming in a buffer following format
 *     <CookieName>=<CookieValue>;path=<path>;domain=<domain>;,<other cookies>
 *
 * input args
 *   cookie buf char* cookie_buf
 *
 * returns
 *   - 0 on success
 *   -   if any cookie insertion fails, returns the return value of the 
 *       ns_add_cookie_val_auto_mode() API for failed call to this API.
 *
 * Notes:
 *  o Cookies are separated by comma.
 *  o This internally calls  ns_add_cookie_val_auto_mode() API.
 *
 * Limitation - If comma comes in the cookie name or value, this will not
 *              give expected results. This API tokenizes the individual
 *              cookies by comma, and name=value pair, path and domain by 
 *              semicolon.
 ***********************************************************************/ 
int
ns_add_cookies_internal (char *cookie_buf, VUser *my_vptr)
{
  #define MAXSZ 1024
  int ret = 0;
  char *cursor = NULL, *ptr_comma = NULL, *ptr_semicolon = NULL, *ptr_equal = NULL;
  char cname[MAXSZ], cvalue[MAXSZ], cpath[MAXSZ], cdomain[MAXSZ], cookie[4*MAXSZ];

  NSDL2_COOKIES(NULL, NULL, "Method called. cookie_buf = %s", cookie_buf);

  if(!cookie_buf || cookie_buf[0] == '\0')
  {
    ret = -1;
    fprintf(stderr, "Error: ns_add_cookies() - cookie_buf supplied is empty or NULL.\n");
    NSDL1_COOKIES(NULL, NULL, "Error: ns_add_cookies() - cookie_buf supplied is empty or NULL");
  }

  cursor = cookie_buf;

  while(*cursor == ' ' || *cursor == '\t')
    cursor++;

  while(1)
  { 

    ptr_comma = strchr(cursor, ',');
    if(ptr_comma)
    {
      strncpy(cookie, cursor, ptr_comma - cursor);
      cookie[ptr_comma - cursor] = '\0';
    }
    else
    {
      strcpy(cookie, cursor);
    }

    if(cookie[0] == '\0')
    {
      ret = -1;
      NSDL1_COOKIES(NULL, NULL, "Error in parsing cookie_buf, Empty cookie data");
    }
    else
    {
      cursor = cookie;

      cname[0] = cvalue[0] = cpath[0] = cdomain[0] = '\0';
      while(1)
      {
        while(*cursor == ' ' || *cursor == '\t')
          cursor++;

        ptr_semicolon = strchr(cursor, ';');
        if(!strncasecmp(cursor, "path", 4))
        {
          read_cookie_attr(cpath, &cursor, ptr_semicolon);
        }
        else if(!strncasecmp(cursor, "domain", 6))
        {
          read_cookie_attr(cdomain, &cursor, ptr_semicolon);
        }
        else
        {
          ptr_equal = strchr(cursor, '=');
          //if(ptr_equal && (ptr_equal < ptr_semicolon) )
          if(ptr_equal)
          {
            strncpy(cname, cursor, ptr_equal - cursor);
            cname[ptr_equal - cursor] = '\0';
            cursor = ptr_equal + 1;
            while(*cursor == ' ' || *cursor == '\t')
              cursor++;
            if(ptr_semicolon && cursor < ptr_semicolon)
            {
              strncpy(cvalue, cursor, (ptr_semicolon - cursor));
              cvalue[ptr_semicolon - cursor] = '\0';
              cursor = ptr_semicolon + 1;
            }
            else if(!ptr_semicolon)
            {
              strcpy(cvalue, cursor);
            }
          }
          else
            break;
        }
        if(!ptr_semicolon) 
          break;
      }// End of while(1)

      if(-1 == ns_add_cookie_val_auto_mode(cname, 
                                           (cdomain[0] == '\0')?NULL:cdomain, 
                                           (cpath[0] == '\0')?NULL:cpath, 
                                            cvalue, my_vptr))
        ret = -1;

    } 
    if(!ptr_comma)
      break;

    cursor = ptr_comma + 1;
    while(*cursor == ' ' || *cursor == '\t')
      cursor++;
  }// End of while(1)

  return (ret);
}

int ns_add_cookies (char *cookie_buf)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_COOKIES(NULL, NULL, "Method called. cookie_buf = %s", cookie_buf);

  return ns_add_cookies_internal (cookie_buf, vptr);
}


//api to return next page in replay mode
int ns_get_replay_page()
{ 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method Called");
  
  int page_id = -1;

  page_id = ns_get_replay_page_ext(vptr);
  
  //it may possible script has less pages than used in index files for replay mode
  if(vptr->sess_ptr->num_pages <= page_id)
  {
    NS_EXIT(-1, "Error: Script has less pages than used in ns_index_%d.\n", my_child_index);
  }
 
  return page_id;
}

u_ns_ts_t ns_get_ms_stamp()
{
  NSDL2_API(NULL, NULL, "Method Called");
  return (get_ms_stamp());
}

char *ns_ip_to_char(unsigned int addr)
{
  NSDL2_API(NULL, NULL, "Method Called");
  return(ns_char_ip(addr));
}

// Called from script debug log Macros 
int ns_debug_log_scr(int log_level, unsigned int mask, char *file, int line, char *fname, char *format, ...)
{

  va_list ap;
  char buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE + 1];
  int amt_written = 0;

  VUser *vptr = TLS_GET_VPTR();
  

  
  va_start (ap, format);
  amt_written = vsnprintf(buffer, MAX_DEBUG_ERR_LOG_BUF_SIZE, format, ap);
  va_end(ap);
  
  buffer[amt_written] = '\0';

  ns_debug_log_ex(log_level, mask, file, line, fname, vptr, NULL, buffer);

  return 0;
}

/* This function is required in shared lib with xml vars ORD=ALL */
/* If any change has to be made to the arguments of this function, same should 
 * be reflected for its lib cal from create_check_func() */
#define DELTA_TAG_TMP_TABLE_ENTRIES 32
int create_tag_tmp_table_entry(int *row_num, NodeVarTableEntry_Shr *nodevar_ptr)
{
  NSDL2_VARS(NULL, NULL, "Method called");
  int *total, *max;
  //char *ptr;

  total = &(nodevar_ptr->tmp_array->total_tmp_tagvar);
  max   = &(nodevar_ptr->tmp_array->max_tmp_tagvar);
  //ptr   = (char *)(nodevar_ptr->tempTagArrayVal);

  if (*total == *max)
  {
   // MY_REALLOC(nodevar_ptr->tmp_array->tempTagArrayVal, 
   //            (*max + DELTA_TAG_TMP_TABLE_ENTRIES) * sizeof(ArrayValEntry), 
   //            "nodevar_ptr->tempTagArrayVal", -1);

    MY_REALLOC_EX(nodevar_ptr->tmp_array->tempTagArrayVal,
               (*max + DELTA_TAG_TMP_TABLE_ENTRIES) * sizeof(ArrayValEntry),
               (*max) * sizeof(ArrayValEntry),
               "nodevar_ptr->tempTagArrayVal", -1);

    *max += DELTA_TAG_TMP_TABLE_ENTRIES;
  }
  *row_num = (*total)++;

  //if(global_settings->debug) 
  NSDL2_VARS(NULL, NULL, "row_num = %d, total = %d, max = %d", *row_num, *total, *max);
  return 0;
}

/* Function is to handle Ajax functionality for Macys; URL 2d array will
 * be directly passed to it with number of urls. This will then add them
 * to the hurl array and the URLs will be hit. */
EmbdUrlsProp *eurls_prop;
int g_num_eurls_set_by_api = 0;
int ns_set_embd_objects(int num_eurls, char **eurls, int both_api_and_auto_fetch_url) 
{
  int i;
  char *ptr;

  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");

  /* Ignore in case no eurls are there */
  if ((num_eurls <= 0) || eurls == NULL)
    return -1;

  if(!(cur_cptr->flags & NS_CPTR_FINAL_RESP))
  {
    NSDL2_API(vptr, NULL, "This is not final resp api will not work on this depth, so returning");
    return -1;
  }

  MY_MALLOC(eurls_prop, sizeof(EmbdUrlsProp)*num_eurls, "EmbdUrlsProp", 1);

  for(i = 0; i < num_eurls; i++) {
    eurls_prop[i].embd_type = 0; 
    MY_MALLOC(ptr, strlen(eurls[i]) + 1, "eurls[i]", i);
    strcpy(ptr, eurls[i]);
    eurls_prop[i].embd_url = ptr;
  }
  g_num_eurls_set_by_api = num_eurls;

  //set_embd_objects(cur_cptr, num_eurls, eurls_prop);
  //FREE_AND_MAKE_NOT_NULL(eurls_prop, "EmbdUrlsProp", -1);
  //FREE_AND_MAKE_NOT_NULL_EX(eurls_prop, sizeof(eurls_prop), "EmbdUrlsProp", -1);

  vptr->flags |= NS_EMBD_OBJS_SET_BY_API;

  if(both_api_and_auto_fetch_url)
    vptr->flags |= NS_BOTH_AUTO_AND_API_EMBD_OBJS_NEED_TO_GO;

  return 0;
}

/* 
 * function to indicate that host URL and port at a given redirection depth must
 * be saved into the appropriate locations in the vptr
 * the actual save will be done elsewhere in NS, where the redirection depth is
 * known
 * inputs 
 * type - 0 -> save hostname
 *      - 1 -> save full URL
 * depth - redirection depth for saving
 * outputs
 * var - save into this variable
 *
 */
int ns_setup_save_url(int type, int depth, char *var)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  
  return (ns_setup_save_url_ext(type, depth, var, vptr));
}

/* 
 * function to set the server in use to the mapping corresponding to that of the
 * rec server that was saved upon redirection. the mapping for this rec server
 * must exist in the server entry tables.
 */

int ns_force_server_mapping(char *rec_host, char* map_to_server)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called."); 
  
  return (ns_force_server_mapping_ext(vptr, rec_host, map_to_server));
}

// Following IPs are used to get AREA IP as a number
unsigned int ns_get_area_ip(unsigned int area_id) {
    
  NSDL2_API(NULL, NULL, "Method Called, area_id = %lu", area_id);

  return(get_an_IP_address_for_area(area_id));
}

// Following IPs are used to get AREA IP as a char (in dotted notation)
char *ns_get_area_ip_char(unsigned int area_id) {

  NSDL2_API(NULL, NULL, "Method Called, area_id = %d", area_id);
  unsigned int addr = get_an_IP_address_for_area(area_id); 
  NSDL2_API(NULL, NULL, "addr = %lu", addr);
  
  return(ns_ip_to_char(addr));
}

/*Get random number as numeric value.*/
int ns_get_random_number_int(int min, int max)
{
  char *format = "%d";
  int format_len = strlen(format);
  int total_len;
  char *rand_num_str;
  int rand_num;
  
  if(max < min){
    NSDL2_API(NULL, NULL, "Error:max is less than min. max= %d, min = %d", max, min);
    return 0;
  }
  
  rand_num_str = get_random_number((double) min, (double) max, format, format_len, &total_len, 0);
  rand_num = atoi(rand_num_str); 
  return rand_num;
}

/*Get random number as string.*/
char *ns_get_random_number_str(int min, int max, char *format)
{
  int format_len = strlen(format);
  int total_len;
 
  if(max < min){
    NSDL2_API(NULL, NULL, "Error:max is less than min. max= %d, min = %d", max, min);
    return NULL;
  }

  return((char *) get_random_number((double) min, (double) max, format, format_len, &total_len, 0));
}

char* ns_get_random_str (int min, int max, char *format) {
  int len;
  if(min < 0 || max < 0 || max < min){
    NSDL2_API(NULL, NULL, "Error: either max/min is negative or max is less than min. max= %d, min = %d", max, min);
    return NULL;
  }
  return((char*)nslb_get_random_string ((double)min, (double)max, format, &len));
}

/*This function is for setting
 * Keep Alive timeout at runtime.
 * Given time is in milli seconds*/
int ns_set_ka_time(int ka_time)
{ 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method Called, setting ka_timeout = %d", vptr->ka_timeout);

  vptr->ka_timeout = ka_time;
  return 0;
}

/*This function is for getting
 * Keep Alive timeout at runtime.
 * Given time is in milli seconds*/
int ns_get_ka_time()
{
  int ka_time;
  VUser *vptr = TLS_GET_VPTR();
    

  NSDL2_API(vptr, NULL, "Method Called");
  ka_time = vptr->ka_timeout;

  return ka_time;
}
/***************************Start CTX**************/

int ns_web_url(int page_id) {
  NSDL2_API(NULL, NULL, "Method Called, page_id = %d", page_id); 

  VUser *vptr = TLS_GET_VPTR();
  int ret;
  
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)   /* Non-Thread Mode */
  {
    NSDL2_API(NULL, NULL, "Method Called, enable_rbu = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu); 
    ret = ns_web_url_ext(vptr, page_id);
    NSDL2_API(NULL, NULL, "User Mode : take the response as [%d] for  page_id = %d", ret, page_id);
  }
  else    /*  Thread Mode */
  {
    NSDL2_API(NULL, NULL, "Method Called(Thread)"); 
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_WEB_URL_REQ;
 
    vptr->thdd_ptr->page_id = page_id;
 
    ret = vutd_send_msg_to_nvm(VUT_WEB_URL, (char *)(&api_req_opcode), sizeof(Ns_api_req));

/*    // Bug 111105 - Done same handling as done for user context
    if(vptr->flags & NS_VPTR_FLAGS_SESSION_COMPLETE)
    {
      vptr->flags &= ~NS_VPTR_FLAGS_SESSION_COMPLETE;
      ns_exit_session();
      return 0;
    } */

    NSDL2_API(NULL, NULL, "Thread Mode : take the response as [%d] for  page_id = %d", ret, page_id);
  }
  return ret;
}

//Currently ns_web_websocket_send is not supported in THREAD mode
int ns_web_websocket_send(int send_id) 
{  
  VUser *vptr = TLS_GET_VPTR();
  
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    NSDL2_API(NULL, NULL, "Method Called, send_id = %d", send_id);
    vptr->ws_send_id = send_id;
    int ret = ns_websocket_ext(vptr, send_id, 0);
    NSDL2_API(vptr, NULL, "ns_web_websocket_send(): Exit, ret = %d", ret);
    return ret;
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), send_id = %d", send_id);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_WEBSOCKET_SEND_REQ;
    vptr->thdd_ptr->page_id = send_id;

    int ret = vutd_send_msg_to_nvm(VUT_WS_SEND, (char *)(&api_req_opcode), sizeof(Ns_api_req));
    /* RBU: In case of RBU script in function ns_web_url_ext() only data structure filled
       Page is not executed there so execute page here  
    */
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
      ret = ns_rbu_execute_page(vptr, send_id);

    NSDL2_API(NULL, NULL, "take the response and take response send_id = %d", send_id);
    return (ret); 
  }
}

int ns_web_websocket_close(int close_id) 
{
  NSDL2_API(NULL, NULL, "Method Called, close_id = %d", close_id);
  
  VUser *vptr = TLS_GET_VPTR();
  int ret;
  
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vptr->ws_close_id = close_id;
    ret = ns_websocket_ext(vptr, close_id, 1);
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), send_id = %d", close_id);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_WEBSOCKET_CLOSE_REQ;
    vptr->thdd_ptr->page_id = close_id;

    ret = vutd_send_msg_to_nvm(VUT_WS_CLOSE, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }

  NSDL2_API(NULL, NULL, "ns_web_websocket_close(): Exit, ret = %d", ret);

  return ret; 
}

int ns_page_think_time(double page_think_time)
{
  /*if(runprof_table_shr_mem[vptr->group_num].gset.script_mode != NS_SCRIPT_MODE_USER_CONTEXT)
  {
    fprintf(stderr, "Error: ns_page_think_time() is valid for execution of script in user context mode.\n");
    return -1;
  }*/
  VUser *vptr = TLS_GET_VPTR();
  

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    NSDL2_API(NULL, NULL, "Method Called, page_think_time = %.3f seconds", page_think_time); 
    ns_page_think_time_ext(vptr, (int )(page_think_time * 1000));
  }
  else   //Thread mode
  {
    NSDL2_API(NULL, NULL, "Method Called(Thread Mode), page_think_time = %.3f seconds", page_think_time); 
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_PAGE_TT_REQ;
    vptr->thdd_ptr->page_think_time = (int)(page_think_time * 1000); 
 
    vutd_send_msg_to_nvm(VUT_PAGE_THINK_TIME, (char *) (&api_req_opcode), sizeof(Ns_api_req));
    NSDL2_API(NULL, NULL, "Sending message to NVM and waiting for reply");
  }

  return(0);
}


int ns_set_ssl_settings(char *cert_file, char *key_file)
{
  NSDL2_API(NULL, NULL, "Method Called, cert_file = %s and keys_file = %s", cert_file, key_file);
  
  VUser *vptr = TLS_GET_VPTR();
  

  ns_set_ssl_setting_ex(vptr, cert_file, key_file);

  return 0;
}

int ns_unset_ssl_settings()
{
  NSDL2_API(NULL, NULL, "Method Called");

  VUser *vptr = TLS_GET_VPTR();
  

  ns_unset_ssl_setting_ex(vptr);
  return 0;
}

int ns_exit_session(void) {

  NSDL2_API(NULL, NULL, "Method Called");

  VUser *vptr = TLS_GET_VPTR();

  // Bug - 111105 
  // Code removed for all modes.
  // IF Session Exit is already not done we call call exit function and there we will set this flag. 
  // This is done to avoid multiple session exit.
  // If this flag is already set we will call ns_exit_session_ext which is also getting called from run_script_exit_func
  // after exit script execution.
  if(!(vptr->flags & NS_VPTR_FLAGS_SESSION_EXIT))
    run_script_exit_func(vptr);
  else
    ns_exit_session_ext(vptr);

  return 0;

}

int ns_sync_point(char *sync_name)
{
   VUser *vptr = TLS_GET_VPTR();
    
  NSDL1_API(NULL, NULL, "Method Called");
  if(!global_settings->sp_enable)
  {
    NSDL2_API(NULL, NULL, "SyncPoint is not enabled");
    return -1;
  }

 if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    NSDL1_API(NULL, NULL, "Method Called");
    ns_sync_point_ext(sync_name, vptr);
    return (0);
  }
  else   //Thread mode
  {
    NSDL1_API(NULL, NULL, "Method Called(Thread)");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SYNC_POINT_REQ;

    check_tran_buf(strlen(sync_name),0,vptr); //If API is first call in flow, memory unallocated
    strcpy(vptr->thdd_ptr->tx_name, sync_name);
    return(vutd_send_msg_to_nvm(NS_API_SYNC_POINT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req)));    
  }
}


/******** End CTX **************/

/*********TR069**********************/
int ns_tr069_cpe_invoke_inform(int page_id) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");


  return(tr069_cpe_invoke_inform_ex(vptr, page_id));
}

int ns_tr069_cpe_invite_rpc(int page_id) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_invite_rpc_ex(vptr, page_id));
}

int ns_tr069_cpe_execute_get_rpc_methods(int page_id) {

 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_execute_get_rpc_methods_ex(vptr, page_id));
}

int ns_tr069_cpe_execute_set_parameter_values(int page_id) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_execute_set_parameter_values_ex(vptr, page_id));
}

int ns_tr069_cpe_execute_get_parameter_values(int page_id) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_execute_get_parameter_values_ex(vptr, page_id));
}

int ns_tr069_cpe_execute_get_parameter_names(int page_id) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_execute_get_parameter_names_ex(vptr, page_id));
}

int ns_tr069_cpe_execute_set_parameter_attributes(int page_id) {

 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_execute_set_parameter_attributes_ex(vptr, page_id));
}

int ns_tr069_cpe_execute_get_parameter_attributes(int page_id) {

 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_execute_get_parameter_attributes_ex(vptr, page_id));
}

// Other api
int ns_tr069_cpe_execute_reboot(int page_id) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");


  return(tr069_cpe_reboot_ex(vptr, page_id));
}

int ns_tr069_cpe_execute_download(int page_id) {
  
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_download_ex(vptr, page_id));
}


// Object Relates api
int ns_tr069_cpe_execute_add_object(int page_id) {

  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_add_object_ex(vptr, page_id));

}

int ns_tr069_cpe_execute_delete_object(int page_id) {

  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_delete_object_ex(vptr, page_id));

}

int ns_tr069_cpe_transfer_complete(int page_id) {

 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  return(tr069_cpe_transfer_complete_ex(vptr, page_id));
}

int ns_tr069_get_rfc(int wait_time) {

 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called. wait_time = %d", wait_time);

  return(ns_tr069_get_rfc_ext (vptr, wait_time));
}

int ns_tr069_register_rfc (char *ip, unsigned short port, char *url) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called, ip = [%s], port = [%hu]", ip, port);

  return(ns_tr069_register_rfc_ext (vptr, ip, port, url));
}

int ns_tr069_get_periodic_inform_time(void) {


  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called.");

  return(ns_tr069_get_periodic_inform_time_ext(vptr));
}

int ns_tr069_get_periodic_inform_time_ex()
{

  int time;

  time =  ns_get_random_number_int(global_settings->tr069_periodic_inform_min_time, global_settings->tr069_periodic_inform_max_time);

  return time;
}

void ns_dos_syn_attack(char *source_ip_add, char *dest_ip_add, unsigned int dest_port, int num_attack){
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called: source_ip_add = [%s], dest_ip_add = [%s], dest_port = [%u]", source_ip_add, dest_ip_add, dest_port);
 return(ns_dos_syn_attack_ext(vptr, source_ip_add, dest_ip_add, dest_port, num_attack)); 
}

void *ns_malloc(int size)
{
void *ptr;

  MY_MALLOC(ptr, size, "From API", -1);
  return ptr;

}

void *ns_realloc(void *ptr, int size, int old_size)
{

  MY_REALLOC_EX(ptr, size, old_size, "From API", -1);
  return ptr;

}

void ns_free(void *ptr, int size)
{
  FREE_AND_MAKE_NOT_NULL_EX(ptr, size, "From API", -1);
}


#define NS_MD5_CHECKSUM_BYTES 16

// Converts MD5 chesksum in brinay (source) to ascii hex (dest)
void ns_md5_checksum_to_ascii(unsigned char *source, unsigned char *dest)
{
  int i;
  for(i = 0; i < NS_MD5_CHECKSUM_BYTES; i++)
    snprintf((char *)&dest[i*2], 3, "%02x", source[i]);
}


// Generate checksum using MD5 in hex
int ns_gen_md5_checksum(const unsigned char *buf, int len, unsigned char *checksum_buf) 
{
  unsigned char checksum_buf_bin[NS_MD5_CHECKSUM_BYTES + 1];
  
  NSDL2_API(NULL, NULL, "Method Called: buf = %p, len = %d", buf, len);

  MD5(buf, len, checksum_buf_bin);
  ns_md5_checksum_to_ascii(checksum_buf_bin, checksum_buf);

  NSDL2_API(NULL, NULL, "Checksum = %s", checksum_buf);

  return 0;
}

// Validate the response body by checking checksum coming in cookie whose name is passed in cookie_name
// It calculates checksum of body and then compares with the checksum coming in cookie.
// Return:
//   Note - If var_name is passed (i.e. not NULL), then it's value is also set as shown below return value
//   0 - Check pass
//      CheckSumStatus: pass
//   -1 - Checksum fail
//      UrlBodyCheckStatus: Fail - Checksum mismatch
//   -2 - Cookie value is not set by server
//      UrlBodyCheckStatus: Fail - CookieNotFound
//
// Note: This method MUST be called from Post URL Callback method

int ns_validate_response_checksum(char *cookie_name, char *var_name) 
{
char *resp_body_ptr;
int resp_body_size;
char checksum_buf[2 * NS_MD5_CHECKSUM_BYTES + 1];

  NSDL2_API(NULL, NULL, "Method Called: cookie_name = %s, var_name = %s", cookie_name, (var_name == NULL)?"NULL":var_name);

  resp_body_ptr = ns_url_get_body_msg(&resp_body_size);

  NSDL2_API(NULL, NULL, "Body repsonse size = %d", resp_body_size);

  ns_gen_md5_checksum((unsigned char *)resp_body_ptr, resp_body_size, (unsigned char *)checksum_buf);

  char *cav_body_check_sum = ns_get_cookie_val_ex(cookie_name, NULL, NULL);

  if(cav_body_check_sum == NULL)
  {
    NSDL2_API(NULL, NULL, "Error - Cookie %s is not present in response", cookie_name);
    if(var_name != NULL)
      ns_save_string("UrlBodyCheckStatus: Fail - CookieNotFound", var_name);
    return -2;
  }
   
  NSDL2_API(NULL, NULL, "CavBodyCheckSum = %s", cav_body_check_sum);
    
  if (strcmp(cav_body_check_sum, checksum_buf) !=0 )
  {
    NSDL2_API(NULL, NULL, "CheckSumStatus: Fail. Checksum from cookie = %s, checksum of body = %s", checksum_buf, cav_body_check_sum);
    if(var_name != NULL)
      ns_save_string("UrlBodyCheckStatus: Fail - Checksum mismatch", var_name);
    return -1;
  }
  
  NSDL2_API(NULL, NULL, "CheckSumStatus: pass");
  if(var_name != NULL)
    ns_save_string("UrlBodyCheckStatus: Pass", var_name);

  return 0;
}

/*Added API to  enable page dump for particular session
 * */
int ns_trace_log_current_sess()
{
 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");


  return (trace_log_current_sess(vptr));

}

/**********************************************************************************
* Purpose    : This function is used to search a string between 
               given LB and RB.

Input        : in_type - input type from which string will be searched 
               it can be NS_ARG_IS_PARAM or NS_ARG_IS_buf
               
               in - input buffer or input parameter from which string is searched
               if in_type is NS_ARG_IS_PARAM then in should be any parameter(search
               parameter, declare parameter etc.) and if in_type is NS_ARG_IS_BUF
               then in should be a buffer.
           
               out_type - output type in which searched string is stored, it can 
               be NS_ARG_IS_PARAM or NS_ARG_IS_buf.
        
               out - output buffer or output parameter in which searched string is stored
               if out_type is NS_ARG_IS_PARAM then out should be any parameter(search
               parameter, declare parameter etc.) and if out_type is NS_ARG_IS_BUF
               then out should be a buffer.
              
               lb- This is the left bound of the string.
               
               rb- This is the right bound of the string.
          
               ord- it can be NS_ORD_ALL, NS_ORD_ANY or any integer value.
        
               start_offset - This is the start offset of the string.
      
               save_len - This is the length of the string which we want to store
                          in out
**********************************************************************************/

int ns_save_searched_string(int in_type, char *in, int out_type, char *out, char *lb, char *rb, int  ord, int start_offset, int save_len)
{
  char *buf_ptr = NULL, *out_buf_ptr = NULL;
  char in_param_name[128 + 1];
  int save_count = -1;

  in_param_name[0] = 0;

 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called");


  if(in_type == NS_ARG_IS_BUF) //If buffer is given
  {
    buf_ptr = in;
  }
  else if(in_type == NS_ARG_IS_PARAM)  //If parameter is given 
  {
    snprintf(in_param_name, 128, "{%s}", in);
    buf_ptr = ns_eval_string(in_param_name);
  }
  else
  {
    fprintf(stderr, "Error: ns_save_searched_string(): Input type must be either NS_ARG_IS_BUF or NS_ARG_IS_PARAM.");
    return NS_STRING_API_ERROR; 
  } 

  //Validation  
  if(buf_ptr == NULL)
  {
    fprintf(stderr, "Error: ns_save_searched_string(): Input buffer is empty. Input buffer should not be empty.\n");
    return NS_STRING_API_ERROR;
  }
 
  // Checking the out type in case of ORD=ALL
  if(ord == NS_ORD_ALL )
   {
     if(out_type == NS_ARG_IS_BUF)
     {
       fprintf(stderr, "ERROR: Only nsl_decl_array type variable is used to store the value in case of ORD=ALL.\n ");
       return NS_STRING_API_ERROR; 
     }
     else
     {
       int var_hashcode = vptr->sess_ptr->var_hash_func(out, strlen(out));
       VarTransTableEntry_Shr* var_ptr;
       if (var_hashcode != -1)
       {
         var_ptr = &vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode];

         if(!(vptr->sess_ptr->var_type_table_shr_mem[var_ptr->user_var_table_idx]) )
         {
           fprintf(stderr, "Only nsl_decl_array type variable is used to store the value in case of ORD=ALL ");
           return NS_STRING_API_ERROR; 
         }
      }
    }
  }

  if((out_buf_ptr = nslb_save_searched_string(buf_ptr, lb, rb, ord, start_offset, save_len, &save_count)) == NULL)
    return NS_STRING_API_ERROR;
  
  NSDL2_API(NULL, NULL, "out_buf_ptr = [%s]", out_buf_ptr);

  //Ouput
  if(out_type == NS_ARG_IS_BUF)
  {
    strcpy(out, out_buf_ptr);
  }
  else if(out_type == NS_ARG_IS_PARAM)
  {
    if(ord == NS_ORD_ALL )
    {
      int i;
      char var_name[128]; // to save index variable name like: var_name_1, var_name_2...

      for(i = 0; i < save_count; i++)
      {
        sprintf(var_name, "%s_%d", out, i + 1); 
        ns_save_string_flag_internal(tempArrayVal[i].value, tempArrayVal[i].length, var_name, 0, vptr, 1);
        tempArrayVal[i].value = NULL;
        tempArrayVal[i].length = 0;
      }
      g_total_temparrayval_entries = 0;
      return save_count; 
    }
    else
      ns_save_string(out_buf_ptr, out); 
  }
  else
  {
    fprintf(stderr, "Error: ns_save_searched_string(): Output type must be either NS_ARG_IS_BUF or NS_ARG_IS_PARAM.");
  }

  //Free the value which have been malloced in nslb_save_searched_string()
  if(out_buf_ptr != NULL)
  {
    free(out_buf_ptr);
    out_buf_ptr = NULL;
  }
 
  if(save_count == 0)
    return -1;  //Not found
  if(save_count == 0 && ord != NS_ORD_ANY)
    return -2;  //Not found in particular ord case
  else if(!strcmp(out, "")) 
    return 0;   //Found but empty
   
  return save_count; //Return number of occurence
}

/*This is to get 8 byte unique number*/
// 4 bytes times since Jan 1, 2012
// 1 Byte - NVM Id
// 3 Bytes - Counter

/* Second from epoch till 1 Jan 2012 */

unsigned long long ns_get_unique_number()
{
  long long out = 0;
  static int count = 1;
  int nvm_id ;
  long long time_since_epoch;
  int time_since_1_jan_2012;

  NSDL1_API(NULL, NULL, "Method called");
  time_since_epoch = time(NULL);
  time_since_1_jan_2012 = (int) (time_since_epoch - TIMESTAMP_2012);
  nvm_id = my_child_index + 1;
  out = (((long long) time_since_1_jan_2012) << 32) + (nvm_id << 24) + (count  & 0xffffff);

  count++;
  if (count >= 0x1000000)
    count = 0;

  return out;
}

int des_decrypt(char *in, int in_len, char *out, int out_len, char *key);
void remove_padding(char *in);
int triplete_decode_bytes(char *str, char *return_str, int *out_len);


/* This api will decrypt the input using 3des ecb mode
 * Input: It will take input key(24 bytes), input string, output buffet, length of output buffer
 * Output: On success 0, on error -1
 * Input Example: gNW-iZj-54c-EmM-xB0-I2i-CMi-JD6
 * Output : arcite
*/

int ns_decode_3des(char *key, char *in, char *out, int out_len, int mode){

  int len;
  char decoded_string[10*1024];

  NSDL1_API(NULL, NULL, "Method called");

  if((triplete_decode_bytes(in, decoded_string, &len) == -1))
    return -1; 

  if ((des_decrypt(decoded_string, len, out, out_len, key) == -1))
    return -1;

  remove_padding(out);

  return 0;
}
/******************************************************************
This API remove all cookies for a particular Vuser

It will work when AUTO_COOKIE is enabled.

*******************************************************************/

int ns_cleanup_cookies()
{
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL1_COOKIES(NULL, NULL, "Method called.");
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_cleanup_cookies() cannot be used if Auto Cookie Mode is disabled\n");
    return -1;
  } 
  //Call to clean all cookies stored for particular virtual user
  free_cookie_value(vptr);

  return 0;
}

/******************************************************************
This API removed specified cookie (if user wants to remove a cookie
with their cookie name or path or domain)

It will work when AUTO_COOKIE is enabled.

Example:
ns_remove_cookie("name2", NULL, NULL)
or
ns_remove_cookie(NULL, "/tours/index.html", NULL)
*******************************************************************/
int ns_remove_cookie(char *name, char *path, char *domain, int free_for_next_req)
{
  int ret;
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL1_COOKIES(NULL, NULL, "Method called.");
  
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_remove_cookie() cannot be used if Auto Cookie Mode is disabled\n");
    return -1;
  }
  find_and_remove_specific_cookie(vptr, name, path, domain, free_for_next_req);

  //Resolved Bug: 23320 Need enhancement to remove cookie in RBU testing
  if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) && 
     (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == 1))
  {
    ret = ns_rbu_remove_cookies(vptr, name, path, domain, free_for_next_req);
    if(ret == -1)
    {
      rbu_fill_page_status(vptr, NULL);
      ns_rbu_log_access_log(vptr, NULL, RBU_ACC_LOG_DUMP_LOG);
      set_rbu_page_status_data_avgtime_data(vptr, NULL);
    }
    return -1;
  }
  return 0;
}

/* API used to get machine type i.e. NDE, NDAppliance, NSAppliance */
char * ns_get_machine_type()
{
  return((char *)global_settings->event_generating_host);
}

/*******************************************************************
 API used for adding header in request:
 syntex:
   ns_web_add_header(header_name, content, flag);

 This API is used like:
   ns_web_add_header("connection", "close", 0);
 
 flag 0 -> added header for main URL
      1 -> added header for Inline URL
      2 -> added header for ALL
********************************************************************/
int ns_web_add_header(char *header, char *content, int flag)
{

  VUser *vptr = TLS_GET_VPTR();
  
  NSDL1_API(NULL, NULL, "Method called");

  if(header == NULL || content == NULL)
  {
    fprintf(stderr, "Error: ns_web_add_header() arguments should not be null.\n");
    return -1;
  }
  if(flag < 0 || flag > 2)
  {
    fprintf(stderr, "Error: Invalid ns_web_add_header() flag passed. It should be either 0, 1 or 2\n");
    return -1;
  }
  ns_web_add_hdr_data(vptr, header, content, flag);

  return 0;
}

/******************************************************************
Syntex:
  ns_web_add_auto_header(header, content, flag);

Example:
  ns_web_add_auto_header("connection", "close", 0);
  
  flag 0 -> added header for main URL
       1 -> added header for Inline URL
       2 -> added header for ALL
******************************************************************/
int ns_web_add_auto_header(char *header, char *content, int flag)
{
  NSDL1_API(NULL, NULL, "Method called");
  
  VUser *vptr = TLS_GET_VPTR();
  

  if(header == NULL || content == NULL)
  {
    fprintf(stderr, "Error: ns_web_add_auto_hdr() arguments should not be null.\n");
    return -1;
  }
 
  if(flag < 0 || flag > 2)
  {
    fprintf(stderr, "Error: Invalid ns_web_add_auto_hdr() flag passed. It should be either 0, 1 or 2\n");
    return -1;
  }
  //TODO: Need to check for NS Variable
  ns_web_add_auto_header_data(vptr, header, content, flag);

  return 0;
}

/****************************************************************
This API remove the header which is added by auto header
Syntex:
  ns_web_remove_auto_header(char *header, int flag)

Example:
  ns_web_remove_auto_header("connection", "close", 0)
  
  flag 0 -> remove header from main URL
       1 -> remove header from Inline URL
       2 -> remove header from ALL
*****************************************************************/
int ns_web_remove_auto_header(char *header, int flag)
{
  NSDL1_API(NULL, NULL, "Method called");
 
  VUser *vptr = TLS_GET_VPTR();
  

  if(header == NULL)
  {
    fprintf(stderr, "Error: ns_web_remove_auto_hdr() arguments should not be null.\n");
    return -1; 
  }
  if(flag < 0 || flag > 2)
  {
    fprintf(stderr, "Error: Invalid ns_web_add_auto_hdr() flag passed. It should be either 0, 1 or 2\n");
    return -1;
  }

  ns_web_remove_auto_header_data(vptr, header, flag, cur_cptr);

  return 0;
}

/****************************************************************
This API remove all auto header from URL(Main, Embedded, both)

Syntex:
  ns_web_cleanup_auto_headers()
*****************************************************************/
int ns_web_cleanup_auto_headers()
{
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL1_API(NULL, NULL, "Method called");
  delete_all_auto_header(vptr);

  return 0;
}

unsigned char *ns_hmac(unsigned char *message , int msg_len, unsigned char *key, size_t key_len, char *md_type, int encode_mode)
{
  NSDL2_API(NULL, NULL, "Method called, message =[%s] key=[%s]", message, key);
  char *b64text;
  static unsigned char out_buf[64+1];
  unsigned char md_value[EVP_MAX_MD_SIZE];
  static unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int md_len, i ;
  int BLOCKSIZE = 64;
  unsigned char i_key_pad[65]= {0};
  unsigned char o_key_pad[65]= {0};
  char key1[key_len + 65];
  const EVP_MD *md;
  
  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  #else
    EVP_MD_CTX mdctx;
  #endif
  
  if(message == NULL || key == NULL){
    fprintf(stderr,"Unable to generate HMAC. Message/Key is empty \n");
    return NULL;
  }
  if(encode_mode < 0 || encode_mode > 2)
  {
    fprintf(stderr,"Encoded mode must be lie between 0 to 2 \n");
    return NULL;
  }


  memcpy(key1, key, key_len);
  OpenSSL_add_all_digests(); 

  md = EVP_get_digestbyname(md_type);
  if(!md) {
    fprintf(stderr,"Error:Unknown message digest \n");
    return NULL;
  }

  if (key_len > BLOCKSIZE)
  {
    unsigned char mdt_value[EVP_MAX_MD_SIZE];
   
   #if OPENSSL_VERSION_NUMBER >= 0x10100000L
     EVP_DigestInit_ex(mdctx, md, NULL);
     EVP_DigestUpdate(mdctx, key1, key_len);
     EVP_DigestFinal(mdctx, mdt_value, &md_len);
   #else
     EVP_MD_CTX_init(&mdctx);
     EVP_DigestInit_ex(&mdctx, md, NULL);
     EVP_DigestUpdate(&mdctx, key1, key_len);
     EVP_DigestFinal_ex(&mdctx, mdt_value, &md_len);
     EVP_MD_CTX_cleanup(&mdctx);
   #endif

    memcpy(key1, mdt_value, md_len);
    key_len = md_len;
  }
  else
  {
    for (i = key_len; i < BLOCKSIZE; i++)
      key1[i] = 0;  // keys shorter than blocksize are zero-padded 
  }

  memcpy( i_key_pad, key1, key_len);
  memcpy( o_key_pad, key1, key_len);

  for (i =0; i< BLOCKSIZE ; i++)
  {
    i_key_pad[i] ^= 0x36 ;
    o_key_pad[i] ^= 0x5c ;
  }

  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, i_key_pad, BLOCKSIZE);
    EVP_DigestUpdate(mdctx, message, msg_len);
    EVP_DigestFinal(mdctx, md_value, &md_len);

    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, o_key_pad, BLOCKSIZE);
    EVP_DigestUpdate(mdctx, md_value, md_len);
    EVP_DigestFinal(mdctx, digest, &md_len);
    EVP_MD_CTX_free(mdctx);
  
    /*cleanup*/
    NSDL2_API(NULL, NULL, "cleanup*******");
    CRYPTO_cleanup_all_ex_data();
  #else

    EVP_MD_CTX_init(&mdctx);
    EVP_DigestInit_ex(&mdctx, md, NULL);
    EVP_DigestUpdate(&mdctx, i_key_pad, BLOCKSIZE);
    EVP_DigestUpdate(&mdctx, message, msg_len);
    EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
    EVP_MD_CTX_cleanup(&mdctx);

    EVP_MD_CTX_init(&mdctx);
    EVP_DigestInit_ex(&mdctx, md, NULL);
    EVP_DigestUpdate(&mdctx, o_key_pad, BLOCKSIZE);
    EVP_DigestUpdate(&mdctx, md_value, md_len);
    EVP_DigestFinal(&mdctx, digest, &md_len);
    EVP_MD_CTX_cleanup(&mdctx);
    /*cleanup*/
    EVP_cleanup();
    RAND_cleanup();
    ERR_free_strings();
    ERR_remove_state(0);
    CRYPTO_cleanup_all_ex_data();
  #endif  

  if(encode_mode == 1){
    Base64Encode(digest, md_len, &b64text);
    NSDL2_API(NULL, NULL, "Encoded_data =[%s]", b64text);
    strcpy((char *)out_buf, b64text);
    FREE_AND_MAKE_NULL(b64text, "encoded hmac buffer", -1);
    return(out_buf);
  }
  else if(encode_mode == 2){
    for(i=0; i<md_len; i++)
      sprintf((char *)(&out_buf[i*2]), "%02x", digest[i]);

    out_buf[2*md_len]='\0';
    NSDL2_API(NULL, NULL, "Encoded_data = [%s], md_len = [%d]", out_buf, md_len);
    return(out_buf);
  }

 return(digest);
}

static char *algorithm = "AWS4-HMAC-SHA256";
static int my_hash256(void* data, unsigned long length, unsigned char* payloadHash)
{
    unsigned char myHash[SHA256_DIGEST_LENGTH];
    int i;
    SHA256_CTX context;

    if(!SHA256_Init(&context))
        return 1;

    if(!SHA256_Update(&context, (unsigned char*)data, length))
        return 1;

    if(!SHA256_Final(myHash, &context))
        return 1;

    for(i=0; i<SHA256_DIGEST_LENGTH; i++)
      sprintf((char*)(payloadHash + (i*2)), "%02x", myHash[i]);

    return 0;
}

int buildStringToSign(char *amzDate, char *credentialScope, char *canonicalRequest, char *stringToSign)
{
  unsigned char canonicalRequest_hash256[64+1] = {0};
  my_hash256(canonicalRequest, strlen(canonicalRequest), canonicalRequest_hash256);
  sprintf(stringToSign, "%s\n%s\n%s\n%s", algorithm, amzDate, credentialScope, canonicalRequest_hash256);
  return 0;
}


int buildCredentialScope(char *dateStamp, char *region, char *service, char *requestType, char *credentialScope)
{
  sprintf(credentialScope, "%s/%s/%s/%s", dateStamp, region, service, requestType);
  return 0;
}


int buildCanonicalRequest(char *method, char *canonicalUri, char *canonicalQueryString, char *canonicalHeaders, char *signedHeaders, 
                          unsigned char *payloadHash, char *canonicalRequest)
{
  sprintf(canonicalRequest, "%s\n%c%s\n%s\n%s\n%s\n%s", method, '/', canonicalUri, canonicalQueryString,
          canonicalHeaders, signedHeaders, payloadHash);
  return 0;
}

int get_path_service_query(char *url, char *canonicalUri, char *uri_service, char *canonicalQueryString)
{
  char *service = NULL;
  char *host = "";
  char *path = "";
  char *query = "";

  if(!url)
    return 1;

  if((service = strstr(url, "//")) != NULL)
    service += 2;
  else
    service = url;

  if ((host = strchr(service, '.')) != NULL)
  {
    *host = '\0';
    host ++;
  }
  else
  {
    return 1;
  }

  if ((path = strchr(host, '/')) != NULL){
      *path = '\0';
      path ++;
  }
  else
  {
    return 1;
  }


  if((query = strchr(path, '?')) != NULL)
  {
    *query = '\0';
     query++;
  }
  else
  {
    query = "";
  }
  strcpy(canonicalUri, path);
  strcpy(canonicalQueryString, query);
  strcpy(uri_service, service);
  NSDL2_API(NULL, NULL, "path =[%s] query =[%s] service = [%s] host = [%s]", path, query, service, host);

  return 0;
}

/* ns_aws_signature API takes 13 arguments named as method(GET or POST),
   uri which user wants to hit, headers, body, region, key, ksecret
   signature which is calculated, sku_value, zipcode, amzDate(it is calculated by this API in the GMT format),
   dateStamp(it is the system date & calculated by this API ) and vendor_code.

*/
int ns_aws_signature(char *method, char *uri, char *canonicalHeaders, char *signedHeaders, char *body, char *region, char *service, char *key, char *ksecret, char *amzDate, char *dateStamp, char *signature)
{
  unsigned char *kDate;
  unsigned char *kRegion; 
  unsigned char *kService;
  unsigned char *kSigning; //stores final output
  char canonicalUri[256 + 1] = {0};
  char lol_service[256 + 1] = {0};
  char canonicalQueryString[256 + 1] = "";
  char canonicalRequest[1024 + 1] = {0};
  char credentialScope[256 + 1] = {0};
  unsigned char payloadHash[64 + 1] = {0};
  char stringToSign[1024 + 1] = {0};
  unsigned char *signature_hmac = {0};
  struct tm *tmp;
  time_t t;
  int i;
  char lol_uri[512+1]; 
  char lol_ksecret[64+1] = "AWS4";

  if(!method || !uri || !body || !region || !key || !ksecret || !dateStamp || !canonicalHeaders || !signedHeaders)
    return -1;
  if (!service || !service[0])
    service = lol_service;
  strcpy(lol_uri, uri);
  strcat(lol_ksecret, ksecret);
  get_path_service_query(lol_uri, canonicalUri, lol_service, canonicalQueryString);

  my_hash256(body, strlen(body), payloadHash);
  //nslb_evp_digest_ex(body, strlen(body), DIGEST_SHA256, payloadHash, 2);
  buildCanonicalRequest(method, canonicalUri, canonicalQueryString, canonicalHeaders, signedHeaders, payloadHash, canonicalRequest);
  buildCredentialScope(dateStamp, region, service, "aws4_request", credentialScope);
  buildStringToSign(amzDate, credentialScope, canonicalRequest, stringToSign);

  kDate = ns_hmac((unsigned char*)dateStamp, strlen(dateStamp), (unsigned char*)lol_ksecret, strlen(lol_ksecret), MD_TYPE_SHA256 , 0); //returns hac in kdate
  kRegion =  ns_hmac((unsigned char *)region, strlen(region), (unsigned char *)kDate, 32, MD_TYPE_SHA256, 0); //returns hmac in kRegion
  kService = ns_hmac((unsigned char *)service, strlen(service), (unsigned char *)kRegion, 32, MD_TYPE_SHA256, 0); // returns hmac in kService
  kSigning =  ns_hmac((unsigned char *)"aws4_request", 12, (unsigned char *)kService, 32, MD_TYPE_SHA256, 0);//returns hmac in kSigning
  signature_hmac =  ns_hmac((unsigned char *)stringToSign, strlen(stringToSign), 
                              (unsigned char *)kSigning, 32, MD_TYPE_SHA256, 2);//returns hmac in kSigning
  strcpy(signature, signature_hmac);

  NSDL2_API(NULL, NULL, "signature_hmac [%s]", signature); 
return 0;
}


int ns_get_hmac_signature(char *method, char *uri, char *headers, char *body, char *region, char *key, char *ksecret, char *signature, char *sku_value, char *zipcode, char *amzDate, char *dateStamp, char *vendor_code)
{
  unsigned char *kDate;
  unsigned char *kRegion; 
  unsigned char *kService;
  unsigned char *kSigning; //stores final output
  char canonicalUri[256 + 1] = {0};
  char service[256 + 1] = {0};
  char canonicalQueryString[256 + 1] = "";
  char canonicalHeaders[256 + 1] = {0};
  char signedHeaders[256 + 1] = {0};
  char canonicalRequest[256 + 1] = {0};
  char credentialScope[256 + 1] = {0};
  unsigned char payloadHash[64 + 1] = {0};
  char stringToSign[1024 + 1] = {0};
  unsigned char *signature_hmac = {0};
  struct tm *tmp;
  time_t t;
  int i;
   
  
  NSDL2_API(NULL, NULL, "Method Called");
  if(body == NULL){
    printf("Give error\n");
    return 1;
  }

  //STEP 1:  Define all of your request requirements - HTTP method, URL/URI, request body, etc.

  //STEP 2: Create a date for headers and the credential string.
  t = time(NULL);
  tmp = gmtime(&t);
  strftime(amzDate, 256, "%Y%m%dT%H%M%SZ", tmp);
  strftime(dateStamp, 256, "%Y%m%d", tmp);

  //Step 3: based on request requirement construct a canonical URI based on the URI path.

  //Step 4: based on the request requirement construct the canonical query string.  This is derived from the URI object provided  

  char new_uri[256+1];
 // sprintf(new_uri, "%s%s%c%s", uri, value, '/', zipcode );
  if(!strcmp(uri, "https://qa.wedeliver.io/v1/availability/")){
    sprintf(new_uri, "%s%s%c%s", uri, sku_value, '/', zipcode );
    uri = new_uri;
    NSDL2_API(NULL, NULL, "new_uri [%s]", new_uri);
  }
  else if( !strcmp(uri,"https://qa.wedeliver.io/v1/calendar/dates/")){
    sprintf(new_uri, "%s%s%c%s", uri, zipcode, '/', vendor_code );
    uri = new_uri;
    NSDL2_API(NULL, NULL, "new_uri [%s]", new_uri);
  }
  else if(!strcmp(uri, "https://qa.wedeliver.io/v1/zipcode/")){
    sprintf(new_uri, "%s%s", uri, zipcode );
    uri = new_uri;
    NSDL2_API(NULL, NULL, "new_uri [%s]", new_uri);
  }
  else if(!strcmp(uri,"https://qa.wedeliver.io/v1/services/")){
    sprintf(new_uri, "%s%s%c%s", uri, sku_value, '/', zipcode );
    uri = new_uri;
    NSDL2_API(NULL, NULL, "new_uri [%s]", new_uri);
  }
  else if(!strcmp(uri,"https://qa.wedeliver.io/v1/deliveryorder")){
    sprintf(new_uri, "%s", uri );
    uri = new_uri;
    NSDL2_API(NULL, NULL, "new_uri [%s]", new_uri);
  }
  
  get_path_service_query(uri, canonicalUri, service, canonicalQueryString);

  //Step 5: Create the canonical headers and signed headers. Header names and value must be trimmed and lowercase, and sorted in ASCII order.
  //Note that there is a trailing \n.
  // processHeaders(headers, canonicalHeaders);
  sprintf(canonicalHeaders, "%s:%s\n", "x-amz-date", amzDate);

  /**
   * Step 6: Create the list of signed headers. This lists the headers in the canonical_headers list, delimited with ";" and in alpha order.
   * Note: The request can include any headers; canonical_headers and signed_headers lists those that you want to be included in the hash
           of the request. 
   * x-amz-date" is always required.
   */
  // processSignedHeaders(headers, signedHeaders);
  sprintf(signedHeaders, "%s", "x-amz-date");

  //Step 7: If the request contains a body - you need to sha-256 hash the payload.  For GET request it should be an empty string
  my_hash256(body, strlen(body), payloadHash);

  //Step 8: Combine elements to create create canonical request
  buildCanonicalRequest(method, canonicalUri, canonicalQueryString, canonicalHeaders, signedHeaders, payloadHash, canonicalRequest);
 
  //Step 9: Construct the credential scope and string to sign.
  buildCredentialScope(dateStamp, region, service, "aws4_request", credentialScope);
  buildStringToSign(amzDate, credentialScope, canonicalRequest, stringToSign);

  //Step 10: Produce the signing key

  kDate = ns_hmac((unsigned char*)dateStamp, strlen(dateStamp), (unsigned char*)ksecret, strlen(ksecret), MD_TYPE_SHA256 , 0); //returns hac in kdate
  kRegion =  ns_hmac((unsigned char *)"us-east-1", 9, (unsigned char *)kDate, 32, MD_TYPE_SHA256, 0); //returns hmac in kRegion
  kService = ns_hmac((unsigned char *)service, 2, (unsigned char *)kRegion, 32, MD_TYPE_SHA256, 0); // returns hmac in kService
  kSigning =  ns_hmac((unsigned char *)"aws4_request", 12, (unsigned char *)kService, 32, MD_TYPE_SHA256, 0);//returns hmac in kSigning
  signature_hmac =  ns_hmac((unsigned char *)stringToSign, strlen(stringToSign), 
                              (unsigned char *)kSigning, 32, MD_TYPE_SHA256, 0);//returns hmac in kSigning
  for(i = 0; i < 32; i++)
    sprintf(signature + (i * 2), "%02x", signature_hmac[i]);
  NSDL2_API(NULL, NULL, "signature_hmac [%s]", signature); 
return 0;
}

long ns_start_timer(char *start_timer_name)
{
  long cur_time;
  char start_time[64 + 1];
  char timer_name[1024];
  char *start_ptr;
  int amt_written = 0 ;

  NSDL2_API( NULL,NULL, "Method called");
 
  // Making variable of timer_ type to avoid Conflicts.
  // For example if start_timer_name = "checkout" than timer_name = timer_checkout 
  amt_written = snprintf(timer_name, 1024, "{timer_%s}", start_timer_name);
  timer_name[amt_written] = '\0';
  
  // Evaluating if timer already started.If start_ptr != -1 this means that timer is already started. 
  start_ptr = ns_eval_string(timer_name);

  NSDL2_API(NULL, NULL, "start_ptr (%s) ", start_ptr);
  if ((cur_time = atol(start_ptr)) != -1)
  {
    NSDL2_API(NULL, NULL,"timer already started at  [%d] for timer_name [%s] ", cur_time, start_timer_name );
    NS_EL_2_ATTR(EID_FOR_API,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, 
               (char*)__FUNCTION__,
               "Warning: Timer already started overriting its original value (%d)  with new timer value for start_timer (%s)",
                            cur_time, start_timer_name);
  }
 
 //Calculate current time stamp in milliseconds .This is the time when timer was started .
  cur_time = get_ms_stamp(); 
  NSDL2_API(NULL, NULL, "start timer is [%ld] ", cur_time);

  amt_written = snprintf(start_time, 64, "%ld", cur_time);
  start_time[amt_written] = '\0';

  amt_written = snprintf(timer_name, 1024, "timer_%s", start_timer_name);
  timer_name[amt_written] = '\0';

  //Saving timestamp in timer name
  ns_save_string(start_time, timer_name);

  return cur_time;
}

long ns_end_timer(char *end_timer_name)
{
  char *start_time;
  long elapsed_time;
  time_t end_time;
  long eval_time;
  char timer_name[1024];
  int amt_written = 0;
 
  NSDL2_API(NULL, NULL,"Method called end_timer_name = [%s]", end_timer_name);
 
  //Making variable of timer_ type to avoid Conflicts.
  // For example if start_timer_name = "checkout" than timer_name = timer_checkout 
  amt_written = snprintf(timer_name, 1024, "{timer_%s}", end_timer_name);
  timer_name[amt_written] = '\0';

 //evaluate time when timer was started 
  start_time = ns_eval_string(timer_name);
  NSDL2_API(NULL, NULL,"start_time is [%s] timer name=[%s] ", start_time, end_timer_name);
 
  //Handling error cases for end timer 
  //case 1: if timer was never started 
  if (!strcmp(start_time, timer_name))
  {
    NS_EL_2_ATTR(EID_FOR_API,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, 
               (char*)__FUNCTION__,
               "Warning: Timer not started for (%s). Therefore  Cannot end timer for (%s)", end_timer_name, end_timer_name);
    return -1;
  }

  //case 2:  where timer is ended twice 
  if ((eval_time = atol(start_time)) == -1) 
  {
    NS_EL_2_ATTR(EID_FOR_API,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__,
               (char*)__FUNCTION__,
               "Warning: Timer not started for (%s). Therefore  Cannot end timer for (%s)", end_timer_name, end_timer_name);
    return -1;
  }

  // Calculating time stamp for end timer in mailliseconds
  end_time = get_ms_stamp(); 

  // Calculate elapsed time  
  elapsed_time = end_time - atol(start_time);
 
  NSDL2_API(NULL, NULL, "timer start time is [%ld]  end time is [%ld]  elapsed time is [%ld]",atol(start_time), end_time, elapsed_time);
  amt_written = snprintf(timer_name, 1024, "timer_%s", end_timer_name);
  timer_name[amt_written] = '\0';

  // Reset timer value to -1, will be used to check whether a timer is ended more then one
  ns_save_string("-1", timer_name); 

  return elapsed_time; 
}

long long ns_get_cur_partition() {

return g_partition_idx;
}

int ns_get_host_name()
{
  PerHostSvrTableEntry_Shr *cur_svr_entry; // Pointer to severentry table 
  int cur_host_id = -1; // Points to host id

  VUser *vptr = TLS_GET_VPTR();
  
  // Check if svr_ptr is not NULL
  if (vptr->last_cptr->url_num->index.svr_ptr != NULL) { 
    // Getting current server entry  
    cur_svr_entry = get_svr_entry(vptr, vptr->last_cptr->url_num->index.svr_ptr);
  if (cur_svr_entry == NULL)
    return -1;
  }

  // Getting host id here 
  cur_host_id = cur_svr_entry->host_id;
  NSDL2_API(NULL, NULL, "host id is [%d] \n ", cur_host_id);
 
  return cur_host_id;
}

char *ns_get_referer()
{
  //This returns referer and will return null in case  referer is not set 

  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "referer is [%s]", vptr->referer);

  return(vptr->referer);
}


int ns_set_referer(char* referer)
{
  if(referer == NULL)
   return -1;

  VUser *vptr = TLS_GET_VPTR();
  

  int len = strlen(referer);
  if(len)
  {
    if(vptr->referer_buf_size < len)
    {
      MY_REALLOC(vptr->referer, len + 1, "vptr->referer", -1);
      vptr->referer_buf_size = len;
     }
     strcpy(vptr->referer, referer);
  }
  vptr->referer_size = len;
  NSDL2_API(vptr, NULL, "referer length is [%d], referer is [%s]", len, vptr->referer);
 
  return 0;
}

int ns_stop_inline_urls()
{

  NSDL4_VARS(NULL, NULL, "Method called ");

  VUser *vptr = TLS_GET_VPTR();
  

  vptr->urls_awaited  -= vptr->urls_left ;
  vptr->urls_left = 0;

  //Removing all the scheduled url from inuse list if url status is free
  connection *cptr = vptr->head_cinuse;
 while (cptr)
 {
   NSDL2_API(vptr, NULL, "ns_stop_inline_urls - Check if cptr has scheduled inline URL. urls awaited = %d, urls left = %d, timer type=%d, connection state = %d, url = %s", vptr->urls_awaited, vptr->urls_left, cptr->timer_ptr->timer_type, cptr->conn_state, cptr->url);
   if ((cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) && ((cptr->conn_state == CNST_FREE) || (cptr->conn_state == CNST_REUSE_CON )))
   {
     NSDL2_API(vptr, NULL, "Removing timer");
     dis_timer_del(cptr->timer_ptr);
     vptr->urls_awaited--;
   }
   cptr = (connection *)cptr->next_inuse;
   }
   NSDL2_API(vptr, NULL, "urls awaited = %d urls left = %d",vptr->urls_awaited, vptr->urls_left);
 
 return 0;
}

int ns_encode_base64_binary(const void* data_buf, size_t dataLength, char* result, size_t resultSize)
{
  NSDL2_API(NULL, NULL, "Method Called");
  return nslb_binary_base64encode(data_buf,dataLength,result,resultSize);
}

/*API for SOAP WS Security*/
int ns_soap_ws_security(char *keyFile, char *certFile, int algorithm, char* token, char* digest_id, char* cert_id, char* sign_id, char* key_info_id, char* sec_token_id)
{
  VUser *vptr = TLS_GET_VPTR();
  

  nsSoapWSSecurityInfo *ns_ws_info = vptr->ns_ws_info; 
  NSDL2_API(vptr, NULL, "Method Called");
  if(!ns_ws_info)
  {
    NSDL2_API(vptr, NULL, "ns_add_soap_ws_security");
    ns_ws_info = ns_add_soap_ws_security(keyFile, certFile, algorithm, token, digest_id, cert_id, sign_id, key_info_id, sec_token_id);
    if(!ns_ws_info)
    {
      //Failed to add ws data
      NSDL4_API(vptr, NULL, "Failed to add ws data");
      return -1;
    }
    strncpy(ns_ws_info->keyFile, keyFile, SOAP_FILE_NAME_SIZE);
    ns_ws_info->keyFile[SOAP_FILE_NAME_SIZE] = '\0';
    //save ns_ws_info on vptr's HTTPData & set vptr apply_ws_security flag
    vptr->ns_ws_info = ns_ws_info;
  }
  else
  {
    if(!strcmp(ns_ws_info->keyFile, keyFile))
    {
      keyFile = NULL;
      certFile = NULL;
    }
    NSDL4_API(vptr, NULL, "ns_update_soap_ws_security");
    if(ns_update_soap_ws_security(ns_ws_info, keyFile, certFile, algorithm, token, digest_id, cert_id, sign_id, key_info_id, sec_token_id) < 0)
    {
      //Failed to update ws data
      NSDL4_API(vptr, NULL, "Failed to update ws data");
      return -1;
    }
    if(keyFile)
    {
      NSDL4_API(vptr, NULL, "update key file");
      strncpy(ns_ws_info->keyFile, keyFile, SOAP_FILE_NAME_SIZE);
      ns_ws_info->keyFile[SOAP_FILE_NAME_SIZE] = '\0';
    }
  }
  vptr->flags |= NS_SOAP_WSSE_ENABLE; 
  return 0;
}

int ns_enable_soap_ws_security()
{
  NSDL2_API(NULL, NULL, "Method Called");
 
  VUser *vptr = TLS_GET_VPTR();
  

  vptr->flags |= NS_SOAP_WSSE_ENABLE; 

  return 0;
}

int ns_disable_soap_ws_security()
{
  NSDL2_API(NULL, NULL, "Method Called");
 
  VUser *vptr = TLS_GET_VPTR();
  

  vptr->flags &= ~NS_SOAP_WSSE_ENABLE; 

  return 0;
}

int ns_evp_digest(char *buffer , int len , int algo , unsigned char *hash)
{
  NSDL2_API(NULL, NULL, "Method Called");
  return nslb_evp_digest(buffer,len,algo,hash);
}

int ns_evp_sign(char *buffer , int len , int algo , char *privKey , unsigned char *signature, int sig_size)
{
  NSDL2_API(NULL, NULL, "Method Called");
  return nslb_evp_sign(buffer,len,algo,privKey,signature,sig_size);
}

int ns_string_split(char *buf, char *fields[], char *token, int max_flds)
{
  NSDL2_API(NULL, NULL, "Method Called");
  return get_tokens_ex2(buf, fields, token, max_flds);
}

// This function is used to select a query randomly and start a transaction for that query.
void ns_db_replay_query(int *idx, int *num_parameters, char *query_param_buf) {
  int query_idx, i;
  char *ptr = NULL;
  char param_name[128];
  int copy_length = 0;
  int copied_length = 0;
  char param_name_value_buf[35000] = {0};
  int param_name_value_copied_len = 0;
  int param_name_copy_len = 0;
  int param_value_copy_len = 0;
  
  NSDL2_API(NULL, NULL, "Method Called");
  
  if(ns_db_query_shr == NULL)
  { 
    fprintf(stderr, "Using API ns_db_replay_query without keyword REPLAY_FILE 11. Hence Exiting.\n");
    END_TEST_RUN 
  } 
 
  // randomly selected query_idx 
  query_idx = ns_get_query_index();
  NSDL2_API(NULL, NULL, "query_idx = %d", query_idx);
 
  ns_start_transaction(ns_db_query_shr[query_idx].trans_name);
  *idx = query_idx;
  *num_parameters = ns_db_query_shr[query_idx].num_parameters;

  for (i = 0; i < *num_parameters; i++)
  {
    sprintf(param_name, "{%s}", ns_db_query_shr[query_idx].query_parameters[i].param_name);

    // Used to call advance param API in case of date, random string, random int and file parameter, only in case if advance_param_flag is set
    if(ns_db_query_shr[query_idx].query_parameters[i].advance_param_flag == 1) {
      NSDL4_API(NULL, NULL, "calling ns_advance_param for VAR parameter = %s", ns_db_query_shr[query_idx].query_parameters[i].param_name);
      ns_advance_param(ns_db_query_shr[query_idx].query_parameters[i].param_name);
    }
    else { 
      NSDL4_API(NULL, NULL, "not calling ns_advance_param for VAR parameter %s", ns_db_query_shr[query_idx].query_parameters[i].param_name);
    }

    ptr =  ns_eval_string(param_name);
    copy_length = strlen(ptr);

    strcpy(query_param_buf + copied_length, ptr);
    copied_length += copy_length;
    strcpy(query_param_buf + copied_length, "\n");
    copied_length += 1;
    
    // to dump all the query parameter name=value pair in debug_trace.log file 
    if(debug_trace_log_value)
    {
      NSDL2_API(NULL, NULL, "Making query name value buff");
      param_name_copy_len = strlen(ns_db_query_shr[query_idx].query_parameters[i].param_name);
      param_value_copy_len = copy_length;

      // Adding 2 bytes for "=" and " "
      if((param_name_value_copied_len + param_name_copy_len + param_value_copy_len + 2) < 35000) 
      {
        strcpy(param_name_value_buf + param_name_value_copied_len, ns_db_query_shr[query_idx].query_parameters[i].param_name);
        param_name_value_copied_len += param_name_copy_len; 
 
        strcpy(param_name_value_buf + param_name_value_copied_len, "=");
        param_name_value_copied_len += 1; 
  
        strcpy(param_name_value_buf + param_name_value_copied_len, ptr);
        param_name_value_copied_len += param_value_copy_len;
  
        strcpy(param_name_value_buf + param_name_value_copied_len, " ");
        param_name_value_copied_len += 1; 
      }
      else {
        NSDL2_API(NULL, NULL, "Max size of param name value buffer reached[35k]. Hence truncating");
      }
    }
  }
  
  query_param_buf[copied_length] = '\0';
  param_name_value_buf[param_name_value_copied_len] = '\0';
  NSDL2_API(NULL, NULL, "idx = %d, num_parameters = %d, query_param_buf = %s, param_name_value_buf = %s", *idx, *num_parameters, query_param_buf,                              param_name_value_buf);

  NS_DT2(NULL, NULL, DM_L1, MM_API, "Going to execute query [%s] with query id %d, having parameter %s", ns_db_query_shr[query_idx].query, 
                                         query_idx, param_name_value_buf);
}

// This is used to end a transaction with status for db_replay mode 11
int ns_db_replay_query_end(int idx, int status, char *msg) {
  
  NSDL2_API(NULL, NULL, "Method Called, idx = %d, status = %d", idx, status);
  if(msg != NULL)
    NSDL2_API(NULL, NULL, "Method Called, msg = %s", msg);
  
  // To check if ns_db_replay_query_end API is used without using keyword REPLAY_FILE 11 
  if(ns_db_query_shr == NULL)
  { 
    NSDL2_API(NULL, NULL, "Using API ns_db_replay_query_end without keyword REPLAY_FILE 11. Hence Exiting.");
    END_TEST_RUN 
  }
  // Log event in case of query failure
  if(status != 0){
    NS_EL_2_ATTR(EID_FOR_API,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
               "Replay db query execution failed for query [%s] with query id %d. Query error status = %d, Error message = %s",
				 ns_db_query_shr[idx].query, idx, status, msg);
  } else {
    NS_DT2(NULL, NULL, DM_L1, MM_API, "Execution Success query with query id %d ", idx); 
  }

  // Status 0 will be considered as success
  // Status 1 will be considered as jnvm internal failure: for exmaple max connection reached, query index greater then total query count.
  // Status >1 will be considered as DB query failure
  // internal error is mapped to TX error code 93 and db error is Mapped to Tx error 94 
  if(status == 1) {
    status = 93; 
  } else if(status > 1) {
    status = 94; 
  }
  ns_end_transaction(ns_db_query_shr[idx].trans_name, status);
  return 0;
}

int ns_save_binary_val(const char* param_value, const char* param_name, int value_length)
{
  VUser *vptr = TLS_GET_VPTR();
  
  return(ns_save_string_flag_internal(param_value, value_length, param_name, 0, vptr, 0));
}


int ns_save_value_from_file(const char *file_name, const char* param_name)
{
  int fd = 0; 
  long ret, file_size = 0;
  struct stat buf;
  char *file_contents;
  char filename[4096];
  
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method called. File Name = %s, Param Name = %s", file_name, param_name);

  if(file_name[0] == '/') //check if absolute path given
    strcpy(filename, file_name);
  else //relative path
    sprintf(filename, "%s/%s", g_ns_wdir, file_name);

  NSDL2_API(vptr, NULL, "File name with path = %s", filename);

  //To get file size use stat
  if(stat(filename, &buf))
  {
    fprintf(stderr, "Unable to get file size of file '%s', Error = %s, function name = %s, line = %d, file = %s", 
                     filename, nslb_strerror(errno), (char *)__FUNCTION__, __LINE__, __FILE__);
    return -1;
  }
  NSDL2_API(vptr, NULL, "Size is %d of file %s", buf.st_size, filename);
  file_size = buf.st_size;
  // No contetnts in a file so write "" & return.
  if(file_size == 0) {
    char *empty_ptr = "";
    file_contents = empty_ptr;
    NSDL2_API(vptr, NULL, "Saving empty value in %s", param_name);
    ns_save_string_flag_internal(file_contents, file_size, param_name, 0, vptr, 1); 
    return 0;
  }

  fd = open(filename, O_RDONLY|O_CLOEXEC);
  if(fd <= 0)
  {
    fprintf(stderr, "Error in openning file %s. Error = %s, function name = %s, line = %d, file = %s", 
                     filename, nslb_strerror(errno), (char *)__FUNCTION__, __LINE__, __FILE__);
    return -1;
  }

  MY_MALLOC_NO_EXIT(file_contents, (file_size + 1), "file contents");

  ret = nslb_read_file_and_fill_buf(fd, file_contents, file_size);

  close(fd);
  if(ret > 0)
    ns_save_string_flag_internal(file_contents, file_size, param_name, 0, vptr, 1); 
  
  FREE_AND_MAKE_NULL(file_contents, NULL, "file contents");
  return 0;
}

int ns_eval_compress_param(char *in_param_name, char *out_param)
{
  char *string = NULL;
  int out_len;
  long len;
  char param_name_buff[1024];
  char err_msg[1024] = {0};

  IW_UNUSED(VUser *vptr = TLS_GET_VPTR());
  

  NSDL2_API(vptr, NULL, "Method called, param_name = %s", in_param_name);

  sprintf(param_name_buff, "{%s}", in_param_name);
  string = ns_eval_string_flag(param_name_buff, 0, &len);
  if(!string) {
    fprintf(stderr, "string is null, returning..");
    return -1;
  }
  NSDL2_API(vptr, NULL, "String = %s, len = %d", string, len); 

  string = ns_eval_string_flag(string, 0, &len);
  NSDL2_API(vptr, NULL, "String2 = %s, len = %d", string, len); 

  //Using g_tls buffer instead of scratch buffer for thread safe
  //nslb_comp_do(string, len, NS_COMP_GZIP, &g_tls.buffer, &g_tls.buffer_size, &out_len, 1024); 
  nslb_compress(string, len, &g_tls.buffer, (size_t *)&g_tls.buffer_size, (size_t *)&out_len, NSLB_COMP_GZIP, err_msg, 1024);
  ns_save_binary_val(g_tls.buffer, out_param, out_len); 

  NSDL2_API(vptr, NULL, "Method exit, g_tls.buffer = %s", g_tls.buffer);
  return 0; 
}

int ns_eval_decompress_param(char *in_param_name, char *out_param)
{
  char *string = NULL;
  char err_msg[1024] = {0};
  char param_name_buff[1024];
  long param_value_len;
  int uncomp_cur_len = 0;

  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method called, param_name = %s", in_param_name);

  sprintf(param_name_buff, "{%s}", in_param_name);
  string = ns_eval_string_flag(param_name_buff, 0, &param_value_len);
  if(!string) {
    fprintf(stderr, "string is null, returning..");
    return -1;
  } 
  //Using g_tls buffer instead of scratch buffer for thread safe
  //nslb_decomp_do_new(string, param_value_len, NS_COMP_GZIP, err_msg, &g_tls.buffer, &g_tls.buffer_size);
  nslb_decompress(string, param_value_len, &g_tls.buffer, (size_t *)&g_tls.buffer_size, (size_t *)&uncomp_cur_len, NSLB_COMP_GZIP,
          err_msg, 1024);
  ns_save_string_flag_internal(g_tls.buffer, g_tls.buffer_size, out_param, 0, vptr, 1); 
  NSDL2_API(vptr, NULL, "Method exit, uncomp_buff = %s", g_tls.buffer);
  return 0;
}

int update_user_flow_count(int id)
{ 
  VUser *vptr = TLS_GET_VPTR();
  
  vptr->runtime_runlogic_flow_id = id;
  update_user_flow_count_ex(vptr, id); 
  return 0;
}

void ns_save_command_output_in_param(char *param, char *command)
{
  char err_msg[1024] = {0};

  NSDL1_API(NULL, NULL, "Method called, param name = %s", param);

  if(nslb_run_and_get_cmd_output_ex(command, ns_nvm_scratch_buf_size, ns_nvm_scratch_buf, err_msg) != 0) {
    fprintf(stderr, "%s", err_msg);
    return;
  }
  NSDL2_API(NULL, NULL, "Output for ns_save_command_output_in_param() api  = %s", ns_nvm_scratch_buf);
  ns_save_string(ns_nvm_scratch_buf, param);
}


/* WebSocket APIs */

#define WS_SEARCH_BUF_LEN     4096
char *ns_web_websocket_search(char *mesg, char *lb, char *rb)
{
  static __thread char *ws_search_buf = NULL;
  static __thread int ws_search_buf_len = 0;
  int count = 0;
  int mesg_len = WS_SEARCH_BUF_LEN;
  char *msg_ptr = mesg;

  NSDL2_WS(NULL, NULL, "Method called, mesg = %p, ws_search_buf = %p, ws_search_buf_len = %d, lb = %p, rb = %p",
                            msg_ptr, ws_search_buf, ws_search_buf_len, lb, rb);

  /*****************************************************
   Allocate memory of size mesg_len
   If first time then allocate memory of size
   mesg_len or WS_SEARCH_BUF_LEN which one is greater
   ****************************************************/
  if(msg_ptr && msg_ptr[0])
  { 
    mesg_len = strlen(msg_ptr);
    if(!ws_search_buf_len && (mesg_len < WS_SEARCH_BUF_LEN) )
      mesg_len = WS_SEARCH_BUF_LEN;
  }
  else
  { 
    NSTL1(NULL, NULL, "Error: WebSocket search failed because input string is not provided.");
    return NULL;
  }

  if(ws_search_buf_len < mesg_len)
  { 
    MY_REALLOC(ws_search_buf, mesg_len + 1, "ws_search_buf_len", -1);
    ws_search_buf_len = mesg_len + 1;
    NSDL2_WS(NULL, NULL, "length of the message is larger than 4096, so reallocating , mesg_len = %d ", mesg_len);
  }
  else
  {
    NSDL2_WS(NULL, NULL, "Sufficent buffer exist, no need to re-allocate, ws_search_buf_len = %d", ws_search_buf_len);
    ws_search_buf[0] = 0;
  }

  count = ns_save_searched_string(NS_ARG_IS_BUF,msg_ptr,NS_ARG_IS_BUF,ws_search_buf,lb,rb,NS_ORD_ANY,0, ws_search_buf_len);
  NSDL2_WS(NULL, NULL, "count = %d", count);

  if(count < 0)
  {
    NSDL2_WS(NULL, NULL, "Searching Staus = Failed, Search Count = %d", count);
    return NULL;
  }
  else
  {
    NSDL2_WS(NULL, NULL, "Searching Status = Pass,  Search Count = %d, Searched String = [%s]", count, ws_search_buf);
  }

  return ws_search_buf;
}


#define STOP_ON_CP_FAIL 1
#define CONTINUE_ON_CP_FAIL 2
#define STOP_ON_CP_PASS 3
#define CONTINUE_ON_CP_PASS 4

int ns_web_websocket_check(char *mesg , char *check_value , int check_action)
{
  int check_status;
  VUser *vptr = TLS_GET_VPTR();
  
  if(mesg && check_value)
  {
    char *chk_ptr = strstr(mesg, check_value);
    if(!chk_ptr)
    {
      NSDL2_WS(vptr, NULL, "Checkpoint failed, chk_ptr = %s", chk_ptr);
      check_status = 0;
    }
    else
    {
      NSDL2_WS(vptr, NULL, "Checkpoint passed, chk_ptr = %s", chk_ptr);
      check_status = 1;
    }



    switch(check_action)
    {
      case STOP_ON_CP_FAIL:
        if(check_status == 0) 
        {
          NSDL2_WS(vptr, NULL, "stop on checkpoint fail, check_status = %d", check_status);
          vptr->ws_status = NS_REQUEST_CV_FAILURE;
          fprintf(stderr,"ENDING TEST BECAUSE CHECKPOINT FAILED\n");
          END_TEST_RUN 
        }
        break;
      case CONTINUE_ON_CP_FAIL:
        if(check_status == 0)
        {
          NSDL2_WS(vptr, NULL, "continue on checkpoint fail, check_status = %d", check_status);
          vptr->ws_status = NS_REQUEST_CV_FAILURE;
        }
        break;
      case STOP_ON_CP_PASS:
        if(check_status == 1)
        {
          NSDL2_WS(vptr, NULL, "stop on checkpoint pass, check_status = %d", check_status);
          vptr->ws_status = NS_REQUEST_ERRMISC;
          fprintf(stderr,"ENDING TEST BECAUSE CHECKPOINT PASSED\n");
          END_TEST_RUN
        }
        break;
      case CONTINUE_ON_CP_PASS:
        if(check_status == 1) 
        {
          NSDL2_WS(vptr, NULL, "continue on checkpoint pass, check_status = %d", check_status);
        }
        break;
      }
    }
    else
    {
      NSTL1(NULL, NULL, "Error: WebSocket checkpoint failed because input or output string is not provided.");
      vptr->ws_status = NS_REQUEST_ERRMISC;
      return -1;
    }  
  return 0;
}

char *ns_web_websocket_read(int con_id, int timeout, int *resp_sz)
{
  int ret = 0;
  *resp_sz = 0;
  char *resp = NULL;
  VUser *vptr = TLS_GET_VPTR();
  

  /* Initialize ws_status for read timer */
  vptr->ws_status = NS_REQUEST_OK;

  NSDL2_WS(vptr, NULL, "Method called, con_id = %d, timeout = %d, vptr = %p, ws_con id = %d", con_id, timeout, vptr, ws_idx_list[con_id]); 

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /*Non thread mode*/
  {
    ret = nsi_web_websocket_read(vptr, con_id, timeout);
    if(!ret)
      switch_to_nvm_ctx(vptr, "WebSocket Frame reading start");
  }
  else
  {
    NSDL2_API(NULL, NULL, "Method called(Thread Mode)");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_WEBSOCKET_READ_REQ;
    vptr->thdd_ptr->page_id = con_id;
    vptr->thdd_ptr->tx_name_size = timeout;   //Using tx_name_size for timeout
    ret = vutd_send_msg_to_nvm(NS_API_WEBSOCKET_READ_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
   
    if(!ret)
    {
      NSDL2_API(NULL, NULL, "WebSocket Frame reading start");
    }
  }

  NSDL2_WS(vptr, NULL, "cptr = %p, vptr->ws_status = %d", vptr->ws_cptr[ws_idx_list[con_id]], vptr->ws_status);
  ret = vptr->ws_status;
  if(!ret) //If timer is not expire
  {
    resp = vptr->url_resp_buff;
    //*resp_sz = vptr->ws_cptr[ws_idx_list[con_id]]->bytes; //cptr->bytes
    *resp_sz = vptr->bytes;
  }
 
  NSDL2_WS(vptr, NULL, "ret = %d, resp = %s, vptr->resp = %s", ret, url_resp_buff, vptr->url_resp_buff);
  if(ret && (vptr->ws_status != NS_REQUEST_TIMEOUT))
  {
    vptr->ws_status = NS_REQUEST_NO_READ;
    ws_avgtime->ws_error_codes[NS_REQUEST_NO_READ]++;
    NSDL2_WS(vptr, NULL, "ws_avgtime->ws_error_codes[%d] = %llu", vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_NO_READ]);
    INC_WS_MSG_READ_FAIL_COUNTER(vptr);   //Updated avg counter for failed msg
  }

  NSDL2_WS(vptr, NULL, "Method exit, resp_sz = %d", *resp_sz);

  return resp;
}
/*This API will decrypt the passed encrypted string*/
char *ns_decrypt(char *encrypt_string)
{
  return nslb_decrypt_passwd(encrypt_string);
}

char *ns_eval_string_copy (char *dest, char *str, int dest_len)
{
  if(dest && str)
  {
    ns_chk_strcpy(dest, ns_eval_string(str), dest_len);
    return dest;
  }
  else
  {
    NSTL1(NULL, NULL, "Error: ns_eval_string_copy() dest_string = %s, source_string = %s, dest_string_len = %d", dest, str, dest_len);
    fprintf(stderr, "Error: ns_eval_string_copy() dest_string = %s, source_string = %s, dest_string_len = %d", dest, str, dest_len);
    return NULL;
  }
}

/*---------------------------------------------------------------------------------------------------------------------------
 * Purpose   : This function will encrypt the data (either text or binary) 
 *
 * Input     : buffer		- Any buffer which needs to be encrypted. This can be parameterized     
 *             buffer_len	- length of input buffer
 *             encryption_algo	- Algo can be AES_128_CBC, AES_128_CTR, AES_192_CBC, AES_192_CTR, AES_256_CBC, AEC_256_CTR
 *             key		- key used to encrypted the buffer. This can be parameterized.
 *	       key_len		- length of key
 *	       ivec		- ivec used to encrypted the buffer. This can be parameterized.
 *	       ivec_len		- length of key
 *	       err_msg		- To dump the err_msg, if issue occurred while encypting the buffer.
 *
 * Output    : On success	- Return the length of encrypted output
 *             On Failure 	- -1  
 *
 * Build_V   : 4.1.12 #9	
 *---------------------------------------------------------------------------------------------------------------------------*/
unsigned char *ns_aes_encrypt(unsigned char *buffer, int buffer_len, int encryption_algo, char base64_encode_option, char *key , int key_len, char *ivec, int ivec_len, char **err_msg)
{
  NSDL2_HTTP(NULL, NULL, "Method called buffer = %s, buffer_len = %d, encryption_algo = %d, base64_encode_option = %d, "
                         "key = %s, key_len = %d, ivec = %s, ivec_len = %d", 
                          buffer, buffer_len, encryption_algo, base64_encode_option, key, key_len, ivec, ivec_len);

  return ns_aes_crypt(buffer, buffer_len, encryption_algo, base64_encode_option, key , key_len, ivec, ivec_len, 1, err_msg);
}

/*---------------------------------------------------------------------------------------------------------------------------
 * Purpose   : This function will decrypt the data (either text or binary) 
 *
 * Input     : buffer		- Any buffer which needs to be decrypted. This can be parameterized     
 *             buffer_len	- length of input buffer
 *             encryption_algo	- Algo can be AES_128_CBC, AES_128_CTR, AES_192_CBC, AES_192_CTR, AES_256_CBC, AEC_256_CTR
 *             key		- key used to decrypted the buffer. This can be parameterized.
 *	       key_len		- length of key
 *	       ivec		- ivec used to decrypted the buffer. This can be parameterized.
 *	       ivec_len		- length of key
 *	       err_msg		- To dump the err_msg, if issue occurred while encypting the buffer.
 *
 * Output    : On success	- Return the length of decrypted output
 *             On Failure 	- -1  
 *
 * Build_V   : 4.1.12 #9	
 *---------------------------------------------------------------------------------------------------------------------------*/
unsigned char *ns_aes_decrypt(unsigned char *encrypted_buffer, int encrypted_buffer_len, int decryption_algo, char base64_encode_option, char *key , int key_len, char *ivec, int ivec_len, char **err_msg)
{
  NSDL2_HTTP(NULL, NULL, "Method called encrypted_buffer = %s, encrypted_buffer_len = %d, decryption_algo = %d, base64_encode_option = %d, "
                         "key = %s, key_len = %d, ivec = %s, ivec_len = %d", 
                          encrypted_buffer, encrypted_buffer_len, decryption_algo, base64_encode_option, key, key_len, ivec, ivec_len);

  return ns_aes_crypt(encrypted_buffer, encrypted_buffer_len, decryption_algo, base64_encode_option, key , key_len, ivec, ivec_len, 0, err_msg);
}

static unsigned char *ns_encode_decode_base64(unsigned char *buffer, int buffer_len, char **err_msg, int encode)
{
  static __thread char out_err_msg[MAX_PARAM_SIZE + 1] = "";
  static __thread unsigned char *output_buffer = NULL;
  static __thread int output_buffer_size = 0;
  static __thread unsigned char *in_buffer = NULL;
  static __thread int in_buffer_size = 0;
  int output_buffer_len = 0;

  NSDL2_HTTP(NULL, NULL, "Method called, buffer = %s, buffer_len = %d, err_msg = %p, encode = %d", buffer, buffer_len, err_msg, encode);

  if(err_msg)
  { 
    out_err_msg[0] = 0;
    *err_msg = out_err_msg;
  }

  if(!buffer || !buffer[0])
  {
    sprintf(out_err_msg, "Error: Invalid input string.\n");
    return NULL;
  }

  if(buffer[0] == '{' && buffer[strlen((char *)buffer) - 1] == '}' && buffer[1] != '"')
  {
    buffer = (unsigned char *)ns_eval_string((char *)buffer);
    buffer_len = strlen((char *)buffer);
    if(buffer_len == 0)
    {
      sprintf(out_err_msg, "Error: Parameter value is not available.\n");
      return NULL;
    }
  }

  if(buffer_len <= 0) 
  {
    sprintf(out_err_msg, "Error: Invalid length of input string.\n");
    return NULL;
  }

  NSDL2_HTTP(NULL, NULL, "buffer = [%s], buffer_len = [%d]", buffer, buffer_len);

  if(encode)
    output_buffer_len = 4*((buffer_len + 2)/3);
  else
    output_buffer_len = 3*((buffer_len)/4);

  if(buffer == output_buffer)
  {
    if(in_buffer_size < buffer_len)
    {
      MY_REALLOC(in_buffer, buffer_len + 1, "Reallocating in_buffer", -1);
      in_buffer_size = buffer_len;
    }
    strncpy((char *)in_buffer, (char *)buffer, buffer_len);
    in_buffer[buffer_len] = '\0';
    buffer = in_buffer;
  }

  if(output_buffer_size < output_buffer_len)
  {
    MY_REALLOC(output_buffer, output_buffer_len + 1, "Reallocating output_buffer", -1);
    output_buffer_size = output_buffer_len;
  }

  NSDL2_HTTP(NULL, NULL, "buffer = [%s], buffer_len = [%d], output_buffer_len = %d, output_buffer_size = %d", 
                          buffer, buffer_len, output_buffer_len, output_buffer_size);
  if(encode)
  {
    if((output_buffer_len = nslb_encode_base64_ex(buffer, buffer_len, output_buffer, output_buffer_size)) == -1)
    {
      sprintf(out_err_msg, "Error: Unable to encode in base64.\n");
      return NULL;
    }
  }
  else
  {
    if((output_buffer_len = nslb_decode_base64_ex(buffer, buffer_len, output_buffer, output_buffer_size)) == -1)
    {
      sprintf(out_err_msg, "Error: Unable to decode from base64.\n");
      return NULL;
    }
  }
  output_buffer[output_buffer_len] = '\0';
  NSDL2_HTTP(NULL, NULL, "input_buffer = [%s], output_buffer = [%s], output_buffer_len = %d", buffer, output_buffer, output_buffer_len); 

  return output_buffer;
}

/*---------------------------------------------------------------------------------------------------------------------------
 * Purpose   : This function will encrypt the data (either text, binary, JSON or parameterized) 
 *
 * Input     : buffer		- Any buffer which needs to be decrypted. This can be parameterized     
 *             buffer_len	- length of input buffer
 *	       err_msg		- To dump the err_msg, if issue occurred while encypting the buffer.
 *
 * Output    : On success	- Return the length of decrypted output
 *             On Failure 	- -1  
 *
 * Build_V   : 4.1.14 #3	
 *---------------------------------------------------------------------------------------------------------------------------*/
unsigned char *ns_encode_base64(unsigned char *buffer, int buffer_len, char **err_msg)
{
  NSDL2_HTTP(NULL, NULL, "Method called, buffer = %s, buffer_len = %d", buffer, buffer_len);
  return ns_encode_decode_base64(buffer, buffer_len, err_msg, 1);
}

unsigned char *ns_decode_base64(unsigned char *encoded_buffer, int encoded_buffer_len, char **err_msg)
{
  NSDL2_HTTP(NULL, NULL, "Method called, encoded_buffer = %s, encoded_buffer_len = %d", encoded_buffer, encoded_buffer_len);
  return ns_encode_decode_base64(encoded_buffer, encoded_buffer_len, err_msg, 0);
}

#define CHECK_RTE_ENABLE(vptr)                                                                                \
  ns_rte *rte = &runprof_table_shr_mem[vptr->group_num].gset.rte_settings.rte;                      \
  if(!runprof_table_shr_mem[vptr->group_num].gset.rte_settings.enable_rte)                          \
  {                                                                                                     \
    NSTL1_OUT(NULL, NULL, "Error: G_RTE_SETTINGS is disabled for group id %d",vptr->group_num);     \
    vptr->page_status = NS_REQUEST_ERRMISC;                                                          \
    return -1;                                                                                          \
  }                                                                                                     

int ns_rte_config(char *input)
{
  char input_buf[1024 +1];
  VUser *vptr = TLS_GET_VPTR();

  NSDL2_API(vptr, NULL, "Method Called, input = %s", input);

  CHECK_RTE_ENABLE(vptr)

  if(input && input[0] == '{')
  { 
    ns_eval_string_copy (input_buf, input, 1024);
  }
  else
  {
    strncpy(input_buf, input, 1024);
    input_buf[1024] = '\0';
  }
  if(nsi_rte_config(rte, input_buf) < 0){
    vptr->page_status = NS_REQUEST_CONFAIL;
    return -1;
  }
  return 0;
}

int ns_rte_connect(char *host, char* username, char* password)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method Called, host = %s, username = %s, password = %s", host, username, password);

  CHECK_RTE_ENABLE(vptr)
  char host_buf[1024 +1];
  char username_buf[1024 +1];
  char password_buf[1024 +1];

  if(host && host[0] == '{')
  {
    ns_eval_string_copy (host_buf, host, 1024);
    host = host_buf;
  }
  if(username && username[0] == '{')
  {
    ns_eval_string_copy (username_buf, username, 1024);
    username = username_buf;
  }
  if(password && password[0] == '{')
  {
    ns_eval_string_copy (password_buf, password, 1024);
    password = password_buf;
  }
  if(nsi_rte_connect(rte, host, username, password) < 0){
    vptr->page_status = NS_REQUEST_CONFAIL;
    return -1;
  }

  return 0;
}

int ns_rte_login()
{
  VUser *vptr = TLS_GET_VPTR();
  
  CHECK_RTE_ENABLE(vptr)
 
  if((nsi_rte_login(rte)) < 0 ){
    vptr->page_status = NS_REQUEST_AUTH_FAIL;
    return -1;
  }

  return 0;
}

int ns_rte_type(char *input)
{
  VUser *vptr = TLS_GET_VPTR();
  
  CHECK_RTE_ENABLE(vptr)
  char input_buf[1024 + 1];
  if(input && input[0] == '{')
  {
    ns_eval_string_copy (input_buf, input, 1024);
    input = input_buf;
  }

  return nsi_rte_type(rte, input);
}

int ns_rte_wait_sync()
{
  VUser *vptr = TLS_GET_VPTR();

  CHECK_RTE_ENABLE(vptr)

  return nsi_rte_wait_sync(rte);
}

int ns_rte_wait_text(char *text, int timeout)
{
  VUser *vptr = TLS_GET_VPTR();
 
  CHECK_RTE_ENABLE(vptr)
  char text_buf[1024 + 1];
  if(text && text[0] == '{')
  {
    ns_eval_string_copy (text_buf, text, 1024);
    text = text_buf;
  }

  return nsi_rte_wait_text(rte, text, timeout);
}

int ns_rte_disconnect()
{
  VUser *vptr = TLS_GET_VPTR();
  
  CHECK_RTE_ENABLE(vptr)

  return nsi_rte_disconnect(rte);
}

int ns_sockjs_close(int close_id)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method Called, close_id = %d", close_id);
  vptr->sockjs_close_id = close_id;

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vut_add_task(vptr, VUT_SOCKJS_CLOSE);

    switch_to_nvm_ctx(vptr, "ns_sockjs_close(): waiting for VUT_SOCKJS_CLOSE to complete");
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), close_id = %d", close_id);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SOCKJS_CLOSE_REQ;

    vutd_send_msg_to_nvm(VUT_SOCKJS_CLOSE, (char *)(&api_req_opcode), sizeof(Ns_api_req));

  }

  NSDL2_API(vptr, NULL, "ns_sockjs_close(): Exit, vptr->sockjs_status = %d", vptr->sockjs_status);
  return vptr->sockjs_status;
}

int ns_xmpp_send(int page_id) 
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method Called");

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vptr->next_pg_id = page_id;
    vut_add_task(vptr, VUT_XMPP_SEND);
    switch_to_nvm_ctx(vptr, "ns_xmpp_send(): waiting for VUT_XMPP_SEND to complete");
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), page_id = %d", page_id);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_XMPP_SEND_REQ;
    vptr->thdd_ptr->page_id = page_id;
    vutd_send_msg_to_nvm(VUT_XMPP_SEND, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }


  NSDL2_API(vptr, NULL, "ns_xmpp_send(): Exit, vptr->xmpp_status = %d", vptr->xmpp_status);
  return vptr->xmpp_status;
}

int ns_xmpp_logout()
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method Called");

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vut_add_task(vptr, VUT_XMPP_LOGOUT);

    switch_to_nvm_ctx(vptr, "ns_xmpp_logout(): waiting for VUT_XMPP_LOGOUT to complete");
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread)");
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_XMPP_LOGOUT_REQ;

    vutd_send_msg_to_nvm(VUT_XMPP_LOGOUT, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }


  NSDL2_API(NULL, NULL, "ns_xmpp_logout()(): Exit, vptr->xmpp_status = %d", vptr->xmpp_status);
  return vptr->xmpp_status;
}

// Added for script debugging feature in 4.1.13
int ns_printf(char *format, ...)
{
  va_list ap;
  va_start (ap, format);

  if(g_debug_script)
  {
    printf("DS_MSG_START:\n");
    vprintf(format, ap);
    printf("\nDS_MSG_END:\n");
  }
  else
    vprintf(format, ap);
  va_end(ap);
  return 0;
}

int ns_fprintf(FILE *fp, char *format, ...)
{
  va_list ap;
  va_start (ap, format);

  if((fp == stdout || fp == stderr) && g_debug_script)
  {
    fprintf(fp, "DS_MSG_START:\n");
    vfprintf(fp, format, ap);
    fprintf(fp, "\nDS_MSG_END:\n");
  }
  else
    vfprintf(fp, format, ap);
  va_end(ap);
  return 0;
}

extern int make_file_path(char *proto_or_xml_fname, char *abs_proto_fname, VUser *vptr, int proto_or_xml);

/**************************************************** 
   Input: 
   xml_data          : xml file name or xml data. This can be
                       NS parameter also.
   is_file_or_buffer : flag to know whether xml_data field is
                       xml file name or xml data  
   proto_fname       : Proto file name
   msg_type          : msg_type of proto
   enc_param         : parameter to store encoded data 
  
   Return value      : Encoded data length 
*****************************************************/
int ns_protobuf_encode(char *xml_data, int is_file_or_buffer, char *proto_fname, char *msg_type, char *enc_param)
{
  void *message;
  char buffer[NS_PB_MAX_PARAM_LEN + 1];
  long xml_data_len, ret;
  int obuf_len;
  unsigned char *proto_buf = NULL;
  unsigned char *obuf = NULL;
  int obuf_size = 0; 
  int var_hashcode;
  struct stat st;

  VUser *vptr = TLS_GET_VPTR();
  

  if(!xml_data || !xml_data[0])
  {
    fprintf(stderr, "Error: XML file name not provided.\n");  
    return -1;
  }
  
  if(!proto_fname || proto_fname[0] == '\0')
  {
    fprintf(stderr, "Error: Proto file name not provided.\n");  
    return -1;
  }

  if(!msg_type || msg_type[0] == '\0')
  {
    fprintf(stderr, "Error: Message Type is not provided.\n");
    return -1;
  }
  
  if(!enc_param || enc_param[0] == '\0')
  {
    fprintf(stderr, "Error: Parameter value is empty.\n");
    return -1;
  }
  
  if((is_file_or_buffer < 0) || (is_file_or_buffer > 1))
  {
    fprintf(stderr, "Error: 'is_file_or_buffer' field can be 0 or 1 only.\n");
    return -1;
  }

  NSDL2_API(NULL, NULL, "enc_param = %s", enc_param);

  //Checking whether parameter is given to save encoded data
  var_hashcode = vptr->sess_ptr->var_hash_func(enc_param, strlen(enc_param));
  NSDL3_API(vptr, NULL, "var_hashcode of enc_param = %d", var_hashcode);

  if(var_hashcode == -1)
  {
    fprintf(stderr, "Error: Provided encoded parameter is not a valid NS parameter.\n");
    return -1;
  }

  //Make complete file path of proto file
  make_file_path(proto_fname, buffer, vptr, 1);

  NSDL2_API(NULL, NULL, "Method called, buffer = %s", buffer);

  if(stat(proto_fname, &st) == -1)
  {
    fprintf(stderr, "Error: Provide file '%s' does not exist, errno = %d, error = %s.\n", proto_fname, errno, nslb_strerror(errno));
    return -1;
  }

  if(st.st_size == 0)
  {
    fprintf(stderr, "Error: File '%s' exist with size 0.\n", proto_fname);
    return -1;
  }

  //creating object
  int sess_id;
  char *sess_name;
  sess_name = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_name;
  sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
  char script_path[NS_PB_MAX_PARAM_LEN + 1];
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  snprintf(script_path, NS_PB_MAX_PARAM_LEN, "%s/%s", GET_NS_TA_DIR(), \
           get_sess_name_with_proj_subproj_int(sess_name, sess_id, "/"));

  message = nslb_create_protobuf_msgobj(buffer, msg_type, script_path, g_ns_wdir);

  if(message == NULL)
  {
    fprintf(stderr, "Error in creating message object.\n");
    return -1;
  }

  NSDL2_API(NULL, NULL, "message = %p", message);

  xml_data_len = strlen(xml_data); 
  var_hashcode = vptr->sess_ptr->var_hash_func(xml_data, xml_data_len);
  NSDL3_API(vptr, NULL, "var_hashcode of xml_data = %d", var_hashcode);
 
  //Checking whether given xml file or buffer is a parameter or not
  if(var_hashcode != -1)
  {
    char param_name[NS_PB_MAX_PARAM_LEN + 1];

    NSDL4_API(NULL, NULL, "Xml file/buffer is provided in parameter");
    sprintf(param_name, "{%s}", xml_data);
    xml_data = ns_eval_string_flag(param_name, 0 , &xml_data_len);
  }
 
  /*Checking whether xml file name is given or data is given   
     If input is file then read the xml file                  
     else if input is buffer then directly give data for encoding*/

  if(is_file_or_buffer == 1)
  {
    //Make complete file path of xml file
    make_file_path(xml_data, buffer, vptr, 0);
 
    //Reading xml file provided and filling in in_xml 
    ret = read_xml_file(buffer, &xml_data, &xml_data_len);
 
    if(ret == -1)
    {
      fprintf(stderr, "Error: Failed to read %s file", xml_data); 
      return -1;
    }
  }
  NSDL2_API(NULL, NULL, "xml_data = %s, xml_data_len = %d", xml_data, xml_data_len);

  obuf_len = nslb_encode_protobuf(xml_data, xml_data_len, 0, message, &obuf, &obuf_size, buffer, NS_PB_MAX_ERROR_MSG_LEN);
  
  //Encode Post body into Google's Protocol Buffer and make a Single Chunck Segmented buffer 
  if(obuf_len == -1)
  { 
    fprintf(stderr, "Error: unable to encode Req Body into Protocol Buffer");
    return -1;
  }

  NSDL2_API(NULL, NULL, "obuf_len = %d", obuf_len);

  MY_MALLOC(proto_buf, NS_ENCODED_BUFFER + 1, "encoded_buffer", -1);

  int proto_buf_len = parse_protobuf_encoded_segbuf(message, obuf, proto_buf, NS_ENCODED_BUFFER);

  ns_save_binary_val((char *)proto_buf, enc_param, proto_buf_len);
  
  if(is_file_or_buffer)    //freeing buffer only if it will be malloced(i.e. when file name will be given as input)
    FREE_AND_MAKE_NOT_NULL(xml_data, NULL, "Freeing xml data buffer");
  FREE_AND_MAKE_NOT_NULL(obuf, NULL, "Freeing encoded buffer");
  FREE_AND_MAKE_NOT_NULL(proto_buf, NULL, "Freeing encoded buffer");

  return proto_buf_len;
}

/*******************************************************************
  Input: 
  encoded_data : buffer or parameter containing encoded data
  len                : length of encoded data
  input_type         : flag to know whether encoded_data is a
                       NS parameter ,buffer or a file
  proto_fname        : proto file name
  msg_type	     : msg_type of proto
  decoded_data       : parameter to store decoded data

********************************************************************/
void ns_protobuf_decode(char *encoded_data, long len, int input_type, char *proto_fname, char *msg_type, char *decoded_param)
{
  static __thread char *out = NULL;
  static __thread int out_buf_size = 0;
  void *message;
  char buffer[NS_PB_MAX_PARAM_LEN + 1];
  int var_hashcode;
  int out_len = NS_DECODED_BUFFER;
  struct stat st;
  struct stat fst;
  VUser *vptr = TLS_GET_VPTR();

  if(!encoded_data || encoded_data[0] == '\0')
  {
    fprintf(stderr, "Error: Provided encoded parameter/buffer is empty.\n");
    return;
  }

  if(len == -1)
  {
    fprintf(stderr, "Error: No encoded data length is provided.\n");
    return;
  }

  if(!proto_fname || proto_fname[0] == '\0')
  {
    fprintf(stderr, "Error: Proto file name not provided.\n");  
    return;
  }

  if(!msg_type || msg_type[0] == '\0')
  {
    fprintf(stderr, "Error: Message Type is not provided.\n");
    return;
  }

  if(!decoded_param || decoded_param[0] == '\0')
  {
    fprintf(stderr, "Error: Provided decoding Parameter value is empty.\n");
    return;
  }

  if((input_type < NS_ARG_IS_BUF) || (input_type > NS_ARG_IS_FILE))
  {
    fprintf(stderr, "Error: 'input_type' field can be 0, 1 or 2 only.\n");
    return;
  }

  var_hashcode = vptr->sess_ptr->var_hash_func(decoded_param, strlen(decoded_param));
  NSDL3_API(vptr, NULL, "Found end of variable. var_hashcode = %d", var_hashcode);

  if(var_hashcode == -1)
  {
    fprintf(stderr, "Error: Provided decoded parameter is not a valid NS parameter.\n");
    return;
  }

  if(input_type == NS_ARG_IS_PARAM)
  {
    var_hashcode = vptr->sess_ptr->var_hash_func(encoded_data, strlen(encoded_data));
    NSDL3_API(vptr, NULL, "Found end of variable. var_hashcode = %d", var_hashcode);

    if(var_hashcode == -1)
    {
      fprintf(stderr, "Error: Provided encoded parameter is not a valid NS parameter.\n");
      return;
    } 
  }
  //Data is in file
  else if(input_type == NS_ARG_IS_FILE)
  {
    if(stat(encoded_data, &fst) == -1)
    {
      fprintf(stderr, "Error: Provide file '%s' does not exist, errno = %d, error = %s.\n", encoded_data, errno, nslb_strerror(errno));
      return;
    }
    if(fst.st_size > out_len)
      out_len = fst.st_size;
  }

  //Make complete file path of proto file
  make_file_path(proto_fname, buffer, vptr, 1);  

  NSDL2_API(NULL, NULL, "Method called, buffer = %s", buffer);

  //Checking whether file provided exists or not
  if(stat(proto_fname, &st) == -1)
  {
    fprintf(stderr, "Error: Provide file '%s' does not exist, errno = %d, error = %s.\n", proto_fname, errno, nslb_strerror(errno));
    return;
  }

  //Checking size of file
  if(st.st_size == 0)
  {
    fprintf(stderr, "Error: File '%s' exist with size 0.\n", proto_fname);
    return;
  }

  //Creating msg object
  int sess_id;
  char *sess_name;
  sess_name = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_name;
  sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
  char script_path[NS_PB_MAX_PARAM_LEN + 1];
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  snprintf(script_path, NS_PB_MAX_PARAM_LEN, "%s/%s", GET_NS_TA_DIR(), \
           get_sess_name_with_proj_subproj_int(sess_name, sess_id, "/"));
  NSDL2_API(NULL, NULL, "script_path = %s", script_path);
  message = nslb_create_protobuf_msgobj(buffer, msg_type, script_path, g_ns_wdir);
  if(message == NULL)
  {
    fprintf(stderr, "Error in creating message object.\n");
    return;
  }

  if(out_len > out_buf_size)
  {
    out_buf_size = out_len; 
    MY_REALLOC(out, out_buf_size + 1, "decoded_buffer", -1);
  }

  if(input_type == NS_ARG_IS_PARAM)
  {
    char param_name[NS_PB_MAX_PARAM_LEN + 1];
    NSDL2_API(NULL, NULL, "Encoded data is provided in parameter");
    sprintf(param_name, "{%s}", encoded_data);
    encoded_data = ns_eval_string_flag(param_name, 0, &len);
  }
  else if(input_type == NS_ARG_IS_FILE)
  {
    NSDL2_API(NULL, NULL, "Encoded data is provided in file");  
    int fd = open(encoded_data, O_RDONLY|O_CLOEXEC);
    if(fd <= 0)
    {
      fprintf(stderr, "Error in openning file %s. Error = %s, function name = %s, line = %d, file = %s",
                       encoded_data, nslb_strerror(errno), (char *)__FUNCTION__, __LINE__, __FILE__);
      return;
    }
    nslb_read_file_and_fill_buf(fd, out, fst.st_size);
    close(fd);
    encoded_data = out;
    len = fst.st_size;
  }

  out_len = nslb_decode_protobuf(message, encoded_data, len, out, out_buf_size);
  if(out_len == -1)
  {
    fprintf(stderr, "Error: Decoded output len is -1");
    return;
  }

  NSDL2_API(NULL, NULL, "out = %s, out_len = %d", out, out_len);

  ns_save_string(out, decoded_param);
}

int ns_read_file(char *file_name , char *filebuf)
{
  int fd = 0;  
  long file_size = 0;
  struct stat buf;
  char *file_contents;

  NSDL2_API(NULL, NULL, "Method called. File Name = %s", file_name);

  //To get file size use stat
  if(stat(file_name, &buf))
  {
    fprintf(stderr, "Unable to get file size of file '%s', Error = %s, function name = %s, line = %d, file = %s",
                     file_name, nslb_strerror(errno), (char *)__FUNCTION__, __LINE__, __FILE__);
    return -1;
  }
  NSDL2_API(NULL, NULL, "Size is %d of file %s", buf.st_size, file_name);
  file_size = buf.st_size;
  // No contetnts in a file so write "" & return.
  if(file_size == 0) {
    char *empty_ptr = "";
    file_contents = empty_ptr;
    NSDL2_API(NULL, NULL, "Saving empty value");
    return 0;
  }

  fd = open(file_name, O_RDONLY|O_CLOEXEC);
  if(fd <= 0)
  {
    fprintf(stderr, "Error in openning file %s. Error = %s, function name = %s, line = %d, file = %s",
                     file_name, nslb_strerror(errno), (char *)__FUNCTION__, __LINE__, __FILE__);
    return -1;
  }

  //MY_MALLOC_NO_EXIT(file_contents, (file_size + 1), "file contents");
  file_contents = filebuf; 
  nslb_read_file_and_fill_buf(fd, file_contents, file_size);

  close(fd);

  filebuf[file_size - 1] = '\0'; 
  //FREE_AND_MAKE_NULL(file_contents, NULL, "file contents");
  return file_size;
}


char *ns_jwt(const char *header, const char *payload, char *key_file)
{
  char *jwt = NULL;
  char *out_buf = NULL;
  int out_len = 0;
  int jwt_len = 0;
  int header_len, payload_len, signature_size, enc_header_size , enc_payload_size;
  char *ptr, *enc_header, *enc_payload, *signature;
  char keybuf[2048];
  int keybuf_len;
  if(((keybuf_len = ns_read_file(key_file, keybuf)) <= 0))
  {
     //Fail to read file
     return NULL;
  }

  if((!header) || (!payload))
  {
    NSTL1(NULL, NULL, "HEADER or PAYLOAD cannot be NULL");
    return NULL;
  }

  header_len = strlen(header);
  payload_len = strlen(payload);
  enc_header_size = 4 * ((header_len + 2) / 3);
  enc_payload_size = 4 * ((payload_len + 2) / 3);
  signature_size =  4 * ((256 + 2) / 3); //Max Signature Size
  jwt_len = enc_header_size + 1 + enc_payload_size + 1 + signature_size;
  if(out_len < jwt_len)
  {
    MY_REALLOC(out_buf, jwt_len+1, "ns_nvm_scratch_buf", -1);
    out_len = jwt_len;
  }
  jwt = out_buf;

  enc_header = jwt;
  ns_encode_base64_binary(header, strlen(header), enc_header, enc_header_size);
  if (( ptr = strchr(enc_header,'=')) != NULL)
  {
    if (*(ptr + 1) == '=')
    {
      *(ptr + 1) = '\0';
      enc_header_size--;
    }
  
    *ptr = '\0';
     enc_header_size--;
  }
  enc_header[enc_header_size] = '.';

 

  enc_payload = enc_header + enc_header_size + 1;
  ns_encode_base64_binary(payload, payload_len, enc_payload, enc_payload_size);
  if (( ptr = strchr(enc_payload, '=')) != NULL)
  {
    if (*(ptr + 1) == '=')
    {
      enc_payload_size--;
    }
  
    *ptr = '\0';
     enc_payload_size--;
  }

 
  NSDL2_API(NULL, NULL, "enc_payload_size = %d, enc_payload = %s", enc_payload_size, enc_payload);
  
  signature = enc_payload + enc_payload_size + 1;

  if(ns_evp_sign(jwt, (enc_header_size + enc_payload_size), DIGEST_SHA256, keybuf, (unsigned char *)signature, signature_size) < 0)
  {
    fprintf(stderr, "Failed to sign\n");
    return NULL;
  }
  enc_payload[enc_payload_size] = '.';

  if (( ptr = strchr(signature,'=')) != NULL)
   *ptr = '\0';

  NSDL2_API(NULL, NULL, "jwt = %s", jwt);

  ptr = jwt;
  while(*ptr)
  {
    if(*ptr == '+')
      *ptr = '-';
    else if (*ptr == '/')
      *ptr = '_';
    ptr++;
  }
  return jwt;
}

#if 0
NetstormAlert *g_ns_alert;

//For testing purpose only
void ns_init_alert(int thread_pool_init_size, int thread_pool_max_size, int alert_queue_init_size, int alert_queue_max_size)
{
  NSDL2_HTTP(NULL, NULL, "Method called, thread_pool_init_size = %d, thread_pool_max_size = %d", thread_pool_init_size, thread_pool_max_size);
  if(!g_ns_alert)
    g_ns_alert = nslb_alert_init(thread_pool_init_size, thread_pool_max_size, alert_queue_init_size, alert_queue_max_size);

  return;
}

//For testing purpose only
int ns_config_alert(char *server_ip, unsigned short server_port, char protocol, char method, char *url,
                    char type, char *policy, char *content_type, unsigned short max_conn_retry,
                    unsigned short retry_timer, unsigned int rate_limit, int trace_fd)
{
  NSDL2_HTTP(NULL, NULL, "Method called, server_ip = %s, server_port = %d", server_ip, server_port);
  if(!g_ns_alert)
  {
    NSTL1(NULL, NULL, "Netstorm alert feature is not initialised");
    return -1;
  }

  if(!server_ip || !server_ip[0] || !url || !url[0] || !policy || !policy[0] || !method ||
          ((method == HTTP_METHOD_POST) && (!content_type || !content_type[0])))
  {
    NSTL1(NULL, NULL, "Netstorm alert configuration is invalid");
    return -1;
  }

  memset(&(global_settings->alert_info), 0, sizeof(AlertInfo));
  strncpy(global_settings->alert_info->server_ip, server_ip, 256);
  global_settings->alert_info->server_port = server_port;
  global_settings->alert_info->protocol = protocol;
  global_settings->alert_info->method = method;
  strncpy(global_settings->alert_info->url, url, 1024);
  global_settings->alert_info->type = type;
  strncpy(global_settings->alert_info->policy, policy, 256);
  if(content_type)
    strncpy(global_settings->alert_info->content_type, content_type, 1024);
  global_settings->alert_info->max_conn_retry = max_conn_retry;
  global_settings->alert_info->retry_timer = retry_timer;
  //global_settings->alert_info->trace_fd = trace_fd;

  if(nslb_alert_config(g_ns_alert, server_ip, server_port, protocol, url, max_conn_retry, retry_timer, rate_limit, trace_fd) < 0)
  {
    NSTL1(NULL, NULL, "Failed to configure alert setting, Err: %s", nslb_get_error());
    return -1;
  }

  return 0;
}
#endif

/*Nerstorm Alert Post Body*/
extern  char g_machine[];
extern unsigned short parent_port_number;

int ns_make_cavisson_alert_body(char *post_buf, int post_buf_size, int alert_type,
                                char *alert_policy, char *alert_msg, char *rule_name, char *alert_value)
{
  char instance[128];
  VUser *vptr = TLS_GET_VPTR();
  char rule_name_str[][16] = { "NS ALERT", "NC ALERT" , "GEN ALERT" };

  if(!rule_name)
  {
    if(!vptr || !vptr->sess_ptr)
      rule_name = rule_name_str[loader_opcode];
    else
      rule_name = vptr->sess_ptr->sess_name;
  }
  
  if(my_port_index == 255)
    strcpy(instance, "CavMain");
  else
    sprintf(instance, "CVM%d", my_child_index + 1);

  return nslb_make_cavisson_alert_body(post_buf,  post_buf_size, g_testrun_idx, alert_msg, alert_type,
                                       rule_name, "Cavisson",
                                       g_machine, instance, global_settings->hierarchical_view_topology_name,
                                       alert_policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number,
                                       alert_value, global_settings->progress_secs);
}

/*Generic API with specified data*/
int ns_send_alert_ex(int alert_type, int alert_method, char *content_type, char *alert_msg, int length)
{
  return nsi_send_alert(alert_type, alert_method, content_type, alert_msg, length);
}

/*Netstorm with specified alert policy*/
int ns_send_alert2(int alert_type, char *alert_policy, char *alert_msg)
{
  char buffer[MAX_ALERT_LENGTH + 1];
  int length;
  NSDL2_HTTP(NULL, NULL, "Method called, alert_type = %d, alert_policy = %s, alert_msg = %s",
                          alert_type, alert_policy, alert_msg);

  if(!alert_policy || !alert_policy[0])
  {
    alert_policy = global_settings->alert_info->policy;
  }

  if(global_settings->alert_info->method == HTTP_METHOD_GET)
  {
    if((length = nslb_make_cavisson_alert_url(buffer, MAX_ALERT_LENGTH, g_testrun_idx, alert_type, alert_policy, alert_msg)) < 0)
    {
      NSTL1(NULL, NULL, "Failed to make URL alert query, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, alert_type, alert_policy, alert_msg);
      return -1;
    }
  }
  else
  {
    if((length = ns_make_cavisson_alert_body(buffer, MAX_ALERT_LENGTH, alert_type, alert_policy, alert_msg, NULL, NULL)) < 0)
    {
      NSTL1(NULL, NULL, "Failed to make URL alert query, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, alert_type, alert_policy, alert_msg);
      return -1;
    }
  }
  return ns_send_alert_ex(alert_type, global_settings->alert_info->method, NS_CONTENT_TYPE_JSON, buffer, length);
}

/*Netstorm with default alert policy*/
int ns_send_alert(int alert_type, char *alert_msg)
{
  NSDL2_HTTP(NULL, NULL, "Method called, alert_type = %d, alert_msg = %s", alert_type, alert_msg);

  return ns_send_alert2(alert_type, global_settings->alert_info->policy, alert_msg);
}

int ns_socket_open(int open_id)
{
  VUser *vptr = TLS_GET_VPTR();
  vptr->page_status = NS_REQUEST_OK;

  vptr->next_pg_id = open_id;
  return ns_socket_open_ex(vptr);
}

int ns_socket_send(int send_id)
{
  int ret = 0;

  VUser *vptr = TLS_GET_VPTR();
  vptr->page_status = NS_REQUEST_OK;

  NSDL2_API(NULL, NULL, "Method Called, send_id = %d", send_id);

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vptr->next_pg_id = send_id;

    ret = ns_socket_ext(vptr, send_id, 0);

    NSDL2_API(vptr, NULL, "Method Exit, ret = %d", ret);
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), send_id = %d", send_id);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SOCKET_SEND_REQ;
    vptr->thdd_ptr->page_id = send_id;

    int ret = vutd_send_msg_to_nvm(VUT_SOCKET_SEND, (char *)(&api_req_opcode), sizeof(Ns_api_req));

    NSDL2_API(NULL, NULL, "take the response and take response send_id = %d", send_id);
    return (ret);
  }

  vptr->page_status = ret;

  return ret;
}

int ns_socket_get_num_msg()
{
  VUser *vptr = TLS_GET_VPTR();

  return ns_socket_get_num_msg_ex(vptr);
}

int ns_socket_recv(int sockid)
{
  VUser *vptr = TLS_GET_VPTR();
  vptr->page_status = NS_REQUEST_OK;

  vptr->next_pg_id = sockid;

  vptr->page_status = nsi_socket_recv(vptr);

  return vptr->page_status;
}

int ns_socket_close(int close_id)
{
  NSDL2_HTTP(NULL, NULL, "Method called, close_id = %d", close_id);

  VUser *vptr = TLS_GET_VPTR();
  int ret = 0;

  vptr->page_status = NS_REQUEST_OK;

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vptr->next_pg_id = close_id;
    ret = ns_socket_ext(vptr, close_id, 1);

    NSDL2_API(vptr, NULL, "Method Exit, ret = %d", ret);
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), close_id = %d", close_id);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SOCKET_CLOSE_REQ;
    vptr->thdd_ptr->page_id = close_id;

    ret = vutd_send_msg_to_nvm(VUT_SOCKET_CLOSE, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }

  vptr->page_status = ret;

  return ret; 
}

int ns_set_connect_timeout(float timeout)
{
  VUser *vptr = TLS_GET_VPTR();

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    NSDL2_HTTP(NULL, NULL, "Method called, timeout = %f", timeout);
    g_socket_vars.socket_settings.conn_to = timeout * 1000;  //Timeout in msecs
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), timeout = %f", timeout);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_CONN_TIMEOUT_REQ;
    vptr->thdd_ptr->page_id = timeout * 1000;

    vutd_send_msg_to_nvm(NS_API_CONN_TIMEOUT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }
  return 0;
}

int ns_set_send_timeout(float timeout)
{
  VUser *vptr = TLS_GET_VPTR();

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    NSDL2_HTTP(NULL, NULL, "Method called, timeout = %f", timeout);
    g_socket_vars.socket_settings.send_to = timeout * 1000;  //Timeout in msecs
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), timeout = %f", timeout);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SEND_TIMEOUT_REQ;
    vptr->thdd_ptr->page_id = timeout * 1000;

    vutd_send_msg_to_nvm(NS_API_SEND_TIMEOUT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }
  return 0;
}

int ns_set_send_inactivity_timeout(float timeout)
{
  VUser *vptr = TLS_GET_VPTR();

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    NSDL2_HTTP(NULL, NULL, "Method called, timeout = %f", timeout);
    g_socket_vars.socket_settings.send_ia_to = timeout * 1000;  //Timeout in msecs
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), timeout = %f", timeout);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SEND_IA_TIMEOUT_REQ;
    vptr->thdd_ptr->page_id = timeout * 1000;

    vutd_send_msg_to_nvm(NS_API_SEND_IA_TIMEOUT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }
  return 0;
}

int ns_set_recv_timeout(float timeout)
{
  VUser *vptr = TLS_GET_VPTR();

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    NSDL2_HTTP(NULL, NULL, "Method called, timeout = %f", timeout);
    g_socket_vars.socket_settings.recv_to = timeout * 1000;  //Timeout in msecs
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), timeout = %f", timeout);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_RECV_TIMEOUT_REQ;
    vptr->thdd_ptr->page_id = timeout * 1000;

    vutd_send_msg_to_nvm(NS_API_RECV_TIMEOUT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }
  return 0;
}

int ns_set_recv_inactivity_timeout(float timeout)
{
  VUser *vptr = TLS_GET_VPTR();

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    NSDL2_HTTP(NULL, NULL, "Method called, timeout = %f", timeout);
    g_socket_vars.socket_settings.recv_ia_to = timeout * 1000;  //Timeout in msecs
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), timeout = %f", timeout);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_RECV_IA_TIMEOUT_REQ;
    vptr->thdd_ptr->page_id = timeout * 1000;

    vutd_send_msg_to_nvm(NS_API_RECV_IA_TIMEOUT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }

  return 0;
}

int ns_set_recv_first_byte_timeout(float timeout)
{
  VUser *vptr = TLS_GET_VPTR();

  if(IS_NS_SCRIPT_MODE_USER_CONTEXT)
  {
    NSDL2_HTTP(NULL, NULL, "Method called, timeout = %f", timeout);
    g_socket_vars.socket_settings.recv_fb_to = timeout * 1000;  //Timeout in msecs
  }
  else
  {
    NSDL2_API(vptr, NULL, "Method Called (Thread), timeout = %f", timeout);
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_RECV_FB_TIMEOUT_REQ;
    vptr->thdd_ptr->page_id = timeout * 1000;

    vutd_send_msg_to_nvm(NS_API_RECV_FB_TIMEOUT_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }

  return 0;
}

int ns_socket_get_fd(int action_idx)
{
  VUser *vptr = TLS_GET_VPTR();
  connection *cptr = NULL;

  if((cptr = nsi_get_cptr_from_sock_id(vptr, action_idx)) == NULL)
    return -1;

  return cptr->conn_fd;
}

int ns_socket_set_options(int action_idx, char *level, char *optname, void *optval, socklen_t optlen)
{
  VUser *vptr = TLS_GET_VPTR();

  if(!level || level[0] == '\0')
  {
    fprintf(stderr, "Error: Socket Level is not provided to set socket option value.\n");
    return -1;
  }

  if(!optname || optname[0] == '\0')
  {
    fprintf(stderr, "Error: Socket Option Name is not provided to set socket option value.\n");
    return -1;
  }

  if(!optval)
  {
    fprintf(stderr, "Error: Socket Option value is not provided to set socket option value.\n");
    return -1;
  }

  NSDL2_API(vptr, NULL, "action_idx = [%d], level = [%s], optname = [%s], optlen = %d", action_idx, level, optname, optlen);

  if(nsi_get_or_set_sock_opt(vptr, action_idx, level, optname, optval, optlen, SET_SOCK_OPT) == -1)
    return -1;

  return 0;
}

int ns_socket_get_options(int action_idx, char *level, char *optname, void *optval, socklen_t *optlen)
{
  VUser *vptr = TLS_GET_VPTR();
  int ret = 0;

  if(!level || level[0] == '\0')
  {
    fprintf(stderr, "Error: Socket Level is not provided to get socket option value.\n");
    return -1;
  }

  if(!optname || optname[0] == '\0')
  {
    fprintf(stderr, "Error: Socket Option Name is not provided to get socket option value.\n");
    return -1;
  }

  if(!optval)
  {
    fprintf(stderr, "Error: Socket Option value is not provided to get socket option value.\n");
    return -1;
  }

  if(!optlen)
  {
    fprintf(stderr, "Error: Socket Option Len is not provided to get socket option value.\n");
    return -1;
  }

  NSDL2_API(vptr, NULL, "action_idx = [%d], level = [%s], optname = [%s], optlen = %d", action_idx, level, optname, *optlen);

  if((ret = nsi_get_or_set_sock_opt(vptr, action_idx, level, optname, optval, *optlen, GET_SOCK_OPT)) == -1)
    return -1;
  else
    *optlen = ret;

  return 0;
}

#define ATTR_ADDR        17
#define ATTR_PORT        6
#define ATTR_HOSTNAME    200

static __thread char *sout_buf = NULL;

char *ns_socket_get_attribute(int action_idx, char *attribute_name)
{
  VUser *vptr = TLS_GET_VPTR();

  static __thread char service[NI_MAXSERV], Host[NI_MAXHOST];
  int loc_attribute;
  connection *cptr = NULL;
  struct sockaddr_in name;
  socklen_t nameLen;

  NSDL2_API(vptr, NULL, "action_idx = [%s], attribute_name = %s", action_idx, attribute_name);

  if(!attribute_name || attribute_name[0] == '\0')
  {
    fprintf(stderr, "Error: Attribute name is not provided.\n");  
    return NULL;
  }

  if((cptr = nsi_get_cptr_from_sock_id(vptr, action_idx)) == NULL)
    return NULL;

  if((loc_attribute = nsi_get_socket_attribute_from_name(attribute_name)) == -1)
    return NULL;

  if(!sout_buf)
    MY_MALLOC(sout_buf, 256 + 1, "SocketAttribute: output buffer", -1);

  switch(loc_attribute)
  {
    case SLOCAL_ADDRESS:
    case SLOCAL_HOSTNAME:
    case SLOCAL_PORT:
    {
      nameLen = sizeof(name);
      if(getsockname(cptr->conn_fd, (struct sockaddr*)&name, &nameLen) == -1)
      {
        fprintf(stderr, "Error in getting socket name");
        return NULL;
      }

      if(loc_attribute == SLOCAL_ADDRESS)
      {
        //convert address to dotted quad notation
        snprintf(sout_buf, ATTR_ADDR, "%s", inet_ntoa(name.sin_addr));
        NSDL3_API(NULL, NULL, "SocketAddr = %s", sout_buf);
        return sout_buf;
      }
      else if(loc_attribute == SLOCAL_HOSTNAME)
      {
        if(getnameinfo((struct sockaddr*)&name, nameLen, Host, sizeof(Host), service, sizeof(service), NI_NAMEREQD) < 0)
        {
          fprintf(stderr, "Failed to convert address to hostname");
          return NULL;
        }
        snprintf(sout_buf + ATTR_ADDR + ATTR_PORT, ATTR_HOSTNAME, "%s", Host);
        NSDL3_API(NULL, NULL, "SocketHostname = %s", sout_buf + ATTR_ADDR + ATTR_PORT);
        return sout_buf + ATTR_ADDR + ATTR_PORT;
      }
      else if(loc_attribute == SLOCAL_PORT)
      {
        snprintf(sout_buf + ATTR_ADDR, ATTR_PORT, "%d", ntohs(name.sin_port));
        NSDL3_API(NULL, NULL, "SocketPort = %s", sout_buf + ATTR_ADDR);
        return sout_buf + ATTR_ADDR;
      }
    }
    break;
    case SREMOTE_ADDRESS:
    case SREMOTE_HOSTNAME:
    case SREMOTE_PORT:
    {
      nameLen = sizeof(name);
      if(getpeername(cptr->conn_fd, (struct sockaddr*)&name, &nameLen) == -1)
      {
        fprintf(stderr, "Failed to get peer name");
        return NULL;
      }
      if(loc_attribute == SREMOTE_ADDRESS)
      {
        snprintf(sout_buf, ATTR_ADDR, "%s", inet_ntoa(name.sin_addr));
        NSDL3_API(NULL, NULL, "SocketRemoteAddr = %s", sout_buf);
        return sout_buf;
      }
      else if(loc_attribute == SREMOTE_HOSTNAME)
      {
        if(getnameinfo((struct sockaddr*)&name, nameLen, Host, sizeof(Host), service, sizeof(service), NI_NAMEREQD) < 0)
        {
          fprintf(stderr, "Failed to convert address to hostname");
          return NULL;
        }
        snprintf(sout_buf + ATTR_ADDR + ATTR_PORT, ATTR_HOSTNAME, "%s", Host);
        NSDL3_API(NULL, NULL, "SocketRemoteHostname = %s", sout_buf + ATTR_ADDR + ATTR_PORT);
        return sout_buf + ATTR_ADDR + ATTR_PORT;
      }
      else if (loc_attribute == SREMOTE_PORT)
      {
        snprintf(sout_buf + ATTR_ADDR, ATTR_PORT, "%d", ntohs(name.sin_port));
        NSDL3_API(NULL, NULL, "SocketRemotePort = %s", sout_buf + ATTR_ADDR);
        return sout_buf + ATTR_ADDR;
      }
    }
    break;
    default:
      fprintf(stderr, "Invalid attribute name [%s] provided.", attribute_name);
      return NULL;
  }
  return NULL;
}

int ns_socket_enable(int action_idx, char *operation)
{
  if(!operation || operation[0] == '\0')
  {
    fprintf(stderr, "Error: Operation to Enable is not provided.\n");  
    return -1;
  }

  ns_socket_enable_ex(action_idx, operation);
    
  return 0; 
}

int ns_socket_disable(int action_idx, char *operation)
{
  if(!operation || operation[0] == '\0')
  {
    fprintf(stderr, "Error: Operation to disable is not provided.\n");  
    return -1;
  }

  ns_socket_disable_ex(action_idx, operation);

  return 0; 
}

char *ns_socket_error(int *lerrno)
{
  static __thread char err_buff[1024];

  *lerrno = errno;
  sprintf(err_buff, "ERROR[%d]: %s", errno, nslb_strerror(errno));

  return err_buff;
}

int ns_socket_start_ssl(int action_idx, char *version, char *ciphers)
{
  connection *cptr = NULL;
  u_ns_ts_t now = get_ms_stamp();

  VUser *vptr = TLS_GET_VPTR();

  if(ns_socket_set_version_cipher(vptr, version, ciphers) == -1)
    fprintf(stderr, "Error: SSL Version is not provided correct.\n");

  if((cptr = nsi_get_cptr_from_sock_id(vptr, action_idx)) == NULL)
    return -1;

  cptr->request_type = SSL_SOCKET_REQUEST;

  upgrade_ssl(cptr, now);
  //handle_connect(cptr, now, 0);

  return 0;
}

char *ns_ascii_to_ebcdic(char *ascii_input, int ascii_len, int *con_len)
{
  NSDL2_API(NULL, NULL, "Method Called, ascii_input = %s, ascii_len = %d", ascii_input, ascii_len);
  return nslb_ascii_to_ebcdic(ascii_input, ascii_len, con_len);
}

char *ns_ebcdic_to_ascii(char *ebcdic_input, int ebcdic_len, int *con_len)
{
  NSDL2_API(NULL, NULL, "Method Called, ebcdic_input = %s, ebcdic_len = %d", ebcdic_input, ebcdic_len);
  return nslb_ebcdic_to_ascii(ebcdic_input, ebcdic_len, con_len);
}

/*bug 93672: gRPC API*/
int ns_grpc_client(int page_id)
{
  return ns_web_url(page_id);
}
/*******Start- Bug 79149 **************************/
int ns_rdp(int page_id)
{
  VUser *vptr = TLS_GET_VPTR();
  NSDL2_API(vptr, NULL, "Method Called. page_id=%d vptr = %p", page_id, vptr);
  NSDL2_API(vptr, NULL, "vptr->group_num = %d runprof_table_shr_mem[vptr->group_num].gset.script_mode = %d", vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.script_mode);
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vptr->next_pg_id = page_id;
    NSDL4_API(vptr, NULL, "adding task = %d", VUT_RDP);
    vut_add_task(vptr, VUT_RDP);
    switch_to_nvm_ctx(vptr, "ns_rdp(): waiting for VUT_RDP to complete");
  }
  //ToDo: S=upport Thread Mode
  NSDL2_API(vptr, NULL, "ns_rdp(): Exit, vptr->rdp_status = %d", vptr->rdp_status);
  return vptr->rdp_status;
}
/*******END- Bug 79149 **************************/

// This is a Static function to add local name in xpath query.
//   For ex - For xpath_query:
//        /Envelope/Body[1]/RetrieveContentResponse/returnval[2]/protection
//   it updates it to
//        //*[local-name()='Envelope']//*[local-name()='Body'][1]//*[local-name()='RetrieveContentResponse']//*[local-name()='returnval'][2]//*[local-name()='protection']
   
static char *ns_add_local_name_xpath_query(char *str, char *modString)
{
  const char *start_local_name="//*[local-name()='";
  int start_local_len = strlen(start_local_name);

  const char *end_local_name = "']";
  int end_local_len = strlen(end_local_name);

  /* walk through other tokens */
  int endbracketset;
  char *ptr = str;
  int modStringlen = 0;

  while(*ptr) {
    ptr++; // ignorning "/" in xml query
    strcpy(modString+modStringlen, start_local_name);
    modStringlen += start_local_len;

    endbracketset = 0;
    while(*ptr && *ptr != '/')
    {
      if(*ptr == '[')
      {
        endbracketset = 1;
        strcpy(modString+modStringlen, end_local_name);
        modStringlen += end_local_len;
      }

      modString[modStringlen++] = *ptr;
      ptr++;
    }
    if(!endbracketset)
    {
      strcpy(modString+modStringlen, end_local_name);
      modStringlen += end_local_len;
    }
     
  }
  modString[modStringlen] = '\0';

  NSDL2_API(NULL, NULL, "Converted query is  = %s", modString);
  return modString;
}

static int nslb_xml_replace_node_attribute(xmlNodeSetPtr nodes, xmlDocPtr doc, char* xml_fragment, int xml_frag_len)
{

  xmlNodePtr cur;
  int size;
  int i;
  xmlNodePtr node;
 
  // Get the number of nodes to be replaced 
  size = (nodes) ? nodes->nodeNr : 0;

  for(i = 0; i < size; ++i) {
    if(!nodes->nodeTab[i])
      return -1;
 
    cur = nodes->nodeTab[i];
    // If accidently it is the doc, then do not replace it
    if (nodes->nodeTab[i] == (void*) doc) {
      fprintf(stderr, "The document node cannot be moved.\n");
      return -1;
    }    

    // Get the Parent node whose child has to be replaced
    xmlParseInNodeContext(cur->parent, xml_fragment, xml_frag_len, 0, &node);

    // First add child to the node to be replaced then delete that node
    xmlAddNextSibling(cur, node);
    /* delete node */
    xmlUnlinkNode(cur);
    /* Free node and children */
    xmlFreeNode(cur); 
  }

  return 0;

}

/* Description : Library function to replace node/attribute in a xml 

Arguments:

Input Arguments:
         in_xml:        XML in char string
         in_xml_len:    Length of XML string
         xpath_query:   XPATH to get the node/attribute to be replaced
         xml_fragment:  Fragment(node/attribute) to be added in new XML
         xml_frag_len:  Length of fragment

Output Arguments:
        out_xml:        XML after getting updated
        out_xml_size:   Size of output XML
*/

int nslb_xml_replace(char *in_xml, int in_xml_len, char *xpath_query, char * xml_fragment, int xml_frag_len, char **out_xml, int *out_xml_size)
{
   /* Init libxml */
    xmlInitParser();
    LIBXML_TEST_VERSION

    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlDocPtr doc;

    /* Load XML document */
    doc = xmlParseMemory(in_xml, in_xml_len);
    if (doc == NULL) {
        fprintf(stderr, "Error: unable to parse provided XML");
        return(-1);
    }

    /* Create xpath evaluation context */
    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL) {
        fprintf(stderr,"Error: unable to create new XPath context\n");
        xmlFreeDoc(doc);
        return(-1);
    }

    /* Evaluate xpath expression */
    xpathObj = xmlXPathEvalExpression(xpath_query, xpathCtx);
    if(xpathObj == NULL) {
        fprintf(stderr,"Error: unable to evaluate xpath expression \"%s\"\n", xpath_query);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return(-1);
    }
    
    if(nslb_xml_replace_node_attribute(xpathObj->nodesetval, doc, xml_fragment, xml_frag_len) == -1)
    {
      fprintf(stderr,"Error: while replacing xml_fragment \"%s\"\n", xml_fragment);
      xmlXPathFreeContext(xpathCtx);
      xmlFreeDoc(doc);
      return(-1);
    } 

    xmlDocDumpFormatMemory(doc, out_xml, out_xml_size, 1);

    /* Cleanup */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    
    return 0;

}

/**************************************************************************************

LIB Function: ns_xml_replace

Description:

The lr_xml_replace function queries the XML input string XML for values matching 
the Query criteria and replaces them either with XmlFragment or 
XmlFragmentParam as the value of the elements matched by the query

Usage:

ns_xml_replace("in_param", "xpath_query", "xml_fragment", "out_param" )

in_param       - Should always be parameter containing an XML data
xpath_query    - can be a String or Parameter containing query to be done on input XML
xml_fragment   - can be a String or Parameter containing fragment to replace the data 
                 retrieved through xpath_query
out_param      - Should always be a parameter, if given NULL, in_param will be considered 
                 as out_param

Ex: Consider the below example:

in_param: parameter containing XML as

"<?xml version=\"1.0\"?>"
  "<catalog>"
   "<book id=\"bk101\">"
      "<author>Gambardella, Matthew</author>"
      "<title>XML Developer's Guide</title>"
      "<genre>Computer</genre>"
   "</book>"
   "<book id=\"bk102\">"
      "<author>Ralls, Kim</author>"
      "<title>Midnight Rain</title>"
      "<genre>Fantasy</genre>"
   "</book>"
  "</catalog>"

xpath_query: /catalog/book[2]/author

xml_fragment: <author>Michael</author>

out_param: Output parameter

Output:

<?xml version="1.0"?>
<catalog>
  <book id="bk101">
    <author>Gambardella, Matthew</author>
    <title>XML Developer's Guide</title>
    <genre>Computer</genre>
  </book>
  <book id="bk102">
    <author>Michael</author>
    <title>Midnight Rain</title>
    <genre>Fantasy</genre>
  </book>
</catalog>



****************************************************************************************/

#define XML_QUERY_FRAGMENT 1024
int ns_xml_replace(char *in_param, char *xpath_query, char *xml_fragment, char *out_param)
{

  char xmlQueryString[XML_QUERY_FRAGMENT];

  static __thread char *xmlFrag = NULL;
  static __thread int xml_frag_size = 0;

  int xml_frag_len, in_param_len;

  VUser *vptr = TLS_GET_VPTR();
  int var_hashcode;

  // Check for VALID in_param 
  if( !in_param || in_param[0] == '\0')
  {
    NSTL1(vptr, NULL, "Error: Provided xml input parameter is empty.");
    return -1;
  }

  // Check for VALID xpath_query
  if( !xpath_query || xpath_query[0] == '\0')
  {
    NSTL1(vptr, NULL, "Error: Provided input xml path query is empty.");
    return -1;
  }

  //  Check for VALID xml_fragment
  if( !xml_fragment || xml_fragment[0] == '\0')
  {
    NSTL1(vptr, NULL, "Error: Provided input xml fragment is empty.");
    return -1;
  }

  // Check if out_param is NULL or empty, if not empty check its hashcode for VALID param
  if( !out_param || out_param[0] == '\0')
    out_param = in_param; // Else make out_param to in_param
  else
  { 
    var_hashcode = vptr->sess_ptr->var_hash_func(out_param, strlen(out_param));
    NSDL2_API(vptr, NULL, "out_param var_hashcode = %d", var_hashcode);
    if(var_hashcode == -1)
    {
      NSTL1(vptr, NULL, "Error: Provided output parameter is not a valid NS parameter.");
      return -1;
    }

    VarTransTableEntry_Shr* var_ptr;
    // Check if out_param is a FILE param
    var_ptr = &vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode];
    NSDL3_API(vptr, NULL, "var_type=%d", var_ptr->var_type);
    if (var_ptr->var_type == VAR)
    {
      NSTL1(vptr, NULL, "Error: Output parameter shouldn't be a File Param in ns_xml_replace API.");
      return -1;
    }
  }

  // in_param should be VALID parameter. Check for its hashcode
  var_hashcode = vptr->sess_ptr->var_hash_func(in_param, strlen(in_param));
  NSDL4_API(vptr, NULL, "in_param var_hashcode = %d", var_hashcode);

  if(var_hashcode == -1)
  {
    NSTL1(vptr, NULL, "Error: Provided input parameter is not a valid NS parameter.");
    return -1;
  }
  
 // xpath_query should be VALID parameter. Check for its hashcode
  var_hashcode = vptr->sess_ptr->var_hash_func(xpath_query, strlen(xpath_query));
  NSDL4_API(vptr, NULL, "xpath_query var_hashcode = %d", var_hashcode);
  // Its a parameter
  if(var_hashcode != -1)
  {
     sprintf(xmlQueryString, "{%s}", xpath_query);
     xpath_query = ns_eval_string(xmlQueryString);
  }
  NSDL4_API(vptr, NULL, "Input xpath_query = %s", xpath_query);

  //Change xpath_query and add local_name
  xpath_query = ns_add_local_name_xpath_query(xpath_query, xmlQueryString);
  NSDL4_API(vptr, NULL, "xpath_query after adding local_name = %s", xpath_query);

  // xml_fragment should be VALID parameter. Check for its hashcode
  xml_frag_len = strlen(xml_fragment);
  var_hashcode = vptr->sess_ptr->var_hash_func(xml_fragment, xml_frag_len);
  NSDL3_API(vptr, NULL, "xpath_fragment var_hashcode = %d", var_hashcode);

  // Its a parameter
  if(var_hashcode != -1)
  {
     char xmlFragString[128];
     sprintf(xmlFragString, "{%s}", xml_fragment);
     // Perform eval_string on xml_fragment
     xml_fragment = ns_eval_string_flag(xmlFragString, 0 , &xml_frag_len);
     // Relocate only in case if size is small
     if(xml_frag_size < xml_frag_len)
     {
       MY_REALLOC_EX(xmlFrag, xml_frag_len , xml_frag_size, "xml fragment", -1);
       xml_frag_size = xml_frag_len;
     }
     strncpy(xmlFrag, xml_fragment, xml_frag_len);
     xml_fragment = xmlFrag;
  }

  NSDL4_API(vptr, NULL, "Provided xml_fragment = %s", xml_fragment);

  // Perform eval string on input param 
  char in_param_string[128];
  sprintf(in_param_string, "{%s}", in_param); 
  in_param = ns_eval_string_flag(in_param_string, 0, &in_param_len);

  // Replace node or attibute
  if(nslb_xml_replace(in_param, in_param_len, xpath_query, xml_fragment, xml_frag_len, &g_tls.buffer, &g_tls.buffer_size) == -1)
  {
    NSTL1(vptr, NULL, "Error: While replacing XML Node/Attribute.");
    NSDL1_API(vptr, NULL, "Error: While replacing XML Node/Attribute.");
    return -1;
  }
  // Finally save the value in out_param
  ns_save_string(g_tls.buffer, out_param);

  return 0;
 
}

#include "./ns_param_api.c"
#include "./ns_save_data_api.c"
//#include "./ymsg/ns_ymsg_api.c"
#include "./ns_click_api.c"
#include "./ns_form_api.c"
#include "./ns_red_client.c"
#include "./ns_test_api.c"
#include "./ns_rbu_api.c"
#include "./ns_rbu_chrome_api.c"
#include "./ns_enc_dec_api.c"
#include "./ns_check_point_api.c"
#include "./ns_mongodb_api.c"

#if ( !(Fedora && RELEASE == 4))
#include "./database/src/ns_db_api.c"
#endif

#include "./ns_socket_api.c"
#include "./ns_rte_api.c"
#include "./ns_jmeter_api.c"
#include "./ns_cassandra_api.c"

#include "./jms/src/ns_ibmmq_api.c"
#include "./jms/src/ns_kafka_api.c"
#include "./jms/src/ns_tibco_api.c"
#include "./ns_js_api.c"
#include "ns_desktop.c"
#include "ns_rdp_api.c"

/*******************************BUG 95528*******************************
Purpose: supporting lr_replace API in Netstorm
Inputs: 1. inp_param   -> parameter on which you want to change a sub string.
        2. find_str    -> string you want to change (case sensitive).
        3. replace_str -> string with what you want to change.


Example:

Assumption:

inp_param   :: "Param" (a declaired parameter)  have value :: Cavisson is a Testing Product.
find_str    :: "product"
replace_str :: "Company"


IF we are using API as:
          ns_replace("Param", "product", "Company");

Output will be:

Cavisson is a Testing Product.                     >>>>> It is not changed because find str is case sensitive.


For getting changed value, we need to pass values like,

     ns_replace("Param", "Product", "Company");    >>>>> find_str is now same as present in string (case sensitive)

Now Output will be:

Cavisson is a Testing Company.

************************************************************************/
int ns_replace(char *inp_param, char* find_str, char* replace_str)
{
  //1. returning function ns_replace_ex with all ocr and ignore case using out_param same as inp_param.
  return ns_replace_ex(inp_param, inp_param, find_str, replace_str, ORD_ALL, 0);
}

int ns_replace_ex(char *inp_param, char *out_param, char* find_str, char* replace_str, int ord, int ignore_case)
{
   int input_str_len = 0;                                   //inp_param length
   VUser *vptr = TLS_GET_VPTR();
   VarTransTableEntry_Shr* var_ptr;
   int var_hashcode;
   char buff[128 + 1];

//1. Check inp_param is NOT NULL and exist.
  if( !inp_param || inp_param[0] == '\0')
  {
    fprintf(stderr, "Error: Provided input parameter is empty.\n");
    return -1;
  }

//2. Check out_param is NOT NULL and exist.
  if( !out_param || out_param[0] == '\0')
  {
    fprintf(stderr, "Error: Provided output parameter is empty.\n");
    return -1;
  }

//3. check find_str is not null and exist.
  if(!find_str || find_str[0] == '\0')
  {
    fprintf(stderr, "Error: String for replace is not provided.\n");
    return -1;
  }

//4. check replace_str exist.
  if(!replace_str)
  {
    fprintf(stderr, "Error: String for replace is not provided.\n");
    return -1;
  }

//5. check if inp_param is paramater, if not then retrun error
  var_hashcode = vptr->sess_ptr->var_hash_func(inp_param, strlen(inp_param));
  NSDL3_API(vptr, NULL, "Found end of variable. var_hashcode = %d", var_hashcode);

  if(var_hashcode == -1)
  {
    fprintf(stderr, "Error: Provided input parameter is not a valid NS parameter.\n");
    return -1;
  }

//6. check if out_param is parameter, if not then retrun error
    var_hashcode = vptr->sess_ptr->var_hash_func(out_param, strlen(out_param));
    NSDL3_API(vptr, NULL, "Found end of variable. var_hashcode = %d", var_hashcode);

    if(var_hashcode == -1)
    {
      fprintf(stderr, "Error: Provided output parameter is not a valid NS parameter.\n");
      return -1;
    }

//7. Check if out_param hash code is not of file parameter, if yes the return with error message.
  var_ptr = &vptr->sess_ptr->vars_trans_table_shr_mem[var_hashcode];
  NSDL3_API(vptr, NULL, "var_type=%d", var_ptr->var_type);
  if (var_ptr->var_type == VAR)
  {
    fprintf(stderr, "Error: File Param is not supported in ns_replace API.\n");
    return -1;
  }

//8. Fetch inp_param value and get string length of it.
  snprintf(buff, 128, "{%s}", inp_param);
  long len;
  char *param_buf = ns_eval_string_flag(buff, 0, &len);
  input_str_len = (int)len;

//9. call function nslb_repelce_string2 function to replace string and check if it return a positive number.
      /*(We are checing positive numbe as return value of lib is no. of changed characters)*/
  if(nslb_replace_string2(param_buf, input_str_len, &g_tls.buffer, &g_tls.buffer_size, find_str, replace_str, 0/*replace offset*/, ord, ignore_case) < 0)
    return -1;

//10. Save result in output parameter.
     ns_save_string(g_tls.buffer, out_param);

  return 0;
}
/****************************************************************************************************/


// End of File
