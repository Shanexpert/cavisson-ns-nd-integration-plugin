/********************************************************************
 * Name            : ns_page.h 
 * Purpose         : - 
 * Initial Version : Monday, July 13 2009
 * Modification    : -
 ********************************************************************/

#ifndef NS_PAGE_H
#define NS_PAGE_H

#define INIT_PAGERELOADPROF_ENTRIES 1024
#define DELTA_PAGERELOADPROF_ENTRIES 1024

#define INIT_PAGECLICKAWAYPROF_ENTRIES 1024
#define DELTA_PAGECLICKAWAYPROF_ENTRIES 1024

#define NS_NEXT_PG_STOP_SESSION -1

#define PAGE_WITH_INLINE_REPEAT 0x01
#define PAGE_WITH_INLINE_DELAY  0x02

typedef struct {
  unsigned int reload_timeout;   /* Given is (ms) Using this we have to decide reload a page or not */ 
  float min_reloads;              /* min reloads & max reload are used to find out the random reload attempts */
  float max_reloads;
} PageReloadProfTableEntry;


/* Shared memory structure of PageReloadProfTableEntry */
typedef struct {
  unsigned int reload_timeout;
  float min_reloads;
  float max_reloads;
} PageReloadProfTableEntry_Shr;


typedef struct {
  char call_check_page;                /* 0 or 1 to decide call check page or not. */
  short int clicked_away_on;           /* On which page to clicked away*/
  unsigned int clickaway_timeout;     /* Given in ms, Using this we have to decide click away a page or not*/
  double clickaway_pct;                /* How many pct of pages has to be click away ?*/
  int transaction_status;
} PageClickAwayProfTableEntry;

/* Shared memory structure of PageClickAwayProfTableEntry */
typedef struct {
  char call_check_page;
  short int clicked_away_on;
  unsigned int clickaway_timeout;
  double clickaway_pct;
  int transaction_status;
} PageClickAwayProfTableEntry_Shr;

/* following entries are user to create table entries of reload & click away*/
#ifndef CAV_MAIN
extern int max_pagereloadprof_entries;
extern int total_pagereloadprof_entries;
extern int max_pageclickawayprof_entries;
extern int total_pageclickawayprof_entries;
extern PageReloadProfTableEntry *pageReloadProfTable;
extern PageReloadProfTableEntry_Shr *pagereloadprof_table_shr_mem;

extern PageClickAwayProfTableEntry *pageClickAwayProfTable;
extern PageClickAwayProfTableEntry_Shr *pageclickawayprof_table_shr_mem;
#else
extern __thread int max_pagereloadprof_entries;
extern __thread int total_pagereloadprof_entries;
extern __thread int max_pageclickawayprof_entries;
extern __thread int total_pageclickawayprof_entries;
extern __thread PageReloadProfTableEntry *pageReloadProfTable;
extern __thread PageReloadProfTableEntry_Shr *pagereloadprof_table_shr_mem;

extern __thread PageClickAwayProfTableEntry *pageClickAwayProfTable;
extern __thread PageClickAwayProfTableEntry_Shr *pageclickawayprof_table_shr_mem;
#endif
/**/


extern void abort_bad_page(connection* cptr, int status, int redirect_flag);
extern inline void handle_page_complete(connection *cptr, VUser *vptr, int done, u_ns_ts_t now, int request_type);
extern void execute_next_page( VUser *vptr, connection* cptr, u_ns_ts_t now, PageTableEntry_Shr* next_page);
extern inline void on_page_start(VUser *vptr, u_ns_ts_t now);
extern void execute_page( VUser *vptr, int page_id, u_ns_ts_t now);


extern void initialize_runprof_page_clickaway_idx();
extern void initialize_runprof_page_reload_idx();
extern void free_runprof_page_reload_idx();
extern void free_runprof_page_clickaway_idx();
extern int validate_idle_secs_wrt_reload_clickaway(int pass, int group_idx, char *buf);
extern int  kw_set_g_page_reload(char *buf, int pass, char *err_msg, int runtime_flag);
extern int  kw_set_g_page_click_away(char *buf, int pass, char *err_msg, int runtime_flag);

/*copy reload & click away struct to shared memory*/
extern void copy_pagereload_into_shared_mem();
extern void copy_pageclickaway_into_shared_mem();
/**/

extern inline void is_reload_page(VUser *vuser, connection *cptr, int *);  // reload a page or not
extern int is_clickaway_page(VUser *vuser, connection *cptr); // click away a page or not
extern void delete_reload_or_clickaway_timer(connection *cptr);
extern void reload_connection( ClientData client_data, u_ns_ts_t now );
extern void click_away_connection( ClientData client_data, u_ns_ts_t now );
extern void chk_and_force_reload_click_away(connection *cptr, u_ns_ts_t now);
extern void create_default_reload_table();
extern void create_default_click_away_table();
extern int xmpp_file_upload_complete(VUser *vptr, u_ns_ts_t now, int status);

#endif
