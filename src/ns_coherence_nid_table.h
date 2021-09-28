#ifndef _NS_COHERNECE_NID_TABLE_H
#define _NS_COHERENCE_NID_TABLE_H

#define DO_NOT_START_SERVER_SIG 0
#define START_SERVER_SIG 1
#define BUF_LENGTH 1024 

typedef struct NIDInstanceTable
{
  char *InstanceName;
  char *MachineName; //This should be the server name
  char *TierName;
  //char *ServerDisplayName;
  pid_t pid;
} NIDInstanceTable;

typedef struct NIDInstanceTableList
{
  int max_nid_inst_tbl_entries;
  int server_index;
  NIDInstanceTable *nid_inst_tbl_ptr;
} NIDInstanceTableList;


extern int kw_set_coherence_nodeid_table_size(char *keyword, char *buf, int runtime_flag, char *err_msg);
extern void process_coh_cluster_mon_vector(char *vector, char *breadcrumb, int start_server_sig_or_not, int row_no, int server_index);
extern void process_coh_cache_service_storage_mon_vector(char *vector, char *breadcrumb, char *gdf, int row_no);
extern int kw_set_coh_cluster_cache_vectors(char *keyword, char *buf);
extern void nid_inst_tbl_dump(FILE *fp);
extern int find_row_idx_from_nid_table(int server_index);
extern int create_nid_table_row_entries();
extern char cluster_vector_file_path[BUF_LENGTH];
extern char cache_vector_file_path[BUF_LENGTH];
#endif
