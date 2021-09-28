#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "nslb_util.h"
#include "ns_replay_db_query.h"
#include "ipmgmt_utils.h"
#include "ns_string.h"
#include "poi.h"
#include "ns_trans_parse.h"
#include "ns_exit.h"

int total_db_query_entries = 0;
int max_db_query_entries = 0;
int cum_query_pct = 0;
NsDbQuery *ns_db_query;
NsDbQuery_Shr *ns_db_query_shr;
int *db_shr_ptr;

static inline int db_query_create_table_entry(int *row_num)
{
  NSDL4_REPLAY(NULL, NULL, "Method called");

  if (total_db_query_entries == max_db_query_entries)
  {
    MY_REALLOC_EX(ns_db_query, (max_db_query_entries + DELTA_DB_QUERY_ENTRIES) * sizeof(NsDbQuery),
                                        (max_db_query_entries * sizeof(NsDbQuery)), "NsDbQuery", -1);
    max_db_query_entries += DELTA_DB_QUERY_ENTRIES;
  }
  *row_num = total_db_query_entries++;

  NSDL4_REPLAY(NULL, NULL, "Method exiting, row_num = %d", *row_num);
  return 0;
}

void parse_query_file(char *replay_file_dir) {
  FILE *replay_db_query_ptr;
  char buff[MAX_LINE_LENGTH + 1];
  char *fields[MAX_DB_QUERY_FILE_ARGS];
  char *parameters[REPLAY_DB_QUERY_NUM_PARAMETERS];
  char *query_parameters[2];
  int i;
  int num_tokens = 0, row_num = 0, num_param_tokens;
  short status;
  
  NSDL2_REPLAY(NULL, NULL, "Method called, replay_file_dir = %s", replay_file_dir);

  replay_db_query_ptr = fopen(replay_file_dir, "r");

  if(!replay_db_query_ptr)
  {
    NS_EXIT(-1, "Unable to open query file %s given in keyword REPLAY_FILE.\nTest Run Cancelled.", replay_file_dir);
  }
  while (nslb_fgets(buff, MAX_LINE_LENGTH, replay_db_query_ptr, 0) != NULL)
  {
    buff[strlen(buff) - 1] = '\0'; // remove \n
    if(buff[0] == '#' || buff[0] == '\0' || buff[0] == '\n')
      continue;

    num_tokens = get_tokens(buff, fields, "|", MAX_DB_QUERY_FILE_ARGS);

    // TODO create usages
    if (num_tokens < 7)
    {
      NS_EXIT(-1, "Num tokens are less than 7 in file %s for the keyword REPLAY_FILE 11.", replay_file_dir);
    } 
     
    status = (short)atoi(fields[0]);

    if (status == 0)
    {
      NSDL2_REPLAY(NULL, NULL, "Status for the line %s is 0. Hence continuing.", buff);
      continue;
    }

    // Create memory for query 
    db_query_create_table_entry(&row_num);

    // Copy pool name
    strncpy(ns_db_query[row_num].pool_name, fields[1], MAX_POOL_TRANS_NAME_SIZE);
    ns_db_query[row_num].pool_name[MAX_POOL_TRANS_NAME_SIZE] = '\0';

    ns_db_query[row_num].query_pct = (int)atof(fields[2]) * 100;
    cum_query_pct += ns_db_query[row_num].query_pct;

    // Copy trans name
    strncpy(ns_db_query[row_num].trans_name, fields[3], MAX_POOL_TRANS_NAME_SIZE);
    ns_db_query[row_num].trans_name[MAX_POOL_TRANS_NAME_SIZE] = '\0';

    add_trans_name(ns_db_query[row_num].trans_name, 0);

    // Tokenize parameters by ,
    ns_db_query[row_num].num_parameters = get_tokens(fields[4], parameters, ",", REPLAY_DB_QUERY_NUM_PARAMETERS);
    if((ns_db_query[row_num].num_parameters == 1) && !(strcmp(parameters[0], "NA"))) {
      ns_db_query[row_num].num_parameters = 0;
      NSDL2_REPLAY(NULL, NULL, "Got NA in parameter");
    } else {
      for(i = 0; i < ns_db_query[row_num].num_parameters; i++)
      {
        num_param_tokens = get_tokens(parameters[i], query_parameters, ":", 2);
        if (num_param_tokens > 2 || num_param_tokens < 2){
          NSDL2_REPLAY(NULL, NULL, "Ignoring this parameter type and value.");
          continue;
        }
        ns_db_query[row_num].query_parameters[i].param_type = atoi(query_parameters[0]);
        strncpy(ns_db_query[row_num].query_parameters[i].param_name, query_parameters[1], MAX_PARAM_NAME_SIZE);
      }
    }
  
    /* setting query type as 0, 1 or 2.
     0 -> Prepared statement               e.g. select * from ? 
     1 -> Direct Query without parameters  e.g. select * from employee
     2 -> Direct Query with parameters     e.g. select * from {param}   */
    ns_db_query[row_num].query_type = (short)atoi(fields[5]);

    strncpy(ns_db_query[row_num].query, fields[6], MAX_BUFF_SIZE);
  }
  if(cum_query_pct == 0){
    NS_EXIT(-1, "Weightage for all queries is 0. Kindly provide weightage to atleast one active query in the file %s.", replay_file_dir);
  }    
  NSDL2_REPLAY(NULL, NULL, "cum_query_pct = %d", cum_query_pct);
}

void copy_query_table_into_shr_memory() {

  int i, j = 0, k;
  int pct;
  int start_pct_id;
  NSDL1_REPLAY(NULL, NULL, "Method Called. total_db_query_entries = %d", total_db_query_entries);
  ns_db_query_shr = (NsDbQuery_Shr*) do_shmget(sizeof(NsDbQuery_Shr) * (total_db_query_entries), "NsDbQuery_Shr");
  db_shr_ptr = (int *) do_shmget(sizeof(int) * (cum_query_pct), "db_shr_ptr");

  for(i = 0; i < total_db_query_entries; i++){
    strcpy(ns_db_query_shr[i].query, ns_db_query[i].query);
    strcpy(ns_db_query_shr[i].trans_name, ns_db_query[i].trans_name);
    strcpy(ns_db_query_shr[i].pool_name, ns_db_query[i].pool_name);
    ns_db_query_shr[i].num_parameters = ns_db_query[i].num_parameters;
  
    for(k = 0; k < ns_db_query[i].num_parameters; k++)
    {
      ns_db_query_shr[i].query_parameters[k].param_type = ns_db_query[i].query_parameters[k].param_type;
      strcpy(ns_db_query_shr[i].query_parameters[k].param_name, ns_db_query[i].query_parameters[k].param_name); 
    }

    pct = ns_db_query[i].query_pct;
     
    start_pct_id = j;

    // This loop is to store the query_idx for each query weightage.
    for(; j < start_pct_id + pct; j++) {
      db_shr_ptr[j] = i;
    }
  }
  FREE_AND_MAKE_NULL(ns_db_query, "NsDbQuery", -1);
}

// This function generates a random no. using sp_handle, selects a query and returns its index.
int ns_get_query_index() {
  int random_num;
  int query_idx;

  NSDL1_REPLAY(NULL, NULL, "Method Called.");
  random_num = ns_get_random(sp_handle);
  query_idx = db_shr_ptr[random_num];

  NSDL1_REPLAY(NULL, NULL, "random_num = %d, query_idx = %d", random_num, query_idx);

  return query_idx;
}

/*  This function is to mark advance_param_flag for FILE parameter in case of single and multiple groups. 
If num_parameter = 1, advance_param_flag is set 1 (as it implies there is only 1 group). 
If num_parameter > 1, then advance_param_flag is set only for the parameter that is used first in the query. 
The value for advance_param_flag is checked in function ns_db_replay_query for calling ns_advance_param API.   */ 
void set_advance_param_flag_in_db_replay() 
{
  int i, j;
  int var_hashcode, num_variables;
  VarTransTableEntry_Shr* var_ptr;
  int grp_var_idx, used_param_idx, already_adv;
  char var_name[256];

  NSDL1_REPLAY(NULL, NULL, "Method Called. total_db_query_entries = %d", total_db_query_entries);
 
  for(i = 0; i < total_db_query_entries; i++) 
  {
    NSDL2_REPLAY(NULL, NULL, "ns_db_query_shr[i].num_parameters = %d", ns_db_query_shr[i].num_parameters);
    for(j = 0; j < ns_db_query_shr[i].num_parameters; j++)
    { 
      // using sess_idx = 0 as we are having only one script that has the same name as scenario.
      var_hashcode = gSessionTable[0].var_hash_func(ns_db_query_shr[i].query_parameters[j].param_name,
                                                    strlen(ns_db_query_shr[i].query_parameters[j].param_name));
      NSDL2_REPLAY(NULL, NULL, "var_hashcode = %d for variable = %s", var_hashcode, ns_db_query_shr[i].query_parameters[j].param_name);
      if (var_hashcode == -1) {
        fprintf(stderr, "Invalid parameter name. Parameter name:%s\n", ns_db_query_shr[i].query_parameters[j].param_name);
        return;
      }

      // got a variable pointer 
      var_ptr = &gSessionTable[0].vars_trans_table_shr_mem[var_hashcode];
      
      //Get number of variable with associated group var_ptr->fparam_grp_idx
      num_variables = groupTable[var_ptr->fparam_grp_idx].num_vars; 
      NSDL2_REPLAY(NULL, NULL, "var_ptr->var_type = %d num_variables = %d", var_ptr->var_type, num_variables);

      switch(var_ptr->var_type)
      {
        case DATE_VAR:
        case RANDOM_VAR:
        case RANDOM_STRING:
          NSDL4_REPLAY(NULL, NULL, "setting advance param flag for parameter %s", ns_db_query_shr[i].query_parameters[j].param_name);
          ns_db_query_shr[i].query_parameters[j].advance_param_flag = 1;
          break;

        case VAR:
          if(num_variables == 1) { 
            NSDL4_REPLAY(NULL, NULL, "setting advance param flag for parameter %s", ns_db_query_shr[i].query_parameters[j].param_name);
            // setting advance_param_flag as this query has only 1 group
            ns_db_query_shr[i].query_parameters[j].advance_param_flag = 1;
          } else if (num_variables > 1) {
            NSDL4_REPLAY(NULL, NULL, "More than one variable present in group. Hence, going to check advance param flag for parameter %s", 
                                      ns_db_query_shr[i].query_parameters[j].param_name);
            
            // Check if any other parameter of this group present in used parameter list upto current parameter location, if any present then
            // Do not advance current parameter     
            already_adv = 0;
            for(grp_var_idx = 0; grp_var_idx < num_variables; grp_var_idx++)
            {
              // Get the first variable name from group
              strcpy(var_name, RETRIEVE_BUFFER_DATA(varTable[groupTable[var_ptr->fparam_grp_idx].start_var_idx + grp_var_idx].name_pointer));
              NSDL4_REPLAY(NULL, NULL, "Variable name from group = %s", var_name);
              for(used_param_idx = j - 1; used_param_idx >= 0; used_param_idx--)
              {
                NSDL4_REPLAY(NULL, NULL, "Variable name in sequence = %s", ns_db_query_shr[i].query_parameters[used_param_idx].param_name);
                //If match found, then we need to break both the loops
                if(!strcmp(var_name, ns_db_query_shr[i].query_parameters[used_param_idx].param_name))
                {
                  NSDL2_REPLAY(NULL, NULL, "Variable %s from this group is already present in used sequence before %s, Hence not setting"
                                                " advance flag", var_name, ns_db_query_shr[i].query_parameters[j].param_name);
                  already_adv = 1; 
                  break;  
                }
                if(already_adv)
                  break;
              }    
            }
            if(!already_adv){
              NSDL4_REPLAY(NULL, NULL, "setting advance param flag for parameter %s", ns_db_query_shr[i].query_parameters[j].param_name);
              ns_db_query_shr[i].query_parameters[j].advance_param_flag = 1;
            }
          }
          break;

        default: 
          NSDL2_REPLAY(NULL, NULL, "not setting advance param flag");
          break;
      }
    }
  }
}

