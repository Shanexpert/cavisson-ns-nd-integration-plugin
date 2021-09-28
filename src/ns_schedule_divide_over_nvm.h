#ifndef __NS_SCHEDULE_DIVIDE_OVER_NVM_H__
#define __NS_SCHEDULE_DIVIDE_OVER_NVM_H__

extern void init_vport_table_phases();
extern int get_total_from_nvm(int nvm_id, int grp_idx);
extern void balance_phase_array_per_nvm(int qty_to_distribute, double *pct_array, double *qty_array, Schedule *schedule, int grp_idx, int phase_id);
extern void balance_phase_array_per_group(int qty_to_distribute, double *pct_array,
                                          double *qty_array, Schedule *schedule, int phase_id, int proc_index);
extern void distribute_phase_over_nvm(int grp_idx, Schedule *schedule);
extern void distribute_group_qty_over_phase(Schedule *schedule, int proc_index);
extern void fill_per_group_phase_table();

#define WORDSIZE 8
extern int next_power_of_n(int value, int base);
#endif  /* __NS_SCHEDULE_DIVIDE_OVER_NVM_H__ */
