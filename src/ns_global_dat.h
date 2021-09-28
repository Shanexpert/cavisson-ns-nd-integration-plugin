#ifndef _NS_GLOBAL_DAT_H_
#define _NS_GLOBAL_DAT_H_

#define TC_INDEFINITE   1
#define TC_SESSION      2
#define TC_TIME         3

#define OPEN_GLOBAL_DATA_FILE(buf, fp) \
  sprintf(buf, "logs/TR%d/global.dat", testidx); \
  if ((fp = fopen(buf, "a+" )) == NULL) { \
    fprintf(stderr, "Error in opening global.dat %s\n", buf); \
    return; \
  }

#define CLOSE_GLOBAL_DATA_FILE(fp) \
  if (fclose(fp) < 0) { \
    fprintf(stderr, "Error in closing global.dat"); \
    return; \
  }

typedef struct TargetCompletion {
  int type;   /* INDEFINITE, SESSION or TIME */
  u_ns_ts_t value;
} targetCompletion;

extern void log_global_dat(cavgtime *cavg, double *pg_data);
extern void estimate_target_completion();
extern void log_global_dat (cavgtime *cavg, double *pg_data);
extern void log_phase_time(int start_end, int phase_type, char *phase_name, char *time);


extern targetCompletion *estimate_completion_schedule_phase_start(targetCompletion *tc,
                                                                  Schedule *schedule, 
                                                                  int phase_idx);

extern targetCompletion * estimate_completion_schedule_phase_ramp_up(targetCompletion *tc,
                                                                     Schedule *schedule, 
                                                                     int phase_idx);

extern targetCompletion * estimate_completion_schedule_phase_ramp_down(targetCompletion *tc,
                                                                       Schedule *schedule, 
                                                                       int phase_idx);

extern targetCompletion * estimate_completion_schedule_phase_stabilize(targetCompletion *tc,
                                                                       Schedule *schedule, 
                                                                       int phase_idx);

extern targetCompletion *estimate_completion_schedule_phase_duration(targetCompletion *tc,
                                                                     Schedule *schedule, 
                                                                     int phase_idx);

extern targetCompletion *estimate_completion_schedule_phases(targetCompletion *tc,
                                                             Schedule *schedule);
extern char target_completion_time[];

extern void update_test_runphase_duration();

extern void log_global_dat_gui_server(); //added for gui server information in global.dat file.
#endif
