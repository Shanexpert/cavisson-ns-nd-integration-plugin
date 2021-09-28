#include "../ns_gdf.h"

#define ACC_DOS_ATTACK_PERIODICS(a, b)\
                if(global_settings->protocol_enabled & DOS_ATTACK_ENABLED) {\
                  (a)->dos_syn_attacks_num_succ += (b)->dos_syn_attacks_num_succ;\
                  (a)->dos_syn_attacks_num_err += (b)->dos_syn_attacks_num_err;\
                }

#define RESET_DOS_ATTACK_AVGTIME(a)\
{\
  if(global_settings->protocol_enabled & DOS_ATTACK_ENABLED)\
  { \
    DosAttackAvgTime *loc_dos_attack_avgtime = (DosAttackAvgTime*)((char*)a + g_dos_attack_avgtime_idx); \
    loc_dos_attack_avgtime->dos_syn_attacks_num_succ = 0;\
    loc_dos_attack_avgtime->dos_syn_attacks_num_err = 0;\
  }\
}

//This is structure for DOS SYN ATTACK data
typedef struct {
  u_ns_4B_t dos_syn_attacks_num_succ; // Number of Successful SYN requests in the sampling period
  u_ns_4B_t dos_syn_attacks_num_err; // Number of Failure SYN requests in the sampling period
} DosAttackAvgTime;

//This is structure for Dos Attack Gdf data 
typedef struct {
  Long_data dos_syn_attacks_succ_ps; //Number of SYN requests per second in the sampling period
  Long_data dos_syn_attacks_err_ps; //Number of SYN requests per second in the sampling period
} DosAttack_gp;

extern DosAttack_gp *dos_attack_gp_ptr;
extern unsigned int dos_attack_gp_idx;

extern int g_dos_attack_avgtime_idx;
extern int g_dos_attack_cavgtime_idx;
extern DosAttackAvgTime *dos_attack_avgtime;

//extern inline void  cache_update_cache_avgtime_size();
extern inline void set_dos_attack_avgtime_ptr();
extern void dos_attack_print_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg);
extern inline void fill_dos_attack_gp(avgtime **avg);
