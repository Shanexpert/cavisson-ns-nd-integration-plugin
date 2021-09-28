
#ifndef NS_UNIQUE_RANGE_VAR_H 
#define NS_UNIQUE_RANGE_VAR_H 

#define UNQRANGVAR_MAX_LEN_512B          512
#define UNQRANGVAR_MAX_LEN_1K            1024

//#define UNIQUE_VAR_STOP_SESSION 0
//#define UNIQUE_VAR_REFRESH 1

#define DELTA_UNIQUE_RANGE_VAR_ENTRIES 32
#define INIT_UNIQUE_RANGE_VAR_ENTRIES 64
/* Refresh Modes */
#define SESSION 1
#define USE 2
#define ONCE 3

/* Action Modes */
#define SESSION_FAILURE 0
#define CONTINUE_WITH_LAST_VALUE 1
#define REPEAT_THE_RANGE_FROM_START 2
#define UNIQUE_RANGE_STOP_TEST 3

typedef struct UniqueRangeVarTableEntry {
  ns_bigbuf_t name; /* index into the big buffer */
  unsigned long start; /* Starting number */
  unsigned long block_size ; /* Block size per Vuser */
  ns_bigbuf_t format; /* Format of the variable */
  int format_len; 
  int action; /* Action taken on rage exhaustance */
  int refresh;
  int sess_idx;
} UniqueRangeVarTableEntry;

typedef struct UniqueRangeVarPerProcessTable {
  char *name; /* index into the big buffer */
  unsigned long start; /* Starting number */
  unsigned long block_size ; /* Block size per Vuser */
  char *format; /* Format of the variable */
  int format_len;
  int action; /* Action taken on rage exhaustance */
  int refresh;
  int uv_table_idx;
  int sess_idx;
  int userid;
} UniqueRangeVarPerProcessTable;

#ifndef CAV_MAIN
extern UniqueRangeVarPerProcessTable *unique_range_var_table;
#else
extern __thread UniqueRangeVarPerProcessTable *unique_range_var_table;
#endif
/*This table is used for picking the value for each user*/
typedef struct UniqueRangeVarVuserTable {
  unsigned long start; /*for start value*/
  unsigned long current;/*For current value*/
  unsigned long end; /* For end of the range*/
} UniqueRangeVarVuserTable;

//TODO:discuss with DJ
#define HANDLE_UNIQUE_RANGE_EXHAUSTED(now)                                                              \
{                                                                                                       \
  if(vptr->page_status == NS_UNIQUE_RANGE_ABORT_SESSION){                                               \
    if(cptr)                                                                                            \
    {                                                                                                   \
      NSDL1_VARS(vptr, NULL, "Use once data is over for parameter. Aborting current session");          \
      NS_FILL_IOVEC(*ns_iovec, null_iovec, NULL_IOVEC_LEN);\
      if(vptr->sess_status == NS_UNIQUE_RANGE_ABORT_SESSION && request->proto.http.type == MAIN_URL)    \
        vptr->next_pg_id = NS_NEXT_PG_STOP_SESSION;                                                     \
        if(vptr->page_status == NS_UNIQUE_RANGE_ABORT_SESSION && request->proto.http.type != MAIN_URL){ \
          vptr->page_status = 0;                                                                        \
          if(vptr->sess_status == NS_UNIQUE_RANGE_ABORT_SESSION)                                       \
            vptr->sess_status = 0;                                                                     \
          }                                                                                             \
          if(cptr->conn_fd > 0)                                                                         \
          {                                                                                             \
            NSDL1_VARS(vptr, NULL, "going to close connection in case of UNIQUE_RANGE");                \
            Close_connection(cptr, 1, now, NS_UNIQUE_RANGE_ABORT_SESSION, NS_COMPLETION_UNIQUE_RANGE_ERROR); \
          }                                                                                              \
          else                                                                                           \
            vptr->urls_awaited--;                                                                        \
          if(request->proto.http.type == EMBEDDED_URL)                                                   \
            vptr->page_status = prev_status;                                                             \
     }                                                                                                   \
     else                                                                                                \
     {                                                                                                   \
       NSDL1_VARS(vptr, NULL, "cptr is NULL: Use once data is over for parameter. Aborting current session"); \
       NS_FILL_IOVEC(*ns_iovec, null_iovec, NULL_IOVEC_LEN);\
       if(vptr->sess_status == NS_UNIQUE_RANGE_ABORT_SESSION)                                             \
         vptr->next_pg_id = NS_NEXT_PG_STOP_SESSION;                                                      \
       if(vptr->page_status == NS_UNIQUE_RANGE_ABORT_SESSION){                                            \
         vptr->page_status = -1;                                                                          \
         if(vptr->sess_status == NS_UNIQUE_RANGE_ABORT_SESSION)                                           \
           vptr->sess_status = -1;                                                                        \
       }                                                                                                  \
       vptr->urls_awaited--;                                                                              \
     }                                                                                                    \
     return -2;                                                                                           \
   }                                                                                                      \
}

#ifndef CAV_MAIN
extern UniqueRangeVarTableEntry *uniquerangevarTable;
extern UniqueRangeVarVuserTable *unique_range_var_vuserTable;
extern int total_unique_rangevar_entries;
#else
extern __thread UniqueRangeVarTableEntry *uniquerangevarTable;
extern __thread UniqueRangeVarVuserTable *unique_range_var_vuserTable;
extern __thread int total_unique_rangevar_entries;
#endif
//extern UniqueRangeVarTablePerChild *unique_range_var_table_per_child;
//extern UniqueRangeVarGlobalTableEntry **uniquerangevarglobalTable;

extern int input_unique_rangevar_data(char* line, int line_number, int sess_idx, char *script_filename);
extern void init_unique_range_var_info(void);
extern int find_unique_range_var_idx(char* name, int sess_idx);
extern void create_unique_range_var_table_per_proc(int process_id);
extern void free_unique_range_var();
#endif
