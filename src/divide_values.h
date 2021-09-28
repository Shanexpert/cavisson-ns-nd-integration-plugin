#ifndef DIVIDE_VALUES_H
#define DIVIDE_VALUES_H

#include "divide_users.h"

typedef struct PerProcVgroupTable {
  int start_val;
    /* start_value: provide start offset of data recode for specified NVM */
  int cur_val;
    /* cur_val: provide current offset of data recode for specified NVM */
  int num_val;
    /* num_val: provide total number of data recorde distributed to specified NVMs. In USE_ONCE mode it reduced after each use. */
  int total_val;
    /* total_val: same as num_val. But it remains constant through one test. */
  int num_script_users; //sum of pct_or_users having same script
    /* num_script_users: provide per script total number of user distributed over all group */
  int last_file_fd;

  int rtc_flag;
    /* rtc_flag: provide file parameter group index into fparam_rtc_tbl, If group not participate into RTC then set it to -1 */ 

  //Handle Shared memory
    /* last_file_fd: use in USE_ONCE moe only to tracke last data file. */
  void *shm_addr;
    /* shm_addr: provide parent's base address of shared memory made at runtime. */
  int shm_key;
    /* shm_key: provide a key to attach shared memory with NVMs. */ 

  //For child access 
  GroupTableEntry_Shr *group_table_shr_mem;
    /* file_param_group_shr: provide start address of group_table_shr_mem of particule NVM /
       and it will be the base address of shared memory segment attached with this NVM*/
  VarTableEntry_Shr *variable_table_shr_mem;
    /* variable_table_shr_mem: provide start pointer of variable_table_shr_mem of particular NVM */
  WeightTableEntry *weight_table_shr_mem;
    /* weight_table_shr_mem: provide start pointer of weight_table_shr_mem of specific NVM */
  PointerTableEntry_Shr *pointer_table_shr_mem;
    /* pointer_table_shr_mem: provide start pointer of pointer_table_shr_mem of specific NVM */
  char *g_big_buf_shr_mem;
    /* g_big_buf_shr_mem: provide start pointer of g_big_buf_shr_mem */

  /* Manish: Re-think Is we need to store parent address? */
  //For parent access in cased of RTC only
  GroupTableEntry_Shr *p_group_table_shr_mem;
    /* file_param_group_shr: provide start address of group_table_shr_mem of particule NVM /
       and it will be the base address of shared memory segment attached with this NVM*/
  VarTableEntry_Shr *p_variable_table_shr_mem;
    /* variable_table_shr_mem: provide start pointer of variable_table_shr_mem of particular NVM */
  WeightTableEntry *p_weight_table_shr_mem;
    /* weight_table_shr_mem: provide start pointer of weight_table_shr_mem of specific NVM */
  PointerTableEntry_Shr *p_pointer_table_shr_mem;
    /* pointer_table_shr_mem: provide start pointer of pointer_table_shr_mem of specific NVM */
  char *p_g_big_buf_shr_mem;
    /* g_big_buf_shr_mem: provide start pointer of g_big_buf_shr_mem */
} PerProcVgroupTable;

#ifndef CAV_MAIN
extern PerProcVgroupTable *per_proc_vgroup_table;
#else
extern __thread PerProcVgroupTable *per_proc_vgroup_table;
#endif
extern PerProcVgroupTable *per_proc_vgroup_table_rtc;

extern int divide_values(PerScriptTotal *psTable, int runtime, int create_per_proc_staticvar_shr_mem);
extern int get_per_proc_num_script_users(int proc_id,  char* sess_name);
#endif
