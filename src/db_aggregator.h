#ifndef DB_AGG_H
#define DB_AGG_H

extern int db_aggregator_recovery();
extern int create_db_aggregator_process(int recovery_flag);
extern void init_component_rec_and_start_db_aggregator();
extern int db_aggregator_pid;
#endif
