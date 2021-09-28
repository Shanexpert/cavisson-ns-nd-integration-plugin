#ifndef DELIVER_REPORT_H
#define DELIVER_REPORT_H

extern int num_seq_increasing;
extern unsigned long long first_increasing;
extern long long last_avg ;

extern int deliver_report (int run_mode, int fd, avgtime **avg, cavgtime **cavg, FILE *rfp, FILE* srfp);
extern void fill_rtg_graph_data(avgtime **avg, cavgtime **cavg, int gen_idx);
//extern void fill_rtg_graph_data(avgtime *avg, void *ftp_avg, void *ldap_avg, cavgtime *cavg, int gen_idx, void *grp_avg,  void *imap_avg, void *jrmi_avg, void *rbu_page_stat_avg, void *page_based_stat_avgtime);
extern inline void send_data_to_secondary_gui_server();
#endif

