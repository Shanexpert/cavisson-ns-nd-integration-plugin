#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_sock.h"
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
#include "poi.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "ipmgmt_utils.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_trace_level.h"
#include "ns_exit.h"
#include "divide_users.h"
#include "ns_parse_src_ip.h"
static UniqueSrcIPTable *unique_src_ip_table = NULL;   //USE_SRC_IP MODE unique
static SharedSrcIPTable *shared_src_ip_table = NULL;   //USE_SRC_IP MODE shared
//static int srcip_debug = 0;

//srcip list is a singly linked list
//elements are added on the tail and always
//taken out at head
//static IP_data *srcip_head, *srcip_tail;

void init_src_ip_lists ();

//get an srcip elemnet from head

#define DIVIDE_IP_PER_PROC_PER_GROUP { \
  for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) \
  { \
    if (runprof_table_shr_mem[grp_idx].gset.num_ip_entries && (runprof_table_shr_mem[grp_idx].gset.src_ip_mode == SRC_IP_UNIQUE))  \
    {  \
      skiped_nvm_count = 0; \
      for (i = 0; i < global_settings->num_process; i++) \
      { \
        if(!per_proc_runprof_table[(i * total_runprof_entries) + grp_idx]) \
        { \
          skiped_nvm_count++; \
          continue; \
        } \
      } \
      num_process = global_settings->num_process - skiped_nvm_count;\
      if(!num_process) \
        continue; \
      group_num_ip = runprof_table_shr_mem[grp_idx].gset.num_ip_entries; \
      num_users = runprof_table_shr_mem[grp_idx].quantity; \
      extra = (group_num_ip - num_users) / num_process; \
      NSDL2_IPMGMT(NULL, NULL, "skiped_nvm_count = %d, group_num_ip = %d, num_ip = %d child_idx = %d num_process = %d", \
                                skiped_nvm_count, group_num_ip, num_ip, child_idx, num_process);  \
      for (i = 0; i < global_settings->num_process; i++)  \
      { \
        if(!per_proc_runprof_table[(i * total_runprof_entries) + grp_idx]) { \
          NSDL2_IPMGMT(NULL, NULL, "Continuing"); \
          continue; \
        } \
        num_ip = per_proc_runprof_table[(i * total_runprof_entries) + grp_idx];\
        if (global_settings->load_key) { \
          num_ip = num_ip + extra; \
        } \
        uniq_src_ip_num[grp_idx][i] += num_ip; \
        NSDL2_IPMGMT(NULL, NULL, "uniq_src_ip_num[%d][%d] = %d", grp_idx, i, uniq_src_ip_num[grp_idx][i]); \
      } \
      gset_ips_ptr[grp_idx] = runprof_table_shr_mem[grp_idx].gset.ips; \
    } \
  } \
}  

#define INIT_MASTER_SRC_IP_TABLE_ENTRIES(gset_ptr, src_ip_shr_ptr) { \
  src_ip_shr_ptr->num_entries = 0; \
  src_ip_shr_ptr->ip_entry = NULL; \
  src_ip_shr_ptr->sock_head = NULL; \
  src_ip_shr_ptr->sock_tail = NULL; \
  for(;i < gset_ptr->num_ip_entries; i++) { \
      NSDL2_IPMGMT(NULL, NULL, "i = %d, j = %d, gset_ptr->ips[i].net_idx = %d", i, j, gset_ptr->ips[i].net_idx); \
      if(j == gset_ptr->ips[i].net_idx) { \
        src_ip_shr_ptr->num_entries++; \
        NSDL2_IPMGMT(NULL, NULL, "src_ip_shr_ptr->num_entries = %d, flag_ip = %d", src_ip_shr_ptr->num_entries, flag_ip); \
        if(flag_ip) { \
          src_ip_shr_ptr->ip_entry = &gset_ptr->ips[i]; \
          flag_ip = 0; \
        } \
      } else \
         break; \
  } \
}

#define SET_GROUP_IP_TO_ALL { \
  gset_ptr->ips = group_default_settings->ips; \
  gset_ptr->num_ip_entries = group_default_settings->num_ip_entries; \
  gset_ptr->g_max_net_idx =  group_default_settings->g_max_net_idx; \
  gset_ptr->master_src_ip_table = group_default_settings->master_src_ip_table;  \
  NSDL2_IPMGMT(NULL, NULL, "gset_ptr = %p, gset_ptr->num_ip_entries = %d gset_ptr->master_src_ip_table = %p gset->master->ips = %p", \
                            gset_ptr, gset_ptr->num_ip_entries, gset_ptr->master_src_ip_table, gset_ptr->master_src_ip_table->ip_entry); \
}

inline void
add_srcip_to_free_list(VUser *vptr, PerGrpUniqueSrcIPTable *el, int grp_idx)
{

  NSDL2_IPMGMT(vptr, NULL, "Method called, vptr = %p, grp_idx = %d unique_src_ip_table = %p", vptr, grp_idx, unique_src_ip_table);
  if (unique_src_ip_table[grp_idx].free_srcip_tail) {
    NSDL2_IPMGMT(vptr, NULL, "Adding ip on free srcip tail");
    unique_src_ip_table[grp_idx].free_srcip_tail->next = el;
  } else {
    NSDL2_IPMGMT(vptr, NULL, "Adding ip on free srcip head");
    unique_src_ip_table[grp_idx].free_srcip_head = el;
  }
  unique_src_ip_table[grp_idx].free_srcip_tail = el;
  el->next = NULL;

  NSDL2_IPMGMT(vptr, NULL, "free_srcip_head = %p, free_srcip_tail = %p",  unique_src_ip_table[grp_idx].free_srcip_head, unique_src_ip_table[grp_idx].free_srcip_tail);
  NSDL2_IPMGMT(vptr, NULL, "busy_srcip_head = %p, busy_srcip_tail = %p",  unique_src_ip_table[grp_idx].busy_srcip_head, unique_src_ip_table[grp_idx].busy_srcip_tail);
  NSDL2_IPMGMT(vptr, NULL, "Alloc src IP = %s by nvm=%d", nslb_sock_ntop((struct sockaddr *)&(el->src_ip->ip_addr)),
                                                                    my_port_index);
}

inline void
add_srcip_to_busy_list(VUser *vptr, PerGrpUniqueSrcIPTable *el, int grp_idx)
{

  NSDL2_IPMGMT(vptr, NULL, "Method called, vptr = %p, grp_idx = %d, unique_src_ip_table = %p", vptr, grp_idx, unique_src_ip_table);
  NSDL2_IPMGMT(vptr, NULL, "Alloc src IP = %s by nvm=%d sees_inst=%u user_index=%u", 
                            nslb_sock_ntop((struct sockaddr *)&(el->src_ip->ip_addr)),
                            my_port_index, vptr->sess_inst, vptr->user_index);
  if (unique_src_ip_table[grp_idx].busy_srcip_tail) {
    NSDL2_IPMGMT(vptr, NULL, "Adding ip on busy srcip tail");
    unique_src_ip_table[grp_idx].busy_srcip_tail->next = el;
  } else {
    NSDL2_IPMGMT(vptr, NULL, "Adding ip on busy srcip head");
    unique_src_ip_table[grp_idx].busy_srcip_head = el;
  }
  unique_src_ip_table[grp_idx].busy_srcip_tail = el;
  NSDL2_IPMGMT(vptr, NULL, "free_srcip_head = %p, free_srcip_tail = %p",  unique_src_ip_table[grp_idx].free_srcip_head, unique_src_ip_table[grp_idx].free_srcip_tail);
  NSDL2_IPMGMT(vptr, NULL, "busy_srcip_head = %p, busy_srcip_tail = %p",  unique_src_ip_table[grp_idx].busy_srcip_head, unique_src_ip_table[grp_idx].busy_srcip_tail);
  el->next = NULL;

}


static IP_data *get_srcip_from_list (VUser *vptr, int grp_idx)
{
  PerGrpUniqueSrcIPTable *el;
  
  NSDL2_IPMGMT(vptr, NULL, "Method called, vptr = %p, grp_idx = %d, unique_src_ip_table = %p", vptr, grp_idx, unique_src_ip_table);

  NSDL2_IPMGMT(vptr, NULL, "free_srcip_head = %p, free_srcip_tail = %p",  unique_src_ip_table[grp_idx].free_srcip_head, unique_src_ip_table[grp_idx].free_srcip_tail);
  NSDL2_IPMGMT(vptr, NULL, "busy_srcip_head = %p, busy_srcip_tail = %p",  unique_src_ip_table[grp_idx].busy_srcip_head, unique_src_ip_table[grp_idx].busy_srcip_tail);

  if (!unique_src_ip_table[grp_idx].free_srcip_head) {
    NSTL1(vptr, NULL, "Could not alloc src IP for nvm=%d sess_inst=%u user_index=%u", my_port_index, vptr->sess_inst, vptr->user_index);
    return NULL;
  }

  //Remove from free list
  el = unique_src_ip_table[grp_idx].free_srcip_head;
  unique_src_ip_table[grp_idx].free_srcip_head = el->next;
  if(!unique_src_ip_table[grp_idx].free_srcip_head)
    unique_src_ip_table[grp_idx].free_srcip_tail = NULL;

  //Add to busy list
  add_srcip_to_busy_list(vptr, el, grp_idx);

 // if(srcip_debug) 
  NSDL2_IPMGMT(vptr, NULL, "Alloc src IP = %s by nvm=%d sees_inst=%u user_index=%u", nslb_sock_ntop((struct sockaddr *)&(el->src_ip->ip_addr)),
                                                                    my_port_index, vptr->sess_inst, vptr->user_index);
  return el->src_ip;
}

//add an element to tail
//Used at the time of freeing user slot
inline void add_srcip_to_list(VUser *vptr, IP_data *el)
{
  int grp_idx = vptr->group_num;
  NSDL2_IPMGMT(vptr, NULL, "Method called, runprof_table_shr_mem[%d].gset.src_ip_mode = %d, unique_src_ip_table = %p",
                            grp_idx, runprof_table_shr_mem[grp_idx].gset.src_ip_mode, unique_src_ip_table);
  if (runprof_table_shr_mem[grp_idx].gset.src_ip_mode == SRC_IP_UNIQUE) {
    NSDL2_IPMGMT(vptr, NULL, "Free src IP = %s by nvm = %d. free_srcip_head = %p, free_srcip_tail = %p busy_srcip_head = %p, "
                             "busy_srcip_tail = %p", nslb_sock_ntop((struct sockaddr *)&(el->ip_addr)), my_port_index,  
                             unique_src_ip_table[grp_idx].free_srcip_head, unique_src_ip_table[grp_idx].free_srcip_tail,
                             unique_src_ip_table[grp_idx].busy_srcip_head, unique_src_ip_table[grp_idx].busy_srcip_tail);

    
    // Remove from busy list :  How do we ensure the returned is same as head?
    if(!unique_src_ip_table[grp_idx].busy_srcip_head) {
      unique_src_ip_table[grp_idx].busy_srcip_tail = NULL;
    } else {
      //Remove from busy list
      PerGrpUniqueSrcIPTable *node = unique_src_ip_table[grp_idx].busy_srcip_head;
      unique_src_ip_table[grp_idx].busy_srcip_head = node->next;
      if(!unique_src_ip_table[grp_idx].busy_srcip_head)
        unique_src_ip_table[grp_idx].busy_srcip_tail = NULL;

      //Add to free list
      node->src_ip = el;
      add_srcip_to_free_list(vptr, node, grp_idx);
    }
  }
}

//Initialize srcip list of netstorm  child
static inline void init_srcip_list(int grp_idx)
{
  int num, i;
  IP_data *start;
  PerGrpUniqueSrcIPTable *src_ip_node;

  start = per_proc_src_ip_table_shr_mem[(my_port_index * total_runprof_entries) + grp_idx].start_ip;
  num = per_proc_src_ip_table_shr_mem[(my_port_index * total_runprof_entries) + grp_idx].num_ip;

  NSDL2_IPMGMT(NULL, NULL, "Method called, start = %p, num = %d child_idx = %d, my_port_index = %d", start, num, child_idx, my_port_index);
  for (i = 0; i < num; i++)
  {
    MY_MALLOC_AND_MEMSET(src_ip_node, sizeof(PerGrpUniqueSrcIPTable), "PerGrpUniqueSrcIPTable", -1);
    src_ip_node->src_ip = start + i;
    add_srcip_to_free_list(NULL, src_ip_node, grp_idx);
  }
}

//Child netstorm init for srcip's
void child_init_src_ip()
{
  int i;
  int grp_idx;
  int max_net_idx;
  NSDL2_IPMGMT(NULL, NULL, "Method called, total_ip_entries = %d", total_ip_entries);
  if(total_ip_entries) {
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
    {
      if (runprof_table_shr_mem[grp_idx].gset.src_ip_mode == SRC_IP_UNIQUE)  //uniq
        init_srcip_list(grp_idx);
      else  //shared
      {
        max_net_idx = runprof_table_shr_mem[grp_idx].gset.g_max_net_idx;
        for (i = 0; i < max_net_idx; i++)
          shared_src_ip_table[(grp_idx * max_net_idx) + i].seq_num = my_port_index;
      }
 
      // To Review in case of shm
      for (i=0; i < total_ip_entries; i++) {
        runprof_table_shr_mem[grp_idx].gset.ips[i].port = v_port_table[my_port_index].min_port;
        NSDL2_IPMGMT(NULL, NULL, "runprof_table_shr_mem[grp_idx].gset.ips[i].port = %d", runprof_table_shr_mem[grp_idx].gset.ips[i].port);
      }
    }
  } else {
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
    {
      if (runprof_table_shr_mem[grp_idx].gset.src_ip_mode == SRC_IP_UNIQUE)  //uniq
        NS_EXIT(1, "Error: IP entries (%d) for group %s are less than number of users (%d) with unique IP mode on child %d. Exiting", 
                    runprof_table_shr_mem[grp_idx].gset.num_ip_entries, runprof_table_shr_mem[grp_idx].scen_group_name, 
                    per_proc_runprof_table[(my_port_index * total_runprof_entries) + grp_idx], child_idx);
    }
  }
}

//Used to initialize user slot
IP_data *get_src_ip(VUser *vptr, int net_idx)
{
  static IP_data ip;
  int grp_idx = vptr->group_num;
  int max_net_idx; 
  NSDL2_IPMGMT(vptr, NULL, "Method called, vptr = %p, grp_idx = %d", vptr, grp_idx);
  GroupSettings *gset = &(runprof_table_shr_mem[grp_idx].gset);  

  if (gset->num_ip_entries && gset->src_ip_mode != SRC_IP_PRIMARY) {
    if(gset->src_ip_mode == SRC_IP_UNIQUE){
       NSDL2_IPMGMT(vptr, NULL, "Unique IP Mode");
       return (get_srcip_from_list (vptr, grp_idx));
    } else { //shared
      NSDL2_IPMGMT(vptr, NULL, "Shared IP Mode");
      max_net_idx = gset->g_max_net_idx; 
      shared_src_ip_table[(grp_idx * max_net_idx) + net_idx].seq_num %= gset->master_src_ip_table[net_idx].num_entries;
      return ((IP_data *)(gset->master_src_ip_table[net_idx].ip_entry + ((shared_src_ip_table[(grp_idx * max_net_idx) + net_idx].seq_num++))));
    }
  } else {
      //static var ip is already all set to 0
      //ip.ip_addr.s_addr = 0;
      NSDL2_IPMGMT(vptr, NULL, "Primary IP Mode");
      ip.ip_addr.sin6_family = AF_INET;
      ip.ip_id = -1;
      return (&ip);
   }
}

static int total_net_idx = 0;
static void create_src_ip_shr_mem()
{
  int i = 0, j, flag_ip;
  int group_num_ip = 0, grp_idx, skiped_nvm_count = 0, num_process = 0;
  int num_users = 0, extra = 0, num_ip = 0;
  GroupSettings *gset_ptr;  
  PerGrpSrcIPTable *per_proc_shr_ptr;
  Master_Src_Ip_Table *src_ip_shr_ptr;
  IP_data *ips_shr_ptr;
  IP_data *gset_ips_ptr[total_runprof_entries];
  int uniq_src_ip_num[total_runprof_entries][global_settings->num_process];

  NSDL1_IPMGMT(NULL, NULL, "Method called");
  // Create Shared Memory 
  int per_proc_src_ip_table_size = global_settings->num_process * total_runprof_entries * sizeof(PerGrpSrcIPTable); 
  int master_src_ip_table_size = total_net_idx * sizeof(Master_Src_Ip_Table); 
  int src_ip_table_shm_size = per_proc_src_ip_table_size + master_src_ip_table_size ;  // IP Mgmt total shared memory size 
  
  NSDL2_IPMGMT(NULL, NULL, "per_proc_src_ip_table_size = %d, master_src_ip_table_size = %d, src_ip_table_shm_size = %d",
                            per_proc_src_ip_table_size, master_src_ip_table_size, src_ip_table_shm_size);
  char *src_ip_table_shr_mem = do_shmget(src_ip_table_shm_size, "PerProcGroup+MasterSrcIP+IP Table");

  // Typecast shared memory pointers according to structure
  per_proc_src_ip_table_shr_mem = (PerGrpSrcIPTable *)src_ip_table_shr_mem;
  master_src_ip_table_shr_mem = (Master_Src_Ip_Table *)(src_ip_table_shr_mem + per_proc_src_ip_table_size);

  src_ip_shr_ptr = master_src_ip_table_shr_mem;
  per_proc_shr_ptr = per_proc_src_ip_table_shr_mem;
  memset(&uniq_src_ip_num, 0, sizeof(int) * global_settings->num_process * total_runprof_entries);

  /* We are removing ips shared memory and using the global memory so that each NVM will get its own memory because NVM updates this 
   * memory while updating the bind port in BIND_SOCKET(vptr->user_ip->port).
   * This design was changed while making USE_SRC_IP from global to group based G_USE_SRC_IP, there we found bug if binding error of port
   * Bug 44541 - IP MGMT || Getting bind address error when we start the test with keep alive keywords.
   */
  ips_shr_ptr = ips;
  group_default_settings->ips = ips_shr_ptr;  // Base pointer to ips
  gset_ptr = group_default_settings;
  gset_ptr->master_src_ip_table =  src_ip_shr_ptr; 
  NSDL2_IPMGMT(NULL, NULL, "ips_shr_ptr = %p, gset_ptr->g_max_net_idx = %d, src_ip_shr_ptr = %p", ips_shr_ptr, gset_ptr->g_max_net_idx, src_ip_shr_ptr);

  for(j = 0; j < gset_ptr->g_max_net_idx; j++, src_ip_shr_ptr++) 
  {
    flag_ip = 1;
    INIT_MASTER_SRC_IP_TABLE_ENTRIES(gset_ptr, src_ip_shr_ptr);
    NSDL2_IPMGMT(NULL, NULL, "inside for loop for all, src_ip_shr_ptr = %p, src_ip_shr_ptr->num_entries = %d", src_ip_shr_ptr, src_ip_shr_ptr->num_entries);
  }

  ips_shr_ptr += group_default_settings->num_ip_entries; // Adding num_ip_entries of ALL to reach ip entries of group
  //Fill Group Based Src Ip Data Shared Memory
  NSDL2_IPMGMT(NULL, NULL, "ips_shr_ptr = %p", ips_shr_ptr);
  flag_ip = 0;
  for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
  {
    i = 0;
    gset_ptr = &(runprof_table_shr_mem[grp_idx].gset);
    gset_ptr->num_ip_entries = runProfTable[grp_idx].gset.num_ip_entries;
    NSDL2_IPMGMT(NULL, NULL, "gset_ptr = %p, gset_ptr->num_ip_entries = %d", gset_ptr, gset_ptr->num_ip_entries);
    if(!gset_ptr->num_ip_entries)
    {
      SET_GROUP_IP_TO_ALL
      continue;
    }

    gset_ptr->ips = ips_shr_ptr;
    ips_shr_ptr += gset_ptr->num_ip_entries; 
    gset_ptr->master_src_ip_table =  src_ip_shr_ptr; 
    NSDL2_IPMGMT(NULL, NULL, "gset_ptr = %p, gset_ptr->num_ip_entries = %d, gset->master_src_ip_table = %p, gset_ptr->g_max_net_idx = %d",
                             gset_ptr, gset_ptr->num_ip_entries, gset_ptr->master_src_ip_table, gset_ptr->g_max_net_idx);
    for(j = 0; j < gset_ptr->g_max_net_idx; j++, src_ip_shr_ptr++) {
      NSDL2_IPMGMT(NULL, NULL, "i = %d, j = %d, gset_ptr->ips[i].net_idx = %d, src_ip_shr_ptr = %p", i, j, gset_ptr->ips[i].net_idx, src_ip_shr_ptr);
      flag_ip = 1;
      INIT_MASTER_SRC_IP_TABLE_ENTRIES(gset_ptr, src_ip_shr_ptr);
      if (gset_ptr->use_same_netid_src && (src_ip_shr_ptr->num_entries == 0)) {
        NS_EXIT(1, "ERROR: UseSameNetID mode enabled. some server address do not have src IP in same net");
      }
    }
    NSDL2_IPMGMT(NULL, NULL, "inside for loop for grp_idx %d, src_ip_shr_ptr = %p, src_ip_shr_ptr->num_entries = %d, src_ip_shr_ptr = %p", grp_idx, src_ip_shr_ptr, src_ip_shr_ptr->num_entries, src_ip_shr_ptr);
    //Give error, if any entry in master_src_ip_table has no addresses
    /*if (gset_ptr->use_same_netid_src && (src_ip_shr_ptr->num_entries == 0)) {
        NS_EXIT(1, "ERROR: UseSameNetID mode enabled. some server address do not have src IP in same net");
    }*/
  }

  DIVIDE_IP_PER_PROC_PER_GROUP

  //Per Proc
  for (i = 0; i < global_settings->num_process; i++) 
  {     
    //Per Group
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++,per_proc_shr_ptr++)
    { 
      if (runprof_table_shr_mem[grp_idx].gset.num_ip_entries && (runprof_table_shr_mem[grp_idx].gset.src_ip_mode == SRC_IP_UNIQUE)) 
      {
        num_ip = uniq_src_ip_num[grp_idx][i];
      //Per Proc
        NSDL2_IPMGMT(NULL, NULL, "num_ip = %d, grp_idx = %d, child_idx = %d, runprof_table_shr_mem[grp_idx].gset.src_ip_mode = %d",
                                num_ip, grp_idx, i, runprof_table_shr_mem[grp_idx].gset.src_ip_mode);

        //Fill PerProcGroupIp Shared Memory
        if(num_ip)
        {
          per_proc_shr_ptr->num_ip = num_ip;
          per_proc_shr_ptr->start_ip = gset_ips_ptr[grp_idx];
          gset_ips_ptr[grp_idx] = per_proc_shr_ptr->start_ip + per_proc_shr_ptr->num_ip;

          NSDL2_IPMGMT(NULL, NULL, "num_ip = %d, start_ip = %p, grp_idx = %d, child_idx = %d", 
                               per_proc_shr_ptr->num_ip, per_proc_shr_ptr->start_ip, grp_idx, i);
          NSDL2_IPMGMT(NULL, NULL, "Ip = %s", nslb_sock_ntop((struct sockaddr *)&(per_proc_shr_ptr->start_ip->ip_addr))); 
        }
        else
        { 
          per_proc_shr_ptr->num_ip = 0;	
          per_proc_shr_ptr->start_ip = NULL;
        } 
        NSDL2_IPMGMT(NULL, NULL, "gset_ips_ptr = %p, per_proc_shr_ptr-->num_ip = %d",  gset_ips_ptr, per_proc_shr_ptr->num_ip);
      }
    }
  }
} 

//Parent netstorm init for srcip's
void
parent_init_src_ip()
{
  int i, ports_per_child;

  NSDL1_IPMGMT(NULL, NULL, "Method called");

  init_src_ip_lists ();
  create_src_ip_shr_mem();
  // For handling of Unique Mode
  MY_MALLOC_AND_MEMSET(unique_src_ip_table, total_runprof_entries * sizeof(UniqueSrcIPTable), "UniqueSrcIPTable", -1);
  // For handling of Shared Mode
  MY_MALLOC_AND_MEMSET(shared_src_ip_table, total_net_idx * total_runprof_entries * sizeof(SharedSrcIPTable), "SharedSrcIPTable", -1);

  if(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED) {
    ports_per_child = ((60*1024/global_settings->num_process)/2) - 16;
  } else {
    ports_per_child = 60*1024/global_settings->num_process - 16;
  }
  NSDL2_IPMGMT(NULL, NULL, "ports_per_child = %d", ports_per_child);
  //Total port range from 1024 to 65535, leave 2k apart for other use and 1 K preveilaged. use 60 K
  for (i = 0; i < global_settings->num_process; i++) {
        v_port_table[i].min_port = 1024 + i*ports_per_child;
        v_port_table[i].max_port = 1024 + (i+1)*ports_per_child - 1;
        if(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED) {
          v_port_table[i].min_listen_port = v_port_table[i].max_port + 1;
          v_port_table[i].max_listen_port = v_port_table[i].max_port + ports_per_child - 1;
       } else {
          v_port_table[i].min_listen_port = v_port_table[i].max_listen_port =  0;
       }
       NSDL2_IPMGMT(NULL, NULL, "NVM:%d min_port = [%d] max_port = [%d] min_listen_port = [%d] max_listen_port = [%d]", 
                                i, v_port_table[i].min_port, v_port_table[i].max_port, v_port_table[i].min_listen_port, 
                                v_port_table[i].max_listen_port);
  }
}

int comfuncnetid(const void *a, const void *b)
{
   NSDL2_IPMGMT(NULL, NULL, "Method called");
   return (((IPRange *)a)->net_id - ((IPRange *)b)->net_id);
}

/*************************************************************************************************************************
This function checks for number of IP assigned in case of unique mode, fills net_idx in server_table_shr_mem and
cIPrangeTable tables. gset->ips is sorted on the basis of net_idx
*************************************************************************************************************************/
void init_src_ip_lists ()
{
  int i, h, grp_idx, s;
  int net_idx;
  int num_users;
  int match_flag = 0, done = 0, total_iprange_entries = 0;
  GroupSettings *gset_ptr;
  PerHostSvrTableEntry_Shr *svr_table_ptr = NULL;
  IPRange *ipRangeTablePtr = NULL;

  NSDL2_IPMGMT(NULL, NULL, "Method called, total_runprof_entries = %d", total_runprof_entries);
      
  //Use net_idx -1 as un-initialized
  for(i = 0; i < total_client_iprange_entries; i++)
    cIPrangeTable[i].net_idx = -1;

  //Use net_idx -1 as un-initialized
  for(i = 0; i < total_server_iprange_entries; i++)
    sIPrangeTable[i].net_idx = -1;

  for(grp_idx = -1; grp_idx < total_runprof_entries; grp_idx++)
  {
    num_users = 0;
    net_idx = -1;
    if(grp_idx  == -1)
      gset_ptr = group_default_settings;
    else
      gset_ptr = &(runprof_table_shr_mem[grp_idx].gset);

    NSDL2_IPMGMT(NULL, NULL, "gset->use_same_netid_src=%hd, gset_ptr->src_ip_mode = %d", gset_ptr->use_same_netid_src, gset_ptr->src_ip_mode);
    if(gset_ptr->src_ip_mode == SRC_IP_UNIQUE) {
      if (global_settings->load_key) {
        if(grp_idx != -1) { // for handling groups(g1, g2 etc..)
          for (i = 0; i < global_settings->num_process; i++) {
            num_users +=  per_proc_runprof_table[(i * total_runprof_entries) + grp_idx];
          }
        } else { // for handling of ALL
          for(i = 0; i < total_runprof_entries; i++) {
            if(!runprof_table_shr_mem[i].gset.num_ip_entries && (runprof_table_shr_mem[grp_idx].gset.src_ip_mode == SRC_IP_UNIQUE))
              num_users += runprof_table_shr_mem[i].quantity; 
          }
        }
        NSDL4_IPMGMT(NULL, NULL, "num_users = %d", num_users);
        if (num_users > gset_ptr->num_ip_entries) {
          if(!gset_ptr->num_ip_entries) {
            if(num_users > group_default_settings->num_ip_entries) {
              NS_EXIT(1, "Error: IP entries (%d) are less than number of users (%d) with unique IP mode. Exiting",
                group_default_settings->num_ip_entries, num_users);
            } 
          } else {
            NS_EXIT(1, "Error: IP entries (%d) are less than number of users (%d) with unique IP mode. Exiting",
                gset_ptr->num_ip_entries, num_users);
          }
          NSDL2_IPMGMT(NULL, NULL, "Considering IP entries for ALL in case of unique mode");
        }
      } else {
        if (global_settings->num_process > gset_ptr->num_ip_entries) {
          NS_EXIT(1, "Error: IP entries (%d) are less than number of NVM (%d) with unique IP mode. Exiting",
                gset_ptr->num_ip_entries, global_settings->num_process);
        }
      }
    }

  /* use_same_netid_src is 0 (default): No server address checking with client address, any client may hit any server
     use_same_netid_src is 1          : Server address (to be hit, actual servers) need not be a netocean address, But only clients 
                                        from same subnet may hit the server. Double while loop over actual server table  and client 
                                        IP ranges table, assigns net-idx to matching entries. Unmatched entries in client range are 
                                        given -1 nextidx. then Client address (ips table) is arranged for each ne-tidx. clients IP's
                                        with -1 netidx not used.
     use_same_netid_src is 2          : Server address (to be hit, actual servers) must be netocean address, only clients from same 
                                        subnet may hit the server. Double while loop over actual server table  and server IP ranges 
                                        table, assigns net-idx to matching entries. Unmatched entries in server range are given -1 
                                        nextidx. then Clinet address (ips table) is arranged for each net-idx. clients IP's with -1 
                                        netidx not used.*/
    if (gset_ptr->use_same_netid_src == 0 ) {
      //If same netid mode off, all ips belong to netid 0
      net_idx = 0;
      for(i = 0; i < total_totsvr_entries; i++)
        totsvr_table_shr_mem[i].net_idx = net_idx;

      for (i = 0; i < gset_ptr->num_ip_entries; i++)
        gset_ptr->ips[i].net_idx = net_idx;
    
      gset_ptr->g_max_net_idx++;
    } 
    else 
    {
      //Read IP entries for same id mode to be effective
      if(!done)
      {
        if (read_ip_entries ()) 
          NS_EXIT(1, "ERROR: Use Same NetID mode enabled. Unable to read ip_entries");
        done = 1;
        net_idx = -1;
        qsort(cIPrangeTable, total_client_iprange_entries, sizeof(IPRange), comfuncnetid);
        NSDL2_IPMGMT(NULL, NULL, "total_client_iprange_entries = %d", total_client_iprange_entries);

        for(i = 0 ; i < total_client_iprange_entries; i++)
        {
          if (i== 0 || cIPrangeTable[i].net_id != cIPrangeTable[i-1].net_id)
          {
	    net_idx++;
	    cIPrangeTable[i].net_idx = net_idx;
          }
          NSDL2_IPMGMT(NULL, NULL, "start_ip = %d, end_ip = %d, num_ip = %d, net_id = %d, netbits = %d, primary_ip = %d, net_idx = %d",
                       cIPrangeTable[i].start_ip, cIPrangeTable[i].end_ip, cIPrangeTable[i].num_ip, cIPrangeTable[i].net_id, 
                       cIPrangeTable[i].netbits, cIPrangeTable[i].primary_ip, cIPrangeTable[i].net_idx);
        }  
        net_idx = -1;
        qsort(sIPrangeTable, total_server_iprange_entries, sizeof(IPRange), comfuncnetid);
        for(i = 0 ; i < total_server_iprange_entries; i++)
        {
          if (i== 0 || sIPrangeTable[i].net_id != sIPrangeTable[i-1].net_id)
          {
	    net_idx++;
	    sIPrangeTable[i].net_idx = net_idx;
          }
        } 
      }
      if (gset_ptr->use_same_netid_src == 1 ) 
      {
        ipRangeTablePtr =  cIPrangeTable;
	total_iprange_entries = total_client_iprange_entries;
      }
      else
      { // gset_ptr->use_same_netid_src == 2
        ipRangeTablePtr =  sIPrangeTable;
	total_iprange_entries = total_server_iprange_entries;
      }
     
      NSDL2_IPMGMT(NULL, NULL, "total_iprange_entries=%d\n", total_iprange_entries);

      for( h = 0 ; h < gset_ptr->svr_host_settings.total_rec_host_entries; h++) 
      {
        for(s = 0; s < gset_ptr->svr_host_settings.host_table[h].total_act_svr_entries; s++) 
        {
          //Assign unique net_idx 0 onward for all server IP's in server_table_shr_mem
          svr_table_ptr = &gset_ptr->svr_host_settings.host_table[h].server_table[s];
          match_flag = 0;
          for(i = 0; i < total_iprange_entries; i++) 
          {
            NSDL2_IPMGMT(NULL, NULL, "For HostId = %d, serverId = %d, iprange_entries = %d, NetId (server & netbits) = %s, NetId = %s", h,
                         s, i, ns_char_ip(get_netid(nslb_get_sigfig_addr(&svr_table_ptr->saddr), ipRangeTablePtr[i].netbits)),
                         ns_char_ip(ipRangeTablePtr[i].net_id));
          
            if((get_netid(nslb_get_sigfig_addr(&svr_table_ptr->saddr), ipRangeTablePtr[i].netbits)) == ipRangeTablePtr[i].net_id) 
            {
              svr_table_ptr->net_idx = ipRangeTablePtr[i].net_idx;
              match_flag = 1; 
            }
          }
          if (match_flag == 0)
          {
            NS_EXIT(1, "ERROR: SAME_SUBNET_SRC configured. given server address (%s) does not match with any Client IP Range.", 
                        ns_char_ip(nslb_get_sigfig_addr(&svr_table_ptr->saddr)));
          }
        }
      }  

      for(s = 0; s <  gset_ptr->num_ip_entries; s++)
      {
        //Assign unique net_idx 0 onward for all server IP's in server_table_shr_mem
        gset_ptr->ips[s].net_idx = -1;
        //Find a matching ip_entries entry and assign uniq net_idx for al matching entries
        for(i = 0; i < total_iprange_entries; i++) {
          NSDL2_IPMGMT(NULL, NULL, "For NumIpEntries = %d, ipRangeEntry = %d, NetId (server & netbits) = %s, NetId = %s", s, i, 
                       ns_char_ip(get_netid(nslb_get_sigfig_addr(&(gset_ptr->ips[s].ip_addr)),
                       ipRangeTablePtr[i].netbits)), ns_char_ip(ipRangeTablePtr[i].net_id));
 
          if(get_netid(nslb_get_sigfig_addr(&(gset_ptr->ips[s].ip_addr)), ipRangeTablePtr[i].netbits) == ipRangeTablePtr[i].net_id) {
            gset_ptr->ips[s].net_idx = ipRangeTablePtr[i].net_idx;
            if (net_idx < ipRangeTablePtr[i].net_idx)
            {
               net_idx = ipRangeTablePtr[i].net_idx;
               gset_ptr->g_max_net_idx++;
            }
          }
        }
      }
    }// Else close
    if(gset_ptr->num_ip_entries)
      qsort(gset_ptr->ips, gset_ptr->num_ip_entries, sizeof(IP_data), comfuncnetid);
    total_net_idx += gset_ptr->g_max_net_idx;
    NSDL2_IPMGMT(NULL, NULL, "gset_ptr->g_max_net_idx %d", gset_ptr->g_max_net_idx);
  }
  //Free up IP entries tables
  if(cIPrangeTable)
    FREE_AND_MAKE_NOT_NULL (cIPrangeTable, "cIPrangeTable", -1);
  if(sIPrangeTable)
    FREE_AND_MAKE_NOT_NULL (sIPrangeTable, "sIPrangeTable", -1);
} //End of init_src_ip_lists()
