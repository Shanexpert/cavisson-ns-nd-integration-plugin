
#ifndef SERVER_STATS_H
#define SERVER_STATS_H

// Added by Neeraj
// Start - Added for NetStorm


extern int init_rstat(int num_hosts, char **hosts);
extern int close_rstat();
extern int get_rstat_data_for_all_servers(int num_servers, Server_stats_gp *server_stats);

// End - Added for NetStorm

#endif
