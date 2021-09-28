
#ifndef NS_TEST_GDF_H 
#define NS_TEST_GDF_H


// These defines used for test purpose only. after testing these will be removed.
//#define TOTAL_USER_URL_ERR   24
//#define TOTAL_USER_PAGE_ERR  64
//#define TOTAL_USER_SESS_ERR  64
//#define TOTAL_USER_TX_ERR    64
//#define MAXPATHLEN           256
//#define MAX_LINE_LENGTH      4024
//#define TOTAL_USER_URL_ERR   24
//#define TOTAL_USER_URL_ERR   24
#define BUFFER_SIZE          50

/*
typedef struct avgtime{

}avgtime;

typedef struct TxTableEntry_Shr {
  char* name; // pointer into the big buf
} TxTableEntry_Shr;

*/

typedef struct Globals {
  int progress_secs;
  int debug;
} Globals;


extern FILE *write_gdf_fp;
extern int no_tcp_monitor ;
extern int no_linux_monitor ;
extern int total_tunnels;
extern int no_of_host;
#ifndef CAV_MAIN
extern int total_tx_entries;
extern TxTableEntry_Shr *tx_table_shr_mem;
#else
extern __thread int total_tx_entries;
extern __thread TxTableEntry_Shr *tx_table_shr_mem;
#endif
//extern char *server_stat_ip[];
extern int testidx;
extern char g_test_start_time[];
extern char tunnelNames[];
extern char g_ns_wdir[];
extern Globals globals;

extern int is_no_tcp_present();
extern int is_no_linux_present();
extern char *get_tunnels();
extern void create_testrun_gdf(int runtime_flag);
extern void allocMsgBuffer();
extern void process_gdf(char *fname, int is_user_monitor, void *info_ptr, int dyn_obj_idx);
extern void create_tmp_gdf();
extern void close_gdf(FILE* fp);



#endif

