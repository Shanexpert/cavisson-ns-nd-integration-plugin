#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
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
#include "ns_log.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_phases_parse.h"
#include "ns_log.h"
#include "ns_schedule_phases.h"
#include "ns_global_dat.h"
#include "wait_forever.h"
#include "ns_exit.h"

static void log_schedule_phase_dat_ex(targetCompletion *tc, 
                               Schedule *schedule, int grp_idx, FILE *sp_fd)
{
  int phase_idx;
  Phases *ph;
  char buf[1024];
  char time[0xff];

  for (phase_idx = 0; phase_idx < schedule->num_phases; phase_idx++) {
    ph = &(schedule->phase_array[phase_idx]);
      
    if (tc->type == TC_INDEFINITE || tc->type == TC_SESSION) {
      return;
    }

    switch (ph->phase_type) {
    case SCHEDULE_PHASE_START:
      estimate_completion_schedule_phase_start(tc, schedule, phase_idx);
      convert_to_hh_mm_ss(tc->value, time);
      fprintf(sp_fd, "TARGET_PHASE_TIME %s %s %s\n", 
              (grp_idx == -1) ? "ALL" : 
              runprof_table_shr_mem[grp_idx].scen_group_name,
              ph->phase_name,
              time);
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      estimate_completion_schedule_phase_ramp_up(tc, schedule, phase_idx);
      convert_to_hh_mm_ss(tc->value, time);
      NSDL4_SCHEDULE(NULL, NULL, "tc->value = %u, %s\n",
                     tc->value, time);
      fprintf(sp_fd, "TARGET_PHASE_TIME %s %s %s\n", 
              grp_idx == -1 ? "ALL" : 
              runprof_table_shr_mem[grp_idx].scen_group_name,
              ph->phase_name,
              time);
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      estimate_completion_schedule_phase_ramp_down(tc, schedule, phase_idx);
      convert_to_hh_mm_ss(tc->value, time);
      fprintf(sp_fd, "TARGET_PHASE_TIME %s %s %s\n", 
              grp_idx == -1 ? "ALL" : 
              runprof_table_shr_mem[grp_idx].scen_group_name,
              ph->phase_name,
              time);
      break;
    case SCHEDULE_PHASE_STABILIZE:
      estimate_completion_schedule_phase_stabilize(tc, schedule, phase_idx);
      NSDL4_SCHEDULE(NULL, NULL, "STABILIZE: tc->value = %u\n",
                     tc->value);
      convert_to_hh_mm_ss(tc->value, time);
      fprintf(sp_fd, "TARGET_PHASE_TIME %s %s %s\n", 
              grp_idx == -1 ? "ALL" : 
              runprof_table_shr_mem[grp_idx].scen_group_name,
              ph->phase_name,
              time);
      break;
    case SCHEDULE_PHASE_DURATION:
      estimate_completion_schedule_phase_duration(tc, schedule, phase_idx);
      if (tc->type == TC_INDEFINITE) {
        strcpy(buf, "INDEFINITE");
      } else if (tc->type == TC_SESSION) {
        sprintf(buf, "%llu", tc->value);
      } else {
        convert_to_hh_mm_ss(tc->value, time);
        strcpy(buf, time);
      }
      fprintf(sp_fd, "TARGET_PHASE_TIME %s %s %s\n", 
              grp_idx == -1 ? "ALL" : 
              runprof_table_shr_mem[grp_idx].scen_group_name,
              ph->phase_name,
              buf);
      break;
    default:                  /* can not be */
      NS_EXIT(-1, "Unknown phase found, hence exiting...");
      break;
    }
  }
}

/**
 * Function logs the phase information that netstorm interprets 
 * from the scenario in the format:
 * 
 * TARGET_PHASE_TIME <Grp> <Phase> <HH:MM:SS>
 *
 * So for simple scenario based schedule a typical entry will be:
 * TARGET_PHASE_TIME ALL RampUP 00:00:10
 *
 * Group based will be:
 * TARGET_PHASE_TIME G1 RampUP 00:00:10
 */
void log_schedule_phases_dat()
{
  Schedule *schedule;
  FILE *sp_fd;
  int grp_idx;
  targetCompletion tc;
  char file[MAX_FILE_NAME];


  sprintf(file, "logs/%s/schedule_phases.dat", global_settings->tr_or_common_files);
  sp_fd = fopen(file, "w");

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule = scenario_schedule;
    grp_idx = -1;
    tc.type = -1;
    tc.value = 0;
    log_schedule_phase_dat_ex(&tc, schedule, grp_idx, sp_fd);
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
      /* NetCloud: In case of generator, we can have scenario configuration with SGRP sessions/users 0
       * Here we need to by-pass RAMP_UP check, because on generator schedule phases for such group does not exists*/
      if ((loader_opcode != CLIENT_LOADER) || ((loader_opcode == CLIENT_LOADER) && (runprof_table_shr_mem[grp_idx].quantity != 0)))
      {
        schedule = &(group_schedule[grp_idx]);
        tc.type = -1;
        tc.value = 0;
        schedule->group_idx = grp_idx; //setting grp idx
        log_schedule_phase_dat_ex(&tc, schedule, grp_idx, sp_fd);
        schedule->group_idx = -1;   //resetting grp idx
      }
    }
  }
  fclose(sp_fd);
}
