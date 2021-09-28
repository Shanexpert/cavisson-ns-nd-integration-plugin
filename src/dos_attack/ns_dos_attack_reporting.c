/************************************************************************************************************************

Program Name: dos_attack/ns_dos_syn_attack_reporting.c
Purpose: This file contains the methods for filling data of dos attack  
Version: Initial version
Programed By: Manish Kumar Mishra
Date: 15/09/2011
Modified Date:
Issues: 

************************************************************************************************************************/

#include "ns_dos_attack_includes.h"
#include "ns_dos_attack_reporting.h"
#include "../netomni/src/core/ni_scenario_distribution.h"
#include "../ns_group_data.h"

DosAttackAvgTime *dos_attack_avgtime = NULL; //This will used to fill dos pointer

//Dos Attack Graph Info..

// For GDF
DosAttack_gp *dos_attack_gp_ptr = NULL;
unsigned int dos_attack_gp_idx;

// For avgtime
int g_dos_attack_avgtime_idx = -1;

inline void dos_attack_update_avgtime_size() {

  NSDL2_SOCKETS(NULL, NULL, "Method Called: g_avgtime_size = %d, g_dos_attack_avgtime_idx = %d",
                                          g_avgtime_size, g_dos_attack_avgtime_idx);

  if(global_settings->protocol_enabled & DOS_ATTACK_ENABLED) {
    g_dos_attack_avgtime_idx = g_avgtime_size;
    g_avgtime_size +=  sizeof(DosAttackAvgTime);

    NSDL4_SOCKETS(NULL, NULL, "Dos Attack is enabled. Setting g_avgtime_size = %d", g_avgtime_size);
  } else {
    NSDL4_SOCKETS(NULL, NULL, "Dos Attack is disabled. g_avgtime_size = %d", g_avgtime_size);
  }

  NSDL2_SOCKETS(NULL, NULL, "Method End: After g_avgtime_size = %d, g_dos_attack_avgtime_idx = %d",
                                          g_avgtime_size, g_dos_attack_avgtime_idx);
}

//This function will set a pointer to dos attack avg structure
inline void set_dos_attack_avgtime_ptr() {

  NSDL2_SOCKETS(NULL, NULL, "Method Called");

  if(global_settings->protocol_enabled & DOS_ATTACK_ENABLED) {
    NSDL2_SOCKETS(NULL, NULL, "Dos Attack is enabled.");
   /* We have allocated average_time with the size of DosAttackAvgTime 
      also now we can point that using g_dos_attack_avgtime_idx      */
    dos_attack_avgtime = (DosAttackAvgTime*)((char *)average_time + g_dos_attack_avgtime_idx);
  } else {
    NSDL2_SOCKETS(NULL, NULL, "Dos Attack is disabled.");
  }

  NSDL2_SOCKETS(NULL, NULL, "Method End: dos_attack_avgtime = %p", dos_attack_avgtime);
}

//This Function will print DOS Attack statistics in progress report
void dos_attack_print_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg) {
u_ns_4B_t dos_syn_attacks_num_succ = 0;
u_ns_4B_t dos_syn_attacks_num_err = 0;

  double dos_syn_attack_succ_ps = 0; 
  double dos_syn_attack_err_ps = 0;

  DosAttackAvgTime* dos_attack_avgtime_local = NULL;

  NSDL2_SOCKETS(NULL, NULL, "Method Called, dos_attack_avgtime = %p, is_periodic = %d", dos_attack_avgtime, is_periodic);
  
  if(global_settings->protocol_enabled & DOS_ATTACK_ENABLED) {
    dos_attack_avgtime_local = (DosAttackAvgTime*)((char*)avg + g_dos_attack_avgtime_idx);
    //dos_attack_avgtime_local = dos_attack_avgtime;

    dos_syn_attacks_num_succ = dos_attack_avgtime_local->dos_syn_attacks_num_succ; 
    dos_syn_attacks_num_err = dos_attack_avgtime_local->dos_syn_attacks_num_err; 

    NSDL2_SOCKETS(NULL, NULL, "dos_attack_avgtime_local = %p, dos_syn_attacks_num_succ = %'.3f, dos_syn_attacks_num_err = %'.3f",dos_attack_avgtime_local, dos_syn_attacks_num_succ, dos_syn_attacks_num_err);

    dos_syn_attack_succ_ps = (dos_syn_attacks_num_succ * 1000.0)/(double)global_settings->progress_secs;
    dos_syn_attack_err_ps = (dos_syn_attacks_num_err * 1000.0)/(double)global_settings->progress_secs;

    // DOS Attack rate (per sec): SYN Attacks OK=1058.123,  SYN Attacks Error=45.045
    fprint2f(fp1, fp2, "    DOS Attack rate (per sec): SYN Attacks OK=%'.3f, SYN Attacks Error=%'.3f\n",
	                 dos_syn_attack_succ_ps, dos_syn_attack_err_ps);
  } else {
    NSDL2_SOCKETS(NULL, NULL, "Dos Attack  is not enabled, hence not showing into progress report");
    return;
  }
}



//This function will fill the data in the structure dos_attack_avgtime 
inline void fill_dos_attack_gp(avgtime **g_avg) {
  int g_idx = 0, gv_idx, grp_idx;
  Long_data converted_raw_data_succ = 0.0;
  Long_data converted_raw_data_err = 0.0;

  if(dos_attack_gp_ptr == NULL) return;
  //DosAttackAvgTime *dos_attack_avg = NULL;
  DosAttackAvgTime* dos_attack_gp_avgtime_local = NULL;
  DosAttackAvgTime *avg = NULL;
  DosAttack_gp *dos_attack_local_gp_ptr = dos_attack_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called:");
  
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  { 
    avg = (DosAttackAvgTime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      dos_attack_gp_avgtime_local = (DosAttackAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_dos_attack_avgtime_idx);

      NSDL2_GDF(NULL, NULL, "dos_attack_gp_avgtime_local = %p, g_dos_attack_avgtime_idx = %d, dos_attack_local_gp_ptr = %p", dos_attack_gp_avgtime_local, g_dos_attack_avgtime_idx, dos_attack_local_gp_ptr);

      NSDL2_GDF(NULL, NULL, "dos_attack_gp_avgtime_local->dos_syn_attacks_num_succ = %u, dos_attack_gp_avgtime_local->dos_syn_attacks_num_err = %u", dos_attack_gp_avgtime_local->dos_syn_attacks_num_succ, dos_attack_gp_avgtime_local->dos_syn_attacks_num_err);
     
      converted_raw_data_succ = convert_long_data_to_ps_long_long((dos_attack_gp_avgtime_local->dos_syn_attacks_num_succ));     
      converted_raw_data_err = convert_long_data_to_ps_long_long((dos_attack_gp_avgtime_local->dos_syn_attacks_num_err));     
     
      NSDL2_GDF(NULL, NULL, "converted_raw_data_succ = %f, converted_raw_data_err = %f",converted_raw_data_succ, converted_raw_data_err);
      
      GDF_COPY_VECTOR_DATA(dos_attack_gp_idx, g_idx, gv_idx, 0, converted_raw_data_succ, dos_attack_local_gp_ptr->dos_syn_attacks_succ_ps); g_idx++;
      GDF_COPY_VECTOR_DATA(dos_attack_gp_idx, g_idx, gv_idx, 0, converted_raw_data_err,  dos_attack_local_gp_ptr->dos_syn_attacks_err_ps); g_idx++;
      g_idx = 0;
      dos_attack_local_gp_ptr++;
    }
  }
}
