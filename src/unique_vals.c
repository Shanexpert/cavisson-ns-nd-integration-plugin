#include <stdio.h>
#include <stdlib.h>
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
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "divide_users.h"
#include "divide_values.h"
#include "unique_vals.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"

#ifndef CAV_MAIN
unique_group_table_type* unique_group_table;
#else
__thread unique_group_table_type* unique_group_table;
#endif
void free_unique_val(unique_group_table_type* unique_group_entry, int val_idx) {
  NSDL2_VARS(NULL, NULL, "Method called, my_port_index = %d, val_idx = %d, num_available = %d, end = %d, num_values = %d",
                          my_port_index, val_idx, unique_group_entry->num_available, unique_group_entry->end,
                          unique_group_entry->num_values);

  if(val_idx < unique_group_entry->num_values)
  {
    unique_group_entry->num_available++;
    unique_group_entry->end++;
    NSDL2_VARS(NULL, NULL, "num_available = %d, num_values = %d, end = %d", 
                            unique_group_entry->num_available, unique_group_entry->num_values, unique_group_entry->end);
    assert(unique_group_entry->num_available <= unique_group_entry->num_values);
 
    if (unique_group_entry->end == unique_group_entry->num_values)
      unique_group_entry->end = 0;
 
    unique_group_entry->value_table[unique_group_entry->end] = val_idx;
  }
  else
  {
    NSDL2_VARS(NULL, NULL, "Ignore Index as in RTC Replace Mode no data avaialable for the index = %d", val_idx);
  }
}

int alloc_unique_val(unique_group_table_type* unique_group_entry, char* variable_name) {
  int return_val;

  NSDL2_VARS(NULL, NULL, "Method called, my_port_index = %d, variable_name = %s, num_available = %d, start = %d, num_values = %d, "
                         "unique_group_entry = %p", 
                          my_port_index, variable_name, unique_group_entry->num_available, unique_group_entry->start,
                          unique_group_entry->num_values, unique_group_entry);

  if (unique_group_entry->num_available == 0) {
    NSDL2_VARS(NULL, NULL, "No More unique values for the variable %s", variable_name);
    //fprintf(stderr, "No More unique values for the variable %s\n", variable_name);
    //end_test_run();
    return -2;
  } else {
    return_val = unique_group_entry->value_table[unique_group_entry->start];
    unique_group_entry->start++;
    if (unique_group_entry->start == unique_group_entry->num_values)
      unique_group_entry->start = 0;
    unique_group_entry->num_available--;
    NSDL2_VARS(NULL, NULL, "After, start = %d, num_available = %d", unique_group_entry->start, unique_group_entry->num_available);
  }

  return return_val;
}

void create_unique_group_tables(void) {
  int i;
  int value_idx;
  unique_group_table_type* unique_table_ptr = NULL;

  NSDL2_VARS(NULL, NULL, "Method called, Fill unique_group_table: my_port_index = %d, unique_group_id = %d, alloc size = %u, "
                         "total_group_entries = %d", 
                                         my_port_index, unique_group_id, (unique_group_id * sizeof(unique_group_table_type)),
                                         total_group_entries);

  MY_MALLOC(unique_group_table, unique_group_id * sizeof(unique_group_table_type), "unique_group_table", -1);
  unique_table_ptr = unique_group_table;

  for (i = 0; i < total_group_entries; i++) 
  {
    NSDL2_VARS(NULL, NULL, "Fparam group id = %d, sequence = %d, per proc num = %d", 
                            i, group_table_shr_mem[i].sequence, 
                            per_proc_vgroup_table[(my_port_index * total_group_entries) + i].num_val);

    if (group_table_shr_mem[i].sequence != UNIQUE)
      continue;

    unique_table_ptr->group_table_id = i;
    unique_table_ptr->num_available = unique_table_ptr->num_values = 
                                      per_proc_vgroup_table[(my_port_index * total_group_entries) + i].num_val;
    unique_table_ptr->start = 0;
    unique_table_ptr->end = unique_table_ptr->num_values - 1;

    MY_MALLOC(unique_table_ptr->value_table, unique_table_ptr->num_values * sizeof(int), "unique_table_ptr->value_table", -1);
    for (value_idx = 0; value_idx < unique_table_ptr->num_values; value_idx++)
    {
      //assert(value_idx < group_table_ptr->num_values);
      assert(value_idx < unique_table_ptr->num_values);
      unique_table_ptr->value_table[value_idx] = value_idx;
    }
    unique_table_ptr++;
  }
}

void
free_uniq_var_if_any (VUser *vptr)
{
int j;
unique_group_table_type* unique_group_ptr = unique_group_table;

  NSDL2_VARS(vptr, NULL, "Method called");
  for (j = 0; j < unique_group_id; j++, unique_group_ptr++) {
    if (vptr->ugtable[unique_group_ptr->group_table_id].cur_val_idx != -1)
      free_unique_val(unique_group_ptr, vptr->ugtable[unique_group_ptr->group_table_id].cur_val_idx);
  }
}

void set_uniq_vars(UserGroupEntry* group_table) {
  unique_group_table_type* uniq_table_ptr = unique_group_table;
  int group_idx;
  int i;

  NSDL2_VARS(NULL, NULL, "Method called, my_port_index = %d", my_port_index);

  for (i = 0; i < unique_group_id; i++, uniq_table_ptr++) {
    group_idx = uniq_table_ptr->group_table_id;
    assert(group_table_shr_mem[group_idx].sequence == UNIQUE);
    group_table[group_idx].cur_val_idx = -1;
  }

  //printf("%d: done with set_uniq_vars\n", my_port_index);
}

