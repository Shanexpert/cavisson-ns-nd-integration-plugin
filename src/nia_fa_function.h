#ifndef NIA_FA_H
#define NIA_FA_H


extern int nia_file_aggregator_recovery(int loader_opcode); 
extern int kw_set_nifa_trace_level(char *buf, char *err_msg, int runtime_flag);
extern int create_nia_file_aggregator_process(int num_child, int recovery_flag, int loader_opcode);
extern void init_component_rec_and_start_nia_fa(int loader_opcode);
extern void set_nia_file_aggregator_pid(int nia_fa_id);
extern int nia_file_aggregator_pid;
#endif
