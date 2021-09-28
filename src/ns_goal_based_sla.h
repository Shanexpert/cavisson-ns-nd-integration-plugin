/********************************************************************
* Name: ns_goal_based_sla.h
* Purpose: File containing fuction prototypes for SLA type goal based scenarios
* Author: Anuj
* Intial version date: 14/05/08
* Last modification date
********************************************************************/

#ifndef NS_GOAL_BASED_SLA_H
#define NS_GOAL_BASED_SLA_H

#define INIT_SLA_ENTRIES 8

/* From SLA_URL to SLA_SESS_WO_WAIT are object type */
#define SLA_ALL 0
#define SLA_URL 1
#define SLA_PAGE 2
#define SLA_TX_W_WAIT 3
#define SLA_TX_WO_WAIT 4
#define SLA_SESS_W_WAIT 5
#define SLA_SESS_WO_WAIT 6
#define SLA_NUM_OBJ_TYPE 7   /* THIS MUST ALWAYS BE AT END OF THE OBJECT IDENTIFIERS */

/* From SLA_MEAN to SLA_FAILURE are object properties */
#define SLA_AVG 0
#define SLA_MEAN 1
#define SLA_MIN 2
#define SLA_MAX 3
#define SLA_80 4
#define SLA_90 5
#define SLA_99 6
#define SLA_FAILURE 7
#define SLA_NUM_OBJ_PROP 8 /* THIS MUST ALWAYS BE AT END OF THE OBJECT PROPERTIES */

// Vector options of SLA
#define SLA_VECTOR_OPTION_ALL       0
#define SLA_VECTOR_OPTION_OVERALL   1
#define SLA_VECTOR_OPTION_SPECIFIED 2
#define SLA_VECTOR_OPTION_NA        99 // Default vector_option

#define SLA_VECTOR_NAME_LENGTH   128

// Relation for SLA
#define SLA_RELATION_LESS_THAN 0
#define SLA_RELATION_GR_THAN   1
#define SLA_RELATION_EQUAL 2

// SLA ALL gdf_rpt_gp_id gdf_rpt_graph_id vector_option vector_name relation value
typedef struct SLATableEntry 
{
  int user_id;                              // This will be ALL always, hardcoded right now (for future use)
  int gdf_rpt_gp_id;                        // Id of the group (GDF) 
  int gdf_rpt_graph_id;                     // Id of the graph (GDF)
  int vector_option;                        // Valid for vector groups only (could be ALL, OVERALL, SPECIFIED), else NA
  char vector_name[SLA_VECTOR_NAME_LENGTH]; // This will be valid iff vector_option is SPECIFIED
  int relation;                             // LESS_TAHN or GREATER_TAHN
  double value;                             // Value to be compare with
  double pct_variation;                     // Allowed % variation (+/-)
} SLATableEntry;

// SLA ALL gdf_rpt_gp_id gdf_rpt_graph_id vector_option vector_name relation value
typedef struct SLATableEntry_Shr 
{
  int user_id;
  int gdf_rpt_gp_num_idx;                   // This will be the index of the particular gp in the gdf 
  int gdf_rpt_graph_num_idx;                // This will be the index of the particular gp in the gdf
  char vector_option;
  short gdf_group_vector_idx; 
  short gdf_graph_vector_idx; 
  char relation;                             // LESS_TAHN or GREATER_TAHN
  double value;                             // Value to be compare with
  double pct_variation;                     // Allowed % variation (+/-)
  short gdf_group_num_vectors;
  short gdf_graph_num_vectors;
} SLATableEntry_Shr;

extern SLATableEntry *slaTable;
extern SLATableEntry_Shr *sla_table_shr_mem;

// extern unsigned long long sla_stats[SLA_NUM_OBJ_TYPE][SLA_NUM_OBJ_PROP];

//############ util.c ################
extern void kw_set_sla(char *buf);
extern int  kw_set_guess(char *buf);
extern void kw_set_stablize(char *buf);
extern void kw_set_capacity_check(char *buf);
extern void validate_capacity_check();
extern void validate_guess_type();
extern void validate_stabilize();
extern void alloc_mem_for_sla_table(void);
extern void *copy_slaTable_to_shr(void *prof_table_shr_mem);
extern void print_slaTable();

//############ deliver_report.c ###############
extern void fill_sla_stats(avgtime *avg);
extern void check_system_capacity(avgtime *avg);

// ########### netstorm.c ############


// ########### ns_parent.c ############
// extern int tstate_criteria_met(avgtime* end_results, int pct);
// extern inline int sys_not_healthy(avgtime* end_results);
// extern inline int test_long_enough(void);
 
extern int original_progress_msecs;
#endif
// End of File

