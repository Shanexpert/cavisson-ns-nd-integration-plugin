#ifndef UNIQUE_VALS_H
#define UNIQUE_VALS_H

#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int num_values;
  int num_available;
  int start;
  int end;
  int* value_table;
  int group_table_id;
} unique_group_table_type;
#ifndef CAV_MAIN
extern unique_group_table_type* unique_group_table;
#else
extern __thread unique_group_table_type* unique_group_table;
#endif
extern void free_unique_val(unique_group_table_type* unique_group_entry, int val_idx);
extern int alloc_unique_val(unique_group_table_type* unique_group_entry, char* variable_name);
extern void create_unique_group_tables(void);
extern void free_uniq_var_if_any (VUser *vptr);
extern void set_uniq_vars(UserGroupEntry* group_table);

#endif
