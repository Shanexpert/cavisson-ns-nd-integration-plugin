#ifndef USER_DISTRIBUTION_H
#define USER_DISTRIBUTION_H

#define GENERATOR_NAME_LEN 512
#define SESSION_RATE_MULTIPLIER  1000.0
typedef struct gen_name_quantity_list {
  char generator_name[GENERATOR_NAME_LEN];
  int qty_per_gen;
  int gen_id;
  double pct_per_gen; 
  double sessions;
}gen_name_quantity_list;

typedef struct grp_name_and_qty {
  char* group_name; 
  double pct_per_grp;
}grp_name_and_qty;

typedef struct gen_capacity_per_grp {
  int cap_per_gen;
  int per_gen_quantity_distributed;
}gen_capacity_per_gen;

typedef struct gen_qty_as_per_capacity {
  int qty_per_gen_cap;
}gen_qty_as_per_capacity;

extern gen_name_quantity_list* gen_list;
extern grp_name_and_qty* grp_qty_list;

typedef struct ScenGrpEntry
{
  int script_or_url;
  int quantity;
  int group_num;
  int grp_type;
  int num_generator; //Used to maintain
  int total_quantity_pct; 
  int pct_flag_set;//In case of PROF_NUM PCT need to store percentage in double array as pct distribution is done after parsing
  int num_fetches; //group based sessions
  double percentage;
  double tot_sessions; 
  double pct_value;
  int *generator_id_list; //Store list of generator id used for this group
  char **generator_name_list; //Used to store generator name list
  double *gen_pct_array;//In case of PROF_NUM PCT need to store percentage in double array as pct distribution is done after parsing
  char sess_name[GENERATOR_NAME_LEN];
  char scen_group_name[GENERATOR_NAME_LEN];
  char generator_name[GENERATOR_NAME_LEN];
  char scen_type[25];
  char uprof_name[GENERATOR_NAME_LEN];
  char sess_or_url_name[GENERATOR_NAME_LEN];
  char cluster_id[GENERATOR_NAME_LEN];
  char proj_name[GENERATOR_NAME_LEN];
  char sub_proj_name[GENERATOR_NAME_LEN];
}ScenGrpEntry;

extern ScenGrpEntry* scen_grp_entry;

extern int divide_usr_wrt_generator(int number_of_generators, double pct, gen_name_quantity_list* gen_list, char *buf, char *err_msg);
extern int divide_usr_wrt_generator_capacity(int rnum, gen_capacity_per_gen* gen_cap);
extern void distribute_pct_among_grps(ScenGrpEntry *scen_grp_entry, int total_quantity);
extern int pct_division_among_gen_per_grp(gen_name_quantity_list* gen_list, int sum, int total_gen_entries, int id, char *buf, char *err_msg);
extern void divide_limit_wrt_generators(int total_generators, int sess_limit, int max_pool_size);
#endif
