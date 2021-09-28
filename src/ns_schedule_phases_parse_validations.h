#ifndef __NS_SCHEDULE_PHASES_PARSE_VALIDATIONS_H__
#define __NS_SCHEDULE_PHASES_PARSE_VALIDATIONS_H__

extern Phases *add_schedule_phase(Schedule *schedule, int phase_type, char *phase_name);
extern void add_default_phases(Schedule *schedule, int grp_idx);
extern void initialize_schedule(Schedule *schedule, int grp_idx);
extern int calculate_high_water_mark(Schedule *schedule, int num_phases);
extern void high_water_mark_check(Schedule *schedule, int grp_mode, int grp_idx);
extern void balance_array(double *array, int len, int total_qty);
extern void convert_pct_to_qty();
extern int get_group_mode(int grp_idx);
extern void validate_phases();
extern void calculate_rpc_per_phase_per_child();

#endif
